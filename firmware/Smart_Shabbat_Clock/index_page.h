// ---------------------- index_page.h ----------------------
#include <pgmspace.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="he" dir="rtl">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>שעון שבת חכם</title>
  <style>
    :root {
      --bg: #f6f2ea;
      --surface: #fffdf8;
      --surface-soft: #fbf7ef;
      --text: #22262d;
      --muted: #6d7480;
      --muted-2: #8a9099;
      --border: #e7ded1;
      --green: #16834d;
      --green-soft: #e7f5ec;
      --amber: #b98518;
      --amber-soft: #fff4d8;
      --red: #c43d36;
      --red-soft: #fdebea;
      --neutral: #5f6874;
      --neutral-soft: #eef0f2;
      --control: #efe8dc;
      --shadow: 0 18px 45px rgba(64, 48, 28, 0.12);
    }

    * { box-sizing: border-box; }

    html, body { margin: 0; min-height: 100%; }

    body {
      background: var(--bg);
      color: var(--text);
      direction: rtl;
      font-family: "Segoe UI", Arial, sans-serif;
      text-align: right;
    }

    button, input, select { font: inherit; }

    button {
      border: 0;
      cursor: pointer;
    }

    button:disabled, select:disabled {
      cursor: not-allowed;
      opacity: 0.48;
    }

    .topbar {
      border-bottom: 1px solid rgba(231, 222, 209, 0.8);
      background: rgba(255, 253, 248, 0.92);
    }

    .topbar-inner {
      width: min(100%, 980px);
      margin: 0 auto;
      padding: 30px 20px 24px;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      gap: 12px;
      text-align: center;
    }

    .brand {
      display: flex;
      flex-direction: column;
      align-items: center;
      min-width: 0;
    }

    .brand-name {
      font-size: clamp(32px, 5vw, 46px);
      line-height: 1.1;
      font-weight: 700;
      letter-spacing: 0;
    }

    .header-status {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 12px;
      min-width: 0;
      flex-wrap: wrap;
    }

    .clock {
      direction: ltr;
      padding: 0;
      border: 0;
      background: transparent;
      font-size: 24px;
      font-weight: 600;
      font-variant-numeric: tabular-nums;
      text-align: center;
      white-space: nowrap;
      color: var(--muted);
    }

    .dots {
      display: flex;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap;
      justify-content: flex-end;
    }

    .dot-line {
      color: var(--red);
      display: inline-flex;
      align-items: center;
      gap: 7px;
      font-size: 14px;
      font-weight: 600;
      white-space: nowrap;
    }

    .dot {
      width: 8px;
      height: 8px;
      border-radius: 999px;
      background: var(--green);
      flex: 0 0 auto;
    }

    .dot.bad { background: var(--red); }
    .dot.warn { background: var(--amber); }

    .banner {
      display: none;
      border-bottom: 1px solid #f1b7b3;
      background: var(--red-soft);
    }

    .banner.time {
      border-bottom-color: #eed8a9;
      background: var(--amber-soft);
    }

    .banner.show { display: block; }

    .banner-inner {
      width: min(100%, 1120px);
      margin: 0 auto;
      padding: 12px 24px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      color: #7c211d;
      font-size: 15px;
      font-weight: 600;
    }

    .banner.time .banner-inner { color: #6d4a08; }

    .banner-title {
      display: flex;
      align-items: center;
      gap: 10px;
    }

    .warn-symbol {
      width: 22px;
      height: 22px;
      display: grid;
      place-items: center;
      border-radius: 999px;
      border: 1px solid currentColor;
      font-weight: 700;
      line-height: 1;
      flex: 0 0 auto;
    }

    .retry {
      color: #7c211d;
      background: #fff;
      border: 1px solid #efc5c2;
      border-radius: 10px;
      padding: 8px 12px;
      font-size: 14px;
      font-weight: 600;
    }

    .page {
      width: min(100%, 980px);
      margin: 0 auto;
      padding: 26px 20px 36px;
    }

    .dashboard {
      display: grid;
      gap: 14px;
    }

    .main-col {
      display: grid;
      grid-template-columns: minmax(0, 3fr) minmax(280px, 2fr);
      grid-template-areas:
        "schedule state"
        "details details";
      gap: 18px;
      align-items: start;
      direction: ltr;
    }

    .state-col {
      display: grid;
      gap: 14px;
      grid-area: state;
      align-content: start;
      direction: rtl;
    }

    .schedule-card {
      grid-area: schedule;
      direction: rtl;
    }

    .details-card {
      grid-area: details;
      box-shadow: none;
      background: rgba(255, 253, 248, 0.72);
      direction: rtl;
    }

    .card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 16px;
      box-shadow: var(--shadow);
      overflow: hidden;
    }

    .hero {
      padding: 20px;
      display: grid;
      grid-template-columns: minmax(0, 1fr) auto;
      gap: 16px;
      align-items: start;
      background: var(--surface);
    }

    .hero.shabbat { background: var(--surface); }

    .badge {
      display: inline-flex;
      align-items: center;
      width: max-content;
      border: 1px solid #d8e9dd;
      color: var(--green);
      background: var(--green-soft);
      border-radius: 999px;
      padding: 6px 12px;
      font-size: 14px;
      font-weight: 600;
    }

    .badge.shabbat {
      border-color: #ead39d;
      color: #7b560d;
      background: var(--amber-soft);
    }

    .relay-title {
      display: flex;
      align-items: center;
      gap: 13px;
      margin-top: 18px;
    }

    .relay-light {
      width: 12px;
      height: 12px;
      border-radius: 999px;
      background: var(--neutral);
      flex: 0 0 auto;
      box-shadow: 0 0 0 5px rgba(95, 104, 116, 0.12);
    }

    .relay-light.on {
      background: var(--green);
      box-shadow: 0 0 0 5px rgba(22, 131, 77, 0.12);
    }

    h1 {
      margin: 0;
      font-size: clamp(26px, 3vw, 34px);
      line-height: 1.12;
      font-weight: 700;
      letter-spacing: 0;
    }

    .hero-desc {
      margin: 10px 25px 0 0;
      color: var(--muted);
      font-size: 16px;
      line-height: 1.55;
    }

    .next-box {
      align-self: start;
      display: inline-flex;
      flex-direction: column;
      gap: 4px;
      min-width: 0;
    }

    .eyebrow {
      margin: 0;
      color: var(--muted);
      font-size: 15px;
      font-weight: 600;
      letter-spacing: 0;
    }

    .next-time {
      direction: ltr;
      font-size: 18px;
      font-weight: 700;
      font-variant-numeric: tabular-nums;
      line-height: 1;
    }

    .next-meta {
      margin-top: 0;
      color: var(--muted);
      font-size: 15px;
    }

    .state-pill {
      display: inline-flex;
      width: max-content;
      margin-top: 2px;
      border-radius: 999px;
      padding: 5px 10px;
      font-size: 14px;
      font-weight: 600;
      color: var(--green);
      background: var(--green-soft);
    }

    .state-pill.off {
      color: var(--neutral);
      background: var(--neutral-soft);
    }

    .panel-head {
      padding: 14px 16px;
      border-bottom: 1px solid var(--border);
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
    }

    .panel-title {
      margin: 0;
      font-size: 18px;
      font-weight: 700;
    }

    .panel-body { padding: 16px; }

    .switch-status {
      margin: 0 0 12px;
      color: var(--muted);
      font-size: 15px;
      font-weight: 600;
    }

    .switch-status.warn {
      color: var(--red);
    }

    .controls-grid {
      display: grid;
      gap: 14px;
    }

    .control-title {
      color: var(--muted);
      font-size: 15px;
      font-weight: 600;
      margin: 0 0 12px;
    }

    .segmented {
      display: grid;
      gap: 5px;
      padding: 5px;
      background: var(--control);
      border: 1px solid var(--border);
      border-radius: 12px;
    }

    .segmented.relay { grid-template-columns: 1fr 1.15fr 1fr; }
    .segmented.mode { grid-template-columns: 1fr 1fr; }

    .segmented button {
      min-height: 40px;
      color: #505964;
      background: transparent;
      border-radius: 9px;
      font-size: 15px;
      font-weight: 600;
      transition: background .16s ease, color .16s ease, box-shadow .16s ease;
    }

    .segmented button:not(:disabled):hover {
      color: var(--text);
      background: rgba(255,255,255,0.52);
    }

    .segmented button.active {
      color: var(--text);
      background: #fffdf8;
      box-shadow: 0 1px 5px rgba(64, 48, 28, 0.12);
    }

    .segmented button.active.on {
      color: var(--green);
    }

    .segmented button.active.off {
      color: var(--neutral);
    }

    .segmented button.active.shabbat {
      color: #7b560d;
      background: #fff7e3;
    }

    .schedule-empty {
      padding: 16px;
      text-align: center;
      color: var(--muted);
      font-size: 15px;
    }

    .day-row {
      display: grid;
      grid-template-columns: 130px 1fr;
      gap: 16px;
      padding: 13px 16px;
      border-top: 1px solid var(--border);
    }

    .day-row:first-child { border-top: 0; }

    .day-name {
      color: var(--muted);
      font-size: 15px;
      font-weight: 600;
      padding-top: 7px;
    }

    .event-list {
      display: flex;
      flex-wrap: wrap;
      gap: 9px;
      min-width: 0;
    }

    .event-chip {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      border: 1px solid #cbe7d4;
      border-radius: 10px;
      background: var(--green-soft);
      color: var(--green);
      padding: 7px 11px;
      font-size: 14px;
      font-weight: 600;
      white-space: nowrap;
    }

    .event-chip.off {
      border-color: #d9dde2;
      background: var(--neutral-soft);
      color: var(--neutral);
    }

    .event-time {
      direction: ltr;
      font-variant-numeric: tabular-nums;
    }

    .btn-outline {
      color: var(--text);
      background: #fff;
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 9px 14px;
      font-size: 15px;
      font-weight: 600;
      box-shadow: 0 1px 4px rgba(64, 48, 28, 0.08);
    }

    .status-toggle {
      width: 100%;
      padding: 13px 14px;
      color: var(--muted);
      background: transparent;
      display: flex;
      align-items: center;
      justify-content: space-between;
      font-size: 16px;
      font-weight: 600;
    }

    .status-toggle:hover { color: var(--text); }

    .status-panel {
      display: none;
      border-top: 1px solid var(--border);
      padding: 14px;
    }

    .status-panel.open { display: block; }

    .status-grid {
      display: grid;
      grid-template-columns: repeat(4, minmax(0, 1fr));
      gap: 10px;
    }

    .status-item {
      background: var(--surface-soft);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 12px;
      min-width: 0;
    }

    .status-label {
      color: var(--muted);
      font-size: 14px;
      margin-bottom: 7px;
    }

    .status-value {
      display: flex;
      align-items: center;
      gap: 7px;
      font-size: 15px;
      font-weight: 600;
    }

    .modal {
      position: fixed;
      inset: 0;
      z-index: 30;
      display: none;
      align-items: center;
      justify-content: center;
      padding: 18px;
      background: rgba(34, 38, 45, 0.42);
    }

    .modal.open { display: flex; }

    .drawer {
      width: min(540px, 100%);
      max-height: min(720px, 92vh);
      display: flex;
      flex-direction: column;
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 18px;
      box-shadow: 0 28px 90px rgba(64, 48, 28, 0.24);
      overflow: hidden;
    }

    .modal-head {
      padding: 18px 20px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      border-bottom: 1px solid var(--border);
    }

    .modal-title {
      margin: 0;
      font-size: 20px;
      font-weight: 700;
    }

    .close-btn {
      width: 36px;
      height: 36px;
      border-radius: 10px;
      background: var(--surface-soft);
      color: var(--muted);
      font-size: 22px;
      line-height: 1;
    }

    .modal-content { overflow: auto; }

    .form {
      padding: 18px 20px;
      display: grid;
      gap: 14px;
      border-bottom: 1px solid var(--border);
    }

    .form-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
    }

    label {
      display: grid;
      gap: 7px;
      color: var(--muted);
      font-size: 15px;
      font-weight: 600;
    }

    select {
      width: 100%;
      min-height: 43px;
      color: var(--text);
      background: #fff;
      border: 1px solid var(--border);
      border-radius: 10px;
      padding: 8px 10px;
      outline: 0;
    }

    select:focus {
      border-color: #c8a557;
      box-shadow: 0 0 0 3px rgba(185, 133, 24, 0.14);
    }

    .add-btn {
      min-height: 44px;
      border-radius: 11px;
      color: #fff;
      background: var(--green);
      font-weight: 700;
    }

    .editor-list { padding: 18px 20px 22px; }

    .editor-day { margin-top: 17px; }

    .editor-day:first-child { margin-top: 0; }

    .editor-day-title {
      margin: 0 0 8px;
      color: var(--muted);
      font-size: 14px;
      font-weight: 600;
    }

    .editor-event {
      display: grid;
      grid-template-columns: 1fr auto;
      align-items: center;
      gap: 10px;
      padding: 8px 0;
      border-top: 1px solid var(--border);
    }

    .delete-btn {
      color: var(--red);
      background: var(--red-soft);
      border: 1px solid #f2cbc8;
      border-radius: 10px;
      padding: 7px 11px;
      font-size: 14px;
      font-weight: 600;
    }

    .toast {
      position: fixed;
      left: 18px;
      bottom: 18px;
      z-index: 40;
      display: none;
      max-width: min(420px, calc(100vw - 36px));
      color: var(--text);
      background: #fff;
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 12px 14px;
      box-shadow: var(--shadow);
      font-size: 15px;
      font-weight: 600;
    }

    .toast.show { display: block; }

    @media (max-width: 820px) {
      .main-col, .hero, .status-grid {
        grid-template-columns: 1fr;
      }

      .main-col {
        grid-template-areas:
          "state"
          "schedule"
          "details";
      }
    }

    @media (max-width: 620px) {
      .topbar-inner {
        padding: 24px 14px 20px;
      }

      .header-status {
        width: 100%;
        justify-content: center;
        gap: 10px;
      }

      .page {
        padding: 16px 12px 26px;
      }

      .hero, .panel-body {
        padding: 16px;
      }

      .next-time {
        font-size: 30px;
      }

      .day-row {
        grid-template-columns: 1fr;
        gap: 10px;
        padding: 14px 18px;
      }

      .day-name {
        padding-top: 0;
      }

      .form-grid {
        grid-template-columns: 1fr;
      }

      .modal {
        align-items: flex-end;
        padding: 0;
      }

      .drawer {
        width: 100%;
        max-height: 92vh;
        border-radius: 18px 18px 0 0;
      }
    }
  </style>
