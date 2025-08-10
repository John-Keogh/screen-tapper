#pragma once
#include <stdint.h>

// replace with a structure
const unsigned long baseIntervalActual = 670000;
const unsigned long jitterRangeActual = 16000;
const unsigned long pauseBetweenTapsActual = 1250;
const unsigned long adGemTapsActual = 7;
const unsigned long floatGemTapsActual = 10;

const unsigned long baseIntervalTest = 13000;
const unsigned long jitterRangeTest = 0;
const unsigned long pauseBetweenTapsTest = 1000;
const unsigned long adGemTapsTest = 3;
const unsigned long floatGemTapsTest = 3;

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