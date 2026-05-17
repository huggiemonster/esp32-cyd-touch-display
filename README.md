# ESP32 Cheap Yellow Display — Touchscreen + TFT Display

Working firmware for the ESP32 Cheap Yellow Display (CYD) that initializes both the TFT display and XPT2046 touchscreen.

## Hardware

- **Board**: ESP32 Cheap Yellow Display (CYD) — 2.4" TFT with touch
- **Display**: 320×240 ILI9341
- **Touchscreen**: XPT2046

## Prerequisites

1. **TFT_eSPI** library by Bodmer — https://github.com/Bodmer/TFT_eSPI
   - ⚠️ You **must** use the custom `User_Setup.h` configured for the CYD. Generic setups will not work.
   - Instructions: https://RandomNerdTutorials.com/cyd/ or https://RandomNerdTutorials.com/esp32-tft/
2. **XPT2046_Touchscreen** library by Paul Stoffregen — https://github.com/PaulStoffregen/XPT2046_Touchscreen

## Pinout

| Pin | ESP32 GPIO | Function |
|-----|-----------|----------|
| T_IRQ | 36 | Touch IRQ |
| T_DIN | 32 | Touch MOSI |
| T_OUT | 39 | Touch MISO |
| T_CLK | 25 | Touch CLK |
| T_CS | 33 | Touch CS |

## Usage

Upload via Arduino IDE or your preferred ESP32 toolchain. The sketch:

1. Prints `"Hello, world!"` and `"Touch screen to test"` to the display
2. On touch, shows the calibrated X, Y coordinates and pressure on the screen
3. Also prints the same info to the Serial Monitor at 115200 baud

## License

Public domain / free to use.
