#!/bin/bash
# PART 80 — RECORD THE OPERATOR'S ROUTE INTO THE CROWD, so I can replay it myself.
#
# WHY. `part80-kickoff.md` §1's item 1 is parallel command recording, and its measurement
# rule is blunt: **8,000+ draws or not at all**. Below that the autonomous route is
# GPU-bound and a CPU saving reads as a dead null — part 79 spent a six-run campaign
# re-learning that (§6dw §3, gotchas 453 and 466). `tools/autoroute.sh` selects Case 0-2
# and tops out at ~6,200 draws, so every CPU item on the board is currently unmeasurable
# without an operator sitting there for every arm.
#
# The operator's survey (`part80_debugjump_probe.sh`) found DebugJump entries that spawn
# into 8,490-8,885 draws. This run records HOW they get there, precisely enough to
# transcribe into a `CW_FAKE_PRESS_SEQ` recipe:
#
#   CW_INPUT_TRACE=1   every pad state change, stamped in milliseconds and decoded into
#                      the recipe's own vocabulary (A, DOWN, LSUP, RSRIGHT...)
#   CW_DEBUG_MENU=1    the DebugJump screen, and the `[debug] ... at Ns` line whose clock
#                      is the SAME epoch as the input trace — which is what lets the
#                      recipe be anchored on the screen landing rather than on boot,
#                      because boot depth is a distribution and not a constant (gotcha 75)
#   CW_FPS_LOG=3       so the crowd shows up as a number while they are standing in it
#
# ZOMBIES IGNORE ALL HUMANS is armed at the operator's request. It is a debug tunable the
# title ships, and it is the right one for a ROUTE recording: without it the run's timing
# depends on whether Chuck gets grabbed, and a recipe transcribed from a run that had to
# fight its way through is a recipe that desynchronises the first time it does not.
# CHUCK GOD MODE and DISABLE DEATH SEQUENCE are armed for the same reason — the same three
# `tools/autoroute.sh` has used since part 72, so the replay and the recording agree.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$HOME/DR2CW-troubleshooting/part80-route}"
TAG="${TAG:-route}"
mkdir -p "$OUT"
for p in $(pgrep -x cw_runtime 2>/dev/null); do echo "!! cw_runtime already running (pid $p); refusing"; exit 2; done
STAMP="$(date +%m%d_%H%M%S)"
LOG="$OUT/rt_${STAMP}_${TAG}.log"
TRACE="$OUT/rt_${STAMP}_${TAG}.trace"

cat <<EOF
===================================================================
 PART 80 — RECORDING YOUR ROUTE INTO THE CROWD
===================================================================
  Armed: ZOMBIES IGNORE ALL HUMANS, CHUCK GOD MODE, DISABLE DEATH SEQUENCE
         (the same three the autonomous route uses, so a replay matches)

  Every button and stick you touch is now logged with a MILLISECOND
  timestamp and decoded into the names my replay understands. The
  DebugJump screen landing is stamped on the same clock, so I can
  anchor the recipe on THAT rather than on boot time -- boot depth
  varies from 24 s to 131 s, so anchoring on the clock alone would
  work once and never again.

  WHAT I NEED:
    1. Go to the crowd spawn you found. Take your time -- pauses are
       recorded exactly, so waiting is free and I will reproduce it.
    2. Once you are there, do whatever makes the draw count sit high:
       walk into the thick of it, turn the camera, stand still.
    3. Say out loud (or just remember) WHICH DebugJump entry you picked
       and how many DOWN presses it was -- I can see the presses, but
       not the names on the screen.
    4. Quit normally when you are happy.

  Anything simple and repeatable is better than anything clever: I have
  to be able to run this a dozen times unattended, three runs per arm.

  log:   $LOG
  trace: $TRACE
===================================================================
EOF

# Case West differences from the Case Zero original this was imported from
# (2026-08-29): the flag mechanism here is CW_DEBUG_TUNABLES with the machine-
# extracted table's own snake_case names (debug_tunables_table.inc), and the
# shader-cache / A2M arms are Case Zero paths that do not exist here — defaults
# are already correct.
# BIN_DIR=build-o3 runs the -O3 guest-image arm; default is the ordinary build.
( cd "$ROOT/runtime/${BIN_DIR:-build}" && env \
    CW_VKDRAW=1 CW_FPS_CAP=500 CW_FPS_LOG=3 \
    CW_DEBUG_MENU=1 CW_INPUT_TRACE=1 \
    "CW_DEBUG_TUNABLES=chuck_in_god_mode,disable_death_sequence,zombies_ignore_all_humans" \
    "CW_VK_FRAME_TRACE=$TRACE" \
    "CW_CAPTURE_KEY=$OUT/$TAG" \
    "CW_SHADER_DUMP=$HOME/DR2CW-troubleshooting/ucode-dumps" \
    "$@" \
    ./cw_runtime > "$LOG" 2>&1 )

echo
echo "  finished.  $LOG"
echo "  internal resolution: $(grep -ao 'internal resolution [0-9x]*' "$LOG" | head -1)"
echo "  debug flags applied:"
grep -a "^\[debug\] " "$LOG" | grep -aE "god_mode|ignore_all_humans|death_sequence" | sed 's/^/    /'
echo
echo "  --- YOUR INPUT, in order (this is what I transcribe) ---"
grep -a "^\[input\]" "$LOG" | sed 's/^/  /'
echo
echo "  --- THE ANCHOR: when the DebugJump screen actually landed ---"
grep -a "through frontend manager" "$LOG" | sed 's/^/  /'
echo
echo "  --- DRAW COUNT OVER TIME (3 s windows) ---"
awk '/^\[fps\]/{ t += 3
        if (match($0, /draws med [0-9]+ \([0-9]+\.\.[0-9]+\)/)) {
            s = substr($0, RSTART, RLENGTH)
            if (match($0, /median \([0-9.]+ ms\)/)) ms = substr($0, RSTART+8, RLENGTH-9)
            printf "  %5ds  %-32s  %s\n", t, s, ms } }' "$LOG"
echo
grep -a "^\[fps\]" "$LOG" | grep -aoE "draws med [0-9]+" | awk '{if($3>m)m=$3}END{print "  best sustained draws med: " m+0 "   (need >= 8,000 for a CPU item)"}'
