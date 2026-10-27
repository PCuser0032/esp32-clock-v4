#pragma once
#include <Arduino.h>
#include <time.h>

namespace TzUtil {
// Валидация POSIX TZ-строки: длина [1, 64), все символы —
// печатные ASCII. Семантику (корректность имени зоны, правил DST)
// не проверяем — этим занимается newlib при setenv/tzset.
bool isValid(const String &tz);

// Текущее эффективное смещение local от UTC в секундах,
// с учётом DST для текущего момента. tm_gmtoff заполняется
// newlib в localtime_r().
int effectiveOffsetSeconds();
}