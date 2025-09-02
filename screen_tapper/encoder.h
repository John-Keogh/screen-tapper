#pragma once
#include <stdint.h>

struct EncoderEvents {
  int8_t  delta = 0;  // number of detents turned
  bool    pressed = false;  // true exactly once on the momentary switch press (LOW edge)
};

// initialize
void encoder_begin(uint8_t clkPin, uint8_t dtPin, uint8_t swPin);

// poll once per loop; returns accumulated turns and a click edge (resets each call)
EncoderEvents encoder_poll();