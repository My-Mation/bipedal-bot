// =====================================================================
// IMU.CPP — MPU6050 raw Wire driver (no Adafruit library)
// =====================================================================
// Uses direct register reads so it works with clone chips that return
// a non-standard WHO_AM_I value (0x70, 0x72, etc.) that cause the
// Adafruit library's begin() to fail even though the device is present.
//
// Accel scale  : ±8 g   → 4096 LSB/g
// Gyro  scale  : ±500°/s → 65.5 LSB/(°/s)  → convert to rad/s ÷57.3
// Temp  formula: (raw / 340.0) + 36.53   (from MPU6050 datasheet)
// =====================================================================

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "imu.h"

// ── MPU6050 register map ────────────────────────────────────────────
static constexpr uint8_t MPU_ADDR     = 0x68;
static constexpr uint8_t REG_PWR_MGMT = 0x6B;
static constexpr uint8_t REG_CONFIG   = 0x1A;
static constexpr uint8_t REG_GYRO_CFG = 0x1B;
static constexpr uint8_t REG_ACCEL_CFG= 0x1C;
static constexpr uint8_t REG_ACCEL_H  = 0x3B;  // first of 14 data bytes
static constexpr uint8_t REG_WHO_AM_I = 0x75;

// Scale factors matching the register config below
static constexpr float ACCEL_SCALE = 4096.0f;   // ±8 g  → LSB/g
static constexpr float GYRO_SCALE  = 65.5f;     // ±500°/s → LSB/(°/s)
static constexpr float DEG2RAD     = M_PI / 180.0f;

// Shared data struct (declared extern in imu.h)
ImuData imuData;

// ── Helpers ─────────────────────────────────────────────────────────
static void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

// ── initIMU ─────────────────────────────────────────────────────────
bool initIMU() {
  Wire.begin(21, 22);    // SDA=GPIO21, SCL=GPIO22
  Wire.setClock(100000); // 100 kHz — reliable for long wires

  // ── I2C scan ────────────────────────────────────────────────────
  Serial.println("[I2C] Scanning bus (SDA=GPIO21, SCL=GPIO22)...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[I2C]  0x%02X", addr);
      if (addr == 0x68 || addr == 0x69) Serial.print(" <- MPU6050");
      Serial.println();
      found++;
    }
  }
  if (!found) {
    Serial.println("[I2C]  Nothing found — check wiring!");
    imuData.ok = false;
    return false;
  }

  // ── WHO_AM_I (informational only — don't gate on it) ─────────────
  uint8_t whoami = readReg(REG_WHO_AM_I);
  Serial.printf("[IMU] WHO_AM_I = 0x%02X ", whoami);
  if      (whoami == 0x68) Serial.println("(genuine MPU6050)");
  else if (whoami == 0x72) Serial.println("(clone — still works)");
  else if (whoami == 0x70) Serial.println("(clone — still works)");
  else                      Serial.printf ("(clone 0x%02X — still works)\n", whoami);

  // ── Wake up & configure ──────────────────────────────────────────
  writeReg(REG_PWR_MGMT, 0x00);  // clear SLEEP bit → wake up
  delay(10);
  writeReg(REG_CONFIG,    0x04);  // DLPF bandwidth ~20 Hz
  writeReg(REG_GYRO_CFG,  0x08);  // ±500 °/s
  writeReg(REG_ACCEL_CFG, 0x10);  // ±8 g

  // Verify it responds properly to a read-back
  uint8_t pw = readReg(REG_PWR_MGMT);
  if (pw == 0xFF) {  // 0xFF = no ACK / bus error
    Serial.println("[IMU] Bus read failed after init — IMU disabled.");
    imuData.ok = false;
    return false;
  }

  imuData.ok = true;
  Serial.println("[IMU] MPU6050 OK — raw Wire driver active.");
  return true;
}

// ── readIMU ─────────────────────────────────────────────────────────
void readIMU() {
  if (!imuData.ok) {
    static unsigned long lastWarn = 0;
    if (millis() - lastWarn >= 5000) {
      lastWarn = millis();
      Serial.println("[IMU] Sensor offline — check wiring.");
    }
    return;
  }

  // Request 14 bytes starting at ACCEL_XOUT_H:
  //  [0-1]  ACCEL_X  [2-3] ACCEL_Y  [4-5] ACCEL_Z
  //  [6-7]  TEMP
  //  [8-9]  GYRO_X  [10-11] GYRO_Y  [12-13] GYRO_Z
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(REG_ACCEL_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (uint8_t)14);

  if (Wire.available() < 14) return;  // partial read — skip frame

  auto rd16 = []() -> int16_t {
    uint8_t hi = Wire.read(), lo = Wire.read();
    return (int16_t)((hi << 8) | lo);
  };

  int16_t raw_ax = rd16();
  int16_t raw_ay = rd16();
  int16_t raw_az = rd16();
  int16_t raw_t  = rd16();
  int16_t raw_gx = rd16();
  int16_t raw_gy = rd16();
  int16_t raw_gz = rd16();

  // Convert to physical units
  imuData.ax   =  raw_ax / ACCEL_SCALE * 9.80665f;  // m/s²
  imuData.ay   =  raw_ay / ACCEL_SCALE * 9.80665f;
  imuData.az   =  raw_az / ACCEL_SCALE * 9.80665f;
  imuData.gx   = (raw_gx / GYRO_SCALE) * DEG2RAD;   // rad/s
  imuData.gy   = (raw_gy / GYRO_SCALE) * DEG2RAD;
  imuData.gz   = (raw_gz / GYRO_SCALE) * DEG2RAD;
  imuData.temp = (raw_t  / 340.0f) + 36.53f;         // °C

  // Pitch / Roll from accelerometer
  imuData.pitch = atan2f(imuData.ay,
                    sqrtf(imuData.ax * imuData.ax + imuData.az * imuData.az))
                  * (180.0f / M_PI);
  imuData.roll  = atan2f(-imuData.ax, imuData.az)
                  * (180.0f / M_PI);

  // ── Serial print ────────────────────────────────────────────────
  Serial.printf(
    "[IMU] Pitch:%6.1f  Roll:%6.1f  "
    "Ax:%6.2f  Ay:%6.2f  Az:%6.2f  "
    "Gx:%6.3f  Gy:%6.3f  Gz:%6.3f  "
    "T:%4.1fC\n",
    imuData.pitch, imuData.roll,
    imuData.ax,    imuData.ay,    imuData.az,
    imuData.gx,    imuData.gy,    imuData.gz,
    imuData.temp
  );
}
