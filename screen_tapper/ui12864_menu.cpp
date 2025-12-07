// Purpose: render the view provided by menu using U8g2.

#include "ui12864_menu.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "config_pins.h"

static U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, LCD12864_CLK, LCD12864_DAT, LCD12864_CS);

// fonts
static const uint8_t* FONT_TITLE   = u8g2_font_7x14B_tf;    // header
static const uint8_t* FONT_BODY    = u8g2_font_6x10_tf;     // normal text
static const uint8_t* FONT_NUMBER  = u8g2_font_helvB10_tf;    // big numbers
static const uint8_t* FONT_SMALL   = u8g2_font_5x8_tf;      // tiny helpers

// layout
static const uint8_t W                = 128;
static const uint8_t H                = 64;
static const uint8_t PAD              = 2;
static const uint8_t LINE_H_BODY      = 10;
static const uint8_t LINE_H_TITLE     = 10;
static const uint8_t MARGIN           = 0;
static const uint8_t GAP_LABEL_VALUE  = 4;
static const uint8_t GEM_Y            = 30;
static const uint8_t TAP_DURATION_Y   = 26;
static const uint8_t CLOCK_Y          = 26;
static const uint8_t COLUMN_GAP       = 20;
static const uint8_t X_HOME_1         = 0;    // icon in first column
static const uint8_t X_HOME_2         = 19;   // text in first column
static const uint8_t Y_HOME_1         = 13;
static const uint8_t X_HOME_SPACE     = 80;
static const uint8_t Y_HOME_SPACE     = 15;

// first visible row for list views
static uint8_t s_first = 0;

// update flags for LCD screen
static bool lcdDirty = true;  // true: update, false: skip
static const uint16_t framePeriodMs = 100; // 10 FPS max
static uint32_t nextFrameMs = 0;

// custom symbols
// left arrow symbol
static const unsigned char larrow_bitmap[] U8X8_PROGMEM = {
  0b00000000,
  0b00000000,
  0b00010000,
  0b00100000,
  0b01111110,
  0b00100000,
  0b00010000,
  0b00000000
};

// return symbol
static const unsigned char return_bitmap[] U8X8_PROGMEM = {
  0b00000000,
  0b00010000,
  0b00111000,
  0b01111100,
  0b00010000,
  0b00011110,
  0b00000000,
  0b00000000
};

// custom gem symbol
static const unsigned char gem16_bitmap[] U8X8_PROGMEM = {
  0b11111000, 0b00111111,
  0b01101100, 0b01101100,
  0b11000110, 0b11000110,
  0b10000010, 0b10000011,
  0b11111110, 0b11111111,
  0b00000110, 0b11000001,
  0b00001100, 0b01100001,
  0b00011000, 0b00110001,
  0b00110000, 0b00011001,
  0b01100000, 0b00001101,
  0b11000000, 0b00000111,
  0b10000000, 0b00000011,
  0b00000000, 0b00000001,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
};

// custom hand pointing symbol
static const unsigned char countdown16_bitmap[] U8X8_PROGMEM = {
  0b11111111, 0b00000011,
  0b11111111, 0b00000011,
  0b00110000, 0b00000000,
  0b00110000, 0b00000000,
  0b00110000, 0b00000000,
  0b00110000, 0b00000000,
  0b00110000, 0b11110000,
  0b00110000, 0b11110000,
  0b00110000, 0b00000000,
  0b00110000, 0b00000000,
  0b00110000, 0b00000000,
  0b00110000, 0b00000000,
  0b00110000, 0b00000000,
  0b00110000, 0b00000000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
};

// wake symbol (clock)
static const unsigned char clock16_bitmap[] U8X8_PROGMEM = {
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
  0b10000000, 0b00000011,
  0b00000000, 0b00000001,
  0b11000000, 0b00000011,
  0b00100000, 0b00000100,
  0b00010000, 0b00001001,
  0b00010000, 0b00001001,
  0b11010000, 0b00001000,
  0b00010000, 0b00001000,
  0b00100000, 0b00000100,
  0b11000000, 0b00000011,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
};

