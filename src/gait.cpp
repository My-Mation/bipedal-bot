// =====================================================================
// GAIT.CPP — Continuous Diagonal-Pair Gait State Machine
// =====================================================================
// Implements the continuous 6-step gait cycle for walking (forward, backward)
// and turning (left, right) without pause or interruption.
// =====================================================================

#include <Arduino.h>
#include "gait.h"
#include "config.h"
#include "servo_engine.h"

// ---------------------------------------------------------------
// Global Robot State
// ---------------------------------------------------------------
RobotState state = STATE_IDLE;

// ---------------------------------------------------------------
// Gait bookkeeping
// ---------------------------------------------------------------
static int           gaitStep      = 0;     // 0..5 within current gait cycle
static bool          stepInitiated = false; // target for this step set?
static bool          holding       = false; // post-move hold pause active?
static unsigned long holdStart     = 0;
static unsigned long stepStartTime = 0;

// ---------------------------------------------------------------
// Helper Functions
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
  stepStartTime = 0;
}

// ---------------------------------------------------------------
// Generic continuous diagonal-gait cycle executor
// ---------------------------------------------------------------
static void runGaitCycle(int pairFirstA,  int pairFirstB,
                          int pairSecondA, int pairSecondB,
                          int moveServoIdx, int seqA, int seqB) {

  int  checkA      = -1, checkB = -1;
  bool singleCheck = false;

  switch (gaitStep) {
    case 0:
      if (!stepInitiated) { liftPair(pairFirstA, pairFirstB); stepInitiated = true; stepStartTime = millis(); }
      checkA = pairFirstA; checkB = pairFirstB;
      break;
    case 1:
      if (!stepInitiated) {
        setTarget(moveServoIdx, seqA);
        if (moveServoIdx == S6) {
          setTarget(S5, SLIDE_HOME);
        }
        stepInitiated = true;
        stepStartTime = millis();
      }
      if (moveServoIdx == S6) {
        checkA = S6;
        checkB = S5;
        singleCheck = false;
      } else {
        checkA = moveServoIdx;
        singleCheck = true;
      }
      break;
    case 2:
      if (!stepInitiated) { lowerPair(pairFirstA, pairFirstB); stepInitiated = true; stepStartTime = millis(); }
      checkA = pairFirstA; checkB = pairFirstB;
      break;
    case 3:
      if (!stepInitiated) { liftPair(pairSecondA, pairSecondB); stepInitiated = true; stepStartTime = millis(); }
      checkA = pairSecondA; checkB = pairSecondB;
      break;
    case 4:
      if (!stepInitiated) { setTarget(moveServoIdx, seqB); stepInitiated = true; stepStartTime = millis(); }
      checkA = moveServoIdx; singleCheck = true;
      break;
    case 5:
      if (!stepInitiated) { lowerPair(pairSecondA, pairSecondB); stepInitiated = true; stepStartTime = millis(); }
      checkA = pairSecondA; checkB = pairSecondB;
      break;
  }

  // Has this step's motion completed or timed out?
  bool motionReached = singleCheck ? servoReached(checkA) : pairReached(checkA, checkB);
  bool timeout = (stepInitiated && (millis() - stepStartTime >= 600));
  bool reached = motionReached || timeout;

  // Start the hold timer when target is reached
  if (reached && !holding) {
    holding   = true;
    holdStart = millis();
  }

  // After hold expires, continuously loop to next step (0..5 -> 0..5 ...)
  if (holding && (millis() - holdStart >= HOLD_MS)) {
    gaitStep      = (gaitStep + 1) % 6;
    stepInitiated = false;
    holding       = false;
  }
}

// ---------------------------------------------------------------
// Enter State
// ---------------------------------------------------------------
void enterState(RobotState newState) {
  state = newState;
  resetGait();

  if (newState == STATE_SIT) {
    sitDown();
  } else if (newState == STATE_IDLE) {
    goHomeAll();
  } else if (newState == STATE_TURN_L || newState == STATE_TURN_R) {
    setTarget(S5, SLIDE_HOME);
  }
}

// ---------------------------------------------------------------
// Continuous Loop Dispatcher
// ---------------------------------------------------------------
void runGait() {
  switch (state) {
    case STATE_WALK_FWD:
      // Walk Forward per walk-dummy.txt: Pair A (S1+S2) leads, S5 slides FORWARD then HOME
      runGaitCycle(S1, S2, S3, S4, S5, SLIDE_FORWARD, SLIDE_HOME);
      break;

    case STATE_WALK_BWD:
      // Walk Backward per walk-dummy.txt: Pair A (S1+S2) leads, S5 slides HOME then FORWARD
      runGaitCycle(S1, S2, S3, S4, S5, SLIDE_HOME, SLIDE_FORWARD);
      break;

    case STATE_TURN_L:
      // Turn Left per walk-dummy.txt: Pair A (S1+S2) leads, S6 rotates TURN then HOME, S5 centered
      runGaitCycle(S1, S2, S3, S4, S6, ROTATE_TURN, ROTATE_HOME);
      break;

    case STATE_TURN_R:
      // Turn Right per walk-dummy.txt: Pair B (S3+S4) leads, S6 rotates TURN then HOME, S5 centered
      runGaitCycle(S3, S4, S1, S2, S6, ROTATE_TURN, ROTATE_HOME);
      break;

    case STATE_SIT:
      if (allServosIdle()) {
        state = STATE_IDLE;
      }
      break;

    default:
      break;
  }
}
