// 3_GestureRemote — tilt the board to send FORWARD, BACK, LEFT, RIGHT or
// STOP to a second ESP32 (run 3_GestureReceiver on that one).
//
// It sends over ESP-NOW, a direct radio link between ESP32 boards. No Wi-Fi
// network or router is needed. Messages are sent to every board nearby, so
// each team picks its own GROUP number; the receiver ignores other groups.
//
// Hold the board flat to send STOP. Tilt more than TILT_LIMIT_DEG to send
// a direction. If FORWARD and BACK (or LEFT and RIGHT) are swapped for the
// way you hold the board, change the SWAP settings.
//
// Wiring: VCC -> 3V3, GND -> GND, SDA -> GPIO21, SCL -> GPIO22.
#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <WiFi.h>
#include <esp_now.h>
#include <MPU6050Classroom.h>

// ---- Pins: look these up in your board's pin diagram ----
// ESP-WROVER-KIT: SDA = GPIO21, SCL = GPIO22
const int SENSOR_SDA = 21;
const int SENSOR_SCL = 22;

const uint8_t GROUP = 1;            // same number on remote and receiver (1..255)
const float   TILT_LIMIT_DEG = 25;  // tilt needed for a direction
const bool    SWAP_FORWARD_BACK = false;
const bool    SWAP_LEFT_RIGHT   = false;

struct Command {                    // the message; the receiver uses the same layout
  char    tag[4];                   // "MPU" marks our messages
  uint8_t group;
  uint8_t gesture;                  // TeachMath::Gesture
};

uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
MPU6050Classroom mpu;

void setup() {
  Serial.begin(115200);
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Problem: the ESP-NOW radio did not start. Press reset.");
    while (true) delay(1000);
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = 0;                 // use the current channel
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) Serial.println("Problem: could not add the broadcast address.");
  Serial.printf("Gesture remote ready, group %d. Tilt the board.\n", GROUP);
}

void loop() {
  mpu.update();

  static uint32_t lastSend = 0;
  if (millis() - lastSend < 100) return;      // 10 messages per second
  lastSend = millis();

  TeachMath::Gesture g = TeachMath::gestureFromTilt(mpu.pitch(), mpu.roll(), TILT_LIMIT_DEG);
  if (SWAP_FORWARD_BACK) {
    if (g == TeachMath::GESTURE_FORWARD) g = TeachMath::GESTURE_BACK;
    else if (g == TeachMath::GESTURE_BACK) g = TeachMath::GESTURE_FORWARD;
  }
  if (SWAP_LEFT_RIGHT) {
    if (g == TeachMath::GESTURE_LEFT) g = TeachMath::GESTURE_RIGHT;
    else if (g == TeachMath::GESTURE_RIGHT) g = TeachMath::GESTURE_LEFT;
  }

  Command c = {{'M', 'P', 'U', 0}, GROUP, (uint8_t)g};
  esp_err_t r = esp_now_send(BROADCAST, (const uint8_t*)&c, sizeof c);

  static int lastShown = -1;
  if ((int)g != lastShown) {
    lastShown = g;
    Serial.printf("sending: %s%s\n", TeachMath::gestureName(g), r == ESP_OK ? "" : "   (send failed)");
  }
}
