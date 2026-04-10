#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include "config.h"
#include "clock.h"
#include "tapper.h"
#include "gem_store.h"
#include "encoder.h"
#include "menu.h"
#include "display.h"

// ===========================================================================
// Application state
// ===========================================================================

// Tap settings (adjustable at runtime via menu)
uint16_t tapDuration = 160;
uint16_t tapDuty     = 160;

// Active mode parameters (switches between kModeActual and kModeTest)
ModeParams activeMode = kModeActual;

// Sleep schedule (adjustable at runtime via menu)
SleepSchedule sched;

// Device flags
bool deviceEnabled   = true;
bool testModeEnabled = false;
bool overrideClock   = false;

// Tap scheduling
unsigned long nextTapTime = 0;

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

    tapper_setDuty(tapDuty);
    tapper_begin(AD_GEMS_MOSFET_GATE_PIN, FLOAT_GEMS_MOSFET_GATE_PIN);

    encoder_begin(ENCODER_CLK, ENCODER_DT, ENCODER_SW);
    menu_begin();
    display_begin();

    gem_store_begin();

    Serial.print(F("Lifetime gems: "));
    Serial.println(gem_store_read_lifetime());

    scheduleNextTap();
}

// ===========================================================================
// loop()
// ===========================================================================

void loop() {
    uint32_t now = millis();

    // --- Input: poll encoder and process menu immediately ---
    // State is committed before rendering so changes appear in the same frame.
    EncoderEvents ev = encoder_poll();
    bool hadInput = (ev.delta != 0 || ev.pressed);

    MenuAction act;
    bool actionFired = menu_update(ev.delta, ev.pressed, act);
    if (actionFired) {
        handleMenuAction(act);
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

    // --- Render display ---
    uint32_t msLeft = 0;
    int32_t  dt     = (int32_t)(nextTapTime - now);
    if (dt > 0) msLeft = (uint32_t)dt;

    MenuView v = buildMenuView(msLeft);

    if (hadInput || actionFired) {
        // Input occurred this frame: render immediately, bypassing the limiter.
        // This is what makes the menu feel instant — the user never waits for
        // the rate-limit window to expire after a click or rotation.
        display_markDirty();
        display_renderNow(v);
    } else {
        // No input: use the rate-limited path. Only redraws when dirty AND
        // the frame period has elapsed, preventing wasteful redraws on frames
        // where only a few milliseconds have passed with nothing changing.

        // Auto-dirty when the countdown ticks or gem count changes
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

    // Occasionally insert a longer "distraction break" (3–8 min) to look more human.
    // Skipped in test mode to keep test cycles fast and predictable.
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
                // "Set Tap Duration" selected from settings — open the editor
                menu_openTapDurationEditor(tapDuration);
            } else {
                // Value saved from the editor
                tapDuration = act.u16a;
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
                menu_reset();
                display_markDirty();
            }
            break;

        case MenuActionType::SetWakeTime:
            if (act.committed) {
                sched.wakeHour   = (uint8_t)act.u16a;
                sched.wakeMinute = (uint8_t)act.u16b;
                menu_reset();
                display_markDirty();
            }
            break;

        case MenuActionType::SetGemCount:
            if (!act.committed) {
                menu_openGemCountEditor(gem_store_total());
            } else {
                // act.u32 holds the new lifetime count — zero is a valid value
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
    // Push live home-screen data to the menu module
    MenuHomeData hd;
    hd.lifetimeGems = gem_store_total();
    hd.msLeft       = msLeft;
    menu_setHomeData(hd);

    // Build the view snapshot from current menu state
    MenuView v;
    menu_getView(v);

    // Inject live application state
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
