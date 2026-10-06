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

Connect the power/GND onto the side rails on the breadboard instead of connecting into components directly.
HC_SR04 connected to IMU
TRIG : GPIO 21
ECHO : GPIO 1

Ticks (why the echo timeout is 40 ms, not 30 ms):
- FreeRTOS counts ticks (1 tick = 10 ms), not ms. Ticks beat from boot, whatever my code does.
- "Wait 3 ticks" = wake after 3 more beats. If I start mid-tick, the first one is shorter.

time (ms):  0    10    20    30    40
ticks:      |-----|-----|-----|-----|
                     ↑ call at 15 → wakes at 40 → waited 25 ms

- So 3 ticks = 20–30 ms. Echo can last 23 ms → might give up too early.
- 4 ticks (40 ms) = 30–40 ms → always catches it.
- Rule: N ticks lasts between N−1 and N ticks. Add 1 tick when I need a minimum.

HC_SR04 workflow:
- app_main sets up both the pins and the ECHO interrupt config (range_init).
- while(1) runs forever. Each loop = 1 ping, every 100 ms.
- One ping (range_measure):
  1. Check ECHO is LOW, empty the mailbox, set waiting_task = main task.
  2. TRIG = 1 for 10 µs, then 0 → sensor sends the sound.
  3. Main task sleeps (max 40 ms) until the ISR notifies it.
  --  ISR: ECHO goes HIGH → save rise_us. ECHO goes LOW → save fall_us, notify main task.
  5. Main task wakes where it stopped: echo_us = fall_us − rise_us. No notify in time → INVALID.
- Print the row, then xTaskDelayUntil sleeps until 100 ms after this ping started.

while(1) ──► ping ──► print ──► sleep until next 100 ms ──┐
  ▲                                                       │
  └───────────────────────────────────────────────────────┘

Sensor: front says HC-SR04, back says RCWL-9610A design, mode pads set to GPIO. [Front](figures/M3/hc-sr04p-front.png) · [Back](figures/M3/hc-sr04p-back-rcwl9610a.png) · [Wired](figures/M3/hc-sr04p-connected-esp32.png) · [Pin plan](figures/M3/esp32-s3-pin-plan.svg)

2026-10-05 — HC_SR04 test (only 15 cm possible on my desk):
- Container at 15 cm: echo 816–817 µs → 0.140 m every ping (101 pings, 10 s). About 1 cm short; spread ≈ 0.2 mm. [Log](results/M3/range-step1-15cm.txt) · [Photo](figures/M3/m3-step1-range-15cm.png)
- Sensor covered with a cloth: INVALID,ESP_ERR_TIMEOUT every ping, never 0 m. [Log](results/M3/range-step1-covered-invalid.txt) · [Photo](figures/M3/m3-step1-range-covered-invalid.png)
- Rows every 0.10 s → xTaskDelayUntil keeps the 100 ms period.


Servo workflow:
- app_main runs servo_init once: builds the 4 MCPWM parts and starts at 1500 µs (middle).
  timer (counts 0 → 19999 µs = 20 ms frame) → operator (links them) → comparator (the mark) → generator (GPIO 14: HIGH at 0, LOW at the mark).
- After that, the hardware sends 1 pulse every 20 ms by itself. No task, no delay.
- Move = move the mark (servo_set_pulse_us). Always clamped to 1300–1700 µs.
- servo_move_slowly: +/− 10 µs every 20 ms until the target → slow move, small current spikes.
- Test: 1500 → 1600 → 1500 → 1400 → 1500, hold 2 s each, print the row. Then app_main ends, but the servo keeps holding 1500.

servo_init ──► hardware pulse every 20 ms (forever, by itself)

for each target: move mark 10 µs ──► wait 20 ms ──► repeat until target ──► print ──► hold 2 s ──► next target

2026-10-05 — Servo test worked: horn moved 1500 → 1600 → 1500 → 1400 → 1500 µs slowly, held each 2 s, then stayed at the middle. No reset on USB power. [Photo](figures/M3/m3-servo-sg90-connected.png)

