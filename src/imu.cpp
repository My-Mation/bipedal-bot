// =====================================================================
// IMU.CPP — MPU6050 driver (via Adafruit MPU6050 library)
// =====================================================================
// Reads accelerometer + gyroscope at ~10 Hz and computes pitch / roll
// from the accelerometer so the web UI can display a live tilt gauge.
// =====================================================================

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>
#include "imu.h"

static Adafruit_MPU6050 mpu;

// Shared data struct (declared extern in imu.h)
ImuData imuData;

// ---------------------------------------------------------------
// initIMU — attach I2C on D21/D22 and configure the sensor
// ---------------------------------------------------------------
bool initIMU() {
  Wire.begin(21, 22);   // SDA = GPIO21 (D21), SCL = GPIO22 (D22)

  if (!mpu.begin()) {
    Serial.println("MPU6050: NOT FOUND on D21/D22 — IMU disabled.");
    imuData.ok = false;
    return false;
  }

  // Sensitivity / filter settings — tweak here if needed
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  imuData.ok = true;
  Serial.println("MPU6050: OK  (SDA=D21, SCL=D22)");
  return true;
}

// ---------------------------------------------------------------
// readIMU — sample the sensor and compute pitch / roll
// ---------------------------------------------------------------
void readIMU() {
  if (!imuData.ok) return;

  sensors_event_t a, g, t;
  mpu.getEvent(&a, &g, &t);

  imuData.ax   = a.acceleration.x;
  imuData.ay   = a.acceleration.y;
  imuData.az   = a.acceleration.z;
  imuData.gx   = g.gyro.x;
  imuData.gy   = g.gyro.y;
  imuData.gz   = g.gyro.z;
  imuData.temp = t.temperature;

  // Pitch: rotation around X-axis (nose up/down)
  imuData.pitch = atan2f(imuData.ay,
                    sqrtf(imuData.ax * imuData.ax + imuData.az * imuData.az))
                  * (180.0f / M_PI);

  // Roll: rotation around Y-axis (lean left/right)
  imuData.roll  = atan2f(-imuData.ax, imuData.az)
                  * (180.0f / M_PI);
}
