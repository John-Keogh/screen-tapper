// Purpose: consume encoder events, maintain menu state, emit actions for the
// application, and provide a view model for the display renderer.

#include "menu.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Settings list
// ---------------------------------------------------------------------------
static const char* const kSettingsItems[] = {
    "Return to Home",
    "Toggle On/Off",
    "Reset Next Tap",
    "Set Tap Duration",
    "Set Tap Duty",
    "Set Sleep Time",
    "Set Wake Time",
    "Set Gem Count",
    "Toggle Test Mode",
    "Override Sleep",
};
static constexpr uint8_t kSettingsCount =
    sizeof(kSettingsItems) / sizeof(kSettingsItems[0]);

// ---------------------------------------------------------------------------
// Editor limits
// ---------------------------------------------------------------------------
static constexpr uint32_t TAP_MIN_MS   = 1;
static constexpr uint32_t TAP_MAX_MS   = 1000;
static constexpr uint32_t TAP_DUTY_MIN = 0;
static constexpr uint32_t TAP_DUTY_MAX = 255;
static constexpr uint32_t GEMS_MIN     = 0;
static constexpr uint32_t GEMS_MAX     = 999999999;
static constexpr const char* UNIT_MS   = "ms";
static constexpr const char* UNIT_DUTY = "/255";
static constexpr const char* UNIT_GEMS = "gems";

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------
static MenuScreen s_screen  = MenuScreen::Home;
static MenuHomeData s_home;

static uint8_t s_sel     = 0;
static bool    s_editing = false;   // numeric editor: knob adjusts value

static uint32_t    s_numVal  = 0;
static uint32_t    s_numMin  = 0;
static uint32_t    s_numMax  = 0;
static const char* s_numUnit = nullptr;

static bool    s_timeEditing = false;
static bool    s_timeOnHour  = true;
static uint8_t s_hh          = 0;
static uint8_t s_mm          = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Wraps v into [0, count-1] with true modulo arithmetic.
static uint8_t wrap(int v, uint8_t count) {
    if (count == 0) return 0;
    v = v % (int)count;
    if (v < 0) v += (int)count;
    return (uint8_t)v;
}

static void enter_settings() {
    s_screen = MenuScreen::Settings;
    s_sel    = 0;
}

static void enter_num_editor(uint32_t initial, uint32_t minV, uint32_t maxV, const char* unit) {
    s_numVal  = initial;
    s_numMin  = minV;
    s_numMax  = maxV;
    s_numUnit = unit;
    s_editing = false;
    s_sel     = 0;
}

static void enter_time_editor(uint8_t hh, uint8_t mm) {
    s_timeEditing = false;
    s_timeOnHour  = true;
    s_hh = hh;
    s_mm = mm;
    s_sel = 0;
}

// ---------------------------------------------------------------------------
// Per-screen update functions
// ---------------------------------------------------------------------------

static bool update_home(int d, bool pressed, MenuAction& act) {
    if (pressed) enter_settings();
    return false;
}

static bool update_settings(int d, bool pressed, MenuAction& act) {
    if (d != 0) s_sel = wrap((int)s_sel + (d > 0 ? +1 : -1), kSettingsCount);
    if (!pressed) return false;

    switch (s_sel) {
        case 0: s_screen = MenuScreen::Home; act.type = MenuActionType::GoHome;               return true;
        case 1: act.type = MenuActionType::ToggleDeviceEnabled;                               return true;
        case 2: act.type = MenuActionType::ResetNextTap;                                      return true;
        case 3: act.type = MenuActionType::SetTapDuration;                                    return true;
        case 4: act.type = MenuActionType::SetTapDuty;                                        return true;
        case 5: act.type = MenuActionType::EnterSleepTimeEditor;                              return true;
        case 6: act.type = MenuActionType::EnterWakeTimeEditor;                               return true;
        case 7: act.type = MenuActionType::SetGemCount;                                       return true;
        case 8: act.type = MenuActionType::ToggleTestMode;                                    return true;
        case 9: act.type = MenuActionType::ToggleOverrideSleep;                               return true;
    }
    return false;
}

