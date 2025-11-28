#pragma once

#include <stdint.h>

#define SPACE 11
#define COLON 10
#define POINT 12

#define FONT_WIDTH 5
#define FONT_HEIGHT 8

extern uint64_t font[256];

// These have to be written backwards manually
extern uint64_t digit_bmp[13];