// 5_LiveWebPage — see the board's tilt live on any phone or laptop.
//
// The board runs a small web page. Every device on the same Wi-Fi can open
// it: type the address from the Serial Monitor (for example
// http://192.168.1.23) into the browser. The page asks the board for new
// values 5 times a second and tilts a picture to match.
//
// Run 5_WiFiSetup first. Some school networks block devices from talking
// to each other; a phone hotspot or a classroom router then works instead.
//
// Wiring: VCC -> 3.3 V, GND -> GND, SDA -> SENSOR_SDA, SCL -> SENSOR_SCL.
// Board: Arduino UNO R4 WiFi.
#if !defined(ARDUINO_UNOR4_WIFI)
#error "This Wi-Fi example needs an Arduino UNO R4 WiFi (the UNO R3 and R4 Minima have no Wi-Fi)."
#endif
#include <MPU6050Classroom.h>
#include <ClassroomWiFi.h>

// ---- Pins: on the UNO R3 and R4 the I2C pins are fixed ----
const int SENSOR_SDA = A4;
const int SENSOR_SCL = A5;

MPU6050Classroom mpu;
ClassroomWiFi wifi;
ClassroomWebPage page;

const char HTML[] = R"html(<!doctype html>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>MPU-6050 live</title>
<body style="font-family:sans-serif;text-align:center;margin:1em">
<h2>MPU-6050 live</h2>
<div style="height:180px;display:flex;align-items:center;justify-content:center">
  <div id=board style="width:160px;height:90px;background:#2a6;border-radius:8px;
       transition:transform .15s linear"></div>
</div>
<p style="font-size:1.4em">pitch <b id=p>-</b>&deg; &nbsp; roll <b id=r>-</b>&deg; &nbsp; yaw <b id=y>-</b>&deg;</p>
<p id=s style="color:#888">connecting...</p>
<script>
async function tick(){
  try{
    const d = await (await fetch('/data')).json();
    p.textContent = d.pitch.toFixed(1); r.textContent = d.roll.toFixed(1); y.textContent = d.yaw.toFixed(1);
    board.style.transform = 'perspective(400px) rotateX(' + d.pitch + 'deg) rotateZ(' + (-d.roll) + 'deg)';
    s.textContent = 'live';
  }catch(e){ s.textContent = 'no answer from the board'; }
  setTimeout(tick, 200);
}
tick();
</script>
)html";

void writeJson(Print& out) {
  out.print(F("{\"pitch\":")); out.print(mpu.pitch(), 1);
  out.print(F(",\"roll\":"));  out.print(mpu.roll(), 1);
  out.print(F(",\"yaw\":"));   out.print(mpu.yaw(), 1);
  out.print('}');
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  if (!wifi.begin(Serial)) return;
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  page.begin();
  Serial.print(F("Open this address in a browser: http://")); Serial.println(wifi.ip());
}

void loop() {
  mpu.update();
  page.handle(HTML, writeJson);
  wifi.keepConnected();
}
