#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include "config.h"
#include "clock.h"
#include "tapper.h"
#include "gem_store.h"
#include "encoder.h"
#include "button.h"
#include "menu.h"
#include "display.h"
#include "settings_store.h"

// ===========================================================================
// Application state
// ===========================================================================

// Tap settings — initialized from EEPROM in setup(), then kept in sync.
uint16_t tapDuration = 160;
uint16_t tapDuty     = 160;

// Active mode parameters (switches between kModeActual and kModeTest)
ModeParams activeMode = kModeActual;

// Sleep schedule — defaults in SleepSchedule struct, overwritten by
// settings_load() if valid EEPROM data exists.
SleepSchedule sched;

// Device flags
bool deviceEnabled   = true;
bool testModeEnabled = false;
bool overrideClock   = false;

// Tap scheduling
unsigned long nextTapTime = 0;

// ===========================================================================
// Helpers
// ===========================================================================

// Build a Settings snapshot from current runtime state for EEPROM saves.
static Settings currentSettings() {
    Settings s;
    s.sched       = sched;
    s.tapDuration = tapDuration;
    s.tapDuty     = tapDuty;
    return s;
}

// ===========================================================================
// Forward declarations
// ===========================================================================

void scheduleNextTap();
static void handleMenuAction(const MenuAction& act);
static MenuView buildMenuView(uint32_t msLeft);

// ===========================================================================
// setup()
// ===========================================================================

void setup() {
    Serial.begin(9600);
    randomSeed(analogRead(0));
    delay(1000);

    clock_begin();

    // Load persisted settings before initializing hardware that uses them.
    Settings s;
    settings_load(s);
    sched       = s.sched;
    tapDuration = s.tapDuration;
    tapDuty     = s.tapDuty;

    tapper_setDuty(tapDuty);
    tapper_begin(AD_GEMS_MOSFET_GATE_PIN, FLOAT_GEMS_MOSFET_GATE_PIN);

    encoder_begin(ENCODER_CLK, ENCODER_DT, ENCODER_SW);
    button_begin(RESET_BUTTON_PIN);
    menu_begin();
    display_begin();

    gem_store_begin();

    Serial.print(F("Lifetime gems: ")); Serial.println(gem_store_read_lifetime());
    Serial.print(F("Sleep:         ")); Serial.print(sched.sleepHour); Serial.print(':'); Serial.println(sched.sleepMinute);
    Serial.print(F("Wake:          ")); Serial.print(sched.wakeHour);  Serial.print(':'); Serial.println(sched.wakeMinute);
    Serial.print(F("Tap duration:  ")); Serial.print(tapDuration); Serial.println(F(" ms"));
    Serial.print(F("Tap duty:      ")); Serial.println(tapDuty);

    scheduleNextTap();
}

// ===========================================================================
// loop()
// ===========================================================================

void loop() {
    uint32_t now = millis();

    // --- Input: encoder ---
    // Processed before rendering so state changes appear in the same frame.
    EncoderEvents ev = encoder_poll();
    bool hadInput = (ev.delta != 0 || ev.pressed);

    MenuAction act;
    bool actionFired = menu_update(ev.delta, ev.pressed, act);
    if (actionFired) {
        handleMenuAction(act);
    }

    // --- Input: reset button ---
    // Reschedules the next tap immediately, regardless of device or sleep state.
    if (button_poll()) {
        scheduleNextTap();
        hadInput = true;
        display_markDirty();
        Serial.println(F("Reset button: next tap rescheduled"));
    }

    // --- Sleep/wake transition ---
    static bool wasAwake = false;
    bool awake = overrideClock || clock_isAwake(sched);
    if (awake && !wasAwake) scheduleNextTap();
    wasAwake = awake;

    // --- Advance tapper state machine ---
    tapper_update(now);

    // --- Fire a tap cycle when it's time ---
    if (!tapper_isActive() && deviceEnabled && awake && (int32_t)(now - nextTapTime) >= 0) {
        tapper_startCycle(
            activeMode.adGemTaps,
            activeMode.floatGemTaps,
            tapDuration,
            activeMode.pauseBetweenTapsMs,
            now
        );

        // Gem accounting: 5 gems per activation, +2 bonus every 6th activation.
        // TODO: refine once earning rates are fully characterized.
        static uint8_t activationCount = 0;
        uint32_t earned = 5;
        activationCount++;
        if (activationCount >= 6) {
            earned += 2;
            activationCount = 0;
        }
        gem_store_add_session(earned);
        scheduleNextTap();
    }

    // --- Compute msLeft ---
    // When the device is off, force to zero so the display shows 00:00.
    uint32_t msLeft = 0;
    if (deviceEnabled) {
        int32_t dt = (int32_t)(nextTapTime - now);
        if (dt > 0) msLeft = (uint32_t)dt;
    }

    // --- Render display ---
    MenuView v = buildMenuView(msLeft);

    if (hadInput || actionFired) {
        display_markDirty();
        display_renderNow(v);
    } else {
        static uint32_t lastSecondsLeft = UINT32_MAX;
        static uint32_t lastGems        = UINT32_MAX;
        uint32_t secondsLeft = msLeft / 1000;
        if (secondsLeft != lastSecondsLeft || v.lifetimeGems != lastGems) {
            display_markDirty();
            lastSecondsLeft = secondsLeft;
            lastGems        = v.lifetimeGems;
        }
        display_render(v);
    }
}

