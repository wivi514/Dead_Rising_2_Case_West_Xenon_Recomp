// THE CAMERA-FOV SUBSTITUTION, GAME-SIDE — the ultrawide CULLING fix.
//
// Imported from Case Zero (their parts 62 and 108, cpu/camera_fov.cpp) at the
// operator's request, 2026-09-10. The MECHANISM is theirs; every ADDRESS below was
// re-derived on this image, because none of theirs exist here.
//
// WHY IT EXISTS. The renderer's composite patch widens what is DRAWN at 21:9 (and,
// since the 16:10 import, what is drawn tall in narrow mode) — but the title's own CPU
// culling still tests objects against the frustum the GAME believes it has, which is
// 16:9. So the flanks of an ultrawide picture show regions the game thinks are
// off-screen, and things pop in and out there. Widening the projection cannot fix that:
// the fix is to hand the GAME a wider fov, so it renders AND culls wide.
//
// HOW. The engine's camera tunables are data-driven NAMED properties. cThirdPersonCam
// registers FOV_Min/+0x15C, FOV_Max/+0x160, FOV_Default/+0x164 and FOV_Rate/+0x168
// through one universal binder — sub(this, name, count, &field) — reached by three
// count=1/2/4 thunks. On THIS image the registration walk is at 0x82462384..0x824623CC
// and the count=1 thunk is 0x82395A88, which tail-calls the binder sub_8236F648.
// (Case Zero: registration at 0x8246065C, thunks 0x82395FF0/0x82396000/0x82396010,
// binder sub_82375518. The four struct offsets are IDENTICAL on both titles — same
// engine, same layout — which is the cross-check that the walk found the right object.)
//
// The value the live camera actually uses does not come from those fields, though: it
// is a behaviour PARAM NODE, read through one tiny virtual accessor —
//
//     mr r11,r3 ; lwz r10,0(r4) ; mr r3,r4 ; lfs f1,0x14(r11) ; lwz r11,4(r10)
//     mtctr r11 ; bctr
//
// — i.e. "push [this+0x14] into the sink object through its vtable+4". That 28-byte
// sequence occurs EXACTLY ONCE in this image, at 0x8246D1A0 (Case Zero: sub_8246BF48,
// byte-identical), and it has no direct callers because it is only ever reached through
// a vtable. Substituting the float at [this+0x14] here hands the widened fov to the
// game itself.
//
// EVERY camera param flows through that one accessor, so the ROAMING camera's fov read
// has to be told apart from all the others by its CALL SITE. `CW_FOV_PARAM_TRACE=1`
// prints each distinct (return address, value) pair once — that census is what names the
// site, and `CW_FOV_SITE=0x…` sets it without a rebuild while it is being hunted.
//
// THE FIELD IS STATE, NOT A CONSTANT. Their first form ("read, add N, call, restore")
// ran the camera to its clamp in seconds, because the game writes its own smoothed fov
// back through the same node and +N compounded every frame. The shipped form captures
// the AUTHORED value the first time the site fires for an object — before any
// substitution can have disturbed it — and enforces base+N absolutely, with no restore.
// A slider back to 0 enforces the base exactly.
//
// `CW_NO_GAME_FOV=1` is the control arm for the whole guest-side substitution, and it
// ANNOUNCES itself: an arm that cannot be shown to have engaged is not an arm.
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>

#include "../gpu/vk_renderer.h"
#include "../host/settings.h"
#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_8236F648);
extern "C" PPC_FUNC(__imp__sub_8246D1A0);

namespace
{
// The call site that reads the ROAMING camera's fov, as measured by the census on this
// image (see the header). 0 means "not yet measured": the hooks still trace, and the
// substitution stays OFF rather than guessing at a site — a wrong site would move some
// other camera parameter and call it a fov.
// MEASURED ON THIS IMAGE, 2026-09-10, by the census this file carries. One boot with
// CW_FOV_PARAM_TRACE=1 produced exactly ONE distinct site:
//
//     [fovparam] lr=8246F730 this=A5AD5AC4 value=43.000000
//
// and TWO independent instruments agree it is the camera's fov. The param node's value
// field is `this+0x14` = A5AD5AD8, and the property binder hook printed
// `"FOV" obj=A5AD5930 count=1 field=A5AD5AD8` — the same address, registered under the
// name FOV. The authored value, 43.0, is the one Case Zero measured for the same
// OverShoulderCam asset, and the call site's dispatch preamble is byte-identical to
// theirs (mr r6/r5/r4/r3 ; lwz r11,0x14(r11) ; mtctr ; bctrl, returning into a
// `lwz r31,0xc(r31)` list walk).
constexpr uint32_t kFovSiteDefault = 0x8246F730;

uint32_t FovSite()
{
    static const uint32_t site = [] {
        if (const char* e = getenv("CW_FOV_SITE"))
        {
            const uint32_t v = uint32_t(strtoul(e, nullptr, 0));
            fprintf(stderr, "[fovgame] CW_FOV_SITE=%08X overrides the measured site "
                            "%08X\n", v, kFovSiteDefault);
            return v;
        }
        return kFovSiteDefault;
    }();
    return site;
}

bool ParamTrace()
{
    static const bool t = getenv("CW_FOV_PARAM_TRACE") != nullptr;
    return t;
}

float ReadField(uint8_t* base, uint32_t node)
{
    uint32_t bits;
    memcpy(&bits, base + node + 0x14, 4);
    bits = __builtin_bswap32(bits);
    float v;
    memcpy(&v, &bits, 4);
    return v;
}

void WriteField(uint8_t* base, uint32_t node, float v)
{
    uint32_t bits;
    memcpy(&bits, &v, 4);
    bits = __builtin_bswap32(bits);
    memcpy(base + node + 0x14, &bits, 4);
}
}   // namespace

