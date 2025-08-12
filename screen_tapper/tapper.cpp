#include "tapper.h"
#include <Arduino.h>

static uint8_t s_adPin = 0;
static uint8_t s_floatPin = 0;

static bool     s_active = false;
static bool     s_inAd = true;
static uint8_t  s_currentPin = 0;

static uint8_t s_adTaps = 0;
static uint8_t s_floatTaps = 0;
static uint8_t s_currentTap = 0;

static bool     s_solenoidOn = false;
static uint32_t s_phaseStart = 0;
static uint16_t s_tapDuration = 10;
static uint16_t s_pause = 1000;

static void driveLow(uint8_t pin)   { digitalWrite(pin, LOW); }
static void driveHigh(uint8_t pin)  { digitalWrite(pin, HIGH); }

void tapper_begin(uint8_t adPin, uint8_t floatPin) {
  s_adPin = adPin;
  s_floatPin = floatPin;
  pinMode(s_adPin, OUTPUT);
  pinMode(s_floatPin, OUTPUT);
  driveLow(s_adPin);
  driveLow(s_floatPin);
  s_active = false;
}

void tapper_startCycle(
  uint8_t adTaps,
  uint8_t floatTaps,
  uint16_t tapDurationMs,
  uint16_t pauseMs,
  uint32_t nowMs
) {
  s_adTaps      = adTaps;
  s_floatTaps   = floatTaps;
  s_tapDuration = tapDurationMs;
  s_pause       = pauseMs;

  s_inAd        = true;
  s_currentPin  = s_adPin;
  s_currentTap  = 0;
  s_solenoidOn  = false;
  s_phaseStart  = nowMs;

  driveLow(s_adPin);
  driveLow(s_floatPin);
  
  s_active = (adTaps > 0) || (floatTaps > 0);
}

static uint8_t targetTapsForStage() {
  return s_inAd ? s_adTaps : s_floatTaps;
}

bool tapper_update(uint32_t nowMs) {
  // drives the active tapping sequence
  if (!s_active) return false;

  if (!s_solenoidOn) {
    if (nowMs - s_phaseStart >= s_pause) {
      driveHigh(s_currentPin);
      s_solenoidOn = true;
      s_phaseStart = nowMs;
    }
    return false;
  }

  if (nowMs - s_phaseStart >= s_tapDuration) {
    driveLow(s_currentPin);
    s_solenoidOn = false;
    s_phaseStart = nowMs;
    s_currentTap++;

    if (s_currentTap >= targetTapsForStage()) {
      if (s_inAd && s_floatTaps > 0) {
        s_inAd        = false;
        s_currentPin  = s_floatPin;
        s_currentTap  = 0;
        return false;
      } else {
        s_active = false;
        driveLow(s_adPin);
        driveLow(s_floatPin);
        return true;
      }
    }
  }
  return false;
}

void tapper_stop() {
  s_active = false;
  s_solenoidOn = false;
  driveLow(s_adPin);
  driveLow(s_floatPin);
}

bool tapper_isActive()    { return s_active; }
bool tapper_inAdStage()   { return s_active && s_inAd; }