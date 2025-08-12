#include "ui.h"

#include <Arduino.h>
#include <LiquidCrystal.h>
#include "config_pins.h"

static LiquidCrystal lcd(
  LCD_RS,
  LCD_EN,
  LCD_D4,
  LCD_D5,
  LCD_D6,
  LCD_D7
);

static UIMode g_mode = UIMode::OFF_MODE;
static bool s_tappingShown = false;

static void clear() { lcd.clear(); }
static void home()  { lcd.setCursor(0, 0); }
static void line2() { lcd.setCursor(0, 1); }

static void printPadded(const char* s) {
  // prints string and pads the rest of the 16-char line with spaces
  int len = 0;
  for (const char* p = s; *p && len < 16; ++p, ++len) lcd.print(*p);
  for (; len<16; ++len) lcd.print(' ');
}

static void printBlankLine() {
  printPadded("");
}

static void fmt_mm_ss(uint32_t msLeft, char* out) {
  // format milliseconds to MM:SS
  uint32_t totalS = msLeft / 1000UL;
  uint32_t mm = totalS / 60UL;
  uint32_t ss = totalS % 60UL;
  snprintf(out, 5+1, "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);
}

static void fmt_commas(uint32_t v, char* out /*>=16*/) {
  // format number into string with commas
  char t[16];
  snprintf(t, sizeof(t), "%lu", (unsigned long)v);
  int len = strlen(t), j = 0;
  for (int i = 0; i < len; ++i) {
    out[j++] = t[i];
    if (((len-i-1) % 3) == 0 && i != len-1) out[j++] = ',';
  }
  out[j] = '\0';
}

static void fmt_time(int hh, int mm, char* out) {
  // format time in HH:MM format
  snprintf(out, 5 + 1, "%02d:%02d", hh, mm);
}

// ---------- public API ----------
void ui_begin() {
  lcd.begin(16, 2);
  // Backlight pin is owned by UI; default to ON
  pinMode(LCD_LED, OUTPUT);
  ui_setBacklight(255);

  clear();
  home();   printPadded("Initializing...");
  line2();  printPadded("Please wait");
  g_mode = UIMode::ACTIVE_MODE;  // default
}

void ui_setMode(UIMode mode) {
  if (mode == g_mode) return;
  g_mode = mode;
  clear();
  home();
  switch (mode) {
    case UIMode::OFF_MODE:     printPadded("Device is off.");   break;
    case UIMode::SLEEP_MODE:   printPadded("Sleeping...");      break;
    case UIMode::ACTIVE_MODE:  printPadded("Ready");            break;
  }
  line2(); printBlankLine();
}

void ui_setBacklight(uint8_t pwm) {
  analogWrite(LCD_LED, pwm);
}

void ui_showTestMode(bool on) {
  ui_showOverlay(on ? "Test Mode: On" : "Test Mode: Off", "", 1000, 1);
}

void ui_showOverride(bool active) {
  ui_showOverlay("Clock Override:", active ? "Enabled" : "Disabled", 1000, 2);
}

void ui_showTapDuration(uint16_t ms) {
  char line2[17];
  snprintf(line2, sizeof(line2), "%u ms", (unsigned)ms);
  ui_showOverlay("Tap Duration:", line2, 1000, 0);
}

void ui_showTapping() {
  if (s_tappingShown) return;
  home();   printPadded("Tapping...");
  line2();  printPadded("");
  s_tappingShown = true;
}

void ui_resetTappingScreen() {
  // allow ui_showTapping() to draw again on the next activation
  s_tappingShown = false;
}

void ui_showOff() {
  home();   printPadded("Device is off.");
  line2();  printBlankLine();
}

void ui_showSleep(uint8_t hh, uint8_t mm) {
  home();   printPadded("Sleeping...");
  line2(); {
    char buf[6];
    fmt_time(hh, mm, buf);
    char line2buf[17];
    snprintf(line2buf, sizeof(line2buf), "Wake at: %s", buf);
    printPadded(line2buf);
  }
}

void ui_showNextTapCountdown(uint32_t msLeft) {
  home();   printPadded("Next tap in:");
  line2(); {
    char buf[8];
    fmt_mm_ss(msLeft, buf);
    printPadded(buf);
  }
}

void ui_showLifetimeGems(uint32_t gems) {
  home();  printPadded("Lifetime Gems:");
  line2();

  char buf[32];
  fmt_commas(gems, buf);

  if (strlen(buf) > 16) {       // doesn't fit on a 16-col line
    printPadded("ERR: too large");
  } else {
    printPadded(buf);
  }
}

// ---------- Overlay system ----------
static bool       ov_active = false;
static uint32_t   ov_until = 0;
static uint8_t    ov_prio = 0;
static char       ov_l1[17];
static char       ov_l2[17];

static uint32_t nextUiTick = 0; // rate limiter

void ui_showOverlay(const char* row1, const char* row2, uint16_t durationMs, uint8_t priority) {
  if (ov_active && priority < ov_prio) return;

  snprintf(ov_l1, sizeof(ov_l1), "%.16s", row1 ? row1 : "");
  snprintf(ov_l2, sizeof(ov_l2), "%.16s", row2 ? row2 : "");

  ov_prio = priority;
  ov_active = true;
  ov_until = millis() + durationMs;

  home();   printPadded(ov_l1);
  line2();  printPadded(ov_l2);
}

bool ui_overlayActive() {
  // returns true while an overlay owns the display
  if (!ov_active) return false;

  // expire check
  if ((int32_t)(millis() - ov_until) >= 0) {
    ov_active = false;
    ov_prio = 0;
    return false;
  }
  return true;
}

void ui_tick() {
  uint32_t now = millis();
  if ((int32_t)(now - nextUiTick) < 0) return;
  nextUiTick = now + 50; // ~20Hz

  if (ui_overlayActive()) {
    home();   printPadded(ov_l1);
    line2();  printPadded(ov_l2);
    return;
  }  
}