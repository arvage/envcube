// ============================================================
//  EnvCube — Web UI
//  Serves a single-page config + sensor console on port 80.
//  Endpoints:
//    GET  /              → HTML app
//    GET  /api/config    → current config as JSON
//    POST /api/config    → save config (room, wifi, mqtt)
//    GET  /api/readings  → live sensor readings as JSON
//    POST /api/reboot    → restart device
// ============================================================

#include "web_ui.h"
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "../storage/nvs_config.h"
#include "../alerts/alert_engine.h"

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
header{background:#1a1d27;padding:16px 20px;border-bottom:1px solid #2a2d3a}
header h1{font-size:1.2rem;color:#7eb8f7;letter-spacing:.5px}
header p{font-size:.75rem;color:#666;margin-top:3px}
nav{display:flex;background:#1a1d27;border-bottom:1px solid #2a2d3a}
nav button{flex:1;padding:12px 8px;background:none;border:none;color:#888;cursor:pointer;font-size:.85rem;border-bottom:2px solid transparent;transition:all .2s}
nav button.active{color:#7eb8f7;border-bottom-color:#7eb8f7}
.tab{display:none;padding:20px;max-width:600px;margin:0 auto}
.tab.active{display:block}
.field{margin-bottom:14px}
label{display:block;font-size:.78rem;color:#888;margin-bottom:4px;text-transform:uppercase;letter-spacing:.4px}
input[type=text],input[type=password],input[type=number]{width:100%;padding:9px 12px;background:#1a1d27;border:1px solid #2a2d3a;border-radius:6px;color:#e0e0e0;font-size:.95rem}
input:focus{outline:none;border-color:#7eb8f7}
input:disabled{color:#555;cursor:default}
.row{display:flex;align-items:center;gap:10px;margin-bottom:14px}
.row input[type=checkbox]{width:18px;height:18px;accent-color:#7eb8f7;cursor:pointer}
.row label{margin:0;text-transform:none;font-size:.9rem;color:#ccc;cursor:pointer}
.actions{margin-top:20px;display:flex;align-items:center;gap:10px;flex-wrap:wrap}
.btn{padding:9px 20px;border-radius:6px;border:none;cursor:pointer;font-size:.88rem;font-weight:600;transition:opacity .15s}
.btn-primary{background:#7eb8f7;color:#0f1117}
.btn-danger{background:#c0392b;color:#fff}
.btn:hover{opacity:.82}
.toast{font-size:.83rem;padding:6px 10px;border-radius:5px;display:none}
.toast.ok{background:#1a3a24;color:#5cc97a}
.toast.err{background:#3a1a1a;color:#e05c5c}
table{width:100%;border-collapse:collapse;font-size:.88rem}
th{text-align:left;padding:8px 10px;color:#555;font-weight:500;font-size:.75rem;text-transform:uppercase;letter-spacing:.4px;border-bottom:1px solid #2a2d3a}
td{padding:10px;border-bottom:1px solid #1e2130;vertical-align:middle}
td.label{color:#888;width:40%}
td.value{font-family:monospace;font-size:.95rem;font-weight:600}
td.status{width:20%;text-align:right}
.badge{display:inline-block;padding:2px 8px;border-radius:4px;font-size:.72rem;font-weight:700;letter-spacing:.3px}
.badge-ok{background:#1a3a24;color:#5cc97a}
.badge-err{background:#3a1a1a;color:#e05c5c}
.badge-na{background:#1e2130;color:#555}
.hint{font-size:.75rem;color:#555;margin-top:4px}
.ts{font-size:.72rem;color:#444;margin-top:10px;text-align:right}
.section-title{font-size:.72rem;color:#555;text-transform:uppercase;letter-spacing:.5px;margin:18px 0 10px}
</style>
</head>
<body>
<header>
  <h1>&#9672; EnvCube</h1>
  <p id="hdr-sub">Connecting...</p>
</header>
<nav>
  <button class="active" onclick="show('console',this)">Console</button>
  <button onclick="show('device',this)">Device</button>
  <button onclick="show('wifi',this)">WiFi</button>
  <button onclick="show('mqtt',this)">MQTT</button>
</nav>

<!-- Console -->
<div id="tab-console" class="tab active">
  <table>
    <thead><tr><th>Sensor</th><th class="value">Reading</th><th style="text-align:right">Status</th></tr></thead>
    <tbody>
      <tr><td class="label">Temperature</td><td class="value" id="r-temp">—</td><td class="status" id="s-temp"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="label">Humidity</td><td class="value" id="r-hum">—</td><td class="status" id="s-hum"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="label">Pressure</td><td class="value" id="r-pres">—</td><td class="status" id="s-pres"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="label">CO&#x2082;</td><td class="value" id="r-co2">—</td><td class="status" id="s-co2"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="label">VOC Index</td><td class="value" id="r-voc">—</td><td class="status" id="s-voc"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="label">NOx Index</td><td class="value" id="r-nox">—</td><td class="status" id="s-nox"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="label">PM2.5</td><td class="value" id="r-pm25">—</td><td class="status" id="s-pm25"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="label">PM10</td><td class="value" id="r-pm10">—</td><td class="status" id="s-pm10"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="label">Noise</td><td class="value" id="r-noise">—</td><td class="status" id="s-noise"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="label">Light</td><td class="value" id="r-lux">—</td><td class="status" id="s-lux"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="label">Smoke (raw)</td><td class="value" id="r-smoke">—</td><td class="status" id="s-smoke"><span class="badge badge-na">—</span></td></tr>
      <tr><td class="label">Presence</td><td class="value" id="r-presence">—</td><td class="status" id="s-presence"><span class="badge badge-na">—</span></td></tr>
    </tbody>
  </table>
  <p class="ts" id="ts">Not yet updated</p>
</div>

<!-- Device -->
<div id="tab-device" class="tab">
  <div class="field">
    <label>Room Name</label>
    <input type="text" id="room_name" maxlength="31" placeholder="e.g. Kitchen">
  </div>
  <div class="field">
    <label>Cube ID</label>
    <input type="text" id="cube_id" disabled>
  </div>
  <div class="field">
    <label>IP Address</label>
    <input type="text" id="ip_addr" disabled>
  </div>
  <div class="actions">
    <button class="btn btn-primary" onclick="save()">Save</button>
    <button class="btn btn-danger" onclick="doReboot()">Reboot</button>
    <span class="toast" id="toast-d"></span>
  </div>
</div>

<!-- WiFi -->
<div id="tab-wifi" class="tab">
  <div class="field">
    <label>SSID</label>
    <input type="text" id="wifi_ssid" maxlength="63">
  </div>
  <div class="field">
    <label>Password</label>
    <input type="password" id="wifi_password" maxlength="63" placeholder="Leave blank to keep current">
    <p class="hint">Device will reboot to apply new WiFi credentials.</p>
  </div>
  <div class="actions">
    <button class="btn btn-primary" onclick="save()">Save &amp; Reboot</button>
    <span class="toast" id="toast-w"></span>
  </div>
</div>

<!-- MQTT -->
<div id="tab-mqtt" class="tab">
  <div class="field">
    <label>Broker Host / IP</label>
    <input type="text" id="mqtt_host" maxlength="63" placeholder="e.g. 192.168.1.100">
  </div>
  <div class="field">
    <label>Port</label>
    <input type="number" id="mqtt_port" min="1" max="65535" style="max-width:120px">
  </div>
  <div class="row">
    <input type="checkbox" id="mqtt_enabled">
    <label for="mqtt_enabled">Enable MQTT publishing</label>
  </div>
  <div class="actions">
    <button class="btn btn-primary" onclick="save()">Save</button>
    <span class="toast" id="toast-m"></span>
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
}
function badge(ok){return ok?'<span class="badge badge-ok">OK</span>':'<span class="badge badge-err">ERR</span>';}
function set(vid,val,sid,ok){
  document.getElementById(vid).textContent=val;
  document.getElementById(sid).innerHTML=badge(ok);
}
function loadReadings(){
  fetch('/api/readings').then(function(r){return r.json();}).then(function(d){
    set('r-temp', d.thermal_ok?d.temperature_c.toFixed(1)+' °C':'—','s-temp',d.thermal_ok);
    set('r-hum',  d.thermal_ok?d.humidity_rh.toFixed(1)+' %':'—','s-hum',d.thermal_ok);
    set('r-pres', d.thermal_ok?d.pressure_hpa.toFixed(1)+' hPa':'—','s-pres',d.thermal_ok);
    set('r-co2',  d.co2_ok?d.co2_ppm+' ppm':'—','s-co2',d.co2_ok);
    set('r-voc',  d.aq_ok?String(d.voc_index):'—','s-voc',d.aq_ok);
    set('r-nox',  d.aq_ok?String(d.nox_index):'—','s-nox',d.aq_ok);
    set('r-pm25', d.pm_ok?d.pm2_5+' μg/m³':'—','s-pm25',d.pm_ok);
    set('r-pm10', d.pm_ok?d.pm10+' μg/m³':'—','s-pm10',d.pm_ok);
    set('r-noise',d.noise_ok?d.noise_db.toFixed(1)+' dBA':'—','s-noise',d.noise_ok);
    set('r-lux',  d.aq_ok?d.lux+' lx':'—','s-lux',d.aq_ok);
    set('r-smoke',d.smoke_ok?String(d.smoke_raw):'—','s-smoke',d.smoke_ok);
    var presVal=d.presence_ok?(d.presence?'Yes ('+d.presence_cm+' cm)':'No'):'—';
    set('r-presence',presVal,'s-presence',d.presence_ok);
    document.getElementById('ts').textContent='Updated: '+new Date().toLocaleTimeString();
  }).catch(function(){});
}
function loadConfig(){
  fetch('/api/config').then(function(r){return r.json();}).then(function(d){
    document.getElementById('room_name').value=d.room_name||'';
    document.getElementById('cube_id').value='#'+d.cube_id;
    document.getElementById('ip_addr').value=d.ip_addr||'';
    document.getElementById('wifi_ssid').value=d.wifi_ssid||'';
    document.getElementById('mqtt_host').value=d.mqtt_host||'';
    document.getElementById('mqtt_port').value=d.mqtt_port||1883;
    document.getElementById('mqtt_enabled').checked=!!d.mqtt_enabled;
    document.getElementById('hdr-sub').textContent=(d.room_name||'EnvCube')+' · '+d.ip_addr;
  }).catch(function(){});
}
function showToast(id,ok,msg){
  var el=document.getElementById(id);
  el.textContent=msg;
  el.className='toast '+(ok?'ok':'err');
  el.style.display='inline-block';
  if(ok) setTimeout(function(){el.style.display='none';},3000);
}
function save(){
  var body={
    room_name:document.getElementById('room_name').value,
    wifi_ssid:document.getElementById('wifi_ssid').value,
    wifi_password:document.getElementById('wifi_password').value,
    mqtt_host:document.getElementById('mqtt_host').value,
    mqtt_port:parseInt(document.getElementById('mqtt_port').value)||1883,
    mqtt_enabled:document.getElementById('mqtt_enabled').checked
  };
  var toastMap={console:'toast-d',device:'toast-d',wifi:'toast-w',mqtt:'toast-m'};
  var tid=toastMap[_tab]||'toast-d';
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
    .then(function(r){return r.json();}).then(function(d){
      if(d.reboot){showToast(tid,true,'Saved — rebooting…');setTimeout(function(){loadConfig();},5000);}
      else{showToast(tid,d.ok,'Saved');loadConfig();}
    }).catch(function(){showToast(tid,false,'Error');});
}
function doReboot(){
  if(!confirm('Reboot EnvCube?'))return;
  fetch('/api/reboot',{method:'POST'}).finally(function(){
    setTimeout(function(){loadConfig();loadReadings();},5000);
  });
}
loadConfig();
loadReadings();
setInterval(loadReadings,3000);
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
    doc["room_name"]    = g_config.room_name;
    doc["cube_id"]      = g_config.cube_id;
    doc["ip_addr"]      = WiFi.localIP().toString();
    doc["wifi_ssid"]    = g_config.wifi_ssid;
    doc["mqtt_host"]    = g_config.mqtt_host;
    doc["mqtt_port"]    = g_config.mqtt_port;
    doc["mqtt_enabled"] = g_config.mqtt_enabled;
    String out;
    serializeJson(doc, out);
    _server.send(200, "application/json", out);
}

static void handlePostConfig() {
    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, _server.arg("plain"))) {
        _server.send(400, "application/json", "{\"ok\":false}");
        return;
    }

    bool needReboot = false;

    if (doc["room_name"].is<const char*>()) {
        strlcpy(g_config.room_name, doc["room_name"].as<const char*>(),
                sizeof(g_config.room_name));
    }

    const char* newSsid = doc["wifi_ssid"].is<const char*>()
                          ? doc["wifi_ssid"].as<const char*>() : "";
    if (strlen(newSsid) > 0) {
        strlcpy(g_config.wifi_ssid, newSsid, sizeof(g_config.wifi_ssid));
        const char* newPass = doc["wifi_password"].is<const char*>()
                              ? doc["wifi_password"].as<const char*>() : "";
        if (strlen(newPass) > 0) {
            strlcpy(g_config.wifi_password, newPass, sizeof(g_config.wifi_password));
        }
        needReboot = true;
    }

    if (doc["mqtt_host"].is<const char*>()) {
        strlcpy(g_config.mqtt_host, doc["mqtt_host"].as<const char*>(),
                sizeof(g_config.mqtt_host));
    }
    if (doc["mqtt_port"].is<int>()) {
        g_config.mqtt_port = (uint16_t)doc["mqtt_port"].as<int>();
    }
    if (doc["mqtt_enabled"].is<bool>()) {
        g_config.mqtt_enabled = doc["mqtt_enabled"].as<bool>();
    }

    NvsConfig::save();

    String resp = String("{\"ok\":true,\"reboot\":") + (needReboot ? "true" : "false") + "}";
    _server.send(200, "application/json", resp);

    if (needReboot) {
        delay(300);
        ESP.restart();
    }
}

static void handleGetReadings() {
    JsonDocument doc;
    doc["temperature_c"] = serialized(String(g_readings.temperature_c, 2));
    doc["humidity_rh"]   = serialized(String(g_readings.humidity_rh, 2));
    doc["pressure_hpa"]  = serialized(String(g_readings.pressure_hpa, 2));
    doc["thermal_ok"]    = g_readings.thermal_ok;
    doc["smoke_raw"]     = g_readings.smoke_raw;
    doc["smoke_ok"]      = g_readings.smoke_ok;
    doc["co2_ppm"]       = g_readings.co2_ppm;
    doc["co2_ok"]        = g_readings.co2_ok;
    doc["voc_index"]     = g_readings.voc_index;
    doc["nox_index"]     = g_readings.nox_index;
    doc["lux"]           = g_readings.lux;
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

static void handleReboot() {
    _server.send(200, "application/json", "{\"ok\":true}");
    delay(200);
    ESP.restart();
}

// ── Public API ────────────────────────────────────────────────

void WebUI::begin() {
    if (_started) return;
    _server.on("/",            HTTP_GET,  handleRoot);
    _server.on("/api/config",  HTTP_GET,  handleGetConfig);
    _server.on("/api/config",  HTTP_POST, handlePostConfig);
    _server.on("/api/readings",HTTP_GET,  handleGetReadings);
    _server.on("/api/reboot",  HTTP_POST, handleReboot);
    _server.begin();
    _started = true;
    Serial.printf("[WebUI] Started — http://%s/\n", WiFi.localIP().toString().c_str());
}

void WebUI::loop() {
    if (_started) _server.handleClient();
}
