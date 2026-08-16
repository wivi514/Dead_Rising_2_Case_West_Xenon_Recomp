// Call counts for the seven guest functions that contain instructions XenonRecomp could
// not translate. See gap_probe.cpp for why this is a runtime counter and not a query
// against the Xenia coverage traces (short version: the traces have a 124 KB hole that
// swallows all seven, and a positive control is what proved it).
#pragma once

// Prints the per-function call counts. Safe to call from a signal handler path: it only
// reads relaxed atomics and writes to stderr. Called from main.cpp's SIGTERM/SIGINT
// handler, because that handler uses _Exit and static destructors never run.
void GapProbe_Report();
