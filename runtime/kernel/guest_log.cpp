// The title's own log, tapped.
//
// sub_8252A098(this, level, fmt, ...) is the routine every subsystem in
// this title reports through — the co-op lobby's "an error occurred"
// paths included, each with a format string saying WHICH error. Retail
// builds format it and drop it. With CW_GUEST_LOG=1 each line is printed
// here as [title] before the original runs, formatted with the same mini
// printf DbgPrint uses. It is what turned the first two-machine session's
// silent refusals into named ones.
//
// Levels: the title passes 1 for errors, 2-3 for notes, 4 for chatter;
// CW_GUEST_LOG=N prints levels up to N (default 3 when set to 1).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ppc_config.h>
#include <ppc_context.h>

extern "C" PPC_FUNC(__imp__sub_8252A098);
size_t GuestFormat(char* out, size_t cap, const char* fmt, PPCContext& ctx, uint8_t* base,
                   size_t firstArg);

namespace
{
int g_guestLogLevel = -1; // -1: not read yet; 0: off

int GuestLogLevel()
{
    if (g_guestLogLevel < 0)
    {
        const char* env = std::getenv("CW_GUEST_LOG");
        g_guestLogLevel = 0;
        if (env && *env && *env != '0')
        {
            const int n = std::atoi(env);
            g_guestLogLevel = n > 1 ? n : 3;
        }
    }
    return g_guestLogLevel;
}
} // namespace

PPC_FUNC(sub_8252A098)
{
    const int level = int(ctx.r4.u32);
    if (GuestLogLevel() >= level && ctx.r5.u32 != 0)
    {
        char buf[1024];
        const char* fmt = reinterpret_cast<const char*>(base + ctx.r5.u32);
        GuestFormat(buf, sizeof buf, fmt, ctx, base, 3);
        const size_t n = std::strlen(buf);
        fprintf(stderr, "[title:%d] %s%s", level, buf, (n && buf[n - 1] == '\n') ? "" : "\n");
    }
    __imp__sub_8252A098(ctx, base);
}
