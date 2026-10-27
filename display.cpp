#include "display.h"
#include "config.h"

#include <string.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

// tm_wday: 0=Sun, 1=Mon, ..., 6=Sat
static const char *DOW[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

static char shadow[LCD_ROWS][LCD_COLS + 1] = { { 0 } };

static unsigned long last_full_redraw = 0;
static const unsigned long FULL_REDRAW_MS = 60000;

static void invalidateShadow() {
  memset(shadow, 0, sizeof(shadow));
  last_full_redraw = millis();
}

static void renderLine(uint8_t row, const char *text) {
  if (row >= LCD_ROWS) return;
  for (uint8_t i = 0; i < LCD_COLS; i++) {
    char c = text[i];
    if (shadow[row][i] != c) {
      lcd.setCursor(i, row);
      lcd.write((uint8_t)c);
      shadow[row][i] = c;
    }
  }
}

// Левое выравнивание + padding пробелами справа до LCD_COLS.
static void padAndRender(uint8_t row, const char *text) {
  char buf[LCD_COLS + 1];
  size_t n = strnlen(text, LCD_COLS);
  memcpy(buf, text, n);
  for (size_t i = n; i < LCD_COLS; i++) buf[i] = ' ';
  buf[LCD_COLS] = 0;
  renderLine(row, buf);
}

// Центрирование: (LCD_COLS - len) / 2 пробелов слева.
// Если len > LCD_COLS — обрезается.
static void padAndCenterRender(uint8_t row, const char *text) {
  char buf[LCD_COLS + 1];
  memset(buf, ' ', LCD_COLS);
  buf[LCD_COLS] = 0;
  size_t n = strnlen(text, LCD_COLS);
  size_t pad = (LCD_COLS - n) / 2;
  memcpy(buf + pad, text, n);
  renderLine(row, buf);
}

// Слева status_left, справа status_right, между ними пробелы.
// Если суммарно не влезает — обрезается левая часть
// (приоритет у правой: IP должен быть читаем).
static void renderLeftRight(uint8_t row,
                            const char *left, const char *right) {
  char buf[LCD_COLS + 1];
  memset(buf, ' ', LCD_COLS);
  buf[LCD_COLS] = 0;

  size_t llen = strnlen(left, LCD_COLS);
  size_t rlen = strnlen(right, LCD_COLS);

  // Минимум 1 пробел между. Обрезаем left, если не влезает.
  if (llen + rlen > (size_t)(LCD_COLS - 1)) {
    int avail = LCD_COLS - 1 - (int)rlen;
    llen = (avail > 0) ? (size_t)avail : 0;
    if (avail < 0) {
      rlen = LCD_COLS - 1;
      llen = 0;
    }
  }

  if (llen) memcpy(buf, left, llen);
  if (rlen) memcpy(buf + LCD_COLS - rlen, right, rlen);
  renderLine(row, buf);
}

bool Display::begin() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  invalidateShadow();
  return true;
}

void Display::showBoot(const String &line2) {
  lcd.clear();
  invalidateShadow();

  char l0[LCD_COLS + 1];
  char l1[LCD_COLS + 1];
  snprintf(l0, sizeof(l0), "%-*.*s", LCD_COLS, LCD_COLS, DEVICE_NAME);
  snprintf(l1, sizeof(l1), "%-*.*s", LCD_COLS, LCD_COLS, line2.c_str());
  renderLine(0, l0);
  renderLine(1, l1);
}

void Display::showTime(const struct tm &t, bool use24h, SyncIcon icon,
                       float temp_c, float humidity_pct, bool climate_valid,
                       bool sound_enabled,
                       const String &status_left, const String &status_right) {
  if (millis() - last_full_redraw > FULL_REDRAW_MS) {
    invalidateShadow();
  }

  // --- Строка 0: время (12) + 3 пробела + NTP (4) + звук (1) ---
  char timePart[16];
  if (use24h) {
    snprintf(timePart, sizeof(timePart), "%02d:%02d:%02d",
             t.tm_hour, t.tm_min, t.tm_sec);
  } else {
    int h = t.tm_hour % 12;
    if (h == 0) h = 12;
    snprintf(timePart, sizeof(timePart), "%02d:%02d:%02d %s",
             h, t.tm_min, t.tm_sec, t.tm_hour < 12 ? "AM" : "PM");
  }

  const char *ind = "    ";
  switch (icon) {
    case SyncIcon::SYNC: ind = "SYNC"; break;
    case SyncIcon::SYNC_OK: ind = " OK "; break;
    case SyncIcon::RTC_LOST: ind = "  ! "; break;
    case SyncIcon::NONE:
    default: ind = "    "; break;
  }

  {
    { char line[LCD_COLS + 1];
  // Раскладка 20 символов:
  //   [0..11]  время (12)
  //   [12]     пробел
  //   [13..16] NTP-иконка (4)
  //   [17]     пробел
  //   [18]     звук (1)
  //   [19]     пробел (резерв)
  snprintf(line, sizeof(line), "%-12.12s %-4.4s %c ",
           timePart, ind, sound_enabled ? 'S' : ' ');
  renderLine(0, line);
}
}

// --- Строка 1: дата (10) + 7 пробелов + DOW (3) = 20 ---
{
  char datePart[16];
  snprintf(datePart, sizeof(datePart), "%02d.%02d.%04d",
           t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
  const char *dow = DOW[(t.tm_wday >= 0 && t.tm_wday <= 6) ? t.tm_wday : 0];

  char line[LCD_COLS + 1];
  // DOW прижат вправо через %3.3s с правым выравниванием.
  snprintf(line, sizeof(line), "%-10.10s%10.10s", datePart, dow);
  renderLine(1, line);
}

// --- Строка 2: климат по центру ---
{
  char content[LCD_COLS + 1];
  if (climate_valid) {
    // "%5.1f" — фиксированная ширина: гарантирует 18 символов,
    // чтобы центрирование выглядело ровно при любых значениях.
    snprintf(content, sizeof(content), "T:%5.1fC  H:%5.1f%%",
             temp_c, humidity_pct);
  } else {
    snprintf(content, sizeof(content), "T: --.-C  H: --.-%%");
  }
  padAndCenterRender(2, content);
}

// --- Строка 3: left + right ---
renderLeftRight(3, status_left.c_str(), status_right.c_str());
}

void Display::showStatusLine(const String &text) {
  // Временные сообщения — левое выравнивание (совместимо с прежним API).
  padAndRender(3, text.c_str());
}

void Display::showMessage(const String &l1, const String &l2,
                          const String &l3, const String &l4) {
  lcd.clear();
  invalidateShadow();

  padAndRender(0, l1.c_str());
  padAndRender(1, l2.c_str());
  padAndRender(2, l3.c_str());
  padAndRender(3, l4.c_str());
}

void Display::clear() {
  lcd.clear();
  invalidateShadow();
}