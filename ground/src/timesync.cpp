#include "timesync.h"
#include <Arduino.h>
#include "config.h"
#include "prefs.h"
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>

static WebServer server(80);
static bool ap_running     = false;
static bool synced         = false;
static bool stop_requested = false; // set inside HTTP handler, actioned from update()

// ---------------------------------------------------------------------------
// HTML page served to the phone browser. Uses XMLHttpRequest for broad
// compatibility (works on older Android / iOS browsers that may lack fetch()).
// ---------------------------------------------------------------------------
static const char HTML_PAGE[] PROGMEM =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<style>"
    "body{font-family:sans-serif;text-align:center;padding:24px;max-width:420px;margin:0 auto}"
    "h2{margin-bottom:8px}"
    "p{color:#555;margin-bottom:24px}"
    "button{font-size:1.2em;padding:16px 0;width:100%;background:#1565C0;"
           "color:#fff;border:none;border-radius:8px;cursor:pointer}"
    "button:active{background:#0D47A1}"
    "#s{margin-top:20px;padding:12px;background:#f5f5f5;border-radius:6px;"
       "font-weight:bold;min-height:1.4em}"
    "</style></head><body>"
    "<h2>&#x1F680; Rocket Ground Station</h2>"
    "<p>Syncs the ground station clock with your phone's current time.</p>"
    "<button onclick='go()'>Set Time to Now</button>"
    "<div id='s'>Ready</div>"
    "<script>"
    "function go(){"
    "document.getElementById('s').textContent='Syncing...';"
    "var t=Math.floor(Date.now()/1000);"
    "var tz=new Date().getTimezoneOffset();"
    "var x=new XMLHttpRequest();"
    "x.onload=function(){document.getElementById('s').textContent=x.responseText;};"
    "x.onerror=function(){document.getElementById('s').textContent='Error \u2014 try again';};"
    "x.open('GET','/settime?t='+t+'&tz='+tz);"
    "x.send();"
    "}"
    "</script></body></html>";

static void handle_root() {
    server.send_P(200, "text/html", HTML_PAGE);
}

static void handle_settime() {
    if (!server.hasArg("t")) {
        server.send(400, "text/plain", "Missing t parameter");
        return;
    }
    time_t epoch = (time_t)server.arg("t").toInt();
    struct timeval tv = { epoch, 0 };
    settimeofday(&tv, nullptr);
    if (server.hasArg("tz")) {
        int16_t tz_minutes = (int16_t)server.arg("tz").toInt();
        prefs_set_tz_offset(tz_minutes);
        Serial.printf("[timesync] TZ offset set to %d min\n", (int)tz_minutes);
    }
    synced = true;
    stop_requested = true; // defer AP stop to timesync_update() — safe outside handler
    server.send(200, "text/plain", "Time set \xe2\x80\x94 you can close this page");
    Serial.printf("[timesync] Time set to epoch %lu\n", (unsigned long)epoch);
}

// ---------------------------------------------------------------------------

static void start_ap() {
    WiFi.softAPConfig(
        IPAddress(10, 0, 0, 42),
        IPAddress(10, 0, 0,  1),
        IPAddress(255, 255, 255, 0)
    );
    WiFi.softAP("WaterRocketGS", nullptr, ESPNOW_CHANNEL);
    server.on("/",        handle_root);
    server.on("/settime", handle_settime);
    server.begin();
    ap_running = true;
    Serial.println("[timesync] AP started — SSID: WaterRocketGS  IP: 10.0.0.42");
}

void timesync_init() {
    WiFi.mode(WIFI_AP_STA);  // must be set before comms_init() so ESP-NOW keeps AP_STA
    start_ap();
}

void timesync_stop() {
    server.stop();
    WiFi.softAPdisconnect(true);
    ap_running = false;
    Serial.println("[timesync] AP stopped");
}

void timesync_restart() {
    if (ap_running) return;
    start_ap();
}

void timesync_update() {
    if (!ap_running) return;
    server.handleClient();
    if (stop_requested) {
        stop_requested = false;
        timesync_stop();
    }
}

bool timesync_is_running() { return ap_running; }
bool timesync_is_synced()  { return synced; }

void timesync_status_line(char* buf, size_t len) {
    if (synced && !ap_running) {
        time_t now = time(NULL);
        struct tm* t = localtime(&now);
        strftime(buf, len, "%Y-%m-%d %H:%M:%S", t);
    } else {
        snprintf(buf, len, "SSID: WaterRocketGS  10.0.0.42");
    }
}
