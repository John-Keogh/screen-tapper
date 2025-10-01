// Purpose: consume encoder events, maintain menu state, emit actions for the app, and provide a view model for drawing.

#include "menu.h"
#include <Arduino.h>

// settings
static const char* const kSettingsItems[] = {
  "Return to Home",
  "Toggle On/Off",
  "Reset Next Tap",
  "Set Tap Duration",
  "Set Sleep Time",
  "Set Wake Time",
  "Set Gem Count",
  "Toggle Test Mode",
  "Override Sleep",
};
static constexpr uint8_t kSettingsCount = sizeof(kSettingsItems) / sizeof(kSettingsItems[0]);

// editor configs
static constexpr uint32_t TAP_MIN_MS    = 1;
static constexpr uint32_t TAP_MAX_MS    = 1000;
static constexpr uint32_t GEMS_MIN      = 0;
static constexpr uint32_t GEMS_MAX      = 999999999;
static constexpr const char* UNIT_MS    = "ms";
static constexpr const char* UNIT_GEMS  = "gems";

// state
static MenuScreen s_screen = MenuScreen::Home;
static MenuHomeData s_home;

static uint8_t s_sel = 0; // selection index for list screens
static bool s_editing = false;  // numeric editor editing mode
static uint32_t s_numVal = 0;   // value for number editor
static uint32_t s_numMin = 0, s_numMax = 0;
static const char* s_numUnit = nullptr;

static bool s_timeEditing = false;  // time editing mode
static bool s_timeOnHour = true;    // which field active
static uint8_t s_hh = 0, s_mm = 0;  // working time

// helper: wrap selection [0, count-1]
static uint8_t wrap(int v, uint8_t count) {
  if (count == 0) return 0;
  while (v < 0) return 0;
  while (v >= count) v -= count;
  return (uint8_t)v;
}

void menu_begin() {
  s_screen = MenuScreen::Home;
  s_sel = 0;
  s_editing = false;
  s_timeEditing = false;
}

// included for clarity of intent when called
void menu_reset() {
  menu_begin();
}

void menu_setHomeData(const MenuHomeData& d) {
  s_home = d;
}

static void enter_settings() {
  s_screen = MenuScreen::Settings;
  s_sel = 0;
}

static void enter_num_editor(uint32_t initial, uint32_t minV, uint32_t maxV, const char* unit) {
  s_screen  = MenuScreen::EditTapDuration; // default
  s_numVal  = initial;
  s_numMin  = minV;
  s_numMax  = maxV;
  s_numUnit = unit;
  s_editing = false;
  s_sel     = 0;  // 0: value, 1: save, 2: return to settings, 3: return to home
}

static void enter_time_editor(uint8_t hh, uint8_t mm) {
  s_timeEditing = false;
  s_timeOnHour = true;
  s_hh = hh;
  s_mm = mm;
  s_sel = 0;  // 0: time, 1: save, 2: return to settings, 3: return to home
}

// update logic per screen
static bool update_home(int d, bool pressed, MenuAction& act) {
  // rotate does nothing
  if (pressed) {
    enter_settings();
  }
  (void)d;
  return false;
}

static bool update_settings(int d, bool pressed, MenuAction& act) {
  // move selection with rotation
  if (d != 0) s_sel = wrap((int)s_sel + (d > 0 ? +1 : -1), kSettingsCount);

  if (!pressed) return false;

  // handle selection
  switch (s_sel) {
    case 0: // return to home
      s_screen = MenuScreen::Home;
      act.type = MenuActionType::GoHome;
      return true;
    
    case 1: // toggle on/off
      act.type = MenuActionType::ToggleDeviceEnabled;
      return true;

    case 2: // reset next tap
      act.type = MenuActionType::ResetNextTap;
      return true;

    case 3: // adjust tap duration
      act.type = MenuActionType::SetTapDuration;
      return true;
    
    case 4: // set sleep time
      act.type = MenuActionType::EnterSleepTimeEditor;
      return true;
    
    case 5: // set wake time
      act.type = MenuActionType::EnterWakeTimeEditor;
      return true;

    case 6: // set gem count
      act.type = MenuActionType::SetGemCount;
      return true;

    case 7: // toggle test mode
      act.type = MenuActionType::ToggleTestMode;
      return true;

    case 8: // toggle override sleep
      act.type = MenuActionType::ToggleOverrideSleep;
      return true;
  }
  return false;
}

static bool update_num_editor(int d, bool pressed, MenuAction& act, MenuActionType commitType) {
  // items:
  // 0: value
  // 1: save
  // 2: return to settings
  // 3: return to home
  if (!s_editing) {
    if (d != 0) s_sel = wrap((int)s_sel + (d > 0 ? +1 : -1), 4);

    if (pressed) {
      switch (s_sel) {
        case 0: // value -> enter editing mode
          s_editing = true;
          break;
        
        case 1: // save -> emit action and go home
          act.type = commitType;
          if (commitType == MenuActionType::SetTapDuration) {
            act.u16a = (uint16_t)s_numVal;
          } else if (commitType == MenuActionType::SetGemCount) {
            act.u32 = s_numVal;
          }
          s_screen = MenuScreen::Home;
          return true;
        
        case 2: // return to settings
          enter_settings();
          break;
        
        case 3: // return to home
          s_screen = MenuScreen::Home;
          act.type = MenuActionType::GoHome;
          return true;
      }
    }
  } else {
    // editing = true: rotate changes value, press exits editing
    if (d != 0) {
      int32_t v = (int32_t)s_numVal + (d > 0 ? +1 : -1);
      if (v < (int32_t)s_numMin) v = (int32_t)s_numMin;
      if (v > (int32_t)s_numMax) v = (int32_t)s_numMax;
      s_numVal = (uint32_t)v;
    }
    if (pressed) {
      s_editing = false; // stop editing; stay on editor screen
    }
  }
  return false;
}

