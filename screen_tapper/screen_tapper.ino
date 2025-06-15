#include <LiquidCrystal.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>

// ==== RTC ====
RTC_DS1307 rtc;
bool rtcAvailable = false;
bool deviceAwake = true;

// ==== Time Settings ====
const int WAKE_UP_HOUR = 6;
const int WAKE_UP_MINUTE = 0;
const int BED_TIME_HOUR = 23;
const int BED_TIME_MINUTE = 59;
const int WAKE_UP_TIME = WAKE_UP_HOUR * 60 + WAKE_UP_MINUTE;
const int BED_TIME = BED_TIME_HOUR * 60 + BED_TIME_MINUTE;

// ==== Pins ====
const int AD_GEMS_MOSFET_GATE_PIN = 23;
const int FLOAT_GEMS_MOSFET_GATE_PIN = 22;
const int ONOFF_BUTTON_PIN = 37;
const int MODE_TOGGLE_PIN = 39;
const int TAP_DURATION_UP = 41;
const int TAP_DURATION_DOWN = 43;
const int GEM_RESET_BUTTON = 45;
const int LCD_LED = 2;

// ==== LCD ====
LiquidCrystal lcd(29, 28, 31, 30, 33, 32);
enum LcdMode { OFF_MODE, SLEEP_MODE, ACTIVE_MODE };
LcdMode currentLcdMode = OFF_MODE;
LcdMode lastLcdMode = ACTIVE_MODE;  // force update on first run
bool lcdEnabled = true;
int lcdLightLevel = 255;

// ==== Button Logic ====
bool deviceEnabled = false;
bool testModeEnabled = true;  // actual device starts as false

// Shared debounce delay for all buttons
const unsigned long debounceDelay = 50;

// === Button States ===
bool lastOnOffButtonState = LOW;
unsigned long lastOnOffDebounceTime = 0;

bool lastTestModeButtonState = LOW;
unsigned long lastTestModeDebounceTime = 0;

bool lastTapDurationUpButtonState = LOW;
unsigned long lastTapDurationUpDebounceTime = 0;

bool lastTapDurationDownButtonState = LOW;
unsigned long lastTapDurationDownDebounceTime = 0;

bool lastGemResetButtonState = LOW;
unsigned long lastGemResetDebounceTime = 0;

// ==== Timing for Solenoids ====
unsigned long nextTapTime = 0;
unsigned long lastTapTime = 0;

// ==== Mode Parameters ====
const unsigned long baseIntervalActual = 660000;
const unsigned long jitterRangeActual = 12000;
const unsigned long pauseBetweenTapsActual = 1000;
const unsigned long adGemTapsActual = 5;
const unsigned long floatGemTapsActual = 12;

const unsigned long baseIntervalTest = 15000;
const unsigned long jitterRangeTest = 0;
const unsigned long pauseBetweenTapsTest = 1000;
const unsigned long adGemTapsTest = 3;
const unsigned long floatGemTapsTest = 3;

unsigned long baseInterval = baseIntervalActual;
unsigned long jitterRange = jitterRangeActual;
unsigned long pauseBetweenTaps = pauseBetweenTapsActual;
unsigned long adGemTaps = adGemTapsActual;
unsigned long floatGemTaps = floatGemTapsActual;
unsigned long tapDuration = 35;

// ==== LCD Display Timing ====
unsigned long lastDisplayChange = 0;
const unsigned long displayInterval = 3000;
int displayState = 0;
const int numDisplayStates = 2;

