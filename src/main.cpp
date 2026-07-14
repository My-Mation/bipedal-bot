/*
  =====================================================================
  6-SERVO BIPED WALKING ROBOT CONTROLLER
  =====================================================================
  Hardware:
    Servo1 (Leg)   -> GPIO13
    Servo2 (Leg)   -> GPIO14
    Servo3 (Leg)   -> GPIO27
    Servo4 (Leg)   -> GPIO26
    Servo5 (Slide) -> GPIO25
    Servo6 (Rotate)-> GPIO32

  Design notes:
    - All 6 servos are driven from a single non-blocking motion engine
      (moveServoSmooth-style update loop, no delay() in loop()).
    - A step-based gait state machine drives forward/backward walking
      and left/right turning using a diagonal-pair gait.
    - Forward and backward walking reuse the SAME gait routine, just
      with the diagonal pair lift order swapped. Same for left/right
      turning. This avoids duplicated gait code.
    - All servo targets are clamped to hardware-safe limits before
      ever being written.
  =====================================================================
*/

#include <ESP32Servo.h>

// ---------------------------------------------------------------
// Servo indices (used everywhere instead of 6 separate variables)
//  0 = Servo1 (Leg, Front-Left group)
//  1 = Servo2 (Leg, Back-Right group)      -> Diagonal Pair A
//  2 = Servo3 (Leg, Front-Right group)
//  3 = Servo4 (Leg, Back-Left group)       -> Diagonal Pair B
//  4 = Servo5 (Slider)
//  5 = Servo6 (Rotator)
// ---------------------------------------------------------------
#define NUM_SERVOS 6
#define S1 0
#define S2 1
#define S3 2
#define S4 3
#define S5 4
#define S6 5

const int SERVO_PINS[NUM_SERVOS] = {13, 14, 27, 26, 25, 32};

Servo servos[NUM_SERVOS];

// ---------------------------------------------------------------
// Calibration (from user's mechanically calibrated robot)
// ---------------------------------------------------------------
const int HOME_POS[NUM_SERVOS] = {2350, 650, 650, 2350, 2500, 2500};

// Maximum safe lift positions for the 4 leg servos (index 0-3 only)
const int LIFT_POS[NUM_SERVOS] = {1000, 1800, 1700, 1170, 2500, 2500}; // idx 4/5 unused here

// Slider (Servo5) fixed calibrated points
const int SLIDE_HOME    = 2500;
const int SLIDE_FORWARD = 1450;

// Rotator (Servo6) fixed calibrated points
const int ROTATE_HOME = 2500;
const int ROTATE_TURN = 1700;

// Hard safety clamps (never exceed these, regardless of what asked)
const int MIN_POS[NUM_SERVOS] = {1000, 500, 500, 1170, 1450, 1700};
const int MAX_POS[NUM_SERVOS] = {2500, 2500, 2500, 2500, 2500, 2500};

// ---------------------------------------------------------------
// Motion engine state (per-servo)
// ---------------------------------------------------------------
int currentPos[NUM_SERVOS];
int targetPos[NUM_SERVOS];
unsigned long lastStepTime[NUM_SERVOS];

const int STEP_SIZE = 15;          // microseconds per motion tick
unsigned long stepInterval = 8;    // ms between motion ticks (speed control)
const unsigned long MIN_INTERVAL = 2;
const unsigned long MAX_INTERVAL = 30;

// ---------------------------------------------------------------
// Gait state machine
// ---------------------------------------------------------------
enum RobotState {
  STATE_IDLE,
  STATE_HOME,
  STATE_WALK_FWD,
  STATE_WALK_BWD,
  STATE_TURN_L,
  STATE_TURN_R,
  STATE_SIT
};

RobotState state = STATE_IDLE;

int gaitStep = 0;              // 0..5 within current gait cycle
bool stepInitiated = false;    // has the target for this step been set?
bool holding = false;          // are we in the post-move hold pause?
unsigned long holdStart = 0;
const unsigned long HOLD_MS = 150;

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------
int clampPos(int index, int value) {
  if (value < MIN_POS[index]) value = MIN_POS[index];
  if (value > MAX_POS[index]) value = MAX_POS[index];
  return value;
}

