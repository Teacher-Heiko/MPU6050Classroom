// ClassroomWiFi.h — Wi-Fi for the MPU6050Classroom examples.
// Works on ESP32 boards and the Arduino UNO R4 WiFi.
//
//   ClassroomWiFi wifi;
//   wifi.begin(Serial);          // saved settings, or the setup page
//
// What it does:
//   1. Wi-Fi provisioning: with no saved settings (or if they fail), the
//      board opens its own Wi-Fi network called "MPU-Setup-xxxx". Join it
//      with a phone or laptop, open the setup page, type the Wi-Fi name and
//      password, press Save. The board stores them and restarts.
//        ESP32: the setup page usually opens by itself after joining.
//        UNO R4 WiFi: open http://192.168.4.1 in the browser.
//   2. Plain Wi-Fi password (WPA2-Personal): enter name + password.
//   3. School login (WPA2-Enterprise, ESP32 only): also enter the username.
//   4. Phone messages with ntfy (https://ntfy.sh): sendNtfy(...).
//
// Settings are stored on the board (ESP32: Preferences; UNO R4: EEPROM) and
// survive a new upload. wifi.forgetSettings() erases them.
//
// Only include this file in Wi-Fi sketches.
#pragma once
#include <Arduino.h>

#if defined(ESP32)
  #include <WiFi.h>
  #include <NetworkClientSecure.h>
  #include <DNSServer.h>
  #include <Preferences.h>
  typedef NetworkClientSecure ClassroomTlsClient;
#elif defined(ARDUINO_UNOR4_WIFI)
  #include <WiFiS3.h>
  #include <WiFiSSLClient.h>
  #include <EEPROM.h>
  typedef WiFiSSLClient ClassroomTlsClient;
#else
  #error "ClassroomWiFi needs an ESP32 board or an Arduino UNO R4 WiFi."
#endif

// ------------------------------------------------------------------------
// Small helpers shared by the setup page and ClassroomWebPage
// ------------------------------------------------------------------------
namespace ClassroomNet {

inline void restartBoard() {
#if defined(ESP32)
  ESP.restart();
#else
  NVIC_SystemReset();
#endif
}

// Decode "a+b%21" -> "a b!" into out (at most outSize-1 characters).
inline void urlDecode(const char* in, size_t inLen, char* out, size_t outSize) {
  size_t o = 0;
  for (size_t i = 0; i < inLen && o + 1 < outSize; i++) {
    char c = in[i];
    if (c == '+') c = ' ';
    else if (c == '%' && i + 2 < inLen) {
      char h[3] = {in[i + 1], in[i + 2], 0};
      c = (char)strtol(h, nullptr, 16);
      i += 2;
    }
    out[o++] = c;
  }
  out[o] = 0;
}

// Find field=value in an x-www-form-urlencoded body.
inline void formField(const char* body, const char* name, char* out, size_t outSize) {
  out[0] = 0;
  size_t nlen = strlen(name);
  const char* p = body;
  while (p && *p) {
    if (strncmp(p, name, nlen) == 0 && p[nlen] == '=') {
      const char* v = p + nlen + 1;
      const char* end = strchr(v, '&');
      urlDecode(v, end ? (size_t)(end - v) : strlen(v), out, outSize);
      return;
    }
    p = strchr(p, '&');
    if (p) p++;
  }
}

// ntfy topic names: letters, digits, '_' and '-', 1..64 characters.
inline bool validTopic(const char* t) {
  size_t n = strlen(t);
  if (n == 0 || n > 64) return false;
  for (size_t i = 0; i < n; i++) {
    char c = t[i];
    if (!(isalnum((unsigned char)c) || c == '_' || c == '-')) return false;
  }
  return true;
}

// Read one HTTP request: the first line into line, and (for POST) the body.
// Returns false on timeout.
inline bool readRequest(Client& c, char* line, size_t lineSize,
                        char* body, size_t bodySize, uint32_t timeoutMs = 1500) {
  uint32_t t0 = millis();
  size_t n = 0; int contentLength = 0; bool firstLine = true;
  char hdr[96]; size_t h = 0;
  line[0] = 0; if (body) body[0] = 0;
  while (millis() - t0 < timeoutMs) {
    if (!c.available()) { delay(1); continue; }
    char ch = (char)c.read();
    if (ch == '\r') continue;
    if (ch != '\n') {
      if (firstLine) { if (n + 1 < lineSize) line[n++] = ch; }
      else if (h + 1 < sizeof hdr) hdr[h++] = ch;
      continue;
    }
    if (firstLine) { line[n] = 0; firstLine = false; continue; }
    hdr[h] = 0;
    if (h == 0) break;                                   // blank line: end of headers
    if (strncasecmp(hdr, "Content-Length:", 15) == 0) contentLength = atoi(hdr + 15);
    h = 0;
  }
  if (firstLine) return false;
  if (body && contentLength > 0) {
    size_t b = 0;
    while (millis() - t0 < timeoutMs && (int)b < contentLength && b + 1 < bodySize) {
      if (c.available()) body[b++] = (char)c.read(); else delay(1);
    }
    body[b] = 0;
  }
  return true;
}

inline void sendHeader(Client& c, const char* type) {
  c.print(F("HTTP/1.1 200 OK\r\nContent-Type: "));
  c.print(type);
  c.print(F("\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n"));
}

} // namespace ClassroomNet

