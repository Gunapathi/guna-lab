#pragma once
// Control page served at http://192.168.4.1/ on the hotspot
// (or http://<device-ip>/ / http://digieyes.local/ on home WiFi)

const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Digi Eyes</title>
<style>
  :root { --bg:#0e0f12; --card:#1a1c21; --line:#2a2e36; --txt:#eef0f3; --mut:#9aa0a6; --acc:#2b8aef; --ok:#3ccf7a; --tile:#252931; }
  * { box-sizing:border-box; -webkit-tap-highlight-color:transparent; }
  body { font-family:system-ui,sans-serif; background:var(--bg); color:var(--txt); margin:0 auto; padding:16px; max-width:520px; }
  header { display:flex; align-items:center; justify-content:space-between; gap:10px; margin:2px 0 14px; }
  h1 { font-size:22px; margin:0; }
  h2 { font-size:12px; text-transform:uppercase; letter-spacing:.08em; color:var(--mut); margin:0 0 10px; }
  .chip { font-size:12px; color:var(--mut); background:var(--card); border:1px solid var(--line); border-radius:99px; padding:5px 10px; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
  .chip::before { content:''; display:inline-block; width:7px; height:7px; border-radius:50%; background:#666; margin-right:6px; }
  .chip.ok::before { background:var(--ok); }
  .card { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:14px; margin-bottom:14px; }
  .grid { display:grid; grid-template-columns:repeat(3,1fr); gap:8px; }
  .wide { grid-column:1 / -1; }
  button { font:inherit; font-size:15px; padding:12px 6px; border:1px solid transparent; border-radius:11px; background:var(--tile); color:var(--txt); cursor:pointer; }
  button:active { transform:scale(.97); }
  .tile { display:flex; flex-direction:column; align-items:center; gap:4px; font-size:13px; }
  .tile i { font-style:normal; font-size:24px; line-height:1.1; }
  .faces .tile i { font-size:30px; }
  .tile.wide { flex-direction:row; justify-content:center; gap:10px; font-size:15px; }
  .on { background:#123a63; border-color:var(--acc); }
  button.pri { background:var(--acc); color:#fff; }
  .row { display:flex; gap:8px; }
  .row > * { flex:1; }
  .sw { display:flex; align-items:center; gap:10px; margin-top:12px; font-size:15px; }
  .sw input { width:20px; height:20px; }

  /* live copy of the OLED */
  .scr { position:relative; background:#000; border-radius:10px; padding:10px; border:1px solid #23262c; }
  .scr canvas { display:block; width:100%; aspect-ratio:2/1; transition:opacity .4s; }
  .scr.dim canvas { opacity:.35; }
  .scr .off { display:none; position:absolute; inset:0; align-items:center; justify-content:center; color:var(--mut); font-size:14px; }
  .scr.lost .off { display:flex; background:rgba(0,0,0,.7); border-radius:10px; }
  .cap { display:flex; justify-content:space-between; align-items:center; margin-top:10px; font-size:13px; color:var(--mut); }
  .cap label { display:flex; align-items:center; gap:6px; }

  .clock { font-size:34px; font-weight:600; letter-spacing:.02em; }
  .date { color:var(--mut); margin:2px 0 12px; }
  .stat { display:grid; grid-template-columns:1fr 1fr; gap:10px 14px; font-size:15px; }
  .stat b { display:block; font-size:12px; color:var(--mut); font-weight:normal; }
  input[type=text], input[type=password] { width:100%; font-size:15px; padding:10px; border-radius:9px;
    border:1px solid var(--line); background:#0b0c0f; color:var(--txt); margin-bottom:8px; }
  .n { border-top:1px solid var(--line); padding:8px 0; font-size:14px; }
  .n:first-child { border-top:none; padding-top:0; }
  .n small, .mut { color:var(--mut); font-size:13px; }
  summary { color:var(--mut); margin-top:12px; cursor:pointer; }
  code { background:#0b0c0f; padding:2px 5px; border-radius:5px; font-size:12px; word-break:break-all; }
</style>
</head>
<body>
  <header>
    <h1>&#128064; Digi Eyes</h1>
    <span class="chip" id="conn">connecting&hellip;</span>
  </header>

  <div class="card">
    <div class="scr" id="scr">
      <canvas id="oled" width="640" height="320"></canvas>
      <div class="off">Device not reachable</div>
    </div>
    <div class="cap"><span>Live screen</span>
      <label><input type="checkbox" id="live" checked> live</label></div>
  </div>

  <div class="card"><h2>Show on screen</h2>
    <div class="grid">
      <button class="tile wide" data-s="idle" onclick="show('idle')"><i>&#127968;</i>Home</button>
      <button class="tile" data-s="eyes" onclick="show('eyes')"><i>&#128064;</i>Eyes</button>
      <button class="tile" data-s="clock" onclick="show('clock')"><i>&#128339;</i>Date &amp; time</button>
      <button class="tile" data-s="notify" onclick="show('notify')"><i>&#128276;</i>Alerts</button>
      <button class="tile" data-s="battery" onclick="show('battery')"><i>&#128267;</i>Battery</button>
      <button class="tile" data-s="info" onclick="show('info')"><i>&#8505;&#65039;</i>Info</button>
      <button class="tile" data-s="connect" onclick="show('connect')"><i>&#128241;</i>Connect QR</button>
    </div>
    <label class="sw"><input type="checkbox" id="stay" onchange="go('stayon?v=' + (this.checked ? 1 : 0))"> Keep screen on (no timeout)</label>
  </div>

  <div class="card"><h2>Faces</h2>
    <div class="grid faces">
      <button class="tile" data-f="normal" onclick="face('normal')"><i>&#128578;</i>Normal</button>
      <button class="tile" data-f="happy" onclick="face('happy')"><i>&#128522;</i>Happy</button>
      <button class="tile" data-f="snoring" onclick="face('snoring')"><i>&#128564;</i>Snoring</button>
      <button class="tile" data-f="angry" onclick="face('angry')"><i>&#128544;</i>Angry</button>
      <button class="tile" data-f="love" onclick="face('love')"><i>&#128525;</i>Love</button>
      <button class="tile" data-f="alone" onclick="face('alone')"><i>&#129402;</i>Alone</button>
      <button class="tile" data-f="confused" onclick="face('confused')"><i>&#128533;</i>Confused</button>
      <button class="tile" data-f="hangover" onclick="face('hangover')"><i>&#129396;</i>Hangover</button>
      <button class="tile" data-f="surprised" onclick="face('surprised')"><i>&#128558;</i>Surprised</button>
    </div>
  </div>

  <div class="card"><h2>Date &amp; time</h2>
    <div class="clock" id="clock">&ndash;&ndash;:&ndash;&ndash;</div>
    <div class="date" id="date">not set yet</div>
    <div class="row">
      <button onclick="syncTime()">&#128260; Sync from this phone</button>
      <button onclick="show('clock')">&#128339; Show it</button>
    </div>
  </div>

  <div class="card"><h2>Notifications</h2>
    <div class="grid">
      <button class="tile" onclick="notify('WhatsApp', 'Mom', 'Dinner at 8? Don\'t be late!')"><i>&#128172;</i>WhatsApp</button>
      <button class="tile" onclick="notify('Gmail', 'GitHub', 'Your pull request was merged. Nice work!')"><i>&#9993;&#65039;</i>Gmail</button>
      <button class="tile" onclick="notify('Calendar', 'Stand-up', 'Team stand-up starts in 10 minutes', 'bell')"><i>&#128197;</i>Reminder</button>
    </div>
    <p class="mut" style="margin:14px 0 8px">Incoming call</p>
    <input type="text" id="cname" placeholder="Caller name" value="Mom">
    <div class="row">
      <button class="pri" onclick="ring()">&#128222; Ring</button>
      <button onclick="go('call?end=1')">&#128245; End call</button>
    </div>
    <details><summary>Write your own notification</summary>
      <p></p>
      <input type="text" id="napp" placeholder="App" value="WhatsApp">
      <input type="text" id="ntitle" placeholder="From / title" value="Test">
      <input type="text" id="ntext" placeholder="Message" value="Hello from my phone!">
      <button class="pri" style="width:100%" onclick="notify($('napp').value, $('ntitle').value, $('ntext').value)">Send to screen</button>
    </details>
    <p class="mut" style="margin:14px 0 8px">Recent</p>
    <div id="list" class="mut">None yet</div>
    <details><summary>Get my phone's real notifications</summary>
      <p class="mut"><b>Android:</b> install the free <b>Chronos</b> app (Play Store), open it,
      go to Watches &rarr; Pair new watch, pick <b>Digi Eyes</b>, then allow notification access and
      choose which apps to forward. Pair from the app, not from Android's Bluetooth settings.</p>
      <p class="mut"><b>Automation apps</b> (MacroDroid, Tasker, iPhone Shortcuts) can also post
      to this address while on the same WiFi:<br><code id="url"></code></p>
    </details>
  </div>

  <div class="card"><h2>Status</h2>
    <div class="stat">
      <div><b>Phone link</b><span id="ble">&ndash;</span></div>
      <div><b>Unread</b><span id="unread">&ndash;</span></div>
      <div><b>Phone battery</b><span id="pbat">&ndash;</span></div>
      <div><b>Device battery</b><span id="dbat">&ndash;</span></div>
    </div>
  </div>

  <div class="card"><h2>WiFi</h2>
    <div class="mut" id="wifi">&ndash;</div>
    <details id="wifiform"><summary>Change home WiFi</summary>
      <form method="post" action="/wifi">
        <p></p>
        <input type="text" name="ssid" placeholder="WiFi name" required>
        <input type="password" name="pass" placeholder="Password">
        <button class="pri" style="width:100%">Save &amp; restart</button>
      </form>
    </details>
  </div>

  <script>
    const $ = id => document.getElementById(id);
    const enc = encodeURIComponent;
    function go(p) { return fetch('/' + p).then(() => setTimeout(poll, 150)).catch(() => {}); }
    function show(s) { go('screen?s=' + s); }
    function face(m) { go('set?mode=' + m); }
    function notify(app, title, text, icon) {
      go('notify?app=' + enc(app) + '&title=' + enc(title) + '&text=' + enc(text) + (icon ? '&icon=' + icon : ''));
    }
    function ring() { go('call?name=' + enc($('cname').value || 'Unknown')); }
    function esc(s) { return s.replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }

    function poll() {
      fetch('/status').then(r => r.json()).then(s => {
        const ap = s.net == 'ap';
        $('conn').textContent = ap ? 'Hotspot ' + s.apSsid : (s.net == 'sta' ? 'WiFi ' + s.ssid : 'Connecting...');
        $('conn').className = 'chip ok';
        $('clock').textContent = s.timeOk ? s.time : '--:--';
        $('date').textContent = s.timeOk ? s.date : 'not set yet - tap Sync';
        $('ble').textContent = s.ble ? 'Connected' : (s.bleOn ? 'Not connected' : 'Off');
        $('pbat').textContent = s.pbat < 0 ? 'unknown' : s.pbat + '%' + (s.pchg ? ' (charging)' : '');
        $('dbat').textContent = s.dbat < 0 ? 'USB power' : s.dbat + '% (' + (s.dmv / 1000).toFixed(2) + ' V)';
        $('unread').textContent = s.unread;
        $('stay').checked = s.stayOn;
        $('scr').classList.toggle('dim', s.screen == 'idle');
        document.querySelectorAll('[data-s]').forEach(b => b.classList.toggle('on', b.dataset.s == s.screen));
        document.querySelectorAll('[data-f]').forEach(b => b.classList.toggle('on', s.screen == 'eyes' && b.dataset.f == s.face));
        $('wifi').textContent = ap
          ? 'Own hotspot "' + s.apSsid + '" - ' + s.clients + ' device' + (s.clients == 1 ? '' : 's') +
            ' connected. Open http://' + s.ip + ' (or http://' + s.host + '.local).' +
            (s.hotspotOnly ? '' : ' Home WiFi not connected, running the setup hotspot.')
          : (s.net == 'sta' ? 'Connected to ' + s.ssid + ' as ' + s.ip + ' (http://' + s.host + '.local)' : 'Connecting...');
        $('wifiform').style.display = s.hotspotOnly ? 'none' : '';
        $('list').innerHTML = s.notifs.length ? s.notifs.map(n =>
          '<div class="n"><b>' + esc(n.app) + '</b> <small>' + esc(n.when) + '</small><br>' +
          (n.title ? esc(n.title) + ': ' : '') + esc(n.text) + '</div>').join('') : 'None yet';
      }).catch(() => { $('conn').textContent = 'offline'; $('conn').className = 'chip'; });
    }

    // Hand the device this phone's clock (on the hotspot there's no internet
    // time) and, on Chrome for Android, its battery level.
    let battery = null;
    function sendPhone() {
      let q = 'phone?t=' + Math.floor(Date.now() / 1000);
      if (battery) q += '&bat=' + Math.round(battery.level * 100) + '&chg=' + (battery.charging ? 1 : 0);
      return fetch('/' + q).catch(() => {});
    }
    function syncTime() { sendPhone().then(() => show('clock')); }
    if (navigator.getBattery) {
      navigator.getBattery().then(b => { battery = b; sendPhone(); b.onlevelchange = b.onchargingchange = sendPhone; });
    } else {
      sendPhone();
    }

    // Live screen: the device's 128x64 frame buffer, 8 rows per byte
    const cx = $('oled').getContext('2d');
    function drawFrame(b) {
      cx.fillStyle = '#000';
      cx.fillRect(0, 0, 640, 320);
      cx.fillStyle = '#e8f5ff';
      for (let page = 0; page < 8; page++)
        for (let x = 0; x < 128; x++) {
          const v = b[page * 128 + x];
          if (v) for (let bit = 0; bit < 8; bit++) if (v >> bit & 1) cx.fillRect(x * 5, (page * 8 + bit) * 5, 4, 4);
        }
    }
    function frame() {
      if (!$('live').checked || document.hidden) return setTimeout(frame, 500);
      fetch('/fb', {cache: 'no-store'}).then(r => r.arrayBuffer()).then(a => {
        if (a.byteLength == 1024) drawFrame(new Uint8Array(a));
        $('scr').classList.remove('lost');
      }).catch(() => $('scr').classList.add('lost')).finally(() => setTimeout(frame, 150));
    }

    $('url').textContent = location.origin + '/notify?app=WhatsApp&title=Mom&text=Hello';
    poll();
    setInterval(poll, 3000);
    frame();
  </script>
</body>
</html>
)rawliteral";
