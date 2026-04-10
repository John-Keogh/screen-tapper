#include "button.h"
#include <Arduino.h>

static constexpr unsigned long DEBOUNCE_MS = 40;

static uint8_t       s_pin           = 0xFF;
static uint8_t       s_lastStable    = HIGH;
static unsigned long s_lastChangeMs  = 0;

void button_begin(uint8_t pin) {
    s_pin          = pin;
    s_lastStable   = HIGH;
    s_lastChangeMs = millis();
    pinMode(s_pin, INPUT_PULLUP);
}

bool button_poll() {
    if (s_pin == 0xFF) return false;

    uint8_t       current = digitalRead(s_pin);
    unsigned long now     = millis();

    if (current != s_lastStable && (now - s_lastChangeMs) >= DEBOUNCE_MS) {
        s_lastChangeMs = now;
        s_lastStable   = current;
        // Report true only on the press edge (HIGH → LOW)
        if (current == LOW) return true;
    }
    return false;
}
