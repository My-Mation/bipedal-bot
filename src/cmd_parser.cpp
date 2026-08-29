#include "cmd_parser.h"
#include "motion_queue.h"
#include "ws_manager.h"
#include "config.h"
#include "buzzer.h"
#include <ArduinoJson.h>
#include <Arduino.h>

void parseCommand(const char* data, size_t len) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);

  if (err) {
#ifdef DEBUG
    Serial.printf("[JSON] Failed to parse: %s\n", err.c_str());
#endif
    return;
  }

  const char* type = doc["type"];
  if (!type) return;

  if (strcmp(type, "heartbeat") == 0) {
    markHeartbeat();
  }
  else if (strcmp(type, "ping") == 0) {
    wsSendAll("{\"type\":\"pong\"}");
  }
  else if (strcmp(type, "beep") == 0) {
    int duration = doc["duration"] | 100;
    buzzerBeep(duration);
  }
  else if (strcmp(type, "estop") == 0) {
    motionEStop();
  }
  else if (strcmp(type, "home") == 0) {
    motionGoHome();
  }
  else if (strcmp(type, "clearQueue") == 0) {
    motionClearQueue();
  }
  else if (strcmp(type, "move") == 0) {
    MotionCmd cmd;
    cmd.id = doc["id"] | -1;
    
    // Initialize defaults
    for (int i=0; i<NUM_SERVOS; i++) {
      cmd.targetPos[i] = -1;
      cmd.speed[i] = 100;
    }

    JsonArray servos = doc["servos"];
    for (JsonObject s : servos) {
      int id = s["id"];
      if (id >= 0 && id < NUM_SERVOS) {
        int pos = s["position"];
        int speed = s["speed"] | 100; // default to 100
        
        // Validation check for limit events
        if (pos < MIN_POS[id] || pos > MAX_POS[id]) {
          char buf[64];
          snprintf(buf, sizeof(buf), "{\"type\":\"event\",\"event\":\"servoLimit\",\"servo\":%d}", id);
          wsSendAll(buf);
        }

        cmd.targetPos[id] = pos; // will be clamped later by servo engine anyway
        cmd.speed[id] = speed;
      } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"type\":\"event\",\"event\":\"invalidServo\",\"servo\":%d}", id);
        wsSendAll(buf);
      }
    }

    if (!motionEnqueue(cmd)) {
      wsSendAll("{\"type\":\"error\",\"code\":\"QUEUE_FULL\"}");
    }
  }
}
