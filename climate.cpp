#include "climate.h"
#include "config.h"
#include "storage.h"

#include <DHT.h>

// =============================================================
//  Внутреннее состояние
// =============================================================

static DHT dht(PIN_DHT, DHT22);

static Climate::Reading g_reading;

// Сырые значения (без offset) — для диагностики.
static float g_raw_temp = NAN;
static float g_raw_hum  = NAN;

// Состояние калибровки.
static int16_t g_applied_offset_x10 = 0;
static bool    g_last_offset_rejected = false;

// Счётчики ошибок и тайминги.
static int           g_consecutive_errors = 0;
static unsigned long g_last_attempt_ms    = 0;
static unsigned long g_last_success_ms    = 0;
static bool          g_ever_succeeded     = false;

// =============================================================
//  Helpers
// =============================================================

static bool rawInPhysicalRange(float t, float h) {
  if (isnan(t) || isnan(h)) return false;
  if (t < DHT_VALID_TEMP_MIN || t > DHT_VALID_TEMP_MAX) return false;
  if (h < 0.0f || h > 100.0f) return false;
  return true;
}

static void countError() {
  if (g_consecutive_errors < 255) g_consecutive_errors++;
  if (g_consecutive_errors >= DHT_ERRORS_TO_INVALID) {
    g_reading.valid = false;
  }
}

// =============================================================
//  API
// =============================================================

void Climate::begin() {
  dht.begin();
  g_reading = Reading{};
  g_raw_temp = NAN;
  g_raw_hum  = NAN;
  g_applied_offset_x10    = 0;
  g_last_offset_rejected  = false;
  g_consecutive_errors    = 0;
  g_last_attempt_ms       = millis();
  g_last_success_ms       = 0;
  g_ever_succeeded        = false;
}

void Climate::tick() {
  unsigned long now = millis();
  if ((now - g_last_attempt_ms) < DHT_READ_MS) return;
  g_last_attempt_ms = now;

  // --- Одно физическое чтение ---
  // readTemperature() запускает физический read;
  // readHumidity() в пределах 2 с берёт результат из кэша
  // Adafruit-библиотеки (гейт по _min_delay).
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  g_raw_temp = t;
  g_raw_hum  = h;

  if (!rawInPhysicalRange(t, h)) {
    countError();
    return;
  }

  // --- Калибровка ---
  int16_t offset_x10 = Storage::get().temp_offset_x10;
  g_applied_offset_x10   = offset_x10;
  g_last_offset_rejected = false;

  float t_cal = t + (offset_x10 / 10.0f);

  // Если offset вытолкнул за физический диапазон — отклоняем.
  if (t_cal < DHT_VALID_TEMP_MIN || t_cal > DHT_VALID_TEMP_MAX) {
    g_last_offset_rejected = true;
    t_cal = t;   // fallback к сырому
  }

  // --- Успех ---
  g_reading.temp_c       = t_cal;
  g_reading.humidity_pct = h;
  g_reading.valid        = true;
  g_consecutive_errors   = 0;
  g_last_success_ms      = now;
  g_ever_succeeded       = true;
}

const Climate::Reading &Climate::current() {
  return g_reading;
}

// =============================================================
//  Диагностика
// =============================================================

int Climate::consecutiveErrors() {
  return g_consecutive_errors;
}

unsigned long Climate::lastSuccessAgeMs() {
  // Если ни разу не было успеха — возвращаем millis(),
  // то есть «с момента старта не было ни одного валидного чтения».
  return g_ever_succeeded ? (millis() - g_last_success_ms) : millis();
}

unsigned long Climate::lastAttemptAgeMs() {
  return millis() - g_last_attempt_ms;
}

float Climate::rawTempC() {
  return g_raw_temp;
}

float Climate::rawHumidityPct() {
  return g_raw_hum;
}

int16_t Climate::appliedOffsetX10() {
  return g_applied_offset_x10;
}

bool Climate::lastOffsetRejected() {
  return g_last_offset_rejected;
}