#include "alarm.h"
#include "config.h"
#include "storage.h"
#include "buzzer.h"

#include <time.h>

// =============================================================
//  Внутреннее состояние
// =============================================================

static bool          g_ringing          = false;
static unsigned long g_ring_start_ms    = 0;

// Уникальный ключ минуты = epoch / 60. 0xFFFFFFFF — «никогда».
// uint32_t хватает на ~8000 лет минут.
static uint32_t      g_last_fired_minute = 0xFFFFFFFF;

// epoch < это значение = время не установлено (1970 + ~20 лет).
// Отсекает старт устройства до SNTP/RTC-синка.
static const time_t MIN_VALID_EPOCH = 1600000000;

// =============================================================
//  Helpers
// =============================================================

static void startRinging() {
  g_ringing = true;
  g_ring_start_ms = millis();
  Buzzer::alarmStart();
  Serial.println("[Alarm] start");
}

// =============================================================
//  API
// =============================================================

void Alarm::begin() {
  g_ringing = false;
  g_ring_start_ms = 0;
  g_last_fired_minute = 0xFFFFFFFF;
  Serial.println("[Alarm] begin");
}

void Alarm::tick() {
  unsigned long now_ms = millis();

  // --- 1. Автостоп ---
  if (g_ringing) {
    if ((now_ms - g_ring_start_ms) >= ALARM_MAX_RING_MS) {
      stop();
    }
    return;
  }

  // --- 2. Проверка условий срабатывания ---
  const Settings &s = Storage::get();
  if (!s.alarm_enabled) return;

  time_t now_epoch = time(nullptr);
  if (now_epoch < MIN_VALID_EPOCH) return;  // время не установлено

  struct tm lt;
  localtime_r(&now_epoch, &lt);

  // День недели: tm_wday 0..6, бит N = день N.
  if ((s.alarm_days & (1 << lt.tm_wday)) == 0) return;

  if (lt.tm_hour != (int)s.alarm_h) return;
  if (lt.tm_min  != (int)s.alarm_m) return;

  uint32_t now_min = (uint32_t)(now_epoch / 60);
  if (g_last_fired_minute == now_min) return;  // уже срабатывал в эту минуту

  g_last_fired_minute = now_min;
  startRinging();
}

void Alarm::stop() {
  if (!g_ringing) return;
  g_ringing = false;
  Buzzer::alarmStop();
  Serial.println("[Alarm] stopped");
}

bool Alarm::isRinging() {
  return g_ringing;
}

void Alarm::notifyTimeChanged() {
  time_t now_epoch = time(nullptr);
  if (now_epoch >= MIN_VALID_EPOCH) {
    g_last_fired_minute = (uint32_t)(now_epoch / 60);
    Serial.printf("[Alarm] notifyTimeChanged, last_fired=%lu\n",
                  (unsigned long)g_last_fired_minute);
  } else {
    // Время невалидно (ручная установка провалилась?) — сбрасываем
    // в «никогда», чтобы следующий валидный tick() мог сработать.
    g_last_fired_minute = 0xFFFFFFFF;
  }
}

// =============================================================
//  Диагностика
// =============================================================

bool Alarm::isEnabled() {
  return Storage::get().alarm_enabled;
}

uint8_t Alarm::hour() {
  return Storage::get().alarm_h;
}

uint8_t Alarm::minute() {
  return Storage::get().alarm_m;
}

uint8_t Alarm::daysMask() {
  return Storage::get().alarm_days;
}

uint32_t Alarm::lastFiredMinute() {
  return g_last_fired_minute;
}

unsigned long Alarm::ringRemainingMs() {
  if (!g_ringing) return 0;
  unsigned long elapsed = millis() - g_ring_start_ms;
  if (elapsed >= ALARM_MAX_RING_MS) return 0;
  return ALARM_MAX_RING_MS - elapsed;
}