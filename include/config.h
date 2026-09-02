#pragma once
#include <stdint.h>

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
#define S6 5   // Rotator (left / right // ---------------------------------------------------------------
// GPIO pin assignments
// ---------------------------------------------------------------
// Servo 1 (S1) Front-Left:  GPIO13
// Servo 2 (S2) Back-Right:  GPIO14
// Servo 3 (S3) Front-Right: Smart Servo (IN1=GPIO18, IN2=GPIO19, POT=GPIO33) -> -1 in PWM array
// Servo 4 (S4) Back-Left:   GPIO26
// Servo 5 (S5) Slider:      GPIO25
// Servo 6 (S6) Rotator:     GPIO27
inline constexpr int SERVO_PINS[NUM_SERVOS] = {13, 14, -1, 26, 25, 27};

// ---------------------------------------------------------------
// S3 Smart Servo (MX1508 + Potentiometer) Pins
// ---------------------------------------------------------------
inline constexpr int S3_PIN_POT = 33; // Internal potentiometer wiper / position feedback (GPIO33)
inline constexpr int S3_PIN_IN1 = 18; // Motor driver IN1 (PWM)
inline constexpr int S3_PIN_IN2 = 19; // Motor driver IN2 (PWM)

// ---------------------------------------------------------------
// MPU6050 I2C Pins
// ---------------------------------------------------------------
inline constexpr int MPU_SDA_PIN = 21;
inline constexpr int MPU_SCL_PIN = 22;

// ---------------------------------------------------------------
// GPS — GY-GPS6MV2 Pins (UART2)
// ---------------------------------------------------------------
inline constexpr int GPS_RX_PIN = 4;  // ESP32 GPIO4 (connects to GPS TX)
inline constexpr int GPS_TX_PIN = 17; // ESP32 GPIO17 (UART2 TX pin, can leave disconnected)
inline constexpr uint32_t GPS_BAUD = 9600;

// ---------------------------------------------------------------
// Buzzer Pin (NPN Transistor Drive via 1kΩ Resistor)
// ---------------------------------------------------------------
inline constexpr int BUZZER_PIN = 23; // Active HIGH control to BC548B base

// ---------------------------------------------------------------
// Headlight / Action Status Indicator Pin (-1 if disabled)
// ---------------------------------------------------------------
inline constexpr int LIGHT_PIN = 32; // GPIO32 Headlight / Action Status LED

// ---------------------------------------------------------------
// Battery Voltage Monitor — RESERVED FOR FUTURE (NOT ACTIVE CURRENTLY)
// GPIO35 is reserved for future battery monitoring; DO NOT read now.
// ---------------------------------------------------------------
inline constexpr int   BAT_PIN_RESERVED = 35;  // Reserved (not read)
inline constexpr float BAT_FULL_V       = 8.40f; // 2S Li-ion full charge voltage
inline constexpr float BAT_EMPTY_V      = 6.00f; // 2S Li-ion cutoff voltage

// ---------------------------------------------------------------
// Calibrated home positions (standing stance)
// NOTE: S1,S2,S4,S5,S6 are in µs pulse width.
//       S3 (Smart Servo) uses RAW ADC COUNTS (0–4095) — NOT µs.
// ---------------------------------------------------------------
//                                S1     S2     S3     S4    S5     S6
inline constexpr int HOME_POS[NUM_SERVOS] = {2350,  650, 1950, 2350, 2500, 2500};

// ---------------------------------------------------------------
// Maximum safe lift position for each leg servo.
// S3 value is in ADC counts (0–4095); all others are µs.
// ---------------------------------------------------------------
//                                  S1     S2     S3    S4     S5     S6
inline constexpr int LIFT_POS[NUM_SERVOS] = {1000, 1800, 2800, 1170, 2500, 2500};

// ---------------------------------------------------------------
// Sit position — all legs folded, body lowered to ground (µs)
// ---------------------------------------------------------------
inline constexpr int SIT_POS[NUM_SERVOS] = {
    1000,   // S1 — Front-Left  leg fully bent
    1800,   // S2 — Back-Right  leg fully bent
    3700,   // S3 — Front-Right leg fully bent (Smart Servo ADC)
    1170,   // S4 — Back-Left   leg fully bent
    2500,   // S5 — Slider at home
    2500    // S6 — Rotator at home
};

// ---------------------------------------------------------------
// Slider (S5) waypoints
// ---------------------------------------------------------------
inline constexpr int SLIDE_HOME    = 2500;
inline constexpr int SLIDE_FORWARD = 1450;   // max extension (user measured)

// ---------------------------------------------------------------
// Rotator (S6) waypoints
// ---------------------------------------------------------------
inline constexpr int ROTATE_HOME = 2500;
inline constexpr int ROTATE_TURN = 1700;

// ---------------------------------------------------------------
// Hard safety clamps — positions are clamped to these before any
// writeMicroseconds() call.  Prevents mechanical damage.
// ---------------------------------------------------------------
inline constexpr int MIN_POS[NUM_SERVOS] = {1000,  500,   50, 1170, 1450, 1700};
inline constexpr int MAX_POS[NUM_SERVOS] = {2500, 2500, 3950, 2500, 2500, 2500};

// ---------------------------------------------------------------
// S3 Smart Servo — reached() tolerance.
// servoReached() for S3 uses abs(current - target) <= this value
// instead of strict equality, because the ADC is noisy.
// ---------------------------------------------------------------
inline constexpr int S3_REACHED_DEADBAND = 20;  // ADC counts (~0.5 % of 4095)

// ---------------------------------------------------------------
// Motion-engine tuning (Fast, snappy & smooth)
// ---------------------------------------------------------------
inline constexpr int           STEP_SIZE        = 25;  // µs moved per motion tick (snappy & responsive)
inline constexpr unsigned long DEFAULT_INTERVAL = 3;   // ms between ticks
inline constexpr unsigned long MIN_INTERVAL     = 1;   // fastest speed (100%)
inline constexpr unsigned long MAX_INTERVAL     = 10;  // slowest speed (1%)

// ---------------------------------------------------------------
// Gait timing
// ---------------------------------------------------------------
inline constexpr unsigned long HOLD_MS = 150;   // pause after each gait sub-step

// ---------------------------------------------------------------
// Motion queue
// ---------------------------------------------------------------
inline constexpr int MOTION_QUEUE_SIZE = 32;  // max queued move commands

// ---------------------------------------------------------------
// Heartbeat safety
// ---------------------------------------------------------------
inline constexpr unsigned long HEARTBEAT_TIMEOUT_MS  = 3000; // ms without heartbeat → ESTOP (3s margin)
inline constexpr unsigned long DISCONNECT_GRACE_MS   = 3000; // ms after WS disconnect before ESTOP