// ------------------------------------------------------------------------
// ClassroomWiFi: connect, provisioning page, ntfy
// ------------------------------------------------------------------------
class ClassroomWiFi {
public:
  struct Settings {
    uint32_t magic;            // marks valid saved settings
    char ssid[33];
    char pass[65];
    char user[65];             // school login username (ESP32 only); empty = plain password
    char topic[65];            // ntfy topic; may be empty
  };

  // Connect with saved settings; open the setup page if there are none or
  // if connecting fails for 20 s. Returns true when connected.
  // Before connecting, it asks in the Serial Monitor whether to change the
  // settings (5 s; no answer = keep them).
  bool begin(Stream& out) {
    _out = &out;
    bool have = load();
    if (have) {
      out.print(F("Saved Wi-Fi: ")); out.println(_s.ssid);
      if (askChange(out)) have = false;
    }
    if (have && connectSaved(20000)) return true;
    if (have) out.println(F("Could not connect with the saved settings."));
    runSetupPage(out);           // does not return: saves and restarts
    return false;
  }

  // Connect without the setup page, with a name and password in the sketch.
  bool beginWith(Stream& out, const char* ssid, const char* password) {
    _out = &out;
    copy(_s.ssid, ssid, sizeof _s.ssid); copy(_s.pass, password, sizeof _s.pass);
    _s.user[0] = 0;
    return connectSaved(20000);
  }

#if defined(ESP32)
  // School login (WPA2-Enterprise, PEAP), ESP32 only.
  bool beginSchool(Stream& out, const char* ssid, const char* username, const char* password) {
    _out = &out;
    copy(_s.ssid, ssid, sizeof _s.ssid); copy(_s.user, username, sizeof _s.user);
    copy(_s.pass, password, sizeof _s.pass);
    return connectSaved(30000);
  }
#endif

  bool connected() { return WiFi.status() == WL_CONNECTED; }
  IPAddress ip()   { return WiFi.localIP(); }
  const char* topic() const { return _s.topic; }

  // If the connection dropped, try again (call it now and then from loop()).
  void keepConnected() {
    if (connected()) return;
    if (millis() - _lastRetry < 10000) return;
    _lastRetry = millis();
    if (_out) _out->println(F("Wi-Fi lost. Reconnecting..."));
    connectSaved(8000);
  }

  void forgetSettings() {
#if defined(ESP32)
    Preferences p; p.begin("mpuwifi", false); p.clear(); p.end();
#else
    Settings empty; memset(&empty, 0, sizeof empty); EEPROM.put(0, empty);
#endif
  }

