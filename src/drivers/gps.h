#pragma once

#include <zephyr/drivers/i2c.h>
#include <stdint.h>
#include <stdbool.h>

#define GPS_I2C_BUS DT_NODELABEL(i2c0)
#define GPS_I2C_ADDR 0x10

// GPS coordinate structure
typedef struct {
    float latitude;
    float longitude;
    bool valid;           // true if we have a valid GPS fix
    bool data_received;   // true if we've received any data from GPS
    bool i2c_ok;          // true if I2C communication is working
    int i2c_error;        // I2C error code (0 = success)
    int bytes_received;    // Number of bytes received in last read
    int nonzero_bytes;     // Number of nonzero bytes in last read
    char last_sentence[80]; // Last NMEA sentence received (full sentence)
    int fix_quality;       // GPS fix quality (0=no fix, 1=GPS, 2=DGPS, etc.)
    int num_satellites;    // Number of satellites in view
    bool time_valid;       // true once we've parsed a UTC time from the GPS
    int utc_hours;         // UTC time-of-day reported by the GPS
    int utc_minutes;
    int utc_seconds;
    bool date_valid;       // true once we've parsed a UTC date (from RMC)
    int utc_day;           // UTC calendar date reported by the GPS (RMC)
    int utc_month;         // 1-12
    int utc_year;          // full year, e.g. 2026
} gps_coords_t;

// Initialize GPS module and start the background polling thread
int gps_init(void);

// Get current GPS coordinates (parses NMEA sentences)
gps_coords_t gps_get_coords(void);

// Send PMTK command to GPS
int gps_send_command(const char *cmd);
