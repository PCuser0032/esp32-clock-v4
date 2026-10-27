#pragma once
#include <Arduino.h>

static const char WEB_JS[] PROGMEM = R"JS(
const $ = id => document.getElementById(id);
let loaded = false;
let netSaveAt = 0;
let netSaveSSID = '';
let rebootCountdownTimer = null;
let mdnsTarget = '';
let bannerMode = 'idle';
let bannerTimer = null;
let deviceUnreachable = false;

/* ---------- Тема ---------- */

function applyTheme(t) {
  document.documentElement.setAttribute('data-theme', t);
  const btn = $('theme-toggle');
  if (btn) {
    if (t === 'light') {
      btn.innerHTML = '<span class="theme-icon">☀</span>Светлая';
    } else {
      btn.innerHTML = '<span class="theme-icon">🌙</span>Тёмная';
    }
  }
  localStorage.setItem('theme', t);
}

function initTheme() {
  const saved = localStorage.getItem('theme') || 'dark';
  applyTheme(saved);
  $('theme-toggle').addEventListener('click', () => {
    const cur = document.documentElement.getAttribute('data-theme') || 'dark';
    applyTheme(cur === 'dark' ? 'light' : 'dark');
  });
}

/* ---------- Баннер ---------- */

function showBanner(html, kind='warn') {
  const b = $('conn-banner');
  b.innerHTML = html;
  b.className = 'conn-banner ' + kind;
  b.classList.remove('hidden');
  if (bannerTimer) { clearTimeout(bannerTimer); bannerTimer = null; }
}

function showBannerAutoHide(html, kind='ok', ms=3000) {
  showBanner(html, kind);
  bannerTimer = setTimeout(() => hideBanner(), ms);
}

function hideBanner() {
  $('conn-banner').classList.add('hidden');
  if (bannerTimer) { clearTimeout(bannerTimer); bannerTimer = null; }
}

/* ---------- Дата/время браузера ---------- */

function pad2(n) { return String(n).padStart(2, '0'); }

function browserLocalNow() {
  const now = new Date();
  return `${now.getFullYear()}-${pad2(now.getMonth() + 1)}-${pad2(now.getDate())}`
       + `T${pad2(now.getHours())}:${pad2(now.getMinutes())}:${pad2(now.getSeconds())}`;
}

/* ---------- TZ пресеты ---------- */

const TZ_PRESETS = [
  ["UTC0",                                 "UTC (UTC+00:00)"],
  ["GMT0BST,M3.5.0/1,M10.5.0",             "Лондон (GMT/BST)"],
  ["CET-1CEST,M3.5.0,M10.5.0/3",           "Берлин/Париж (CET/CEST)"],
  ["EET-2EEST,M3.5.0/3,M10.5.0/4",         "Киев/Хельсинки (EET/EEST)"],
  ["MSK-3",                                "Москва (UTC+03:00, без DST)"],
  ["EST5EDT,M3.2.0,M11.1.0",               "Нью-Йорк (EST/EDT)"],
  ["CST6CDT,M3.2.0,M11.1.0",               "Чикаго (CST/CDT)"],
  ["MST7MDT,M3.2.0,M11.1.0",               "Денвер (MST/MDT)"],
  ["PST8PDT,M3.2.0,M11.1.0",               "Лос-Анджелес (PST/PDT)"],
  ["JST-9",                                "Токио (UTC+09:00)"],
  ["AEST-10AEDT,M10.1.0,M4.1.0/3",         "Сидней (AEST/AEDT)"],
  ["__custom__",                           "Другая…"]
];

const OFFSET_RANGE = [-12,-11,-10,-9,-8,-7,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14];

function fmtOffsetHuman(h){
  const sign = h >= 0 ? '+' : '−';
  return `UTC${sign}${String(Math.abs(h)).padStart(2,'0')}:00`;
}

function buildTzFromOffsetClient(h){
  if (h === 0) return "UTC0";
  return `<${h >= 0 ? '+' : '-'}${String(Math.abs(h)).padStart(2,'0')}>${-h}`;
}

function parseTzOffset(tz){
  let m = tz.match(/^<([+-]\d{2})>(-?\d+)$/);
  if (m) {
    const off  = parseInt(m[1]);
    const posix= parseInt(m[2]);
    if (posix === -off) return off;
  }
  if (tz === 'UTC0' || tz === 'GMT0' || tz === 'UTC') return 0;
  return null;
}

