// Saved M4 module; not built. See firmware/main/README.md to run it again.
#include "vision.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

static uint32_t frame_count = 0;                 // frames processed since start

// Freenove ESP32-S3 WROOM camera pins (the underlined pins on the board, M3 pin plan).
// -1 = not connected on this board.
static const camera_config_t CAMERA_CONFIG = {
    .pin_pwdn = -1,
    .pin_reset = -1,
    .pin_xclk = 15,          // clock we give the camera
    .pin_sccb_sda = 4,       // settings bus (like I2C, M2)
    .pin_sccb_scl = 5,
    .pin_d7 = 16,            // 8 data wires: one byte per pixel clock
    .pin_d6 = 17,
    .pin_d5 = 18,
    .pin_d4 = 12,
    .pin_d3 = 10,
    .pin_d2 = 8,
    .pin_d1 = 9,
    .pin_d0 = 11,
    .pin_vsync = 6,          // "new picture starts"
    .pin_href = 7,           // "a row is being sent"
    .pin_pclk = 13,          // pixel clock

    .xclk_freq_hz = 20000000,         // 20 MHz
    .ledc_timer = LEDC_TIMER_0,       // hardware that makes XCLK
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_RGB565, // 2 bytes per pixel, colour kept
    .frame_size = FRAMESIZE_QQVGA,    // 160×120
    .jpeg_quality = 12,               // unused (not JPEG)
    .fb_count = 1,                    // one frame buffer: borrow it, then give it back
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

// Orientation (step 1): board mounted USB at the TOP. With the board USB-down the image was
// upside down only; turning the board over rotates it 180°, which leaves it mirrored instead.
static const int FLIP_VERTICAL = 0;
static const int MIRROR = 1;

// The text picture is a tiny, rough preview of the camera's actual frame,
// drawn with letters in the serial monitor:
// 32 columns × 12 rows, each character = 5×10 pixels.
// Characters are about twice as tall as wide, so 5×10 keeps the picture's shape.
#define PREVIEW_COLS 32
#define PREVIEW_ROWS 12
static const char RAMP[] = " .:-=+*#%@";   // dark → bright

// Red rule (step 3). Red sits at both ends of the hue circle, so it needs two hue ranges.
// Tested on a red circle on the iPad under the desk lamp (H ≈ 5°, S ≈ 0.77, V ≈ 0.84).
static const float RED_HUE_LOW_MAX = 20.0f;    // 0–20°: red, slightly orange
static const float RED_HUE_HIGH_MIN = 340.0f;  // 340–360°: red, slightly pink
static const float RED_SAT_MIN = 0.5f;         // strong colour (rejects white, grey, skin)
static const float RED_VAL_MIN = 0.25f;        // not almost black (dark pixels have unreliable hue)

// Step 5: when is a red area a real target?
static const uint32_t MIN_RED_PIXELS = 200;    // fewer red pixels = noise
static const int MIN_BLOB_BLOCKS = 2;          // a lone '#' block is noise, not a blob
static const float MIN_CONFIDENCE = 0.8f;      // main blob must hold ≥ 80% of the red blocks

// One pixel's colour: RGB (0-255) and HSV
typedef struct {
    uint8_t r;  // red
    uint8_t g;  // green
    uint8_t b;  // blue
    float h;    // hue, 0-360 degrees
    float s;    // saturation, 0-1
    float v;    // value (brightness, 0-1)
} pixel_color_t;

esp_err_t vision_init(void)
{
    esp_err_t result = esp_camera_init(&CAMERA_CONFIG);
    if (result != ESP_OK) {
        return result;
    }

    // Which sensor answered on the settings bus (like WHO_AM_I in M2).
    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    camera_sensor_info_t *info = esp_camera_sensor_get_info(&sensor->id);
    printf("Camera sensor: %s (PID 0x%04X)\n", info ? info->name : "unknown", (unsigned)sensor->id.PID);

    sensor->set_vflip(sensor, FLIP_VERTICAL);
    sensor->set_hmirror(sensor, MIRROR);

    printf("Free PSRAM: %u bytes\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return ESP_OK;
}

// RGB565 pixel (2 bytes, high byte first) → brightness 0–255.
static uint8_t pixel_brightness(const uint8_t *bytes, pixel_color_t *color)
{
    // Save rgb as well for hsv
    uint16_t pixel = ((uint16_t)bytes[0] << 8) | bytes[1];  // join 2 bytes, like M2's decode
    uint8_t r5 = (pixel >> 11) & 0x1F;                    // top 5 bits (0-31)
    uint8_t g6 = (pixel >> 5) & 0x3F;                     // middle 6 bits (0-63)
    uint8_t b5 = pixel & 0x1F;                            // bottom 5 bits (0-31)

    // Scale each to 0–255 BEFORE saving, so HSV compares fair numbers (green's 63 ≠ red's 31).
    color->r = r5 * 255 / 31;
    color->g = g6 * 255 / 63;
    color->b = b5 * 255 / 31;

    // Average of the three = brightness.
    return (uint8_t)((color->r + color->g + color->b) / 3);
}

// Transform rgb to hsv for color detection
static void rgb_to_hsv(pixel_color_t *color)
{
    // Strongest and weakest channel.
    uint8_t max = color->r > color->g ? color->r : color->g;
    max = max > color->b ? max : color->b;
    uint8_t min = color->r < color->g ? color->r : color->g;
    min = min < color->b ? min : color->b;
    float range = (float)(max - min);  // float, so divisions keep decimals

    color->v = max / 255.0f;

    // Black: no colour at all. Stop here to avoid dividing by 0.
    if (max == 0) {
        color->s = 0;
        color->h = 0;
        return;
    }
    color->s = range / max;

    // Grey: all channels equal, so hue means nothing. Avoid dividing by 0.
    if (range == 0) {
        color->h = 0;
        return;
    }

    // Hue depends on which channel is strongest.
    if (max == color->r) {
        color->h = 60.0f * (color->g - color->b) / range;           // -60 to 60, 0 = red
    } else if (max == color->g) {
        color->h = 60.0f * (color->b - color->r) / range + 120.0f;  // 60 to 180, 120 = green
    } else {
        color->h = 60.0f * (color->r - color->g) / range + 240.0f;  // 180 to 360, 240 = blue
    }

    // Red wraps around: -20° is the same as 340°.
    if (color->h < 0) {
        color->h += 360.0f;
    }
}

// Is this pixel red? Hue near 0° (either side of the wrap) AND strong colour AND not too dark.
static bool is_red(const pixel_color_t *color)
{
    bool red_hue = color->h <= RED_HUE_LOW_MAX || color->h >= RED_HUE_HIGH_MIN;
    return red_hue && color->s >= RED_SAT_MIN && color->v >= RED_VAL_MIN;
}

// find the separate blobs of '#' in the mask (blocks touching up/down/left/right = same blob).
// Returns how many blobs have at least MIN_BLOB_BLOCKS blocks, plus the biggest blob's size
// and the total number of '#' blocks.
static int count_blobs(char masks[PREVIEW_ROWS][PREVIEW_COLS + 1], int *largest, int *red_blocks)
{
    bool seen[PREVIEW_ROWS][PREVIEW_COLS] = {{false}};  // "already painted?" for each block
    int queue[PREVIEW_ROWS * PREVIEW_COLS];             //
    int blobs = 0;
    *largest = 0;
    *red_blocks = 0;

    for (int row = 0; row < PREVIEW_ROWS; row++) {
        for (int col = 0; col < PREVIEW_COLS; col++) {
            if (masks[row][col] != '#') {
                continue;
            }
            (*red_blocks)++;
            if (seen[row][col]) {
                continue;                        // already part of a blob we counted
            }

            // New blob: start at this block and spread to every touching '#' block.
            int size = 0;
            int head = 0, tail = 0;
            queue[tail++] = row * PREVIEW_COLS + col;   // store (row, col) as one number
            seen[row][col] = true;
            while (head < tail) {
                int r = queue[head] / PREVIEW_COLS;
                int c = queue[head] % PREVIEW_COLS;
                head++;
                size++;

                // up, down, left, right
                const int dr[4] = {-1, 1, 0, 0};
                const int dc[4] = {0, 0, -1, 1};

                for (int k = 0; k < 4; k++) {
                    int nr = r + dr[k], nc = c + dc[k];
                    if (nr < 0 || nr >= PREVIEW_ROWS || nc < 0 || nc >= PREVIEW_COLS) {
                        continue;                         // off the picture
                    }
                    if (masks[nr][nc] == '#' && !seen[nr][nc]) {
                        seen[nr][nc] = true;
                        queue[tail++] = nr * PREVIEW_COLS + nc;
                    }
                }
            }

            if (size >= MIN_BLOB_BLOCKS) {  // 1 lone block = noise, don't count it
                blobs++;
            }
            if (size > *largest) {          // remember largest
                *largest = size;
            }
        }
    }
    return blobs;
}

// print the whole frame as text, so the Mac can turn it into a picture (host/frame_to_ppm.py).
// One line per pixel row: 640 hex characters (160 pixels × 2 bytes RGB565 × 2 characters per byte), a space,
// then 40 hex characters = 160 mask bits (1 = red pixel, same is_red rule as on the board).
static void dump_frame(const camera_fb_t *fb, const vision_result_t *result)
{
    static const char HEX[] = "0123456789ABCDEF";
    char line[160 * 4 + 1 + 40 + 1];
    printf("FRAME_BEGIN %u %u %d %.1f %.1f\n", (unsigned)fb->width, (unsigned)fb->height,
           result->valid, result->cx, result->cy);
    for (int y = 0; y < fb->height; y++) {
        int n = 0;
        uint8_t bits = 0;                                     // 4 mask bits → 1 hex character
        char mask_hex[41];
        int m = 0;
        for (int x = 0; x < fb->width; x++) {
            const uint8_t *bytes = &fb->buf[(y * fb->width + x) * 2];
            line[n++] = HEX[bytes[0] >> 4];                   // 2 bytes → 4 hex characters
            line[n++] = HEX[bytes[0] & 0x0F];
            line[n++] = HEX[bytes[1] >> 4];
            line[n++] = HEX[bytes[1] & 0x0F];

            pixel_color_t color = {0};
            pixel_brightness(bytes, &color);
            rgb_to_hsv(&color);
            bits = (bits << 1) | (is_red(&color) ? 1 : 0);
            if (x % 4 == 3) {
                mask_hex[m++] = HEX[bits];
                bits = 0;
            }
        }
        mask_hex[m] = '\0';
        line[n++] = ' ';
        line[n] = '\0';
        printf("%s%s\n", line, mask_hex);
    }
    printf("FRAME_END\n");
}

esp_err_t vision_process(vision_result_t *result, bool print_preview, bool dump_if_valid)
{
    // Borrow the latest frame (pointer into the driver's buffer, not our own copy).
    camera_fb_t *fb = esp_camera_fb_get();
    int64_t now_us = esp_timer_get_time();                                      // capture time
    if (fb == NULL) {
        return ESP_FAIL; // no frame
    }
    frame_count++;

    // Two pictures side by side, one block = one character:
    // left = brightness (as step 1), right = red mask ('#' if more than half the block is red).
    int block_w = fb->width / PREVIEW_COLS;
    int block_h = fb->height / PREVIEW_ROWS;
    int block_pixels = block_w * block_h;                                       // 50
    uint32_t red_total = 0;                                                     // red pixels in the whole frame (N)
    uint32_t sum_x = 0;                                                         // sum of x of every red pixel
    uint32_t sum_y = 0;                                                         // sum of y of every red pixel

    // Lines are kept and printed after the loops, so the centroid can be marked on the mask.
    char lines[PREVIEW_ROWS][PREVIEW_COLS + 1];
    char masks[PREVIEW_ROWS][PREVIEW_COLS + 1];

    for (int row = 0; row < PREVIEW_ROWS; row++) {                              // which TEXT row (0-11)
        for (int col = 0; col < PREVIEW_COLS; col++) {                          // which TEXT column (0-31)
            uint32_t sum = 0;
            int red_count = 0;
            for (int y = row * block_h; y < (row + 1) * block_h; y++) {         // pixel(frame) rows of this block
                for (int x = col * block_w; x < (col + 1) * block_w; x++) {     // pixels(frame) across this block
                    pixel_color_t color = {0};
                    sum += pixel_brightness(&fb->buf[(y * fb->width + x) * 2], &color); // row by row, 2 bytes each
                    rgb_to_hsv(&color);
                    if (is_red(&color)) {
                        red_count++;                                            // count red pixels
                        sum_x += x;                                             // step 4: add its position
                        sum_y += y;
                    }
                }
            }
            uint32_t mean = sum / block_pixels;
            lines[row][col] = RAMP[mean * (sizeof(RAMP) - 1) / 256];
            masks[row][col] = (red_count * 2 > block_pixels) ? '#' : '.';      // more than half red → '#'
            red_total += red_count;
        }
        lines[row][PREVIEW_COLS] = '\0';
        masks[row][PREVIEW_COLS] = '\0';
    }

    // is it ONE real target? Check size, then count blobs, then confidence.
    int largest = 0, red_blocks = 0;
    int blobs = count_blobs(masks, &largest, &red_blocks);
    // Confidence only means something when there is a blob (blobs > 0 also means red_blocks ≥ 2, so no ÷ 0).
    float confidence = blobs > 0 ? (float)largest / red_blocks : 0.0f;         // share of red in the main blob

    const char *reason = NULL;                                                  // why invalid (NULL = valid)
    if (red_total == 0) {
        reason = "no red pixels";
    } else if (red_total < MIN_RED_PIXELS) {
        reason = "too small (noise)";
    } else if (blobs == 0) {
        reason = "red scattered, no blob";
    } else if (blobs > 1) {
        reason = "ambiguous: more than one blob";
    } else if (confidence < MIN_CONFIDENCE) {
        reason = "low confidence";
    }
    bool valid = (reason == NULL);

    // centroid = average position of the red pixels. Only used when valid (never "the middle").
    float cx = valid ? (float)sum_x / red_total : 0.0f;                         // 0–159, 80 = middle
    float cy = valid ? (float)sum_y / red_total : 0.0f;                         // 0–119, 60 = middle
    if (valid) {
        masks[(int)cy / block_h][(int)cx / block_w] = 'X';                      // mark it on the mask
    }

    // fill the small result (copied into the queue by the caller, never the frame itself).
    result->frame = frame_count;
    result->capture_us = now_us;
    result->process_us = (uint32_t)(esp_timer_get_time() - now_us);             // detection time, before any printing
    result->valid = valid;
    result->cx = cx;
    result->cy = cy;
    result->confidence = confidence;
    result->red_pixels = red_total;
    result->blobs = blobs;
    result->reason = valid ? "ok" : reason;

    if (print_preview) {
        printf("+--------------------------------+  +--------------------------------+  frame %u\n", (unsigned)frame_count);
        for (int row = 0; row < PREVIEW_ROWS; row++) {
            printf("|%s|  |%s|\n", lines[row], masks[row]);
        }
        printf("+--------------------------------+  +--------------------------------+\n");
        printf(" brightness                          red mask (X = centroid)\n");
    }
    if (dump_if_valid && valid) {
        dump_frame(fb, result);
    }

    // Give the buffer back so the driver can fill it again.
    esp_camera_fb_return(fb);
    return ESP_OK;
}
