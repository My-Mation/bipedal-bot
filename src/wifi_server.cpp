// =====================================================================
// WIFI_SERVER.CPP — WiFi Access Point + Web Server + WebSocket
// =====================================================================
// The ESP32 creates a WiFi hotspot called "BipedBot".
// Connect to it, then open http://192.168.4.1 in any browser.
//
// WebSocket protocol (/ws):
//   Client → ESP32 : single ASCII char  ('f','b','l','r','s','h','i','+','-')
//   ESP32 → Client : JSON with state, speed, servo positions, and IMU data
//     {"state":"FORWARD","speed":8,
//      "pos":[2350,650,650,2350,2500,2500],
//      "pitch":2.1,"roll":-0.5,
//      "ax":0.12,"ay":-0.05,"az":9.78,
//      "gx":0.01,"gy":-0.02,"gz":0.00,
//      "imu":1}
// =====================================================================

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include "wifi_server.h"
#include "html_page.h"
#include "gait.h"
#include "servo_engine.h"
#include "commands.h"
#include "config.h"
#include "imu.h"

// ---------------------------------------------------------------
// readBatteryPercent — 16× oversampled ADC read on BAT_PIN (GPIO35)
// Voltage divider: Battery+ -> 30kΩ -> GPIO35 -> 10kΩ -> GND
// Returns 0–100 (clamped).
// ---------------------------------------------------------------
static int readBatteryPercent() {
  long raw = 0;
  for (int i = 0; i < BAT_SAMPLES; i++) raw += analogRead(BAT_PIN);
  raw /= BAT_SAMPLES;

  // Reconstruct actual battery voltage
  float v_gpio = (raw / BAT_ADC_MAX) * BAT_VREF;   // voltage at GPIO35
  float v_bat  = v_gpio / BAT_DIVIDER;              // actual battery voltage

  // Map to 0-100 %
  float pct = (v_bat - BAT_EMPTY_V) / (BAT_FULL_V - BAT_EMPTY_V) * 100.f;
  return (int)constrain(pct, 0.f, 100.f);
}

// ---------------------------------------------------------------
// Access Point credentials
// ---------------------------------------------------------------
#define AP_SSID "BipedBot"
#define AP_PASS "12345678"   // min 8 chars; set "" for open network

// ---------------------------------------------------------------
// Server and WebSocket instances
// ---------------------------------------------------------------
static AsyncWebServer server(80);
static AsyncWebSocket  ws("/ws");

// ---------------------------------------------------------------
// Map RobotState → human-readable label
// ---------------------------------------------------------------
static const char* stateName() {
  switch (state) {
    case STATE_WALK_FWD: return "FORWARD";
    case STATE_WALK_BWD: return "BACKWARD";
    case STATE_TURN_L:   return "TURN LEFT";
    case STATE_TURN_R:   return "TURN RIGHT";
    case STATE_HOME:     return "HOME";
    case STATE_SIT:      return "SIT";
    case STATE_IDLE:
    default:             return "IDLE";
  }
}

// ---------------------------------------------------------------
// broadcastStatus — build JSON and push to every connected client
// ---------------------------------------------------------------
void broadcastStatus() {
  if (ws.count() == 0) return;

  int bat = readBatteryPercent();

  char buf[384];
  snprintf(buf, sizeof(buf),
    "{"
      "\"state\":\"%s\","
      "\"speed\":%lu,"
      "\"pos\":[%d,%d,%d,%d,%d,%d],"
      "\"pitch\":%.1f,\"roll\":%.1f,"
      "\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,"
      "\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f,"
      "\"temp\":%.1f,\"imu\":%d,"
      "\"bat\":%d"
    "}",
    stateName(), stepInterval,
    currentPos[0], currentPos[1], currentPos[2],
    currentPos[3], currentPos[4], currentPos[5],
    imuData.pitch, imuData.roll,
    imuData.ax, imuData.ay, imuData.az,
    imuData.gx, imuData.gy, imuData.gz,
    imuData.temp, (int)imuData.ok,
    bat
  );

  ws.textAll(buf);
}

// ---------------------------------------------------------------
// WebSocket event handler
// ---------------------------------------------------------------
static void onWsEvent(AsyncWebSocket* /*srv*/, AsyncWebSocketClient* /*client*/,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {

    case WS_EVT_CONNECT:
      broadcastStatus();   // send current state to new client immediately
      break;

    case WS_EVT_DISCONNECT:
      // Safety: stop the robot if the browser disconnects
      enterState(STATE_IDLE);
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 &&
          info->len == len && info->opcode == WS_TEXT && len > 0) {
        handleCommand(static_cast<char>(data[0]));
        broadcastStatus();
      }
      break;
    }

    case WS_EVT_ERROR:
    case WS_EVT_PONG:
    default:
      break;
  }
}

// ---------------------------------------------------------------
// initWiFi — call once from setup()
// ---------------------------------------------------------------
void initWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS[0] ? AP_PASS : nullptr);

  Serial.println();
  Serial.print  ("WiFi AP : \"");
  Serial.print  (AP_SSID);
  Serial.println("\"");
  Serial.print  ("Password: ");
  Serial.println(AP_PASS[0] ? AP_PASS : "(open)");
  Serial.print  ("URL     : http://");
  Serial.println(WiFi.softAPIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", HTML_PAGE);
  });

  server.onNotFound([](AsyncWebServerRequest* req) {
    req->redirect("/");
  });

  server.begin();
  Serial.println("Web server started.");
}

// ---------------------------------------------------------------
// tickWifi — call every loop() iteration
//   • Reads IMU and broadcasts telemetry at 10 Hz (every 100 ms)
//   • Cleans up stale WebSocket connections
// ---------------------------------------------------------------
void tickWifi() {
  static unsigned long lastBcast = 0;
  unsigned long now = millis();

  if (now - lastBcast >= 100) {
    lastBcast = now;
    readIMU();           // sample MPU6050 (no-op if not detected)
    broadcastStatus();   // push JSON with IMU + battery to all clients
  }

  ws.cleanupClients();
}
