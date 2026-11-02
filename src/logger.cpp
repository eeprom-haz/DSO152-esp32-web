// Background logger: samples scope frames into a CSV file on LittleFS, uploads
// it to an FTP server and optionally publishes the measurements over MQTT.
// All work happens from the main loop so the live WebSocket view is untouched.
#include "logger.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Provided by main.cpp: queue a scope key press for the next frame.
void queueKey(uint8_t key);

namespace {

constexpr const char *PREF_NS = "dso152";
constexpr const char *PENDING_CSV = "/pending.csv";
constexpr const char *CSV_TMP = "/dso152_log.csv";
constexpr const char *CSV_HEADER = "time,vavg,vmin,vmax,vrms,vdiv,run\n";
constexpr size_t PENDING_MAX = 600000;
constexpr size_t PENDING_KEEP = 300000;

const int DIV[11] = {1600, 800, 400, 200, 100, 40, 20, 8, 4, 2, 1};
const char *const VOLTS[11] = {"10mV", "20mV", "50mV", "100mV", "200mV", "500mV",
                               "1V", "2V", "5V", "10V", "20V"};

struct Settings {
  bool log_on = false;
  uint16_t log_int = 30;
  String ftp_host, ftp_user, ftp_pass, ftp_dir = "/";
  uint16_t ftp_port = 21, ftp_every = 60;
  bool mqtt_on = false;
  String mqtt_host, mqtt_user, mqtt_pass, mqtt_topic = "dso152";
  uint16_t mqtt_port = 1883, mqtt_int = 5;
  int tz_min = 0;
};

Settings settings;
Preferences prefs;
WebServer *http = nullptr;
WiFiClient mqttClient;
PubSubClient mqtt(mqttClient);

uint32_t lastLogMs = 0, lastFtpMs = 0, lastMqttTry = 0, lastMqttPub = 0;
String ftpStatus = "idle";

float sumAvg = 0, sumRms = 0, maxVmax = -1e9f, minVmin = 1e9f;
uint32_t count = 0;
int lastVi = 0;
bool lastRun = false;

bool haveLatest = false;
float latestAvg = 0, latestMin = 0, latestMax = 0, latestPp = 0, latestRms = 0;
int latestVi = 0;
bool latestRun = false;

// --- settings --------------------------------------------------------------
void loadSettings() {
  prefs.begin(PREF_NS, false);
  settings.log_on = prefs.getBool("log_on", false);
  settings.log_int = prefs.getUShort("log_int", 30);
  settings.ftp_host = prefs.getString("ftp_host", "");
  settings.ftp_port = prefs.getUShort("ftp_port", 21);
  settings.ftp_user = prefs.getString("ftp_user", "");
  settings.ftp_pass = prefs.getString("ftp_pass", "");
  settings.ftp_dir = prefs.getString("ftp_dir", "/");
  settings.ftp_every = prefs.getUShort("ftp_every", 60);
  settings.mqtt_on = prefs.getBool("mqtt_on", false);
  settings.mqtt_host = prefs.getString("mqtt_host", "");
  settings.mqtt_port = prefs.getUShort("mqtt_port", 1883);
  settings.mqtt_user = prefs.getString("mqtt_user", "");
  settings.mqtt_pass = prefs.getString("mqtt_pass", "");
  settings.mqtt_topic = prefs.getString("mqtt_topic", "dso152");
  settings.mqtt_int = prefs.getUShort("mqtt_int", 5);
  settings.tz_min = prefs.getInt("tz_min", 0);
  prefs.end();
  if (!settings.log_int) settings.log_int = 30;
  if (!settings.ftp_port) settings.ftp_port = 21;
  if (!settings.ftp_every) settings.ftp_every = 60;
  if (!settings.mqtt_port) settings.mqtt_port = 1883;
  if (!settings.mqtt_int) settings.mqtt_int = 5;
  if (!settings.ftp_dir.length()) settings.ftp_dir = "/";
  if (!settings.mqtt_topic.length()) settings.mqtt_topic = "dso152";
}

void saveSettings() {
  prefs.begin(PREF_NS, false);
  prefs.putBool("log_on", settings.log_on);
  prefs.putUShort("log_int", settings.log_int);
  prefs.putString("ftp_host", settings.ftp_host);
  prefs.putUShort("ftp_port", settings.ftp_port);
  prefs.putString("ftp_user", settings.ftp_user);
  prefs.putString("ftp_pass", settings.ftp_pass);
  prefs.putString("ftp_dir", settings.ftp_dir);
  prefs.putUShort("ftp_every", settings.ftp_every);
  prefs.putBool("mqtt_on", settings.mqtt_on);
  prefs.putString("mqtt_host", settings.mqtt_host);
  prefs.putUShort("mqtt_port", settings.mqtt_port);
  prefs.putString("mqtt_user", settings.mqtt_user);
  prefs.putString("mqtt_pass", settings.mqtt_pass);
  prefs.putString("mqtt_topic", settings.mqtt_topic);
  prefs.putUShort("mqtt_int", settings.mqtt_int);
  prefs.putInt("tz_min", settings.tz_min);
  prefs.end();
}

// --- time ------------------------------------------------------------------
bool nowLocal(String &out) {
  time_t now = time(nullptr);
  int64_t local = (int64_t)now + (int64_t)settings.tz_min * 60;
  time_t stamp = (time_t)local;
  struct tm tm;
  if (!gmtime_r(&stamp, &tm)) return false;
  int year = tm.tm_year + 1900;
  if (year < 2024) return false;
  char buf[24];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", year, tm.tm_mon + 1,
           tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  out = buf;
  return true;
}

// --- measurements ----------------------------------------------------------
void resetAccumulator() {
  sumAvg = sumRms = 0;
  maxVmax = -1e9f;
  minVmin = 1e9f;
  count = 0;
}

void onFrameImpl(const Frame &f) {
  int vi = f.state[16];
  if (vi > 10) vi = 10;
  float probe = f.state[3] == 1 ? 10.0f : 1.0f;
  float k = probe / (float)DIV[vi] / 100.0f;
  float vavg = f.meas[0] * k;
  float vmax = f.meas[1] * k;
  float vmin = f.meas[2] * k;
  float vpp = f.meas[3] * k;
  float vrms = f.meas[5] * k;
  bool run = f.state[2] == 1;

  haveLatest = true;
  latestAvg = vavg;
  latestMin = vmin;
  latestMax = vmax;
  latestPp = vpp;
  latestRms = vrms;
  latestVi = vi;
  latestRun = run;

  sumAvg += vavg;
  sumRms += vrms;
  if (vmax > maxVmax) maxVmax = vmax;
  if (vmin < minVmin) minVmin = vmin;
  count++;
  lastVi = vi;
  lastRun = run;
}

// --- CSV file --------------------------------------------------------------
void trimPending() {
  File in = LittleFS.open(PENDING_CSV, "r");
  if (!in) return;
  size_t size = in.size();
  if (size <= PENDING_MAX) {
    in.close();
    return;
  }
  in.seek(size - PENDING_KEEP);
  while (in.available()) {
    if (in.read() == '\n') break;
  }
  LittleFS.remove(CSV_TMP);
  File out = LittleFS.open(CSV_TMP, "w");
  if (!out) {
    in.close();
    return;
  }
  bool writeOk = true;
  uint8_t buf[512];
  while (in.available()) {
    size_t n = in.read(buf, sizeof(buf));
    if (!n) break;
    if (out.write(buf, n) != n) {
      writeOk = false;
      break;
    }
  }
  size_t outSize = out.size();
  in.close();
  out.close();
  if (!writeOk || !outSize) {
    LittleFS.remove(CSV_TMP);
    return;
  }
  LittleFS.remove(PENDING_CSV);
  LittleFS.rename(CSV_TMP, PENDING_CSV);
}

void appendLine(const String &line) {
  File f = LittleFS.open(PENDING_CSV, FILE_APPEND);
  if (!f) return;
  f.print(line);
  f.print('\n');
  f.close();
  trimPending();
}

void logInterval() {
  if (!settings.log_on) {
    resetAccumulator();
    return;
  }
  if (!count) return;
  String stamp;
  if (!nowLocal(stamp)) {
    resetAccumulator();
    return;
  }
  char line[160];
  snprintf(line, sizeof(line), "%s,%.3f,%.3f,%.3f,%.3f,%s,%s", stamp.c_str(),
           sumAvg / count, minVmin, maxVmax, sumRms / count, VOLTS[lastVi],
           lastRun ? "RUN" : "STOP");
  appendLine(String(line));
  resetAccumulator();
}

// --- FTP (passive mode, hand written) --------------------------------------
int replyCode(const String &reply) {
  if (reply.length() < 3) return 0;
  return reply.substring(0, 3).toInt();
}

String readLine(WiFiClient &c, uint32_t deadline) {
  String line;
  while (millis() < deadline) {
    while (c.available()) {
      int ch = c.read();
      if (ch < 0) break;
      if (ch == '\n') {
        line.trim();
        return line;
      }
      if (ch != '\r') line += (char)ch;
      if (line.length() > 512) {
        line.trim();
        return line;
      }
    }
    if (!c.connected()) break;
    delay(2);
  }
  line.trim();
  return line;
}

String readReply(WiFiClient &c, uint32_t deadline) {
  String line = readLine(c, deadline);
  if (line.length() >= 4 && line[3] == '-') {
    String code = line.substring(0, 3);
    while (millis() < deadline) {
      String next = readLine(c, deadline);
      if (!next.length()) break;
      line = next;
      if (next.startsWith(code)) break;
    }
  }
  return line;
}

bool ftpUpload() {
  if (!settings.ftp_host.length()) {
    ftpStatus = "error: ftp host not set";
    return false;
  }
  File probe = LittleFS.open(PENDING_CSV, "r");
  if (!probe) {
    ftpStatus = "error: no log";
    return false;
  }
  size_t fileSize = probe.size();
  if (!fileSize) {
    probe.close();
    ftpStatus = "error: no data";
    return false;
  }
  String first = probe.readStringUntil('\n');
  probe.close();
  first.trim();
  String date = first.substring(0, first.length() < 10 ? first.length() : 10);
  bool clean = date.length() == 10;
  for (size_t i = 0; clean && i < date.length(); i++) {
    char c = date[i];
    if (!((c >= '0' && c <= '9') || c == '-')) clean = false;
  }
  if (!clean) {
    String stamp;
    date = nowLocal(stamp) ? stamp.substring(0, 10) : String("unknown");
  }
  String name = "dso152_";
  name += date;
  name += ".csv";

  WiFiClient ctrl;
  WiFiClient data;
  uint32_t deadline = millis() + 15000;
  String err;
  String reply;
  size_t uploaded = fileSize;
  bool ok = false;

  do {
    uint32_t left = deadline - millis();
    if ((int32_t)left <= 0) {
      err = "timeout";
      break;
    }
    if (!ctrl.connect(settings.ftp_host.c_str(), settings.ftp_port,
                      (int32_t)(left < 10000 ? left : 10000))) {
      err = "connect";
      break;
    }

    reply = readReply(ctrl, deadline);
    if (replyCode(reply) != 220) {
      err = reply;
      break;
    }

    bool anon = !settings.ftp_user.length();
    String user = anon ? String("anonymous") : settings.ftp_user;
    String pass = anon ? (settings.ftp_pass.length() ? settings.ftp_pass : String("anonymous@"))
                       : settings.ftp_pass;
    ctrl.print("USER "); ctrl.print(user); ctrl.print("\r\n");
    reply = readReply(ctrl, deadline);
    int code = replyCode(reply);
    if (code == 331) {
      ctrl.print("PASS "); ctrl.print(pass); ctrl.print("\r\n");
      reply = readReply(ctrl, deadline);
      code = replyCode(reply);
    }
    if (code != 230) {
      err = reply;
      break;
    }

    ctrl.print("TYPE I\r\n");
    reply = readReply(ctrl, deadline);
    if (replyCode(reply) != 200) {
      err = reply;
      break;
    }

    if (settings.ftp_dir.length()) {
      ctrl.print("CWD "); ctrl.print(settings.ftp_dir); ctrl.print("\r\n");
      readReply(ctrl, deadline);                 // ignore failure
    }

    ctrl.print("SIZE "); ctrl.print(name); ctrl.print("\r\n");
    reply = readReply(ctrl, deadline);
    code = replyCode(reply);
    bool isNew;
    if (code == 213) {
      isNew = false;
    } else if (code == 550) {
      isNew = true;
    } else {
      err = reply;
      break;
    }

    ctrl.print("PASV\r\n");
    reply = readReply(ctrl, deadline);
    if (replyCode(reply) != 227) {
      err = reply;
      break;
    }
    int l = reply.indexOf('('), r = reply.indexOf(')', l + 1);
    if (l < 0 || r < 0) {
      err = reply;
      break;
    }
    int h1, h2, h3, h4, p1, p2;
    if (sscanf(reply.substring(l + 1, r).c_str(), "%d,%d,%d,%d,%d,%d", &h1, &h2,
               &h3, &h4, &p1, &p2) != 6) {
      err = reply;
      break;
    }
    IPAddress ip((uint8_t)h1, (uint8_t)h2, (uint8_t)h3, (uint8_t)h4);
    uint16_t port = (uint16_t)(p1 * 256 + p2);
    left = deadline - millis();
    if ((int32_t)left <= 0) {
      err = "timeout";
      break;
    }
    if (!data.connect(ip, port, (int32_t)(left < 10000 ? left : 10000))) {
      err = "data connect";
      break;
    }

    ctrl.print("APPE "); ctrl.print(name); ctrl.print("\r\n");
    reply = readReply(ctrl, deadline);
    int appReply = replyCode(reply);
    if (appReply != 150 && appReply != 125 && appReply != 226) {
      err = reply;
      break;
    }

    File f = LittleFS.open(PENDING_CSV, "r");
    if (!f) {
      err = "open log";
      break;
    }
    if (isNew) {
      size_t n = strlen(CSV_HEADER);
      if (data.write((const uint8_t *)CSV_HEADER, n) != n) {
        f.close();
        err = "data write";
        break;
      }
    }
    uint8_t buf[1024];
    while (f.available()) {
      size_t n = f.read(buf, sizeof(buf));
      if (!n) break;
      if (data.write(buf, n) != n) {
        err = "data write";
        break;
      }
    }
    f.close();
    if (err.length()) break;

    data.flush();
    data.stop();

    if (appReply == 150 || appReply == 125) {
      reply = readReply(ctrl, deadline);
      if (replyCode(reply) != 226) {
        err = reply;
        break;
      }
    }

    ctrl.print("QUIT\r\n");
    readReply(ctrl, deadline);
    ok = true;
  } while (0);

  data.stop();
  ctrl.stop();

  if (ok) {
    LittleFS.remove(PENDING_CSV);
    String stamp;
    String hhmm = nowLocal(stamp) ? stamp.substring(11, 16) : String("--:--");
    ftpStatus = "ok "; ftpStatus += hhmm; ftpStatus += ", "; ftpStatus += String((unsigned)uploaded); ftpStatus += " bytes";
  } else {
    ftpStatus = "error: "; ftpStatus += err.length() ? err : String("timeout");
    if (ftpStatus.length() > 120) ftpStatus = ftpStatus.substring(0, 120);
  }
  return ok;
}

// --- MQTT ------------------------------------------------------------------
void mqttCallback(char *topic, byte *payload, unsigned int len) {
  String t(topic);
  String p;
  p.reserve(len);
  for (unsigned int i = 0; i < len; i++) p += (char)payload[i];
  if (t == settings.mqtt_topic + "/cmd" && p == "run") queueKey(0x04);  // KEY_RUN_STOP
}

void mqttMaybeConnect() {
  if (!settings.mqtt_on || !settings.mqtt_host.length()) {
    if (mqtt.connected()) mqtt.disconnect();
    return;
  }
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) return;
  if (millis() - lastMqttTry < 10000) return;
  lastMqttTry = millis();

