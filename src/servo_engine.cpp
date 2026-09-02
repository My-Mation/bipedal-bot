// =====================================================================
// SERVO_ENGINE.CPP — High-Performance Motion Engine & S3 Smart Servo
// =====================================================================
// Controls 5 standard PWM servos with fast, snappy interpolation and
// 1 closed-loop Smart Servo (S3: DC motor + potentiometer feedback)
// with high-torque PID holding lock, anti-stall, and strict linear tracking.
// =====================================================================

#include <ESP32Servo.h>
#include "servo_engine.h"

// ---------------------------------------------------------------
// Internal servo objects
// ---------------------------------------------------------------
static Servo servos[NUM_SERVOS];

// ---------------------------------------------------------------
// Per-servo runtime state (declared extern in servo_engine.h)
// ---------------------------------------------------------------
int currentPos[NUM_SERVOS];
int targetPos[NUM_SERVOS];

// ---------------------------------------------------------------
// Per-servo step interval (ms between motion ticks)
// ---------------------------------------------------------------
static unsigned long perServoInterval[NUM_SERVOS];
static unsigned long lastStepTime[NUM_SERVOS];

// ---------------------------------------------------------------
// Speed (1–100) → step interval (ms) mapping
// ---------------------------------------------------------------
static unsigned long speedToInterval(int speed) {
  speed = constrain(speed, 1, 100);
  return (unsigned long)(MAX_INTERVAL - ((speed - 1) * (MAX_INTERVAL - MIN_INTERVAL)) / 99);
}

// ---------------------------------------------------------------
// Smart Servo S3 — Closed Loop PID & Hardware State
// ---------------------------------------------------------------
static const int PWM_FREQ_HZ                 = 20000;
static const int PWM_RES_BITS                = 8;
static const int PWM_MAX                     = (1 << PWM_RES_BITS) - 1;
static const unsigned long S3_LOOP_PERIOD_MS = 10; // 100 Hz control loop

static const int S3_LEDC_CH1                 = 14;
static const int S3_LEDC_CH2                 = 15;

// Tunable PID & dynamic holding parameters
static float s3_Kp             = 0.75f;  // High holding torque stiffness
static float s3_Ki             = 0.08f;  // High integral authority against manual deflection
static float s3_Kd             = 0.06f;  // Active damping to eliminate overshoot
static int   s3_DEADBAND       = 5;      // Tight deadband for rock-solid lock
static int   s3_MIN_PWM        = 75;     // Base voltage to instantly overcome gear friction
static int   s3_MAX_PWM_STEP   = 120;    // Rapid acceleration / counter-force response
static float s3_INTEGRAL_LIMIT = 120.0f; // Max integral correction duty
static float s3_ADC_ALPHA      = 0.6f;   // Fast ADC low-pass filter

// Runtime state
static float         s3_filteredAdc    = 0;
static bool          s3_filterInit     = false;
static float         s3_integral       = 0;
static long          s3_lastPosition   = 0;
static bool          s3_lastPosInit    = false;
static int           s3_outputPwm      = 0;
static unsigned long s3_lastLoopMs     = 0;
static long          s3_lastRawAdc     = 0;
static long          s3_lastError      = 0;
static bool          s3_isLocked       = false;
static bool          s3_isStalled      = false;
static bool          s3_invertMotor    = false; // Configurable motor direction

// Stall detection state
static unsigned long s3_stallTimer     = 0;
static long          s3_stallSamplePos = 0;

// ---------------------------------------------------------------
// Potentiometer Multi-Sample Median Filter with Glitch Rejection
// ---------------------------------------------------------------
static long s3_readFilteredPot() {
  long samples[5];
  for (int i = 0; i < 5; i++) {
    samples[i] = analogRead(S3_PIN_POT);
    delayMicroseconds(40);
  }
  // Insertion sort for median
  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 5; j++) {
      if (samples[i] > samples[j]) {
        long tmp = samples[i];
        samples[i] = samples[j];
        samples[j] = tmp;
      }
    }
  }
  long median = samples[2];

  // Glitch rejection: Ignore instantaneous dropouts to 0 caused by motor PWM spikes
  if (s3_lastPosInit && s3_lastPosition > 200 && median < 20) {
    return s3_lastPosition;
  }
  return median;
}

