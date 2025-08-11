#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include "ui.h"
#include "time_settings.h"
#include "config_pins.h"
#include "input.h"
#include "clock.h"
#include "tapper.h"

uint16_t tapDuration = 13;
SleepSchedule sched;


// ==== LCD ====
unsigned long lastDisplayChange = 0;
const unsigned long displayInterval = 2500;
int displayState = 0;
const int numDisplayStates = 2;

// ==== Button Logic ====
bool deviceEnabled = true;
bool testModeEnabled = false;
bool tapDurationUpMessageActive = false;
unsigned long tapDurationUpMessageStart = 0;
bool tapDurationDownMessageActive = false;
unsigned long tapDurationDownMessageStart = 0;
bool overrideClock = false;

// ==== Solenoids ====
unsigned long nextTapTime = 0;
unsigned long lastTapTime = 0;
bool tappingActive = false;
bool adGemsComplete = false;
int currentTap = 0;
bool solenoidOn = false;
unsigned long tapPhaseStart = 0;
int tappingPin = 0;
int totalTaps = 0;

// ==== Mode Parameters ====
#include "mode_params.h"

unsigned long baseInterval = baseIntervalActual;
unsigned long jitterRange = jitterRangeActual;
unsigned long pauseBetweenTaps = pauseBetweenTapsActual;
unsigned long adGemTaps = adGemTapsActual;
unsigned long floatGemTaps = floatGemTapsActual;



// ==== Override Clock Message ====
bool showOverrideClockMessage = false;
unsigned long overrideClockMessageStart = 0;

// ==== Gem Tracking (EEPROM) ====
// EEPROM layout:
//  - 0–1: uint16_t slot index (last used slot)
//  - 2–255: reserved for future settings/data
//  - 256–4095: gem count history (each 4 bytes)
const uint16_t SETTINGS_START = 2;
const uint16_t SETTINGS_SIZE = 254;
const uint16_t SLOT_INDEX_ADDR = 0;
const uint16_t GEM_SLOTS_START = 256;
const uint8_t BYTES_PER_SLOT = 5; // 4 bytes (gem count) + 1 byte (corruption detection) = 5 bytes
const uint16_t USABLE_EEPROM = EEPROM.length() - GEM_SLOTS_START; // 4096 - 256 = 3840 bytes available for gem data
const uint16_t MAX_GEM_SLOTS = USABLE_EEPROM / BYTES_PER_SLOT;  // 3840 / 5 = 768 slots
const uint8_t GEMS_PER_SAVE = 25; // min number of gems before updating lifetime EEPROM count
uint32_t sessionGemCount = 0;
uint32_t displayedGemCount = 0;
int activationCount = 0;
uint32_t lifetimeGemCount = 0; // initialization - will be read from EEPROM later


void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(0));

  clock_begin();
  bool rtcAvailable = clock_available(); // this isn't used for anything?

  tapper_begin(AD_GEMS_MOSFET_GATE_PIN, FLOAT_GEMS_MOSFET_GATE_PIN);

  ui_begin();
  ui_setMode(UIMode::ACTIVE_MODE);

  input_begin();
  
  delay(1000);

  // Initialization for lifetime gem count - only for first time use
  uint16_t storedSlot;
  EEPROM.get(SLOT_INDEX_ADDR, storedSlot);
  if (storedSlot == 0xFFFF) {  // both bytes are 0xFF
    Serial.println("EEPROM uninitialized. Initializing slot index to 0.");
    EEPROM.put(SLOT_INDEX_ADDR, (uint16_t)0);
  } else {
    Serial.println("EEPROM already initialized.");
    // // uncomment if manually resetting EEPROM lifetime gem data
    // Serial.println("Resetting EEPROM lifetime gem data.");
    // clearGemCountEEPROM();
    Serial.print("Slot Index Address: ");
    Serial.println(storedSlot);
  }

  lifetimeGemCount = readLifetimeGemCount();
  Serial.print("Lifetime Gem Count: ");
  Serial.println(lifetimeGemCount);
  Serial.println("");

  scheduleNextTap();
}

