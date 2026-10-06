/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// Saved M1 exercise; not built. See firmware/main/README.md to run it again.
#include "m1_tasks.h"
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"

#define STALE_TIMEOUT_US 200000 // 200 ms = 2 missed perception period

// Defining messages
typedef struct {
    uint32_t sequence;      // Observation number
    int64_t acquired_us;    // When it was made (us since boot)
    float target_x;         // Synthetic target, -1 left to +1 right
} observation_t;

// Defining timing sample
typedef struct {
    uint32_t cycle;        // control cycle number
    int64_t  wake_us;      // when control woke (us since boot)
    uint32_t ticks;        // ulTaskNotifyTake return value; >1 = missed periods
    uint8_t  stale;        // 1 if the observation was stale this cycle
} timing_sample_t;

// Shared: latest observation + control task ID for the timer
static QueueHandle_t observation_queue;
static TaskHandle_t control_task_handle = NULL;

// Timing log: control -> logger
static QueueHandle_t timing_queue;
static uint32_t timing_drops = 0;      // Samples lost when queue is full

// Perception: synthetic observation at 10 Hz
static void perception_task(void *argument)
{
    // Frequency = 1/period
    // 10 Hz = 0.1s, 100ms (1/10)

    observation_t obs = {0};

    while (1) {
        // Simulate acquiring an observation
        obs.target_x = 0.25f;                       // Example target value
        obs.acquired_us = esp_timer_get_time();     // Get current time in microseconds
        xQueueOverwrite(observation_queue, &obs);   // Send the observation to the queue
        obs.sequence++;                             // Increment sequence number

        // Intentionally pause
        if (obs.sequence % 50 == 0) { // Every 50 obs.
            vTaskDelay(pdMS_TO_TICKS(500));
        }

        // Delay for 100 milliseconds
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Control: wakes every 20 ms from the timer
static void control_task(void *argument)
{
    // 50 Hz = 0.02s, 20ms (1/50)

    BaseType_t result;
    observation_t obs = {0};
    uint32_t stale_count = 0;
    uint32_t cycle = 0;             // Control cycle number

    while (1) {
        uint32_t ticks = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   // sleep until timer's notification
        int64_t now_us = esp_timer_get_time();                      // Get current time when control woke
        uint8_t stale = 0;                                          // 1 if obs too old this cycle
        result = xQueuePeek(observation_queue, &obs, 0);            // Try to receive an observation from the queue

        if (result == pdTRUE) {
            // Check obs age
            int64_t age_us = now_us - obs.acquired_us;

            if (age_us > STALE_TIMEOUT_US) {
                stale_count++;
                stale = 1;
            }
        }

        // Hand timing to logger, no printing here
        timing_sample_t s = {
            cycle++,
            now_us,
            ticks,
            stale
        };

        if (xQueueSend(timing_queue, &s, 0) != pdTRUE) {   // 0 = never wait
            timing_drops++;                                // Queue full, drop not block
        }
    }
}

static void sensor_task(void *argument)
{
    while (1) {
        // Simulate sensor processing
        printf("Sensor task is running...\n");
        vTaskDelay(pdMS_TO_TICKS(500)); // Delay for 500 milliseconds
    }
}

// Logger: prints timing off the critical path
static void logger_task(void *argument)
{
    timing_sample_t s;
    while (1) {
        xQueueReceive(timing_queue, &s, portMAX_DELAY);   // sleep until a sample arrives
        printf("T,%" PRIu32 ",%lld,%" PRIu32 ",%u\n", s.cycle, s.wake_us, s.ticks, s.stale); // T,cycle,wake_us,ticks,stale
    }
}

static void control_timer_callback(void *argument)
{
    xTaskNotifyGive(control_task_handle); // Notify control_task by its ID
}

void m1_start_tasks(void)
{
    // Creating the "mailbox"
    observation_queue = xQueueCreate(1, sizeof(observation_t));  // Create a queue that can hold 1 observation_t message
    if (observation_queue == NULL) {
        printf("Failed to create observation queue\n");
        return;
    }

    // Creating logger's queue
    timing_queue = xQueueCreate(64, sizeof(timing_sample_t));    // 64 x 20 ms = ~1.3 s if logger stalls
    if (timing_queue == NULL) {
        printf("Failed to create timing queue\n");
        return;
    }

    // Create tasks: control highest (3), others lowest (1)
    BaseType_t task_result;
    esp_err_t timer_result;

    task_result = xTaskCreate(perception_task, "perception_task", 4096, NULL, 1, NULL);
    if (task_result != pdPASS) {
        printf("Failed to create perception task\n");
        return;
    }

    task_result = xTaskCreate(control_task, "control_task", 4096, NULL, 3, &control_task_handle);
    if (task_result != pdPASS) {
        printf("Failed to create control task\n");
        return;
    }

    task_result = xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 1, NULL);
    if (task_result != pdPASS) {
        printf("Failed to create sensor task\n");
        return;
    }

    task_result = xTaskCreate(logger_task, "logger_task", 4096, NULL, 1, NULL);
    if (task_result != pdPASS) {
        printf("Failed to create logger task\n");
        return;
    }

    // Set timer settings
    esp_timer_create_args_t timer_args = {
        .callback = control_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "control_timer",
    };

    // Timer ID
    esp_timer_handle_t control_timer = NULL;

    // Creates the timer
    timer_result = esp_timer_create(&timer_args, &control_timer);
    if (timer_result != ESP_OK) {
            printf("Failed to create control timer\n");
            return;
    }

    // Start timer
    timer_result = esp_timer_start_periodic(control_timer, 20000); // 20ms
    if (timer_result != ESP_OK) {
        printf("Failed to start control timer\n");
        return;
    }


}
