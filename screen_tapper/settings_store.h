#pragma once
#include "config.h"
#include <stdint.h>

// =============================================================================
// EEPROM Settings Region  (bytes 2–255, 254 bytes available)
//
// Addr  Size  Contents
// ----  ----  --------
//  2     1    sleep hour        (0-23)
//  3     1    sleep minute      (0-59)
//  4     1    wake hour         (0-23)
//  5     1    wake minute       (0-59)
//  6-7   2    tap duration (ms) (uint16_t, 1-1000)
//  8     1    tap duty          (uint8_t,  0-255)
//  9-255       reserved for future settings
// =============================================================================

// Holds all persisted runtime settings in one place.
struct Settings {
    SleepSchedule sched;
    uint16_t tapDuration = 160;  // solenoid on-time per tap (ms)
    uint8_t  tapDuty     = 160;  // solenoid PWM drive level (0-255)
};

// Load all settings from EEPROM. Any value that is uninitialized or out of
// its valid range is left at the struct default, so a fresh EEPROM is safe.
void settings_load(Settings& s);

// Persist all settings to EEPROM. Uses EEPROM.update() internally so only
// bytes that actually changed are written, protecting write-cycle lifetime.
// Only call this when a value has been confirmed changed by the user.
void settings_save(const Settings& s);
