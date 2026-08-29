#pragma once
#include <Arduino.h>

// =====================================================================
// BUZZER.H — Active buzzer control via BC548B NPN Transistor
// =====================================================================
// ESP32 GPIO 23 → 1kΩ resistor → BC548B Base
// BC548B Collector/Emitter controls 5V buzzer ground/supply
// =====================================================================

void initBuzzer();
void buzzerOn();
void buzzerOff();
void buzzerBeep(unsigned int durationMs = 100);
