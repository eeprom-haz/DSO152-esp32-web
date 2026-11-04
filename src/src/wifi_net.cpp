// Known networks live in Preferences (plain passwords, like the FTP login).
// The list is most recently added first so the page and WiFiMulti agree.
#include "wifi_net.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <esp_wifi.h>
#include <string.h>

#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#endif

namespace {

constexpr uint8_t MAX_NETWORKS = 6;
constexpr uint8_t MAX_SCAN = 48;
constexpr const char *PREF_NS = "dso152wifi";
constexpr const char *AP_SSID = "DSO152";
constexpr const char *AP_PASS = "dso152scope";
constexpr uint32_t STA_RETRY_MS = 20000;
constexpr uint32_t AP_RETRY_MS = 60000;

struct Network {
  String ssid;
  String pass;
};

struct ScanNet {
  String ssid;
  int rssi = 0;
  bool open = false;
};

Network networks[MAX_NETWORKS];
uint8_t netCount = 0;

Preferences prefs;
WebServer *http = nullptr;
WiFiMulti *multi = nullptr;
DNSServer dns;
bool hotspot = false;
bool connectRequested = false;
uint32_t lastStaOk = 0;
uint32_t lastApCheck = 0;
uint8_t failedTries = 0;

String jsonEscape(const String &s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '"' || c == '\\') out += '\\';
    if (c >= 32) out += (char)c;
  }
  return out;
}

// C3 SuperMini boards have a poor antenna match: at full power (19.5 dBm) phones
// can't join the hotspot and it can't join a router. Start at 15 dBm for more
// range; if that can't connect at boot, drop to 8.5 dBm (known good) and retry.
// The DevKit stays at normal power (lowering it broke its weak link).
#if CONFIG_IDF_TARGET_ESP32C3
wifi_power_t c3Power = WIFI_POWER_15dBm;
#endif

void setPower() {
#if CONFIG_IDF_TARGET_ESP32C3
  WiFi.setTxPower(c3Power);
#endif
}

