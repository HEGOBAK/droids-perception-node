#include "imu.h"

#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"

// Write one register, then read the same register back to confirm the sensor stored it.
static esp_err_t write_and_verify(i2c_master_dev_handle_t device, const char *name,
                                  uint8_t address, uint8_t value)
{
    // First byte: register address. Second byte: value to write.
    uint8_t command[] = {address, value};
    esp_err_t result = i2c_master_transmit(device, command, sizeof(command), 100);
    if (result != ESP_OK) {
        printf("%s write failed: %s\n", name, esp_err_to_name(result));
        return result;
    }

    uint8_t stored = 0;
    result = i2c_master_transmit_receive(device, &address, 1, &stored, 1, 100);
    if (result != ESP_OK) {
        printf("%s readback failed: %s\n", name, esp_err_to_name(result));
        return result;
    }

    printf("%s = 0x%02X (expected 0x%02X)\n", name, (unsigned)stored, (unsigned)value);
    if (stored != value) {
        printf("Unexpected %s value\n", name);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

esp_err_t imu_init(i2c_master_bus_handle_t imu_bus, i2c_master_dev_handle_t *imu_device, 
                    gyro_bias_t *gyro_bias)
{
    // Ask whether a device responds at 0x68; timeout is 100 ms.
    esp_err_t imu_result = i2c_master_probe(imu_bus, 0x68, 100);
    if (imu_result != ESP_OK) {
        printf("I2C probe failed: %s\n", esp_err_to_name(imu_result));
        return imu_result;
    }
    printf("I2C device responded at 0x68\n");

    // Describe the device on our existing bus.
    i2c_device_config_t imu_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x68,
        .scl_speed_hz = 100000, // 100 kHz communication clock
    };

    imu_result = i2c_master_bus_add_device(imu_bus, &imu_config, imu_device);
    if (imu_result != ESP_OK) {
        printf("Add IMU failed: %s\n", esp_err_to_name(imu_result));
        return imu_result;
    }

    // Select WHO_AM_I, then read its one-byte value.
    uint8_t identity_address = 0x75;
    uint8_t identity = 0;
    imu_result = i2c_master_transmit_receive(
        *imu_device,
        &identity_address, 1,
        &identity, 1,
        100);
    if (imu_result != ESP_OK) {
        printf("Identity read failed: %s\n", esp_err_to_name(imu_result));
        goto fail;
    }

    printf("WHO_AM_I = 0x%02X (expected 0x68)\n", (unsigned)identity);
    if (identity != 0x68) {
        printf("Unexpected identity; stopping IMU setup\n");
        imu_result = ESP_ERR_INVALID_RESPONSE;
        goto fail;
    }

    // Wake the sensor (PWR_MGMT_1 = 0x01).
    if (write_and_verify(*imu_device, "PWR_MGMT_1", 0x6B, 0x01) != ESP_OK) {
        imu_result = ESP_ERR_INVALID_RESPONSE;
        goto fail;
    }

    // Gyro range ±250 degrees/s; sensitivity: 131 counts per degree/s.
    // Zero also leaves this sensor's self-test bits disabled.
    if (write_and_verify(*imu_device, "GYRO_CONFIG", 0x1B, 0x00) != ESP_OK) {
        imu_result = ESP_ERR_INVALID_RESPONSE;
        goto fail;
    }

    // Accel range ±2 g; sensitivity: 16384 counts per g.
    // Zero also leaves this sensor's self-test bits disabled.
    if (write_and_verify(*imu_device, "ACCEL_CONFIG", 0x1C, 0x00) != ESP_OK) {
        imu_result = ESP_ERR_INVALID_RESPONSE;
        goto fail;
    }

    // Low-pass filter (DLPF_CFG = 4): gyro ~20 Hz, accel ~21 Hz, about 8.5 ms delay.
    // We read at 100 Hz, so faster shaking must be removed first or it adds false gyro angle.
    if (write_and_verify(*imu_device, "CONFIG", 0x1A, 0x04) != ESP_OK) {
        imu_result = ESP_ERR_INVALID_RESPONSE;
        goto fail;
    }

    // Measure gyro bias while still, then check that removing it gives near 0 dps.
    imu_result = imu_measure_gyro_bias(*imu_device, gyro_bias);
    if (imu_result != ESP_OK) {
        printf("Gyro bias measurement failed: %s\n", esp_err_to_name(imu_result));
        goto fail;
    }

    return ESP_OK;

fail:
    // The device was added but setup failed: remove it so the caller gets nothing half set up.
    if (i2c_master_bus_rm_device(*imu_device) != ESP_OK) {
        printf("Remove IMU failed\n");
    }
    *imu_device = NULL;
    return imu_result;
}

void imu_run(i2c_master_bus_handle_t imu_bus)
{
    gyro_bias_t gyro_bias = {0};
    i2c_master_dev_handle_t imu_device = NULL;
    esp_err_t imu_result = imu_init(imu_bus, &imu_device, &gyro_bias);
    if (imu_result != ESP_OK) {
        printf("IMU init failed: %s\n", esp_err_to_name(imu_result));
        return;     // imu_init already removed the device
    }
    imu_result = imu_log_corrected_gyro(imu_device, &gyro_bias);
    if (imu_result != ESP_OK) {
        printf("Bias check read failed: %s\n", esp_err_to_name(imu_result));
        goto cleanup;
    }

    // Step 6: track roll with the complementary filter.
    // (Step 5's pose log is still available: swap in imu_log_tilt(imu_device, &gyro_bias).)
    imu_result = imu_log_roll_filter(imu_device, &gyro_bias);
    if (imu_result != ESP_OK) {
        printf("Roll filter ended with invalid data: %s\n", esp_err_to_name(imu_result));
        goto cleanup;
    }

cleanup:
    // Common exit: release the device on success or failure.
    imu_result = i2c_master_bus_rm_device(imu_device);
    if (imu_result != ESP_OK) {
        printf("Remove IMU failed: %s\n", esp_err_to_name(imu_result));
    }
}
