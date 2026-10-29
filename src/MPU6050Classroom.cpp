#include "MPU6050Classroom.h"
#include "dmp_image_v20.h"
#include "dmp_image_v612.h"

// ---- Register map (InvenSense RM-MPU-6000A) ----
static const uint8_t R_XA_OFFS_H   = 0x06; // accel offsets 0x06..0x0B
static const uint8_t R_XG_OFFS_H   = 0x13; // gyro  offsets 0x13..0x18
static const uint8_t R_SMPLRT_DIV  = 0x19;
static const uint8_t R_CONFIG      = 0x1A;
static const uint8_t R_GYRO_CFG    = 0x1B;
static const uint8_t R_ACCEL_CFG   = 0x1C;
static const uint8_t R_MOT_THR     = 0x1F;
static const uint8_t R_MOT_DUR     = 0x20;
static const uint8_t R_ZRMOT_THR   = 0x21;
static const uint8_t R_ZRMOT_DUR   = 0x22;
static const uint8_t R_FIFO_EN     = 0x23;
static const uint8_t R_I2C_SLV0_A  = 0x25;
static const uint8_t R_I2C_MST_CTL = 0x24;
static const uint8_t R_INT_PIN_CFG = 0x37;
static const uint8_t R_INT_ENABLE  = 0x38;
static const uint8_t R_INT_STATUS  = 0x3A;
static const uint8_t R_ACCEL_XOUT  = 0x3B;
static const uint8_t R_USER_CTRL   = 0x6A;
static const uint8_t R_PWR_MGMT_1  = 0x6B;
static const uint8_t R_BANK_SEL    = 0x6D;
static const uint8_t R_MEM_START   = 0x6E;
static const uint8_t R_MEM_R_W     = 0x6F;
static const uint8_t R_DMP_CFG_1   = 0x70;
static const uint8_t R_DMP_CFG_2   = 0x71;
static const uint8_t R_FIFO_COUNTH = 0x72;
static const uint8_t R_FIFO_R_W    = 0x74;
static const uint8_t R_WHO_AM_I    = 0x75;

static const uint8_t MEM_CHUNK = 16;

// ---------------- low-level I2C ----------------
bool MPU6050Classroom::writeReg(uint8_t reg, uint8_t val) {
  _wire->beginTransmission(_addr);
  _wire->write(reg); _wire->write(val);
  return _wire->endTransmission() == 0;
}

