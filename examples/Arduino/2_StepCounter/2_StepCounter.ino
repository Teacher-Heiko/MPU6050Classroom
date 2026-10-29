// 2_StepCounter — counts steps while you carry the board.
//
// Each step makes the total acceleration rise above 1.2 g after a dip
// below 1.0 g. Steps closer than 0.25 s are counted once.
//
// Limits: works best with the board held still in the hand or in a
// pocket, on even walking. Running, stairs or a swinging arm give less
// accurate counts. Compare with your own count over 50 steps.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
// For walking you need a battery or a long USB cable.
#if defined(ESP32)
#error "This example is for Arduino UNO R3/R4. Open the same example in the ESP32 folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;

MPU6050Classroom mpu;
TeachMath::StepCounter steps;

void setup() {
  Serial.begin(115200);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Step counter ready. Start walking."));
}

void loop() {
  if (!mpu.update()) return;
  if (steps.update(millis(), mpu.gForce())) {
    Serial.print(F("steps: ")); Serial.println(steps.steps);
  }
}
