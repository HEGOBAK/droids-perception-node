#pragma once

#include <stdint.h>
#include "esp_err.h"

// Set up TRIG (GPIO 21), ECHO (GPIO 1) and the echo interrupt. Call once.
esp_err_t range_init(void);

// Send one ping and wait for its echo.
// ESP_OK: echo_us is filled. ESP_ERR_TIMEOUT: no echo came back, so the reading is invalid.
// ESP_ERR_INVALID_STATE: ECHO was still HIGH from an earlier ping, so no new ping was sent.
esp_err_t range_measure(int64_t *echo_us);

// Turn an echo time into distance in meters (sound goes there and back, so divide by 2).
float range_echo_to_m(int64_t echo_us);
