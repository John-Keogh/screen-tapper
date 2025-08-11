#pragma once
#include <stdint.h>

struct InputEvents {
  bool onOffPressed     = false;
  bool testModePressed  = false;
  bool tapUpPressed     = false;
  bool tapDownPressed   = false;
  bool overridePressed  = false;
};

void input_begin();
InputEvents input_poll();