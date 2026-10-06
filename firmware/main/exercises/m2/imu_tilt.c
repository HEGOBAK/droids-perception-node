#include "imu_tilt.h"

#include <math.h>
#include <stdio.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Axes come straight from the HW-123 arrows, board flat on the breadboard:
// X → towards the power rails, Y → towards row 1 (VCC end), Z → up out of the chip.
// No sign flips or swaps are needed for this mounting.

static const float RAD_TO_DEG = 180.0f / (float)M_PI;

// Each report averages 50 readings, 10 ms apart (about 0.5 s).
static const unsigned SAMPLES_PER_REPORT = 50;
static const unsigned SAMPLE_DELAY_MS = 10;

// 120 reports ≈ 60 s: enough time to move through the test poses.
static const unsigned REPORT_COUNT = 120;

tilt_t imu_tilt_from_accel(const imu_reading_t *reading)
{
    float ax = reading->ax;
    float ay = reading->ay;
    float az = reading->az;

    tilt_t tilt;
    // Roll: how gravity splits between Y and Z.
    tilt.roll_deg = atan2f(ay, az) * RAD_TO_DEG;
    // Pitch: how much gravity lies along X, compared with the Y–Z plane.
    tilt.pitch_deg = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;
    return tilt;
}

esp_err_t imu_log_tilt(i2c_master_dev_handle_t device, const gyro_bias_t *bias)
{
    printf("Tilt test: hold each pose still for a few reports (about 60 s total)\n");
    printf("report,time_s,ax_g,ay_g,az_g,roll_deg,pitch_deg\n");

    for (unsigned report = 0; report < REPORT_COUNT; report++) {
        // Average the readings first, then work out the angles once (less noise).
        imu_reading_t mean = {0};
        for (unsigned sample = 0; sample < SAMPLES_PER_REPORT; sample++) {
            imu_reading_t reading;
            esp_err_t result = imu_read_corrected(device, bias, &reading);
            if (result != ESP_OK) {
                return result; // Never print a tilt from a failed read.
            }
            mean.ax += reading.ax / SAMPLES_PER_REPORT;
            mean.ay += reading.ay / SAMPLES_PER_REPORT;
            mean.az += reading.az / SAMPLES_PER_REPORT;
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_DELAY_MS));
        }

        tilt_t tilt = imu_tilt_from_accel(&mean);

        float time_s = esp_timer_get_time() / 1e6f;
        printf("%u,%.2f,%.4f,%.4f,%.4f,%.1f,%.1f\n",
               report + 1, time_s, mean.ax, mean.ay, mean.az, tilt.roll_deg, tilt.pitch_deg);
    }
    return ESP_OK;
}
