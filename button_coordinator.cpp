#include "button_coordinator.h"

ButtonCoordinator::ButtonCoordinator(Button &a, Button &b,
                                     unsigned long short_ms,
                                     unsigned long reset_ms)
  : a_(a), b_(b), short_ms_(short_ms), reset_ms_(reset_ms) {}

void ButtonCoordinator::setEnabled(bool e) {
  if (!e && enabled_) {
    if (armed_) {
      armed_ = false;
      arm_start_ms_ = 0;
      a_.setSuppressLong(false);
      b_.setSuppressLong(false);
    }
  }
  enabled_ = e;
}

unsigned long ButtonCoordinator::comboAgeMs() const {
  return armed_ ? (millis() - arm_start_ms_) : 0;
}

bool ButtonCoordinator::consumeReboot() {
  if (!reboot_pending_) return false;
  reboot_pending_ = false;
  return true;
}

bool ButtonCoordinator::consumeReset() {
  if (!reset_pending_) return false;
  reset_pending_ = false;
  return true;
}

void ButtonCoordinator::tick() {
  if (!enabled_) return;

  bool both = a_.isPressed() && b_.isPressed();
  unsigned long t = millis();

  if (!armed_) {
    if (both) {
      a_.setSuppressLong(true);
      b_.setSuppressLong(true);
      if (arm_start_ms_ == 0) arm_start_ms_ = t;
      if ((t - arm_start_ms_) >= short_ms_) armed_ = true;
    } else if (arm_start_ms_ != 0) {
      // Combo abort: была попытка, но не дотянули до arm.
      a_.setSuppressLong(false);
      b_.setSuppressLong(false);
      arm_start_ms_ = 0;
    }
    return;
  }

  // --- armed ---

  // 1. reset по удержанию до reset_ms
  if (both && !reset_fired_ && (t - arm_start_ms_) >= reset_ms_) {
    reset_fired_ = true;
    reset_pending_ = true;
    return;
  }

  // 2. reboot на первом релизе из пары (до reset_ms)
  if (!a_.isPressed() || !b_.isPressed()) {
    if (!reset_fired_) reboot_pending_ = true;
    armed_ = false;
    arm_start_ms_ = 0;
    a_.setSuppressLong(false);
    b_.setSuppressLong(false);
  }
}