static const unsigned char moon16_bitmap[] U8X8_PROGMEM = {
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,
  0b11000000, 0b00000001,
  0b10100000, 0b00000000,
  0b01010000, 0b00000000,
  0b01001000, 0b00000000,
  0b01001000, 0b00000000,
  0b01001000, 0b00000000,
  0b10001000, 0b00000000,
  0b00001000, 0b00100011,
  0b00001000, 0b00111100,
  0b00010000, 0b00010000,
  0b00100000, 0b00001100,
  0b11000000, 0b00000011,
  0b00000000, 0b00000000,
  0b00000000, 0b00000000,  
};


// small helpers
static void headerBar(const char* title) {
  u8g2.setFont(FONT_TITLE);
  u8g2.setCursor(PAD, LINE_H_TITLE);
  u8g2.print(title);
}

static void drawLabelAt(int x, int y, const char* s) {
  u8g2.setFont(FONT_BODY);
  u8g2.setCursor(x, y);
  u8g2.print(s ? s : "");
}

static void fmt_commas(uint32_t v, char* out, size_t outSz) {
  char t[16];
  snprintf(t, sizeof(t), "%lu", (unsigned long)v);
  int len = strlen(t), j = 0;
  for (int i = 0; i < len && j < (int)outSz - 1; ++i) {
    out[j++] = t[i];
    if (((len - i - 1) % 3) == 0 && i != len - 1 && j < (int)outSz - 1) out[j++] = ',';
  }
  out[j] = '\0';
}

