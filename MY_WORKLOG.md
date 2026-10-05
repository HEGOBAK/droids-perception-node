# Droids Worklog

---------------------------------------------------------------------------------

## M0 — First firmware and physical inventory

Your application code + reusable code from ESP-IDF
                        ↓
              Toolchain builds them
                        ↓
                 Firmware files
                        ↓
           Flash them onto your ESP32
                        ↓
             ESP32 runs your application
          
             
A driver is software that helps OS communicate with a particular piece of hardware. CH343 is a chip that provides a USB-to-serial connection. Its driver doesn’t compile code.


ESP32: C code reads a distance sensor
                 ↓
          Sends the measurement
                 ↓
Mac: Python code draws the measurement


CMake prepares the build instructions like which files to compile and how they connect.
Ninja carries out those instruction, it runs the compiler and linker in the correct order. It also avoids unnecessary work like when you change one source file, it usually rebuilds only the affected parts.


CMake prepares the plan
          ↓
Ninja runs the build steps
          ↓
Compiler + linker produce firmware


2026-10-03 — M0 step 3 verified: after connecting the ESP32, `ls /dev/cu.*` showed the new serial port `/dev/cu.usbmodem5B414838131`. [Screenshot evidence](figures/M0/serial-port-before-after-connection.png).

2026-10-03 — M0 step 5 verified: the ESP32-S3 runs Hello World and sends boot information and the restart countdown to the serial monitor. The photo shows ESP-IDF v6.1 and 8 MB flash. [Photo evidence](figures/M0/hello-world-running-on-esp32s3.png).


vTaskDelay expects the number of ticks no seconds thats why we need pdMS_TO_TICKS to make the tranform. 
A blocking RTOS delay pauses the current task, not the whole ESP32. While the main task waits, FreeRTOS can run other ready tasks. “Blocked” simply means that task is temporarily waiting and cannot continue yet.

"idf.py save-defconfig" lets me save my config - Like when I mod flash setting from 2MB to 8MB

2026-10-03 — M0 steps 6–7 record:

- ESP-IDF: `v6.1`; the exported configuration identifies the framework as `6.1.0`.
- Board: Freenove ESP32-S3 WROOM. Earlier `esptool flash-id` output identified ESP32-S3 revision v0.2, 8 MB flash and 8 MB embedded PSRAM.
- Current flash configuration: 8 MB, DIO mode, 80 MHz, verified in `firmware/sdkconfig`.
- Serial port: `/dev/cu.usbmodem5B414838131`.
- Flash and monitor command used in this workflow, from `firmware/`: `idf.py -p /dev/cu.usbmodem5B414838131 flash monitor`.
- The source prints `Hello from my Droids project!` and a repeating counter with `vTaskDelay(pdMS_TO_TICKS(1000))`. I confirmed that it works after reset. This hardware result is user-reported; the earlier photo above shows the original Hello World program.
- Saved configuration checked: `firmware/sdkconfig.defaults` contains `CONFIG_IDF_TARGET="esp32s3"` and `CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y`. This check inspected files without rebuilding or flashing.
- Remaining step 7 task: commit the intended source, CMake files and `sdkconfig.defaults`, excluding generated build files. Physical inventory completion is tracked separately in `docs/HARDWARE.md`.

Questions: 

1. Trace what happens from editing `hello_world_main.c` to seeing your new message on the board. What do building, flashing, resetting and monitoring each do?
-> Edit the file with c code. Build with idf.py which involves cmake and ninja (compile + link + transforming to ESP32 readable source) then flashing it into chip. Monitor the board's output in terminal. 


2. What happens to the main task during `vTaskDelay(pdMS_TO_TICKS(1000))`? Why do we convert milliseconds to ticks, and can other tasks run while it waits?
-> Block/Wait for 1 second because 1000 milliseconds = 1 second. We convert because vTaskDelay uses ticks. Yes, other tasks can run while it waits.

3. If you cloned this project onto another Mac, what settings would `sdkconfig.defaults` restore? Why keep that file in Git but leave generated build files out?
-> It will restore the configs or the environment. 


Assistant comments (corrections):

