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
//
// sub_8252A030 is the SAME logger with a zero-based level (it adds one and
// then formats identically). The co-op join's whole verdict is reported
// through it — "Received Handshake message from …", "Received handshake but
// currently has no server!", "Accepted client from …", "Rejected client
// from …" — which is why the first two-machine session could watch the
// host dequeue the guest's handshake and then see nothing at all. Both
// entry points are tapped now; lines from this one are marked [title:N*].
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ppc_config.h>
#include <ppc_context.h>

extern "C" PPC_FUNC(__imp__sub_8252A098);
extern "C" PPC_FUNC(__imp__sub_8252A030);
extern "C" PPC_FUNC(__imp__sub_82536B88);
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

static void GuestLogLine(PPCContext& ctx, uint8_t* base, int level, const char* mark)
{
    if (GuestLogLevel() >= level && ctx.r5.u32 != 0)
    {
        char buf[1024];
        const char* fmt = reinterpret_cast<const char*>(base + ctx.r5.u32);
        GuestFormat(buf, sizeof buf, fmt, ctx, base, 3);
        const size_t n = std::strlen(buf);
        fprintf(stderr, "[title:%d%s] %s%s", level, mark, buf,
                (n && buf[n - 1] == '\n') ? "" : "\n");
    }
}

PPC_FUNC(sub_8252A098)
{
    GuestLogLine(ctx, base, int(ctx.r4.u32), "");
    __imp__sub_8252A098(ctx, base);
}

// sub_82536B88(ok, file, message, line) is the online code's DESYNC check:
// when `ok` is false it formats "*** DESYNC ***: <message> > <file>:<line>"
// through the same retail no-op sink. These are the silent drops — the
// link manager's "Packet Sequence Number out of order" is one — so a
// failed check is printed at level 1 whenever the guest log is on at all.
PPC_FUNC(sub_82536B88)
{
    if (GuestLogLevel() >= 1 && (ctx.r3.u32 & 0xff) == 0 && ctx.r5.u32 != 0)
    {
        const char* msg = reinterpret_cast<const char*>(base + ctx.r5.u32);
        const char* file = ctx.r4.u32 ? reinterpret_cast<const char*>(base + ctx.r4.u32) : "?";
        const char* slash = std::strrchr(file, '/');
        fprintf(stderr, "[title:desync] %s  (%s:%u)\n", msg, slash ? slash + 1 : file,
                ctx.r6.u32);
    }
    __imp__sub_82536B88(ctx, base);
}

PPC_FUNC(sub_8252A030)
{
    GuestLogLine(ctx, base, int(ctx.r4.u32) + 1, "*");
    __imp__sub_8252A030(ctx, base);
}
