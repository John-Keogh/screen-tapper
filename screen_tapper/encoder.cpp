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

// --- DEBUG COUNTER ---
static long s_debugCount = 0;   // starts at 0

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

  // --- DEBUG BANNER ---
  Serial.println(F("[ENC] begin"));
  Serial.print(F("[ENC] pins: CLK=")); Serial.print(s_clk);
  Serial.print(F(" DT="));             Serial.print(s_dt);
  Serial.print(F(" SW="));             Serial.println(s_sw);
  Serial.print(F("[ENC] init CLK="));  Serial.print(s_lastClk);
  Serial.print(F(" DT="));             Serial.println(digitalRead(s_dt));
  Serial.println(F("[ENC] debug counter reset to 0"));
}

EncoderEvents encoder_poll() {
  EncoderEvents ev;
  unsigned long now = millis();

  // rotation handling (one report per falling edge of CLK)
  uint8_t clkNow = digitalRead(s_clk);
  if (clkNow != s_lastClk && clkNow == LOW) {
    uint8_t dtNow = digitalRead(s_dt);

    int step = (dtNow != clkNow) ? +1 : -1;
    // If direction feels reversed on your hardware, flip the sign:
    // step = -step;   // <--- uncomment to invert

    s_debugCount += step;
    ev.delta += step;

    Serial.print(F("[ENC] STEP "));
    Serial.print((step > 0) ? F("CW ") : F("CCW "));
    Serial.print(F("clk=")); Serial.print(clkNow);
    Serial.print(F(" dt="));  Serial.print(dtNow);
    Serial.print(F("  count=")); Serial.println(s_debugCount);
  }
  s_lastClk = clkNow;

  // press handling
  uint8_t swNow = digitalRead(s_sw);
  if (swNow != s_swLastStable) {
    if (now - s_swLastChangeMs >= SW_DEBOUNCE_MS) {
      s_swLastChangeMs = now;
      if (swNow == LOW) {
        ev.pressed = true;
        Serial.println(F("[ENC] PRESS"));
      }
      s_swLastStable = swNow;
    }
  }

  return ev;
}
