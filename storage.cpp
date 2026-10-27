#include "storage.h"
#include "config.h"
#include "defaults.h"

#include <Preferences.h>

// ВАЖНО про стоимость NVS.
// Preferences::putString/putBool/putUChar/putShort на ESP32-Arduino
// делает nvs_commit() после каждой операции. save() пишет
// 4 + NTP_MAX_SERVERS + 6 = 13 ключей, то есть 13 коммитов
// на одно сохранение. Для веб-UI, где «Сохранить» жмётся вручную,
// это нормально (редкие события, ~10 мс на коммит).
// Если появятся сценарии с автосохранением — батчить через
// prefs.putBytes() одной структурой, либо минимизировать
// число отдельных save()-вызовов.

static const char *NS = "esp32clock";
static Preferences prefs;
static Settings g_cfg;  // единственный источник правды в рантайме

static const char *NTP_DEFAULTS[NTP_MAX_SERVERS] = {
  DEFAULT_NTP_0,
  DEFAULT_NTP_1,
  DEFAULT_NTP_2
};

static Settings loadFromNvs() {
  Settings s;

  // --- Сеть ---
  s.sta_ssid     = prefs.getString("ssid", "");
  s.sta_password = prefs.getString("pass", "");

  // --- Время ---
  s.tz_string = prefs.getString("tzs", DEFAULT_TZ_STRING);
  if (s.tz_string.length() == 0)
    s.tz_string = DEFAULT_TZ_STRING;
  s.use_24h = prefs.getBool("h24", DEFAULT_24H);

  // NTP — принципиально различать «ключа нет» (дефолт) и
  // «значение пустое» (пользователь осознанно стёр сервер),
  // поэтому здесь остаётся isKey().
  char key[8];
  for (int i = 0; i < NTP_MAX_SERVERS; i++) {
    snprintf(key, sizeof(key), "ntp%d", i);
    if (prefs.isKey(key)) {
      s.ntp_servers[i] = prefs.getString(key, "");
    } else {
      s.ntp_servers[i] = NTP_DEFAULTS[i];
    }
  }

  // --- Звук ---
  s.sound_enabled = prefs.getBool("snd", DEFAULT_SOUND_ENABLED);

  // --- Будильник ---
  // constrain / маска — защита от мусора в NVS (старая прошивка,
  // ручное вмешательство, битый сектор). Стоит копейки.
  s.alarm_enabled = prefs.getBool("al_on", DEFAULT_ALARM_ENABLED);
  s.alarm_h       = (uint8_t)constrain(
                      prefs.getUChar("al_h", DEFAULT_ALARM_H), 0, 23);
  s.alarm_m       = (uint8_t)constrain(
                      prefs.getUChar("al_m", DEFAULT_ALARM_M), 0, 59);
  s.alarm_days    = (uint8_t)(prefs.getUChar("al_d", DEFAULT_ALARM_DAYS) & 0x7F);

  // --- Калибровка температуры ---
  s.temp_offset_x10 = (int16_t)constrain(
                        prefs.getShort("t_off", DEFAULT_TEMP_OFFSET_X10),
                        TEMP_OFFSET_MIN_X10, TEMP_OFFSET_MAX_X10);

  return s;
}

void Storage::begin() {
  prefs.begin(NS, false);
  g_cfg = loadFromNvs();
}

const Settings &Storage::get() {
  return g_cfg;
}

void Storage::save(const Settings &s) {
  // Сеть
  prefs.putString("ssid", s.sta_ssid);
  prefs.putString("pass", s.sta_password);

  // Время
  prefs.putString("tzs", s.tz_string);
  prefs.putBool("h24", s.use_24h);

  char key[8];
  for (int i = 0; i < NTP_MAX_SERVERS; i++) {
    snprintf(key, sizeof(key), "ntp%d", i);
    prefs.putString(key, s.ntp_servers[i]);
  }

  // Звук
  prefs.putBool("snd", s.sound_enabled);

  // Будильник — сохраняем как есть. Валидацию делает вызывающий код
  // (web_ui и .ino), но constrain на чтении всё равно страхует.
  prefs.putBool ("al_on", s.alarm_enabled);
  prefs.putUChar("al_h",  s.alarm_h);
  prefs.putUChar("al_m",  s.alarm_m);
  prefs.putUChar("al_d",  s.alarm_days);

  // Калибровка
  prefs.putShort("t_off", s.temp_offset_x10);

  g_cfg = s;
}

void Storage::reset() {
  prefs.clear();
  g_cfg = loadFromNvs();
}