function buildTzOffsetSelect(){
  const sel = $('tz-offset');
  sel.innerHTML = '';
  for (const h of OFFSET_RANGE){
    const o = document.createElement('option');
    o.value = String(h);
    o.textContent = fmtOffsetHuman(h);
    sel.appendChild(o);
  }
}

function buildTzSelect(){
  const sel = $('tz-preset');
  sel.innerHTML = '';
  for (const [v,label] of TZ_PRESETS){
    const o = document.createElement('option');
    o.value = v; o.textContent = label;
    sel.appendChild(o);
  }
  sel.addEventListener('change', () => {
    $('tz-custom').style.display =
      sel.value === '__custom__' ? 'block' : 'none';
    updateTzNamedPreview();
  });
  $('tz-custom').addEventListener('input', updateTzNamedPreview);
}

function setTzUI(tz){
  const off = parseTzOffset(tz);
  if (off !== null){
    setTzMode('offset');
    $('tz-offset').value = String(off);
    updateTzOffsetPreview();
    return;
  }
  setTzMode('named');
  const known = TZ_PRESETS.some(([v]) => v === tz && v !== '__custom__');
  if (known){
    $('tz-preset').value = tz;
    $('tz-custom').style.display = 'none';
  } else {
    $('tz-preset').value = '__custom__';
    $('tz-custom').style.display = 'block';
    $('tz-custom').value = tz;
  }
  updateTzNamedPreview();
}

function setTzMode(mode){
  document.querySelectorAll('input[name=tzmode]').forEach(r => {
    r.checked = (r.value === mode);
  });
  $('tz-offset-block').style.display = (mode === 'offset') ? 'block' : 'none';
  $('tz-named-block').style.display  = (mode === 'named')  ? 'block' : 'none';
}

function getTzMode(){
  const r = document.querySelector('input[name=tzmode]:checked');
  return r ? r.value : 'offset';
}

function getTzValue(){
  if (getTzMode() === 'offset'){
    return buildTzFromOffsetClient(parseInt($('tz-offset').value));
  }
  const sel = $('tz-preset');
  return sel.value === '__custom__' ? $('tz-custom').value.trim() : sel.value;
}

function updateTzOffsetPreview(){
  const h = parseInt($('tz-offset').value);
  $('tz-offset-preview').textContent =
    `Будет использоваться: ${fmtOffsetHuman(h)} (без перехода на летнее время)`;
}

function updateTzNamedPreview(){
  const tz = getTzValue();
  $('tz-named-preview').textContent = tz ? `POSIX TZ: ${tz}` : 'Введите POSIX TZ';
}

/* ---------- Утилиты ---------- */

function toast(msg, kind='ok'){
  const el = document.createElement('div');
  el.className = 'toast ' + kind;
  el.textContent = msg;
  $('toasts').appendChild(el);
  requestAnimationFrame(()=> el.classList.add('show'));
  setTimeout(()=>{ el.classList.remove('show');
                   setTimeout(()=> el.remove(), 300); }, 3000);
}

function hint(id, text, kind=''){
  const el = $(id);
  el.className = 'hint ' + kind;
  el.textContent = text || '';
}

async function api(path, method='GET', body){
  const opt = {method, headers:{'Content-Type':'application/json'}};
  if (body !== undefined) opt.body = JSON.stringify(body);
  const r = await fetch(path, opt);
  const txt = await r.text();
  try { return JSON.parse(txt || '{}'); } catch { return {}; }
}

async function withBusy(btn, fn){
  const orig = btn.textContent;
  btn.disabled = true;
  btn.textContent = '…';
  try { return await fn(); }
  finally { btn.disabled = false; btn.textContent = orig; }
}

async function copyToClipboard(text) {
  try {
    if (navigator.clipboard && window.isSecureContext) {
      await navigator.clipboard.writeText(text);
      return true;
    }
  } catch(e) { /* fallthrough */ }
  try {
    const ta = document.createElement('textarea');
    ta.value = text;
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.appendChild(ta);
    ta.focus();
    ta.select();
    const ok = document.execCommand('copy');
    document.body.removeChild(ta);
    return ok;
  } catch(e) { return false; }
}

/* ---------- Миграция на mDNS ---------- */

