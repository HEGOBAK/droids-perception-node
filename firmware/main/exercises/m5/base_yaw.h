#pragma once

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"

typedef struct {
	float yaw_deg;
	float rate_dps;
	bool valid;
	int64_t time_us;
} sensor_packet_t;

BaseType_t get_sensor_packet(sensor_packet_t *packet);

void run_imu(void);