// 2_TiltAlarm — beeps when a box, bag or bin is tipped over.
//
// The sketch measures how far the board is tilted from flat (0 deg = flat,
// 90 deg = on its side). Above ALARM_DEG the buzzer and LED turn on; they
// turn off again below ALARM_DEG - 5, so the alarm does not flicker.
// Fix the board flat inside the box before you start the sketch.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
// Optional: an active buzzer between BUZZER_PIN and GND.
#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: look these up in your board's pin diagram ----
// ESP-WROVER-KIT: SDA = GPIO21, SCL = GPIO22
const int SENSOR_SDA = 21;
const int SENSOR_SCL = 22;
const int   LED_PIN    = 2   /* WROVER-KIT RGB LED, green */;
const int   BUZZER_PIN = 26  /* free when no camera is fitted */;
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
