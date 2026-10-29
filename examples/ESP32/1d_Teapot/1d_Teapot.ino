// 1d_Teapot — sends the board's motion to a 3D viewer on the computer.
// Viewers (in the library's extras folder):
//   TeacupViewer.html  — Chrome or Edge, nothing to install (teapot or airplane)
//   processing/TeapotViewer — Processing
// Close the Serial Monitor before you connect a viewer.
//
// The serial line carries binary packets, so the sketch prints no text.
// If set-up fails, the LED blinks the error number (count the blinks).
// Automatic freeze repair still runs; the model pauses for about 1 s during
// a repair, and the LED stays on while a repair is failing.
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
const int LED_PIN = 2   /* WROVER-KIT RGB LED, green */;

MPU6050Classroom mpu;
uint8_t fifo[42];
uint8_t packet[14];
uint8_t counter = 0;

void blinkError(int code) {
  while (true) {
    for (int i = 0; i < code; i++) {
      digitalWrite(LED_PIN, HIGH); delay(200);
      digitalWrite(LED_PIN, LOW);  delay(200);
    }
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  delay(1500);
  Wire.begin(SENSOR_SDA, SENSOR_SCL);
  Wire.setClock(400000);
  MpuError e = mpu.begin(DmpVersion::V612, SENSOR_SDA, SENSOR_SCL);
  if (e != MPU_OK) blinkError((int)e);
  mpu.calibrate();
  mpu.start();
}

void loop() {
  digitalWrite(LED_PIN, mpu.healthy() ? LOW : HIGH);
  if (!mpu.readPacket(fifo)) return;
  TeachMath::buildTeapotPacket(fifo, counter++, packet);
  Serial.write(packet, 14);
}
