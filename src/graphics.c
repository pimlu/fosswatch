#include "graphics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "font.h"
#include "drivers/sharp.h"


/* Bresenham line draw */
void drawLine(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    for (int i = 0; ; i++) {
        setPixel(x0, y0, 1);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/*
Draws a digit 0 through 9, or :, . or space at the specified coords;
*/

void drawDigit(int x0, int y0, uint8_t digit, uint8_t scale){
    for(int i = 0; i< FONT_HEIGHT * scale; i++){
        uint8_t real_i = i / scale;
        for(int j = 0; j< FONT_WIDTH* scale; j++){
            uint8_t real_j = j / scale;
            uint8_t idx = real_i * FONT_WIDTH + real_j;
            uint8_t bit = (digit_bmp[digit] & (1ll << idx)) >> idx;
            setPixel(x0 + j, y0 + i, bit);
        }
    }
}

/*
Draws a character athe specified coords;
*/

void drawChar(int x0, int y0, char ch, uint8_t scale){
    // Handle characters that might not be in font - show as space or placeholder
    // NMEA sentences use: $, *, comma, period, letters, numbers
    // If font[ch] is 0 (empty), it means the character isn't defined
    // For now, we'll try to render it anyway - if it's empty, nothing will show
    
    for(int i = 0; i< FONT_HEIGHT * scale; i++){
        uint8_t real_i = i / scale;
        for(int j = 0; j< FONT_WIDTH* scale; j++){
            uint8_t real_j = j / scale;
            uint8_t idx = real_i * FONT_WIDTH + real_j;
            uint8_t bit = (font[ch] & (1ll << idx)) >> idx;
            setPixel(x0 + j, y0 + i, bit);
        }
    }
}

void drawTime( uint8_t hh, uint8_t mm, uint8_t ss, uint8_t x0, uint8_t y0, uint8_t scale){
    uint8_t out[] = {0, 0, COLON, 0, 0, COLON, 0, 0};
    out[0] = hh / 10;
    out[1] = hh % 10;
    out[3] = mm / 10;
    out[4] = mm % 10;
    out[6] = ss / 10;
    out[7] = ss % 10;
    for(int i = 0; i< 8; i++){
        drawDigit(x0 + i * ( FONT_WIDTH + 1) * scale, y0, out[i], scale);
    }

}

void drawText(char* str, uint8_t x0, uint8_t y0, uint8_t scale){

    for(int i = 0 ; str[i]; i++){
        drawChar(x0 + i * ( FONT_WIDTH + 1) * scale, y0, str[i], scale);
    }

}

void drawFloat(float value, int decimals, uint8_t x0, uint8_t y0, uint8_t scale) {
    // Handle negative numbers
    if (value < 0) {
        drawChar(x0, y0, '-', scale);
        x0 += (FONT_WIDTH + 1) * scale;
        value = -value;
    }
    
    // Extract integer and fractional parts
    int int_part = (int)value;
    float frac_part = value - int_part;
    
    // Print integer part
    char int_str[16];
    snprintf(int_str, sizeof(int_str), "%d", int_part);
    drawText(int_str, x0, y0, scale);
    x0 += strlen(int_str) * (FONT_WIDTH + 1) * scale;
    
    // Print decimal point
    if (decimals > 0) {
        drawDigit(x0, y0, POINT, scale);
        x0 += (FONT_WIDTH + 1) * scale;
        
        // Print fractional part
        for (int i = 0; i < decimals; i++) {
            frac_part *= 10;
            int digit = (int)frac_part % 10;
            drawDigit(x0, y0, digit, scale);
            x0 += (FONT_WIDTH + 1) * scale;
        }
    }
}