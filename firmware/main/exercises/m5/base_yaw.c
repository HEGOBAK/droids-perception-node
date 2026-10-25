#include "base_yaw.h"

#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "../m2/imu.h"
#include "../m2/i2c_bus.h"

#define YAW_AXIS gz						// the column that moved
static const float YAW_SIGN = +1.0f;	// +1 if it went + on a LEFT turn, -1 if it went + on a RIGHT turn

// Set once by run_imu(), then read by the sensor task (locals would vanish when run_imu returns).
static i2c_master_dev_handle_t imu_device = NULL;
static gyro_bias_t gyro_bias = {0};

// Shared
static QueueHandle_t sensor_queue;


// Sensor task ------------------------------------------------------------------------------------------------------------------------------- //
static const unsigned SENSOR_PERIOD_MS = 10; // 100hz = 10 ms

static void sensor_task(void *argument) // priority 5
{
	float rate = 0;
	float yaw = 0;
	int64_t now_us = 0;
	int64_t last_us = esp_timer_get_time();
	float dt = 0;

	TickType_t last_wake = xTaskGetTickCount();   

	while (1) {
		imu_reading_t reading;
		sensor_packet_t packet;

		esp_err_t result = imu_read_corrected(imu_device, &gyro_bias, &reading);
		now_us = esp_timer_get_time();
		dt = (now_us - last_us) / 1e6f;
		last_us = now_us;

		if (result != ESP_OK) {
			packet = (sensor_packet_t){
				.valid = false,
			};
		}
		else {
			rate = YAW_SIGN * reading.YAW_AXIS;
			yaw += rate * dt;
			packet = (sensor_packet_t){
				.rate_dps = rate, 
				.yaw_deg = yaw,
				.valid = true,
				.time_us = now_us,
			};
		}

		xQueueOverwrite(sensor_queue, &packet);
		xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
	}
}
// -------------------------------------------------------------------------------------------------------------------------------------------- //


// Get sensor packet -------------------------------------------------------------------------------------------------------------------------- //
BaseType_t get_sensor_packet(sensor_packet_t *packet)
{
	if (sensor_queue == NULL) {
		return pdFALSE;
	}
	return xQueuePeek(sensor_queue, packet, 0);
}
// -------------------------------------------------------------------------------------------------------------------------------------------- //


void run_imu(void)
{
    i2c_master_bus_handle_t bus = NULL;
	esp_err_t result = board_i2c_create(&bus);
	if (result != ESP_OK) {
		printf("I2C setup failed: %s\n", esp_err_to_name(result));
		return;
	}

	printf("Keep the base still (~10 s bias measurement)\n");
	result = imu_init(bus, &imu_device, &gyro_bias);
	if (result != ESP_OK) {
		printf("IMU init failed: %s\n", esp_err_to_name(result));
		return;
	}

	sensor_queue = xQueueCreate(1, sizeof(sensor_packet_t));    // mailbox of one slot
	if (sensor_queue == NULL) {
		printf("Failed to create sensor queue\n");
		return;
	}

	printf("IMU ready\n");
	xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);
}