  // Send a phone message through ntfy (HTTPS). topic: from the setup page
  // or your sketch. Returns true when the ntfy server accepted it.
  // Anyone who knows the topic name can read the messages: keep it secret.
  bool sendNtfy(const char* topic, const char* title, const char* message) {
    if (!ClassroomNet::validTopic(topic)) {
      if (_out) _out->println(F("ntfy: the topic name is missing or has characters other than letters, digits, - and _."));
      return false;
    }
    if (!connected()) { if (_out) _out->println(F("ntfy: no Wi-Fi connection.")); return false; }
    ClassroomTlsClient c;
#if defined(ESP32)
    c.useBuiltinCACertBundle();            // checks ntfy.sh's certificate
#endif
    if (!c.connect("ntfy.sh", 443)) {
      if (_out) _out->println(F("ntfy: could not reach ntfy.sh (no internet, or blocked by the network)."));
      return false;
    }
    size_t len = strlen(message);
    c.print(F("POST /")); c.print(topic); c.print(F(" HTTP/1.1\r\nHost: ntfy.sh\r\n"));
    c.print(F("Title: ")); printHeaderSafe(c, title); c.print(F("\r\n"));
    c.print(F("Content-Type: text/plain\r\nContent-Length: ")); c.print((unsigned)len);
    c.print(F("\r\nConnection: close\r\n\r\n"));
    c.print(message);

    char status[40]; size_t n = 0; uint32_t t0 = millis();
    while (millis() - t0 < 5000 && n + 1 < sizeof status) {
      if (!c.available()) { if (!c.connected()) break; delay(5); continue; }
      char ch = (char)c.read();
      if (ch == '\n') break;
      status[n++] = ch;
    }
    status[n] = 0;
    c.stop();
    bool ok = strstr(status, " 200") != nullptr;
    if (_out) {
      if (ok) _out->println(F("ntfy: message sent."));
      else { _out->print(F("ntfy: the server answered: ")); _out->println(n ? status : "(nothing)"); }
    }
    return ok;
  }

private:
  Settings _s = {};
  Stream*  _out = nullptr;
  uint32_t _lastRetry = 0;
  static const uint32_t MAGIC = 0x4D505557;   // "MPUW"

  static void copy(char* dst, const char* src, size_t n) {
    if (!src) src = "";
    strncpy(dst, src, n - 1); dst[n - 1] = 0;
  }
  static void printHeaderSafe(Print& p, const char* s) {
    for (; s && *s; s++) if (*s != '\r' && *s != '\n') p.print(*s);
  }

  bool askChange(Stream& out) {
    while (out.available()) out.read();
    out.println(F("Type c and press Enter within 5 s to change the Wi-Fi settings."));
    uint32_t t0 = millis();
    while (millis() - t0 < 5000) {
      if (out.available()) { char ch = (char)out.read(); if (ch == 'c' || ch == 'C') return true; }
      delay(10);
    }
    return false;
  }

  bool load() {
#if defined(ESP32)
    Preferences p;
    if (!p.begin("mpuwifi", true)) return false;
    bool ok = p.isKey("ssid");
    if (ok) {
      p.getString("ssid", _s.ssid, sizeof _s.ssid);
      p.getString("pass", _s.pass, sizeof _s.pass);
      p.getString("user", _s.user, sizeof _s.user);
      p.getString("topic", _s.topic, sizeof _s.topic);
    }
    p.end();
    return ok && _s.ssid[0];
#else
    EEPROM.get(0, _s);
    if (_s.magic != MAGIC) { memset(&_s, 0, sizeof _s); return false; }
    _s.ssid[32] = 0; _s.pass[64] = 0; _s.user[64] = 0; _s.topic[64] = 0;
    return _s.ssid[0] != 0;
#endif
  }

  void save() {
#if defined(ESP32)
    Preferences p; p.begin("mpuwifi", false);
    p.putString("ssid", _s.ssid); p.putString("pass", _s.pass);
    p.putString("user", _s.user); p.putString("topic", _s.topic);
    p.end();
#else
    _s.magic = MAGIC;
    EEPROM.put(0, _s);
#endif
  }

