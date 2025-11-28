#pragma once

#include <stdio.h>


#include <zephyr/drivers/spi.h>

#define SHARP_H 168
#define SHARP_W 144

# define M_PI 3.141592653589793238462643383279

#define SHARP_ROW_W (SHARP_W/8)
#define SHARP_PIXELS (SHARP_H * SHARP_ROW_W)

// https://www.sharpsde.com/fileadmin/products/Displays/2016_SDE_App_Note_for_Memory_LCD_programming_V1.3.pdf

#define CMD_LINE_W (1 + SHARP_ROW_W + 1)

#define CMD_ROW_W (1 + CMD_LINE_W + 1)
#define CMD_W (1 + SHARP_H * CMD_LINE_W + 1)

int spi_sharp_init();

/** Draws whats in the buffer  */
void sharp_draw();

/** Clears the existing screen */
void clear_screen();



extern uint8_t buffer[CMD_W];
/* Buffer is a 1 + CMD_LINE_W * SHARP_H + 1 length array
We have a control signal before and after the data. 

Each row also contains a control signal 
*/
static inline void setPixel(uint8_t x, uint8_t y, uint8_t bit);


/* Buffer is a 1 + CMD_LINE_W * SHARP_H + 1 length array
We have a control signal before and after the data. 

Each row also contains a control signal 
*/
static inline void setPixel(uint8_t x, uint8_t y, uint8_t bit) {
    if (x >= SHARP_W || y >= SHARP_H) return;
    uint16_t idx =  1+ CMD_LINE_W * y + 1 + (x >> 3);
    buffer[idx] |= (bit << (x & 7));
}