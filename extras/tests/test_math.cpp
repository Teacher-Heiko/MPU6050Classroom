// Host-side tests for TeachMath.h (the parts of the library that are pure
// maths and need no hardware). Build and run:
//   g++ -std=c++17 -I ../../src test_math.cpp -o test_math && ./test_math
//
// Reports every check it ran, PASS or FAIL, and total coverage. A check that
// cannot run counts as FAIL, never as a silent skip.
#include "TeachMath.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static int passed = 0, failed = 0;
#define CHECK(name, cond) do { \
  if (cond) { passed++; printf("PASS  %s\n", name); } \
  else      { failed++; printf("FAIL  %s\n", name); } \
} while (0)

static bool near(float a, float b, float tol = 0.2f) { return fabsf(a - b) <= tol; }

// Pack a quaternion (floats) into the first 16 bytes of a fake FIFO packet
// the way the DMP does: Q14 int16 in the top two bytes of each 32-bit slot.
static void packQuat(uint8_t* p, float w, float x, float y, float z) {
  float q[4] = {w, x, y, z};
  memset(p, 0, 16);
  for (int i = 0; i < 4; i++) {
    int16_t v = (int16_t)lrintf(q[i] * 16384.0f);
    p[i*4]     = (uint8_t)(v >> 8);
    p[i*4 + 1] = (uint8_t)(v & 0xFF);
  }
}

