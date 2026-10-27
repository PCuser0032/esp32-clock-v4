#pragma once

// =============================================================
//  Аппаратная конфигурация и компиляционные константы.
//  Всё, что не меняется в рантайме через web-UI.
// =============================================================

// --- Идентификация устройства ---
#define DEVICE_NAME "ESP32-CLOCK"
#define AP_PASSWORD "esp32password"
#define MDNS_HOSTNAME "esp32-clock"

// --- I2C ---
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22
#define I2C_CLOCK_HZ 100000

// --- LCD2004 (I2C) ---
#define LCD_I2C_ADDR 0x27
#define LCD_COLS 20
#define LCD_ROWS 4

// --- DS3231 ---
#define RTC_I2C_ADDR 0x68

// --- Кнопки ---
// BTN_ACTIVE_LOW = 0 → активный ВЫСОКИЙ (INPUT_PULLDOWN);
// BTN_ACTIVE_LOW = 1 → активный НИЗКИЙ (INPUT_PULLUP).
// На твоём железе проверено: 0 — рабочий вариант.
#define BTN_ACTIVE_LOW 0
#define PIN_BUTTON  4
#define PIN_BUTTON2 13

#define BTN_DEBOUNCE_MS  30
#define BTN_SHORT_MAX_MS 1000

// Недеструктивные действия: force NTP (Button 1),
// показать статус звука (Button 2), arm combo-reboot.
// Единый порог — меньше когнитивной нагрузки.
#define BTN_LONG_MS 3000

// Деструктивное действие: combo full reset.
#define BTN_COMBO_RESET_MS 10000

// -------------------------------------------------------------

// --- DHT22 ---
#define PIN_DHT 27
#define DHT_READ_MS 2000            // даташит: не чаще 0.5 Гц
#define DHT_ERRORS_TO_INVALID 3     // 3 подряд ошибки → "--"
#define DHT_VALID_TEMP_MIN (-40.0f) // физический диапазон DHT22
#define DHT_VALID_TEMP_MAX ( 80.0f)

// --- Buzzer (пассивный, LEDC/PWM) ---
#define PIN_BUZZER 32
#define BUZZER_RES_BITS 10          // 10 бит → макс. скважность 1023
#define BUZZER_FREQ_FEEDBACK    2000
#define BUZZER_FREQ_ALARM_LOW   1000
#define BUZZER_FREQ_ALARM_HIGH  2000
#define BUZZER_ALARM_ON_MS       500
#define BUZZER_ALARM_OFF_MS     1500

// --- Будильник ---
#define ALARM_MAX_RING_MS 60000     // автостоп, если никто не подошёл
#define ALARM_BLINK_MS      500     // период мигания статус-строки LCD

// --- NTP ---
#define NTP_MAX_SERVERS 3

// --- Тайминги ---
#define DISPLAY_REFRESH_MS 250
#define NTP_SYNC_RETRY_MS  30000
#define NTP_SYNC_PERIOD_MS (24UL * 60 * 60 * 1000)

// --- Wi-Fi reconnect ---
#define WIFI_RETRY_MS     30000
#define WIFI_RETRY_MAX_MS 300000

// --- Калибровка температуры (в 0.1 °C) ---
// Погрешность DHT22 по даташиту: ±0.5 °C типично,
// ±1 °C макс в 0..50 °C, ±2 °C макс вне 0..50 °C.
// Диапазон покрывает worst-case — «ровно по паспорту».
#define TEMP_OFFSET_MIN_X10 (-20)   // -2.0 °C
#define TEMP_OFFSET_MAX_X10 ( 20)   // +2.0 °C