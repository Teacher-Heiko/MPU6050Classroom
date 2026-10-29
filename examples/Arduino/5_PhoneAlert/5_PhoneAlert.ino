// 5_PhoneAlert — a fall or a crash sends a message to your phone.
//
// Uses the fall detector (free fall, then impact) and the crash limit
// (a shock above CRASH_LIMIT_G). Each alert goes to the ntfy topic saved
// with 5_WiFiSetup. At most one message every 30 s, so the free ntfy
// server is not flooded.
//
// Run 5_WiFiSetup first. Install the ntfy app and subscribe to your topic.
// This is a learning project, not a safety device. Do not rely on it to
// protect a person.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
// Board: Arduino UNO R4 WiFi.
#if !defined(ARDUINO_UNOR4_WIFI)
#error "This Wi-Fi example needs an Arduino UNO R4 WiFi (the UNO R3 and R4 Minima have no Wi-Fi)."
#endif
#include <MPU6050Classroom.h>
#include <ClassroomWiFi.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;
const float CRASH_LIMIT_G = 0.9;
const uint32_t MIN_GAP_MS = 30000;

MPU6050Classroom mpu;
ClassroomWiFi wifi;
TeachMath::FallDetector fall;
uint32_t lastAlert = 0;

void alert(const char* title, const char* text) {
  Serial.print(F("ALERT: ")); Serial.println(text);
  if (lastAlert && millis() - lastAlert < MIN_GAP_MS) {
    Serial.println(F("  (not sent: less than 30 s since the last message)"));
    return;
  }
  lastAlert = millis();
  wifi.sendNtfy(wifi.topic(), title, text);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  if (!wifi.begin(Serial)) return;
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Phone alert ready."));
}

void loop() {
  wifi.keepConnected();
  if (!mpu.update()) return;

  if (fall.update(millis(), mpu.gForce())) {
    alert("Fall detected", "The board fell: free fall, then impact.");
  } else if (!fall.falling() && mpu.shock() > CRASH_LIMIT_G) {
    char text[48];
    snprintf(text, sizeof text, "Crash: shock %d.%02d g", (int)mpu.shock(), (int)(mpu.shock() * 100) % 100);
    alert("Crash detected", text);
  }
}
