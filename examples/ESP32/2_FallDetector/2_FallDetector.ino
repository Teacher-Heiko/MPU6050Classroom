// 2_FallDetector — detects that the board was dropped.
//
// A fall has two parts, in this order:
//   1. Free fall: the sensor reads close to 0 g for at least 60 ms.
//   2. Impact: within 1 s, a spike above 1.8 g when it lands.
// A bump or a shake has no free-fall part, so it is not reported.
//
// Try it: tape the board and its wires down, then drop it onto a cushion or
// a bed from about 30 cm. Do not drop it onto a hard floor.
//
// This is a learning project, not a safety device. Do not rely on it to
// protect a person.
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
const int LED_PIN    = 2   /* WROVER-KIT RGB LED, green */;
const int BUZZER_PIN = 26  /* free when no camera is fitted */;
const uint32_t ALARM_MS = 3000;

MPU6050Classroom mpu;
TeachMath::FallDetector fall;    // limits: fall.freeFallBelowG, fall.impactAboveG, ...

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Fall detector ready. Drop the board onto a cushion."));
}

void loop() {
  static uint32_t alarmStart = 0;
  static bool wasFalling = false;

  bool alarmOn = alarmStart && millis() - alarmStart < ALARM_MS;
  digitalWrite(LED_PIN, alarmOn ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, alarmOn ? HIGH : LOW);

  if (!mpu.update()) return;

  if (fall.update(millis(), mpu.gForce())) {
    alarmStart = millis();
    Serial.println(F("FALL DETECTED: free fall, then impact."));
  }
  if (fall.falling() && !wasFalling) Serial.println(F("  free fall started..."));
  wasFalling = fall.falling();
}
