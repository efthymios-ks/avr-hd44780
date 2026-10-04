#include "hd44780.h"
#include "hd44780_config.h"
#include "hd44780_internal.h"
#include "io_macros.h"

#include <math.h>
#include <stddef.h>
#include <avr/io.h>
#include <util/delay.h>

// HD44780 command bits.
#define HD44780_CMD_CLEAR_DISPLAY   0x01
#define HD44780_CMD_RETURN_HOME     0x02
#define HD44780_CMD_ENTRY_MODE_SET  0x04
#define HD44780_CMD_DISPLAY_CTRL    0x08
#define HD44780_CMD_FUNCTION_SET    0x20
#define HD44780_CMD_SET_CGRAM_ADDR  0x40
#define HD44780_CMD_SET_DDRAM_ADDR  0x80

#define HD44780_ENTRY_INCREMENT     0x02
#define HD44780_ENTRY_NO_SHIFT      0x00

#define HD44780_DISPLAY_ON          0x04
#define HD44780_CURSOR_OFF          0x00
#define HD44780_BLINK_OFF           0x00

#define HD44780_MODE_8BIT           0x10
#define HD44780_MODE_4BIT           0x00
#define HD44780_LINES_2             0x08
#define HD44780_LINES_1             0x00
#define HD44780_DOTS_5X8            0x00

#define HD44780_BUSY_FLAG_BIT       7

// Pulse width for the EN line (microseconds). HD44780 requires >= 450 ns.
#define HD44780_PULSE_US            1u

// Fixed-delay fallback used when RW is not wired. Picks a conservative worst
// case that covers every command the library issues except Clear/Home.
#define HD44780_CMD_DELAY_US        50u
#define HD44780_CLEAR_DELAY_US      2000u

// Row start addresses in DDRAM, indexed by y.
static const uint8_t row_offsets[4] = {
#if HD44780_ROWS == 1
    0x00, 0x00, 0x00, 0x00
#elif HD44780_ROWS == 2
    0x00, 0x40, 0x00, 0x40
#else
#if HD44780_COLUMNS == 16
    0x00, 0x40, 0x10, 0x50
#else
    0x00, 0x40, (uint8_t)HD44780_COLUMNS, (uint8_t)(0x40 + HD44780_COLUMNS)
#endif
#endif
};

// Test capture state. Only used when hd44780_test_capture_begin() is active.
static uint8_t *capture_buffer = NULL;
static uint16_t capture_capacity = 0;
static uint16_t capture_count = 0;

static void pulse_enable(void)
{
    IO_WRITE(HD44780_PIN_EN, IO_HIGH);
    _delay_us(HD44780_PULSE_US);
    IO_WRITE(HD44780_PIN_EN, IO_LOW);
    _delay_us(HD44780_PULSE_US);
}

static void drive_nibble(uint8_t nibble)
{
#ifdef HD44780_DATA_PORT
    // Fast path when D4..D7 live on four consecutive bits of one port.
    // Writes the whole nibble in a single read-modify-write, which the
    // compiler turns into `in / andi / or / out` on AVR.
    uint8_t mask = (uint8_t)(0x0F << HD44780_DATA_SHIFT);
    IO_PORT_(HD44780_DATA_PORT) = (uint8_t)((IO_PORT_(HD44780_DATA_PORT) & (uint8_t)~mask) | (uint8_t)((nibble & 0x0F) << HD44780_DATA_SHIFT));
#else
    IO_WRITE(HD44780_PIN_D4, (nibble & 0x01) ? IO_HIGH : IO_LOW);
    IO_WRITE(HD44780_PIN_D5, (nibble & 0x02) ? IO_HIGH : IO_LOW);
    IO_WRITE(HD44780_PIN_D6, (nibble & 0x04) ? IO_HIGH : IO_LOW);
    IO_WRITE(HD44780_PIN_D7, (nibble & 0x08) ? IO_HIGH : IO_LOW);
#endif
}

