#pragma once

// =====================================================================
// SERVO_ENGINE.H — Non-blocking smooth motion engine
// =====================================================================
// Drives 6 servos (5 PWM + 1 PID DC motor) toward their targets in
// small increments each loop() tick.  Never calls delay().
// =====================================================================

#include "config.h"

// ---------------------------------------------------------------
// Per-servo runtime state (defined in servo_engine.cpp)
// ---------------------------------------------------------------
extern int           currentPos[NUM_SERVOS];   // current position (µs for PWM, ADC for S3)
extern int           targetPos[NUM_SERVOS];    // desired position

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
// Uses default speed (DEFAULT_INTERVAL).
// ---------------------------------------------------------------
void setTarget(int servoIndex, int value);

// ---------------------------------------------------------------
// Queue a new target with an explicit speed (1=slowest, 100=fastest).
// Speed is ignored for S3 (PID self-regulates).
// ---------------------------------------------------------------
void setTargetWithSpeed(int servoIndex, int value, int speed);

// ---------------------------------------------------------------
// True when servo[index] has reached its target.
// ---------------------------------------------------------------
bool servoReached(int servoIndex);

// ---------------------------------------------------------------
// True when BOTH servos in a pair have reached their targets.
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
// Send all servos to HOME_POS (queues targets at default speed).
// ---------------------------------------------------------------
void goHomeAll();

// ---------------------------------------------------------------
// Send all servos to SIT_POS (queues targets at default speed).
// ---------------------------------------------------------------
void sitDown();

// ---------------------------------------------------------------
// Print current positions to Serial.
// ---------------------------------------------------------------
void printPositions();
