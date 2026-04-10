#pragma once
#include <stdint.h>
#include "menu.h"

// Initialize the 128×64 LCD renderer.
void display_begin();

// Mark the display as needing a redraw.
void display_markDirty();

// Render immediately, bypassing the frame-rate limiter.
// Use after input events for instant visual feedback.
void display_renderNow(const MenuView& v);

// Render the current MenuView, rate-limited to kFramePeriodMs.
// Safe to call every loop(). Use for auto-updates (countdown, gem count).
void display_render(const MenuView& v);
