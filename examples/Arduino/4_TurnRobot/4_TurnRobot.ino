// 4_TurnRobot — a robot that drives a square using exact 90 deg turns.
//
// It drives forward for DRIVE_MS, then turns on the spot until yaw has
// changed by 90 deg, and repeats: a square. Near the target angle the turn
// slows down, so the robot does not overshoot.
//
// Safety: first test with the wheels off the ground. If a turn takes longer
// than 5 s, the robot stops and prints a hint (for example: TURN_SIGN wrong,
// or a motor wired the other way round).
//
// Limit: yaw drifts slowly, so after many squares the heading is a few
// degrees off. Each turn is measured on its own, so a single turn stays
// accurate.
//
// Wiring: sensor VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
//         DRV8833 motor driver: see the pin lines below. Motors and driver
//         on their own battery, with its GND joined to the board's GND.
#if defined(ESP32)
#error "This example is for Arduino UNO R3/R4. Open the same example in the ESP32 folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;
const int LEFT_IN1 = 5, LEFT_IN2 = 6, RIGHT_IN1 = 9, RIGHT_IN2 = 10;      // PWM pins on the UNO

void motorSetup() {
  pinMode(LEFT_IN1, OUTPUT); pinMode(LEFT_IN2, OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT); pinMode(RIGHT_IN2, OUTPUT);
}
// speed -255..255: forward = PWM on in1, backward = PWM on in2.
void motor(int in1, int in2, int speed) {
  speed = constrain(speed, -255, 255);
  if (speed >= 0) { analogWrite(in1, speed); analogWrite(in2, 0); }
  else            { analogWrite(in1, 0);     analogWrite(in2, -speed); }
}

const int   DRIVE_SPEED = 160;       // 0..255
const int   TURN_SPEED  = 140;
const uint32_t DRIVE_MS = 1500;
const float TURN_SIGN = 1.0;         // -1.0 if the robot turns the wrong way

MPU6050Classroom mpu;

void stopMotors() { motor(LEFT_IN1, LEFT_IN2, 0); motor(RIGHT_IN1, RIGHT_IN2, 0); }

// Turn on the spot by +90 deg (right) using yaw. Returns false on timeout.
bool turn90() {
  while (!mpu.update()) {}
  float start = mpu.yaw();
  uint32_t t0 = millis();
  for (;;) {
    if (millis() - t0 > 5000) { stopMotors(); return false; }
    if (!mpu.update()) continue;
    float turned = TURN_SIGN * TeachMath::angleDiff(mpu.yaw(), start);
    float left = 90.0f - turned;                       // degrees still to turn
    if (left <= 1.0f) break;
    int speed = left < 20.0f ? TURN_SPEED / 2 : TURN_SPEED;   // slow down near the end
    motor(LEFT_IN1, LEFT_IN2, speed);
    motor(RIGHT_IN1, RIGHT_IN2, -speed);
  }
  stopMotors();
  return true;
}

void setup() {
  Serial.begin(115200);
  motorSetup();
  stopMotors();
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Turn robot: driving a square."));
}

void loop() {
  motor(LEFT_IN1, LEFT_IN2, DRIVE_SPEED);
  motor(RIGHT_IN1, RIGHT_IN2, DRIVE_SPEED);
  uint32_t t0 = millis();
  while (millis() - t0 < DRIVE_MS) mpu.update();       // keep reading while driving
  stopMotors();
  delay(300);

  if (!turn90()) {
    Serial.println(F("The turn took longer than 5 s. Stopped."));
    Serial.println(F("  Check TURN_SIGN and that both motors turn the right way."));
    for (;;) delay(1000);
  }
  Serial.print(F("turned 90 deg, yaw now ")); Serial.print(mpu.yaw(), 1); Serial.println(F(" deg"));
  delay(300);
}
