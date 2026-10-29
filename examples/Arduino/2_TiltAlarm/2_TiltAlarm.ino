// 2_TiltAlarm — beeps when a box, bag or bin is tipped over.
//
// The sketch measures how far the board is tilted from flat (0 deg = flat,
// 90 deg = on its side). Above ALARM_DEG the buzzer and LED turn on; they
// turn off again below ALARM_DEG - 5, so the alarm does not flicker.
// Fix the board flat inside the box before you start the sketch.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
// Optional: an active buzzer between BUZZER_PIN and GND.
#if defined(ESP32)
#error "This example is for Arduino UNO R3/R4. Open the same example in the ESP32 folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;
const int   LED_PIN    = LED_BUILTIN;
const int   BUZZER_PIN = 8;
const float ALARM_DEG  = 30.0;

MPU6050Classroom mpu;
TeachMath::ThresholdSwitch tiltAlarm;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  tiltAlarm.onAbove = ALARM_DEG;
  tiltAlarm.offBelow = ALARM_DEG - 5;
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Tilt alarm ready."));
}

void loop() {
  if (!mpu.update()) return;

  float tilt = TeachMath::tiltDeg(mpu.pose());
  if (tiltAlarm.update(tilt)) {
    Serial.print(tiltAlarm.on ? F("ALARM: tipped over, tilt ") : F("OK again, tilt "));
    Serial.print(tilt, 0);
    Serial.println(F(" deg"));
  }
  digitalWrite(LED_PIN, tiltAlarm.on ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, tiltAlarm.on ? HIGH : LOW);
}
