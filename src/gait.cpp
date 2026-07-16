// =====================================================================
// GAIT.CPP — Diagonal-pair gait state machine
// =====================================================================
// Implements the 6-step gait cycle used by all four movement modes
// (forward, backward, turn-left, turn-right).  A single generic
// runGaitCycle() function handles all four directions — the only
// difference is which diagonal pair leads and which actuator moves.
// =====================================================================

#include <Arduino.h>
#include "gait.h"
#include "config.h"
#include "servo_engine.h"

// ---------------------------------------------------------------
// State (extern declared in gait.h)
// ---------------------------------------------------------------
RobotState state = STATE_IDLE;

// ---------------------------------------------------------------
// Gait bookkeeping (private to this file)
// ---------------------------------------------------------------
static int           gaitStep      = 0;     // 0..5 within current gait cycle
static bool          stepInitiated = false; // has the target for this step been set?
static bool          holding       = false; // are we in the post-move hold pause?
static unsigned long holdStart     = 0;

// ---------------------------------------------------------------
// Private helpers: gait building blocks
// ---------------------------------------------------------------

// Lift both legs of a diagonal pair to their LIFT positions.
static void liftPair(int a, int b) {
  setTarget(a, LIFT_POS[a]);
  setTarget(b, LIFT_POS[b]);
}

// Lower both legs of a diagonal pair back to HOME positions.
static void lowerPair(int a, int b) {
  setTarget(a, HOME_POS[a]);
  setTarget(b, HOME_POS[b]);
}

// Reset gait bookkeeping (called on every state transition).
static void resetGait() {
  gaitStep      = 0;
  stepInitiated = false;
  holding       = false;
}

// ---------------------------------------------------------------
// Generic diagonal-gait cycle executor.
//
// Parameters
//   pairFirstA/B  : servo indices of the FIRST diagonal pair (lifted steps 0-2)
//   pairSecondA/B : servo indices of the SECOND diagonal pair (lifted steps 3-5)
//   moveServoIdx  : S5 (slider) for walking, S6 (rotator) for turning
//   seqA          : target sent to moveServoIdx during step 1
//   seqB          : target sent to moveServoIdx during step 4
//
// Cycle (6 steps, loops forever while state is unchanged):
//   0: lift pairFirst
//   1: move actuator to seqA     (slider/rotator provides propulsion)
//   2: lower pairFirst
//   3: lift pairSecond
//   4: move actuator to seqB     (return stroke)
//   5: lower pairSecond  → repeat
// ---------------------------------------------------------------
static void runGaitCycle(int pairFirstA,  int pairFirstB,
                          int pairSecondA, int pairSecondB,
                          int moveServoIdx, int seqA, int seqB) {

  int  checkA      = -1, checkB = -1;
  bool singleCheck = false;

  switch (gaitStep) {
    case 0:
      if (!stepInitiated) { liftPair(pairFirstA, pairFirstB); stepInitiated = true; }
      checkA = pairFirstA; checkB = pairFirstB;
      break;
    case 1:
      if (!stepInitiated) { setTarget(moveServoIdx, seqA); stepInitiated = true; }
      checkA = moveServoIdx; singleCheck = true;
      break;
    case 2:
      if (!stepInitiated) { lowerPair(pairFirstA, pairFirstB); stepInitiated = true; }
      checkA = pairFirstA; checkB = pairFirstB;
      break;
    case 3:
      if (!stepInitiated) { liftPair(pairSecondA, pairSecondB); stepInitiated = true; }
      checkA = pairSecondA; checkB = pairSecondB;
      break;
    case 4:
      if (!stepInitiated) { setTarget(moveServoIdx, seqB); stepInitiated = true; }
      checkA = moveServoIdx; singleCheck = true;
      break;
    case 5:
      if (!stepInitiated) { lowerPair(pairSecondA, pairSecondB); stepInitiated = true; }
      checkA = pairSecondA; checkB = pairSecondB;
      break;
  }

  // Has this step's motion completed?
  bool reached = singleCheck ? servoReached(checkA) : pairReached(checkA, checkB);

  // Start the hold timer on first reaching the target.
  if (reached && !holding) {
    holding   = true;
    holdStart = millis();
  }

  // After the hold expires, advance to the next step.
  if (holding && (millis() - holdStart >= HOLD_MS)) {
    gaitStep      = (gaitStep + 1) % 6;
    stepInitiated = false;
    holding       = false;
  }
}

// ---------------------------------------------------------------
// Public API — enterState
// ---------------------------------------------------------------
void enterState(RobotState newState) {
  state = newState;
  resetGait();
  // Kick off the sit motion the moment we enter STATE_SIT
  if (newState == STATE_SIT) {
    sitDown();
  }
}

// ---------------------------------------------------------------
// Public API — runGait
// Dispatch to the correct gait configuration for the current state.
// ---------------------------------------------------------------
void runGait() {
  switch (state) {
    case STATE_WALK_FWD:
      // Diagonal A (S1+S2) leads; slider drives FORWARD then returns HOME.
      runGaitCycle(S1, S2, S3, S4, S5, SLIDE_FORWARD, SLIDE_HOME);
      break;

    case STATE_WALK_BWD:
      // Diagonal B (S3+S4) leads instead → reverses travel direction.
      runGaitCycle(S3, S4, S1, S2, S5, SLIDE_FORWARD, SLIDE_HOME);
      break;

    case STATE_TURN_L:
      // Diagonal A leads; rotator drives TURN then returns HOME.
      runGaitCycle(S1, S2, S3, S4, S6, ROTATE_TURN, ROTATE_HOME);
      break;

    case STATE_TURN_R:
      // Diagonal B leads instead → reverses turn direction.
      runGaitCycle(S3, S4, S1, S2, S6, ROTATE_TURN, ROTATE_HOME);
      break;

    case STATE_SIT:
      // Sit motion is triggered once in enterState().
      // Here we just wait for all servos to finish, then go IDLE.
      if (allServosIdle()) {
        state = STATE_IDLE;
      }
      break;

    default:
      break;
  }
}
