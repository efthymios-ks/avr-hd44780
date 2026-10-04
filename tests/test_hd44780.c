#include "unity.h"
#include "avr/io.h"
#include "hd44780.h"
#include "hd44780_config.h"
#include "hd44780_internal.h"

void setUp(void) {}
void tearDown(void) {}


#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>

// Captured nibble encoding: bit 7 = 1 for data, 0 for command.
// Low 4 bits = nibble value.

#define BUFSZ 512
static uint8_t capture[BUFSZ];

static void reset_state(void)
{
    fake_io_reset();
    memset(capture, 0, sizeof(capture));
    hd44780_test_capture_begin(capture, BUFSZ);
}

static uint16_t end_capture(void)
{
    return hd44780_test_capture_end();
}

// Reconstruct a byte from two consecutive nibbles starting at index i.
// Returns the full byte and advances the caller's index.
static uint8_t byte_from(uint16_t i, bool *is_data)
{
    uint8_t hi = capture[i];
    uint8_t lo = capture[i + 1];
    if (is_data) {
        *is_data = (hi & 0x80) != 0;
    }
    return (uint8_t)(((hi & 0x0F) << 4) | (lo & 0x0F));
}

// Discard the init prologue: 3 individual command nibbles (0x03) + 1 nibble
// (0x02) switching to 4-bit, then three full command bytes (function set,
// display control, entry mode) and the clear command. Returns the byte index
// after the init sequence.
static uint16_t skip_init(uint16_t count)
{
    (void)count;
    // 4 single nibbles + 4 full bytes (function set, display ctrl, entry mode, clear)
    return 4u + 4u * 2u;
}

static void init_should_emit_full_sequence(void)
{
    reset_state();
    hd44780_init();
    uint16_t count = end_capture();

    // 4 single nibbles + 4 bytes * 2 nibbles = 12 captures.
    TEST_ASSERT_EQUAL_UINT16(12, count);

    // First four captures are command nibbles 0x03, 0x03, 0x03, 0x02.
    TEST_ASSERT_EQUAL_UINT8(0x03, capture[0]);
    TEST_ASSERT_EQUAL_UINT8(0x03, capture[1]);
    TEST_ASSERT_EQUAL_UINT8(0x03, capture[2]);
    TEST_ASSERT_EQUAL_UINT8(0x02, capture[3]);

    // Function set: 4-bit, 2 lines, 5x8 dots -> 0x28.
    bool is_data;
    TEST_ASSERT_EQUAL_UINT8(0x28, byte_from(4, &is_data));
    TEST_ASSERT_FALSE(is_data);
    // Display control: display on, cursor off, blink off -> 0x0C.
    TEST_ASSERT_EQUAL_UINT8(0x0C, byte_from(6, &is_data));
    TEST_ASSERT_FALSE(is_data);
    // Entry mode: increment, no shift -> 0x06.
    TEST_ASSERT_EQUAL_UINT8(0x06, byte_from(8, &is_data));
    TEST_ASSERT_FALSE(is_data);
    // Clear display -> 0x01.
    TEST_ASSERT_EQUAL_UINT8(0x01, byte_from(10, &is_data));
    TEST_ASSERT_FALSE(is_data);
}

static void goto_row0_should_emit_0x80(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_goto(0, 0);
    (void)end_capture();
    bool is_data;
    TEST_ASSERT_EQUAL_UINT8(0x80, byte_from(base, &is_data));
    TEST_ASSERT_FALSE(is_data);
}

static void goto_row1_should_emit_0xC0(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_goto(0, 1);
    (void)end_capture();
    bool is_data;
    TEST_ASSERT_EQUAL_UINT8(0xC0, byte_from(base, &is_data));
    TEST_ASSERT_FALSE(is_data);
}

static void goto_row2_should_emit_0x94_for_20x4(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_goto(0, 2);
    (void)end_capture();
    bool is_data;
    // 20x4: row 2 starts at 0x14 -> command 0x80 | 0x14 = 0x94.
    TEST_ASSERT_EQUAL_UINT8(0x94, byte_from(base, &is_data));
    TEST_ASSERT_FALSE(is_data);
}

static void goto_row3_should_emit_0xD4_for_20x4(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_goto(0, 3);
    (void)end_capture();
    bool is_data;
    // 20x4: row 3 starts at 0x54 -> command 0x80 | 0x54 = 0xD4.
    TEST_ASSERT_EQUAL_UINT8(0xD4, byte_from(base, &is_data));
    TEST_ASSERT_FALSE(is_data);
}