bool MPU6050Classroom::readRegs(uint8_t reg, uint8_t* buf, uint8_t n) {
  _wire->beginTransmission(_addr);
  _wire->write(reg);
  if (_wire->endTransmission(false) != 0) return false;
  if (_wire->requestFrom((int)_addr, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = _wire->read();
  return true;
}

uint8_t MPU6050Classroom::readReg(uint8_t reg) {
  uint8_t b = 0xFF; readRegs(reg, &b, 1); return b;
}

bool MPU6050Classroom::writeBits(uint8_t reg, uint8_t bitStart, uint8_t len, uint8_t val) {
  uint8_t b;
  if (!readRegs(reg, &b, 1)) return false;
  uint8_t mask = ((1 << len) - 1) << (bitStart - len + 1);
  val <<= (bitStart - len + 1);
  b = (b & ~mask) | (val & mask);
  return writeReg(reg, b);
}

void MPU6050Classroom::log(const __FlashStringHelper* m)        { if (_dbg) _dbg->println(m); }
void MPU6050Classroom::say(const __FlashStringHelper* m)        { if (_report) _report->println(m); }

bool MPU6050Classroom::askYesNo(Stream& s, const char* question,
                            uint32_t timeoutMs, bool defaultYes) {
  while (s.available()) s.read();                     // ignore old input
  s.println(question);
  s.print(F("  Type y or n, then press Enter ("));
  s.print(timeoutMs / 1000);
  s.print(F(" s; no answer = "));
  s.print(defaultYes ? 'y' : 'n');
  s.println(F(")."));
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (s.available()) {
      char c = (char)s.read();
      if (c == 'y' || c == 'Y') { s.println(F("  Answer: yes")); return true; }
      if (c == 'n' || c == 'N') { s.println(F("  Answer: no"));  return false; }
    }
    delay(10);
  }
  if (defaultYes) s.println(F("  No answer: using yes."));
  else            s.println(F("  No answer: using no."));
  return defaultYes;
}

const __FlashStringHelper* MPU6050Classroom::errorName(MpuError e) {
  switch (e) {
    case MPU_OK:             return F("MPU_OK");
    case MPU_ERR_NO_DEVICE:  return F("MPU_ERR_NO_DEVICE");
    case MPU_ERR_WRONG_CHIP: return F("MPU_ERR_WRONG_CHIP");
    case MPU_ERR_FW_WRITE:   return F("MPU_ERR_FW_WRITE");
    case MPU_ERR_FW_VERIFY:  return F("MPU_ERR_FW_VERIFY");
    case MPU_ERR_I2C:        return F("MPU_ERR_I2C");
  }
  return F("unknown error");
}
void MPU6050Classroom::log(const __FlashStringHelper* m, int v) { if (_dbg) { _dbg->print(m); _dbg->println(v); } }

// ---------------- identity ----------------
static const __FlashStringHelper* nameForId(uint8_t id) {
  switch (id) {
    case 0x68: return F("MPU-6050 / MPU-6000 (genuine ID)");
    case 0x70: return F("MPU-6500 (different sensor; this DMP code is untested on it)");
    case 0x71: return F("MPU-9250 (different sensor; use a 9250 library)");
    case 0x72: return F("unknown clone (raw reads may work; DMP unproven)");
    case 0x98: return F("non-6050 part (same ID as ICM-20689; DMP unproven)");
    default:   return F("unrecognised ID (clone, or wrong wiring)");
  }
}

ChipIdentity MPU6050Classroom::identify() {
  ChipIdentity c{false, _addr, 0xFF, F("no device")};
  _wire->beginTransmission(_addr);
  if (_wire->endTransmission() != 0) return c;
  c.present = true;
  c.whoAmI  = readReg(R_WHO_AM_I);
  c.name    = nameForId(c.whoAmI);
  return c;
}

// ---------------- DMP memory (banked) ----------------
bool MPU6050Classroom::writeMemory(uint8_t bank, uint8_t startAddr,
                               const uint8_t* data, uint8_t n, bool fromProgmem) {
  if (!writeReg(R_BANK_SEL, bank & 0x1F)) return false;
  if (!writeReg(R_MEM_START, startAddr)) return false;
  _wire->beginTransmission(_addr);
  _wire->write(R_MEM_R_W);
  for (uint8_t i = 0; i < n; i++)
    _wire->write(fromProgmem ? pgm_read_byte(data + i) : data[i]);
  return _wire->endTransmission() == 0;
}

bool MPU6050Classroom::readMemory(uint8_t bank, uint8_t startAddr, uint8_t* data, uint8_t n) {
  if (!writeReg(R_BANK_SEL, bank & 0x1F)) return false;
  if (!writeReg(R_MEM_START, startAddr)) return false;
  return readRegs(R_MEM_R_W, data, n);
}

MpuError MPU6050Classroom::loadFirmware(const uint8_t* image, uint16_t size) {
  uint8_t verify[MEM_CHUNK];
  for (uint16_t i = 0; i < size; i += MEM_CHUNK) {
    uint8_t chunk = (uint8_t)min((uint16_t)MEM_CHUNK, (uint16_t)(size - i));
    uint8_t bank = i >> 8, addr = i & 0xFF;
    if (!writeMemory(bank, addr, image + i, chunk, true)) {
      log(F("firmware write failed at byte "), i);
      return MPU_ERR_FW_WRITE;
    }
    if (!readMemory(bank, addr, verify, chunk)) return MPU_ERR_I2C;
    for (uint8_t k = 0; k < chunk; k++) {
      if (verify[k] != pgm_read_byte(image + i + k)) {
        log(F("firmware verify failed at byte "), i + k);
        return MPU_ERR_FW_VERIFY;
      }
    }
  }
  log(F("firmware written and verified, bytes: "), size);
  return MPU_OK;
}

// ---------------- init sequences (ported from the reference library) ----------------
MpuError MPU6050Classroom::initV612() {
  uint8_t ok = 1;
  ok &= writeBits(R_PWR_MGMT_1, 7, 1, 1); delay(100);       // device reset
  ok &= writeBits(R_USER_CTRL, 2, 3, 0b111); delay(100);    // full signal-path reset
  ok &= writeReg(R_PWR_MGMT_1, 0x01);                        // clock = PLL X gyro
  ok &= writeReg(R_INT_ENABLE, 0x00);
  ok &= writeReg(R_FIFO_EN,    0x00);                        // DMP uses its own FIFO feed
  ok &= writeReg(R_ACCEL_CFG,  0x00);                        // +/-2g
  ok &= writeReg(R_INT_PIN_CFG,0x80);                        // INT active-low, clear-on-read
  ok &= writeReg(R_PWR_MGMT_1, 0x01);
  ok &= writeReg(R_SMPLRT_DIV, 4);                           // 1 kHz / (1+4) = 200 Hz
  ok &= writeReg(R_CONFIG,     0x01);                        // DLPF 188 Hz
  if (!ok) return MPU_ERR_I2C;

  MpuError e = loadFirmware(DMP_IMAGE_V612, DMP_IMAGE_V612_SIZE);
  if (e != MPU_OK) return e;

  ok &= writeReg(R_DMP_CFG_1, 0x04);                         // program start 0x0400
  ok &= writeReg(R_DMP_CFG_2, 0x00);
  ok &= writeReg(R_GYRO_CFG,  0x18);                         // +/-2000 deg/s
  ok &= writeReg(R_USER_CTRL, 0xC0);                         // DMP_EN | FIFO_EN
  ok &= writeReg(R_INT_ENABLE,0x02);                         // RAW_DMP_INT
  ok &= writeBits(R_USER_CTRL, 2, 1, 1);                     // FIFO reset
  ok &= writeBits(R_USER_CTRL, 7, 1, 0);                     // DMP off until start()
  _packetSize = 28;
  return ok ? MPU_OK : MPU_ERR_I2C;
}

MpuError MPU6050Classroom::initV20() {
  uint8_t ok = 1;
  ok &= writeBits(R_PWR_MGMT_1, 7, 1, 1); delay(30);         // reset
  ok &= writeBits(R_PWR_MGMT_1, 6, 1, 0);                    // sleep off
  ok &= writeReg(R_I2C_SLV0_A, 0x7F);                        // reference "slave dance"
  ok &= writeBits(R_USER_CTRL, 5, 1, 0);                     // I2C master off
  ok &= writeReg(R_I2C_SLV0_A, 0x68);
  ok &= writeBits(R_USER_CTRL, 1, 1, 1); delay(20);          // reset I2C master
  ok &= writeBits(R_PWR_MGMT_1, 2, 3, 0x03);                 // clock = PLL Z gyro
  ok &= writeReg(R_INT_ENABLE, 0x12);                        // FIFO_OFLOW | DMP_INT
  ok &= writeReg(R_SMPLRT_DIV, 4);                           // 200 Hz
  ok &= writeBits(R_CONFIG, 5, 3, 0x01);                     // ext sync TEMP_OUT_L
  ok &= writeBits(R_CONFIG, 2, 3, 0x03);                     // DLPF 42 Hz
  ok &= writeReg(R_GYRO_CFG, 0x18);                          // +/-2000 deg/s
  if (!ok) return MPU_ERR_I2C;

  MpuError e = loadFirmware(DMP_IMAGE_V20, DMP_IMAGE_V20_SIZE);
  if (e != MPU_OK) return e;

  uint8_t fifoRate[2] = {0x00, 0x01};                        // FIFO rate divisor
  ok &= writeMemory(0x02, 0x16, fifoRate, 2, false);
  ok &= writeReg(R_DMP_CFG_1, 0x03);                         // program start 0x0300
  ok &= writeReg(R_DMP_CFG_2, 0x00);
  ok &= writeBits(0x00, 0, 1, 0);                            // OTP bank valid = false
  ok &= writeReg(R_MOT_THR,   2);
  ok &= writeReg(R_ZRMOT_THR, 156);
  ok &= writeReg(R_MOT_DUR,   80);
  ok &= writeReg(R_ZRMOT_DUR, 0);
  ok &= writeBits(R_USER_CTRL, 6, 1, 1);                     // FIFO enable
  ok &= writeBits(R_USER_CTRL, 3, 1, 1);                     // DMP reset
  ok &= writeBits(R_USER_CTRL, 7, 1, 0);                     // DMP off until start()
  _packetSize = 42;
  ok &= writeBits(R_USER_CTRL, 2, 1, 1);                     // FIFO reset
  (void)readReg(R_INT_STATUS);                               // clear int status
  return ok ? MPU_OK : MPU_ERR_I2C;
}

MpuError MPU6050Classroom::begin(DmpVersion v, int sdaPin, int sclPin, bool allowClones) {
  _sda = sdaPin; _scl = sclPin;
  return begin(v, allowClones);
}

MpuError MPU6050Classroom::begin(DmpVersion v, bool allowClones) {
  _version = v;
  _allowClones = allowClones;
#if defined(ESP32)
  if (_sda < 0 || _scl < 0) { _sda = SDA; _scl = SCL; }   // board defaults
#endif
  _running = false;
  _healthy = true;
  MpuError e = initChip();
  if (e == MPU_OK && _autoRepair && _report) {
    _report->println(F("MPU6050Classroom: automatic freeze repair is ON."));
    _report->println(F("  If the sensor stops sending data, the library repairs it"));
    _report->println(F("  and prints a message here. You do not need to do anything."));
  }
  return e;
}

// Identify, then run the firmware's start-up sequence. Used by begin()
// and by recover(); prints nothing for students.
MpuError MPU6050Classroom::initChip() {
  ChipIdentity id = identify();
  if (!id.present) { log(F("no device on the bus")); return MPU_ERR_NO_DEVICE; }
  log(F("WHO_AM_I: "), id.whoAmI);

  if (id.whoAmI != 0x68 && !_allowClones) return MPU_ERR_WRONG_CHIP;

  MpuError e = (_version == DmpVersion::V612) ? initV612() : initV20();
  _ready = (e == MPU_OK);
  return e;
}

MpuError MPU6050Classroom::start() {
  if (!_ready) return MPU_ERR_I2C;
  if (!writeBits(R_USER_CTRL, 2, 1, 1)) return MPU_ERR_I2C;  // FIFO reset
  if (!writeBits(R_USER_CTRL, 7, 1, 1)) return MPU_ERR_I2C;  // DMP on
  // INT pin: active-low 50 us pulse per packet, for both firmware versions.
  if (!writeReg(R_INT_PIN_CFG, 0x80)) return MPU_ERR_I2C;
  (void)readReg(R_INT_STATUS);
  _running = true;
  _fd.reset(millis());
  if (_intPin >= 0) checkInterrupt(!_repairing);
  return MPU_OK;
}

// ---------------- calibration (averaging, IMU_Zero style) ----------------
bool MPU6050Classroom::readAveraged(int32_t accel[3], int32_t gyro[3], uint16_t samples) {
  int64_t a[3] = {0,0,0}, g[3] = {0,0,0};
  uint8_t raw[14];
  for (uint16_t i = 0; i < samples; i++) {
    if (!readRegs(R_ACCEL_XOUT, raw, 14)) return false;
    a[0] += TeachMath::be16(raw+0);  a[1] += TeachMath::be16(raw+2);  a[2] += TeachMath::be16(raw+4);
    g[0] += TeachMath::be16(raw+8);  g[1] += TeachMath::be16(raw+10); g[2] += TeachMath::be16(raw+12);
    delay(2);
  }
  for (int i = 0; i < 3; i++) { accel[i] = a[i] / samples; gyro[i] = g[i] / samples; }
  return true;
}

MpuError MPU6050Classroom::calibrate(uint8_t passes) {
  // Sensor must be flat and still. Gyro offset registers use the +/-1000 dps
  // scale (raw/4 at +/-250 dps); accel offset registers use the +/-16g scale
  // (raw/8 at +/-2g) and bit 0 of each low byte must be preserved.
  uint8_t prevGyroCfg = readReg(R_GYRO_CFG), prevAccCfg = readReg(R_ACCEL_CFG);
  writeReg(R_GYRO_CFG, 0x00);   // +/-250 dps
  writeReg(R_ACCEL_CFG, 0x00);  // +/-2 g
  delay(50);

  for (uint8_t p = 0; p < passes; p++) {
    int32_t a[3], g[3];
    if (!readAveraged(a, g, 200)) return MPU_ERR_I2C;
    a[2] -= 16384;                                  // remove gravity on Z

    uint8_t buf[6];
    if (!readRegs(R_XA_OFFS_H, buf, 6)) return MPU_ERR_I2C;
    for (int i = 0; i < 3; i++) {
      int16_t cur = TeachMath::be16(buf + 2*i);
      uint8_t tcBit = buf[2*i + 1] & 0x01;          // temperature-comp bit
      int16_t next = cur - (int16_t)(a[i] / 8);
      writeReg(R_XA_OFFS_H + 2*i,     (uint8_t)(next >> 8));
      writeReg(R_XA_OFFS_H + 2*i + 1, (uint8_t)((next & 0xFE) | tcBit));
    }
    if (!readRegs(R_XG_OFFS_H, buf, 6)) return MPU_ERR_I2C;
    for (int i = 0; i < 3; i++) {
      int16_t cur  = TeachMath::be16(buf + 2*i);
      int16_t next = cur - (int16_t)(g[i] / 4);
      writeReg(R_XG_OFFS_H + 2*i,     (uint8_t)(next >> 8));
      writeReg(R_XG_OFFS_H + 2*i + 1, (uint8_t)(next & 0xFF));
    }
    log(F("calibration pass done: "), p + 1);
  }
  writeReg(R_GYRO_CFG, prevGyroCfg);
  writeReg(R_ACCEL_CFG, prevAccCfg);
  delay(50);

  // Save the final offsets so recover() can restore them without
  // recalibrating a board that is tilted or moving.
  uint8_t buf[6];
  if (readRegs(R_XA_OFFS_H, buf, 6)) {
    for (int i = 0; i < 3; i++) _off[i] = TeachMath::be16(buf + 2*i);
    if (readRegs(R_XG_OFFS_H, buf, 6)) {
      for (int i = 0; i < 3; i++) _off[3+i] = TeachMath::be16(buf + 2*i);
      _offSaved = true;
    }
  }
  return MPU_OK;
}

// ---------------- recovery layer ----------------
void MPU6050Classroom::busClear(int sdaPin, int sclPin, uint32_t clockHz) {
  log(F("bus clear on SDA/SCL: "), sdaPin);
  _wire->end();                                       // release the pins from the I2C hardware
  pinMode(sdaPin, INPUT_PULLUP);
  pinMode(sclPin, INPUT_PULLUP);
  // Up to 9 manual clock pulses until the slave releases SDA.
  for (uint8_t i = 0; i < 9 && digitalRead(sdaPin) == LOW; i++) {
    pinMode(sclPin, OUTPUT); digitalWrite(sclPin, LOW); delayMicroseconds(10);
    pinMode(sclPin, INPUT_PULLUP);                      delayMicroseconds(10);
  }
  // A STOP condition: SDA low -> high while SCL is high.
  pinMode(sdaPin, OUTPUT); digitalWrite(sdaPin, LOW); delayMicroseconds(10);
  pinMode(sdaPin, INPUT_PULLUP);                      delayMicroseconds(10);
#if defined(ESP32)
  _wire->begin(sdaPin, sclPin);
#else
  _wire->begin();
#endif
  _wire->setClock(clockHz);
}

MpuError MPU6050Classroom::recover(int sdaPin, int sclPin) {
  log(F("recover: resetting sensor"));
  int sda = sdaPin >= 0 ? sdaPin : _sda;
  int scl = sclPin >= 0 ? sclPin : _scl;
  if (sda >= 0 && scl >= 0) busClear(sda, scl);
  writeBits(R_PWR_MGMT_1, 7, 1, 1);          // chip reset (best effort)
  delay(100);

  MpuError e = initChip();                    // reload + verify firmware
  if (e != MPU_OK) return e;

  if (_offSaved) {                            // restore boot calibration
    for (int i = 0; i < 3; i++) {
      // accel low bytes carry a temperature-compensation bit: preserve it
      uint8_t lo = readReg(R_XA_OFFS_H + 2*i + 1) & 0x01;
      writeReg(R_XA_OFFS_H + 2*i,     (uint8_t)(_off[i] >> 8));
      writeReg(R_XA_OFFS_H + 2*i + 1, (uint8_t)((_off[i] & 0xFE) | lo));
      writeReg(R_XG_OFFS_H + 2*i,     (uint8_t)(_off[3+i] >> 8));
      writeReg(R_XG_OFFS_H + 2*i + 1, (uint8_t)(_off[3+i] & 0xFF));
    }
  }
  return start();
}

// ---------------- hardware task watchdog (ESP32) ----------------
#if defined(ESP32)
#include <esp_task_wdt.h>
#include <esp_arduino_version.h>

bool MPU6050Classroom::watchdogEnable(uint32_t seconds) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_deinit();                      // drop any default config
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = seconds * 1000;
  cfg.idle_core_mask = 0;                     // watch only subscribed tasks
  cfg.trigger_panic = true;                   // panic -> reboot
  if (esp_task_wdt_init(&cfg) != ESP_OK) return false;
#else
  if (esp_task_wdt_init(seconds, true) != ESP_OK) return false;
#endif
  return esp_task_wdt_add(NULL) == ESP_OK;    // subscribe the loop task
}
void MPU6050Classroom::watchdogPet()     { esp_task_wdt_reset(); }
void MPU6050Classroom::watchdogDisable() { esp_task_wdt_delete(NULL); }
#else
// Non-ESP32 build: no hardware watchdog available through this library.
bool MPU6050Classroom::watchdogEnable(uint32_t) { return false; }
void MPU6050Classroom::watchdogPet()            {}
void MPU6050Classroom::watchdogDisable()        {}
#endif

