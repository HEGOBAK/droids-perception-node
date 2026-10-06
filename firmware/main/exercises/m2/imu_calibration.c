#include "imu_calibration.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Provisional for this module only: midpoint of upright/inverted Z means.
// (1.853845 + -0.218091) / 2 ≈ 0.818 g. Not a factory calibration.
static const float ACCEL_Z_OFFSET_G = 0.818f;

// Provisional Z scale: half the upright-to-inverted difference.
// (1.853845 - -0.218091) / 2 ≈ 1.036, so Z reads 3.6% too large. X and Y read ≈ 1 g already.
static const float ACCEL_Z_SCALE = 1.036f;

// Bias run: 1000 samples, 10 ms apart (about 12 s including read time).
static const unsigned BIAS_SAMPLE_COUNT = 1000;
static const unsigned BIAS_DELAY_MS = 10;

// Set to true to also print every raw sample as CSV (slower; save it to results/).
static const bool PRINT_RAW_ROWS = false;

// Bias check: a few corrected samples after the bias run.
static const unsigned CHECK_SAMPLE_COUNT = 10;
static const unsigned CHECK_DELAY_MS = 100;

// 0x3B starts 7 signed pairs in this order. CHANNEL_COUNT is the total.
enum { AX, AY, AZ, TEMP, GX, GY, GZ, CHANNEL_COUNT };

static const char *CHANNEL_NAMES[CHANNEL_COUNT] = {"ax", "ay", "az", "temp", "gx", "gy", "gz"};
static const char *CHANNEL_UNITS[CHANNEL_COUNT] = {"g", "g", "g", "C", "dps", "dps", "dps"};

// value = raw / scale + offset. Temperature: raw / 340 + 36.53 (datasheet).
static const float CHANNEL_SCALE[CHANNEL_COUNT] = {16384.0f, 16384.0f, 16384.0f, 340.0f, 131.0f, 131.0f, 131.0f};
static const float CHANNEL_OFFSET[CHANNEL_COUNT] = {0.0f, 0.0f, 0.0f, 36.53f, 0.0f, 0.0f, 0.0f};

// We don't keep 1000 readings in memory. Each new reading just updates four numbers,
// and at the end those four numbers are enough to work out the mean, standard deviation, min and max.
typedef struct {
    double sum;     // all readings added up
    double sum_sq;  // all readings squared, added up
    int16_t min;    // smallest seen so far
    int16_t max;    // largest seen so far
} channel_stats_t;

// MPU6050 sends two bytes, we transform it into one number (negatives included)
static int16_t decode_signed_pair(const uint8_t *bytes)
{
    uint16_t combined = ((uint16_t)bytes[0] << 8) | bytes[1];
    int32_t signed_value = combined;
    if (combined >= 0x8000) {
        signed_value -= 65536;
    }
    return (int16_t)signed_value;
}

// One complete reading, stored in raw (one row of data)
static esp_err_t read_raw_sample(i2c_master_dev_handle_t device, int16_t raw[CHANNEL_COUNT])
{
    uint8_t data_address = 0x3B;
    uint8_t bytes[14] = {0};
    esp_err_t result = i2c_master_transmit_receive(
        device,
        &data_address, 1,
        bytes, sizeof(bytes),
        100);
    if (result != ESP_OK) {
        return result;
    }

    for (int channel = 0; channel < CHANNEL_COUNT; channel++) {
        raw[channel] = decode_signed_pair(&bytes[channel * 2]); // splits the 14 bytes into 7 numbers
    }
    return ESP_OK;
}

// Convert raw counts to useful units.
static float to_units(int channel, double raw)
{
    return raw / CHANNEL_SCALE[channel] + CHANNEL_OFFSET[channel];
}

// Remove the Z shift first, then undo the stretch.
static float correct_az(float az_g)
{
    return (az_g - ACCEL_Z_OFFSET_G) / ACCEL_Z_SCALE;
}

