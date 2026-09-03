#include "motion_queue.h"
#include "servo_engine.h"
#include "ws_manager.h"
#include "gait.h"
#include <Arduino.h>

static MotionCmd queue[MOTION_QUEUE_SIZE];
static int qHead = 0;
static int qTail = 0;
static int qCount = 0;

static bool executing = false;
static long currentCmdId = -1;
static unsigned long currentCmdStartTime = 0;
static int activeTargets[NUM_SERVOS] = {-1, -1, -1, -1, -1, -1};

static bool currentCmdServosReached() {
  bool hasTargets = false;
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (activeTargets[i] != -1) {
      hasTargets = true;
      if (!servoReached(i)) return false;
    }
  }
  return hasTargets ? true : allServosIdle();
}

void motionQueueInit() {
  qHead = 0;
  qTail = 0;
  qCount = 0;
  executing = false;
  currentCmdId = -1;
  for (int i = 0; i < NUM_SERVOS; i++) activeTargets[i] = -1;
}

bool motionEnqueue(const MotionCmd& cmd) {
  if (qCount >= MOTION_QUEUE_SIZE) {
    return false;
  }
  queue[qTail] = cmd;
  qTail = (qTail + 1) % MOTION_QUEUE_SIZE;
  qCount++;
  return true;
}

void motionClearQueue() {
  qHead = 0;
  qTail = 0;
  qCount = 0;
  executing = false;
  currentCmdId = -1;
  for (int i = 0; i < NUM_SERVOS; i++) activeTargets[i] = -1;
  
  // Abort currently moving servos to prevent deadlock
  for (int i = 0; i < NUM_SERVOS; i++) {
    targetPos[i] = currentPos[i];
  }
  
  wsSendAll("{\"type\":\"event\",\"event\":\"queueEmpty\"}");
}

void motionEStop() {
  enterState(STATE_IDLE);
  motionClearQueue();
  executing = false;
  goHomeAll();
  wsSendAll("{\"type\":\"event\",\"event\":\"estop\"}");
}

void motionGoHome() {
  enterState(STATE_IDLE);
  motionClearQueue();
  executing = false;
  goHomeAll();
  wsSendAll("{\"type\":\"event\",\"event\":\"home\"}");
}

int motionQueueSize() {
  return qCount;
}

bool isMotionExecuting() {
  return executing || qCount > 0 || state != STATE_IDLE || !allServosIdle();
}

void motionTick() {
  // If we are currently executing a command, check if it's done
  if (executing) {
    bool done = currentCmdServosReached();
    bool timeout = (millis() - currentCmdStartTime > 2000);

    if (done || timeout) {
      if (timeout && !done) {
        // Timeout reached! Abort current step
#ifdef DEBUG
        Serial.println("[QUEUE] Command timeout! Aborting step.");
#endif
        wsSendAll("{\"type\":\"event\",\"event\":\"timeout\"}");
        // Stop targeted servos where they are
        for (int i = 0; i < NUM_SERVOS; i++) {
          if (activeTargets[i] != -1) {
            targetPos[i] = currentPos[i];
          }
        }
      }
      // Finished
      if (currentCmdId != -1) {
        char buf[80];
        snprintf(buf, sizeof(buf), "{\"type\":\"finished\",\"id\":%ld}", currentCmdId);
        wsSendAll(buf);
        snprintf(buf, sizeof(buf), "{\"type\":\"event\",\"event\":\"finished\",\"id\":%ld}", currentCmdId);
        wsSendAll(buf);
      }
      executing = false;
      currentCmdId = -1;
      for (int i = 0; i < NUM_SERVOS; i++) activeTargets[i] = -1;
      
      if (qCount == 0) {
        wsSendAll("{\"type\":\"event\",\"event\":\"queueEmpty\"}");
      }
    } else {
      return; // Still moving
    }
  }

  // If we are idle and have commands, start the next one
  if (!executing && qCount > 0) {
    MotionCmd& cmd = queue[qHead];
    
    currentCmdId = cmd.id;
    for (int i = 0; i < NUM_SERVOS; i++) {
      activeTargets[i] = cmd.targetPos[i];
      if (cmd.targetPos[i] != -1) {
        setTargetWithSpeed(i, cmd.targetPos[i], cmd.speed[i]);
      }
    }
    
    qHead = (qHead + 1) % MOTION_QUEUE_SIZE;
    qCount--;
    executing = true;
    currentCmdStartTime = millis();

    if (currentCmdId != -1) {
      char buf[64];
      snprintf(buf, sizeof(buf), "{\"type\":\"started\",\"id\":%ld}", currentCmdId);
      wsSendAll(buf);
    }
  }
}
