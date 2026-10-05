#pragma once

#include "driver/i2c_master.h"

// Create the shared bus on SDA41/SCL42; caller owns the returned handle.
esp_err_t board_i2c_create(i2c_master_bus_handle_t *bus);
