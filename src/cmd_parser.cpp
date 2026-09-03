#include "cmd_parser.h"
#include "motion_queue.h"
#include "servo_engine.h"
#include "ws_manager.h"
#include "config.h"
#include "buzzer.h"
#include "gait.h"
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
    enterState(STATE_IDLE);
    motionEStop();
  }
  else if (strcmp(type, "home") == 0 || strcmp(type, "stand") == 0) {
    enterState(STATE_IDLE);
  }
  else if (strcmp(type, "sit") == 0) {
    enterState(STATE_SIT);
  }
  else if (strcmp(type, "forward") == 0) {
    enterState(STATE_WALK_FWD);
  }
  else if (strcmp(type, "backward") == 0) {
    enterState(STATE_WALK_BWD);
  }
  else if (strcmp(type, "left") == 0) {
    enterState(STATE_TURN_L);
  }
  else if (strcmp(type, "right") == 0) {
    enterState(STATE_TURN_R);
  }
  else if (strcmp(type, "stop") == 0) {
    enterState(STATE_IDLE);
  }
  else if (strcmp(type, "walk") == 0) {
    const char* dir = doc["dir"] | doc["direction"] | "forward";
    if (strcmp(dir, "forward") == 0)      enterState(STATE_WALK_FWD);
    else if (strcmp(dir, "backward") == 0) enterState(STATE_WALK_BWD);
    else if (strcmp(dir, "left") == 0)     enterState(STATE_TURN_L);
    else if (strcmp(dir, "right") == 0)    enterState(STATE_TURN_R);
    else if (strcmp(dir, "stop") == 0)     enterState(STATE_IDLE);
  }
  else if (strcmp(type, "clearQueue") == 0) {
    enterState(STATE_IDLE);
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