// ---------------------------------------------------------------
// Motor driver output
// Phase-aligned: signedPwm > 0 increases ADC, signedPwm < 0 decreases ADC
// ---------------------------------------------------------------
static void s3_driveMotor(int signedPwm) {
  if (s3_invertMotor) {
    signedPwm = -signedPwm;
  }
  signedPwm = constrain(signedPwm, -PWM_MAX, PWM_MAX);

  if (signedPwm > 0) {
    // Rotate in direction that INCREASES potentiometer ADC count
    ledcWrite(S3_LEDC_CH1, signedPwm);
    ledcWrite(S3_LEDC_CH2, 0);
  } else if (signedPwm < 0) {
    // Rotate in direction that DECREASES potentiometer ADC count
    ledcWrite(S3_LEDC_CH1, 0);
    ledcWrite(S3_LEDC_CH2, -signedPwm);
  } else {
    ledcWrite(S3_LEDC_CH1, 0);
    ledcWrite(S3_LEDC_CH2, 0);
  }
}

// ---------------------------------------------------------------
// Initialise all servos
// ---------------------------------------------------------------
void initServos() {
  Serial.println("==========================================");
  Serial.println("Initializing Servo Motion Engine:");
  Serial.printf("  S1 (Front-Left Leg)  : GPIO %d\n", SERVO_PINS[0]);
  Serial.printf("  S2 (Back-Right Leg)  : GPIO %d\n", SERVO_PINS[1]);
  Serial.printf("  S3 (Front-Right Smart): IN1=GPIO%d, IN2=GPIO%d, POT=GPIO%d\n", S3_PIN_IN1, S3_PIN_IN2, S3_PIN_POT);
  Serial.printf("  S4 (Back-Left Leg)   : GPIO %d\n", SERVO_PINS[3]);
  Serial.printf("  S5 (Slider / Leg)    : GPIO %d\n", SERVO_PINS[4]);
  Serial.printf("  S6 (Rotator / Turn)  : GPIO %d\n", SERVO_PINS[5]);
  Serial.println("==========================================");

  // Allocate all timers for ESP32Servo to support 5+ PWM channels
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  for (int i = 0; i < NUM_SERVOS; i++) {
    perServoInterval[i] = DEFAULT_INTERVAL;
    lastStepTime[i]     = 0;

    if (i == 2) {
      // S3 — DC motor with potentiometer feedback
      analogReadResolution(12);
      analogSetPinAttenuation(S3_PIN_POT, ADC_11db);
      ledcSetup(S3_LEDC_CH1, PWM_FREQ_HZ, PWM_RES_BITS);
      ledcAttachPin(S3_PIN_IN1, S3_LEDC_CH1);
      ledcSetup(S3_LEDC_CH2, PWM_FREQ_HZ, PWM_RES_BITS);
      ledcAttachPin(S3_PIN_IN2, S3_LEDC_CH2);
      s3_driveMotor(0);

      long initial       = s3_readFilteredPot();
      s3_lastRawAdc      = initial;
      s3_filteredAdc     = initial;
      s3_filterInit      = true;
      s3_lastPosition    = initial;
      s3_lastPosInit     = true;
      s3_stallSamplePos  = initial;
      s3_stallTimer      = millis();
      s3_lastLoopMs      = millis();
      targetPos[i]       = HOME_POS[i];
      currentPos[i]      = (int)initial;
    } else {
      servos[i].setPeriodHertz(50);
      int ch = servos[i].attach(SERVO_PINS[i], 500, 2500);
      Serial.printf("  Attached Servo S%d (GPIO %d) -> PWM Channel %d\n", i + 1, SERVO_PINS[i], ch);
      currentPos[i] = HOME_POS[i];
      targetPos[i]  = HOME_POS[i];
      servos[i].writeMicroseconds(currentPos[i]);
    }
  }
}

// ---------------------------------------------------------------
// Clamp to hardware-safe range
// ---------------------------------------------------------------
int clampPos(int index, int value) {
  if (value < MIN_POS[index]) value = MIN_POS[index];
  if (value > MAX_POS[index]) value = MAX_POS[index];
  return value;
}

