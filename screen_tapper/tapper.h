#pragma once
#include <stdint.h>

// Initialize solenoid output pins.
void tapper_begin(uint8_t adPin, uint8_t floatPin);

// Start a full tap cycle (ad-gem taps, then float-gem taps).
void tapper_startCycle(
    uint8_t  adTaps,
    uint8_t  floatTaps,
    uint16_t tapDurationMs,
    uint16_t pauseMs,
    uint32_t nowMs
);

// Set solenoid drive strength (0–255 PWM).
void tapper_setDuty(uint8_t duty);

// Advance the tapper state machine. Call once per loop().
// Returns true exactly once when the full cycle completes.
bool tapper_update(uint32_t nowMs);

// Immediately cut power to both solenoids and clear state.
void tapper_stop();

// True while any tap stage is running.
bool tapper_isActive();
