#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "watch_face.h"
#include "drivers/sharp.h"
#include "graphics.h"
#include "font.h"
#include "drivers/gps.h"
#include "track.h"
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
LOG_MODULE_REGISTER(watch_face, LOG_LEVEL_DBG);

/* Local time offset applied to the GPS UTC clock.
 * -4 = US Eastern Daylight Time (summer). Use -5 for EST (winter). */
#define GPS_TZ_OFFSET_HOURS (-4)

/* Currently displayed face. */
static watch_face_t current_face = FACE_DIGITAL;

void watch_face_set(watch_face_t face) {
    if (face < FACE_COUNT) {
        current_face = face;
    }
}

watch_face_t watch_face_get(void) {
    return current_face;
}

void watch_face_next(void) {
    current_face = (current_face + 1) % FACE_COUNT;
}

/* ---- local-time / calendar helpers ------------------------------------- */

static int days_in_month(int year, int month) {
    static const int dm[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 &&
        ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
        return 29;
    }
    return dm[month - 1];
}

/* Shift a calendar date by a small number of days (used for timezone
 * rollover across midnight). */
static void add_days(int *year, int *month, int *day, int delta) {
    int d = *day;
    while (delta > 0) {
        d++;
        if (d > days_in_month(*year, *month)) {
            d = 1;
            if (++(*month) > 12) { *month = 1; (*year)++; }
        }
        delta--;
    }
    while (delta < 0) {
        d--;
        if (d < 1) {
            if (--(*month) < 1) { *month = 12; (*year)--; }
            d = days_in_month(*year, *month);
        }
        delta++;
    }
    *day = d;
}

/* Apply the timezone offset to a UTC hour, returning the local hour and how
 * many days the date rolled by. */
static int utc_hour_to_local(int utc_hour, int *day_delta) {
    int total = utc_hour + GPS_TZ_OFFSET_HOURS;
    int delta = 0;
    while (total < 0)   { total += 24; delta--; }
    while (total >= 24) { total -= 24; delta++; }
    *day_delta = delta;
    return total;
}

/* ---- digital 4-field face ---------------------------------------------- */

void render_digital_watch_face() {
    gps_coords_t coords = gps_get_coords();
    clear_screen();

    /* --- Field 1 & 2: local time and date ------------------------------ */
    int hours, minutes, seconds;
    int day_delta = 0;
    char date_buf[16];

    if (coords.time_valid) {
        hours = utc_hour_to_local(coords.utc_hours, &day_delta);
        minutes = coords.utc_minutes;
        seconds = coords.utc_seconds;
    } else {
        /* No GPS time yet: fall back to uptime so the face still ticks. */
        int64_t ms = k_uptime_get();
        hours   = (int)((ms / 3600000) % 24);
        minutes = (int)((ms / 60000) % 60);
        seconds = (int)((ms / 1000) % 60);
    }

    if (coords.time_valid && coords.date_valid) {
        int y = coords.utc_year, m = coords.utc_month, d = coords.utc_day;
        add_days(&y, &m, &d, day_delta);
        snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d", y, m, d);
    } else {
        snprintf(date_buf, sizeof(date_buf), "----/--/--");
    }

    /* Date (scale 2) then the time as the hero (scale 3, spans full width). */
    drawText(date_buf, 6, 8, 2);
    drawTime((uint8_t)hours, (uint8_t)minutes, (uint8_t)seconds, 0, 32, 3);

    /* --- Field 3: distance since power-on ------------------------------ */
    float miles = track_distance_m() / 1609.344f;
    char dist_buf[16];
    snprintf(dist_buf, sizeof(dist_buf), "%.2f", (double)miles);
    drawText("DISTANCE (MI)", 6, 66, 1);
    drawText(dist_buf, 6, 78, 3);

    /* --- Field 4: recent pace ------------------------------------------ */
    float pace;
    char pace_buf[16];
    if (track_pace_min_per_mile(&pace) && pace < 100.0f) {
        int pm = (int)pace;
        int ps = (int)((pace - pm) * 60.0f + 0.5f);
        if (ps >= 60) { ps -= 60; pm += 1; }
        snprintf(pace_buf, sizeof(pace_buf), "%d:%02d", pm, ps);
    } else {
        snprintf(pace_buf, sizeof(pace_buf), "--:--");
    }
    drawText("PACE (MIN/MI)", 6, 112, 1);
    drawText(pace_buf, 6, 124, 3);

    /* --- small status line: GPS fix quality / satellites --------------- */
    char status_buf[24];
    if (coords.valid) {
        snprintf(status_buf, sizeof(status_buf), "GPS FIX  SATS:%d",
                 coords.num_satellites);
    } else {
        snprintf(status_buf, sizeof(status_buf), "NO FIX  SATS:%d",
                 coords.num_satellites);
    }
    drawText(status_buf, 6, 156, 1);
}

/* ---- classic analog face (unchanged behaviour) ------------------------- */

void render_analog_watch_face() {
    LOG_INF("render analog watch face");
    clear_screen();
    drawText("FOSS-WATCH", 0, 0, 2);

    /* Snapshot of the GPS state (non-blocking; updated by the GPS thread). */
    gps_coords_t coords = gps_get_coords();

    int hours, minutes, seconds, milliseconds;
    if (coords.time_valid) {
        /* Drive the clock from the time the GPS reports. */
        int local_h = ((coords.utc_hours + GPS_TZ_OFFSET_HOURS) % 24 + 24) % 24;
        hours   = local_h % 12;            /* analog clock, 12-hour */
        minutes = coords.utc_minutes;
        seconds = coords.utc_seconds;
        milliseconds = seconds * 1000;     /* no sub-second info from GPS */
    } else {
        /* Fall back to uptime until the GPS gives us a time. */
        int64_t ms = k_uptime_get();       /* milliseconds since boot */
        hours   = (ms / 3600000) % 12;
        minutes = (ms / 60000) % 60;
        milliseconds = ms % 60000;
        seconds = milliseconds / 1000;
    }

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
    if (coords.valid) {
        drawText("Lat:", 0, 100, 1);
        drawFloat(coords.latitude, 5, 30, 100, 1);

        drawText("Lon:", 0, 110, 1);
        drawFloat(coords.longitude, 5, 30, 110, 1);
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
}

/* ---- dispatch ---------------------------------------------------------- */

void render_watch_screen() {
    /* Feed the movement tracker every tick, regardless of which face is shown,
     * so distance-since-boot stays accurate even while viewing the analog face. */
    gps_coords_t coords = gps_get_coords();
    track_update(coords.valid, coords.latitude, coords.longitude, k_uptime_get());

    switch (current_face) {
    case FACE_ANALOG:
        render_analog_watch_face();
        break;
    case FACE_DIGITAL:
    default:
        render_digital_watch_face();
        break;
    }
}
