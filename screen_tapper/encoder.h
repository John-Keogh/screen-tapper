#pragma once
#include <stdint.h>

struct EncoderEvents {
    int8_t delta   = 0;      // signed detent count since last poll (CW = positive)
    bool   pressed = false;  // true exactly once on the switch press (falling edge)
};

void          encoder_begin(uint8_t clkPin, uint8_t dtPin, uint8_t swPin);
EncoderEvents encoder_poll();   // call once per loop(); resets state each call