  mqtt.setServer(settings.mqtt_host.c_str(), settings.mqtt_port);
  mqtt.setSocketTimeout(3);
  mqtt.setCallback(mqttCallback);
  String id = "dso152-"; id += WiFi.macAddress();
  id.replace(":", "");
  bool ok;
  if (settings.mqtt_user.length())
    ok = mqtt.connect(id.c_str(), settings.mqtt_user.c_str(), settings.mqtt_pass.c_str());
  else
    ok = mqtt.connect(id.c_str());
  if (ok) {
    String cmd = settings.mqtt_topic + "/cmd";
    mqtt.subscribe(cmd.c_str());
  }
}

void mqttPublish() {
  if (!settings.mqtt_on || !mqtt.connected() || !haveLatest) return;
  if (millis() - lastMqttPub < (uint32_t)settings.mqtt_int * 1000UL) return;
  lastMqttPub = millis();

  String stamp;
  if (!nowLocal(stamp)) return;
  char json[256];
  snprintf(json, sizeof(json),
           "{\"vavg\":%.3f,\"vmin\":%.3f,\"vmax\":%.3f,\"vpp\":%.3f,"
           "\"vrms\":%.3f,\"vdiv\":\"%s\",\"run\":%s,\"time\":\"%s\"}",
           latestAvg, latestMin, latestMax, latestPp, latestRms, VOLTS[latestVi],
           latestRun ? "true" : "false", stamp.c_str());
  String topic = settings.mqtt_topic + "/measure";
  mqtt.publish(topic.c_str(), json);
}

