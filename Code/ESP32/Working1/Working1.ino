#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Wire.h>
#include <MPU6050_light.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <FirebaseClient.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// -----------------------------
// Wi-Fi + Firebase Configuration
// -----------------------------
#define WIFI_SSID "12345678"
#define WIFI_PASSWORD "12345678"
#define Web_API_KEY "AIzaSyAHNVsFVFqipyglpEbuW8EI2N0j3Nf8sxo"
#define DATABASE_URL "https://paralysis-hand-recovery-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "yashasrgupta@gmail.com"
#define USER_PASS "123456789"

// -----------------------------
// Firebase Objects
// -----------------------------
void processData(AsyncResult &aResult);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);

// -----------------------------
// IMU System Variables
// -----------------------------
MPU6050 mpu(Wire);
unsigned long imu_timer = 0;
int imu_mode = 1;
const int imu_modeButtonPin = 0;
const int imu_sessionButtonPin = 33;
bool imu_lastButtonState = HIGH;
unsigned long imu_lastDebounceTime = 0;
const unsigned long imu_debounceDelay = 200;
int sessionID = 1;
const int EEPROM_SIZE = 4;
unsigned long pressStartTime = 0;
bool isHolding = false;

// -----------------------------
// Training System Variables
// -----------------------------
#define BTN_MODE 33
#define BTN_START 0
#define RELAY_SOLENOID 26
#define RELAY_PUMP 27
#define SERVO_1_PIN 18
#define SERVO_2_PIN 19
#define SERVO_3_PIN 23
#define POT_PIN 35
#define I2C_SDA 21
#define I2C_SCL 22

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servo1, servo2, servo3;
bool running = false;
int mode = 1;
int setReps = 10;
int remainingReps = 0;
bool solenoidState = false;

unsigned long lastToggleTime = 0;
int pos1 = 90, pos2 = 90, pos3 = 90;
int dir1 = 1;
int phase = 0;
unsigned long lastStepTime = 0;
unsigned long stepDelay = 25;
bool pausing = false;
unsigned long pauseStart = 0;
unsigned long pauseDuration = 1000;
unsigned long lastDebounceTime_mode = 0;
unsigned long lastDebounceTime_start = 0;
const unsigned long debounceDelay = 250;
bool lastButtonState_mode = HIGH;
bool lastButtonState_start = HIGH;

// -----------------------------
// Mode Toggle Switch (System Selector)
// -----------------------------
#define TOGGLE_PIN 32 // HIGH = IMU Firebase, LOW = Training
bool toggleState;

// -----------------------------
// Helper & Forward Declarations
// -----------------------------
void connectWiFi();
void setupFirebase();
void loadSessionID();
void saveSessionID();
void imuSystemSetup();
void imuSystemLoop();
void trainingSystemSetup();
void trainingSystemLoop();
void updateLCD();
void mode1_process();
void mode2_process();
void mode3_process();
void resetRelaysAndServos();
void goNeutral(bool smooth = false);
void handleModeTransition(int oldMode, int newMode);
bool checkToggleAndAbortTraining(); // returns true if toggle changed to IMU (HIGH) and we aborted

// ===========================================================
// SETUP
// ===========================================================
void setup() {
  Serial.begin(115200);

  // initialize toggle pin first so early checks work
  pinMode(TOGGLE_PIN, INPUT_PULLUP);

  toggleState = digitalRead(TOGGLE_PIN);
  if (toggleState) {
    imuSystemSetup();
    Serial.println("Boot: IMU mode selected (toggle HIGH).");
  } else {
    trainingSystemSetup();
    Serial.println("Boot: Training mode selected (toggle LOW).");
  }
}

// ===========================================================
// LOOP
// ===========================================================
void loop() {
  // Always read toggle each loop to decide which system should run
  bool currentToggle = digitalRead(TOGGLE_PIN);
  if (currentToggle && !toggleState) {
    // switching from training -> IMU
    Serial.println("Toggle moved to HIGH: switching to IMU system.");
    // run a safe stop for training
    resetRelaysAndServos();
    goNeutral(false); // immediate neutral
    // initialize IMU system if not already (do light init)
    imuSystemSetup();
  } else if (!currentToggle && toggleState) {
    // switching from IMU -> training
    Serial.println("Toggle moved to LOW: switching to Training system.");
    trainingSystemSetup();
  }
  toggleState = currentToggle;

  // run the active system — these are expected to be non-blocking or to check the toggle frequently
  if (toggleState) {
    imuSystemLoop();
  } else {
    trainingSystemLoop();
  }
}

