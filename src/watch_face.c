#include <stdint.h>
#include "watch_face.h"
#include "drivers/sharp.h"
#include "graphics.h"

void render_watch_screen(){
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
    
}