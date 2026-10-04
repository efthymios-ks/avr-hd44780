#ifndef HD44780_CONFIG_H
#define HD44780_CONFIG_H

// Compile-time configuration for the HD44780 driver. Every setting may be
// overridden with -D or a project-wide config header passed via -include.

#ifndef HD44780_COLUMNS
#define HD44780_COLUMNS 20
#endif

#ifndef HD44780_ROWS
#define HD44780_ROWS 4
#endif

// Some 16x1 modules behave as 8+8 internally (DDRAM 0x00..0x07 then 0x40..0x47).
// Set to 1 for those; leave at 0 for the contiguous 0x00..0x0F variant.
#ifndef HD44780_16X1_SPLIT
#define HD44780_16X1_SPLIT 0
#endif

// Pin assignments. 4-bit mode only. Written as two tokens: port letter + bit.
#ifndef HD44780_PIN_RS
#define HD44780_PIN_RS B, 0
#endif

// HD44780_PIN_RW is optional. If undefined, library runs in write-only mode
// (RW tied to GND) and uses fixed delays instead of busy-flag polling. Common
// on breadboards and 3.3V setups.
// #define HD44780_PIN_RW B, 1

#ifndef HD44780_PIN_EN
#define HD44780_PIN_EN B, 2
#endif

#ifndef HD44780_PIN_D4
#define HD44780_PIN_D4 D, 4
#endif
#ifndef HD44780_PIN_D5
#define HD44780_PIN_D5 D, 5
#endif
#ifndef HD44780_PIN_D6
#define HD44780_PIN_D6 D, 6
#endif
#ifndef HD44780_PIN_D7
#define HD44780_PIN_D7 D, 7
#endif

// Optional fast path. When D4..D7 are four consecutive bits of one port,
// define HD44780_DATA_PORT to the port letter (B, C, D) and HD44780_DATA_SHIFT
// to the LSB bit index (0 for bits 0..3, 4 for bits 4..7). The driver then
// writes the whole nibble in a single read-modify-write (`in / andi / or / out`)
// instead of four `IO_WRITE` calls. The individual HD44780_PIN_D4..D7 defines
// are not used when the fast path is active.
//
// Example (D4..D7 on PD4..PD7, matching the default pin layout):
//   #define HD44780_DATA_PORT D
//   #define HD44780_DATA_SHIFT 4
//
// Leave undefined for the generic bit-at-a-time path.

#if (HD44780_ROWS != 1) && (HD44780_ROWS != 2) && (HD44780_ROWS != 4)
#error "HD44780_ROWS must be 1, 2, or 4"
#endif

#if defined(HD44780_DATA_PORT) && !defined(HD44780_DATA_SHIFT)
#error "HD44780_DATA_PORT requires HD44780_DATA_SHIFT (0 or 4)"
#endif

#endif