// ---------------- FIFO ----------------
uint16_t MPU6050Classroom::fifoCount() {
  uint8_t b[2];
  if (!readRegs(R_FIFO_COUNTH, b, 2)) return 0;
  return ((uint16_t)b[0] << 8) | b[1];
}

void MPU6050Classroom::resetFifo() {
  writeBits(R_USER_CTRL, 2, 1, 1);
}

bool MPU6050Classroom::packetAvailable() {
  return fifoCount() >= _packetSize;
}

bool MPU6050Classroom::readPacket(uint8_t* out) {
  if (_repairing) return false;
  uint32_t now = millis();

  // With a working INT pin, skip the I2C query until the sensor signals a
  // new packet. A poll every 20 ms still runs, so a missed signal costs
  // at most 20 ms and never stops the data.
  if (_intActive && _intPending == 0 && (now - _lastPollMs) < 20) return false;
  _intPending = 0;
  _lastPollMs = now;

  uint16_t count = fifoCount();
  bool got = false;
  if (count >= 1024 || (count >= _packetSize && (count % _packetSize) != 0)) {
    resetFifo();                                      // overflow or torn packet
    log(F("FIFO reset, count was: "), count);
    if (count >= 1024) {
      _overflows++;
      if (_report && (_lastOverflowWarn == 0 || now - _lastOverflowWarn > 10000)) {
        _lastOverflowWarn = now;
        _report->println(F("MPU6050Classroom: warning - motion data was lost."));
        _report->println(F("  The sensor's memory filled up because loop() waited too long"));
        _report->println(F("  between reads (for example a long delay()). See the 4_SlowLoop example."));
      }
    }
  } else if (count >= _packetSize) {
    bool ok = true;
    while (ok && count > _packetSize) {               // drain stale packets
      uint8_t burn[MEM_CHUNK];
      uint16_t drop = count - _packetSize;
      uint8_t n = (uint8_t)min((uint16_t)MEM_CHUNK, drop);
      ok = readRegs(R_FIFO_R_W, burn, n);
      count -= n;
    }
    for (uint16_t off = 0; ok && off < _packetSize; off += MEM_CHUNK) {
      uint8_t n = (uint8_t)min((uint16_t)MEM_CHUNK, (uint16_t)(_packetSize - off));
      ok = readRegs(R_FIFO_R_W, out + off, n);       // the FIFO pops, so repeat reads continue
    }
    if (ok) {
      got = true;
      _packetsRead++;
    }
  }

  if (_running && _autoRepair) {
    TeachMath::FreezeState s = _fd.update(now, count > 0, got ? out : nullptr);
    if (s != TeachMath::FREEZE_NONE) { attemptRepair(s); return false; }
  }
  return got;
}

