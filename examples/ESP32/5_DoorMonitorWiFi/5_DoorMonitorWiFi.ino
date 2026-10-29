// 5_DoorMonitorWiFi — a message on your phone when a door opens.
//
// Same door logic as 3_DoorMonitor: fix the board flat on the door, away
// from the hinges, and start with the door closed. When the door opens
// beyond OPEN_DEG a message goes to your ntfy topic; when it closes, a
// second message says for how long it was open.
//
// Run 5_WiFiSetup first. Install the ntfy app and subscribe to your topic.
// Only monitor doors you are allowed to monitor.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
// Board: any ESP32 board with Wi-Fi (tested by compiling for the ESP-WROVER-KIT).
#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <MPU6050Classroom.h>
#include <ClassroomWiFi.h>

// ---- Pins: look these up in your board's pin diagram ----
// ESP-WROVER-KIT: SDA = GPIO21, SCL = GPIO22
const int SENSOR_SDA = 21;
const int SENSOR_SCL = 22;
const float OPEN_DEG = 10.0;

MPU6050Classroom mpu;
ClassroomWiFi wifi;
TeachMath::ThresholdSwitch door;
float closedYaw = 0;
bool  haveZero = false;
uint32_t openedAt = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);
  if (!wifi.begin(Serial)) return;
  door.onAbove = OPEN_DEG;
  door.offBelow = OPEN_DEG / 2;
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Door monitor ready. The door must be closed now."));
}

void loop() {
  wifi.keepConnected();
  if (!mpu.update()) return;
  if (!haveZero) { closedYaw = mpu.yaw(); haveZero = true; }

  float angle = fabs(TeachMath::angleDiff(mpu.yaw(), closedYaw));
  if (door.update(angle)) {
    if (door.on) {
      openedAt = millis();
      Serial.println(F("Door OPENED"));
      wifi.sendNtfy(wifi.topic(), "Door", "The door was opened.");
    } else {
      char text[48];
      snprintf(text, sizeof text, "The door was closed after %lu s.", (unsigned long)((millis() - openedAt) / 1000));
      Serial.println(text);
      wifi.sendNtfy(wifi.topic(), "Door", text);
    }
  }
  if (!door.on) closedYaw += TeachMath::angleDiff(mpu.yaw(), closedYaw) * 0.001f;   // follow slow drift
}
