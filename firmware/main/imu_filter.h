#pragma once

#include "driver/i2c_master.h"
#include "imu_calibration.h"

// One complementary filter step: trust the gyro short-term, the accelerometer long-term.
float imu_filter_step(float angle_deg, float gyro_dps, float accel_angle_deg,
                      float dt_s, float tau_s);

// Track roll for a fixed time, printing gyro-only, accel-only and filtered angles side by side.
// A failed read marks the angles INVALID and the loop keeps running; it never locks up.
esp_err_t imu_log_roll_filter(i2c_master_dev_handle_t device, const gyro_bias_t *bias);
