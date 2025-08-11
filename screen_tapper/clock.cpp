#include "clock.h"
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

static RTC_DS1307 g_rtc;
static bool s_rtcAvailable = false;

void clock_begin() {
  Wire.begin();
  s_rtcAvailable = g_rtc.begin();
}

bool clock_available() {
  return s_rtcAvailable;
}

bool clock_nowHM(uint8_t& hour, uint8_t& minute) {
  if (!s_rtcAvailable) return false;
  DateTime now = g_rtc.now();
  hour    = now.hour();
  minute  = now.minute();
  return true;
}

uint16_t clock_nowMinutes() {
  if (!s_rtcAvailable) return CLOCK_MINUTES_INVALID;
  DateTime now = g_rtc.now();
  return (uint16_t)(now.hour() * 60u + now.minute());
}

bool clock_isAwake(const SleepSchedule& schedule) {
  if (!s_rtcAvailable) return false;

  const uint16_t wake = toMinutes(schedule.wakeHour,  schedule.wakeMinute);
  const uint16_t bed  = toMinutes(schedule.sleepHour, schedule.sleepMinute);
  const uint16_t nowM = clock_nowMinutes();
  if (nowM == CLOCK_MINUTES_INVALID) return false;

  if (wake < bed) {
    // normal daytime window; [wake, bed)
    return (nowM >= wake) && (nowM < bed);
  } else if (wake > bed) {
    // crosses midnight: [wake, 24h) U [0, bed)
    return (nowM >= wake) || (nowM < bed);
  } else {
    // wake == bed - treat as awake
    return true;
  }
}