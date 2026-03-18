#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// -------------------- PIN ASSIGNMENTS --------------------
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

// -------------------- GLOBAL OBJECTS --------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servo1, servo2, servo3;

// -------------------- VARIABLES --------------------
bool running = false;
int mode = 1;
int setReps = 10;
int remainingReps = 0;
bool solenoidState = false;

// -------------------- MODE 1 VARIABLES --------------------
unsigned long lastToggleTime = 0;

// -------------------- MODE 2 + 3 (Servo) VARIABLES --------------------
int pos1 = 90, pos2 = 90, pos3 = 90;
int target1 = 135, target2 = 45;
int servo3Dir = 1;
int phase = 0; // for servo pair phase control

unsigned long lastStepTime = 0;
unsigned long stepDelay = 25;
bool pausing = false;
unsigned long pauseStart = 0;
unsigned long pauseDuration = 1000; // 1 sec pause at extremes

// -------------------- DEBOUNCE --------------------
unsigned long lastDebounceTime_mode = 0;
unsigned long lastDebounceTime_start = 0;
const unsigned long debounceDelay = 250;
bool lastButtonState_mode = HIGH;
bool lastButtonState_start = HIGH;

// -------------------- FUNCTION DECLARATIONS --------------------
void updateLCD();
void mode1_process();
void mode2_process();
void mode3_process();
void resetRelaysAndServos();
void goNeutral();

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200);

  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(RELAY_SOLENOID, OUTPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  digitalWrite(RELAY_SOLENOID, HIGH);
  digitalWrite(RELAY_PUMP, HIGH);

  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();

  // Servos
  servo1.attach(SERVO_1_PIN);
  servo2.attach(SERVO_2_PIN);
  servo3.attach(SERVO_3_PIN);
  goNeutral();

  updateLCD();
}

// -------------------- LOOP --------------------
void loop() {
  bool currentModeState = digitalRead(BTN_MODE);
  bool currentStartState = digitalRead(BTN_START);

  // --- Mode Button ---
  if (currentModeState == LOW && lastButtonState_mode == HIGH && (millis() - lastDebounceTime_mode > debounceDelay)) {
    lastDebounceTime_mode = millis();
    
    int previousMode = mode;   // store old mode before changing
    goNeutral();

    mode++;
    if (mode > 3) mode = 1;

    // Handle transitions between modes
    handleModeTransition(previousMode, mode);

    Serial.print("Mode changed to: ");
    Serial.println(mode);

    running = false;
    resetRelaysAndServos();
    updateLCD();
  }

  lastButtonState_mode = currentModeState;

  // --- Start/Stop Button ---
  if (currentStartState == LOW && lastButtonState_start == HIGH && (millis() - lastDebounceTime_start > debounceDelay)) {
    lastDebounceTime_start = millis();
    running = !running;
    Serial.print("Running: ");
    Serial.println(running);
    if (running) {
      remainingReps = setReps;
      pausing = false;
      phase = 0;
    } else {
      resetRelaysAndServos();
    }
    updateLCD();
  }
  lastButtonState_start = currentStartState;

  // Adjust reps when idle
  if (!running) {
    int potValue = analogRead(POT_PIN);
    setReps = map(potValue, 0, 4095, 5, 50);
  }

  // Execute mode logic
  if (running) {
    if (mode == 1) mode1_process();
    else if (mode == 2) mode2_process();
    else if (mode == 3) mode3_process();
  }

  updateLCD();
  delay(50);
}

