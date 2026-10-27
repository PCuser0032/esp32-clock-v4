#pragma once
#include <Arduino.h>

static const char WEB_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CLOCK</title>
<script>
  (function(){
    var t = localStorage.getItem('theme') || 'dark';
    document.documentElement.setAttribute('data-theme', t);
  })();
</script>
<link rel="stylesheet" href="/style.css?v=6">
</head>
<body>

<div id="conn-banner" class="conn-banner hidden">
  <span id="conn-banner-text"></span>
</div>

<header>
  <h1>ESP32-CLOCK</h1>
  <button id="theme-toggle" class="theme-toggle" title="Переключить тему"></button>
</header>
<main>

<section>
  <h2>Текущее время</h2>
  <div class="clock" id="clock">--:--:--</div>
  <div class="date"  id="date">--.--.---- ---</div>
</section>

<section>
  <h2>Микроклимат</h2>
  <div class="kv"><span>Температура</span><span id="climate-temp">—</span></div>
  <div class="kv"><span>Влажность</span><span id="climate-hum">—</span></div>
  <label>Калибровка температуры (°C, шаг 0.1)</label>
  <input type="number" id="temp-offset" min="-2" max="2" step="0.1" value="0">
  <div class="hint">Диапазон ±2 °C — паспортная погрешность DHT22. Применяется к показаниям датчика.</div>
  <div class="row">
    <button id="btn-save-climate">Сохранить</button>
    <span class="hint" id="hint-climate"></span>
  </div>
</section>

<section>
  <h2>Статус</h2>
  <div class="kv"><span>Wi-Fi режим</span><span id="wifi-mode">—</span></div>
  <div class="kv">
    <span>Локальное имя</span>
    <span class="addr">
      <span id="mdns-host">—</span>
      <button class="copy-btn" data-copy="mdns-host" title="Скопировать">⎘</button>
    </span>
  </div>
  <div class="kv">
    <span>STA IP</span>
    <span class="addr">
      <span id="sta-ip">—</span>
      <button class="copy-btn" data-copy="sta-ip" title="Скопировать">⎘</button>
    </span>
  </div>
  <div class="kv"><span>AP IP / клиентов</span><span id="ap-ip">—</span></div>
  <div class="kv"><span>Часовой пояс</span><span id="tz-effective">—</span></div>
  <div class="kv"><span>NTP</span><span id="ntp">—</span></div>
  <div class="kv"><span>DS3231</span><span id="rtc">—</span></div>
  <div class="kv"><span>Звук</span><span id="sound-state">—</span></div>
  <div class="kv"><span>Будильник</span><span id="alarm-state">—</span></div>
  <div class="kv"><span>Heap / min</span><span id="heap">—</span></div>
</section>

<section>
  <h2>Управление временем</h2>
  <label>Ручная установка (локальное время)</label>
  <div class="datetime-row">
    <input type="datetime-local" id="set-dt" step="1">
    <button class="sec small" id="btn-now" title="Подставить текущее время браузера">Использовать время браузера</button>
  </div>
  <div class="row">
    <button id="btn-set">Установить</button>
    <button class="sec" id="btn-sync">Синхронизировать NTP</button>
    <span class="hint" id="hint-time"></span>
  </div>
</section>

<section>
  <h2>Параметры времени</h2>

  <label>Режим часового пояса</label>
  <div class="radio-row">
    <label><input type="radio" name="tzmode" value="offset"> Смещение от UTC</label>
    <label><input type="radio" name="tzmode" value="named"> Именованный / POSIX</label>
  </div>

  <div id="tz-offset-block" class="tz-block">
    <label>Смещение (часы)</label>
    <select id="tz-offset"></select>
    <div class="hint" id="tz-offset-preview"></div>
  </div>

  <div id="tz-named-block" class="tz-block" style="display:none">
    <label>Часовой пояс</label>
    <select id="tz-preset"></select>
    <input type="text" id="tz-custom" style="display:none;margin-top:8px"
           placeholder="POSIX TZ, напр. CET-1CEST,M3.5.0,M10.5.0/3">
    <div class="hint" id="tz-named-preview"></div>
  </div>

  <label>NTP серверы (по порядку попыток)</label>
  <input type="text" id="ntp-0" placeholder="основной, напр. pool.ntp.org">
  <input type="text" id="ntp-1" style="margin-top:6px" placeholder="резервный">
  <input type="text" id="ntp-2" style="margin-top:6px" placeholder="резервный">
  <label style="margin-top:14px">
    <input type="checkbox" id="h24"> 24-часовой формат
  </label>
  <div class="row">
    <button id="btn-save-cfg">Сохранить</button>
    <span class="hint" id="hint-cfg"></span>
  </div>
</section>

<section>
  <h2>Будильник</h2>
  <label>
    <input type="checkbox" id="alarm-enabled"> Будильник включён
  </label>
  <label>Время срабатывания</label>
  <input type="time" id="alarm-time" value="07:00">
  <label style="margin-top:14px">Дни недели</label>
  <div class="day-checks">
    <label class="day-check" data-mask="2">Пн</label>
    <label class="day-check" data-mask="4">Вт</label>
    <label class="day-check" data-mask="8">Ср</label>
    <label class="day-check" data-mask="16">Чт</label>
    <label class="day-check" data-mask="32">Пт</label>
    <label class="day-check" data-mask="64">Сб</label>
    <label class="day-check" data-mask="1">Вс</label>
  </div>
  <div class="hint">По умолчанию — Пн–Пт. Если у вас график 2/2, «сутки-трое» и т.п. — отметьте нужные дни.</div>
  <div class="row">
    <button id="btn-save-alarm">Сохранить</button>
    <span class="hint" id="hint-alarm"></span>
  </div>
</section>

<section>
  <h2>Сеть Wi-Fi</h2>
  <label>SSID</label>
  <input type="text" id="ssid" placeholder="имя сети">
  <label>Пароль</label>
  <input type="password" id="pass" placeholder="пароль">
  <div class="row">
    <button id="btn-save-net">Сохранить</button>
    <button class="sec" id="btn-clear-net">Сбросить сеть</button>
    <span class="hint" id="hint-net"></span>
  </div>
</section>

<section>
  <h2>Диагностика</h2>
  <div class="row">
    <button class="sec" id="btn-diag">Показать /api/diag</button>
    <button class="sec" id="btn-ntp-restart">Перезапустить SNTP</button>
  </div>
  <div class="row">
    <button class="sec" id="btn-test-beep">Тест: бип 200 мс</button>
    <button class="sec" id="btn-test-alarm">Тест: длинный бип (3 с)</button>
    <span class="hint" id="hint-test"></span>
  </div>
  <pre class="diag" id="diag-out">Нажмите «Показать /api/diag».</pre>
</section>

<section>
  <h2>Обслуживание</h2>
  <div class="row">
    <button class="sec" id="btn-factory-reset">Полный сброс настроек</button>
    <span class="hint" id="hint-reset"></span>
  </div>
  <div class="hint" style="margin-top:8px">
    Физические кнопки:<br>
    • B1 (GPIO4) — синхронизация NTP<br>
    • B2 (GPIO13) — звук вкл/выкл; тестовый бип (удержание)<br>
    • B1+B2 (3 с) — перезагрузка<br>
    • B1+B2 (10 с) — полный сброс
  </div>
</section>

</main>
<div id="toasts"></div>
<script src="/app.js?v=6"></script>
</body>
</html>
)HTML";