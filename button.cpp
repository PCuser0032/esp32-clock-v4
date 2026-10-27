#include "button.h"

Button::Button(uint8_t pin, bool active_low,
               unsigned long debounce_ms,
               unsigned long short_max_ms,
               unsigned long long_hold_ms)
  : pin_(pin),
    active_low_(active_low),
    debounce_ms_(debounce_ms),
    short_max_ms_(short_max_ms),
    long_hold_ms_(long_hold_ms) {}

bool Button::readPressed() const {
  int v = digitalRead(pin_);
  return active_low_ ? (v == LOW) : (v == HIGH);
}

void Button::begin() {
  pinMode(pin_, active_low_ ? INPUT_PULLUP : INPUT_PULLDOWN);
  raw_pressed_ = stable_pressed_ = readPressed();
  last_change_ms_ = millis();
  press_start_ms_ = millis();
  // Кнопка уже зажата на старте — long считаем «занятым»:
  // иначе пользователь, включивший питание с зажатой кнопкой,
  // получит LONG_HOLD через long_hold_ms.
  long_fired_ = stable_pressed_;
}

unsigned long Button::holdMs() const {
  return stable_pressed_ ? (millis() - press_start_ms_) : 0;
}

void Button::setSuppressLong(bool s) {
  suppress_long_ = s;
}

Button::Event Button::tick() {
  bool now_pressed = readPressed();
  unsigned long t = millis();

  // --- Debounce raw ---
  if (now_pressed != raw_pressed_) {
    raw_pressed_ = now_pressed;
    last_change_ms_ = t;
  }

  // --- Стабилизация ---
  if ((t - last_change_ms_) >= debounce_ms_ && raw_pressed_ != stable_pressed_) {
    stable_pressed_ = raw_pressed_;

    if (stable_pressed_) {
      press_start_ms_ = t;
      long_fired_ = false;
    } else {
      unsigned long held = t - press_start_ms_;
      released_hold_ms_ = held;
      bool was_long = long_fired_;
      long_fired_ = false;

      if (!was_long && held < short_max_ms_) {
        return Event::SHORT_PRESS;
      }
      // held ∈ [short_max, long_hold) — «зона молчания»,
      // или long уже отработал — событие не возвращаем.
      return Event::NONE;
    }
  }

  // --- LONG ---
  if (stable_pressed_ && !long_fired_
      && (t - press_start_ms_) >= long_hold_ms_) {
    long_fired_ = true;
    if (!suppress_long_) {
      return Event::LONG_HOLD;
    }
    // suppressed — consumed silently
  }

  return Event::NONE;
}