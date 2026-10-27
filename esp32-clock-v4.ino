#include <Wire.h>
#include <esp_chip_info.h>
#include <esp_system.h>
#include <esp_mac.h>

#include "config.h"
#include "storage.h"
#include "display.h"
#include "time_manager.h"
#include "wifi_manager.h"
#include "web_ui.h"
#include "button.h"
#include "button_coordinator.h"
#include "buzzer.h"
#include "climate.h"
#include "alarm.h"
#include "captive_portal.h"
#include "mdns_manager.h"
#include "reboot_scheduler.h"

enum class State {
  BOOT,
  AP_ONLY,
  AP_STA_CONNECTING,
  AP_STA_ONLINE
};

static State state = State::BOOT;
static unsigned long last_display = 0;
static unsigned long last_ntp = 0;
static unsigned long msg_until = 0;
static bool rtc_error_shown = false;

// --- Кнопки и координатор комбинации ---
static Button btn1(PIN_BUTTON, BTN_ACTIVE_LOW,
                   BTN_DEBOUNCE_MS, BTN_SHORT_MAX_MS, BTN_LONG_MS);
static Button btn2(PIN_BUTTON2, BTN_ACTIVE_LOW,
                   BTN_DEBOUNCE_MS, BTN_SHORT_MAX_MS, BTN_LONG_MS);
static ButtonCoordinator combo(btn1, btn2,
                               BTN_LONG_MS, BTN_COMBO_RESET_MS);

// Флаг: Button 2 была использована для остановки будильника.
// До её отпускания любые события B2 «съедаются», чтобы не
// сработал toggle звука или тестовый бип после остановки.
static bool g_alarm_b2_consumed = false;

static void setState(State s) {
  state = s;
  static const char *n[] = {
    "BOOT", "AP_ONLY", "AP_STA_CONNECTING", "AP_STA_ONLINE"
  };
  Serial.printf("[FSM] -> %s\n", n[(int)s]);
}

static void showTempMsg(const String &l1, const String &l2, unsigned long ms) {
  Display::showMessage(l1, l2);
  msg_until = millis() + ms;
}

static const char *ntpStateLabel(TimeManager::NtpState s) {
  switch (s) {
    case TimeManager::NtpState::IN_PROGRESS: return "syncing...";
    case TimeManager::NtpState::OK: return "ok";
    case TimeManager::NtpState::TIMEOUT: return "timeout";
    case TimeManager::NtpState::IDLE:
    default: return "idle";
  }
}

// ------------------------------------------------------------------
//  Диагностика при старте
// ------------------------------------------------------------------

static const char *resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT_PIN";
    case ESP_RST_SW: return "SW_RESTART";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

