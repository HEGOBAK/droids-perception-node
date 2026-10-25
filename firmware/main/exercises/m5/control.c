#include "control.h"

#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "../m3/servo.h"
#include "../m4/vision.h"
#include "base_yaw.h"

// Control task: wakes every 20 ms from the timer, reads the latest observation, and commands the servo
static const uint64_t CONTROL_RATE = 20000; // micro seconds

// Shared: latest observation + control task ID for the timer
static QueueHandle_t observation_queue;          // 1 slot: always the latest result
static TaskHandle_t control_task_handle = NULL;
static QueueHandle_t logger_queue;               // 64 slots: control → logger  

// Logger struct: control → logger
typedef enum { LOG_ROW = 0, LOG_SUMMARY = 1 } log_kind_t;

typedef struct {
    log_kind_t kind;

    // Data to be logged
    float time_s;
    float dt_ms;
    bool fresh;
    float age_ms;
    float cx;
    float e;
    float I;
    float theta_track;
    float yaw;
    float theta_stab;
    float theta;
    bool clamped;          // Clamp activated
    bool imu_valid;

    // Summary every 5 seconds (250 control loops at 20 ms each)
    int ticks;              
    float mean_dt;         
    float max_dt;          
    int late;               
    int log_drops;          

} logger_t;


// Percerption task ------------------------------------------------------------------------------------------------------------------------ //
static const unsigned PERCEPTION_PERIOD_MS = 100;  // try a new frame every 100 ms

static void perception_task(void *arg)  // priority 4
{
	TickType_t last_wake = xTaskGetTickCount();   // fixed start time per loop (M1 / step 1)
	
    while (1) {                 // a task never returns: it loops forever
        vision_result_t result;

        if (vision_process(&result, false, false) == ESP_OK) {
            xQueueOverwrite(observation_queue, &result);   // copy the struct in, replacing the old one
        } else {
            printf("Capture failed\n");
        }
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PERCEPTION_PERIOD_MS));
    }
}
// -------------------------------------------------------------------------------------------------------------------------------------------- //


// Control task ------------------------------------------------------------------------------------------------------------------------------- //
static const float KP = 20;                          // proportional gain (20)
static const float KI = 40;                          // integral gain (40)
static const float LIMIT_DEG = 30;                  // maximum angle to turn the servo (M5: ±30°)
static const float MAX_STEP_DEG = 1.2;              // maximum change per control loop (M5: 1.2° per 20 ms = 60°/s)
static const int64_t OBSERVATION_STALE_LIMIT_US = 200000;       // 200 ms: if the result is older than this, ignore it (M5: 200 ms)
static const int64_t SENSOR_STALE_LIMIT_US = 30000;  // 30 ms: if the sensor data is older than this, ignore it (M5: 30 ms)
static const int64_t LOST_TARGET_LIMIT_US = 2000000;  // 2 s: if the target is lost for this long, reset the integral term to 0
static const signed SIGN = -1;                      // -1: bigger pulse turns R, +1: bigger pulse turns L
static const bool STABILIZE = true;                        // true: use the IMU to stabilize the base, false: just track the target

static float clamp(float track, float min_deg, float max_deg, bool *clamped)   // clamp the angle to the safe limits
{
    if (track < min_deg) {
        *clamped = true;
        return min_deg;
    } else if (track > max_deg) {
        *clamped = true;
        return max_deg;
    } else {
        return track;
    }
}

static void control_timer_callback(void *argument)  // called from the timer ISR, not a task
{
    xTaskNotifyGive(control_task_handle); // Notify control_task by its ID
}

