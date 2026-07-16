#pragma once

// =====================================================================
// GAIT.H — Gait state machine and walking / turning routines
// =====================================================================
// Declares the robot state enum, the current state variable, and all
// functions that advance the diagonal-pair gait cycle.
// =====================================================================

// ---------------------------------------------------------------
// Robot top-level states
// ---------------------------------------------------------------
enum RobotState {
  STATE_IDLE,       // stationary, holding last position
  STATE_HOME,       // returning to home; goes IDLE when done
  STATE_WALK_FWD,   // continuous forward walk
  STATE_WALK_BWD,   // continuous backward walk
  STATE_TURN_L,     // continuous left turn
  STATE_TURN_R,     // continuous right turn
  STATE_SIT         // reserved for future sit behaviour
};

extern RobotState state;  // defined in gait.cpp

// ---------------------------------------------------------------
// Transition to a new state and reset gait bookkeeping.
// ---------------------------------------------------------------
void enterState(RobotState newState);

// ---------------------------------------------------------------
// Run one iteration of the current gait (call from loop() when
// in a walking or turning state).
// ---------------------------------------------------------------
void runGait();
