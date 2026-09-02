#include "ws_manager.h"
#include "cmd_parser.h"
#include "motion_queue.h"
#include "config.h"
#include "buzzer.h"
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
      Serial.println("[WS] Phone Client connected");
#endif
      clientConnected = true;
      markHeartbeat();
      buzzerConnectedSignal(); // Beep when phone app connects to WebSocket!
      break;

    case WS_EVT_DISCONNECT:
#ifdef DEBUG
      Serial.println("[WS] Phone Client disconnected");
#endif
      disconnectTime = millis();
      clientConnected = false;
      buzzerDisconnectedSignal(); // Beep when phone app disconnects!
      break;

    case WS_EVT_DATA: {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (info->opcode == WS_TEXT) {
        markHeartbeat(); // Any incoming WS frame keeps heartbeat alive
        if (info->final && info->index == 0 && info->len == len) {
          data[len] = 0; // null terminate safely
          parseCommand((const char*)data, len);
        } else {
          // Reassemble fragmented frames safely
          static char wsRxBuf[1024];
          if (info->index == 0) {
            wsRxBuf[0] = '\0';
          }
          if (info->index + len < sizeof(wsRxBuf)) {
            memcpy(wsRxBuf + info->index, data, len);
            wsRxBuf[info->index + len] = '\0';
          }
          if (info->index + len == info->len) {
            parseCommand(wsRxBuf, info->len);
          }
        }
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
      markHeartbeat(); 
  }
}