// ===========================================================
// ---------------- IMU FIREBASE SYSTEM ----------------
// ===========================================================
void imuSystemSetup() {
  Wire.begin();
  pinMode(imu_modeButtonPin, INPUT_PULLUP);
  pinMode(imu_sessionButtonPin, INPUT_PULLUP);

  connectWiFi();
  setupFirebase();

  Serial.println("Initializing MPU6050...");
  byte status = mpu.begin();
  Serial.print("MPU6050 status: ");
  Serial.println(status);

  Serial.println("Calculating offsets...");
  delay(1000);
  mpu.calcGyroOffsets();
  Serial.println("Done!");

  loadSessionID();
}

void imuSystemLoop() {
  app.loop();
  mpu.update();

  int reading = digitalRead(imu_modeButtonPin);
  if (reading != imu_lastButtonState) imu_lastDebounceTime = millis();
  if ((imu_lastButtonState == LOW) && (millis() - imu_lastDebounceTime) > imu_debounceDelay) {
    imu_mode = (imu_mode == 1) ? 2 : 1;
    Serial.print("IMU Mode changed to: ");
    Serial.println(imu_mode);
    Database.set<int>(aClient, "/IMU/mode", imu_mode, processData, "mode");
  }
  imu_lastButtonState = reading;

  int sessionBtnState = digitalRead(imu_sessionButtonPin);
  if (sessionBtnState == LOW) {
    if (!isHolding) {
      isHolding = true;
      pressStartTime = millis();
    } else if (millis() - pressStartTime > 10000) {
      sessionID = 1;
      saveSessionID();
      Serial.println("Session ID reset to 1");
      Database.set<int>(aClient, "/IMU/sessionID", sessionID, processData, "sessionID");
    }
  } else {
    if (isHolding) {
      unsigned long pressDuration = millis() - pressStartTime;
      if (pressDuration < 10000) {
        sessionID++;
        saveSessionID();
        Serial.print("Session incremented → ");
        Serial.println(sessionID);
        Database.set<int>(aClient, "/IMU/sessionID", sessionID, processData, "sessionID");
      }
      isHolding = false;
    }
  }

  if ((millis() - imu_timer) > 1000) {
    imu_timer = millis();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Mode: ");
    lcd.print(imu_mode);
    lcd.print("  S:");
    lcd.print(sessionID);

    lcd.setCursor(0, 1);
    if (imu_mode == 1) {
      int yVal = int(mpu.getAngleY());
      lcd.print("Y: ");
      lcd.print(yVal);
      Database.set<int>(aClient, "/IMU/y", yVal, processData, "y");
    } else {
      int zVal = int(mpu.getAngleZ());
      lcd.print("Z: ");
      lcd.print(zVal);
      Database.set<int>(aClient, "/IMU/z", zVal, processData, "z");
    }
  }
}

// ---------------- Firebase Helper Functions ----------------
void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;
  if (aResult.isError()) {
    Firebase.printf("Error task: %s, msg: %s, code: %d\n",
                    aResult.uid().c_str(),
                    aResult.error().message().c_str(),
                    aResult.error().code());
  }
}

void connectWiFi() {
  Serial.print("Connecting to Wi-Fi ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retry++;
    if (retry > 60) {
      Serial.println("\nWi-Fi connect failed — restarting...");
      ESP.restart();
    }
  }
  Serial.println("\nWi-Fi connected");
}

void setupFirebase() {
  ssl_client.setInsecure();
  initializeApp(aClient, app, getAuth(user_auth), processData, "auth");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
  Serial.println("Firebase initialized");
}

void loadSessionID() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, sessionID);
  if (sessionID <= 0 || sessionID > 100000) {
    sessionID = 1;
    EEPROM.put(0, sessionID);
    EEPROM.commit();
  }
}

void saveSessionID() {
  EEPROM.put(0, sessionID);
  EEPROM.commit();
}

