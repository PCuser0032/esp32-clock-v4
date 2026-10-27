#include "mdns_manager.h"

#include <ESPmDNS.h>

static bool running = false;
static String host = "";

bool MdnsManager::begin(const char *hostname) {
  // Идемпотентность: повторный вызов при уже работающем mDNS —
  // no-op. Это позволяет вызывать begin() из ветки «STA connected»
  // без риска накопить несколько сервисов.
  if (running)
    return true;

  if (MDNS.begin(hostname)) {
    MDNS.addService("http", "tcp", 80);
    host = String(hostname) + ".local";
    running = true;
    Serial.printf("[mDNS] http://%s\n", host.c_str());
    return true;
  }
  host = "";
  running = false;
  Serial.println("[mDNS] failed to start");
  return false;
}

void MdnsManager::stop() {
  if (!running)
    return;
  MDNS.end();
  host = "";
  running = false;
  Serial.println("[mDNS] stopped");
}

bool MdnsManager::isRunning() {
  return running;
}

String MdnsManager::getHost() {
  return host;
}