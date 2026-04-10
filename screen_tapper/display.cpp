// Purpose: render the MenuView onto the 128×64 ST7920 LCD using U8g2.

#include "display.h"
#include "config.h"
#include <Arduino.h>
#include <U8g2lib.h>

static U8G2_ST7920_128X64_F_SW_SPI u8g2(
    U8G2_R0, LCD12864_CLK, LCD12864_DAT, LCD12864_CS
);

// ---------------------------------------------------------------------------
// Fonts
// ---------------------------------------------------------------------------
static const uint8_t* FONT_TITLE  = u8g2_font_7x14B_tf;
static const uint8_t* FONT_BODY   = u8g2_font_6x10_tf;
static const uint8_t* FONT_NUMBER = u8g2_font_helvB10_tf;
static const uint8_t* FONT_SMALL  = u8g2_font_5x8_tf;

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------
static constexpr uint8_t W              = 128;
static constexpr uint8_t H              = 64;
static constexpr uint8_t PAD            = 2;
static constexpr uint8_t LINE_H_BODY    = 10;
static constexpr uint8_t LINE_H_TITLE   = 10;
static constexpr uint8_t MARGIN         = 0;
static constexpr uint8_t COLUMN_GAP     = 20;
static constexpr uint8_t TAP_DURATION_Y = 26;
static constexpr uint8_t CLOCK_Y        = 26;

// Home screen layout
static constexpr uint8_t X_HOME_1     = 0;    // icon column x
static constexpr uint8_t X_HOME_2     = 19;   // text column x offset
static constexpr uint8_t Y_HOME_1     = 13;   // first row y
static constexpr uint8_t X_HOME_SPACE = 80;   // second column x
static constexpr uint8_t Y_HOME_SPACE = 15;   // row spacing

// ---------------------------------------------------------------------------
// Frame rate limiting
// ---------------------------------------------------------------------------
static bool     lcdDirty     = true;
static uint32_t nextFrameMs  = 0;
static constexpr uint16_t kFramePeriodMs = 100;  // 10 FPS max

// First visible row for list views (scroll state)
static uint8_t s_listFirst = 0;

// ---------------------------------------------------------------------------
// Custom bitmaps (PROGMEM)
// ---------------------------------------------------------------------------
static const unsigned char larrow_bitmap[] U8X8_PROGMEM = {
    0b00000000, 0b00000000, 0b00010000, 0b00100000,
    0b01111110, 0b00100000, 0b00010000, 0b00000000
};

static const unsigned char return_bitmap[] U8X8_PROGMEM = {
    0b00000000, 0b00010000, 0b00111000, 0b01111100,
    0b00010000, 0b00011110, 0b00000000, 0b00000000
};

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

// ---------------------------------------------------------------------------
// Internal drawing helpers
// ---------------------------------------------------------------------------

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
        if (((len - i - 1) % 3) == 0 && i != len - 1 && j < (int)outSz - 1)
            out[j++] = ',';
    }
    out[j] = '\0';
}

static void fmt_mm_ss(uint32_t ms, char* out, size_t outSz) {
    uint32_t s  = ms / 1000UL;
    uint32_t mm = s / 60UL;
    uint32_t ss = s % 60UL;
    snprintf(out, outSz, "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);
}

static void drawSoftKeys(const char* left, const char* right) {
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

// ---------------------------------------------------------------------------
// View renderers
// ---------------------------------------------------------------------------

static void viewHome(const MenuView& v) {
    char buf[24];

    // --- Gem count (row 0, col 0) ---
    fmt_commas(v.lifetimeGems, buf, sizeof(buf));
    u8g2.setFont(FONT_NUMBER);
    u8g2.drawXBMP(X_HOME_1, Y_HOME_1 - 12, 16, 16, gem16_bitmap);
    u8g2.setCursor(X_HOME_1 + X_HOME_2, Y_HOME_1);
    u8g2.print(buf);

    // --- Countdown (row 1, col 0) ---
    fmt_mm_ss(v.msLeft, buf, sizeof(buf));
    u8g2.setFont(FONT_NUMBER);
    u8g2.setCursor(X_HOME_1 + X_HOME_2, Y_HOME_1 + Y_HOME_SPACE);
    u8g2.print(buf);

    // --- Sleep time (row 2, col 0) ---
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)v.sleepHour, (unsigned)v.sleepMinute);
    u8g2.setFont(FONT_NUMBER);
    u8g2.drawXBMP(X_HOME_1, Y_HOME_1 + 2 * Y_HOME_SPACE - 13, 16, 16, moon16_bitmap);
    u8g2.setCursor(X_HOME_1 + X_HOME_2, Y_HOME_1 + 2 * Y_HOME_SPACE);
    u8g2.print(buf);

    // --- Wake time (row 3, col 0) ---
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)v.wakeHour, (unsigned)v.wakeMinute);
    u8g2.setFont(FONT_NUMBER);
    u8g2.drawXBMP(X_HOME_1, Y_HOME_1 + 3 * Y_HOME_SPACE - 13, 16, 16, clock16_bitmap);
    u8g2.setCursor(X_HOME_1 + X_HOME_2, Y_HOME_1 + 3 * Y_HOME_SPACE);
    u8g2.print(buf);

    // --- Tap duration (row 0, col 1) ---
    snprintf(buf, sizeof(buf), "%u ms", (unsigned)v.tapDuration);
    u8g2.setFont(FONT_NUMBER);
    u8g2.setCursor(X_HOME_1 + X_HOME_SPACE, Y_HOME_1);
    u8g2.print(buf);

    // --- Device on/off (row 1, col 1) ---
    u8g2.setFont(FONT_NUMBER);
    u8g2.setCursor(X_HOME_1 + X_HOME_SPACE, Y_HOME_1 + Y_HOME_SPACE);
    u8g2.print(v.deviceEnabled ? "On" : "Off");

    // --- Override sleep indicator (row 2, col 1) ---
    u8g2.setFont(FONT_NUMBER);
    u8g2.setCursor(X_HOME_1 + X_HOME_SPACE, Y_HOME_1 + 2 * Y_HOME_SPACE);
    u8g2.print(v.overrideClock ? "OVR" : "");

    // --- Test mode indicator (row 3, col 1) ---
    u8g2.setFont(FONT_NUMBER);
    u8g2.setCursor(X_HOME_1 + X_HOME_SPACE, Y_HOME_1 + 3 * Y_HOME_SPACE);
    u8g2.print(v.testModeEnabled ? "TEST" : "");
}

