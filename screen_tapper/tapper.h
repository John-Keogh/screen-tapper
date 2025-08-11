#pragma once
#include <stdint.h>

void tapper_begin(uint8_t adPin, uint8_t floatPin);
void tapper_startCycle(
  uint8_t   adTaps,
  uint8_t   floatTaps,
  uint16_t  tapDurationMs,
  uint16_t  pauseMs,
  uint32_t  nowMs
);
bool tapper_update(uint32_t nowMs);   // returns true exactly once when the full tap cycle is complete
void tapper_stop();   // emergency stop (turns outputs LOW, clears state)
bool tapper_isActive();   // true while any stage is running
bool tapper_inAdStage();  // true if in ad tapping stage (remove?)