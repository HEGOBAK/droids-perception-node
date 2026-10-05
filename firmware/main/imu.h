#pragma once

#include "driver/i2c_master.h"

// Check identity, configure the IMU, then run the M2 measurements once.
void imu_run(i2c_master_bus_handle_t imu_bus);
