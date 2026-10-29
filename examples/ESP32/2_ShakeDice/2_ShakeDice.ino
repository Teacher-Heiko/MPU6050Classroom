// 2_ShakeDice — shake the board to roll a die.
//
// A shake is at least 3 strong jolts close together. When the shaking
// stops, the sketch rolls a number from 1 to 6 and draws the die face.
// One knock is not a shake, so the die does not roll by accident.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: look these up in your board's pin diagram ----
// ESP-WROVER-KIT: SDA = GPIO21, SCL = GPIO22
const int SENSOR_SDA = 21;
const int SENSOR_SCL = 22;

MPU6050Classroom mpu;
TeachMath::ShakeDetector shake;
bool seeded = false;

void drawDie(int n) {
  // Rows of the die face; 'o' marks a dot.
  static const char* const faces[6][3] = {
    {"     ", "  o  ", "     "}, {"o    ", "     ", "    o"}, {"o    ", "  o  ", "    o"},
    {"o   o", "     ", "o   o"}, {"o   o", "  o  ", "o   o"}, {"o   o", "o   o", "o   o"}};
  Serial.println(F("+-------+"));
  for (int r = 0; r < 3; r++) { Serial.print(F("| ")); Serial.print(faces[n - 1][r]); Serial.println(F(" |")); }
  Serial.println(F("+-------+"));
}

void setup() {
  Serial.begin(115200);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Shake the board to roll the die."));
}

void loop() {
  if (!mpu.update()) return;
  if (!shake.update(millis(), mpu.shock())) return;

  if (!seeded) { randomSeed(micros()); seeded = true; }   // the moment of the shake is random enough
  int n = random(1, 7);                                    // 1..6
  Serial.print(F("You rolled ")); Serial.println(n);
  drawDie(n);
}