// --- HTTP handlers ---------------------------------------------------------
String jsonEscape(const String &s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') out += '\\';
    if (c >= 32) out += c;
  }
  return out;
}

bool formBool(const char *name) {
  if (!http->hasArg(name)) return false;
  String v = http->arg(name);
  return v != "0" && v != "false" && v != "off";
}

int formInt(const char *name, int fallback) {
  if (!http->hasArg(name)) return fallback;
  return atoi(http->arg(name).c_str());
}

void handleSettingsGet() {
  String j = "{";
  j += "\"log_on\":";
  j += settings.log_on ? "true" : "false";
  j += ",\"log_int\":";
  j += String(settings.log_int);
  j += ",\"ftp_host\":\"";
  j += jsonEscape(settings.ftp_host);
  j += "\",\"ftp_port\":";
  j += String(settings.ftp_port);
  j += ",\"ftp_user\":\"";
  j += jsonEscape(settings.ftp_user);
  j += "\",\"ftp_pass\":\"\",\"ftp_pass_set\":";
  j += settings.ftp_pass.length() ? "true" : "false";
  j += ",\"ftp_dir\":\"";
  j += jsonEscape(settings.ftp_dir);
  j += "\",\"ftp_every\":";
  j += String(settings.ftp_every);
  j += ",\"mqtt_on\":";
  j += settings.mqtt_on ? "true" : "false";
  j += ",\"mqtt_host\":\"";
  j += jsonEscape(settings.mqtt_host);
  j += "\",\"mqtt_port\":";
  j += String(settings.mqtt_port);
  j += ",\"mqtt_user\":\"";
  j += jsonEscape(settings.mqtt_user);
  j += "\",\"mqtt_pass\":\"\",\"mqtt_pass_set\":";
  j += settings.mqtt_pass.length() ? "true" : "false";
  j += ",\"mqtt_topic\":\"";
  j += jsonEscape(settings.mqtt_topic);
  j += "\",\"mqtt_int\":";
  j += String(settings.mqtt_int);
  j += ",\"tz_min\":";
  j += String(settings.tz_min);
  j += "}";
  http->sendHeader("Cache-Control", "no-store");
  http->send(200, "application/json", j);
}

