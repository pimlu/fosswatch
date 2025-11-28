
#include "font.h"

uint64_t font[256] = {
    #include "font.txt"
};

 uint64_t digit_bmp[13] = {
    #include "digit_bmp.txt"
 };