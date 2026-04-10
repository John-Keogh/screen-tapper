#pragma once
#include <stdint.h>

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------
enum class MenuScreen {
    Home,
    Settings,
    EditTapDuration,
    EditTapDuty,
    EditSleepTime,
    EditWakeTime,
    EditGemCount,
};

// ---------------------------------------------------------------------------
// Actions emitted by the menu to the application
// ---------------------------------------------------------------------------
enum class MenuActionType {
    None,
    GoHome,
    ToggleDeviceEnabled,
    ResetNextTap,
    SetTapDuration,        // u16a = new duration (ms); 0 means "open the editor"
    SetTapDuty,            // u16a = new duty (0–255);  0 means "open the editor"
    EnterSleepTimeEditor,
    SetSleepTime,          // u16a = hour, u16b = minute
    EnterWakeTimeEditor,
    SetWakeTime,           // u16a = hour, u16b = minute
    SetGemCount,           // u32  = new lifetime count
    ToggleTestMode,
    ToggleOverrideSleep,
};

struct MenuAction {
    MenuActionType type      = MenuActionType::None;
    bool           committed = false; // true = value was saved from editor; false = open the editor
    uint16_t u16a = 0;
    uint16_t u16b = 0;
    uint32_t u32  = 0;
};

// ---------------------------------------------------------------------------
// View model (what the renderer reads each frame)
// ---------------------------------------------------------------------------
enum class ViewKind { Home, List, EditNumber, EditTime };

struct MenuView {
    ViewKind    kind  = ViewKind::Home;
    const char* title = "";

    // Home screen data
    uint32_t lifetimeGems = 0;
    uint32_t msLeft       = 0;

    // Status flags (injected by the app each frame)
    bool     deviceEnabled   = true;
    bool     overrideClock   = false;
    bool     testModeEnabled = false;
    uint16_t tapDuration     = 0;

    // Sleep/wake times (injected by the app each frame)
    uint8_t sleepHour   = 0;
    uint8_t sleepMinute = 0;
    uint8_t wakeHour    = 0;
    uint8_t wakeMinute  = 0;

    // List view
    const char* const* items     = nullptr;
    uint8_t            itemCount = 0;
    uint8_t            selected  = 0;

    // Number editor
    uint32_t    value   = 0;
    uint32_t    minVal  = 0;
    uint32_t    maxVal  = 0;
    const char* unit    = nullptr;
    bool        editing = false;

    // Time editor
    uint8_t hh          = 0;
    uint8_t mm          = 0;
    bool    editingHour = true;
    bool    editingTime = false;
};

// Live data pushed to the home screen each frame
struct MenuHomeData {
    uint32_t lifetimeGems = 0;
    uint32_t msLeft       = 0;
};

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

void menu_begin();
void menu_reset();

// Push live home-screen data before calling menu_update().
void menu_setHomeData(const MenuHomeData& d);

// Feed encoder input; returns true and populates outAction when an action fires.
bool menu_update(int encDelta, bool pressed, MenuAction& outAction);

// Build the view model for this frame (call after menu_update()).
void menu_getView(MenuView& outView);

// Open specific editors directly (called by the app in response to actions).
void menu_openTapDurationEditor(uint32_t initial);
void menu_openTapDutyEditor(uint32_t initial);
void menu_openSleepTimeEditor(uint8_t hh, uint8_t mm);
void menu_openWakeTimeEditor(uint8_t hh, uint8_t mm);
void menu_openGemCountEditor(uint32_t initial);
