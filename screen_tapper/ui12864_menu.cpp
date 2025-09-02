// Purpose: render the view provided by menu using U8g2.

#include "ui12864_menu.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "config_pins.h"

static U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, LCD12864_CLK, LCD12864_DAT, LCD12864_CS);

// fonts
static const uint8_t* FONT_TITLE   = u8g2_font_7x14B_tf;    // header
static const uint8_t* FONT_BODY    = u8g2_font_6x10_tf;     // normal text
static const uint8_t* FONT_NUMBER  = u8g2_font_10x20_tf;    // big numbers
static const uint8_t* FONT_SMALL   = u8g2_font_5x8_tf;      // tiny helpers

// layout
static const uint8_t W            = 128;
static const uint8_t H            = 64;
static const uint8_t PAD          = 2;
static const uint8_t LINE_H_BODY  = 10;
static const uint8_t LINE_H_TITLE = 14;