void setTarget(int index, int value) {
  targetPos[index] = clampPos(index, value);
}

bool servoReached(int index) {
  return currentPos[index] == targetPos[index];
}

bool pairReached(int a, int b) {
  return servoReached(a) && servoReached(b);
}

// Non-blocking per-tick update of all 6 servos toward their targets.
// This is the "moveServoSmooth" motion engine, called every loop().
void updateServos() {
  unsigned long now = millis();
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (currentPos[i] == targetPos[i]) continue;
    if (now - lastStepTime[i] < stepInterval) continue;

    if (currentPos[i] < targetPos[i]) {
      currentPos[i] += STEP_SIZE;
      if (currentPos[i] > targetPos[i]) currentPos[i] = targetPos[i];
    } else {
      currentPos[i] -= STEP_SIZE;
      if (currentPos[i] < targetPos[i]) currentPos[i] = targetPos[i];
    }

    servos[i].writeMicroseconds(currentPos[i]);
    lastStepTime[i] = now;
  }
}

bool allServosIdle() {
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (!servoReached(i)) return false;
  }
  return true;
}

// ---------------------------------------------------------------
// Gait building blocks (reusable, used by walk + turn routines)
// ---------------------------------------------------------------
void liftPair(int a, int b) {
  setTarget(a, LIFT_POS[a]);
  setTarget(b, LIFT_POS[b]);
}

void lowerPair(int a, int b) {
  setTarget(a, HOME_POS[a]);
  setTarget(b, HOME_POS[b]);
}

void slideTo(int value) {
  setTarget(S5, value);
}

void rotateTo(int value) {
  setTarget(S6, value);
}

void goHomeAll() {
  for (int i = 0; i < NUM_SERVOS; i++) {
    setTarget(i, HOME_POS[i]);
  }
}

void resetGait() {
  gaitStep = 0;
  stepInitiated = false;
  holding = false;
}

// ---------------------------------------------------------------
// Generic diagonal gait executor.
// pairFirst / pairSecond: the two leg servos of each diagonal pair,
//   lifted/lowered in that order.
// moveServoIndex: S5 (slider) for walking, S6 (rotator) for turning.
// seqA / seqB: the two target values commanded on moveServoIndex
//   during step 1 and step 4 of the cycle.
//
// Cycle:
//   0: lift pairFirst
//   1: move (slide/rotate) to seqA
//   2: lower pairFirst
//   3: lift pairSecond
//   4: move (slide/rotate) to seqB
//   5: lower pairSecond   -> loop back to 0
// ---------------------------------------------------------------
void runGaitCycle(int pairFirstA, int pairFirstB,
                   int pairSecondA, int pairSecondB,
                   int moveServoIndex, int seqA, int seqB) {

  int checkA = -1, checkB = -1; // indices to test for "reached" this step
  bool singleCheck = false;

  switch (gaitStep) {
    case 0:
      if (!stepInitiated) { liftPair(pairFirstA, pairFirstB); stepInitiated = true; }
      checkA = pairFirstA; checkB = pairFirstB;
      break;
    case 1:
      if (!stepInitiated) { setTarget(moveServoIndex, seqA); stepInitiated = true; }
      checkA = moveServoIndex; singleCheck = true;
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
      if (!stepInitiated) { setTarget(moveServoIndex, seqB); stepInitiated = true; }
      checkA = moveServoIndex; singleCheck = true;
      break;
    case 5:
      if (!stepInitiated) { lowerPair(pairSecondA, pairSecondB); stepInitiated = true; }
      checkA = pairSecondA; checkB = pairSecondB;
      break;
  }

  bool reached = singleCheck ? servoReached(checkA) : pairReached(checkA, checkB);

  if (reached && !holding) {
    holding = true;
    holdStart = millis();
  }

  if (holding && (millis() - holdStart >= HOLD_MS)) {
    gaitStep = (gaitStep + 1) % 6;
    stepInitiated = false;
    holding = false;
  }
}

