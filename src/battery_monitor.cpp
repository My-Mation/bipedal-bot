#include "battery_monitor.h"
#include "config.h"
#include <Arduino.h>

void initBattery() {
  // Battery monitoring is not currently implemented.
  // GPIO35 is reserved for future battery monitoring. DO NOT read now.
}

float batteryVoltage() {
  // Return nominal 7.4V static value while battery voltage monitor is disabled
  return 7.40f;
}

int batteryPercent() {
  // Return 100% nominal while battery voltage monitor is disabled
  return 100;
}

