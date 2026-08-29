#pragma once

// =====================================================================
// IMU.H — MPU6050 accelerometer / gyroscope interface
// =====================================================================
// SDA → GPIO 21   SCL → GPIO 22
//
// NOTE: MPU6050 has no magnetometer.  Yaw cannot be computed reliably.
//       yawSupported = false always.  Use MPU9250/BNO085 for true yaw.
// =====================================================================

struct ImuData {
  float ax = 0, ay = 0, az = 0;   // Acceleration  (m/s²)
  float gx = 0, gy = 0, gz = 0;   // Gyroscope     (rad/s)
  float pitch = 0, roll = 0;       // Tilt angles   (degrees)
  float temp  = 0;                  // Die temperature (°C)
  bool  ok           = false;       // false if sensor absent / init failed
  bool  yawSupported = false;       // always false for MPU6050
};

extern ImuData imuData;

// Initialise the MPU6050.  Returns true on success.
bool initIMU();

// Read one sample and update imuData.  Call periodically from loop().
void readIMU();
