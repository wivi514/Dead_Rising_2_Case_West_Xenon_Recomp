// The co-op SEATING trace: what the host does with a joining player's
// handshake, hop by hop. Off unless CW_SEAT_LOG=1. Diagnostic, not a feature.
//
// WHY. The first two-machine Case West session ended with every layer below
// the game healthy — punch, the title's reliable layer, a steady gameplay
// stream — and the guest parked forever on "attempting to join": the host
// dequeued the guest's 109-byte handshake (`Recv deq_size: 109`) and then
// nothing was logged at all. The title's join verdict is reported through
// its second logger entry (sub_8252A030, tapped in guest_log.cpp now), but
// even with that the hops between the reliable layer and cLocalServer::Accept
// are silent when they DROP, so this prints each of them:
//
//   sub_8254E5B0  cDataUnit::Create(raw, len, buf, max)   — the unit off the wire
//   sub_825547E8  cP2PGameplayTraffic2::UnPackDataFromRecv(this, n, unit)
//   sub_8253F0E8  the Event sink (needs traffic->0x88; matches the sender's
//                 address against the four peer slots before delivering)
//   sub_8254B2D8  the connection manager's HandleMessage(this, msg):
//                 byte 0x14 == 3 is the Handshake
//   sub_8254B098  HandleHandshake(this, req): this->0x7c is the server
//   sub_8254A7C0  cLocalServer::Accept(this, conn, req)  ("Accept Step 1")
//   sub_82558650  cTopologyManager's link-accepted registration: the peer's
//                 address/port/xuid as the host recorded them
//   sub_8253E8E0  cTopologyManager::HandleMessage(this, msg): a Handshake
//                 (0x14 == 3, 0x10 == 0x88) is routed to the topology whose
//                 id (+0x48) equals the request's target id — bits 1..7 of
//                 the decoded word — and DROPPED without a word otherwise
//   sub_8252CEC0  the decoder that produces that word
//
// Every function here is a recompiled weak symbol; the override calls the
// original through __imp__ and only prints around it, so with the switch off
// the title's behaviour is untouched. Remove once co-op seating lands.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <ppc_config.h>
#include <ppc_context.h>

extern "C" PPC_FUNC(__imp__sub_8254E5B0);
extern "C" PPC_FUNC(__imp__sub_825547E8);
extern "C" PPC_FUNC(__imp__sub_8253F0E8);
extern "C" PPC_FUNC(__imp__sub_8254B2D8);
extern "C" PPC_FUNC(__imp__sub_8254B098);
extern "C" PPC_FUNC(__imp__sub_8254A7C0);
extern "C" PPC_FUNC(__imp__sub_82558650);
extern "C" PPC_FUNC(__imp__sub_8253E8E0);
extern "C" PPC_FUNC(__imp__sub_8252CEC0);
extern "C" PPC_FUNC(__imp__sub_82517D68);

namespace
{
bool SeatLog()
{
    static const bool on = [] {
        const char* e = std::getenv("CW_SEAT_LOG");
        return e && *e && *e != '0';
    }();
    return on;
}

uint32_t L32(uint8_t* base, uint32_t a) { return a ? PPC_LOAD_U32(a) : 0; }
uint16_t L16(uint8_t* base, uint32_t a) { return a ? PPC_LOAD_U16(a) : 0; }
uint8_t L8(uint8_t* base, uint32_t a) { return a ? PPC_LOAD_U8(a) : 0; }
uint64_t L64(uint8_t* base, uint32_t a) { return a ? PPC_LOAD_U64(a) : 0; }

void Hex(uint8_t* base, uint32_t a, size_t n, char* out, size_t cap)
{
    size_t o = 0;
    for (size_t i = 0; i < n && o + 3 < cap; i++)
        o += size_t(snprintf(out + o, cap - o, "%02x", L8(base, uint32_t(a + i))));
}
} // namespace

