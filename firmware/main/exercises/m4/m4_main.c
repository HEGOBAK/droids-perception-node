// Saved M4 exercise (step 6: perception task + 1-slot queue); not built. See firmware/main/README.md to run it again.
#include "m4_main.h"

#include <stdio.h>
#include "esp_err.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "vision.h"

// M4 step 6: a perception task finds the target and sends a small result through a 1-slot queue
// (M1 pattern: latest value only, a copy, never the frame pointer). The main task reads and prints it.

static const unsigned PERCEPTION_PERIOD_MS = 100;  // try a new frame every 100 ms
static const unsigned PREVIEW_EVERY = 10;          // text pictures only every 10th frame (printing is slow)
static const int64_t DUMP_AFTER_US = 3000000;      // save one frame once valid, after 3 s (colours settled)
static const unsigned REPORT_PERIOD_MS = 500;      // main task prints the latest result twice a second

static QueueHandle_t result_queue = NULL;          // 1 slot: always the latest result

static void perception_task(void *arg)
{
    bool dumped = false;        // has the frame been dumped yet
    unsigned loop = 0;          // counts passes, to print the preview every PREVIEW_EVERY
    while (1) {                 // a task never returns: it loops forever

        vision_result_t result;
        bool preview = (loop % PREVIEW_EVERY == 0);

        // After 3 s, because the first frames have slightly off colours while the camera's auto exposure settles.
        bool want_dump = !dumped && esp_timer_get_time() > DUMP_AFTER_US;

        if (vision_process(&result, preview, want_dump) == ESP_OK) {
            xQueueOverwrite(result_queue, &result);   // copy the struct in, replacing the old one
            if (want_dump && result.valid) {
                dumped = true;                        // only one frame dump per run
            }
        } else {
            printf("Capture failed\n");
        }
        loop++;
        vTaskDelay(pdMS_TO_TICKS(PERCEPTION_PERIOD_MS));
    }
}

void m4_run(void)
{
    esp_err_t result = vision_init();
    if (result != ESP_OK) {
        printf("Camera setup failed: %s\n", esp_err_to_name(result));
        return;
    }

    result_queue = xQueueCreate(1, sizeof(vision_result_t));    // mailbox of one slot
    if (result_queue == NULL) {
        printf("Failed to create result queue\n");
        return;
    }
    if (xTaskCreate(perception_task, "perception", 8192, NULL, 5, NULL) != pdPASS) { // priority 5
        printf("Failed to create perception task\n");
        return;
    }

    printf("time_s,frame,age_ms,valid,cx,cy,confidence,red_pixels,blobs,process_ms,free_heap,reason\n");
    while (1) {
        vision_result_t latest;
        if (xQueuePeek(result_queue, &latest, 0) == pdTRUE) {      // copy out, leave it for the next look
            int64_t now_us = esp_timer_get_time();
            printf("%.2f,%u,%.0f,%d,%.1f,%.1f,%.2f,%u,%d,%.1f,%u,%s\n",
                   now_us / 1e6f,                                           // current time
                   (unsigned)latest.frame,                                  // frame number
                   (now_us - latest.capture_us) / 1000.0f,                  // how old the result is (age)
                   latest.valid, latest.cx, latest.cy, latest.confidence,   // data
                   (unsigned)latest.red_pixels, latest.blobs,               // data
                   latest.process_us / 1000.0f,                             // process time
                   (unsigned)esp_get_free_heap_size(),                      // watch for leaks
                   latest.reason
                );
        }
        vTaskDelay(pdMS_TO_TICKS(REPORT_PERIOD_MS));
    }
}
