#pragma once
#include <stdint.h>
#include "menu.h"

// initialize
void ui12864_menu_begin();

// render current view
void ui12864_menu_render(const MenuView& v);