// Capture one nibble when the test seam is active. Byte layout:
// bit 7 = 1 for data, 0 for command. Low 4 bits = the nibble value.
static void capture_record(uint8_t nibble, bool is_data)
{
    if (capture_buffer == NULL || capture_count >= capture_capacity) {
        return;
    }
    uint8_t encoded = (uint8_t)(nibble & 0x0F);
    if (is_data) {
        encoded |= 0x80;
    }
    capture_buffer[capture_count++] = encoded;
}

// Send a single 4-bit nibble, with RS set per is_data.
// Does not touch the busy flag — callers are responsible for sequencing.
static void send_nibble(uint8_t nibble, bool is_data)
{
    IO_WRITE(HD44780_PIN_RS, is_data ? IO_HIGH : IO_LOW);
    drive_nibble(nibble);
    pulse_enable();
    capture_record(nibble, is_data);
}

#ifdef HD44780_PIN_RW
static void wait_busy(void)
{
    // Switch D4..D7 to inputs and read the busy flag on D7 (high nibble).
    IO_MODE(HD44780_PIN_D4, IO_INPUT);
    IO_MODE(HD44780_PIN_D5, IO_INPUT);
    IO_MODE(HD44780_PIN_D6, IO_INPUT);
    IO_MODE(HD44780_PIN_D7, IO_INPUT);
    IO_WRITE(HD44780_PIN_RS, IO_LOW);
    IO_WRITE(HD44780_PIN_RW, IO_HIGH);

    uint8_t busy;
    do {
        IO_WRITE(HD44780_PIN_EN, IO_HIGH);
        _delay_us(HD44780_PULSE_US);
        busy = IO_READ(HD44780_PIN_D7) ? 1u : 0u;
        IO_WRITE(HD44780_PIN_EN, IO_LOW);
        _delay_us(HD44780_PULSE_US);

        // Still have to clock the low nibble out even though we ignore it.
        IO_WRITE(HD44780_PIN_EN, IO_HIGH);
        _delay_us(HD44780_PULSE_US);
        IO_WRITE(HD44780_PIN_EN, IO_LOW);
        _delay_us(HD44780_PULSE_US);
    } while (busy);

    IO_MODE(HD44780_PIN_D4, IO_OUTPUT);
    IO_MODE(HD44780_PIN_D5, IO_OUTPUT);
    IO_MODE(HD44780_PIN_D6, IO_OUTPUT);
    IO_MODE(HD44780_PIN_D7, IO_OUTPUT);
    IO_WRITE(HD44780_PIN_RW, IO_LOW);
}

static uint8_t read_address(void)
{
    IO_MODE(HD44780_PIN_D4, IO_INPUT);
    IO_MODE(HD44780_PIN_D5, IO_INPUT);
    IO_MODE(HD44780_PIN_D6, IO_INPUT);
    IO_MODE(HD44780_PIN_D7, IO_INPUT);
    IO_WRITE(HD44780_PIN_RS, IO_LOW);
    IO_WRITE(HD44780_PIN_RW, IO_HIGH);

    uint8_t value = 0;
    IO_WRITE(HD44780_PIN_EN, IO_HIGH);
    _delay_us(HD44780_PULSE_US);
    value |= (uint8_t)(IO_READ(HD44780_PIN_D4) ? 0x10u : 0u);
    value |= (uint8_t)(IO_READ(HD44780_PIN_D5) ? 0x20u : 0u);
    value |= (uint8_t)(IO_READ(HD44780_PIN_D6) ? 0x40u : 0u);
    IO_WRITE(HD44780_PIN_EN, IO_LOW);
    _delay_us(HD44780_PULSE_US);

    IO_WRITE(HD44780_PIN_EN, IO_HIGH);
    _delay_us(HD44780_PULSE_US);
    value |= (uint8_t)(IO_READ(HD44780_PIN_D4) ? 0x01u : 0u);
    value |= (uint8_t)(IO_READ(HD44780_PIN_D5) ? 0x02u : 0u);
    value |= (uint8_t)(IO_READ(HD44780_PIN_D6) ? 0x04u : 0u);
    value |= (uint8_t)(IO_READ(HD44780_PIN_D7) ? 0x08u : 0u);
    IO_WRITE(HD44780_PIN_EN, IO_LOW);
    _delay_us(HD44780_PULSE_US);

    IO_MODE(HD44780_PIN_D4, IO_OUTPUT);
    IO_MODE(HD44780_PIN_D5, IO_OUTPUT);
    IO_MODE(HD44780_PIN_D6, IO_OUTPUT);
    IO_MODE(HD44780_PIN_D7, IO_OUTPUT);
    IO_WRITE(HD44780_PIN_RW, IO_LOW);
    return value;
}
#else
static void wait_busy(void)
{
    _delay_us(HD44780_CMD_DELAY_US);
}
#endif

