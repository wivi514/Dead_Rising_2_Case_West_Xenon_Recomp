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
// THE F4 DEBUG MENU (ported 2026-09-17 — operator: "implement that F4 open the in
// game debug menu like case zero"; docs/imported-fixes.md §14)
// ----------------------------------------------------------------------------------
// The shipped executable still carries Blue Castle's complete cDebugMenu item tree
// (System Menu, Chartz Menu, Thread Edit Menu, NPC To Spawn, FontTest, a header that
// reads "DEBUG-ONLY TUNABLES" — all at 0x8206C8xx..CBxx here). Retail BUILDS it at
// boot and DESTROYS it before gameplay; the menu's own renderer draws nothing in a
// retail build. Case Zero's answer, kept here: capture the engine-allocated object as
// it is constructed, keep it alive through its teardown, and draw its nodes on the
// HOST overlay (host/window.cpp's Host_DebugMenu*, shared code) — F4 toggles it,
// Up/Down select, Enter/Right/Left act, and each node is driven through the guest's
// own value pointers and callbacks by its vtable. On top of the native tree sit five
// curated categories of the title's boolean tunables, resolved BY NAME into THIS
// title's extracted table (41 of the sibling's 46; five have no name here or in the
// sibling's own extraction and are not offered).
//
// Every address below was re-derived on this image (tools/shape_match.py, the vtable
// enumeration in §14):
// * cDebugMenu startup constructor `sub_824AB2E0` (CZ sub_824AAEB8): the sibling's
//   "System Menu" builder maps to ours by that string's one referrer (+0x428), and
//   the constructor at the same shift is the same function minus a 48-instruction
//   block of item insertions the sibling's tree has and ours does not (mnemonic diff
//   0.76, every other block identical). It reads `limited_debug_menu` (0x82A7433F,
//   our table) right after storing the cDebugMenu vtable 0x8206CBEC — the gate the
//   sibling names.
// * node base `sub_824A8528` (CZ sub_824A8120): every node constructor ends here to
//   validate and store its label at node+0x14 (`stw r28, 0x14(r27)`, the 0x12c bound
//   and the `twui` assert all present). It ends in a tail `b`, which is why the
//   blr-terminated shape search missed it — found at the shift, confirmed by the body.
// * destructor `sub_824AA970` (CZ sub_824A8FE0): one of two 1.000 shape matches; the
//   one whose two vtable stores land in the debug-menu region (0x8206CBEC/C690) and
//   which calls into it. The other candidate (sub_8259A058) is an unrelated class.
// * node vtables: six per image, same order, same shape. bool 0x8206C738 (its slot 8
//   `sub_824A8F40` is the unique 1.000 match of the sibling's), int 0x8206C768 (slot 8
//   `sub_824A9010`, unique), action 0x8206C8A4 (slots 1/2/8 `sub_824AA2D0`/
//   `sub_824A9388`/`sub_824A9350`, all unique), selector 0x8206C8E4 (by position and
//   the two shared no-op slots, as there). The field layout (+0x20 value/callback,
//   +0x24/+0x28 bounds or choices/count) is the sibling's, unchanged in the code
//   that reads it.
//
// NOT PORTED (per-title surfaces this port has no addresses for): AutoChuck, the
// zombie spawn/clear sweep, PP awards, the level cap — their pumps stay no-ops.
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
#include <iterator>
#include <string>
#include <vector>

#include "pc_options_cw.h"
#include "../host/window.h"

#include <ppc_config.h>   // ppc_context.h #errors without it
#include <ppc_context.h>

extern "C" PPC_FUNC(__imp__sub_824A4C90);   // the tunables loader
extern "C" PPC_FUNC(__imp__sub_82812410);   // request screen transition (mgr, hash, 0)
extern "C" PPC_FUNC(__imp__sub_827815D0);   // name -> hash (fe_probe's CONTROL A)
extern "C" PPC_FUNC(__imp__sub_824AB2E0);   // cDebugMenu startup constructor (F4 menu)
extern "C" PPC_FUNC(__imp__sub_824A8528);   // debug-menu node base: validate/store label
extern "C" PPC_FUNC(__imp__sub_824AA970);   // cDebugMenu destructor

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
            // The two bytes that select the COMPLETE embedded cDebugMenu tree (the
            // sibling's kLimitedDebugMenu / kEnableDevFeatures; both are in this
            // title's table by name). Retail already holds 0 in both — the write is
            // explicit so the log says which tree the F4 menu is built from.
            for (const char* name : { "limited_debug_menu", "enable_dev_features" })
                if (const Tunable* t = Find(name))
                    Apply(base, *t, 0);
                else
                    fprintf(stderr, "[debug] %s: NOT in this title's table — the F4 "
                                    "menu may come up limited\n", name);
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
    // THE VISUALS PANEL (imported from Case Zero part 60). Selecting Visuals in the
    // title's own options hub is swallowed entirely — we return the dispatcher's own
    // "not handled" (0) and open the host-drawn settings panel instead, so the hub
    // stays alive underneath and gets its input back when the panel closes. Before
    // the trace below on purpose: a swallowed transition is not a screen change and
    // should not read as one in the log.
    if (PcOptions_FilterScreenTransition(ctx, base))
    {
        ctx.r3.u64 = 0;
        return;
    }
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

