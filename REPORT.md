# Integrated Perception Node — development report

Status: template; no completed implementation or results asserted.

## Goal and scope

Describe the target, controlled axis, base-motion constraints, and intended demonstration. Explain differences from full SLAM.

## Hardware and reproducibility

Record exact board/revision, camera, sensors, power arrangement, final pin map, firmware commit, ESP-IDF/component versions, host dependencies, commands and calibration procedure. Include a wiring diagram/photo without personal information.

## Architecture and design choices

Show task ownership, priorities, rates, queues, notifications, timeout behavior and memory management. Explain why each choice was made and what alternatives you tried.

## Measurement, estimation and control

Explain sensor calibration, units, coordinate frames, image centroid, PID equation, servo limits, stabilization axis and yaw drift. Distinguish servo command from measured physical angle.

## Communication and visualization

Give the packet specification and corruption recovery behavior. Derive the mapping transform and explain range-to-target association and timestamp alignment.

## Experiments and results

For each trial: hypothesis, conditions, actual procedure, raw data path, commit, metric definition, result, plot and interpretation. Report failures and repeats, not only the best trial.

| Experiment | Conditions / repeats | Metric | Measured result | Evidence |
|---|---|---|---|---|
| Sensor calibration | TBD | TBD | Not measured | |
| Tracking | TBD | TBD | Not measured | |
| Stabilization on/off | TBD | TBD | Not measured | |
| Timing under camera load | TBD | TBD | Not measured | |
| Protocol corruption | TBD | TBD | Not tested | |
| Mapping checks | TBD | TBD | Not measured | |

## Challenges and learning

Describe a failure you investigated, the evidence that isolated it, and your revised understanding. Identify AI-assisted work and explain how you checked it.

## Limitations and next steps

Discuss heading drift, fixed-origin assumption, servo accuracy/backlash, ultrasonic beam ambiguity, synchronization, lighting, and limited trials. State what the evidence does and does not support.
