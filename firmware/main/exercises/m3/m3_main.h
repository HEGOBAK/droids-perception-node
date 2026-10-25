#pragma once

#include <stdio.h>
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "range.h"
#include "servo.h"

// M3: range sensor pings a fixed target every 100 ms while the servo sweeps ±30° (20 s), then servo back to 0.
void m3_run(void);
