#pragma once
#include <Arduino.h>

// =============================================================
//  Будильник. FSM: IDLE → RINGING → STOPPED.
//
//  Условия срабатывания (проверяются в tick()):
//    * alarm_enabled == true
//    * день недели включён в alarm_days (бит tm_wday)
//    * локальное tm_hour:tm_min совпадает с alarm_h:alarm_m
//    * last_fired_minute != now (защита от повтора в ту же минуту)
//
//  Источник времени — системное time() + localtime_r().
//  Это работает и с RTC, и с SNTP, и после setLocal().
//  Если время ещё не установлено (epoch < 1600000000) — будильник молчит.
//
//  Сработав, зовёт Buzzer::alarmStart() и держит RINGING до:
//    * ручного stop() (Button 2 short в .ino),
//    * автостопа через ALARM_MAX_RING_MS (60 с).
//
//  notifyTimeChanged() вызывается из TimeManager::setLocal()
//  (шаг 8), чтобы ручная установка 07:00 не запускала звонок
//  в момент установки.
// =============================================================

namespace Alarm {

void begin();

// Вызывается из loop() каждый тик. Дешёвый: проверяет
// только состояние FSM + условия срабатывания.
void tick();

// Ручная остановка (кнопка 2 в .ino). Идемпотентна.
void stop();

bool isRinging();

// Сброс last_fired_minute на текущую минуту. Вызывается
// из TimeManager::setLocal(). При следующем tick() будильник
// не сработает в эту минуту, даже если время попало в окно.
void notifyTimeChanged();

// --- Диагностика (для /api/diag) ---
bool           isEnabled();        // alarm_enabled из Storage
uint8_t        hour();
uint8_t        minute();
uint8_t        daysMask();
uint32_t       lastFiredMinute();  // 0xFFFFFFFF = ни разу
unsigned long  ringRemainingMs();  // 0 если не звенит

}  // namespace Alarm