static void send_byte(uint8_t value, bool is_data)
{
    send_nibble((uint8_t)(value >> 4), is_data);
    send_nibble((uint8_t)(value & 0x0F), is_data);
}

void hd44780_init(void)
{
    IO_MODE(HD44780_PIN_RS, IO_OUTPUT);
#ifdef HD44780_PIN_RW
    IO_MODE(HD44780_PIN_RW, IO_OUTPUT);
    IO_WRITE(HD44780_PIN_RW, IO_LOW);
#endif
    IO_MODE(HD44780_PIN_EN, IO_OUTPUT);
#ifdef HD44780_DATA_PORT
    // Fast path: set the four D4..D7 bits as outputs in one DDR write.
    IO_DDR_(HD44780_DATA_PORT) |= (uint8_t)(0x0F << HD44780_DATA_SHIFT);
#else
    IO_MODE(HD44780_PIN_D4, IO_OUTPUT);
    IO_MODE(HD44780_PIN_D5, IO_OUTPUT);
    IO_MODE(HD44780_PIN_D6, IO_OUTPUT);
    IO_MODE(HD44780_PIN_D7, IO_OUTPUT);
#endif

    IO_WRITE(HD44780_PIN_RS, IO_LOW);
    IO_WRITE(HD44780_PIN_EN, IO_LOW);
    drive_nibble(0x00);

    // HD44780 spec: wait > 40 ms after VCC crosses 2.7 V.
    _delay_ms(50);

    // Three "function set to 8-bit" nibbles bring the controller to a known
    // state regardless of what mode it powered up in.
    send_nibble(0x03, false);
    _delay_ms(5);
    send_nibble(0x03, false);
    _delay_us(150);
    send_nibble(0x03, false);
    _delay_us(150);

    // Switch to 4-bit mode. From here on, bytes are sent as two nibbles.
    send_nibble(0x02, false);
    _delay_us(150);

#if HD44780_ROWS == 1
    uint8_t lines = HD44780_LINES_1;
#else
    uint8_t lines = HD44780_LINES_2;
#endif
    hd44780_send_command((uint8_t)(HD44780_CMD_FUNCTION_SET | HD44780_MODE_4BIT | lines | HD44780_DOTS_5X8));
    hd44780_send_command((uint8_t)(HD44780_CMD_DISPLAY_CTRL | HD44780_DISPLAY_ON | HD44780_CURSOR_OFF | HD44780_BLINK_OFF));
    hd44780_send_command((uint8_t)(HD44780_CMD_ENTRY_MODE_SET | HD44780_ENTRY_INCREMENT | HD44780_ENTRY_NO_SHIFT));
    hd44780_clear();
}

