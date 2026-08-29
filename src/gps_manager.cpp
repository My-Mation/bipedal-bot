#include "gps_manager.h"
#include "config.h"
#include <HardwareSerial.h>

GpsData gpsData;

static HardwareSerial gpsSerial(2); // ESP32 UART2

void initGPS() {
  // GPS TX -> ESP32 RX2 (GPIO16), GPS RX <- ESP32 TX2 (GPIO17)
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#ifdef DEBUG
  Serial.printf("[GPS] HardwareSerial(2) initialized: RX=GPIO%d, TX=GPIO%d @ %u baud\n",
                GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
#endif
}

void readGPS() {
  while (gpsSerial.available() > 0) {
    char c = gpsSerial.read();
    (void)c; // Read stream to prevent buffer overflow
  }
}