// Dispatch to the correct gait configuration for the current state.
void runGait() {
  switch (state) {
    case STATE_WALK_FWD:
      // Diagonal A (S1+S2) leads, slider goes FORWARD then HOME
      runGaitCycle(S1, S2, S3, S4, S5, SLIDE_FORWARD, SLIDE_HOME);
      break;
    case STATE_WALK_BWD:
      // Diagonal B (S3+S4) leads instead -> reverses travel direction
      runGaitCycle(S3, S4, S1, S2, S5, SLIDE_FORWARD, SLIDE_HOME);
      break;
    case STATE_TURN_L:
      // Diagonal A leads, rotator goes TURN then HOME
      runGaitCycle(S1, S2, S3, S4, S6, ROTATE_TURN, ROTATE_HOME);
      break;
    case STATE_TURN_R:
      // Diagonal B leads instead -> reverses turn direction
      runGaitCycle(S3, S4, S1, S2, S6, ROTATE_TURN, ROTATE_HOME);
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------
void enterState(RobotState newState) {
  state = newState;
  resetGait();
}

void printPositions() {
  Serial.println("---- Servo Positions (us) ----");
  const char* names[NUM_SERVOS] = {"S1", "S2", "S3", "S4", "S5(slide)", "S6(rotate)"};
  for (int i = 0; i < NUM_SERVOS; i++) {
    Serial.print(names[i]);
    Serial.print(" = ");
    Serial.println(currentPos[i]);
  }
  Serial.print("Speed interval = ");
  Serial.print(stepInterval);
  Serial.println(" ms");
  Serial.println("-------------------------------");
}

void handleCommand(char cmd) {
  switch (cmd) {
    case 'f':
      Serial.println("CMD: Walking forward");
      enterState(STATE_WALK_FWD);
      break;
    case 'b':
      Serial.println("CMD: Walking backward");
      enterState(STATE_WALK_BWD);
      break;
    case 'l':
      Serial.println("CMD: Turning left");
      enterState(STATE_TURN_L);
      break;
    case 'r':
      Serial.println("CMD: Turning right");
      enterState(STATE_TURN_R);
      break;
    case 's':
      Serial.println("CMD: Stop");
      enterState(STATE_IDLE);
      break;
    case 'h':
      Serial.println("CMD: Homing");
      goHomeAll();
      enterState(STATE_HOME);
      break;
    case 'p':
      printPositions();
      break;
    case '+':
      if (stepInterval > MIN_INTERVAL) stepInterval -= 2;
      Serial.print("Speed increased. Interval = ");
      Serial.println(stepInterval);
      break;
    case '-':
      if (stepInterval < MAX_INTERVAL) stepInterval += 2;
      Serial.print("Speed decreased. Interval = ");
      Serial.println(stepInterval);
      break;
    default:
      // ignore newlines / unknown characters
      break;
  }
}

// ---------------------------------------------------------------
// Setup / Loop
// ---------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  for (int i = 0; i < NUM_SERVOS; i++) {
    servos[i].attach(SERVO_PINS[i]);
    currentPos[i] = HOME_POS[i];
    targetPos[i] = HOME_POS[i];
    lastStepTime[i] = 0;
    servos[i].writeMicroseconds(currentPos[i]);
  }

  state = STATE_IDLE;

  Serial.println();
  Serial.println("6-Servo Biped Walker Ready.");
  Serial.println("Commands:");
  Serial.println("  f = Walk forward (continuous)");
  Serial.println("  b = Walk backward (continuous)");
  Serial.println("  l = Turn left (continuous)");
  Serial.println("  r = Turn right (continuous)");
  Serial.println("  s = Stop immediately");
  Serial.println("  h = Return home");
  Serial.println("  p = Print servo positions");
  Serial.println("  + = Faster");
  Serial.println("  - = Slower");
}

void loop() {
  // 1. Handle incoming serial commands (non-blocking)
  if (Serial.available()) {
    char cmd = Serial.read();
    handleCommand(cmd);
  }

  // 2. Advance the gait / homing state machine
  switch (state) {
    case STATE_HOME:
      if (allServosIdle()) {
        state = STATE_IDLE;
      }
      break;
    case STATE_WALK_FWD:
    case STATE_WALK_BWD:
    case STATE_TURN_L:
    case STATE_TURN_R:
      runGait();
      break;
    case STATE_IDLE:
    default:
      // nothing to do; servos hold last commanded position
      break;
  }

  // 3. Update all servo positions smoothly (always runs, non-blocking)
  updateServos();
}