#ifndef HD44780_INTERNAL_H
#define HD44780_INTERNAL_H

// Internal API. Not for application code — subject to change without notice.
// Included only by src/hd44780.c and tests/test_hd44780.c.

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Begin capturing every nibble sent by send_nibble() into a caller-provided
// buffer. Each byte is encoded as: bit 7 = 1 for data, 0 for command;
// bits 3..0 = the nibble value. High nibble is sent first, then low nibble.
void hd44780_test_capture_begin(uint8_t *buffer, uint16_t capacity);

// Stop capturing and return the number of bytes written to the buffer.
uint16_t hd44780_test_capture_end(void);

#ifdef __cplusplus
}
#endif

#endif
