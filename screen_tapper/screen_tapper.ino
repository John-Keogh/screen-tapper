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
#include "encoder.h"
#include "menu.h"
#include "ui12864_menu.h"

uint16_t tapDuration = 100;

// ==== Clock ====
SleepSchedule sched;

// ==== LCD ====
unsigned long lastDisplayChange = 0;
const unsigned long displayInterval = 2500;
int displayState = 0;
const int numDisplayStates = 2;
uint32_t lastShownSeconds = 0;

// ==== Button Logic ====
bool deviceEnabled = true; // device should be on to begin with (opposite this value for some reason)
bool testModeEnabled = false;
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
uint32_t baseInterval = baseIntervalActual;
uint32_t jitterRange = jitterRangeActual;
uint32_t pauseBetweenTaps = pauseBetweenTapsActual;
uint8_t adGemTaps = adGemTapsActual;
uint8_t floatGemTaps = floatGemTapsActual;

// ==== Override Clock Message ====
bool showOverrideClockMessage = false;
unsigned long overrideClockMessageStart = 0;

// ==== Gem Tracking (EEPROM) ====
uint32_t sessionGemCount = 0;
uint32_t displayedGemCount = 0;
int activationCount = 0;
uint32_t lifetimeGemCount = 0; // initialization - will be read from EEPROM later
static uint32_t lastShownGems;

// ==== Menu Helpers ====
static void handleMenuAction(const MenuAction& act);
static void renderMenuFrame(uint32_t msLeft, uint32_t gems, const EncoderEvents& ev);

void setup() {
  delay(1000);

  Serial.begin(9600);
  randomSeed(analogRead(0));

  clock_begin();
  bool rtcAvailable = clock_available(); // this isn't used for anything?

  tapper_begin(AD_GEMS_MOSFET_GATE_PIN, FLOAT_GEMS_MOSFET_GATE_PIN);

  ui_begin();
  ui_setMode(UIMode::ACTIVE_MODE);

  encoder_begin(ENCODER_CLK, ENCODER_DT, ENCODER_SW);
  menu_begin();
  ui12864_menu_begin();
  ui12864_lcd_backlight_begin(LCD12864_LED);

  input_begin();

  gem_store_begin();
  lifetimeGemCount = gem_store_read_lifetime();

  Serial.print("Lifetime Gem Count: ");
  Serial.println(lifetimeGemCount);
  Serial.println("");

  lastShownSeconds = UINT32_MAX;
  lastShownGems = lifetimeGemCount;

  scheduleNextTap();
}