// ==== Test Mode Message ====
bool showTestModeMessage = false;
String testModeMessage = "";
unsigned long testModeMessageStart = 0;
const unsigned long testModeMessageDuration = 2000;

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

  lcd.begin(16, 2);
  lcd.print("Initializing...");

  Wire.begin();
  rtcAvailable = rtc.begin();

  if (rtcAvailable) {
      deviceAwake = true;
  }

  pinMode(AD_GEMS_MOSFET_GATE_PIN, OUTPUT);
  pinMode(FLOAT_GEMS_MOSFET_GATE_PIN, OUTPUT);
  pinMode(ONOFF_BUTTON_PIN, INPUT);
  pinMode(MODE_TOGGLE_PIN, INPUT);
  pinMode(TAP_DURATION_UP, INPUT);
  pinMode(TAP_DURATION_DOWN, INPUT);
  pinMode(GEM_RESET_BUTTON, INPUT);
  pinMode(LCD_LED, OUTPUT);
  analogWrite(LCD_LED, 255); // 0 is always off; 255 is always on
  randomSeed(analogRead(0));

  // Initialization for lifetime gem count - only for first time use
  uint16_t storedSlot;
  EEPROM.get(SLOT_INDEX_ADDR, storedSlot);
  if (storedSlot == 0xFFFF) {  // both bytes are 0xFF
    Serial.println("EEPROM uninitialized. Initializing slot index to 0.");
    EEPROM.put(SLOT_INDEX_ADDR, (uint16_t)0);
  } else {
    Serial.println("EEPROM already initialized.");
    // uncomment if manually resetting EEPROM lifetime gem data
    // Serial.println("Resetting EEPROM lifetime gem data.");
    // clearGemCountEEPROM();
    Serial.print("Slot Index Address: ");
    Serial.println(storedSlot);
  }

  lifetimeGemCount = readLifetimeGemCount();

  scheduleNextTap();
}

void loop() {
  handleOnOffButton();
  handleTestModeToggleButton();
  handleTapDurationUpButton();
  handleTapDurationDownButton();
  handleGemResetButton();

  if (!deviceEnabled) {
    currentLcdMode = OFF_MODE;
    updateLcdDisplay();
    return;
  } else if (!deviceAwake) {
    currentLcdMode = SLEEP_MODE;
  } else {
    currentLcdMode = ACTIVE_MODE;
  }

  updateLcdDisplay();

  // Use RTC module to check if device is in waking hours
  // Red LED will light up when in waking hours
  if (rtcAvailable) {
    DateTime now = rtc.now();
    int currentTimeMinutes = now.hour() * 60 + now.minute();

    if (WAKE_UP_TIME < BED_TIME) {
      // Example: WAKE = 6:00, BED = 22:00 → active during day
      deviceAwake = (currentTimeMinutes >= WAKE_UP_TIME && currentTimeMinutes < BED_TIME);
    } else {
      // Example: WAKE = 6:00, BED = 1:00 → active across midnight
      deviceAwake = (currentTimeMinutes >= WAKE_UP_TIME || currentTimeMinutes < BED_TIME);
    }

    if (!deviceAwake) return; // Sleep time: do nothing
  }
  
  unsigned long currentTime = millis();
  if (currentTime >= nextTapTime && deviceEnabled) {
    performTaps(AD_GEMS_MOSFET_GATE_PIN, adGemTaps);
    performTaps(FLOAT_GEMS_MOSFET_GATE_PIN, floatGemTaps);
    lastTapTime = currentTime;
    sessionGemCount += 5;
    activationCount++;
    if (activationCount % 6 == 0) {
      sessionGemCount += 2;
      activationCount = 0;
    }
    scheduleNextTap();
  }

  if (sessionGemCount >= GEMS_PER_SAVE) {
    lifetimeGemCount += sessionGemCount;
    saveGemCount(lifetimeGemCount);
    sessionGemCount = 0;
  }
  displayedGemCount = lifetimeGemCount + sessionGemCount;
}

void scheduleNextTap() {
  long jitter = random(-jitterRange, jitterRange);

  if (random(12) == 0) {
    // Generate a skip interval between 3 and 8 minutes (in milliseconds)
    unsigned long randomSkip = random(3 * 60 * 1000, 8 * 60 * 1000);
    nextTapTime = millis() + baseInterval + randomSkip + jitter;
  } else {
    nextTapTime = millis() + baseInterval + jitter;
  }
  // Serial.print("millis(): ");
  // Serial.println(millis());
  // Serial.print("baseInterval: ");
  // Serial.println(baseInterval);
  // Serial.print("Next tap in ms: ");
  // Serial.println(nextTapTime - millis());
}

