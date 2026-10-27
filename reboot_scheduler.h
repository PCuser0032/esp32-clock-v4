#pragma once
#include <Arduino.h>

namespace RebootScheduler {
// Запланировать перезагрузку через delay_ms. Повторный вызов
// переписывает время (идемпотентно «последний выиграл»).
void schedule(unsigned long delay_ms);

bool isScheduled();

// Вызывается из loop() после WebUI::tick() и CaptivePortal::tick(),
// чтобы HTTP-ответы успели уйти до ESP.restart().
void tick();
}