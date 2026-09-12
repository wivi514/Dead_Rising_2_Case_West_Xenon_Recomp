#!/usr/bin/env python3
"""Find a sibling port's guest function in THIS image by instruction shape.

WHY THIS EXISTS
---------------
Every Case Zero feature that hooks a guest function names an address that does not
exist here: same engine, same compiler, different link. Parts 8 and 11 re-derived a
dozen of them by hand — dump the sibling's instruction bytes, mask what the linker
changes, search our image, disassemble the hit and read it side by side — and wrote
the recipe down twice (docs/native-kbm-import.md, runtime/cpu/rumble_guest.cpp).
Doing it by hand a thirteenth time is how a wrong address gets typed; this is the
recipe as a tool, with the one refinement those parts learned the hard way: a loose
prefix match is a CANDIDATE, so every hit is reported with its agreement score and
whether it falls on a recompiled function start, and the caller decides.

WHAT IS MASKED
--------------
The sibling's function is read from its image until the first `blr` (or --count
instructions). Each 32-bit instruction keeps its primary opcode and register fields
exactly; what the link moves is wildcarded:
  * I-form `b`/`bl` and B-form `bc*`: the displacement (opcode, BO/BI, AA/LK kept);
  * every D/DS-form immediate (addi/addis/lwz/stw/cmpi/ori/lfs/... — the low 16 bits),
    because global displacements and `lis/addi` address pairs differ image to image;
  * everything else (X/XO/A/M-form, VMX) must match bit for bit.
A first attempt that kept load displacements exact matched NOTHING (native-kbm-import).

USAGE
-----
  python3 tools/shape_match.py 0x828B5B60             # the sibling address to find
  python3 tools/shape_match.py 0x828B5B60 --count 40  # a fixed prefix length
  python3 tools/shape_match.py 0x828B5B60 --min-agree 0.9
Prints every candidate with its agreement over the matched length, the sibling's and
our disassembly lengths, and whether `ppc/` knows a function at that address. A unique
1.000 match on a function start is a finding; anything else is a lead.

TWO TRAPS MET ON THE FIRST DAY
------------------------------
* The pattern runs "until blr", and a function that ends in a TAIL CALL (`b epilogue`)
  or a thread-entry stub that never returns has no blr of its own — the pattern runs
  on into the next function and the agreement drops (0.78 for a 43-instruction wrapper
  read as 59). When the sibling's disassembly ends in `b`, pass --count.
* Immediates are wildcarded, so a 1.000 match says the CODE is the same, not that the
  struct offsets are: compare the D-form immediates of the two functions afterwards
  (a lis/addi pair that differs is a relocated global; a lwz/stw displacement that
  differs is a struct that grew — cpu/havok_threads.cpp records such a check).

Positive controls (run them when the tool changes): 0x82805A58 -> sub_828003D8 (the
rumble tick, part 11) and 0x82845160 -> sub_825B5FB8 (the fence loop, part 11), both
unique 1.000 on the first run.
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OURS = os.path.join(ROOT, "assets", "game", "default_image.bin")
SIBLING = os.path.expanduser(
    "~/GithubRepo/Dead_Rising_2_Case_Zero_Xenon_Recomp/assets/game/default_image.bin")
BASE = 0x82000000

# Primary opcodes whose low 16 bits are an immediate/displacement the link may change.
D_FORM = {7, 8, 10, 11, 12, 13, 14, 15, 24, 25, 26, 27, 28, 29,
          32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
          48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62}
BRANCH_I, BRANCH_B = 18, 16
BLR = 0x4E800020


def mask_of(insn):
    op = insn >> 26
    if op == BRANCH_I:
        return 0xFC000003            # opcode + AA/LK
    if op == BRANCH_B:
        return 0xFFFF0003            # opcode + BO + BI + AA/LK
    if op in D_FORM:
        return 0xFFFF0000            # opcode + RT + RA (DS-form: also drops XO, fine)
    return 0xFFFFFFFF


def read_words(path):
    with open(path, "rb") as f:
        data = f.read()
    n = len(data) // 4
    return struct.unpack(">%dI" % n, data[:n * 4])


def function_words(words, addr, count):
    i = (addr - BASE) // 4
    out = []
    while i < len(words) and len(out) < count:
        out.append(words[i])
        if words[i] == BLR:
            break
        i += 1
    return out


def ppc_function_starts():
    """Addresses the recompilation treats as function starts (ppc/*.cpp definitions)."""
    starts = set()
    ppc = os.path.join(ROOT, "ppc")
    if not os.path.isdir(ppc):
        return starts
    import re
    pat = re.compile(rb"PPC_FUNC_IMPL\(__imp__sub_([0-9A-Fa-f]{8})\)")
    for name in os.listdir(ppc):
        if name.endswith(".cpp"):
            with open(os.path.join(ppc, name), "rb") as f:
                for m in pat.finditer(f.read()):
                    starts.add(int(m.group(1), 16))
    return starts


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("addr", help="the SIBLING's function address (hex)")
    ap.add_argument("--count", type=int, default=400, help="max instructions to take (default: until blr)")
    ap.add_argument("--min-agree", type=float, default=0.85)
    ap.add_argument("--sibling", default=SIBLING)
    ap.add_argument("--ours", default=OURS)
    ap.add_argument("--anchor", type=int, default=12,
                    help="exact-shape prefix length used to seed candidates (default 12)")
    args = ap.parse_args()

    addr = int(args.addr, 16)
    sib = read_words(args.sibling)
    ours = read_words(args.ours)
    pat = function_words(sib, addr, args.count)
    if not pat:
        sys.exit("nothing at %08X in the sibling image" % addr)
    masks = [mask_of(w) for w in pat]
    n = len(pat)
    anchor = min(args.anchor, n)
    print("sibling %08X: %d instructions to blr (masked %d of them)" %
          (addr, n, sum(1 for m in masks if m != 0xFFFFFFFF)))

    # Seed on the anchor prefix, then score the whole pattern at each seed.
    seeds = []
    a_pat = [(pat[i] & masks[i], masks[i]) for i in range(anchor)]
    limit = len(ours) - n
    w0, m0 = a_pat[0]
    for i in range(limit):
        if ours[i] & m0 != w0:
            continue
        ok = True
        for k in range(1, anchor):
            w, m = a_pat[k]
            if ours[i + k] & m != w:
                ok = False
                break
        if ok:
            seeds.append(i)

    starts = ppc_function_starts()
    hits = []
    for i in seeds:
        agree = sum(1 for k in range(n) if ours[i + k] & masks[k] == pat[k] & masks[k])
        score = agree / n
        if score >= args.min_agree:
            # how long until OUR blr from here
            j = i
            while j < len(ours) and ours[j] != BLR and j - i < args.count * 2:
                j += 1
            hits.append((score, BASE + i * 4, j - i + 1))
    hits.sort(reverse=True)
    if not hits:
        print("NO candidate agrees >= %.2f over %d instructions (%d anchor seeds)" %
              (args.min_agree, n, len(seeds)))
        sys.exit(1)
    for score, va, ourlen in hits:
        tag = "function start" if va in starts else ("NOT a ppc/ function start" if starts else "ppc/ absent")
        print("  %08X  agreement %.3f over %d  (ours runs %d to blr)  %s" %
              (va, score, n, ourlen, tag))
    if len(hits) == 1 and hits[0][0] == 1.0:
        print("UNIQUE exact-shape match: sub_%08X" % hits[0][1])
    else:
        print("%d candidate(s) — a lead, not a finding; disassemble and compare (tools/gdis.py)" % len(hits))


if __name__ == "__main__":
    main()
