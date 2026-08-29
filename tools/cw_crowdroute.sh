#!/bin/bash
# Replay the operator's crowd route, unattended — the heavy-band measurement run.
#
# Imported from Case Zero's tools/part80_crowdroute.sh (2026-08-29) and adapted: the
# route is the operator's own session of 2026-08-29 (12,880 pad samples -> 387 inputs,
# `config/cw_crowd_route.seq`), which goes through the MAIN MENU (no DebugJump) and
# sustains 4,700-6,600 draws med at 10-11 ms frames. It is THEIR route, not a
# synthesised one; the transcription rules are cw_transcribe_route.py's.
#
#   tools/cw_crowdroute.sh <tag> [ENV=VAL ...]
#     SOAK=N      extra seconds standing still at the end (default 60)
#     BIN_SRC=…   binary to run (default runtime/build/cw_runtime). Point it at
#                 runtime/build-o3/cw_runtime for the -O3 guest-image arm — the copy
#                 to cw_runtime_crowd makes both arms run under one process name.
#     SEQFILE=…   route file (default config/cw_crowd_route.seq)
#     RES=WxH     internal resolution, PINNED (default 2560x1440, the operator's own)
#
# Every run writes a per-frame trace for tools/cw_trace_band.py, and GATES itself:
# a run that never reached the crowd is renamed .rejected so no glob can pick it up.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$HOME/DR2CW-troubleshooting/crowdroute}"
TAG="${1:?usage: cw_crowdroute.sh <tag> [ENV=VAL ...]}"
shift
mkdir -p "$OUT"

# pgrep -x on the TRUNCATED comm name (15 chars), never pgrep -f (it matches the
# waiting shell's own command line — the sibling deadlocked three waiters that way).
for p in $(pgrep -x cw_runtime 2>/dev/null) $(pgrep -x cw_runtime_crow 2>/dev/null); do
    echo "!! a cw_runtime is already running (pid $p); refusing"; exit 2
done

SEQFILE="${SEQFILE:-$ROOT/config/cw_crowd_route.seq}"
[ -r "$SEQFILE" ] || { echo "!! no route file: $SEQFILE"; exit 2; }
BASE_SEQ="$(grep -a '^CW_FAKE_PRESS_SEQ=' "$SEQFILE" | tail -1 | cut -d= -f2-)"
[ -n "$BASE_SEQ" ] || { echo "!! $SEQFILE has no CW_FAKE_PRESS_SEQ= line"; exit 2; }

# The recorded lead-in, read out of the route file rather than duplicated here.
REC_LEADIN="$(printf '%s' "$BASE_SEQ" | sed -n 's/^NONE@\([0-9]*\),.*/\1/p')"
LEADIN="${LEADIN:-$REC_LEADIN}"
SEQ="$(printf '%s' "$BASE_SEQ" | sed "s/^NONE@${REC_LEADIN},/NONE@${LEADIN},/")"

# SOAK — extra seconds standing still at the end, in the crowd, where the frame time
# is actually measured (a turning camera changes the draw set every frame).
SOAK="${SOAK:-60}"
SEQ="$SEQ,NONE@$((SOAK * 1000)),NONE"

FPSLOG="${FPSLOG:-3}"
RES="${RES:-2560x1440}"   # PINNED — never take it from the desktop or the settings file.

# Timeout derived from the recipe's own length rather than fixed.
RECIPE_MS=$(printf '%s' "$SEQ" | tr ',' '\n' | sed -n 's/.*@\([0-9]*\)$/\1/p' \
            | awk '{s+=$1} END {print s+0}')
TIMEOUT="${TIMEOUT:-$(( 60 + RECIPE_MS / 1000 ))}"

BIN=cw_runtime_crowd
BIN_SRC="${BIN_SRC:-$ROOT/runtime/build/cw_runtime}"
[ -x "$BIN_SRC" ] || { echo "!! no such binary: $BIN_SRC"; exit 2; }
cp -f "$BIN_SRC" "$ROOT/runtime/build/$BIN"
HEAD="$(cd "$ROOT" && git rev-parse --short HEAD 2>/dev/null || echo unknown) [$BIN_SRC]"
STAMP="$(date +%m%d_%H%M%S)"
LOG="$OUT/crowd_${STAMP}_${TAG}.log"
TRACE="$OUT/crowd_${STAMP}_${TAG}.trace"

echo "=== $TAG  ($HEAD)  lead-in ${LEADIN}ms + route + ${SOAK}s soak  timeout ${TIMEOUT}s"
echo "    -> $LOG"
( cd "$ROOT/runtime/build" && env \
    CW_VKDRAW=1 "CW_FPS_CAP=500" "CW_FPS_LOG=$FPSLOG" "CW_VK_RES=$RES" \
    CW_DEBUG_MENU=1 "CW_FAKE_START_MS=100" "CW_FAKE_PRESS_SEQ=$SEQ" \
    "CW_DEBUG_TUNABLES=chuck_in_god_mode,disable_death_sequence,zombies_ignore_all_humans" \
    "CW_VK_FRAME_TRACE=$TRACE" \
    "CW_SHADER_DUMP=$HOME/DR2CW-troubleshooting/ucode-dumps" \
    "$@" timeout "$TIMEOUT" "./$BIN" > "$LOG" 2>&1 )

# THE ROUTE'S OWN GATE. The recording sustained 4,700-6,600 draws med; a replay that
# never holds 4,000 has desynchronised somewhere and is not this route.
peak=$(grep -a "^\[fps\]" "$LOG" | grep -aoE "draws med [0-9]+" | awk '{if($3>m)m=$3}END{print m+0}')
sustained=$(grep -a "^\[fps\]" "$LOG" | grep -aoE "draws med [0-9]+" | awk '$3>=4000' | wc -l)
echo "  peak windowed draws med: $peak    windows at >=4000 draws: $sustained"
if [ "${peak:-0}" -lt 4500 ] || [ "$sustained" -lt 4 ]; then
    echo "  ** DID NOT REACH/HOLD THE CROWD (need peak >=4500 and >=4 windows >=4000)."
    awk '/^\[fps\]/{ t += '"$FPSLOG"'
        if (match($0, /draws med [0-9]+/)) printf "     %4ds  %s\n", t, substr($0, RSTART, RLENGTH) }' "$LOG" | tail -14
    mv -f "$LOG" "${LOG%.log}.rejected"
    mv -f "$TRACE" "${TRACE%.trace}.trace.rejected" 2>/dev/null
    echo "  (renamed to ${LOG%.log}.rejected so no glob can pick it up)"
    exit 3
fi
echo "  OK"
