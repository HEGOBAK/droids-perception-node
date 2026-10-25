#pragma once

#include "imu_calibration.h"
#include "imu_filter.h"
#include "imu_tilt.h"
#include "driver/i2c_master.h"

esp_err_t imu_init(i2c_master_bus_handle_t imu_bus, i2c_master_dev_handle_t *imu_device, 
                    gyro_bias_t *gyro_bias);
void imu_run(i2c_master_bus_handle_t imu_bus);