int main() {
  using namespace TeachMath;
  uint8_t pkt[42];
  Pose pose;

  // 1. be16: sign handling
  { uint8_t b[2] = {0xFF, 0xFE}; CHECK("be16 negative (-2)", be16(b) == -2); }
  { uint8_t b[2] = {0x40, 0x00}; CHECK("be16 positive (16384)", be16(b) == 16384); }

  // 2. identity quaternion -> level pose, gravity straight down body Z
  packQuat(pkt, 1, 0, 0, 0);
  poseFromPacket(pkt, pose);
  CHECK("identity: yaw 0",   near(pose.yawDeg,   0));
  CHECK("identity: pitch 0", near(pose.pitchDeg, 0));
  CHECK("identity: roll 0",  near(pose.rollDeg,  0));
  CHECK("identity: gravity z = 1", near(pose.gz, 1.0f, 0.01f));

  // 3. 90-degree yaw about Z: q = (cos45, 0, 0, sin45)
  packQuat(pkt, 0.70710678f, 0, 0, 0.70710678f);
  poseFromPacket(pkt, pose);
  CHECK("yaw 90: yaw = -90 (DMP sign convention)", near(fabsf(pose.yawDeg), 90, 0.5f));
  CHECK("yaw 90: pitch stays 0", near(pose.pitchDeg, 0, 0.5f));

  // 4. 90-degree roll about X: q = (cos45, sin45, 0, 0)
  packQuat(pkt, 0.70710678f, 0.70710678f, 0, 0);
  poseFromPacket(pkt, pose);
  CHECK("roll 90: |roll| = 90", near(fabsf(pose.rollDeg), 90, 0.5f));
  CHECK("roll 90: gravity moves to Y axis", near(fabsf(pose.gy), 1.0f, 0.01f));

  // 5. 90-degree pitch about Y: q = (cos45, 0, sin45, 0)
  packQuat(pkt, 0.70710678f, 0, 0.70710678f, 0);
  poseFromPacket(pkt, pose);
  CHECK("pitch 90: |pitch| = 90", near(fabsf(pose.pitchDeg), 90, 1.0f));

  // 6. upside-down (180-degree roll): pitch correction branch (gz < 0)
  packQuat(pkt, 0, 1, 0, 0);
  poseFromPacket(pkt, pose);
  CHECK("upside-down: gravity z = -1", near(pose.gz, -1.0f, 0.01f));
  CHECK("upside-down: |roll| = 180", near(fabsf(pose.rollDeg), 180, 1.0f));

  // 7. non-normalised input gets normalised
  packQuat(pkt, 0.5f, 0, 0, 0);   // |q| = 0.5, still "identity" direction
  poseFromPacket(pkt, pose);
  float n = sqrtf(pose.qw*pose.qw + pose.qx*pose.qx + pose.qy*pose.qy + pose.qz*pose.qz);
  CHECK("quaternion normalised to length 1", near(n, 1.0f, 0.001f));

  // 8. accel/gyro parsing offsets per firmware version
  memset(pkt, 0, sizeof(pkt));
  pkt[16]=0x01; pkt[17]=0x02;  // v612 accel X = 0x0102 ; v20 gyro X = 0x0102
  pkt[28]=0x03; pkt[29]=0x04;  // v20 accel X = 0x0304
  pkt[22]=0x05; pkt[23]=0x06;  // v612 gyro X = 0x0506
  int16_t a[3], g[3];
  parseAccel(pkt, true, a);  CHECK("v6.12 accel X at byte 16", a[0] == 0x0102);
  parseAccel(pkt, false, a); CHECK("v2.0  accel X at byte 28", a[0] == 0x0304);
  parseGyro(pkt, true, g);   CHECK("v6.12 gyro X at byte 22",  g[0] == 0x0506);
  parseGyro(pkt, false, g);  CHECK("v2.0  gyro X at byte 16",  g[0] == 0x0102);

  // 9. teapot packet framing
  uint8_t fifo[16]; for (int i = 0; i < 16; i++) fifo[i] = (uint8_t)i;
  uint8_t tp[14];
  buildTeapotPacket(fifo, 7, tp);
  CHECK("teapot: header $ 0x02", tp[0]=='$' && tp[1]==0x02);
  CHECK("teapot: quat bytes 0,1,4,5,8,9,12,13",
        tp[2]==0 && tp[3]==1 && tp[4]==4 && tp[5]==5 &&
        tp[6]==8 && tp[7]==9 && tp[8]==12 && tp[9]==13);
  CHECK("teapot: counter and CRLF", tp[11]==7 && tp[12]=='\r' && tp[13]=='\n');


  // 10. freeze detector
  {
    FreezeDetector fd; fd.reset(0);
    uint8_t qa[16]; memset(qa, 1, 16);
    uint8_t qb[16]; memset(qb, 2, 16);
    // healthy: new packet every 5 ms for 3 s, alternating values
    bool anyFreeze = false;
    for (uint32_t t = 5; t <= 3000; t += 5)
      if (fd.update(t, true, (t / 5) % 2 ? qa : qb) != FREEZE_NONE) anyFreeze = true;
    CHECK("freeze: healthy stream never triggers", !anyFreeze);

    // FIFO empty: triggers only after more than 1500 ms
    FreezeState s1400 = fd.update(3000 + 1400, false, nullptr);
    FreezeState s1600 = fd.update(3000 + 1600, false, nullptr);
    CHECK("freeze: empty FIFO for 1400 ms is not yet a freeze", s1400 == FREEZE_NONE);
    CHECK("freeze: empty FIFO for 1600 ms is NO_DATA", s1600 == FREEZE_NO_DATA);
  }
  {
    // slow sketch: one call every 2000 ms, FIFO overflowed each time, no packet read
    FreezeDetector fd; fd.reset(0);
    bool anyFreeze = false;
    for (uint32_t t = 2000; t <= 20000; t += 2000)
      if (fd.update(t, true, nullptr) != FREEZE_NONE) anyFreeze = true;
    CHECK("freeze: slow sketch with overflowing FIFO never triggers", !anyFreeze);
  }
  {
    // slow sketch AND dead sensor: first call after the gap sees an empty FIFO
    FreezeDetector fd; fd.reset(0);
    CHECK("freeze: slow sketch + empty FIFO after 2 s is NO_DATA",
          fd.update(2000, false, nullptr) == FREEZE_NO_DATA);
  }
  {
    // stuck: identical quaternion for more than 5000 ms
    FreezeDetector fd; fd.reset(0);
    uint8_t qa[16]; memset(qa, 7, 16);
    FreezeState s = FREEZE_NONE; uint32_t firstStuck = 0;
    for (uint32_t t = 5; t <= 6000 && s == FREEZE_NONE; t += 5) {
      s = fd.update(t, true, qa); if (s != FREEZE_NONE) firstStuck = t;
    }
    CHECK("freeze: identical quaternion becomes STUCK", s == FREEZE_STUCK);
    CHECK("freeze: STUCK only after more than 5000 ms", firstStuck > 5000);
  }
  {
    // stuck is not judged when no packets are read (slow sketch)
    FreezeDetector fd; fd.reset(0);
    uint8_t qa[16]; memset(qa, 7, 16);
    fd.update(10, true, qa);
    CHECK("freeze: no packets read for 8 s is not STUCK while FIFO has data",
          fd.update(8000, true, nullptr) == FREEZE_NONE);
  }


  // 11. shock maths (sudden movement)
  {
    uint8_t pk[28]; memset(pk, 0, sizeof pk);
    auto setA = [&](int16_t x, int16_t y, int16_t z) {
      int16_t v[3] = {x, y, z};
      for (int i = 0; i < 3; i++) { pk[16+2*i] = (uint8_t)(v[i] >> 8); pk[17+2*i] = (uint8_t)(v[i] & 0xFF); }
    };
    setA(0, 0, 8192);
    float rest = accelMagnitude(pk, true);
    CHECK("shock: magnitude at rest equals Z reading", near(rest, 8192.0f, 0.5f));
    CHECK("shock: at rest is 0 g", near(shockG(rest, rest), 0.0f, 0.001f));
    setA(0, 0, 16384);
    CHECK("shock: twice gravity is 1 g", near(shockG(accelMagnitude(pk, true), rest), 1.0f, 0.001f));
    setA(0, 0, 0);
    CHECK("shock: free fall is 1 g", near(shockG(accelMagnitude(pk, true), rest), 1.0f, 0.001f));
    setA(8192, 0, 0);
    CHECK("shock: board on its side (still) is 0 g", near(shockG(accelMagnitude(pk, true), rest), 0.0f, 0.001f));
    CHECK("shock: zero rest value gives 0, not a crash", shockG(100.0f, 0.0f) == 0.0f);
  }


  // 12. fall detector
  {
    FallDetector fd;
    bool hit = false;
    uint32_t t = 0;
    for (; t < 500; t += 5)  hit |= fd.update(t, 1.0f);     // still
    for (; t < 750; t += 5)  hit |= fd.update(t, 0.05f);    // 250 ms free fall
    bool before = hit;
    for (; t < 850; t += 5)  hit |= fd.update(t, 1.0f);     // brief normal reading
    hit |= fd.update(t, 2.2f); t += 5;                      // landing
    CHECK("fall: nothing reported before landing", !before);
    CHECK("fall: free fall then impact is a fall", hit);

    FallDetector fd2; bool hit2 = false; t = 0;
    for (; t < 30; t += 5) hit2 |= fd2.update(t, 0.1f);     // 30 ms dip only
    hit2 |= fd2.update(t, 2.2f);
    CHECK("fall: a 30 ms dip then a spike is not a fall", !hit2);

    FallDetector fd3; bool hit3 = false; t = 0;
    for (; t < 300; t += 5)  hit3 |= fd3.update(t, 0.1f);   // fall...
    for (; t < 2000; t += 5) hit3 |= fd3.update(t, 1.0f);   // ...caught, no impact
    hit3 |= fd3.update(t, 2.2f);                            // late spike
    CHECK("fall: impact more than 1 s later is not a fall", !hit3);

    FallDetector fd4; t = 0;
    for (; t < 300; t += 5) fd4.update(t, 0.1f);
    CHECK("fall: landing reading straight after free fall counts", fd4.update(t, 2.5f));

    FallDetector fd5; bool hit5 = false; t = 0;
    for (; t < 3000; t += 5) hit5 |= fd5.update(t, (t / 5) % 2 ? 1.9f : 1.0f);
    CHECK("fall: shaking without free fall is not a fall", !hit5);
  }

  // 13. tap counter
  {
    TapCounter tc; uint8_t got = 0; uint32_t t = 0;
    auto feed = [&](uint32_t until, float s) { for (; t < until; t += 5) { uint8_t c = tc.update(t, s); if (c) got = c; } };
    feed(100, 0.0f);
    feed(110, 0.9f);  feed(400, 0.0f);   // tap 1
    feed(410, 0.9f);  feed(700, 0.0f);   // tap 2
    feed(710, 0.9f);  feed(2000, 0.0f);  // tap 3, then quiet
    CHECK("tap: three knocks give a group of 3", got == 3);

    TapCounter tc2; got = 0; t = 0;
    uint8_t g2 = 0;
    for (; t < 60; t += 5) { uint8_t c = tc2.update(t, 0.9f); if (c) g2 = c; }   // one long knock (ringing)
    for (; t < 2000; t += 5) { uint8_t c = tc2.update(t, 0.0f); if (c) g2 = c; }
    CHECK("tap: ringing within 120 ms counts as one tap", g2 == 1);
  }

  // 14. gestures
  CHECK("gesture: small tilt is FLAT",    gestureFromTilt(5, -8)  == GESTURE_FLAT);
  CHECK("gesture: pitch +40 is FORWARD",  gestureFromTilt(40, 10) == GESTURE_FORWARD);
  CHECK("gesture: pitch -40 is BACK",     gestureFromTilt(-40, 5) == GESTURE_BACK);
  CHECK("gesture: roll +40 is RIGHT",     gestureFromTilt(10, 40) == GESTURE_RIGHT);
  CHECK("gesture: roll -40 is LEFT",      gestureFromTilt(-5, -40)== GESTURE_LEFT);

  // 15. level bar
  {
    char b[16];
    levelBar(0, 10, b, 11);   CHECK("bar: 0 deg puts * in the centre", strcmp(b, "[-----*-----]") == 0);
    levelBar(10, 10, b, 11);  CHECK("bar: +range puts * at the right end", strcmp(b, "[-----|----*]") == 0);
    levelBar(-50, 10, b, 11); CHECK("bar: beyond range is clamped left", strcmp(b, "[*----|-----]") == 0);
  }

  // 16. PID
  {
    Pid pid; pid.kp = 2; pid.ki = 0; pid.kd = 0;
    CHECK("pid: P only gives kp*error", near(pid.step(10, 0.01f), 20.0f, 0.001f));
    Pid pi; pi.kp = 0; pi.ki = 1;
    pi.step(1, 0.5f);
    CHECK("pid: I sums error*dt", near(pi.step(1, 0.5f), 1.0f, 0.001f));
    Pid pd; pd.kd = 1;
    pd.step(0, 0.1f);
    CHECK("pid: D is change of error / dt", near(pd.step(1, 0.1f), 10.0f, 0.001f));
    Pid sat; sat.kp = 1000;
    CHECK("pid: output is clamped to +255", sat.step(10, 0.01f) == 255.0f);
    Pid wind; wind.kp = 0; wind.ki = 100; wind.outMax = 10;
    for (int i = 0; i < 100; i++) wind.step(1, 0.1f);
    CHECK("pid: integral stops growing while saturated", wind.integral < 1.0f);
  }


  // 17. angles
  CHECK("angle: 350 - 10 wraps to -20",  near(angleDiff(350, 10), -20, 0.01f));
  CHECK("angle: -170 - 170 wraps to 20", near(angleDiff(-170, 170), 20, 0.01f));
  CHECK("angle: 90 - 0 is 90",           near(angleDiff(90, 0), 90, 0.01f));
  {
    Pose flat{}; flat.gz = 1;   Pose side{}; side.gz = 0;   Pose up{}; up.gz = -1;
    CHECK("tilt: flat is 0 deg",         near(tiltDeg(flat), 0, 0.01f));
    CHECK("tilt: on its side is 90 deg", near(tiltDeg(side), 90, 0.01f));
    CHECK("tilt: upside down is 180 deg",near(tiltDeg(up), 180, 0.01f));
  }
  {
    // 90 degrees about Z in 0.5 s = 180 deg/s
    Pose a{}; a.qw = 1;
    Pose b{}; b.qw = cosf((float)M_PI/4); b.qz = sinf((float)M_PI/4);
    CHECK("speed: 90 deg in 0.5 s is 180 deg/s", near(angularSpeedDps(a, b, 0.5f), 180, 0.5f));
    Pose c = b; c.qw = -b.qw; c.qz = -b.qz;   // same rotation, opposite sign
    CHECK("speed: q and -q give the same speed", near(angularSpeedDps(a, c, 0.5f), 180, 0.5f));
    CHECK("speed: no turn is 0 deg/s", near(angularSpeedDps(a, a, 0.01f), 0, 0.5f));
  }

  // 18. threshold switch
  {
    ThresholdSwitch s; s.onAbove = 10; s.offBelow = 5;
    bool c1 = s.update(11);
    bool c2 = s.update(7);
    bool c3 = s.update(4);
    CHECK("switch: turns on above 10", c1);
    CHECK("switch: stays on at 7 (between limits)", !c2);
    CHECK("switch: turns off below 5", c3 && !s.on);
  }

  // 19. step counter: 2 steps per second for 5 s
  {
    StepCounter sc; uint32_t t = 0;
    for (; t < 5000; t += 10) {
      uint32_t ph = t % 500;
      float g = ph < 100 ? 1.4f : (ph < 250 ? 0.8f : 1.0f);
      sc.update(t, g);
    }
    CHECK("steps: 2 per second for 5 s counts 10", sc.steps == 10);
    StepCounter still; for (t = 0; t < 5000; t += 10) still.update(t, 1.02f);
    CHECK("steps: standing still counts 0", still.steps == 0);
  }

  // 20. shake detector
  {
    ShakeDetector sd; uint32_t t = 0; bool shake = false;
    for (; t < 1000; t += 10) shake |= sd.update(t, (t % 200) < 20 ? 1.0f : 0.1f);  // 5 jolts
    for (; t < 2000; t += 10) shake |= sd.update(t, 0.0f);
    CHECK("shake: 5 jolts then quiet is a shake", shake);
    ShakeDetector one; bool s1 = false;
    for (t = 0; t < 30; t += 10) s1 |= one.update(t, 1.0f);   // one knock
    for (; t < 2000; t += 10) s1 |= one.update(t, 0.0f);
    CHECK("shake: one knock is not a shake", !s1);
  }

  // 21. jump timer
  {
    JumpTimer jt; uint32_t t = 0, air = 0;
    for (; t < 200; t += 5) jt.update(t, 1.0f);
    for (; t < 500; t += 5) jt.update(t, 0.1f);      // 300 ms in the air
    air = jt.update(t, 2.0f);
    CHECK("jump: 300 ms airtime measured", air >= 295 && air <= 305);
    JumpTimer bump; uint32_t a2 = 0;
    for (t = 0; t < 40; t += 5) bump.update(t, 0.1f);
    a2 = bump.update(t, 1.0f);
    CHECK("jump: 40 ms dip is a bump, not a jump", a2 == 0);
  }

  // 22. shake level
  {
    ShakeLevel sl; for (int i = 0; i < 500; i++) sl.update(0.5f);
    CHECK("shake level: steady 0.5 g settles at 0.5 g", near(sl.level, 0.5f, 0.01f));
  }

  printf("\n%d checks run: %d passed, %d failed\n", passed + failed, passed, failed);
  return failed == 0 ? 0 : 1;
}
