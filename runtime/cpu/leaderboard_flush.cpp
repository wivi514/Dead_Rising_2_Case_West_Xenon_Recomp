// The PP leaderboard's flush timer: the title writes a dirty stats cache at
// most once every 360 s, and a player who saves and opens the board sees the
// old number for up to six minutes (Case Zero's player issue #4, "leaderboard
// does not get properly synced" — their 8f23fb6, imported 2026-09-17 with EVERY
// address and offset re-derived on this image; docs/imported-fixes.md §14).
//
// WHAT THE TITLE DOES (read out of THIS image). The PP board is
// LEADERBOARD_PRESTIGE_POINTS, and here it is board INDEX 0: Case West's name
// table has three boards (PRESTIGE_POINTS, TEST_CONSTANT_DO_NOT_WRITE, TEST_SUM)
// where Case Zero's has four (a LEADERBOARD_GAME_1 in front), so every "board 1"
// in the sibling's module is "board 0" here — the Update below says so itself
// (`li r4, 0` where theirs has `li r4, 1`). The stats cache (`cStatsCache`,
// constructed by sub_82549838 — a 3-board loop at stride 0x68 and the ONLY other
// reader of the 360.0 constant) takes a stat through CacheStat (sub_825374C8,
// "caching a stat to leaderboard %s" at 0x8207AB70 — its one referrer). A board-0
// cache sets a DIRTY byte (+0x80 here; +0xE8 there, one board further along); the
// cache's per-frame update (sub_82537660, a unique 1.000 shape match) accumulates dt
// into the flush timer (+0x168 here; +0x1D8 there) and, once DIRTY and the timer
// exceeds the float at 0x820757BC (360.0, exactly two readers image-wide: the
// constructor, as the timer's initial value so the first flush is immediate, and
// the update, as the threshold), writes the cache through WriteBoard (sub_825375E8,
// "trying to write stats to %s") -> cMsGameSession::WriteStats (sub_8259FD90 —
// unique 1.000 match, and every struct offset the trace reads is identical) ->
// XSessionWriteStats (000B0025, kernel/imports.cpp) -> libxlive's durable queue ->
// the server. The six minutes are the title's own choice, made for a console where
// every write cost a Live round trip; XenonLive's queue takes them one by one for
// nothing, and the only writes the cadence gates are the ones a save or a load
// already made.
//
// ONE DIVERGENCE FROM THE SIBLING WORTH KNOWING: here the trial-experience byte
// (`enable_trial_experience`, 0x82A744F2 in debug_tunables_table.inc) gates
// CacheStat itself — a trial build caches nothing — where in Case Zero it gates
// WriteBoard. The trace prints it at the same place it did there.
//
// THE FIX: the threshold becomes CW_LEADERBOARD_FLUSH_S seconds (default 2;
// `=360` is the title's own value, the control). Stored at the constant's
// address before the constructor and the update read it — the flat map is
// writable and the constant has no other reader. The write is refused, loudly,
// if the word there is not 360.0: that is the one check that catches a moved
// image.
//
// CW_LEADERBOARD_TRACE=1 prints the chain; CW_LEADERBOARD_FORCE_WRITE=N marks
// the cache dirty on the N-th update call (a write of whatever the cache holds
// — the way to watch the chain without a save).
//
// GATE (gotcha 323): `nm cw_runtime` must show sub_82549838, sub_82537660,
// sub_825375E8, sub_8259FD90 and sub_825374C8 as T at their own addresses.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_82549838);   // cStatsCache constructor      (CZ sub_82579390)
extern "C" PPC_FUNC(__imp__sub_82537660);   // cStatsCache::Update(dt)      (CZ sub_82567798)
extern "C" PPC_FUNC(__imp__sub_825375E8);   // WriteBoard(board, stats)     (CZ sub_82567720)
extern "C" PPC_FUNC(__imp__sub_8259FD90);   // cMsGameSession::WriteStats   (CZ sub_825CDB50)
extern "C" PPC_FUNC(__imp__sub_825374C8);   // CacheStat(board, stats)      (CZ sub_825794A0)