// --------- LOOP FUNCTION ---------
void loop() {
  unsigned long now = millis();

  tapper_update(now);

  bool awake = overrideClock || clock_isAwake(sched);
  if (!tapper_isActive() && deviceEnabled && awake && now >= nextTapTime) {
    tapper_startCycle(
      adGemTaps,
      floatGemTaps,
      tapDuration,
      pauseBetweenTaps,
      now
    );

    sessionGemCount += 5;
    activationCount++;
    if (activationCount % 6 == 0) {
      sessionGemCount += 2;
      activationCount = 0;
    }

    scheduleNextTap();
  }

  InputEvents events = input_poll();

  // On/off
  if (events.onOffPressed) {
    deviceEnabled = !deviceEnabled;
    ui_setMode(deviceEnabled ? UIMode::ACTIVE_MODE : UIMode::OFF_MODE);
    if (deviceEnabled) scheduleNextTap();   // when turning back on
    return;  // early exit to avoid screen churn this frame
  }

  // Test mode toggle
  if (events.testModePressed) {
    testModeEnabled = !testModeEnabled;
    if (!testModeEnabled) {
      baseInterval = baseIntervalActual;
      jitterRange = jitterRangeActual;
      pauseBetweenTaps = pauseBetweenTapsActual;
      adGemTaps = adGemTapsActual;
      floatGemTaps = floatGemTapsActual;
    } else {
      baseInterval = baseIntervalTest;
      jitterRange = jitterRangeTest;
      pauseBetweenTaps = pauseBetweenTapsTest;
      adGemTaps = adGemTapsTest;
      floatGemTaps = floatGemTapsTest;
    }
    ui_showTestMode(testModeEnabled);
    scheduleNextTap();   // re‑seed timing for the new mode
    return;
  }

  // Tap duration up
  if (events.tapUpPressed) {
    tapDuration++;
    ui_showTapDuration(tapDuration);
    tapDurationUpMessageActive = true;
    tapDurationUpMessageStart = now;
    return;
  }

  // Tap duration down
  if (events.tapDownPressed) {
    if (tapDuration > 1) tapDuration--;
    ui_showTapDuration(tapDuration);
    tapDurationDownMessageActive = true;
    tapDurationDownMessageStart = now;
    return;
  }

  // Override clock
  if (events.overridePressed) {
    overrideClock = !overrideClock;
    ui_showOverride(overrideClock);
    if (overrideClock) ui_setBacklight(255);
    return;
  }

  // Emergency stop
  if (!deviceEnabled) {
    tapper_stop();
  }

  if (!deviceEnabled) {
    ui_setMode(UIMode::OFF_MODE);
    ui_showOff();
    return;
  }

  if (!overrideClock && !awake) {
    ui_setMode(UIMode::SLEEP_MODE);
    ui_setBacklight(0);
    ui_showSleep(sched.wakeHour, sched.wakeMinute);
    return;
  }

  ui_setMode(UIMode::ACTIVE_MODE);
  ui_setBacklight(255);

  if (tapper_isActive()) {
    ui_showTapping();
    return;
  }

  // Rotating “idle” screens (countdown / lifetime gems)
  if (now - lastDisplayChange >= displayInterval) {
    displayState = (displayState + 1) % numDisplayStates;
    lastDisplayChange = now;
  }

  if (displayState == 0) {
    unsigned long timeLeft = (nextTapTime > now) ? (nextTapTime - now) : 0;
    ui_showNextTapCountdown(timeLeft);
  } else {
    displayedGemCount = lifetimeGemCount + sessionGemCount;
    ui_showLifetimeGems(displayedGemCount);
  }

  if (sessionGemCount >= GEMS_PER_SAVE) {
    lifetimeGemCount += sessionGemCount;
    saveGemCount(lifetimeGemCount);
    sessionGemCount = 0;
  }
}

void scheduleNextTap() {
  long jitter = random(-jitterRange, jitterRange);

  if (random(12) == 0 && !testModeEnabled) {
    // Generate a skip interval between 3 and 8 minutes (in milliseconds)
    unsigned long randomSkip = random(3UL * 60 * 1000, 8UL * 60 * 1000);
    
    unsigned long base = millis() + baseInterval + randomSkip;
    long adjusted = (long)base + jitter;

    if (adjusted < 0) adjusted = 0;

    nextTapTime = (unsigned long)adjusted;

  } else {
    long adjusted = (long)(millis() + baseInterval) + jitter;
    if (adjusted < 0) adjusted = 0;
    nextTapTime = (unsigned long)adjusted;
  }
}

void startTapSequence(int mosfetPin, int numTaps) {
  tappingActive = true;
  currentTap = 0;
  solenoidOn = false;
  tapPhaseStart = millis();
  tappingPin = mosfetPin;
  totalTaps = numTaps;
  // ui_clear();
}