Step 4 calibration setup:
- Sheet with angle lines every 15° (L = counter-clockwise, R = clockwise), servo shaft on C. Angles are right at any zoom. [Sheet](figures/M3/m3-servo-calibration-sheet.pdf) · [Photo](figures/M3/m3-step4-calibration-setup.png)
- Horn turned half a turn so at 1500 µs it points along 0.
- Calibration run config:
  servo.c: MIN_US = 1000, MAX_US = 2000 (widened for this run only, set back after)
  app_main.c: HOLD_MS = 3000, TEST_US = 1500, 1300, 1100, 1000, 1500, 1700, 1900, 2000, 1500
- Read the angle at each pulse. Buzz/hum = end stop → unplug USB.

2026-10-05 — Step 4 calibration result (read by eye, ±3°):
1000 = R50, 1100 = R40, 1300 = R20, 1500 = 0, 1700 = L20, 1900 = L40, 2000 = L50, no buzz.
- Neutral = 1500 µs. Bigger pulse = L (counter-clockwise from above) → + angle = L.
- 10 µs per degree (1000 µs for 100°), same on both sides.
- Safe limits = ±50° minus 5° margin = ±45° → 1050–1950 µs.
- Now in servo.c (one place): NEUTRAL_US 1500, US_PER_DEG 10, DIRECTION +1, MIN_US 1050, MAX_US 1950.
- New: servo_set_angle_deg(deg) → pulse = 1500 + deg × 10. M5 can command angles, not µs.
- Check: commanded 0 → +30 → 0 → −30 → 0. Arm landed on L30, R30 and 0. Step 4 done.

2026-10-05 — Step 5: range + servo together. [Log](results/M3/range-servo-step5.txt) · [Photo](figures/M3/m3-step5-range-while-servo-sweeps.png)
- Every 100 ms: servo steps 2° (sweep ±30°), sensor pings a fixed box at ~15 cm. 20 s run.
- 182 pings, 0 INVALID, no reset, rows every 0.10 s.
- Distance 0.146–0.147 m the whole time the servo swept (3 full sweeps).
- Only the first 1.5 s read 0.133 m. Not the servo: the same angles later read 0.147. Most likely my hand near the sensor at the start.
- So servo current/noise on USB 5V did not disturb the range or reset the board (unloaded servo).


Questions: 

1. Why do we divide the echo time by two? Use your step 1 numbers: 816 µs echo → what distance, and why not 0.280 m?
-> The sound goes to the target and back. 343 x (816/100000)/2 = 0.14m not 0.28m.

2. Why does commanding 30° not prove the shaft physically reached 30°? Think about what goes from the ESP32 to the servo, what comes back, and what current_us really stores.
-> ESP32 sends the mark or the pulse width. current_us stores the latest commanded pulse width

3. Why can a servo destabilize the board even when the GPIO signal is correct? Think about where your servo gets its power from, what happens when it starts moving or is blocked, and what the IMU and range sensor share with it.
-> When the motor turns, it produces torque in the opposite direction and therefore, if connected to the board, will move the IMU and range sensor from their original position or the direction they face. Servo gets its power from the 5.5 power source from ESP32 and that the ESP32 is already supplying 3.3 to IMU and range sensor


Assistant comments (corrections):

