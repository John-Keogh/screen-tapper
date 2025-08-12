#pragma once
#include <stdint.h>

// replace with a structure
const uint32_t baseIntervalActual       = 670000;
const uint32_t jitterRangeActual        = 16000;
const uint32_t pauseBetweenTapsActual   = 1250;
const uint8_t adGemTapsActual           = 7;
const uint8_t floatGemTapsActual        = 10;

const uint32_t baseIntervalTest         = 13000;
const uint32_t jitterRangeTest          = 0;
const uint32_t pauseBetweenTapsTest     = 1000;
const uint8_t adGemTapsTest             = 3;
const uint8_t floatGemTapsTest          = 3;

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