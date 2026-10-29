// BackgroundTask — ADVANCED. Read the sensor in a separate task, so a slow
// loop() no longer loses motion data.
//
// The ESP32 runs several tasks at once (FreeRTOS). Here a small "sensor
// task" sleeps until the INT pin signals a new packet, reads it, and stores
// the newest pose. loop() is deliberately slow (300 ms of other work per
// pass, as in stage 3 of the 4_SlowLoop example) and only copies the newest
// pose when it needs one.
//
// Compare with SlowLoop stage 3: look at the "lost" column here.
//
// Rules this sketch follows (break them and you get random crashes):
//   - Only the sensor task talks to the sensor after setup(). loop() reads
//     counters and the copied pose only.
//   - The shared pose is copied inside a lock (portENTER_CRITICAL), so loop()
//     never sees a half-written pose.
//
// Without a working INT wire, the sensor task checks every 5 ms instead.
// That also works, but costs more processor time and cannot count losses.
//
// Wiring: VCC 3V3, GND GND, SDA GPIO21, SCL GPIO22, INT GPIO27.
#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <Wire.h>
#include <MPU6050Classroom.h>

const int SENSOR_SDA = 21;
const int SENSOR_SCL = 22;
const int INT_PIN = 27;
const DmpVersion FIRMWARE = DmpVersion::V612;
const uint32_t SLOW_WORK_MS = 300;

MPU6050Classroom mpu;

// Shared between the two tasks
portMUX_TYPE poseLock = portMUX_INITIALIZER_UNLOCKED;
TeachMath::Pose sharedPose;
bool havePose = false;

void sensorTask(void*) {
  for (;;) {
    mpu.waitForInterrupt(50);          // sleeps until a packet is ready
    TeachMath::Pose p;
    if (mpu.getPose(p)) {              // freeze repair also runs here
      portENTER_CRITICAL(&poseLock);
      sharedPose = p;
      havePose = true;
      portEXIT_CRITICAL(&poseLock);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Wire.begin(SENSOR_SDA, SENSOR_SCL);
  Wire.setClock(400000);
  mpu.reportTo(Serial);

  MpuError e = mpu.begin(FIRMWARE, SENSOR_SDA, SENSOR_SCL);
  if (e != MPU_OK) {
    Serial.print("Sensor problem: "); Serial.println(MPU6050Classroom::errorName(e));
    Serial.println("Check VCC, GND, SDA, SCL wires, then press the reset button.");
    while (true) delay(1000);
  }
  Serial.println("Calibrating - keep the board flat and still...");
  mpu.calibrate();
  mpu.start();

  if (MPU6050Classroom::askYesNo(Serial, "Use the INT pin (GPIO27) to wake the sensor task?", 10000, true))
    mpu.useInterruptPin(INT_PIN);

  // Priority 2 is above loop() (priority 1), so the sensor task runs as
  // soon as a packet is ready, even while loop() is busy. Same core as loop().
  xTaskCreatePinnedToCore(sensorTask, "sensor", 4096, nullptr, 2, nullptr, ARDUINO_RUNNING_CORE);
  Serial.println("Sensor task started. loop() now does 300 ms of other work per pass.");
  Serial.println("  made   read   lost   |  yaw / pitch / roll (deg)");
}

void loop() {
  delay(SLOW_WORK_MS);                 // pretend: servos, screen, Wi-Fi

  TeachMath::Pose p;
  bool ok;
  portENTER_CRITICAL(&poseLock);
  p = sharedPose; ok = havePose;
  portEXIT_CRITICAL(&poseLock);

  static uint32_t lastReport = 0, lastMade = 0, lastRead = 0;
  if (millis() - lastReport < 1000) return;
  lastReport = millis();
  uint32_t made = mpu.interruptCount(), read = mpu.packetsRead();
  uint32_t dMade = made - lastMade, dRead = read - lastRead;
  lastMade = made; lastRead = read;

  char line[96];
  if (mpu.interruptActive()) {
    uint32_t lost = dMade > dRead ? dMade - dRead : 0;
    snprintf(line, sizeof line, "  %4lu   %4lu   %4lu   |  ",
             (unsigned long)dMade, (unsigned long)dRead, (unsigned long)lost);
  } else {
    snprintf(line, sizeof line, "     ?   %4lu      ?   |  ", (unsigned long)dRead);
  }
  Serial.print(line);
  if (!ok) { Serial.println("no data yet"); return; }
  Serial.print(p.yawDeg, 1);   Serial.print(" / ");
  Serial.print(p.pitchDeg, 1); Serial.print(" / ");
  Serial.println(p.rollDeg, 1);
}