- **M3 notes overall:** Pin plan, tick timing, HC_SR04 and servo workflows, calibration and step 5 are correct and match the logs. "HC_SR04 connected to IMU" means it shares the IMU's 3.3 V and GND rails, not the IMU itself. Range step 4 (ruler at 0.20 / 0.50 / 1.00 m) was skipped: only 15 cm fit on the desk, so range bias is known at one distance only (about −1 cm). The servo runs from USB 5V, unloaded; calibration was read by eye (±3°).
- **Q1:** Correct. One fix: µs → s is ÷ 1,000,000, not 100,000. 343 × 0.000816 / 2 = 0.140 m. Without ÷ 2 you get 0.280 m, the round trip.
- **Q2:** Half the answer. The other half: nothing comes back. There is no wire from the servo to the ESP32, so `current_us` is only what you asked for, not where the shaft is. If the shaft is blocked, the supply sags or it hits a stop, the code still says 30°. Only an outside reference (your calibration sheet) shows the real angle.
- **Q3:** Mostly mechanical, but the main reason is electrical. Starting or blocked, the servo pulls large current spikes (hundreds of mA) from the same USB 5V that feeds the ESP32's 3.3 V. The voltage dips → the board can reset or sensors read wrong, and motor noise on the shared ground can glitch ECHO or I2C. The 5V pin is USB 5 V (about 5.0 V, not 5.5). Step 5 showed no problem because the servo was unloaded and moved slowly. Reaction torque is real but small here.


---------------------------------------------------------------------------------

## M4 — Camera and colored-object detection

WE use RGB565 to store each pixel (2 bytes / 16 bits)

bit:  15 14 13 12 11 | 10  9  8  7  6  5 |  4  3  2  1  0
       R  R  R  R  R |  G  G  G  G  G  G |  B  B  B  B  B
       
return (uint8_t)((r * 255 / 31 + g * 255 / 63 + b * 255 / 31) / 3);
- To scale each of them fairly on 0-255 (green max number is 63 not 31 like others)

Text preview = picture cut into blocks:
- Camera: 160 × 120 pixels. Screen: 32 × 12 characters.
- So 32 × 12 = 384 blocks, each 5 wide × 10 tall = 50 pixels → 1 character (its average brightness).

 x:  0    5    10   15  ...                    155   160
 y 0 ┌────┬────┬────┬─ ... ─┬────┐
     │ c0 │ c1 │ c2 │       │c31 │  ← text row 0 (pixel rows 0–9)
  10 ├────┼────┼────┼─ ... ─┼────┤
     │ c0 │ c1 │ c2 │       │c31 │  ← text row 1 (pixel rows 10–19)
  20 ├────┼────┼────┼─ ... ─┼────┤
     ...                              ...
 110 ├────┼────┼────┼─ ... ─┼────┤
     │ c0 │ c1 │ c2 │       │c31 │  ← text row 11 (pixel rows 110–119)
 120 └────┴────┴────┴─ ... ─┴────┘
 
Finding the pixel byte : 
- pixel (3, 2) is pixel number 2 × 160 + 3 = 323, which starts at byte 646
- Imagine we have a pointer that starts at the top left corner then goes right all the way until the end
    then goes to the next row. If the pixel is y = 2, then it is on the third row since y = 0 and therefore
    there is 2 rows above it so 2 x 160 for everything above its row then + x is the number from left (0) on its own row
    
Average brightness → character: mean is 0–255. sizeof(RAMP) - 1 = 10 characters (the -1 skips the hidden end marker that every C string has). So mean × 10 ÷ 256 gives a slot from 0 to 9. Dark picks a space, bright picks @.

line[PREVIEW_COLS] = '\0' adds that end marker, so printf("%s") knows where the text stops. That's why line has room for 32 + 1 characters.

Simple camera setup [Photo](figures/M4/m4-camera-setup.png)

