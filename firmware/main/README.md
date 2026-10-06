# Firmware organization

- `app_main.c`: application entry point for the current milestone (M4).
- `CMakeLists.txt`: lists only the files that are built now.
- `exercises/`: saved earlier milestones. Readable, but not built and do not run.
  - `m0/m0_basics.c` / `.h`: chip diagnostics and a seconds counter.
  - `m1/m1_tasks.c` / `.h`: task, queue, notification and timing exercise.
  - `m2/`: the full M2 IMU code.
    - `m2_main.c` / `.h`: `m2_run()` creates the bus, runs the IMU, then releases the bus (was `app_main`).
    - `i2c_bus.c` / `.h`: board wiring and shared I2C controller configuration (SDA41, SCL42).
    - `imu.c` / `.h`: IMU address, communication speed, identity check, wake, range and low-pass filter configuration (each written and read back), then the M2 run order.
    - `imu_calibration.c` / `.h`: stationary bias run (1000 samples → mean, std, min/max, temperature, dt), a short bias-removed gyro check, and `imu_read_corrected`: the one read every module uses (accel with Z offset/scale corrected, gyro with bias removed). Set `PRINT_RAW_ROWS` to true to log every raw sample.
    - `imu_tilt.c` / `.h`: roll and pitch from gravity (axes from the HW-123 arrows), and a 60 s tilt log for checking poses by hand.
    - `imu_filter.c` / `.h`: single-axis complementary filter for roll (gyro short-term, accelerometer long-term, measured dt), with a timed log comparing gyro-only, accel-only and filtered angles. Failed reads mark the angles INVALID without stopping the loop. `RUN_S` and `TEST_BIAS_ERROR_DPS` are the step 7 test knobs.
  - `m3/`: the full M3 range and servo code.
    - `m3_main.c` / `.h`: `m3_run()` (step 5): every 100 ms the servo steps 2° (sweep ±30°) and the range sensor pings a fixed target, printing `time_s,angle_deg,echo_us,distance_m` for 20 s, then the servo returns to 0.
    - `range.c` / `.h`: HC-SR04P on TRIG GPIO 21 / ECHO GPIO 1 (3.3 V). 10 µs trigger pulse, echo edges timed in an interrupt, task woken by notification, 40 ms timeout → invalid, never 0 m.
    - `servo.c` / `.h`: SG90 on GPIO 14 via MCPWM (timer → operator → comparator → generator, 1 tick = 1 µs, 20 ms frame). Calibrated: neutral 1500 µs, 10 µs per degree, + = L (counter-clockwise from above), safe limits 1050–1950 µs (±45°). `servo_set_angle_deg` / `servo_angle_to_us` take degrees; `servo_move_slowly` steps 10 µs per 20 ms. Powered from the ESP32 5V pin (USB), unloaded.

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
