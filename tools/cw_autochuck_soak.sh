#!/usr/bin/env bash
# The AutoChuck soak — this port's heavy-load measurement scene.
#
# WHY THIS EXISTS: every CPU claim needs the heavy band (Case Zero part 80: below it a
# CPU saving reads as a dead null), and this port had no scene above ~1,500 draws until
# the operator named the AutoChuck column (2026-08-29): DebugJump, RIGHT x4, is a level
# where the game PLAYS ITSELF in a zombie crowd. Measured at 3,100-4,800 draws and
# 9-12 ms frames in its heavy stretches — the first load on this title where a
# multi-millisecond item is even expressible.
#
#   tools/cw_autochuck_soak.sh <outtag> [downs] [ENV=VAL ...]
#     downs picks the level within the column: 0=X_AutoChuck 1=X_AutoChuck1
#     2=X_AutoChuck2 3=X_AutoChuck3 4=X_AutoChuckWeapons 5=X_AutoChuckVehicles
#     6=X_AutoChuck4 7=X_AutoChuckThrowWeapons
#
# Products: <outtag>.log ([fps] windows every 3 s) and <outtag>.trace (one line per
# frame: draws, wall, GPU, fence — the input to tools/cw_trace_band.py). No profiler:
# the soak is the throughput arm and stays uninstrumented; the trace columns used are
# the unconditional ones.
#
# [Y] IGNORE HUMANS is toggled before the jump so the soak cannot end with Chuck eaten
# (operator's advice). Input goes quiet after arrival — AutoChuck drives.
set -u
OUT="${1:?usage: cw_autochuck_soak.sh <outtag> [downs] [ENV=VAL ...]}"
DOWNS="${2:-0}"
shift
[ $# -gt 0 ] && case "$1" in *=*) ;; *) shift ;; esac
SOAK_MS="${SOAK_MS:-180000}"

NAV="NONE@21000,START,NONE@5000,F2,NONE@1500,Y@200,NONE@600"
for ((i=0; i<4; i++)); do NAV="$NAV,RIGHT@300,NONE@500"; done
for ((i=0; i<DOWNS; i++)); do NAV="$NAV,DOWN@300,NONE@500"; done
NAV="$NAV,A"
# The arrival dismissals from cw_jump_probe.sh, which reached this level intact.
ARRIVE="NONE,NONE,NONE,NONE,NONE,START,NONE,NONE,A,NONE,NONE,NONE,NONE,NONE,NONE,NONE,DOWN,NONE,A,NONE,NONE,A,NONE@2500,A,NONE@2500,A,NONE@2500"

pgrep -x cw_runtime | while read -r p; do kill "$p"; done
sleep 1
rm -f "$OUT.log" "$OUT.trace"

env "$@" CW_DEBUG_MENU=1 CW_FAKE_START_MS=1000 \
    CW_FAKE_PRESS_SEQ="$NAV,$ARRIVE,NONE@${SOAK_MS}" \
    CW_FPS_LOG=3 CW_VK_FRAME_TRACE="$OUT.trace" CW_VKDRAW=1 \
    timeout $(( SOAK_MS / 1000 + 75 )) ./cw_runtime 2> "${OUT}.log"
pgrep -x cw_runtime | while read -r p; do kill "$p"; done
echo "== ${OUT}: [fps] windows $(grep -c '\[fps\]' "${OUT}.log"), trace frames $(($(wc -l < "${OUT}.trace") - 1))"
grep '\[fps\]' "${OUT}.log" | tail -4
