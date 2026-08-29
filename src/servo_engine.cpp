// =====================================================================
// SERVO_ENGINE.CPP — Non-blocking smooth motion engine
// =====================================================================
// Owns the per-servo runtime state and drives all 6 servos toward
// their targets in small increments on every loop() tick.
//
// S3 (index 2) is a DC motor with a potentiometer, driven by a
// MX1508 H-bridge on LEDC channels 14/15.  Its position is read as
// a 12-bit ADC count.  The PID loop runs every S3_LOOP_PERIOD_MS ms.
// Speed is NOT controllable for S3 in v1 (PID self-regulates).
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
// Per-servo step interval (ms between motion ticks).
// Controls speed independently for each servo.
// ---------------------------------------------------------------
static unsigned long perServoInterval[NUM_SERVOS];
static unsigned long lastStepTime[NUM_SERVOS];

// ---------------------------------------------------------------
// Speed (1–100) → step interval (ms) mapping
// speed=100 → MIN_INTERVAL (fastest), speed=1 → MAX_INTERVAL (slowest)
// ---------------------------------------------------------------
static unsigned long speedToInterval(int speed) {
  speed = constrain(speed, 1, 100);
  // Linear interpolation: 100→MIN_INTERVAL, 1→MAX_INTERVAL
  return (unsigned long)(MAX_INTERVAL - ((speed - 1) * (MAX_INTERVAL - MIN_INTERVAL)) / 99);
}

// ---------------------------------------------------------------
// Smart Servo S3 — PID state
// ---------------------------------------------------------------
static const int PWM_FREQ_HZ          = 20000;
static const int PWM_RES_BITS         = 8;
static const int PWM_MAX              = (1 << PWM_RES_BITS) - 1;
static const unsigned long S3_LOOP_PERIOD_MS = 10;
static const int S3_LEDC_CH1          = 14;
static const int S3_LEDC_CH2          = 15;

static float s3_Kp                    = 0.55f;
static float s3_Ki                    = 0.05f;
static float s3_Kd                    = 0.08f;
static int   s3_DEADBAND              = 30;
static int   s3_MIN_PWM               = 70;
static int   s3_MAX_PWM_STEP          = 90;
static float s3_INTEGRAL_LIMIT        = 80.0f;
static float s3_ADC_ALPHA             = 0.6f;

static float         s3_filteredAdc   = 0;
static bool          s3_filterInit    = false;
static float         s3_integral      = 0;
static long          s3_lastPosition  = 0;
static bool          s3_lastPosInit   = false;
static int           s3_outputPwm     = 0;
static unsigned long s3_lastLoopMs    = 0;

static void s3_driveMotor(int signedPwm) {
  signedPwm = -signedPwm; // Inverted motor polarity to match potentiometer feedback
  signedPwm = constrain(signedPwm, -PWM_MAX, PWM_MAX);
  if (signedPwm > 0) {
    ledcWrite(S3_LEDC_CH1, 0);
    ledcWrite(S3_LEDC_CH2, signedPwm);
  } else if (signedPwm < 0) {
    ledcWrite(S3_LEDC_CH1, -signedPwm);
    ledcWrite(S3_LEDC_CH2, 0);
  } else {
    ledcWrite(S3_LEDC_CH1, 0);
    ledcWrite(S3_LEDC_CH2, 0);
  }
}

// ---------------------------------------------------------------
// Initialise — attach, write home, prime state arrays.
// ---------------------------------------------------------------
void initServos() {
  Serial.println("==========================================");
  Serial.println("Servo GPIO Pin Mappings:");
  Serial.printf("  S1 (Front-Left Leg)  : GPIO %d\n", SERVO_PINS[0]);
  Serial.printf("  S2 (Back-Right Leg)  : GPIO %d\n", SERVO_PINS[1]);
  Serial.printf("  S3 (Front-Right Smart): IN1=GPIO%d, IN2=GPIO%d, POT=GPIO%d\n", S3_PIN_IN1, S3_PIN_IN2, S3_PIN_POT);
  Serial.printf("  S4 (Back-Left Leg)   : GPIO %d\n", SERVO_PINS[3]);
  Serial.printf("  S5 (Slider)          : GPIO %d\n", SERVO_PINS[4]);
  Serial.printf("  S6 (Rotator)         : GPIO %d\n", SERVO_PINS[5]);
  Serial.println("==========================================");

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

      long initial    = analogRead(S3_PIN_POT);
      s3_filteredAdc  = initial;
      s3_filterInit   = true;
      s3_lastPosition = initial;
      s3_lastPosInit  = true;
      s3_lastLoopMs   = millis();
      targetPos[i]    = HOME_POS[i];
      currentPos[i]   = (int)initial;
    } else {
      servos[i].attach(SERVO_PINS[i]);
      currentPos[i] = HOME_POS[i];
      targetPos[i]  = HOME_POS[i];
      servos[i].writeMicroseconds(currentPos[i]);
    }
  }
}