  bool connectSaved(uint32_t timeoutMs) {
    Stream& out = *_out;
    out.print(F("Connecting to Wi-Fi \"")); out.print(_s.ssid); out.print(F("\""));
#if defined(ESP32)
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    if (_s.user[0]) {
      out.print(F(" with school login "));
      out.print(_s.user);
      WiFi.begin(_s.ssid, WPA2_AUTH_PEAP, _s.user, _s.user, _s.pass);
    } else {
      WiFi.begin(_s.ssid, _s.pass);
    }
#else
    if (_s.user[0]) out.print(F(" (school login is not supported on the UNO R4 WiFi; using the password only)"));
    WiFi.begin(_s.ssid, _s.pass);
#endif
    out.println();
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
      delay(500); out.print('.');
#if defined(ARDUINO_UNOR4_WIFI)
      if (WiFi.status() != WL_CONNECTED) WiFi.begin(_s.ssid, _s.pass);
#endif
    }
    out.println();
    if (WiFi.status() != WL_CONNECTED) return false;
#if defined(ARDUINO_UNOR4_WIFI)
    for (int i = 0; i < 20 && WiFi.localIP() == IPAddress(0, 0, 0, 0); i++) delay(100);
#endif
    out.print(F("Wi-Fi connected. This board's address: ")); out.println(WiFi.localIP());
    return true;
  }

  void makeDefaultTopic(char* out, size_t n) {
    const char* hex = "0123456789abcdef";
    copy(out, "mpu-", n);
    size_t i = 4;
    for (int k = 0; k < 12 && i + 1 < n; k++) {
#if defined(ESP32)
      uint32_t r = esp_random();
#else
      uint32_t r = (uint32_t)random(0, 16);
#endif
      out[i++] = hex[r & 0x0F];
    }
    out[i] = 0;
  }

  void runSetupPage(Stream& out) {
    char apName[24];
#if defined(ESP32)
    uint64_t mac = ESP.getEfuseMac();
    snprintf(apName, sizeof apName, "MPU-Setup-%04X", (unsigned)(mac >> 32) & 0xFFFF);
#else
    randomSeed(analogRead(A0) ^ micros());
    snprintf(apName, sizeof apName, "MPU-Setup-%04X", (unsigned)random(0, 0xFFFF));
#endif
    const char* apPass = "mpu6050setup";
    char suggested[65];
    if (ClassroomNet::validTopic(_s.topic)) copy(suggested, _s.topic, sizeof suggested);
    else makeDefaultTopic(suggested, sizeof suggested);

#if defined(ESP32)
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName, apPass);
    IPAddress apIP = WiFi.softAPIP();
    DNSServer dns;
    dns.start(53, "*", apIP);                 // phones then show the page by themselves
    NetworkServer server(80);
#else
    WiFi.beginAP(apName, apPass);
    delay(1000);
    IPAddress apIP = WiFi.softAPIP();
    WiFiServer server(80);