// Shared numeric editor (tap duration, tap duty, gem count).
// items: 0=Value, 1=Save, 2=Back, 3=Home
static bool update_num_editor(int d, bool pressed, MenuAction& act, MenuActionType commitType) {
    if (!s_editing) {
        if (d != 0) s_sel = wrap((int)s_sel + (d > 0 ? +1 : -1), 4);
        if (!pressed) return false;

        switch (s_sel) {
            case 0:  // enter value-editing mode
                s_editing = true;
                break;
            case 1:  // save and return home
                act.type      = commitType;
                act.committed = true;
                if (commitType == MenuActionType::SetTapDuration ||
                    commitType == MenuActionType::SetTapDuty) {
                    act.u16a = (uint16_t)s_numVal;
                } else if (commitType == MenuActionType::SetGemCount) {
                    act.u32 = s_numVal;
                }
                s_screen = MenuScreen::Home;
                return true;
            case 2:  // back to settings
                enter_settings();
                break;
            case 3:  // return home
                s_screen = MenuScreen::Home;
                act.type = MenuActionType::GoHome;
                return true;
        }
    } else {
        // Knob adjusts the value; press exits editing mode
        if (d != 0) {
            int32_t v = (int32_t)s_numVal + (d > 0 ? +1 : -1);
            if (v < (int32_t)s_numMin) v = (int32_t)s_numMin;
            if (v > (int32_t)s_numMax) v = (int32_t)s_numMax;
            s_numVal = (uint32_t)v;
        }
        if (pressed) s_editing = false;
    }
    return false;
}