// ===================================================================================
// THE F4 DEBUG MENU — the title's own cDebugMenu tree, drawn on the host overlay.
// See the header for how every address was derived. The model is the sibling's:
// the host shows a flat list of labels; the guest side keeps a parallel vector of
// "nodes" — a guest cDebugMenu node address, or one of the synthetic ids below — and
// acts on the selected one when the overlay reports Enter (1), Right (2) or Left (-1).
// ===================================================================================
namespace
{
constexpr uint32_t kNativeMenu = 0xFFFFF0FC;
constexpr uint32_t kCustomMenuBase = 0xFFFFE000;
constexpr uint32_t kCustomBoolBase = 0xFFFFD000;

// The node vtables of THIS image (six; the two unhandled are the base and a float).
constexpr uint32_t kVtBool = 0x8206C738;      // +0x20 -> byte
constexpr uint32_t kVtInt = 0x8206C768;       // +0x20 -> s32, +0x24/+0x28 low/high
constexpr uint32_t kVtAction = 0x8206C8A4;    // +0x20 -> callback(1)
constexpr uint32_t kVtSelector = 0x8206C8E4;  // +0x20 names, +0x24 -> u32 selected, +0x28 count
// cDebugMenu's own vtable, stored at +0 by the constructor right before it reads
// `limited_debug_menu` — the liveness check the F4 toggle makes (below).
constexpr uint32_t kVtDebugMenu = 0x8206CBEC;
std::atomic<uint32_t> g_debugMenuDtorCalls{ 0 };   // on ANY object (gotcha 151)

const char* const kCustomCategoryNames[] = {
    "PLAYER / WEAPONS >", "ZOMBIES / AI >", "VEHICLES >",
    "WORLD / RENDERING >", "UI / GAME FLOW >"
};

// The curated categories, BY NAME into this title's table (kTunables). The sibling's
// list was by address; mapping it here went through its names (the extractor run
// against the sibling's image) and 41 of 46 resolve. Not offered here because no
// name exists for them in one table or the other: disable_skill_move_cams (absent
// from this title), SHOW WHEEL TRANSFORMS, HIDE DEBUG TIRE MARKS, BUTTON THROUGH
// TIMED DIALOGS, DISABLE LEVEL UP MESSAGE (no name in the sibling's own extraction).
struct CustomBool
{
    uint8_t category;
    const char* label;
    const char* name;
};
const CustomBool kCustomBools[] = {
    {0, "CHUCK GOD MODE",               "chuck_in_god_mode"},
    {0, "CHUCK GHOST MODE",             "chuck_ghost_mode"},
    {0, "INFINITE PROP DURABILITY",     "infinite_prop_durability"},
    {0, "GET ALL COMBO CARDS",          "get_all_combo_cards"},
    {0, "ENABLE ALL SKILL MOVES",       "enable_all_skill_moves"},
    {0, "DISABLE DEATH SEQUENCE",       "disable_death_sequence"},
    {0, "SHOW CHUCK INFO",              "show_chuck_info"},
    {0, "SHOW WEAPON DEBUG INFO",       "weapon_show_debug_info"},
    {0, "SHOW JUMP HEIGHT",             "show_jump_height"},
    {0, "SHOW COMBO SEQUENCE COUNTER",  "show_combo_seq_counter"},

    {1, "ZOMBIES IGNORE ALL HUMANS",    "zombies_ignore_all_humans"},
    {1, "ZOMBIE DEBUG INFO",            "zombie_show_debug_info"},
    {1, "NPC DEBUG INFO",               "npc_show_debug_info"},
    {1, "SHOW ZOMBIE LINE OF SIGHT",    "show_zombie_los"},
    {1, "FORCE QUEEN BEES TO SPAWN",    "force_queen_bees_to_spawn"},
    {1, "DRAW DAMAGE LOGS",             "draw_damage_logs"},

    {2, "SHOW VEHICLE INFO",            "vehicle_show_info"},
    {2, "SHOW VEHICLE HEALTH",          "vehicle_show_health_info"},
    {2, "SHOW SUSPENSION",              "vehicle_show_suspension"},
    {2, "SHOW CENTER OF MASS",          "vehicle_show_center_of_mass"},
    {2, "PLOT ENVIRONMENT COLLISION",   "vehicle_plot_enviro_collision"},
    {2, "ENABLE VEHICLE DEBUG BUTTONS", "vehicle_enable_debug_buttons"},
    {2, "VEHICLE CAMERA FREE LOOK",     "vehicle_camera_free_look"},
    {2, "DISABLE PROCEDURAL ANIMATION", "vehicle_disable_procedural_animation"},
    {2, "HIDE ACTORS IN VEHICLES",      "hide_actors_in_vehicle"},

    {3, "ENABLE COLLISION VIEWER",      "enable_collision_viewer"},
    {3, "ENABLE AABB VIEWER",           "enable_aabb_viewer"},
    {3, "ENABLE VISUAL DEBUGGER",       "enable_visual_debugger"},
    {3, "SHOW CAMERA INFO",             "show_camera_info"},
    {3, "STATIONARY CAMERA",            "enable_stationary_cam"},
    {3, "DISABLE TIME OF DAY",          "disable_time_of_day"},
    {3, "CHUCK GRAVITY TEST",           "chuck_gravity_test"},

    {4, "SHOW FRONTEND SCREEN INFO",    "display_fe_screen_info"},
    {4, "SHOW LOADING TIMES",           "debug_show_loading_time"},
    {4, "SHOW GUIDE ARROW DEBUG INFO",  "display_guide_arrow_debug_info"},
    {4, "EVERYTHING UNLOCKED FOR MISSIONS", "missions_everything_unlocked"},
    {4, "SHOW ALL NOTEBOOK ENTRIES",    "notebook_show_all"},
    {4, "DISABLE TUTORIALS",            "disable_tutorials"},
    {4, "DISABLE MOVIES",               "disable_movies"},
    {4, "DISABLE CASE FILE POPUPS",     "disable_casefiles_popup"},
    {4, "DISABLE VIBRATION",            "disable_vibration"},
};

uint32_t CustomBoolAddress(size_t i)
{
    const Tunable* t = Find(kCustomBools[i].name);
    return t ? t->address : 0;
}

uint32_t g_debugMenuObject = 0;
bool g_buildingDebugMenu = false;
bool g_debugMenuActive = false;
std::vector<uint32_t> g_debugMenuNodes;          // every node seen during construction
std::vector<uint32_t> g_debugMenuNativeNodes;    // those with a label
std::vector<std::string> g_debugMenuNativeLabels;
std::vector<uint32_t> g_debugMenuVisibleNodes;   // what the overlay shows now
std::vector<std::string> g_debugMenuBaseLabels;
int32_t g_currentMenu = -1;                      // -1 root, -3 native, >=0 a category

std::string ReadGuestString(uint8_t* base, uint32_t address, size_t max = 96)
{
    std::string out;
    for (size_t i = 0; i < max; ++i)
    {
        const char c = static_cast<char>(PPC_LOAD_U8(address + uint32_t(i)));
        if (!c || static_cast<unsigned char>(c) < 32)
            break;
        out.push_back(c);
    }
    return out;
}

void PublishDebugMenuLabels(uint8_t* base)
{
    std::vector<std::string> labels;
    labels.reserve(g_debugMenuVisibleNodes.size());
    for (size_t i = 0; i < g_debugMenuVisibleNodes.size(); ++i)
    {
        const uint32_t node = g_debugMenuVisibleNodes[i];
        std::string label = g_debugMenuBaseLabels[i];
        if (node >= kCustomBoolBase && node < kCustomBoolBase + std::size(kCustomBools))
        {
            const uint32_t addr = CustomBoolAddress(node - kCustomBoolBase);
            label += !addr ? " : (NOT IN TABLE)" : PPC_LOAD_U8(addr) ? " : ON" : " : OFF";
            labels.push_back(std::move(label));
            continue;
        }
        if (node == kNativeMenu ||
            (node >= kCustomMenuBase && node < kCustomMenuBase + std::size(kCustomCategoryNames)))
        {
            labels.push_back(std::move(label));
            continue;
        }
        const uint32_t vtable = PPC_LOAD_U32(node);
        if (vtable == kVtBool && PPC_LOAD_U32(node + 0x20))
            label += PPC_LOAD_U8(PPC_LOAD_U32(node + 0x20)) ? " : ON" : " : OFF";
        else if (vtable == kVtInt && PPC_LOAD_U32(node + 0x20))
            label += " : " + std::to_string(
                static_cast<int32_t>(PPC_LOAD_U32(PPC_LOAD_U32(node + 0x20))));
        else if (vtable == kVtSelector && PPC_LOAD_U32(node + 0x24))
        {
            const uint32_t selected = PPC_LOAD_U32(PPC_LOAD_U32(node + 0x24));
            const uint32_t count = PPC_LOAD_U32(node + 0x28);
            const uint32_t choices = PPC_LOAD_U32(node + 0x20);
            label += " : ";
            if (choices && selected < count)
            {
                const uint32_t name = PPC_LOAD_U32(choices + selected * 4);
                label += name ? ReadGuestString(base, name, 64) : std::to_string(selected);
            }
            else
                label += std::to_string(selected);
        }
        labels.push_back(std::move(label));
    }
    Host_DebugMenuSetItems(labels);
}

void ShowDebugMenuRoot(uint8_t* base)
{
    g_currentMenu = -1;
    g_debugMenuVisibleNodes.clear();
    g_debugMenuBaseLabels.clear();
    for (uint32_t category = 0; category < std::size(kCustomCategoryNames); ++category)
    {
        g_debugMenuVisibleNodes.push_back(kCustomMenuBase + category);
        g_debugMenuBaseLabels.push_back(kCustomCategoryNames[category]);
    }
    g_debugMenuVisibleNodes.push_back(kNativeMenu);
    g_debugMenuBaseLabels.push_back("ORIGINAL ENGINE DEBUG ITEMS >");
    PublishDebugMenuLabels(base);
}

void ShowCustomMenu(uint8_t* base, uint32_t category)
{
    g_currentMenu = static_cast<int32_t>(category);
    g_debugMenuVisibleNodes.clear();
    g_debugMenuBaseLabels.clear();
    for (uint32_t i = 0; i < std::size(kCustomBools); ++i)
    {
        if (kCustomBools[i].category != category)
            continue;
        g_debugMenuVisibleNodes.push_back(kCustomBoolBase + i);
        g_debugMenuBaseLabels.push_back(kCustomBools[i].label);
    }
    PublishDebugMenuLabels(base);
}

void ShowNativeMenu(uint8_t* base)
{
    g_currentMenu = -3;
    g_debugMenuVisibleNodes = g_debugMenuNativeNodes;
    g_debugMenuBaseLabels = g_debugMenuNativeLabels;
    PublishDebugMenuLabels(base);
}
} // namespace

