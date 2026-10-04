#ifndef HD44780_H
#define HD44780_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <avr/pgmspace.h>

// Public API. 4-bit mode HD44780 character LCD driver.
// Configure pins and geometry via src/hd44780_config.h (or -D overrides).

void hd44780_init(void);
void hd44780_send_command(uint8_t cmd);
void hd44780_send_data(uint8_t data);
void hd44780_clear(void);
void hd44780_clear_line(uint8_t row);
void hd44780_goto(uint8_t x, uint8_t y);
void hd44780_get_position(uint8_t *x, uint8_t *y);
void hd44780_create_char(uint8_t index, const uint8_t pattern[8]);
void hd44780_create_char_p(uint8_t index, const uint8_t *pattern_pgm);
void hd44780_print_char(char c);
void hd44780_print_string(const char *s);
void hd44780_print_string_p(const char *s_pgm);
void hd44780_print_int(int32_t value);
void hd44780_print_float(float value, uint8_t decimals);

#ifdef __cplusplus
}
#endif

#endif
