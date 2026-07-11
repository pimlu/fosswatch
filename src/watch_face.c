#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "watch_face.h"
#include "drivers/sharp.h"
#include "graphics.h"
#include "font.h"
#include "drivers/gps.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
LOG_MODULE_REGISTER(watch_face, LOG_LEVEL_DBG);

bool gngga_displayed = false;

void render_watch_screen(){
    printk("TEST!");
    LOG_INF("render watch screen");
    printf("render_watch_screen\n");
    clear_screen();
    drawText("FOSS-WATCH", 0, 0, 2);

    int64_t ms = k_uptime_get();   /* milliseconds since boot */
    // int64_t s  = ms / 1000;

    int hours   = (ms / 3600000) % 12;    /* analog clock, 12-hour */
    int minutes = (ms / 60000) % 60;
    int milliseconds = ms % 60000;
    int seconds = milliseconds / 1000;

    /* Center of display */
    int cx = SHARP_W / 2;
    int cy = SHARP_H / 2;
    int radius = (SHARP_H < SHARP_W ? SHARP_H : SHARP_W) / 2 - 2;

    /* Angles in radians (0 at 12 o’clock, clockwise) */
    float secAngle = (milliseconds / 60000.0f) * 2 * M_PI;
    float minAngle = (minutes / 60.0f) * 2 * M_PI;
    float hourAngle = ((hours + minutes/60.0f) / 12.0f) * 2 * M_PI;

    /* Hand endpoints */
    int sx = cx + (int)(radius * 0.9f * sinf(secAngle));
    int sy = cy - (int)(radius * 0.9f * cosf(secAngle));
    int mx = cx + (int)(radius * 0.75f * sinf(minAngle));
    int my = cy - (int)(radius * 0.75f * cosf(minAngle));
    int hx = cx + (int)(radius * 0.5f * sinf(hourAngle));
    int hy = cy - (int)(radius * 0.5f * cosf(hourAngle));

    /* Draw hands */
    drawLine(cx, cy, hx, hy);  // hour hand
    drawLine(cx, cy, mx, my);  // minute hand
    drawLine(cx, cy, sx, sy);  // second hand
    
   
    drawTime(hours, minutes, seconds,  0, 140, 2);
    
    // Display GPS coordinates with debug info
    gps_coords_t coords = gps_get_coords();
    if (coords.valid) {
        drawText("Lat:", 0, 100, 1);
        drawFloat(coords.latitude, 4, 30, 100, 1);
        
        drawText("Lon:", 0, 110, 1);
        drawFloat(coords.longitude, 4, 30, 110, 1);
    } else {
        // Show detailed debug info
        char debug_buf[32];
        if (!coords.i2c_ok) {
            snprintf(debug_buf, sizeof(debug_buf), "I2C err:%d", coords.i2c_error);
            drawText(debug_buf, 0, 100, 1);
        } else if (!coords.data_received) {
            snprintf(debug_buf, sizeof(debug_buf), "No data err:%d", coords.i2c_error);
            drawText(debug_buf, 0, 100, 1);
            snprintf(debug_buf, sizeof(debug_buf), "Bytes:%d nz:%d", 
                    coords.bytes_received, coords.nonzero_bytes);
            drawText(debug_buf, 0, 110, 1);
        } else {
            drawText("GPS: No fix", 0, 100, 1);
            // Show fix quality and satellite count
            char status_buf[32];
            if (coords.fix_quality >= 0) {
                snprintf(status_buf, sizeof(status_buf), "Q:%d Sats:%d", 
                        coords.fix_quality, coords.num_satellites);
                drawText(status_buf, 0, 110, 1);
                
                // Show full NMEA sentence if available (split across multiple lines)
                if (coords.last_sentence[0] != '\0') {
                    int len = strlen(coords.last_sentence);
                    // Display up to 23 chars per line (144px / 6px per char, leave some margin)
                    int chars_per_line = 23;
                    int y_pos = 120;
                    int line = 0;
                    
                    // Display sentence across multiple lines
                    for (int i = 0; i < len && line < 3; i += chars_per_line) {
                        char line_buf[24];
                        int remaining = len - i;
                        int copy_len = (remaining < chars_per_line) ? remaining : chars_per_line;
                        strncpy(line_buf, &coords.last_sentence[i], copy_len);
                        line_buf[copy_len] = '\0';
                        drawText(line_buf, 0, y_pos + (line * 10), 1);
                        line++;
                    }
                }
            } else if (coords.last_sentence[0] != '\0') {
                // Show sentence type if no quality info yet
                char sentence_type[8] = {0};
                int len = strlen(coords.last_sentence);
                int copy_len = (len < 7) ? len : 7;
                strncpy(sentence_type, coords.last_sentence, copy_len);
                sentence_type[copy_len] = '\0';
                drawText(sentence_type, 0, 110, 1);
            } else {
                drawText("Waiting...", 0, 110, 1);
            }
        }
    }

    // Only freeze if we have a valid fix with both coordinates
    if (!gngga_displayed && coords.valid && 
        coords.fix_quality > 0 && 
        coords.latitude != 0.0f && coords.longitude != 0.0f) {
        gngga_displayed = true;
        // Sleep for 5 minutes to keep the screen frozen
        k_msleep(5 * 60 * 1000);
    }
}