// ============================================================
//  EnvCube — Web UI
//  GET  /              → HTML app (5 tabs)
//  GET  /api/config    → current config as JSON
//  POST /api/config    → save config
//  GET  /api/readings  → live sensor readings as JSON
//  GET  /api/weather   → current outdoor weather as JSON
//  POST /api/weather/fetch → trigger immediate weather fetch
//  GET  /api/i2cscan   → scan I2C bus, return found addresses
//  POST /api/reboot    → restart device
//  POST /api/reset     → factory reset + restart
// ============================================================

#include "web_ui.h"
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "../config.h"
#include "../storage/nvs_config.h"
#include "../alerts/alert_engine.h"
#include "../display/oled.h"
#include "../connectivity/weather.h"
#include "../outputs/buzzer.h"
#include "logger.h"

static WebServer _server(80);
static bool      _started = false;

// ── HTML ──────────────────────────────────────────────────────
static const char HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EnvCube</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#0f1117;color:#e0e0e0;min-height:100vh}
header{background:#1a1d27;padding:14px 20px;border-bottom:1px solid #2a2d3a;display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:8px}
header h1{font-size:1.1rem;color:#7eb8f7;letter-spacing:.5px}
header p{font-size:.72rem;color:#666}
.ver{font-size:.7rem;color:#444;background:#1e2130;padding:2px 8px;border-radius:4px}
nav{display:flex;background:#1a1d27;border-bottom:1px solid #2a2d3a;overflow-x:auto}
nav button{flex-shrink:0;padding:11px 14px;background:none;border:none;color:#888;cursor:pointer;font-size:.82rem;border-bottom:2px solid transparent;white-space:nowrap;transition:all .2s}
nav button.active{color:#7eb8f7;border-bottom-color:#7eb8f7}
.tab{display:none;padding:18px;max-width:620px;margin:0 auto}
.tab.active{display:block}
.field{margin-bottom:13px}
label{display:block;font-size:.75rem;color:#888;margin-bottom:4px;text-transform:uppercase;letter-spacing:.4px}
input[type=text],input[type=password],input[type=number]{width:100%;padding:8px 11px;background:#1a1d27;border:1px solid #2a2d3a;border-radius:6px;color:#e0e0e0;font-size:.92rem}
input:focus{outline:none;border-color:#7eb8f7}
input:disabled{color:#555}
.row{display:flex;align-items:center;gap:10px;margin-bottom:13px}
.row input[type=checkbox]{width:17px;height:17px;accent-color:#7eb8f7;cursor:pointer}
.row label{margin:0;text-transform:none;font-size:.88rem;color:#ccc;cursor:pointer}
.half{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.actions{margin-top:18px;display:flex;align-items:center;gap:10px;flex-wrap:wrap}
.btn{padding:8px 18px;border-radius:6px;border:none;cursor:pointer;font-size:.86rem;font-weight:600;transition:opacity .15s}
.btn-primary{background:#7eb8f7;color:#0f1117}
.btn-danger{background:#c0392b;color:#fff}
.btn-warn{background:#e67e22;color:#fff}
.btn-ghost{background:#1e2130;color:#aaa;border:1px solid #2a2d3a}
.btn:hover{opacity:.82}
.toast{font-size:.8rem;padding:5px 10px;border-radius:5px;display:none}
.toast.ok{background:#1a3a24;color:#5cc97a}
.toast.err{background:#3a1a1a;color:#e05c5c}
table{width:100%;border-collapse:collapse;font-size:.86rem}
th{text-align:left;padding:7px 9px;color:#555;font-weight:500;font-size:.73rem;text-transform:uppercase;letter-spacing:.4px;border-bottom:1px solid #2a2d3a}
td{padding:9px;border-bottom:1px solid #1e2130;vertical-align:middle}
td.lbl{color:#888;width:42%}
td.val{font-family:monospace;font-size:.9rem;font-weight:600}
td.sta{width:18%;text-align:right}
.badge{display:inline-block;padding:2px 7px;border-radius:4px;font-size:.7rem;font-weight:700}
.badge-ok{background:#1a3a24;color:#5cc97a}
.badge-err{background:#3a1a1a;color:#e05c5c}
.badge-na{background:#1e2130;color:#555}
.divider{font-size:.72rem;color:#555;text-transform:uppercase;letter-spacing:.5px;margin:16px 0 10px;padding-bottom:5px;border-bottom:1px solid #2a2d3a}
.hint{font-size:.72rem;color:#555;margin-top:3px}
.ts{font-size:.7rem;color:#444;margin-top:8px;text-align:right}
.weather-card{background:#1a1d27;border:1px solid #2a2d3a;border-radius:8px;padding:14px;margin-bottom:14px}
.weather-card h3{font-size:.8rem;color:#666;margin-bottom:8px;text-transform:uppercase;letter-spacing:.4px}
.weather-big{font-size:2rem;font-weight:700;color:#7eb8f7}
.weather-row{display:flex;gap:20px;margin-top:6px;font-size:.85rem;color:#aaa}
</style>
</head>
<body>
<header>
  <div><h1>&#9672; EnvCube</h1><p id="hdr-sub">Connecting...</p></div>
  <span class="ver" id="ver-badge">v—</span>
</header>
<nav>
  <button class="active" onclick="show('console',this)">Console</button>
  <button onclick="show('device',this)">Device</button>
  <button onclick="show('wifi',this)">WiFi</button>
  <button onclick="show('mqtt',this)">MQTT</button>
  <button onclick="show('weather',this)">Weather</button>
  <button onclick="show('log',this)">Log</button>
</nav>

<!-- Console -->
<div id="tab-console" class="tab active">
  <div class="divider">Indoor sensors</div>
  <table>
    <thead><tr><th>Sensor</th><th class="val">Reading</th><th style="text-align:right">Status</th></tr></thead>
    <tbody>
      <tr><td class="lbl">Temperature</td><td class="val" id="r-temp">—</td><td class="sta" id="s-temp"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="lbl">Humidity</td><td class="val" id="r-hum">—</td><td class="sta" id="s-hum"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="lbl">Pressure</td><td class="val" id="r-pres">—</td><td class="sta" id="s-pres"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="lbl">CO&#x2082;</td><td class="val" id="r-co2">—</td><td class="sta" id="s-co2"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="lbl">VOC Index</td><td class="val" id="r-voc">—</td><td class="sta" id="s-voc"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="lbl">NOx Index</td><td class="val" id="r-nox">—</td><td class="sta" id="s-nox"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="lbl">PM2.5</td><td class="val" id="r-pm25">—</td><td class="sta" id="s-pm25"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="lbl">PM10</td><td class="val" id="r-pm10">—</td><td class="sta" id="s-pm10"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="lbl">Noise</td><td class="val" id="r-noise">—</td><td class="sta" id="s-noise"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="lbl">Light</td><td class="val" id="r-lux">—</td><td class="sta" id="s-lux"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="lbl">Smoke (raw)</td><td class="val" id="r-smoke">—</td><td class="sta" id="s-smoke"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="lbl">Presence</td><td class="val" id="r-pres2">—</td><td class="sta" id="s-pres2"><span class="badge badge-na">—</span></td></tr>
    </tbody>
  </table>
  <div class="divider" style="margin-top:18px">Outdoor weather</div>
  <div class="weather-card" id="weather-card">
    <h3>Outdoor conditions</h3>
    <div class="weather-big" id="w-temp">—</div>
    <div class="weather-row">
      <span>Humidity: <b id="w-hum">—</b></span>
      <span>UV: <b id="w-uv">—</b></span>
      <span id="w-cond">—</span>
    </div>
  </div>
  <p class="ts" id="ts">Not yet updated</p>
</div>

<!-- Device -->
<div id="tab-device" class="tab">
  <div class="field"><label>Room Name</label><input type="text" id="room_name" maxlength="31"></div>
  <div class="half">
    <div class="field"><label>Cube ID</label><input type="text" id="cube_id" disabled></div>
    <div class="field"><label>Firmware</label><input type="text" id="fw_ver" disabled></div>
  </div>
  <div class="field"><label>IP Address</label><input type="text" id="ip_addr" disabled></div>
  <div class="divider">Buzzer test</div>
  <div class="row" style="gap:8px;flex-wrap:wrap">
    <select id="buzz-pattern" style="background:#1a1d27;border:1px solid #2a2d3a;border-radius:6px;color:#e0e0e0;padding:8px 10px;font-size:.88rem;flex:1;min-width:200px">
      <option value="confirm">Confirm (880 Hz, short)</option>
      <option value="beep1">1 Beep (2 kHz)</option>
      <option value="beep2">2 Beeps (2 kHz)</option>
      <option value="beep3">3 Beeps (2 kHz)</option>
      <option value="warning">Warning (1 kHz)</option>
      <option value="alarm">Alarm (continuous)</option>
    </select>
    <button class="btn btn-primary" onclick="buzzerTest()">&#9654; Test</button>
    <button class="btn btn-ghost" onclick="buzzerStop()">&#9646;&#9646; Stop</button>
  </div>
  <div class="actions">
    <button class="btn btn-primary" onclick="save()">Save</button>
    <button class="btn btn-warn" onclick="doReboot()">Reboot</button>
    <button class="btn btn-danger" onclick="doReset()">Factory Reset</button>
    <span class="toast" id="toast-d"></span>
  </div>
</div>

<!-- WiFi -->
<div id="tab-wifi" class="tab">
  <div class="divider">Primary network</div>
  <div class="field"><label>SSID</label><input type="text" id="wifi_ssid" maxlength="63"></div>
  <div class="field"><label>Password</label><input type="password" id="wifi_password" maxlength="63" placeholder="Leave blank to keep current"></div>
  <div class="divider">Failover network</div>
  <div class="field"><label>SSID</label><input type="text" id="wifi_ssid2" maxlength="63" placeholder="Optional — used if primary fails"></div>
  <div class="field"><label>Password</label><input type="password" id="wifi_password2" maxlength="63" placeholder="Leave blank to keep current"></div>
  <div class="divider">AP fallback</div>
  <div class="field">
    <label>Switch to AP mode after (seconds, 0 = never)</label>
    <input type="number" id="wifi_ap_timeout" min="0" max="3600" style="max-width:120px">
    <p class="hint">If WiFi can't connect within this time, the device opens its own setup hotspot.</p>
  </div>
  <p class="hint" style="margin-bottom:10px">Device reboots to apply new WiFi credentials.</p>
  <div class="actions"><button class="btn btn-primary" onclick="save()">Save &amp; Reboot</button><span class="toast" id="toast-w"></span></div>
</div>

<!-- MQTT -->
<div id="tab-mqtt" class="tab">
  <div class="field"><label>Broker Host / IP</label><input type="text" id="mqtt_host" maxlength="63" placeholder="e.g. 192.168.1.100"></div>
  <div class="half">
    <div class="field"><label>Port</label><input type="number" id="mqtt_port" min="1" max="65535"></div>
    <div class="field" style="display:flex;align-items:flex-end;padding-bottom:1px">
      <div class="row" style="margin:0"><input type="checkbox" id="mqtt_enabled"><label for="mqtt_enabled">Enabled</label></div>
    </div>
  </div>
  <div class="divider">Authentication (optional)</div>
  <div class="field"><label>Username</label><input type="text" id="mqtt_user" maxlength="63" placeholder="Leave blank if not required"></div>
  <div class="field"><label>Password</label><input type="password" id="mqtt_password" maxlength="63" placeholder="Leave blank to keep current"></div>
  <div class="actions"><button class="btn btn-primary" onclick="save()">Save</button><span class="toast" id="toast-m"></span></div>
</div>

<!-- Log -->
<div id="tab-log" class="tab">
  <div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:10px">
    <div class="divider" style="margin:0;border:none">Device log</div>
    <div style="display:flex;gap:8px;align-items:center">
      <label style="display:flex;align-items:center;gap:6px;font-size:.78rem;color:#888;text-transform:none;cursor:pointer">
        <input type="checkbox" id="log-scroll" checked style="accent-color:#7eb8f7"> Auto-scroll
      </label>
      <button class="btn btn-ghost" style="padding:4px 10px;font-size:.78rem" onclick="clearLog()">Clear</button>
    </div>
  </div>
  <pre id="log-pre" style="background:#0a0c12;border:1px solid #2a2d3a;border-radius:6px;padding:12px;font-size:.75rem;line-height:1.5;height:380px;overflow-y:auto;white-space:pre-wrap;word-break:break-all;color:#c0c0c0;margin:0"></pre>
</div>

<!-- Weather -->
<div id="tab-weather" class="tab">
  <div class="divider">Location</div>
  <div class="half">
    <div class="field"><label>Latitude</label><input type="text" id="latitude" maxlength="12" placeholder="e.g. 33.6846"></div>
    <div class="field"><label>Longitude</label><input type="text" id="longitude" maxlength="12" placeholder="e.g. -117.826"></div>
  </div>
  <p class="hint" style="margin-bottom:14px">Used for Open-Meteo weather fetch. Updates every 10 minutes when connected.</p>
  <div class="divider">Current conditions</div>
  <div class="weather-card">
    <h3>Outdoor</h3>
    <div class="weather-big" id="wt-temp">—</div>
    <div class="weather-row">
      <span>Humidity: <b id="wt-hum">—</b></span>
      <span>UV: <b id="wt-uv">—</b></span>
      <span id="wt-cond">—</span>
    </div>
    <p class="ts" id="wt-age">—</p>
  </div>
  <div class="actions">
    <button class="btn btn-primary" onclick="save()">Save Location</button>
    <button class="btn btn-ghost" onclick="fetchWeather()">Fetch Now</button>
    <span class="toast" id="toast-weather"></span>
  </div>
</div>

<script>
var _tab='console';
function show(t,btn){
  document.querySelectorAll('.tab').forEach(function(e){e.classList.remove('active')});
  document.querySelectorAll('nav button').forEach(function(e){e.classList.remove('active')});
  document.getElementById('tab-'+t).classList.add('active');
  btn.classList.add('active');
  _tab=t;
  if(t==='weather')loadWeather();
  if(t==='log')loadLog();
}
function badge(ok){return ok?'<span class="badge badge-ok">OK</span>':'<span class="badge badge-err">ERR</span>';}
function set(vid,val,sid,ok){document.getElementById(vid).textContent=val;document.getElementById(sid).innerHTML=badge(ok);}
var WMO={0:'Clear sky',1:'Mainly clear',2:'Partly cloudy',3:'Overcast',45:'Fog',48:'Icy fog',51:'Light drizzle',61:'Light rain',63:'Rain',65:'Heavy rain',71:'Light snow',73:'Snow',80:'Rain showers',81:'Showers',95:'Thunderstorm'};
function wmoDesc(c){return WMO[c]||('Code '+c);}
function loadReadings(){
  fetch('/api/readings').then(function(r){return r.json();}).then(function(d){
    set('r-temp',d.thermal_ok?d.temperature_c.toFixed(1)+' °C':'—','s-temp',d.thermal_ok);
    set('r-hum', d.thermal_ok?d.humidity_rh.toFixed(1)+' %':'—','s-hum',d.thermal_ok);
    set('r-pres',d.pressure_ok?d.pressure_hpa.toFixed(1)+' hPa':'—','s-pres',d.pressure_ok);
    set('r-co2', d.co2_ok?d.co2_ppm+' ppm':'—','s-co2',d.co2_ok);
    set('r-voc', d.aq_ok?String(d.voc_index):'—','s-voc',d.aq_ok);
    set('r-nox', d.aq_ok?String(d.nox_index):'—','s-nox',d.aq_ok);
    set('r-pm25',d.pm_ok?d.pm2_5+' μg/m³':'—','s-pm25',d.pm_ok);
    set('r-pm10',d.pm_ok?d.pm10+' μg/m³':'—','s-pm10',d.pm_ok);
    set('r-noise',d.noise_ok?d.noise_db.toFixed(1)+' dBA':'—','s-noise',d.noise_ok);
    set('r-lux', d.lux_ok?d.lux+' lx':'—','s-lux',d.lux_ok);
    set('r-smoke',d.smoke_ok?String(d.smoke_raw):'—','s-smoke',d.smoke_ok);
    var p=d.presence_ok?(d.presence?'Yes ('+d.presence_cm+' cm)':'No'):'—';
    set('r-pres2',p,'s-pres2',d.presence_ok);
    document.getElementById('ts').textContent='Updated: '+new Date().toLocaleTimeString();
  }).catch(function(){});
  fetch('/api/weather').then(function(r){return r.json();}).then(function(w){
    if(w.valid){
      document.getElementById('w-temp').textContent=w.temp_f.toFixed(1)+'°F';
      document.getElementById('w-hum').textContent=w.humidity.toFixed(0)+'%';
      document.getElementById('w-uv').textContent=w.uv_index.toFixed(1);
      document.getElementById('w-cond').textContent=wmoDesc(w.weather_code);
    } else {
      document.getElementById('w-temp').textContent='No data';
      document.getElementById('w-cond').textContent='Set lat/lon in Weather tab';
    }
  }).catch(function(){});
}
function loadWeather(){
  fetch('/api/weather').then(function(r){return r.json();}).then(function(w){
    if(w.valid){
      document.getElementById('wt-temp').textContent=w.temp_f.toFixed(1)+'°F';
      document.getElementById('wt-hum').textContent=w.humidity.toFixed(0)+'%';
      document.getElementById('wt-uv').textContent=w.uv_index.toFixed(1);
      document.getElementById('wt-cond').textContent=wmoDesc(w.weather_code);
      var age=Math.round((Date.now()-w.fetched_ms)/60000);
      document.getElementById('wt-age').textContent='Fetched '+age+' min ago';
    } else {
      document.getElementById('wt-temp').textContent='No data';
      document.getElementById('wt-cond').textContent=w.lat_set?'Waiting for first fetch':'Set latitude and longitude below';
    }
  }).catch(function(){});
}
function loadConfig(){
  fetch('/api/config').then(function(r){return r.json();}).then(function(d){
    document.getElementById('room_name').value=d.room_name||'';
    document.getElementById('cube_id').value='#'+d.cube_id;
    document.getElementById('fw_ver').value=d.firmware_version||'';
    document.getElementById('ip_addr').value=d.ip_addr||'';
    document.getElementById('wifi_ssid').value=d.wifi_ssid||'';
    document.getElementById('wifi_ssid2').value=d.wifi_ssid2||'';
    document.getElementById('wifi_ap_timeout').value=d.wifi_ap_timeout_secs||120;
    document.getElementById('mqtt_host').value=d.mqtt_host||'';
    document.getElementById('mqtt_port').value=d.mqtt_port||1883;
    document.getElementById('mqtt_enabled').checked=!!d.mqtt_enabled;
    document.getElementById('mqtt_user').value=d.mqtt_user||'';
    document.getElementById('latitude').value=d.latitude||'';
    document.getElementById('longitude').value=d.longitude||'';
    document.getElementById('hdr-sub').textContent=(d.room_name||'EnvCube')+' · '+d.ip_addr;
    document.getElementById('ver-badge').textContent='v'+d.firmware_version;
  }).catch(function(){});
}
function showToast(id,ok,msg){
  var el=document.getElementById(id);
  el.textContent=msg;el.className='toast '+(ok?'ok':'err');el.style.display='inline-block';
  if(ok)setTimeout(function(){el.style.display='none';},3000);
}
function save(){
  var toastMap={console:'toast-d',device:'toast-d',wifi:'toast-w',mqtt:'toast-m',weather:'toast-weather'};
  var tid=toastMap[_tab]||'toast-d';
  var body={
    room_name:document.getElementById('room_name').value,
    wifi_ssid:document.getElementById('wifi_ssid').value,
    wifi_password:document.getElementById('wifi_password').value,
    wifi_ssid2:document.getElementById('wifi_ssid2').value,
    wifi_password2:document.getElementById('wifi_password2').value,
    wifi_ap_timeout_secs:parseInt(document.getElementById('wifi_ap_timeout').value)||0,
    mqtt_host:document.getElementById('mqtt_host').value,
    mqtt_port:parseInt(document.getElementById('mqtt_port').value)||1883,
    mqtt_enabled:document.getElementById('mqtt_enabled').checked,
    mqtt_user:document.getElementById('mqtt_user').value,
    mqtt_password:document.getElementById('mqtt_password').value,
    latitude:parseFloat(document.getElementById('latitude').value)||0,
    longitude:parseFloat(document.getElementById('longitude').value)||0
  };
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){return r.json();}).then(function(d){
      if(d.reboot){showToast(tid,true,'Saved — rebooting…');}
      else{showToast(tid,d.ok?'ok':'err',d.ok?'Saved':'Error');if(d.ok)loadConfig();}
    }).catch(function(){showToast(tid,false,'Error');});
}
function fetchWeather(){
  fetch('/api/weather/fetch',{method:'POST'}).then(function(r){return r.json();}).then(function(d){
    showToast('toast-weather',d.ok,d.ok?'Fetched!':'Fetch failed (check lat/lon)');
    if(d.ok)setTimeout(loadWeather,500);
  }).catch(function(){showToast('toast-weather',false,'Error');});
}
function doReboot(){if(!confirm('Reboot EnvCube?'))return;fetch('/api/reboot',{method:'POST'});}
function doReset(){
  if(!confirm('FACTORY RESET — all config will be erased. Are you sure?'))return;
  if(!confirm('This cannot be undone. Continue?'))return;
  fetch('/api/reset',{method:'POST'}).finally(function(){showToast('toast-d',true,'Resetting…');});
}
var _logSeq=-1,_logLines=[];
function levelColor(l){return l==='E'?'#e05c5c':l==='W'?'#e6a817':l==='I'?'#7eb8f7':'#888';}
function parseLine(l){
  // Match IDF log format: [tag][level][...] message
  var m=l.match(/^\((\w)\) (.*)$/);
  if(m) return '<span style="color:'+levelColor(m[1])+'">'+escHtml(m[0])+'</span>';
  // Match [E][tag:line] format
  var m2=l.match(/^\[(\w)\]\[/);
  if(m2) return '<span style="color:'+levelColor(m2[1])+'">'+escHtml(l)+'</span>';
  return escHtml(l);
}
function escHtml(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}
function loadLog(){
  fetch('/api/log').then(function(r){return r.json();}).then(function(d){
    if(d.seq===_logSeq) return;
    _logSeq=d.seq;
    _logLines=d.lines||[];
    renderLog();
  }).catch(function(){});
}
function renderLog(){
  var pre=document.getElementById('log-pre');
  pre.innerHTML=_logLines.map(parseLine).join('\n');
  if(document.getElementById('log-scroll').checked) pre.scrollTop=pre.scrollHeight;
}
function clearLog(){_logLines=[];document.getElementById('log-pre').innerHTML='';}
function buzzerTest(){
  var p=document.getElementById('buzz-pattern').value;
  fetch('/api/buzzer/test',{method:'POST',headers:{'Content-Type':'text/plain'},body:p});
}
function buzzerStop(){
  fetch('/api/buzzer/test',{method:'POST',headers:{'Content-Type':'text/plain'},body:'stop'});
}
loadConfig();loadReadings();
setInterval(loadReadings,3000);
setInterval(function(){if(_tab==='log')loadLog();},2000);
</script>
</body>
</html>
)rawhtml";

// ── Handlers ──────────────────────────────────────────────────

static void handleRoot() {
    _server.send_P(200, "text/html", HTML);
}

static void handleGetConfig() {
    JsonDocument doc;
    doc["room_name"]         = g_config.room_name;
    doc["cube_id"]           = g_config.cube_id;
    doc["firmware_version"]  = ENVCUBE_VERSION;
    doc["ip_addr"]           = WiFi.localIP().toString();
    doc["wifi_ssid"]             = g_config.wifi_ssid;
    doc["wifi_ssid2"]            = g_config.wifi_ssid2;
    doc["wifi_ap_timeout_secs"]  = g_config.wifi_ap_timeout_secs;
    doc["mqtt_host"]         = g_config.mqtt_host;
    doc["mqtt_port"]         = g_config.mqtt_port;
    doc["mqtt_enabled"]      = g_config.mqtt_enabled;
    doc["mqtt_user"]         = g_config.mqtt_user;
    // mqtt_password intentionally omitted from GET
    doc["latitude"]          = g_config.latitude;
    doc["longitude"]         = g_config.longitude;
    String out;
    serializeJson(doc, out);
    _server.send(200, "application/json", out);
}

static void handlePostConfig() {
    if (!_server.hasArg("plain")) { _server.send(400, "application/json", "{\"ok\":false}"); return; }
    JsonDocument doc;
    if (deserializeJson(doc, _server.arg("plain"))) { _server.send(400, "application/json", "{\"ok\":false}"); return; }

    bool needReboot = false;

    if (doc["room_name"].is<const char*>())
        strlcpy(g_config.room_name, doc["room_name"].as<const char*>(), sizeof(g_config.room_name));

    // WiFi primary
    const char* newSsid = doc["wifi_ssid"].is<const char*>() ? doc["wifi_ssid"].as<const char*>() : "";
    if (strlen(newSsid) > 0) {
        strlcpy(g_config.wifi_ssid, newSsid, sizeof(g_config.wifi_ssid));
        const char* newPass = doc["wifi_password"].is<const char*>() ? doc["wifi_password"].as<const char*>() : "";
        if (strlen(newPass) > 0)
            strlcpy(g_config.wifi_password, newPass, sizeof(g_config.wifi_password));
        needReboot = true;
    }

    // WiFi failover
    if (doc["wifi_ssid2"].is<const char*>())
        strlcpy(g_config.wifi_ssid2, doc["wifi_ssid2"].as<const char*>(), sizeof(g_config.wifi_ssid2));
    const char* newPass2 = doc["wifi_password2"].is<const char*>() ? doc["wifi_password2"].as<const char*>() : "";
    if (strlen(newPass2) > 0)
        strlcpy(g_config.wifi_password2, newPass2, sizeof(g_config.wifi_password2));
    if (doc["wifi_ap_timeout_secs"].is<int>())
        g_config.wifi_ap_timeout_secs = (uint16_t)doc["wifi_ap_timeout_secs"].as<int>();

    // MQTT
    if (doc["mqtt_host"].is<const char*>())
        strlcpy(g_config.mqtt_host, doc["mqtt_host"].as<const char*>(), sizeof(g_config.mqtt_host));
    if (doc["mqtt_port"].is<int>())
        g_config.mqtt_port = (uint16_t)doc["mqtt_port"].as<int>();
    if (doc["mqtt_enabled"].is<bool>())
        g_config.mqtt_enabled = doc["mqtt_enabled"].as<bool>();
    if (doc["mqtt_user"].is<const char*>())
        strlcpy(g_config.mqtt_user, doc["mqtt_user"].as<const char*>(), sizeof(g_config.mqtt_user));
    const char* newMqttPass = doc["mqtt_password"].is<const char*>() ? doc["mqtt_password"].as<const char*>() : "";
    if (strlen(newMqttPass) > 0)
        strlcpy(g_config.mqtt_password, newMqttPass, sizeof(g_config.mqtt_password));

    // Location
    if (doc["latitude"].is<float>())  g_config.latitude  = doc["latitude"].as<float>();
    if (doc["longitude"].is<float>()) g_config.longitude = doc["longitude"].as<float>();

    NvsConfig::save();

    String resp = String("{\"ok\":true,\"reboot\":") + (needReboot ? "true" : "false") + "}";
    _server.send(200, "application/json", resp);
    if (needReboot) { delay(300); ESP.restart(); }
}

static void handleGetReadings() {
    JsonDocument doc;
    doc["temperature_c"] = serialized(String(g_readings.temperature_c, 2));
    doc["humidity_rh"]   = serialized(String(g_readings.humidity_rh, 2));
    doc["pressure_hpa"]  = serialized(String(g_readings.pressure_hpa, 2));
    doc["thermal_ok"]    = g_readings.thermal_ok;
    doc["pressure_ok"]   = g_readings.pressure_ok;
    doc["smoke_raw"]     = g_readings.smoke_raw;
    doc["smoke_ok"]      = g_readings.smoke_ok;
    doc["co2_ppm"]       = g_readings.co2_ppm;
    doc["co2_ok"]        = g_readings.co2_ok;
    doc["voc_index"]     = g_readings.voc_index;
    doc["nox_index"]     = g_readings.nox_index;
    doc["lux"]           = g_readings.lux;
    doc["lux_ok"]        = g_readings.lux_ok;
    doc["aq_ok"]         = g_readings.aq_ok;
    doc["pm1_0"]         = g_readings.pm1_0;
    doc["pm2_5"]         = g_readings.pm2_5;
    doc["pm10"]          = g_readings.pm10;
    doc["pm_ok"]         = g_readings.pm_ok;
    doc["presence"]      = g_readings.presence;
    doc["presence_cm"]   = g_readings.presence_cm;
    doc["presence_ok"]   = g_readings.presence_ok;
    doc["noise_db"]      = serialized(String(g_readings.noise_db, 2));
    doc["noise_ok"]      = g_readings.noise_ok;
    doc["last_update_ms"]= g_readings.last_update_ms;
    String out;
    serializeJson(doc, out);
    _server.send(200, "application/json", out);
}

static void handleGetWeather() {
    JsonDocument doc;
    doc["valid"]        = g_weather.valid;
    doc["temp_f"]       = serialized(String(g_weather.temp_f, 2));
    doc["humidity"]     = serialized(String(g_weather.humidity, 1));
    doc["weather_code"] = g_weather.weather_code;
    doc["uv_index"]     = serialized(String(g_weather.uv_index, 1));
    doc["fetched_ms"]   = g_weather.fetched_ms;
    doc["lat_set"]      = (g_config.latitude != 0.0f || g_config.longitude != 0.0f);
    doc["last_error"]   = Weather::_lastError;
    String out;
    serializeJson(doc, out);
    _server.send(200, "application/json", out);
}

static void handleWeatherFetch() {
    if (g_config.latitude == 0.0f && g_config.longitude == 0.0f) {
        _server.send(400, "application/json", "{\"ok\":false,\"msg\":\"No location set\"}");
        return;
    }
    Weather::requestFetch();  // picked up by taskConnectivity — avoids blocking loop()
    _server.send(200, "application/json", "{\"ok\":true}");
}

static void handleBuzzerTest() {
    String pattern = _server.arg("plain");
    pattern.trim();
    if      (pattern == "confirm") Buzzer::confirm();
    else if (pattern == "beep1")   Buzzer::beep(1, BUZZ_FREQ_ALERT);
    else if (pattern == "beep2")   Buzzer::beep(2, BUZZ_FREQ_ALERT);
    else if (pattern == "beep3")   Buzzer::beep(3, BUZZ_FREQ_ALERT);
    else if (pattern == "warning") Buzzer::beep(1, BUZZ_FREQ_WARN);
    else if (pattern == "alarm")   Buzzer::alarm();
    else if (pattern == "stop")    Buzzer::stop();
    _server.send(200, "application/json", "{\"ok\":true}");
}

static void handleReboot() {
    _server.send(200, "application/json", "{\"ok\":true}");
    delay(200);
    ESP.restart();
}

static void handleReset() {
    _server.send(200, "application/json", "{\"ok\":true}");
    delay(200);
    NvsConfig::factoryReset();
}

static void handleGetLog() {
    static char logBuf[8192];
    Logger::getJson(logBuf, sizeof(logBuf));
    _server.send(200, "application/json", logBuf);
}

static void handleI2cScan() {
    JsonDocument doc;
    JsonArray arr = doc["devices"].to<JsonArray>();
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) arr.add(addr);
    }
    doc["count"] = arr.size();
    String out;
    serializeJson(doc, out);
    _server.send(200, "application/json", out);
}

// ── Public API ────────────────────────────────────────────────

void WebUI::begin() {
    if (_started) return;
    _server.on("/",                  HTTP_GET,  handleRoot);
    _server.on("/api/config",        HTTP_GET,  handleGetConfig);
    _server.on("/api/config",        HTTP_POST, handlePostConfig);
    _server.on("/api/readings",      HTTP_GET,  handleGetReadings);
    _server.on("/api/weather",       HTTP_GET,  handleGetWeather);
    _server.on("/api/weather/fetch", HTTP_POST, handleWeatherFetch);
    _server.on("/api/buzzer/test",   HTTP_POST, handleBuzzerTest);
    _server.on("/api/reboot",        HTTP_POST, handleReboot);
    _server.on("/api/reset",         HTTP_POST, handleReset);
    _server.on("/api/log",           HTTP_GET,  handleGetLog);
    _server.on("/api/i2cscan",       HTTP_GET,  handleI2cScan);
    _server.begin();
    _started = true;
    Serial.printf("[WebUI] Started — http://%s/\n", WiFi.localIP().toString().c_str());
}

void WebUI::loop() {
    if (_started) _server.handleClient();
}
