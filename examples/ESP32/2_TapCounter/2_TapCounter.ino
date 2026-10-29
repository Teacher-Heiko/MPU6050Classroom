// 2_TapCounter — knock on the table and the sketch counts the knocks.
//
// Knocks less than 0.7 s apart form one group. When the table goes quiet,
// the sketch prints the group size and reacts:
//   2 knocks: LED on/off      3 knocks: prints the current tilt
// Add your own actions for other numbers.
//
// Tip: lay the board flat on the table, close to where you knock. Missed
// knocks: lower TAP_LIMIT_G. Too many: raise it.
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
const int   LED_PIN = 2   /* WROVER-KIT RGB LED, green */;
const float TAP_LIMIT_G = 0.5;    // a knock is a shock above this, in g

MPU6050Classroom mpu;
TeachMath::TapCounter taps;
bool ledOn = false;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  taps.tapAboveShockG = TAP_LIMIT_G;
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.println(F("Knock on the table: 2 knocks = LED, 3 knocks = tilt."));
}

void loop() {
  if (!mpu.update()) return;

  uint8_t n = taps.update(millis(), mpu.shock());
  if (n == 0) return;

  Serial.print(n); Serial.println(n == 1 ? F(" knock") : F(" knocks"));
  if (n == 2) {
    ledOn = !ledOn;
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    Serial.println(ledOn ? F("  LED on") : F("  LED off"));
  } else if (n == 3) {
    Serial.print(F("  pitch ")); Serial.print(mpu.pitch(), 1);
    Serial.print(F(" deg, roll ")); Serial.print(mpu.roll(), 1); Serial.println(F(" deg"));
  }
}
