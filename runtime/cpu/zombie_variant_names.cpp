#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ppc_recomp_shared.h"

// ZOMBIE VARIANT MESH NAMES — Case Zero 78632f1 (2026-10-08), re-derived for Case West.
//
// sub_827ACF00 builds each zombie mesh's hide-mask entry from its name by calling the
// wildcard classifier sub_827A2E10. Variant digits normally precede "Shape":
// `chestupper_neck2ShapeDeformed`, `headwear2ShapeDeformed`. Case West's zombie_security
// and zombie_soldier archives also contain `chestupper_neckShape1Deformed`,
// `chestupper_neckShape3Deformed`, `chestupper_neckShape2Deformed`, and
// `headwearShape2Deformed`. Those names leave the variant digit out of the classifier's
// hide-mask entry, so the distant model can show a part that the close model hides.
//
// The classifier is a unique 1.000 shape match here at sub_8273B160 (CZ sub_827A2E10);
// the mesh-table builder is a unique 1.000 match at sub_827468F0 (CZ sub_827ACF00).
// Their relevant argument/register behavior matches. Rewrite only the known body-part
// prefixes while classifying: `<part>Shape<digit>Deformed` becomes
// `<part><digit>ShapeDeformed`, same length, then restore the asset name. The archive also
// has `geo_lodShape4Deformed`; it is outside the two body-part families covered here and its
// classification was not measured. `CW_ZOMBIE_NAME_FIX=0` restores the shipped classification
// for an A/B.
//
// The source archives establish the affected names; the runtime mask values still need
// the DebugJump A/B after the next build (see docs/imported-fixes.md §16).

extern "C" PPC_FUNC(__imp__sub_8273B160);

namespace
{
bool Enabled()
{
    static const bool on = [] {
        const char* e = std::getenv("CW_ZOMBIE_NAME_FIX");
        return !(e && std::strcmp(e, "0") == 0);
    }();
    return on;
}

int MisplacedVariantDigit(const char* s, size_t n)
{
    // Only the two mesh-name families whose neighboring variants establish the convention.
    static constexpr const char* kPrefixes[] = {"chestupper_neck", "headwear"};
    for (const char* prefix : kPrefixes)
    {
        const size_t p = std::strlen(prefix);
        if (n < p + 5 + 1 + 8 || std::memcmp(s, prefix, p) != 0 ||
            std::memcmp(s + p, "Shape", 5) != 0)
            continue;
        const size_t d = p + 5;
        if (std::isdigit(static_cast<unsigned char>(s[d])) &&
            std::memcmp(s + d + 1, "Deformed", 8) == 0)
            return int(d);
    }
    return -1;
}
}   // namespace

PPC_FUNC(sub_8273B160)
{
    const uint32_t nameVa = ctx.r5.u32;
    if (!Enabled() || nameVa == 0)
    {
        __imp__sub_8273B160(ctx, base);
        return;
    }

    char* name = reinterpret_cast<char*>(base + nameVa);
    size_t n = 0;
    while (n < 63 && name[n])
        ++n;
    if (n == 63)
    {
        __imp__sub_8273B160(ctx, base);
        return;
    }
    const int d = MisplacedVariantDigit(name, n);
    if (d < 0)
    {
        __imp__sub_8273B160(ctx, base);
        return;
    }

    char saved[64];
    std::memcpy(saved, name, n + 1);
    const char digit = name[d];
    std::memmove(name + d - 4, name + d - 5, 5); // move "Shape" one byte right
    name[d - 5] = digit;

    static std::atomic<unsigned> shown{0};
    if (shown.fetch_add(1, std::memory_order_relaxed) < 8)
        std::fprintf(stderr, "[zvariant] classifying '%s' as '%s' (CW_ZOMBIE_NAME_FIX=0 "
                             "restores the shipped classification)\n",
                     saved, name);
    __imp__sub_8273B160(ctx, base);
    std::memcpy(name, saved, n + 1);
}
