// Saved M3 module; not built. See firmware/main/README.md to run it again.
#include "servo.h"

#include "driver/mcpwm_prelude.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Pin from the M3 pin plan (docs/HARDWARE.md).
static const int SERVO_PIN = 14;

// MCPWM timer: 1 tick = 1 µs, so pulse widths are written directly in µs.
// This is a hardware counter, not the FreeRTOS 10 ms tick.
static const uint32_t TIMER_HZ = 1000000;
static const uint32_t FRAME_US = 20000;           // one pulse every 20 ms (50 Hz)

// Servo settings, all in one place (M5 will use them).
// Step 4 calibration (2026-10-05, unloaded, USB 5V):
// 1000 µs = R50, 1500 µs = 0, 2000 µs = L50, no buzz → 10 µs per degree, bigger pulse = L.
// Angle sign: + = L = counter-clockwise seen from above (shaft up).
static const uint32_t NEUTRAL_US = 1500;
static const float US_PER_DEG = 10.0f;
static const float DIRECTION = +1.0f;             // +1: bigger pulse turns L (+)
// Safe limits: measured ±50° minus a 5° margin = ±45°.
static const uint32_t MIN_US = 1050;              // R45
static const uint32_t MAX_US = 1950;              // L45

// Slow moves: 10 µs every 20 ms = 500 µs per second. Small steps = small current spikes.
static const uint32_t STEP_US = 10;
static const unsigned STEP_DELAY_MS = 20;

static mcpwm_cmpr_handle_t comparator = NULL;     // the "mark" that sets the pulse width
static uint32_t current_us = 1500;                // last commanded pulse width

esp_err_t servo_init(void)
{
    // 1. Timer: counts 0 → 19999 µs, then restarts (= the 20 ms frame).
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = TIMER_HZ,
        .period_ticks = FRAME_US,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    esp_err_t result = mcpwm_new_timer(&timer_config, &timer);
    if (result != ESP_OK) {
        return result;
    }

    // 2. Operator: the box that links the timer to the comparator and generator.
    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t oper_config = {
        .group_id = 0, // same group as the timer
    };
    result = mcpwm_new_operator(&oper_config, &oper);
    if (result != ESP_OK) {
        return result;
    }
    result = mcpwm_operator_connect_timer(oper, timer);
    if (result != ESP_OK) {
        return result;
    }

    // 3. Comparator: holds the mark. New values apply when the count restarts,
    //    so a pulse is never cut in half.
    mcpwm_comparator_config_t cmpr_config = {
        .flags.update_cmp_on_tez = true,
    };
    result = mcpwm_new_comparator(oper, &cmpr_config, &comparator);
    if (result != ESP_OK) {
        return result;
    }

    // 4. Generator: drives GPIO 14. HIGH when the count starts at 0, LOW when it hits the mark.
    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t gen_config = {
        .gen_gpio_num = SERVO_PIN,
    };
    result = mcpwm_new_generator(oper, &gen_config, &generator);
    if (result != ESP_OK) {
        return result;
    }
    result = mcpwm_generator_set_action_on_timer_event(generator,
        MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
    if (result != ESP_OK) {
        return result;
    }
    result = mcpwm_generator_set_action_on_compare_event(generator,
        MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW));
    if (result != ESP_OK) {
        return result;
    }

    // Set the mark to neutral before starting, so the very first pulse is safe.
    result = servo_set_pulse_us(NEUTRAL_US);
    if (result != ESP_OK) {
        return result;
    }

    // Start the timer. From here the hardware repeats the pulse by itself.
    result = mcpwm_timer_enable(timer);
    if (result != ESP_OK) {
        return result;
    }
    return mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP);
}

esp_err_t servo_set_pulse_us(uint32_t pulse_us)
{
    // Never command past the safe limits, whatever the caller asks.
    if (pulse_us < MIN_US) {
        pulse_us = MIN_US;
    }
    if (pulse_us > MAX_US) {
        pulse_us = MAX_US;
    }

    esp_err_t result = mcpwm_comparator_set_compare_value(comparator, pulse_us); // 1 tick = 1 µs
    if (result == ESP_OK) {
        current_us = pulse_us;
    }
    return result;
}

uint32_t servo_angle_to_us(float angle_deg)
{
    // Angle → pulse with the calibration. Round to the nearest µs.
    float pulse = NEUTRAL_US + DIRECTION * angle_deg * US_PER_DEG;
    if (pulse < 0.0f) {
        pulse = 0.0f; // never negative; servo_set_pulse_us clamps to the safe limits anyway
    }
    return (uint32_t)(pulse + 0.5f);
}

esp_err_t servo_set_angle_deg(float angle_deg)
{
    return servo_set_pulse_us(servo_angle_to_us(angle_deg));
}

esp_err_t servo_move_slowly(uint32_t target_us)
{
    // Step from where we are towards the target, one small step per 20 ms frame.
    while (current_us != target_us) {
        uint32_t next_us = current_us;
        if (target_us > current_us) {
            next_us = (target_us - current_us > STEP_US) ? current_us + STEP_US : target_us;
        } else {
            next_us = (current_us - target_us > STEP_US) ? current_us - STEP_US : target_us;
        }

        uint32_t before_us = current_us;
        esp_err_t result = servo_set_pulse_us(next_us);
        if (result != ESP_OK) {
            return result;
        }
        if (current_us == before_us) {
            break; // Clamped at a limit: can't get closer, so stop.
        }
        vTaskDelay(pdMS_TO_TICKS(STEP_DELAY_MS));
    }
    return ESP_OK;
}
