#include "i2c_bus.h"

esp_err_t board_i2c_create(i2c_master_bus_handle_t *bus)
{
    // Config I2C connection to the IMU
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,              // I2C Controller number 0
        .sda_io_num = 41,                   // GPIO numbers
        .scl_io_num = 42,                   // GPIO numbers
        .clk_source = I2C_CLK_SRC_DEFAULT,  // Clock source
        .glitch_ignore_cnt = 7,
    };

    return i2c_new_master_bus(&bus_config, bus);
}
