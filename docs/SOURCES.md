# Sources and version policy

Reviewed October 2, 2026. Vendor and Espressif documentation support technical choices; the assignment PDF defines requested behavior. Read documentation matching the **installed** ESP-IDF version, because `stable` links move. Current stable pages displayed v6.1 when reviewed; this is not a claim that our unimplemented firmware is compatible or tested on it. Record and pin the actual working ESP-IDF release and camera component version during M0/M4.

- Local `Embedded Onboarding Tasks (2026).pdf`: task requirements and report deliverable; deliberately excluded from Git.
- [sim2real-pendulum](https://github.com/HEGOBAK/sim2real-pendulum): reference learning structure; README, START_HERE and M0–M4 were read locally. Its code and original worklog were not modified.
- [ESP32-S3 getting started](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/index.html): installation, build and upload entry point.
- [Freenove board resources](https://github.com/Freenove/Freenove_ESP32_S3_WROOM_Board) and [FNK0085 documentation](https://docs.freenove.com/projects/fnk0085/en/latest/index.html): identify the matching hardware before using pin maps. Arduino examples are hardware references, not a substitute for the assigned framework.
- [Espressif camera driver](https://github.com/espressif/esp32-camera): camera ownership, supported hardware and configuration.
- [ESP-IDF FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/freertos_idf.html): scheduling and communication.
- [MCPWM](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/mcpwm.html), [RMT](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/rmt.html), [I2C](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/i2c.html), [UART](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/uart.html): driver references.
- [TDK MPU6000/6050 specification](https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6000-datasheet1.pdf): chip reference; not a GY-521 breakout schematic.
- [REXQualis downloads](https://www.rexqualis.com/download/): locate the exact Basic Kit manual; contents and power capability still require confirmation.

Amazon order titles support only the inventory recorded in HARDWARE.md. No manufacturer-confirmed HC-SR04P schematic for the purchased module has been established. All performance thresholds, task rates, protocol layout and exercises in this guide are proposed project design choices unless labeled as brief requirements.