void handleSettingsPost() {
  settings.log_on = formBool("log_on");
  settings.mqtt_on = formBool("mqtt_on");
  if (http->hasArg("log_int")) {
    int v = formInt("log_int", settings.log_int);
    if (v > 0) settings.log_int = v;
  }
  if (http->hasArg("ftp_host")) settings.ftp_host = http->arg("ftp_host");
  if (http->hasArg("ftp_port")) {
    int v = formInt("ftp_port", settings.ftp_port);
    if (v > 0 && v < 65536) settings.ftp_port = v;
  }
  if (http->hasArg("ftp_user")) settings.ftp_user = http->arg("ftp_user");
  if (http->hasArg("ftp_pass") && http->arg("ftp_pass").length())
    settings.ftp_pass = http->arg("ftp_pass");
  if (http->hasArg("ftp_dir")) {
    settings.ftp_dir = http->arg("ftp_dir");
    if (!settings.ftp_dir.length()) settings.ftp_dir = "/";
  }
  if (http->hasArg("ftp_every")) {
    int v = formInt("ftp_every", settings.ftp_every);
    if (v > 0) settings.ftp_every = v;
  }
  if (http->hasArg("mqtt_host")) settings.mqtt_host = http->arg("mqtt_host");
  if (http->hasArg("mqtt_port")) {
    int v = formInt("mqtt_port", settings.mqtt_port);
    if (v > 0 && v < 65536) settings.mqtt_port = v;
  }
  if (http->hasArg("mqtt_user")) settings.mqtt_user = http->arg("mqtt_user");
  if (http->hasArg("mqtt_pass") && http->arg("mqtt_pass").length())
    settings.mqtt_pass = http->arg("mqtt_pass");
  if (http->hasArg("mqtt_topic")) {
    settings.mqtt_topic = http->arg("mqtt_topic");
    if (!settings.mqtt_topic.length()) settings.mqtt_topic = "dso152";
  }
  if (http->hasArg("mqtt_int")) {
    int v = formInt("mqtt_int", settings.mqtt_int);
    if (v > 0) settings.mqtt_int = v;
  }
  if (http->hasArg("tz_min")) settings.tz_min = formInt("tz_min", settings.tz_min);

  saveSettings();
  if (mqtt.connected()) mqtt.disconnect();
  lastMqttTry = millis() - 10000;      // reconnect on the next loop
  lastLogMs = millis();
  http->send(200, "text/plain", "ok");
}

