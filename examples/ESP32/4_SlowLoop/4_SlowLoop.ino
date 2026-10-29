// 4_SlowLoop — what happens to motion data when loop() is slow?
//
// The sensor makes a new packet of motion data about every 5 ms and keeps
// it in a small memory until the board reads it. This sketch runs three
// stages, 6 s each; in each, loop() spends a different time on "other
// work" (a delay() that stands for servos, a screen or Wi-Fi):
//   stage 1: 0 ms    stage 2: 50 ms    stage 3: 300 ms
// Once per second it prints:
//   made     = packets the sensor made (counted with the INT pin)
//   read     = packets this sketch read
//   lost     = made - read
//   overflow = times the sensor's memory filled up and was emptied
//
// Questions while you watch:
//   - In which stage does "lost" grow, and why?
//   - Without the INT pin, can the sketch tell how many packets it missed?
//   - Does the INT pin stop the losses, or only show them?
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL,
//         INT -> INT_PIN (an interrupt-capable pin on your board).
#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: look these up in your board's pin diagram ----
// ESP-WROVER-KIT: SDA = GPIO21, SCL = GPIO22
const int SENSOR_SDA = 21;
const int SENSOR_SCL = 22;
const int INT_PIN = 27  /* free when no camera is fitted */;

const uint32_t STAGE_MS = 6000;
const uint32_t WORK_MS[3] = {0, 50, 300};

MPU6050Classroom mpu;
uint32_t stageStart = 0, lastReport = 0, lastMade = 0, lastRead = 0, lastOverflow = 0;
int stage = -1;

void printColumn(uint32_t v) {             // right-aligned in 7 characters
  char b[12]; snprintf(b, sizeof b, "%7lu", (unsigned long)v); Serial.print(b);
}

void setup() {
  Serial.begin(115200);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  if (MPU6050Classroom::askYesNo(Serial, "Use the INT pin to count every packet the sensor makes?"))
    mpu.useInterruptPin(INT_PIN);            // prints whether the INT wire works
  stageStart = lastReport = millis();
}

void loop() {
  int s = ((millis() - stageStart) / STAGE_MS) % 3;
  if (s != stage) {
    stage = s;
    Serial.println();
    Serial.print(F("=== Stage ")); Serial.print(stage + 1);
    Serial.print(F(": loop() does ")); Serial.print(WORK_MS[stage]);
    Serial.println(F(" ms of other work per pass ==="));
    Serial.println(F("   made   read   lost  overflow"));
  }

  mpu.update();                                    // one read per pass of loop()
  if (WORK_MS[stage] > 0) delay(WORK_MS[stage]);   // pretend: servos, screen, Wi-Fi

  if (millis() - lastReport >= 1000) {
    lastReport = millis();
    uint32_t made = mpu.interruptCount(), rd = mpu.packetsRead(), over = mpu.fifoOverflows();
    uint32_t dMade = made - lastMade, dRead = rd - lastRead, dOver = over - lastOverflow;
    lastMade = made; lastRead = rd; lastOverflow = over;
    if (mpu.interruptActive()) {
      printColumn(dMade); printColumn(dRead); printColumn(dMade > dRead ? dMade - dRead : 0);
      printColumn(dOver); Serial.println();
    } else {
      Serial.print(F("      ?")); printColumn(dRead); Serial.print(F("      ?")); printColumn(dOver);
      Serial.println(F("   (no INT pin: made and lost unknown)"));
    }
  }
}
