#pragma once
#include <stdint.h> // what does this do?

// old - replace with SleepSchedule structure eventually
constexpr uint8_t WAKE_UP_HOUR = 5;
constexpr uint8_t WAKE_UP_MINUTE = 50;
constexpr uint8_t BED_TIME_HOUR = 23;
constexpr uint8_t BED_TIME_MINUTE = 0;
constexpr uint8_t WAKE_UP_TIME = WAKE_UP_HOUR * 60 + WAKE_UP_MINUTE;
constexpr uint8_t BED_TIME = BED_TIME_HOUR * 60 + BED_TIME_MINUTE;

// struct SleepSchedule{
//   uint8_t wakeHour    = 5;
//   uint8_t wakeMinute  = 45;
//   uint8_t bedHour     = 23;
//   uint8_t bedMinute   = 30;
// };

// inline uint16_t toMinutes(uint8_t h, uint8_t m){ return h*60 + m; }