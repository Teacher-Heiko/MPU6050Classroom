// 5_WiFiSetup — connect the board to Wi-Fi and test a phone message.
//
// First start (no saved settings):
//   The board opens its own Wi-Fi network "MPU-Setup-xxxx" (password
//   mpu6050setup). Join it with a phone or laptop and open the setup page
//   (the Serial Monitor shows the address). Enter the Wi-Fi name and
//   password and press Save. The board restarts and connects.
// Later starts: it connects with the saved settings. To change them, type c
// in the Serial Monitor during the first 5 s.
//
// School networks that ask for a username (WPA2-Enterprise): fill in the
// username field on the setup page. ESP32 only; the UNO R4 WiFi cannot join
// such networks.
//
// Phone messages: install the free "ntfy" app, add a subscription with the
// topic name shown in the Serial Monitor, and this sketch sends a test
// message. Anyone who knows the topic name can read its messages, so keep
// it secret and do not send private information.
//
// Board: any ESP32 board with Wi-Fi (tested by compiling for the ESP-WROVER-KIT).
#if !defined(ESP32)
#error "This example is for ESP32 boards. Open the same example in the Arduino folder."
#endif
#include <ClassroomWiFi.h>

ClassroomWiFi wifi;

void setup() {
  Serial.begin(115200);
  delay(1500);
  if (!wifi.begin(Serial)) return;          // saved settings, or the setup page
  if (wifi.topic()[0]) {
    Serial.print(F("ntfy topic: ")); Serial.println(wifi.topic());
    Serial.println(F("Subscribe to this topic in the ntfy app, then watch for a test message."));
    wifi.sendNtfy(wifi.topic(), "MPU6050Classroom", "Wi-Fi works. This is a test message.");
  } else {
    Serial.println(F("No ntfy topic saved. Type c at the next start to add one."));
  }
}

void loop() {
  wifi.keepConnected();
  delay(100);
}