// Capture the genuine engine-allocated cDebugMenu object and every node built in the
// constructor's dynamic extent (the node-base hook below), then read each node's
// label (node+0x14) and publish the root. Activation later drives THESE pointers —
// never host-made state.
PPC_FUNC(sub_824AB2E0)
{
    g_debugMenuObject = ctx.r3.u32;
    g_debugMenuNodes.clear();
    g_buildingDebugMenu = getenv("CW_DEBUG_MENU") != nullptr;
    __imp__sub_824AB2E0(ctx, base);
    g_buildingDebugMenu = false;

    g_debugMenuNativeNodes.clear();
    g_debugMenuNativeLabels.clear();
    const bool dump = getenv("CW_DEBUG_MENU_DUMP") != nullptr;
    size_t byType[5] = {};   // bool, int, action, selector, other — the engagement census
    for (uint32_t node : g_debugMenuNodes)
    {
        const uint32_t address = PPC_LOAD_U32(node + 0x14);
        if (!address)
            continue;
        std::string label = ReadGuestString(base, address);
        if (label.empty())
            continue;
        const uint32_t vt = PPC_LOAD_U32(node);
        byType[vt == kVtBool ? 0 : vt == kVtInt ? 1 : vt == kVtAction ? 2
               : vt == kVtSelector ? 3 : 4]++;
        if (dump)
            fprintf(stderr, "[debug-node] %08X type=%08X value=%08X aux=%08X/%08X '%s'\n",
                    node, vt, PPC_LOAD_U32(node + 0x20), PPC_LOAD_U32(node + 0x24),
                    PPC_LOAD_U32(node + 0x28), label.c_str());
        g_debugMenuNativeNodes.push_back(node);
        g_debugMenuNativeLabels.push_back(std::move(label));
    }
    if (g_buildingDebugMenu || getenv("CW_DEBUG_MENU"))
        ShowDebugMenuRoot(base);
    fprintf(stderr, "[debug] captured cDebugMenu object %08X root=%08X (%zu nodes, %zu "
                    "labelled: %zu bool, %zu int, %zu action, %zu selector, %zu other) — "
                    "F4 opens it%s\n",
            g_debugMenuObject, PPC_LOAD_U32(g_debugMenuObject + 0x24), g_debugMenuNodes.size(),
            g_debugMenuNativeNodes.size(), byType[0], byType[1], byType[2], byType[3], byType[4],
            getenv("CW_DEBUG_MENU") ? "" : " (needs CW_DEBUG_MENU=1)");
}