PPC_FUNC(sub_8254E5B0)
{
    const uint32_t raw = ctx.r3.u32, len = ctx.r4.u32;
    if (SeatLog())
    {
        char hex[128];
        Hex(base, raw, len < 40 ? len : 40, hex, sizeof hex);
        fprintf(stderr, "[seat] cDataUnit::Create raw=%08X len=%u type=%u wire=%s\n", raw, len,
                L8(base, raw + 1), hex);
    }
    __imp__sub_8254E5B0(ctx, base);
    if (SeatLog())
        fprintf(stderr, "[seat]   -> unit %08X\n", ctx.r3.u32);
}

PPC_FUNC(sub_825547E8)
{
    const uint32_t self = ctx.r3.u32, n = ctx.r4.u32, unit = ctx.r5.u32;
    if (SeatLog())
        fprintf(stderr,
                "[seat] Traffic::UnPackDataFromRecv this=%08X state(+3c)=%u eventSink(+88)=%08X "
                "nfs(+90)=%08X n=%u unit=%08X unit+10=%08X unit+14=%02X\n",
                self, L32(base, self + 0x3c), L32(base, self + 0x88), L32(base, self + 0x90), n,
                unit, L32(base, unit + 0x10), L8(base, unit + 0x14));
    __imp__sub_825547E8(ctx, base);
    if (SeatLog())
        fprintf(stderr, "[seat]   -> %u\n", ctx.r3.u32);
}

PPC_FUNC(sub_8253F0E8)
{
    const uint32_t self = ctx.r3.u32, hdr = ctx.r4.u32, body = ctx.r5.u32;
    if (SeatLog())
    {
        char h1[64], h2[64];
        Hex(base, hdr, 0x14, h1, sizeof h1);
        Hex(base, body, 0x20, h2, sizeof h2);
        const uint32_t target = L32(base, self + 0x20);
        const uint32_t tvt = L32(base, target);
        fprintf(stderr,
                "[seat] EventSink this=%08X target(+20)=%08X (vtable %08X, +8 -> %08X) hdr=%s body=%s\n",
                self, target, tvt, L32(base, tvt + 8), h1, h2);
        for (uint32_t i = 0; i < 4; i++)
        {
            const uint32_t slot = L32(base, self + 0x94 + i * 0xc);
            if (slot)
                fprintf(stderr, "[seat]   peer slot %u: %08X addr=%08X port=%u state=%u\n", i,
                        slot, L32(base, slot + 0x18), L16(base, slot + 0x1c),
                        L32(base, slot + 4));
        }
    }
    __imp__sub_8253F0E8(ctx, base);
}

PPC_FUNC(sub_8254B2D8)
{
    const uint32_t self = ctx.r3.u32, msg = ctx.r4.u32;
    if (SeatLog())
        fprintf(stderr,
                "[seat] Manager::HandleMessage this=%08X server(+7c)=%08X msg=%08X msg+10=%08X "
                "msg+14=%02X (3 = Handshake)\n",
                self, L32(base, self + 0x7c), msg, L32(base, msg + 0x10), L8(base, msg + 0x14));
    __imp__sub_8254B2D8(ctx, base);
    if (SeatLog())
        fprintf(stderr, "[seat]   -> %u\n", ctx.r3.u32);
}

PPC_FUNC(sub_8254B098)
{
    const uint32_t self = ctx.r3.u32, req = ctx.r4.u32;
    if (SeatLog())
    {
        const uint32_t idx = L32(base, req + 0xc);
        const uint32_t table = L32(base, self + 0x4c);
        const uint32_t entry = table ? L32(base, table + 0xb4) : 0;
        const uint32_t slot = (entry && idx < 4) ? L32(base, entry + idx * 0xc + 0x94) : 0;
        fprintf(stderr,
                "[seat] HandleHandshake this=%08X server(+7c)=%08X req=%08X req+c(idx)=%u "
                "req+54=%08X req+58=%08X slotTable=%08X slot=%08X\n",
                self, L32(base, self + 0x7c), req, idx, L32(base, req + 0x54),
                L32(base, req + 0x58), entry, slot);
    }
    __imp__sub_8254B098(ctx, base);
    if (SeatLog())
        fprintf(stderr, "[seat]   -> %u\n", ctx.r3.u32);
}

