#pragma once

// =====================================================================
// SERVO_ENGINE.H — Non-blocking smooth motion engine
// =====================================================================
// Declares the per-servo state arrays and the functions that smoothly
// drive all 6 servos toward their targets without ever calling delay().
// =====================================================================

#include "config.h"

// ---------------------------------------------------------------
// Per-servo runtime state (defined in servo_engine.cpp)
// ---------------------------------------------------------------
extern int           currentPos[NUM_SERVOS];   // current pulse width (µs)
extern int           targetPos[NUM_SERVOS];    // desired pulse width (µs)
extern unsigned long lastStepTime[NUM_SERVOS]; // millis() of last tick
extern unsigned long stepInterval;             // ms between motion ticks

// ---------------------------------------------------------------
// Initialise all servos: attach, write HOME, prime state arrays.
// Call once from setup().
// ---------------------------------------------------------------
void initServos();

// ---------------------------------------------------------------
// Clamp a requested position to the servo's hardware-safe range.
// ---------------------------------------------------------------
int clampPos(int servoIndex, int value);

// ---------------------------------------------------------------
// Queue a new target position (clamped automatically).
// ---------------------------------------------------------------
void setTarget(int servoIndex, int value);

// ---------------------------------------------------------------
// True when servo[index] has reached its target.
// ---------------------------------------------------------------
bool servoReached(int servoIndex);

// ---------------------------------------------------------------
// True when BOTH servos in a diagonal pair have reached their targets.
// ---------------------------------------------------------------
bool pairReached(int a, int b);

// ---------------------------------------------------------------
// True when every servo has reached its target.
// ---------------------------------------------------------------
bool allServosIdle();

// ---------------------------------------------------------------
// Advance every servo one motion-tick toward its target.
// Call unconditionally on every loop() iteration.
// ---------------------------------------------------------------
void updateServos();

// ---------------------------------------------------------------
// Send all servos to HOME_POS immediately (queues targets).
// ---------------------------------------------------------------
void goHomeAll();

// ---------------------------------------------------------------
// Send all servos to SIT_POS immediately (queues targets).
// ---------------------------------------------------------------
void sitDown();

// ---------------------------------------------------------------
// Print current positions + speed to Serial.
// ---------------------------------------------------------------
void printPositions();