// Every debug-menu node constructor finishes here to validate/store its label.
// Collected only inside cDebugMenu's real startup constructor, so unrelated uses of
// the common node base stay untouched.
PPC_FUNC(sub_824A8528)
{
    const uint32_t node = ctx.r3.u32;
    __imp__sub_824A8528(ctx, base);
    if (!g_buildingDebugMenu || node == g_debugMenuObject)
        return;
    for (uint32_t seen : g_debugMenuNodes)
        if (seen == node)
            return;
    g_debugMenuNodes.push_back(node);
}

// Retail destroys the fully populated startup cDebugMenu before gameplay, and it
// cannot be rebuilt later (its constructor consumes startup-only registry state).
// Keep this one engine-allocated instance — nodes and storage — while the
// instrument is on; every other instance and every ordinary run gets the original
// destructor exactly.
PPC_FUNC(sub_824AA970)
{
    g_debugMenuDtorCalls.fetch_add(1, std::memory_order_relaxed);
    if (ctx.r3.u32 == g_debugMenuObject && g_debugMenuObject && getenv("CW_DEBUG_MENU"))
    {
        fprintf(stderr, "[debug] preserving populated cDebugMenu %08X at teardown; "
                        "root=%08X\n", g_debugMenuObject,
                PPC_LOAD_U32(g_debugMenuObject + 0x24));
        ctx.r3.u64 = g_debugMenuObject;
        return;
    }
    __imp__sub_824AA970(ctx, base);
}

