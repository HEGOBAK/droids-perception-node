# Only the current M2 modules are built. Saved exercises are excluded.
idf_component_register(SRCS "app_main.c" "i2c_bus.c" "imu.c" "imu_calibration.c" "imu_tilt.c" "imu_filter.c"
                       PRIV_REQUIRES esp_driver_i2c esp_timer
                       INCLUDE_DIRS "")
