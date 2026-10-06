#include "imu_filter.h"
#include "imu_tilt.h"

#include <stdbool.h>
#include <stdio.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Time constant: changes faster than tau follow the gyro, slower ones follow the accelerometer.
// Try 0.1, 0.5 and 2.0 s to see the trade-off.
static const float TAU_S = 0.5f;

// Step every 10 ms; print every 10th step (10 rows per second).
static const unsigned STEP_DELAY_MS = 10;
static const unsigned PRINT_EVERY = 10;

// Run length. Use 30 s for movement tests, longer (e.g. 120 s) for the still drift test.
static const unsigned RUN_S = 30;

// Step 7 bias test: pretend the bias estimate is wrong by this much (normally 0).
static const float TEST_BIAS_ERROR_DPS = 0.0f;

// Countdown before logging starts, so there is time to get ready.
static const unsigned COUNTDOWN_S = 5;

float imu_filter_step(float angle_deg, float gyro_dps, float accel_angle_deg,
                      float dt_s, float tau_s)
{
    // alpha is close to 1: mostly the gyro prediction, with a small pull towards the accelerometer.
    float alpha = tau_s / (tau_s + dt_s);
    float predicted_deg = angle_deg + gyro_dps * dt_s; // last angle + how far the gyro says we turned
    return alpha * predicted_deg + (1.0f - alpha) * accel_angle_deg;
}

esp_err_t imu_log_roll_filter(i2c_master_dev_handle_t device, const gyro_bias_t *bias)
{
    printf("Roll filter: tau %.2f s, run %u s, test bias error %.2f dps\n",
           TAU_S, RUN_S, TEST_BIAS_ERROR_DPS);

    // Nothing is read during the countdown, so waiting here cannot affect the angles.
    for (unsigned seconds_left = COUNTDOWN_S; seconds_left > 0; seconds_left--) {
        printf("Starting in %u...\n", seconds_left);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    printf("Go!\n");
    printf("time_s,dt_ms,gyro_only_deg,accel_deg,filtered_deg\n");

    // Angles are only valid after a good read. A failed read marks them invalid and the
    // loop keeps going; the next good read restarts them from the accelerometer.
    bool valid = false;             // can the angles be trusted right now?
    unsigned failed_reads = 0;      // how many reads failed in a row
    float gyro_only_deg = 0.0f;     // the running angles
    float filtered_deg = 0.0f;
    int64_t last_us = 0;            // time of the last good read (for dt)
    unsigned step = 0;              // counts good steps (to print every 10th)

    // Stop on time, not step count, so failed reads cannot make the run longer.
    int64_t end_us = esp_timer_get_time() + (int64_t)RUN_S * 1000000;
    while (esp_timer_get_time() < end_us) {
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));

        imu_reading_t reading;
        int64_t now_us = esp_timer_get_time();
        esp_err_t result = imu_read_corrected(device, bias, &reading);
        if (result != ESP_OK) {
            // Bounded: each failed read gives up after the 100 ms I2C timeout.
            // Print only the first failure; the VALID line later reports how many there were.
            if (valid) {
                printf("%.2f,INVALID,%s\n", now_us / 1e6f, esp_err_to_name(result));
            }
            valid = false;
            failed_reads++;
            continue;
        }

        float accel_deg = imu_tilt_from_accel(&reading).roll_deg;
        float gx = reading.gx + TEST_BIAS_ERROR_DPS; // Roll turns about X, so it uses gx.

        if (!valid) {
            // First good read, or back after failures: restart from the accelerometer.
            gyro_only_deg = accel_deg;
            filtered_deg = accel_deg;
            last_us = now_us;
            valid = true;
            printf("%.2f,VALID,angles restarted from accel after %u failed reads\n",
                   now_us / 1e6f, failed_reads);
            failed_reads = 0;
            continue;
        }

        // Use the measured gap, not the nominal 10 ms.
        float dt_s = (now_us - last_us) / 1e6f;
        last_us = now_us;

        gyro_only_deg += gx * dt_s;
        filtered_deg = imu_filter_step(filtered_deg, gx, accel_deg, dt_s, TAU_S);

        step++;
        if (step % PRINT_EVERY == 0) {
            printf("%.2f,%.2f,%.2f,%.2f,%.2f\n",
                   now_us / 1e6f, dt_s * 1e3f, gyro_only_deg, accel_deg, filtered_deg);
        }
    }

    // Report whether the run ended with good data.
    return valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}
