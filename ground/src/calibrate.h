#pragma once

// Run the interactive touchscreen calibration wizard.
// Shows a 2-second hold-to-enter gate, then guides the user through tapping
// two crosshair targets. Computed values are saved to NVS and applied immediately.
// Returns when calibration is complete or the user releases the screen during the gate.
void calibrate_run();
