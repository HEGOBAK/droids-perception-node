#pragma once

#include "driver/i2c_master.h"
#include "imu_calibration.h"

// Tilt angles in degrees.
typedef struct {
    float roll_deg;   // rotation about X: positive when +y is up
    float pitch_deg;  // rotation about Y: positive when +x is down
} tilt_t;

// Work out tilt from gravity (accel only). Only valid while the board is not accelerating.
tilt_t imu_tilt_from_accel(const imu_reading_t *reading);

// Print averaged tilt twice a second so poses can be checked by hand.
esp_err_t imu_log_tilt(i2c_master_dev_handle_t device, const gyro_bias_t *bias);
