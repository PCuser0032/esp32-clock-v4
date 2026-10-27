#include "time_manager.h"
#include "config.h"
#include "defaults.h"
#include "tz_util.h"
#include "alarm.h"

#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include <RTClib.h>
#include <WiFi.h>
#include <esp_sntp.h>

static RTC_DS3231 rtc;
static bool rtc_ok = false;
static bool rtc_lost = false;
static bool ntp_synced = false;
static String tz_string = DEFAULT_TZ_STRING;

static unsigned long last_start_ntp_ms = 0;
static unsigned long last_ok_ms = 0;

static volatile bool ntp_callback_fired = false;
static volatile bool ntp_synced_after_start = false;

// Флаг «идёт попытка синхронизации». Внешне виден как NtpState::IN_PROGRESS.
static bool sync_pending = false;

// Длительность попытки до автоматического перехода в TIMEOUT.
static const unsigned long NTP_ATTEMPT_TIMEOUT_MS = 20000;
// Длительность показа OK на LCD после успеха.
static const unsigned long NTP_OK_DISPLAY_MS = 1000;

static void onSntpSync(struct timeval * /*tv*/) {
  ntp_callback_fired = true;
  ntp_synced_after_start = true;
}

static void applyTZ(const String &tz) {
  tz_string = TzUtil::isValid(tz) ? tz : String("UTC0");
  setenv("TZ", tz_string.c_str(), 1);
  tzset();
}

static void syncSystemFromRtc() {
  if (!rtc_ok)
    return;
  struct timeval tv;
  tv.tv_sec = rtc.now().unixtime();
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

bool TimeManager::begin() {
  rtc_ok = rtc.begin();
  if (!rtc_ok)
    return false;

  if (rtc.lostPower()) {
    rtc_lost = true;
    DateTime fb(F(__DATE__), F(__TIME__));
    if (fb.year() < 2020 || fb.year() > 2100)
      fb = DateTime(2024, 1, 1, 0, 0, 0);
    rtc.adjust(fb);
    Serial.printf("[RTC] lostPower -> fallback %04d-%02d-%02d %02d:%02d:%02d\n",
                  fb.year(), fb.month(), fb.day(),
                  fb.hour(), fb.minute(), fb.second());
  }

  syncSystemFromRtc();
  // TZ применяется из .ino через applySettings().
  return true;
}

void TimeManager::applySettings(const Settings &s) {
  applyTZ(s.tz_string);
}

bool TimeManager::readUTC(struct tm &out) {
  if (!rtc_ok)
    return false;
  time_t epoch = rtc.now().unixtime();
  gmtime_r(&epoch, &out);
  return true;
}

bool TimeManager::readLocal(struct tm &out) {
  if (!rtc_ok)
    return false;
  time_t epoch = rtc.now().unixtime();
  localtime_r(&epoch, &out);
  return true;
}

bool TimeManager::setLocal(int year, int mon, int day,
                           int hour, int min, int sec) {
  if (!rtc_ok)
    return false;

  if (year < 2020 || year > 2100)
    return false;
  if (mon < 1 || mon > 12)
    return false;
  if (day < 1 || day > 31)
    return false;
  if (hour < 0 || hour > 23)
    return false;
  if (min < 0 || min > 59)
    return false;
  if (sec < 0 || sec > 59)
    return false;

  struct tm lt = {};
  lt.tm_year = year - 1900;
  lt.tm_mon = mon - 1;
  lt.tm_mday = day;
  lt.tm_hour = hour;
  lt.tm_min = min;
  lt.tm_sec = sec;
  lt.tm_isdst = -1;

  time_t epoch = mktime(&lt);
  if (epoch <= 0)
    return false;

  rtc.adjust(DateTime((uint32_t)epoch));

  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);

  // Ручная установка времени могла попасть точно на минуту будильника
  // (например, пользователь ставит 07:00 при alarm_h=7, alarm_m=0).
  // Без уведомления Alarm::tick() через долю секунды увидит совпадение
  // и немедленно зазвонит. notifyTimeChanged() ставит last_fired_minute
  // в текущую минуту — звонка не будет ни в эту минуту, ни в следующие
  // (там alarm_m уже не совпадает). Завтра в 07:00 — зазвенит.
  //
  // ВАЖНО: вызов строго ПОСЛЕ settimeofday() — notifyTimeChanged()
  // внутри вызывает time(nullptr) и берёт уже новое значение.
  Alarm::notifyTimeChanged();

  rtc_lost = false;
  ntp_synced = false;
  ntp_callback_fired = false;
  ntp_synced_after_start = false;
  sync_pending = false;
  last_ok_ms = 0;  // ручная установка гасит индикатор OK
  return true;
}

