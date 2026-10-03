# Requirement-to-evidence map

Source: local `Embedded Onboarding Tasks (2026).pdf`, four pages. The PDF describes a report as the submission while separately specifying technical behavior. Treat both as relevant; this guide does not interpret "report" as permission to omit the technical work.

| Brief requirement | Milestone | Evidence to save |
|---|---|---|
| ESP-IDF, not Arduino abstraction | M0 | Version, build command, successful flash log |
| Three tasks, priorities, communication | M1, M8 | Task diagram, timing measurements, resource ownership |
| Camera centroid through FreeRTOS queue | M4 | Frame, mask, centroid, queue behavior |
| Fixed-rate PID with notifications/semaphores | M5 | Timestamp log, loop jitter and missed deadlines |
| Servo through MCPWM | M3, M5 | Configuration and measured pulse behavior |
| MPU6050 over I2C and range via RMT/timer | M2, M3 | Raw/calibrated sensor data, timeout behavior |
| Base-motion compensation | M5 | Matched enabled/disabled experiment |
| Binary UART: header, timestamp, X, distance, attitude, servo, integrity | M6 | Packet spec, parser tests, captured stream |
| Python 3D viewer using range and orientation | M7 | Frame definitions, synthetic checks, measured trajectory |
| Development report explaining decisions and challenges | M8 | Report, reproducible run steps, evidence and limitations |

## Scope questions to resolve explicitly

One servo rotates around one axis; it cannot independently correct pitch, roll, and yaw. Begin with a documented single-axis demonstration and confirm whether the team expects more. Four servos in a package do not make a multi-axis mechanism.

An MPU6050 has no magnetometer. Accelerometer/gyro fusion supports tilt estimation, but heading drifts without an external heading reference. Call the output an estimate, not ground truth. The brief's "Ground Truth" wording does not remove this physical limitation.

Range plus orientation does not recover arbitrary movement of the sensor origin. For a first mapping demonstration, keep the base origin fixed and allow documented rotations; do not claim full SLAM or position tracking. Translation would need additional measurement or a justified estimator.

An ultrasonic return is not automatically from the colored object. Align the sensor and camera, isolate the target, and label association uncertain when another surface could generate the echo.

Record any team clarification in the worklog. These are design boundaries to explain, not reasons to buy parts preemptively.