// ---------------- automatic freeze repair ----------------
void MPU6050Classroom::attemptRepair(uint8_t reason) {
  uint32_t now = millis();
  if (!_healthy && (int32_t)(now - _nextRepairTry) < 0) return;   // wait between tries

  if (reason == TeachMath::FREEZE_STUCK)
    say(F("MPU6050Classroom: the sensor data stopped changing for 5 s. Repairing..."));
  else
    say(F("MPU6050Classroom: the sensor stopped sending data for 1.5 s. Repairing..."));

  _repairing = true;
  MpuError e = recover();
  _repairing = false;

  if (e == MPU_OK) {
    _repairs++;
    _healthy = true;
    if (_report) {
      _report->print(F("MPU6050Classroom: repair worked (repair number "));
      _report->print(_repairs);
      _report->println(F("). If this happens often, check the wires."));
    }
  } else {
    _repairFails++;
    _healthy = false;
    _nextRepairTry = millis() + 2000;
    if (_report) {
      _report->print(F("MPU6050Classroom: repair failed ("));
      _report->print(errorName(e));
      _report->println(F("). Check VCC, GND, SDA and SCL. Trying again in 2 s."));
    }
  }
  _fd.reset(millis());
}

// ---------------- INT pin ----------------
MPU6050Classroom* MPU6050Classroom::_intOwner = nullptr;
void MPU6050Classroom::onInterruptNoArg() { if (_intOwner) onInterrupt(_intOwner); }