PPC_FUNC(sub_8254A7C0)
{
    const uint32_t self = ctx.r3.u32, conn = ctx.r4.u32, req = ctx.r5.u32;
    if (SeatLog())
        fprintf(stderr, "[seat] cLocalServer::Accept this=%08X state(+c)=%u conn=%08X req=%08X\n",
                self, L32(base, self + 0xc), conn, req);
    __imp__sub_8254A7C0(ctx, base);
    if (SeatLog())
        fprintf(stderr, "[seat]   -> %u\n", ctx.r3.u32);
}

PPC_FUNC(sub_82558650)
{
    const uint32_t self = ctx.r3.u32;
    const uint32_t listener = L32(base, self + 0xb0);
    const uint32_t conn = listener ? L32(base, listener + 0x70) : 0;
    if (SeatLog())
        fprintf(stderr,
                "[seat] LinkAccepted this=%08X conn=%08X peer addr=%08X port=%u xuid=%016llX "
                "registrar(+b4)=%08X\n",
                self, conn, L32(base, conn + 0x80), L16(base, conn + 0x86),
                (unsigned long long)L64(base, conn + 0x78), L32(base, self + 0xb4));
    __imp__sub_82558650(ctx, base);
    if (SeatLog())
        fprintf(stderr, "[seat]   -> %u\n", ctx.r3.u32);
}

PPC_FUNC(sub_8253E8E0)
{
    const uint32_t self = ctx.r3.u32, msg = ctx.r4.u32;
    if (SeatLog())
    {
        fprintf(stderr, "[seat] TopoMan::HandleMessage this=%08X msg=%08X msg+10=%02X msg+14=%02X\n",
                self, msg, L8(base, msg + 0x10), L8(base, msg + 0x14));
        for (uint32_t i = 0; i < 4; i++)
        {
            const uint32_t t = L32(base, self + 0x94 + i * 4);
            if (t)
                fprintf(stderr, "[seat]   topology %u: %08X id(+48)=%d vtable=%08X\n", i, t,
                        int(L32(base, t + 0x48)), L32(base, t));
        }
    }
    __imp__sub_8253E8E0(ctx, base);
    if (SeatLog())
        fprintf(stderr, "[seat]   -> %u\n", ctx.r3.u32);
}

PPC_FUNC(sub_8252CEC0)
{
    const uint32_t out = ctx.r3.u32;
    __imp__sub_8252CEC0(ctx, base);
    if (SeatLog())
    {
        const uint32_t w = L32(base, out + 0x24);
        fprintf(stderr,
                "[seat] handshake decoded: word(+24)=%08X target id=%d (bits 1..7) +28=%02X +2c=%08X\n",
                w, int(int32_t(w << 1) >> 25), L8(base, out + 0x28), L32(base, out + 0x2c));
    }
}

// The event sink's sequenced path decodes the event header with this and
// then looks for a peer slot whose address (+0x18) equals the decoded +0x20;
// no slot, no delivery, no message.
PPC_FUNC(sub_82517D68)
{
    const uint32_t out = ctx.r3.u32;
    __imp__sub_82517D68(ctx, base);
    if (SeatLog())
        fprintf(stderr,
                "[seat] event header decoded: +10=%02X id(+14)=%u seq(+15)=%u +18=%016llX from(+20)=%08X\n",
                L8(base, out + 0x10), L8(base, out + 0x14), L8(base, out + 0x15),
                (unsigned long long)L64(base, out + 0x18), L32(base, out + 0x20));
}