static void viewList(const MenuView& v) {
    constexpr uint8_t visibleRows = 6;

    // Edge-stick scrolling
    if (v.selected < s_listFirst) {
        s_listFirst = v.selected;
    } else if (v.selected >= (uint8_t)(s_listFirst + visibleRows)) {
        s_listFirst = (uint8_t)(v.selected - (visibleRows - 1));
    }

    if (v.itemCount <= visibleRows) {
        s_listFirst = 0;
    } else {
        uint8_t maxFirst = (uint8_t)(v.itemCount - visibleRows);
        if (s_listFirst > maxFirst) s_listFirst = maxFirst;
    }

    u8g2.setFont(FONT_BODY);
    uint8_t y = 0;
    for (uint8_t i = 0; i < visibleRows; ++i) {
        uint8_t idx = (uint8_t)(s_listFirst + i);
        if (idx >= v.itemCount) break;

        bool        selected = (idx == v.selected);
        const char* text     = v.items[idx] ? v.items[idx] : "";

        u8g2.setCursor(PAD, y + LINE_H_BODY);
        u8g2.print(selected ? ">" : " ");
        u8g2.setCursor(8, y + LINE_H_BODY);
        u8g2.print(text);

        // Icon: return arrow for immediate-action items, forward arrow for sub-menus
        const unsigned char* bmp =
            (idx == 0 || idx == 1 || idx == 2 || idx == 8 || idx == 9)
                ? return_bitmap : larrow_bitmap;
        u8g2.drawXBMP(W - 18, y + LINE_H_BODY - 7, 8, 8, bmp);

        y += LINE_H_BODY + MARGIN;
    }
}

static void viewEditNumber(const MenuView& v) {
    // Format the current value
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

    // Center the value
    u8g2.setFont(FONT_NUMBER);
    int numW = u8g2.getStrWidth(buf);
    int numX = (W - numW) / 2;
    int numY = TAP_DURATION_Y;

    if (v.editing) {
        // Show active ">" marker while value is being edited
        const char* bigMarker = ">";
        int markerW = u8g2.getStrWidth(bigMarker);
        int markerX = numX - markerW - 1;
        if (markerX < PAD) markerX = PAD;
        u8g2.setCursor(markerX, numY);
        u8g2.print(bigMarker);
    }

    u8g2.setCursor(numX, numY);
    u8g2.print(buf);

    // Bottom 2×2 choice grid: [Value, Back] / [Save, Home]
    const char* choices[4] = { "Value", "Save", "Back", "Home" };
    const int   row1Y = H - 20;
    const int   row2Y = H - 8;

    u8g2.setFont(FONT_BODY);
    const int markerGW = u8g2.getStrWidth(">");

    auto labelW = [&](int idx) -> int {
        return u8g2.getStrWidth(choices[idx] ? choices[idx] : "");
    };
    auto centeredStartX = [&](int idxL, int idxR) -> int {
        int total = labelW(idxL) + COLUMN_GAP + labelW(idxR);
        int sx    = (W - total) / 2;
        if (sx < PAD + markerGW) sx = PAD + markerGW;
        return sx;
    };
    auto drawRow = [&](int idxL, int idxR, int y) {
        int sx    = centeredStartX(idxL, idxR);
        int leftX = sx;
        int rightX = sx + labelW(idxL) + COLUMN_GAP;
        bool showMarkers = !v.editing;

        if (showMarkers && v.selected == idxL) { u8g2.setCursor(leftX  - markerGW, y); u8g2.print(">"); }
        u8g2.setCursor(leftX,  y); u8g2.print(choices[idxL]);

        if (showMarkers && v.selected == idxR) { u8g2.setCursor(rightX - markerGW, y); u8g2.print(">"); }
        u8g2.setCursor(rightX, y); u8g2.print(choices[idxR]);
    };

    drawRow(0, 2, row1Y);  // Value | Back
    drawRow(1, 3, row2Y);  // Save  | Home
}

