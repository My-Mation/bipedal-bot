#include "ws_manager.h"
#include "cmd_parser.h"
#include "motion_queue.h"
#include "config.h"
#include <Arduino.h>

static AsyncWebSocket ws("/ws");
static unsigned long lastHeartbeat = 0;
static unsigned long disconnectTime = 0;
static bool clientConnected = false;

void wsSendAll(const char* msg) {
  if (ws.count() > 0) {
    ws.textAll(msg);
  }
}

void markHeartbeat() {
  lastHeartbeat = millis();
}

bool isHeartbeatLost() {
  if (ws.count() == 0 || !clientConnected) {
    return false; // Handled by disconnect grace period
  }
  return (millis() - lastHeartbeat) > HEARTBEAT_TIMEOUT_MS;
}

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
#ifdef DEBUG
      Serial.println("[WS] Client connected");
#endif
      clientConnected = true;
      markHeartbeat();
      break;

    case WS_EVT_DISCONNECT:
#ifdef DEBUG
      Serial.println("[WS] Client disconnected");
#endif
      disconnectTime = millis();
      clientConnected = false;
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0; // null terminate safely
        parseCommand((const char*)data, len);
      }
      break;
    }
    default:
      break;
  }
}

void initWebSocket(AsyncWebServer& server) {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
}

void tickWebSocket() {
  ws.cleanupClients();

  if (!clientConnected && disconnectTime > 0) {
    if (millis() - disconnectTime > DISCONNECT_GRACE_MS) {
#ifdef DEBUG
      Serial.println("[WS] Disconnect grace period expired -> ESTOP");
#endif
      motionEStop();
      disconnectTime = 0; // Prevent re-triggering
    }
  }

  if (isHeartbeatLost()) {
#ifdef DEBUG
      Serial.println("[WS] Heartbeat lost -> ESTOP");
#endif
      motionEStop();
      // To prevent continuous triggering, fake a heartbeat or set a flag,
      // but since we keep checking, ESTOP will just repeatedly run until heartbeat resumes or disconnects.
      // Better to just update heartbeat to now so it triggers again in 3s if still no data.
      markHeartbeat(); 
  }
}
