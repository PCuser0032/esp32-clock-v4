#include "tz_util.h"

#include <time.h>

namespace TzUtil {

bool isValid(const String& tz) {
  if (tz.length() == 0 || tz.length() >= 64) return false;
  for (size_t i = 0; i < tz.length(); i++) {
    char c = tz[i];
    if (c < 0x20 || c > 0x7E) return false;
  }
  return true;
}

int effectiveOffsetSeconds() {
  time_t now = time(nullptr);

  struct tm l, g;
  localtime_r(&now, &l);
  gmtime_r(&now, &g);

  // Идея: возьмём два «типа» времени — локальное и UTC — и
  // интерпретируем ОБА как локальные (через mktime). Тогда
  // разница полученных epoch'ов равна смещению local от UTC.
  //
  // tm_isdst = -1 — чтобы mktime() применил те же правила DST,
  // что и localtime_r() выше. Если поставить 0 — для зон с DST
  // получим смещение на час меньше.
  //
  // Проверка на UTC+13: локальное время пересекает полночь,
  // наивное вычитание tm_hour даст -11 ч. Здесь же mktime
  // корректно разберётся с датой, и diff будет +13 ч.
  l.tm_isdst = -1;
  g.tm_isdst = -1;

  time_t t_local = mktime(&l);
  time_t t_utc = mktime(&g);

  // mktime возвращает -1 при ошибке. Оба вызова используют одну
  // и ту же TZ, так что ошибочная ситуация симметрична — в этом
  // случае честно отдаём 0, а не мусорное значение.
  if (t_local == (time_t)-1 || t_utc == (time_t)-1) return 0;

  return (int)(t_local - t_utc);
}

}  // namespace TzUtil