// ===========================================================
// ---------------- TRAINING SYSTEM ----------------
// ===========================================================
void trainingSystemSetup() {
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(RELAY_SOLENOID, OUTPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  digitalWrite(RELAY_SOLENOID, LOW);
  digitalWrite(RELAY_PUMP, LOW);

  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();

  servo1.attach(SERVO_1_PIN);
  servo2.attach(SERVO_2_PIN);
  servo3.attach(SERVO_3_PIN);

  // ensure safe neutral positions at start
  pos1 = pos2 = pos3 = 90;
  goNeutral(false); // immediate neutral on setup
  updateLCD();

  running = false;
  Serial.println("Training system ready");
}

void trainingSystemLoop() {
  // quick check to allow toggle to switch externally
  if (checkToggleAndAbortTraining()) return;

  bool currentModeState = digitalRead(BTN_MODE);
  bool currentStartState = digitalRead(BTN_START);

  // Mode Button (with debounce)
  if (currentModeState == LOW && lastButtonState_mode == HIGH && (millis() - lastDebounceTime_mode > debounceDelay)) {
    lastDebounceTime_mode = millis();
    int previousMode = mode;
    goNeutral(true);
    mode++;
    if (mode > 3) mode = 1;
    handleModeTransition(previousMode, mode);
    running = false;
    resetRelaysAndServos();
    updateLCD();
  }
  lastButtonState_mode = currentModeState;

  // Start/Stop Button (with debounce)
  if (currentStartState == LOW && lastButtonState_start == HIGH && (millis() - lastDebounceTime_start > debounceDelay)) {
    lastDebounceTime_start = millis();
    running = !running;
    if (running) {
      remainingReps = setReps;
      pausing = false;
      phase = 0;
    } else {
      resetRelaysAndServos();
      goNeutral(true);
    }
    updateLCD();
  }
  lastButtonState_start = currentStartState;

  if (!running) {
    int potValue = analogRead(POT_PIN);
    setReps = map(potValue, 4095, 0, 5, 50);
  }

  if (running) {
    // inside each mode process we also check toggle frequently
    if (mode == 1) mode1_process();
    else if (mode == 2) mode2_process();
    else if (mode == 3) mode3_process();
  }

  updateLCD();
  // short non-blocking sleep: loop returns fast so toggle is read frequently
  delay(20);
}

// Helper that other functions call; if TOGGLE_PIN is HIGH it stops training safely and returns true
bool checkToggleAndAbortTraining() {
  if (digitalRead(TOGGLE_PIN) == HIGH) {
    // We must stop training and prepare to switch to IMU mode
    Serial.println("Toggle HIGH detected inside training -> aborting training and returning to IMU mode.");
    running = false;
    resetRelaysAndServos();
    goNeutral(false); // immediate neutral
    // Note: top-level loop will re-run imuSystemSetup on the next iteration
    return true;
  }
  return false;
}

// Non-blocking neutral movement but still loops — checks toggle inside to allow immediate abort
void goNeutral(bool smooth) {
  if (!smooth) {
    pos1 = pos2 = pos3 = 90;
    servo1.write(90);
    servo2.write(90);
    servo3.write(90);
    return;
  }

  // Smooth move in small steps (allowing toggle checks)
  while (pos1 != 90 || pos2 != 90 || pos3 != 90) {
    if (digitalRead(TOGGLE_PIN) == HIGH) {
      // abort neutral early if toggle to IMU — caller will handle switching
      Serial.println("Toggle HIGH detected during goNeutral — aborting neutral.");
      return;
    }

    if (pos1 < 90) pos1++;
    else if (pos1 > 90) pos1--;

    if (pos2 < 90) pos2++;
    else if (pos2 > 90) pos2--;

    if (pos3 < 90) pos3++;
    else if (pos3 > 90) pos3--;

    servo1.write(pos1);
    servo2.write(pos2);
    servo3.write(pos3);

    delay(15); // small step delay but toggle is checked each step
  }
}

void updateLCD() {
  // quick check to allow toggle switch out
  if (checkToggleAndAbortTraining()) return;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mode: ");
  lcd.print(mode);

  lcd.setCursor(0, 1);
  if (!running) {
    lcd.print("Set Reps: ");
    lcd.print(setReps);
  } else {
    lcd.print("Reps Left: ");
    lcd.print(remainingReps);
  }
  delay(500);
}

void handleModeTransition(int oldMode, int newMode) {
  Serial.print("Transition: Mode ");
  Serial.print(oldMode);
  Serial.print(" -> Mode ");
  Serial.println(newMode);

  if (oldMode == 1 && newMode == 2) {
    Serial.println("Releasing air: Activating pump for 2s (non-blocking-check)...");
    // Activate for ~2000 ms but check toggle frequently
    digitalWrite(RELAY_SOLENOID, HIGH);
    unsigned long end = millis() + 2000;
    while (millis() < end) {
      if (digitalRead(TOGGLE_PIN) == HIGH) {
        digitalWrite(RELAY_SOLENOID, LOW);
        Serial.println("Toggle HIGH detected during pump activation — aborting transition.");
        return;
      }
      delay(10);
    }
    digitalWrite(RELAY_SOLENOID, LOW);
  } else if (oldMode == 2 && newMode == 3) {
    Serial.println("Resetting Servo1 and Servo2...");
    goNeutral(true);
  } else if (oldMode == 3 && newMode == 1) {
    Serial.println("Resetting Servo3...");
    goNeutral(true);
  }
}

void resetRelaysAndServos() {
  digitalWrite(RELAY_SOLENOID, LOW);
  digitalWrite(RELAY_PUMP, LOW);
}

// MODE 1 process
void mode1_process() {
  if (checkToggleAndAbortTraining()) return;

  digitalWrite(RELAY_PUMP, HIGH);
  unsigned long currentTime = millis();

  if (currentTime - lastToggleTime >= 3000) {
    if (checkToggleAndAbortTraining()) return;

    solenoidState = !solenoidState;
    digitalWrite(RELAY_SOLENOID, solenoidState ? LOW : HIGH);
    lastToggleTime = currentTime;

    if (!solenoidState) {
      remainingReps--;
      Serial.print("Remaining Reps: ");
      Serial.println(remainingReps);
      if (remainingReps <= 0) {
        running = false;
        resetRelaysAndServos();
        updateLCD();
      }
    }
  }
}

// MODE 2 process (dual servo)
void mode2_process() {
  if (checkToggleAndAbortTraining()) return;

  // Sweep 1: pos1 = 90 → 140  (pos2 = 180 - pos1)
  for (pos1 = 90; pos1 <= 140; pos1 += 1) {
    if (!running || checkToggleAndAbortTraining()) return;
    if (digitalRead(BTN_START) == LOW) {
      running = false;
      Serial.println("Stopped mid-sweep (90→140).");
      return;
    }

    pos2 = 180 - pos1;
    servo1.write(pos1);
    servo2.write(pos2);
    delay(15);
  }

  if (!running || checkToggleAndAbortTraining()) return;
  delay(300);

  // Sweep 2: pos1 = 140 → 90
  for (pos1 = 140; pos1 >= 90; pos1 -= 1) {
    if (!running || checkToggleAndAbortTraining()) return;
    if (digitalRead(BTN_START) == LOW) {
      running = false;
      Serial.println("Stopped mid-sweep (140→90).");
      return;
    }

    pos2 = 180 - pos1;
    servo1.write(pos1);
    servo2.write(pos2);
    delay(15);
  }

  if (!running || checkToggleAndAbortTraining()) return;
  delay(300);

  // Sweep 3: pos1 = 90 → 40
  for (pos1 = 90; pos1 >= 40; pos1 -= 1) {
    if (!running || checkToggleAndAbortTraining()) return;
    if (digitalRead(BTN_START) == LOW) {
      running = false;
      Serial.println("Stopped mid-sweep (90→40).");
      return;
    }

    pos2 = 180 - pos1;
    servo1.write(pos1);
    servo2.write(pos2);
    delay(15);
  }

  if (!running || checkToggleAndAbortTraining()) return;
  delay(300);

  // Sweep 4: pos1 = 40 → 90
  for (pos1 = 40; pos1 <= 90; pos1 += 1) {
    if (!running || checkToggleAndAbortTraining()) return;
    if (digitalRead(BTN_START) == LOW) {
      running = false;
      Serial.println("Stopped mid-sweep (40→90).");
      return;
    }

    pos2 = 180 - pos1;
    servo1.write(pos1);
    servo2.write(pos2);
    delay(15);
  }

  // One full rep done
  remainingReps--;
  Serial.print("Remaining Reps: ");
  Serial.println(remainingReps);

  if (remainingReps <= 0) {
    running = false;
    updateLCD();
  }
}

// MODE 3 process (single servo)
void mode3_process() {
  // Abort immediately if toggle goes HIGH (switch to IMU mode)
  if (checkToggleAndAbortTraining()) return;

  // Three sweep phases like in your reference example
  // Sweep 1: from 90° → 135°
  for (pos3 = 90; pos3 <= 135; pos3 += 1) {
    // Check stop or toggle at every step
    if (!running || checkToggleAndAbortTraining()) return;
    if (digitalRead(BTN_START) == LOW) {  // Stop button pressed
      running = false;
      Serial.println("Stopped mid-sweep (90→135).");
      return;
    }
    servo3.write(pos3);
    delay(15);
  }

  // Small pause at end of sweep
  if (!running || checkToggleAndAbortTraining()) return;
  delay(300);

  // Sweep 2: from 135° → 45°
  for (pos3 = 135; pos3 >= 45; pos3 -= 1) {
    if (!running || checkToggleAndAbortTraining()) return;
    if (digitalRead(BTN_START) == LOW) {
      running = false;
      Serial.println("Stopped mid-sweep (135→45).");
      return;
    }
    servo3.write(pos3);
    delay(15);
  }

  if (!running || checkToggleAndAbortTraining()) return;
  delay(300);

  // Sweep 3: from 45° → 90° (return to neutral)
  for (pos3 = 45; pos3 <= 90; pos3 += 1) {
    if (!running || checkToggleAndAbortTraining()) return;
    if (digitalRead(BTN_START) == LOW) {
      running = false;
      Serial.println("Stopped mid-sweep (45→90).");
      return;
    }
    servo3.write(pos3);
    delay(15);
  }

  // One full rep done
  remainingReps--;
  Serial.print("Remaining Reps: ");
  Serial.println(remainingReps);

  if (remainingReps <= 0) {
    running = false;
    updateLCD();
  }
}