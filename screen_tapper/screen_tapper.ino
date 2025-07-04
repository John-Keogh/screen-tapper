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
const int AD_GEMS_MOSFET_GATE_PIN = 26;
const int FLOAT_GEMS_MOSFET_GATE_PIN = 22;
const int ONOFF_BUTTON_PIN = 37;
const int MODE_TOGGLE_PIN = 39;
const int TAP_DURATION_UP = 41;
const int TAP_DURATION_DOWN = 43;
const int OVERRIDE_CLOCK_BUTTON = 45;
const int LCD_LED = 2;

// ==== LCD ====
LiquidCrystal lcd(29, 28, 31, 30, 33, 32);
enum LcdMode { OFF_MODE, SLEEP_MODE, ACTIVE_MODE };
LcdMode currentLcdMode = OFF_MODE;
LcdMode lastLcdMode = ACTIVE_MODE;  // force update on first run
bool lcdEnabled = true;
int lcdLightLevel = 255;

// ==== Button Logic ====
bool deviceEnabled = true;
bool testModeEnabled = false;
bool tapDurationUpMessageActive = false;
unsigned long tapDurationUpMessageStart = 0;
bool tapDurationDownMessageActive = false;
unsigned long tapDurationDownMessageStart = 0;
bool overrideClock = false;

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

bool lastOverrideClockButtonState = LOW;
unsigned long lastOverrideClockDebounceTime = 0;

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
const unsigned long baseIntervalActual = 670000;
const unsigned long jitterRangeActual = 16000;
const unsigned long pauseBetweenTapsActual = 1250;
const unsigned long adGemTapsActual = 7;
const unsigned long floatGemTapsActual = 10;

const unsigned long baseIntervalTest = 13000;
const unsigned long jitterRangeTest = 0;
const unsigned long pauseBetweenTapsTest = 1000;
const unsigned long adGemTapsTest = 3;
const unsigned long floatGemTapsTest = 3;

unsigned long baseInterval = baseIntervalActual;
unsigned long jitterRange = jitterRangeActual;
unsigned long pauseBetweenTaps = pauseBetweenTapsActual;
unsigned long adGemTaps = adGemTapsActual;
unsigned long floatGemTaps = floatGemTapsActual;
unsigned long tapDuration = 50;

// ==== LCD Display Timing ====
unsigned long lastDisplayChange = 0;
const unsigned long displayInterval = 3000;
int displayState = 0;
const int numDisplayStates = 2;

// ==== Test Mode Message ====
bool showTestModeMessage = false;
String testModeMessage = "";
unsigned long testModeMessageStart = 0;
const unsigned long testModeMessageDuration = 1500;

// ==== Override Clock Message ====
bool showOverrideClockMessage = false;
String overrideClockMessageTop = "";
String overrideClockMessageBottom = "";
unsigned long overrideClockMessageStart = 0;
const unsigned long overrideClockMessageDuration = 1500;

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

  Wire.begin();
  rtcAvailable = rtc.begin();

  pinMode(AD_GEMS_MOSFET_GATE_PIN, OUTPUT);
  pinMode(FLOAT_GEMS_MOSFET_GATE_PIN, OUTPUT);
  pinMode(ONOFF_BUTTON_PIN, INPUT);
  pinMode(MODE_TOGGLE_PIN, INPUT);
  pinMode(TAP_DURATION_UP, INPUT);
  pinMode(TAP_DURATION_DOWN, INPUT);
  pinMode(OVERRIDE_CLOCK_BUTTON, INPUT);
  pinMode(LCD_LED, OUTPUT);
  analogWrite(LCD_LED, 255); // 0 is always off; 255 is always on
  randomSeed(analogRead(0));

  lcd.begin(16, 2);
  lcd.print("Initializing...");
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

