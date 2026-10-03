# Droids Perception Node

Build and understand an ESP32-S3 system that sees a colored target, tracks it with a servo, measures distance and orientation, and sends data to a Python 3D viewer.

**[Start here](START_HERE.md) → M0–M8.** Use [MY_WORKLOG.md](MY_WORKLOG.md) for your explanations and measurements.

This is a learning guide and project foundation, not completed firmware. No hardware tests or performance results have been claimed.

## The path

**Bring up → measure → control → communicate → integrate → evaluate.**

| Guide | End product |
|---|---|
| [M0](Learning/M0.md) | Verified inventory and first ESP-IDF firmware upload |
| [M1](Learning/M1.md) | Three communicating, timed FreeRTOS tasks |
| [M2](Learning/M2.md) | Calibrated IMU readings and an orientation estimate |
| [M3](Learning/M3.md) | Bounded distance measurements and safe servo movement |
| [M4](Learning/M4.md) | Camera color centroid and confidence measurement |
| [M5](Learning/M5.md) | Fixed-rate PID tracking and single-axis stabilization |
| [M6](Learning/M6.md) | Tested binary telemetry and host parser |
| [M7](Learning/M7.md) | A geometrically defined 3D trajectory viewer |
| [M8](Learning/M8.md) | Repeatable experiments and a defensible report |

## Current status

- Amazon marked the camera board, servo pack, IMU pack, electronics kit, and ultrasonic pack delivered on October 1–2, 2026.
- Cable, servo power, measurement tools, and mounting still need physical confirmation. See [hardware audit](docs/HARDWARE.md).
- ESP-IDF, CMake, and Ninja were not found in the active shell; standard `~/esp` and `~/.espressif` folders were absent. This does not exclude installations elsewhere.
- No firmware, host application, sensor calibration, or hardware validation exists here yet.

## Reference and boundaries

The learning structure follows [sim2real-pendulum](https://github.com/HEGOBAK/sim2real-pendulum): small assignments, explicit checks, and explanations in your own words. Here the plant is physical hardware. PID runs online; it is not the neural policy trained with Adam in the pendulum project.

The local onboarding PDF is the assignment source. It specifies ESP-IDF rather than Arduino, task communication, MCPWM servo control, IMU/distance sensing, binary UART packets, and host-side mapping. The report explains the process. See [requirements](docs/REQUIREMENTS.md) and [sources](docs/SOURCES.md).

The source PDF stays local and is excluded from Git. All milestone statuses begin incomplete. Preserve failures and actual measurements; do not invent a successful result.