async function tryMigrate() {
  if (window.location.hostname.endsWith('.local')) return false;
  const host  = localStorage.getItem('pendingRedirectHost');
  const until = parseInt(localStorage.getItem('pendingRedirectUntil') || '0');
  if (!host || Date.now() > until) {
    if (host) {
      localStorage.removeItem('pendingRedirectHost');
      localStorage.removeItem('pendingRedirectUntil');
    }
    return false;
  }
  if (window.location.hostname === host) return false;
  try {
    const url = `http://${host}/api/status`;
    const r = await fetch(url, {method: 'GET', cache: 'no-store'});
    if (r.ok) {
      localStorage.removeItem('pendingRedirectHost');
      localStorage.removeItem('pendingRedirectUntil');
      console.log('[migrate] device found at', host, '→ redirecting');
      window.location.href = `http://${host}/`;
      return true;
    }
  } catch(e) { /* ещё недоступно */ }
  return false;
}

/* ---------- Опрос состояния ---------- */

async function refresh(){
  try {
    const s = await api('/api/status');

    if (s.mdns_host_target) mdnsTarget = s.mdns_host_target;

    if (deviceUnreachable) {
      deviceUnreachable = false;
      if (bannerMode === 'rebooting' || bannerMode === 'waiting_wifi') {
        if (s.wifi_mode === 'STA+AP') {
          bannerMode = 'idle';
          showBannerAutoHide(`Подключено к ${netSaveSSID || 'сети'}`, 'ok', 3500);
          netSaveAt   = 0;
          netSaveSSID = '';
        } else {
          bannerMode = 'waiting_wifi';
          showBanner(`Устройство готово, подключение к Wi-Fi…`, 'warn');
          netSaveAt = Date.now();
        }
      } else if (bannerMode === 'idle') {
        showBannerAutoHide('Соединение восстановлено', 'ok', 2500);
      }
    }

    if (bannerMode === 'waiting_wifi') {
      if (s.wifi_mode === 'STA+AP') {
        bannerMode = 'idle';
        showBannerAutoHide(`Подключено к ${netSaveSSID || 'сети'}`, 'ok', 3500);
        netSaveAt   = 0;
        netSaveSSID = '';
      } else if (netSaveAt > 0 && Date.now() - netSaveAt > 40000) {
        bannerMode = 'idle';
        showBanner(
          `Не удалось подключиться к «${netSaveSSID}».<br>` +
          `Проверьте SSID и пароль. Подключитесь к <b>ESP32-CLOCK</b> ` +
          `и откройте <a href="http://192.168.4.1/">192.168.4.1</a>.`,
          'err');
        bannerTimer = setTimeout(() => hideBanner(), 15000);
        netSaveAt   = 0;
      } else {
        const elapsed = Math.round((Date.now() - netSaveAt) / 1000);
        const mdns = mdnsTarget ? mdnsTarget : 'esp32-clock.local';
        showBanner(
          `Подключение к «${netSaveSSID}»… (${elapsed} с)<br>` +
          `<span style="opacity:.75">Ожидание по адресу ` +
          `<b>${mdns}</b></span>`,
          'warn');
      }
    }

    /* --- Шапка: время --- */
    $('clock').textContent = s.time;
    $('date').textContent  = s.date + '  ' + s.weekday;

    /* --- Статус --- */
    $('wifi-mode').textContent = s.wifi_mode;
    $('sta-ip').textContent    = s.sta_ip || '—';

    if (s.mdns_host) {
      $('mdns-host').textContent = s.mdns_host;
      $('mdns-host').style.opacity = '1';
      $('mdns-host').title = 'Активно';
    } else if (s.mdns_host_target) {
      $('mdns-host').textContent = s.mdns_host_target;
      $('mdns-host').style.opacity = '0.5';
      $('mdns-host').title = 'Будет доступно после подключения к Wi-Fi';
    } else {
      $('mdns-host').textContent = '—';
      $('mdns-host').style.opacity = '1';
    }

    $('ap-ip').textContent = `${s.ap_ip || '—'} / ${s.ap_clients ?? 0}`;
    $('tz-effective').textContent = s.tz_effective;

    $('ntp').innerHTML = s.ntp_synced
      ? '<span class="ok">синхронизировано</span>'
      : '<span class="warn">нет</span>';

    if (!s.rtc_ok)        $('rtc').innerHTML = '<span class="err">ошибка</span>';
    else if (s.rtc_lost)  $('rtc').innerHTML = '<span class="warn">потеря питания</span>';
    else                  $('rtc').innerHTML = '<span class="ok">OK</span>';

    /* --- Звук --- */
    $('sound-state').innerHTML = s.sound_enabled
      ? '<span class="ok">ON</span>'
      : '<span class="warn">OFF</span>';

    /* --- Будильник --- */
    if (s.alarm_ringing) {
      $('alarm-state').innerHTML = '<span class="err">звенит</span>';
    } else if (s.alarm_enabled) {
      $('alarm-state').innerHTML =
        `<span class="ok">${pad2(s.alarm_h)}:${pad2(s.alarm_m)}</span>`;
    } else {
      $('alarm-state').textContent = 'выключен';
    }

    /* --- Климат --- */
    if (s.temperature !== null && s.temperature !== undefined) {
      $('climate-temp').textContent = s.temperature.toFixed(1) + ' °C';
      $('climate-hum').textContent  = s.humidity.toFixed(1) + ' %';
    } else {
      $('climate-temp').textContent = '—';
      $('climate-hum').textContent  = '—';
    }

    /* --- Heap --- */
    if (typeof s.heap_free === 'number'){
      const free = Math.round(s.heap_free / 1024);
      const min  = Math.round(s.heap_min_free / 1024);
      const big  = Math.round(s.heap_max_alloc / 1024);
      const warn = (s.heap_free < 30000) ? ' err' : '';
      $('heap').innerHTML =
        `<span class="${warn.trim()}">${free}K / ${min}K / max ${big}K</span>`;
    }

    /* --- Инициализация полей (один раз) --- */
    if (!loaded){
      setTzUI(s.tz);
      for (let i = 0; i < 3; i++)
        $('ntp-' + i).value = (Array.isArray(s.ntp) && s.ntp[i]) || '';
      $('h24').checked = s.h24;
      $('ssid').value = s.ssid || '';

      // Калибровка
      $('temp-offset').value = ((s.temp_offset_x10 || 0) / 10).toFixed(1);

      // Будильник
      $('alarm-enabled').checked = !!s.alarm_enabled;
      $('alarm-time').value =
        `${pad2(s.alarm_h ?? 7)}:${pad2(s.alarm_m ?? 0)}`;
      document.querySelectorAll('.day-check').forEach(el => {
        const m = parseInt(el.dataset.mask);
        el.classList.toggle('checked', (s.alarm_days & m) !== 0);
      });

      loaded = true;
    }
  } catch(e) {
    if (!deviceUnreachable) {
      deviceUnreachable = true;
      if (bannerMode === 'countdown' || bannerMode === 'rebooting') {
        bannerMode = 'rebooting';
        showBanner(
          `Устройство перезагружается.<br>` +
          `<span style="opacity:.75">Если вы уже в домашней сети — ` +
          `страница переключится автоматически…</span>`,
          'warn');
      } else if (bannerMode !== 'waiting_wifi') {
        showBanner('Нет соединения с устройством', 'err');
      }
    }
    tryMigrate();
  }
}

