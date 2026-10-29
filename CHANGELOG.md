# Changelog

All notable changes to MPU6050Classroom are listed here, newest first.
Version numbers follow [Semantic Versioning](https://semver.org/):
MAJOR.MINOR.PATCH.

## 1.0.0 — first public release

- Two-line start with `easyStart(Serial, SENSOR_SDA, SENSOR_SCL)`; pins are
  always set in the sketch.
- Automatic freeze repair in every sketch, with a message for each repair.
- Optional INT pin, checked at start-up; the library carries on without it
  if no signals arrive.
- Boards: ESP32, Arduino UNO R3, UNO R4 Minima and UNO R4 WiFi.
- 55 examples in two folders (`ESP32`, `Arduino`), in five levels.
- Wi-Fi helpers (ESP32 and UNO R4 WiFi): setup page, school login (ESP32),
  phone messages through ntfy, small live web pages.
- Project logic in `TeachMath`, with computer tests.
- Earlier, unpublished versions were called **MPU6050Teach**. Remove that
  library before installing this one.
