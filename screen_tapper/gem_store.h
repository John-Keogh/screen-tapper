#pragma once
#include <stdint.h>

constexpr int GEMS_PER_SAVE = 25;

void gem_store_begin();
uint32_t gem_store_read_lifetime();
void gem_store_write_lifetime(uint32_t lifetime);
void gem_store_clear_all();