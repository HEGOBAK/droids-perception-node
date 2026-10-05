# Firmware organization

- `app_main.c`: application entry point; creates the bus, runs the IMU and releases the bus.
- `i2c_bus.c` / `.h`: board wiring and shared I2C controller configuration (SDA41, SCL42).
- `imu.c` / `.h`: IMU address, communication speed, identity check, wake, range and low-pass filter configuration (each written and read back), then the M2 run order.
- `imu_calibration.c` / `.h`: stationary bias run (1000 samples → mean, std, min/max, temperature, dt), a short bias-removed gyro check, and `imu_read_corrected`: the one read every module uses (accel with Z offset/scale corrected, gyro with bias removed). Set `PRINT_RAW_ROWS` to true to log every raw sample.
- `imu_tilt.c` / `.h`: roll and pitch from gravity (axes from the HW-123 arrows), and a 60 s tilt log for checking poses by hand.
- `imu_filter.c` / `.h`: single-axis complementary filter for roll (gyro short-term, accelerometer long-term, measured dt), with a timed log comparing gyro-only, accel-only and filtered angles. Failed reads mark the angles INVALID without stopping the loop. `RUN_S` and `TEST_BIAS_ERROR_DPS` are the step 7 test knobs.
- `exercises/m0_basics.c`: saved chip diagnostics and counter.
- `exercises/m1_tasks.c`: saved task, queue, notification and timing exercise.

Headers declare functions; source files implement them. Include headers, not `.c` files. CMake lists the source files to compile and the linker connects their function calls.

The saved exercises are readable source but are excluded from CMake and do not run. To restore them later, add the required source files, dependencies and function declarations, then deliberately call their entry functions. The M0 counter runs forever; place it after any other startup work.

The M2 run happens once per boot, in this order: identity check, wake/range/low-pass setup with readback, 1000-sample gyro bias run, 10-sample bias check, then the 30 s roll filter log. Keep the board still until the filter log starts. An address response alone does not verify identity; expected WHO_AM_I is `0x68`.

The accel Z correction (offset 0.818 g, scale 1.036) is provisional: it comes from hand-held upright/inverted poses. Hardware runs so far are reported by the user; the assistant builds the code but has not run it on hardware.