static logger_t log_helper(log_kind_t kind, float time_s, float dt_ms, bool fresh, float age_ms, 
                float cx, float e, float I, float theta_track, float yaw, float theta_stab, 
                float theta, bool clamped, bool imu_valid, int ticks, float mean_dt, 
                float max_dt, int late, int log_drops)
{
    return (logger_t){
        .kind = kind,
        .time_s = time_s,
        .dt_ms = dt_ms,
        .fresh = fresh,
        .age_ms = age_ms,
        .cx = cx,
        .e = e,
        .I = I,
        .theta_track = theta_track,
        .yaw = yaw,
        .theta_stab = theta_stab,
        .theta = theta,
        .clamped = clamped,
        .imu_valid = imu_valid,
        .ticks = ticks,
        .mean_dt = mean_dt,
        .max_dt = max_dt,
        .late = late,
        .log_drops = log_drops
    };
}

void control_task(void *argument)   // priority 6
{
	float last_theta = 0;
	float theta_track = 0;
    float theta_stab = 0;
    float yaw_ref = -100000000;
    float I = 0;
	int64_t now_us;
	int64_t last_us = esp_timer_get_time();
    int64_t last_target_us = 0;
	float dt;
	unsigned tick = 0;      
    float sum_dt = 0;
    float max_dt = 0;
    int log_drops = 0;
    int late = 0;

	while (1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   // sleep until timer's notification
        now_us = esp_timer_get_time();
        dt = (now_us - last_us) / 1e6f;     // seconds since last control loop (M1)
        last_us = now_us;

        theta_stab = 0;
		float theta = 0;
		float e = 0;
        bool clamped = false;
        
        // Check if the IMU is valid and use it to stabilize the base if STABILIZE is true
        sensor_packet_t imu_packet = {0};
        bool yaw_fresh = false;
        
        if (get_sensor_packet(&imu_packet) == pdTRUE) {
            yaw_fresh = imu_packet.valid && (now_us - imu_packet.time_us) < SENSOR_STALE_LIMIT_US;  // 30 ms stale limit

            if (yaw_ref == -100000000 && yaw_fresh) yaw_ref = imu_packet.yaw_deg;  // set the reference yaw on the first valid reading
            if (STABILIZE && yaw_fresh) theta_stab = -(imu_packet.yaw_deg - yaw_ref);  // stabilize the base using the IMU
            else theta_stab = 0;  
        }
        
        // Check if there is a new observation in the queue
        vision_result_t latest = {0};
        bool observation_fresh = false;
		if (xQueuePeek(observation_queue, &latest, 0) == pdTRUE) {      						// copy out, leave it for the next loop
			observation_fresh = latest.valid && (now_us - latest.capture_us) < OBSERVATION_STALE_LIMIT_US;		// 200 ms stale limit (M1)

			if (observation_fresh) {
                last_target_us = now_us;                          // remember when the target was last seen
    			e = (latest.cx - 79.5f) / 79.5f;    // normalized error: -1 = left, +1 = right, 0 = center
    			float I_try = I + e * dt;                        // integral of the error
                float u = KP * e + KI * I_try;                // control signal: proportional + integral
                if (fabsf(u) < LIMIT_DEG) {                          // anti-windup: if the control signal is saturated, don't integrate
                    I = I_try;                    
                }
    			theta_track = SIGN * (KP * e + KI * I);      // proportional control: angle to track the target
			}
			else {
                if (now_us - last_target_us > LOST_TARGET_LIMIT_US) {  // if the target has been lost for a long time, reset the integral term
                    I = 0;
                }
            }
		}

        // Servo command: combine tracking and stabilization, then clamp to the safe limits and limit the speed of change
        theta = clamp(theta_track + theta_stab, -LIMIT_DEG, +LIMIT_DEG, &clamped);                          // position limit
        theta = clamp(theta, last_theta - MAX_STEP_DEG, last_theta + MAX_STEP_DEG, &clamped);               // speed limit
        servo_set_angle_deg(theta);

        last_theta = theta;
        tick++;

        // Send timing data to logger queue
        sum_dt += dt;
        if (dt > max_dt) max_dt = dt;
        if (dt > 0.025f) late++;

        // log_helper(LOG_ROW, time_s, dt_ms, fresh, age_ms, cx, e, I, theta_track, yaw, theta_stab, theta, clamped, imu_valid, ticks, mean_dt, max_dt, late, log_drops)
        logger_t log_data;
        if (tick % 250 == 0) {
            log_data = log_helper(LOG_SUMMARY, now_us / 1e6f, dt * 1000.0f, observation_fresh, (now_us - latest.capture_us) / 1000.0f,
                            latest.cx, e, I, theta_track, imu_packet.yaw_deg, theta_stab, theta, clamped, (STABILIZE && yaw_fresh),
                            tick, sum_dt / 250.0f, max_dt * 1000.0f, late, log_drops);
            sum_dt = 0;
            max_dt = 0;
            log_drops = 0;
            late = 0;
        }
        else {
            log_data = log_helper(LOG_ROW, now_us / 1e6f, dt * 1000.0f, observation_fresh, (now_us - latest.capture_us) / 1000.0f,
                            latest.cx, e, I, theta_track, imu_packet.yaw_deg, theta_stab, theta, clamped, (STABILIZE && yaw_fresh),
                            tick, sum_dt / 250.0f, max_dt * 1000.0f, late, log_drops);
        }

        if (xQueueSend(logger_queue, &log_data, 0) != pdTRUE) log_drops++;
	}
}
// -------------------------------------------------------------------------------------------------------------------------------------------- //


