// =====================================================================
// MAIN.CPP — ESP32 Flutter-Controlled Robot
// =====================================================================
// Dumb-but-safe servo actuator. Flutter is the brain.
// =====================================================================

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "config.h"
#include "buzzer.h"
#include "servo_engine.h"
#include "imu.h"
#include "gps_manager.h"
#include "wifi_manager.h"
#include "ws_manager.h"
#include "motion_queue.h"
#include "battery_monitor.h"
#include "telemetry.h"
#include "cmd_parser.h"

static AsyncWebServer server(80);
static bool g_lightState = false;

void setLightState(bool state) {
  g_lightState = state;
  if (LIGHT_PIN >= 0) {
    digitalWrite(LIGHT_PIN, g_lightState ? HIGH : LOW);
  }
}

bool getLightState() {
  return g_lightState;
}

static void testSingleServo(int index) {
  MotionCmd cmd1 = {-1, {-1, -1, -1, -1, -1, -1}, {100, 100, 100, 100, 100, 100}};
  MotionCmd cmd2 = {-1, {-1, -1, -1, -1, -1, -1}, {100, 100, 100, 100, 100, 100}};
  
  if (index == 4) { // S5 Slider
    cmd1.targetPos[index] = SLIDE_FORWARD;
    cmd2.targetPos[index] = SLIDE_HOME;
  } else if (index == 5) { // S6 Rotator
    cmd1.targetPos[index] = ROTATE_TURN;
    cmd2.targetPos[index] = ROTATE_HOME;
  } else { // Leg servos S1, S2, S3, S4
    cmd1.targetPos[index] = LIFT_POS[index];
    cmd2.targetPos[index] = HOME_POS[index];
  }
  
  motionEnqueue(cmd1);
  motionEnqueue(cmd2);
}

static String serialInputBuffer = "";

static void handleSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  // 1. Direct number input -> Move S3 Smart Servo directly (e.g. "2500", "500")
  bool isNumber = true;
  for (unsigned int i = 0; i < cmd.length(); i++) {
    if (!isDigit(cmd[i])) {
      isNumber = false;
      break;
    }
  }
  if (isNumber) {
    int target = cmd.toInt();
    motionClearQueue();
    setTarget(2, target);
    Serial.printf("\n>>> [S3 TARGET SET] Front-Right S3 moving to %d (clamped: %d)\n\n", target, clampPos(2, target));
    return;
  }

  cmd.toLowerCase();

  // 2. Light toggle command
  if (cmd == "light" || cmd == "led") {
    setLightState(!getLightState());
    Serial.printf("\n>>> [LIGHT] Headlight (GPIO %d) set to: %s\n\n",
                  LIGHT_PIN, getLightState() ? "ON" : "OFF");
    return;
  }

  // 3. Invert motor direction command ("inv", "invert", "dir")
  if (cmd == "inv" || cmd == "invert" || cmd == "dir") {
    s3ToggleInverted();
    Serial.printf("\n>>> [S3 POLARITY] Motor Direction Inverted is now: %s\n\n",
                  s3GetInverted() ? "TRUE (Inverted)" : "FALSE (Normal)");
    return;
  }

  // 4. Stop command ("stop") -> hold current position
  if (cmd == "stop") {
    motionClearQueue();
    setTarget(2, currentPos[2]);
    Serial.printf("\n>>> [S3 STOP] Holding position at %d\n\n", currentPos[2]);
    return;
  }

  // 5. Multi-argument format: "<servo_num> <position>" (e.g. "3 2500" or "5 1500" or "1 1500")
  int servoNum = -1, posVal = -1;
  if (sscanf(cmd.c_str(), "%d %d", &servoNum, &posVal) == 2) {
    if (servoNum >= 1 && servoNum <= 6) {
      int idx = servoNum - 1;
      motionClearQueue();
      setTarget(idx, posVal);
      Serial.printf("\n>>> [TARGET SET] Servo S%d (GPIO %d) target set to %d (clamped: %d)\n\n",
                    servoNum, (idx == 2 ? S3_PIN_IN1 : SERVO_PINS[idx]), posVal, clampPos(idx, posVal));
      return;
    }
  }

  // 6. Named servo with position: "s3 2000" or "s5 1500"
  int sIdx = -1, sPos = -1;
  if (sscanf(cmd.c_str(), "s%d %d", &sIdx, &sPos) == 2) {
    if (sIdx >= 1 && sIdx <= 6) {
      int idx = sIdx - 1;
      motionClearQueue();
      setTarget(idx, sPos);
      Serial.printf("\n>>> [TARGET SET] Servo S%d (GPIO %d) target set to %d (clamped: %d)\n\n",
                    sIdx, (idx == 2 ? S3_PIN_IN1 : SERVO_PINS[idx]), sPos, clampPos(idx, sPos));
      return;
    }
  }

  // 7. Test commands for individual servos
  if (cmd == "1" || cmd == "s1") {
    Serial.println("\n[TEST] Testing Servo 1 (Front-Left Leg - GPIO13)...");
    testSingleServo(0);
  } else if (cmd == "2" || cmd == "s2") {
    Serial.println("\n[TEST] Testing Servo 2 (Back-Right Leg - GPIO14)...");
    testSingleServo(1);
  } else if (cmd == "3" || cmd == "s3") {
    Serial.println("\n[TEST] Testing Servo 3 (Front-Right Smart Servo - GPIO18/19/33)...");
    testSingleServo(2);
  } else if (cmd == "4" || cmd == "s4") {
    Serial.println("\n[TEST] Testing Servo 4 (Back-Left Leg - GPIO26)...");
    testSingleServo(3);
  } else if (cmd == "5" || cmd == "s5") {
    Serial.println("\n[TEST] Testing Servo 5 (Slider - GPIO25)...");
    testSingleServo(4);
  } else if (cmd == "6" || cmd == "s6") {
    Serial.println("\n[TEST] Testing Servo 6 (Rotator - GPIO27)...");
    testSingleServo(5);
  } else if (cmd == "home") {
    Serial.println("\n[TEST] Moving all servos to HOME stance...");
    motionGoHome();
  } else if (cmd == "sit") {
    Serial.println("\n[TEST] Sitting down (all legs folded)...");
    sitDown();
  } else if (cmd == "status") {
    printPositions();
  } else {
    Serial.printf("\n[HELP] Commands: 'light' to toggle GPIO32 LED, type a number (e.g. '2500') to move S3, 'inv', 'stop', 'home', 'sit', or '1'..'6'.\n\n");
  }
}

