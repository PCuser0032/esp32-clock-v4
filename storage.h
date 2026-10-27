#pragma once
#include <Arduino.h>
#include "config.h"

struct Settings {
  // --- Сеть ---
  String sta_ssid;
  String sta_password;

  // --- Время ---
  String ntp_servers[NTP_MAX_SERVERS];
  String tz_string;
  bool use_24h;

  // --- Звук ---
  // Управляет только обратной связью (бип на кнопки, boot beep,
  // переключение звука). Будильник звенит независимо от этого флага.
  bool sound_enabled;

  // --- Будильник ---
  bool alarm_enabled;
  uint8_t alarm_h;     // 0..23
  uint8_t alarm_m;     // 0..59
  uint8_t alarm_days;  // битовая маска, бит N = tm_wday N (0=Sun..6=Sat)

  // --- Калибровка температуры ---
  // Хранится в 0.1 °C (например, +0.8 °C → 8).
  // Применяется только к показаниям DHT22 (t + offset).
  int16_t temp_offset_x10;
};

namespace Storage {
void begin();

// Кэш в RAM. Дешево, без обращений к NVS.
const Settings &get();

// Записать в NVS и обновить кэш.
void save(const Settings &s);

// Стереть NVS и сбросить кэш в дефолты.
void reset();
}