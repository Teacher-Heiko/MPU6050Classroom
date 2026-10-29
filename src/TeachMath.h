// TeachMath.h — pure maths and packet parsing for MPU6050Classroom.
// No Arduino dependencies, so the same code runs in the host-side tests.
#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>

namespace TeachMath {

// ---------- DMP FIFO packet layouts (verified against library source) ----------
// Both versions: quaternion W,X,Y,Z as 32-bit big-endian starting at byte 0.
// The useful 16-bit value is the top two bytes of each 32-bit field.
//   v2.0  packet = 42 bytes: quat 0..15, gyro16 at 16/20/24, accel16 at 28/32/36
//   v6.12 packet = 28 bytes: quat 0..15, accel16 at 16/18/20, gyro16 at 22/24/26

struct Pose {
  float qw, qx, qy, qz;   // unit quaternion
  float gx, gy, gz;       // gravity direction, unit vector
  float yawDeg, pitchDeg, rollDeg;
};

inline int16_t be16(const uint8_t* p) {
  return (int16_t)(((uint16_t)p[0] << 8) | p[1]);
}

// Raw 16-bit quaternion, scale 1.0f == 16384 (Q14), both firmware versions.
inline void parseQuatRaw(const uint8_t* packet, int16_t q[4]) {
  q[0] = be16(packet + 0);
  q[1] = be16(packet + 4);
  q[2] = be16(packet + 8);
  q[3] = be16(packet + 12);
}

inline void parseAccel(const uint8_t* packet, bool v612, int16_t a[3]) {
  if (v612) { a[0]=be16(packet+16); a[1]=be16(packet+18); a[2]=be16(packet+20); }
  else      { a[0]=be16(packet+28); a[1]=be16(packet+32); a[2]=be16(packet+36); }
}

inline void parseGyro(const uint8_t* packet, bool v612, int16_t g[3]) {
  if (v612) { g[0]=be16(packet+22); g[1]=be16(packet+24); g[2]=be16(packet+26); }
  else      { g[0]=be16(packet+16); g[1]=be16(packet+20); g[2]=be16(packet+24); }
}

// Quaternion -> gravity direction (body frame), then yaw/pitch/roll in degrees.
// Pitch correction for |pitch| > 90 degrees follows the v6.12 reference maths.
inline void poseFromPacket(const uint8_t* packet, Pose& out) {
  int16_t qi[4];
  parseQuatRaw(packet, qi);
  float w = qi[0] / 16384.0f, x = qi[1] / 16384.0f;
  float y = qi[2] / 16384.0f, z = qi[3] / 16384.0f;
  float n = sqrtf(w*w + x*x + y*y + z*z);
  if (n > 0.0f) { w/=n; x/=n; y/=n; z/=n; }
  out.qw=w; out.qx=x; out.qy=y; out.qz=z;

  out.gx = 2.0f * (x*z - w*y);
  out.gy = 2.0f * (w*x + y*z);
  out.gz = w*w - x*x - y*y + z*z;

  float yaw   = atan2f(2.0f*x*y - 2.0f*w*z, 2.0f*w*w + 2.0f*x*x - 1.0f);
  float pitch = atan2f(out.gx, sqrtf(out.gy*out.gy + out.gz*out.gz));
  float roll  = atan2f(out.gy, out.gz);
  if (out.gz < 0.0f) pitch = (pitch > 0.0f ? (float)M_PI - pitch : -(float)M_PI - pitch);

  const float R2D = 180.0f / (float)M_PI;
  out.yawDeg = yaw * R2D; out.pitchDeg = pitch * R2D; out.rollDeg = roll * R2D;
}

// InvenSense teapot packet: '$' 0x02 q0h q0l q1h q1l q2h q2l q3h q3l 0 count '\r' '\n'
// The quaternion bytes sit at the same offsets in both firmware versions.
inline void buildTeapotPacket(const uint8_t* fifo, uint8_t count, uint8_t out[14]) {
  out[0]='$'; out[1]=0x02;
  out[2]=fifo[0];  out[3]=fifo[1];
  out[4]=fifo[4];  out[5]=fifo[5];
  out[6]=fifo[8];  out[7]=fifo[9];
  out[8]=fifo[12]; out[9]=fifo[13];
  out[10]=0x00; out[11]=count; out[12]='\r'; out[13]='\n';
}

// ---------- Sudden movement (shock) ----------
// Length of the acceleration vector in the packet's own units. At rest it
// equals gravity (1 g) whichever way the board is turned.
inline float accelMagnitude(const uint8_t* packet, bool v612) {
  int16_t a[3];
  parseAccel(packet, v612, a);
  return sqrtf((float)a[0]*a[0] + (float)a[1]*a[1] + (float)a[2]*a[2]);
}

// How far the reading is from rest, in g. restMag is the magnitude measured
// while the board was still (= 1 g). A hit pushes it above 0; free fall
// (reading 0 g) also gives 1.0, because 0 g is 1 g away from rest.
inline float shockG(float mag, float restMag) {
  if (restMag <= 0.0f) return 0.0f;
  return fabsf(mag / restMag - 1.0f);
}


// ---------- Total acceleration in g ----------
// 1.0 at rest, 0.0 in free fall, above 1.0 during a hit.
inline float gForce(float mag, float restMag) {
  return restMag > 0.0f ? mag / restMag : 0.0f;
}

// ---------- Fall detector (free fall, then impact) ----------
// A dropped object first reads close to 0 g (free fall), then a spike
// when it lands. Both must happen, in that order, to count as a fall.
struct FallDetector {
  float    freeFallBelowG = 0.4f;   // "falling" when total acceleration is below this
  uint32_t freeFallMinMs  = 60;     // ... for at least this long (about 2 cm of drop)
  float    impactAboveG   = 1.8f;   // landing spike above this
  uint32_t impactWithinMs = 1000;   // landing must follow within this time