void handleStatus() {
  String stamp;
  nowLocal(stamp);
  size_t pending = 0;
  File f = LittleFS.open(PENDING_CSV, "r");
  if (f) {
    pending = f.size();
    f.close();
  }
  String mq = !settings.mqtt_on ? "off" : (mqtt.connected() ? "connected" : "disconnected");
  String j = "{\"time\":\"";
  j += jsonEscape(stamp);
  j += "\",\"pending_bytes\":";
  j += String((unsigned)pending);
  j += ",\"ftp_status\":\"";
  j += jsonEscape(ftpStatus);
  j += "\",\"mqtt\":\"";
  j += mq;
  j += "\"}";
  http->sendHeader("Cache-Control", "no-store");
  http->send(200, "application/json", j);
}

void handleFtpUpload() {
  if (WiFi.status() == WL_CONNECTED) ftpUpload();
  else ftpStatus = "error: wifi down";
  http->send(200, "text/plain", ftpStatus);
}

void handleLogCsv() {
  File src = LittleFS.open(PENDING_CSV, "r");
  size_t size = src ? src.size() : 0;
  size_t total = strlen(CSV_HEADER) + size;

  WiFiClient client = http->client();
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/csv\r\n");
  client.print("Content-Disposition: attachment; filename=\"dso152_log.csv\"\r\n");
  client.print("Content-Length: ");
  client.print((unsigned long)total);
  client.print("\r\nConnection: close\r\n\r\n");
  client.print(CSV_HEADER);
  if (src) {
    uint8_t buf[512];
    while (src.available()) {
      size_t n = src.read(buf, sizeof(buf));
      if (!n) break;
      client.write(buf, n);
    }
    src.close();
  }
  client.flush();
  client.stop();
}

