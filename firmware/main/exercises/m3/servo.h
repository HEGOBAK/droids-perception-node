#pragma once

#include <stdint.h>
#include "esp_err.h"

// Set up MCPWM on GPIO 14 and hold the servo at neutral (1500 µs). Call once.
esp_err_t servo_init(void);

// Command a pulse width in µs. Values outside the safe limits are clamped.
esp_err_t servo_set_pulse_us(uint32_t pulse_us);

// Angle (degrees, + = L = counter-clockwise from above, 0 = straight ahead) → pulse width in µs.
uint32_t servo_angle_to_us(float angle_deg);

// Command an angle in degrees. Clamped to the safe limits (±45°).
esp_err_t servo_set_angle_deg(float angle_deg);

// Move to a pulse width in small steps, so the servo turns slowly (gentle on USB power).
	// calling servo_set_pulse_us many times
esp_err_t servo_move_slowly(uint32_t target_us);
