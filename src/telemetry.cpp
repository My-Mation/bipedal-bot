#include "telemetry.h"
#include "ws_manager.h"
#include "battery_monitor.h"
#include "imu.h"
#include "gps_manager.h"
#include "cmd_parser.h"
#include "servo_engine.h"
#include "motion_queue.h"
#include <Arduino.h>
#include <WiFi.h>

static unsigned long lastTelemetry = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 50; // 20 Hz broadcast rate (smooth & bandwidth efficient)

void telemetryTick() {
  unsigned long now = millis();
  if (now - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
    lastTelemetry = now;
    
    // Only construct and send telemetry if there is at least 1 connected client
    char buf[896];
    snprintf(buf, sizeof(buf),
      "{"
        "\"type\":\"telemetry\","
        "\"v\":%.2f,"
        "\"batteryVoltage\":%.2f,"
        "\"bat\":%d,"
        "\"batteryPercent\":%d,"
        "\"light\":%s,"
        "\"moving\":%s,"
        "\"queue\":%d,"
        "\"rssi\":%d,"
        "\"wifiRSSI\":%d,"
        "\"pitch\":%.1f,"
        "\"roll\":%.1f,"
        "\"yaw\":0.0,"
        "\"yawSupported\":false,"
        "\"ax\":%.2f,\"accelX\":%.2f,"
        "\"ay\":%.2f,\"accelY\":%.2f,"
        "\"az\":%.2f,\"accelZ\":%.2f,"
        "\"gx\":%.2f,\"gyroX\":%.2f,"
        "\"gy\":%.2f,\"gyroY\":%.2f,"
        "\"gz\":%.2f,\"gyroZ\":%.2f,"
        "\"servos\":[%d,%d,%d,%d,%d,%d],"
        "\"imu\":{"
          "\"ok\":%s,"
          "\"pitch\":%.1f,"
          "\"roll\":%.1f,"
          "\"yaw\":0.0,"
          "\"ax\":%.2f,\"accelX\":%.2f,"
          "\"ay\":%.2f,\"accelY\":%.2f,"
          "\"az\":%.2f,\"accelZ\":%.2f,"
          "\"gx\":%.2f,\"gyroX\":%.2f,"
          "\"gy\":%.2f,\"gyroY\":%.2f,"
          "\"gz\":%.2f,\"gyroZ\":%.2f,"
          "\"temp\":%.1f"
        "},"
        "\"gps\":{"
          "\"valid\":%s,"
          "\"lat\":%.6f,\"latitude\":%.6f,"
          "\"lng\":%.6f,\"longitude\":%.6f,"
          "\"alt\":%.1f,\"altitude\":%.1f,"
          "\"speed\":%.1f,"
          "\"sats\":%d,\"satellites\":%d,"
          "\"hdop\":%.1f"
        "}"
      "}",
      batteryVoltage(), batteryVoltage(),
      batteryPercent(), batteryPercent(),
      getLightState() ? "true" : "false",
      isMotionExecuting() ? "true" : "false",
      motionQueueSize(),
      WiFi.RSSI(), WiFi.RSSI(),
      imuData.pitch, imuData.roll,
      imuData.ax, imuData.ax,
      imuData.ay, imuData.ay,
      imuData.az, imuData.az,
      imuData.gx, imuData.gx,
      imuData.gy, imuData.gy,
      imuData.gz, imuData.gz,
      currentPos[0], currentPos[1], currentPos[2],
      currentPos[3], currentPos[4], currentPos[5],
      imuData.ok ? "true" : "false",
      imuData.pitch, imuData.roll,
      imuData.ax, imuData.ax,
      imuData.ay, imuData.ay,
      imuData.az, imuData.az,
      imuData.gx, imuData.gx,
      imuData.gy, imuData.gy,
      imuData.gz, imuData.gz,
      imuData.temp,
      gpsData.valid ? "true" : "false",
      gpsData.latitude, gpsData.latitude,
      gpsData.longitude, gpsData.longitude,
      gpsData.altitude, gpsData.altitude,
      gpsData.speed,
      gpsData.satellites, gpsData.satellites,
      gpsData.hdop
    );
    
    wsSendAll(buf);
  }
}
