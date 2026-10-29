// 3_CrashLogger — remembers the last 20 crashes, even after a restart.
//
// Each shock above SHOCK_LIMIT_G is saved in the board's own memory
// (EEPROM, no memory card needed): which start-up it happened in, the time
// since that start-up, the shock and the tilt. Type in the Serial Monitor:
//   l  then Enter: list the saved crashes
//   e  then Enter: erase them
// When all 20 places are full, the oldest crash is replaced.
//
// Limits: the board has no clock, so the time is "seconds since start-up",
// not the time of day. The memory survives a restart and a new upload of
// this sketch, but a different sketch may overwrite it. EEPROM wears out
// after about 100 000 writes per place, so this is fine for crashes but not
// for saving data many times a second.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
#if defined(ESP32)
#error "This example is for Arduino UNO R3/R4. Open the same example in the ESP32 folder."
#endif
#include <MPU6050Classroom.h>
#include <EEPROM.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;
const float SHOCK_LIMIT_G = 0.8;
const int   MAX_CRASHES = 20;
const int   LOG_ADDRESS = 512;      // after the Wi-Fi settings, so both can live on one board

struct Crash {                      // 8 bytes per crash
  uint16_t startup;                 // which start-up of the board
  uint16_t seconds;                 // seconds since that start-up
  uint16_t shockCentiG;             // shock in hundredths of a g
  int8_t   pitchDeg, rollDeg;
};
struct LogHeader {
  uint32_t magic;                   // marks a valid log
  uint16_t startups;
  uint8_t  next, count;             // where the next crash goes; how many are saved
};
const uint32_t MAGIC = 0x4352534C;  // "CRSL"

MPU6050Classroom mpu;
LogHeader hdr;

void saveMemory() {
#if defined(ESP32)
  EEPROM.commit();                  // ESP32: write the changes to flash
#endif
}

void eraseLog() {
  hdr.magic = MAGIC; hdr.next = 0; hdr.count = 0;
  EEPROM.put(LOG_ADDRESS, hdr);
  saveMemory();
  Serial.println(F("Crash log erased."));
}

void listLog() {
  if (hdr.count == 0) { Serial.println(F("No crashes saved.")); return; }
  Serial.println(F("start-up  seconds  shock_g  pitch_deg  roll_deg"));
  int first = (hdr.next - hdr.count + MAX_CRASHES) % MAX_CRASHES;   // oldest first
  for (int i = 0; i < hdr.count; i++) {
    Crash c;
    EEPROM.get(LOG_ADDRESS + (int)sizeof(LogHeader) + ((first + i) % MAX_CRASHES) * (int)sizeof(Crash), c);
    char line[60];
    snprintf(line, sizeof line, "%8u  %7u  %3u.%02u  %9d  %8d",
             c.startup, c.seconds, c.shockCentiG / 100, c.shockCentiG % 100, c.pitchDeg, c.rollDeg);
    Serial.println(line);
  }
}

void addCrash(float shock) {
  Crash c;
  c.startup = hdr.startups;
  c.seconds = (uint16_t)min(millis() / 1000UL, 65535UL);
  c.shockCentiG = (uint16_t)(shock * 100 + 0.5f);
  c.pitchDeg = (int8_t)constrain((int)mpu.pitch(), -127, 127);
  c.rollDeg  = (int8_t)constrain((int)mpu.roll(), -127, 127);
  EEPROM.put(LOG_ADDRESS + (int)sizeof(LogHeader) + hdr.next * (int)sizeof(Crash), c);
  hdr.next = (hdr.next + 1) % MAX_CRASHES;
  if (hdr.count < MAX_CRASHES) hdr.count++;
  EEPROM.put(LOG_ADDRESS, hdr);
  saveMemory();
}

void setup() {
  Serial.begin(115200);
#if defined(ESP32)
  EEPROM.begin(1024);               // ESP32: the EEPROM is a part of the flash memory
#endif
  EEPROM.get(LOG_ADDRESS, hdr);
  if (hdr.magic != MAGIC) { hdr.startups = 0; eraseLog(); }
  hdr.startups++;
  EEPROM.put(LOG_ADDRESS, hdr);
  saveMemory();

  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  Serial.print(F("Crash logger ready. This is start-up ")); Serial.print(hdr.startups);
  Serial.print(F("; ")); Serial.print(hdr.count); Serial.println(F(" crashes saved."));
  Serial.println(F("Type l to list them, e to erase them."));
}

void loop() {
  if (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == 'l' || ch == 'L') listLog();
    if (ch == 'e' || ch == 'E') {
      if (MPU6050Classroom::askYesNo(Serial, "Erase all saved crashes?")) eraseLog();
    }
  }

  static uint32_t lastEvent = 0;
  if (!mpu.update()) return;
  float shock = mpu.shock();
  if (shock <= SHOCK_LIMIT_G || millis() - lastEvent < 1000) return;   // ignore bounces for 1 s
  lastEvent = millis();
  addCrash(shock);
  Serial.print(F("CRASH: shock ")); Serial.print(shock, 2);
  Serial.print(F(" g, saved (")); Serial.print(hdr.count); Serial.println(F(" in the log)"));
}
