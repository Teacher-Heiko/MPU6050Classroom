// 1a_WhoAmI — which motion chip is on your board?
// Reads the chip's WHO_AM_I register. A genuine MPU-6050 answers 0x68.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
#if defined(ESP32)
#error "This example is for Arduino UNO R3/R4. Open the same example in the ESP32 folder."
#endif
#include <MPU6050Classroom.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;

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
  Wire.begin();                 // UNO: I2C is always on A4/A5
  Serial.println(F("Looking for the motion sensor..."));
  show(mpuLow,  F("Address 0x68: "));
  show(mpuHigh, F("Address 0x69: "));
  Serial.println(F("0x68 = genuine MPU-6050. Anything else: show your teacher."));
  Serial.println(F("Nothing answers at all? Check the four wires and the two pin numbers."));
}

void loop() {}
