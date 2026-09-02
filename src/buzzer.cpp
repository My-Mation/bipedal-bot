#include "buzzer.h"
#include "config.h"
#include <Arduino.h>

void initBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Default off
#ifdef DEBUG
  Serial.printf("[BUZZER] Control initialized on GPIO%d\n", BUZZER_PIN);
#endif
}

void buzzerOn() {
  digitalWrite(BUZZER_PIN, HIGH);
}

void buzzerOff() {
  digitalWrite(BUZZER_PIN, LOW);
}

void buzzerBeep(unsigned int durationMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(BUZZER_PIN, LOW);
}

void buzzerWifiReadySignal() {
  buzzerBeep(120);
}

void buzzerConnectedSignal() {
  buzzerBeep(70);
  delay(50);
  buzzerBeep(70);
}

void buzzerDisconnectedSignal() {
  buzzerBeep(300);
}