// ===========================================================================
// Tap scheduling
// ===========================================================================

void scheduleNextTap() {
    long jitter = random(-(long)activeMode.jitterRangeMs, (long)activeMode.jitterRangeMs);

    bool addBreak = !testModeEnabled && (random(12) == 0);
    unsigned long base = millis() + activeMode.baseIntervalMs;
    if (addBreak) {
        base += (unsigned long)random(3UL * 60 * 1000, 8UL * 60 * 1000);
    }

    long adjusted = (long)base + jitter;
    if (adjusted < 0) adjusted = 0;
    nextTapTime = (unsigned long)adjusted;
}

// ===========================================================================
// Menu action handler
// ===========================================================================

static void handleMenuAction(const MenuAction& act) {
    switch (act.type) {

        case MenuActionType::GoHome:
            menu_reset();
            display_markDirty();
            break;

        case MenuActionType::ToggleDeviceEnabled:
            deviceEnabled = !deviceEnabled;
            if (!deviceEnabled) {
                tapper_stop();
            } else {
                scheduleNextTap();
            }
            menu_reset();
            display_markDirty();
            break;

        case MenuActionType::ResetNextTap:
            scheduleNextTap();
            menu_reset();
            display_markDirty();
            break;

        case MenuActionType::SetTapDuration:
            if (!act.committed) {
                menu_openTapDurationEditor(tapDuration);
            } else {
                tapDuration = act.u16a;
                settings_save(currentSettings());
                menu_reset();
            }
            display_markDirty();
            break;

        case MenuActionType::SetTapDuty:
            if (!act.committed) {
                menu_openTapDutyEditor(tapDuty);
            } else {
                tapDuty = act.u16a;
                tapper_setDuty(tapDuty);
                settings_save(currentSettings());
                menu_reset();
            }
            display_markDirty();
            break;

        case MenuActionType::EnterSleepTimeEditor:
            menu_openSleepTimeEditor(sched.sleepHour, sched.sleepMinute);
            display_markDirty();
            break;

        case MenuActionType::EnterWakeTimeEditor:
            menu_openWakeTimeEditor(sched.wakeHour, sched.wakeMinute);
            display_markDirty();
            break;

        case MenuActionType::SetSleepTime:
            if (act.committed) {
                sched.sleepHour   = (uint8_t)act.u16a;
                sched.sleepMinute = (uint8_t)act.u16b;
                settings_save(currentSettings());
                menu_reset();
                display_markDirty();
            }
            break;

        case MenuActionType::SetWakeTime:
            if (act.committed) {
                sched.wakeHour   = (uint8_t)act.u16a;
                sched.wakeMinute = (uint8_t)act.u16b;
                settings_save(currentSettings());
                menu_reset();
                display_markDirty();
            }
            break;

        case MenuActionType::SetGemCount:
            if (!act.committed) {
                menu_openGemCountEditor(gem_store_total());
            } else {
                gem_store_write_lifetime(act.u32);
                menu_reset();
            }
            display_markDirty();
            break;

        case MenuActionType::ToggleTestMode:
            testModeEnabled = !testModeEnabled;
            activeMode      = testModeEnabled ? kModeTest : kModeActual;
            scheduleNextTap();
            menu_reset();
            display_markDirty();
            break;

        case MenuActionType::ToggleOverrideSleep:
            overrideClock = !overrideClock;
            menu_reset();
            display_markDirty();
            break;

        default:
            break;
    }
}

// ===========================================================================
// View construction
// ===========================================================================

static MenuView buildMenuView(uint32_t msLeft) {
    MenuHomeData hd;
    hd.lifetimeGems = gem_store_total();
    hd.msLeft       = msLeft;
    menu_setHomeData(hd);

    MenuView v;
    menu_getView(v);

    v.lifetimeGems    = gem_store_total();
    v.msLeft          = msLeft;
    v.sleepHour       = sched.sleepHour;
    v.sleepMinute     = sched.sleepMinute;
    v.wakeHour        = sched.wakeHour;
    v.wakeMinute      = sched.wakeMinute;
    v.deviceEnabled   = deviceEnabled;
    v.overrideClock   = overrideClock;
    v.tapDuration     = tapDuration;
    v.testModeEnabled = testModeEnabled;

    return v;
}