- **M0 notes:** Your build pipeline, driver explanation and blocking-delay explanation are correct. `pdMS_TO_TICKS` converts milliseconds to ticks. The C-to-Python sensor diagram describes the future workflow; M0 has not tested sensors yet.
- **Q1:** Mostly correct. Building produces machine-code firmware, not “ESP32 readable source.” Flashing writes it to persistent flash memory. Resetting restarts the board and runs the stored firmware; monitoring displays its serial output.
- **Q2:** Correct. The delay is approximately one second; tick timing and scheduling affect when the task resumes. Other ready tasks can run during the wait.
- **Q3:** It restores project configuration choices—here, ESP32-S3 and 8 MB flash—when generating a fresh configuration. It does not install ESP-IDF, Python or the toolchain. Commit this small configuration file so settings are reproducible; generated build files can be recreated from source.

---------------------------------------------------------------------------------

## M1 — C and FreeRTOS foundations

Think of perception as an eye receptor that receives an image/data, it processes it and produce an observation result that may be passed down.

Synthetic observations are just made-up results for practice. In M1, for example, a number such as target_x = 0.25 is supplied instead of detecting a real ball and this lets us learn task communication before dealing with a camera.

Control just means how to respond

Perception produces a new position every 100 ms.
Control runs every 20 ms, so it may use the same position several times before a newer one arrives.

Sequence recognize whether we received a new observation or are still using the same one.
acquired_us calculate how old the observation is.
target_x stores a position

scheduler is the part of FreeRTOS that selects which ready task runs on a CPU core.
States of task :
- Running = Executing instructions now
- Ready = Able to run, but waiting for CPU time
- Blocked = Waiting for something, such as a delay or data
- Suspended = Explicitly paused until explicitly resumes

A program runs on a CPU core. RAM only holds the program's instructions and data while it's running.
Core affinity = which core(s) a task is allowed to run on

A task is basically an analogy of a node (ROS), a function with its own stack, run by scheduler

Queue is like a mailbox, a FIF buffer in RAM that tasks copy msgs into and out of, and it also has built-in rules that make is safe

A message is a bundle of data passed between tasks. I can use struct in C to define the shape of the bundle.
Bundle can be seen as a one recorded sighting of the target or plainly sets of data. It may answers three questions: Which sighting? When? Where? 

Sender task ──xQueueSend()──► [ QUEUE: stores the message ] ──xQueueReceive()──► Receiver task
   (acts)                          (holds + coordinates)                              (acts)

2026-10-03 — M1 task-creation check: the serial monitor shows repeating perception, control and sensor task messages alongside the Droids counter (0–4), confirming all three task functions execute on the ESP32-S3. [Screenshot evidence](figures/M1/three-freertos-tasks-serial-output.png). This demonstrates task execution

2026-10-03 — M1 step 3 queue check: the serial monitor shows sequences 0–3, each read repeatedly with synthetic `target_x=0.250000`. This supports control reusing the latest observation between perception updates through the one-slot overwrite/peek queue. The configured periods are 100 ms for perception and 20 ms for control; this screenshot does not measure actual frequencies. [Screenshot evidence](figures/M1/latest-observation-queue-serial-output.png).
   
To prevent drifting: 
Periodic timer → notification → control wakes and works → waits for next notification
                                  
A task notification is a signal directed to one task. The queue still carries observations; the notification means “time to check the observation.” So each tick goes timer → callback → notify(handle) → control_task wakes → does its work → sleeps again.

the delay sets the gap between runs, while the timer sets when each run starts. That fixed start time is what "fixed-frequency control" in the brief means.

vTaskDelay(20 ms) : 
|work 3ms|----wait 20ms----|work 3ms|----wait 20ms----|
period = 23 ms, not 20
         
microsecond clock (esp_timer) :
tick        tick        tick        tick
|work 3ms|..|work 3ms|..|work 3ms|..
period = 20 ms, fixed

Add esp_timer to PRIV_REQUIRES in main/CMakeList.txt

a periodic timer schedules regular events, but actual task starts still have scheduling jitter. Measuring that is precisely what step 6 does later on. 

Duriung a pause, the gap between obs is 500 + 100 = 600ms
An obs is stale after 200ms so 600 - 200 = 400ms
Control wakes every 20ms so 400 / 20 = 20
Stale count will increase 20 every pause if everything works
I set pauses happen every 50obs which is 50 x 100ms = 5s and taken into account the pause time itself (500ms) so 5.5


2026-10-03 — M1 steps 4–5 stale-input check: with control woken every 20 ms by an `esp_timer` notification and perception paused 500 ms every 50 observations, the monitor shows 20 stale cycles per pause matching the predicted 400 ms / 20 ms = 20 while control kept running. [Screenshot evidence](figures/M1/stale-observation-detection-serial-output.png).

