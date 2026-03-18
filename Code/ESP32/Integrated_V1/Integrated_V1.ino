#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// -------------------- PIN ASSIGNMENTS --------------------
#define BTN_MODE 33            // Mode switch button
#define BTN_START 0            // Inbuilt BOOT button (start/stop)
#define RELAY_SOLENOID 26      // Relay channel 1 → solenoid air valves
#define RELAY_PUMP 27          // Relay channel 2 → air pump
#define SERVO_1_PIN 18
#define SERVO_2_PIN 19
#define POT_PIN 35             // Potentiometer (for setting reps)
#define I2C_SDA 21
#define I2C_SCL 22

// -------------------- GLOBAL OBJECTS --------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);  // LCD address might be 0x3F or 0x27
Servo servo1, servo2;

// -------------------- VARIABLES --------------------
bool running = false;
int mode = 1;
int setReps = 10;
int remainingReps = 0;
unsigned long lastActionTime = 0;
bool solenoidState = false;
bool servoPhase = false;

// -------------------- DEBOUNCE --------------------
unsigned long lastDebounceTime_mode = 0;
unsigned long lastDebounceTime_start = 0;
const unsigned long debounceDelay = 250;  // ms
bool lastButtonState_mode = LOW;
bool lastButtonState_start = LOW;

// -------------------- TIMERS --------------------
unsigned long lastToggleTime = 0;
unsigned long servoMoveStart = 0;
bool servoMoving = false;

// -------------------- FUNCTION DECLARATIONS --------------------
void updateLCD();
void mode1_process();
void mode2_process();
void resetRelaysAndServos();

void setup() {
  Serial.begin(115200);

  // Pins
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(RELAY_SOLENOID, OUTPUT);
  pinMode(RELAY_PUMP, OUTPUT);
  digitalWrite(RELAY_SOLENOID, LOW); // relay off (active LOW)
  digitalWrite(RELAY_PUMP, LOW);     // relay off (active LOW)

  // I2C & LCD
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();

  // Servos
  servo1.attach(SERVO_1_PIN);
  servo2.attach(SERVO_2_PIN);
  servo1.write(90);
  servo2.write(90);

  updateLCD();
}

void loop() {
  // ---------- BUTTON HANDLING ----------
  bool currentModeState = digitalRead(BTN_MODE);
  bool currentStartState = digitalRead(BTN_START);

  // MODE button
  if (currentModeState == LOW && lastButtonState_mode == HIGH && (millis() - lastDebounceTime_mode > debounceDelay)) {
    lastDebounceTime_mode = millis();
    mode++;
    if (mode > 3) mode = 1;
    Serial.print("Mode changed to: ");
    Serial.println(mode);
    resetRelaysAndServos();
    running = false;
    updateLCD();
  }
  lastButtonState_mode = currentModeState;

  // START/STOP button
  if (currentStartState == LOW && lastButtonState_start == HIGH && (millis() - lastDebounceTime_start > debounceDelay)) {
    lastDebounceTime_start = millis();
    running = !running;
    Serial.print("Running: ");
    Serial.println(running);
    if (running) {
      remainingReps = setReps;
      lastToggleTime = millis();
      servoMoving = false;
    } else {
      resetRelaysAndServos();
    }
    updateLCD();
  }
  lastButtonState_start = currentStartState;

  // ---------- LOGIC ----------
  if (!running) {
    // Adjust set reps with potentiometer
    int potValue = analogRead(POT_PIN);
    setReps = map(potValue, 0, 4095, 5, 50);
  }

  // Execute active mode logic if running
  if (running) {
    if (mode == 1) mode1_process();
    else if (mode == 2) mode2_process();
  }

  updateLCD();
  delay(100);
}

// ------------------------------------------------------------
// MODE 1: Air Pump + Solenoid Toggle
// ------------------------------------------------------------
void mode1_process() {
  digitalWrite(RELAY_PUMP, LOW);  // Pump ON (active LOW)
  unsigned long currentTime = millis();

  if (currentTime - lastToggleTime >= 5000) {  // Every 5 seconds toggle solenoid
    solenoidState = !solenoidState;
    digitalWrite(RELAY_SOLENOID, solenoidState ? LOW : HIGH); // Active LOW
    lastToggleTime = currentTime;

    // When solenoid turns ON -> count a rep (10 sec cycle = 1 rep)
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
// MODE 2: Servo Alternating Motion
// ------------------------------------------------------------
void mode2_process() {
  unsigned long currentTime = millis();

  if (!servoMoving) {
    servoMoving = true;
    servoPhase = !servoPhase;

    if (servoPhase) {
      // Move servo1 CW, servo2 CCW
      for (int i = 90; i <= 135; i++) {
        servo1.write(i);
        servo2.write(180 - i);  // 90->45
        delay(25);
      }
    } else {
      // Move servo1 CCW, servo2 CW
      for (int i = 90; i >= 45; i--) {
        servo1.write(i);
        servo2.write(180 - i);  // 90->135
        delay(25);
      }
    }

    delay(2000);  // pause at extreme
    // Return to 90
    servo1.write(90);
    servo2.write(90);
    delay(2000);

    remainingReps--;
    Serial.print("Remaining Reps: ");
    Serial.println(remainingReps);
    if (remainingReps <= 0) {
      running = false;
      resetRelaysAndServos();
      updateLCD();
    }
    servoMoving = false;
  }
}

// ------------------------------------------------------------
// RESET RELAYS & SERVOS
// ------------------------------------------------------------
void resetRelaysAndServos() {
  digitalWrite(RELAY_SOLENOID, HIGH);
  digitalWrite(RELAY_PUMP, HIGH);
  servo1.write(90);
  servo2.write(90);
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