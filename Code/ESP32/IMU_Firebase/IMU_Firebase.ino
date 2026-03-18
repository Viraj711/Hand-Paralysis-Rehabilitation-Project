/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete instructions at https://RandomNerdTutorials.com/esp32-firebase-realtime-database/
*********/
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <MPU6050.h>

// Network and Firebase credentials
#define WIFI_SSID "RFLAcademy_23_4G"
#define WIFI_PASSWORD "RFLKandivali2023"

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

// Timer variables for sending data every 10 seconds
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // 0.5 seconds in milliseconds

// MPU6050 object
MPU6050 mpu;

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
  // Maintain authentication and async tasks
  app.loop();
  // Check if authentication is ready
  if (app.ready()){ 
    // Periodic data sending every 10 seconds
    unsigned long currentTime = millis();
    if (currentTime - lastSendTime >= sendInterval){
      // Update the last send time
      lastSendTime = currentTime;

      // Read IMU values
      int16_t ax, ay, az;
      int16_t gx, gy, gz;

      mpu.getAcceleration(&ax, &ay, &az);
      mpu.getRotation(&gx, &gy, &gz);


      // Upload data to Firebase
      Database.set<int>(aClient, "/IMU/accelX", ax, processData, "AccelX");
      Database.set<int>(aClient, "/IMU/accelY", ay, processData, "AccelY");
      Database.set<int>(aClient, "/IMU/accelZ", az, processData, "AccelZ");

      Database.set<int>(aClient, "/IMU/gyroX", gx, processData, "GyroX");
      Database.set<int>(aClient, "/IMU/gyroY", gy, processData, "GyroY");
      Database.set<int>(aClient, "/IMU/gyroZ", gz, processData, "GyroZ");

      Serial.println("Uploaded IMU data to Firebase");
    }
  }
}

void processData(AsyncResult &aResult) {
  if (!aResult.isResult())
    return;

  if (aResult.isEvent())
    Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());

  if (aResult.isDebug())
    Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());

  if (aResult.isError())
    Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());

  if (aResult.available())
    Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
}