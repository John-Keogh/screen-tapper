#pragma once
#include <stdint.h>

// Initialize a debounced button on the given pin (INPUT_PULLUP, active LOW).
void button_begin(uint8_t pin);

// Poll the button. Returns true exactly once per press (falling edge).
// Call once per loop().
bool button_poll();
