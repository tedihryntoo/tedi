
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

WebServer server(80);
Preferences preferences;

const char* AP_SSID = "ESP32-WiFi-Manager";
const char* AP_PASSWORD = "ganti-password-ini";

String savedSSID;
String savedPassword;

bool pendingConnect = false;
bool pendingAPOnly = false;
bool pendingRestart = false;

// =============================
// HALAMAN WEB LOKAL ESP32
// =============================

const char LOCAL_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Local Config</title>
<style>
*{box-sizing:border-box;font-family:Arial}
body{margin:0;background:#0b1220;color:#e5e7eb}
main{max-width:680px;margin:auto;padding:20px}
.card{background:#111c2e;border:1px solid #26364f;
border-radius:14px;padding:18px;margin-bottom:15px}
input,select{width:100%;padding:13px;background:#0b1220;
color:white;border:1px solid #334155;border-radius:8px}
label{display:block;margin:14px 0 7px}
button{width:100%;padding:13px;border:0;border-radius:8px;
background:#2563eb;color:white;font-weight:bold;margin-top:10px}
.secondary{background:#334155}
.danger{background:#991b1b}
p{color:#94a3b8}
#message{color:#38bdf8}
</style>
</head>
<body>
<main>
<h2>ESP32 WiFi Manager</h2>

<div class="card">
<h3>Status Koneksi</h3>
<p id="status">Memuat...</p>
<p id="ip">IP: -</p>
<button class="secondary" onclick="refreshStatus()">Refresh Status</button>
</div>

<div class="card">
<h3>Pengaturan WiFi</h3>

<label>Pilih jaringan WiFi</label>
<select id="ssid">
<option value="">Scan WiFi terlebih dahulu</option>
</select>

<button class="secondary" onclick="scanWiFi()">Scan WiFi</button>

<label>SSID manual (opsional)</label>
<input id="manualSSID" placeholder="Nama WiFi">

<label>Password WiFi</label>
<input id="password" type="password" placeholder="Password">

<button onclick="saveWiFi()">Simpan & Hubungkan</button>

<p id="message"></p>
</div>

<div class="card">
<h3>Pengaturan Mode</h3>
<button class="secondary" onclick="backToAP()">Kembali ke Mode AP</button>
<button class="secondary" onclick="reconnect()">Hubungkan WiFi Tersimpan</button>
<button class="danger" onclick="restartESP()">Restart ESP32</button>
</div>

</main>

<script>
const $ = id => document.getElementById(id);

async function refreshStatus() {
  try {
    const data = await (await fetch('/api/status')).json();

    $('status').textContent = data.connected
      ? 'Terhubung ke ' + data.ssid
      : 'Tidak terhubung ke router';

    $('ip').textContent =
      'IP AP: ' + data.ap_ip + ' | IP Router: ' + (data.sta_ip || '-');
  } catch(e) {
    $('status').textContent = 'Gagal membaca status';
  }
}

async function scanWiFi() {
  $('message').textContent = 'Memindai jaringan...';

  try {
    const data = await (await fetch('/api/scan')).json();
    $('ssid').innerHTML = '';

    data.networks.forEach(network => {
      const option = document.createElement('option');
      option.value = network.ssid;
      option.textContent =
        network.ssid + ' (' + network.rssi + ' dBm)';
      $('ssid').appendChild(option);
    });

    $('message').textContent =
      data.networks.length + ' jaringan ditemukan.';
  } catch(e) {
    $('message').textContent = 'Scan gagal.';
  }
}

async function saveWiFi() {
  const ssid = $('manualSSID').value.trim() || $('ssid').value;
  const password = $('password').value;

  if (!ssid) {
    $('message').textContent = 'Pilih atau masukkan SSID.';
    return;
  }

  const body = new URLSearchParams({ssid, password});

  try {
    const response = await fetch('/api/save', {
      method: 'POST',
      headers: {'Content-Type':'application/x-www-form-urlencoded'},
      body
    });

    $('message').textContent = await response.text();
    setTimeout(refreshStatus, 2500);
  } catch(e) {
    $('message').textContent = 'Gagal mengirim konfigurasi.';
  }
}

async function backToAP() {
  if (!confirm('Kembali ke Mode AP saja?')) return;

  try {
    $('message').textContent =
      await (await fetch('/api/ap', {method:'POST'})).text();
  } catch(e) {
    $('message').textContent = 'ESP32 sedang berpindah ke Mode AP.';
  }

  setTimeout(refreshStatus, 1500);
}

async function reconnect() {
  try {
    $('message').textContent =
      await (await fetch('/api/reconnect', {method:'POST'})).text();
  } catch(e) {
    $('message').textContent = 'Mencoba menghubungkan ulang.';
  }
}

async function restartESP() {
  if (!confirm('Restart ESP32?')) return;

  try {
    $('message').textContent =
      await (await fetch('/api/restart', {method:'POST'})).text();
  } catch(e) {
    $('message').textContent = 'ESP32 sedang restart.';
  }
}

refreshStatus();
setInterval(refreshStatus, 7000);
</script>
</body>
</html>
)HTML";

// =============================
// PENYIMPANAN KONFIGURASI
// =============================

void loadCredentials() {
  preferences.begin("wifi-config", true);
  savedSSID = preferences.getString("ssid", "");
  savedPassword = preferences.getString("password", "");
  preferences.end();
}

void saveCredentials(String ssid, String password) {
  preferences.begin("wifi-config", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();

  savedSSID = ssid;
  savedPassword = password;
}

// =============================
// MODE AP + STA
// =============================

void startAPAndSTA() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.println("Mode AP + STA aktif");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

// =============================
// MODE AP SAJA
// =============================

void startAPOnly() {
  WiFi.disconnect(true, false);
  delay(300);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  Serial.println("Mode AP saja aktif");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

// =============================
// KONEKSI WIFI
// =============================

bool connectSavedWiFi(unsigned long timeout = 15000) {
  if (savedSSID.length() == 0) return false;

  startAPAndSTA();

  WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < timeout) {
    delay(300);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi berhasil terhubung!");
    Serial.print("IP Router: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WiFi gagal. AP tetap aktif.");
  return false;
}

// =============================
// ROUTE WEB
// =============================

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", LOCAL_PAGE);
}

void handleStatus() {
  bool connected = WiFi.status() == WL_CONNECTED;

  String json = "{";
  json += "\"connected\":";
  json += connected ? "true" : "false";
  json += ",\"ssid\":\"";
  json += connected ? WiFi.SSID() : "";
  json += "\",\"sta_ip\":\"";
  json += connected ? WiFi.localIP().toString() : "";
  json += "\",\"ap_ip\":\"";
  json += WiFi.softAPIP().toString();
  json += "\",\"rssi\":";
  json += connected ? String(WiFi.RSSI()) : "0";
  json += "}";

  server.send(200, "application/json", json);
}

void handleScan() {
  int count = WiFi.scanNetworks();

  String json = "{\"networks\":[";

  bool first = true;

  for (int i = 0; i < count; i++) {
    String ssid = WiFi.SSID(i);

    if (ssid.length() == 0) continue;

    if (!first) json += ",";

    json += "{\"ssid\":\"" + ssid + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"secure\":";
    json += WiFi.encryptionType(i) == WIFI_AUTH_OPEN
            ? "false" : "true";
    json += "}";

    first = false;
  }

  json += "]}";

  WiFi.scanDelete();

  server.send(200, "application/json", json);
}

void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  if (ssid.length() == 0) {
    server.send(400, "text/plain", "SSID tidak boleh kosong.");
    return;
  }

  if (password.length() > 63) {
    server.send(400, "text/plain", "Password terlalu panjang.");
    return;
  }

  saveCredentials(ssid, password);
  pendingConnect = true;

  server.send(200, "text/plain",
    "WiFi tersimpan. ESP32 sedang mencoba koneksi baru.");
}

void handleBackAP() {
  pendingAPOnly = true;

  server.send(200, "text/plain",
    "Perintah diterima. ESP32 kembali ke Mode AP.");
}

void handleReconnect() {
  pendingConnect = true;

  server.send(200, "text/plain",
    "Mencoba menghubungkan WiFi tersimpan.");
}

void handleRestart() {
  pendingRestart = true;

  server.send(200, "text/plain",
    "ESP32 akan restart.");
}

// =============================
// SETUP
// =============================

void setup() {
  Serial.begin(115200);

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  loadCredentials();

  startAPAndSTA();

  if (savedSSID.length() > 0) {
    connectSavedWiFi();
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/save", HTTP_POST, handleSave);
  server.on("/api/ap", HTTP_POST, handleBackAP);
  server.on("/api/reconnect", HTTP_POST, handleReconnect);
  server.on("/api/restart", HTTP_POST, handleRestart);

  server.begin();

  Serial.println("Web server aktif.");
  Serial.println("Buka http://192.168.4.1");
}

// =============================
// LOOP
// =============================

void loop() {
  server.handleClient();

  if (pendingConnect) {
    pendingConnect = false;
    connectSavedWiFi(20000);
  }

  if (pendingAPOnly) {
    pendingAPOnly = false;
    startAPOnly();
  }

  if (pendingRestart) {
    delay(300);
    ESP.restart();
  }

  delay(2);
}