// Main job: measure the gyro bias
esp_err_t imu_measure_gyro_bias(i2c_master_dev_handle_t device, gyro_bias_t *bias)
{
    // Allow startup to settle before the first sample.
    vTaskDelay(pdMS_TO_TICKS(100));
    printf("Keep the breadboard still: %u samples, nominal %u ms spacing\n",
           BIAS_SAMPLE_COUNT, BIAS_DELAY_MS);
    if (PRINT_RAW_ROWS) {
        printf("sample,read_start_us,ax_raw,ay_raw,az_raw,temp_raw,gx_raw,gy_raw,gz_raw\n");
    }

    // Reset stats
    // The sums start at 0. min starts at the largest possible value so that the
    // first real reading is always smaller and replaces it. max starts at the
    // smallest possible value for the same reason.
    channel_stats_t stats[CHANNEL_COUNT]; // Each data has its own stats
    for (int channel = 0; channel < CHANNEL_COUNT; channel++) {
        stats[channel] = (channel_stats_t){.min = INT16_MAX, .max = INT16_MIN};
    }

    int64_t first_us = 0;
    int64_t last_us = 0;
    for (unsigned sample = 0; sample < BIAS_SAMPLE_COUNT; sample++) {
        int16_t raw[CHANNEL_COUNT];
        // Timestamp is host-controller read start, not the sensor's sample time.
        int64_t read_start_us = esp_timer_get_time();
        esp_err_t result = read_raw_sample(device, raw);
        if (result != ESP_OK) {
            return result; // Never report a bias from a partial run.
        }

        if (sample == 0) {
            first_us = read_start_us;
        }
        last_us = read_start_us;

        // Add this sample to each channel's totals.
        for (int channel = 0; channel < CHANNEL_COUNT; channel++) {
            channel_stats_t *s = &stats[channel];
            s->sum += raw[channel];
            s->sum_sq += (double)raw[channel] * raw[channel];
            if (raw[channel] < s->min) {
                s->min = raw[channel];
            }
            if (raw[channel] > s->max) {
                s->max = raw[channel];
            }
        }

        if (PRINT_RAW_ROWS) {
            printf("%u,%lld,%d,%d,%d,%d,%d,%d,%d\n",
                   sample + 1, (long long)read_start_us,
                   raw[AX], raw[AY], raw[AZ], raw[TEMP], raw[GX], raw[GY], raw[GZ]);
        }

        vTaskDelay(pdMS_TO_TICKS(BIAS_DELAY_MS));
    }

    // Actual spacing includes I2C time (and printing, if raw rows are on).
    float elapsed_s = (last_us - first_us) / 1e6f;  // us -> s
    float mean_dt_ms = (last_us - first_us) / 1e3f / (BIAS_SAMPLE_COUNT - 1);
    printf("Summary: %u samples over %.2f s, mean dt %.2f ms\n",
           BIAS_SAMPLE_COUNT, elapsed_s, mean_dt_ms);

    // Mean = bias estimate. Std = noise around it. Min/max show any spikes.
    printf("channel,unit,mean,std,min,max\n");
    float mean[CHANNEL_COUNT];
    for (int channel = 0; channel < CHANNEL_COUNT; channel++) {
        const channel_stats_t *s = &stats[channel];
        double mean_raw = s->sum / BIAS_SAMPLE_COUNT;
        double variance_raw = (s->sum_sq - s->sum * mean_raw) / (BIAS_SAMPLE_COUNT - 1); // Noise
        if (variance_raw < 0) {
            variance_raw = 0; // Rounding can push a near-zero value below zero.
        }

        mean[channel] = to_units(channel, mean_raw);
        // The offset shifts values but not their spread, so only scale the std.
        float std = sqrt(variance_raw) / CHANNEL_SCALE[channel];

        printf("%s,%s,%.4f,%.4f,%.4f,%.4f\n",
               CHANNEL_NAMES[channel], CHANNEL_UNITS[channel],
               mean[channel], std,
               to_units(channel, s->min), to_units(channel, s->max));
    }

    // Stationary check: should be near 1 g after the provisional Z offset and scale.
    float az_corrected = correct_az(mean[AZ]);
    float magnitude_corrected = sqrtf(mean[AX] * mean[AX] + mean[AY] * mean[AY] +
                                      az_corrected * az_corrected);
    printf("Accel magnitude with Z offset %.3f g and scale %.3f corrected: %.4f g\n",
           ACCEL_Z_OFFSET_G, ACCEL_Z_SCALE, magnitude_corrected);

    bias->gx = mean[GX];
    bias->gy = mean[GY];
    bias->gz = mean[GZ];
    printf("Gyro bias: gx=%.4f gy=%.4f gz=%.4f dps\n", bias->gx, bias->gy, bias->gz);
    return ESP_OK;
}

esp_err_t imu_log_corrected_gyro(i2c_master_dev_handle_t device, const gyro_bias_t *bias)
{
    // Still stationary, so corrected values should stay near 0 dps.
    printf("Bias check: %u gyro samples with bias removed\n", CHECK_SAMPLE_COUNT);
    printf("sample,read_start_us,gx_dps,gy_dps,gz_dps\n");

    for (unsigned sample = 0; sample < CHECK_SAMPLE_COUNT; sample++) {
        imu_reading_t reading;
        int64_t read_start_us = esp_timer_get_time();
        esp_err_t result = imu_read_corrected(device, bias, &reading);
        if (result != ESP_OK) {
            return result;
        }

        printf("%u,%lld,%.4f,%.4f,%.4f\n",
               sample + 1, (long long)read_start_us, reading.gx, reading.gy, reading.gz);

        vTaskDelay(pdMS_TO_TICKS(CHECK_DELAY_MS));
    }
    return ESP_OK;
}

esp_err_t imu_read_corrected(i2c_master_dev_handle_t device, const gyro_bias_t *bias,
                             imu_reading_t *reading)
{
    int16_t raw[CHANNEL_COUNT];
    esp_err_t result = read_raw_sample(device, raw);
    if (result != ESP_OK) {
        return result;
    }

    reading->ax = to_units(AX, raw[AX]);
    reading->ay = to_units(AY, raw[AY]);
    reading->az = correct_az(to_units(AZ, raw[AZ]));
    reading->gx = to_units(GX, raw[GX]) - bias->gx;
    reading->gy = to_units(GY, raw[GY]) - bias->gy;
    reading->gz = to_units(GZ, raw[GZ]) - bias->gz;
    return ESP_OK;
}
