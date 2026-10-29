// 2_SpiritLevel — a digital spirit level.
//
// Shows pitch and roll as bars and lights the LED when the board is level
// within LEVEL_LIMIT_DEG:
//   pitch [-----|-*---] +2.1 deg   roll [-----*-----] +0.1 deg   not level
//
// Important: "level" means "the same as during calibration". Start the
// sketch with the board on a surface you know is level (check it once with
// a real spirit level).
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
#if defined(ESP32)
#error "This example is for Arduino UNO R3/R4. Open the same example in the ESP32 folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;
const int   LED_PIN = LED_BUILTIN;
const float LEVEL_LIMIT_DEG = 1.0;   // counts as level within this angle
const float BAR_RANGE_DEG   = 10.0;  // the bar ends show +/-10 degrees

MPU6050Classroom mpu;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Place the board on the surface you want to check."));
}

void loop() {
  if (!mpu.update()) return;

  bool level = fabs(mpu.pitch()) < LEVEL_LIMIT_DEG && fabs(mpu.roll()) < LEVEL_LIMIT_DEG;
  digitalWrite(LED_PIN, level ? HIGH : LOW);

  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 250) return;
  lastPrint = millis();

  char bar[16];
  Serial.print(F("pitch "));
  TeachMath::levelBar(mpu.pitch(), BAR_RANGE_DEG, bar, 11); Serial.print(bar);
  Serial.print(' '); Serial.print(mpu.pitch(), 1);
  Serial.print(F(" deg   roll "));
  TeachMath::levelBar(mpu.roll(), BAR_RANGE_DEG, bar, 11); Serial.print(bar);
  Serial.print(' '); Serial.print(mpu.roll(), 1);
  Serial.println(level ? F(" deg   LEVEL") : F(" deg   not level"));
}
