#pragma once

// MOSFET gates
constexpr int AD_GEMS_MOSFET_GATE_PIN = 26;
constexpr int FLOAT_GEMS_MOSFET_GATE_PIN = 22;

// Buttons (eventually remove once LCD module is added)
constexpr int ONOFF_BUTTON_PIN = 37;
constexpr int MODE_TOGGLE_PIN = 39;
constexpr int TAP_DURATION_UP = 41;
constexpr int TAP_DURATION_DOWN = 43;
constexpr int OVERRIDE_CLOCK_BUTTON = 45;

// LCD (eventually update once LCD module is added)
constexpr int LCD_LED = 2;
constexpr int LCD_RS = 29;
constexpr int LCD_EN = 28;
constexpr int LCD_D4 = 31;
constexpr int LCD_D5 = 30;
constexpr int LCD_D6 = 33;
constexpr int LCD_D7 = 32;