// The camera-behaviour PARAM GETTER. See the header for how it was identified.
PPC_FUNC(sub_8246D1A0)
{
    if (ParamTrace())
    {
        static std::set<std::pair<uint32_t, uint32_t>> seen;
        static std::mutex mu;
        uint32_t bits;
        memcpy(&bits, base + ctx.r3.u32 + 0x14, 4);
        std::lock_guard<std::mutex> lock(mu);
        if (seen.emplace(uint32_t(ctx.lr), bits).second)
            fprintf(stderr, "[fovparam] lr=%08X this=%08X value=%f\n",
                    uint32_t(ctx.lr), ctx.r3.u32, ReadField(base, ctx.r3.u32));
    }

    static const bool off = [] {
        const bool o = getenv("CW_NO_GAME_FOV") != nullptr;
        if (o)
            fprintf(stderr, "[fov] CW_NO_GAME_FOV=1 — the guest-side fov substitution is "
                            "OFF; the game culls to its own 16:9 frustum\n");
        return o;
    }();

    const uint32_t site = FovSite();
    // The site's TIMELINE, for the same question their part 108 asked: a transition that
    // renders at the wrong ratio is either a camera class that never reads here, or the
    // roaming camera not reading (the count drops to zero and the field keeps whatever
    // the transition left).
    if (ParamTrace() && site && uint32_t(ctx.lr) == site)
    {
        static auto secStart = std::chrono::steady_clock::now();
        static unsigned fired = 0;
        ++fired;
        const auto now = std::chrono::steady_clock::now();
        if (now - secStart >= std::chrono::seconds(1))
        {
            fprintf(stderr, "[fovparam] site %08X fired %u times in the last second; "
                            "field now %.2f\n", site, fired, ReadField(base, ctx.r3.u32));
            fired = 0;
            secStart = now;
        }
    }

    if (!off && site && uint32_t(ctx.lr) == site)
    {
        static std::mutex mu;
        static std::map<uint32_t, uint32_t> baseBits;   // node -> authored value
        static bool wasActive = false;
        const int fovAdj = Settings_Fov();
        // THE WIDE-MODE OVER-WIDEN. The 21:9 view is k = 9W/16H wider in tan space than
        // the game's 16:9 frustum, so at ANY slider value the flanks show regions the
        // game believes are off-screen. Hand the game v' = 2*atan(k * tan(v/2)) — its
        // own frustum then covers the wide view's horizontal exactly — and the
        // renderer's composite patch narrows the projection back, so the PICTURE is
        // unchanged while the CULLING covers all of it. In narrow mode (16:10) the same
        // accessor returns 1/k > 1 and the axes swap; the mechanism is identical.
        const float wideK = VkRenderer_WideFovFactor();
        std::lock_guard<std::mutex> lock(mu);
        auto it = baseBits.find(ctx.r3.u32);
        if (it == baseBits.end())
        {
            uint32_t bits;
            memcpy(&bits, base + ctx.r3.u32 + 0x14, 4);
            it = baseBits.emplace(ctx.r3.u32, bits).first;
        }
        if (fovAdj != 0 || wideK != 1.0f || wasActive)
        {
            uint32_t sw = __builtin_bswap32(it->second);
            float v;
            memcpy(&v, &sw, 4);
            v += float(fovAdj);
            if (v < 10.0f)
                v = 10.0f;
            if (v > 120.0f)
                v = 120.0f;
            if (wideK != 1.0f)
            {
                v = 2.0f * std::atan(wideK * std::tan(v * 0.00872664626f)) * 57.2957795f;
                if (v > 150.0f)
                    v = 150.0f;
            }
            WriteField(base, ctx.r3.u32, v);
            if (!wasActive)
            {
                wasActive = true;
                fprintf(stderr, "[fovgame] game-side fov ACTIVE at the camera param "
                                "getter: base%+d deg, wide-culling factor %.4f -> "
                                "%.2f deg (node %08X, site %08X)\n",
                        fovAdj, wideK, v, ctx.r3.u32, site);
            }
        }
    }
    __imp__sub_8246D1A0(ctx, base);
}

// The universal named-property binder: sub(this, name, count, &field). Recon only —
// it registers at boot and zone load, never per frame, so the trace is gotcha-7 safe.
PPC_FUNC(sub_8236F648)
{
    static const bool on = getenv("CW_FOV_PROP_TRACE") != nullptr;
    static const bool traceAll = getenv("CW_PROP_TRACE_ALL") != nullptr;
    const uint32_t nameVa = ctx.r4.u32;
    if ((on || traceAll) && nameVa >= 0x82000000 && nameVa < 0x82C00000)
    {
        char name[48] = {};
        for (int i = 0; i < 47; i++)
        {
            const char c = char(base[nameVa + i]);
            if (!c || uint8_t(c) < 0x20 || uint8_t(c) > 0x7E)
                break;
            name[i] = c;
        }
        if (on && (strstr(name, "FOV") || strstr(name, "Fov")))
            fprintf(stderr, "[fovprop] \"%s\" obj=%08X count=%u field=%08X\n",
                    name, ctx.r3.u32, ctx.r5.u32, ctx.r6.u32);
        if (traceAll && name[0])
        {
            static std::set<std::string> seen;
            static std::mutex mu;
            std::lock_guard<std::mutex> lk(mu);
            if (seen.insert(name).second)
                fprintf(stderr, "[prop] \"%s\" count=%u field=%08X\n",
                        name, ctx.r5.u32, ctx.r6.u32);
        }
    }
    __imp__sub_8236F648(ctx, base);
}