void hd44780_send_command(uint8_t cmd)
{
    wait_busy();
    send_byte(cmd, false);
#ifndef HD44780_PIN_RW
    if (cmd == HD44780_CMD_CLEAR_DISPLAY || cmd == HD44780_CMD_RETURN_HOME) {
        _delay_us(HD44780_CLEAR_DELAY_US);
    }
#endif
}

void hd44780_send_data(uint8_t data)
{
    wait_busy();
    send_byte(data, true);
}

void hd44780_clear(void)
{
    // The send_command path handles the 1.52 ms clear delay via wait_busy()
    // when RW is wired, or the extended fixed delay when it is not.
    hd44780_send_command(HD44780_CMD_CLEAR_DISPLAY);
}

void hd44780_clear_line(uint8_t row)
{
    if (row >= HD44780_ROWS) {
        return;
    }
    hd44780_goto(0, row);
    for (uint8_t i = 0; i < HD44780_COLUMNS; i++) {
        hd44780_send_data(' ');
    }
    hd44780_goto(0, row);
}

void hd44780_goto(uint8_t x, uint8_t y)
{
    if (x >= HD44780_COLUMNS || y >= HD44780_ROWS) {
        return;
    }
    uint8_t addr = row_offsets[y];
#if HD44780_COLUMNS == 16 && HD44780_ROWS == 1 && HD44780_16X1_SPLIT
    if (x >= 8) {
        addr = 0x40;
        x = (uint8_t)(x - 8);
    }
#endif
    addr = (uint8_t)(addr + x);
    hd44780_send_command((uint8_t)(HD44780_CMD_SET_DDRAM_ADDR | addr));
}

void hd44780_get_position(uint8_t *x, uint8_t *y)
{
    if (x == NULL || y == NULL) {
        return;
    }
#ifdef HD44780_PIN_RW
    wait_busy();
    uint8_t addr = read_address();
    uint8_t rx = 0, ry = 0;
#if HD44780_ROWS == 1
    rx = (uint8_t)(addr & 0x7F);
#if HD44780_COLUMNS == 16 && HD44780_16X1_SPLIT
    if (rx >= 0x40) {
        rx = (uint8_t)(rx - 0x40 + 8);
    }
#endif
#elif HD44780_ROWS == 2
    if (addr >= 0x40) {
        rx = (uint8_t)(addr - 0x40);
        ry = 1;
    } else {
        rx = addr;
    }
#else
    // Four-row layouts.
#if HD44780_COLUMNS == 16
    uint8_t base_r2 = 0x10;
    uint8_t base_r3 = 0x50;
#else
    uint8_t base_r2 = (uint8_t)HD44780_COLUMNS;
    uint8_t base_r3 = (uint8_t)(0x40 + HD44780_COLUMNS);
#endif
    if (addr >= base_r3) {
        rx = (uint8_t)(addr - base_r3);
        ry = 3;
    } else if (addr >= 0x40) {
        rx = (uint8_t)(addr - 0x40);
        ry = 1;
    } else if (addr >= base_r2) {
        rx = (uint8_t)(addr - base_r2);
        ry = 2;
    } else {
        rx = addr;
    }
#endif
    *x = rx;
    *y = ry;
#else
    // No RW line, cannot read back from the controller.
    *x = 0;
    *y = 0;
#endif
}

void hd44780_create_char(uint8_t index, const uint8_t pattern[8])
{
    if (index >= 8 || pattern == NULL) {
        return;
    }
    hd44780_send_command((uint8_t)(HD44780_CMD_SET_CGRAM_ADDR | (uint8_t)(index << 3)));
    for (uint8_t i = 0; i < 8; i++) {
        hd44780_send_data(pattern[i]);
    }
    hd44780_send_command((uint8_t)(HD44780_CMD_SET_DDRAM_ADDR | 0x00));
}

