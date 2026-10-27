#include "captive_portal.h"

#include <DNSServer.h>
#include <WiFi.h>

static DNSServer dns;
static bool running = false;

void CaptivePortal::begin() {
  IPAddress apIP = WiFi.softAPIP();
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  running = dns.start(53, "*", apIP);
  Serial.printf("[CP] DNS %s, AP %s\n",
                running ? "started" : "failed",
                apIP.toString().c_str());
}

void CaptivePortal::tick() {
  if (running)
    dns.processNextRequest();
}

bool CaptivePortal::isRunning() {
  return running;
}