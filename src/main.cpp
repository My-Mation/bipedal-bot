// =====================================================================
// MAIN.CPP — 6-Servo Biped Walking Robot (PlatformIO / ESP32)
// =====================================================================
// This file is intentionally thin.  All logic lives in separate modules:
//
//   include/config.h       — pins, calibration, timing constants
//   include/servo_engine.h — smooth non-blocking motion engine API
//   include/gait.h         — gait state machine API
//   include/commands.h     — serial command handler API
//
//   src/servo_engine.cpp   — motion engine implementation
//   src/gait.cpp           — gait cycle implementation
//   src/commands.cpp       — command dispatcher implementation
// =====================================================================

#include <Arduino.h>
#include "servo_engine.h"
#include "gait.h"
#include "commands.h"
#include "wifi_server.h"
#include "imu.h"

// ---------------------------------------------------------------
// setup — runs once at power-on / reset
// ---------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  initServos();        // attach all servos, write home positions
  initIMU();           // start MPU6050 on D21/D22
  initWiFi();          // start AP hotspot + web server

  enterState(STATE_IDLE);

  Serial.println();
  Serial.println("6-Servo Biped Walker Ready.");
  Serial.println("Commands:");
  Serial.println("  f = Walk forward  (continuous)");
  Serial.println("  b = Walk backward (continuous)");
  Serial.println("  l = Turn left     (continuous)");
  Serial.println("  r = Turn right    (continuous)");
  Serial.println("  s = Stop immediately");
  Serial.println("  h = Return home");
  Serial.println("  p = Print servo positions");
  Serial.println("  + = Faster");
  Serial.println("  - = Slower");
}

// ---------------------------------------------------------------
// loop — runs repeatedly
// ---------------------------------------------------------------
void loop() {
  // 1. Read and dispatch serial commands (non-blocking single byte)
  if (Serial.available()) {
    handleCommand(static_cast<char>(Serial.read()));
  }

  // 2. Advance the gait / homing state machine
  switch (state) {
    case STATE_HOME:
      // Transition to IDLE once all servos have reached home
      if (allServosIdle()) {
        enterState(STATE_IDLE);
        broadcastStatus(); // notify UI that homing is complete
      }
      break;

    case STATE_WALK_FWD:
    case STATE_WALK_BWD:
    case STATE_TURN_L:
    case STATE_TURN_R:
      runGait();
      break;

    case STATE_IDLE:
    default:
      // Servos hold their last commanded position — nothing to do.
      break;
  }

  // 3. Advance all servos one smooth step toward their targets
  updateServos();

  // 4. Tick WiFi: read IMU at 10 Hz, broadcast telemetry, clean WS clients
  tickWifi();
}