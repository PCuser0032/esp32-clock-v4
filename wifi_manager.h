#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#include "storage.h"

namespace WifiManager {

// Явное состояние Wi-Fi-подсистемы.
enum class WifiState {
  IDLE,        // AP-only, STA не настроен
  CONNECTING,  // попытки подключения идут
  CONNECTED,   // STA подключён
  BACKOFF,     // между попытками, интервал растёт экспоненциально
};

void begin(const Settings &s);
void tick();

// Сохранить новые credentials и запланировать перезагрузку через reboot_delay_ms.
// Смена STA «на лету» на ESP32 не надёжна,
// поэтому используется контролируемый рестарт:
// после него WiFi.begin() в setup() запускает подключение с чистого состояния драйвера.
//
// Рестарт выполняет RebootScheduler::tick() из loop().
void applyNewCredentialsAndReboot(const String &ssid,
                                  const String &password,
                                  unsigned long reboot_delay_ms);

bool isSTAConnected();
IPAddress getSTAIP();
IPAddress getAPIP();
int getStationCount();
int getRSSI();
WifiState state();
String getStatusText();
}