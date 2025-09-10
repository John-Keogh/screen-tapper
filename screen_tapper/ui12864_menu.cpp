// Purpose: render the view provided by menu using U8g2.

#include "ui12864_menu.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "config_pins.h"

static U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, LCD12864_CLK, LCD12864_DAT, LCD12864_CS);

// fonts
static const uint8_t* FONT_TITLE   = u8g2_font_7x14B_tf;    // header
static const uint8_t* FONT_BODY    = u8g2_font_6x10_tf;     // normal text
static const uint8_t* FONT_NUMBER  = u8g2_font_logisoso18_tf;    // big numbers
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

// first visible row for list views
static uint8_t s_first = 0;
static const uint8_t kReturnIdx = 0;  // "Return to Home" item

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

// views
static void viewHome(const MenuView& v) {
  u8g2.drawFrame(0, 0, W, H);

  // big gem count in the middle
  char gems[24];
  fmt_commas(v.lifetimeGems, gems, sizeof(gems));

  u8g2.setFont(FONT_NUMBER);
  int16_t gemsW   = u8g2.getStrWidth(gems);
  int16_t totalW  = 16 + 2 + gemsW;  // 16 px icon + spacing
  int16_t gx = (W - totalW) / 2;
  int16_t gy = GEM_Y;

  // draw the icon baseline-aligned
  u8g2.drawXBMP(gx, gy - 15, 16, 16, gem16_bitmap);  // adjust -16 if FONT_NUMBER is ~16px tall

  u8g2.setCursor(gx + 16 + 2, gy);
  u8g2.print(gems);

  char t[16];
  fmt_mm_ss(v.msLeft, t, sizeof(t));
  u8g2.setFont(FONT_NUMBER);
  int16_t timeW = u8g2.getStrWidth(t);
  int16_t tx = (W - timeW) / 2;
  int16_t ty = H - 10;
  u8g2.setCursor(tx, ty);
  u8g2.print(t);
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
  headerBar(v.title);

  // slots: 0 is value; 1 is save; 2 is back; 3 is home
  // show big centered value with unit
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

  u8g2.setFont(FONT_NUMBER);
  int w = u8g2.getStrWidth(buf);
  int x = (W - w) / 2;
  int y = GEM_Y;
  if (v.editing) {
    // draw a light box behind the value to show "editing" state
    int h = 22;
    u8g2.drawRBox(x - MARGIN, y - h + GAP_LABEL_VALUE, w + GAP_LABEL_VALUE, h, 3);
    u8g2.setDrawColor(0);
    u8g2.setCursor(x, y);
    u8g2.print(buf);
    u8g2.setDrawColor(1);
  } else {
    u8g2.setCursor(x, y);
    u8g2.print(buf);
  }

  // soft options along bottom as selectable rows
  // render a tiny menu bar of choices indicating which is selected
  const char* choices[4] = { "Value", "Save", "Back", "Home" };
  int cx = PAD;
  int cy = H - 10;
  u8g2.setFont(FONT_BODY);
  for (uint8_t i = 0; i < 4; ++i) {
    const char* c = choices[i];
    int cw = u8g2.getStrWidth(c) + 4;
    if (v.selected == i) {
      u8g2.drawBox(cx - 1, cy - 9, cw + 2, 11);
      u8g2.setDrawColor(0);
      u8g2.setCursor(cx, cy);
      u8g2.print(c);
      u8g2.setDrawColor(1);
    } else {
      u8g2.setCursor(cx, cy);
      u8g2.print(c);
    }
    cx += cw + GAP_LABEL_VALUE;
  }
}

static void viewEditTime(const MenuView& v) {
  headerBar(v.title);

  // show HH:MM centered; highlight active field when editing
  char t[8];
  snprintf(t, sizeof(t), "%02u:%02u", (unsigned)v.hh, (unsigned)v.mm);
  u8g2.setFont(FONT_NUMBER);
  int w = u8g2.getStrWidth(t);
  int x = (W - w) / 2;
  int baseline = GEM_Y;

  // draw time
  u8g2.setCursor(x, baseline);
  u8g2.print(t);

  if (v.editingTime) {
    // underline HH or MM area
    int hhW = u8g2.getStrWidth("00"); // approximate width for HH
    int colonW = u8g2.getStrWidth(":");
    int mmW = hhW;

    int ux = v.editingHour ? x : x + hhW + colonW;
    int uw = v.editingHour ? hhW : mmW;
    u8g2.drawHLine(ux, baseline + 3, uw);
  }

  // bottom choices
  const char* choices[4] = { "Time", "Save", "Back", "Home" };
  int cx = PAD;
  int cy = H - 10;
  u8g2.setFont(FONT_BODY);
  for (uint8_t i = 0; i < 4; ++i) {
    const char* c = choices[i];
    int cw = u8g2.getStrWidth(c) + 4;
    if (v.selected == i) {
      u8g2.drawBox(cx - 1, cy - 9, cw + 2, 11);
      u8g2.setDrawColor(0);
      u8g2.setCursor(cx, cy);
      u8g2.print(c);
      u8g2.setDrawColor(1);
    } else {
      u8g2.setCursor(cx, cy);
      u8g2.print(c);
    }
    cx += cw + 4;
  }
}

// API
void ui12864_menu_begin() {
  u8g2.begin();
  // u8g2.setBusClock(50000); // 50 kHz; uncomment if you see noise
  u8g2.clearBuffer();
  headerBar("Boot");
  drawLabelAt(PAD, LINE_H_TITLE + LINE_H_BODY, "Starting...");
  u8g2.sendBuffer();
}

void ui12864_menu_render(const MenuView& v) {
  u8g2.clearBuffer();

  switch (v.kind) {
    case ViewKind::Home:        viewHome(v);        break;
    case ViewKind::List:        viewList(v);        break;
    case ViewKind::EditNumber:  viewEditNumber(v);  break;
    case ViewKind::EditTime:    viewEditTime(v);    break;
  }
  u8g2.sendBuffer();
}