static void printDiagnostics() {
  Serial.println();
  Serial.println("---------------- MCU -----------------");

  esp_chip_info_t chip;
  esp_chip_info(&chip);
  const char *model = "?";
  switch (chip.model) {
    case CHIP_ESP32: model = "ESP32"; break;
    case CHIP_ESP32S2: model = "ESP32-S2"; break;
    case CHIP_ESP32S3: model = "ESP32-S3"; break;
    case CHIP_ESP32C3: model = "ESP32-C3"; break;
    default: model = "other"; break;
  }
  Serial.printf("Chip           : %s rev %d, %d core(s)\n",
                model, chip.revision, chip.cores);
  Serial.printf("CPU freq       : %u MHz\n", getCpuFrequencyMhz());
  Serial.printf("XTAL freq      : %u MHz\n", getXtalFrequencyMhz());
  Serial.printf("APB freq       : %u MHz\n", getApbFrequency() / 1000000);

  uint32_t flash_size = ESP.getFlashChipSize();
  uint32_t flash_speed = ESP.getFlashChipSpeed();
  Serial.printf("Flash size     : %u MB\n", flash_size / (1024 * 1024));
  Serial.printf("Flash speed    : %u MHz\n", flash_speed / 1000000);

  const char *fmode = "?";
  switch (ESP.getFlashChipMode()) {
    case FM_QIO: fmode = "QIO"; break;
    case FM_QOUT: fmode = "QOUT"; break;
    case FM_DIO: fmode = "DIO"; break;
    case FM_DOUT: fmode = "DOUT"; break;
    default: fmode = "?"; break;
  }
  Serial.printf("Flash mode     : %s\n", fmode);
  Serial.printf("Sketch size    : %u bytes\n", ESP.getSketchSize());
  Serial.printf("Sketch free    : %u bytes\n", ESP.getFreeSketchSpace());

  Serial.printf("Heap total     : %u bytes\n", ESP.getHeapSize());
  Serial.printf("Heap free      : %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Heap min free  : %u bytes\n", ESP.getMinFreeHeap());
  Serial.printf("Heap max alloc : %u bytes\n", ESP.getMaxAllocHeap());

  Serial.printf("ESP-IDF        : %s\n", ESP.getSdkVersion());
  Serial.printf("Arduino core   : %d.%d.%d\n",
                ESP_ARDUINO_VERSION_MAJOR,
                ESP_ARDUINO_VERSION_MINOR,
                ESP_ARDUINO_VERSION_PATCH);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf("MAC STA        : %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  Serial.printf("MAC AP         : %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.printf("Reset reason   : %s\n", resetReasonStr(esp_reset_reason()));

  Serial.println("---------------------------------------");
  Serial.println();
}

// ------------------------------------------------------------------
//  Кнопки
// ------------------------------------------------------------------

static void handleButtons() {
  // ---- 1. Звенит будильник: всё подавлено, кроме B2 = stop ----
  if (Alarm::isRinging()) {
    // Важен порядок: сначала выключаем combo (если было armed,
    // setEnabled(false) сбросит suppress_long), затем ставим
    // suppress заново для обеих кнопок.
    combo.setEnabled(false);
    btn1.setSuppressLong(true);
    btn2.setSuppressLong(true);

    btn1.tick();  // только debounce, событие игнорируем
    btn2.tick();

    // Остановка: B2 удерживается >= 100 мс. Любая длительность
    // (short/medium/long) — это stop. Проверка по holdMs() стреляет
    // сразу во время удержания, не дожидаясь отпускания.
    if (btn2.isPressed() && btn2.holdMs() >= 100) {
      Alarm::stop();
      showTempMsg("Alarm stopped", "", 1500);
      Buzzer::playShort();
      g_alarm_b2_consumed = true;
    }
    return;
  }

  // ---- 2. Обычный режим ----
  // Пока B2 не отпущена после остановки будильника — держим suppress,
  // события B2 съедаем (иначе на отпускании выстрелит SHORT_PRESS
  // и переключит звук).
  btn1.setSuppressLong(false);
  btn2.setSuppressLong(g_alarm_b2_consumed);
  combo.setEnabled(true);

  Button::Event e1 = btn1.tick();
  Button::Event e2 = btn2.tick();
  combo.tick();

  if (g_alarm_b2_consumed) {
    e2 = Button::Event::NONE;
    if (!btn2.isPressed()) {
      g_alarm_b2_consumed = false;
      btn2.setSuppressLong(false);
    }
  }

  // --- 2.1. Combo-события (приоритет) ---
  if (combo.consumeReset()) {
    Display::showMessage("FULL RESET", "rebooting...");
    Buzzer::playTriple();
    Storage::reset();
    RebootScheduler::schedule(1500);
    return;
  }
  if (combo.consumeReboot()) {
    Display::showMessage("Rebooting", "please wait");
    Buzzer::playDouble();
    RebootScheduler::schedule(500);
    return;
  }
  if (combo.isResetReached()) return;

  // --- 2.2. Живой фидбек combo ---
  static unsigned long last_combo_render = 0;
  unsigned long t = millis();
  if (t - last_combo_render >= 500) {
    last_combo_render = t;

    if (combo.isArmed()) {
      msg_until = 0;
      unsigned long age = combo.comboAgeMs();
      unsigned long remain = (BTN_COMBO_RESET_MS - age + 999) / 1000;
      char b[17];
      snprintf(b, sizeof(b), "%lus to reset", remain);
      Display::showMessage("Release = reboot", b);
    } else if (btn1.isPressed() && btn2.isPressed()) {
      msg_until = 0;
      Display::showMessage("Both held", "3s=reb 10s=reset");
    }
  }

  // --- 2.3. Button 1 — force NTP ---
  if (e1 == Button::Event::SHORT_PRESS) {
    Buzzer::playShort();
    if (!WifiManager::isSTAConnected()) {
      showTempMsg("NTP", "no Wi-Fi", 1500);
    } else {
      TimeManager::startNTP(Storage::get());
      char b[17];
      snprintf(b, sizeof(b), "NTP: %s",
               ntpStateLabel(TimeManager::ntpState()));
      showTempMsg("Sync started", b, 1500);
    }
  }
  // e1 == LONG_HOLD — no-op.

  // --- 2.4. Button 2 — звук ---
  if (e2 == Button::Event::LONG_HOLD) {
    Buzzer::tone(BUZZER_FREQ_FEEDBACK, 150);
    showTempMsg("Buzzer test", "one beep", 1500);
  } else if (e2 == Button::Event::SHORT_PRESS) {
    Settings s = Storage::get();
    bool new_state = !s.sound_enabled;
    s.sound_enabled = new_state;
    Storage::save(s);

    if (new_state) {
      Buzzer::setFeedbackEnabled(true);
      Buzzer::playUp();
    } else {
      Buzzer::playDown();
      Buzzer::setFeedbackEnabled(false);
    }
    showTempMsg("Sound toggled", new_state ? "ON" : "OFF", 1500);
  }
}

// ------------------------------------------------------------------
//  setup / loop
// ------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32-CLOCK ===");

  printDiagnostics();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_CLOCK_HZ);

  Storage::begin();

  Display::begin();
  Display::showBoot("starting...");

  btn1.begin();
  btn2.begin();

  WebUI::setButtons(&btn1, &btn2);

  Buzzer::begin();
  Buzzer::setFeedbackEnabled(Storage::get().sound_enabled);

  // Climate до TimeManager: датчик не зависит от времени,
  // но полезно иметь свежие T/H как можно раньше.
  Climate::begin();

  // Alarm после Storage (читает Settings) и после Climate
  // (не зависит, но порядок чистый по Д.8).
  Alarm::begin();

  if (!TimeManager::begin()) {
    Serial.println("[RTC] not found");
    Display::showMessage("RTC error", "check wiring");
    delay(1500);
  }

  const Settings &s = Storage::get();
  TimeManager::applySettings(s);

  WifiManager::begin(s);

  WebUI::begin();
  CaptivePortal::begin();

  if (s.sta_ssid.length() > 0) {
    setState(State::AP_STA_CONNECTING);
    Display::showBoot("Wi-Fi connecting");
  } else {
    setState(State::AP_ONLY);
    Display::showBoot("AP: " + WifiManager::getAPIP().toString());
  }

  if (TimeManager::rtcLost()) {
    Display::showMessage("RTC lost power", "set time via web");
    delay(1500);
  }

  enableLoopWDT();
  Serial.println("[WDT] loop watchdog enabled");

  // Boot beep — в самом конце setup(). Buzzer::tick() в loop()
  // погасит тон через 100 мс. Если звук выключен — playBoot()
  // выйдет по feedback_enabled=false.
  Buzzer::playBoot();
}

