// The PC graphics options panel, Case West's half (imported from Case Zero part 60).
// See pc_options_cw.cpp.
#pragma once

#include <cstdint>

struct PPCContext;

// Called from the sub_82812410 hook in debug_tunables_cw.cpp BEFORE the real
// transition runs. Returns true when the transition must be SWALLOWED (the hook then
// returns 0 without running the real body): selecting Visuals in the title's own
// options hub opens the host-rendered settings panel instead of the guest screen.
bool PcOptions_FilterScreenTransition(PPCContext& ctx, uint8_t* base);

// Called from the XamInputGetState pump (a guest thread with a usable context, like
// every DebugTunables_Pump*). While the panel is up this IS its input handling —
// selection, value stepping and closing — because the panel is drawn by the host and
// the guest hub underneath is deliberately not told about any of these presses.
// `buttons` is the pad word the guest just received from this very poll.
void PcOptions_Pump(PPCContext& ctx, uint8_t* base, uint32_t buttons);
