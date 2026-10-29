// 4_EarthquakeModel — measures shaking on a model building.
//
// Fix the board to the top floor of your model and shake the base (by hand,
// or with a shaking table). The sketch prints two values 20 times a second
// in the format of the Arduino IDE's Serial Plotter
// (Tools > Serial Plotter):
//   shock  — each reading's distance from rest, in g
//   level  — the smoothed shaking strength, in g
// Every 10 s it also prints the strongest level seen.
//
// Compare designs: same shaking, different buildings. Which one shakes
// less at the top?
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

MPU6050Classroom mpu;
TeachMath::ShakeLevel level;
float peakLevel = 0, peakShock = 0;

void setup() {
  Serial.begin(115200);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
}

void loop() {
  if (!mpu.update()) return;
  float s = mpu.shock();
  float l = level.update(s);
  if (s > peakShock) peakShock = s;
  if (l > peakLevel) peakLevel = l;

  static uint32_t lastPlot = 0, lastPeak = 0;
  if (millis() - lastPlot >= 50) {
    lastPlot = millis();
    Serial.print(F("shock:")); Serial.print(peakShock, 3);   // biggest since the last line
    Serial.print(F(",level:")); Serial.println(l, 3);
    peakShock = 0;
  }
  if (millis() - lastPeak >= 10000) {
    lastPeak = millis();
    Serial.print(F("strongest_level_10s:")); Serial.println(peakLevel, 3);
    peakLevel = 0;
  }
}
