// 2_CrashDetect — notice a sudden movement: a hit, a crash, or a fall.
//
// At rest the sensor feels only gravity: 1 g. mpu.shock() is how far each
// reading is from rest, in g: close to 0 while still, larger during a hit.
// A fall counts too: in free fall the sensor reads 0 g, 1 g away from rest.
//
// Limit: the accelerometer range is +/-2 g per axis, so the sketch can
// DETECT a hard crash but cannot MEASURE how hard it was.
//
// Try it: tap the table, slap it, drop the board onto a cushion. Change
// SHOCK_LIMIT_G until only the events you want are reported.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
#if defined(ESP32)
#error "This example is for Arduino UNO R3/R4. Open the same example in the ESP32 folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;
const int   LED_PIN = LED_BUILTIN;
const float SHOCK_LIMIT_G = 0.8;     // how big a jolt must be, in g

MPU6050Classroom mpu;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.print(F("A shock above ")); Serial.print(SHOCK_LIMIT_G, 1); Serial.println(F(" g is reported."));
}

void loop() {
  static uint32_t lastEvent = 0, lastReport = 0;
  static float biggest = 0;

  if (millis() - lastEvent > 1000) digitalWrite(LED_PIN, LOW);
  if (!mpu.update()) return;

  float shock = mpu.shock();
  if (shock > biggest) biggest = shock;

  if (shock > SHOCK_LIMIT_G && millis() - lastEvent > 1000) {   // then ignore bounces for 1 s
    lastEvent = millis();
    digitalWrite(LED_PIN, HIGH);
    Serial.print(F("SUDDEN MOVEMENT! shock = ")); Serial.print(shock, 2); Serial.println(F(" g"));
  }

  if (millis() - lastReport >= 2000) {          // helps you choose a good limit
    lastReport = millis();
    Serial.print(F("  biggest shock in the last 2 s: ")); Serial.print(biggest, 2); Serial.println(F(" g"));
    biggest = 0;
  }
}