Printing not advised in the task when we want to record the timestamps for the task. We can create a new task (logger) and sets its priority lower than the critical path (the task we want to record its timestamps) then sends the timestamps to a queue which will then be received by the logger and printed

By using ctrl+T and ctrl+L durig monitor, I saved the output into a file and moved it to result/

ticks = 3 means three signals were handled in one control iteration: two extra notifications. Cycle numbers 10, 11, 14 mean two missing records, 12 and 13.

2026-10-03 — M1 step 6 timing analysis: `host/analyze_timing.py` reports minimum, median, nearest-rank p95 and maximum absolute deviations from 20 ms as `0.000 ms` for `results/log.hello_world.20261003211716.txt`, with 0 extra notifications in recorded cycles and 0 missing log samples between recorded cycles. The saved trace contains 439 samples spanning 8.76 seconds; these results describe this recording only. At my request, this trace is used instead of requiring a 60-second recording. [Screenshot evidence](figures/M1/control-timing-deviation-analysis.png).

Current task communication:

```text
Perception → one-slot observation queue (overwrite) → Control (peek)
20 ms timer → callback → notification → Control
Control → 64-slot timing queue → Logger → serial output
Sensor → independent placeholder task (500 ms delay)
```

Ownership: perception and control each own their local observation; the queue copies the struct, and peeking leaves the queued value available for reuse. Timing samples are also copied. Control never waits for logger queue space: it drops a sample and increments `timing_drops` if the queue is full. All target positions remain synthetic.

Questions:

1. Why do we use a one-slot overwrite/peek queue for observations, but a separate FIFO queue for timing samples? Who owns each copy of an observation?
-> Observations can keep updating and does not need past records. Perception and control each has their own copy of observation.

2. When perception pauses for an extra 500 ms, why can control continue running? Explain the notification, the 200 ms stale threshold, and why about 20 control cycles see stale input.
-> They are two seperate task and control doesn't wait for perception. The control wakes when notification hits and the notification is sent by a built-in esp timer. When the observation doesn't update since 200 ms aso, the stale is put on threshold. Perception pause 600 ms and control goes into stale threshold after 200ms so 600 - 200 = 400ms and control wakes every 20ms so 400/20=20 therefore 20 control cycles

3. How does Python turn `wake_us` into timing deviations and p95? What different problems do extra notifications and missing cycle numbers reveal, and what can our 8.76-second trace establish?
-> The difference between next wake_us is the interval and the interval - perfect period = deviation. Not sure about p95. Extra notifications means multiple timer signals accumulated before control handled them, meaning control work's time is more than the time interval. Missing cycle number reveals that some cycles are dropped because the timing queue is full.

Assistant comments (corrections):

- **M1 notes overall:** Your task states, message fields, queue/notification distinction and separate logger are understood correctly. `acquired_us` stores a timestamp; subtract it from the current time to get age. RAM holds runtime data and some code; ESP32 can also execute flash-backed code through cache. Queue operations coordinate message copies, not all shared data. Your jitter correction is important: a periodic timer does not guarantee exact task start times. The ROS-node comparison is only an analogy.
- **Q1:** Correct. The queue also owns a copy. Timing uses FIFO because we need successive samples to measure intervals; overwriting would lose that history.
- **Q2:** Correct reasoning. Stale means observation age exceeds 200 ms. About 20 stale cycles is an expectation; scheduling and threshold boundaries can change the exact count.
- **Q3:** Convert timestamp differences from microseconds to seconds, then use the absolute difference from 0.020 s. Nearest-rank p95 sorts deviations and selects rank `ceil(0.95 × N)`: at least 95% are at or below that value. Extra notifications indicate backlog, which can also result from scheduling delays, not only slow control work. Missing cycle numbers indicate missing log records, not necessarily unexecuted control cycles; a full queue is one possible cause. The 8.76-second trace supports only this recorded run, not long-term timing guarantees.

---------------------------------------------------------------------------------

## M2 — IMU measurement and orientation

M2 initial hardware inspection — the photo shows an IMU breakout marked `HW-123`. The ordered module was described as GY-521/MPU6050; [Photo evidence](figures/M2/hw123-imu-module-unsoldered-headers.png).

Bias: a consistent offset. If a stationary gyro reports +0.5°/s, subtracting its measured stationary average helps.
Noise: readings fluctuate. Averaging helps estimate the bias.
Drift: accumulated angle error. A remaining 0.5°/s error becomes 5° after 10 seconds.

