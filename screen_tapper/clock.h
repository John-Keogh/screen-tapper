#pragma once
#include <stdint.h>
#include "time_settings.h"

constexpr uint16_t CLOCK_MINUTES_INVALID = 0xFFFF;

void clock_begin();
bool clock_available();
bool clock_nowHM(uint8_t& hour, uint8_t& minute);
uint16_t clock_nowMinutes(); // minutes since midnight; returns 0xFFFF if RTC unavailable
bool clock_isAwake(const SleepSchedule& schedule);