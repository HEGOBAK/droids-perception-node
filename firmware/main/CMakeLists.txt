# Only the current milestone is built. Saved exercises in exercises/ are excluded.
# The camera driver (espressif/esp32-camera) stays listed in idf_component.yml, ready for M4/M5.
idf_component_register(SRCS "app_main.c"
                       INCLUDE_DIRS "")