  uint8_t  state = 0;               // 0 waiting, 1 falling, 2 waiting for impact
  uint32_t since = 0;

  bool falling() const { return state == 1; }

  // Feed one reading. Returns true exactly once per detected fall.
  bool update(uint32_t now, float g) {
    switch (state) {
      case 0:
        if (g < freeFallBelowG) { state = 1; since = now; }
        break;
      case 1:
        if (g >= freeFallBelowG) {
          if (now - since < freeFallMinMs) { state = 0; break; }   // too short: a bump
          if (g > impactAboveG) { state = 0; return true; }        // landed at once
          state = 2; since = now;
        }
        break;
      case 2:
        if (g > impactAboveG) { state = 0; return true; }
        if (now - since > impactWithinMs) state = 0;               // no landing: ignore
        break;
    }
    return false;
  }
};

// ---------- Tap counter ----------
// Counts short knocks. Taps closer together than groupMs belong to one
// group; when the table goes quiet, update() returns the group size.
struct TapCounter {
  float    tapAboveShockG = 0.5f;   // a knock is a shock above this
  uint32_t deadMs  = 120;           // ignore the ringing right after a knock
  uint32_t groupMs = 700;           // quiet time that ends a group

  uint32_t lastTap = 0;
  uint8_t  count = 0;

