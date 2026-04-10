#include "encoder.h"
#include "config.h"
#include <Arduino.h>

// Debounce window for the push switch
static constexpr unsigned long SW_DEBOUNCE_MS = 30;

// Pin storage
static uint8_t s_clk = 0xFF;
static uint8_t s_dt  = 0xFF;
static uint8_t s_sw  = 0xFF;

// Rotation detection
static uint8_t s_lastClk = HIGH;

// Switch debounce state (active LOW)
static unsigned long s_swLastChangeMs = 0;
static uint8_t       s_swLastStable   = HIGH;

#ifdef DEBUG
static long s_debugCount = 0;
#endif

void encoder_begin(uint8_t clkPin, uint8_t dtPin, uint8_t swPin) {
    s_clk = clkPin;
    s_dt  = dtPin;
    s_sw  = swPin;

    pinMode(s_clk, INPUT_PULLUP);
    pinMode(s_dt,  INPUT_PULLUP);
    pinMode(s_sw,  INPUT_PULLUP);

    s_lastClk      = digitalRead(s_clk);
    s_swLastStable = digitalRead(s_sw);
    s_swLastChangeMs = millis();

#ifdef DEBUG
    s_debugCount = 0;
    Serial.println(F("[ENC] begin"));
    Serial.print(F("[ENC] pins: CLK=")); Serial.print(s_clk);
    Serial.print(F(" DT="));             Serial.print(s_dt);
    Serial.print(F(" SW="));             Serial.println(s_sw);
    Serial.print(F("[ENC] init CLK="));  Serial.print(s_lastClk);
    Serial.print(F(" DT="));             Serial.println(digitalRead(s_dt));
    Serial.println(F("[ENC] debug counter reset to 0"));
#endif
}

EncoderEvents encoder_poll() {
    EncoderEvents ev;
    unsigned long now = millis();

    // Rotation: report one step per falling edge of CLK
    uint8_t clkNow = digitalRead(s_clk);
    if (clkNow != s_lastClk && clkNow == LOW) {
        uint8_t dtNow = digitalRead(s_dt);
        int8_t step = (dtNow != clkNow) ? +1 : -1;
        // If direction feels reversed on your hardware, flip the sign:
        // step = -step;
        ev.delta += step;

#ifdef DEBUG
        s_debugCount += step;
        Serial.print(F("[ENC] STEP "));
        Serial.print((step > 0) ? F("CW ") : F("CCW "));
        Serial.print(F("clk=")); Serial.print(clkNow);
        Serial.print(F(" dt="));  Serial.print(dtNow);
        Serial.print(F("  count=")); Serial.println(s_debugCount);
#endif
    }
    s_lastClk = clkNow;

    // Press: debounced falling edge detection
    uint8_t swNow = digitalRead(s_sw);
    if (swNow != s_swLastStable && (now - s_swLastChangeMs) >= SW_DEBOUNCE_MS) {
        s_swLastChangeMs = now;
        if (swNow == LOW) {
            ev.pressed = true;
#ifdef DEBUG
            Serial.println(F("[ENC] PRESS"));
#endif
        }
        s_swLastStable = swNow;
    }

    return ev;
}