// Non-blocking serial command processor (zero delay)
static void processSerialCommands() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialInputBuffer.length() > 0) {
        handleSerialCommand(serialInputBuffer);
        serialInputBuffer = "";
      }
    } else {
      serialInputBuffer += c;
    }
  }
}

void setup() {
  Serial.begin(115200);

  // 0. Initialize Action Light (if assigned)
  if (LIGHT_PIN >= 0) {
    pinMode(LIGHT_PIN, OUTPUT);
    digitalWrite(LIGHT_PIN, LOW);
  }

  // 1. Start Wi-Fi AP & WebServer FIRST so hotspot turns on unconditionally
  initWiFiAP();
  initWebSocket(server);
  server.begin();

  // 2. Initialize actuators and state
  initBuzzer();
  initBattery();
  initServos();
  motionQueueInit();

  // 3. Initialize sensors
  initIMU();
  initGPS();

  Serial.println();
  Serial.println("==========================================================");
  Serial.println("ESP32 Walking Bot - High-Performance Controller Ready!");
  Serial.println("==========================================================");
  Serial.println("Terminal Controls:");
  Serial.println("  light        -> Toggle Headlight on GPIO32");
  Serial.println("  <number>     -> Move S3 Front-Right directly (e.g. 2500, 1500, 75)");
  Serial.println("  inv          -> Toggle S3 Motor Direction Polarity");
  Serial.println("  stop         -> Hold S3 at current position");
  Serial.println("  <servo> <pos>-> Move any servo (e.g. '1 1500', '5 2000')");
  Serial.println("  1, 2, 3.. 6  -> Test individual servo");
  Serial.println("  home / sit   -> Stance controls");
  Serial.println("==========================================================");
}

void loop() {
  processSerialCommands();   // Non-blocking serial command processor
  updateServos();            // servo_engine: advance all servos one tick (100Hz PID for S3)
  motionTick();              // motion_queue: pop + start next command if idle
  tickWebSocket();           // ws_manager: cleanup stale clients, heartbeat check
  readIMU();                 // imu: sample sensors
  readGPS();                 // gps: process UART2 GPS stream
  telemetryTick();           // telemetry: broadcast at ~30 Hz

  // Stream live S3 potentiometer & lock status to Serial Monitor every 2000ms
  static unsigned long lastStreamMs = 0;
  if (millis() - lastStreamMs >= 2000) {
    lastStreamMs = millis();
    long raw, filtered, err;
    int pwm;
    bool locked, inv, stalled;
    getS3DebugInfo(raw, filtered, err, pwm, locked, inv, stalled);
    Serial.printf("[S3 POT: %4ld (raw: %4ld) | TGT: %4d | ERR: %+5ld | PWM: %+4d | %s%s]\n",
                  filtered, raw, targetPos[2], err, pwm,
                  locked ? "LOCKED" : (stalled ? "STALLED (backing off)" : "DRIVING"),
                  inv ? " [INV]" : "");
  }

  // Control Action Light (if assigned): HIGH if light state is ON OR executing motion
  if (LIGHT_PIN >= 0) {
    digitalWrite(LIGHT_PIN, (g_lightState || isMotionExecuting()) ? HIGH : LOW);
  }
}