// 3_GestureReceiver — runs on the SECOND ESP32 (the robot). It receives
// the commands from 3_GestureRemote and prints them. This board needs no
// sensor.
//
// Safety: if no command arrives for 0.5 s (remote switched off, out of
// range), the receiver switches to STOP by itself.
//
// To drive a robot, fill in drive() at the bottom, for example with
// TeachPwm::motorWrite() for two motors on a DRV8833 driver.
#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <WiFi.h>
#include <esp_now.h>
#include <esp_arduino_version.h>
#include <MPU6050Classroom.h>          // for TeachMath::Gesture and gestureName()

const uint8_t  GROUP = 1;          // must match the remote
const uint32_t TIMEOUT_MS = 500;
const int      LED_PIN = 2;        // WROVER-KIT RGB LED, green: on while commands arrive

struct Command {                   // same layout as in 3_GestureRemote
  char    tag[4];
  uint8_t group;
  uint8_t gesture;
};

volatile uint8_t  lastGesture = TeachMath::GESTURE_FLAT;
volatile uint32_t lastMs = 0;

// ESP-NOW calls this when a message arrives. Keep it short.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
#else
void onReceive(const uint8_t* mac, const uint8_t* data, int len) {
#endif
  if (len != sizeof(Command)) return;
  Command c;
  memcpy(&c, data, sizeof c);
  if (memcmp(c.tag, "MPU", 4) != 0 || c.group != GROUP) return;   // not ours
  if (c.gesture > TeachMath::GESTURE_RIGHT) return;
  lastGesture = c.gesture;
  lastMs = millis();
}

void drive(TeachMath::Gesture g) {
  // Put your motor code here. For now the command is only printed.
  (void)g;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  pinMode(LED_PIN, OUTPUT);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Problem: the ESP-NOW radio did not start. Press reset.");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(onReceive);
  Serial.printf("Receiver ready, group %d. Waiting for the remote...\n", GROUP);
}

void loop() {
  bool fresh = lastMs != 0 && millis() - lastMs < TIMEOUT_MS;
  TeachMath::Gesture g = fresh ? (TeachMath::Gesture)lastGesture : TeachMath::GESTURE_FLAT;
  digitalWrite(LED_PIN, fresh ? HIGH : LOW);
  drive(g);

  static int lastShown = -1;
  static bool lastFresh = false;
  if ((int)g != lastShown || fresh != lastFresh) {
    lastShown = g; lastFresh = fresh;
    Serial.printf("command: %s%s\n", TeachMath::gestureName(g), fresh ? "" : "   (no signal: stopped)");
  }
  delay(10);
}
