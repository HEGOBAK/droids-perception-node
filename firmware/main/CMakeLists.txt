# Only the current milestone is built. Saved exercises in exercises/ are excluded.
# The camera driver (espressif/esp32-camera) stays listed in idf_component.yml, ready for M4/M5.
# M5 H1: servo only, to hold 0° while the servo body is lined up.
idf_component_register(SRCS "app_main.c" "exercises/m3/servo.c"
                       PRIV_REQUIRES esp_driver_mcpwm
                       INCLUDE_DIRS "")
