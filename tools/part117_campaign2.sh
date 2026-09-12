#!/bin/bash
# Part 117 campaign 2: separate the split's share from the wake's, price the bundle on the
# renderer thread, and measure the huge-page advice — four env arms on ONE binary, three
# runs each, alternated (the same-binary discipline of tools/part116_ab.sh, four ways).
#
#   A  split, wake OFF          CW_PUMP_SPLIT=1 CW_WAITANY_WAKE=0
#   B  split + wake (default)   CW_PUMP_SPLIT=1
#   C  B + the part-109 bundle  + CW_VK_SCOPED_SHARED_ZERO=1 CW_VK_TEXMEMO=1
#   D  B, huge-page advice off  + CW_NO_HUGEPAGES=1
#
# Reader: tools/part116_guestcpu.py <A logs> -- <B logs>, pairwise.
# Usage: tools/part117_campaign2.sh <bin> [N=3]
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${1:?usage: part117_campaign2.sh <bin> [N]}"
N="${2:-3}"
export OUT="${OUT:-$HOME/DR2CZ-troubleshooting/part117}"
for i in $(seq 1 "$N"); do
    PERF=0 BIN_SRC="$BIN" "$ROOT/tools/part116_probe.sh" "c2A_normal$i" CW_PUMP_SPLIT=1 CW_WAITANY_WAKE=0
    PERF=0 BIN_SRC="$BIN" "$ROOT/tools/part116_probe.sh" "c2B_normal$i" CW_PUMP_SPLIT=1
    PERF=0 BIN_SRC="$BIN" "$ROOT/tools/part116_probe.sh" "c2C_normal$i" CW_PUMP_SPLIT=1 CW_VK_SCOPED_SHARED_ZERO=1 CW_VK_TEXMEMO=1
    PERF=0 BIN_SRC="$BIN" "$ROOT/tools/part116_probe.sh" "c2D_normal$i" CW_PUMP_SPLIT=1 CW_NO_HUGEPAGES=1
done
echo "C2 DONE"
