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

static AsyncWebServer server(80);

static void testSingleServo(int index) {
  MotionCmd cmd1 = {-1, {-1, -1, -1, -1, -1, -1}, {50, 50, 50, 50, 50, 50}};
  MotionCmd cmd2 = {-1, {-1, -1, -1, -1, -1, -1}, {50, 50, 50, 50, 50, 50}};
  
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

static void processSerialCommands() {
  while (Serial.available() > 0) {
    String input = Serial.readString();
    input.trim();
    input.toLowerCase();
    if (input.length() == 0) continue;

    int servoNum = -1, posVal = -1;
    if (sscanf(input.c_str(), "%d %d", &servoNum, &posVal) == 2) {
      if (servoNum >= 1 && servoNum <= 6) {
        int idx = servoNum - 1;
        Serial.printf("[TEST] Moving Servo %d directly to target %d...\n", servoNum, posVal);
        MotionCmd cmd = {-1, {-1, -1, -1, -1, -1, -1}, {50, 50, 50, 50, 50, 50}};
        cmd.targetPos[idx] = posVal;
        motionEnqueue(cmd);
        continue;
      }
    }
    
    if (input == "1" || input == "s1") {
      Serial.println("[TEST] Testing Servo 1 (Front-Left Leg - GPIO13)...");
      testSingleServo(0);
    } else if (input == "2" || input == "s2") {
      Serial.println("[TEST] Testing Servo 2 (Back-Right Leg - GPIO14)...");
      testSingleServo(1);
    } else if (input == "3" || input == "s3") {
      Serial.println("[TEST] Testing Servo 3 (Front-Right Smart Servo - GPIO18/19/34)...");
      testSingleServo(2);
    } else if (input == "4" || input == "s4") {
      Serial.println("[TEST] Testing Servo 4 (Back-Left Leg - GPIO26)...");
      testSingleServo(3);
    } else if (input == "5" || input == "s5") {
      Serial.println("[TEST] Testing Servo 5 (Slider - GPIO25)...");
      testSingleServo(4);
    } else if (input == "6" || input == "s6") {
      Serial.println("[TEST] Testing Servo 6 (Rotator - GPIO32)...");
      testSingleServo(5);
    } else if (input == "home") {
      Serial.println("[TEST] Moving all servos to HOME...");
      motionGoHome();
    } else if (input == "sit") {
      Serial.println("[TEST] Sitting down...");
      sitDown();
    } else if (input == "status") {
      printPositions();
    } else {
      Serial.printf("Unknown command '%s'. Type 1, 2, 3, 4, 5, 6, home, sit, or '4 1500'.\n", input.c_str());
    }
  }
}

void setup() {
  Serial.begin(115200);

  // 0. Initialize Action Light (GPIO33)
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);

  // 1. Start Wi-Fi AP & WebServer FIRST so hotspot turns on unconditionally
  initWiFiAP();
  initWebSocket(server);
  server.begin();

  // 2. Initialize actuators and state
  initBuzzer();
  buzzerBeep(100); // Quick startup chirp
  initBattery();
  initServos();
  motionQueueInit();

  // 3. Initialize sensors
  initIMU();
  initGPS();

  Serial.println();
  Serial.println("==================================================");
  Serial.println("ESP32 Actuator Ready for Serial Testing!");
  Serial.println("Commands:");
  Serial.println("  1 -> Move Servo 1 (Front-Left Leg)");
  Serial.println("  2 -> Move Servo 2 (Back-Right Leg)");
  Serial.println("  3 -> Move Servo 3 (Front-Right Smart Servo)");
  Serial.println("  4 -> Move Servo 4 (Back-Left Leg)");
  Serial.println("  5 -> Move Servo 5 (Slider)");
  Serial.println("  6 -> Move Servo 6 (Rotator)");
  Serial.println("  home -> Reset all to Home Stance");
  Serial.println("  sit  -> Fold legs & Sit");
  Serial.println("==================================================");
}

enum AutoMotionState { STATE_SITTING_DOWN, STATE_WAIT_SIT, STATE_STANDING_UP, STATE_WAIT_STAND };
static AutoMotionState autoState = STATE_SITTING_DOWN;
static unsigned long stateTimer = 0;

static void runContinuousSitStand() {
  unsigned long now = millis();
  
  switch (autoState) {
    case STATE_SITTING_DOWN:
      Serial.println("[AUTO] Moving all 6 servos to SIT position...");
      sitDown();
      stateTimer = 0;
      autoState = STATE_WAIT_SIT;
      break;

    case STATE_WAIT_SIT:
      if (allServosIdle()) {
        if (stateTimer == 0) {
          stateTimer = now;
        } else if (now - stateTimer >= 2500) {
          stateTimer = 0;
          autoState = STATE_STANDING_UP;
        }
      }
      break;

    case STATE_STANDING_UP:
      Serial.println("[AUTO] Moving all 6 servos to STAND (HOME) position...");
      goHomeAll();
      stateTimer = 0;
      autoState = STATE_WAIT_STAND;
      break;

    case STATE_WAIT_STAND:
      if (allServosIdle()) {
        if (stateTimer == 0) {
          stateTimer = now;
        } else if (now - stateTimer >= 2500) {
          stateTimer = 0;
          autoState = STATE_SITTING_DOWN;
        }
      }
      break;
  }
}

void loop() {
  processSerialCommands();   // Read interactive commands from Serial Monitor
  updateServos();            // servo_engine: advance all servos one tick
  motionTick();              // motion_queue: pop + start next command if idle
  runContinuousSitStand();   // Continuous sit & stand demo loop
  tickWebSocket();           // ws_manager: cleanup stale clients, heartbeat check
  readIMU();                 // imu: sample sensors
  readGPS();                 // gps: process UART2 GPS stream
  telemetryTick();           // telemetry: broadcast at ~30 Hz

  // Print servo positions every 1 second
  static unsigned long lastPosPrintMs = 0;
  if (millis() - lastPosPrintMs >= 1000) {
    lastPosPrintMs = millis();
    printPositions();
  }

  // Control GPIO 33 Light: ON while executing/moving, OFF when idle
  digitalWrite(LIGHT_PIN, isMotionExecuting() ? HIGH : LOW);
}