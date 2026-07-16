#pragma once

// =====================================================================
// CONFIG.H — Hardware pins, calibration, and tuning constants
// =====================================================================
// This is the ONLY place you need to touch when re-calibrating servos
// or changing timing.  Everything else reads from here.
// =====================================================================

// ---------------------------------------------------------------
// Servo count & named indices
// ---------------------------------------------------------------
#define NUM_SERVOS 6

#define S1 0   // Leg — Front-Left  ]  Diagonal
#define S2 1   // Leg — Back-Right  ]  Pair A
#define S3 2   // Leg — Front-Right ]  Diagonal
#define S4 3   // Leg — Back-Left   ]  Pair B
#define S5 4   // Slider  (forward / backward translation)
#define S6 5   // Rotator (left / right turning)

// ---------------------------------------------------------------
// GPIO pin assignments
// ---------------------------------------------------------------
inline constexpr int SERVO_PINS[NUM_SERVOS] = {13, 14, -1, 26, 25, 32}; // S3 (index 2) uses custom pins

// ---------------------------------------------------------------
// S3 Smart Servo (MX1508 + Potentiometer) Pins
// ---------------------------------------------------------------
inline constexpr int S3_PIN_POT = 34;
inline constexpr int S3_PIN_IN1 = 18;
inline constexpr int S3_PIN_IN2 = 19;

// ---------------------------------------------------------------
// Calibrated home positions (µs pulse width)
// ---------------------------------------------------------------
inline constexpr int HOME_POS[NUM_SERVOS] = {2350, 650, 2000, 2350, 2500, 2500};

// ---------------------------------------------------------------
// Maximum safe lift position for each leg servo (µs)
// Indices 4 & 5 are placeholders — the slider/rotator are never lifted.
// ---------------------------------------------------------------
inline constexpr int LIFT_POS[NUM_SERVOS] = {1000, 1800, 1000, 1170, 2500, 2500};

// ---------------------------------------------------------------
// Sit position — all legs folded, body lowered to ground (µs)
// ---------------------------------------------------------------
inline constexpr int SIT_POS[NUM_SERVOS] = {
    1000,   // S1 — Front-Left  leg fully bent
    1800,   // S2 — Back-Right  leg fully bent
    75,     // S3 — Front-Right leg fully bent (Smart Servo ADC)
    1170,   // S4 — Back-Left   leg fully bent
    2500,   // S5 — Slider at home
    2500    // S6 — Rotator at home
};

// ---------------------------------------------------------------
// Slider (S5) waypoints
// ---------------------------------------------------------------
inline constexpr int SLIDE_HOME    = 2500;
inline constexpr int SLIDE_FORWARD = 1450;

// ---------------------------------------------------------------
// Rotator (S6) waypoints
// ---------------------------------------------------------------
inline constexpr int ROTATE_HOME = 2500;
inline constexpr int ROTATE_TURN = 1700;

// ---------------------------------------------------------------
// Hard safety clamps — positions are clamped to these before any
// writeMicroseconds() call.  Prevents mechanical damage.
// ---------------------------------------------------------------
inline constexpr int MIN_POS[NUM_SERVOS] = {1000, 500, 50, 1170, 1450, 1700};
inline constexpr int MAX_POS[NUM_SERVOS] = {2500, 2500, 3950, 2500, 2500, 2500};

// ---------------------------------------------------------------
// Motion-engine tuning
// ---------------------------------------------------------------
inline constexpr int           STEP_SIZE    = 15;  // µs moved per motion tick
inline constexpr unsigned long DEFAULT_INTERVAL = 8;   // ms between ticks (start)
inline constexpr unsigned long MIN_INTERVAL     = 2;   // fastest speed (+)
inline constexpr unsigned long MAX_INTERVAL     = 30;  // slowest speed (-)

// ---------------------------------------------------------------
// Gait timing
// ---------------------------------------------------------------
inline constexpr unsigned long HOLD_MS = 150;   // pause after each gait sub-step
