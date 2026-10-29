// 3_CameraGimbal — a servo keeps a platform level while the board tilts.
//
// Fix the sensor board to the base (the part you tilt) and a small platform
// to the servo horn. When the base rolls by +10 deg, the servo turns by
// -10 deg, so the platform stays level. A hobby servo turns 0..180 deg, so
// this works for rolls between about -90 and +90 deg.
//
// Power: a servo can draw more current than the board supplies, and its
// electrical noise can freeze the sensor. Power the servo from a separate
// 5 V supply and connect that supply's GND to the board's GND.
//
// Wiring: sensor VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
//         servo signal -> SERVO_PIN, servo + -> separate 5 V, servo - -> GND.
#if defined(ESP32)
#error "This example is for Arduino UNO R3/R4. Open the same example in the ESP32 folder."
#endif
#include <MPU6050Classroom.h>
#include <Servo.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;
const int   SERVO_PIN = 9;
const float DIRECTION = -1.0;      // if the platform tilts the wrong way, use +1.0
const float CENTRE_DEG = 90.0;     // servo angle when the base is level
const float SMOOTHING = 0.2;       // 0..1: lower = smoother but slower

MPU6050Classroom mpu;
Servo servo;
float servoDeg = CENTRE_DEG;

void setup() {
  Serial.begin(115200);
  servo.attach(SERVO_PIN);
  servo.write((int)CENTRE_DEG);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Tilt the base sideways; the platform should stay level."));
}

void loop() {
  if (!mpu.update()) return;
  float target = constrain(CENTRE_DEG + DIRECTION * mpu.roll(), 0.0f, 180.0f);
  servoDeg += (target - servoDeg) * SMOOTHING;     // move part of the way each time
  servo.write((int)(servoDeg + 0.5f));

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 500) return;
  lastPrint = millis();
  Serial.print(F("base roll ")); Serial.print(mpu.roll(), 1);
  Serial.print(F(" deg   servo ")); Serial.print(servoDeg, 1);
  Serial.println((target <= 0 || target >= 180) ? F(" deg   (servo at its limit)") : F(" deg"));
}
