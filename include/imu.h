#pragma once

// =====================================================================
// IMU.H — MPU6050 accelerometer / gyroscope interface
// =====================================================================
// SDA → GPIO 21 (D21)
// SCL → GPIO 22 (D22)
// =====================================================================

// Processed data snapshot — updated by readIMU()
struct ImuData {
  float ax = 0, ay = 0, az = 0;   // Acceleration  (m/s²)
  float gx = 0, gy = 0, gz = 0;   // Gyroscope     (rad/s)
  float pitch = 0, roll = 0;       // Tilt angles   (degrees)
  float temp  = 0;                  // Die temperature (°C)
  bool  ok    = false;              // false if sensor absent / init failed
};

extern ImuData imuData;  // defined in imu.cpp, read anywhere

// Initialise the MPU6050. Returns true on success.
// Call once from setup().
bool initIMU();

// Read one sample from the MPU6050 and update imuData.
// Call periodically (e.g. every 100 ms) from loop().
void readIMU();
