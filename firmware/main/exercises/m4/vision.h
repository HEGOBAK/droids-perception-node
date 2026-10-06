#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// One detection result. A small value struct: safe to COPY into a queue (never the frame pointer).
typedef struct {
    uint32_t frame;          // frame number since start
    int64_t capture_us;      // when the frame was taken (esp_timer µs, like M1/M2/M3)
    uint32_t process_us;     // how long the detection took
    bool valid;              // one real target found
    float cx;                // centroid in pixels (0–159, 80 = middle), only when valid
    float cy;                // (0–119, 60 = middle)
    float confidence;        // share of red blocks in the main blob (0–1)
    uint32_t red_pixels;     // red pixels in the whole frame
    int blobs;               // separate red blobs
    const char *reason;      // "ok" or why invalid (points to fixed text, so copying is safe)
} vision_result_t;

// Turn the camera on: 160×120 RGB565, frame buffer in PSRAM. Prints the detected sensor. Call once.
esp_err_t vision_init(void);

// Borrow one frame, find the red target, fill result, give the buffer back.
// print_preview: also print the brightness + mask text pictures.
// dump_if_valid: if the target is valid, also print the whole frame + mask as hex (for host/frame_to_ppm.py).
esp_err_t vision_process(vision_result_t *result, bool print_preview, bool dump_if_valid);