// Pull printed ASCII back out of the capture buffer starting at byte index.
static void collect_printed(uint16_t start, uint16_t count, char *out, size_t out_sz)
{
    size_t pos = 0;
    for (uint16_t i = start; i + 1 < count && pos + 1 < out_sz; i += 2) {
        bool is_data = (capture[i] & 0x80) != 0;
        if (!is_data) {
            continue;
        }
        uint8_t byte = (uint8_t)(((capture[i] & 0x0F) << 4) | (capture[i + 1] & 0x0F));
        out[pos++] = (char)byte;
    }
    out[pos] = '\0';
}

static void print_int_zero_should_be_0(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_print_int(0);
    uint16_t count = end_capture();
    char out[32];
    collect_printed(base, count, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("0", out);
}

static void print_int_negative_one_should_be_neg1(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_print_int(-1);
    uint16_t count = end_capture();
    char out[32];
    collect_printed(base, count, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("-1", out);
}

static void print_int_int32_max(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_print_int(INT32_MAX);
    uint16_t count = end_capture();
    char out[32];
    collect_printed(base, count, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("2147483647", out);
}

// Regression against the X2 Int2bcd bug: INT32_MIN used to print nothing
// because the sign-flip step overflowed.
static void print_int_int32_min(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_print_int(INT32_MIN);
    uint16_t count = end_capture();
    char out[32];
    collect_printed(base, count, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("-2147483648", out);
}

// Regression against the X1 PrintDouble leading-zero drop bug.
// 1.05 with 2 decimals previously printed "1.5".
static void print_float_leading_zero_in_fraction(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_print_float(1.05f, 2);
    uint16_t count = end_capture();
    char out[32];
    collect_printed(base, count, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("1.05", out);
}

// Regression against the X1 truncation bug: 0.3 used to print "0.2".
static void print_float_rounding_near_boundary(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_print_float(0.3f, 1);
    uint16_t count = end_capture();
    char out[32];
    collect_printed(base, count, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("0.3", out);
}

static void print_float_negative_fractional(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_print_float(-0.5f, 1);
    uint16_t count = end_capture();
    char out[32];
    collect_printed(base, count, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("-0.5", out);
}

// Regression against the LCD_ClearLine off-by-one: must write exactly
// HD44780_COLUMNS spaces, not COLUMNS+1, and finish with a goto back to (0, row).
static void clear_line_writes_columns_spaces_then_resets_position(void)
{
    reset_state();
    hd44780_init();
    uint16_t base = skip_init(0);
    hd44780_clear_line(0);
    uint16_t count = end_capture();

    // Expected traffic:
    //   1 command byte (goto(0,0))
    // + HD44780_COLUMNS data bytes (space)
    // + 1 command byte (goto(0,0))
    uint16_t expected_bytes = 2u + (uint16_t)HD44780_COLUMNS;
    uint16_t traffic = (uint16_t)(count - base);
    TEST_ASSERT_EQUAL_UINT16(expected_bytes * 2u, traffic);

    bool is_data;
    // First: goto(0, 0) -> 0x80.
    TEST_ASSERT_EQUAL_UINT8(0x80, byte_from(base, &is_data));
    TEST_ASSERT_FALSE(is_data);

    // Next HD44780_COLUMNS entries are data bytes with value ' '.
    uint16_t cursor = (uint16_t)(base + 2u);
    for (uint8_t i = 0; i < HD44780_COLUMNS; i++) {
        TEST_ASSERT_EQUAL_UINT8((uint8_t)' ', byte_from(cursor, &is_data));
        TEST_ASSERT_TRUE(is_data);
        cursor = (uint16_t)(cursor + 2u);
    }

    // Final: goto(0, 0) again.
    TEST_ASSERT_EQUAL_UINT8(0x80, byte_from(cursor, &is_data));
    TEST_ASSERT_FALSE(is_data);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(init_should_emit_full_sequence);
    RUN_TEST(goto_row0_should_emit_0x80);
    RUN_TEST(goto_row1_should_emit_0xC0);
    RUN_TEST(goto_row2_should_emit_0x94_for_20x4);
    RUN_TEST(goto_row3_should_emit_0xD4_for_20x4);
    RUN_TEST(print_int_zero_should_be_0);
    RUN_TEST(print_int_negative_one_should_be_neg1);
    RUN_TEST(print_int_int32_max);
    RUN_TEST(print_int_int32_min);
    RUN_TEST(print_float_leading_zero_in_fraction);
    RUN_TEST(print_float_rounding_near_boundary);
    RUN_TEST(print_float_negative_fractional);
    RUN_TEST(clear_line_writes_columns_spaces_then_resets_position);
    return UNITY_END();
}
