#include "gps.h"
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const struct device *i2c_bus = DEVICE_DT_GET(GPS_I2C_BUS);
static const uint16_t gps_addr = GPS_I2C_ADDR;
static char nmea_buffer[90];  // NMEA sentences are typically 70-80 chars
static int nmea_buffer_pos = 0;
static gps_coords_t last_coords = {0.0f, 0.0f, false, false, false, 0, 0, 0, "", 0, 0};

// Helper function to check if a field is empty (matches Adafruit isEmpty)
static bool is_empty(const char *p) {
    if (p == NULL) return true;
    if (*p == ',' || *p == '*' || *p == '\0') return true;
    return false;
}

// Parse coordinate from NMEA format DDDMM.MMMM (matches Adafruit parseCoord exactly)
static bool parse_coord(const char *pStart, float *angleDegrees, char *dir) {
    const char *p = pStart;
    if (is_empty(p)) {
        return false;  // no number
    }
    
    // get the number in DDDMM.mmmm format and break into components
    char degreebuff[10] = {0}; // Ensure string is terminated after strncpy
    const char *e = strchr(p, '.');
    if (e == NULL || e - p > 6)
        return false;                // no decimal point in range
    
    strncpy(degreebuff, p, e - p); // get DDDMM
    long dddmm = atol(degreebuff);
    long degrees = (dddmm / 100);         // truncate the minutes
    long minutes = dddmm - degrees * 100; // remove the degrees
    
    p = e;                                // start from the decimal point
    float decminutes = atof(e); // the fraction after the decimal point
    
    // Find next comma to get direction
    const char *next_comma = strchr(p, ',');
    if (next_comma == NULL) {
        return false;
    }
    p = next_comma + 1;            // go to the next field
    
    // get the NSEW direction as a character
    char nsew = 'X';
    if (!is_empty(p))
        nsew = *p; // field is not empty
    else
        return false; // no direction provided
    
    // set the various numerical formats to their values
    long fixed = degrees * 10000000L + (minutes * 10000000L) / 60L +
                 (long)(decminutes * 10000000L) / 60L;
    float deg = fixed / 10000000.0f;
    
    if (nsew == 'S' || nsew == 'W') { // fixed and deg are signed, but DDDMM.mmmm is not
        fixed = -fixed;
        deg = -deg;
    }
    
    // reject directions that are not NSEW
    if (nsew != 'N' && nsew != 'S' && nsew != 'E' && nsew != 'W')
        return false;
    
    // reject angles that are out of range
    if (nsew == 'N' || nsew == 'S')
        if (deg < -90.0f || deg > 90.0f)
            return false;
    if (deg < -180.0f || deg > 180.0f)
        return false;
    
    // store in locations passed as args
    if (angleDegrees != NULL)
        *angleDegrees = deg;
    if (dir != NULL)
        *dir = nsew;
    
    return true;
}

// NMEA sentence parsing - lifted from Adafruit GGA parsing
static bool parse_nmea_gga(const char *sentence, gps_coords_t *coords) {
    // Check if it's a GGA sentence
    if (strncmp(sentence, "$GPGGA", 6) != 0 && 
        strncmp(sentence, "$GNGGA", 6) != 0) {
        return false;
    }
    
    // Make a copy for parsing (strtok modifies the string)
    char sentence_copy[128];
    strncpy(sentence_copy, sentence, sizeof(sentence_copy) - 1);
    sentence_copy[sizeof(sentence_copy) - 1] = '\0';
    
    // Find first comma (after sentence type)
    char *p = strchr(sentence_copy, ',');
    if (p == NULL) return false;
    p++;  // Skip to char after the comma
    
    // Field 1: Time (skip it)
    p = strchr(p, ',');
    if (p == NULL) return false;
    p++;
    
    // Field 2: Latitude (parse with parseCoord)
    float lat_deg = 0.0f;
    char lat_dir = 'N';
    bool lat_parsed = parse_coord(p, &lat_deg, &lat_dir);
    
    // Skip to next field (latitude direction is already parsed by parseCoord)
    p = strchr(p, ',');
    if (p == NULL) return false;
    p++;
    p = strchr(p, ',');  // Skip lat direction field
    if (p == NULL) return false;
    p++;
    
    // Field 4: Longitude (parse with parseCoord)
    float lon_deg = 0.0f;
    char lon_dir = 'E';
    bool lon_parsed = parse_coord(p, &lon_deg, &lon_dir);
    
    // Skip to next field (longitude direction is already parsed by parseCoord)
    p = strchr(p, ',');
    if (p == NULL) return false;
    p++;
    p = strchr(p, ',');  // Skip lon direction field
    if (p == NULL) return false;
    p++;
    
    // Field 6: Fix quality
    int quality = 0;
    if (!is_empty(p)) {
        quality = atoi(p);
    }
    coords->fix_quality = quality;
    
    // Move to next field
    p = strchr(p, ',');
    if (p == NULL) return false;
    p++;
    
    // Field 7: Number of satellites
    if (!is_empty(p)) {
        coords->num_satellites = atoi(p);
    }
    
    // Only set coordinates if both were successfully parsed AND we have a fix
    // This matches Adafruit's logic: fix = true only when fixquality > 0
    if (quality > 0 && lat_parsed && lon_parsed) {
        coords->latitude = lat_deg;
        coords->longitude = lon_deg;
        coords->valid = true;
    } else {
        coords->latitude = 0.0f;
        coords->longitude = 0.0f;
        coords->valid = false;
    }
    
    return true;  // Successfully parsed GGA sentence
}

