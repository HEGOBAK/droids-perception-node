# Firmware organization

- `app_main.c`: application entry point. M6 starts from the finished M5 system, so it still calls `m5_run()` until M6 adds its telemetry task.
- `idf_component.yml` + `../dependencies.lock`: the camera driver `espressif/esp32-camera` (2.1.8, built with ESP-IDF 6.1), used by M4 and later. Downloaded into `managed_components/` on the first build (not in Git). PSRAM (octal) is on in `../sdkconfig.defaults` for the camera frame buffer.
- `CMakeLists.txt`: lists only the files that are built now.
- `exercises/`: one folder per milestone. Earlier ones are saved and not built; M5 is built from here and reuses files from m2/, m3/ and m4/ by path (see `CMakeLists.txt`).
  - `m0/m0_basics.c` / `.h`: chip diagnostics and a seconds counter.
  - `m1/m1_tasks.c` / `.h`: task, queue, notification and timing exercise.
  - `m2/`: the full M2 IMU code.
    - `m2_main.c` / `.h`: `m2_run()` creates the bus, runs the IMU, then releases the bus (was `app_main`).
    - `i2c_bus.c` / `.h`: board wiring and shared I2C controller configuration (SDA41, SCL42).
    - `imu.c` / `.h`: `imu_init(bus, &device, &bias)`: probe, add the device, identity check, wake, range and low-pass filter configuration (each written and read back), then the gyro bias run; removes the device itself if a later step fails. `imu_run()` calls it, then the M2 run order. M5 calls `imu_init` directly.
    - `imu_calibration.c` / `.h`: stationary bias run (1000 samples → mean, std, min/max, temperature, dt), a short bias-removed gyro check, and `imu_read_corrected`: the one read every module uses (accel with Z offset/scale corrected, gyro with bias removed). Set `PRINT_RAW_ROWS` to true to log every raw sample.
    - `imu_tilt.c` / `.h`: roll and pitch from gravity (axes from the HW-123 arrows), and a 60 s tilt log for checking poses by hand.
    - `imu_filter.c` / `.h`: single-axis complementary filter for roll (gyro short-term, accelerometer long-term, measured dt), with a timed log comparing gyro-only, accel-only and filtered angles. Failed reads mark the angles INVALID without stopping the loop. `RUN_S` and `TEST_BIAS_ERROR_DPS` are the step 7 test knobs.
  - `m3/`: the full M3 range and servo code.
    - `m3_main.c` / `.h`: `m3_run()` (step 5): every 100 ms the servo steps 2° (sweep ±30°) and the range sensor pings a fixed target, printing `time_s,angle_deg,echo_us,distance_m` for 20 s, then the servo returns to 0.
    - `range.c` / `.h`: HC-SR04P on TRIG GPIO 21 / ECHO GPIO 1 (3.3 V). 10 µs trigger pulse, echo edges timed in an interrupt, task woken by notification, 40 ms timeout → invalid, never 0 m.
    - `servo.c` / `.h`: SG90 on GPIO 14 via MCPWM (timer → operator → comparator → generator, 1 tick = 1 µs, 20 ms frame). Calibrated: neutral 1500 µs, 10 µs per degree, + = L (counter-clockwise from above), safe limits 1050–1950 µs (±45°). `servo_set_angle_deg` / `servo_angle_to_us` take degrees; `servo_move_slowly` steps 10 µs per 20 ms. Powered from the ESP32 5V pin (USB), unloaded.
  - `m4/`: the full M4 camera and red-target code (board mounted USB-up).
    - `m4_main.c` / `.h`: `m4_run()`: a `perception` task runs `vision_process()` every 100 ms and copies each `vision_result_t` into a 1-slot queue (`xQueueOverwrite`); the main loop peeks it twice a second and prints `time_s,frame,age_ms,valid,cx,cy,confidence,red_pixels,blobs,process_ms,free_heap,reason`. Text previews every 10th frame; one frame dump after 3 s.
    - `vision.c` / `.h`: OV3660 on the ribbon connector (pins 4–13, 15–18), 160×120 RGB565, one frame buffer in PSRAM, `MIRROR = 1`. Per pixel RGB565 → RGB (0–255) → HSV → red rule (H ≤ 20° or ≥ 340°, S ≥ 0.5, V ≥ 0.25). Per 5×10 block '#' if more than half red. Valid only with ≥ 200 red pixels, exactly one blob (flood fill, ≥ 2 blocks) and confidence (main blob share) ≥ 0.8. Centroid = mean x, y of red pixels. ~49 ms per frame.
  - `m5/`: PID tracking and single-axis yaw stabilization (rig: servo boxed on a cardboard base, board on the horn, IMU flat on the breadboard).
    - `m5_main.c` / `.h`: `m5_run()`: `run_imu()` (keep the base still ~10 s for the bias), `servo_init()`, `vision_init()`, then `run_control()`.
    - `base_yaw.c` / `.h`: I2C bus + `imu_init`, then a `sensor` task (prio 5, every 10 ms) integrates `yaw += YAW_SIGN · gz · dt` (yaw axis gz, sign +1 = base turned left) and overwrites a 1-slot queue with `{yaw_deg, rate_dps, valid, time_us}`. `get_sensor_packet()` peeks it (returns pdFALSE if the IMU never started).
    - `control.c` / `.h`: `perception` task (prio 4, 100 ms) → 1-slot observation queue. `control` task (prio 6) woken by a 20 ms esp_timer via notification. PI tracking `theta_track = SIGN·(KP·e + KI·I)` with KP 20, KI 40, SIGN −1, anti-windup (I only updated while |u| < 30°), I frozen when the target is stale (> 200 ms), reset after 2 s lost. Stabilization `theta_stab = −(yaw − yaw_ref)` when the IMU packet is valid and < 30 ms old (`STABILIZE`). `theta = theta_track + theta_stab`, clamped to ±30° and 1.2° per tick (60 °/s). `logger` task (prio 1) prints one `LOG,…` row per tick from a 64-slot queue (drops counted) and a `TIMING,…` summary every 250 ticks. Control never prints or waits for a frame.

