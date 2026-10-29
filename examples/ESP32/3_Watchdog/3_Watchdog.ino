// Watchdog — for projects that must keep running with nobody at the
// computer, such as a laser turret or a robot.
//
// Two layers of protection:
//   1. Automatic freeze repair (built into the library, always on):
//      the library notices a frozen sensor and repairs it by itself.
//   2. Hardware watchdog (this sketch turns it on): if a repair keeps
//      failing, the sketch stops "petting" the watchdog, and the board
//      reboots after 8 s. setup() needs no key press, so the project
//      starts again on its own.
//
// Wiring (3.3 V, GND, SDA, SCL):
//   ESP-WROVER-KIT:  SDA = GPIO21,      SCL = GPIO22
//
// Try it: while it runs, pull out the SDA wire for a few seconds, then put
// it back. Read the messages in the Serial Monitor.
#define DEBUG 0   // 1 = also show the library's full diary

#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <Wire.h>
#include <MPU6050Classroom.h>

const int SENSOR_SDA = 21;  // ESP-WROVER-KIT default SDA
const int SENSOR_SCL = 22;  // ESP-WROVER-KIT default SCL
const DmpVersion FIRMWARE = DmpVersion::V612;

const uint32_t WDT_SECONDS = 8;        // reboot after this long without petting
const uint32_t GIVE_UP_MS  = 10000;    // stop petting after repairs fail this long

MPU6050Classroom mpu;
uint32_t unhealthySince = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);                       // no key press: must restart unattended
  Wire.begin(SENSOR_SDA, SENSOR_SCL);
  Wire.setClock(400000);
  mpu.reportTo(Serial);
#if DEBUG
  mpu.attachDebug(Serial);
#endif
  Serial.println("Watchdog demo starting...");

  // Keep retrying instead of stopping: a loose wire pushed back in later
  // still lets the project start.
  MpuError e;
  while ((e = mpu.begin(FIRMWARE, SENSOR_SDA, SENSOR_SCL)) != MPU_OK) {
    Serial.print("Sensor not ready (");
    Serial.print(MPU6050Classroom::errorName(e));
    Serial.println("). Check the wires. Trying again in 1 s.");
    delay(1000);
    mpu.busClear(SENSOR_SDA, SENSOR_SCL);
  }
  mpu.calibrate();                   // once, board flat and still; saved for repairs
  mpu.start();

  if (MPU6050Classroom::watchdogEnable(WDT_SECONDS))
    Serial.println("Hardware watchdog is ON: the board reboots if the sensor cannot be repaired.");
  else
    Serial.println("Hardware watchdog could NOT be turned on. Freeze repair still works.");
}

void loop() {
  TeachMath::Pose p;
  bool got = mpu.getPose(p);         // freeze repair happens inside here

  // Pet the watchdog while the sensor is healthy, and for a short while
  // after a repair starts failing (to give the repair a few tries).
  if (mpu.healthy()) {
    unhealthySince = 0;
    MPU6050Classroom::watchdogPet();
  } else {
    if (unhealthySince == 0) unhealthySince = millis();
    if (millis() - unhealthySince < GIVE_UP_MS) {
      MPU6050Classroom::watchdogPet();
    } else {
      static bool told = false;
      if (!told) { Serial.println("Repairs keep failing. Rebooting the board soon..."); told = true; }
    }
  }
  delay(1);                          // give the system's idle task time to run

  if (!got) return;
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 500) return;
  lastPrint = millis();
  Serial.print("yaw ");         Serial.print(p.yawDeg, 1);
  Serial.print(" deg  pitch "); Serial.print(p.pitchDeg, 1);
  Serial.print(" deg  roll ");  Serial.print(p.rollDeg, 1);
  Serial.print(" deg  | repairs so far: "); Serial.println(mpu.repairCount());
}
