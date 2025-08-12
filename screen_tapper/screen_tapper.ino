#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include "ui.h"
#include "time_settings.h"
#include "config_pins.h"
#include "mode_params.h"
#include "input.h"
#include "clock.h"
#include "tapper.h"
#include "gem_store.h"

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
unsigned long baseInterval = baseIntervalActual;
unsigned long jitterRange = jitterRangeActual;
unsigned long pauseBetweenTaps = pauseBetweenTapsActual;
unsigned long adGemTaps = adGemTapsActual;
unsigned long floatGemTaps = floatGemTapsActual;

// ==== Override Clock Message ====
bool showOverrideClockMessage = false;
unsigned long overrideClockMessageStart = 0;

// ==== Gem Tracking (EEPROM) ====
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

  gem_store_begin();
  lifetimeGemCount = gem_store_read_lifetime();
  
  delay(1000);

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
    // saveGemCount(lifetimeGemCount);
    gem_store_write_lifetime(lifetimeGemCount);
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