void handleOnOffButton() {
  if (millis() - lastOnOffDebounceTime >= debounceDelay) {
    bool onOffButtonState = digitalRead(ONOFF_BUTTON_PIN);
    if (onOffButtonState != lastOnOffButtonState) {
      lastOnOffDebounceTime = millis();
      lastOnOffButtonState = onOffButtonState;

      if (onOffButtonState == LOW) {
        deviceEnabled = !deviceEnabled; // Toggle device ON/OFF
        if (deviceEnabled == true) {
          // nextTapTime = millis() + baseInterval;
          lastTapTime = millis();
          scheduleNextTap();
        }
      }
    }
  }
}

void handleTestModeToggleButton() {
  if (millis() - lastTestModeDebounceTime >= debounceDelay) {
    bool testModeButtonState = digitalRead(MODE_TOGGLE_PIN);
    if (testModeButtonState != lastTestModeButtonState) {
      lastTestModeDebounceTime = millis();
      lastTestModeButtonState = testModeButtonState;

      if (testModeButtonState == LOW) {
        testModeEnabled = !testModeEnabled;  // Toggle test mode

        if (!testModeEnabled) {
          baseInterval = baseIntervalActual;
          jitterRange = jitterRangeActual;
          pauseBetweenTaps = pauseBetweenTapsActual;
          adGemTaps = adGemTapsActual;
          floatGemTaps = floatGemTapsActual;
          testModeMessage = "Test Mode: Off  "; // has to be opposite for some reason
        } else {
          baseInterval = baseIntervalTest;
          jitterRange = jitterRangeTest;
          pauseBetweenTaps = pauseBetweenTapsTest;
          adGemTaps = adGemTapsTest;
          floatGemTaps = floatGemTapsTest;
          testModeMessage = "Test Mode: On   "; // has to be opposite for some reason
        }
        showTestModeMessage = true;
        testModeMessageStart = millis();
      }
    }
  }
}

void handleTapDurationUpButton() {
  if (millis() - lastTapDurationUpDebounceTime >= debounceDelay) {
    bool tapDurationUpButtonState = digitalRead(TAP_DURATION_UP);
    if (tapDurationUpButtonState != lastTapDurationUpButtonState) {
      lastTapDurationUpDebounceTime = millis();
      lastTapDurationUpButtonState = tapDurationUpButtonState;

      if (tapDurationUpButtonState == LOW) {
        tapDuration ++;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Tap Duration:");
        lcd.setCursor(0, 1);
        lcd.print(tapDuration);
        delay(1000);
        lcd.clear();
      }
    }
  }
}

void handleTapDurationDownButton() {
  if (millis() - lastTapDurationDownDebounceTime >= debounceDelay) {
    bool tapDurationDownButtonState = digitalRead(TAP_DURATION_DOWN);
    if (tapDurationDownButtonState != lastTapDurationDownButtonState) {
      lastTapDurationDownDebounceTime = millis();
      lastTapDurationDownButtonState = tapDurationDownButtonState;

      if (tapDurationDownButtonState == LOW) {
        tapDuration --;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Tap Duration:");
        lcd.setCursor(0, 1);
        lcd.print(tapDuration);
        delay(1000);
        lcd.clear();
      }
    }
  }
}

void handleGemResetButton() {
  if (millis() - lastGemResetDebounceTime >= debounceDelay) {
    bool tapGemResetButtonState = digitalRead(GEM_RESET_BUTTON);
    if (tapGemResetButtonState != lastGemResetButtonState) {
      lastGemResetDebounceTime = millis();
      lastGemResetButtonState = tapGemResetButtonState;

      if (tapGemResetButtonState == LOW) {
        sessionGemCount = 0;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Gem Count:");
        lcd.setCursor(0, 1);
        lcd.print("Reset");
        delay(1000);
        lcd.clear();
      }
    }
  }
}


