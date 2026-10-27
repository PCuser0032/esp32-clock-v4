#include "buzzer.h"
#include "config.h"

#include <driver/gpio.h>   // gpio_reset_pin, gpio_set_direction, gpio_set_level

// =============================================================
//  Внутреннее состояние.
// =============================================================

static bool feedback_enabled = true;
static bool ledc_attached = false;

struct Step {
  uint16_t freq;
  uint16_t ms;
};

static const uint8_t QUEUE_SIZE = 8;
static Step queue[QUEUE_SIZE];
static uint8_t queue_len = 0;
static uint8_t queue_head = 0;
static bool queue_running = false;
static unsigned long queue_step_until_ms = 0;

static unsigned long manual_until_ms = 0;

static bool alarm_active = false;
static bool alarm_on = false;
static bool alarm_low_phase = true;
static unsigned long alarm_phase_until_ms = 0;

// =============================================================
//  Helpers
// =============================================================

static void silence() {
  // 1. Сначала гасим LEDC в ноль через ledcWrite (duty=0, не 1).
  if (ledc_attached) {
    ledcWrite(PIN_BUZZER, 0);
    ledcWriteTone(PIN_BUZZER, 0);
    ledcDetach(PIN_BUZZER);
    ledc_attached = false;
  }
  // 2. Принудительно снимаем GPIO routing через IDF API.
  //    Arduino-обёртки (pinMode/digitalWrite) НЕ снимают routing
  //    на Core 3.x — отсюда писк. gpio_reset_pin делает это железно.
  gpio_reset_pin((gpio_num_t)PIN_BUZZER);
  // 3. Пин в high-Z = тишина (проверено на этом железе).
  gpio_set_direction((gpio_num_t)PIN_BUZZER, GPIO_MODE_INPUT);
}

static void putTone(uint16_t freq) {
  if (freq == 0) { silence(); return; }
  // Пин в high-Z. Переводим в OUTPUT перед attach.
  gpio_set_direction((gpio_num_t)PIN_BUZZER, GPIO_MODE_OUTPUT);
  if (!ledc_attached) {
    ledcAttach(PIN_BUZZER, freq, BUZZER_RES_BITS);
    ledc_attached = true;
  }
  ledcWriteTone(PIN_BUZZER, freq);
}

static void queueClear() {
  queue_len = 0;
  queue_head = 0;
  queue_running = false;
}

static void queueStart(const Step *steps, uint8_t n) {
  if (n > QUEUE_SIZE) n = QUEUE_SIZE;
  for (uint8_t i = 0; i < n; i++) queue[i] = steps[i];
  queue_len = n;
  queue_head = 0;
  queue_running = true;
  putTone(queue[0].freq);
  queue_step_until_ms = millis() + queue[0].ms;
}

static void alarmClear() {
  alarm_active = false;
  alarm_on = false;
  alarm_phase_until_ms = 0;
}

static void playSteps(const Step *steps, uint8_t n) {
  if (!feedback_enabled) return;
  if (alarm_active) return;
  queueStart(steps, n);
}

// =============================================================
//  API
// =============================================================

void Buzzer::begin() {
  // Пин в high-Z, ничего не драйвим. Тишина до первого тона.
  gpio_reset_pin((gpio_num_t)PIN_BUZZER);
  gpio_set_direction((gpio_num_t)PIN_BUZZER, GPIO_MODE_INPUT);
  ledc_attached = false;
}

void Buzzer::tick() {
  unsigned long now = millis();

  if (alarm_active) {
    if (now >= alarm_phase_until_ms) {
      if (alarm_on) {
        silence();
        alarm_on = false;
        alarm_phase_until_ms = now + BUZZER_ALARM_OFF_MS;
      } else {
        uint16_t f = alarm_low_phase ? BUZZER_FREQ_ALARM_LOW
                                     : BUZZER_FREQ_ALARM_HIGH;
        putTone(f);
        alarm_on = true;
        alarm_low_phase = !alarm_low_phase;
        alarm_phase_until_ms = now + BUZZER_ALARM_ON_MS;
      }
    }
    return;
  }

  if (queue_running) {
    if (now >= queue_step_until_ms) {
      queue_head++;
      if (queue_head >= queue_len) {
        silence();
        queueClear();
        return;
      }
      putTone(queue[queue_head].freq);
      queue_step_until_ms = now + queue[queue_head].ms;
    }
    return;
  }

  if (manual_until_ms && now >= manual_until_ms) {
    silence();
    manual_until_ms = 0;
  }
}

void Buzzer::setFeedbackEnabled(bool e) {
  feedback_enabled = e;
}

void Buzzer::tone(uint16_t freq, uint16_t ms) {
  alarmClear();
  queueClear();
  manual_until_ms = 0;
  if (freq == 0 || ms == 0) {
    silence();
    return;
  }
  putTone(freq);
  manual_until_ms = millis() + ms;
}

void Buzzer::off() {
  alarmClear();
  queueClear();
  manual_until_ms = 0;
  silence();
}

// =============================================================
//  Feedback-паттерны
// =============================================================

static const Step PAT_SHORT[]  = {{BUZZER_FREQ_FEEDBACK, 80}};
static const Step PAT_LONG[]   = {{BUZZER_FREQ_FEEDBACK, 200}};
static const Step PAT_UP[]     = {{BUZZER_FREQ_ALARM_LOW,  80},
                                  {BUZZER_FREQ_ALARM_HIGH, 80}};
static const Step PAT_DOWN[]   = {{BUZZER_FREQ_ALARM_HIGH, 80},
                                  {BUZZER_FREQ_ALARM_LOW,  80}};
static const Step PAT_DOUBLE[] = {{BUZZER_FREQ_FEEDBACK, 80},
                                  {0, 80},
                                  {BUZZER_FREQ_FEEDBACK, 80}};
static const Step PAT_TRIPLE[] = {{BUZZER_FREQ_FEEDBACK, 80},
                                  {0, 80},
                                  {BUZZER_FREQ_FEEDBACK, 80},
                                  {0, 80},
                                  {BUZZER_FREQ_FEEDBACK, 80}};
static const Step PAT_BOOT[]   = {{BUZZER_FREQ_FEEDBACK, 100}};

void Buzzer::playShort()  { playSteps(PAT_SHORT,  1); }
void Buzzer::playLong()   { playSteps(PAT_LONG,   1); }
void Buzzer::playUp()     { playSteps(PAT_UP,     2); }
void Buzzer::playDown()   { playSteps(PAT_DOWN,   2); }
void Buzzer::playDouble() { playSteps(PAT_DOUBLE, 3); }
void Buzzer::playTriple() { playSteps(PAT_TRIPLE, 5); }
void Buzzer::playBoot()   { playSteps(PAT_BOOT,   1); }

// =============================================================
//  Будильник
// =============================================================

void Buzzer::alarmStart() {
  queueClear();
  manual_until_ms = 0;
  alarm_active = true;
  alarm_on = true;
  alarm_low_phase = false;
  putTone(BUZZER_FREQ_ALARM_LOW);
  alarm_phase_until_ms = millis() + BUZZER_ALARM_ON_MS;
}

void Buzzer::alarmStop() {
  alarmClear();
  silence();
}

bool Buzzer::isAlarmActive() {
  return alarm_active;
}