void IRAM_ATTR MPU6050Classroom::onInterrupt(void* arg) {
  MPU6050Classroom* m = (MPU6050Classroom*)arg;
  m->_intCount = m->_intCount + 1;
  m->_intPending = m->_intPending + 1;
#if defined(ESP32)
  TaskHandle_t task = m->_waitTask;
  if (task) {
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(task, &woken);
    if (woken == pdTRUE) portYIELD_FROM_ISR();
  }
#endif
}

bool MPU6050Classroom::useInterruptPin(int pin) {
  stopInterruptPin();
  int irq = digitalPinToInterrupt(pin);
  if (irq < 0) {
    if (_report) {
      _report->print(F("MPU6050Classroom: pin "));
      _report->print(pin);
      _report->println(F(" cannot receive interrupts on this board."));
      _report->println(F("  Check your board's pin diagram for interrupt pins. Continuing without the INT pin."));
    }
    return false;
  }
  _intPin = pin;
  _intCount = 0;
  _intPending = 0;
  pinMode(pin, INPUT_PULLUP);    // keeps the line quiet if the wire is missing
#if defined(ESP32)
  attachInterruptArg(irq, onInterrupt, this, FALLING);
#else
  _intOwner = this;              // one sensor with INT per sketch on these boards
  attachInterrupt(irq, onInterruptNoArg, FALLING);
#endif
  if (!_running) return true;    // checked when start() runs
  return checkInterrupt(true);
}