Added `esp_driver_i2c` to `PRIV_REQUIRES` in `firmware/main/CMakeLists.txt`.
A bus is the shared SDA/SCL connection between the ESP32 and sensor.

Making connection: 
Describe bus: SDA41, SCL42.
Create bus, get handle.
Probe 0x68 for ACK.
Verify ID, then read.

AD0 Low and High = two different addresses. For this experiment, I connect it to GND so its address is 0x68.

2026-10-04 — M2 wiring: [ESP32–IMU breadboard connection photo](figures/M2/Connect_IMU_ESP32.png), saved during wiring troubleshooting; this photo is not a verified pin-by-pin wiring reference.

Set up communication:
Describe device: address 0x68, 7-bit address, 100 kHz.
Add device to bus, get device handle.
Read WHO_AM_I register 0x75.
Check returned ID is 0x68.

Set up cleanup using i2c_master_bus_rm_device (IMPORTANT)


i2c_master_bus_add_device(imu_bus, &imu_config, &imu_device);
- On this existing bus, register this device with these communication settings.

2026-10-04 — M2 step 2 complete: the saved serial monitor shows `I2C device responded at 0x68` and `WHO_AM_I = 0x68 (expected 0x68)` [Screenshot evidence](figures/M2/imu-step2-success.png).

register(WHO_AM_I) address = 0x75
wake address = 0x6B
gyro address = 0x1B
accel address = 0x1C

The MPU6050 stores measurements in 14 consecutive bytes starting at 0x3B

data address = 0x3B

Data received on the first try:
Raw sensor bytes: FB 40 02 B4 76 84 01 20 FE DD 00 91 00 19
Acceleration magnitude is about 1.85 g. If the board was stationary, that is unexpectedly far from 1 g.

Investigating the acceleration error:
- Added a 100 ms settling delay and took 20 stationary readings per pose. Upright Z stayed near +1.854 g, so the delay did not fix it.
- Upside down, Z averaged −0.218 g. Returning upright gave about +1.86 g again.
- The midpoint estimates a Z offset: (1.854 - 0.218) / 2 ≈ +0.818 g
- On its side, Y was about −0.99 g and Z about +0.805 g. This separate pose supports the offset estimate. The cause is still unknown.
- Added a provisional correction for this IMU: az_corrected = az - 0.818f. 

![IMU inverted during the offset test](figures/M2/Accel_Issue.png)

Correction check — 20 new readings per pose: corrected acceleration magnitude averaged 1.0431 g upright (1.0310–1.0537 g) and 0.9917 g sideways (0.9870–0.9968 g). The Z correction removes most of the error, but upright still reads about 4.3% high. 

For step4, I have 2 functions:
- imu_measure_gyro_bias: while the board is still, it takes 1000 readings, works out the average and spread of each measurement, prints a summary, and returns the gyro bias.
- imu_log_corrected_gyro: it takes 10 more readings, subtracts that bias, and prints them. They should be close to 0.

I use this data structure to store: enum { AX, AY, AZ, TEMP, GX, GY, GZ, CHANNEL_COUNT };
Every iteration, I store compile their raw data in stats{CHANNEL_COUNT}

2026-10-04 — M2 steps 3–4 complete: all range readbacks match (ACCEL_CONFIG = 0x00 after fixing the readback register). 1000 still samples over 9.99 s (dt 10.00 ms), temperature 35.1 °C. Gyro bias: gx −2.271, gy +1.142, gz +0.170 °/s (std ≈ 0.10 °/s each). Corrected check readings stay near 0 °/s. [Screenshot evidence](figures/M2/imu-step4-gyro-bias.png).

Sign convention (right-hand rule):
- Roll is positive when the +Y end (VCC end) lifts.
- Pitch is positive when the +X end (the rail side) dips.

My test poses for tilt:
1. Flat
2. VCC end up
3. VCC end down
4. Flat
5. Rail side down
6. Rail side up
7. Flat

2026-10-04 — M2 step 5 tilt test, poses held by hand (so expected angles are approximate). [Log](results/M2/log.hello_world.20261004202933.txt)

| Pose | Roll | Pitch |
|---|---|---|
| Flat (start / middle / end) | 2.1 / 2.0 / 2.0° | 3.9 / 4.1 / 4.0° |
| VCC end up | +91 to +96° | ≈ 0° |
| VCC end down | −88 to −90° | ≈ 0° |
| Rail side down | not meaningful | +81° |
| Rail side up | not meaningful | −87° |

