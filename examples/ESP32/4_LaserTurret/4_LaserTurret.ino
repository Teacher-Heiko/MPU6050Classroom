// 4_LaserTurret — aim a laser (or a camera) by moving your hand.
//
// Hold the sensor board in your hand. Turning it left and right (yaw)
// moves the pan servo; tilting it up and down (pitch) moves the tilt servo.
// At the start, the direction you hold the board counts as straight ahead.
//
// LASER SAFETY: use only a low-power laser module (Class 2, at most 1 mW).
// Never point it at people, animals, mirrors or windows, and never look into
// the beam. The laser switches off when the sensor stops working or when you
// tilt your hand more than 60 deg down ("safe" position).
//
// Power: servos need a separate 5 V supply, with its GND joined to the
// board's GND.
//
// Wiring: sensor VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
//         pan servo -> PAN_PIN, tilt servo -> TILT_PIN, laser module -> LASER_PIN.
#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <MPU6050Classroom.h>
#include <TeachPwm.h>

// ---- Pins: look these up in your board's pin diagram ----
// ESP-WROVER-KIT: SDA = GPIO21, SCL = GPIO22
const int SENSOR_SDA = 21;
const int SENSOR_SCL = 22;
const int   PAN_PIN = 26, TILT_PIN = 25, LASER_PIN = 27;   // camera and LCD must not be fitted
const float PAN_DIRECTION = 1.0;     // -1.0 if the pan servo turns the wrong way
const float TILT_DIRECTION = 1.0;    // -1.0 if the tilt servo turns the wrong way
const float SAFE_PITCH_DEG = -60;    // hand pointed down this far = laser off

MPU6050Classroom mpu;
float zeroYaw = 0;
bool  haveZero = false;

void setup() {
  Serial.begin(115200);
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);
  TeachPwm::servoAttach(PAN_PIN);
  TeachPwm::servoAttach(TILT_PIN);
  TeachPwm::servoWrite(PAN_PIN, 90);
  TeachPwm::servoWrite(TILT_PIN, 90);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Hold the board pointing straight ahead..."));
}

void loop() {
  if (!mpu.update()) { if (!mpu.healthy()) digitalWrite(LASER_PIN, LOW); return; }
  if (!haveZero) { zeroYaw = mpu.yaw(); haveZero = true; Serial.println(F("Ready.")); }

  float pan  = constrain(90 + PAN_DIRECTION  * TeachMath::angleDiff(mpu.yaw(), zeroYaw), 0.0f, 180.0f);
  float tilt = constrain(90 + TILT_DIRECTION * mpu.pitch(), 0.0f, 180.0f);
  TeachPwm::servoWrite(PAN_PIN, pan);
  TeachPwm::servoWrite(TILT_PIN, tilt);

  bool safe = mpu.pitch() < SAFE_PITCH_DEG || !mpu.healthy();
  digitalWrite(LASER_PIN, safe ? LOW : HIGH);

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 500) return;
  lastPrint = millis();
  Serial.print(F("pan ")); Serial.print(pan, 0); Serial.print(F(" deg   tilt ")); Serial.print(tilt, 0);
  Serial.println(safe ? F(" deg   laser OFF (safe)") : F(" deg   laser ON"));
}
