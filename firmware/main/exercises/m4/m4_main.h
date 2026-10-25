#pragma once

#include <stdio.h>
#include "esp_err.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "vision.h"

// M4: camera on, perception task finds the red target every 100 ms, results through a 1-slot queue,
// main loop prints one row twice a second (dumps one frame for host/frame_to_png.py). Runs forever.
void m4_run(void);
