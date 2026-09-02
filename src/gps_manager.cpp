#include "gps_manager.h"
#include "config.h"
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>

GpsData gpsData;

static HardwareSerial gpsSerial(2); // ESP32 UART2
static TinyGPSPlus tinyGps;
static uint32_t rawBytesReceived = 0;
static unsigned long lastGpsDebugMs = 0;

void initGPS() {
  // Enable internal pullup on RX pin
  pinMode(GPS_RX_PIN, INPUT_PULLUP);

  // Initialize HardwareSerial(2) with RX=GPIO4, TX=GPIO17
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#ifdef DEBUG
  Serial.printf("[GPS] HardwareSerial(2) initialized: RX=GPIO%d, TX=GPIO%d @ %u baud\n",
                GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
#endif
}

void readGPS() {
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    Serial.write(c); // Print raw NMEA string directly to Serial Monitor
    tinyGps.encode(c);
    rawBytesReceived++;
  }

  // Update shared gpsData struct
  gpsData.valid = tinyGps.location.isValid();
  if (gpsData.valid) {
    gpsData.latitude  = tinyGps.location.lat();
    gpsData.longitude = tinyGps.location.lng();
  }
  if (tinyGps.altitude.isValid()) {
    gpsData.altitude = (float)tinyGps.altitude.meters();
  }
  if (tinyGps.speed.isValid()) {
    gpsData.speed = (float)tinyGps.speed.kmph();
  }
  if (tinyGps.satellites.isValid()) {
    gpsData.satellites = (int)tinyGps.satellites.value();
  }
  if (tinyGps.hdop.isValid()) {
    gpsData.hdop = (float)tinyGps.hdop.hdop();
  }

  // Print GPS Status to Serial Monitor every 2 seconds (2000ms)
  unsigned long now = millis();
  if (now - lastGpsDebugMs >= 2000) {
    lastGpsDebugMs = now;
    
    uint32_t processed = tinyGps.charsProcessed();
    uint32_t failedCheck = tinyGps.failedChecksum();

    if (rawBytesReceived < 10) {
      Serial.printf("\n[GPS] ⚠️ NO DATA (%u bytes). Check: 1) GPS TX -> ESP32 GPIO%d, 2) Power: GPS VCC -> 5V/VIN pin!\n",
                    rawBytesReceived, GPS_RX_PIN);
    } else if (failedCheck > 5 && processed < 10) {
      Serial.printf("\n[GPS] ⚠️ BAUD MISMATCH? Raw Bytes: %u | Failed Checksum: %u | Parsed OK: %u. Try setting GPS_BAUD = 38400 or 115200 in config.h!\n",
                    rawBytesReceived, failedCheck, processed);
    } else if (gpsData.valid) {
      Serial.printf("\n[GPS] ✅ FIX OK | Lat: %.6f | Lng: %.6f | Alt: %.1fm | Speed: %.1fkm/h | Sats: %d | HDOP: %.1f\n",
                    gpsData.latitude, gpsData.longitude, gpsData.altitude, gpsData.speed,
                    gpsData.satellites, gpsData.hdop);
    } else {
      Serial.printf("\n[GPS] 🛰️ SEARCHING SATS... | Sats: %d | Raw Bytes: %u | Valid NMEA Chars: %u (Move outdoors for sky view)\n",
                    gpsData.satellites, rawBytesReceived, processed);
    }
  }
}
