#pragma once

// M4: camera on, perception task finds the red target every 100 ms, results through a 1-slot queue,
// main loop prints one row twice a second (dumps one frame for host/frame_to_png.py). Runs forever.
void m4_run(void);