void performTaps(int mosfetPin, int numTaps) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tapping...");
  
  for (int i = 0; i < numTaps; i++) {
    digitalWrite(mosfetPin, HIGH);
    delay(tapDuration);
    digitalWrite(mosfetPin, LOW);
    delay(pauseBetweenTaps);
  }
}

void updateLcdDisplay() {
  if (showTestModeMessage) {
    if (millis() - testModeMessageStart <= testModeMessageDuration) {
      // lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(testModeMessage);
      lcd.setCursor(0, 1);
      lcd.print("                ");
      return;
    } else {
      showTestModeMessage = false;
      lcd.clear();
    }
  }
  // Only update the screen when mode changes or when ACTIVE_MODE is rotating
  if (currentLcdMode != lastLcdMode) {
    lcd.clear();
    lastDisplayChange = millis();  // reset the interval timer
  }

  lastLcdMode = currentLcdMode;

  // === OFF MODE ===
  if (currentLcdMode == OFF_MODE) {
    lcd.setCursor(0, 0);
    lcd.print("Device is off.");
    return;
  }

  // === SLEEP MODE ===
  if (currentLcdMode == SLEEP_MODE) {
    lcdLightLevel = 0;
    analogWrite(LCD_LED, lcdLightLevel);

    lcd.setCursor(0, 0);
    lcd.print("Sleeping...");

    lcd.setCursor(0, 1);
    lcd.print("Wake at: ");

    // Format the time as HH:MM
    lcd.print(WAKE_UP_HOUR);
    lcd.print(':');
    if (WAKE_UP_MINUTE < 10) lcd.print('0');
    lcd.print(WAKE_UP_MINUTE);

    return;
  }

  // === ACTIVE MODE ===
  if (millis() - lastDisplayChange >= displayInterval) {
    displayState = (displayState + 1) % numDisplayStates;
    lastDisplayChange = millis();
    lcd.clear();
    lcdLightLevel = 255;
    analogWrite(LCD_LED, lcdLightLevel);
  }

  lcd.setCursor(0, 0);

  if (displayState == 0) {
    lcd.print("Next tap in:");

    unsigned long now = millis();
    unsigned long timeLeft = (nextTapTime > now) ? (nextTapTime - now) : 0;
    int seconds = (timeLeft / 1000) % 60;
    int minutes = (timeLeft / 1000) / 60;

    lcd.setCursor(0, 1);
    if (minutes < 10) lcd.print('0');
    lcd.print(minutes);
    lcd.print(':');
    if (seconds < 10) lcd.print('0');
    lcd.print(seconds);
  }

  else if (displayState == 1) {
    lcd.print("Lifetime Gems:  ");
    lcd.setCursor(0, 1);
    lcd.print(displayedGemCount);
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
  EEPROM.put(SLOT_INDEX_ADDR, nextSlotIndex);
  EEPROM.put(SLOT_INDEX_ADDR+4, checksum)
}

// Steps:
// Reading
// 1) Read 4-byte gemCount from EEPROM
// 2) Read 1-byte checksum
// 3) If checksum == 0xFF -> treat as uninitialized
// 3) Else -> compute checksum and compare to stored value
// 4) If computed checksum == stored checksum -> proceed as normal
// 4) Else -> corruption detected... do something?
// Writing
// 1) Compute checksum from current gemCount
// 2) Write 4 bytes of gemCount to EEPROM
// 3) Wrtie 1 byte of checksum to EEPROM
//
// Changes:
// 1) 5 bytes per slot instead of 4
// Bytes 1-4 stores gemCount
// Byte 5 stores checksum

void clearGemCountEEPROM() {
  // debug tool function for manually resetting all gem-related EEPROM data
  for (uint16_t i = GEM_SLOTS_START; i < EEPROM.length(); ++i) {
    EEPROM.update(i, 0);  // or EEPROM.write(i, 0)
  }
  EEPROM.put(SLOT_INDEX_ADDR, (uint16_t)0);
}
