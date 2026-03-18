#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Wire.h>
#include <MPU6050_light.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ---------------- Firebase & Wi-Fi config (from your provided block) ----------------
#define WIFI_SSID "Robofunlab_2.4"
#define WIFI_PASSWORD "#$Rfl25*"

#define Web_API_KEY "AIzaSyCDrxz5PiTwP7i3RarEHaf3BT907w00pkc"
#define DATABASE_URL "https://hand-paralysis-project-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "rfl.virajdhruve@gmail.com"
#define USER_PASS "Pass@123"

// forward-declare callback used by FirebaseClient
void processData(AsyncResult &aResult);

// Firebase objects (matching your Firebase snippet structure)
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);

// ---------------- Your original MPU6050_light IMU + button code (kept exactly) ----------------
MPU6050 mpu(Wire);

unsigned long timer = 0;
int mode = 1;               // Start in Mode 1
const int buttonPin = 0;   // Button connected to GPIO0
bool lastButtonState = HIGH; // because INPUT_PULLUP keeps it HIGH by default
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

void connectWiFi() {
  Serial.print("Connecting to Wi-Fi ");
  Serial.print(WIFI_SSID);
  Serial.print(" ");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    retry++;
    if (retry > 60) { // ~30s
      Serial.println("\nWi-Fi connect failed — restarting...");
      ESP.restart();
    }
  }

  Serial.println("\nWi-Fi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void setupFirebase() {
  Serial.println("Initializing Firebase...");
  // ESP32: accept insecure for now (no cert)
  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);

  // initializeApp with Async client and auth callback style from your snippet
  initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  Serial.println("Firebase initialized");
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  pinMode(buttonPin, INPUT_PULLUP);  // Button with internal pull-up

  // connect Wi-Fi and initialize Firebase (added)
  connectWiFi();
  setupFirebase();

  Serial.println("Initializing MPU6050...");
  byte status = mpu.begin();
  Serial.print("MPU6050 status: ");
  Serial.println(status);

  Serial.println("Calculating offsets, do not move MPU6050...");
  delay(1000);
  mpu.calcGyroOffsets();
  Serial.println("Done!\n");
}

void loop() {
  // Allow Firebase client to process background tasks (added)
  app.loop();

  mpu.update();

  // ----- BUTTON HANDLING -----
  int reading = digitalRead(buttonPin);

  // Debounce logic (kept exactly)
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Detect a press (HIGH → LOW transition)
    if (lastButtonState == LOW) {
      mode = (mode == 1) ? 2 : 1;  // Toggle between 1 and 2
      Serial.print("Mode changed to: ");
      Serial.println(mode);

      // Upload mode change immediately (optional but useful)
      Database.set<int>(aClient, "/IMU/mode", mode, processData, "mode");
    }
  }

  lastButtonState = reading;

  // ----- IMU DATA PRINT & FIREBASE UPLOAD ----- (every 200ms as in original)
  if ((millis() - timer) > 500) {  // every 200ms
    timer = millis();

    if (mode == 1) {
      Serial.print("Y: ");
      int yVal = int(mpu.getAngleY());
      Serial.println(yVal);

      // Upload mode (kept) and active value (only Y in mode 1)
      Database.set<int>(aClient, "/IMU/mode", mode, processData, "mode");
      Database.set<int>(aClient, "/IMU/y", yVal, processData, "y");
    } else if (mode == 2) {
      Serial.print("Z: ");
      int zVal = int(mpu.getAngleZ());
      Serial.println(zVal);

      // Upload mode (kept) and active value (only Z in mode 2)
      Database.set<int>(aClient, "/IMU/mode", mode, processData, "mode");
      Database.set<int>(aClient, "/IMU/z", zVal, processData, "z");
    }
  }
}

// Minimal Firebase callback (keeps structure from your provided code)
void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;

  if (aResult.isEvent()) {
    Firebase.printf("Event task: %s, msg: %s, code: %d\n",
                    aResult.uid().c_str(),
                    aResult.eventLog().message().c_str(),
                    aResult.eventLog().code());
  }

  if (aResult.isDebug()) {
    Firebase.printf("Debug task: %s, msg: %s\n",
                    aResult.uid().c_str(),
                    aResult.debug().c_str());
  }

  if (aResult.isError()) {
    Firebase.printf("Error task: %s, msg: %s, code: %d\n",
                    aResult.uid().c_str(),
                    aResult.error().message().c_str(),
                    aResult.error().code());
  }

  if (aResult.available()) {
    Firebase.printf("task: %s, payload: %s\n",
                    aResult.uid().c_str(),
                    aResult.c_str());
  }
}