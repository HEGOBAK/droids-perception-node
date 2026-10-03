# Start here

[README](README.md) · [Begin M0](Learning/M0.md)

Your aim is to finish with something you can run, check, and explain without borrowing someone else's words. This guide is comprehensive within the onboarding scope; mastery comes from implementing, measuring, debugging, and explaining each milestone.

## First session

1. Read [hardware audit](docs/HARDWARE.md). Tick only items you have physically checked.
2. Read [M0](Learning/M0.md). Connect only the camera board by USB for the first upload; leave external sensors and servos disconnected.
3. Record your board markings, cable, toolchain version, and first output in [MY_WORKLOG.md](MY_WORKLOG.md).
4. Continue when the milestone checks pass. If one fails, record the smallest failing example and ask for a hint or debugging help.

## How to use each milestone

Read the assignment, attempt one small piece, run its checks, then explain it in your own words. The targets in this guide are proposed learning checks, not official grading thresholds. If a target proves unrealistic, report the measurement and justify a revised target rather than silently changing it.

Separate four things in your notes: what you expected, what you observed, what you inferred, and what you will test next. "Not sure" is a useful starting point.

## What to know before you begin

You do not need to master C first. Learn these as they appear: fixed-width integers, pointers, arrays, structs, function calls, header/source separation, bit masks, and object lifetime. Python experience helps with logic; C requires you to manage sizes and ownership explicitly.

A microcontroller repeatedly responds to physical inputs under timing and memory constraints. ESP-IDF supplies drivers and build tools. FreeRTOS schedules tasks. A peripheral is hardware that performs a specialized job such as counting pulses or generating PWM.

| Pendulum experience | Transfer to this project |
|---|---|
| State, input, and simulation timestep | Measured state, servo command, measured sample interval |
| Fit physical parameters | Estimate gyro bias, camera geometry, and servo direction |
| Evaluate on one shared reference | Repeat tracking trials under matched conditions |
| Tensor shapes and units | Image dimensions, packet byte offsets, radians and meters |
| Gradient-based policy training | Different approach here: explicit PID feedback |

## Working folders

`firmware/` is reserved for your ESP-IDF app; `host/` for Python decoding and plotting; `tests/` for automated checks; `results/` for small measured datasets; `figures/` for labeled plots. They are placeholders, not runnable applications.

Do not store credentials, order IDs, addresses, or raw private account screenshots. Record only project-relevant part models and evidence. Keep large recordings local unless deliberately selected for the repository.

Use [architecture](docs/ARCHITECTURE.md), [protocol](docs/PROTOCOL.md), and [troubleshooting](docs/TROUBLESHOOTING.md) as references when their milestone needs them.