// --------- LOOP FUNCTION ---------
void loop() {
  uint32_t now = millis();

  auto ev = encoder_poll();

  static bool wasAwake = false;
  bool awake = overrideClock || clock_isAwake(sched);

  if (awake && !wasAwake) {
    scheduleNextTap();
  }
  wasAwake = awake;

  tapper_update(now);

  if (!tapper_isActive() && deviceEnabled && awake && (int32_t)(now - nextTapTime) >= 0) {
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

  static bool wasTapping = false;
  bool isTapping = tapper_isActive();

  if (!isTapping && wasTapping) {
    ui_resetTappingScreen();
  }
  wasTapping = isTapping;

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
    return;
  }

  // Tap duration down
  if (events.tapDownPressed) {
    if (tapDuration > 1) tapDuration--;
    ui_showTapDuration(tapDuration);
    return;
  }

  // Override clock
  if (events.overridePressed) {
    overrideClock = !overrideClock;
    ui_showOverride(overrideClock);
    if (overrideClock) ui_setBacklight(255);
    return;
  }

  if (!deviceEnabled) {
    tapper_stop();
    ui_setMode(UIMode::OFF_MODE);
    ui_showOff();

    uint32_t gemsNow = lifetimeGemCount + sessionGemCount;
    renderMenuFrame(0, gemsNow, ev);
    return;
  }

  if (!overrideClock && !awake) {
    ui_setMode(UIMode::SLEEP_MODE);
    ui_setBacklight(0);
    ui_showSleep(sched.wakeHour, sched.wakeMinute);
    
    uint32_t gemsNow = lifetimeGemCount + sessionGemCount;
    renderMenuFrame(0, gemsNow, ev); // show 00:00 countdown as placeholder
    return;
  }

  ui_setMode(UIMode::ACTIVE_MODE);
  ui_setBacklight(255);

  if (tapper_isActive()) {
    ui_showTapping();

    uint32_t gemsNow = lifetimeGemCount + sessionGemCount;
    renderMenuFrame(0, gemsNow, ev);
    return;
  }

  ui_tick();
  if (ui_overlayActive()) return;

  // Rotating “idle” screens (countdown / lifetime gems)
  if (now - lastDisplayChange >= displayInterval) {
    displayState = (displayState + 1) % numDisplayStates;
    lastDisplayChange = now;
  }

  uint32_t timeLeft = 0;
  int32_t dt = (int32_t)(nextTapTime - now);
  if (dt > 0) timeLeft = (uint32_t) dt;
  if (displayState == 0) {
    ui_showNextTapCountdown(timeLeft);
  } else {
    displayedGemCount = lifetimeGemCount + sessionGemCount;
    ui_showLifetimeGems(displayedGemCount);
  }

  if (sessionGemCount >= GEMS_PER_SAVE) {
    lifetimeGemCount += sessionGemCount;
    gem_store_write_lifetime(lifetimeGemCount);
    sessionGemCount = 0;
  }

  renderMenuFrame(timeLeft, displayedGemCount, ev);
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

static void handleMenuAction(const MenuAction& act) {
  switch (act.type) {
    case MenuActionType::GoHome: {
      menu_reset();
      ui12864_markDirty();
      break;
    }

    case MenuActionType::ToggleDeviceEnabled: {
      deviceEnabled = !deviceEnabled;

      if (!deviceEnabled) {
        // Turning OFF: stop hardware, show OFF mode
        tapper_stop();
        ui_setMode(UIMode::OFF_MODE);
      } else {
        // Turning ON: ACTIVE + seed timing
        ui_setMode(UIMode::ACTIVE_MODE);
        scheduleNextTap();
      }

      // Reset menu to Home and force a redraw
      menu_reset();
      ui12864_markDirty();
      break;
    }

    case MenuActionType::ResetNextTap: {
      scheduleNextTap();
      menu_reset();
      ui12864_markDirty();
      break;
    }

    case MenuActionType::SetTapDuration: {
      if (act.u16a == 0) {
        menu_openTapDurationEditor(tapDuration);
        ui12864_markDirty();
      } else {
        tapDuration = act.u16a;
        ui_showTapDuration(tapDuration);
        menu_reset();
        ui12864_markDirty();
      }
      break;
    }
    
    case MenuActionType::EnterSleepTimeEditor: {
      menu_openSleepTimeEditor(sched.sleepHour, sched.sleepMinute);
      ui12864_markDirty();
      break;
    }

    case MenuActionType::EnterWakeTimeEditor: {
      menu_openWakeTimeEditor(sched.wakeHour, sched.wakeMinute);
      ui12864_markDirty();
      break;
    }

    case MenuActionType::SetSleepTime: {
      sched.sleepHour = (uint8_t)act.u16a;
      sched.sleepMinute = (uint8_t)act.u16b;
      menu_reset();
      ui12864_markDirty();
      break;
    }

    case MenuActionType::SetWakeTime: {
      sched.wakeHour = (uint8_t)act.u16a;
      sched.wakeMinute = (uint8_t)act.u16b;
      menu_reset();
      ui12864_markDirty();
      break;
    }

    case MenuActionType::ToggleTestMode: {
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

      scheduleNextTap();
      menu_reset();
      ui12864_markDirty();
      break;
    }

    case MenuActionType::ToggleOverrideSleep: {
      overrideClock = !overrideClock;
      menu_reset();
      ui12864_markDirty();
      break;
    }

    // Wire the rest later (SetTapDuration, SetSleepTime, etc.)
    default:
      break;
  }
}

static void renderMenuFrame(uint32_t msLeft, uint32_t gems, const EncoderEvents& ev) {
  // live data for home view
  MenuHomeData hd;
  hd.lifetimeGems = gems;
  hd.msLeft = msLeft;
  menu_setHomeData(hd);

  MenuAction act;
  if (menu_update(ev.delta, ev.pressed, act)) {
    handleMenuAction(act);
  }

  uint32_t secondsLeft = msLeft / 1000;
  if (secondsLeft != lastShownSeconds || gems != lastShownGems || ev.delta != 0 || ev.pressed) {
    lastShownSeconds = secondsLeft;
    lastShownGems    = gems;
    ui12864_markDirty();
  }

  // ---- Build the final view snapshot for this frame ----
  MenuView v;
  menu_getView(v);              // base view from the menu state machine
  v.lifetimeGems = gems;        // ensure these are current for this frame
  v.msLeft       = msLeft;

  // inject sleep/wake from sched
  v.sleepHour   = sched.sleepHour;
  v.sleepMinute = sched.sleepMinute;
  v.wakeHour    = sched.wakeHour;
  v.wakeMinute  = sched.wakeMinute;

  // inject statuses
  v.deviceEnabled = deviceEnabled;
  v.overrideClock = overrideClock;
  v.tapDuration   = tapDuration;
  v.testModeEnabled = testModeEnabled;

  // Draw current view (frame-limited inside ui12864_menu_render)
  ui12864_menu_render(v);
}