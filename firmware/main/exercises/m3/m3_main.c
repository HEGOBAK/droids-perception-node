// Saved M3 exercise (step 5: range + servo together); not built. See firmware/main/README.md to run it again.
#include "m3_main.h"

#include <stdio.h>
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "range.h"
#include "servo.h"

// M3 step 5: range keeps measuring a fixed target while the servo sweeps.
// If servo current or noise disturbs anything, the distance jumps or the board resets.

// Same 100 ms ping period as the range test (M3 step 1).
static const unsigned LOOP_PERIOD_MS = 100;

// Sweep ±30° at 2° per loop = 20° per second (slow, gentle on USB power).
static const float SWEEP_LIMIT_DEG = 30.0f;
static const float SWEEP_STEP_DEG = 2.0f;

// 200 loops × 100 ms = 20 s.
static const unsigned LOOP_COUNT = 200;

void m3_run(void)
{
    esp_err_t result = range_init();
    if (result != ESP_OK) {
        printf("Range setup failed: %s\n", esp_err_to_name(result));
        return;
    }
    result = servo_init(); // starts at 0° (1500 µs)
    if (result != ESP_OK) {
        printf("Servo setup failed: %s\n", esp_err_to_name(result));
        return;
    }

    printf("time_s,angle_deg,echo_us,distance_m\n");

    float angle_deg = 0.0f;
    float step_deg = SWEEP_STEP_DEG;
    TickType_t last_wake = xTaskGetTickCount();   // fixed start time per loop (M1 / step 1)

    for (unsigned loop = 0; loop < LOOP_COUNT; loop++) {
        // Servo: one small step, turn around at the sweep limits.
        angle_deg += step_deg;
        if (angle_deg >= SWEEP_LIMIT_DEG || angle_deg <= -SWEEP_LIMIT_DEG) {
            step_deg = -step_deg;
        }
        servo_set_angle_deg(angle_deg);

        // Range: one ping, same as step 1.
        int64_t now_us = esp_timer_get_time();
        int64_t echo_us = 0;
        result = range_measure(&echo_us);
        if (result == ESP_OK) {
            printf("%.2f,%.0f,%lld,%.3f\n", now_us / 1e6f, angle_deg, (long long)echo_us, range_echo_to_m(echo_us));
        } else {
            printf("%.2f,%.0f,INVALID,%s\n", now_us / 1e6f, angle_deg, esp_err_to_name(result));
        }

        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }

    // Back to the middle, slowly, and hold there.
    servo_move_slowly(servo_angle_to_us(0.0f));
    printf("Step 5 done; servo back at 0\n");
}