static bool update_time_editor(int d, bool pressed, MenuAction& act, MenuActionType commitType) {
  // items:
  // 0: time (HH:MM)
  // 1: save
  // 2: return to settings
  // 3: return to home
  if (!s_timeEditing) {
    if (d != 0) s_sel = wrap((int)s_sel + (d > 0 ? +1 : -1), 4);

    if (pressed) {
      switch (s_sel) {
        case 0: // enter editing time, starting on hour
          s_timeEditing = true;
          s_timeOnHour = true;
          break;

        case 1: // save
          act.type = commitType;
          act.u16a = s_hh;
          act.u16b = s_mm;
          s_screen = MenuScreen::Home;
          return true;
        
        case 2: // return to settings
          enter_settings();
          break;
        
        case 3: // return to home
          s_screen = MenuScreen::Home;
          act.type = MenuActionType::GoHome;
          return true;
      }
    }
  } else {
    // editing time: rotate changes HH or MM; press switches field (HH<->MM)
    if (d != 0) {
      if (s_timeOnHour) {
        int hh = (int)s_hh + (d > 0 ? +1 : -1);
        if (hh < 0) hh = 23;
        if (hh > 23) hh = 0;
        s_hh = (uint8_t)hh;
      } else {
        int mm = (int)s_mm + (d > 0 ? +1 : -1);
        if (mm < 0) mm = 59;
        if (mm > 59) mm = 0;
        s_mm = (uint8_t)mm;
      }
    }
    if (pressed) {
      if (s_timeOnHour) {
        s_timeOnHour = false; // move to minutes
      } else {
        s_timeEditing = false; // finish editing; remain on editor screen
      }
    }
  }
  return false;
}

// public update
bool menu_update(int encDelta, bool pressed, MenuAction& outAction) {
  outAction = {};
  switch (s_screen) {
    case MenuScreen::Home:
      return update_home(encDelta, pressed, outAction);
    
    case MenuScreen::Settings:
      return update_settings(encDelta, pressed, outAction);
    
    case MenuScreen::EditTapDuration:
      return update_num_editor(encDelta, pressed, outAction, MenuActionType::SetTapDuration);

    case MenuScreen::EditGemCount:
      return update_num_editor(encDelta, pressed, outAction, MenuActionType::SetGemCount);

    case MenuScreen::EditSleepTime:
      return update_time_editor(encDelta, pressed, outAction, MenuActionType::SetSleepTime);
    
    case MenuScreen::EditWakeTime:
      return update_time_editor(encDelta, pressed, outAction, MenuActionType::SetWakeTime);
  }
  return false;
}

// build the view model
void menu_getView(MenuView& v) {
  v = {};
  switch (s_screen) {
    case MenuScreen::Home:
      v.kind          = ViewKind::Home;
      v.title         = "Home";
      v.lifetimeGems  = s_home.lifetimeGems;
      v.msLeft        = s_home.msLeft;
      break;

    case MenuScreen::Settings:
      v.kind          = ViewKind::List;
      v.title         = "Settings";
      v.items         = kSettingsItems;
      v.itemCount     = kSettingsCount;
      v.selected      = s_sel;
      break;
    
    case MenuScreen::EditTapDuration:
      v.kind          = ViewKind::EditNumber;
      v.title         = "Tap Duration";
      v.value         = s_numVal;
      v.minVal        = TAP_MIN_MS;
      v.maxVal        = TAP_MAX_MS;
      v.unit          = UNIT_MS;
      v.editing       = s_editing;
      v.items         = nullptr;
      v.selected      = s_sel;
      break;
    
    case MenuScreen::EditGemCount:
      v.kind          = ViewKind::EditNumber;
      v.title         = "Gem Count";
      v.value         = s_numVal;
      v.minVal        = GEMS_MIN;
      v.maxVal        = GEMS_MAX;
      v.unit          = UNIT_GEMS;
      v.editing       = s_editing;
      v.selected      = s_sel;
      break;

    case MenuScreen::EditSleepTime:
      v.kind          = ViewKind::EditTime;
      v.title         = "Sleep Time";
      v.hh            = s_hh;
      v.mm            = s_mm;
      v.editingTime   = s_timeEditing;
      v.editingHour   = s_timeOnHour;
      v.selected      = s_sel;
      break;
    
    case MenuScreen::EditWakeTime:
      v.kind          = ViewKind::EditTime;
      v.title         = "Wake Time";
      v.hh            = s_hh;
      v.mm            = s_mm;
      v.editingTime   = s_timeEditing;
      v.editingHour   = s_timeOnHour;
      v.selected      = s_sel;
      break;
  }
}

void menu_openTapDurationEditor(uint32_t initial) {
  enter_num_editor(initial, TAP_MIN_MS, TAP_MAX_MS, UNIT_MS);
  s_screen = MenuScreen::EditTapDuration;
}

void menu_openSleepTimeEditor(uint8_t hh, uint8_t mm) {
  enter_time_editor(hh, mm);
  s_screen = MenuScreen::EditSleepTime;
}

void menu_openWakeTimeEditor(uint8_t hh, uint8_t mm) {
  enter_time_editor(hh, mm);
  s_screen = MenuScreen::EditWakeTime;
}