void printWifi() {
  Serial.printf("Wi-Fi: %s -> http://%s/ or http://dso152.local/\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

// Re-register mDNS after a reconnect that happened outside main.cpp.
void restartMdns() {
  MDNS.end();
  MDNS.begin("dso152");
  MDNS.addService("http", "tcp", 80);
}

void saveNetworks() {
  prefs.begin(PREF_NS, false);
  prefs.putUChar("n", netCount);
  for (uint8_t i = 0; i < MAX_NETWORKS; i++) {
    String sk = "s";
    sk += i;
    String pk = "p";
    pk += i;
    if (i < netCount) {
      prefs.putString(sk.c_str(), networks[i].ssid);
      prefs.putString(pk.c_str(), networks[i].pass);
    } else {
      prefs.remove(sk.c_str());
      prefs.remove(pk.c_str());
    }
  }
  prefs.end();
}

void rebuildMulti() {
  delete multi;
  multi = new WiFiMulti();
  for (uint8_t i = 0; i < netCount; i++)
    multi->addAP(networks[i].ssid.c_str(), networks[i].pass.c_str());
}

void loadNetworks() {
  prefs.begin(PREF_NS, true);
  uint8_t n = prefs.getUChar("n", 0);
  if (n > MAX_NETWORKS) n = MAX_NETWORKS;
  Network tmp[MAX_NETWORKS];
  uint8_t count = 0;
  for (uint8_t i = 0; i < n; i++) {
    String sk = "s";
    sk += i;
    String pk = "p";
    pk += i;
    String ssid = prefs.getString(sk.c_str(), "");
    String pass = prefs.getString(pk.c_str(), "");
    if (!ssid.length()) continue;
    tmp[count].ssid = ssid;
    tmp[count].pass = pass;
    count++;
  }
  prefs.end();
  netCount = count;
  for (uint8_t i = 0; i < count; i++) networks[i] = tmp[i];
  for (uint8_t i = count; i < MAX_NETWORKS; i++) networks[i] = Network();
  if (netCount) return;

  // First boot after the old WiFiManager build: keep the network it saved.
  // WiFiManager stores the station config in NVS, readable once STA mode is on.
  wifi_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  char ssid[33] = {0};
  char pass[65] = {0};
  if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK) {
    memcpy(ssid, cfg.sta.ssid, 32);
    memcpy(pass, cfg.sta.password, 64);
  }
#ifdef WIFI_DEFAULT_SSID
  // Local builds only: src/wifi_secrets.h (git-ignored) seeds the owner's network.
  if (!ssid[0]) {
    strncpy(ssid, WIFI_DEFAULT_SSID, 32);
    strncpy(pass, WIFI_DEFAULT_PASS, 64);
  }
#endif
  if (!ssid[0]) return;
  networks[0].ssid = ssid;
  networks[0].pass = pass;
  netCount = 1;
  saveNetworks();
}

void startHotspot() {
  if (hotspot) return;
  hotspot = true;
  failedTries = 0;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  setPower();
  dns.start(53, "*", WiFi.softAPIP());
  lastApCheck = millis();
  Serial.println("No known Wi-Fi. Hotspot DSO152 (password dso152scope) -> http://192.168.4.1/");
}

void stopHotspot() {
  if (!hotspot) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  setPower();
  hotspot = false;
  lastStaOk = millis();
  failedTries = 0;
  printWifi();
  restartMdns();
}

void handleWifiGet() {
  bool ap = hotspot;
  String j = "{\"mode\":\"";
  j += ap ? "ap" : "sta";
  j += "\",\"ssid\":\"";
  if (!ap) j += jsonEscape(WiFi.SSID());
  j += "\",\"ip\":\"";
  j += ap ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  j += "\",\"rssi\":";
  j += String(ap ? 0 : WiFi.RSSI());
  j += ",\"saved\":[";
  for (uint8_t i = 0; i < netCount; i++) {
    if (i) j += ',';
    j += '"';
    j += jsonEscape(networks[i].ssid);
    j += '"';
  }
  j += "]}";
  http->sendHeader("Cache-Control", "no-store");
  http->send(200, "application/json", j);
}

void handleWifiScan() {
  int n = WiFi.scanComplete();
  if (n == -2) {
    WiFi.scanNetworks(true);
    http->send(200, "application/json", "{\"busy\":true}");
    return;
  }
  if (n == -1) {
    http->send(200, "application/json", "{\"busy\":true}");
    return;
  }

  ScanNet nets[MAX_SCAN];
  uint8_t count = 0;
  for (int i = 0; i < n && count < MAX_SCAN; i++) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    int rssi = WiFi.RSSI(i);
    bool open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    int found = -1;
    for (uint8_t k = 0; k < count; k++) {
      if (nets[k].ssid == ssid) {
        found = k;
        break;
      }
    }
    if (found >= 0) {
      if (rssi > nets[found].rssi) {
        nets[found].rssi = rssi;
        nets[found].open = open;
      }
    } else {
      nets[count].ssid = ssid;
      nets[count].rssi = rssi;
      nets[count].open = open;
      count++;
    }
  }

  // Strongest first.
  for (uint8_t i = 1; i < count; i++) {
    ScanNet cur = nets[i];
    int k = i - 1;
    while (k >= 0 && nets[k].rssi < cur.rssi) {
      nets[k + 1] = nets[k];
      k--;
    }
    nets[k + 1] = cur;
  }

  String j = "{\"busy\":false,\"nets\":[";
  for (uint8_t i = 0; i < count; i++) {
    if (i) j += ',';
    j += "{\"ssid\":\"";
    j += jsonEscape(nets[i].ssid);
    j += "\",\"rssi\":";
    j += String(nets[i].rssi);
    j += ",\"open\":";
    j += nets[i].open ? "true" : "false";
    j += "}";
  }
  j += "]}";
  WiFi.scanDelete();
  http->sendHeader("Cache-Control", "no-store");
  http->send(200, "application/json", j);
}

