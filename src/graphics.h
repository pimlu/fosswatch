#pragma once
#include <stdint.h>



/* Bresenham line draw */
void drawLine(int x0, int y0, int x1, int y1);

/*
Draws a digit 0 through 9, or :, . or space at the specified coords;
*/

void drawDigit(int x0, int y0, uint8_t digit, uint8_t scale);

void drawTime( uint8_t hh, uint8_t mm, uint8_t ss, uint8_t x0, uint8_t y0, uint8_t scale);

void drawText(char* str, uint8_t x0, uint8_t x1, uint8_t scale);

void drawChar(int x0, int y0, char ch, uint8_t scale);