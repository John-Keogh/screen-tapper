#pragma once
#include <stdint.h>
#include "menu.h"

// Initialize the 128x64 UI renderer (fonts, cache, timing)
void ui12864_menu_begin();

// Turn on the LCD backlight
void ui12864_lcd_backlight_begin(uint8_t ledPin);

// Control LCD backlight brightness
void ui12864_lcd_backlight_set(uint8_t ledPin, uint8_t level);

// Force a redraw on next allowed frame (rarely needed; most redraws are auto)
void ui12864_markDirty();

// Render current view if (a) something changed OR (b) frame limiter allows.
// Safe to call every loop; will early-return when nothing to do.
void ui12864_menu_render(const MenuView& v);
