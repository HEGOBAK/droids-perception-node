# Debug the smallest failing layer

| Symptom | First useful check |
|---|---|
| `idf.py` missing | Activate the installed ESP-IDF environment in this terminal; record its path |
| No serial port | Compare ports before/after connection; try a known data cable and correct connector |
| Upload fails | Close other serial programs, check selected target/port, use vendor boot procedure |
| Reset when servo moves | Disconnect servo power; check supply sag, common ground, mechanical load |
| IMU not found | Check wiring and pull-up voltage, address selection, bus timeout; test alone |
| Camera initialization fails | Confirm model pin map and PSRAM settings; inspect ribbon with power removed |
| Range always zero/timeout | Keep timeout distinct from zero, check ECHO logic level and target alignment |
| Servo jitters | Check pulse timing and supply first, then noisy measurements and PID gains |
| Target moves away from center | Check servo direction with a tiny manual command before PID tuning |
| Control pauses during frames | Inspect priority, blocking operations, lock duration and queue semantics |
| Binary parser fails often | Match baud, field sizes, endian and CRC coverage; separate console logs |
| Plot rotates opposite to hardware | Verify axes, radians, rotation order and servo sign with synthetic cases |
| Yaw drifts | Measure gyro bias; a six-axis IMU has no absolute heading observation |

Keep a failing log before changing anything. Change one variable, state the expected effect, and rerun the same check. Record exact versions and commands. "It works now" is less useful than the condition that changed.