static void fmt_mm_ss(uint32_t ms, char* out, size_t outSz) {
  uint32_t s = ms / 1000UL;
  uint32_t mm = s / 60UL;
  uint32_t ss = s % 60UL;
  snprintf(out, outSz, "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);
}

static void drawSoftKeys(const char* left, const char* right) {
  // small footer helpers
  u8g2.setFont(FONT_SMALL);
  if (left) {
    u8g2.setCursor(PAD, H - MARGIN);
    u8g2.print(left);
  }
  if (right) {
    int16_t x = W - PAD - (int16_t)u8g2.getStrWidth(right);
    u8g2.setCursor(x, H - MARGIN);
    u8g2.print(right);
  }
}

static void maybeMarkDirtyFromView(const MenuView& v) {
  uint32_t secondsLeft = v.msLeft / 1000;

}

// views
static void viewHome(const MenuView& v) {
  // gem count
  // time remaining
  // sleep time
  // wake time
  // on/off - make separate off screen?
  // override sleep - maybe just incorporate to sleep/wake time?
  // tap duration

  // gem count - [0, 0]
  char gems[24];
  fmt_commas(v.lifetimeGems, gems, sizeof(gems));
  u8g2.setFont(FONT_NUMBER);
  int16_t gems_x = X_HOME_1;
  int16_t gems_y = Y_HOME_1;

  u8g2.drawXBMP(gems_x, gems_y - 12, 16, 16, gem16_bitmap);  // adjust -16 if FONT_NUMBER is ~16px tall
  u8g2.setCursor(gems_x + X_HOME_2, gems_y);
  u8g2.print(gems);

  // countdown to next tap - [1, 0]
  char tapCountdown[16];
  fmt_mm_ss(v.msLeft, tapCountdown, sizeof(tapCountdown));
  u8g2.setFont(FONT_NUMBER);
  int16_t tapTime_x = X_HOME_1 + X_HOME_2;
  int16_t tapTime_y = Y_HOME_1 + Y_HOME_SPACE;

  // u8g2.drawXBMP(tx, ty - 15, 16, 16, countdown16_bitmap);
  // u8g2.setCursor(tx + 16 + 2, ty);
  u8g2.setCursor(tapTime_x, tapTime_y);
  u8g2.print(tapCountdown);

  // sleep time - [2, 0]
  char sleepTime[8];
  snprintf(sleepTime, sizeof(sleepTime), "%02u:%02u", (unsigned)v.sleepHour, (unsigned)v.sleepMinute);
  u8g2.setFont(FONT_NUMBER);
  int16_t sleepTime_x = X_HOME_1 + X_HOME_2;
  int16_t sleepTime_y = Y_HOME_1 + 2 * Y_HOME_SPACE;
  u8g2.drawXBMP(sleepTime_x - X_HOME_2, sleepTime_y - 13, 16, 16, moon16_bitmap);  // adjust -16 if FONT_NUMBER is ~16px tall
  u8g2.setCursor(sleepTime_x, sleepTime_y);
  u8g2.print(sleepTime);

  // wake time - [3, 0]
  char wakeTime[8];
  snprintf(wakeTime, sizeof(wakeTime), "%02u:%02u", (unsigned)v.wakeHour, (unsigned)v.wakeMinute);
  u8g2.setFont(FONT_NUMBER);
  int16_t wakeTime_x = X_HOME_1 + X_HOME_2;
  int16_t wakeTime_y = Y_HOME_1 + 3 * Y_HOME_SPACE;
  u8g2.drawXBMP(wakeTime_x - X_HOME_2, wakeTime_y - 13, 16, 16, clock16_bitmap);  // adjust -16 if FONT_NUMBER is ~16px tall
  u8g2.setCursor(wakeTime_x, wakeTime_y);
  u8g2.print(wakeTime);

  // tap duration - [0, 1]
  char tapDurationStr[12];
  snprintf(tapDurationStr, sizeof(tapDurationStr), "%u ms", (unsigned)v.tapDuration);
  int16_t tapDurationStr_x = X_HOME_1 + X_HOME_SPACE;
  int16_t tapDurationStr_y = Y_HOME_1;
  u8g2.setCursor(tapDurationStr_x, tapDurationStr_y);
  u8g2.print(tapDurationStr);

  // device enabled - [1, 1]
  const char* deviceOnOff;
  if (v.deviceEnabled) {
    deviceOnOff = "On";
    } else {
      deviceOnOff = "Off";
    }
  u8g2.setFont(FONT_NUMBER);
  int16_t deviceOnOff_x = X_HOME_1 + X_HOME_SPACE;
  int16_t deviceOnOff_y = Y_HOME_1 + Y_HOME_SPACE;
  u8g2.setCursor(deviceOnOff_x, deviceOnOff_y);
  u8g2.print(deviceOnOff);

  // override sleep - [2, 1]
  const char* overrideClockOnOff;
  if (v.overrideClock) {
    overrideClockOnOff = "OVR";
    } else {
      overrideClockOnOff = "";
    }
  u8g2.setFont(FONT_NUMBER);
  int16_t overrideClockOnOff_x = X_HOME_1 + X_HOME_SPACE;
  int16_t overrideClockOnOff_y = Y_HOME_1 + 2 * Y_HOME_SPACE;
  u8g2.setCursor(overrideClockOnOff_x, overrideClockOnOff_y);
  u8g2.print(overrideClockOnOff);

  // test mode - [3, 1]
  const char* testModeDisplay;
  if (v.testModeEnabled) {
    testModeDisplay = "TEST";
  } else {
    testModeDisplay = "";
  }
  u8g2.setFont(FONT_NUMBER);
  uint16_t testModeDisplay_x = X_HOME_1 + X_HOME_SPACE;
  uint16_t testModeDisplay_y = Y_HOME_1 + 3 * Y_HOME_SPACE;
  u8g2.setCursor(testModeDisplay_x, testModeDisplay_y);
  u8g2.print(testModeDisplay);
}

static void viewList(const MenuView& v) {
  const uint8_t visibleRows = 6;

  // ---- edge-stick scrolling ----
  // If selection moved above current window, scroll up.
  if (v.selected < s_first) {
    s_first = v.selected;
  }
  // If selection moved below current window, scroll down.
  else if (v.selected >= (uint8_t)(s_first + visibleRows)) {
    // move window just enough so selected sits on the bottom row
    s_first = (uint8_t)(v.selected - (visibleRows - 1));
  }

  // Clamp s_first so we don't run past the end
  if (v.itemCount <= visibleRows) {
    s_first = 0;
  } else {
    uint8_t maxFirst = (uint8_t)(v.itemCount - visibleRows);
    if (s_first > maxFirst) s_first = maxFirst;
  }

  // draw rows
  u8g2.setFont(FONT_BODY);
  uint8_t y = 0;
  for (uint8_t i = 0; i < visibleRows; ++i) {
    uint8_t idx = (uint8_t)(s_first + i);
    if (idx >= v.itemCount) break;

    bool selected = (idx == v.selected);
    const char* text = v.items[idx] ? v.items[idx] : "";

    const int xMarker = PAD;
    const int xText   = 8;  // leave room for "> "

    u8g2.setCursor(xMarker, y + LINE_H_BODY);
    u8g2.print(selected ? ">" : " ");

    u8g2.setCursor(xText, y + LINE_H_BODY);
    u8g2.print(text);

    const uint8_t iconW = 8;
    const uint8_t iconH = 8;
    const int16_t x = W - 18;
    const int16_t yIcon = y + LINE_H_BODY - 7;

    // idxes correspond to actions that don't prompt further input
    const unsigned char* bmp =
      (idx == 0 || idx == 1 || idx == 2 || idx == 7) ? return_bitmap : larrow_bitmap;

    u8g2.drawXBMP(x, yIcon, iconW, iconH, bmp);

    y += LINE_H_BODY + MARGIN;
  }
}

static void viewEditNumber(const MenuView& v) {
  // slots: 0 = Value, 1 = Save, 2 = Back, 3 = Home

  // ---------- Big centered value ----------
  char buf[24];
  if (v.unit && strcmp(v.unit, "ms") == 0) {
    snprintf(buf, sizeof(buf), "%lu ms", (unsigned long)v.value);
  } else if (v.unit && strcmp(v.unit, "gems") == 0) {
    char n[24];
    fmt_commas(v.value, n, sizeof(n));
    snprintf(buf, sizeof(buf), "%s", n);
  } else {
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)v.value);
  }

  const bool editingValue      = v.editing;  // actively editing the Value
  const bool showBottomMarkers = !v.editing; // hide all bottom '>' while editing

  // Measure number
  u8g2.setFont(FONT_NUMBER);
  int numW = u8g2.getStrWidth(buf);
  int numX = (W - numW) / 2;
  int numY = TAP_DURATION_Y;

  // When editing the value, show a big ">" next to the number with tight spacing
  if (editingValue) {
    const char* bigMarker = ">";
    u8g2.setFont(FONT_NUMBER);
    int markerW = u8g2.getStrWidth(bigMarker);
    int markerX = numX - markerW - 1;  // tighter padding
    if (markerX < PAD) markerX = PAD;
    int markerY = numY;
    u8g2.setCursor(markerX, markerY);
    u8g2.print(bigMarker);
  }

  // Draw the big value (no background box)
  u8g2.setFont(FONT_NUMBER);
  u8g2.setCursor(numX, numY);
  u8g2.print(buf);

  // ---------- Bottom choices in a 2×2 grid, centered (labels only) ----------
  // Visual layout:
  //  [Value(0)]   [Back(2)]
  //  [Save (1)]   [Home(3)]
  //
  // Centering uses ONLY label widths. The small '>' is drawn in a fixed gutter
  // at a constant offset to the left of each label to avoid any jitter.
  const char* choices[4] = { "Value", "Save", "Back", "Home" };

  // Row baselines near the bottom
  const int row1Y = H - 20;
  const int row2Y = H - 8;

  // Font and marker metrics
  u8g2.setFont(FONT_BODY);
  const int markerGutterW = u8g2.getStrWidth(">"); // fixed gutter width for all cells

  // Helper to measure label width (no marker)
  auto labelW = [&](int idx) -> int {
    return u8g2.getStrWidth(choices[idx] ? choices[idx] : "");
  };

  // Compute centered startX for a row (labels only)
  auto centeredStartX = [&](int idxLeft, int idxRight) -> int {
    int leftW  = labelW(idxLeft);
    int rightW = labelW(idxRight);
    int totalW = leftW + COLUMN_GAP + rightW;
    int startX = (W - totalW) / 2;
    if (startX < PAD + markerGutterW) startX = PAD + markerGutterW; // keep room for gutter
    return startX;
  };

  // Draw one row: labels centered; markers in fixed gutter to the left of each label
  auto drawCenteredRow = [&](int idxLeft, int idxRight, int y) {
    int startX  = centeredStartX(idxLeft, idxRight);
    int leftW   = labelW(idxLeft);
    int rightW  = labelW(idxRight);

    int leftX   = startX;
    int rightX  = startX + leftW + COLUMN_GAP;

    // LEFT cell
    {
      bool selLeft     = (v.selected == idxLeft);
      bool showMarker  = showBottomMarkers && selLeft;
      // Draw marker in fixed gutter (not included in centering)
      if (showMarker) {
        u8g2.setCursor(leftX - markerGutterW, y);
        u8g2.print(">");
      }
      u8g2.setCursor(leftX, y);
      u8g2.print(choices[idxLeft]);
    }

    // RIGHT cell
    {
      bool selRight    = (v.selected == idxRight);
      bool showMarker  = showBottomMarkers && selRight;
      if (showMarker) {
        u8g2.setCursor(rightX - markerGutterW, y);
        u8g2.print(">");
      }
      u8g2.setCursor(rightX, y);
      u8g2.print(choices[idxRight]);
    }
  };

  // Rows (swap Save/Back positions visually; order unchanged)
  drawCenteredRow(0, 2, row1Y); // Value (0), Back (2)
  drawCenteredRow(1, 3, row2Y); // Save  (1), Home (3)
}


