# HD44780 DDRAM addressing by display size

Row start addresses for every supported HD44780-based character LCD. The library
uses these to translate `hd44780_goto(x, y)` into a `Set DDRAM Address` command.

## 16x1

Two internal layouts exist. Set `HD44780_16X1_SPLIT` accordingly.

- `HD44780_16X1_SPLIT 0` (Type A, contiguous): `0x00..0x0F`
- `HD44780_16X1_SPLIT 1` (Type B, split):     `0x00..0x07` + `0x40..0x47`

## 16x2

| Row | Range         |
|-----|---------------|
| 0   | `0x00..0x0F`  |
| 1   | `0x40..0x4F`  |

## 16x4

| Row | Range         |
|-----|---------------|
| 0   | `0x00..0x0F`  |
| 1   | `0x40..0x4F`  |
| 2   | `0x10..0x1F`  |
| 3   | `0x50..0x5F`  |

## 20x2

| Row | Range         |
|-----|---------------|
| 0   | `0x00..0x13`  |
| 1   | `0x40..0x53`  |

## 20x4

| Row | Range         |
|-----|---------------|
| 0   | `0x00..0x13`  |
| 1   | `0x40..0x53`  |
| 2   | `0x14..0x27`  |
| 3   | `0x54..0x67`  |

## 40x2

| Row | Range         |
|-----|---------------|
| 0   | `0x00..0x27`  |
| 1   | `0x40..0x67`  |

## 40x4

Two HD44780 controllers, one EN pin each — not supported by this library.

| Controller | Row | Range         |
|------------|-----|---------------|
| A          | 0   | `0x00..0x27`  |
| A          | 1   | `0x40..0x67`  |
| B          | 0   | `0x00..0x27`  |
| B          | 1   | `0x40..0x67`  |
