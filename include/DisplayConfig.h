#pragma once

#include <Arduino.h>

// TENSTAR ESP32-C3-Zero bring-up pins for the Estardyn 3.5" ST7796S SPI TFT.
// Keep these in one place so the wiring stays easy to verify against the board.
constexpr int TFT_PIN_SCLK = 4;
constexpr int TFT_PIN_MOSI = 6;
constexpr int TFT_PIN_CS = 7;
constexpr int TFT_PIN_DC = 1;
constexpr int TFT_PIN_RST = 0;
constexpr int TFT_PIN_BL = 5;
constexpr bool TFT_BL_ACTIVE_HIGH = true;

constexpr uint32_t SERIAL_BAUD = 115200;

constexpr int TFT_WIDTH = 320;
constexpr int TFT_HEIGHT = 480;

constexpr const char *LAUNCH_DISPLAY_NAME = "LaunchDisplay";
constexpr const char *LAUNCH_DISPLAY_WIFI_AP = "LaunchDisplay-Setup";
