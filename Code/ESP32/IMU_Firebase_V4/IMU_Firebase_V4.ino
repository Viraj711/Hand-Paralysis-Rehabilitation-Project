#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Wire.h>
#include <MPU6050_light.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>  // Added for session storage
#include <FirebaseClient.h>

// ---------------- Firebase & Wi-Fi config ----------------
#define WIFI_SSID "Robofunlab_2.4"
#define WIFI_PASSWORD "#$Rfl25*"

#define Web_API_KEY "AIzaSyCDrxz5PiTwP7i3RarEHaf3BT907w00pkc"
#define DATABASE_URL "https://hand-paralysis-project-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "rfl.virajdhruve@gmail.com"
#define USER_PASS "Pass@123"

// forward-declare callback used by FirebaseClient
void processData(AsyncResult &aResult);

// Firebase objects (same as your code)
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);

// ---------------- MPU6050 & Button ----------------
MPU6050 mpu(Wire);

unsigned long timer = 0;
int mode = 1;                // Start in Mode 1
const int modeButtonPin = 0; // Mode toggle button
const int sessionButtonPin = 33; // Session control button
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

// ---------------- EEPROM Session Management ----------------
int sessionID = 1;
const int EEPROM_SIZE = 4;
unsigned long pressStartTime = 0;
bool isHolding = false;

// ---------------- Wi-Fi Setup ----------------
void connectWiFi() {
  Serial.print("Connecting to Wi-Fi ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    retry++;
    if (retry > 60) {
      Serial.println("\nWi-Fi connect failed — restarting...");
      ESP.restart();
    }
  }

  Serial.println("\nWi-Fi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ---------------- Firebase Setup ----------------
void setupFirebase() {
  Serial.println("Initializing Firebase...");
  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);

  initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  Serial.println("Firebase initialized");
}

// ---------------- EEPROM Functions ----------------
void loadSessionID() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, sessionID);
  if (sessionID <= 0 || sessionID > 100000) {
    sessionID = 1;
    EEPROM.put(0, sessionID);
    EEPROM.commit();
  }
  Serial.print("Session ID loaded: ");
  Serial.println(sessionID);
}

void saveSessionID() {
  EEPROM.put(0, sessionID);
  EEPROM.commit();
  Serial.print("Session ID saved: ");
  Serial.println(sessionID);
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(9600);
  Wire.begin();

  pinMode(modeButtonPin, INPUT_PULLUP);
  pinMode(sessionButtonPin, INPUT_PULLUP);

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

  loadSessionID();
}

// ---------------- Loop ----------------
void loop() {
  app.loop();
  mpu.update();

  // ----- MODE BUTTON -----
  int reading = digitalRead(modeButtonPin);
  if (reading != lastButtonState) lastDebounceTime = millis();

  if ((lastButtonState == LOW) && (millis() - lastDebounceTime) > debounceDelay) {
    mode = (mode == 1) ? 2 : 1;
    Serial.print("Mode changed to: ");
    Serial.println(mode);
    Database.set<int>(aClient, "/IMU/mode", mode, processData, "mode");
  }
  lastButtonState = reading;

  // ----- SESSION BUTTON -----
  int sessionBtnState = digitalRead(sessionButtonPin);
  if (sessionBtnState == LOW) {
    if (!isHolding) {
      isHolding = true;
      pressStartTime = millis();
    } else if (millis() - pressStartTime > 10000) {
      // Held for 10s — reset session
      sessionID = 1;
      saveSessionID();
      Serial.println("Session ID reset to 1 (held 10s)");
      delay(1000);
    }
  } else {
    if (isHolding) {
      unsigned long pressDuration = millis() - pressStartTime;
      if (pressDuration < 10000) {
        sessionID++;
        saveSessionID();
        Serial.print("Session incremented → ");
        Serial.println(sessionID);
      }
      isHolding = false;
    }
  }

  // ----- IMU DATA + FIREBASE UPLOAD -----
  if ((millis() - timer) > 1000) {
    timer = millis();

    if (mode == 1) {
      int yVal = int(mpu.getAngleY());
      Serial.printf("Mode 1 | Session %d | Y: %d\n", sessionID, yVal);

      Database.set<int>(aClient, "/IMU/mode", mode, processData, "mode");
      Database.set<int>(aClient, "/IMU/sessionID", sessionID, processData, "session");
      Database.set<int>(aClient, "/IMU/y", yVal, processData, "y");

    } else if (mode == 2) {
      int zVal = int(mpu.getAngleZ());
      Serial.printf("Mode 2 | Session %d | Z: %d\n", sessionID, zVal);

      Database.set<int>(aClient, "/IMU/mode", mode, processData, "mode");
      Database.set<int>(aClient, "/IMU/sessionID", sessionID, processData, "session");
      Database.set<int>(aClient, "/IMU/z", zVal, processData, "z");
    }
  }
}

// ---------------- Firebase Callback ----------------
void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;

  if (aResult.isError()) {
    Firebase.printf("Error task: %s, msg: %s, code: %d\n",
                    aResult.uid().c_str(),
                    aResult.error().message().c_str(),
                    aResult.error().code());
  }

  // if (aResult.available()) {
  //   Firebase.printf("task: %s, payload: %s\n",
  //                   aResult.uid().c_str(),
  //                   aResult.c_str());
  // }
}