void TimeManager::startNTP(const Settings &s) {
  String tz = TzUtil::isValid(s.tz_string) ? s.tz_string : String("UTC0");

  const char *srv[NTP_MAX_SERVERS] = { nullptr };
  int n = 0;
  for (int i = 0; i < NTP_MAX_SERVERS; i++) {
    if (s.ntp_servers[i].length() > 0 && n < NTP_MAX_SERVERS) {
      srv[n++] = s.ntp_servers[i].c_str();
    }
  }

  if (n == 0) {
    Serial.println("[NTP] no servers configured, skipping SNTP start");
    sync_pending = false;
    return;
  }

  ntp_callback_fired = false;
  ntp_synced_after_start = false;

  sntp_set_time_sync_notification_cb(onSntpSync);
  esp_sntp_stop();

  const char *s1 = srv[0];
  const char *s2 = (n > 1) ? srv[1] : srv[0];
  const char *s3 = (n > 2) ? srv[2] : srv[0];

  configTzTime(tz.c_str(), s1, s2, s3);

  last_start_ntp_ms = millis();
  sync_pending = true;
  tz_string = tz;

  String list;
  for (int i = 0; i < n; i++) {
    if (i > 0)
      list += " / ";
    list += srv[i];
  }
  Serial.printf("[NTP] startNTP, servers: %s\n", list.c_str());
}

bool TimeManager::trySyncNTP() {
  if (!rtc_ok)
    return false;

  if (!ntp_callback_fired || !ntp_synced_after_start)
    return false;
  if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
    return false;

  time_t now = time(nullptr);
  if (now < 1600000000)
    return false;

  rtc.adjust(DateTime((uint32_t)now));
  ntp_synced = true;
  rtc_lost = false;
  sync_pending = false;
  last_ok_ms = millis();

  ntp_callback_fired = false;
  ntp_synced_after_start = false;

  struct tm g;
  gmtime_r(&now, &g);
  Serial.printf("[NTP] RTC updated: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
                g.tm_hour, g.tm_min, g.tm_sec);
  return true;
}

bool TimeManager::isSynced() {
  return ntp_synced;
}
bool TimeManager::rtcOk() {
  return rtc_ok;
}
bool TimeManager::rtcLost() {
  return rtc_lost;
}

TimeManager::NtpState TimeManager::ntpState() {
  if (sync_pending) {
    if (last_start_ntp_ms && (millis() - last_start_ntp_ms) > NTP_ATTEMPT_TIMEOUT_MS) {
      return NtpState::TIMEOUT;
    }
    return NtpState::IN_PROGRESS;
  }
  if (last_ok_ms && (millis() - last_ok_ms) < NTP_OK_DISPLAY_MS) {
    return NtpState::OK;
  }
  return NtpState::IDLE;
}

bool TimeManager::isSyncInProgress() {
  return ntpState() == NtpState::IN_PROGRESS;
}

const char *TimeManager::sntpStatusStr() {
  switch (sntp_get_sync_status()) {
    case SNTP_SYNC_STATUS_RESET:
      return "RESET";
    case SNTP_SYNC_STATUS_COMPLETED:
      return "COMPLETED";
    case SNTP_SYNC_STATUS_IN_PROGRESS:
      return "IN_PROGRESS";
    default:
      return "?";
  }
}

unsigned long TimeManager::lastStartNtpMs() {
  return last_start_ntp_ms;
}

String TimeManager::resolveHost(const String &host) {
  IPAddress ip;
  if (WiFi.hostByName(host.c_str(), ip)) {
    return ip.toString();
  }
  return String("0.0.0.0");
}