static void viewEditTime(const MenuView& v) {
    // Format and center the time
    char t[8];
    snprintf(t, sizeof(t), "%02u:%02u", (unsigned)v.hh, (unsigned)v.mm);

    u8g2.setFont(FONT_NUMBER);
    int timeW = u8g2.getStrWidth(t);
    int timeX = (W - timeW) / 2;
    int timeY = CLOCK_Y;

    if (v.editingTime) {
        const char* bigMarker = ">";
        int markerW = u8g2.getStrWidth(bigMarker);
        int markerX = timeX - markerW - 1;
        if (markerX < PAD) markerX = PAD;
        u8g2.setCursor(markerX, timeY);
        u8g2.print(bigMarker);
    }

    u8g2.setCursor(timeX, timeY);
    u8g2.print(t);

    // Underline active field while editing
    if (v.editingTime) {
        int hhW    = u8g2.getStrWidth("00");
        int colonW = u8g2.getStrWidth(":");
        int ux     = v.editingHour ? timeX : timeX + hhW + colonW;
        u8g2.drawHLine(ux, timeY + 3, hhW);
    }

    // Bottom 2×2 choice grid: [Time, Back] / [Save, Home]
    const char* choices[4] = { "Time", "Save", "Back", "Home" };
    const int   row1Y = H - 20;
    const int   row2Y = H - 8;

    u8g2.setFont(FONT_BODY);
    const int markerGW = u8g2.getStrWidth(">");

    auto labelW = [&](int idx) -> int {
        return u8g2.getStrWidth(choices[idx] ? choices[idx] : "");
    };
    auto centeredStartX = [&](int idxL, int idxR) -> int {
        int total = labelW(idxL) + COLUMN_GAP + labelW(idxR);
        int sx    = (W - total) / 2;
        if (sx < PAD + markerGW) sx = PAD + markerGW;
        return sx;
    };
    auto drawRow = [&](int idxL, int idxR, int y) {
        int sx     = centeredStartX(idxL, idxR);
        int leftX  = sx;
        int rightX = sx + labelW(idxL) + COLUMN_GAP;
        bool showMarkers = !v.editingTime;

        if (showMarkers && v.selected == idxL) { u8g2.setCursor(leftX  - markerGW, y); u8g2.print(">"); }
        u8g2.setCursor(leftX,  y); u8g2.print(choices[idxL]);

        if (showMarkers && v.selected == idxR) { u8g2.setCursor(rightX - markerGW, y); u8g2.print(">"); }
        u8g2.setCursor(rightX, y); u8g2.print(choices[idxR]);
    };

    drawRow(0, 2, row1Y);  // Time | Back
    drawRow(1, 3, row2Y);  // Save | Home
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

static void drawCurrentView(const MenuView& v) {
    u8g2.clearBuffer();
    switch (v.kind) {
        case ViewKind::Home:       viewHome(v);       break;
        case ViewKind::List:       viewList(v);       break;
        case ViewKind::EditNumber: viewEditNumber(v); break;
        case ViewKind::EditTime:   viewEditTime(v);   break;
    }
    u8g2.sendBuffer();
}

void display_begin() {
    u8g2.setBusClock(2000000);  // 2 MHz — ST7920 is rated for 2.5 MHz max.
                                 // Cuts software-SPI transfer time ~4x vs default.
    u8g2.begin();
    u8g2.clearBuffer();
    headerBar("Boot");
    drawLabelAt(PAD, LINE_H_TITLE + LINE_H_BODY, "Starting...");
    u8g2.sendBuffer();
    lcdDirty    = true;
    nextFrameMs = 0;
}

void display_markDirty() {
    lcdDirty = true;
}

void display_renderNow(const MenuView& v) {
    // Bypass the frame-rate limiter entirely. Used after input events so
    // the user sees the response immediately without waiting for the next
    // rate-limited window. Resets the limiter so we don't double-draw.
    drawCurrentView(v);
    lcdDirty    = false;
    nextFrameMs = millis() + kFramePeriodMs;
}

void display_render(const MenuView& v) {
    // Rate-limited path for auto-updates (countdown tick, gem count, etc.).
    // Skips the draw if nothing changed or the limiter hasn't elapsed yet.
    uint32_t now = millis();
    if (!lcdDirty || now < nextFrameMs) return;

    drawCurrentView(v);
    lcdDirty    = false;
    nextFrameMs = now + kFramePeriodMs;
}