Camera workflow:
- app_main runs vision_init once: XCLK on, settings written over SCCB, DMA + buffer in PSRAM. Prints sensor (like WHO_AM_I) and free PSRAM.
- while(1), every 1 s, one vision_capture_preview:
  1. fb = esp_camera_fb_get() → borrow the frame (a pointer to the driver's buffer, not my copy).
  2. Print width × height, format and bytes (expect 38400).
  3. Text preview: 32 × 12 blocks of 5 × 10 pixels → average brightness → 1 character.
  4. esp_camera_fb_return(fb) → give it back. Don't use fb after this.
- Orientation test: black square on the iPad's left / top → dark on image left / top. If not → MIRROR / FLIP_VERTICAL = 1.

vision_init ──► while(1): borrow ──► print info ──► text preview ──► return ──► wait 1 s ──┐
                  ▲                                                                         │
                  └─────────────────────────────────────────────────────────────────────────┘

Orientation (iPad drawings): [Half](figures/M4/m4-step1-orientation-half.png) · [Plus](figures/M4/m4-step1-orientation-plus.png) · [Smile](figures/M4/m4-step1-orientation-smile.png)
- Half: dark on camera's left → dark on image left → left/right OK.
- Smile: preview upside down → image flipped vertically only.
- Plus: symmetric, no info, but the picture is sharp.

I decide to turn the board over (USB on top) because USB cable easier to route for later builds, and the camera hangs straight with gravity → tape holds better.
- Turning a camera over = rotate 180° = flip up/down AND left/right.
- Up/down cancels the old flip → up is OK. Left/right becomes mirrored -> FLIP_VERTICAL = 0, MIRROR = 1.

Check after the flip: smile upright, and the black strip on the iPad's left is dark on image left → orientation OK : [Photo](figures/M4/m4-step1-orientation-fixed.png)

I use HSV to dinstint color (in our case is red) instead of RGB :
Hue =           which colour (an angle on a colour wheel)
Saturation =    how strong the colour is (0 = grey, 1 = pure colour)
Value =         how bright

For hue, we need to know that red sits where the circle joins up: both just above 0° and just below 360°. A slightly orange red might be 10°, and a slightly pink red 350°. So the rule for red needs two pieces.

For red :
H ≤ 20° OR H ≥ 340° (both sides of the wrap-around) (Two pieces)
S ≥ 0.5             (strongly coloured, which rejects white, grey and skin)
V ≥ 0.25             (not almost black; very dark pixels have unreliable hue)

I then get HSV from RGB like this :
max = biggest of R, G, B
min = smallest of R, G, B

V = max / 255                          how bright the strongest channel is
S = (max − min) / max                  0 when all equal (grey), 1 when one channel is 0
H = depends on WHICH channel is max:
      R is max → 60 × (G − B) / (max − min)          (around 0°, can go negative → add 360)
      G is max → 60 × (B − R) / (max − min) + 120
      B is max → 60 × (R − G) / (max − min) + 240
      
Two examples:
- Bright red (230, 30, 30): max = 230 (R), min = 30.
        -> V = 0.90
        -> S = 200 / 230 = 0.87
        -> H = 60 × (30 − 30) / 200 = 0° ✓
- White (240, 240, 240): max = min, so S = 0, and the hue doesn't matter. Rejected

Step 2: format RGB565 (2 bytes/pixel). Stride = 160 × 2 = 320 bytes per row. 38400 = 120 × 320 → no padding, so (y × 160 + x) × 2 finds every pixel.

Step 3 code (vision.c):
- Bug I had: saved raw r/g/b (0–31, 0–63, 0–31) → green always looked strongest → wrong hue. Fix: scale to 0–255 BEFORE saving.
- Every pixel: RGB → HSV → is_red? → red_count++. Per block: more than half red (red_count × 2 > 50) → '#', else '.'.
- Brightness preview and red mask printed side by side + "red pixels: N of 19200".
- Centre pixel H/S/V printed only to check the red rule.

2026-10-06 — Step 3 test (red rule H ≤ 20° or ≥ 340°, S ≥ 0.5, V ≥ 0.25):
- Red circle on iPad: round '#' blob where the circle is, red pixels ≈ 4360 of 19200, centre H 5°, S 0.77 → red. [Photo](figures/M4/m4-step3-red-circle-mask.png)
- White only: mask all '.', red pixels 0 of 19200, centre S 0.03 → not red. [Photo](figures/M4/m4-step3-white-no-red.png)
- First frame centre H was 18° (near the 20° limit), then 5° → camera auto exposure / white balance settles after start-up.

Now we need to find the center of the red object, I do so by averaging all red pixel's positions.
cx = (sum of the x of every red pixel) ÷ (number of red pixels)
cy = (sum of the y of every red pixel) ÷ (number of red pixels)

Step 4 — centroid = the middle of the red pixels (one point for M5 to steer by):
- cx = sum of x of red pixels / N, cy = sum of y / N. N = 0 → INVALID (never "the middle").
- In my loop: where red_count++, also sum_x += x and sum_y += y (uint32_t, sums get big).
- 160 × 120 picture → middle is cx 80, cy 60. cx < 80 = target on image left.
- 'X' marks the centroid on the mask (pixel ÷ block size → text position). Lines are stored first and printed after, because the centroid is only known once all pixels are done.
- Two red objects → centroid lands in the empty space between them. So one target only.

2026-10-06 — Step 4 test:
- Circle near the middle: X right in the centre of the '#' blob, valid. [Photo](figures/M4/m4-step4-centroid-centre.png)
- Circle moved to camera's left: blob + X moved to image left (cx went down), valid. [Photo](figures/M4/m4-step4-centroid-left.png)
- Circle erased: mask all '.', INVALID (no red pixels). [Photo](figures/M4/m4-step4-centroid-invalid.png)
- Circle partly off the edge → centroid only averages the visible part → sits a bit inward of the real centre.

A frame may have noise, multiple red targets and we must filter them.
We only accept one target and how we know if there is really one target is by going through these checklist :
    1. Any red at all? -> 0 red pixels -> no red pixels
    2. Big enough? -> fewer than 200 red pixels -> too small (noise)
    3. A blob exists? -> red pixels scattered, but no group of 2+ # blocks -> red scattered, no blob
    4. Only ONE blob? -> 2 or more separate blobs -> ambiguous: more than one blob
    5. Confident? -> the main blob holds under 80% of the red -> low confidence
    
I configure it as : 
min red pixels = 200;   
min connect blocks to become a blob = 2;          
min confidence = 0.8f;   

I use the method (flood-fill) like the paint-bucket tool in a drawing app :
Go through the mask block by block.
When you find a # you haven't seen yet, that's a new blob. Pour paint on it: mark it seen, then mark every touching #, then every # touching those, and so on, until no more connect.
Count how many blocks got painted: that's the blob's size.
Keep scanning. Painted blocks are skipped, so each blob is counted once.

Used head and tail as a to-do list (queue) for the flood fill: tail = where the next touching '#' is added, head = the next block to paint. When head catches up with tail, nothing is left → the blob is finished.

Used "if (nr < 0 || nr >= PREVIEW_ROWS || nc < 0 || nc >= PREVIEW_COLS) { continue; }" to check if the block is off the grid/picture so we don't access memory outside of mask

Until step 5, one loop did everything: capture, detect, print, wait. The project's design (and M5) needs detection and control separated:

Perception is slow and irregular: a frame plus the maths takes tens of milliseconds.
Control (M5) must run on time, every 20 ms, and must never wait for the camera.
So the camera work moves into its own task, and it hands results over through a queue.

workflow:
- app_main: vision_init once, create a 1-slot queue, start the perception task.
- Perception task, every 100 ms:
  1. vision_process: borrow frame → find red target → fill result {frame, time, valid, cx, cy, confidence, reason} → return frame.
  2. Every 10th frame: print the text previews.
  3. Once, after 3 s, on the first valid frame: dump the frame (see below).
  4. xQueueOverwrite: copy the result into the 1 slot (latest only, never the frame pointer).
- Main task, every 500 ms: xQueuePeek → copy the latest result → print one row (age_ms = how old, process_ms, free_heap).

perception: borrow ──► detect ──► result ──► return frame ──► overwrite queue ──► wait 100 ms ──┐
               ▲                                                                                │
               └────────────────────────────────────────────────────────────────────────────────┘
main:       peek queue ──► print row ──► wait 500 ms ──┐
               ▲                                       │
               └───────────────────────────────────────┘

Frame dump (board has no screen, serial only carries text):
- 1 byte → 2 hex characters (top 4 bits, bottom 4 bits). 1 pixel = 2 bytes = 4 characters.
- 1 line per pixel row: 640 characters (160 pixels) + space + 40 characters of mask (4 red bits per character, board's own is_red).
- FRAME_BEGIN (width, height, valid, cx, cy) … 120 rows … FRAME_END.
- Mac: python3 host/frame_to_png.py <log> → PNG with picture | mask | outline + green cross at the centroid.

2026-10-06 — Steps 5–7 test (one iPad session, 2 runs): [Log](results/M4/m4-steps5-7-log.txt) · [Frame](figures/M4/m4-step6-frame.png)
- Step 5, one circle: valid, blobs 1, confidence 1.00.
- Step 5, two circles: 30 rows "ambiguous: more than one blob", blobs 2, confidence 0.56–0.71.
- Step 5, real noise: circle erased → red pen icon (12 pixels) → "too small (noise)", not a target.
- Step 6, frame dump → PNG: picture, mask and green cross line up. Pen icon = a few lone mask dots, ignored.
- Step 6, queue: frame keeps rising, age_ms normally 55–205 ms.
- Step 7, direction: cx 85 → 26 (camera's left) → 79 → 131 (camera's right). Erased → INVALID.
- Step 7, memory: free_heap 8681715 bytes the whole time (~3.5 min over 2 runs) → no leak.
- Step 7, timing: processing 48.3–48.9 ms per frame. Real rate 5.9 frames/s (170 ms = 100 ms wait + ~49 ms work + printing).
- During the frame dump (~7 s) age_ms reached 7025 ms: perception busy printing, main reads the same old result → stale. Dump = debugging only, off in M5.
- Script fix: the main task's rows landed between the frame rows → frame_to_png.py now only accepts lines shaped like frame rows.

M4 tests complete (lighting / background not tested). Questions below still to answer.


Questions: 

1. What does image shape (120, 160, 3) mean? How many bytes is one of your frames, and why?
-> 120 rows, 160 column picture frame and 3 data per pixel. Each pixel has 2 bytes so 120 x 160 x 2. 

2. Why does a JPEG's size not equal width × height × channels? Why did we use RGB565 instead of JPEG for finding red?
-> Not sure

3. Why can averaging all matching pixels put the centroid between two separate objects? What did your step 5 code do about it?
-> Average only the position of every matching pixels gives the center in reference of the entire frame on the object itself. Check for one valid object before finding the centroid. 


Assistant comments (corrections):

- **M4 notes overall:** RGB565 unpacking, pixel address, text preview blocks, HSV and the red rule, flood fill, head/tail and the task/queue split are correct and match the code and logs. The red pen icon in the frame is a good real noise case: too few pixels to fill a block, so it never became a blob. Lighting and similar-background tests were not done, so the red thresholds are only proven under your lamp and on the iPad screen.
- **Q1:** Mostly correct. The 3 is channels (R, G, B values per pixel), not bytes. Your format packs those 3 values into 2 bytes (5 + 6 + 5 bits), so one frame = 120 × 160 × 2 = 38 400 bytes. The same picture in RGB888 (1 byte per channel) would be 120 × 160 × 3 = 57 600.
- **Q2:** JPEG is compressed: it drops detail the eye won't notice and stores repeating areas briefly, so its size depends on the picture (a plain white screen is small, a busy scene big). It is not a grid of pixels, so you can't read pixel (x, y) without decoding the whole image first, which costs time and memory. RGB565 gives every pixel at a known place, (y × 160 + x) × 2, always 38 400 bytes, so each pixel can be tested for red straight away.
- **Q3:** Half right. The centroid is the mean of all red pixel positions. With two separate red areas, the positions from both sides average to a point between them, where there is no red at all (e.g. blobs at cx 30 and 130 → 80, the empty middle). Your step 5 code counts blobs with flood fill and marks the frame INVALID ("ambiguous") when there are 2 or more, so a centroid is only given for exactly one blob. Your two-circle test showed this in 30 rows.


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