// ---------------------------------------------------------------
// Clamp to hardware-safe range.
// ---------------------------------------------------------------
int clampPos(int index, int value) {
  if (value < MIN_POS[index]) value = MIN_POS[index];
  if (value > MAX_POS[index]) value = MAX_POS[index];
  return value;
}

// ---------------------------------------------------------------
// Set target — clamped, default speed.
// ---------------------------------------------------------------
void setTarget(int index, int value) {
  targetPos[index] = clampPos(index, value);
}

// ---------------------------------------------------------------
// Set target — clamped, per-servo speed (1–100).
// Speed ignored for S3.
// ---------------------------------------------------------------
void setTargetWithSpeed(int index, int value, int speed) {
  targetPos[index] = clampPos(index, value);
  if (index != 2) {
    perServoInterval[index] = speedToInterval(speed);
  }
}

// ---------------------------------------------------------------
// Reached checks.
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
// Motion tick — advance each servo one step toward its target.
// Must be called every loop() iteration.
// ---------------------------------------------------------------
void updateServos() {
  unsigned long now = millis();

  for (int i = 0; i < NUM_SERVOS; i++) {

    if (i == 2) {
      // ── S3: PID-controlled DC motor ───────────────────────────
      if (now - s3_lastLoopMs < S3_LOOP_PERIOD_MS) continue;
      float dt    = (now - s3_lastLoopMs) / 1000.0f;
      s3_lastLoopMs = now;

      long raw = analogRead(S3_PIN_POT);
      if (!s3_filterInit) {
        s3_filteredAdc = raw; s3_filterInit = true;
      } else {
        s3_filteredAdc += s3_ADC_ALPHA * (raw - s3_filteredAdc);
      }
      long position = (long)(s3_filteredAdc + 0.5f);
      currentPos[i] = (int)position;

      long activeTarget = targetPos[i];
      if (position > 3980 || position < 20) {
        activeTarget = (position > 2048) ? 3950 : 50;
      }

      long error   = activeTarget - position;
      long absErr  = labs(error);
      int  out     = 0;

      if (raw < 50 || raw > 4040 || absErr <= s3_DEADBAND) {
        s3_integral = 0;
        s3_outputPwm = 0;
        s3_driveMotor(0);
      } else {
        s3_integral += error * dt;
        s3_integral  = constrain(s3_integral,
                                 -s3_INTEGRAL_LIMIT / s3_Ki,
                                  s3_INTEGRAL_LIMIT / s3_Ki);
        float dTerm = 0;
        if (s3_lastPosInit) {
          dTerm = -s3_Kd * ((position - s3_lastPosition) / dt);
        }
        float desired = s3_Kp * error + s3_Ki * s3_integral + dTerm;
        if (desired > 0) desired = constrain(desired + s3_MIN_PWM, s3_MIN_PWM, PWM_MAX);
        else             desired = constrain(desired - s3_MIN_PWM, -PWM_MAX, -s3_MIN_PWM);
        out = (int)desired;

        int delta    = constrain(out - s3_outputPwm, -s3_MAX_PWM_STEP, s3_MAX_PWM_STEP);
        s3_outputPwm = constrain(s3_outputPwm + delta, -PWM_MAX, PWM_MAX);
        s3_driveMotor(s3_outputPwm);
      }
      s3_lastPosition = position;
      s3_lastPosInit  = true;

    } else {
      // ── Standard PWM servo ────────────────────────────────────
      if (currentPos[i] == targetPos[i]) continue;
      if (now - lastStepTime[i] < perServoInterval[i]) continue;

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
}

// ---------------------------------------------------------------
// Go home (default speed).
// ---------------------------------------------------------------
void goHomeAll() {
  for (int i = 0; i < NUM_SERVOS; i++) setTarget(i, HOME_POS[i]);
}

// ---------------------------------------------------------------
// Sit down (default speed).
// ---------------------------------------------------------------
void sitDown() {
  for (int i = 0; i < NUM_SERVOS; i++) setTarget(i, SIT_POS[i]);
}

// ---------------------------------------------------------------
// Debug dump.
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