Every angle moved in the expected direction, and flat returned within 0.1°. Gyro bias repeated within 0.01 °/s of the first run (gx −2.264, gy +1.152, gz +0.169 °/s).
Open: flat reads 2° / 4° instead of 0° (desk, mounting or sensor offset — not yet separated). Rail side down reads 81°, not 90°.

Z scale fix: X and Y read ≈ 1 g on their edges, but Z reads 3.6% too big.
- Scale from upright/inverted: (1.853845 + 0.218091) / 2 ≈ 1.036
- Now az = (az − 0.818) / 1.036. Flat magnitude should be ≈ 1.00 g instead of 1.04 g.

2026-10-04 — M2 step 6: complementary filter for roll.
gyro_only_deg -> Adds up the gyro alone -> drift
accel_deg -> gravity alone, worked out fresh each time -> noise, and being fooled by movement
filtered_deg -> the blend of the two -> the result you actually want

angle = alpha * (angle + gx * dt) + (1 − alpha) * accel_roll, alpha = tau / (tau + dt)
dt is measured every step (10.00 ms every time).

My test: still → tilt to ~40° and back → slide left/right → still. Same test for 3 tau values:

| tau | Still noise (filtered std) | Recovery after disturbance | Log |
|---|---|---|---|
| 0.1 s | 0.031° | ~0.3 s | [Log](results/M2/imu-step6-roll-filter-tau0.1.txt) |
| 0.5 s | 0.021° | ~1.2 s | [Log](results/M2/imu-step6-roll-filter-tau0.5.txt) |
| 2.0 s | 0.018° | > 4 s (not back by the end) | [Log](results/M2/imu-step6-roll-filter-tau2.0.txt) |

Accel alone is much noisier (std ≈ 0.17°).

What I learned:
- Small tau follows the accel more: noisier, but fixes errors fast.
- Big tau is smoother, but slow to fix errors.
- Gyro-only never comes back to start (+0.55°, −0.51°, +0.68° off at the end). Filtered always does.
- Shaking makes the gyro wrong: in pauses during the slide, gyro-only was off by up to 7° while accel said the board barely moved. Likely cause: the sensor's low-pass filter is off (gyro passes ~256 Hz, but I only read at 100 Hz).
- My slides weren't the same each run, so the slide numbers can't be compared fairly.

Sensor low-pass filter (step 7 start):
- What: the MPU6050's built-in filter (DLPF). It removes changes faster than ~20 Hz before I read the data.
- Why: I read at 100 Hz, but the gyro was passing ~256 Hz. Fast shaking between reads got added up as fake angle (up to 7° off in step 6).
- How: write CONFIG (0x1A) = 0x04 and read it back. Cost: ~8.5 ms delay.
- Gain: less false angle, quieter accel.
- Expected: CONFIG = 0x04 (expected 0x04).
- Observed: 

Tilt from accel (atan2f):
- atan2f(y, x) = angle from two sides, in radians → × RAD_TO_DEG.
- Better than atan(y/x): no divide-by-zero, and keeps the signs (can tell upright from upside down).
- roll = atan2f(ay, az); pitch = atan2f(-ax, sqrt(ay² + az²)).

Gyro vs accel:
- Accel = angle now (like GPS); worked out fresh from each reading.
- Gyro = turning speed (like a speedometer); angle = running total of gx × dt, so it's kept from one loop to the next.
- The running total also keeps every small error → drift.

Complementary filter, tau:
- tau = time constant, how fast accel pulls drift back (63% after 1 tau, ~95% after 3).
- alpha = tau / (tau + dt) = 0.5 / 0.51 ≈ 0.98 → trust gyro 98%, accel 2%.
- Bigger tau = smoother but slower to fix drift; smaller = faster fix but more shake.

2026-10-04 — M2 step 7: testing (tau 0.5 s, low-pass filter on).

Still run, full log from the bias summary: [Log](results/M2/imu-step7-still-full-summary.txt)
- Accel magnitude with Z offset + scale: 1.0108 g (was 1.039 g with offset only).
- Gyro noise (std): ~0.03 °/s per axis (was ~0.10 °/s before the low-pass filter).
- Bias: gx −2.278, gy +1.326, gz +0.197 °/s at 32.9 °C. gy moved +0.18 °/s from my 35.1 °C runs → bias changes, so I measure it every startup.
- 30 s still: gyro-only drifted +0.06° (≈ 0.12°/min). Filtered stayed 2.15–2.19°.

