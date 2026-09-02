#include "cmd_parser.h"
#include "motion_queue.h"
#include "servo_engine.h"
#include "ws_manager.h"
#include "config.h"
#include "buzzer.h"
#include <ArduinoJson.h>
#include <Arduino.h>

// ---------------------------------------------------------------
// Helper: Enqueue the 6-step diagonal walking gait cycle
// ---------------------------------------------------------------
//  Step 0: Lift Pair First  (e.g., S1 & S2)
//  Step 1: Move Actuator   (e.g., S5 Slider forward to SLIDE_FORWARD)
//  Step 2: Lower Pair First (S1 & S2 down to HOME)
//  Step 3: Lift Pair Second (e.g., S3 & S4)
//  Step 4: Return Actuator  (S5 Slider back to SLIDE_HOME)
//  Step 5: Lower Pair Second (S3 & S4 down to HOME)
// ---------------------------------------------------------------
static void queueGaitCycle(int pairFirstA,  int pairFirstB,
                           int pairSecondA, int pairSecondB,
                           int moveServoIdx, int seqA, int seqB) {
  MotionCmd step0 = {-1, {-1, -1, -1, -1, -1, -1}, {100, 100, 100, 100, 100, 100}};
  step0.targetPos[pairFirstA] = LIFT_POS[pairFirstA];
  step0.targetPos[pairFirstB] = LIFT_POS[pairFirstB];

  MotionCmd step1 = {-1, {-1, -1, -1, -1, -1, -1}, {100, 100, 100, 100, 100, 100}};
  step1.targetPos[moveServoIdx] = seqA;

  MotionCmd step2 = {-1, {-1, -1, -1, -1, -1, -1}, {100, 100, 100, 100, 100, 100}};
  step2.targetPos[pairFirstA] = HOME_POS[pairFirstA];
  step2.targetPos[pairFirstB] = HOME_POS[pairFirstB];

  MotionCmd step3 = {-1, {-1, -1, -1, -1, -1, -1}, {100, 100, 100, 100, 100, 100}};
  step3.targetPos[pairSecondA] = LIFT_POS[pairSecondA];
  step3.targetPos[pairSecondB] = LIFT_POS[pairSecondB];

  MotionCmd step4 = {-1, {-1, -1, -1, -1, -1, -1}, {100, 100, 100, 100, 100, 100}};
  step4.targetPos[moveServoIdx] = seqB;

  MotionCmd step5 = {-1, {-1, -1, -1, -1, -1, -1}, {100, 100, 100, 100, 100, 100}};
  step5.targetPos[pairSecondA] = HOME_POS[pairSecondA];
  step5.targetPos[pairSecondB] = HOME_POS[pairSecondB];

  motionEnqueue(step0);
  motionEnqueue(step1);
  motionEnqueue(step2);
  motionEnqueue(step3);
  motionEnqueue(step4);
  motionEnqueue(step5);
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
    queueGaitCycle(S1, S2, S3, S4, S5, SLIDE_FORWARD, SLIDE_HOME);
  }
  else if (strcmp(type, "backward") == 0) {
    queueGaitCycle(S3, S4, S1, S2, S5, SLIDE_FORWARD, SLIDE_HOME);
  }
  else if (strcmp(type, "left") == 0) {
    queueGaitCycle(S1, S2, S3, S4, S6, ROTATE_TURN, ROTATE_HOME);
  }
  else if (strcmp(type, "right") == 0) {
    queueGaitCycle(S3, S4, S1, S2, S6, ROTATE_TURN, ROTATE_HOME);
  }
  else if (strcmp(type, "walk") == 0) {
    const char* dir = doc["dir"] | doc["direction"] | "forward";
    if (strcmp(dir, "forward") == 0)      queueGaitCycle(S1, S2, S3, S4, S5, SLIDE_FORWARD, SLIDE_HOME);
    else if (strcmp(dir, "backward") == 0) queueGaitCycle(S3, S4, S1, S2, S5, SLIDE_FORWARD, SLIDE_HOME);
    else if (strcmp(dir, "left") == 0)     queueGaitCycle(S1, S2, S3, S4, S6, ROTATE_TURN, ROTATE_HOME);
    else if (strcmp(dir, "right") == 0)    queueGaitCycle(S3, S4, S1, S2, S6, ROTATE_TURN, ROTATE_HOME);
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
