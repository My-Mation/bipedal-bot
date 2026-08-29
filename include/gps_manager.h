#pragma once
#include <Arduino.h>

// =====================================================================
// GPS_MANAGER.H — GY-GPS6MV2 interface (HardwareSerial 2)
// =====================================================================
// RX2 → ESP32 GPIO 16 (connects to GPS TX)
// TX2 → ESP32 GPIO 17 (connects to GPS RX)
// =====================================================================

struct GpsData {
  float latitude  = 0.0f;
  float longitude = 0.0f;
  float altitude  = 0.0f;
  float speed     = 0.0f;
  int   satellites= 0;
  bool  valid     = false;
};

extern GpsData gpsData;

void initGPS();
void readGPS();
