#pragma once

// =====================================================================
// WIFI_SERVER.H — WiFi Access Point + AsyncWebServer + WebSocket
// =====================================================================
// The ESP32 hosts its own WiFi hotspot.  Connect to "BipedBot" and
// open http://192.168.4.1 in any browser to get the controller UI.
// =====================================================================

// Start the WiFi AP, web server, and WebSocket endpoint.
// Call once from setup().
void initWiFi();

// Push current state, speed, servo positions and IMU data to every
// connected WebSocket client.
void broadcastStatus();

// Call from loop() every iteration:
//  - reads the IMU and broadcasts status every 100 ms
//  - releases timed-out WebSocket clients
void tickWifi();
