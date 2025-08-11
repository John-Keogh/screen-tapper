// Move: the EEPROM slot/index logic, checksums, lifetime/session update rules.

// In .ino: replace direct EEPROM references with clean calls like “get lifetime”, “add to session”, and “flush if needed”.

// Compile/test: print the totals and confirm no regression.

#pragma once
#include <stdint.h>

void gem_store_begin();
uint32_t gem_store_read_lifetime();
void gem_store_write_lifetime(uint32_t lifetime);
void gem_store_clear_all();