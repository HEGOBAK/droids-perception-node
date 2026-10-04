# Hardware and software readiness

[Start here](../START_HERE.md)


## Ordered and marked delivered

| Item | Pack / stated specification | Role | Delivery |
|---|---|---|---|
| Freenove ESP32 ESP32-S3 camera board kit, ASIN B0BMQ8F7FN | 8 MB flash; 1 GB card | Compute and camera | Oct 1 |
| Beffkkip SG90, ASIN B07MLR1498 | 4 micro servos | Actuation; spare units | Oct 1 |
| HiLetgo GY-521 MPU6050, ASIN B00LP25V1A | 3 modules | Accelerometer and gyro | Oct 1 |
| HC-SR04P, ASIN B0GL8NJCVT | 2 modules; listing states 3–5.5 V | Range sensing | Oct 2 |
| REXQualis basic electronics kit, ASIN B078XV3RK2 | Breadboard, jumpers, LEDs, resistors, power module | Prototyping | Oct 1 |

**Conclusion: the main component categories are covered. A complete working setup is not yet confirmed.** The HC-SR04P is a variant of the sensor named in the brief; its exact board behavior and logic levels must be checked. Pack sizes are listed descriptions, not counts verified by opening the packages.

## Check before buying anything else

- [ ] USB **data** cable fitting the actual board and Mac; adapter/hub if needed. A charging-only cable will not flash firmware.
- [ ] Working regulated servo supply, with connectors and current capacity covering the actual servo's load and startup/stall demand. A breadboard regulator module is not itself a power source. Do not assume it can supply a servo reliably.
- [ ] Multimeter available to borrow or use: check rail voltage, polarity, and continuity with power disconnected for continuity tests. Never place a meter in current mode directly across a supply.
- [ ] Suitable jumper ends (male/female combinations), breadboard access to needed pins, and fitted headers. A wide board may need two breadboards or breakout wires.
- [ ] Rigid base and light bracket for camera and range sensor, servo horn and screws; mounting for IMU on the base. A second axis needs another bracket and explicit geometry even though spare servos exist.
- [ ] Colored matte target large enough for both camera and ultrasonic beam, ruler/tape measure, and angle reference for controlled trials.
- [ ] Optional but useful: logic analyzer/oscilloscope, strain relief, and suitable local decoupling selected after power measurements. Soldering equipment is conditional on headers being unfitted.

Nothing here establishes a need for a new computer, GPU, Raspberry Pi, separate camera, Arduino board, or additional main sensors.

## Power and wiring gate

ESP32 GPIO is a 3.3 V interface. Do not wire an unverified 5 V signal to it. Verify the HC-SR04P's ECHO level at the chosen supply; a wide supply range in a listing is not proof of its output level. If necessary use a suitable level interface. A simple resistor divider is for a unidirectional signal; it is not a universal I2C level shifter. Confirm thresholds, resistor values, and signal timing before wiring.

Power the servo from a suitable regulated supply, not an ESP32 GPIO or its 3.3 V rail. Connect signal ground between the servo supply and controller. Avoid connecting two independent positive power rails together or backfeeding the USB port. Verify the board's power-path documentation before any external board power connection. Disconnect power before rewiring. Start with one unloaded servo and conservative travel.

The MPU6050 chip specification does not establish the supply and pull-up wiring of every GY-521 clone. Inspect the actual module; ensure I2C pull-ups terminate at a compatible logic voltage. Do not guess wiring from wire colors or another vendor's board photo.

## Pin plan: fill before connecting

Use the [Freenove vendor resources](https://github.com/Freenove/Freenove_ESP32_S3_WROOM_Board) only after confirming your board model/revision. The order title alone cannot confirm FNK0085 revision, camera model, PSRAM size, or header layout.

| Function | GPIO | Voltage / ownership | Confirmed against board |
|---|---|---|---|
| Camera bus, SCCB, clock | TBD | Reserve all vendor camera pins | No |
| IMU SDA / SCL | TBD / TBD | I2C owner; pull-ups verified | No |
| Servo signal | TBD | MCPWM output | No |
| Ultrasonic TRIG / ECHO | TBD / TBD | Output / protected input | No |
| Telemetry TX / RX or board bridge | TBD | Confirm actual UART path | No |

Reserve flash/PSRAM, USB, boot-strapping, camera, and any enabled SD-card pins before assigning peripherals. Leave SD disabled initially. A USB Serial/JTAG console is not automatically a hardware UART: for the brief's UART requirement, confirm an onboard USB-UART bridge or arrange a 3.3 V USB-UART adapter. Such an adapter is conditional, not yet a confirmed purchase need.

## Software audit

Git, GitHub CLI, and Python were found. `idf.py`, `cmake`, and `ninja` were absent from the active PATH; `IDF_PATH` was unset and standard ESP directories absent. Installation elsewhere remains possible. Follow M0 to install or activate ESP-IDF and record exact versions. Do not mark setup complete until an actual build and upload succeed.