namespace
{
constexpr uint32_t kFlushTimerConst = 0x820757BC;   // float, 360.0 as shipped (CZ 0x8207BFE0)
constexpr uint32_t kDirtyByte = 0x80;               // cStatsCache +0x80 (CZ +0xE8)
constexpr uint32_t kFlushTimer = 0x168;             // cStatsCache +0x168 (CZ +0x1D8)
constexpr uint32_t kTrialByte = 0x82A744F2;         // enable_trial_experience (CZ 0x82A57BFE)

bool TraceOn()
{
    static const bool on = getenv("CW_LEADERBOARD_TRACE") != nullptr;
    return on;
}

void ApplyFlushSeconds(uint8_t* base)
{
    static bool applied = false;
    if (applied)
        return;
    applied = true;
    float seconds = 2.0f;
    if (const char* e = getenv("CW_LEADERBOARD_FLUSH_S"))
    {
        const float v = strtof(e, nullptr);
        if (v > 0.0f)
            seconds = v;
        else
            fprintf(stderr, "[leaderboard] CW_LEADERBOARD_FLUSH_S=%s is not > 0 — ignored\n", e);
    }
    uint32_t bits = PPC_LOAD_U32(kFlushTimerConst);
    float was;
    memcpy(&was, &bits, sizeof(was));
    if (was != 360.0f)
    {
        // Not the constant this was written for: the image moved under us.
        fprintf(stderr, "[leaderboard] flush-timer constant reads %g, not 360 — left alone\n",
                was);
        return;
    }
    memcpy(&bits, &seconds, sizeof(bits));
    PPC_STORE_U32(kFlushTimerConst, bits);
    fprintf(stderr, "[leaderboard] PP board flush timer 360 s -> %g s "
                    "(CW_LEADERBOARD_FLUSH_S=360 restores the title's cadence)\n",
            seconds);
}
}

PPC_FUNC(sub_82549838)
{
    ApplyFlushSeconds(base);
    __imp__sub_82549838(ctx, base);
}

PPC_FUNC(sub_82537660)
{
    ApplyFlushSeconds(base);
    if (!TraceOn())
    {
        __imp__sub_82537660(ctx, base);
        return;
    }
    const uint32_t self = ctx.r3.u32;
    static auto last = std::chrono::steady_clock::now();
    static uint32_t calls = 0;
    ++calls;
    const auto now = std::chrono::steady_clock::now();
    static const long forceAt = getenv("CW_LEADERBOARD_FORCE_WRITE")
        ? strtol(getenv("CW_LEADERBOARD_FORCE_WRITE"), nullptr, 10) : 0;
    if (forceAt > 0 && calls == uint32_t(forceAt))
    {
        PPC_STORE_U8(self + kDirtyByte, 1);
        fprintf(stderr, "[lbtrace] FORCED dirty at update call %u\n", calls);
    }
    const bool dirtyBefore = PPC_LOAD_U8(self + kDirtyByte) != 0;
    uint32_t bits = PPC_LOAD_U32(self + kFlushTimer);
    float timer;
    memcpy(&timer, &bits, 4);
    __imp__sub_82537660(ctx, base);
    const bool dirtyAfter = PPC_LOAD_U8(self + kDirtyByte) != 0;
    if (now - last > std::chrono::seconds(30) || dirtyBefore != dirtyAfter)
    {
        last = now;
        fprintf(stderr, "[lbtrace] update: %u calls, timer %.1f s, dirty %d -> %d, dt %.4f\n",
                calls, timer, dirtyBefore, dirtyAfter, float(ctx.f1.f64));
    }
}

PPC_FUNC(sub_825375E8)
{
    const uint32_t board = ctx.r4.u32;
    __imp__sub_825375E8(ctx, base);
    if (TraceOn())
        fprintf(stderr, "[lbtrace] WriteBoard(%u) -> %u (trial byte %u)\n", board,
                ctx.r3.u32 & 0xFF, PPC_LOAD_U8(kTrialByte));
}

PPC_FUNC(sub_8259FD90)
{
    const uint32_t self = ctx.r3.u32;
    const uint32_t sessState = PPC_LOAD_U32(self + 0x1E8);
    const uint32_t liveState = PPC_LOAD_U32(self + 0x118);
    const uint32_t pending = PPC_LOAD_U32(self + 0x2E8);
    __imp__sub_8259FD90(ctx, base);
    if (TraceOn())
        fprintf(stderr, "[lbtrace] cMsGameSession::WriteStats: session state %u, live "
                        "state %u, %u pending -> %u\n",
                sessState, liveState, pending, ctx.r3.u32 & 0xFF);
}

PPC_FUNC(sub_825374C8)
{
    const uint32_t board = ctx.r4.u32;
    __imp__sub_825374C8(ctx, base);
    if (TraceOn())
        fprintf(stderr, "[lbtrace] CacheStat(board %u) (trial byte %u — a trial caches nothing)\n",
                board, PPC_LOAD_U8(kTrialByte));
}
