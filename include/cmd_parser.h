#pragma once
#include <stddef.h>

void parseCommand(const char* data, size_t len);

// Headlight / Action LED Control (GPIO 32)
void setLightState(bool state);
bool getLightState();
