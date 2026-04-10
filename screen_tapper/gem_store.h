#pragma once
#include <stdint.h>

// Initialize EEPROM gem store. Call once in setup().
void gem_store_begin();

// Read the persisted lifetime gem count from EEPROM.
uint32_t gem_store_read_lifetime();

// Add gems earned this session. Flushes to EEPROM automatically
// when the accumulated session total reaches kGemSaveThreshold.
// Returns the current lifetime + session total.
uint32_t gem_store_add_session(uint32_t gemsEarned);

// Returns the current lifetime + unflushed session total without modifying anything.
uint32_t gem_store_total();

// Overwrite the stored lifetime count directly (e.g. from the menu gem-count editor).
void gem_store_write_lifetime(uint32_t lifetime);

// Erase all gem data from EEPROM and reset session count to zero.
void gem_store_clear_all();
