#pragma once
#include <Arduino.h>

// =============================================================
//  Пассивный зуммер на LEDC/PWM. Non-blocking: все паттерны
//  крутятся в tick(), который надо звать из loop().
//
//  Приоритет в tick():
//    1. alarm_active   — будильник (игнорирует feedback_enabled)
//    2. queue_running  — feedback-паттерны (playShort/Long/...)
//    3. manual_until   — одноразовый tone()
//
//  feedback_enabled — внутренний флаг. Модуль НЕ читает Storage
//  напрямую (чистая архитектура). Вызывающий код (setup, web_ui,
//  handleButtons) ставит флаг при изменении sound_enabled.
//
//  Будильник звенит независимо от feedback_enabled — это
//  критическая функция, а не «обратная связь на нажатие».
// =============================================================

namespace Buzzer {

// Настраивает LEDC на PIN_BUZZER. Тон не подаёт.
// По умолчанию feedback_enabled = true; .ino выставит реальное
// значение из Storage::get().sound_enabled.
void begin();

// Вызывается из loop(). Гасит тон по истечении длительности,
// переключает шаги паттерна, крутит alarm.
void tick();

// Включает/выключает feedback-паттерны (playShort/... и boot).
// Будильник (alarmStart) флаг игнорирует.
void setFeedbackEnabled(bool e);

// --- Одноразовый тон (перетирает всё, включая будильник) ---
// Низкоуровневый вызов — для тестов через /api/test/beep.
// Если freq == 0 — просто тишина.
void tone(uint16_t freq, uint16_t ms);

// Немедленно всё заглушить (очередь, alarm, manual).
void off();

// --- Feedback-паттерны (проверяют feedback_enabled) ---
void playShort();    // «клик» 2000 Гц, 80 мс
void playLong();     // «клик» 2000 Гц, 200 мс
void playUp();       // 1000 → 2000 (включение звука)
void playDown();     // 2000 → 1000 (выключение звука)
void playDouble();   // бип-пауза-бип (combo reboot)
void playTriple();   // 3×бип (combo full reset)
void playBoot();     // boot beep при старте (если sound on)

// --- Будильник ---
// Чередование LOW/HIGH с периодом BUZZER_ALARM_ON_MS/OFF_MS.
// После alarmStart() звонит до alarmStop() (или до автостопа
// на уровне Alarm::tick(), который сам зовёт alarmStop()).
void alarmStart();
void alarmStop();
bool isAlarmActive();

}  // namespace Buzzer