// ------------------------------------------------------------
// MODE 1: Air Pump + Solenoid Toggle
// ------------------------------------------------------------
void mode1_process() {
  digitalWrite(RELAY_PUMP, LOW); // Pump ON
  unsigned long currentTime = millis();

  if (currentTime - lastToggleTime >= 5000) { // toggle every 5 sec
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

// ------------------------------------------------------------
// MODE 2: Dual Servo (Servo1 + Servo2)
// ------------------------------------------------------------
void mode2_process() {
  // Run one full repetition cycle
  for (int i = 90; i <= 135; i++) {
    servo1.write(i);
    servo2.write(180 - i);
    delay(25);
  }

  delay(2000); // pause at extreme

  for (int i = 135; i >= 90; i--) {
    servo1.write(i);
    servo2.write(180 - i);
    delay(25);
  }

  for (int i = 90; i >= 45; i--) {
    servo1.write(i);
    servo2.write(180 - i);
    delay(25);
  }

  delay(2000); // pause at other extreme

  for (int i = 45; i <= 90; i++) {
    servo1.write(i);
    servo2.write(180 - i);
    delay(25);
  }

  // One repetition complete
  delay(2000);
  remainingReps--;
  Serial.print("Remaining Reps: ");
  Serial.println(remainingReps);

  if (remainingReps <= 0) {
    running = false;
    resetRelaysAndServos();
    updateLCD();
  }
}

// ------------------------------------------------------------
// MODE 3: Single Servo (Servo3)
// ------------------------------------------------------------
void mode3_process() {
  unsigned long currentTime = millis();

  if (pausing) {
    if (currentTime - pauseStart >= pauseDuration) {
      pausing = false;
    } else {
      return; // still pausing
    }
  }

  if (currentTime - lastStepTime >= stepDelay) {
    lastStepTime = currentTime;

    pos3 += servo3Dir;
    servo3.write(pos3);

    if (pos3 >= 135) {
      servo3Dir = -1;
      pausing = true;
      pauseStart = millis();
    }
    if (pos3 <= 45) {
      servo3Dir = 1;
      pausing = true;
      pauseStart = millis();
      remainingReps--;
      Serial.print("Remaining Reps: ");
      Serial.println(remainingReps);
      if (remainingReps <= 0) {
        running = false;
        updateLCD();
        return;
      }
    }
  }
}

// ------------------------------------------------------------
// RESET RELAYS & SERVOS
// ------------------------------------------------------------
void resetRelaysAndServos() {
  digitalWrite(RELAY_SOLENOID, HIGH);
  digitalWrite(RELAY_PUMP, HIGH);
  goNeutral();
}

// ------------------------------------------------------------
// GO TO NEUTRAL POSITION
// ------------------------------------------------------------
void goNeutral() {
  pos1 = pos2 = pos3 = 90;
  target1 = target2 = 90;
  servo1.write(90);
  servo2.write(90);
  servo3.write(90);
  delay(500);
}

// ------------------------------------------------------------
// LCD DISPLAY
// ------------------------------------------------------------
void updateLCD() {
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
}

// ------------------------------------------------------------
// HANDLE MODE TRANSITIONS
// ------------------------------------------------------------
void handleModeTransition(int oldMode, int newMode) {
  Serial.print("Transition: Mode ");
  Serial.print(oldMode);
  Serial.print(" -> Mode ");
  Serial.println(newMode);

  // --- Mode 1 -> Mode 2 ---
  if (oldMode == 1 && newMode == 2) {
    Serial.println("Releasing air: Activating pump for 2s...");
    digitalWrite(RELAY_SOLENOID, LOW);   // Turn ON pump (active LOW)
    delay(2000);                     // Run for 2 seconds
    digitalWrite(RELAY_SOLENOID, HIGH);  // Turn OFF pump
  }

  // --- Mode 2 -> Mode 3 ---
  else if (oldMode == 2 && newMode == 3) {
    Serial.println("Resetting Servo1 and Servo2 to neutral...");
    servo1.write(90);
    servo2.write(90);
    delay(500);
  }

  // --- Mode 3 -> Mode 1 ---
  else if (oldMode == 3 && newMode == 1) {
    Serial.println("Resetting Servo3 to neutral...");
    servo3.write(90);
    delay(500);
  }
}

