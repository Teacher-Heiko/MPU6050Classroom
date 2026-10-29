// 1c_YawPitchRoll — your first motion sketch.
// Prints yaw, pitch and roll in degrees, twice a second.
// Keep the board flat and still while it starts, then tilt it.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
//
// DEBUG 1 also prints the quaternion and the library's full diary.
#define DEBUG 0
#if defined(ESP32)
#error "This example is for Arduino UNO R3/R4. Open the same example in the ESP32 folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;

MPU6050Classroom mpu;

void setup() {
  Serial.begin(115200);
#if DEBUG
  mpu.attachDebug(Serial);
#endif
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);   // finds, loads, calibrates, starts
}

void loop() {
  if (!mpu.update()) return;                 // no new data yet

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 500) return;
  lastPrint = millis();

  Serial.print(F("yaw "));          Serial.print(mpu.yaw(), 1);
  Serial.print(F(" deg   pitch ")); Serial.print(mpu.pitch(), 1);
  Serial.print(F(" deg   roll "));  Serial.print(mpu.roll(), 1);
  Serial.println(F(" deg"));
#if DEBUG
  const TeachMath::Pose& p = mpu.pose();
  Serial.print(F("  quaternion w ")); Serial.print(p.qw, 3);
  Serial.print(F("  x ")); Serial.print(p.qx, 3);
  Serial.print(F("  y ")); Serial.print(p.qy, 3);
  Serial.print(F("  z ")); Serial.println(p.qz, 3);
#endif
}
