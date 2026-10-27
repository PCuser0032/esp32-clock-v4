#include "wifi_manager.h"
#include "config.h"
#include "reboot_scheduler.h"

#include <WiFi.h>
#include <esp_wifi.h>

static bool sta_enabled = false;
static String sta_ssid, sta_pass;
static unsigned long last_retry = 0;
static unsigned long retry_interval = WIFI_RETRY_MS;  // растёт по экспоненте
static wl_status_t last_status = WL_IDLE_STATUS;

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[WiFi] STA disconnected, reason=%d\n",
                    (int)info.wifi_sta_disconnected.reason);
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFi] STA link up");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFi] STA got IP: %s\n",
                    WiFi.localIP().toString().c_str());
      break;
    default:
      break;
  }
}

void WifiManager::begin(const Settings &s) {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  sta_ssid = s.sta_ssid;
  sta_pass = s.sta_password;
  sta_enabled = (sta_ssid.length() > 0);

  WiFi.onEvent(onWiFiEvent);
  WiFi.mode(sta_enabled ? WIFI_AP_STA : WIFI_AP);

  bool ap_ok = WiFi.softAP(DEVICE_NAME, AP_PASSWORD);
  Serial.printf("[WiFi] softAP %s, AP IP: %s\n",
                ap_ok ? "OK" : "FAIL",
                WiFi.softAPIP().toString().c_str());

  if (sta_enabled) {
    Serial.printf("[WiFi] STA connecting to \"%s\"\n", sta_ssid.c_str());
    WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
  }

  last_status = WiFi.status();
  last_retry = millis();
  retry_interval = WIFI_RETRY_MS;
}

void WifiManager::applyNewCredentialsAndReboot(const String &ssid,
                                               const String &password,
                                               unsigned long reboot_delay_ms) {
  // Обновляем «намерение» — фактически применится после перезагрузки,
  // через setup() → WifiManager::begin() → WiFi.begin().
  sta_ssid = ssid;
  sta_pass = password;
  sta_enabled = (ssid.length() > 0);

  Serial.printf("[WiFi] credentials saved, reboot in %lu ms\n",
                reboot_delay_ms);

  RebootScheduler::schedule(reboot_delay_ms);
}

void WifiManager::tick() {
  wl_status_t st = WiFi.status();
  if (st != last_status) {
    Serial.printf("[WiFi] status %d -> %d\n", (int)last_status, (int)st);
    if (st == WL_CONNECTED) {
      last_retry = millis();
      retry_interval = WIFI_RETRY_MS;  // успех — сбрасываем backoff
    }
    last_status = st;
  }

  if (!sta_enabled)
    return;
  if (WiFi.status() == WL_CONNECTED)
    return;

  // Резервный механизм: если auto-reconnect молчит слишком долго,
  // форсируем один reconnect. Интервал растёт экспоненциально:
  // WIFI_RETRY_MS → 2× → 4× → … → WIFI_RETRY_MAX_MS.
  unsigned long now = millis();
  if (now - last_retry < retry_interval)
    return;
  last_retry = now;

  unsigned long next = (retry_interval >= WIFI_RETRY_MAX_MS / 2)
                         ? WIFI_RETRY_MAX_MS
                         : retry_interval * 2;

  Serial.printf("[WiFi] forcing reconnect; backoff %lu -> %lu s\n",
                retry_interval / 1000, next / 1000);
  WiFi.reconnect();

  retry_interval = next;
}

bool WifiManager::isSTAConnected() {
  return sta_enabled && WiFi.status() == WL_CONNECTED;
}

IPAddress WifiManager::getSTAIP() {
  return WiFi.localIP();
}
IPAddress WifiManager::getAPIP() {
  return WiFi.softAPIP();
}
int WifiManager::getStationCount() {
  return WiFi.softAPgetStationNum();
}
int WifiManager::getRSSI() {
  return isSTAConnected() ? WiFi.RSSI() : 0;
}

WifiManager::WifiState WifiManager::state() {
  if (!sta_enabled)
    return WifiState::IDLE;
  if (WiFi.status() == WL_CONNECTED)
    return WifiState::CONNECTED;
  // Интервал выше базового — значит мы уже сдвинулись по backoff,
  // то есть одна попытка (или больше) не удалась.
  if (retry_interval > WIFI_RETRY_MS)
    return WifiState::BACKOFF;
  return WifiState::CONNECTING;
}

String WifiManager::getStatusText() {
  switch (state()) {
    case WifiState::IDLE:
      return "AP only";
    case WifiState::CONNECTING:
      return "connecting...";
    case WifiState::CONNECTED:
      return "STA+AP";
    case WifiState::BACKOFF:
      return "reconnecting...";
  }
  return "?";
}