void handleWifiAdd() {
  String ssid = http->hasArg("ssid") ? http->arg("ssid") : String();
  String pass = http->hasArg("pass") ? http->arg("pass") : String();
  if (!ssid.length() || ssid.length() > 32) {
    http->send(400, "text/plain", "ssid must be 1-32 characters");
    return;
  }
  if (pass.length() && (pass.length() < 8 || pass.length() > 63)) {
    http->send(400, "text/plain", "password must be empty or 8-63 characters");
    return;
  }

  int found = -1;
  for (uint8_t i = 0; i < netCount; i++) {
    if (networks[i].ssid == ssid) {
      found = i;
      break;
    }
  }
  Network entry;
  entry.ssid = ssid;
  entry.pass = pass;
  if (found >= 0) {
    for (int i = found; i > 0; i--) networks[i] = networks[i - 1];
    networks[0] = entry;
  } else {
    if (netCount < MAX_NETWORKS) netCount++;
    for (int i = netCount - 1; i > 0; i--) networks[i] = networks[i - 1];
    networks[0] = entry;
  }
  saveNetworks();
  rebuildMulti();
  http->send(200, "text/plain", "ok");
}

void handleWifiDel() {
  String ssid = http->hasArg("ssid") ? http->arg("ssid") : String();
  int found = -1;
  for (uint8_t i = 0; i < netCount; i++) {
    if (networks[i].ssid == ssid) {
      found = i;
      break;
    }
  }
  if (found < 0) {
    http->send(404, "text/plain", "not found");
    return;
  }
  for (int i = found; i < netCount - 1; i++) networks[i] = networks[i + 1];
  netCount--;
  networks[netCount] = Network();
  saveNetworks();
  rebuildMulti();
  http->send(200, "text/plain", "ok");
}

void handleWifiConnect() {
  connectRequested = true;
  http->send(200, "text/plain", "trying");
}

}  // namespace

namespace wifi_net {

void begin(WebServer &server) {
  http = &server;
  server.on("/wifi", HTTP_GET, handleWifiGet);
  server.on("/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/wifi/add", HTTP_POST, handleWifiAdd);
  server.on("/wifi/del", HTTP_POST, handleWifiDel);
  server.on("/wifi/connect", HTTP_POST, handleWifiConnect);

  WiFi.mode(WIFI_STA);
  setPower();
  WiFi.setHostname("dso152");
  loadNetworks();
  rebuildMulti();

  uint8_t r = multi->run(15000);
#if CONFIG_IDF_TARGET_ESP32C3
  if (netCount && r != WL_CONNECTED && c3Power != WIFI_POWER_8_5dBm) {
    Serial.println("Wi-Fi failed at 15 dBm, retrying at 8.5 dBm");
    WiFi.disconnect();
    c3Power = WIFI_POWER_8_5dBm;
    setPower();
    r = multi->run(15000);
  }
#endif
  if (netCount && r == WL_CONNECTED) {
    lastStaOk = millis();
    failedTries = 0;
    printWifi();
  } else {
    startHotspot();
  }
}

void loop() {
  if (hotspot) dns.processNextRequest();

  // /wifi/connect: try once now, even if a phone is still on the hotspot.
  if (connectRequested) {
    connectRequested = false;
    if (hotspot && netCount) {
      if (multi->run(8000) == WL_CONNECTED) {
        stopHotspot();
        return;
      }
      WiFi.disconnect();         // stop background retries hopping the AP channel
      lastApCheck = millis();    // next automatic try in 60 s
    }
  }

  if (hotspot) {
    if (millis() - lastApCheck >= AP_RETRY_MS) {
      lastApCheck = millis();
      if (WiFi.softAPgetStationNum() == 0 && netCount) {
        if (multi->run(8000) == WL_CONNECTED) stopHotspot();
        else WiFi.disconnect();
      }
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    lastStaOk = millis();
    failedTries = 0;
    return;
  }
  if (millis() - lastStaOk < STA_RETRY_MS) return;

  lastStaOk = millis();
  bool ok = netCount && multi->run(10000) == WL_CONNECTED;
  if (ok) {
    failedTries = 0;
    printWifi();
    restartMdns();
  } else if (++failedTries >= 3) {
    startHotspot();
  }
}

bool apMode() { return hotspot; }

}  // namespace wifi_net
