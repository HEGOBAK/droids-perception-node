#include "m5_main.h"

#include <stdio.h>
#include "esp_err.h"
#include "../m3/servo.h"
#include "../m4/vision.h"
#include "control.h"
#include "base_yaw.h"

void m5_run(void) 
{
	// Start IMU
	printf("Start IMU\n");
	run_imu();

	// Start servo
	esp_err_t result = servo_init(); // starts at 0° (1500 µs)
    if (result != ESP_OK) {
        printf("Servo setup failed: %s\n", esp_err_to_name(result));
        return;
    }
	printf("Servo ready\n");

	// Start camera
	result = vision_init();
    if (result != ESP_OK) {
        printf("Camera setup failed: %s\n", esp_err_to_name(result));
        return;
    }
	printf("Camera ready\n");

	// Start control
	printf("Start control\n");
	run_control();
}