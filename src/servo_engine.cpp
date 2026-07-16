// =====================================================================
// SERVO_ENGINE.CPP — Non-blocking smooth motion engine
// =====================================================================
// Owns the per-servo runtime state and drives all 6 servos toward their
// targets in small increments on every loop() tick — no delay() anywhere.
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
int           currentPos[NUM_SERVOS];
int           targetPos[NUM_SERVOS];
unsigned long lastStepTime[NUM_SERVOS];
unsigned long stepInterval = DEFAULT_INTERVAL;

// ---------------------------------------------------------------
// Smart Servo S3 specific state
// ---------------------------------------------------------------
static const int PWM_FREQ_HZ   = 20000;
static const int PWM_RES_BITS  = 8;
static const int PWM_MAX       = (1 << PWM_RES_BITS) - 1;
static const unsigned long S3_LOOP_PERIOD_MS = 10;
static const int S3_LEDC_CH1 = 14;
static const int S3_LEDC_CH2 = 15;

float s3_Kp = 0.55f;
float s3_Ki = 0.05f;
float s3_Kd = 0.08f;
int   s3_DEADBAND = 6;
int   s3_MIN_PWM = 70;
int   s3_MAX_PWM_STEP_PER_LOOP = 90;
float s3_INTEGRAL_LIMIT_PWM = 80.0f;
float s3_ADC_FILTER_ALPHA = 0.6f;

float s3_filteredAdc = 0;
bool  s3_filterInit = false;
float s3_integralAccum = 0;
long  s3_lastPosition = 0;
bool  s3_lastPositionInit = false;
int   s3_currentOutputPwm = 0;
unsigned long s3_lastLoopMs = 0;

long s3_circularDiff(long a, long b) {
  return a - b; 
}

void s3_driveMotor(int signedPwm) {
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
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (i == 2) {
      // Init Smart Servo S3
      analogReadResolution(12);          
      analogSetPinAttenuation(S3_PIN_POT, ADC_11db); 
      ledcSetup(S3_LEDC_CH1, PWM_FREQ_HZ, PWM_RES_BITS);
      ledcAttachPin(S3_PIN_IN1, S3_LEDC_CH1);
      ledcSetup(S3_LEDC_CH2, PWM_FREQ_HZ, PWM_RES_BITS);
      ledcAttachPin(S3_PIN_IN2, S3_LEDC_CH2);
      s3_driveMotor(0);
      
      long initial = analogRead(S3_PIN_POT);
      s3_filteredAdc = initial;
      s3_filterInit = true;
      s3_lastPosition = initial;
      s3_lastPositionInit = true;
      targetPos[i] = HOME_POS[i];
      currentPos[i] = initial;
      s3_lastLoopMs = millis();
    } else {
      servos[i].attach(SERVO_PINS[i]);
      currentPos[i]   = HOME_POS[i];
      targetPos[i]    = HOME_POS[i];
      lastStepTime[i] = 0;
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
// Queue a new target (always clamped).
// ---------------------------------------------------------------
void setTarget(int index, int value) {
  targetPos[index] = clampPos(index, value);
}

// ---------------------------------------------------------------
// Reached checks.
// ---------------------------------------------------------------
bool servoReached(int index) {
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
      // Smart Servo S3 Update
      if (now - s3_lastLoopMs < S3_LOOP_PERIOD_MS) continue; 
      float dt = (now - s3_lastLoopMs) / 1000.0f;
      s3_lastLoopMs = now;

      long raw = analogRead(S3_PIN_POT);
      if (!s3_filterInit) {
        s3_filteredAdc = raw;
        s3_filterInit = true;
      } else {
        s3_filteredAdc += s3_ADC_FILTER_ALPHA * (raw - s3_filteredAdc);
      }
      long position = (long)(s3_filteredAdc + 0.5f);
      currentPos[i] = position; // Expose for debugging

      long activeTarget = targetPos[i];
      if (position > 3980 || position < 20) {
        activeTarget = (position > 2048) ? 3950 : 50;
      }

      long error = s3_circularDiff(activeTarget, position);
      long absError = labs(error);

      int outputPwm = 0;
      if (absError <= s3_DEADBAND) {
        outputPwm = 0;
        s3_integralAccum = 0;
      } else {
        s3_integralAccum += error * dt;
        s3_integralAccum = constrain(s3_integralAccum, -s3_INTEGRAL_LIMIT_PWM / s3_Ki, s3_INTEGRAL_LIMIT_PWM / s3_Ki);
        
        float pTerm = s3_Kp * error;
        float iTerm = s3_Ki * s3_integralAccum;
        
        float dTerm = 0;
        if (s3_lastPositionInit) {
          long posDelta = s3_circularDiff(position, s3_lastPosition);
          dTerm = -s3_Kd * (posDelta / dt);
        }

        float desired = pTerm + iTerm + dTerm;
        if (desired > 0) {
          desired = constrain(desired + s3_MIN_PWM, s3_MIN_PWM, PWM_MAX);
        } else {
          desired = constrain(desired - s3_MIN_PWM, -PWM_MAX, -s3_MIN_PWM);
        }
        outputPwm = (int)desired;
      }

      int pwmDelta = outputPwm - s3_currentOutputPwm;
      pwmDelta = constrain(pwmDelta, -s3_MAX_PWM_STEP_PER_LOOP, s3_MAX_PWM_STEP_PER_LOOP);
      s3_currentOutputPwm += pwmDelta;
      s3_currentOutputPwm = constrain(s3_currentOutputPwm, -PWM_MAX, PWM_MAX);

      s3_driveMotor(s3_currentOutputPwm);
      s3_lastPosition = position;
      s3_lastPositionInit = true;
      
    } else {
      // Standard Servo Update
      if (currentPos[i] == targetPos[i]) continue;            // already there
      if (now - lastStepTime[i] < stepInterval) continue;     // not time yet

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
// Queue all servos back to their home positions.
// ---------------------------------------------------------------
void goHomeAll() {
  for (int i = 0; i < NUM_SERVOS; i++) {
    setTarget(i, HOME_POS[i]);
  }
}

// ---------------------------------------------------------------
// Queue all servos to their sit positions.
// ---------------------------------------------------------------
void sitDown() {
  for (int i = 0; i < NUM_SERVOS; i++) {
    setTarget(i, SIT_POS[i]);
  }
}


// ---------------------------------------------------------------
// Debug: dump current positions and speed to Serial.
// ---------------------------------------------------------------
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
  Serial.println("------------------------------");
}