void DebugTunables_ToggleFullDebugMenu(PPCContext&, uint8_t* base)
{
    if (!getenv("CW_DEBUG_MENU"))
    {
        static bool said = false;
        if (!said)
        {
            said = true;
            fprintf(stderr, "[debug] F4: ignored, CW_DEBUG_MENU is not set (the menu tree "
                            "is only kept alive under it)\n");
        }
        return;
    }
    g_debugMenuActive = !g_debugMenuActive;
    if (g_debugMenuActive && g_debugMenuVisibleNodes.empty())
        ShowDebugMenuRoot(base);
    Host_DebugMenuSetVisible(g_debugMenuActive);
    // LIVENESS, measured at every open rather than assumed: on the first headless run
    // through a level load the destructor hook never saw this object (the sibling's
    // retail tears its menu down before gameplay; whether this title's does is a
    // question, not a premise). The object's vtable word and its first node's label
    // are what a freed-and-reused allocation would lose; the destructor counter says
    // whether the hook fires at all.
    const bool objectAlive = g_debugMenuObject && PPC_LOAD_U32(g_debugMenuObject) == kVtDebugMenu;
    bool nodesAlive = false;
    if (!g_debugMenuNativeNodes.empty())
    {
        const uint32_t n0 = g_debugMenuNativeNodes.front();
        nodesAlive = PPC_LOAD_U32(n0 + 0x14) &&
                     ReadGuestString(base, PPC_LOAD_U32(n0 + 0x14)) == g_debugMenuNativeLabels.front();
    }
    fprintf(stderr, "[debug] F4: host debug-menu renderer %s (%zu retained nodes, %zu "
                    "labelled; object %08X vtable %s, first node label %s; the destructor "
                    "hook has run on %u object%s)\n",
            g_debugMenuActive ? "opened" : "closed", g_debugMenuNodes.size(),
            g_debugMenuNativeNodes.size(), g_debugMenuObject,
            objectAlive ? "INTACT" : "GONE", nodesAlive ? "INTACT" : "GONE",
            g_debugMenuDtorCalls.load(), g_debugMenuDtorCalls.load() == 1 ? "" : "s");
}

