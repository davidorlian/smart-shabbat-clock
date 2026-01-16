// ---------------------- index_page.h ----------------------
#include <pgmspace.h>
// CHANGE HERE: edit the HTML/CSS/JS below to customize the web UI.
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset='UTF-8'>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>שעון שבת חכם</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            margin: 0;
            padding: 40px;
            min-height: 100vh;
            box-sizing: border-box;
            direction: rtl; 
            text-align: right;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            background: rgba(255, 255, 255, 0.95);
            border-radius: 20px;
            padding: 40px;
            box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1);
        }
        h2, .time-display {
            color: #333;
            text-align: center;
        }
        .time-display {
            font-size: 32px;
            margin-bottom: 20px;
        }
        .section-group {
            margin-top: 30px;
            padding: 20px;
            background: #f9f9f9;
            border-radius: 10px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.05);
        }
        .section-header {
            text-align: center;
            color: #444;
            margin-top: 0;
            margin-bottom: 20px;
            border-bottom: 2px solid #764ba2;
            padding-bottom: 10px;
        }
        .control-group, .schedule-group {
            margin: 30px 0;
            text-align: center;
        }
        .control-label {
            font-size: 18px;
            font-weight: bold;
            color: #333;
            margin-bottom: 15px;
            display: block;
        }
        select, input[type="time"], input[type="number"] {
            font-size: 16px;
            padding: 6px;
            margin: 10px;
            border: 1px solid #ccc;
            border-radius: 5px;
            text-align: right;
        }
        .toggle-switch {
            position: relative;
            display: inline-block;
            width: 80px;
            height: 40px;
        }
        .toggle-row {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 12px;            
            margin-top: 6px;
            direction: rtl;
        }
        .toggle-caption {
            font-size: 14px;
            font-weight: bold;
            color: #444;
            line-height: 1;
            direction: rtl;
        }    
        .toggle-switch input { opacity: 0; width: 0; height: 0; }
        .slider {
            position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
            background-color: #ccc; transition: .3s; border-radius: 40px;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2);
        }
        .slider:before {
            position: absolute; content: "";
            height: 32px; width: 32px; left: 4px; bottom: 4px;
            background-color: white; transition: .3s; border-radius: 50%;
            box-shadow: 0 2px 10px rgba(0, 0, 0, 0.3);
            transform: translateX(40px);
        }
        input:checked + .slider { background-color: #4CAF50; }
        input:checked + .slider:before { transform: translateX(0); }
        
        /* כפתורי On/Auto/Off */
        .three-way-switch {
            text-align: center;
        }
        .three-way-switch button {
            padding: 10px 20px; margin: 0 5px; border: none; border-radius: 5px;
            font-size: 14px; cursor: pointer; background-color: #ccc; color: #000;
        }
        .three-way-switch button.active { background-color: #4CAF50; color: #fff; }
        .relay-indicator {
            display: inline-block; width: 16px; height: 16px; border-radius: 50%;
            margin-right: 10px; 
            vertical-align: middle; background-color: #bbb;
        }
        .relay-indicator.on { background-color: #00cc44; }
        
        table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        th, td { border: 1px solid #ccc; padding: 8px; text-align: center; }
        th { background-color: #f2f2f2; }
        .hint { font-size: 12px; color: #555; margin-top: 6px; text-align: center; }
    </style>
</head>
<body>
    <div class="container">
        <div class="time-display" id="clockDisplay">--:--</div>
        <h2>✨ מערכת שליטה חכמה</h2>
        <div id="statusBar" class="hint">טוען סטטוס...</div>

        <div id="manualTime" class="control-group" style="display:none">
          <label class="control-label">קביעת זמן ידנית</label>
          <div style="display:flex;gap:8px;justify-content:center;flex-wrap:wrap; direction: rtl;">
            <input id="mt_S" type="number" min="0" max="59"   placeholder="SS">
            <input id="mt_M" type="number" min="0" max="59"   placeholder="MM">
            <input id="mt_H" type="number" min="0" max="23"   placeholder="HH">
            <input id="mt_d" type="number" min="1" max="31"   placeholder="DD">
            <input id="mt_m" type="number" min="1" max="12"   placeholder="MM">
            <input id="mt_y" type="number" min="2020" max="2099" placeholder="YYYY">
            <button id="btnSetTime" onclick="setManualTime()">הגדר</button>
          </div>
          <div class="hint">מופיע רק כאשר הזמן של המכשיר אינו תקין.</div>
        </div>

        <div class="section-group">
            <h3 class="section-header">🕰️ שעון שבת חכם </h3>

            <div class="control-group">
                <label class="control-label">מצב הממסר</label>
                <div class="three-way-switch">
                    <button onclick="setRelayMode('relay_on')" id="btn_on">On</button>
                    <button onclick="setRelayMode('relay_auto')" id="btn_auto">Auto</button>
                    <button onclick="setRelayMode('relay_off')" id="btn_off">Off</button>
                    <span class="relay-indicator" id="relayLed"></span>
                </div>
            </div>

            <div class="schedule-group">
                <label class="control-label">הוסף אירוע ללו"ז</label>

                <div style="display:flex; gap:10px; justify-content:center; align-items:center; flex-wrap:wrap; direction: rtl;">
                    
                    <div style="display:flex; flex-direction:column; align-items:center;">
                      <span class="schedule-label">דקות</span>
                      <select id="mm"></select>
                    </div>

                    <div style="display:flex; flex-direction:column; align-items:center;">
                        <span class="schedule-label">שעה</span>
                        <select id="hh"></select>
                    </div>

                    <div style="display:flex; flex-direction:column; align-items:center;">
                        <span class="schedule-label">יום</span>
                        <select id="scheduleDay" title="בחר יום(ים)">
                          <option value="0">א'</option>
                          <option value="1">ב'</option>
                          <option value="2">ג'</option>
                          <option value="3">ד'</option>
                          <option value="4">ה'</option>
                          <option value="5">ו'</option>
                          <option value="6">ש'</option>
                        </select>
                    </div>

                    <div style="display:flex; flex-direction:column; align-items:center;">
                        <span class="schedule-label">מצב</span>
                        <select id="scheduleState">
                          <option value="on">הדלקה</option>
                          <option value="off">כיבוי</option>
                        </select>
                    </div>
                    
                    <button id="btnAddEvent" onclick="addSchedule()">הוסף</button>
                </div>

                <h3>לו"ז נוכחי</h3>
                <table id="scheduleTable">
                    <thead>
                        <tr><th>זמן</th><th>יום</th><th>מצב</th><th>פעולות</th></tr>
                    </thead>
                    <tbody></tbody>
                </table>
            </div>
        </div>

        <div class="section-group">
            <h3 class="section-header">🔒 מתג שבת חכם </h3>
            <div class="control-group">
                <label class="control-label">מתג הפיזי</label>
                <div class="toggle-row">
                    <span class="toggle-caption">פעיל (שבוע)</span>
                    <label class="toggle-switch">
                        <input type="checkbox" id="shabbatMode" onchange="toggleMode(this)">
                        <span class="slider"></span>
                    </label>
                    <span class="toggle-caption">נעול (שבת)</span>
                </div>
                <div class="hint">
                  מבטל השפעה של לחיצה על המתג הפיזי ומקפיא את המצב הנוכחי של המכשיר.
                </div>
            </div>
        </div>

        <div class="status" id="status">טוען סטטוס...</div>
    </div>

    <script>
        let currentRelayMode = 'relay_auto';
        let currentSchedule = [];
        
        async function setManualTime() {
          const y = parseInt(document.getElementById('mt_y').value, 10);
          const m = parseInt(document.getElementById('mt_m').value, 10);
          const d = parseInt(document.getElementById('mt_d').value, 10);
          const H = parseInt(document.getElementById('mt_H').value, 10);
          const M = parseInt(document.getElementById('mt_M').value, 10);
          const S = parseInt(document.getElementById('mt_S').value, 10);
        
          if (!(y >= 2020 && m >= 1 && m <= 12 && d >= 1 && d <= 31 && H >= 0 && H <= 23 && M >= 0 && M <= 59 && S >= 0 && S <= 59)) {
            alert('Fill all fields with valid values');
            return;
          }
          const qs = new URLSearchParams({ y, m, d, H, M, S }).toString();
          try {
            const res = await fetch(`/set_time?${qs}`);
            const text = await res.text();
            if (!res.ok) { alert(text || 'Set time failed'); return; }
            updateStatus();
          } catch (e) { alert('Network error: ' + e.message); }
        }
        
        async function sendCmd(cmd) {
          try {
            const res = await fetch(`/cmd?c=${encodeURIComponent(cmd)}`);
            const text = await res.text();
            if (!res.ok) { alert(text || 'Command failed'); return false; }
            updateStatus();
            return true;
          } catch (e) { alert('Network error: ' + e.message); return false; }
        }
        
        async function setRelayMode(mode) {
          const ok = await sendCmd(mode);
          if (ok) { currentRelayMode = mode; highlightRelayButtons(); }
        }
        
        function highlightRelayButtons() {
          document.querySelectorAll('.three-way-switch button').forEach(btn => btn.classList.remove('active'));
          if (currentRelayMode === 'relay_on') document.getElementById('btn_on').classList.add('active');
          else if (currentRelayMode === 'relay_off') document.getElementById('btn_off').classList.add('active');
          else document.getElementById('btn_auto').classList.add('active');
        }
        
        async function toggleMode(toggle) {
          const cmd = toggle.checked ? 'shabbat' : 'week';
          const ok = await sendCmd(cmd);
          if (!ok) { toggle.checked = !toggle.checked; }
        }
        
        function fillTimeSelects() {
          const hh = document.getElementById('hh');
          const mm = document.getElementById('mm');
          if (!hh || !mm) return;
          hh.innerHTML = ''; mm.innerHTML = '';
          for (let h = 0; h < 24; h++) {
            const o = document.createElement('option');
            o.value = h; o.textContent = String(h).padStart(2, '0');
            hh.appendChild(o);
          }
          for (let m = 0; m < 60; m++) {
            const o = document.createElement('option');
            o.value = m; o.textContent = String(m).padStart(2, '0');
            mm.appendChild(o);
          }
        }
        
        async function addSchedule() {
          const hhEl = document.getElementById('hh');
          const mmEl = document.getElementById('mm');
          const state = document.getElementById('scheduleState').value;
          const day   = parseInt(document.getElementById('scheduleDay').value, 10);
        
          if (!hhEl || !mmEl || isNaN(day)) { alert('Please select a time and day.'); return; }
        
          const hour   = parseInt(hhEl.value, 10);
          const minute = parseInt(mmEl.value, 10);
          const existing = currentSchedule.find(e => e.day === day && e.hour === hour && e.minute === minute);
        
          if (existing) {
            if (existing.state === state) { alert("The new event is identical to the existing one. No changes made."); return; } 
            else {
              const confirmOverwrite = confirm(`למחוק ${String(hour).padStart(2,'0')}:${String(minute).padStart(2,'0')} ולהחליף במצב חדש?`);
              if (!confirmOverwrite) return;
            }
          }
        
          try {
            const res = await fetch(`/schedule?hour=${hour}&minute=${minute}&state=${state}&day=${day}`);
            const text = await res.text();
            if (!res.ok) { alert(text || 'Failed to add/update event'); return; }
            loadSchedule();
          } catch (e) { alert('Network error: ' + e.message); }
        }
        
        function formatDaysText(entry) {
          const dnames = ['א','ב','ג','ד','ה','ו','ש'];
          if (entry.day >= 0 && entry.day <= 6) return dnames[entry.day] + "'";
          return '';
        }
        
        function loadSchedule() {
          fetch('/schedule_list')
            .then(r => r.json())
            .then(data => {
              currentSchedule = data;
              const tbody = document.getElementById('scheduleTable').getElementsByTagName('tbody')[0];
              tbody.innerHTML = '';
              data.forEach(entry => {
                const row = tbody.insertRow();
                row.insertCell(0).textContent = `${String(entry.hour).padStart(2,'0')}:${String(entry.minute).padStart(2,'0')}`;
                row.insertCell(1).textContent = entry.state === 'on' ? 'הדלקה' : 'כיבוי';
                row.insertCell(2).textContent = formatDaysText(entry);
                const actions = row.insertCell(3);
                const btn = document.createElement('button');
                btn.textContent = 'מחק';
                btn.onclick = () => deleteSchedule(entry.day, entry.hour, entry.minute);
                actions.appendChild(btn);
              });
            });
        }
        
        function deleteSchedule(day, hour, minute) {
          const dayText = formatDaysText({ day: Number(day) });
          const hh = String(hour).padStart(2,'0');
          const mm = String(minute).padStart(2,'0');
        
          if (!confirm(`למחוק ${hh}:${mm} ביום ${dayText}?`)) return;
        
          fetch(`/schedule_delete?day=${day}&hour=${hour}&minute=${minute}`)
            .then(async r => {
              const t = await r.text();
              if (!r.ok) { alert(t); return; }
              loadSchedule();
            })
            .catch(err => alert('Network error: ' + err));
        }
        
        function updateStatus() {
          fetch('/status', { cache: 'no-store' })
            .then(r => r.json())
            .then(data => {
              document.getElementById('clockDisplay').textContent = data.time || '--:--';
              const toggle = document.getElementById('shabbatMode');
              if (toggle) toggle.checked = !!data.shabbat;
              updateRelayLed(!!data.relay);
              if (data.relayMode === 1)      currentRelayMode = 'relay_on';
              else if (data.relayMode === 0) currentRelayMode = 'relay_off';
              else                           currentRelayMode = 'relay_auto';
              highlightRelayButtons();
        
              const sb = document.getElementById('statusBar');
              if (sb) {
                const parts = [];
                parts.push(`זמן: ${data.timeValid ? 'תקין' : 'לא תקין'}`);
                parts.push(`HC-12: ${data.hc12Ok ? 'תקין' : 'לא ידוע'}`);
                sb.textContent = parts.join(' | ');
                sb.style.color = data.timeValid ? '' : '#b00020';
              }
              const mt = document.getElementById('manualTime');
              if (mt) mt.style.display = data.timeValid ? 'none' : '';
              ['hh','mm','scheduleDay','scheduleState','btnAddEvent'].forEach(id => {
                const el = document.getElementById(id);
                if (el) el.disabled = !data.timeValid;
              });
            })
            .catch(_ => {
              const sb = document.getElementById('statusBar');
              if (sb) { sb.textContent = 'שגיאת סטטוס'; sb.style.color = '#b00020'; }
            });
        }
        
        function updateRelayLed(isOn) {
          const led = document.getElementById('relayLed');
          if (isOn) led.classList.add('on');
          else led.classList.remove('on');
        }
        
        window.onload = function() {
          fillTimeSelects();
          updateStatus(); 
          loadSchedule(); 
          setInterval(updateStatus, 10000);
        }
    </script>
</body>
</html>
)rawliteral";
