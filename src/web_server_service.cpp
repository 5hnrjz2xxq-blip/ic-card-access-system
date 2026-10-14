#include "web_server_service.h"
#include "access_controller.h"
#include "storage_manager.h"
#include <WiFi.h>

WebServerService::WebServerService()
    : server_(80) {
}

WebServerService& WebServerService::instance() {
    static WebServerService inst;
    return inst;
}

void WebServerService::begin(uint16_t port) {
    if (started_) return;

    // 确保 lwIP/TCP-IP 栈已初始化（未设置 WiFi 模式时 lwIP 未启动，
    // 直接调用 WiFiServer::begin 会触发 tcpip_send_msg_wait_sem assert 崩溃）
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
        delay(100);
    }

    setupRoutes();
    server_.begin();
    started_ = true;
    Serial.printf("[Web] HTTP WebServer listening on port %d\n", port);
}

void WebServerService::update() {
    if (started_) {
        server_.handleClient();
    }
}

void WebServerService::setupRoutes() {
    // 允许跨域访问
    server_.enableCORS(true);

    server_.on("/", HTTP_GET, [this]() { handleRoot(); });
    server_.on("/api/unlock", HTTP_ANY, [this]() { handleUnlock(); });
    server_.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server_.on("/api/logs", HTTP_GET, [this]() { handleLogs(); });
    server_.on("/api/whitelist", HTTP_GET, [this]() { handleWhitelist(); });
    server_.on("/api/whitelist/add", HTTP_ANY, [this]() { handleAddCard(); });
    server_.on("/api/whitelist/del", HTTP_ANY, [this]() { handleDeleteCard(); });
}

void WebServerService::handleUnlock() {
    AccessController::instance().unlockRemote("HTTP/NFC");
    server_.send(200, "application/json", 
        "{\"status\":\"ok\",\"action\":\"unlocked\",\"duration_ms\":3000}");
}

void WebServerService::handleStatus() {
    bool isUnlocked = (AccessController::instance().getLockState() == LockState::UNLOCKED);
    String json = "{";
    json += "\"lockState\":\"" + String(isUnlocked ? "UNLOCKED" : "LOCKED") + "\",";
    json += "\"whitelistCount\":" + String(StorageManager::instance().getCardCount()) + ",";
    json += "\"logCount\":" + String(StorageManager::instance().getRecordCount()) + ",";
    json += "\"uptimeSec\":" + String(millis() / 1000);
    json += "}";
    server_.send(200, "application/json", json);
}

void WebServerService::handleWhitelist() {
    auto cards = StorageManager::instance().getAllCards();
    String json = "{\"cards\":[";
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i > 0) json += ",";
        json += "\"" + cards[i] + "\"";
    }
    json += "]}";
    server_.send(200, "application/json", json);
}

void WebServerService::handleLogs() {
    auto records = StorageManager::instance().getRecentRecords(15);
    String json = "{\"records\":[";
    for (size_t i = 0; i < records.size(); ++i) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"time\":\"" + String(records[i].timeStr) + "\",";
        json += "\"uid\":\"" + String(records[i].uid) + "\",";
        json += "\"result\":" + String((int)records[i].result) + ",";
        json += "\"source\":\"" + String(records[i].source) + "\"";
        json += "}";
    }
    json += "]}";
    server_.send(200, "application/json", json);
}

void WebServerService::handleAddCard() {
    String uid = server_.arg("uid");
    if (uid.isEmpty()) {
        server_.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing uid param\"}");
        return;
    }
    bool ok = StorageManager::instance().addCard(uid);
    server_.send(200, "application/json", 
        ok ? "{\"status\":\"ok\",\"action\":\"added\"}" : "{\"status\":\"error\",\"message\":\"Capacity full or invalid\"}");
}

void WebServerService::handleDeleteCard() {
    String uid = server_.arg("uid");
    if (uid.isEmpty()) {
        server_.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing uid param\"}");
        return;
    }
    bool ok = StorageManager::instance().removeCard(uid);
    server_.send(200, "application/json", 
        ok ? "{\"status\":\"ok\",\"action\":\"removed\"}" : "{\"status\":\"error\",\"message\":\"Card not found\"}");
}

void WebServerService::handleRoot() {
    // 内置轻量响应式移动端控制台
    static const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>智能IC卡门禁管理</title>
<style>
body{font-family:-apple-system,sans-serif;margin:0;padding:16px;background:#0d1117;color:#c9d1d9}
.card{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:16px;margin-bottom:14px}
h2{margin-top:0;font-size:18px;color:#58a6ff}
button{width:100%;padding:14px;background:#238636;color:#fff;border:none;border-radius:8px;font-size:16px;font-weight:bold;cursor:pointer}
button:active{background:#2ea043}
.badge{display:inline-block;padding:4px 8px;border-radius:4px;font-size:12px;background:#21262d}
.log-item{font-size:13px;border-bottom:1px solid #21262d;padding:8px 0;display:flex;justify-content:space-between}
.pass{color:#3fb950}.reject{color:#f85149}
</style>
</head>
<body>
<div class="card">
  <h2>🚪 门禁控制</h2>
  <p>状态: <span id="st" class="badge">加载中...</span></p>
  <button onclick="unlock()">一键开门 (开锁3秒)</button>
</div>
<div class="card">
  <h2>📋 最近进出刷卡记录</h2>
  <div id="logs">加载中...</div>
</div>
<script>
function refresh(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('st').innerText = d.lockState === 'UNLOCKED' ? '已开锁 (UNLOCKED)' : '闭锁中 (LOCKED)';
    document.getElementById('st').style.color = d.lockState === 'UNLOCKED' ? '#3fb950' : '#8b949e';
  });
  fetch('/api/logs').then(r=>r.json()).then(d=>{
    let h = '';
    d.records.forEach(r=>{
      let cls = (r.result===0)?'pass':'reject';
      let tag = (r.result===0)?'通过':'拒绝';
      h += `<div class="log-item"><span>${r.time} [${r.source}] <b>${r.uid}</b></span><span class="${cls}">${tag}</span></div>`;
    });
    document.getElementById('logs').innerHTML = h || '暂无记录';
  });
}
function unlock(){
  fetch('/api/unlock').then(()=>setTimeout(refresh, 500));
}
setInterval(refresh, 2000);
refresh();
</script>
</body>
</html>
)rawliteral";

    server_.send_P(200, "text/html", PAGE);
}
