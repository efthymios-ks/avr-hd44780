# Changelog

## v2.0.0 — 2026-10-04

Complete rewrite.  
Every public symbol is renamed and the configuration macros are redesigned,  
so this release is a hard break with v1.  
Portfolio project, no backward-compatibility shims.  
See the migration plan §3.5 and §8.6.

### Renames

| v1                                                          | v2                                                                      |
|-------------------------------------------------------------|-------------------------------------------------------------------------|
| `LCD_Setup()`                                               | `hd44780_init()`                                                        |
| `LCD_SendCommand() / LCD_SendData()`                        | `hd44780_send_command() / hd44780_send_data()`                          |
| `LCD_Clear() / LCD_ClearLine()`                             | `hd44780_clear() / hd44780_clear_line()`                                |
| `LCD_GotoXY()`                                              | `hd44780_goto()`                                                        |
| `LCD_GetP() / LCD_GetX() / LCD_GetY()`                      | `hd44780_get_position(&x, &y)`                                          |
| `LCD_BuildChar() / LCD_BuildChar_P()`                       | `hd44780_create_char() / hd44780_create_char_p()`                       |
| `LCD_PrintChar() / LCD_PrintString() / LCD_PrintString_P()` | `hd44780_print_char() / hd44780_print_string() / hd44780_print_string_p()` |
| `LCD_PrintInteger()`                                        | `hd44780_print_int()`                                                   |
| `LCD_PrintDouble(v, tens)`                                  | `hd44780_print_float(v, decimals)`                                      |
| `LCD_WaitBusy()`                                            | internal `static wait_busy()`                                           |
| `LCD_Size 1604`, `LCD_Type A`                               | `HD44780_COLUMNS 16`, `HD44780_ROWS 4`, `HD44780_16X1_SPLIT 0/1`        |
| `LCD_D4..LCD_D7`, `LCD_RS`, `LCD_RW`, `LCD_EN`              | `HD44780_PIN_D4..D7`, `HD44780_PIN_RS`, `HD44780_PIN_RW` (optional), `HD44780_PIN_EN` |

### Fixes

- **X1 / plan §7:** `hd44780_print_float()` now preserves leading zeros in the fractional part.  
  `1.05f, 2` prints `"1.05"` (used to print `"1.5"`),  
  and `0.3f, 1` prints `"0.3"` (used to print `"0.2"` due to float truncation).  
  The new implementation scales to an integer with rounding and emits leading zeros explicitly.
- **X2 / plan §7:** `hd44780_print_int(INT32_MIN)` prints `"-2147483648"`.  
  The v1 `Int2bcd` negated the value as a signed int32 and overflowed,  
  printing nothing at all.
- **plan §8.6 bug 1:** `hd44780_create_char_p()` now reads from flash via `pgm_read_byte(&pattern[i])`.  
  The v1 `LCD_BuildChar_P` passed the value, not the address,  
  so custom characters loaded from PROGMEM were garbage.
- **plan §8.6 bug 2:** The `LCD_Type A` / `LCD_Type B` setup was broken because neither identifier was ever `#define`d.  
  `LCD_Type == B` evaluated as `0 == 0` and 16x1 Type A was unreachable.  
  Replaced with an explicit `HD44780_16X1_SPLIT` flag (0 or 1).
- **plan §8.6 bug 3:** `hd44780_clear_line()` writes exactly `HD44780_COLUMNS` spaces (v1 wrote `COLUMNS + 1`).
- **plan §8.6 bug 4:** The `Int2bcd > 1000000000` loop boundary caused 1,000,000,000 to print as `":"`.  
  The replacement never has this class of bug.
- **plan §8.6 bug 7:** Power-on wait is now 50 ms.  
  The HD44780 datasheet requires more than 40 ms after VCC rises;  
  v1 waited 20 ms.
- **plan §8.6 bug 8:** Removed the `if (Position < 0)` dead check that was always false against the `uint8_t` parameter.
- **plan §8.6 bug 9:** `function set` honours `HD44780_ROWS == 1`.  
  v1 always set the two-line bit,  
  which broke proper 1-line modules.
- **plan §8.6 bug 10:** `hd44780_print_string()` takes `const char *` so callers can pass string literals without casting away `const`.

### Design

- Single `send_nibble()` shared by command, data, and init paths.
- Optional `HD44780_PIN_RW`.  
  If undefined, the driver runs write-only with fixed delays —  
  common on breadboards.
- Row start addresses computed from `HD44780_COLUMNS` for non-16-column four-row displays (e.g. 20x4).
- Portable integer-to-string helper, so the library behaves identically on the host test target (glibc) and on avr-libc.
- Test seam via `src/hd44780_internal.h`:  
  tests call `hd44780_test_capture_begin()` to record every nibble the driver sends.

### Project changes

- Repo renamed `AVR-HD44780` -> `avr-hd44780`.
- Restructured to `src/ examples/ tests/ sim/ docs/ scripts/` (plan §4).
- Added `Build.ps1`, `Simulate.ps1`, `scripts/Common.psm1` for Windows + Linux build/test automation.  
  `Build.ps1` auto-installs the toolchain.
- Added host-side Unity tests under `tests/` with fake AVR registers and a capture buffer that records every nibble emitted by the driver.
- Added GitHub Actions CI running the full build matrix (atmega32, atmega328p; `-Os` and `-O0`) plus the host tests.
- Datasheets moved to `docs/datasheets/`; demo screenshot to `docs/images/`.
- Added `docs/lcd-addressing.md` with the DDRAM map for every supported size.

## v1 — initial release

Original Arduino-style driver (`LCD_Setup`, `LCD_PrintInteger`, ...).
