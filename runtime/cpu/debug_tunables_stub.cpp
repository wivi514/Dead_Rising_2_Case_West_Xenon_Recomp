// Case West stand-in for cpu/debug_tunables.cpp, which is Case Zero's debug-menu /
// DebugJump / AutoChuck toolchain and is parked in runtime/port-pending/ because its 29
// hooked guest addresses are that title's, not this one's. Read that directory's
// README before touching this file.
//
// WHY A STUB AND NOT JUST DELETING THE CALLS
// ------------------------------------------
// `kernel/imports.cpp` calls into this API from the pad and screen paths. Deleting the
// call sites would silently change how input is reported; leaving them pointing at
// honest no-ops keeps the seam intact and visible, so restoring the real module later is
// a file swap rather than an archaeology exercise.
//
// Every entry point here does nothing and says so ONCE, because the alternative — a
// silent no-op — is exactly the failure gotcha 5 is about. A run whose navigation
// instruments are absent should say so in its log rather than looking like a run whose
// instruments were present and found nothing.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <ppc_config.h>   // ppc_context.h #errors without it
#include <ppc_context.h>

namespace
{
void NoteOnce(const char* what)
{
    static std::atomic<uint32_t> seen{ 0 };
    // One line per distinct caller, capped, so a pump called every frame cannot flood.
    if (seen.fetch_add(1) < 8)
        fprintf(stderr,
                "[tunables] %s: Case Zero's debug-menu toolchain is NOT ported yet "
                "(runtime/port-pending/) — this call does nothing\n",
                what);
}
} // namespace

// The pumps: called per frame from the pad path. Silent by design after the first note.
void DebugTunables_PumpAutoChuck(PPCContext&, uint8_t*) {}
void DebugTunables_PumpDebugMenu(PPCContext&, uint8_t*) {}
void DebugTunables_PumpPendingScreen(PPCContext&, uint8_t*) {}

// The requests: operator-triggered, so each one that fires is worth a line.
void DebugTunables_RequestDebugJump(PPCContext&, uint8_t*) { NoteOnce("RequestDebugJump"); }
void DebugTunables_RequestDebugEnter(PPCContext&, uint8_t*) { NoteOnce("RequestDebugEnter"); }
void DebugTunables_ToggleFullDebugMenu(PPCContext&, uint8_t*) { NoteOnce("ToggleFullDebugMenu"); }

// Queries. Both answers are the "nothing is driving the menu" answer, which is true.
uint32_t DebugTunables_ScreenRequestsServiced() { return 0; }
bool DebugTunables_WantAutoBack() { return false; }

// Two more the renderer's live-position dump calls into. Both report "no answer"
// rather than a fabricated one: `CW_DebugPlayerPos` returning 0 means "no position
// available", which is what `vk_renderer.cpp` already checks for, and writing 0 bytes
// of player object is honest rather than writing zeros that would read as a real dump.
extern "C" int CW_DebugPlayerPos(float out[3], long long* ageMs)
{
    if (out)
        out[0] = out[1] = out[2] = 0.0f;
    if (ageMs)
        *ageMs = 0;
    return 0; // 0 = no position; the caller skips the annotation entirely
}

extern "C" uint32_t CW_DebugWritePlayerObject(FILE*, uint32_t)
{
    return 0; // 0 bytes written
}
