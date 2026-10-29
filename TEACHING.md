# Teaching with MPU6050Classroom

This page is for teachers. It describes what students learn at each level,
a suggested order, the mistakes students make most often, and how the
library keeps the load on students low.

The examples have **not yet been run on real hardware** (see the README).
Try each example yourself on your classroom boards before a lesson.

---

## Suggested order

| Lesson | Examples | Students learn to |
|---|---|---|
| 1 | `1a_WhoAmI`, `1b_SelfTest` | wire an I2C sensor, set two pin numbers, read a test report |
| 2 | `1c_YawPitchRoll` | relate yaw, pitch and roll to real movement (use the axes picture in `extras/images/axes.svg`) |
| 3 | `1d_Teapot` with the 3D viewer | see the same numbers as a moving 3D model |
| 4–6 | any two or three level-2 projects | turn a measurement into a decision with a limit (threshold) |
| 7–9 | one level-3 project | combine the sensor with an output (servo, radio) or memory |
| 10+ | level 4 or 5, as a project | control loops, timing, Wi-Fi and phone messages |

Level 2 works well as a choice: each student or pair picks one project
(spirit level, tilt alarm, crash detector, fall detector, tap counter,
shake dice, step counter) and presents how it decides.

## What each level teaches

**Level 1 — First steps.** Wiring (power, ground, two data wires), the
idea of a pin number, checking that hardware works before writing code,
and what yaw, pitch and roll mean.

**Level 2 — Projects with the sensor only.** Every project turns a
measurement into a yes/no decision: "is the tilt above 30°?", "was there a
free fall followed by an impact?". Students meet limits (thresholds), the
gap between "on" and "off" limits that stops flickering (hysteresis), and
waiting times that stop one event counting twice.

**Level 3 — Bigger projects.** The sensor drives something: a servo
(gimbal), a second board by radio (gesture remote), or saved data (crash
logger). Students meet separate power supplies, smoothing, and memory that
survives a restart.

**Level 4 — Advanced.** Control and timing: a robot that turns exactly
90°, a PID controller that keeps a robot upright, and why a slow `loop()`
loses data (`4_SlowLoop` lets students measure this themselves).

**Level 5 — Wi-Fi.** Connecting a device to a network, a setup page
instead of passwords in code, phone messages, and the privacy of public
message topics.

## Reading the project logic

Each project's decision is a short, commented piece of code in
`src/TeachMath.h` (for example `FallDetector`, `TapCounter`,
`StepCounter`, `Pid`). Students can open the file, read how the decision
is made, and change a limit. Each piece has computer tests in
`extras/tests/test_math.cpp`, which advanced students can extend.

## Common mistakes and what to say

| What the student sees | Usual cause | What to check |
|---|---|---|
| "No sensor answers" | wrong pin numbers, or a loose wire | the two pin lines against the board's pin diagram; every wire pushed in |
| WHO_AM_I is not `0x68` | a clone or a different sensor on the board | run `1b_SelfTest`; some clones need `allowClones` (experienced users) |
| Many "repair" messages | loose jumper wires or a poor breadboard contact | shorter wires, re-seat them, avoid long breadboard rows |
| Freezes when a servo or motor moves | shared power supply | a separate supply for servos and motors, with a common ground |
| Angles slowly change while the board is still | yaw drift (no compass on the MPU-6050) | expected for yaw; pitch and roll stay steady |
| "Level" is not level | the board was not flat during calibration | restart with the board on a level surface |
| Upload error about the wrong folder | an ESP32 example opened for an UNO, or the other way round | open the same example from the other folder |
| The viewer shows nothing | the Serial Monitor is still open | close the Serial Monitor; only one program can use the port |
| Wi-Fi setup page does not open | the phone left the board's network | rejoin `MPU-Setup-xxxx`, open the address from the Serial Monitor |
| No phone message | wrong topic, or the school network blocks ntfy.sh | compare the topic in the app and the Serial Monitor; try a phone hotspot |

## Safety in the classroom

- The fall, crash and phone-alert projects are for learning. They are not
  safety devices and must not be used to protect a person.
- Laser turret: Class 2 laser modules only (at most 1 mW); never aim at
  people, animals, mirrors or windows.
- Robots: first tests with the wheels off the ground.
- Drop tests: onto a cushion or a bed, with the board and wires taped down.
- Door monitors and phone alerts: only on doors and in places where
  students have permission.

## How the library keeps the load on students low

The library is designed so that students spend their attention on the
project, not on set-up. These are the choices behind that:

- **One call to start.** `easyStart()` replaces about ten set-up lines
  (I2C, firmware loading, calibration, start). Students see one line with
  one meaning.
- **Two pin lines at the top of every sketch.** The only thing students
  must look up is in one fixed place, with the same names in every
  example (`SENSOR_SDA`, `SENSOR_SCL`).
- **Ready-made values with units.** `pitch()` in degrees and `shock()` in
  g, instead of raw numbers or quaternions. The raw data stays available
  for experienced students.
- **Errors in plain words, with the next step.** "No sensor answers. This
  sketch uses SDA = pin 21 ... check these against your board's pin
  diagram" instead of an error number. The board then retries, so a
  student fixes the wire without re-uploading.
- **Freezes repaired automatically.** Students are not stopped by a fault
  they cannot see; the message tells them it happened.
- **One idea per example.** Each sketch does one thing. Names start with
  the level, so the order is visible in the menu.
- **Decisions in named, readable pieces.** `FallDetector`, `TapCounter`
  and others hide the details but can be opened and read when a student
  is ready.
- **Wrong board caught early.** A sketch for the wrong board stops at
  compile time with a message naming the right folder, before anything
  confusing happens on the board.
- **No passwords in code.** The Wi-Fi setup page means students never edit
  network details in a sketch, which also keeps passwords out of shared
  code.
