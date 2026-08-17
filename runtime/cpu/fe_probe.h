// Call counts for the frontend cFEMeter widget's creator, constructor and value
// accessors. See fe_probe.cpp for how the addresses were derived from the widget-class
// factory table and the vtable at 0x820BDBE8, and for what each outcome means.
#pragma once

// Prints the counts. Safe on a shutdown path: relaxed atomic loads and stderr only.
// Called from BOTH exits — main.cpp's signal handler and window.cpp's Shutdown — since
// both use _Exit and static destructors never run.
void FeProbe_Report();
