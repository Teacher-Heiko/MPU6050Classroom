// 1b_SelfTest — checks that the sensor and the library work on your board.
// Reports every check, PASS or FAIL, and how many checks ran. A check that
// could not run counts as FAIL with the reason; it is never skipped silently.
// Keep the board flat and still while it runs.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
#if defined(ESP32)
#error "This example is for Arduino UNO R3/R4. Open the same example in the ESP32 folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;

MPU6050Classroom mpu;
int passed = 0, failed = 0;
const int TOTAL_DEFINED = 8;

void report(const __FlashStringHelper* name, bool ok) {
  if (ok) passed++; else failed++;
  Serial.print(ok ? F("PASS  ") : F("FAIL  "));
  Serial.println(name);
}

void finish() {
  Serial.println();
  Serial.print(F("Coverage: ")); Serial.print(passed + failed);
  Serial.print(F(" of "));       Serial.print(TOTAL_DEFINED);
  Serial.println(F(" defined checks ran."));
  Serial.print(passed); Serial.print(F(" passed, "));
  Serial.print(failed); Serial.println(F(" failed."));
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Wire.begin();                 // UNO: I2C is always on A4/A5
  mpu.reportTo(Serial);
  Serial.println(F("MPU6050Classroom self-test. Keep the board flat and still."));
  Serial.println();

  ChipIdentity id = mpu.identify();
  report(F("1. device answers on I2C"), id.present);
  report(F("2. WHO_AM_I is 0x68 (genuine MPU-6050)"), id.present && id.whoAmI == 0x68);
  if (!id.present) {
    for (int i = 3; i <= TOTAL_DEFINED; i++) report(F("(not run: no device answered)"), false);
    finish();
    return;
  }

  MpuError e = mpu.begin(DmpVersion::V612, SENSOR_SDA, SENSOR_SCL, /*allowClones=*/true);
  report(F("3. firmware v6.12 loads and verifies"), e == MPU_OK);
  e = mpu.begin(DmpVersion::V20, SENSOR_SDA, SENSOR_SCL, true);
  report(F("4. firmware v2.0 loads and verifies"), e == MPU_OK);

  e = mpu.calibrate();
  int32_t a[3] = {0, 0, 0}, g[3] = {0, 0, 0};
  bool readOk = (e == MPU_OK) && mpu.readAveraged(a, g, 100);
  report(F("5. calibration: gyro average below 50 (raw units)"),
         readOk && labs(g[0]) < 50 && labs(g[1]) < 50 && labs(g[2]) < 50);
  report(F("6. accelerometer Z reads 1 g while flat (16384 +/- 2000)"),
         readOk && labs(a[2] - 16384) < 2000);

  mpu.start();
  uint8_t buf[42];
  bool gotPacket = false;
  uint32_t t0 = millis();
  while (millis() - t0 < 2000) { if (mpu.readPacket(buf)) { gotPacket = true; break; } }
  report(F("7. motion data arrives within 2 s"), gotPacket);

  if (gotPacket) {
    int16_t q[4]; TeachMath::parseQuatRaw(buf, q);
    float n = sqrtf((float)q[0]*q[0] + (float)q[1]*q[1] + (float)q[2]*q[2] + (float)q[3]*q[3]) / 16384.0f;
    report(F("8. quaternion length within 10% of 1"), n > 0.9f && n < 1.1f);
  } else {
    report(F("8. quaternion length (not run: no motion data)"), false);
  }
  finish();
}

void loop() {}
