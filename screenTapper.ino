#include <LiquidCrystal.h>
#include <Wire.h>
#include <RTClib.h>

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

// ==== LCD ====
LiquidCrystal lcd(29, 28, 31, 30, 33, 32);
enum LcdMode { OFF_MODE, SLEEP_MODE, ACTIVE_MODE };
LcdMode currentLcdMode = OFF_MODE;
LcdMode lastLcdMode = ACTIVE_MODE;  // force update on first run
bool lcdEnabled = true;

// ==== Button Logic ====
bool deviceEnabled = false;
bool testModeEnabled = true;  // actual device starts as false

// Shared debounce delay for all buttons
const unsigned long debounceDelay = 50;

// === Button States ===
bool lastOnOffButtonState = LOW;
unsigned long lastOnOffDebounceTime = 0;

bool lastLcdButtonState = LOW;
unsigned long lastLcdDebounceTime = 0;

bool lastTestModeButtonState = LOW;
unsigned long lastTestModeDebounceTime = 0;

// ==== Timing for Solenoids ====
unsigned long nextTapTime = 0;
unsigned long lastTapTime = 0;

// ==== Mode Parameters ====
const unsigned long baseIntervalActual = 660000;
const unsigned long jitterRangeActual = 12000;
const unsigned long tapDurationActual = 35;
const unsigned long pauseBetweenTapsActual = 1000;
const unsigned long adGemTapsActual = 5;
const unsigned long floatGemTapsActual = 12;

const unsigned long baseIntervalTest = 15000;
const unsigned long jitterRangeTest = 0;
const unsigned long tapDurationTest = 35;
const unsigned long pauseBetweenTapsTest = 1000;
const unsigned long adGemTapsTest = 3;
const unsigned long floatGemTapsTest = 3;

unsigned long baseInterval = baseIntervalActual;
unsigned long jitterRange = jitterRangeActual;
unsigned long tapDuration = tapDurationActual;
unsigned long pauseBetweenTaps = pauseBetweenTapsActual;
unsigned long adGemTaps = adGemTapsActual;
unsigned long floatGemTaps = floatGemTapsActual;

// ==== Gem Tracking ====
unsigned long gemCount = 0;
unsigned long activationCount = 0;

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
  randomSeed(analogRead(0));

  scheduleNextTap();
}

void loop() {
  handleOnOffButton();
  handleTestModeToggleButton();

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
    gemCount += 5;
    activationCount++;
    if (activationCount % 6 == 0) {
      gemCount += 2;
      activationCount = 0;
    }
    scheduleNextTap();
  }
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
  Serial.print("millis(): ");
  Serial.println(millis());
  Serial.print("baseInterval: ");
  Serial.println(baseInterval);
  Serial.print("Next tap in ms: ");
  Serial.println(nextTapTime - millis());
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
          tapDuration = tapDurationActual;
          pauseBetweenTaps = pauseBetweenTapsActual;
          adGemTaps = adGemTapsActual;
          floatGemTaps = floatGemTapsActual;
          testModeMessage = "Test Mode: Off  "; // has to be opposite for some reason
        } else {
          baseInterval = baseIntervalTest;
          jitterRange = jitterRangeTest;
          tapDuration = tapDurationTest;
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
    lcd.setCursor(0, 0);
    lcd.print("Sleeping...");

    lcd.setCursor(0, 1);
    lcd.print("Wake at: ");

    // Format the time as HH:MM
    // if (WAKE_UP_HOUR < 10) lcd.print('0');
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
    lcd.print("Gems Collected:");
    lcd.setCursor(0, 1);
    lcd.print(gemCount);
  }
}