void MPU6050Classroom::stopInterruptPin() {
#if defined(ESP32)
  if (_intPin >= 0) detachInterrupt(digitalPinToInterrupt(_intPin));
#else
  if (_intPin >= 0) { detachInterrupt(digitalPinToInterrupt(_intPin)); _intOwner = nullptr; }
#endif
  _intPin = -1;
  _intActive = false;
}

// Listen for 100 ms. The DMP sends one signal per packet, so a working
// wire delivers many signals in that time.
bool MPU6050Classroom::checkInterrupt(bool announce) {
  uint32_t c0 = _intCount;
  uint32_t t0 = millis();
  while (millis() - t0 < 100) delay(1);
  uint32_t seen = _intCount - c0;
  _intActive = (seen >= 2);
  if (_report) {
    if (_intActive && announce) {
      _report->print(F("MPU6050Classroom: INT pin on GPIO"));
      _report->print(_intPin);
      _report->println(F(" is working. Fast response is ON."));
    } else if (!_intActive) {
      _report->print(F("MPU6050Classroom: no signals on the INT pin (GPIO"));
      _report->print(_intPin);
      _report->println(F(") in 100 ms."));
      _report->println(F("  Check the INT wire. Continuing without the INT pin."));
    }
  }
  log(F("INT signals in 100 ms: "), (int)seen);
  return _intActive;
}

