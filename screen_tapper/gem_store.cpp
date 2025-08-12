#include "gem_store.h"
#include <Arduino.h>
#include <EEPROM.h>

// ------- Layout -------
// [0..1]   : uint16_t slot index (last written slot)
// [2..255] : reserved (settings)
// [256.. ] : ring buffer of slots, each 5 bytes:
//            [0..3] uint32_t lifetime count
//            [4]    uint8_t  XOR checksum of the 4 data bytes

static constexpr uint16_t SLOT_INDEX_ADDR   = 0;
static constexpr uint16_t SETTINGS_START    = 2;
static constexpr uint16_t GEM_SLOTS_START   = 256;
static constexpr uint8_t  BYTES_PER_SLOT    = 5;

static inline uint16_t eeprom_len() {
  return EEPROM.length();
}

static inline uint16_t usable_bytes() {
  uint16_t len = eeprom_len();
  return (len > GEM_SLOTS_START) ? (len - GEM_SLOTS_START) : 0;
}

static inline uint16_t max_slots() {
  uint16_t u = usable_bytes();
  return (u / BYTES_PER_SLOT);
}

static inline uint8_t checksum32(uint32_t v) {
  return (uint8_t)((v & 0xFF) ^ ((v >> 8) & 0xFF) ^ ((v >> 16) & 0xFF) ^ ((v >> 24) & 0xFF));
}

static inline uint16_t slot_addr(uint16_t slotIdx) {
  return (uint16_t)(GEM_SLOTS_START + (slotIdx * BYTES_PER_SLOT));
}

static uint16_t sanitize_slot_index(uint16_t idx) {
  // ensure slot index is sane
  // if uninitialized (0xFFFF) or > max, set to 0
  uint16_t m = max_slots();
  if (m == 0) return 0;
  if (idx == 0xFFFF || idx >= m) return 0;
  return idx;
}

void gem_store_begin() {
  // check that there's room for at least one slot
  if (max_slots() == 0) return;

  uint16_t storedSlot;
  EEPROM.get(SLOT_INDEX_ADDR, storedSlot);

  // if uninitialized or invalid, set to 0
  uint16_t fixed = sanitize_slot_index(storedSlot);
  if (fixed != storedSlot) {
    EEPROM.put(SLOT_INDEX_ADDR, (uint16_t)0);
  }
}

uint32_t gem_store_read_lifetime() {
  uint16_t m = max_slots();
  if (m == 0) return 0;

  uint16_t lastSlot;
  EEPROM.get(SLOT_INDEX_ADDR, lastSlot);
  lastSlot = sanitize_slot_index(lastSlot);

  // read current slot
  uint16_t addr = slot_addr(lastSlot);
  uint32_t value = 0;
  EEPROM.get(addr, value);
  uint8_t chk = EEPROM.read(addr + BYTES_PER_SLOT - 1);

  // if brand new, treat as 0
  if (value == 0xFFFFFFFFUL) {
    return 0;
  }

  uint8_t want = checksum32(value);

  // if checksum missing or matches, accept value
  if (chk == 0xFF || chk == want) {
    return value;
  }

  // fallback: try previous slot (if any)
  if (lastSlot > 0) {
    uint16_t prevAddr = slot_addr(lastSlot - 1);
    uint32_t prevVal = 0;
    EEPROM.get(prevAddr, prevVal);
    uint8_t prevChk = EEPROM.read(prevAddr + BYTES_PER_SLOT - 1);
    if (prevChk == checksum32(prevVal)) {
      return prevVal;
    }
  }

  // if all else fails (corrupt and no valid fallback)
  return 0;
}


void gem_store_write_lifetime(uint32_t lifetime) {
  uint16_t m = max_slots();
  if (m == 0) return;

  uint16_t lastSlot;
  EEPROM.get(SLOT_INDEX_ADDR, lastSlot);
  lastSlot = sanitize_slot_index(lastSlot);

  uint16_t nextSlot = (uint16_t)((lastSlot + 1) % m);
  uint16_t addr     = slot_addr(nextSlot);

  // Write value + checksum
  EEPROM.put(addr, lifetime);
  uint8_t chk = checksum32(lifetime);
  EEPROM.put((uint16_t)(addr + BYTES_PER_SLOT - 1), chk);

  // Update last slot index
  EEPROM.put(SLOT_INDEX_ADDR, nextSlot);
}

void gem_store_clear_all() {
  // clear only the gem slots region to 0xFF
  // reset slot index
  uint16_t len = eeprom_len();
  for (uint16_t i = GEM_SLOTS_START; i < len; ++i) {
    EEPROM.update(i, 0xFF);
  }
  EEPROM.put(SLOT_INDEX_ADDR, (uint16_t)0);
}