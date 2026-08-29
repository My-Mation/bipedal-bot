#include "telemetry.h"
#include "ws_manager.h"
#include "battery_monitor.h"
#include "imu.h"
#include "servo_engine.h"
#include "motion_queue.h"
#include <Arduino.h>
#include <WiFi.h>

static unsigned long lastTelemetry = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 33; // ~30 Hz

void telemetryTick() {
  unsigned long now = millis();
  if (now - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
    lastTelemetry = now;
    
    // Only send telemetry if there are connected clients to save CPU
    // We check via wsSendAll but it's better to construct string only when needed.
    // However, wsSendAll checks ws.count() > 0 anyway. But string formatting is costly.
    
    char buf[512];
    snprintf(buf, sizeof(buf),
      "{"
        "\"type\":\"telemetry\","
        "\"batteryPercent\":%d,"
        "\"batteryVoltage\":%.2f,"
        "\"roll\":%.1f,"
        "\"pitch\":%.1f,"
        "\"yaw\":null,"
        "\"yawSupported\":false,"
        "\"accelX\":%.2f,\"accelY\":%.2f,\"accelZ\":%.2f,"
        "\"gyroX\":%.2f,\"gyroY\":%.2f,\"gyroZ\":%.2f,"
        "\"queue\":%d,"
        "\"moving\":%s,"
        "\"wifiRSSI\":%d,"
        "\"servos\":[%d,%d,%d,%d,%d,%d]"
      "}",
      batteryPercent(), batteryVoltage(),
      imuData.roll, imuData.pitch,
      imuData.ax, imuData.ay, imuData.az,
      imuData.gx, imuData.gy, imuData.gz,
      motionQueueSize(),
      isMotionExecuting() ? "true" : "false",
      WiFi.RSSI(),
      currentPos[0], currentPos[1], currentPos[2],
      currentPos[3], currentPos[4], currentPos[5]
    );
    
    wsSendAll(buf);
  }
}
