// Saved M3 module; not built. See firmware/main/README.md to run it again.
#include "range.h"

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Pins from the M3 pin plan. A pin has a name (the number) and a state (1 or 0).
static const gpio_num_t TRIG_PIN = GPIO_NUM_21;
static const gpio_num_t ECHO_PIN = GPIO_NUM_1;

// Speed of sound at about 20 °C. Changes about 0.6 m/s per °C.
static const float SOUND_SPEED_M_S = 343.0f;

// Longest echo is about 23 ms (4 m there and back). One tick is 10 ms (M2),
// so a 40 ms timeout really waits 30–40 ms, which still covers 23 ms.
static const unsigned ECHO_TIMEOUT_MS = 40;

// Shared with the ISR. volatile: the ISR can change them at any moment, so always re-read.
static volatile int64_t rise_us = 0;              // time ECHO went HIGH
static volatile int64_t fall_us = 0;              // time ECHO went LOW
static volatile TaskHandle_t waiting_task = NULL; // task to wake when the echo ends

// Runs the instant ECHO changes. Keep it tiny: save the time, wake the task, nothing else.
static void IRAM_ATTR echo_isr(void *arg)
{
    int64_t now_us = esp_timer_get_time(); // same microsecond clock as M1/M2 timestamps

    if (gpio_get_level(ECHO_PIN)) {
        rise_us = now_us;                  // went HIGH: echo starts
        return;
    }

    fall_us = now_us;                      // went LOW: echo ends
    // Only wake the task if a task is actually waiting and this ping's rising edge was seen.
    if (waiting_task != NULL && rise_us != 0) {
        BaseType_t higher_priority_woken = pdFALSE;
        // M1 notification, ISR version. Also notes if the woken task is more important than the interrupted one.
        vTaskNotifyGiveFromISR(waiting_task, &higher_priority_woken);
        portYIELD_FROM_ISR(higher_priority_woken); // if so, switch to it right away
    }
}

esp_err_t range_init(void)
{
    // TRIG: we drive it. Start LOW.
    gpio_config_t trig_config = {
        .pin_bit_mask = 1ULL << TRIG_PIN,
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t result = gpio_config(&trig_config);
    if (result != ESP_OK) {
        return result;
    }
    gpio_set_level(TRIG_PIN, 0); // TRIG off

    // ECHO: we read it and get an interrupt on both edges.
    // Pull-down keeps it LOW if the wire is unplugged, so that reads as "no echo".
    gpio_config_t echo_config = {
        .pin_bit_mask = 1ULL << ECHO_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE, // interrupt on LOW→HIGH (start) and HIGH→LOW (end)
    };
    result = gpio_config(&echo_config);
    if (result != ESP_OK) {
        return result;
    }

    // Start the shared GPIO interrupt service, then connect echo_isr to ECHO.
    result = gpio_install_isr_service(0);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }
    return gpio_isr_handler_add(ECHO_PIN, echo_isr, NULL);
}

esp_err_t range_measure(int64_t *echo_us)
{
    // ECHO must be LOW before a new ping, or an old echo would mix with the new one.
    if (gpio_get_level(ECHO_PIN) != 0) {
        return ESP_ERR_INVALID_STATE;
    }

    // Clear any late notification from an earlier ping, then reset the times.
    ulTaskNotifyTake(pdTRUE, 0);
    rise_us = 0;
    fall_us = 0;
    waiting_task = xTaskGetCurrentTaskHandle(); // the task running this function (main task)

    // 10 µs trigger pulse. vTaskDelay can't do µs (1 tick = 10 ms), so busy-wait this short bit only.
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    // Sleep until the ISR says the echo ended, or give up (same call as M1's control_task, with a timeout).
    uint32_t woken = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ECHO_TIMEOUT_MS));
    waiting_task = NULL;
    if (woken == 0) {
        return ESP_ERR_TIMEOUT; // no echo: invalid, never 0 m
    }

    *echo_us = fall_us - rise_us;
    return ESP_OK;
}

float range_echo_to_m(int64_t echo_us)
{
    return SOUND_SPEED_M_S * (echo_us / 1e6f) / 2.0f;
}
