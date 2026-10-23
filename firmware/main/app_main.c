#include <stdio.h>
#include "exercises/m3/servo.h"

void app_main(void)
{
    // M5 H1: hold the servo at 0° (1500 µs) so the servo body can be turned until the horn points forward.
    // Earlier milestones are saved in exercises/ (see README.md to run them again).
    esp_err_t result = servo_init(); // starts at 0° (1500 µs)
    if (result != ESP_OK) {
        printf("Servo setup failed: %s\n", esp_err_to_name(result));
        return;
    }
    printf("M5 H1: servo holding 0 deg (1500 us)\n");
    // app_main ends here; the MCPWM keeps sending the 1500 µs pulse by itself.
}
