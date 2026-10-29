// 5_EarthquakeWebLogger — a live shaking graph in the browser.
//
// Fix the board to the top of your model building. Open the address from
// the Serial Monitor on a phone or laptop on the same Wi-Fi. The page draws
// the shaking strength (in g) over the last 60 seconds and shows the
// strongest value so far. "Reset peak" starts a new test.
//
// Run 5_WiFiSetup first. While the page loads, the board may skip a few
// readings; the graph shows the smoothed level, so short gaps do not matter.
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
const int SAMPLES = 120;            // 120 samples, 2 per second = 60 s

MPU6050Classroom mpu;
ClassroomWiFi wifi;
ClassroomWebPage page;
TeachMath::ShakeLevel level;
float history[SAMPLES];
int   head = 0;
float peak = 0;

const char HTML[] = R"html(<!doctype html>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Earthquake model</title>
<body style="font-family:sans-serif;margin:1em">
<h2>Earthquake model: shaking strength</h2>
<canvas id=c width=600 height=240 style="width:100%;max-width:600px;border:1px solid #ccc"></canvas>
<p>now <b id=n>-</b> g &nbsp; strongest <b id=pk>-</b> g
<button onclick="fetch('/data?reset')">Reset peak</button></p>
<p style="color:#888">The last 60 seconds, newest on the right. Scale: 0 to 1 g.</p>
<script>
const g = c.getContext('2d');
async function tick(){
  try{
    const d = await (await fetch('/data')).json();
    n.textContent = d.now.toFixed(3); pk.textContent = d.peak.toFixed(3);
    g.clearRect(0,0,c.width,c.height);
    g.strokeStyle='#ddd'; for(let i=1;i<4;i++){g.beginPath();g.moveTo(0,c.height*i/4);g.lineTo(c.width,c.height*i/4);g.stroke();}
    g.strokeStyle='#c33'; g.lineWidth=2; g.beginPath();
    d.h.forEach((v,i)=>{const x=i*c.width/(d.h.length-1), y=c.height*(1-Math.min(v,1)); i?g.lineTo(x,y):g.moveTo(x,y);});
    g.stroke();
  }catch(e){}
  setTimeout(tick, 1000);
}
tick();
</script>
)html";

void writeJson(Print& out) {
  if (page.requested("reset")) peak = 0;
  out.print(F("{\"now\":")); out.print(level.level, 3);
  out.print(F(",\"peak\":")); out.print(peak, 3);
  out.print(F(",\"h\":["));
  for (int i = 0; i < SAMPLES; i++) {
    if (i) out.print(',');
    out.print(history[(head + i) % SAMPLES], 3);       // oldest first
  }
  out.print(F("]}"));
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  if (!wifi.begin(Serial)) return;
  mpu.easyStart(Serial, SENSOR_SDA, SENSOR_SCL);
  for (int i = 0; i < SAMPLES; i++) history[i] = 0;
  page.begin();
  Serial.print(F("Open this address in a browser: http://")); Serial.println(wifi.ip());
}

void loop() {
  if (mpu.update()) {
    float l = level.update(mpu.shock());
    if (l > peak) peak = l;
  }
  static uint32_t lastSample = 0;
  if (millis() - lastSample >= 500) {
    lastSample = millis();
    history[head] = level.level;
    head = (head + 1) % SAMPLES;
  }
  page.handle(HTML, writeJson);
  wifi.keepConnected();
}
