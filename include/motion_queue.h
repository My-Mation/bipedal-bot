#pragma once
#include "config.h"

struct MotionCmd {
  long id; // Packet ID
  int  targetPos[NUM_SERVOS]; // -1 means do not move
  int  speed[NUM_SERVOS];     // 1-100, S3 ignores this
};

void motionQueueInit();
bool motionEnqueue(const MotionCmd& cmd);
void motionClearQueue();
void motionEStop();
void motionGoHome();
void motionTick();
int  motionQueueSize();
bool isMotionExecuting();
