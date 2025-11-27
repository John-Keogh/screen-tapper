#pragma once
#include <stdint.h>

// replace with a structure
const uint32_t baseIntervalActual       = 670000; // nominal time between taps (ms)
const uint32_t jitterRangeActual        = 16000;  // jitter for time between taps (ms)
const uint32_t pauseBetweenTapsActual   = 500;
const uint8_t adGemTapsActual           = 5;
const uint8_t floatGemTapsActual        = 25;

const uint32_t baseIntervalTest         = 5000;
const uint32_t jitterRangeTest          = 0;
const uint32_t pauseBetweenTapsTest     = 500;
const uint8_t adGemTapsTest             = 4;
const uint8_t floatGemTapsTest          = 4;

// struct ModeParams {
//   uint32_t baseInterval;
//   uint32_t jitterRange;
//   uint16_t pauseBetweenTaps;
//   uint8_t adGemTaps;
//   uint8_t floatGemTaps;
//   uint16_t tapDurationMs;
// };

// namespace Modes {
//   inline const ModeParams Actual  { 670000, 16000, 1250, 7, 10, 10 };
//   inline const ModeParams Test    { 13000, 0, 1000, 3, 3, 10 };
// }