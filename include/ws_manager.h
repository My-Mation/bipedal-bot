#pragma once
#include <ESPAsyncWebServer.h>

void initWebSocket(AsyncWebServer& server);
void tickWebSocket();
void wsSendAll(const char* msg);

// Heartbeat handling
void markHeartbeat();
bool isHeartbeatLost();
