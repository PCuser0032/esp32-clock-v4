#pragma once
#include <Arduino.h>

// =============================================================
//  Обёртка над DHT22 (пин PIN_DHT).
//
//  Публичный API минимален: begin(), tick(), current().
//  Всё остальное — диагностика для /api/diag.
//
//  Модуль сам читает Storage::get().temp_offset_x10 на каждом
//  успешном чтении — калибровка подхватывается без setOffset().
//
//  Будильник, звук, кнопки — не знают про Climate. .ino подаёт
//  current() в Display::showTime() на шаге 9.
// =============================================================

namespace Climate {

struct Reading {
  float temp_c       = NAN;   // с калибровкой
  float humidity_pct = NAN;   // без калибровки (её нет в даташите)
  bool  valid        = false; // false → на LCD "--"
};

void begin();

// Вызывается из loop(). Опрос не чаще DHT_READ_MS.
// Внутри — один физический read, валидация, калибровка, кэш.
void tick();

// Кэш последнего валидного чтения. Всегда валиден по ссылке —
// если !valid, поля содержат NAN.
const Reading &current();

// --- Диагностика (для /api/diag) ---
int           consecutiveErrors();     // счётчик подряд ошибок
unsigned long lastSuccessAgeMs();      // ms с последнего успеха
unsigned long lastAttemptAgeMs();      // ms с последней попытки
float         rawTempC();              // без калибровки
float         rawHumidityPct();        // без калибровки
int16_t       appliedOffsetX10();      // применённый offset (для отладки)
bool          lastOffsetRejected();    // offset отклонён на последнем read

}  // namespace Climate