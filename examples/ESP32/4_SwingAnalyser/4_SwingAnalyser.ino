// 4_SwingAnalyser — measures the speed of a racket, bat or club swing.
//
// The turning speed comes from the change in orientation between two
// readings (degrees per second). A swing starts above 200 deg/s and ends
// below 50 deg/s; the sketch then reports the peak speed and the duration.
//
// Limit: the gyroscope range is 2000 deg/s. Faster swings are cut off at
// that value, and the result is then too low.
//
// Mounting: tape the board firmly to the handle. Keep the wires short and
// secured, or use a battery. Swing carefully, with nobody close to you.
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
const float START_DPS = 200;
const float END_DPS   = 50;

MPU6050Classroom mpu;
TeachMath::Pose last;
uint32_t lastUs = 0;
bool swinging = false;
float peak = 0;
uint32_t swingStart = 0;

void setup() {
  Serial.begin(115200);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Swing analyser ready. Hold still, then swing."));
}

void loop() {
  if (!mpu.update()) return;
  uint32_t nowUs = micros();
  if (lastUs == 0) { last = mpu.pose(); lastUs = nowUs; return; }

  float dps = TeachMath::angularSpeedDps(last, mpu.pose(), (nowUs - lastUs) / 1e6f);
  last = mpu.pose();
  lastUs = nowUs;

  if (!swinging && dps > START_DPS) { swinging = true; peak = dps; swingStart = millis(); }
  if (swinging) {
    if (dps > peak) peak = dps;
    if (dps < END_DPS) {
      swinging = false;
      Serial.print(F("SWING: peak ")); Serial.print(peak, 0);
      Serial.print(F(" deg/s, lasted ")); Serial.print((millis() - swingStart) / 1000.0, 2);
      Serial.println(peak >= 1900 ? F(" s (near the 2000 deg/s limit)") : F(" s"));
    }
  }
}
