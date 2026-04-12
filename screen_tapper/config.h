#pragma once
#include <stdint.h>

// =============================================================================
// DEBUG
// Comment out this line to strip all debug Serial output from the build.
// =============================================================================
// #define DEBUG

// =============================================================================
// PIN ASSIGNMENTS
// =============================================================================

// MOSFET gates (solenoid drivers)
constexpr int AD_GEMS_MOSFET_GATE_PIN    = 9;
constexpr int FLOAT_GEMS_MOSFET_GATE_PIN = 11;

// Rotary encoder
constexpr int ENCODER_CLK = 40;
constexpr int ENCODER_DT  = 42;
constexpr int ENCODER_SW  = 44;

// 128x64 LCD (ST7920, software SPI) — pins 4, 6, 8 only; backlight not PWM-controlled
constexpr int LCD12864_CLK = 53;
constexpr int LCD12864_DAT = 51;
constexpr int LCD12864_CS  = 49;

// Tap-timer reset button (mounted on LCD module, INPUT_PULLUP, active LOW)
constexpr int RESET_BUTTON_PIN = 31;

// =============================================================================
// TAP TIMING PARAMETERS
// =============================================================================

struct ModeParams {
    uint32_t baseIntervalMs;    // nominal time between tap cycles (ms)
    uint32_t jitterRangeMs;     // +/- random jitter added to each interval (ms)
    uint16_t pauseBetweenTapsMs;// pause between individual taps within a cycle (ms)
    uint8_t  adGemTaps;         // number of taps for the ad-gem solenoid per cycle
    uint8_t  floatGemTaps;      // number of taps for the float-gem solenoid per cycle
};

// Normal operating mode
constexpr ModeParams kModeActual = {
    670000,  // baseIntervalMs
    16000,   // jitterRangeMs
    750,     // pauseBetweenTapsMs
    5,       // adGemTaps
    17,      // floatGemTaps
};

// Faster test mode for verifying hardware behavior
constexpr ModeParams kModeTest = {
    7000,    // baseIntervalMs
    0,       // jitterRangeMs
    750,     // pauseBetweenTapsMs
    3,       // adGemTaps
    5,       // floatGemTaps
};

// =============================================================================
// SLEEP SCHEDULE
// =============================================================================

struct SleepSchedule {
    uint8_t wakeHour   = 7;
    uint8_t wakeMinute = 30;
    uint8_t sleepHour  = 23;
    uint8_t sleepMinute = 59;
};

inline uint16_t toMinutes(uint8_t h, uint8_t m) { return h * 60u + m; }

// =============================================================================
// GEM STORE
// =============================================================================

// Save lifetime gem count to EEPROM after this many session gems accumulate.
constexpr uint16_t kGemSaveThreshold = 25;
