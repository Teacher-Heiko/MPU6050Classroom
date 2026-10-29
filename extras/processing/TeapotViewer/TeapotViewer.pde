// TeapotViewer — original Processing 4 sketch for the MPU6050Classroom Teapot demo.
// Written from scratch for this library (not the i2cdevlib MPUTeapot sketch).
// Draws a 3D board that follows the sensor, from the 14-byte teapot packets:
//   '$' 0x02 q0h q0l q1h q1l q2h q2l q3h q3l 0x00 count '\r' '\n'
//
// Setup: Sketch > Import Library > Serial. Set PORT_INDEX below, press Run.
import processing.serial.*;

final int BAUD = 115200;
final int PORT_INDEX = 0;          // run once, read the console list, adjust

Serial port;
float qw = 1, qx = 0, qy = 0, qz = 0;
int packets = 0;

void setup() {
  size(700, 500, P3D);
  println("Serial ports:");
  printArray(Serial.list());
  port = new Serial(this, Serial.list()[PORT_INDEX], BAUD);
}

void draw() {
  readPackets();
  background(25);
  lights();
  fill(255); textSize(14);
  text("packets: " + packets, 10, 20);
  text(String.format("q = %.2f  %.2f  %.2f  %.2f", qw, qx, qy, qz), 10, 40);

  translate(width/2, height/2, 0);
  // quaternion -> axis-angle rotation (sensor axes mapped to screen axes)
  float angle = 2 * acos(constrain(qw, -1, 1));
  float s = sqrt(max(1e-9, 1 - qw*qw));
  rotate(angle, qx/s, qz/s, -qy/s);

  // the "board": a green PCB slab with a chip and a pin-1 dot
  fill(30, 120, 60);  box(220, 16, 140);
  pushMatrix(); translate(0, -14, 0); fill(40); box(60, 12, 60); popMatrix();
  pushMatrix(); translate(-90, -10, -55); fill(230, 60, 60); box(14, 6, 14); popMatrix();
}

void readPackets() {
  while (port.available() >= 14) {
    if (port.read() != '$') continue;          // resync on header
    if (port.read() != 0x02) continue;
    byte[] b = new byte[12];
    if (port.readBytes(b) != 12) return;
    qw = toQ(b[0], b[1]);
    qx = toQ(b[2], b[3]);
    qy = toQ(b[4], b[5]);
    qz = toQ(b[6], b[7]);
    packets++;
  }
}

float toQ(byte hi, byte lo) {
  int v = ((hi & 0xFF) << 8) | (lo & 0xFF);
  if (v >= 32768) v -= 65536;
  return v / 16384.0f;
}
