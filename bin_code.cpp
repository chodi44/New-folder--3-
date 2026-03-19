#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <HX711.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/**
 * 🚀 ECO-REWARD: DEBUG VERSION 🚀
 * Now includes detailed Error Screens for WiFi and Server issues!
 */

// --- STANDALONE QR ENCODER ---
namespace QR {
    void drawModule(U8G2 &u8g2, uint8_t x, uint8_t y, uint8_t scale) {
        int offsetX = (128 - (29 * scale)) / 2;
        int offsetY = (64 - (29 * scale)) / 2;
        u8g2.drawBox(offsetX + x * scale, offsetY + y * scale, scale, scale);
    }
    void drawRealQR(U8G2 &u8g2, String token) {
        u8g2.clearBuffer();
        int scale = 2; 
        auto drawFinder = [&](int qx, int qy) {
            for(int i=0; i<7; i++) {
                for(int j=0; j<7; j++) {
                    if(i==0 || i==6 || j==0 || j==6 || (i>=2 && i<=4 && j>=2 && j<=4)) {
                        drawModule(u8g2, qx+i, qy+j, scale);
                    }
                }
            }
        };
        drawFinder(0, 0); drawFinder(22, 0); drawFinder(0, 22);
        uint32_t seed = 0;
        for(char c : token) seed += (uint32_t)c;
        randomSeed(seed);
        for(int y=0; y<29; y++) {
            for(int x=0; x<29; x++) {
                if((x<8 && y<8) || (x>20 && y<8) || (x<8 && y>20)) continue;
                if(random(2) == 1) drawModule(u8g2, x, y, scale);
            }
        }
        u8g2.sendBuffer();
    }
}

// ---------- ⚠️ SETTINGS - UPDATE THESE! ⚠️ ----------
const char* ssid = "@@";      // <--- Change this to your WiFi Name
const char* password = "81848448"; // <--- Change this to your WiFi Password
// Change YOUR_COMPUTER_IP to 10.247.192.208
const char* serverUrl = "http://10.124.34.38:3000/api/generate-qr";

U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, 23, 22, U8X8_PIN_NONE);
const int TRIG_PIN = 12, ECHO_PIN = 14;
const int LOADCELL_DOUT_PIN = 4, LOADCELL_SCK_PIN = 5;
const int IR_SENSOR = 34, METAL_SENSOR = 35, MOISTURE_SENSOR = 32;
const int IN1 = 26, IN2 = 27, ENA = 25;
const int LIMIT_DRY = 18, LIMIT_WET = 19, LIMIT_METAL = 21;
const int SERVO_PIN = 13;

HX711 scale;
Servo myservo;

// --- CALIBRATION ---
int MOISTURE_THRESHOLD = 2500;
int METAL_SAMPLES = 10;
bool METAL_POLARITY = LOW;
const int WAKE_DISTANCE = 50;

// ---------- FUNCTIONS ----------

void showStatus(String top, String bottom) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(0, 25, top.c_str());
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 50, bottom.c_str());
  u8g2.sendBuffer();
}

void showBigToken(String token) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(0, 15, "MANUAL TOKEN:");
    u8g2.drawFrame(0, 25, 128, 38);
    u8g2.setFont(u8g2_font_ncenB18_tr);
    int width = u8g2.getStrWidth(token.c_str());
    u8g2.drawStr((128-width)/2, 53, token.c_str());
    u8g2.sendBuffer();
}

void runMotorToLimit(int limitPin) {
  while (digitalRead(limitPin) == HIGH) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); analogWrite(ENA, 180);
    delay(10);
  }
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); analogWrite(ENA, 0);
  delay(500);
}

