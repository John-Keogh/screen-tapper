#include "gem_store.h"
#include "config.h"
#include <Arduino.h>
#include <EEPROM.h>

// =============================================================================
// EEPROM Layout
// [0..1]   uint16_t  index of last-written slot
// [2..255] reserved for future settings
// [256..]  ring buffer of slots, each 5 bytes:
//            [0..3] uint32_t lifetime gem count
//            [4]    uint8_t  XOR checksum of the 4 data bytes
// =============================================================================

static constexpr uint16_t SLOT_INDEX_ADDR = 0;
static constexpr uint16_t GEM_SLOTS_START = 256;
static constexpr uint8_t  BYTES_PER_SLOT  = 5;

// Session state (not persisted)
static uint32_t s_lifetimeGems = 0;
static uint32_t s_sessionGems  = 0;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static uint16_t usableBytes() {
    uint16_t len = EEPROM.length();
    return (len > GEM_SLOTS_START) ? (len - GEM_SLOTS_START) : 0;
}

static uint16_t maxSlots() {
    return usableBytes() / BYTES_PER_SLOT;
}

static uint8_t checksum32(uint32_t v) {
    return (uint8_t)((v & 0xFF) ^ ((v >> 8) & 0xFF) ^ ((v >> 16) & 0xFF) ^ ((v >> 24) & 0xFF));
}

static uint16_t slotAddr(uint16_t slotIdx) {
    return (uint16_t)(GEM_SLOTS_START + slotIdx * BYTES_PER_SLOT);
}

static uint16_t sanitizeSlotIndex(uint16_t idx) {
    uint16_t m = maxSlots();
    if (m == 0 || idx == 0xFFFF || idx >= m) return 0;
    return idx;
}

static uint32_t readLifetimeFromEEPROM() {
    uint16_t m = maxSlots();
    if (m == 0) return 0;

    uint16_t lastSlot;
    EEPROM.get(SLOT_INDEX_ADDR, lastSlot);
    lastSlot = sanitizeSlotIndex(lastSlot);

    uint16_t addr  = slotAddr(lastSlot);
    uint32_t value = 0;
    EEPROM.get(addr, value);
    uint8_t chk = EEPROM.read(addr + BYTES_PER_SLOT - 1);

    if (value == 0xFFFFFFFFUL) return 0;  // blank/unwritten slot

    if (chk == 0xFF || chk == checksum32(value)) return value;

    // Checksum mismatch — try the previous slot as fallback
    if (lastSlot > 0) {
        uint16_t prevAddr = slotAddr(lastSlot - 1);
        uint32_t prevVal  = 0;
        EEPROM.get(prevAddr, prevVal);
        uint8_t prevChk = EEPROM.read(prevAddr + BYTES_PER_SLOT - 1);
        if (prevChk == checksum32(prevVal)) return prevVal;
    }

    return 0;
}

static void writeLifetimeToEEPROM(uint32_t lifetime) {
    uint16_t m = maxSlots();
    if (m == 0) return;

    uint16_t lastSlot;
    EEPROM.get(SLOT_INDEX_ADDR, lastSlot);
    lastSlot = sanitizeSlotIndex(lastSlot);

    uint16_t nextSlot = (uint16_t)((lastSlot + 1) % m);
    uint16_t addr     = slotAddr(nextSlot);

    EEPROM.put(addr, lifetime);
    uint8_t chk = checksum32(lifetime);
    EEPROM.put((uint16_t)(addr + BYTES_PER_SLOT - 1), chk);
    EEPROM.put(SLOT_INDEX_ADDR, nextSlot);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void gem_store_begin() {
    if (maxSlots() == 0) return;

    uint16_t storedSlot;
    EEPROM.get(SLOT_INDEX_ADDR, storedSlot);
    uint16_t fixed = sanitizeSlotIndex(storedSlot);
    if (fixed != storedSlot) {
        EEPROM.put(SLOT_INDEX_ADDR, (uint16_t)0);
    }

    s_lifetimeGems = readLifetimeFromEEPROM();
    s_sessionGems  = 0;
}

uint32_t gem_store_read_lifetime() {
    return s_lifetimeGems;
}

uint32_t gem_store_add_session(uint32_t gemsEarned) {
    s_sessionGems += gemsEarned;

    if (s_sessionGems >= kGemSaveThreshold) {
        s_lifetimeGems += s_sessionGems;
        writeLifetimeToEEPROM(s_lifetimeGems);
        s_sessionGems = 0;
    }

    return s_lifetimeGems + s_sessionGems;
}

uint32_t gem_store_total() {
    return s_lifetimeGems + s_sessionGems;
}

void gem_store_write_lifetime(uint32_t lifetime) {
    s_lifetimeGems = lifetime;
    s_sessionGems  = 0;
    writeLifetimeToEEPROM(lifetime);
}

void gem_store_clear_all() {
    uint16_t len = EEPROM.length();
    for (uint16_t i = GEM_SLOTS_START; i < len; ++i) {
        EEPROM.update(i, 0xFF);
    }
    EEPROM.put(SLOT_INDEX_ADDR, (uint16_t)0);
    s_lifetimeGems = 0;
    s_sessionGems  = 0;
}
