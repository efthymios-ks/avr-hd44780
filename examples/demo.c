#include "hd44780.h"
#include "hd44780_config.h"

#include <avr/pgmspace.h>

// A small "arrow" glyph stored in flash to exercise hd44780_create_char_p.
static const uint8_t arrow_glyph[8] PROGMEM = {
    0b00000,
    0b00100,
    0b00110,
    0b11111,
    0b11111,
    0b00110,
    0b00100,
    0b00000,
};

int main(void)
{
    hd44780_init();

    hd44780_create_char_p(0, arrow_glyph);

    hd44780_goto(0, 0);
    hd44780_print_string("avr-hd44780 v2");

#if HD44780_ROWS >= 2
    hd44780_goto(0, 1);
    hd44780_print_int(-12345);
#endif

#if HD44780_ROWS >= 4
    hd44780_goto(0, 2);
    // Regression against the X1 leading-zero-drop bug: expect "1.05", not "1.5".
    hd44780_print_float(1.05f, 2);

    hd44780_goto(0, 3);
    for (uint8_t i = 0; i < HD44780_COLUMNS; i++) {
        hd44780_print_char((char)0);
    }
#endif

    while (1) {
    }
    return 0;
}