bool MPU6050Classroom::waitForInterrupt(uint32_t timeoutMs) {
#if defined(ESP32)
  if (!_intActive) { delay(5); return true; }   // no INT wire: plain polling
  _waitTask = xTaskGetCurrentTaskHandle();
  if (_intPending > 0) return true;
  return ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeoutMs)) > 0;
#else
  (void)timeoutMs;
  delay(5);
  return true;
#endif
}

// ---------------- easy mode ----------------
static void explainError(Stream& out, MpuError e, int sda, int scl) {
  switch (e) {
    case MPU_ERR_NO_DEVICE:
      out.println(F("Problem: no sensor answers on the I2C wires."));
      out.print(F("  This sketch uses SDA = pin ")); out.print(sda);
      out.print(F(" and SCL = pin ")); out.print(scl); out.println(F("."));
      out.println(F("  Check these against your board's pin diagram, then check"));
      out.println(F("  VCC to 3.3 V, GND to GND, and that every wire is pushed in."));
      break;
    case MPU_ERR_WRONG_CHIP:
      out.println(F("Problem: the chip is not a genuine MPU-6050 (run the 1a_WhoAmI example)."));
      out.println(F("  Show your teacher; this board may carry a different sensor."));
      break;
    case MPU_ERR_FW_WRITE:
    case MPU_ERR_FW_VERIFY:
      out.println(F("Problem: the motion firmware did not load correctly."));
      out.println(F("  Check that the wires are short and firmly pushed in."));
      break;
    default:
      out.println(F("Problem: the I2C wires gave an error."));
      out.println(F("  Check the wires and the 3.3 V power."));
      break;
  }
}

