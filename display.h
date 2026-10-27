#pragma once
#include <Arduino.h>
#include <time.h>

namespace Display {

enum class SyncIcon {
  NONE,     // пусто
  SYNC,     // идёт синхронизация NTP
  SYNC_OK,  // только что синхронизировались
  RTC_LOST  // DS3231 потерял питание
};

bool begin();
void showBoot(const String &line2);

// Строка 0: время (слева) + NTP-иконка и звук (справа).
// Строка 1: дата (слева) + DOW (справа).
// Строка 2: климат по центру.
// Строка 3: status_left (слева) + status_right (справа).
void showTime(const struct tm &local, bool use24h, SyncIcon icon,
              float temp_c, float humidity_pct, bool climate_valid,
              bool sound_enabled,
              const String &status_left = "",
              const String &status_right = "");

// Рисует только строку 3. Для временных сообщений (кнопки, combo).
void showStatusLine(const String &text);

// Полный экран (4 строки), очищает перед отрисовкой.
void showMessage(const String &l1, const String &l2 = "",
                 const String &l3 = "", const String &l4 = "");

void clear();
}