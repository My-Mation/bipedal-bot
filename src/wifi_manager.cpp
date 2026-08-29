#include <WiFi.h>
#include "wifi_manager.h"
#include <Arduino.h>

#define AP_SSID "BipedBot"
#define AP_PASS "12345678"

void initWiFiAP() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  
  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  WiFi.softAPConfig(local_ip, gateway, subnet);
  bool success = WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  
  Serial.println("==========================================");
  if (success) {
    Serial.printf("Wi-Fi Hotspot Started!\n");
    Serial.printf("SSID    : %s\n", AP_SSID);
    Serial.printf("PASS    : %s\n", AP_PASS);
    Serial.printf("IP ADDR : http://%s\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("ERROR: Failed to start Wi-Fi Hotspot!");
  }
  Serial.println("==========================================");
}
