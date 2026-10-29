// TeachPwm.h — servo and motor PWM helpers for ESP32, for the project
// examples. Works with ESP32 Arduino core 3.x (ledcAttach/ledcWrite by pin)
// and 2.x (ledcSetup/ledcAttachPin by channel).
#pragma once
#include <Arduino.h>

#if defined(ESP32)
#include <esp_arduino_version.h>

namespace TeachPwm {

#if ESP_ARDUINO_VERSION_MAJOR < 3
inline int channelFor(uint8_t pin) {           // core 2.x: one channel per pin
  static int8_t pins[16] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
  for (int c = 0; c < 16; c++) if (pins[c] == pin) return c;
  for (int c = 0; c < 16; c++) if (pins[c] < 0) { pins[c] = pin; return c; }
  return -1;
}
#endif

// Start PWM on a pin. Returns false if no PWM channel was free.
inline bool attach(uint8_t pin, uint32_t freqHz, uint8_t bits) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  return ledcAttach(pin, freqHz, bits);
#else
  int c = channelFor(pin);
  if (c < 0) return false;
  ledcSetup(c, freqHz, bits);
  ledcAttachPin(pin, c);
  return true;
#endif
}

inline void write(uint8_t pin, uint32_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  int c = channelFor(pin);
  if (c >= 0) ledcWrite(c, duty);
#endif
}

// ---- Hobby servo: 50 Hz, pulse 500..2500 us for 0..180 degrees ----
const uint8_t SERVO_BITS = 16;
inline bool servoAttach(uint8_t pin) { return attach(pin, 50, SERVO_BITS); }
inline void servoWrite(uint8_t pin, float degrees) {
  if (degrees < 0) degrees = 0;
  if (degrees > 180) degrees = 180;
  float us = 500.0f + degrees * (2000.0f / 180.0f);
  uint32_t duty = (uint32_t)(us * 65535.0f / 20000.0f);   // 20 ms period
  write(pin, duty);
}

// ---- DC motor on a two-input driver such as DRV8833 ----
// speed -255..255. Forward: PWM on in1, in2 low. Backward: the reverse.
const uint8_t MOTOR_BITS = 8;
inline bool motorAttach(uint8_t in1, uint8_t in2) {
  return attach(in1, 20000, MOTOR_BITS) && attach(in2, 20000, MOTOR_BITS);
}
inline void motorWrite(uint8_t in1, uint8_t in2, int speed) {
  if (speed > 255) speed = 255;
  if (speed < -255) speed = -255;
  if (speed >= 0) { write(in1, (uint32_t)speed);  write(in2, 0); }
  else            { write(in1, 0); write(in2, (uint32_t)(-speed)); }
}

} // namespace TeachPwm
#endif
