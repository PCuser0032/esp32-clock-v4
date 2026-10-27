#pragma once

// =============================================================
//  Значения по умолчанию для пользовательских настроек.
//  Применяются при первом запуске или после factory reset.
//  Дальше живут в NVS и меняются через web-UI.
// =============================================================

// --- Часовой пояс и формат ---
#define DEFAULT_TZ_STRING "MSK-3"
#define DEFAULT_24H true

// --- NTP-серверы по умолчанию (индекс 0 — основной) ---
#define DEFAULT_NTP_0 "pool.ntp.org"
#define DEFAULT_NTP_1 "time.google.com"
#define DEFAULT_NTP_2 "time.cloudflare.com"

// --- Звук ---
// true = бип при нажатиях кнопок, на старте, при переключении режима.
// Будильник звенит независимо от этого флага (см. alarm.cpp).
#define DEFAULT_SOUND_ENABLED true

// --- Будильник ---
// Дефолт — выключен, но дни уже расставлены «рабочие»:
// пользователь включает alarm_enabled и сразу получает Пн–Пт в 07:00.
// Если у него график 2/2, «сутки-трое» и т.п. — меняет чекбоксы в web-UI.
//
// tm_wday: 0=Sun, 1=Mon, ..., 6=Sat. Бит N = день N.
// 0x3E == 0b0111110 == биты 1..5 == Пн–Пт.
#define DEFAULT_ALARM_ENABLED false
#define DEFAULT_ALARM_H       7
#define DEFAULT_ALARM_M       0
#define DEFAULT_ALARM_DAYS    0x3E

// --- Калибровка температуры (в 0.1 °C) ---
// Хранится как int16_t. 0 = без калибровки.
#define DEFAULT_TEMP_OFFSET_X10 0