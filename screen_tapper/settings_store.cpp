#include "settings_store.h"
#include <EEPROM.h>

// EEPROM addresses within the settings region (bytes 2-255)
static constexpr uint16_t ADDR_SLEEP_HOUR    = 2;
static constexpr uint16_t ADDR_SLEEP_MINUTE  = 3;
static constexpr uint16_t ADDR_WAKE_HOUR     = 4;
static constexpr uint16_t ADDR_WAKE_MINUTE   = 5;
static constexpr uint16_t ADDR_TAP_DURATION  = 6;  // 2 bytes (uint16_t)
static constexpr uint16_t ADDR_TAP_DUTY      = 8;  // 1 byte  (uint8_t)

static bool validHour(uint8_t v)        { return v <= 23; }
static bool validMinute(uint8_t v)      { return v <= 59; }
static bool validDuration(uint16_t v)   { return v >= 1 && v <= 1000; }
// tapDuty is uint8_t, so any value 0-255 is valid; 0xFF is the only
// uninitialized sentinel we need to detect. We treat 0xFF as uninitialized
// since a duty of 255 can be stored as 0xFF -- but 255 is a valid setting.
// To disambiguate, we rely on duration validity: if duration is valid, we
// trust the duty byte as-is (including 255). If duration is invalid, we
// skip both tap settings together, keeping compiled-in defaults.

void settings_load(Settings& s) {
    // --- Sleep/wake schedule ---
    uint8_t sh = EEPROM.read(ADDR_SLEEP_HOUR);
    uint8_t sm = EEPROM.read(ADDR_SLEEP_MINUTE);
    uint8_t wh = EEPROM.read(ADDR_WAKE_HOUR);
    uint8_t wm = EEPROM.read(ADDR_WAKE_MINUTE);

    if (validHour(sh) && validMinute(sm) && validHour(wh) && validMinute(wm)) {
        s.sched.sleepHour   = sh;
        s.sched.sleepMinute = sm;
        s.sched.wakeHour    = wh;
        s.sched.wakeMinute  = wm;
    }

    // --- Tap duration and duty ---
    // Both are loaded or neither is — if duration is out of range the EEPROM
    // is uninitialized or corrupt, so we leave both at compiled-in defaults.
    uint16_t dur = 0;
    EEPROM.get(ADDR_TAP_DURATION, dur);
    if (validDuration(dur)) {
        s.tapDuration = dur;
        s.tapDuty     = EEPROM.read(ADDR_TAP_DUTY);
        // tapDuty has no invalid range (0-255 are all valid), so we accept
        // whatever is stored once we know the duration slot is initialized.
    }
}

void settings_save(const Settings& s) {
    // EEPROM.update() skips the physical write when the stored byte already
    // matches, protecting the ~100,000 write-cycle lifetime of each cell.
    EEPROM.update(ADDR_SLEEP_HOUR,   s.sched.sleepHour);
    EEPROM.update(ADDR_SLEEP_MINUTE, s.sched.sleepMinute);
    EEPROM.update(ADDR_WAKE_HOUR,    s.sched.wakeHour);
    EEPROM.update(ADDR_WAKE_MINUTE,  s.sched.wakeMinute);
    EEPROM.put(ADDR_TAP_DURATION,    s.tapDuration);  // put() handles uint16_t
    EEPROM.update(ADDR_TAP_DUTY,     s.tapDuty);
}
