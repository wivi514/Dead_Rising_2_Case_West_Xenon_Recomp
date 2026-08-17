// Case West's own debug tunables and DebugJump screen, switched back on.
//
// WHY THIS EXISTS
// ---------------
// Reaching a place in this game costs either an operator playing to it or a
// CW_FAKE_PRESS_SEQ recipe manufacturing its way there (which can never be a gate —
// gotcha 78). Case Zero solved this by re-enabling the title's OWN development
// scaffolding, which retail ships switched off rather than compiled out; the operator
// asked for the same here ("implement debug jump from case zero... it'll be easier
// that way", 2026-08-16). This is that port — re-derived, not copied: every guest
// address in this file was extracted from Case West's own image, because the parked
// Case Zero module's 29 hook addresses are that title's (see port-pending/README.md,
// and the one address that would have linked silently to an unrelated function).
//
// HOW EACH ADDRESS WAS DERIVED (so it can be re-derived rather than trusted)
// --------------------------------------------------------------------------
// * The tunables loader is sub_824A4C90: found by the addi that materialises the
//   string "enable_debug_jump_menu" (0x8206B444), which lives inside it. It reads
//   ~400 named bools through get-bool-by-name sub_82786708 and stores each into a
//   fixed byte around 0x82A744xx. In retail every lookup misses and every byte is 0.
// * The (name -> byte) table is machine-extracted by tools/find_debug_tunables.py,
//   which models the loader's PIPELINED store (name N's result is stored while name
//   N+1 loads — the exact off-by-one that burned Case Zero's first probe and this
//   port's finding 43) and confirms every byte by counting its lbz readers
//   image-wide. 401 confirmed entries; zero-reader entries are excluded.
// * The frontend screen-name hash is sub_827815D0(name, len) — Case Zero's
//   sub_8276E398 fingerprint-matched (40-opcode masked signature, unique hit), and
//   independently already known to this port as the intern function fe_probe calls
//   CONTROL A.
// * The screen-transition request is sub_82812410(manager, hash, 0) — Case Zero's
//   sub_827F6D40 fingerprint-matched (unique hit). The manager is CAPTURED from the
//   title's own transitions by the hook below, never guessed.
// * "DebugJump" (0x8206D8C0, len 9) and "DebugEnter" (0x8206E284, len 10) are the
//   image's own strings; debugjump.txt ships in data/frontend/mainmenu.big at 4,144
//   bytes — the same size as Case Zero's.
//
// WHAT IS DELIBERATELY NOT PORTED YET
// -----------------------------------
// Case Zero's custom F4 overlay menu, AutoChuck, zombie spawning and PP awards are a
// much larger per-title surface. The pumps for them exist here as silent no-ops so
// kernel/imports.cpp keeps its seam; port them when a task needs them.
//
// GATE (gotcha 323): after ANY build, `nm cw_runtime` must show sub_824A4C90 and
// sub_82812410 as T at their own addresses — a W at __imp__'s address means the hook
// silently failed to land and every zero this file's flags produce is void.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <ppc_config.h>   // ppc_context.h #errors without it
#include <ppc_context.h>

extern "C" PPC_FUNC(__imp__sub_824A4C90);   // the tunables loader
extern "C" PPC_FUNC(__imp__sub_82812410);   // request screen transition (mgr, hash, 0)
extern "C" PPC_FUNC(__imp__sub_827815D0);   // name -> hash (fe_probe's CONTROL A)