void handleLogClear() {
  LittleFS.remove(PENDING_CSV);
  http->send(200, "text/plain", "ok");
}

}  // namespace

namespace logger {

void begin(WebServer &server) {
  http = &server;
  loadSettings();
  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");

  lastLogMs = lastFtpMs = millis();
  lastMqttPub = 0;
  lastMqttTry = millis() - 10000;
  resetAccumulator();

  server.on("/settings", HTTP_GET, handleSettingsGet);
  server.on("/settings", HTTP_POST, handleSettingsPost);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/ftp/upload", HTTP_POST, handleFtpUpload);
  server.on("/log.csv", HTTP_GET, handleLogCsv);
  server.on("/log/clear", HTTP_POST, handleLogClear);
}

void loop() {
  mqttMaybeConnect();
  mqtt.loop();
  mqttPublish();

  if (millis() - lastLogMs >= (uint32_t)settings.log_int * 1000UL) {
    lastLogMs = millis();
    logInterval();
  }

  if (settings.ftp_host.length() &&
      millis() - lastFtpMs >= (uint32_t)settings.ftp_every * 60000UL) {
    lastFtpMs = millis();
    if (WiFi.status() == WL_CONNECTED) {
      File f = LittleFS.open(PENDING_CSV, "r");
      bool data = f && f.size();
      if (f) f.close();
      if (data) ftpUpload();
    }
  }
}

void onFrame(const Frame &f) { onFrameImpl(f); }

bool wantsScope() { return settings.log_on || settings.mqtt_on; }

}  // namespace logger
