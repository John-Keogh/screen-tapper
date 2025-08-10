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
  clear();
  home();   printPadded( on ? "Test Mode: On" : "Test Mode: Off");
  line2();  printBlankLine();
}

void ui_showOverride(bool active) {
  clear();
  home();   printPadded("Override Clock:");
  line2();  printPadded(active ? "Active" : "Inactive");
}

void ui_showTapDuration(uint16_t ms) {
  clear();
  home();   printPadded("Tap Duration:");
  line2(); {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u ms", (unsigned)ms);
    printPadded(buf);
  }
}

void ui_showTapping() {
  clear();
  home();   printPadded("Tapping...");
  line2();  printBlankLine();
}

void ui_showOff() {
  clear();
  home();   printPadded("Device is off.");
  line2();  printBlankLine();
}

void ui_showSleep(uint8_t hh, uint8_t mm) {
  clear();
  home();   printPadded("Sleeping...");
  line2(); {
    char buf[6];
    fmt_time(hh, mm, buf);
    char line2buf[16];
    snprintf(line2buf, sizeof(line2buf), "Wake at: %s", buf);
    printPadded(line2buf);
  }
}

void ui_showNextTapCountdown(uint32_t msLeft) {
  clear();
  home();   printPadded("Next tap in:");
  line2(); {
    char buf[8];
    fmt_mm_ss(msLeft, buf);
    printPadded(buf);
  }
}

void ui_showLifetimeGems(uint32_t gems) {
  clear();
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

void ui_clear() {
  clear();
}