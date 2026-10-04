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