/* ---------- Действия: время ---------- */

$('btn-now').addEventListener('click', () => {
  $('set-dt').value = browserLocalNow();
  toast('Подставлено текущее время браузера');
});

$('btn-set').addEventListener('click', () => withBusy($('btn-set'), async () => {
  const v = $('set-dt').value;
  if (!v){ toast('Выберите дату и время', 'warn'); return; }
  const r = await api('/api/time', 'POST', {datetime:v});
  if (r.ok){ toast('Время установлено'); hint('hint-time','OK','ok'); }
  else     { toast('Не удалось установить время', 'err');
             hint('hint-time','ошибка','err'); }
  refresh();
}));

$('btn-sync').addEventListener('click', () => withBusy($('btn-sync'), async () => {
  const r = await api('/api/sync', 'POST');
  if (r.ok){ toast('Запущена синхронизация'); hint('hint-time','запущено','ok'); }
  else     { toast('NTP недоступен', 'err');  hint('hint-time','нет связи','err'); }
  refresh();
}));

/* ---------- Сохранение: параметры времени ---------- */

$('btn-save-cfg').addEventListener('click', () => withBusy($('btn-save-cfg'), async () => {
  const tz = getTzValue();
  if (!tz){ toast('Укажите часовой пояс', 'warn'); return; }
  const ntp = [0,1,2].map(i => $('ntp-' + i).value.trim());
  const r = await api('/api/settings','POST',{ tz, ntp, h24: $('h24').checked });
  if (r.ok){ toast('Настройки сохранены'); hint('hint-cfg','OK','ok'); }
  else     { toast('Ошибка сохранения', 'err'); hint('hint-cfg','ошибка','err'); }
  loaded = false; refresh();
}));

