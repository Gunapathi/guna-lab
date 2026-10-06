#pragma once
// Control page served at http://<device-ip>/ (or http://digieyes.local/)

const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Digi Eyes</title>
<style>
  :root { --bg:#111; --card:#1b1d22; --line:#2c3038; --txt:#eee; --mut:#9aa0a6; --acc:#2b8aef; }
  * { box-sizing:border-box; }
  body { font-family:sans-serif; background:var(--bg); color:var(--txt); margin:0 auto; padding:16px; max-width:520px; }
  h1 { font-size:22px; margin:4px 0 14px; }
  h2 { font-size:12px; text-transform:uppercase; letter-spacing:.08em; color:var(--mut); margin:0 0 10px; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:14px; margin-bottom:14px; }
  .grid { display:grid; grid-template-columns:repeat(3,1fr); gap:8px; }
  button { font-size:15px; padding:13px 4px; border:none; border-radius:10px; background:#2c3038; color:var(--txt); }
  button:active { background:#1c5fa8; }
  button.pri { background:var(--acc); color:#fff; width:100%; }
  .stat { display:grid; grid-template-columns:1fr 1fr; gap:10px 14px; font-size:15px; }
  .stat b { display:block; font-size:12px; color:var(--mut); font-weight:normal; }
  input[type=text], input[type=password] { width:100%; font-size:15px; padding:10px; border-radius:8px;
    border:1px solid var(--line); background:#0c0d10; color:var(--txt); margin-bottom:8px; }
  .sw { display:flex; align-items:center; gap:10px; margin-top:12px; font-size:15px; }
  .sw input { width:20px; height:20px; }
  .n { border-top:1px solid var(--line); padding:8px 0; font-size:14px; }
  .n:first-child { border-top:none; padding-top:0; }
  .n small, .mut { color:var(--mut); font-size:13px; }
  summary { color:var(--mut); margin-top:10px; }
  code { background:#0c0d10; padding:2px 5px; border-radius:5px; font-size:12px; word-break:break-all; }
</style>
</head>
<body>
  <h1>&#128064; Digi Eyes</h1>

  <div class="card"><h2>Status</h2>
    <div class="stat">
      <div><b>Time</b><span id="time">&ndash;</span></div>
      <div><b>Phone link</b><span id="ble">&ndash;</span></div>
      <div><b>Phone battery</b><span id="pbat">&ndash;</span></div>
      <div><b>Device battery</b><span id="dbat">&ndash;</span></div>
      <div><b>WiFi</b><span id="net">&ndash;</span></div>
      <div><b>Unread</b><span id="unread">&ndash;</span></div>
    </div>
  </div>

  <div class="card"><h2>Show on screen</h2>
    <div class="grid">
      <button onclick="go('screen?s=eyes')">&#128064; Eyes</button>
      <button onclick="go('screen?s=clock')">&#128339; Clock</button>
      <button onclick="go('screen?s=notify')">&#128276; Alerts</button>
      <button onclick="go('screen?s=battery')">&#128267; Battery</button>
      <button onclick="go('screen?s=info')">&#8505;&#65039; Info</button>
      <button onclick="go('screen?s=idle')">&#127769; Dim</button>
    </div>
    <label class="sw"><input type="checkbox" id="stay" onchange="go('stayon?v=' + (this.checked ? 1 : 0))"> Keep screen on (no timeout)</label>
  </div>

  <div class="card"><h2>Faces</h2>
    <div class="grid">
      <button onclick="face('normal')">&#128578; Normal</button>
      <button onclick="face('happy')">&#128516; Happy</button>
      <button onclick="face('surprised')">&#128558; Surprised</button>
      <button onclick="face('angry')">&#128544; Angry</button>
      <button onclick="face('sleepy')">&#128564; Sleepy</button>
      <button onclick="face('wink')">&#128521; Wink</button>
      <button onclick="face('love')">&#128525; Love</button>
      <button onclick="face('lookleft')">&#128072; Left</button>
      <button onclick="face('lookright')">&#128073; Right</button>
    </div>
  </div>

  <div class="card"><h2>Notifications</h2>
    <div id="list" class="mut">None yet</div>
    <details><summary>Send a test notification</summary>
      <p></p>
      <input type="text" id="napp" placeholder="App" value="WhatsApp">
      <input type="text" id="ntitle" placeholder="From / title" value="Test">
      <input type="text" id="ntext" placeholder="Message" value="Hello from my phone!">
      <button class="pri" onclick="sendTest()">Send to screen</button>
    </details>
    <details><summary>Get my phone's notifications</summary>
      <p class="mut"><b>Android:</b> install the free <b>Chronos</b> app (Play Store), open it,
      go to Watches &rarr; Pair new watch, pick <b>Digi Eyes</b>, then allow notification access and
      choose which apps to forward. Pair from the app, not from Android's Bluetooth settings.</p>
      <p class="mut"><b>Automation apps</b> (MacroDroid, Tasker, iPhone Shortcuts) can also post
      to this address while on the same WiFi:<br><code id="url"></code></p>
    </details>
  </div>

  <div class="card"><h2>WiFi</h2>
    <div class="mut" id="wifi">&ndash;</div>
    <details><summary>Change home WiFi</summary>
      <form method="post" action="/wifi">
        <p></p>
        <input type="text" name="ssid" placeholder="WiFi name" required>
        <input type="password" name="pass" placeholder="Password">
        <button class="pri">Save &amp; restart</button>
      </form>
    </details>
  </div>

  <script>
    const $ = id => document.getElementById(id);
    const enc = encodeURIComponent;
    function go(p) { return fetch('/' + p).then(() => setTimeout(poll, 200)); }
    function face(m) { go('set?mode=' + m); }
    function sendTest() {
      go('notify?app=' + enc($('napp').value) + '&title=' + enc($('ntitle').value) + '&text=' + enc($('ntext').value));
    }
    function esc(s) { return s.replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }

    function poll() {
      fetch('/status').then(r => r.json()).then(s => {
        $('time').textContent = s.timeOk ? s.time + ', ' + s.date : 'not set yet';
        $('ble').textContent = s.ble ? 'Connected' : (s.bleOn ? 'Not connected' : 'Off');
        $('pbat').textContent = s.pbat < 0 ? 'unknown' : s.pbat + '%' + (s.pchg ? ' (charging)' : '');
        $('dbat').textContent = s.dbat < 0 ? 'USB power' : s.dbat + '% (' + (s.dmv / 1000).toFixed(2) + ' V)';
        $('net').textContent = s.net == 'sta' ? s.ssid + ' (' + s.rssi + ' dBm)' : (s.net == 'ap' ? 'Setup hotspot' : 'Connecting...');
        $('unread').textContent = s.unread;
        $('stay').checked = s.stayOn;
        $('wifi').textContent = s.net == 'sta' ? 'Connected to ' + s.ssid + ' as ' + s.ip + ' (http://' + s.host + '.local)'
                                              : 'Home WiFi not connected, running the setup hotspot.';
        $('list').innerHTML = s.notifs.length ? s.notifs.map(n =>
          '<div class="n"><b>' + esc(n.app) + '</b> <small>' + esc(n.when) + '</small><br>' +
          (n.title ? esc(n.title) + ': ' : '') + esc(n.text) + '</div>').join('') : 'None yet';
      }).catch(() => {});
    }

    // Hand the device this phone's clock (useful on the setup hotspot, where
    // there's no internet time) and, on Chrome for Android, its battery level.
    function sendPhone(b) {
      let q = 'phone?t=' + Math.floor(Date.now() / 1000);
      if (b) q += '&bat=' + Math.round(b.level * 100) + '&chg=' + (b.charging ? 1 : 0);
      fetch('/' + q).catch(() => {});
    }
    if (navigator.getBattery) {
      navigator.getBattery().then(b => { sendPhone(b); b.onlevelchange = b.onchargingchange = () => sendPhone(b); });
    } else {
      sendPhone();
    }

    $('url').textContent = location.origin + '/notify?app=WhatsApp&title=Mom&text=Hello';
    poll();
    setInterval(poll, 3000);
  </script>
</body>
</html>
)rawliteral";
