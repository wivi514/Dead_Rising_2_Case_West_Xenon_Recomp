// The "Zombie heap" grown by 16 MiB, taken from the streaming world's texture heap, so mods
// can load more zombie types at once.
//
// WHY THIS EXISTS (2026-10-13, for the DR2 CZ/CW modding toolkit, phase 15; Case Zero's
// runtime/cpu/zombie_heap.cpp, with every address re-derived on this image). The title
// makes heap "Zombie heap" (the loaded zombie types' models and textures) a constant
// 30,177,280 bytes, the same as Case Zero, where the street's own types leave 1.6 MB of
// it free and a brought type that is added rather than swapped crashed the load
// ("Failed to allocate memory!"). A loaded type takes its pack (zombies.big/<type>.big)
// unpacked plus ~5.48 MB the loaded types share (the toolkit's docs/formats.md §Engine
// memory: the heaps). Case West's levels are not measured; the two ports stay the same
// shape so a mod behaves alike in both.
//
// WHERE THE MEMORY COMES FROM. The title carves every heap out of one fixed block taken
// at boot, so the bytes come off the streaming world's texture heap ("SW Texture heap"):
//   * sub_82484BF8 (its size: two entries of the table sub_82484B28 reads, plus 0x10E400
//     unless a flag is set; one caller, returning to 0x8242EDF4, once at boot) returns GROWTH less;
//   * the Zombie heap's two calls in its manager (re-run for every level, the size built
//     at 0x82588978 from 0x01E19000 - 0x151800, or 20 MB when the byte at 0x82A744D3 is
//     set) ask GROWTH more: its block from heap 0 (sub_82792530, return address
//     0x82588994) and the heap made in it (sub_82790278, return address 0x825889AC).
//     Both are generic functions, so only a call from there carrying 30,177,280 is touched.
// The Zombie heap grows only if the texture heap really shrank first, and the two calls
// grow together or not at all.
//
// Default: 16 MiB. CW_NO_ZOMBIE_HEAP_GROWTH=1 keeps the title's sizes (the control).
// CW_ZOMBIE_HEAP_GROWTH_MB=N picks another amount (1..64). Logs one line per change.
#include <cstdio>
#include <cstdlib>

#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_82484BF8);
extern "C" PPC_FUNC(__imp__sub_82792530);
extern "C" PPC_FUNC(__imp__sub_82790278);

namespace
{
constexpr uint32_t kTitleZombieHeap = 30177280;     // 0x01E19000 - 0x151800, at 0x82588978
constexpr uint32_t kBlockReturn = 0x82588994;       // after "bl sub_82792530" (its block)
constexpr uint32_t kHeapReturn = 0x825889AC;        // after "bl sub_82790278" (the heap)
constexpr uint32_t kTextureFloor = 64u << 20;       // refuse to shrink a heap smaller than this

uint32_t Growth()
{
    static const uint32_t g = [] {
        if (getenv("CW_NO_ZOMBIE_HEAP_GROWTH"))
            return 0u;
        if (const char* e = getenv("CW_ZOMBIE_HEAP_GROWTH_MB"))
        {
            const long v = strtol(e, nullptr, 10);
            if (v >= 1 && v <= 64)
                return uint32_t(v) << 20;
            fprintf(stderr, "[zombieheap] CW_ZOMBIE_HEAP_GROWTH_MB=%s ignored (1..64)\n", e);
        }
        return 16u << 20;
    }();
    return g;
}

bool g_textureShrunk = false;   // the bytes the Zombie heap takes were freed
bool g_blockGrown = false;      // this level's Zombie heap block was asked bigger
}   // namespace

// The SW Texture heap's size: GROWTH less.
PPC_FUNC(sub_82484BF8)
{
    __imp__sub_82484BF8(ctx, base);
    const uint32_t g = Growth();
    if (!g)
        return;
    const uint32_t was = ctx.r3.u32;
    if (was < kTextureFloor + g)
    {
        fprintf(stderr, "[zombieheap] NOT shrinking the SW Texture heap: %u bytes is too small "
                        "(image moved?); the Zombie heap keeps its size\n", was);
        return;
    }
    ctx.r3.u64 = was - g;
    g_textureShrunk = true;
    fprintf(stderr, "[zombieheap] SW Texture heap: %u -> %u bytes (%u MiB to the Zombie heap; "
                    "CW_NO_ZOMBIE_HEAP_GROWTH=1 keeps both)\n", was, ctx.r3.u32, g >> 20);
}

// The Zombie heap's block, from heap 0.
PPC_FUNC(sub_82792530)
{
    const bool zombieBlock = ctx.lr == kBlockReturn;   // read now: the callee changes lr
    if (zombieBlock)
    {
        g_blockGrown = false;
        if (g_textureShrunk && ctx.r3.u32 == kTitleZombieHeap)
        {
            ctx.r3.u64 = kTitleZombieHeap + Growth();
            g_blockGrown = true;
        }
    }
    __imp__sub_82792530(ctx, base);
    if (zombieBlock && g_blockGrown && ctx.r3.u32 == 0)
        fprintf(stderr, "[zombieheap] the grown Zombie heap block (%u bytes) did not fit in "
                        "heap 0\n", kTitleZombieHeap + Growth());
}

// The Zombie heap itself, in that block: the same size as the block.
PPC_FUNC(sub_82790278)
{
    if (ctx.lr == kHeapReturn && g_blockGrown && ctx.r4.u32 == kTitleZombieHeap)
    {
        ctx.r4.u64 = kTitleZombieHeap + Growth();
        static bool logged = false;
        if (!logged)
            fprintf(stderr, "[zombieheap] Zombie heap: %u -> %u bytes (mods can load more "
                            "zombie types at once)\n", kTitleZombieHeap, ctx.r4.u32);
        logged = true;
    }
    __imp__sub_82790278(ctx, base);
}
