// The PC graphics options panel — Case West's guest-side half.
//
// WHY THIS FILE EXISTS, AND WHY IT IS NOT CASE ZERO'S
// ---------------------------------------------------
// Case Zero's `cpu/pc_options.cpp` is 959 lines, and on its DEFAULT path about 120 of
// them run. The other ~840 are the part-60 "native screen resurrection" experiment —
// redirecting the transition to the shipped-but-hollow OptionsPC guest screen,
// hooking its descriptor's default verb handler, and rotating its spin widgets — plus
// the trace scaffolding that diagnosed it. That arm carries essentially all of the
// file's guest exposure: five hooked functions, three data addresses, ~15 structure
// offsets, and a hardcoded `.text` bound that is WRONG for this title in the safe
// direction (gotcha 3, the sibling-constants trap — Case Zero's `.text` starts at
// 0x82000000, ours at 0x82150000, so a range check copied over would quietly pass
// everything).
//
// So this port takes the default path only, and the guest surface collapses to ONE
// hook we already own and ONE string in our own image. Everything else the panel
// needs is host-side and came across unmodified in `host/settings.cpp` and the
// `EmitSettingsOverlay` layout in `host/window.cpp`.
//
// HOW IT IS REACHED. The title's own Help & Options hub already has a "Visuals" row.
// Selecting it asks the frontend to transition to the screen named "OptionsVisual";
// we intercept that request, open the host panel, and SWALLOW the transition, so the
// hub stays alive underneath as the backdrop and gets its input back untouched the
// moment the panel closes. No new screen, no repacked asset, no menu of our own to
// navigate to.
//
// THE HASH IS COMPUTED, NOT TRANSCRIBED. Case Zero compares against two interned
// name-hash globals it located by hand (0x82A58D20/0x82A58D28). We do not need them:
// the title's own name-hash function is `sub_827815D0(name, len)` — already used by
// the DebugJump port next door (finding 57) and by fe_probe's CONTROL A — so we hash
// our own image's "OptionsVisual" string with the guest's own code and compare that.
// Same answer, derived rather than inherited, and it cannot rot if a rebuild moves
// the interned table.
//
// CONTROL ARM: CW_NO_PC_OPTIONS=1 removes the whole feature — Visuals opens whatever
// the shipped screen is, exactly as before this file existed. CW_TEST_PANEL=1 (read
// in settings.cpp) opens the panel at boot with no input needed, which is the
// headless repro for anything about what the rows DISPLAY.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pc_options_cw.h"

#include <ppc_config.h>   // ppc_context.h #errors without it
#include <ppc_context.h>
#include "../gpu/vd.h"
#include "../gpu/vk_renderer.h"
#include "../host/settings.h"
#include "../host/window.h"
#include "ppc_recomp_shared.h"

// The title's own name -> hash function. Declared here rather than included because
// `ppc_recomp_shared.h` only ever declares the WEAK alias; this is the real body, and
// calling it is how a hash we compute is the same hash the guest compares.
extern "C" PPC_FUNC(__imp__sub_827815D0);

namespace {

// "OptionsVisual", found in this image's own screen-name table at 0x8206D900 — four
// entries along from "DebugJump" (0x8206D8C0), which is the table the DebugJump port
// already drives. Length excludes the terminator, matching how the guest calls the
// hash function on its own literals.
constexpr uint32_t kOptionsVisualName = 0x8206D900;
constexpr uint32_t kOptionsVisualLen  = 13;

bool Disabled()
{
    static const bool off = getenv("CW_NO_PC_OPTIONS") != nullptr;
    return off;
}

// The pad word from the previous poll, for edge detection. Set to ~0u when the panel
// opens so that everything held AT THAT MOMENT — the A press that selected Visuals,
// most of all — counts as already-pressed. Without it the panel's first poll reads
// that same A as a fresh edge and instantly steps a row or closes itself.
uint32_t g_prevButtons = 0;

// The interned hash of "OptionsVisual", computed once with the guest's own function.
// Zero until the first transition gives us a context to call it on.
std::atomic<uint32_t> g_optionsVisualHash{ 0 };

uint32_t OptionsVisualHash(PPCContext& ctx, uint8_t* base)
{
    const uint32_t cached = g_optionsVisualHash.load(std::memory_order_acquire);
    if (cached)
        return cached;
    // Hash on a SAVED context. This runs inside another function's hook, so the
    // callee is free to clobber every volatile register; restoring them afterwards is
    // what keeps the real transition — which has not run yet — seeing its own
    // arguments.
    PPCContext saved = ctx;
    ctx.r3.u64 = kOptionsVisualName;
    ctx.r4.u64 = kOptionsVisualLen;
    __imp__sub_827815D0(ctx, base);
    const uint32_t h = ctx.r3.u32;
    ctx = saved;
    g_optionsVisualHash.store(h, std::memory_order_release);
    fprintf(stderr, "[pcopt] \"OptionsVisual\" hashes to %08X (computed with the "
                    "title's own sub_827815D0) — selecting Visuals now opens the "
                    "host settings panel; CW_NO_PC_OPTIONS=1 restores the shipped "
                    "screen\n", h);
    return h;
}

} // namespace

