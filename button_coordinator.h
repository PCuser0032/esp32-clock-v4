#pragma once
#include <Arduino.h>
#include "button.h"

// =============================================================
//  Детект комбинации «обе кнопки зажаты».
//
//  Логика:
//    * Обе зажаты одновременно → suppress_long на обеих (индивидуальные
//      LONG не выстрелят, пока комбинация активна).
//    * Обе удерживаются ≥ short_ms → combo «armed».
//    * Отпускание одной в состоянии armed (до reset_ms) → consumeReboot().
//    * Обе удерживаются ≥ reset_ms → consumeReset().
//    * Abort: одна отпущена ДО arm — suppress снимается, оставшаяся
//      кнопка продолжает работать индивидуально.
//
//  tick() вызывать ПОСЛЕ btn1.tick() и btn2.tick() в handleButtons().
//  setEnabled(false) — временно (например, во время звонка будильника).
// =============================================================

class ButtonCoordinator {
public:
  ButtonCoordinator(Button &a, Button &b,
                    unsigned long short_ms,
                    unsigned long reset_ms);

  void tick();

  void setEnabled(bool e);
  bool isEnabled() const { return enabled_; }

  bool isArmed() const { return armed_; }
  unsigned long comboAgeMs() const;
  bool isResetReached() const { return reset_fired_; }

  // one-shot, сбрасываются после вызова.
  bool consumeReboot();
  bool consumeReset();

private:
  Button &a_;
  Button &b_;
  const unsigned long short_ms_;
  const unsigned long reset_ms_;

  bool enabled_ = true;
  bool armed_ = false;
  bool reset_fired_ = false;
  bool reboot_pending_ = false;
  bool reset_pending_ = false;
  unsigned long arm_start_ms_ = 0;
};