static void viewEditTime(const MenuView& v) {
  // slots: 0 = Time, 1 = Save, 2 = Back, 3 = Home

  // ---------- Big centered time ----------
  char t[8];
  snprintf(t, sizeof(t), "%02u:%02u", (unsigned)v.hh, (unsigned)v.mm);

  const bool editingTime       = v.editingTime;       // actively editing time
  const bool showBottomMarkers = !v.editingTime;      // hide all bottom '>' while editing

  // Measure time string
  u8g2.setFont(FONT_NUMBER);
  int timeW = u8g2.getStrWidth(t);
  int timeX = (W - timeW) / 2;
  int timeY = CLOCK_Y;

  // When editing, show a big ">" next to the time (tight spacing)
  if (editingTime) {
    const char* bigMarker = ">";
    u8g2.setFont(FONT_NUMBER);
    int markerW = u8g2.getStrWidth(bigMarker);
    int markerX = timeX - markerW - 1;   // tight padding like viewEditNumber
    if (markerX < PAD) markerX = PAD;
    int markerY = timeY;
    u8g2.setCursor(markerX, markerY);
    u8g2.print(bigMarker);
  }

  // Draw the big time
  u8g2.setFont(FONT_NUMBER);
  u8g2.setCursor(timeX, timeY);
  u8g2.print(t);

  // While editing, underline the active field (HH or MM)
  if (editingTime) {
    int hhW    = u8g2.getStrWidth("00");  // approximate width for HH/MM
    int colonW = u8g2.getStrWidth(":");
    int ux     = v.editingHour ? timeX : timeX + hhW + colonW;
    int uw     = hhW;
    u8g2.drawHLine(ux, timeY + 3, uw);
  }

  // ---------- Bottom choices in a 2×2 grid, centered (labels only) ----------
  // Visual layout (order unchanged):
  //  [Time(0)]   [Back(2)]
  //  [Save(1)]   [Home(3)]
  const char* choices[4] = { "Time", "Save", "Back", "Home" };

  // Row baselines near the bottom
  u8g2.setFont(FONT_BODY);
  const int row1Y = H - 20;
  const int row2Y = H - 8;

  // Marker gutter is fixed width so labels never shift
  const int markerGutterW = u8g2.getStrWidth(">");

  // Measure label width (no marker)
  auto labelW = [&](int idx) -> int {
    return u8g2.getStrWidth(choices[idx] ? choices[idx] : "");
  };

  // Compute centered startX for a row (labels only)
  auto centeredStartX = [&](int idxLeft, int idxRight) -> int {
    int leftW  = labelW(idxLeft);
    int rightW = labelW(idxRight);
    int totalW = leftW + COLUMN_GAP + rightW;
    int startX = (W - totalW) / 2;
    if (startX < PAD + markerGutterW) startX = PAD + markerGutterW; // leave room for marker gutter
    return startX;
  };

  // Draw one centered row (labels centered; markers in fixed gutter to the left)
  auto drawCenteredRow = [&](int idxLeft, int idxRight, int y) {
    int startX = centeredStartX(idxLeft, idxRight);
    int leftW  = labelW(idxLeft);

    int leftX  = startX;
    int rightX = startX + leftW + COLUMN_GAP;

    // LEFT cell
    {
      bool selLeft    = (v.selected == idxLeft);
      bool showMarker = showBottomMarkers && selLeft;
      if (showMarker) {
        u8g2.setCursor(leftX - markerGutterW, y);
        u8g2.print(">");
      }
      u8g2.setCursor(leftX, y);
      u8g2.print(choices[idxLeft]);
    }

    // RIGHT cell
    {
      bool selRight   = (v.selected == idxRight);
      bool showMarker = showBottomMarkers && selRight;
      if (showMarker) {
        u8g2.setCursor(rightX - markerGutterW, y);
        u8g2.print(">");
      }
      u8g2.setCursor(rightX, y);
      u8g2.print(choices[idxRight]);
    }
  };

  // Rows (swap Save/Back visually; order unchanged)
  drawCenteredRow(0, 2, row1Y); // Time (0), Back (2)
  drawCenteredRow(1, 3, row2Y); // Save (1), Home (3)
}

