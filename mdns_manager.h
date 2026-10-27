#pragma once
#include <Arduino.h>

namespace MdnsManager {
// Запускает mDNS с указанным именем (без ".local").
// Возвращает true при успехе. Идемпотентна: повторный вызов
// при уже работающем mDNS — no-op.
bool begin(const char *hostname);

// Останавливает mDNS. Вызывается при потере STA: имя в этом
// состоянии не резолвится, и логичнее перезапустить сервис
// при следующем WL_CONNECTED.
void stop();

bool isRunning();

// Полное имя хоста: "<hostname>.local". Пусто, если mDNS не запущен.
String getHost();
}