  // Feed one shock reading. Returns 0, or the number of taps in a group
  // that has just finished.
  uint8_t update(uint32_t now, float shockG) {
    if (shockG > tapAboveShockG && (count == 0 || now - lastTap > deadMs)) {
      if (count < 255) count++;
      lastTap = now;
    }
    if (count > 0 && now - lastTap > groupMs) {
      uint8_t c = count; count = 0; return c;
    }
    return 0;
  }
};

// ---------- Tilt gestures ----------
enum Gesture : uint8_t { GESTURE_FLAT, GESTURE_FORWARD, GESTURE_BACK, GESTURE_LEFT, GESTURE_RIGHT };

inline Gesture gestureFromTilt(float pitchDeg, float rollDeg, float limitDeg = 25.0f) {
  if (fabsf(pitchDeg) < limitDeg && fabsf(rollDeg) < limitDeg) return GESTURE_FLAT;
  if (fabsf(pitchDeg) >= fabsf(rollDeg)) return pitchDeg > 0 ? GESTURE_FORWARD : GESTURE_BACK;
  return rollDeg > 0 ? GESTURE_RIGHT : GESTURE_LEFT;
}

inline const char* gestureName(Gesture g) {
  switch (g) {
    case GESTURE_FLAT:    return "FLAT (stop)";
    case GESTURE_FORWARD: return "FORWARD";
    case GESTURE_BACK:    return "BACK";
    case GESTURE_LEFT:    return "LEFT";
    case GESTURE_RIGHT:   return "RIGHT";
  }
  return "?";
}

// ---------- Spirit-level bar ----------
// Writes a text bar such as "[-----|--*--]" for an angle between
// -rangeDeg and +rangeDeg. width is the number of positions (odd works best).
inline void levelBar(float angleDeg, float rangeDeg, char* out, int width) {
  if (width < 3) { out[0] = 0; return; }
  out[0] = '[';
  for (int i = 1; i <= width; i++) out[i] = '-';
  int mid = 1 + width / 2;
  out[mid] = '|';
  float f = angleDeg / rangeDeg;
  if (f < -1.0f) f = -1.0f;
  if (f >  1.0f) f =  1.0f;
  int pos = mid + (int)lrintf(f * (width / 2));
  out[pos] = '*';
  out[width + 1] = ']';
  out[width + 2] = 0;
}

// ---------- PID controller ----------
// output = kp*error + ki*(sum of error*dt) + kd*(change of error / dt)
struct Pid {
  float kp = 0, ki = 0, kd = 0;
  float outMin = -255, outMax = 255;
  float integral = 0, lastError = 0;
  bool  first = true;

  void reset() { integral = 0; lastError = 0; first = true; }

  float step(float error, float dtSeconds) {
    if (dtSeconds <= 0.0f) dtSeconds = 0.001f;
    float deriv = first ? 0.0f : (error - lastError) / dtSeconds;
    first = false;
    lastError = error;
    float out = kp * error + ki * (integral + error * dtSeconds) + kd * deriv;
    // Anti-windup: only keep integrating while the output is not saturated.
    if (out > outMin && out < outMax) integral += error * dtSeconds;
    if (out > outMax) out = outMax;
    if (out < outMin) out = outMin;
    return out;
  }
};


// ---------- Angles ----------
// Difference a - b in degrees, wrapped to -180..+180 (so 350 - 10 = -20).
inline float angleDiff(float aDeg, float bDeg) {
  float d = fmodf(aDeg - bDeg, 360.0f);
  if (d > 180.0f)  d -= 360.0f;
  if (d < -180.0f) d += 360.0f;
  return d;
}

// How far the board is tilted from flat, in degrees (0 = flat, 90 = on its
// side, 180 = upside down), from the gravity direction in a Pose.
inline float tiltDeg(const Pose& p) {
  float c = p.gz;
  if (c > 1.0f) c = 1.0f;
  if (c < -1.0f) c = -1.0f;
  return acosf(c) * 180.0f / (float)M_PI;
}

// Turning speed in degrees per second between two poses dtSeconds apart.
// Works for any axis, and the same way for both firmware versions.
inline float angularSpeedDps(const Pose& a, const Pose& b, float dtSeconds) {
  if (dtSeconds <= 0.0f) return 0.0f;
  float dot = a.qw*b.qw + a.qx*b.qx + a.qy*b.qy + a.qz*b.qz;
  if (dot < 0.0f) dot = -dot;            // q and -q are the same rotation
  if (dot > 1.0f) dot = 1.0f;
  float angleRad = 2.0f * acosf(dot);
  return angleRad * 180.0f / (float)M_PI / dtSeconds;
}

// ---------- Threshold switch with hysteresis ----------
// Turns ON above onAbove and OFF below offBelow. The gap between the two
// stops flickering when the value hovers near one limit.
struct ThresholdSwitch {
  float onAbove = 10.0f, offBelow = 5.0f;
  bool  on = false;
  // Returns true when the state changed.
  bool update(float v) {
    if (!on && v > onAbove)  { on = true;  return true; }
    if (on  && v < offBelow) { on = false; return true; }
    return false;
  }
};

// ---------- Step counter ----------
// A step shows as a rise of the total acceleration above riseAboveG after
// it dipped below armBelowG. minGapMs stops one step counting twice.
struct StepCounter {
  float    riseAboveG = 1.2f;
  float    armBelowG  = 1.0f;
  uint32_t minGapMs   = 250;   // fastest running is about 4 steps per second
  uint32_t steps = 0, lastStep = 0;
  bool     armed = true;       // the first step counts too

