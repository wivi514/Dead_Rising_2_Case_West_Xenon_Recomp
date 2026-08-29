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

# THE CAMERA SWEEP IS THE LOAD, not an extra. AutoChuck's roam degenerates — the
# operator watched two runs walk Chuck into a wall (one then teleported into the
# storage bay and wall-jumped the soak away), and all four hands-off soaks collected
# under 150 heavy frames in ~50,000: the crowd is AROUND Chuck, and a wall-facing
# camera collapses the draw count. The right stick moves the CAMERA, not Chuck, so
# sweeping it panned the first probe into 3,100-4,800-draw views for whole windows.
# The sweep runs for the entire soak.
SOAK=""
half=$(( SOAK_MS / 12000 ))
for ((i=0; i<half; i++)); do SOAK="$SOAK,RSRIGHT@6000,RSLEFT@6000"; done
SOAK="${SOAK#,}"

pgrep -x cw_runtime | while read -r p; do kill "$p"; done
sleep 1
rm -f "$OUT.log" "$OUT.trace"

env "$@" CW_DEBUG_MENU=1 CW_FAKE_START_MS=1000 \
    CW_FAKE_PRESS_SEQ="$NAV,$ARRIVE,$SOAK,NONE@2000" \
    CW_FPS_LOG=3 CW_VK_FRAME_TRACE="$OUT.trace" CW_VKDRAW=1 \
    timeout $(( SOAK_MS / 1000 + 75 )) ./cw_runtime 2> "${OUT}.log"
pgrep -x cw_runtime | while read -r p; do kill "$p"; done
echo "== ${OUT}: [fps] windows $(grep -c '\[fps\]' "${OUT}.log"), trace frames $(($(wc -l < "${OUT}.trace") - 1))"
grep '\[fps\]' "${OUT}.log" | tail -4

# THE ACCEPTANCE GATE, same shape as Case Zero's crowd route (a failed run renamed so
# no glob can pick it up). AutoChuck is the title's own AI and can degenerate — the
# operator watched one run walk Chuck into a wall and jump at it for the whole soak,
# a static ~800-draw scene that contributes nothing to the heavy band. A run must put
# at least GATE_FRAMES trace frames at or above GATE_DRAWS or it is .rejected.
GATE_DRAWS="${GATE_DRAWS:-2500}"
GATE_FRAMES="${GATE_FRAMES:-1000}"
heavy=$(awk -v g="$GATE_DRAWS" 'NR>1 && $2>=g {n++} END {print n+0}' "${OUT}.trace")
if [ "$heavy" -lt "$GATE_FRAMES" ]; then
    mv "${OUT}.trace" "${OUT}.trace.rejected"
    mv "${OUT}.log" "${OUT}.log.rejected"
    echo "== ${OUT}: REJECTED — only $heavy frames at >= $GATE_DRAWS draws (need $GATE_FRAMES). AutoChuck degenerated; rerun."
else
    echo "== ${OUT}: ACCEPTED — $heavy frames at >= $GATE_DRAWS draws."
fi