int gps_init(void) {
    if (!device_is_ready(i2c_bus)) {
        return -1;
    }
    
    // Send initialization commands via I2C
    // Turn on GGA and RMC sentences
    gps_send_command("PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
    k_msleep(100);
    
    // Set update rate to 1Hz
    gps_send_command("PMTK220,1000");
    k_msleep(100);
    
    return 0;
}

int gps_send_command(const char *cmd) {
    if (!device_is_ready(i2c_bus)) {
        return -1;
    }
    
    char pmtk_cmd[64];
    snprintf(pmtk_cmd, sizeof(pmtk_cmd), "$PMTK%s*", cmd);
    
    // Calculate checksum (XOR of all bytes between $ and *)
    uint8_t checksum = 0;
    for (int i = 1; pmtk_cmd[i] != '*'; i++) {
        checksum ^= pmtk_cmd[i];
    }
    
    char full_cmd[70];
    snprintf(full_cmd, sizeof(full_cmd), "%s%02X\r\n", pmtk_cmd, checksum);
    
    // Adafruit GPS I2C writes commands to register 0xFE (command register)
    uint8_t cmd_reg = 0xFE;
    uint8_t write_buf[70];
    write_buf[0] = cmd_reg;
    memcpy(&write_buf[1], full_cmd, strlen(full_cmd));
    
    int ret = i2c_write(i2c_bus, write_buf, strlen(full_cmd) + 1, gps_addr);
    if (ret < 0) {
        return ret;
    }
    
    return 0;
}

gps_coords_t gps_get_coords(void) {
    // Check I2C bus status
    if (!device_is_ready(i2c_bus)) {
        last_coords.i2c_ok = false;
        last_coords.data_received = false;
        last_coords.valid = false;
        last_coords.i2c_error = -1;
        last_coords.bytes_received = 0;
        last_coords.nonzero_bytes = 0;
        last_coords.last_sentence[0] = '\0';
        return last_coords;
    }
    
    last_coords.i2c_ok = true;
    // Clear last sentence initially
    last_coords.last_sentence[0] = '\0';
    
    // Adafruit GPS I2C reads from register 0xFF (data buffer)
    // Read up to 32 bytes at a time (like the Python code)
    uint8_t reg = 0xFF;
    uint8_t data[32];
    memset(data, 0, sizeof(data));  // Clear buffer
    
    // Write register address, then read data
    int ret = i2c_write_read(i2c_bus, gps_addr, &reg, 1, data, sizeof(data));
    last_coords.i2c_error = ret;
    last_coords.bytes_received = sizeof(data);
    
    if (ret == 0) {
        // Count nonzero bytes
        int nonzero_count = 0;
        for (int i = 0; i < sizeof(data); i++) {
            if (data[i] != 0) {
                nonzero_count++;
            }
        }
        last_coords.nonzero_bytes = nonzero_count;
        
        if (nonzero_count > 0) {
            last_coords.data_received = true;
            
            // Process received bytes
            for (int i = 0; i < sizeof(data); i++) {
                char c = data[i];
                
                if (c == '\n' || c == '\r') {
                    if (nmea_buffer_pos > 0) {
                        nmea_buffer[nmea_buffer_pos] = '\0';
                        
                        // Only store valid NMEA sentences (must start with '$')
                        if (nmea_buffer[0] == '$' && nmea_buffer_pos > 5) {
                            // Check if it's a GGA sentence
                            bool is_gga = (strncmp(nmea_buffer, "$GPGGA", 6) == 0 || 
                                          strncmp(nmea_buffer, "$GNGGA", 6) == 0);
                            
                            if (is_gga) {
                                // Store full sentence for debugging
                                int sentence_len = strlen(nmea_buffer);
                                int copy_len = (sentence_len < sizeof(last_coords.last_sentence) - 1) ? 
                                              sentence_len : sizeof(last_coords.last_sentence) - 1;
                                strncpy(last_coords.last_sentence, nmea_buffer, copy_len);
                                last_coords.last_sentence[copy_len] = '\0';
                                
                                // Try to parse as GGA sentence
                                gps_coords_t coords = last_coords;  // Start with previous coords to preserve them
                                coords.data_received = true;
                                coords.i2c_ok = true;
                                coords.i2c_error = 0;
                                coords.bytes_received = sizeof(data);
                                coords.nonzero_bytes = nonzero_count;
                                
                                // Parse the sentence (this will update coords if successful)
                                parse_nmea_gga(nmea_buffer, &coords);
                                
                                // Update last_coords with parsed data (preserves previous coords if parsing fails)
                                last_coords = coords;
                                
                                // Always store the sentence so we can see what we're parsing
                                strncpy(last_coords.last_sentence, nmea_buffer, copy_len);
                                last_coords.last_sentence[copy_len] = '\0';
                            }
                        }
                        
                        nmea_buffer_pos = 0;
                    }
                } else if (c != '\0' && nmea_buffer_pos < (sizeof(nmea_buffer) - 1)) {
                    nmea_buffer[nmea_buffer_pos++] = c;
                } else if (nmea_buffer_pos >= (sizeof(nmea_buffer) - 1)) {
                    // Buffer overflow, reset
                    nmea_buffer_pos = 0;
                }
            }
        } else {
            // I2C read succeeded but got all zeros - device might not be responding
            last_coords.data_received = false;
        }
    } else {
        // I2C read failed
        last_coords.data_received = false;
        last_coords.i2c_ok = false;
        last_coords.bytes_received = 0;
        last_coords.nonzero_bytes = 0;
    }
    
    return last_coords;
}