void loop() {
  WifiManager::tick();
  WebUI::tick();
  CaptivePortal::tick();
  RebootScheduler::tick();

  // Климат и будильник — до handleButtons(), чтобы:
  //  * будильник, сработавший в этом тике, уже был виден кнопкам;
  //  * климат на LCD обновился в этом же тике.
  Climate::tick();
  Alarm::tick();

  handleButtons();

  unsigned long now = millis();

  if (state == State::AP_STA_CONNECTING && WifiManager::isSTAConnected()) {
    setState(State::AP_STA_ONLINE);
    Serial.printf("[WiFi] STA IP: %s\n",
                  WifiManager::getSTAIP().toString().c_str());
  }

  static bool sta_was_connected = false;
  bool sta_now = WifiManager::isSTAConnected();

  if (sta_now && !sta_was_connected) {
    Serial.println("[NET] STA connected");
    MdnsManager::begin(MDNS_HOSTNAME);
    if (!TimeManager::isSynced()) {
      Serial.printf("[NET] DNS resolve pool.ntp.org -> %s\n",
                    TimeManager::resolveHost("pool.ntp.org").c_str());
      TimeManager::startNTP(Storage::get());
      last_ntp = now;
    } else {
      Serial.println("[NET] already synced, skipping SNTP start");
    }
  } else if (!sta_now && sta_was_connected) {
    Serial.println("[NET] STA lost");
    MdnsManager::stop();
  }
  sta_was_connected = sta_now;

  if (sta_now && TimeManager::trySyncNTP()) {
    // last_ok_ms выставлен внутри trySyncNTP().
  }

  unsigned long retry_window = TimeManager::isSynced()
                                 ? NTP_SYNC_PERIOD_MS
                                 : NTP_SYNC_RETRY_MS;
  if (sta_now && (now - last_ntp > retry_window)) {
    if (TimeManager::ntpState() == TimeManager::NtpState::IN_PROGRESS) {
      // Ждём завершения текущей попытки.
    } else {
      last_ntp = now;
      Serial.printf("[NTP] restart SNTP (synced=%d, status=%s)\n",
                    TimeManager::isSynced(), TimeManager::sntpStatusStr());
      TimeManager::startNTP(Storage::get());
    }
  }

  bool showing_msg = (msg_until > 0 && now < msg_until);
  if (msg_until > 0 && now >= msg_until) {
    msg_until = 0;
    Display::clear();
  }

  bool use24h = Storage::get().use_24h;

  if (!btn1.isPressed() && !btn2.isPressed()
      && !showing_msg && (now - last_display > DISPLAY_REFRESH_MS)) {
    last_display = now;
    struct tm lt;
    if (TimeManager::readLocal(lt)) {
      rtc_error_shown = false;

      Display::SyncIcon icon = Display::SyncIcon::NONE;
      switch (TimeManager::ntpState()) {
        case TimeManager::NtpState::IN_PROGRESS:
          icon = Display::SyncIcon::SYNC;
          break;
        case TimeManager::NtpState::OK:
          icon = Display::SyncIcon::SYNC_OK;
          break;
        case TimeManager::NtpState::TIMEOUT:
        case TimeManager::NtpState::IDLE:
        default:
          if (TimeManager::rtcLost())
            icon = Display::SyncIcon::RTC_LOST;
          break;
      }

      // Строка 3: слева — режим (или ALARM!), справа — IP (или B2=stop).
      String status_left, status_right;
      if (Alarm::isRinging()) {
        bool blink_on = ((now / ALARM_BLINK_MS) % 2) == 0;
        if (blink_on) {
          status_left = "ALARM!";
          status_right = "B2=stop";
        }
        // при !blink_on — оба пустые, строка 3 «гаснет» (мигание)
      } else {
        status_left = WifiManager::getStatusText();
        status_right = WifiManager::isSTAConnected()
                         ? WifiManager::getSTAIP().toString()
                         : WifiManager::getAPIP().toString();
      }

      const Climate::Reading &r = Climate::current();
      Display::showTime(lt, use24h, icon,
                        r.temp_c, r.humidity_pct, r.valid,
                        Storage::get().sound_enabled,
                        status_left, status_right);
    } else if (!rtc_error_shown) {
      rtc_error_shown = true;
      Display::showMessage("RTC error", "check wiring");
    }
  }

  Buzzer::tick();
}
