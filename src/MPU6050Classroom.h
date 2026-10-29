// MPU6050Classroom — a teaching-focused MPU-6050 DMP library, written from scratch
// on top of Wire. The DMP firmware images are the MIT-licensed captures from
// the i2cdevlib/ElectronicCats library; the firmware itself is InvenSense's
// and cannot be authored from scratch.
//
// Design rules:
//  - The library prints only where you point it: reportTo(Serial) for short
//    student messages (freeze repairs, INT wire status), attachDebug(Serial)
//    for the full diary. With neither attached it stays silent, so binary
//    streams such as the Teapot packets are never corrupted.
//  - Automatic freeze repair is ON by default (see readPacket()).
//  - Every operation returns a named error code, never a bare true/false.
//  - Firmware is verified after writing, byte for byte.
//  - The DMP version (2.0 or 6.12) is chosen at run time in begin().
//
// MIT license. Firmware images Copyright (c) 2012 Jeff Rowberg (MIT).
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "TeachMath.h"

#if !defined(ESP32)
  typedef void* TaskHandle_t;   // host/stub builds only; the library targets ESP32
#endif
#ifndef IRAM_ATTR
  #define IRAM_ATTR
#endif

enum class DmpVersion : uint8_t { V20, V612 };

enum MpuError : uint8_t {
  MPU_OK              = 0,
  MPU_ERR_NO_DEVICE   = 1,  // nothing answered on I2C
  MPU_ERR_WRONG_CHIP  = 2,  // WHO_AM_I is not 0x68 (clone or different sensor)
  MPU_ERR_FW_WRITE    = 3,  // I2C write failed while loading firmware
  MPU_ERR_FW_VERIFY   = 4,  // read-back of firmware did not match
  MPU_ERR_I2C         = 5,  // other I2C failure
};

struct ChipIdentity {
  bool    present;   // something ACKed on the bus
  uint8_t address;   // 0x68 or 0x69
  uint8_t whoAmI;    // raw register 0x75
  const __FlashStringHelper* name;  // human-readable chip name
};

class MPU6050Classroom {
public:
  explicit MPU6050Classroom(uint8_t address = 0x68, TwoWire& wire = Wire)
    : _addr(address), _wire(&wire) {}

  // Optional diary. The library stays silent unless this is attached.
  void attachDebug(Stream& s) { _dbg = &s; }

  // Short, student-friendly messages: start-up notice, freeze repairs,
  // INT wire status, "loop() too slow" warnings. Call before begin().
  void reportTo(Stream& s) { _report = &s; }

  // Ask a yes/no question in the Serial Monitor. Waits up to timeoutMs for
  // y or n (then Enter). No answer: returns defaultYes. Never waits forever,
  // so a board with no one at the computer still starts.
  static bool askYesNo(Stream& s, const char* question,
                       uint32_t timeoutMs = 10000, bool defaultYes = false);

  // Readable name for an error code, e.g. "MPU_ERR_NO_DEVICE".
  static const __FlashStringHelper* errorName(MpuError e);

  // ---- Easy mode: two calls are enough ----
  // easyStart() starts I2C, finds the sensor, loads the firmware, calibrates,
  // starts the DMP and measures 1 g at rest. It explains any problem in
  // plain words and keeps retrying until the sensor works (fix the wires
  // while it waits). Keep the board flat and still while it runs.
  // Call Serial.begin(115200) first.
  // The pins are required: look them up in your board's pin diagram.
  // ESP32: any two free pins. UNO R3/R4: fixed, A4 (SDA) and A5 (SCL).
  void easyStart(Stream& out, int sdaPin, int sclPin,
                 DmpVersion v = DmpVersion::V612);

  // Read new data if there is any. Returns true when the values changed.
  bool update();
  float yaw()   const { return _pose.yawDeg; }     // degrees
  float pitch() const { return _pose.pitchDeg; }   // degrees
  float roll()  const { return _pose.rollDeg; }    // degrees
  float gForce() const { return _gForce; }         // total acceleration in g: 1 at rest, 0 in free fall
  float shock()  const { return _gForce > 1.0f ? _gForce - 1.0f : 1.0f - _gForce; }  // distance from rest, in g
  const TeachMath::Pose& pose() const { return _pose; }

  // Measure the accelerometer at rest (= 1 g), averaged over ms. easyStart()
  // calls it; call it yourself if you use begin()/calibrate()/start().
  bool measureRest(uint32_t ms = 500);
  float restValue() const { return _restMag; }

  // Identify whatever is on the bus. Never fails hard; read the struct.
  ChipIdentity identify();

  // Full start-up: wake the chip, load + verify the chosen firmware, leave
  // the DMP disabled. allowClones=true skips the WHO_AM_I gate (at your risk).
  // The pins are needed for the automatic freeze repair (bus clear).
  // Without them the board's default SDA/SCL pins are used.
  MpuError begin(DmpVersion v, bool allowClones = false);
  MpuError begin(DmpVersion v, int sdaPin, int sclPin, bool allowClones = false);

  // Averaging calibration. Keep the sensor flat and still (~2 s).
  // Writes offsets into the chip's offset registers. Call before start().
  MpuError calibrate(uint8_t passes = 3);

  // Turn the DMP on (begin() leaves it off, matching the reference sequence).
  MpuError start();

  // True when at least one full packet is waiting in the FIFO.
  bool packetAvailable();

  // Read the newest packet, discarding stale ones; overflow-safe.
  // Returns false if no complete packet or the FIFO had to be reset.
  // Automatic freeze repair runs here: if the FIFO stays empty for 1.5 s,
  // or the quaternion stays exactly the same for 5 s, the library calls
  // recover() by itself and reports it. A failed repair is retried every 2 s.
  bool readPacket(uint8_t* out);

