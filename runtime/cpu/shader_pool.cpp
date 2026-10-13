// The renderer's "Shader Constant Sets" pool, grown so mods can bring pixel shaders.
//
// WHY THIS EXISTS (2026-10-12, for the DR2 CZ/CW modding toolkit, phase 15; Case Zero's
// runtime/cpu/shader_pool.cpp, with every address re-derived on this image). A model
// brought from Case Zero draws with Case Zero's own pixel shaders, which the toolkit
// adds to Case West's shader banks. The renderer keeps ONE constant set per loaded pixel
// shader in a fixed pool, and Case West ships 1,337 pixel shaders against a pool of
// 1,340: three more and the title asserts
//   INVALID_HANDLE != shader_constant_set_handle && "Shader constant set overflow."
//
// WHERE THE SIZE COMES FROM. The renderer's pool builder sub_8275A168 reads the size at
// 0x8275AA94 (`lwz r4, 3380(r11)` = 0x82A00D34, the second word of the renderer's size
// table at 0x82A00D30) and builds the pool kept at renderer +3504 (sub_82757548 ->
// sub_82756410). Both arrays are allocated FROM that count (sub_82745E10: the sets;
// sub_82154190: count*8 bytes of free list), so a larger value is a larger pool, not an
// overrun. Unlike Case Zero (whose boot budget function overwrites its image's 384 with
// 1300), nothing in this image stores to 0x82A00D34 (a scan of every store through a
// `lis 0x82A0` base finds none, and the same scan finds Case Zero's store): the 1,340 is
// the image's own. Hooking the builder anyway keeps the two ports the same shape and
// is right whatever runs before it.
//
// Default: 4096 sets (~80 KB of guest heap more). CW_NO_SHADER_POOL_GROWTH=1 keeps the
// title's own size (the control). CW_SHADER_POOL=N picks another size (1340..65535).
// Logs one line when it raises the size. The write is refused, loudly, when the word
// there is not the title's 1340 at that moment: the one check that catches a moved image.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_8275A168);

namespace
{
// The pools grown, each read by the builder below (two 1340s of the image's table, and
// the description pool the boot sets to 231).
struct PoolWord
{
    uint32_t address;
    uint32_t title;     // the image's own value
    const char* what;
};
constexpr PoolWord kPools[] = {
    {0x82A00D34, 1340, "shader constant sets (read at 0x8275AA94, renderer +3504)"},
    {0x82A00D2C, 1340, "the pool read at 0x8275A9F8 (renderer +3492)"},
    // One per shader DESCRIPTION (.sdo): the boot budget function sub_825A6000 stores
    // 231 here, exactly the 231 descriptions Case West ships (none free); Case Zero's
    // matching pool ran out with Case West's gas zombie (its gotcha 643).
    {0x82A00D24, 231, "shader descriptions (read at 0x8275A990, renderer +3484)"},
};
constexpr uint32_t kDefaultGrownSize = 4096;

uint32_t GrownSize()
{
    if (getenv("CW_NO_SHADER_POOL_GROWTH"))
        return 0;
    if (const char* e = getenv("CW_SHADER_POOL"))
    {
        const long v = strtol(e, nullptr, 10);
        if (v >= 1340 && v <= 65535)
            return uint32_t(v);
        fprintf(stderr, "[shaderpool] CW_SHADER_POOL=%s ignored (1340..65535)\n", e);
    }
    return kDefaultGrownSize;
}

void Grow(uint8_t* base, const PoolWord& p, uint32_t grown)
{
    uint32_t be;
    memcpy(&be, base + p.address, 4);
    const uint32_t was = __builtin_bswap32(be);
    if (was == grown)
        return;   // a second build of the pools: already grown
    if (was != p.title)
    {
        fprintf(stderr, "[shaderpool] NOT growing %s: 0x%08X holds %u, expected %u (image "
                        "moved?)\n", p.what, p.address, was, p.title);
        return;
    }
    be = __builtin_bswap32(grown);
    memcpy(base + p.address, &be, 4);
    fprintf(stderr, "[shaderpool] %s: %u -> %u (mods can bring pixel shaders; "
                    "CW_NO_SHADER_POOL_GROWTH=1 keeps %u)\n", p.what, was, grown, was);
}
}   // namespace

// The renderer's pool builder: raise the pools' counts, then build.
PPC_FUNC(sub_8275A168)
{
    static bool done = false;
    if (const uint32_t grown = GrownSize(); grown && !done)
        for (const PoolWord& p : kPools)
            Grow(base, p, grown);
    done = true;
    __imp__sub_8275A168(ctx, base);
}