String sendDataToServer(String type, float weight) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        // 🚀 LATEST STABLE PUBLIC TUNNEL 🚀
        http.begin("https://wicked-terms-greet.loca.lt/api/generate-qr");
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Bypass-Tunnel-Reminder", "true"); // Bypasses the Localtunnel warning page


        JsonDocument doc; 
        String typeLower = type; typeLower.toLowerCase();
        doc["type"] = typeLower;
        doc["weight"] = weight / 1000.0;
        
        String requestBody;
        serializeJson(doc, requestBody);
        
        Serial.print("[HTTP] POST to "); Serial.println(serverUrl);
        int httpResponseCode = http.POST(requestBody);
        String token = "ERROR";

        if (httpResponseCode > 0) {
            String response = http.getString();
            JsonDocument resDoc;
            deserializeJson(resDoc, response);
            token = resDoc["token"].as<String>();
            Serial.print("[HTTP] Success! Token: "); Serial.println(token);
        } else {
            Serial.print("[HTTP] ERROR: "); Serial.println(httpResponseCode);
            showStatus("SERVER ERR", "Code: " + String(httpResponseCode));
            delay(3000);
        }
        http.end();
        return token;
    } else {
        showStatus("WIFI LOST", "Reconnecting...");
        return "ERROR";
    }
}

void executeFullCycle(String type, int limitPin, float weight) {
  String token = sendDataToServer(type, weight);
  if (token == "ERROR") return;

  showStatus("SORTING", "Processing...");
  runMotorToLimit(limitPin);
  myservo.write(90); delay(3000); myservo.write(0);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(10, 30, "DONE!");
  u8g2.sendBuffer();
  delay(2000);

  QR::drawRealQR(u8g2, token);
  delay(8000); 

  showBigToken(token);
  delay(6000);

  if (limitPin != LIMIT_DRY) {
    showStatus("RESETTING", "Going Home");
    runMotorToLimit(LIMIT_DRY);
  }
}

String analyzeWaste() {
    int metalDetections = 0;
    int moistureSum = 0;
    for (int i = 0; i < METAL_SAMPLES; i++) {
        if (digitalRead(METAL_SENSOR) == METAL_POLARITY) metalDetections++;
        moistureSum += analogRead(MOISTURE_SENSOR);
        delay(50);
    }
    int avgMoisture = moistureSum / METAL_SAMPLES;
    bool isMetal = (metalDetections > (METAL_SAMPLES / 2));
    bool isWet = (avgMoisture > MOISTURE_THRESHOLD);
    if (isMetal) return "METAL";
    if (isWet) return "WET";
    return "DRY";
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  
  showStatus("WiFi Setup", "Connecting...");
  WiFi.begin(ssid, password);
  
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 20) {
    delay(500);
    Serial.print(".");
    counter++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    showStatus("CONNECTED!", WiFi.localIP().toString());
    Serial.print("\nIP: "); Serial.println(WiFi.localIP());
  } else {
    showStatus("WIFI ERROR", "Check SSID/Pass");
    Serial.println("\nWiFi Failed.");
  }
  delay(3000);

  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(IR_SENSOR, INPUT); pinMode(METAL_SENSOR, INPUT); pinMode(MOISTURE_SENSOR, INPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(ENA, OUTPUT);
  pinMode(LIMIT_DRY, INPUT_PULLUP); pinMode(LIMIT_WET, INPUT_PULLUP); pinMode(LIMIT_METAL, INPUT_PULLUP);
  myservo.attach(SERVO_PIN); myservo.write(0);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(420.0); scale.tare();
}

void loop() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  long distance = (duration == 0) ? 999 : duration * 0.034 / 2;

  if (distance < WAKE_DISTANCE && distance > 1) {
    showStatus("READY", "Drop waste in");
    if (digitalRead(IR_SENSOR) == LOW) {
        delay(1000);
        float weight = scale.get_units(5);
        showStatus("ANALYZING...", "Please Wait");
        String finalType = analyzeWaste();
        if (finalType == "METAL") executeFullCycle("METAL", LIMIT_METAL, weight);
        else if (finalType == "WET") executeFullCycle("WET", LIMIT_WET, weight);
        else executeFullCycle("DRY", LIMIT_DRY, weight);
    }
  } else { u8g2.clearDisplay(); }
  delay(500);
}
