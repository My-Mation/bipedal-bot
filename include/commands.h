#pragma once

// =====================================================================
// COMMANDS.H — Serial command handler
// =====================================================================
// Declares the function that reads and dispatches single-character
// commands from the Serial monitor.
// =====================================================================

// ---------------------------------------------------------------
// Parse one character and dispatch the corresponding action.
// Call from loop() whenever Serial.available() is true.
// ---------------------------------------------------------------
void handleCommand(char cmd);
