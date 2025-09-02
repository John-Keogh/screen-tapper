// Purpose: read the rotary encoder (CLK/DT/SW) and produce events.

#include "encoder.h"
#include <Arduino.h>

// debounce for the push switch
static const unsigned long SW_DEBOUNCE_MS = 30;

// pin storage
static uint8_t s_clk = 0xFF;
static uint8_t s_dt  = 0xFF;
static uint8_t s_sw  = 0xFF;

// rotation detection
static uint8_t s_lastClk = HIGH;

// state for switch debouncing (active LOW)
static unsigned long s_swLastChangeMs = 0;
static uint8_t s_swLastStable = HIGH;

void encoder_begin(uint8_t clkPin, uint8_t dtPin, uint8_t swPin) {
  s_clk = clkPin;
  s_dt  = dtPin;
  s_sw  = swPin;

  pinMode(s_clk, INPUT_PULLUP);
  pinMode(s_dt,  INPUT_PULLUP);
  pinMode(s_sw,  INPUT_PULLUP);

  // initialize state
  s_lastClk = digitalRead(s_clk);
  s_swLastStable = digitalRead(s_sw);
  s_swLastChangeMs = millis();
}

EncoderEvents encoder_poll() {
  EncoderEvents ev;
  unsigned long now = millis();

  // rotation handling
  uint8_t clkNow = digitalRead(s_clk);
  if (clkNow != s_lastClk && clkNow == LOW) {
    uint8_t dtNow = digitalRead(s_dt);
    ev.delta += (dtNow != clkNow) ? +1: -1;
  }
  s_lastClk = clkNow;

  // press handling
  uint8_t swNow = digitalRead(s_sw);
  if (swNow != s_swLastStable) {
    if (now - s_swLastChangeMs >= SW_DEBOUNCE_MS) {
      s_swLastChangeMs = now;
      if (swNow == LOW) {
        ev.pressed = true;
      }
      s_swLastStable = swNow;
    }
  }

  return ev;
}