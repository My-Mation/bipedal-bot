// =====================================================================
// COMMANDS.CPP — Serial command handler
// =====================================================================
// Translates single-character commands received over Serial into robot
// state transitions or configuration changes.  All commands are listed
// in the help text printed by setup() in main.cpp.
// =====================================================================

#include <Arduino.h>
#include "commands.h"
#include "config.h"
#include "servo_engine.h"
#include "gait.h"

// ---------------------------------------------------------------
// handleCommand — parse one byte and act on it.
// ---------------------------------------------------------------
void handleCommand(char cmd) {
  switch (cmd) {

    // --- Motion commands ---
    case 'f':
      Serial.println("CMD: Walking forward");
      enterState(STATE_WALK_FWD);
      break;

    case 'b':
      Serial.println("CMD: Walking backward");
      enterState(STATE_WALK_BWD);
      break;

    case 'l':
      Serial.println("CMD: Turning left");
      enterState(STATE_TURN_L);
      break;

    case 'r':
      Serial.println("CMD: Turning right");
      enterState(STATE_TURN_R);
      break;

    // --- Stop / Home / Sit ---
    case 's':
      Serial.println("CMD: Stop");
      enterState(STATE_IDLE);
      break;

    case 'h':
      Serial.println("CMD: Homing");
      goHomeAll();
      enterState(STATE_HOME);
      break;

    case 'i':
      Serial.println("CMD: Sit");
      enterState(STATE_SIT);
      break;


    // --- Diagnostics ---
    case 'p':
      printPositions();
      break;

    // --- Speed adjustment ---
    case '+':
      if (stepInterval > MIN_INTERVAL) stepInterval -= 2;
      Serial.print("Speed increased. Interval = ");
      Serial.print(stepInterval);
      Serial.println(" ms");
      break;

    case '-':
      if (stepInterval < MAX_INTERVAL) stepInterval += 2;
      Serial.print("Speed decreased. Interval = ");
      Serial.print(stepInterval);
      Serial.println(" ms");
      break;

    default:
      // Silently ignore newlines ('\r', '\n') and unknown characters.
      break;
  }
}