/* ---------- Сохранение: микроклимат ---------- */

$('btn-save-climate').addEventListener('click', () => withBusy($('btn-save-climate'), async () => {
  const v = parseFloat($('temp-offset').value);
  if (isNaN(v)) { toast('Введите число', 'warn'); return; }
  if (v < -2 || v > 2) { toast('Диапазон: −2.0 … +2.0 °C', 'warn'); return; }
  const r = await api('/api/settings','POST', { temp_offset: v });
  if (r.ok){ toast('Калибровка сохранена'); hint('hint-climate','OK','ok'); }
  else     { toast('Ошибка сохранения', 'err'); hint('hint-climate','ошибка','err'); }
  refresh();
}));

/* ---------- Сохранение: будильник ---------- */

$('btn-save-alarm').addEventListener('click', () => withBusy($('btn-save-alarm'), async () => {
  const tval = $('alarm-time').value || '07:00';
  const parts = tval.split(':').map(Number);
  if (parts.length < 2 || parts.some(isNaN)) {
    toast('Неверное время будильника', 'warn'); return;
  }
  let mask = 0;
  document.querySelectorAll('.day-check.checked').forEach(el => {
    mask |= parseInt(el.dataset.mask);
  });
  if (mask === 0) {
    toast('Выберите хотя бы один день', 'warn'); return;
  }
  const body = {
    alarm_enabled: $('alarm-enabled').checked,
    alarm_h: parts[0],
    alarm_m: parts[1],
    alarm_days: mask,
  };
  const r = await api('/api/settings','POST', body);
  if (r.ok){ toast('Будильник сохранён'); hint('hint-alarm','OK','ok'); }
  else     { toast('Ошибка сохранения', 'err'); hint('hint-alarm','ошибка','err'); }
  refresh();
}));

/* ---------- Сеть ---------- */

$('btn-save-net').addEventListener('click', () => withBusy($('btn-save-net'), async () => {
  const ssid = $('ssid').value.trim();
  if (!ssid){ toast('Введите SSID', 'warn'); return; }

  const ok = confirm(
    `Устройство перезагрузится и подключится к сети «${ssid}».\n\n` +
    `После этого эта страница потеряет связь с устройством, ` +
    `потому что вы сейчас в сети ESP32-CLOCK.\n\n` +
    `Если ваш телефон/компьютер тоже подключится к сети «${ssid}», ` +
    `страница переключится автоматически.\n\n` +
    `Иначе вернитесь в Wi-Fi и подключитесь к ESP32-CLOCK.`
  );
  if (!ok) return;

  const r = await api('/api/network','POST',{ssid, password:$('pass').value});
  if (!r.ok) {
    toast('Ошибка сохранения', 'err');
    hint('hint-net', 'ошибка сохранения', 'err');
    return;
  }

  const target = mdnsTarget || 'esp32-clock.local';
  localStorage.setItem('pendingRedirectHost', target);
  localStorage.setItem('pendingRedirectUntil', String(Date.now() + 180000));

  netSaveSSID = ssid;
  netSaveAt   = 0;
  bannerMode  = 'countdown';

  const totalMs = r.reboot_in_ms || 5000;
  let remaining = Math.ceil(totalMs / 1000);
  showBanner(`Сохранено. Отключение через ${remaining}…`, 'warn');

  if (rebootCountdownTimer) clearInterval(rebootCountdownTimer);
  rebootCountdownTimer = setInterval(() => {
    remaining--;
    if (remaining > 0) {
      showBanner(`Сохранено. Отключение через ${remaining}…`, 'warn');
    } else {
      clearInterval(rebootCountdownTimer);
      rebootCountdownTimer = null;
      bannerMode = 'rebooting';
      showBanner(
        `Устройство перезагружается.<br>` +
        `<span style="opacity:.75">Если вы уже в сети «${ssid}» — ` +
        `страница переключится автоматически…</span>`,
        'warn');
      netSaveAt = Date.now();
    }
  }, 1000);

  toast('Сохранено, устройство перезагружается');
}));

