// DSO152 Wi-Fi viewer: reads the stock DSO152 firmware's RAM over SWD and
// streams it to browsers on the local network. The scope firmware is not
// modified; controls are sent as the scope's own key codes.
//
// RAM map and scaling: see ../swd_viewer.py (laptop version of the same thing).
#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

#include "frame.h"
#include "logger.h"
#include "qr_png.h"
#include "swd.h"
#include "web_page.h"
#include "wifi_net.h"

// --- wiring (overridden per board in platformio.ini) ------------------------
#ifndef PIN_SWCLK
#define PIN_SWCLK 18
#endif
#ifndef PIN_SWDIO
#define PIN_SWDIO 19
#endif
#ifndef SWD_HALF_PERIOD
#define SWD_HALF_PERIOD 0                 // raise if reads fail on long wires
#endif

// --- DSO152 stock firmware RAM --------------------------------------------
// The scope's own on-screen trace: 300 x s32, in screen pixels relative to
// zero, rebuilt by the stock firmware only when it completes a capture (so it
// holds still while waiting for a trigger or stopped, like the scope screen).
constexpr uint32_t A_TRACE = 0x20003C54;
constexpr uint32_t A_WRITE_POS = 0x2000012C;
constexpr uint32_t A_STATE = 0x20000110;     // 0x20 bytes of settings
constexpr uint32_t A_ZEROS = 0x20003C28;     // u16[2][11]
constexpr uint32_t A_MEAS = 0x20000138;      // s32[6]
constexpr uint32_t A_TIM1_PSC = 0x40012C28;  // PSC, ARR
constexpr uint32_t A_KEY = 0x2000010C;
constexpr uint32_t A_CURSOR = 0x20000126;
constexpr uint8_t KEY_NEXT = 0x08, KEY_TRIG_MODE = 0x01, KEY_RUN_STOP = 0x04;

// --- power ----------------------------------------------------------------
// Fixed: changing the CPU clock at runtime with Wi-Fi up stalled the network.
#ifndef CPU_MHZ
#define CPU_MHZ 80
#endif

// Reply to a "mem" debug command: 'M' 'R', u32 address, u16 words, data
constexpr size_t PEEK_MAX_WORDS = 256;

WebServer http(80);
WebSocketsServer ws(81);
Frame frame;
bool scopeOk = false;
uint32_t lastFrameMs = 0;
uint32_t frameIntervalMs = 66;   // ~15 fps, browser can change it
uint32_t lastConnectTry = 0;
bool radioAwake = false;
// Speed log on the serial monitor every 5 s while streaming
uint32_t statFrames = 0, statReadUs = 0, statT0 = 0;

// Full speed (radio awake, fast CPU) only while someone is watching; modem
// sleep and a slow CPU the rest of the time.
void setRadioAwake(bool awake) {
  if (wifi_net::apMode()) return;   // modem sleep is not allowed in AP mode
  if (awake == radioAwake) return;
  radioAwake = awake;
  WiFi.setSleep(!awake);
}

// One pending control command from a browser, run between frames.
struct Command {
  enum Type { NONE, ADJUST, KEY, PEEK } type = NONE;
  uint8_t item = 0;
  uint8_t key = 0;
  uint8_t client = 0;
  uint32_t addr = 0;
  uint16_t words = 0;
} pending;

// Used by the logger (MQTT "run" command) as well as the page.
void queueKey(uint8_t key) {
  pending = {Command::KEY, 0, key};
}

bool scopeConnect() {
  uint32_t id = 0;
  bool ok = swd::connect(&id);
  Serial.printf("SWD connect: %s DPIDR=%08lx\n", ok ? "ok" : "failed", (unsigned long)id);
  return ok;
}

bool readFrame() {
  uint32_t w[16];
  memcpy(frame.magic, "DS", 2);
  static int32_t trace[TRACE_POINTS];
  frame.version = 2;
  frame.flags = 0;
  if (!swd::readWords(A_WRITE_POS, w, 1)) return false;
  frame.pos = w[0] & 0xFFFF;
  if (!swd::readWords(A_STATE, (uint32_t *)frame.state, 8)) return false;
  if (!swd::readWords(A_ZEROS, (uint32_t *)frame.zeros, 11)) return false;
  if (!swd::readWords(A_MEAS, (uint32_t *)frame.meas, 6)) return false;
  if (!swd::readWords(A_TIM1_PSC, w, 2)) return false;
  frame.psc = w[0];
  frame.arr = w[1];
  if (!swd::readWords(A_TRACE, (uint32_t *)trace, TRACE_POINTS)) return false;
  for (int i = 0; i < TRACE_POINTS; i++)
    frame.trace[i] = (int16_t)constrain(trace[i], -32768, 32767);
  frame.flags = 1;
  return true;
}

// Debug: read target memory for a browser ("mem <hexaddr> <words>")
void peek(uint8_t client, uint32_t addr, size_t words) {
  static uint8_t reply[8 + PEEK_MAX_WORDS * 4];
  words = min(words, PEEK_MAX_WORDS);
  reply[0] = 'M';
  reply[1] = 'R';
  memcpy(reply + 2, &addr, 4);
  uint16_t n = words;
  memcpy(reply + 6, &n, 2);
  if (!swd::readWords(addr & ~3u, (uint32_t *)(reply + 8), words)) n = 0;
  memcpy(reply + 6, &n, 2);
  ws.sendBIN(client, reply, 8 + n * 4);
}