</head>
<body>
  <header class="topbar">
    <div class="topbar-inner">
      <div class="brand">
        <div class="brand-name">שעון שבת חכם</div>
      </div>
      <div class="header-status">
        <div id="clockDisplay" class="clock">--:--</div>
        <div class="dots">
          <span class="dot-line" style="display:none"><span id="headerDot" class="dot warn"></span><span id="headerHealth"></span></span>
        </div>
      </div>
    </div>
  </header>

  <section id="offlineBanner" class="banner">
    <div class="banner-inner">
      <div class="banner-title"><span class="warn-symbol">!</span><span>אין תקשורת עם המכשיר</span></div>
      <button class="retry" onclick="updateStatus()">נסה שוב</button>
    </div>
  </section>

  <section id="timeBanner" class="banner time">
    <div class="banner-inner">
      <div class="banner-title"><span class="warn-symbol">!</span><span>שעון המכשיר אינו תקין. לוח הזמנים מושבת עד לסנכרון זמן.</span></div>
    </div>
  </section>

  <main class="page">
    <div class="dashboard">
      <section class="main-col">
        <div class="state-col">
          <div id="hero" class="card hero">
            <div>
              <div id="modeBadge" class="badge">מצב הממסר</div>
              <div class="relay-title">
                <span id="relayLight" class="relay-light"></span>
                <h1 id="systemTitle">הממסר --</h1>
              </div>
              <p id="relayDescription" class="hero-desc">טוען מצב ממסר...</p>
            </div>

            <aside class="next-box">
              <p class="eyebrow">אירוע הבא</p>
              <div id="nextSummary" class="next-meta">לא מוגדר</div>
            </aside>
          </div>

          <section class="card">
            <div class="panel-body">
              <p class="control-title">שליטה בממסר</p>
              <div class="segmented relay">
                <button id="relayOnBtn" class="on" onclick="setRelayMode('relay_on')">דלוק</button>
                <button id="relayAutoBtn" onclick="setRelayMode('relay_auto')">אוטומטי</button>
                <button id="relayOffBtn" class="off" onclick="setRelayMode('relay_off')">כבוי</button>
              </div>
            </div>
          </section>

          <section class="card">
            <div class="panel-body">
              <p class="control-title">מתג שבת חכם</p>
              <p id="switchStatus" class="switch-status">טוען מצב מתג...</p>
              <div class="segmented mode">
                <button id="weekBtn" onclick="setAppMode('week')">חול</button>
                <button id="shabbatBtn" class="shabbat" onclick="setAppMode('shabbat')">שבת</button>
              </div>
            </div>
          </section>
        </div>

        <section class="card schedule-card">
          <div class="panel-head">
            <h2 class="panel-title">לוח זמנים</h2>
            <button class="btn-outline" onclick="openEditor()">עריכה</button>
          </div>
          <div id="scheduleView"></div>
        </section>

        <section class="card details-card">
          <button class="status-toggle" onclick="toggleSystemStatus()">
            <span>פרטי מערכת</span>
            <span id="systemChevron">פתח</span>
          </button>
          <div id="systemPanel" class="status-panel">
            <div class="status-grid">
              <div class="status-item">
                <div class="status-label">שעון</div>
                <div class="status-value"><span id="sysTimeDot" class="dot warn"></span><span id="sysTimeValue">טוען</span></div>
              </div>
              <div class="status-item">
                <div class="status-label">קשר HC-12</div>
                <div class="status-value"><span id="sysRadioDot" class="dot warn"></span><span id="sysRadioValue">טוען</span></div>
              </div>
              <div class="status-item">
                <div class="status-label">ממסר</div>
                <div id="sysRelayValue" class="status-value">--</div>
              </div>
              <div class="status-item">
                <div class="status-label">אירועים בלוח</div>
                <div id="sysScheduleCount" class="status-value">0</div>
              </div>
            </div>
          </div>
        </section>
      </section>
    </div>
  </main>

  <div id="editorModal" class="modal" onclick="closeEditorFromBackdrop(event)">
    <section class="drawer" aria-modal="true" role="dialog">
      <div class="modal-head">
        <h2 class="modal-title">עריכת לוח זמנים</h2>
        <button class="close-btn" onclick="closeEditor()" aria-label="סגור">×</button>
      </div>
      <div class="modal-content">
        <form class="form" onsubmit="addSchedule(event)">
          <p class="control-title">הוספת אירוע</p>
          <div class="form-grid">
            <label>יום
              <select id="scheduleDay"></select>
            </label>
            <label>פעולה
              <select id="scheduleState">
                <option value="on">הדלקה</option>
                <option value="off">כיבוי</option>
              </select>
            </label>
            <label>שעה
              <select id="hh"></select>
            </label>
            <label>דקות
              <select id="mm"></select>
            </label>
          </div>
          <button id="btnAddEvent" class="add-btn" type="submit">הוסף אירוע</button>
        </form>
        <div class="editor-list">
          <p class="control-title">אירועים קיימים</p>
          <div id="editorSchedule"></div>
        </div>
      </div>
    </section>
  </div>

  <div id="toast" class="toast"></div>

  <script>
    const DAYS = ['ראשון', 'שני', 'שלישי', 'רביעי', 'חמישי', 'שישי', 'שבת'];
    let currentStatus = null;
    let currentSchedule = [];
    let lastScheduleRevision = null;
    let scheduleRevisionSupported = false;
    let offline = false;
    let toastTimer = null;

    function $(id) {
      return document.getElementById(id);
    }

    function pad(n) {
      return String(n).padStart(2, '0');
    }

    function formatTime(hour, minute) {
      return pad(hour) + ':' + pad(minute);
    }

    function showToast(message) {
      const el = $('toast');
      el.textContent = message;
      el.classList.add('show');
      clearTimeout(toastTimer);
      toastTimer = setTimeout(() => el.classList.remove('show'), 3200);
    }

    function relayModeName(mode) {
      if (mode === 1 || mode === 'on' || mode === 'relay_on') return 'דלוק ידנית';
      if (mode === 0 || mode === 'off' || mode === 'relay_off') return 'כבוי ידנית';
      return 'אוטומטי לפי לוח הזמנים';
    }

    function relayModeKey(mode) {
      if (mode === 1 || mode === 'on' || mode === 'relay_on') return 'on';
      if (mode === 0 || mode === 'off' || mode === 'relay_off') return 'off';
      return 'auto';
    }

    function setDot(dot, ok, warn) {
      dot.classList.toggle('bad', !ok && !warn);
      dot.classList.toggle('warn', !!warn);
    }

    function fillSelects() {
      const day = $('scheduleDay');
      day.innerHTML = '';
      DAYS.forEach((name, index) => {
        const opt = document.createElement('option');
        opt.value = index;
        opt.textContent = 'יום ' + name;
        day.appendChild(opt);
      });

      const hh = $('hh');
      const mm = $('mm');
      hh.innerHTML = '';
      mm.innerHTML = '';
      for (let h = 0; h < 24; h++) {
        const opt = document.createElement('option');
        opt.value = h;
        opt.textContent = pad(h);
        hh.appendChild(opt);
      }
      for (let m = 0; m < 60; m++) {
        const opt = document.createElement('option');
        opt.value = m;
        opt.textContent = pad(m);
        mm.appendChild(opt);
      }
    }

    async function fetchJson(url) {
      const res = await fetch(url, { cache: 'no-store' });
      if (!res.ok) {
        const text = await res.text();
        throw new Error(text || 'Request failed');
      }
      return res.json();
    }

    async function sendCmd(cmd) {
      try {
        let res = await fetch('/cmd?cmd=' + encodeURIComponent(cmd), { cache: 'no-store' });

        // Older firmware builds accept the same command using the "c" parameter.
        if (res.status === 400) {
          const text = await res.clone().text();
          if (text.indexOf('Missing cmd') >= 0) {
            res = await fetch('/cmd?c=' + encodeURIComponent(cmd), { cache: 'no-store' });
          }
        }

        if (!res.ok) {
          const text = await res.text();
          throw new Error(text || 'Command failed');
        }
        await updateStatus();
        return true;
      } catch (err) {
        showToast('הפעולה נכשלה. כדאי לנסות שוב בעוד רגע.');
        return false;
      }
    }

    async function setRelayMode(cmd) {
      disableControls(true);
      await sendCmd(cmd);
      disableControls(false);
    }

    async function setAppMode(mode) {
      disableControls(true);
      await sendCmd(mode);
      disableControls(false);
    }

    function disableControls(disabled) {
      ['relayOnBtn', 'relayAutoBtn', 'relayOffBtn', 'weekBtn', 'shabbatBtn'].forEach(id => {
        const el = $(id);
        if (el) el.disabled = disabled;
      });
      applyTimeValidity();
    }

    function sortedSchedule() {
      return currentSchedule.slice().sort((a, b) => {
        const aw = Number(a.day) * 1440 + Number(a.hour) * 60 + Number(a.minute);
        const bw = Number(b.day) * 1440 + Number(b.hour) * 60 + Number(b.minute);
        return aw - bw;
      });
    }

    function groupByDay() {
      const groups = {};
      sortedSchedule().forEach(event => {
        const day = Number(event.day);
        if (!groups[day]) groups[day] = [];
        groups[day].push(event);
      });
      return groups;
    }

    function parseStatusMinutes(timeText) {
      if (!timeText || timeText.indexOf(':') < 0) return null;
      const parts = timeText.split(':');
      const hour = Number(parts[0]);
      const minute = Number(parts[1]);
      if (isNaN(hour) || isNaN(minute)) return null;
      return hour * 60 + minute;
    }

    function findNextEvent() {
      if (!currentStatus || !currentStatus.timeValid || currentSchedule.length === 0) return null;

      const nowMinutes = parseStatusMinutes(currentStatus.time);
      if (nowMinutes === null) return null;

      const today = new Date().getDay();
      const sorted = sortedSchedule();
      const next = sorted.find(event => {
        const day = Number(event.day);
        const minuteOfDay = Number(event.hour) * 60 + Number(event.minute);
        return day > today || (day === today && minuteOfDay > nowMinutes);
      });
      return next || sorted[0] || null;
    }

    function renderNextEvent() {
      const next = findNextEvent();
      if (!next) {
        $('nextSummary').textContent = 'לא מוגדר';
        return;
      }

      const action = next.state === 'on' ? 'הדלקה' : 'כיבוי';
      $('nextSummary').textContent = formatTime(next.hour, next.minute) + ', יום ' + DAYS[Number(next.day)] + ' - ' + action;
    }

    function eventChip(event, withAction) {
      const chip = document.createElement(withAction ? 'div' : 'span');
      chip.className = 'event-chip' + (event.state === 'off' ? ' off' : '');

      const text = document.createElement('span');
      text.innerHTML = '<span class="event-time">' + formatTime(event.hour, event.minute) + '</span> ' + (event.state === 'on' ? 'הדלקה' : 'כיבוי');
      chip.appendChild(text);

      if (withAction) {
        const btn = document.createElement('button');
        btn.className = 'delete-btn';
        btn.type = 'button';
        btn.textContent = 'מחק';
        btn.onclick = () => deleteSchedule(event.day, event.hour, event.minute);
        chip.appendChild(btn);
      }
      return chip;
    }

    function renderSchedule() {
      const view = $('scheduleView');
      const editor = $('editorSchedule');
      const groups = groupByDay();
      const activeDays = Object.keys(groups).map(Number).sort((a, b) => a - b);

      view.innerHTML = '';
      editor.innerHTML = '';
      $('sysScheduleCount').textContent = currentSchedule.length;

      if (activeDays.length === 0) {
        view.innerHTML = '<div class="schedule-empty">אין אירועים מוגדרים</div>';
        editor.innerHTML = '<div class="schedule-empty">אין אירועים מוגדרים</div>';
        renderNextEvent();
        return;
      }

      activeDays.forEach(day => {
        const row = document.createElement('div');
        row.className = 'day-row';
        const name = document.createElement('div');
        name.className = 'day-name';
        name.textContent = 'יום ' + DAYS[day];
        const list = document.createElement('div');
        list.className = 'event-list';
        groups[day].forEach(event => list.appendChild(eventChip(event, false)));
        row.appendChild(name);
        row.appendChild(list);
        view.appendChild(row);

        const editorDay = document.createElement('div');
        editorDay.className = 'editor-day';
        const title = document.createElement('p');
        title.className = 'editor-day-title';
        title.textContent = 'יום ' + DAYS[day];
        editorDay.appendChild(title);
        groups[day].forEach(event => {
          const line = document.createElement('div');
          line.className = 'editor-event';
          line.appendChild(eventChip(event, false));
          const btn = document.createElement('button');
          btn.className = 'delete-btn';
          btn.type = 'button';
          btn.textContent = 'מחק';
          btn.onclick = () => deleteSchedule(event.day, event.hour, event.minute);
          line.appendChild(btn);
          editorDay.appendChild(line);
        });
        editor.appendChild(editorDay);
      });

      renderNextEvent();
    }

    async function loadSchedule(forceRevision) {
      try {
        const data = await fetchJson('/schedule_list');
        currentSchedule = Array.isArray(data) ? data : [];
        if (forceRevision !== undefined) lastScheduleRevision = forceRevision;
        renderSchedule();
      } catch (err) {
        showToast('טעינת לוח הזמנים נכשלה: ' + err.message);
      }
    }

    async function addSchedule(event) {
      event.preventDefault();
      const day = Number($('scheduleDay').value);
      const hour = Number($('hh').value);
      const minute = Number($('mm').value);
      const state = $('scheduleState').value;
      const url = '/schedule?day=' + day + '&hour=' + hour + '&minute=' + minute + '&state=' + encodeURIComponent(state);

      try {
        const res = await fetch(url, { cache: 'no-store' });
        if (!res.ok) {
          const text = await res.text();
          throw new Error(text || 'Failed to save event');
        }
        await loadSchedule();
        await updateStatus();
      } catch (err) {
        showToast('שמירת האירוע נכשלה: ' + err.message);
      }
    }

    async function deleteSchedule(day, hour, minute) {
      if (!confirm('למחוק את האירוע ביום ' + DAYS[Number(day)] + ' בשעה ' + formatTime(hour, minute) + '?')) return;
      try {
        const url = '/schedule_delete?day=' + day + '&hour=' + hour + '&minute=' + minute;
        const res = await fetch(url, { cache: 'no-store' });
        if (!res.ok) {
          const text = await res.text();
          throw new Error(text || 'Failed to delete event');
        }
        await loadSchedule();
        await updateStatus();
      } catch (err) {
        showToast('מחיקת האירוע נכשלה: ' + err.message);
      }
    }

    function applyTimeValidity() {
      const invalid = currentStatus && !currentStatus.timeValid;
      ['scheduleDay', 'scheduleState', 'hh', 'mm', 'btnAddEvent'].forEach(id => {
        const el = $(id);
        if (el) el.disabled = !!invalid;
      });
    }

    function renderStatus() {
      const data = currentStatus || {};
      const timeOk = !offline && !!data.timeValid;
      const radioOk = !offline && !!data.hc12Ok;
      const relayOn = !!data.relay;
      const shabbat = !!data.shabbat;
      const modeKey = relayModeKey(data.relayMode);

      $('clockDisplay').textContent = data.time || '--:--';
      $('offlineBanner').classList.toggle('show', offline);
      $('timeBanner').classList.toggle('show', !offline && !timeOk);

      let headerIssue = '';
      if (offline) headerIssue = 'אין תקשורת למתג';
      else if (!timeOk) headerIssue = 'הזמן לא מסונכרן';
      else if (!radioOk) headerIssue = 'מתג שבת לא מחובר';
      $('headerHealth').textContent = headerIssue;
      $('headerHealth').parentElement.style.display = headerIssue ? 'inline-flex' : 'none';
      setDot($('headerDot'), false, !offline && !!headerIssue);
      setDot($('sysTimeDot'), timeOk, !offline && !data.timeValid);
      setDot($('sysRadioDot'), radioOk, false);
      $('sysTimeValue').textContent = timeOk ? 'תקין' : 'שגיאה';
      $('sysRadioValue').textContent = radioOk ? 'תקין' : 'לא זמין';

      $('hero').classList.toggle('shabbat', false);
      $('modeBadge').className = 'badge';
      $('modeBadge').textContent = 'מצב הממסר';
      $('relayLight').classList.toggle('on', relayOn);
      $('systemTitle').textContent = relayOn ? 'הממסר דולק' : 'הממסר כבוי';
      $('relayDescription').textContent = relayModeName(data.relayMode);
      $('switchStatus').textContent = radioOk ? (shabbat ? 'מצב שבת' : 'מצב חול') : 'מתג שבת לא מחובר';
      $('switchStatus').classList.toggle('warn', !radioOk);
      $('sysRelayValue').textContent = relayModeName(data.relayMode);

      $('relayOnBtn').classList.toggle('active', modeKey === 'on');
      $('relayAutoBtn').classList.toggle('active', modeKey === 'auto');
      $('relayOffBtn').classList.toggle('active', modeKey === 'off');
      $('weekBtn').classList.toggle('active', !shabbat);
      $('shabbatBtn').classList.toggle('active', shabbat);

      applyTimeValidity();
      renderNextEvent();
    }

    async function updateStatus() {
      try {
        const data = await fetchJson('/status');
        offline = false;
        currentStatus = data;

        if (Object.prototype.hasOwnProperty.call(data, 'scheduleRevision')) {
          scheduleRevisionSupported = true;
          const nextRevision = Number(data.scheduleRevision);
          if (lastScheduleRevision === null) {
            lastScheduleRevision = nextRevision;
          } else if (nextRevision !== lastScheduleRevision) {
            await loadSchedule(nextRevision);
          }
        } else if (!scheduleRevisionSupported) {
          scheduleRevisionSupported = false;
        }

        renderStatus();
      } catch (err) {
        offline = true;
        currentStatus = currentStatus || {};
        renderStatus();
      }
    }

    function toggleSystemStatus() {
      const panel = $('systemPanel');
      const open = !panel.classList.contains('open');
      panel.classList.toggle('open', open);
      $('systemChevron').textContent = open ? 'סגור' : 'פתח';
    }

    function openEditor() {
      $('editorModal').classList.add('open');
    }

    function closeEditor() {
      $('editorModal').classList.remove('open');
    }

    function closeEditorFromBackdrop(event) {
      if (event.target === event.currentTarget) closeEditor();
    }

    window.addEventListener('load', async () => {
      fillSelects();
      await loadSchedule();
      await updateStatus();
      setInterval(updateStatus, 10000);
    });
  </script>
</body>
</html>
)rawliteral";
