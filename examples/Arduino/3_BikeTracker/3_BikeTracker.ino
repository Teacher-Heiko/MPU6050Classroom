// 3_BikeTracker — jumps and lean angle on a bike, skateboard or scooter.
//
// Jumps: while the wheels are in the air the sensor is in free fall (about
// 0 g). The sketch measures that airtime and reports it on landing.
// Lean: roll angle in turns. Every 5 s it prints the furthest lean to each
// side since the last report.
//
// Mounting: fix the board firmly to the frame or deck, flat, with its
// X axis pointing forward. Start the sketch with the bike upright and still.
// Protect the electronics, and wear a helmet.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
// Needs a battery. Connect to the Serial Monitor afterwards, or keep a
// laptop nearby, to read the results.
#if defined(ESP32)
#error "This example is for Arduino UNO R3/R4. Open the same example in the ESP32 folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;

MPU6050Classroom mpu;
TeachMath::JumpTimer jump;
float leanLeft = 0, leanRight = 0;
uint32_t jumps = 0;

void setup() {
  Serial.begin(115200);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Bike tracker ready."));
}

void loop() {
  if (!mpu.update()) return;

  uint32_t air = jump.update(millis(), mpu.gForce());
  if (air) {
    jumps++;
    Serial.print(F("JUMP ")); Serial.print(jumps);
    Serial.print(F(": airtime ")); Serial.print(air / 1000.0, 2); Serial.println(F(" s"));
  }

  float roll = mpu.roll();
  if (roll < -leanLeft) leanLeft = -roll;
  if (roll > leanRight) leanRight = roll;

  static uint32_t lastReport = 0;
  if (millis() - lastReport >= 5000) {
    lastReport = millis();
    Serial.print(F("furthest lean: left ")); Serial.print(leanLeft, 0);
    Serial.print(F(" deg, right ")); Serial.print(leanRight, 0); Serial.println(F(" deg"));
    leanLeft = leanRight = 0;
  }
}
