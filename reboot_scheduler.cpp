#include "reboot_scheduler.h"

static bool scheduled = false;
static unsigned long at = 0;

void RebootScheduler::schedule(unsigned long delay_ms) {
  scheduled = true;
  at = millis() + delay_ms;
  Serial.printf("[Reboot] scheduled in %lu ms\n", delay_ms);
}

bool RebootScheduler::isScheduled() {
  return scheduled;
}

void RebootScheduler::tick() {
  if (scheduled && (long)(millis() - at) >= 0) {
    Serial.println("[Reboot] rebooting now");
    ESP.restart();
  }
}