namespace
{

struct Tunable
{
    const char* name;
    uint32_t address;
    int readers;
};

const Tunable kTunables[] = {
#include "debug_tunables_table.inc"
};

const Tunable* Find(const std::string& name)
{
    for (const Tunable& t : kTunables)
        if (name == t.name)
            return &t;
    return nullptr;
}

// The preset CW_DEBUG_MENU=1 applies. Same three as Case Zero's menu preset — the
// names all exist in this title's table (verified by the extractor's output).
const char* const kMenuPreset[] = {
    "enable_dev_only_debug_tiwwchnt",
    "enable_debug_jump_menu",
    "display_fe_screen_info",
};

// test_mode is the internal latch Case Zero found gating the frontend's literal
// DebugJump transition. There it was a u32 outside the byte table; here the loader
// stores it with the same stb stream as everything else (the extractor caught it at
// 0x82A7433E), but with ZERO lbz readers — its consumers likely read wider. Stored as
// a byte to match the observed store; if the jump screen never opens, this is the
// first constant to re-examine.
constexpr uint32_t kTestMode = 0x82A7433E;

uint32_t g_frontendTransitionManager = 0;
std::atomic<uint32_t> g_screenRequestsServiced{ 0 };

struct PendingScreen
{
    uint32_t nameAddress = 0;
    uint32_t nameLength = 0;
    const char* name = nullptr;
};
PendingScreen g_pendingScreen;

const auto g_debugEpoch = std::chrono::steady_clock::now();
long long DebugElapsedSeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - g_debugEpoch)
        .count();
}

void Apply(uint8_t* base, const Tunable& t, uint8_t value)
{
    uint8_t* p = base + t.address;
    const uint8_t before = *p;
    *p = value;
    fprintf(stderr, "[debug] %-36s @%08X  %u -> %u   (%d reader%s)\n", t.name,
            t.address, before, value, t.readers, t.readers == 1 ? "" : "s");
}

void ApplyFromEnvironment(uint8_t* base)
{
    bool any = false;
    if (const char* menu = getenv("CW_DEBUG_MENU"))
    {
        if (menu[0] && strcmp(menu, "0") != 0)
        {
            fprintf(stderr, "[debug] CW_DEBUG_MENU=%s — enabling the title's own "
                            "debug menu and DebugJump screen\n", menu);
            for (const char* name : kMenuPreset)
                if (const Tunable* t = Find(name))
                    Apply(base, *t, 1);
            const uint8_t before = *(base + kTestMode);
            *(base + kTestMode) = 1;
            fprintf(stderr, "[debug] %-36s @%08X  %u -> 1   (byte; see header note)\n",
                    "test_mode", kTestMode, before);
            any = true;
        }
    }
    // CW_DEBUG_TUNABLES=name[=0|1],name,...  — anything in the extracted table.
    if (const char* list = getenv("CW_DEBUG_TUNABLES"))
    {
        std::string spec(list);
        size_t pos = 0;
        while (pos < spec.size())
        {
            const size_t comma = spec.find(',', pos);
            std::string item = spec.substr(
                pos, comma == std::string::npos ? std::string::npos : comma - pos);
            pos = comma == std::string::npos ? spec.size() : comma + 1;
            if (item.empty())
                continue;
            uint8_t value = 1;
            const size_t eq = item.find('=');
            if (eq != std::string::npos)
            {
                value = uint8_t(atoi(item.c_str() + eq + 1));
                item.resize(eq);
            }
            if (const Tunable* t = Find(item))
            {
                Apply(base, *t, value);
                any = true;
            }
            else
            {
                // A typo that silently did nothing would be indistinguishable from a
                // tunable that does nothing (gotcha 5) — fail loudly, list the names.
                fprintf(stderr,
                        "[debug] CW_DEBUG_TUNABLES: unknown name '%s' — %zu known "
                        "names are in runtime/cpu/debug_tunables_table.inc\n",
                        item.c_str(), std::size(kTunables));
            }
        }
    }
    if (any)
        fprintf(stderr, "[debug] tunables applied at the entry-point config load; "
                        "nothing rewrites them after this point\n");
}

void RequestFrontendScreen(PPCContext& ctx, uint8_t* base, uint32_t nameAddress,
                           uint32_t nameLength, const char* name)
{
    if (!getenv("CW_DEBUG_MENU"))
    {
        fprintf(stderr, "[debug] %s: ignored, CW_DEBUG_MENU is not set\n", name);
        return;
    }
    if (!g_frontendTransitionManager)
    {
        g_pendingScreen = { nameAddress, nameLength, name };
        fprintf(stderr, "[debug] %s: no frontend transition manager yet at %llds — "
                        "request HELD until the first screen transition captures one\n",
                name, DebugElapsedSeconds());
        return;
    }
    ctx.r3.u64 = nameAddress;
    ctx.r4.u64 = nameLength;
    __imp__sub_827815D0(ctx, base);
    const uint32_t screenHash = ctx.r3.u32;
    ctx.r3.u64 = g_frontendTransitionManager;
    ctx.r4.u64 = screenHash;
    ctx.r5.u64 = 0;
    __imp__sub_82812410(ctx, base);
    g_screenRequestsServiced.fetch_add(1, std::memory_order_release);
    fprintf(stderr, "[debug] requested %s through frontend manager %08X (hash %08X) "
                    "at %llds\n", name, g_frontendTransitionManager, screenHash,
            DebugElapsedSeconds());
}

} // namespace