$('btn-clear-net').addEventListener('click', () => withBusy($('btn-clear-net'), async () => {
  if (!confirm('Удалить сохранённую сеть? Устройство вернётся в режим AP-only.')) return;

  localStorage.removeItem('pendingRedirectHost');
  localStorage.removeItem('pendingRedirectUntil');

  const r = await api('/api/network','DELETE');
  if (!r.ok) { toast('Ошибка сброса', 'err'); return; }

  netSaveSSID = '';
  netSaveAt   = 0;
  bannerMode  = 'countdown';

  const totalMs = r.reboot_in_ms || 1500;
  let remaining = Math.ceil(totalMs / 1000);
  showBanner(`Сброшено. Перезагрузка через ${remaining}…`, 'warn');

  if (rebootCountdownTimer) clearInterval(rebootCountdownTimer);
  rebootCountdownTimer = setInterval(() => {
    remaining--;
    if (remaining > 0) {
      showBanner(`Сброшено. Перезагрузка через ${remaining}…`, 'warn');
    } else {
      clearInterval(rebootCountdownTimer);
      rebootCountdownTimer = null;
      bannerMode = 'rebooting';
      showBanner('Устройство перезагружается в AP-only…', 'warn');
    }
  }, 1000);

  toast('Сеть сброшена, устройство перезагружается');
}));

/* ---------- Диагностика ---------- */

$('btn-diag').addEventListener('click', () => withBusy($('btn-diag'), async () => {
  try{
    const r = await fetch('/api/diag');
    $('diag-out').textContent = await r.text();
  }catch(e){
    $('diag-out').textContent = 'Ошибка запроса /api/diag';
  }
}));

$('btn-ntp-restart').addEventListener('click', () => withBusy($('btn-ntp-restart'), async () => {
  const r = await api('/api/ntp-restart','POST');
  toast(r.ok ? 'SNTP перезапущен' : 'Не удалось', r.ok ? 'ok' : 'err');
}));

$('btn-test-beep').addEventListener('click', () => withBusy($('btn-test-beep'), async () => {
  const r = await api('/api/test/beep','POST', {freq:2000, ms:200});
  toast(r.ok ? 'Бип' : 'Ошибка', r.ok ? 'ok' : 'err');
}));

$('btn-test-alarm').addEventListener('click', () => withBusy($('btn-test-alarm'), async () => {
  const r = await api('/api/test/alarm','POST', {});
  toast(r.ok ? 'Длинный бип 3 с' : 'Ошибка', r.ok ? 'ok' : 'err');
}));

/* ---------- Обслуживание ---------- */

$('btn-factory-reset').addEventListener('click', () => withBusy($('btn-factory-reset'), async () => {
  if (!confirm('Полный сброс всех настроек (Wi-Fi, TZ, NTP, климат, будильник)?')) return;
  localStorage.removeItem('pendingRedirectHost');
  localStorage.removeItem('pendingRedirectUntil');
  await api('/api/factory-reset','POST');
  toast('Сброшено, перезагрузка…');
  hint('hint-reset','сброшено','warn');
}));

/* ---------- Инициализация ---------- */

initTheme();
buildTzOffsetSelect();
buildTzSelect();
document.querySelectorAll('input[name=tzmode]').forEach(r => {
  r.addEventListener('change', () => setTzMode(r.value));
});
$('tz-offset').addEventListener('change', updateTzOffsetPreview);
setTzMode('offset');
updateTzOffsetPreview();
$('set-dt').value = browserLocalNow();

document.querySelectorAll('.day-check').forEach(el => {
  el.addEventListener('click', () => el.classList.toggle('checked'));
});

document.querySelectorAll('.copy-btn').forEach(btn => {
  btn.addEventListener('click', async () => {
    const src = $(btn.dataset.copy);
    const text = src ? src.textContent : '';
    if (!text || text === '—') {
      toast('Значение недоступно', 'warn');
      return;
    }
    if (await copyToClipboard(text)) toast('Скопировано: ' + text);
    else toast('Не удалось скопировать', 'err');
  });
});

(async () => {
  if (localStorage.getItem('pendingRedirectHost')) {
    await tryMigrate();
  }
})();

setInterval(refresh, 1000);
refresh();
)JS";