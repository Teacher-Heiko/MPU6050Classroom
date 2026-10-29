// 4_BalancingRobot — ADVANCED. A two-wheeled robot that keeps itself upright.
//
// How it works: a background task wakes on every INT signal (about 200 per
// second), reads the tilt, and a PID controller sets the motor power:
//   tilting forward -> wheels drive forward, under the robot, to catch it.
//
// SAFETY FIRST
//   - First tests: hold the robot, or lift it so the wheels spin freely.
//   - Keep fingers and hair away from the wheels.
//   - The motors stop when the tilt passes FALL_LIMIT_DEG, when sensor data
//     stops for 50 ms, and while a sensor repair is running.
//   - After a stop, hold the robot upright for 1 s to start again.
//
// TUNING (type in the Serial Monitor, then Enter):
//   p 15     set Kp       i 0.5   set Ki       d 0.8   set Kd
//   t 1.5    set the target angle (the robot's balance point, in degrees)
//   ?        show the current values
// Method: set i and d to 0. Raise p until the robot wobbles back and forth,
// then raise d until the wobble calms down. Add a little i last, if the
// robot slowly drifts in one direction.
//
// Mounting: fix the sensor firmly to the robot body. If tilting forward
// does not change pitch, set USE_ROLL to true. If the wheels drive the
// wrong way (the robot falls faster), set ANGLE_SIGN to -1.
//
// Wiring (LCD and camera must NOT be fitted to the WROVER-KIT):
//   sensor: VCC 3V3, GND GND, SDA GPIO21, SCL GPIO22, INT GPIO27
//   DRV8833 motor driver: AIN1 GPIO18, AIN2 GPIO19, BIN1 GPIO23, BIN2 GPIO25
//   motors and driver: separate battery; battery GND to ESP32 GND.
#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: look these up in your board's pin diagram ----
// ESP-WROVER-KIT: SDA = GPIO21, SCL = GPIO22
const int SENSOR_SDA = 21;
const int SENSOR_SCL = 22;
#include <TeachPwm.h>

const int INT_PIN = 27;
const int LEFT_IN1 = 18, LEFT_IN2 = 19, RIGHT_IN1 = 23, RIGHT_IN2 = 25;

const bool  USE_ROLL = false;        // true if the sensor is mounted sideways
const float ANGLE_SIGN = 1.0;        // -1.0 if the wheels drive the wrong way
const float FALL_LIMIT_DEG = 40.0;   // motors off beyond this tilt
const float UPRIGHT_DEG = 3.0;       // "upright" for the restart

MPU6050Classroom mpu;
TeachMath::Pid pid;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;   // guards pid and target

float targetDeg = 0.0;
volatile float shownAngle = 0, shownOutput = 0;
volatile bool  running = false;
volatile uint32_t lastDataMs = 0;

void motors(int speed) {
  TeachPwm::motorWrite(LEFT_IN1, LEFT_IN2, speed);
  TeachPwm::motorWrite(RIGHT_IN1, RIGHT_IN2, speed);
}

void balanceTask(void*) {
  uint32_t lastUs = micros(), uprightSince = 0;
  for (;;) {
    mpu.waitForInterrupt(20);
    if (!mpu.update()) continue;
    uint32_t nowUs = micros();
    float dt = (nowUs - lastUs) / 1e6f;
    lastUs = nowUs;
    lastDataMs = millis();

    float angle = ANGLE_SIGN * (USE_ROLL ? mpu.roll() : mpu.pitch());
    shownAngle = angle;

    if (!running) {                                   // wait until held upright for 1 s
      motors(0);
      if (fabsf(angle) < UPRIGHT_DEG) {
        if (uprightSince == 0) uprightSince = millis();
        if (millis() - uprightSince > 1000) {
          portENTER_CRITICAL(&lock); pid.reset(); portEXIT_CRITICAL(&lock);
          running = true;
        }
      } else {
        uprightSince = 0;
      }
      continue;
    }
    if (fabsf(angle) > FALL_LIMIT_DEG || !mpu.healthy()) {   // fallen over
      running = false; uprightSince = 0;
      motors(0);
      continue;
    }
    portENTER_CRITICAL(&lock);
    float out = pid.step(targetDeg - angle, dt);
    portEXIT_CRITICAL(&lock);
    out = -out;                    // error = target - angle: drive toward the tilt
    shownOutput = out;
    motors((int)out);
  }
}

void showSettings() {
  Serial.printf("Kp %.2f   Ki %.2f   Kd %.2f   target %.1f deg\n", pid.kp, pid.ki, pid.kd, targetDeg);
}

void handleCommand() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;
  char cmd = line.charAt(0);
  float v = line.substring(1).toFloat();
  portENTER_CRITICAL(&lock);
  if (cmd == 'p') pid.kp = v;
  else if (cmd == 'i') { pid.ki = v; pid.integral = 0; }
  else if (cmd == 'd') pid.kd = v;
  else if (cmd == 't') targetDeg = v;
  portEXIT_CRITICAL(&lock);
  showSettings();
}

void setup() {
  Serial.begin(115200);
  TeachPwm::motorAttach(LEFT_IN1, LEFT_IN2);
  TeachPwm::motorAttach(RIGHT_IN1, RIGHT_IN2);
  motors(0);

  pid.kp = 15.0; pid.ki = 0.0; pid.kd = 0.5;       // starting values: tune them
  pid.outMin = -255; pid.outMax = 255;

  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  mpu.useInterruptPin(INT_PIN);    // without it, the task checks every 5 ms

  xTaskCreatePinnedToCore(balanceTask, "balance", 4096, nullptr, 3, nullptr, ARDUINO_RUNNING_CORE);
  Serial.println("Hold the robot upright and still for 1 s to start.");
  showSettings();
}

void loop() {
  handleCommand();

  // Safety net: no fresh data for 50 ms (for example during a sensor
  // repair) -> motors off.
  if (running && millis() - lastDataMs > 50) {
    running = false;
    motors(0);
    Serial.println("No sensor data: motors stopped.");
  }

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint >= 500) {
    lastPrint = millis();
    Serial.printf("%s   angle %+6.1f deg   motor %+4.0f\n",
                  running ? "BALANCING" : "stopped  ", shownAngle, shownOutput);
  }
  delay(5);
}