// Logger task -------------------------------------------------------------------------------------------------------------------------------- //
static void print_log(logger_t log_data){
    printf("LOG,%.3f,%.2f,%d,%.0f,%.1f,%+.3f,%+.3f,%+.1f,%+.1f,%+.1f,%+.1f,%d,%d\n",
           log_data.time_s,
           log_data.dt_ms,
           log_data.fresh,
           log_data.age_ms,
           log_data.cx,
           log_data.e,
           log_data.I,
           log_data.theta_track,
           log_data.yaw,
           log_data.theta_stab,
           log_data.theta,
           log_data.clamped,
           log_data.imu_valid
    );
}

static void print_summary(logger_t log_data){
    printf("TIMING,%d,%.2f,%.2f,%d,%d\n",
           log_data.ticks,
           log_data.mean_dt,
           log_data.max_dt,
           log_data.late,
           log_data.log_drops);
}

static void logger_task(void *argument)   // priority 1
{
    printf("LOG Header: time_s,dt_ms,fresh,age_ms,cx,e,I,theta_track,yaw,theta_stab,theta,clamped,imu_valid\n");
    while (1) {
        logger_t log_data;
        if (xQueueReceive(logger_queue, &log_data, portMAX_DELAY) == pdTRUE) {
            if (log_data.kind == LOG_SUMMARY) {
                print_summary(log_data);
                print_log(log_data);
            } else {
                print_log(log_data);
            }
        }
    }
}
// -------------------------------------------------------------------------------------------------------------------------------------------- //


void run_control(void)
{
	observation_queue = xQueueCreate(1, sizeof(vision_result_t));    // mailbox of one slot
    if (observation_queue == NULL) {
        printf("Failed to create result queue\n");
        return;
    }

    logger_queue = xQueueCreate(64, sizeof(logger_t));    // 64 x 20 ms = ~1.3 s if logger stalls
    if (logger_queue == NULL) {
        printf("Failed to create logger queue\n");
        return;
    }

    if (xTaskCreate(control_task, "control", 4096, NULL, 6, &control_task_handle) != pdPASS) { 		// priority 6
		printf("Failed to create control task\n");
        return;
	}

    if (xTaskCreate(perception_task, "perception", 8192, NULL, 4, NULL) != pdPASS) { // priority 4
        printf("Failed to create perception task\n");
        return;
    }

    if (xTaskCreate(logger_task, "logger", 4096, NULL, 1, NULL) != pdPASS) { // priority 1
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
    esp_err_t timer_result = esp_timer_create(&timer_args, &control_timer);
    if (timer_result != ESP_OK) {
            printf("Failed to create control timer\n");
            return;
    }

    // Start timer
    timer_result = esp_timer_start_periodic(control_timer, CONTROL_RATE);
    if (timer_result != ESP_OK) {
        printf("Failed to start control timer\n");
        return;
    }
}