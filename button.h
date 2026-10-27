#pragma once
#include <Arduino.h>

// =============================================================
//  Одна тактовая кнопка. Класс (не namespace) — кнопок две,
//  состояние должно быть экземплярным.
//
//  События:
//    SHORT_PRESS — отпущена в окне [debounce, short_max_ms)
//    LONG_HOLD   — достигнут порог long_hold_ms, кнопка ещё зажата
//                  (срабатывает один раз, до отпускания)
//
//  Между short_max_ms и long_hold_ms — «зона молчания».
//
//  setSuppressLong(true) — LONG_HOLD «съедается» молча (long_fired=true),
//  без ретроактивного выстрела после снятия подавления. Используется
//  ButtonCoordinator, чтобы не выстрелить одиночный LONG в момент,
//  когда пользователь на самом деле держит комбинацию.
// =============================================================

class Button {
public:
  enum class Event {
    NONE = 0,
    SHORT_PRESS,
    LONG_HOLD,
  };

  Button(uint8_t pin,
         bool active_low,
         unsigned long debounce_ms,
         unsigned long short_max_ms,
         unsigned long long_hold_ms);

  void begin();
  Event tick();

  bool isPressed() const { return stable_pressed_; }
  unsigned long holdMs() const;
  unsigned long releasedHoldMs() const { return released_hold_ms_; }

  void setSuppressLong(bool s);
  bool isLongSuppressed() const { return suppress_long_; }

  uint8_t pin() const { return pin_; }

private:
  bool readPressed() const;

  const uint8_t pin_;
  const bool active_low_;
  const unsigned long debounce_ms_;
  const unsigned long short_max_ms_;
  const unsigned long long_hold_ms_;

  bool raw_pressed_ = false;
  bool stable_pressed_ = false;
  bool long_fired_ = false;
  bool suppress_long_ = false;

  unsigned long last_change_ms_ = 0;
  unsigned long press_start_ms_ = 0;
  unsigned long released_hold_ms_ = 0;
};