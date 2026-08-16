// Case West stand-in for gpu/d3d_draw.cpp (D3D pivot phase C), parked in
// runtime/port-pending/ because its hook addresses are Case Zero's.
//
// The D3D arm was always inert unless `CW_D3D_DRAW=1`, so the honest behaviour here is
// the behaviour the disabled arm already had: report not-enabled, service nothing, and
// let the PM4 command processor run — which is the boot engine and the control arm, and
// which transplants without any per-title addresses at all.
//
// `D3dDraw_Enabled()` returning false is what keeps `gpu/pm4.cpp` and `gpu/vd.cpp` on
// their normal path, so nothing else has to know this module is missing.
#include <cstdint>
#include <cstdio>

#include <ppc_config.h>   // ppc_context.h #errors without it
#include <ppc_context.h>

#include "d3d_draw.h"

bool D3dDraw_Enabled() { return false; }
bool D3dDraw_RedirectActive() { return false; }
bool D3dDraw_ServiceContent(PPCContext&, uint8_t*, CzGuestFunc) { return false; }
bool D3dDraw_ServiceReserve(PPCContext&, uint8_t*) { return false; }
bool D3dDraw_ServiceRealRing(PPCContext&, uint8_t*, CzGuestFunc) { return false; }
void D3dDraw_OnSwap(uint8_t*) {}
void D3dDraw_ScratchRange(uint32_t& va, uint32_t& bytes) { va = 0; bytes = 0; }

void D3dDraw_DumpStats()
{
    fprintf(stderr, "[d3d] the D3D translation arm is not ported to Case West "
                    "(runtime/port-pending/) — PM4 is the only path\n");
}

// The chain statistics counters lived in d3d_hooks.cpp, which is parked. `vd.cpp` reads
// them on the interrupt path, so they must exist; an all-zero snapshot is the truthful
// reading when nothing is counting, and `ChainStats_Read`'s consumers already treat a
// zero denominator as "no data" rather than dividing by it.
#include "../cpu/chain_stats.h"

ChainStats ChainStats_Read() { return ChainStats{}; }
void ChainStats_CountIsr() {}