// ---------------------------------------------------------------
// Set target — clamped, default speed
// ---------------------------------------------------------------
void setTarget(int index, int value) {
  targetPos[index] = clampPos(index, value);
  if (index == 2) {
    s3_integral = 0; // Clear integral windup on new target
    s3_isStalled = false;
    s3_stallTimer = millis();
    s3_stallSamplePos = currentPos[2];
  }
}

// ---------------------------------------------------------------
// Set target — clamped, per-servo speed (1–100)
// ---------------------------------------------------------------
void setTargetWithSpeed(int index, int value, int speed) {
  targetPos[index] = clampPos(index, value);
  if (index == 2) {
    s3_integral = 0; // Clear integral windup on new target
    s3_isStalled = false;
    s3_stallTimer = millis();
    s3_stallSamplePos = currentPos[2];
  } else {
    perServoInterval[index] = speedToInterval(speed);
  }
}

// ---------------------------------------------------------------
// Reached checks
// ---------------------------------------------------------------
bool servoReached(int index) {
  if (index == 2) {
    return abs(currentPos[index] - targetPos[index]) <= S3_REACHED_DEADBAND;
  }
  return currentPos[index] == targetPos[index];
}

bool pairReached(int a, int b) {
  return servoReached(a) && servoReached(b);
}

bool allServosIdle() {
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (!servoReached(i)) return false;
  }
  return true;
}

// ---------------------------------------------------------------
// S3 Smart Servo Inspection & Control Helpers
// ---------------------------------------------------------------
void getS3DebugInfo(long &rawAdc, long &filteredAdc, long &error, int &outputPwm, bool &locked, bool &inverted, bool &stalled) {
  rawAdc      = s3_lastRawAdc;
  filteredAdc = (long)(s3_filteredAdc + 0.5f);
  error       = s3_lastError;
  outputPwm   = s3_outputPwm;
  locked      = s3_isLocked;
  inverted    = s3_invertMotor;
  stalled     = s3_isStalled;
}

void s3SetInverted(bool inv) {
  s3_invertMotor = inv;
  s3_integral = 0;
}

bool s3GetInverted() {
  return s3_invertMotor;
}

void s3ToggleInverted() {
  s3_invertMotor = !s3_invertMotor;
  s3_integral = 0;
}

void s3SetGains(float kp, float ki, float kd, int minPwm, int deadband) {
  s3_Kp = kp;
  s3_Ki = ki;
  s3_Kd = kd;
  s3_MIN_PWM = minPwm;
  s3_DEADBAND = deadband;
  s3_integral = 0;
}