  // ---- Automatic freeze repair ----
  void setAutoRepair(bool on) { _autoRepair = on; }   // default: on
  bool autoRepair() const     { return _autoRepair; }
  bool healthy() const        { return _healthy; }    // false while a repair is failing
  uint32_t repairCount() const   { return _repairs; }     // successful repairs
  uint32_t repairFailures() const { return _repairFails; }

  // ---- Counters (for demos and diagnosis) ----
  uint32_t packetsRead() const   { return _packetsRead; }
  uint32_t fifoOverflows() const { return _overflows; }

  // ---- INT pin (optional fifth wire) ----
  // Connect the sensor's INT pin to a GPIO and call this after start().
  // The library listens for signals for 100 ms: if they arrive, the INT pin
  // is used; if not, it reports the problem and carries on without it.
  // Returns true when the INT pin is working.
  bool useInterruptPin(int pin);
  void stopInterruptPin();
  bool interruptActive() const   { return _intActive; }
  uint32_t interruptCount() const { return _intCount; }  // signals since useInterruptPin()
  // Block the calling task until the next INT signal or the timeout.
  // Without a working INT pin it waits 5 ms and returns true (polling).
  // For background tasks; see the 4_BackgroundTask example.
  bool waitForInterrupt(uint32_t timeoutMs);

  // Convenience: readPacket + maths. False when no new data.
  bool getPose(TeachMath::Pose& p) {
    uint8_t buf[42];
    if (!readPacket(buf)) return false;
    TeachMath::poseFromPacket(buf, p);
    return true;
  }

  // ---- Recovery layer (ported from the laser-turret hardening work) ----
  // Clock SCL by hand to free a slave holding SDA low, then restart the bus.
  // mpu reset commands cannot reach a sensor that is jamming the bus.
  void busClear(int sdaPin, int sclPin, uint32_t clockHz = 400000);

  // Bring a frozen sensor back: optional bus clear, chip reset, firmware
  // reload + verify, restore of the offsets saved by calibrate(). Does NOT
  // recalibrate (the board may be tilted or moving), then starts the DMP.
  // Pass the pins to include a bus clear; omit them to use the pins given
  // to begin() (or the board defaults).
  MpuError recover(int sdaPin = -1, int sclPin = -1);

  bool offsetsSaved() const { return _offSaved; }

  // Hardware task watchdog (ESP32; handles core 2.x and 3.x APIs).
  // enable: reboot if watchdogPet() is not called for `seconds`.
  // Pet it only while the system is healthy — a failed recovery should
  // stop petting so the board reboots instead of hanging "alive".
  static bool watchdogEnable(uint32_t seconds);
  static void watchdogPet();
  static void watchdogDisable();

  uint16_t packetSize() const { return _packetSize; }
  DmpVersion version()  const { return _version; }
  const __FlashStringHelper* firmwareName() const {
    if (_version == DmpVersion::V612) return F("DMP 6.12 (3062 bytes)");
    return F("DMP 2.0 (1929 bytes)");
  }

  // Raw register access, public on purpose: students can explore.
  bool    writeReg(uint8_t reg, uint8_t val);
  bool    readRegs(uint8_t reg, uint8_t* buf, uint8_t n);
  uint8_t readReg(uint8_t reg);
  bool    readAveraged(int32_t accel[3], int32_t gyro[3], uint16_t samples);

private:
  MpuError loadFirmware(const uint8_t* image, uint16_t size);
  MpuError initV20();
  MpuError initV612();
  bool writeMemory(uint8_t bank, uint8_t startAddr, const uint8_t* data,
                   uint8_t n, bool fromProgmem);
  bool readMemory(uint8_t bank, uint8_t startAddr, uint8_t* data, uint8_t n);
  bool writeBits(uint8_t reg, uint8_t bitStart, uint8_t len, uint8_t val);
  uint16_t fifoCount();
  void resetFifo();
  void log(const __FlashStringHelper* msg);
  void log(const __FlashStringHelper* msg, int v);
  void say(const __FlashStringHelper* msg);
  MpuError initChip();
  bool checkInterrupt(bool announce);
  void attemptRepair(uint8_t reason);
  static void IRAM_ATTR onInterrupt(void* arg);
  static void onInterruptNoArg();
  static MPU6050Classroom* _intOwner;

  uint8_t   _addr;
  TwoWire*  _wire;
  Stream*   _dbg = nullptr;
  DmpVersion _version = DmpVersion::V612;
  uint16_t  _packetSize = 28;
  bool      _ready = false;
  bool      _allowClones = false;
  bool      _offSaved = false;
  int16_t   _off[6] = {0,0,0,0,0,0};   // ax, ay, az, gx, gy, gz

  TeachMath::Pose _pose = {1,0,0,0, 0,0,1, 0,0,0};
  float     _restMag = 0, _gForce = 1.0f;
  Stream*   _report = nullptr;
  int       _sda = -1, _scl = -1;
  bool      _running = false;
  bool      _autoRepair = true;
  bool      _healthy = true;
  bool      _repairing = false;
  uint32_t  _repairs = 0, _repairFails = 0, _nextRepairTry = 0;
  uint32_t  _packetsRead = 0, _overflows = 0, _lastOverflowWarn = 0;
  TeachMath::FreezeDetector _fd;

  int       _intPin = -1;
  bool      _intActive = false;
  volatile uint32_t _intCount = 0;
  volatile uint32_t _intPending = 0;
  volatile TaskHandle_t _waitTask = nullptr;
  uint32_t  _lastPollMs = 0;
};
