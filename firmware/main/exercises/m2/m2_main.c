// Saved M2 exercise; not built. See firmware/main/README.md to run it again.
#include "m2_main.h"

#include <stdio.h>
#include "esp_err.h"
#include "i2c_bus.h"
#include "imu.h"

void m2_run(void)
{
    // M2: create the shared connection, then run the IMU on it.
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t result = board_i2c_create(&bus);
    if (result != ESP_OK) {
        printf("I2C setup failed: %s\n", esp_err_to_name(result));
        return;
    }

    imu_run(bus);

    // No sampling tasks yet: release the bus after this one-shot run.
    result = i2c_del_master_bus(bus);
    if (result != ESP_OK) {
        printf("I2C cleanup failed: %s\n", esp_err_to_name(result));
    }
}