// Shared time editor (sleep time, wake time).
// items: 0=Time, 1=Save, 2=Back, 3=Home
static bool update_time_editor(int d, bool pressed, MenuAction& act, MenuActionType commitType) {
    if (!s_timeEditing) {
        if (d != 0) s_sel = wrap((int)s_sel + (d > 0 ? +1 : -1), 4);
        if (!pressed) return false;

        switch (s_sel) {
            case 0:  // enter time-editing mode
                s_timeEditing = true;
                s_timeOnHour  = true;
                break;
            case 1:  // save
                act.type      = commitType;
                act.committed = true;
                act.u16a = s_hh;
                act.u16b = s_mm;
                s_screen = MenuScreen::Home;
                return true;
            case 2:  // back to settings
                enter_settings();
                break;
            case 3:  // return home
                s_screen = MenuScreen::Home;
                act.type = MenuActionType::GoHome;
                return true;
        }
    } else {
        // Knob adjusts HH or MM; press advances field (HH → MM → done)
        if (d != 0) {
            if (s_timeOnHour) {
                int hh = (int)s_hh + (d > 0 ? +1 : -1);
                if (hh < 0)  hh = 23;
                if (hh > 23) hh = 0;
                s_hh = (uint8_t)hh;
            } else {
                int mm = (int)s_mm + (d > 0 ? +1 : -1);
                if (mm < 0)  mm = 59;
                if (mm > 59) mm = 0;
                s_mm = (uint8_t)mm;
            }
        }
        if (pressed) {
            if (s_timeOnHour) {
                s_timeOnHour = false;  // move to minutes
            } else {
                s_timeEditing = false; // done editing; stay on editor screen
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void menu_begin() {
    s_screen      = MenuScreen::Home;
    s_sel         = 0;
    s_editing     = false;
    s_timeEditing = false;
}

void menu_reset() { menu_begin(); }

void menu_setHomeData(const MenuHomeData& d) { s_home = d; }

bool menu_update(int encDelta, bool pressed, MenuAction& outAction) {
    outAction = {};
    switch (s_screen) {
        case MenuScreen::Home:            return update_home(encDelta, pressed, outAction);
        case MenuScreen::Settings:        return update_settings(encDelta, pressed, outAction);
        case MenuScreen::EditTapDuration: return update_num_editor(encDelta, pressed, outAction, MenuActionType::SetTapDuration);
        case MenuScreen::EditTapDuty:     return update_num_editor(encDelta, pressed, outAction, MenuActionType::SetTapDuty);
        case MenuScreen::EditGemCount:    return update_num_editor(encDelta, pressed, outAction, MenuActionType::SetGemCount);
        case MenuScreen::EditSleepTime:   return update_time_editor(encDelta, pressed, outAction, MenuActionType::SetSleepTime);
        case MenuScreen::EditWakeTime:    return update_time_editor(encDelta, pressed, outAction, MenuActionType::SetWakeTime);
    }
    return false;
}

void menu_getView(MenuView& v) {
    v = {};
    switch (s_screen) {
        case MenuScreen::Home:
            v.kind         = ViewKind::Home;
            v.title        = "Home";
            v.lifetimeGems = s_home.lifetimeGems;
            v.msLeft       = s_home.msLeft;
            break;

        case MenuScreen::Settings:
            v.kind      = ViewKind::List;
            v.title     = "Settings";
            v.items     = kSettingsItems;
            v.itemCount = kSettingsCount;
            v.selected  = s_sel;
            break;

        case MenuScreen::EditTapDuration:
            v.kind    = ViewKind::EditNumber;
            v.title   = "Tap Duration";
            v.value   = s_numVal;
            v.minVal  = TAP_MIN_MS;
            v.maxVal  = TAP_MAX_MS;
            v.unit    = UNIT_MS;
            v.editing = s_editing;
            v.selected = s_sel;
            break;

        case MenuScreen::EditTapDuty:
            v.kind    = ViewKind::EditNumber;
            v.title   = "Tap Duty";
            v.value   = s_numVal;
            v.minVal  = TAP_DUTY_MIN;
            v.maxVal  = TAP_DUTY_MAX;
            v.unit    = UNIT_DUTY;
            v.editing = s_editing;
            v.selected = s_sel;
            break;

        case MenuScreen::EditGemCount:
            v.kind    = ViewKind::EditNumber;
            v.title   = "Gem Count";
            v.value   = s_numVal;
            v.minVal  = GEMS_MIN;
            v.maxVal  = GEMS_MAX;
            v.unit    = UNIT_GEMS;
            v.editing = s_editing;
            v.selected = s_sel;
            break;

        case MenuScreen::EditSleepTime:
            v.kind        = ViewKind::EditTime;
            v.title       = "Sleep Time";
            v.hh          = s_hh;
            v.mm          = s_mm;
            v.editingTime = s_timeEditing;
            v.editingHour = s_timeOnHour;
            v.selected    = s_sel;
            break;

        case MenuScreen::EditWakeTime:
            v.kind        = ViewKind::EditTime;
            v.title       = "Wake Time";
            v.hh          = s_hh;
            v.mm          = s_mm;
            v.editingTime = s_timeEditing;
            v.editingHour = s_timeOnHour;
            v.selected    = s_sel;
            break;
    }
}

void menu_openTapDurationEditor(uint32_t initial) {
    s_screen = MenuScreen::EditTapDuration;
    enter_num_editor(initial, TAP_MIN_MS, TAP_MAX_MS, UNIT_MS);
}

void menu_openTapDutyEditor(uint32_t initial) {
    s_screen = MenuScreen::EditTapDuty;
    enter_num_editor(initial, TAP_DUTY_MIN, TAP_DUTY_MAX, UNIT_DUTY);
}

void menu_openSleepTimeEditor(uint8_t hh, uint8_t mm) {
    s_screen = MenuScreen::EditSleepTime;
    enter_time_editor(hh, mm);
}

void menu_openWakeTimeEditor(uint8_t hh, uint8_t mm) {
    s_screen = MenuScreen::EditWakeTime;
    enter_time_editor(hh, mm);
}

void menu_openGemCountEditor(uint32_t initial) {
    s_screen = MenuScreen::EditGemCount;
    enter_num_editor(initial, GEMS_MIN, GEMS_MAX, UNIT_GEMS);
}
