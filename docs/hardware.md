# LaunchDisplay Hardware Notes

This note captures the verified hardware combination for the current LaunchDisplay bring-up.

## Verified Setup

- Board: TENSTAR ESP32-C3-Zero Mini Development Board
- MCU: ESP32-C3
- Display: Estardyn 3.5" 320x480 ST7796S SPI TFT module
- Display library: LovyanGFX
- Power: USB

## Working Pin Map

| TENSTAR ESP32-C3-Zero Mini | TFT |
| --- | --- |
| `GPIO4` | `SCL` / `SCLK` |
| `GPIO6` | `SDA` / `MOSI` |
| `GPIO7` | `CS` |
| `GPIO1` | `DC` |
| `GPIO0` | `RST` |
| `GPIO5` | `BL` |
| `5V` | `VCC` |
| `GND` | `GND` |

## Notes

- `GPIO2`, `GPIO8`, and `GPIO9` are strapping pins on the ESP32-C3, so they are left unused.
- `GPIO18` and `GPIO19` are associated with USB serial/JTAG on the C3 family, so they are also left unused.
- `GPIO20` and `GPIO21` are left free so the default serial path stays simple.

## Wiring diagrams

- [`docs/wiring-schematic.svg`](./wiring-schematic.svg)
- [`docs/wiring-breadboard.svg`](./wiring-breadboard.svg)

## Bring-Up Result

The current firmware successfully:

- connects to Wi-Fi using the reused setup flow,
- initializes the TFT display,
- and renders the LaunchDisplay standby / launch screen.

That makes this a verified baseline for future launch data integration work.