// void ui12864_setBacklight(uint8_t pwm) {
//   analogWrite(LCD12864_LED, pwm);
// }


// API
void ui12864_menu_begin() {
  u8g2.begin();
  // u8g2.setBusClock(50000); // 50 kHz; uncomment if you see noise
  u8g2.clearBuffer();
  headerBar("Boot");
  drawLabelAt(PAD, LINE_H_TITLE + LINE_H_BODY, "Starting...");
  u8g2.sendBuffer();
  lcdDirty = true;
  nextFrameMs = 0;
}

void ui12864_lcd_backlight_begin(uint8_t ledPin) {
  pinMode(ledPin, OUTPUT);
  analogWrite(ledPin, 0); // full brightness
}

void ui12864_lcd_backlight_set(uint8_t ledPin, uint8_t level) {
  if (level > 255) level = 255;
  if (level < 0) level = 255;
  analogWrite(ledPin, level);
}

void ui12864_markDirty() {
  lcdDirty = true;  // true: update, false: skip
}

void ui12864_menu_render(const MenuView& v) {
  maybeMarkDirtyFromView(v);

  uint32_t now = millis();
  if (lcdDirty == false || now < nextFrameMs) return;
  
  u8g2.clearBuffer();

  switch (v.kind) {
    case ViewKind::Home:        viewHome(v);        break;
    case ViewKind::List:        viewList(v);        break;
    case ViewKind::EditNumber:  viewEditNumber(v);  break;
    case ViewKind::EditTime:    viewEditTime(v);    break;
  }
  u8g2.sendBuffer();

  lcdDirty = false;
  nextFrameMs = now + framePeriodMs;
}