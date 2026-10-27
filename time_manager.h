#pragma once
#include <Arduino.h>
#include <time.h>
#include "storage.h"

namespace TimeManager {
// Явное состояние NTP-попытки.
// Заменяет разрозненные флаги (sync_pending + last_start_ntp_ms),
// которые раньше надо было собирать в вызывающем коде.
enum class NtpState {
  IDLE,         // SNTP не запущен (или давно завершился)
  IN_PROGRESS,  // идёт попытка синхронизации (< 20 с от старта)
  OK,           // только что синхронизировались (в течение ~1 с)
  TIMEOUT,      // попытка не уложилась в 20 с, ждём следующего retry
};

bool begin();
void applySettings(const Settings &s);

bool readUTC(struct tm &out);
bool readLocal(struct tm &out);

bool setLocal(int year, int mon, int day,
              int hour, int min, int sec);

void startNTP(const Settings &s);
bool trySyncNTP();

bool isSynced();
bool rtcOk();
bool rtcLost();

// --- NTP state ---
NtpState ntpState();
// Эквивалент ntpState() == NtpState::IN_PROGRESS.
// Оставлен как читаемый shorthand для частого случая.
bool isSyncInProgress();

// --- Диагностика ---
const char *sntpStatusStr();
unsigned long lastStartNtpMs();
String resolveHost(const String &host);
}