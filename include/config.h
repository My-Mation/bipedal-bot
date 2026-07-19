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
// Battery voltage monitor — GPIO35 (input-only ADC)
// Voltage divider: Battery+ → 30kΩ → GPIO35 → 10kΩ → GND
//   V_gpio = V_bat × (10 / (30+10)) = V_bat × 0.25
//   Full  (4.20 V) → GPIO35 = 1.050 V → ADC ≈ 1050/3300 × 4095 ≈ 1304
//   Empty (3.00 V) → GPIO35 = 0.750 V → ADC ≈  750/3300 × 4095 ≈  931
// ---------------------------------------------------------------
inline constexpr int  BAT_PIN       = 35;     // ADC1_CH7 (input-only)
inline constexpr float BAT_R_TOP    = 30000.f; // 30 kΩ upper resistor
inline constexpr float BAT_R_BOT    = 10000.f; // 10 kΩ lower resistor
inline constexpr float BAT_DIVIDER  = BAT_R_BOT / (BAT_R_TOP + BAT_R_BOT); // 0.25
inline constexpr float BAT_VREF     = 3.3f;    // ESP32 ADC reference
inline constexpr float BAT_ADC_MAX  = 4095.f;  // 12-bit ADC
inline constexpr float BAT_FULL_V   = 4.20f;   // Li-ion full charge voltage
inline constexpr float BAT_EMPTY_V  = 3.00f;   // Li-ion cutoff voltage
inline constexpr int   BAT_SAMPLES  = 16;      // oversampling count

// ---------------------------------------------------------------
// Calibrated home positions
// NOTE: S1,S2,S4,S5,S6 are in µs pulse width.
//       S3 (Smart Servo) uses RAW ADC COUNTS (0–4095) — NOT µs.
//       SIT_POS[2]=75 (ADC) is the correct reference for the scale.
// ---------------------------------------------------------------
//                                S1     S2     S3     S4    S5     S6
inline constexpr int HOME_POS[NUM_SERVOS] = {2350,  500, 2000, 1500, 2500, 2500};

// ---------------------------------------------------------------
// Maximum safe lift position for each leg servo.
// S3 value is in ADC counts (0–4095); all others are µs.
// Indices 4 & 5 are placeholders — the slider/rotator are never lifted.
// ---------------------------------------------------------------
// LIFT = leg raised in air (used during gait swing phase)
//                                  S1     S2     S3    S4     S5     S6
inline constexpr int LIFT_POS[NUM_SERVOS] = {1000, 1100, 1000, 1000, 2500, 2500};

// ---------------------------------------------------------------
// Sit position — all legs folded, body lowered to ground (µs)
// ---------------------------------------------------------------
inline constexpr int SIT_POS[NUM_SERVOS] = {
    1000,   // S1 — Front-Left  leg fully bent
    1600,   // S2 — Back-Right  leg fully bent
    75,     // S3 — Front-Right leg fully bent (Smart Servo ADC)
     500,   // S4 — Back-Left   leg fully bent
    2500,   // S5 — Slider at home
    2500    // S6 — Rotator at home
};

// ---------------------------------------------------------------
// Slider (S5) waypoints
// ---------------------------------------------------------------
inline constexpr int SLIDE_HOME    = 2500;
inline constexpr int SLIDE_FORWARD = 1200;   // max extension (user measured)

// ---------------------------------------------------------------
// Rotator (S6) waypoints
// ---------------------------------------------------------------
inline constexpr int ROTATE_HOME = 2500;
inline constexpr int ROTATE_TURN = 1700;

// ---------------------------------------------------------------
// Hard safety clamps — positions are clamped to these before any
// writeMicroseconds() call.  Prevents mechanical damage.
// ---------------------------------------------------------------
//                                     S1     S2    S3    S4    S5     S6
inline constexpr int MIN_POS[NUM_SERVOS] = {1000,  500,   50,  500, 1200, 1700};
inline constexpr int MAX_POS[NUM_SERVOS] = {2500, 1600, 3950, 1500, 2500, 2500};

// ---------------------------------------------------------------
// S3 Smart Servo — reached() tolerance.
// servoReached() for S3 uses abs(current - target) <= this value
// instead of strict equality, because the ADC is noisy.
// ---------------------------------------------------------------
inline constexpr int S3_REACHED_DEADBAND = 30;  // ADC counts (~0.7 % of 4095)

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
