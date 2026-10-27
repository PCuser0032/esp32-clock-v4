#pragma once
#include <Arduino.h>

static const char WEB_CSS[] PROGMEM = R"CSS(
:root, :root[data-theme="dark"] {
  --bg:#0f1115;--card:#1a1d23;--card2:#0b0d10;--fg:#e8e8e8;--muted:#8a94a6;
  --accent:#4ea1ff;--ok:#4effa1;--err:#ff6b6b;--warn:#ffcb6b;
  --border:#2a2f38;--toast-bg:#20242c;--shadow:rgba(0,0,0,.4);
}
:root[data-theme="light"] {
  --bg:#f5f7fa;--card:#ffffff;--card2:#f0f2f5;--fg:#1a1d23;--muted:#5b6470;
  --accent:#0077d4;--ok:#0e9f4a;--err:#d32f2f;--warn:#c77700;
  --border:#e1e4e8;--toast-bg:#ffffff;--shadow:rgba(0,0,0,.12);
}

*{box-sizing:border-box}
html,body{transition:background-color .2s ease, color .2s ease}
body{margin:0;font-family:system-ui,-apple-system,sans-serif;
     background:var(--bg);color:var(--fg);min-height:100vh}

.conn-banner{
  position:fixed;top:0;left:0;right:0;
  padding:10px 16px;text-align:center;font-size:14px;z-index:200;
  background:var(--toast-bg);color:var(--fg);
  border-bottom:2px solid var(--border);
  box-shadow:0 4px 12px var(--shadow);
  transition:transform .25s ease, opacity .25s ease;
}
.conn-banner.hidden{transform:translateY(-100%);opacity:0;pointer-events:none}
.conn-banner.warn{border-bottom-color:var(--warn)}
.conn-banner.err {border-bottom-color:var(--err)}
.conn-banner.ok  {border-bottom-color:var(--ok)}

header{position:relative;display:flex;align-items:center;
       justify-content:center;padding:14px 16px;
       border-bottom:1px solid var(--border);
       transition:border-color .2s ease}
header h1{margin:0;font-size:16px;letter-spacing:.08em;color:var(--muted)}

.theme-toggle{
  position:absolute;right:16px;top:50%;transform:translateY(-50%);
  min-width:auto;width:auto;height:36px;padding:0 12px;
  background:var(--card);color:var(--fg);
  border:1px solid var(--border);border-radius:8px;
  font-size:13px;line-height:1;cursor:pointer;
  display:inline-flex;align-items:center;gap:6px;
  transition:background-color .15s, border-color .15s, color .15s;
}
.theme-toggle:hover{filter:brightness(1.05);border-color:var(--accent)}
.theme-icon{font-size:15px}

main{max-width:620px;margin:0 auto;padding:16px}
section{background:var(--card);border:1px solid var(--border);
        border-radius:10px;padding:16px;margin-bottom:14px;
        transition:background-color .2s ease, border-color .2s ease}
section h2{margin:0 0 12px;font-size:12px;text-transform:uppercase;
           letter-spacing:.1em;color:var(--muted)}
.clock{font-size:40px;font-weight:600;text-align:center;
       font-variant-numeric:tabular-nums;color:var(--accent)}
.date{text-align:center;color:var(--muted);margin-top:4px}
.kv{display:flex;justify-content:space-between;padding:6px 0;
    font-size:14px;border-bottom:1px dashed var(--border);gap:12px}
.kv:last-child{border-bottom:none}
.kv span:last-child{font-variant-numeric:tabular-nums;text-align:right}
label{display:block;font-size:12px;color:var(--muted);margin:10px 0 4px}
input,select{width:100%;padding:10px;background:var(--card2);color:var(--fg);
      border:1px solid var(--border);border-radius:6px;font-size:14px;
      font-family:inherit;transition:background-color .2s, border-color .2s}
input[type=checkbox],input[type=radio]{width:auto;margin-right:6px}
input[type=time]{max-width:140px}
input[type=number]{max-width:140px}
.row{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px;align-items:center}
button{padding:10px 16px;background:var(--accent);color:#fff;border:none;
       border-radius:6px;font-size:14px;cursor:pointer;
       transition:filter .15s, background-color .2s;
       min-width:120px}
button.sec{background:var(--card2);color:var(--fg);border:1px solid var(--border)}
button.small{min-width:auto;padding:10px 14px}
button:hover:not(:disabled){filter:brightness(1.08)}
button:disabled{opacity:.55;cursor:not-allowed}
.hint{font-size:12px;color:var(--muted);margin-left:auto}
.hint.ok{color:var(--ok)}.hint.err{color:var(--err)}.hint.warn{color:var(--warn)}
.radio-row{display:flex;gap:16px;margin:6px 0 12px}
.radio-row label{margin:0;font-size:14px;color:var(--fg);cursor:pointer}
.tz-block{margin-top:6px}
.ok{color:var(--ok)}.err{color:var(--err)}.warn{color:var(--warn)}

.datetime-row{display:flex;gap:8px;align-items:stretch;flex-wrap:wrap}
.datetime-row input{flex:1;min-width:200px;width:auto}
.datetime-row button{flex-shrink:0}

/* --- Дни недели (Будильник) --- */
.day-checks{display:flex;gap:6px;flex-wrap:wrap;margin:8px 0}
.day-check{
  padding:8px 12px;background:var(--card2);color:var(--fg);
  border:1px solid var(--border);border-radius:6px;
  font-size:14px;cursor:pointer;user-select:none;
  transition:background-color .15s, border-color .15s, color .15s;
  margin:0;
}
.day-check:hover{border-color:var(--accent)}
.day-check.checked{background:var(--accent);color:#fff;border-color:var(--accent)}

pre.diag{background:var(--card2);border:1px solid var(--border);
         border-radius:6px;padding:12px;font-size:12px;
         max-height:400px;overflow:auto;white-space:pre-wrap;
         color:var(--fg)}

.addr{display:flex;align-items:center;gap:6px;justify-content:flex-end;
      font-variant-numeric:tabular-nums}
.copy-btn{width:auto;min-width:auto;padding:2px 8px;font-size:12px;
          background:var(--card2);color:var(--muted);
          border:1px solid var(--border);border-radius:4px;
          cursor:pointer;line-height:1.4;
          transition:color .15s, border-color .15s}
.copy-btn:hover{color:var(--fg);border-color:var(--accent)}

#toasts{position:fixed;right:16px;bottom:16px;display:flex;
        flex-direction:column;gap:8px;z-index:99;pointer-events:none}
.toast{background:var(--toast-bg);border:1px solid var(--border);
       color:var(--fg);padding:10px 14px;border-radius:8px;font-size:14px;
       box-shadow:0 8px 24px var(--shadow);
       opacity:0;transform:translateY(8px);transition:.25s;max-width:280px}
.toast.show{opacity:1;transform:none}
.toast.ok  {border-color:var(--ok)}
.toast.err {border-color:var(--err)}
.toast.warn{border-color:var(--warn)}

@media (max-width: 480px){
  header h1{font-size:14px}
  .theme-toggle{height:32px;padding:0 8px;font-size:12px}
  .datetime-row button{width:100%}
}
)CSS";