#pragma once

#include "driver/i2c_master.h"

// Stationary gyro bias in degrees/s.
typedef struct {
    float gx;
    float gy;
    float gz;
} gyro_bias_t;

// One corrected reading: both sensors from the same moment.
// Accel in g (Z offset and scale corrected); gyro in degrees/s (bias removed).
typedef struct {
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
} imu_reading_t;

// Average stationary samples using the verified ±2 g / ±250 degrees/s ranges.
// Prints a summary and returns the gyro bias. The caller retains the device handle.
esp_err_t imu_measure_gyro_bias(i2c_master_dev_handle_t device, gyro_bias_t *bias);

// Print a few gyro samples with the bias removed to check the correction.
esp_err_t imu_log_corrected_gyro(i2c_master_dev_handle_t device, const gyro_bias_t *bias);

// Read one corrected sample of both sensors at once.
esp_err_t imu_read_corrected(i2c_master_dev_handle_t device, const gyro_bias_t *bias,
                             imu_reading_t *reading);