#endif
    server.begin();

    out.println();
    out.println(F("=== Wi-Fi setup ==="));
    out.print(F("1. On a phone or laptop, join the Wi-Fi network: ")); out.println(apName);
    out.print(F("   Password: ")); out.println(apPass);
    out.print(F("2. Open http://")); out.print(apIP); out.println(F(" in the browser"));
    out.println(F("   (on many phones the page opens by itself)."));
    out.println(F("3. Enter the Wi-Fi name and password, then press Save."));

    char line[128], body[384];
    for (;;) {
#if defined(ESP32)
      dns.processNextRequest();
      NetworkClient c = server.accept();
#else
      WiFiClient c = server.available();
#endif
      if (!c) { delay(2); continue; }
      if (!ClassroomNet::readRequest(c, line, sizeof line, body, sizeof body)) { c.stop(); continue; }

      if (strncmp(line, "POST /save", 10) == 0) {
        Settings s = {};
        ClassroomNet::formField(body, "ssid",  s.ssid,  sizeof s.ssid);
        ClassroomNet::formField(body, "pass",  s.pass,  sizeof s.pass);
        ClassroomNet::formField(body, "user",  s.user,  sizeof s.user);
        ClassroomNet::formField(body, "topic", s.topic, sizeof s.topic);
        ClassroomNet::sendHeader(c, "text/html");
        if (s.ssid[0] == 0) {
          c.print(F("<meta name=viewport content='width=device-width'><p>The Wi-Fi name is empty. <a href='/'>Back</a></p>"));
          c.stop();
          continue;
        }
        c.print(F("<meta name=viewport content='width=device-width'><body style='font-family:sans-serif'>"
                  "<h2>Saved</h2><p>The board restarts and connects to the Wi-Fi. "
                  "Watch the Serial Monitor for its address.</p></body>"));
        delay(200);
        c.stop();
        _s = s;
        save();
        out.print(F("Settings saved for Wi-Fi \"")); out.print(_s.ssid); out.println(F("\". Restarting..."));
        if (_s.topic[0]) { out.print(F("ntfy topic: ")); out.println(_s.topic); }
        delay(1000);
        ClassroomNet::restartBoard();
      }

      // Any other request: the setup form.
      ClassroomNet::sendHeader(c, "text/html");
      c.print(F("<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
                "<title>MPU-6050 Wi-Fi setup</title>"
                "<body style='font-family:sans-serif;max-width:28em;margin:1em auto;padding:0 1em'>"
                "<h2>Wi-Fi setup</h2><form method=POST action=/save>"
                "<p>Wi-Fi name<br><input name=ssid required maxlength=32 style='width:100%'></p>"
                "<p>Wi-Fi password<br><input name=pass type=password maxlength=64 style='width:100%'></p>"));
#if defined(ESP32)
      c.print(F("<p>School login username (only for networks that ask for a username; leave empty otherwise)"
                "<br><input name=user maxlength=64 style='width:100%'></p>"));
#endif
      c.print(F("<p>ntfy topic for phone messages (optional; keep it secret)<br>"
                "<input name=topic maxlength=64 pattern='[A-Za-z0-9_-]*' style='width:100%' value='"));
      c.print(suggested);
      c.print(F("'></p><p><button style='font-size:1.1em;padding:.4em 1.2em'>Save</button></p></form></body>"));
      c.stop();
    }
  }
};

// ------------------------------------------------------------------------
// ClassroomWebPage: a tiny web server for live pages on the local Wi-Fi
// ------------------------------------------------------------------------
// page.begin();  then in loop():  page.handle(HTML, writeJson);
//   "/"      -> the HTML page (stored in your sketch)
//   "/data"  -> JSON written by your writeJson(Print&) function
class ClassroomWebPage {
public:
  explicit ClassroomWebPage(uint16_t port = 80) : _server(port) {}
  void begin() { _server.begin(); }

  // Call often from loop(). Handles at most one browser request per call.
  void handle(const char* html, void (*writeJson)(Print& out)) {
#if defined(ESP32)
    NetworkClient c = _server.accept();
#else
    WiFiClient c = _server.available();
#endif
    if (!c) return;
    char* line = _line;
    if (!ClassroomNet::readRequest(c, _line, sizeof _line, nullptr, 0, 500)) { c.stop(); return; }
    if (strncmp(line, "GET /data", 9) == 0) {
      ClassroomNet::sendHeader(c, "application/json");
      writeJson(c);
    } else {
      ClassroomNet::sendHeader(c, "text/html");
      c.print(html);
    }
    c.stop();
  }

  // True if the current request contains text, e.g. requested("reset")
  // for a request to /data?reset. Use it inside your writeJson function.
  bool requested(const char* text) const { return strstr(_line, text) != nullptr; }

private:
  char _line[96] = {0};
#if defined(ESP32)
  NetworkServer _server;
#else
  WiFiServer _server;
#endif
};