// ---------------------------------------------------------------
// Motion tick — advance all servos toward their targets
// ---------------------------------------------------------------
void updateServos() {
  unsigned long now = millis();

  for (int i = 0; i < NUM_SERVOS; i++) {

    if (i == 2) {
      // ── S3: Closed-Loop Smart Servo ───────────────────────────
      if (now - s3_lastLoopMs < S3_LOOP_PERIOD_MS) continue;
      float dt = (now - s3_lastLoopMs) / 1000.0f;
      s3_lastLoopMs = now;

      // 1. Read potentiometer (5-sample median + glitch rejection) & smooth
      long raw = s3_readFilteredPot();
      s3_lastRawAdc = raw;
      if (!s3_filterInit) {
        s3_filteredAdc = raw;
        s3_filterInit = true;
      } else {
        s3_filteredAdc += s3_ADC_ALPHA * (raw - s3_filteredAdc);
      }
      long position = (long)(s3_filteredAdc + 0.5f);
      currentPos[i] = (int)position;

      // 2. Strict Linear Target & Error Calculation
      // Clamped strictly to physical safe bounds so it NEVER drives past limits
      long target = constrain(targetPos[i], MIN_POS[i], MAX_POS[i]);
      targetPos[i] = target;

      // Strictly linear tracking (NO circular wrap / NO modulo)
      long error = target - position;
      s3_lastError = error;
      long absError = labs(error);

      int desiredOutputPwm = 0;

      // 3. Active Holding Lock & Anti-Stall Logic
      if (absError <= s3_DEADBAND) {
        // Firmly in position -> Lock mode (zero active drive, clear integrator)
        desiredOutputPwm = 0;
        s3_integral = 0;
        s3_isLocked = true;
        s3_isStalled = false;
        s3_stallTimer = now;
        s3_stallSamplePos = position;
      } else {
        s3_isLocked = false;

        // Stall check: If commanding power for > 350ms but position hasn't budged
        if (abs(s3_outputPwm) >= s3_MIN_PWM) {
          if (abs(position - s3_stallSamplePos) <= 4) {
            if (now - s3_stallTimer > 350) {
              s3_isStalled = true;
            }
          } else {
            s3_stallSamplePos = position;
            s3_stallTimer = now;
            s3_isStalled = false;
          }
        } else {
          s3_stallTimer = now;
          s3_stallSamplePos = position;
        }

        if (s3_isStalled) {
          // If physically stuck, back off slightly to protect gears/motor
          desiredOutputPwm = (error > 0) ? -s3_MIN_PWM : s3_MIN_PWM;
        } else {
          // Normal PID calculation with anti-windup
          s3_integral += error * dt;
          s3_integral = constrain(s3_integral, -s3_INTEGRAL_LIMIT / s3_Ki, s3_INTEGRAL_LIMIT / s3_Ki);

          float pTerm = s3_Kp * error;
          float iTerm = s3_Ki * s3_integral;
          float dTerm = 0;
          if (s3_lastPosInit && dt > 0.0001f) {
            dTerm = -s3_Kd * ((position - s3_lastPosition) / dt);
          }

          float desired = pTerm + iTerm + dTerm;

          // Friction compensation: provide immediate overcome-friction voltage
          if (desired > 0) {
            desired = constrain(desired + s3_MIN_PWM, s3_MIN_PWM, PWM_MAX);
          } else if (desired < 0) {
            desired = constrain(desired - s3_MIN_PWM, -PWM_MAX, -s3_MIN_PWM);
          }

          desiredOutputPwm = (int)desired;
        }
      }

      // 4. Hard Safety Endstop Protection
      // Cannot drive forward if already at/past MAX_POS, cannot drive backward if at/below MIN_POS
      if (position >= MAX_POS[i] && desiredOutputPwm > 0) {
        desiredOutputPwm = 0;
        s3_integral = 0;
      }
      if (position <= MIN_POS[i] && desiredOutputPwm < 0) {
        desiredOutputPwm = 0;
        s3_integral = 0;
      }

      // 5. Acceleration / Slew Rate Limiter (Snappy & controlled)
      int delta = constrain(desiredOutputPwm - s3_outputPwm, -s3_MAX_PWM_STEP, s3_MAX_PWM_STEP);
      s3_outputPwm = constrain(s3_outputPwm + delta, -PWM_MAX, PWM_MAX);

      // 6. Drive Motor
      s3_driveMotor(s3_outputPwm);

      s3_lastPosition = position;
      s3_lastPosInit  = true;

    } else {
      // ── Standard PWM Servos (S1, S2, S4, S5, S6) ─────────────
      if (currentPos[i] == targetPos[i]) continue;
      if (now - lastStepTime[i] < perServoInterval[i]) continue;

      int delta = targetPos[i] - currentPos[i];
      if (abs(delta) <= STEP_SIZE) {
        currentPos[i] = targetPos[i];
      } else if (delta > 0) {
        currentPos[i] += STEP_SIZE;
      } else {
        currentPos[i] -= STEP_SIZE;
      }

      servos[i].writeMicroseconds(currentPos[i]);
      lastStepTime[i] = now;
    }
  }
}

// ---------------------------------------------------------------
// Go home (fast & smooth)
// ---------------------------------------------------------------
void goHomeAll() {
  for (int i = 0; i < NUM_SERVOS; i++) setTarget(i, HOME_POS[i]);
}

// ---------------------------------------------------------------
// Sit down (fast & smooth)
// ---------------------------------------------------------------
void sitDown() {
  for (int i = 0; i < NUM_SERVOS; i++) setTarget(i, SIT_POS[i]);
}

// ---------------------------------------------------------------
// Debug dump
// ---------------------------------------------------------------
void printPositions() {
  const char* names[NUM_SERVOS] = {"S1","S2","S3(ADC)","S4","S5","S6"};
  Serial.println("---- Servo Positions ----");
  for (int i = 0; i < NUM_SERVOS; i++) {
    Serial.printf("  %s  cur=%d  tgt=%d  interval=%lums\n",
                  names[i], currentPos[i], targetPos[i], perServoInterval[i]);
  }
  Serial.println("-------------------------");
}

