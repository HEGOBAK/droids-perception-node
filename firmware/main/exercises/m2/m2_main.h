#pragma once

#include <stdio.h>
#include "esp_err.h"
#include "i2c_bus.h"
#include "imu.h"

// M2: create the I2C bus, run the IMU checks, bias, filter log, then release the bus.
void m2_run(void);
