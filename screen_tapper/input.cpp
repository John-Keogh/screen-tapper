#include "input.h"
#include "config_pins.h"
#include <Arduino.h>

static const unsigned long DEBOUNCE_MS = 40;

static bool edgeLow(uint8_t pin) {
  // returns true exactly once when pin transitions to LOW
  struct Debounce {
    unsigned long t = 0;  // last time pin changed state
    uint8_t last = HIGH;  // last stable state
  };
  static Debounce db[70]; // 70 because Arduino Mega has 70 pins

  if (pin >= NUM_DIGITAL_PINS) return false;

  uint8_t currentState = digitalRead(pin);
  unsigned long now = millis();

  if (currentState != db[pin].last && (now - db[pin].t) > DEBOUNCE_MS) {
    db[pin].t = now;
    db[pin].last = currentState;
    // return true if this was a press edge (HIGH -> LOW)
    // return false (ignore) if this was a release edge (LOW -> HIGH)
    if (currentState == LOW) return true;
  }
  return false;
}

void input_begin() {
  pinMode(ONOFF_BUTTON_PIN,       INPUT_PULLUP);
  pinMode(MODE_TOGGLE_PIN,        INPUT_PULLUP);
  pinMode(TAP_DURATION_UP,        INPUT_PULLUP);
  pinMode(TAP_DURATION_DOWN,      INPUT_PULLUP);
  pinMode(OVERRIDE_CLOCK_BUTTON,  INPUT_PULLUP);
}

InputEvents input_poll() {
  InputEvents e;

  if (edgeLow(ONOFF_BUTTON_PIN))      e.onOffPressed    = true;
  if (edgeLow(MODE_TOGGLE_PIN))       e.testModePressed = true;
  if (edgeLow(TAP_DURATION_UP))       e.tapUpPressed    = true;
  if (edgeLow(TAP_DURATION_DOWN))     e.tapDownPressed  = true;
  if (edgeLow(OVERRIDE_CLOCK_BUTTON)) e.overridePressed = true;

  return e;
}