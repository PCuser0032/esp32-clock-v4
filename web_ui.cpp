#include "web_ui.h"
#include "config.h"
#include "storage.h"
#include "time_manager.h"
#include "tz_util.h"
#include "wifi_manager.h"
#include "mdns_manager.h"
#include "reboot_scheduler.h"
#include "climate.h"
#include "alarm.h"
#include "buzzer.h"
#include "button.h"

#include "html.h"
#include "css.h"
#include "js.h"

#include <math.h>
#include <time.h>

#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>

static WebServer server(80);

// Указатели на кнопки для diag. nullptr = секция отсутствует.
static Button *g_btn1 = nullptr;
static Button *g_btn2 = nullptr;

// ---------- helpers ----------

static const char *WD[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

static void fmtTimeStr(const struct tm &t, bool h24, char *out, size_t n) {
  if (h24) {
    snprintf(out, n, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
  } else {
    int h = t.tm_hour % 12;
    if (h == 0) h = 12;
    snprintf(out, n, "%02d:%02d:%02d %s",
             h, t.tm_min, t.tm_sec, t.tm_hour < 12 ? "AM" : "PM");
  }
}

static void fmtDateStr(const struct tm &t, char *out, size_t n) {
  snprintf(out, n, "%02d.%02d.%04d",
           t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
}

static void fmtEffectiveOffset(int seconds, char *out, size_t n) {
  int total_min = seconds / 60;
  int hh = total_min / 60;
  int mm = abs(total_min % 60);
  snprintf(out, n, "UTC%+03d:%02d", hh, mm);
}

static bool sanitizeTz(String &tz) {
  tz.trim();
  return TzUtil::isValid(tz);
}

// ---------- static content ----------

static void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", WEB_HTML);
}

static void handleCss() {
  server.sendHeader("Cache-Control", "public, max-age=3600");
  server.send_P(200, "text/css", WEB_CSS);
}

static void handleJs() {
  server.sendHeader("Cache-Control", "public, max-age=3600");
  server.send_P(200, "application/javascript", WEB_JS);
}

// ---------- /api/status ----------

static void handleStatus() {
  const Settings &s = Storage::get();
  struct tm lt;
  bool have = TimeManager::readLocal(lt);

  char tbuf[24] = "--:--:--";
  char dbuf[16] = "--.--.----";
  const char *weekday = "---";
  if (have) {
    fmtTimeStr(lt, s.use_24h, tbuf, sizeof(tbuf));
    fmtDateStr(lt, dbuf, sizeof(dbuf));
    weekday = WD[(lt.tm_wday >= 0 && lt.tm_wday <= 6) ? lt.tm_wday : 0];
  }

  char tzbuf[16];
  fmtEffectiveOffset(TzUtil::effectiveOffsetSeconds(), tzbuf, sizeof(tzbuf));

  JsonDocument doc;
  doc["time"] = tbuf;
  doc["date"] = dbuf;
  doc["weekday"] = weekday;
  doc["wifi_mode"] = WifiManager::getStatusText();
  doc["sta_ip"] = WifiManager::isSTAConnected()
                    ? WifiManager::getSTAIP().toString()
                    : "";
  doc["mdns_host"] = MdnsManager::getHost();
  doc["mdns_host_target"] = String(MDNS_HOSTNAME) + ".local";
  doc["ap_ip"] = WifiManager::getAPIP().toString();
  doc["ap_clients"] = WifiManager::getStationCount();
  doc["ntp_synced"] = TimeManager::isSynced() && WifiManager::isSTAConnected();
  doc["rtc_ok"] = TimeManager::rtcOk();
  doc["rtc_lost"] = TimeManager::rtcLost();
  doc["tz"] = s.tz_string;
  doc["tz_effective"] = tzbuf;

  JsonArray ntpArr = doc["ntp"].to<JsonArray>();
  for (int i = 0; i < NTP_MAX_SERVERS; i++)
    ntpArr.add(s.ntp_servers[i]);

  doc["h24"] = s.use_24h;
  doc["ssid"] = s.sta_ssid;

  doc["heap_free"] = ESP.getFreeHeap();
  doc["heap_min_free"] = ESP.getMinFreeHeap();
  doc["heap_max_alloc"] = ESP.getMaxAllocHeap();

  // --- Климат ---
  // temperature/humidity = null, если valid=false. JS различает
  // "нет данных" (null) и "0.0 °C" (реальное значение).
  const Climate::Reading &r = Climate::current();
  if (r.valid) {
    doc["temperature"] = r.temp_c;
    doc["humidity"] = r.humidity_pct;
  } else {
    doc["temperature"] = nullptr;
    doc["humidity"] = nullptr;
  }
  doc["climate_valid"] = r.valid;
  doc["temp_offset_x10"] = (int)s.temp_offset_x10;

  // --- Звук ---
  doc["sound_enabled"] = s.sound_enabled;

  // --- Будильник ---
  doc["alarm_enabled"] = s.alarm_enabled;
  doc["alarm_h"] = (int)s.alarm_h;
  doc["alarm_m"] = (int)s.alarm_m;
  doc["alarm_days"] = (int)s.alarm_days;
  doc["alarm_ringing"] = Alarm::isRinging();

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ---------- /api/diag ----------

static void handleDiag() {
  JsonDocument doc;

  doc["wifi_mode"] = (int)WiFi.getMode();
  doc["sta_status"] = (int)WiFi.status();
  doc["sta_ssid"] = WiFi.SSID();
  doc["sta_ip"] = WiFi.localIP().toString();
  doc["sta_gw"] = WiFi.gatewayIP().toString();
  doc["sta_dns"] = WiFi.dnsIP().toString();
  doc["sta_rssi"] = WiFi.RSSI();
  doc["ap_ip"] = WiFi.softAPIP().toString();
  doc["ap_clients"] = WiFi.softAPgetStationNum();

  const Settings &s = Storage::get();

  JsonArray res = doc["ntp_resolve"].to<JsonArray>();
  for (int i = 0; i < NTP_MAX_SERVERS; i++) {
    if (s.ntp_servers[i].length() == 0) continue;
    JsonObject o = res.add<JsonObject>();
    o["host"] = s.ntp_servers[i];
    o["ip"] = TimeManager::resolveHost(s.ntp_servers[i]);
  }

  doc["sntp_status"] = TimeManager::sntpStatusStr();
  doc["ntp_synced"] = TimeManager::isSynced();
  doc["last_start_ms"] = TimeManager::lastStartNtpMs();
  doc["uptime_ms"] = millis();

  time_t now = time(nullptr);
  doc["sys_time_epoch"] = (uint32_t)now;
  struct tm g, l;
  gmtime_r(&now, &g);
  localtime_r(&now, &l);
  char b[32];
  snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d:%02d UTC",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
           g.tm_hour, g.tm_min, g.tm_sec);
  doc["sys_utc"] = b;
  snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d:%02d",
           l.tm_year + 1900, l.tm_mon + 1, l.tm_mday,
           l.tm_hour, l.tm_min, l.tm_sec);
  doc["sys_local"] = b;

  doc["heap_free"] = ESP.getFreeHeap();
  doc["heap_min_free"] = ESP.getMinFreeHeap();

  // --- Climate ---
  {
    JsonObject cl = doc["climate"].to<JsonObject>();
    const Climate::Reading &r = Climate::current();
    cl["valid"] = r.valid;
    if (r.valid) {
      cl["temp_c"] = r.temp_c;
      cl["humidity_pct"] = r.humidity_pct;
    }
    // Сырые значения: NaN → null (JSON не поддерживает NaN).
    float rt = Climate::rawTempC();
    float rh = Climate::rawHumidityPct();
    if (isnan(rt)) cl["raw_temp_c"] = nullptr;
    else           cl["raw_temp_c"] = rt;
    if (isnan(rh)) cl["raw_humidity_pct"] = nullptr;
    else           cl["raw_humidity_pct"] = rh;

    cl["consecutive_errors"]  = Climate::consecutiveErrors();
    cl["last_success_age_ms"] = Climate::lastSuccessAgeMs();
    cl["last_attempt_age_ms"] = Climate::lastAttemptAgeMs();
    cl["applied_offset_x10"]  = (int)Climate::appliedOffsetX10();
    cl["offset_rejected"]     = Climate::lastOffsetRejected();
  }

  // --- Sound ---
  doc["sound_enabled"] = s.sound_enabled;

  // --- Alarm ---
  {
    JsonObject al = doc["alarm"].to<JsonObject>();
    al["enabled"] = Alarm::isEnabled();
    al["hour"] = (int)Alarm::hour();
    al["minute"] = (int)Alarm::minute();
    al["days_mask"] = (int)Alarm::daysMask();
    al["ringing"] = Alarm::isRinging();
    al["ring_remaining_ms"] = Alarm::ringRemainingMs();
    uint32_t lf = Alarm::lastFiredMinute();
    if (lf == 0xFFFFFFFF) al["last_fired_minute"] = nullptr;
    else                  al["last_fired_minute"] = lf;
  }

  // --- Buttons ---
  if (g_btn1 && g_btn2) {
    JsonObject btns = doc["buttons"].to<JsonObject>();
    JsonObject b1 = btns["btn1"].to<JsonObject>();
    b1["pressed"] = g_btn1->isPressed();
    b1["hold_ms"] = g_btn1->holdMs();
    JsonObject b2 = btns["btn2"].to<JsonObject>();
    b2["pressed"] = g_btn2->isPressed();
    b2["hold_ms"] = g_btn2->holdMs();
  }

  String out;
  serializeJsonPretty(doc, out);
  server.send(200, "application/json", out);
}

// ---------- /api/time ----------

static void handleSetTime() {
  if (!server.hasArg("plain"))
    return server.send(400, "text/plain", "");
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")))
    return server.send(400, "text/plain", "");
  const char *dt = doc["datetime"] | "";
  if (!dt || !*dt)
    return server.send(400, "text/plain", "");

  int Y = 0, M = 0, D = 0, h = 0, m = 0, sec = 0;
  int parsed = sscanf(dt, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &sec);
  if (parsed < 5)
    return server.send(400, "text/plain", "");
  if (parsed == 5)
    sec = 0;

  bool ok = TimeManager::setLocal(Y, M, D, h, m, sec);
  server.send(200, "application/json",
              ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

// ---------- /api/sync / ntp-restart ----------

static void handleSync() {
  if (!WifiManager::isSTAConnected()) {
    server.send(200, "application/json",
                "{\"ok\":false,\"err\":\"no sta\"}");
    return;
  }
  TimeManager::startNTP(Storage::get());
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleNtpRestart() {
  TimeManager::startNTP(Storage::get());
  server.send(200, "application/json", "{\"ok\":true}");
}

// ---------- /api/network ----------

static void handleNetwork() {
  if (server.method() == HTTP_DELETE) {
    Settings s = Storage::get();
    s.sta_ssid = "";
    s.sta_password = "";
    Storage::save(s);
    WifiManager::applyNewCredentialsAndReboot("", "", 1500);
    server.send(200, "application/json",
                "{\"ok\":true,\"reboot_in_ms\":1500}");
    return;
  }
  if (!server.hasArg("plain"))
    return server.send(400, "text/plain", "");
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")))
    return server.send(400, "text/plain", "");
  Settings s = Storage::get();
  s.sta_ssid = String((const char *)(doc["ssid"] | ""));
  s.sta_password = String((const char *)(doc["password"] | ""));
  Storage::save(s);

  WifiManager::applyNewCredentialsAndReboot(s.sta_ssid, s.sta_password, 2000);
  server.send(200, "application/json",
              "{\"ok\":true,\"reboot_in_ms\":2000}");
}

// ---------- /api/settings ----------

static void handleSettings() {
  if (!server.hasArg("plain"))
    return server.send(400, "text/plain", "");
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")))
    return server.send(400, "text/plain", "");

  Settings s = Storage::get();

  String old_ntp[NTP_MAX_SERVERS];
  for (int i = 0; i < NTP_MAX_SERVERS; i++)
    old_ntp[i] = s.ntp_servers[i];

  bool sound_changed = false;

  // --- Время ---
  if (doc["tz"].is<const char *>()) {
    String tz = doc["tz"].as<String>();
    if (sanitizeTz(tz))
      s.tz_string = tz;
    else {
      server.send(400, "application/json", "{\"ok\":false,\"err\":\"bad tz\"}");
      return;
    }
  }
  if (doc["ntp"].is<JsonArray>()) {
    JsonArray arr = doc["ntp"].as<JsonArray>();
    for (int i = 0; i < NTP_MAX_SERVERS; i++) {
      if (i < (int)arr.size() && arr[i].is<const char *>()) {
        String v = arr[i].as<String>();
        v.trim();
        s.ntp_servers[i] = v;
      } else {
        s.ntp_servers[i] = "";
      }
    }
  }
  if (doc["h24"].is<bool>())
    s.use_24h = doc["h24"].as<bool>();

  // --- Звук ---
  if (doc["sound_enabled"].is<bool>()) {
    bool nv = doc["sound_enabled"].as<bool>();
    if (s.sound_enabled != nv) sound_changed = true;
    s.sound_enabled = nv;
  }

  // --- Будильник ---
  if (doc["alarm_enabled"].is<bool>())
    s.alarm_enabled = doc["alarm_enabled"].as<bool>();
  if (doc["alarm_h"].is<int>()) {
    int v = doc["alarm_h"].as<int>();
    if (v >= 0 && v <= 23) s.alarm_h = (uint8_t)v;
  }
  if (doc["alarm_m"].is<int>()) {
    int v = doc["alarm_m"].as<int>();
    if (v >= 0 && v <= 59) s.alarm_m = (uint8_t)v;
  }
  if (doc["alarm_days"].is<int>())
    s.alarm_days = (uint8_t)(doc["alarm_days"].as<int>() & 0x7F);

  // --- Калибровка ---
  // Принимаем либо temp_offset_x10 (int), либо temp_offset (float).
  if (doc["temp_offset_x10"].is<int>()) {
    int v = doc["temp_offset_x10"].as<int>();
    s.temp_offset_x10 = (int16_t)constrain(v,
                          TEMP_OFFSET_MIN_X10, TEMP_OFFSET_MAX_X10);
  } else if (doc["temp_offset"].is<float>()) {
    float v = doc["temp_offset"].as<float>();
    int v10 = (int)roundf(v * 10.0f);
    s.temp_offset_x10 = (int16_t)constrain(v10,
                          TEMP_OFFSET_MIN_X10, TEMP_OFFSET_MAX_X10);
  }

  Storage::save(s);
  TimeManager::applySettings(s);

  // Синхронизация feedback-флага зуммера с NVS.
  if (sound_changed)
    Buzzer::setFeedbackEnabled(s.sound_enabled);

  bool ntp_changed = false;
  for (int i = 0; i < NTP_MAX_SERVERS; i++) {
    if (old_ntp[i] != s.ntp_servers[i]) { ntp_changed = true; break; }
  }
  if (ntp_changed && WifiManager::isSTAConnected()) {
    Serial.println("[NTP] servers changed, restarting SNTP");
    TimeManager::startNTP(s);
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

// ---------- /api/factory-reset ----------

static void handleFactoryReset() {
  Storage::reset();
  server.send(200, "application/json", "{\"ok\":true}");
  server.client().flush();
  RebootScheduler::schedule(200);
}

// ---------- /api/test/beep / alarm ----------

static void handleTestBeep() {
  JsonDocument doc;
  if (server.hasArg("plain")) {
    deserializeJson(doc, server.arg("plain"));
  }
  uint16_t freq = doc["freq"] | BUZZER_FREQ_FEEDBACK;
  uint16_t ms   = doc["ms"]   | 150;
  // Защита от «вечного писка»: не больше 5 секунд на один запрос.
  if (ms > 5000) ms = 5000;

  Buzzer::tone(freq, ms);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleTestAlarm() {
  // Длинный бип для проверки громкости зуммера. Не паттерн
  // будильника — паттерн проверяется «в бою» через выставление
  // alarm_* и ожидание срабатывания.
  Buzzer::tone(BUZZER_FREQ_ALARM_HIGH, 3000);
  server.send(200, "application/json", "{\"ok\":true}");
}

// ---------- not found / captive portal ----------

static void handleNotFound() {
  String uri = server.uri();
  if (uri.startsWith("/api/")) {
    server.send(404, "application/json",
                "{\"ok\":false,\"err\":\"not found\"}");
    return;
  }
  if (server.method() == HTTP_GET) {
    String host = server.hostHeader();
    host.toLowerCase();
    String mdnsHost = String(MDNS_HOSTNAME) + ".local";
    mdnsHost.toLowerCase();
    String staIp = WifiManager::isSTAConnected()
                     ? WifiManager::getSTAIP().toString()
                     : "";
    bool isRealAddress = (host == mdnsHost) || (staIp.length() > 0 && host == staIp);
    if (!isRealAddress) {
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
      return;
    }
  }
  server.send(404, "text/plain", "Not Found");
}

// ---------- lifecycle ----------

void WebUI::setButtons(Button *b1, Button *b2) {
  g_btn1 = b1;
  g_btn2 = b2;
}

void WebUI::begin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/style.css", HTTP_GET, handleCss);
  server.on("/app.js", HTTP_GET, handleJs);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/diag", HTTP_GET, handleDiag);
  server.on("/api/time", HTTP_POST, handleSetTime);
  server.on("/api/sync", HTTP_POST, handleSync);
  server.on("/api/ntp-restart", HTTP_POST, handleNtpRestart);
  server.on("/api/network", HTTP_POST, handleNetwork);
  server.on("/api/network", HTTP_DELETE, handleNetwork);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/factory-reset", HTTP_POST, handleFactoryReset);
  server.on("/api/test/beep", HTTP_POST, handleTestBeep);
  server.on("/api/test/alarm", HTTP_POST, handleTestAlarm);
  server.onNotFound(handleNotFound);

  server.enableCORS(true);
  server.begin();
}

void WebUI::tick() {
  server.handleClient();
}