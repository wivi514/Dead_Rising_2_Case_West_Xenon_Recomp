#!/usr/bin/env python3
"""Extract Case West's debug-tunables table from the loader, mechanically.

WHY THIS EXISTS
---------------
The retail image still carries Blue Castle's development scaffolding, switched off
rather than compiled out: a single loader (`sub_824A4C90` here; Case Zero's was
`sub_824A2470`) looks ~400 boolean tunables up BY NAME through a get-bool-by-name
(`sub_82786708` here) and stores each answer into a fixed byte of one .data struct.
In retail every lookup misses and every byte is 0; the runtime's debug_tunables
module re-enables features by writing 1s into chosen bytes after the loader returns.

Deriving (name -> byte) pairs BY EYE was the exact off-by-one that cost Case Zero's
first probe its answer (their file documents it, and this port re-learned it as
gotcha 322): the loader pipelines `bl get-bool` for name N with the `stb` of name
N-1's result. So this tool walks the loader linearly, models just enough of the
register file (lis/addi/mr) to know what r4 held at each `bl` and which register the
result landed in, and attributes each `stb` to the LAST EXECUTED call — the pairing
the pipeline actually implements.

Every extracted byte is then CONFIRMED by scanning the whole image for `lbz`
consumers of that address (via lis/addi base tracking in a light linear pass). A
tunable nothing reads is a switch connected to nothing; those are listed separately
and the runtime must not offer them (Case Zero's rule, kept).

Output: a C table fragment on stdout, one line per tunable:
    { "name", 0xADDR, readers },
Run it against a freshly dumped image whenever the config changes:
    python3 tools/find_debug_tunables.py [image.bin]
"""
import struct
import sys
import bisect

IMAGE = sys.argv[1] if len(sys.argv) > 1 else "assets/game/default_image.bin"
BASE = 0x82000000
TEXT_LO, TEXT_SZ = 0x82150000, 0x87BC54
LOADER_LO, LOADER_HI = 0x824A4C90, 0x824A8528
GETBOOL = 0x82786708

data = open(IMAGE, "rb").read()


def u32(va):
    off = va - BASE
    return struct.unpack(">I", data[off:off + 4])[0]


def cstr(va, maxlen=64):
    s = b""
    off = va - BASE
    while data[off] != 0 and len(s) < maxlen:
        s += bytes([data[off]])
        off += 1
    return s.decode("latin1")


def walk_loader():
    """Yield (name_va, byte_va) pairs from the loader's pipelined stream."""
    regs = {}           # reg -> known constant (from lis/addi chains)
    last_call_name = None    # r4's value at the most recent executed bl GETBOOL
    result_regs = set()      # regs holding that call's result (r3, plus mr copies)
    pairs = []
    for va in range(LOADER_LO, LOADER_HI, 4):
        w = u32(va)
        op = w >> 26
        rd = (w >> 21) & 31
        ra = (w >> 16) & 31
        simm = w & 0xFFFF
        if simm & 0x8000:
            simm -= 0x10000
        if op == 15:                      # addis/lis
            regs[rd] = ((regs.get(ra, 0) if ra else 0) + (simm << 16)) & 0xFFFFFFFF
        elif op == 14:                    # addi/li
            regs[rd] = ((regs.get(ra, 0) if ra else 0) + simm) & 0xFFFFFFFF
        elif op == 31 and ((w >> 1) & 0x3FF) == 444 and rd == (w >> 11 & 31):
            # or rA,rS,rS  (mr): rA <- rS. Encoding: rS=rd field, rA=ra field.
            src = rd
            dst = ra
            if src in (3,) or src in result_regs:
                result_regs.add(dst)
            elif dst in result_regs:
                result_regs.discard(dst)
            if src in regs:
                regs[dst] = regs[src]
            else:
                regs.pop(dst, None)
        elif op == 38:                    # stb rS, d(rA)
            if rd in result_regs and ra in regs and last_call_name:
                target = (regs[ra] + simm) & 0xFFFFFFFF
                pairs.append((last_call_name, target))
                last_call_name = None     # one store per call
        elif op == 18 and not (w & 2):    # b/bl
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            if (w & 1) and (va + li) & 0xFFFFFFFF == GETBOOL:
                last_call_name = regs.get(4)
                result_regs = {3}
            elif w & 1:
                # some other call clobbers r3
                result_regs = set()
    return pairs


def count_lbz_readers(byte_vas):
    """One linear pass over .text counting lbz consumers of each byte address."""
    want = {va: 0 for va in byte_vas}
    regs = {}
    for off in range(TEXT_LO - BASE, TEXT_LO - BASE + TEXT_SZ, 4):
        w = struct.unpack(">I", data[off:off + 4])[0]
        op = w >> 26
        rd = (w >> 21) & 31
        ra = (w >> 16) & 31
        simm = w & 0xFFFF
        if simm & 0x8000:
            simm -= 0x10000
        if op == 15:
            regs[rd] = ((regs.get(ra, 0) if ra else 0) + (simm << 16)) & 0xFFFFFFFF
        elif op == 14:
            regs[rd] = ((regs.get(ra, 0) if ra else 0) + simm) & 0xFFFFFFFF
        elif op == 34:                    # lbz rD, d(rA)
            if ra in regs:
                t = (regs[ra] + simm) & 0xFFFFFFFF
                if t in want:
                    want[t] += 1
            regs.pop(rd, None)
        elif op == 18:
            regs = {}                     # branch: reset the light model
    return want


pairs = walk_loader()
readers = count_lbz_readers({p[1] for p in pairs})
print(f"// {len(pairs)} tunables extracted from sub_{LOADER_LO:08X}; "
      f"get-bool-by-name sub_{GETBOOL:08X}")
dead = []
for name_va, byte_va in pairs:
    name = cstr(name_va)
    n = readers.get(byte_va, 0)
    if n == 0:
        dead.append((name, byte_va))
        continue
    print(f'    {{ "{name}", 0x{byte_va:08X}, {n} }},')
print(f"// {len(dead)} tunables with ZERO detected lbz readers, EXCLUDED "
      f"(a switch this scan cannot show is connected):")
for name, byte_va in dead:
    print(f"//   {name} 0x{byte_va:08X}")