// The post-hook. The loader must run FIRST — it writes every one of these bytes from
// the (empty) retail config, so a pre-hook's work would be overwritten immediately.
PPC_FUNC(sub_824A4C90)
{
    __imp__sub_824A4C90(ctx, base);
    ApplyFromEnvironment(base);
}

// Record the real manager on every native screen transition, then behave unchanged.
// CW_SCREEN_TRACE=1 logs every distinct screen hash with a timestamp — the tool that
// let Case Zero identify screens the title opens by itself.
PPC_FUNC(sub_82812410)
{
    g_frontendTransitionManager = ctx.r3.u32;
    if (getenv("CW_SCREEN_TRACE"))
    {
        static std::vector<uint32_t> seen;
        const uint32_t hash = ctx.r4.u32;
        bool isNew = true;
        for (uint32_t h : seen)
            if (h == hash) { isNew = false; break; }
        if (isNew)
            seen.push_back(hash);
        fprintf(stderr, "[screen] transition -> hash %08X at %llds%s\n", hash,
                DebugElapsedSeconds(), isNew ? "   <-- FIRST TIME" : "");
    }
    __imp__sub_82812410(ctx, base);
}

void DebugTunables_RequestDebugJump(PPCContext& ctx, uint8_t* base)
{
    RequestFrontendScreen(ctx, base, 0x8206D8C0, 9, "DebugJump");
}

void DebugTunables_RequestDebugEnter(PPCContext& ctx, uint8_t* base)
{
    RequestFrontendScreen(ctx, base, 0x8206E284, 10, "DebugEnter");
}

void DebugTunables_PumpPendingScreen(PPCContext& ctx, uint8_t* base)
{
    if (!g_pendingScreen.nameAddress || !g_frontendTransitionManager)
        return;
    const PendingScreen held = g_pendingScreen;
    g_pendingScreen = {};
    fprintf(stderr, "[debug] servicing the HELD %s request at %llds\n", held.name,
            DebugElapsedSeconds());
    RequestFrontendScreen(ctx, base, held.nameAddress, held.nameLength, held.name);
}

// Not ported yet (Case Zero's F4 overlay / AutoChuck surface): silent no-ops keep
// the imports.cpp seam; the header says what porting them takes.
void DebugTunables_PumpAutoChuck(PPCContext&, uint8_t*) {}
void DebugTunables_PumpDebugMenu(PPCContext&, uint8_t*) {}
void DebugTunables_ToggleFullDebugMenu(PPCContext&, uint8_t*)
{
    static bool said = false;
    if (!said)
    {
        said = true;
        fprintf(stderr, "[debug] ToggleFullDebugMenu: the F4 overlay is not ported to "
                        "Case West yet — only F2 DebugJump / F3 DebugEnter are\n");
    }
}

uint32_t DebugTunables_ScreenRequestsServiced()
{
    return g_screenRequestsServiced.load(std::memory_order_acquire);
}
bool DebugTunables_WantAutoBack() { return false; }

// The renderer's live-position dump hooks into these in Case Zero via guest_probe
// addresses this port has not re-derived; keep the honest "no answer" stubs.
extern "C" int CW_DebugPlayerPos(float out[3], long long* ageMs)
{
    if (out)
        out[0] = out[1] = out[2] = 0.0f;
    if (ageMs)
        *ageMs = 0;
    return 0;
}
extern "C" uint32_t CW_DebugWritePlayerObject(FILE*, uint32_t) { return 0; }