A. Slide with low-pass filter: [Log](results/M2/imu-step7-A-slide-lowpass.txt) · [Plot](figures/M2/m2-step7-slide-lowpass-before-after.svg)
- Accel noise when still: 0.191° → 0.043° (≈ 4× quieter). Filtered: 0.021° → 0.008°.
- Gyro-only error in slide pauses: up to 7° before → ~0.1–0.4° now.
- Filtered stayed 1.7–2.5° during the slides and ended at 2.10° (accel 2.11°).

C. Bias wrong on purpose (+1 °/s): [Log](results/M2/imu-step7-C-bias-error-1dps.txt) · [Plot](figures/M2/m2-step7-bias-error-test.svg)
- Gyro-only: +1.002 °/s (2.3° → 32.2° in 30 s).
- Filtered: settled 0.500° above accel = error × tau = 1 × 0.5. Exactly as predicted.

E. Unplug SDA: [Log](results/M2/imu-step7-E-unplug-retry.txt) · [Plot](figures/M2/m2-step7-unplug-recovery.svg)
- INVALID at 22.66 s, loop kept running (~110 ms per failed read), VALID again at 30.47 s after 71 failed reads.
- Angles restarted from accel (2.17° vs 2.10° before). Run ended on time.
- First try never came back: [Log](results/M2/imu-step7-E-unplug.txt). Most likely SDA wasn't making contact when I pushed it back.

Step 6 plot (3 tau values): [Plot](figures/M2/m2-step6-tau-comparison.svg). Plots are made by `python3 host/plot_m2.py`.

Questions: 
1. Why does averaging 1000 still gyro samples give a good bias estimate? Use your step 4 numbers (mean vs std). What did test C show happens to gyro-only and to the filtered angle when the bias is wrong by 1 °/s?
-> Avg larger sample size gives better result. gyro_only angle adds up the bias and gives a 30 degree error on the final result if we run for 30s while the filtered angle was not affect that much.

2. Why can't this sensor alone keep yaw (rotation about the vertical axis) correct forever? What does the accel see when you turn the flat board around Z, and what happens to a gyro-only angle over time?
-> not sure

3. Why must we use the measured dt and the configured sensitivity (131 counts per °/s, 16384 counts per g)? What would happen to the angle if dt were 10% wrong, or if the gyro range were changed to ±500 °/s but the code still divided by 131?
-> Has to be the time interval between data collected because it is using that dt to calculate the position now from prev. We chose that option from the start. If dt were 10% wrong, we would accumulate large error. Would result in error because frequency doesn't match.


Assistant comments (corrections):

- **M2 notes overall:** Your I2C steps, register addresses, offset/scale method and step 6–7 results are correct and match the logs. The `enum` doesn't store data; it names positions 0–6, and each loop adds readings to running totals in `stats[CHANNEL_COUNT]` rather than storing them. "Accel = angle now" holds only while the board isn't accelerating. Fill in "Observed" for the low-pass filter.
- **Q1:** Mostly correct. Averaging shrinks random noise by √N: 0.10 / √1000 ≈ 0.003 °/s, tiny next to the −2.27 °/s bias. In test C, gyro-only grows without limit (30° in 30 s), while filtered settles at a fixed 0.5° (error × tau) and never grows.
- **Q2:** Turning the flat board around Z doesn't change gravity's direction, so the accel can't see yaw. Only gz measures it, and its leftover bias keeps adding up (0.003 °/s ≈ 11°/hour). Yaw needs another reference, such as a compass or camera; the MPU6050 has neither.
- **Q3:** Right idea for dt: angle step = rate × dt, so a 10% dt error makes every turn 10% wrong (it adds nothing while still). The sensitivity issue is scale, not frequency: at ±500 °/s (65.5 counts per °/s), dividing by 131 makes a 90° turn read 45°. That's why the code reads the range registers back before using 131 and 16384.


---------------------------------------------------------------------------------

## M3 — Distance and servo independently


Questions: 


Assistant comments (corrections):


---------------------------------------------------------------------------------

## M4 — Camera and colored-object detection


Questions: 


Assistant comments (corrections):

---------------------------------------------------------------------------------

## M5 — PID tracking and single-axis stabilization

Questions:

Assistant comments (corrections):

---------------------------------------------------------------------------------

## M6 — Binary UART and robust parsing

Questions:

Assistant comments (corrections):

---------------------------------------------------------------------------------

## M7 — Geometry and a 3D viewer

Questions:

Assistant comments (corrections):

---------------------------------------------------------------------------------

## M8 — Integration, evaluation and mastery

Questions:

Assistant comments (corrections):
