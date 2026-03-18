#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <MPU6050.h>
#include <math.h>

// Network and Firebase credentials
#define WIFI_SSID "Robofunlab_2.4"
#define WIFI_PASSWORD "#$Rfl25*"

#define Web_API_KEY "AIzaSyCDrxz5PiTwP7i3RarEHaf3BT907w00pkc"
#define DATABASE_URL "https://hand-paralysis-project-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "rfl.virajdhruve@gmail.com"
#define USER_PASS "Pass@123"

// User function
void processData(AsyncResult &aResult);

// Authentication
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);

// Firebase components
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// Timer variables for sending data
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 00; // 5Hz

// MPU6050 object
MPU6050 mpu;

// Offsets for calibration
float baseRoll = 0, basePitch = 0, baseYaw = 0;

// Yaw integration variables
float yaw = 0;
unsigned long lastTime = 0;

void setup(){
  Serial.begin(115200);

  // Initialize I2C
  Wire.begin();
  mpu.initialize();

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while (1);
  }
  Serial.println("MPU6050 connected successfully");

  // Calibrate baseline orientation
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  baseRoll = atan2(ay, az) * 180 / PI;
  basePitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180 / PI;
  baseYaw = 0; // Start yaw at 0
  lastTime = millis();

  Serial.printf("Calibration done. BaseRoll: %.2f, BasePitch: %.2f, BaseYaw: %.2f\n",
                baseRoll, basePitch, baseYaw);

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  
  // Configure SSL client
  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);
  
  // Initialize Firebase
  initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
}

void loop(){
  app.loop();
  if (app.ready()){ 
    unsigned long currentTime = millis();
    if (currentTime - lastSendTime >= sendInterval){
      lastSendTime = currentTime;

      // Read IMU values
      int16_t ax, ay, az;
      int16_t gx, gy, gz;
      mpu.getAcceleration(&ax, &ay, &az);
      mpu.getRotation(&gx, &gy, &gz);

      // Convert to angles
      float roll  = atan2(ay, az) * 180 / PI - baseRoll;
      float pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 180 / PI - basePitch;

      // --- Yaw using gyro integration ---
      unsigned long dt = currentTime - lastTime;
      lastTime = currentTime;
      float gyroZdeg = gz / 131.0; // MPU6050 scale factor for ±250°/s
      yaw += gyroZdeg * (dt / 1000.0); // integrate
      if (yaw > 180) yaw -= 360;
      if (yaw < -180) yaw += 360;

      // Upload to Firebase
      Database.set<float>(aClient, "/IMU/roll", roll, processData, "Roll");
      Database.set<float>(aClient, "/IMU/pitch", pitch, processData, "Pitch");
      Database.set<float>(aClient, "/IMU/yaw", yaw, processData, "Yaw");

      Serial.printf("Uploaded -> Roll: %.2f, Pitch: %.2f, Yaw: %.2f\n",
                    roll, pitch, yaw);
    }
  }
}

void processData(AsyncResult &aResult) {
  if (!aResult.isResult())
    return;

  if (aResult.isEvent())
    Firebase.printf("Event task: %s, msg: %s, code: %d\n",
                    aResult.uid().c_str(),
                    aResult.eventLog().message().c_str(),
                    aResult.eventLog().code());

  if (aResult.isDebug())
    Firebase.printf("Debug task: %s, msg: %s\n",
                    aResult.uid().c_str(),
                    aResult.debug().c_str());

  if (aResult.isError())
    Firebase.printf("Error task: %s, msg: %s, code: %d\n",
                    aResult.uid().c_str(),
                    aResult.error().message().c_str(),
                    aResult.error().code());

  if (aResult.available())
    Firebase.printf("task: %s, payload: %s\n",
                    aResult.uid().c_str(),
                    aResult.c_str());
}