bool PcOptions_FilterScreenTransition(PPCContext& ctx, uint8_t* base)
{
    if (Disabled())
        return false;
    const uint32_t want = OptionsVisualHash(ctx, base);
    if (!want || ctx.r4.u32 != want)
        return false;

    Settings_SetOverlayVisible(true);
    g_prevButtons = ~0u;   // see the note on the declaration

    static uint64_t opens = 0;
    uint32_t rw = 0, rh = 0;
    Settings_InternalRes(rw, rh);
    // The values the panel is ABOUT TO SHOW, logged at every open. This is the
    // instrument for any "it forgot my setting" report: if this line says the store
    // is right while the panel shows something else, the defect is presentation-side;
    // if the store is already wrong, the writer is upstream of here.
    fprintf(stderr, "[pcopt] Visuals -> host settings panel (open %llu) — showing "
                    "res=%ux%u mode=%d vsync=%d tier=%d cap=%d fov=%+d\n",
            (unsigned long long)(++opens), rw, rh, int(Settings_DisplayMode()),
            Settings_VSync() ? 1 : 0, Settings_ShadowTier(), Settings_FpsCap(),
            Settings_Fov());
    return true;   // SWALLOW: the caller returns 0 and no guest screen change happens
}

void PcOptions_Pump(PPCContext& ctx, uint8_t* base, uint32_t buttons)
{
    (void)ctx;
    (void)base;
    if (Disabled() || !Settings_OverlayVisible())
        return;

    const uint32_t pressed = buttons & ~g_prevButtons;
    g_prevButtons = buttons;
    if (!pressed)
        return;
    // A COOLDOWN on top of edge detection. The title polls this import many times per
    // frame and from more than one thread, and pure edge detection against one shared
    // previous-state word misfires across those polls — Case Zero's operator measured
    // it as "one press jumps five values". 180 ms is the classic menu repeat gate;
    // holding a direction is deliberately NOT auto-repeat, one step per press.
    static std::chrono::steady_clock::time_point lastAction{};
    const auto now = std::chrono::steady_clock::now();
    if (now - lastAction < std::chrono::milliseconds(180))
        return;
    lastAction = now;

    constexpr uint32_t kUp = 0x0001, kDown = 0x0002, kLeft = 0x0004, kRight = 0x0008,
                       kB = 0x2000, kA = 0x1000;

    // One applier for both input styles (Left/Right, and the console-style A step):
    // Case Zero's first version was two hand-copied switch blocks, which is how a
    // fifth row lands in one and not the other. `dir` is +1 or -1.
    auto applyRow = [](int row, int dir) {
        switch (row)
        {
            case 0:
            {
                // THE DISPLAY'S OWN MODE LIST: every distinct size the monitor
                // reports that the renderer can produce — 1920x1080 and friends
                // included, which no integer multiple of 1280x720 could express.
                // Fallback when there is no display list (headless, or SDL said
                // nothing): the synthesized 16:9 ladder, so the row still steps.
                // Selecting persists the size; it applies at the NEXT LAUNCH,
                // because the live path froze Case Zero's operator's machine twice
                // and stays parked there.
                uint32_t modes[64];
                int count = Host_DisplayModeList(modes, 32);
                if (count == 0)
                    for (uint32_t sc = 1; sc <= 4; ++sc)
                    {
                        modes[count * 2] = 1280 * sc;
                        modes[count * 2 + 1] = 720 * sc;
                        ++count;
                    }
                uint32_t cw = 0, ch = 0;
                Settings_InternalRes(cw, ch);
                int at = 0;
                for (int i = 0; i < count; ++i)
                    if (modes[i * 2] == cw && modes[i * 2 + 1] == ch)
                        at = i;
                // CLAMPED at the ends, no wrap. Case Zero's "it always shows 720p
                // when I open it" was exactly this: their resolution was the LAST
                // list entry, so the first right-press wrapped to the smallest every
                // time, three sessions running. An ordered ladder clamps.
                at += dir;
                if (at < 0)
                    at = 0;
                if (at >= count)
                    at = count - 1;
                Settings_SetInternalRes(modes[at * 2], modes[at * 2 + 1]);
                fprintf(stderr, "[pcopt] resolution %ux%u — next launch\n",
                        modes[at * 2], modes[at * 2 + 1]);
                break;
            }
            case 1:
            {
                const int m = (int(Settings_DisplayMode()) + dir + 3) % 3;
                Settings_SetDisplayMode(CzDisplayMode(m));
                break;
            }
            case 2:
                Settings_SetVSync(!Settings_VSync());
                VkRenderer_RequestSwapchainRebuild();
                break;
            case 3:
            {
                // SHADOW QUALITY: LOW / MEDIUM / HIGH only.
                //
                // Case Zero's row grew three further rungs (RT LOW/MEDIUM/HIGH) for
                // its ray-traced cascade. Those are NOT ported — operator's
                // instruction, 2026-08-23, because that work does not yet produce a
                // correct picture there. Nothing about ray tracing is imported, so
                // there is no availability predicate to consult and no stored RT tier
                // to step past: the ladder is three wide and reads the raster tier
                // directly. Applied live — the renderer re-reads the tier once per
                // frame and the atlas rebuilds through the same path a guest resize
                // takes.
                const int t = (Settings_ShadowTier() + dir + 3) % 3;
                Settings_SetShadowTier(t);
                break;
            }
            case 4:
            {
                // The frame cap ladder. OFF is first so the default reads as
                // "nothing capped", matching the 500-ceiling default that never
                // binds. Clamped like the resolution ladder — wrapping an ordered
                // list is the same first-press surprise.
                static const int kCaps[] = { 0, 30, 60, 90, 120, 240, 480 };
                constexpr int kNumCaps = int(sizeof kCaps / sizeof *kCaps);
                int at = 0;
                for (int i = 0; i < kNumCaps; ++i)
                    if (kCaps[i] == Settings_FpsCap())
                        at = i;
                at += dir;
                if (at < 0)
                    at = 0;
                if (at >= kNumCaps)
                    at = kNumCaps - 1;
                const int cap = kCaps[at];
                Settings_SetFpsCap(cap);
                Vd_SetFpsCapLive(cap);   // applies within one pump tick
                break;
            }
            case 5:
            {
                // FIELD OF VIEW: degrees of adjustment, -10..+30, one per press,
                // clamped at the ends. In Case Zero this drives a GAME-SIDE
                // substitution (cpu/camera_fov.cpp) so the world renders and CULLS
                // wide; that module is 100% guest addresses — including a link-register
                // value identifying one call site — and is NOT ported yet. Here the
                // value is stored and applied by the renderer's own projection patch,
                // which is the arm Case Zero had before its part 61. Re-deriving the
                // game-side half needs this title's own camera census; the recipe is
                // in the sibling's camera_fov.cpp header.
                int fov = Settings_Fov() + dir;
                if (fov < -10)
                    fov = -10;
                if (fov > 30)
                    fov = 30;
                Settings_SetFov(fov);
                fprintf(stderr, "[pcopt] fov %+d — live\n", fov);
                break;
            }
        }
    };

    int sel = Settings_OverlaySelection();
    if (pressed & (kUp | kDown))
    {
        sel = (sel + ((pressed & kDown) ? 1 : 5)) % 6;
        Settings_SetOverlaySelection(sel);
    }
    else if (pressed & (kLeft | kRight))
        applyRow(sel, (pressed & kRight) ? 1 : -1);
    else if (pressed & kA)
        applyRow(sel, 1);   // A steps the selected value forward, console-style
    else if (pressed & kB)
    {
        // B closes. The hub underneath never saw any of this input; it resumes
        // untouched.
        Settings_SetOverlayVisible(false);
        fprintf(stderr, "[pcopt] settings panel closed\n");
    }
}