// @@@@@ LOOP FUNCTION @@@@@
void loop() {
  handleOnOffButton();
  handleTestModeToggleButton();
  handleTapDurationUpButton();
  handleTapDurationDownButton();
  handleOverrideClockButton();

  // Check if device is in waking hours
  if (rtcAvailable && !overrideClock) {
    checkRTC();
  }

  if (!deviceEnabled) {
    currentLcdMode = OFF_MODE;
    updateLcdDisplay();
    return;
  } else if (!deviceAwake && !overrideClock) {
    currentLcdMode = SLEEP_MODE;
    updateLcdDisplay();
    return;
  } else {
    currentLcdMode = ACTIVE_MODE;
  }

  updateLcdDisplay();

  updateTapSequence();
  
  unsigned long currentTime = millis();
  if (currentTime >= nextTapTime && deviceEnabled && !tappingActive) {
    Serial.println("");
    Serial.println("Starting tap sequence.");
    startTapSequence(AD_GEMS_MOSFET_GATE_PIN, adGemTaps);
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

void checkRTC() {
  DateTime now = rtc.now();
  int currentTimeMinutes = now.hour() * 60 + now.minute();

  if (WAKE_UP_TIME < BED_TIME) {
    deviceAwake = (currentTimeMinutes >= WAKE_UP_TIME && currentTimeMinutes < BED_TIME);
  } else {
    deviceAwake = (currentTimeMinutes >= WAKE_UP_TIME || currentTimeMinutes < BED_TIME);
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
  lcd.clear();
}

void updateTapSequence() {
  if (!tappingActive) return;

  unsigned long now = millis();

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
        scheduleNextTap();
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
        tapDuration++;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Tap Duration:");
        lcd.setCursor(0, 1);
        lcd.print(tapDuration);

        // Start the message timer
        tapDurationUpMessageStart = millis();
        tapDurationUpMessageActive = true;
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
        tapDuration--;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Tap Duration:");
        lcd.setCursor(0, 1);
        lcd.print(tapDuration);

        // Start the message timer
        tapDurationDownMessageStart = millis();
        tapDurationDownMessageActive = true;
      }
    }
  }
}

void handleOverrideClockButton() {
  if (millis() - lastOverrideClockDebounceTime >= debounceDelay) {
    bool overrideClockButtonState = digitalRead(OVERRIDE_CLOCK_BUTTON);
    if (overrideClockButtonState != lastOverrideClockButtonState) {
      lastOverrideClockDebounceTime = millis();
      lastOverrideClockButtonState = overrideClockButtonState;

      if (overrideClockButtonState == LOW) {
        overrideClock = !overrideClock;

        if (!overrideClock) {
          overrideClockMessageTop = "Override        ";
          overrideClockMessageBottom = "Clock: Inactive ";
        } else {
          overrideClockMessageTop = "Override        ";
          overrideClockMessageBottom = "Clock: Active   ";
          analogWrite(LCD_LED, 255);
        }
        showOverrideClockMessage = true;
        overrideClockMessageStart = millis();      }
    }
  }
}

void updateLcdDisplay() {
  // === TEST MODE ===
  if (showTestModeMessage) {
    if (millis() - testModeMessageStart <= testModeMessageDuration) {
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

  // === OVERRIDE CLOCK ===
  if (showOverrideClockMessage) {
    if (millis() - overrideClockMessageStart <= overrideClockMessageDuration) {
      lcd.setCursor(0, 0);
      lcd.print(overrideClockMessageTop);
      lcd.setCursor(0, 1);
      lcd.print(overrideClockMessageBottom);
      return;
    } else {
      showOverrideClockMessage = false;
      lcd.clear();
    }
  }

  // Only update the screen when mode changes or when ACTIVE_MODE is rotating
  if (currentLcdMode != lastLcdMode) {
    lcd.clear();
    lastDisplayChange = millis();  // reset the interval timer
  }

  // === TAP DURATION UP ===
  if (tapDurationUpMessageActive) {
    if (millis() - tapDurationUpMessageStart >= 1000) {
      lcd.clear();
      tapDurationUpMessageActive = false;
      lastDisplayChange = millis();
    } else {
      lcd.setCursor(0, 0);
      lcd.print("Tap Duration:");
      lcd.setCursor(0, 1);
      lcd.print(tapDuration);
      return;
    }
  }

  // === TAP DURATION DOWN ===
  if (tapDurationDownMessageActive) {
    if (millis() - tapDurationDownMessageStart >= 1000) {
      lcd.clear();
      tapDurationDownMessageActive = false;
      lastDisplayChange = millis();
    } else {
      lcd.setCursor(0, 0);
      lcd.print("Tap Duration:");
      lcd.setCursor(0, 1);
      lcd.print(tapDuration);
      return;
    }
  }

  // === ACTIVELY TAPPING ===
  if (tappingActive) {
    lcd.setCursor(0, 0);
    lcd.print("Tapping...");
    return;
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
    lcd.print("Next tap in:    ");

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
    char formattedGemCount[20];
    formatNumberWithCommas(displayedGemCount, formattedGemCount);
    lcd.print(formattedGemCount);
  }
}

void formatNumberWithCommas(uint32_t value, char* outBuffer) {
  char temp[16];  // Temporary buffer for the numeric string
  sprintf(temp, "%lu", value);  // Convert number to string

  int len = strlen(temp);
  if (len > 12) return;

  int j = 0;

  for (int i = 0; i < len; ++i) {
    outBuffer[j++] = temp[i];

    // Insert comma if there are still digits ahead and it's at a 3-digit boundary from the end
    if (((len - i - 1) % 3 == 0) && (i != len - 1)) {
      outBuffer[j++] = ',';
    }
  }

  outBuffer[j] = '\0';  // Null-terminate the final string
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