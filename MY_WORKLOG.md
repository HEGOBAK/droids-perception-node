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


Questions: 


Assistant comments (corrections):


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