void updateTapSequence(unsigned long now) {
  if (!tappingActive) return;

  if (!solenoidOn && now - tapPhaseStart >= pauseBetweenTaps) {
    // Turn solenoid on
    // Serial.print("Solenoid: ");
    // Serial.println(tappingPin);
    digitalWrite(tappingPin, HIGH);
    solenoidOn = true;
    tapPhaseStart = now;
  }
  else if (solenoidOn && now - tapPhaseStart >= tapDuration) {
    // Turn solenoid off
    digitalWrite(tappingPin, LOW);
    solenoidOn = false;
    tapPhaseStart = now;
    currentTap++;

    if (currentTap >= totalTaps) {
      tappingActive = false;

      // Transition to next solenoid
      if (!adGemsComplete) {
        adGemsComplete = true;
        startTapSequence(FLOAT_GEMS_MOSFET_GATE_PIN, floatGemTaps);
      } else {
        adGemsComplete = false;
      }
    }
  }
}

uint32_t readLifetimeGemCount() {
  // retrieve latest slot index
  uint16_t lastSlotIndex;
  EEPROM.get(SLOT_INDEX_ADDR, lastSlotIndex);

  // fallback in case of corruption
  if (lastSlotIndex >= MAX_GEM_SLOTS) {
    lastSlotIndex = 0;
    uint32_t savedCount = 0;
    EEPROM.get(GEM_SLOTS_START, savedCount);
    return savedCount;
  }

  // load last stored gem count
  uint16_t readAddress = GEM_SLOTS_START + (lastSlotIndex * BYTES_PER_SLOT);
  uint32_t savedCount = 0;
  EEPROM.get(readAddress, savedCount);

  if (savedCount == 0xFFFFFFFF) {
    return 0; // special case when uninitialized
  }

  // load last stored checksum
  uint8_t storedChecksum = EEPROM.read(readAddress + BYTES_PER_SLOT - 1);

  // exit early if no checksum detected (only occurs during first use)
  if (storedChecksum == 0xFF) {
    return savedCount;
  }

  // compute checksum based on loaded gem count
  uint8_t computedChecksum = (savedCount & 0xFF) ^
                              ((savedCount >> 8) & 0xFF) ^
                              ((savedCount >> 16) & 0xFF) ^
                              ((savedCount >> 24) & 0xFF);

  // compare computed checksum with stored checksum to check for corruption
  if (computedChecksum != storedChecksum) {
    // fallback: try previous slot (if one exists)
    if (lastSlotIndex > 0) {
      uint16_t previousAddress = GEM_SLOTS_START + ((lastSlotIndex - 1) * BYTES_PER_SLOT);
      uint32_t previousSavedCount = 0;
      EEPROM.get(previousAddress, previousSavedCount);
      uint8_t previousChecksum = EEPROM.read(previousAddress + BYTES_PER_SLOT - 1);
      uint8_t previousComputedChecksum = (previousSavedCount & 0xFF) ^
                                          ((previousSavedCount >> 8) & 0xFF) ^
                                          ((previousSavedCount >> 16) & 0xFF) ^
                                          ((previousSavedCount >> 24) & 0xFF);
      if (previousChecksum == previousComputedChecksum) {
        return previousSavedCount; // successful fallback
      }
    }
    return 0; // corruption detected with no valid fallback
  }

  return savedCount;
}

void saveGemCount(uint32_t countToSave) {
  uint16_t lastSlotIndex ;
  EEPROM.get(SLOT_INDEX_ADDR, lastSlotIndex);
  if (lastSlotIndex >= MAX_GEM_SLOTS) {
    lastSlotIndex = 0;  // fallback in case of corruption
  }
  uint8_t checksum = (countToSave & 0xFF) ^
                      ((countToSave >> 8) & 0xFF) ^
                      ((countToSave >> 16) & 0xFF) ^
                      ((countToSave >> 24) & 0xFF);

  uint8_t nextSlotIndex = (lastSlotIndex + 1) % MAX_GEM_SLOTS;
  uint16_t writeAddress = GEM_SLOTS_START + (nextSlotIndex * BYTES_PER_SLOT);

  EEPROM.put(writeAddress, countToSave);
  EEPROM.put(writeAddress + BYTES_PER_SLOT - 1, checksum);
  EEPROM.put(SLOT_INDEX_ADDR, nextSlotIndex);
}

void clearGemCountEEPROM() {
  // debug tool function for manually resetting all gem-related EEPROM data
  for (uint16_t i = GEM_SLOTS_START; i < EEPROM.length(); ++i) {
    EEPROM.update(i, 0xFF);
  }
  EEPROM.put(SLOT_INDEX_ADDR, (uint16_t)0);
}