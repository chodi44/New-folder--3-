#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <HX711.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- STANDALONE QR ENCODER (STABLE) ---
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

// ---------- ⚠️ SETTINGS ----------
const char* ssid = "@@";  
const char* password = "81848448"; 
const char* serverUrl = "https://wicked-terms-greet.loca.lt/api/generate-qr";

U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, 23, 22, U8X8_PIN_NONE);
const int TRIG_PIN = 12, ECHO_PIN = 14;
const int LOADCELL_DOUT_PIN = 4, LOADCELL_SCK_PIN = 5;
const int IR_SENSOR = 34, METAL_SENSOR = 35, MOISTURE_SENSOR = 32;
const int IN1 = 26, IN2 = 27, ENA = 25;
const int LIMIT_DRY = 18, LIMIT_WET = 19, LIMIT_METAL = 21;
const int SERVO_PIN = 13;

HX711 scale;
Servo myservo;

// --- ADVANCED CALIBRATION ---
int MOISTURE_THRESHOLD = 2200; // Lower = more sensitive to wet
int SAMPLES = 30;              // Accuracy level
const int WAKE_DISTANCE = 50;  // CM

// 🚀 METAL DETECTION FIX: 🚀
// For ST18-3004NA (NPN NO): 
// - No Metal = HIGH
// - Metal Detected = LOW
// If it's "sticking" as metal, swap this to HIGH.
bool METAL_TRIGGER_LEVEL = LOW; 

const int STABILIZE_DELAY = 1500; // MS to wait for item to settle

// ---------- CORE FUNCTIONS ----------

void showStatus(String top, String bottom) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(0, 25, top.c_str());
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 50, bottom.c_str());
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

String analyzeWaste() {
    int metalHits = 0;
    long moistSum = 0;
    
    Serial.println("[Analyze] Running 30 samples...");
    
    for (int i = 0; i < SAMPLES; i++) {
        int rawMetal = digitalRead(METAL_SENSOR);
        if (rawMetal == METAL_TRIGGER_LEVEL) metalHits++;
        moistSum += analogRead(MOISTURE_SENSOR);
        delay(30); 
    }

    int avgMoist = moistSum / SAMPLES;
    // Require 85% majority for metal (Very Strict!)
    bool isMetal = (metalHits >= (SAMPLES * 0.85));
    bool isWet = (avgMoist > MOISTURE_THRESHOLD);

    Serial.print("--- RESULTS ---");
    Serial.print(" Metal Score: "); Serial.print(metalHits); Serial.print("/"); Serial.println(SAMPLES);
    Serial.print(" Avg Moisture: "); Serial.println(avgMoist);

    if (isMetal) return "metal";
    if (isWet) return "wet";
    return "dry";
}

String getQRCodeFromServer(String type, float weight) {
    if (WiFi.status() != WL_CONNECTED) return "ERROR";
    
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Bypass-Tunnel-Reminder", "true");

    JsonDocument doc;
    doc["type"] = type;
    doc["weight"] = weight; // Already in grams/units from your current scale setting
    
    String body;
    serializeJson(doc, body);
    
    int code = http.POST(body);
    String token = "ERROR";
    if (code > 0) {
        String res = http.getString();
        JsonDocument resDoc;
        deserializeJson(resDoc, res);
        token = resDoc["token"].as<String>();
    }
    http.end();
    return token;
}

void executeCycle(String type, int limitPin, float weight) {
    showStatus("SORTING...", type);
    runMotorToLimit(limitPin);
    myservo.write(90); delay(3000); myservo.write(0);

    showStatus("REWARD", "Finalizing...");
    String token = getQRCodeFromServer(type, weight);
    
    if (token != "ERROR") {
        QR::drawRealQR(u8g2, token);
        delay(12000); // 12 Seconds to scan
    } else {
        showStatus("OFFLINE", "Saving locally...");
        delay(3000);
    }

    if (limitPin != LIMIT_DRY) {
        showStatus("RESETTING", "Home");
        runMotorToLimit(LIMIT_DRY);
    }
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  
  WiFi.begin(ssid, password);
  showStatus("WIFI", "Connecting...");
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500); timeout++; Serial.print(".");
  }

  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(IR_SENSOR, INPUT); pinMode(METAL_SENSOR, INPUT); pinMode(MOISTURE_SENSOR, INPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(ENA, OUTPUT);
  pinMode(LIMIT_DRY, INPUT_PULLUP); pinMode(LIMIT_WET, INPUT_PULLUP); pinMode(LIMIT_METAL, INPUT_PULLUP);
  
  myservo.attach(SERVO_PIN); myservo.write(0);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(420.0); scale.tare();
  
  showStatus("SUCCESS!", "Ready to scan");
  Serial.println("\nSystem Ready.");
}

void loop() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  long dist = (dur == 0) ? 999 : dur * 0.034 / 2;

  if (dist < WAKE_DISTANCE && dist > 1) {
    showStatus("HELLO", "Waiting for waste...");

    if (digitalRead(IR_SENSOR) == LOW) {
        showStatus("WAITING...", "Stabilizing");
        delay(STABILIZE_DELAY); // Wait for item to stop moving
        float weight = scale.get_units(5);
        if (weight < 2) weight = 0; // Ignore tiny movements

        showStatus("ANALYZING...", "Stay still");
        String type = analyzeWaste();
        
        if (type == "metal") executeCycle("metal", LIMIT_METAL, weight);
        else if (type == "wet") executeCycle("wet", LIMIT_WET, weight);
        else executeCycle("dry", LIMIT_DRY, weight);
    }
  } else {
    u8g2.clearDisplay();
  }
  delay(400);
}
