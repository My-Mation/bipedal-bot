// =====================================================================
// GAIT.CPP — Diagonal-pair gait state machine
// =====================================================================
// Gait layout (6-step cycle per direction):
//
//  Step 0 : Lift  pair A           (swing phase — A in air)
//  Step 1 : Move actuator → pos A  (body slides on grounded B)
//  Step 2 : Lower pair A           (plant A)
//  Step 3 : Lift  pair B           (swing phase — B in air)
//  Step 4 : Move actuator → pos B  (body slides on grounded A)
//  Step 5 : Lower pair B           (plant B)   → repeat from 0
//
// Forward walk : seqA = SLIDE_FORWARD, seqB = SLIDE_HOME
// Backward walk: seqA = SLIDE_HOME,    seqB = SLIDE_FORWARD  (swapped)
// Turn L       : seqA = ROTATE_TURN,   seqB = ROTATE_HOME  (pair A leads)
// Turn R       : seqA = ROTATE_TURN,   seqB = ROTATE_HOME  (pair B leads)
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
static int           gaitStep      = 0;
static bool          stepInitiated = false;
static bool          holding       = false;
static unsigned long holdStart     = 0;

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------
static void liftPair(int a, int b) {
  setTarget(a, LIFT_POS[a]);
  setTarget(b, LIFT_POS[b]);
}

static void lowerPair(int a, int b) {
  setTarget(a, HOME_POS[a]);
  setTarget(b, HOME_POS[b]);
}

static void resetGait() {
  gaitStep      = 0;
  stepInitiated = false;
  holding       = false;
}

// ---------------------------------------------------------------
// Generic 6-step diagonal gait executor.
//
//   pairFirstA/B  : diagonal pair lifted first  (steps 0-2)
//   pairSecondA/B : diagonal pair lifted second (steps 3-5)
//   moveServoIndex: S5 (slider) for walking, S6 (rotator) for turning
//   seqA          : actuator target used on step 1
//   seqB          : actuator target used on step 4
//
// The "reached + HOLD_MS" guard is evaluated once per call;
// when both conditions are met the gaitStep advances and
// stepInitiated is cleared so the next case fires fresh.
// ---------------------------------------------------------------
static void runGaitCycle(int pairFirstA, int pairFirstB,
                         int pairSecondA, int pairSecondB,
                         int moveServoIndex, int seqA, int seqB) {

  int  checkA      = -1, checkB = -1;
  bool singleCheck = false;

  switch (gaitStep) {
    case 0:  // Lift pair A
      if (!stepInitiated) { liftPair(pairFirstA, pairFirstB); stepInitiated = true; }
      checkA = pairFirstA;  checkB = pairFirstB;
      break;

    case 1:  // Move actuator to seqA  (pair B grounded → body shifts)
      if (!stepInitiated) { setTarget(moveServoIndex, seqA); stepInitiated = true; }
      checkA = moveServoIndex;  singleCheck = true;
      break;

    case 2:  // Lower pair A
      if (!stepInitiated) { lowerPair(pairFirstA, pairFirstB); stepInitiated = true; }
      checkA = pairFirstA;  checkB = pairFirstB;
      break;

    case 3:  // Lift pair B
      if (!stepInitiated) { liftPair(pairSecondA, pairSecondB); stepInitiated = true; }
      checkA = pairSecondA;  checkB = pairSecondB;
      break;

    case 4:  // Move actuator to seqB  (pair A grounded → body shifts)
      if (!stepInitiated) { setTarget(moveServoIndex, seqB); stepInitiated = true; }
      checkA = moveServoIndex;  singleCheck = true;
      break;

    case 5:  // Lower pair B → cycle complete
      if (!stepInitiated) { lowerPair(pairSecondA, pairSecondB); stepInitiated = true; }
      checkA = pairSecondA;  checkB = pairSecondB;
      break;
  }

  // ── Advance to next step once servos reach target + HOLD_MS ──
  bool reached = singleCheck ? servoReached(checkA) : pairReached(checkA, checkB);

  if (reached && !holding) {
    holding   = true;
    holdStart = millis();
  }

  if (holding && (millis() - holdStart >= HOLD_MS)) {
    gaitStep      = (gaitStep + 1) % 6;
    stepInitiated = false;
    holding       = false;
  }
}

// ---------------------------------------------------------------
// enterState
// ---------------------------------------------------------------
void enterState(RobotState newState) {
  state = newState;
  resetGait();
  if (newState == STATE_SIT) {
    sitDown();
  }
}

// ---------------------------------------------------------------
// runGait — dispatch for the current state
// ---------------------------------------------------------------
void runGait() {
  switch (state) {

    case STATE_WALK_FWD:
      // Diagonal A (S1+S2) leads; slider goes FORWARD then returns HOME
      runGaitCycle(S1, S2, S3, S4,
                   S5, SLIDE_FORWARD, SLIDE_HOME);
      break;

    case STATE_WALK_BWD:
      // Diagonal B (S3+S4) leads → reverses travel direction
      runGaitCycle(S3, S4, S1, S2,
                   S5, SLIDE_FORWARD, SLIDE_HOME);
      break;

    case STATE_TURN_L:
      // Diagonal A leads; rotator provides the turning moment
      runGaitCycle(S1, S2, S3, S4,
                   S6, ROTATE_TURN, ROTATE_HOME);
      break;

    case STATE_TURN_R:
      // Diagonal B leads → reverses turn direction
      runGaitCycle(S3, S4, S1, S2,
                   S6, ROTATE_TURN, ROTATE_HOME);
      break;

    case STATE_SIT:
      // Sit motion triggered once in enterState(); wait for completion.
      if (allServosIdle()) {
        state = STATE_IDLE;
      }
      break;

    default:
      break;
  }
}
