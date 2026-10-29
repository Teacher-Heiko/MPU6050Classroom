// 3_DoorMonitor — notices when a door opens, and how far.
//
// Fix the board flat on the door, close to the edge away from the hinges.
// Start the sketch with the door closed: that position counts as 0 deg.
// The door swings around the hinge, so its angle shows as a change in yaw.
//
// Yaw drifts slowly over time (the MPU-6050 has no compass). While the door
// is closed, the sketch slowly follows that drift, so "closed" stays 0 deg.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: look these up in your board's pin diagram ----
// ESP-WROVER-KIT: SDA = GPIO21, SCL = GPIO22
const int SENSOR_SDA = 21;
const int SENSOR_SCL = 22;
const int   LED_PIN = 2   /* WROVER-KIT RGB LED, green */;
const float OPEN_DEG = 10.0;     // counts as open above this angle

MPU6050Classroom mpu;
TeachMath::ThresholdSwitch door;
float closedYaw = 0;
bool  haveZero = false;
uint32_t openedAt = 0;
float widest = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  door.onAbove = OPEN_DEG;
  door.offBelow = OPEN_DEG / 2;
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Door monitor ready. The door must be closed now."));
}

void loop() {
  if (!mpu.update()) return;
  if (!haveZero) { closedYaw = mpu.yaw(); haveZero = true; }

  float angle = fabs(TeachMath::angleDiff(mpu.yaw(), closedYaw));
  if (door.update(angle)) {
    if (door.on) {
      openedAt = millis(); widest = angle;
      Serial.println(F("Door OPENED"));
    } else {
      Serial.print(F("Door closed after ")); Serial.print((millis() - openedAt) / 1000.0, 1);
      Serial.print(F(" s; widest opening ")); Serial.print(widest, 0); Serial.println(F(" deg"));
    }
  }
  if (door.on) {
    if (angle > widest) widest = angle;
  } else {
    closedYaw += TeachMath::angleDiff(mpu.yaw(), closedYaw) * 0.001f;   // follow slow drift
  }
  digitalWrite(LED_PIN, door.on ? HIGH : LOW);
}
