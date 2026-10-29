// 1a_WhoAmI — which motion chip is on your board?
// Reads the chip's WHO_AM_I register. A genuine MPU-6050 answers 0x68.
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

MPU6050Classroom mpuLow(0x68);
MPU6050Classroom mpuHigh(0x69);   // some boards set the address pin AD0 high

void show(MPU6050Classroom& m, const __FlashStringHelper* label) {
  ChipIdentity id = m.identify();
  Serial.print(label);
  if (!id.present) { Serial.println(F("nothing answers")); return; }
  Serial.print(F("WHO_AM_I = 0x"));
  Serial.print(id.whoAmI, HEX);
  Serial.print(F("  -> "));
  Serial.println(id.name);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Wire.begin(SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Looking for the motion sensor..."));
  show(mpuLow,  F("Address 0x68: "));
  show(mpuHigh, F("Address 0x69: "));
  Serial.println(F("0x68 = genuine MPU-6050. Anything else: show your teacher."));
  Serial.println(F("Nothing answers at all? Check the four wires and the two pin numbers."));
}

void loop() {}