Headers declare functions; source files implement them. Include headers, not `.c` files. CMake lists the source files to compile and the linker connects their function calls.

## Running a saved milestone again

Each milestone needs two edits: list its files in `CMakeLists.txt`, then include its header and call its entry function in `app_main.c`. Paths are relative to `firmware/main/`. Undo both edits to go back to the current milestone.

**M0**
```cmake
idf_component_register(SRCS "app_main.c" "exercises/m0/m0_basics.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS "")
```
```c
#include "exercises/m0/m0_basics.h"

void app_main(void)
{
    m0_print_chip_info();
    m0_run_counter(); // Runs forever, so call it last.
}
```

**M1**
```cmake
idf_component_register(SRCS "app_main.c" "exercises/m1/m1_tasks.c"
                       PRIV_REQUIRES esp_timer
                       INCLUDE_DIRS "")
```
```c
#include "exercises/m1/m1_tasks.h"

void app_main(void)
{
    m1_start_tasks();
}
```

**M2**
```cmake
idf_component_register(SRCS "app_main.c"
                            "exercises/m2/m2_main.c" "exercises/m2/i2c_bus.c" "exercises/m2/imu.c"
                            "exercises/m2/imu_calibration.c" "exercises/m2/imu_tilt.c" "exercises/m2/imu_filter.c"
                       PRIV_REQUIRES esp_driver_i2c esp_timer
                       INCLUDE_DIRS "")
```
```c
#include "exercises/m2/m2_main.h"

void app_main(void)
{
    m2_run();
}
```

M2 needs the IMU wired on SDA41/SCL42. Its run happens once per boot: identity check, wake/range/low-pass setup with readback, 1000-sample gyro bias run, 10-sample bias check, then the 30 s roll filter log. Keep the board still until the filter log starts. The accel Z correction (offset 0.818 g, scale 1.036) is provisional, from hand-held upright/inverted poses. `host/plot_m2.py` plots the saved M2 logs in `results/M2/`.

**M3**
```cmake
idf_component_register(SRCS "app_main.c"
                            "exercises/m3/m3_main.c" "exercises/m3/range.c" "exercises/m3/servo.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_mcpwm esp_timer
                       INCLUDE_DIRS "")
```
```c
#include "exercises/m3/m3_main.h"

void app_main(void)
{
    m3_run();
}
```

M3 needs the range sensor on TRIG 21 / ECHO 1 (3.3 V rail) and the servo on GPIO 14 (5V rail, shared GND). To use only one device, call its functions directly: `range_init()` + `range_measure()`, or `servo_init()` + `servo_set_angle_deg()`. Saved M3 logs are in `results/M3/`.

**M4**
```cmake
idf_component_register(SRCS "app_main.c" "exercises/m4/m4_main.c" "exercises/m4/vision.c"
                       PRIV_REQUIRES esp_timer
                       INCLUDE_DIRS "")
```
```c
#include "exercises/m4/m4_main.h"

void app_main(void)
{
    m4_run();
}
```

M4 needs nothing wired except USB (camera on the ribbon), the board standing USB-up, and the camera library from `idf_component.yml`. Turn a saved frame into a picture with `python3 host/frame_to_png.py <monitor log>`. Saved M4 logs are in `results/M4/`.

**M5**
```cmake
idf_component_register(SRCS "app_main.c" "exercises/m5/m5_main.c" "exercises/m5/control.c" "exercises/m5/base_yaw.c"
                            "exercises/m3/servo.c" "exercises/m4/vision.c"
                            "exercises/m2/i2c_bus.c" "exercises/m2/imu.c" "exercises/m2/imu_calibration.c"
                            "exercises/m2/imu_filter.c" "exercises/m2/imu_tilt.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_mcpwm esp_timer esp_driver_i2c
                       INCLUDE_DIRS "")
```
```c
#include "exercises/m5/m5_main.h"

void app_main(void)
{
    m5_run();
}
```

M5 needs the servo on GPIO 14 (5V rail), the IMU on SDA41/SCL42 (3V3, shared GND), the camera on the ribbon and the red target ~30 cm in front. Keep the base still during the ~10 s bias run at boot. Set `STABILIZE` in `control.c` to compare with and without the IMU. Analyse logs with `python3 host/analyze_m5.py results/M5/b6-*.txt` (needs matplotlib); saved M5 logs are in `results/M5/`, plots in `figures/M5/`.
