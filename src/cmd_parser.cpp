#include "cmd_parser.h"
#include "motion_queue.h"
#include "servo_engine.h"
#include "ws_manager.h"
#include "config.h"
#include "buzzer.h"
#include <ArduinoJson.h>
#include <Arduino.h>

static void queueStep(int servoIdx, int pos1, int pos2) {
  MotionCmd cmd1 = {-1, {-1, -1, -1, -1, -1, -1}, {100, 100, 100, 100, 100, 100}};
  MotionCmd cmd2 = {-1, {-1, -1, -1, -1, -1, -1}, {100, 100, 100, 100, 100, 100}};
  cmd1.targetPos[servoIdx] = pos1;
  cmd2.targetPos[servoIdx] = pos2;
  motionEnqueue(cmd1);
  motionEnqueue(cmd2);
}

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
  else if (strcmp(type, "light") == 0) {
    if (doc["state"].is<bool>()) {
      setLightState(doc["state"].as<bool>());
    } else if (doc["toggle"].is<bool>() && doc["toggle"].as<bool>()) {
      setLightState(!getLightState());
    } else {
      setLightState(!getLightState()); // default toggle if no extra key
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"type\":\"event\",\"event\":\"lightState\",\"state\":%s}",
             getLightState() ? "true" : "false");
    wsSendAll(buf);
  }
  else if (strcmp(type, "estop") == 0) {
    motionEStop();
  }
  else if (strcmp(type, "home") == 0 || strcmp(type, "stand") == 0) {
    motionGoHome();
  }
  else if (strcmp(type, "sit") == 0) {
    sitDown();
  }
  else if (strcmp(type, "forward") == 0) {
    queueStep(4, SLIDE_FORWARD, SLIDE_HOME); // S5 Slider
  }
  else if (strcmp(type, "backward") == 0) {
    queueStep(4, 3000, SLIDE_HOME); // S5 Slider
  }
  else if (strcmp(type, "left") == 0) {
    queueStep(5, ROTATE_TURN, ROTATE_HOME); // S6 Rotator
  }
  else if (strcmp(type, "right") == 0) {
    queueStep(5, 3300, ROTATE_HOME); // S6 Rotator
  }
  else if (strcmp(type, "walk") == 0) {
    const char* dir = doc["dir"] | doc["direction"] | "forward";
    if (strcmp(dir, "forward") == 0)      queueStep(4, SLIDE_FORWARD, SLIDE_HOME);
    else if (strcmp(dir, "backward") == 0) queueStep(4, 3000, SLIDE_HOME);
    else if (strcmp(dir, "left") == 0)     queueStep(5, ROTATE_TURN, ROTATE_HOME);
    else if (strcmp(dir, "right") == 0)    queueStep(5, 3300, ROTATE_HOME);
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
