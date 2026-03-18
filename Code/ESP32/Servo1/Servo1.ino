#include <ESP32Servo.h>

// Servo objects
Servo servo1;  
Servo servo2;  
Servo servo3;  

// Servo positions
int pos1 = 90;
int pos2 = 90;
int pos3 = 90;

// Pins
const int servo1Pin = 32;
const int servo2Pin = 27;
const int servo3Pin = 26;
const int startBtnPin = 34;  // external button (not pull-up)
const int modeBtnPin  = 0;   // internal pull-up button

// States
bool running = false;
int mode = 1;  // 1 = servo1+servo2 mode, 2 = servo3 mode

// Timing for button debounce
unsigned long lastDebounceTimeStart = 0;
unsigned long lastDebounceTimeMode = 0;
const unsigned long debounceDelay = 200;

// Variables for non-blocking servo motion
int target1 = 90, target2 = 90, stepDelay = 25;
unsigned long lastStepTime = 0;

int servo3Dir = 1;  // direction for servo3 (1=forward, -1=backward)

// Pause handling
bool pausing = false;
unsigned long pauseStart = 0;
const unsigned long pauseDuration = 2000; // 2 seconds

void setup() {
  servo1.attach(servo1Pin);
  servo2.attach(servo2Pin);
  servo3.attach(servo3Pin);

  servo1.write(90);
  servo2.write(90);
  servo3.write(90);

  pinMode(startBtnPin, INPUT);       // external resistor button
  pinMode(modeBtnPin, INPUT_PULLUP); // internal pull-up

  Serial.begin(9600);
}

// --- Move all servos to neutral before mode change ---
void goNeutral() {
  pos1 = pos2 = pos3 = 90;
  servo1.write(90);
  servo2.write(90);
  servo3.write(90);
  delay(500);  // small delay to settle
}

void loop() {
  // --- Handle Start/Stop button ---
  if (digitalRead(startBtnPin) == HIGH && (millis() - lastDebounceTimeStart > debounceDelay)) {
    running = !running;  // Toggle start/stop
    lastDebounceTimeStart = millis();
    Serial.print("Running: "); Serial.println(running);
  }

  // --- Handle Mode button ---
  if (digitalRead(modeBtnPin) == LOW && (millis() - lastDebounceTimeMode > debounceDelay)) {
    goNeutral();  // return to neutral before switching mode
    mode = (mode == 1) ? 2 : 1;  // Switch mode
    lastDebounceTimeMode = millis();
    Serial.print("Mode: "); Serial.println(mode);
  }

  if (!running) return;  // Do nothing if stopped

  // Handle pause
  if (pausing) {
    if (millis() - pauseStart >= pauseDuration) {
      pausing = false; // end pause
    } else {
      return; // still pausing, do nothing
    }
  }

  // --- Mode 1: Servo1 + Servo2 smooth motion ---
  if (mode == 1) {
    if (millis() - lastStepTime >= stepDelay) {
      lastStepTime = millis();

      // If reached target, switch targets (outwards ↔ inwards)
      if (pos1 == target1 && pos2 == target2) {
        // Start pause when reaching extreme
        pausing = true;
        pauseStart = millis();

        if (target1 == 135 && target2 == 45) { target1 = 90; target2 = 90; }
        else if (target1 == 90 && target2 == 90) { target1 = 45; target2 = 135; }
        else { target1 = 135; target2 = 45; }
      }

      // Step servo1
      if (pos1 < target1) pos1++;
      else if (pos1 > target1) pos1--;

      // Step servo2
      if (pos2 < target2) pos2++;
      else if (pos2 > target2) pos2--;

      servo1.write(pos1);
      servo2.write(pos2);
    }
  }

  // --- Mode 2: Servo3 sweeping ---
  else if (mode == 2) {
    if (millis() - lastStepTime >= stepDelay) {
      lastStepTime = millis();

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
      }
    }
  }
}
