# LaunchDisplay
<<<<<<< HEAD

Minimal firmware bring-up for the LaunchDisplay hardware:

- ESP32-C3
- Estardyn 3.5" 320x480 ST7796S SPI TFT display
- Wi-Fi onboarding with WiFiManager

## Current Status

As of August 15, 2026, the first hardware bring-up milestone is complete:

- TENSTAR ESP32-C3-Zero boots successfully
- Wi-Fi provisioning and reconnect work
- Estardyn 3.5" SPI TFT initializes successfully
- Basic `LaunchDisplay` rendering works

The remaining work is product-level polish and launch data integration.

The current goal is deliberately small:

1. Bring up the ESP32-C3.
2. Reuse the working Wi-Fi setup flow from the earlier project.
3. Initialize the TFT display.
4. Render a basic `LaunchDisplay` hello / standby screen.

## Wiring

These pins are the current bring-up assumptions and should be edited in one place if your C3 board exposes different GPIOs.

### TENSTAR ESP32-C3-Zero Mini pinout for the display

For the TFT bring-up, the current verified wiring is:

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

## Why these pins

- `GPIO2`, `GPIO8`, and `GPIO9` are strapping pins on the ESP32-C3, so I’m avoiding them for now.
- `GPIO18` and `GPIO19` are tied to USB serial/JTAG on the C3 family, so I’d rather leave them alone for this first pass.
- `GPIO20` and `GPIO21` are commonly used by UART0, so they’re also better left untouched during bring-up.

## Reference Hardware

The verified bring-up combination is:

- Board: TENSTAR ESP32-C3-Zero Mini Development Board
- MCU: ESP32-C3
- Display: Estardyn 3.5" 320x480 ST7796S SPI TFT module
- Library: LovyanGFX

The active pin map is documented in [`include/DisplayConfig.h`](./include/DisplayConfig.h).

### Wiring diagrams

- [Schematic-style diagram](./docs/wiring-schematic.svg)
- [Breadboard-style diagram](./docs/wiring-breadboard.svg)

## Notes

- This is a bring-up target, not the full LaunchDisplay app.
- The display is intentionally rendered with a single full-screen hello-world page before any launch API work.
- If the panel revision turns out to want a different LovyanGFX driver class, that change should stay isolated to `include/DisplayConfig.h`.
=======
A desktop display for upcoming rocket launches running on an ESP32 C3. 
>>>>>>> origin/main