// Inject a key code and wait for the stock firmware to take it; it applies
// the change slightly after clearing the code, hence the settle delay.
bool press(uint8_t key) {
  if (!swd::writeByte(A_KEY, key)) return false;
  for (uint32_t t0 = millis(); millis() - t0 < 1000;) {
    uint8_t v;
    if (swd::readByte(A_KEY, &v) && v == 0xFF) {
      delay(300);
      return true;
    }
    delay(20);
  }
  return false;
}

bool adjust(uint8_t item, uint8_t key) {
  for (int i = 0; i < 12; i++) {
    uint8_t cur;
    if (!swd::readByte(A_CURSOR, &cur)) return false;
    if (cur == item) return press(key);
    if (!press(KEY_NEXT)) return false;
  }
  return false;
}

// Browser commands: "adj <item> <key>", "mode", "run", "fps <n>",
// "mem <hexaddr> <words>" (debug read)
void onWs(uint8_t client, WStype_t type, uint8_t *payload, size_t len) {
  if (type == WStype_CONNECTED) {
    Serial.printf("browser %u connected\n", client);
  } else if (type == WStype_TEXT) {
    String msg((const char *)payload, len);
    int item, key, fps, words;
    unsigned long addr;
    if (sscanf(msg.c_str(), "adj %d %d", &item, &key) == 2) {
      pending = {Command::ADJUST, (uint8_t)item, (uint8_t)key};
    } else if (msg == "mode") {
      pending = {Command::KEY, 0, KEY_TRIG_MODE};
    } else if (msg == "run") {
      pending = {Command::KEY, 0, KEY_RUN_STOP};
    } else if (sscanf(msg.c_str(), "mem %lx %d", &addr, &words) == 2 && words > 0) {
      pending = {Command::PEEK, 0, 0, client, (uint32_t)addr, (uint16_t)words};
    } else if (sscanf(msg.c_str(), "fps %d", &fps) == 1 && fps >= 1 && fps <= 30) {
      frameIntervalMs = 1000 / fps;
    }
  }
}

void setup() {
  setCpuFrequencyMhz(CPU_MHZ);
  btStop();
  Serial.begin(115200);
  Serial.println("\nDSO152 Wi-Fi viewer");

  swd::begin(PIN_SWCLK, PIN_SWDIO, SWD_HALF_PERIOD);
  scopeConnect();               // boot-time check, shown on the serial monitor
  swd::release();
  scopeOk = false;              // reconnect when the first browser opens

  // Known networks (or the onboard hotspot) are handled by wifi_net.
  wifi_net::begin(http);
  if (!wifi_net::apMode())
    Serial.printf("Wi-Fi: %s -> http://%s/ or http://dso152.local/\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());

  MDNS.begin("dso152");
  MDNS.addService("http", "tcp", 80);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  logger::begin(http);
  http.on("/", [] { http.send_P(200, "text/html", WEB_PAGE); });
  http.on("/qr.png", [] {
    http.sendHeader("Cache-Control", "max-age=86400");
    http.send_P(200, "image/png", (const char *)QR_PNG, QR_PNG_LEN);
  });
  http.begin();
  ws.begin();
  ws.onEvent(onWs);
}

void loop() {
  http.handleClient();
  ws.loop();
  logger::loop();
  wifi_net::loop();

  bool watching = ws.connectedClients() > 0;
  bool background = !watching && logger::wantsScope();

  // Only touch the scope while someone is watching or the logger needs frames
  if (!watching && !background) {
    if (scopeOk) {
      swd::release();
      scopeOk = false;
    }
    setRadioAwake(false);
    delay(20);
    return;
  }
  setRadioAwake(watching);

  if (!scopeOk) {
    if (millis() - lastConnectTry > 1000) {
      lastConnectTry = millis();
      scopeOk = scopeConnect();
      if (!scopeOk && watching) {
        frame.flags = 0;
        ws.broadcastBIN((uint8_t *)&frame, 4);   // tell browsers: no scope
      }
    }
    delay(5);
    return;
  }

  if (pending.type != Command::NONE) {
    Command c = pending;
    pending.type = Command::NONE;
    if (c.type == Command::PEEK) {
      peek(c.client, c.addr, c.words);
    } else {
      bool ok = c.type == Command::ADJUST ? adjust(c.item, c.key) : press(c.key);
      if (!ok) scopeOk = false;
    }
  }

  // Background logging samples once per second; browsers keep their own rate.
  uint32_t interval = watching ? frameIntervalMs : 1000;
  if (millis() - lastFrameMs >= interval) {
    lastFrameMs = millis();
    uint32_t t0 = micros();
    if (readFrame()) {
      logger::onFrame(frame);
      if (watching) {
        statReadUs += micros() - t0;
        statFrames++;
        ws.broadcastBIN((uint8_t *)&frame, sizeof(frame));
        if (millis() - statT0 >= 5000) {
          Serial.printf("stream: %.1f fps, SWD read %lu us/frame, %u browser(s), RSSI %d dBm\n",
                        statFrames * 1000.0f / (millis() - statT0),
                        (unsigned long)(statFrames ? statReadUs / statFrames : 0),
                        ws.connectedClients(), WiFi.RSSI());
          statFrames = statReadUs = 0;
          statT0 = millis();
        }
      }
    } else {
      Serial.println("SWD read failed, reconnecting");
      scopeOk = false;
    }
  }
  delay(1);
}
