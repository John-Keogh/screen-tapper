#pragma once
#include <stdint.h>

enum class UIMode { OFF_MODE, SLEEP_MODE, ACTIVE_MODE };

void ui_begin();
void ui_setMode(UIMode mode);
void ui_setBacklight(uint8_t pwm);
void ui_showTestMode(bool on);
void ui_showOverride(bool active);
void ui_showTapDuration(uint16_t ms);
void ui_showTapping();
void ui_showOff();
void ui_showSleep(uint8_t hh, uint8_t mm);
void ui_showNextTapCountdown(uint32_t msLeft);
void ui_showLifetimeGems(uint32_t gems);
void ui_clear();