void hd44780_create_char_p(uint8_t index, const uint8_t *pattern_pgm)
{
    if (index >= 8 || pattern_pgm == NULL) {
        return;
    }
    hd44780_send_command((uint8_t)(HD44780_CMD_SET_CGRAM_ADDR | (uint8_t)(index << 3)));
    for (uint8_t i = 0; i < 8; i++) {
        hd44780_send_data(pgm_read_byte(&pattern_pgm[i]));
    }
    hd44780_send_command((uint8_t)(HD44780_CMD_SET_DDRAM_ADDR | 0x00));
}

void hd44780_print_char(char c)
{
    hd44780_send_data((uint8_t)c);
}

void hd44780_print_string(const char *s)
{
    if (s == NULL) {
        return;
    }
    while (*s) {
        hd44780_send_data((uint8_t)*s++);
    }
}

void hd44780_print_string_p(const char *s_pgm)
{
    if (s_pgm == NULL) {
        return;
    }
    uint8_t c = pgm_read_byte(s_pgm);
    while (c) {
        hd44780_send_data(c);
        s_pgm++;
        c = pgm_read_byte(s_pgm);
    }
}

// Convert a signed 32-bit value to its base-10 ASCII representation. Handles
// INT32_MIN safely by negating in the unsigned domain, where -(uint32_t)x is
// well-defined for every value including INT32_MIN. Negating INT32_MIN as a
// signed int32 is undefined behaviour and was the X2 bug in v1.
static void int32_to_str(int32_t value, char *out)
{
    uint32_t uval;
    char *p = out;

    if (value < 0) {
        *p++ = '-';
        uval = (uint32_t)0 - (uint32_t)value;
    } else {
        uval = (uint32_t)value;
    }

    // Build digits in reverse into a temporary scratch area.
    char scratch[11];
    uint8_t len = 0;
    if (uval == 0u) {
        scratch[len++] = '0';
    } else {
        while (uval > 0u) {
            scratch[len++] = (char)('0' + (uval % 10u));
            uval /= 10u;
        }
    }

    while (len > 0u) {
        *p++ = scratch[--len];
    }
    *p = '\0';
}

void hd44780_print_int(int32_t value)
{
    // int32 range plus sign and NUL: 12 bytes.
    char buffer[12];
    int32_to_str(value, buffer);
    hd44780_print_string(buffer);
}

void hd44780_print_float(float value, uint8_t decimals)
{
    if (isnan(value)) {
        hd44780_print_string("nan");
        return;
    }
    if (isinf(value)) {
        if (value < 0) {
            hd44780_print_char('-');
        }
        hd44780_print_string("inf");
        return;
    }

    if (value < 0.0f) {
        hd44780_print_char('-');
        value = -value;
    }

    // Scale to integer with rounding to avoid 0.3 -> "0.2" style truncation.
    uint32_t scale = 1;
    for (uint8_t i = 0; i < decimals; i++) {
        scale *= 10u;
    }
    float scaled = value * (float)scale + 0.5f;
    uint32_t rounded = (uint32_t)scaled;

    uint32_t int_part = rounded / scale;
    uint32_t frac_part = rounded % scale;

    hd44780_print_int((int32_t)int_part);

    if (decimals == 0) {
        return;
    }

    hd44780_print_char('.');

    // Emit leading zeros of the fractional part before printing the number.
    // Without this, 1.05 with decimals=2 would print as "1.5".
    uint32_t divisor = scale / 10u;
    while (divisor > frac_part && divisor > 1u) {
        hd44780_print_char('0');
        divisor /= 10u;
    }
    if (frac_part > 0u) {
        hd44780_print_int((int32_t)frac_part);
    } else {
        hd44780_print_char('0');
    }
}

void hd44780_test_capture_begin(uint8_t *buffer, uint16_t capacity)
{
    capture_buffer = buffer;
    capture_capacity = capacity;
    capture_count = 0;
}

uint16_t hd44780_test_capture_end(void)
{
    uint16_t count = capture_count;
    capture_buffer = NULL;
    capture_capacity = 0;
    capture_count = 0;
    return count;
}
