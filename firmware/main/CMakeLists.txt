# Only the current milestone is built. Saved exercises in exercises/ are excluded.
# The camera driver (espressif/esp32-camera) stays listed in idf_component.yml, ready for M4/M5.
# Current: M6 starts from the finished M5 system (servo + camera + IMU, M2/M3/M4 files reused by path).
# M6 adds its telemetry task on top; until then this builds exactly M5.
idf_component_register(SRCS "app_main.c" "exercises/m5/m5_main.c" "exercises/m5/control.c" "exercises/m5/base_yaw.c" 
                              "exercises/m3/servo.c" "exercises/m4/vision.c"
                              "exercises/m2/i2c_bus.c" "exercises/m2/imu.c" "exercises/m2/imu_calibration.c"
                              "exercises/m2/imu_filter.c" "exercises/m2/imu_tilt.c"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_mcpwm esp_timer esp_driver_i2c
                       INCLUDE_DIRS "")