// Not ported (AutoChuck is a per-title surface this port has no addresses for).
void DebugTunables_PumpAutoChuck(PPCContext&, uint8_t*) {}

// Runs on the XamInputGetState bridge — a guest thread with a usable context, which
// an action node's callback needs.
void DebugTunables_PumpDebugMenu(PPCContext& ctx, uint8_t* base)
{
    if (!g_debugMenuActive)
        return;
    uint32_t index = 0;
    int32_t action = 0;
    if (!Host_DebugMenuConsumeAction(index, action) || index >= g_debugMenuVisibleNodes.size())
        return;

    const uint32_t node = g_debugMenuVisibleNodes[index];
    const char* label = g_debugMenuBaseLabels[index].c_str();
    if (action == -1 && g_currentMenu != -1)
    {
        ShowDebugMenuRoot(base);
        return;
    }
    if (node >= kCustomMenuBase && node < kCustomMenuBase + std::size(kCustomCategoryNames))
    {
        if (action == 1 || action == 2)
            ShowCustomMenu(base, node - kCustomMenuBase);
        return;
    }
    if (node == kNativeMenu)
    {
        if (action == 1 || action == 2)
            ShowNativeMenu(base);
        return;
    }
    if (node >= kCustomBoolBase && node < kCustomBoolBase + std::size(kCustomBools))
    {
        if (action == 1 || action == 2 || action == -1)
        {
            const CustomBool& item = kCustomBools[node - kCustomBoolBase];
            const uint32_t addr = CustomBoolAddress(node - kCustomBoolBase);
            if (!addr)
            {
                fprintf(stderr, "[debug] '%s' (%s) is not in this title's table — nothing "
                                "to toggle\n", item.label, item.name);
                return;
            }
            const uint8_t next = PPC_LOAD_U8(addr) ? 0 : 1;
            PPC_STORE_U8(addr, next);
            fprintf(stderr, "[debug] Case West bool '%s' (%s) @%08X -> %s\n", item.label,
                    item.name, addr, next ? "ON" : "OFF");
            PublishDebugMenuLabels(base);
        }
        return;
    }

    // A native node: driven by its own vtable type through its own value pointers.
    const uint32_t vtable = PPC_LOAD_U32(node);
    if (vtable == kVtBool)
    {
        const uint32_t value = PPC_LOAD_U32(node + 0x20);
        if (value)
        {
            const uint8_t next = PPC_LOAD_U8(value) ? 0 : 1;
            PPC_STORE_U8(value, next);
            fprintf(stderr, "[debug] menu bool '%s' -> %s\n", label, next ? "ON" : "OFF");
        }
    }
    else if (vtable == kVtInt)
    {
        const uint32_t value = PPC_LOAD_U32(node + 0x20);
        if (value)
        {
            int32_t current = static_cast<int32_t>(PPC_LOAD_U32(value));
            const int32_t low = static_cast<int32_t>(PPC_LOAD_U32(node + 0x24));
            const int32_t high = static_cast<int32_t>(PPC_LOAD_U32(node + 0x28));
            current += action < 0 ? -1 : 1;
            if (low <= high)
            {
                if (current < low) current = low;
                if (current > high) current = high;
            }
            PPC_STORE_U32(value, static_cast<uint32_t>(current));
            fprintf(stderr, "[debug] menu integer '%s' -> %d\n", label, current);
        }
    }
    else if (vtable == kVtAction && action == 1)
    {
        const uint32_t callback = PPC_LOAD_U32(node + 0x20);
        if (callback)
        {
            fprintf(stderr, "[debug] menu action '%s' -> callback %08X\n", label, callback);
            ctx.r3.u64 = 1;
            ctx.ctr.u64 = callback;
            PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32);
        }
    }
    else if (vtable == kVtSelector)
    {
        const uint32_t selectedAddress = PPC_LOAD_U32(node + 0x24);
        const uint32_t count = PPC_LOAD_U32(node + 0x28);
        if (selectedAddress && count && action != 1)
        {
            uint32_t selected = PPC_LOAD_U32(selectedAddress);
            selected = action < 0 ? (selected ? selected - 1 : count - 1) : (selected + 1) % count;
            PPC_STORE_U32(selectedAddress, selected);
            fprintf(stderr, "[debug] menu selector '%s' -> %u/%u\n", label, selected, count);
        }
    }
    else
        fprintf(stderr, "[debug] menu item '%s': unsupported native type %08X\n", label,
                vtable);
    PublishDebugMenuLabels(base);
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