void MPU6050Classroom::easyStart(Stream& out, int sdaPin, int sclPin, DmpVersion v) {
  delay(1500);                                   // time to open the Serial Monitor
  reportTo(out);
#if defined(ESP32)
  _wire->begin(sdaPin, sclPin);                  // ESP32: any two free pins can be I2C
#else
  // UNO R3 / R4: the I2C pins are fixed in hardware.
  if (sdaPin != (int)SDA || sclPin != (int)SCL) {
    out.println(F("Problem: SENSOR_SDA / SENSOR_SCL do not match this board's I2C pins."));
    out.println(F("  On this board the I2C pins are fixed: use A4 for SDA and A5 for SCL."));
    out.println(F("  Fix the two pin lines at the top of the sketch and upload again."));
    for (;;) delay(1000);
  }
  _wire->begin();
#endif
  _wire->setClock(400000);

  out.println(F("Starting the MPU-6050..."));
  for (;;) {
    MpuError e = begin(v, sdaPin, sclPin, false);
    if (e == MPU_OK) break;
    explainError(out, e, sdaPin, sclPin);
    out.println(F("  Trying again in 3 s..."));
    delay(3000);
    busClear(sdaPin, sclPin);
  }
  out.print(F("Firmware loaded: ")); out.println(firmwareName());
  out.println(F("Calibrating: keep the board flat and still (about 3 s)..."));
  calibrate();
  start();
  if (!measureRest(500))
    out.println(F("Warning: no data while measuring 1 g at rest. Shock values will read 0."));
  out.println(F("Ready."));
  out.println();
}

bool MPU6050Classroom::measureRest(uint32_t ms) {
  uint8_t buf[42];
  float sum = 0; uint32_t n = 0;
  uint32_t t0 = millis();
  bool v612 = (_version == DmpVersion::V612);
  while (millis() - t0 < ms) {
    if (readPacket(buf)) { sum += TeachMath::accelMagnitude(buf, v612); n++; }
    delay(1);
  }
  if (n == 0) return false;
  _restMag = sum / n;
  log(F("rest magnitude (1 g) in raw units: "), (int)_restMag);
  return true;
}

bool MPU6050Classroom::update() {
  uint8_t buf[42];
  if (!readPacket(buf)) return false;
  TeachMath::poseFromPacket(buf, _pose);
  _gForce = TeachMath::gForce(
      TeachMath::accelMagnitude(buf, _version == DmpVersion::V612), _restMag);
  return true;
}
