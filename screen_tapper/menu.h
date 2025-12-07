#pragma once
#include <stdint.h>

// top-level screens
enum class MenuScreen {
  Home,
  Settings,
  EditTapDuration,
  EditTapDuty,
  EditSleepTime,
  EditWakeTime,
  EditGemCount,
};

// actions
enum class MenuActionType {
  None,
  GoHome,
  ToggleDeviceEnabled,
  ResetNextTap,
  SetTapDuration,
  SetTapDuty,
  EnterSleepTimeEditor,
  SetSleepTime,
  EnterWakeTimeEditor,
  SetWakeTime,
  SetGemCount,
  ToggleTestMode,
  ToggleOverrideSleep,
};

struct MenuAction {
  MenuActionType type = MenuActionType::None;
  uint16_t u16a = 0;
  uint16_t u16b = 0;
  uint32_t u32 =  0;
};

// view model
enum class ViewKind { Home, List, EditNumber, EditTime };

struct MenuView {
  ViewKind kind = ViewKind::Home;
  const char* title = "";

  // home
  uint32_t lifetimeGems = 0;
  uint32_t msLeft = 0;

  // sleep/wake times initialization (set in time_settings.h)
  uint8_t wakeHour    = 0;
  uint8_t wakeMinute  = 0;
  uint8_t sleepHour   = 0;
  uint8_t sleepMinute = 0;

  // statuses
  bool deviceEnabled = true;
  bool overrideClock = false;
  uint16_t tapDuration = 0;
  bool testModeEnabled = false;

  // list view
  const char* const* items = nullptr;
  uint8_t itemCount = 0;
  uint8_t selected = 0;

  // number editor
  uint32_t value =  0;
  uint32_t minVal = 0;
  uint32_t maxVal = 0;
  const char* unit = nullptr;
  bool editing = false; // true when knob adjusts value

  // time editor
  uint8_t hh = 0;
  uint8_t mm = 0;
  bool editingHour = true;
  bool editingTime = false;
};

// live data
struct MenuHomeData {
  uint32_t lifetimeGems = 0;
  uint32_t msLeft = 0;
};

// API
void menu_begin();
void menu_reset();

// supply home data each frame
void menu_setHomeData(const MenuHomeData& d);

// advance menu with encoder input
bool menu_update(int encDelta, bool pressed, MenuAction& outAction);

// call after menu_update() to ask what to draw
void menu_getView(MenuView& outView);

// open the tap duration number editor
void menu_openTapDurationEditor(uint32_t initial);
void menu_openTapDutyEditor(uint32_t currentDuty);

// open time editor
void menu_openSleepTimeEditor(uint8_t hh, uint8_t mm);
void menu_openWakeTimeEditor(uint8_t hh, uint8_t mm);