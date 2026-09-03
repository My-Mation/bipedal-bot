#pragma once
#include "config.h"

// ---------------------------------------------------------------
// Robot State Enum
// ---------------------------------------------------------------
enum RobotState {
  STATE_IDLE = 0,
  STATE_WALK_FWD,
  STATE_WALK_BWD,
  STATE_TURN_L,
  STATE_TURN_R,
  STATE_SIT
};

extern RobotState state;

// ---------------------------------------------------------------
// Public Gait API
// ---------------------------------------------------------------
void enterState(RobotState newState);
void runGait();