  // Returns true when a step was counted.
  bool update(uint32_t now, float g) {
    if (g < armBelowG) armed = true;
    if (armed && g > riseAboveG && (steps == 0 || now - lastStep >= minGapMs)) {
      steps++; lastStep = now; armed = false;
      return true;
    }
    return false;
  }
};

// ---------- Shake detector ----------
// A shake is at least minPeaks jolts close together; it ends when the
// board is quiet for quietMs. update() returns true once, when a shake ends.
struct ShakeDetector {
  float    peakAboveShockG = 0.6f;
  uint8_t  minPeaks = 3;
  uint32_t peakGapMs = 100;    // jolts closer than this count once
  uint32_t quietMs   = 500;
  uint8_t  peaks = 0;
  uint32_t lastPeak = 0;

  bool update(uint32_t now, float shockG) {
    if (shockG > peakAboveShockG && (peaks == 0 || now - lastPeak > peakGapMs)) {
      if (peaks < 255) peaks++;
      lastPeak = now;
    }
    if (peaks > 0 && now - lastPeak > quietMs) {
      bool shake = peaks >= minPeaks;
      peaks = 0;
      return shake;
    }
    return false;
  }
};

// ---------- Jump timer (bike, skateboard, trampoline) ----------
// Airtime = time spent in free fall (below freeFallBelowG). update()
// returns the airtime in ms once, on landing; 0 otherwise. Very short dips
// (below minAirMs) are bumps, not jumps.
struct JumpTimer {
  float    freeFallBelowG = 0.4f;
  uint32_t minAirMs = 80;
  bool     inAir = false;
  uint32_t takeOff = 0;

  uint32_t update(uint32_t now, float g) {
    if (!inAir && g < freeFallBelowG) { inAir = true; takeOff = now; return 0; }
    if (inAir && g >= freeFallBelowG) {
      inAir = false;
      uint32_t air = now - takeOff;
      return air >= minAirMs ? air : 0;
    }
    return 0;
  }
};

// ---------- Shaking strength (earthquake model) ----------
// A smoothed average of the shock, in g. smoothing 0..1: lower = steadier.
struct ShakeLevel {
  float smoothing = 0.05f;
  float level = 0.0f;
  float update(float shockG) { level += (shockG - level) * smoothing; return level; }
};

// ---------- Freeze detector (pure logic, host-tested) ----------
// Decides when the sensor has frozen. The library calls update() on every
// readPacket() call. Two rules:
//   NO_DATA: the sensor's memory (FIFO) was empty for longer than noDataMs.
//            A full or overflowing FIFO counts as "alive": the sensor is
//            still producing data, the sketch is just reading too slowly.
//   STUCK:   packets keep arriving, but the quaternion bytes stay exactly
//            the same for longer than stuckMs. Judged only on packets that
//            were actually read, so a slow sketch never triggers it.
enum FreezeState : uint8_t { FREEZE_NONE = 0, FREEZE_NO_DATA = 1, FREEZE_STUCK = 2 };

struct FreezeDetector {
  uint32_t noDataMs = 1500;
  uint32_t stuckMs  = 5000;
  uint32_t lastAlive = 0, lastChange = 0;
  uint8_t  lastQuat[16];
  bool     started = false, haveQuat = false;

  void reset(uint32_t now) {
    lastAlive = lastChange = now; started = true; haveQuat = false;
  }

  // fifoHadData: the FIFO held any bytes at this call (even if it overflowed).
  // quat: the 16 quaternion bytes of a packet read at this call, or nullptr.
  FreezeState update(uint32_t now, bool fifoHadData, const uint8_t* quat) {
    if (!started) reset(now);
    if (fifoHadData) lastAlive = now;
    if (quat) {
      if (!haveQuat || memcmp(quat, lastQuat, 16) != 0) {
        memcpy(lastQuat, quat, 16); haveQuat = true; lastChange = now;
      } else if (now - lastChange > stuckMs) {
        return FREEZE_STUCK;
      }
    }
    if (now - lastAlive > noDataMs) return FREEZE_NO_DATA;
    return FREEZE_NONE;
  }
};

} // namespace TeachMath
