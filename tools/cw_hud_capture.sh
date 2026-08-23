#!/usr/bin/env bash
# Drive the game to the DebugJump destination with the HUD up and capture frames.
#
# WHY THIS EXISTS: every question about the HUD needs the same 90-second approach —
# title, DebugJump, skip the arrival cinematic, dismiss the save prompt — and typing
# that press sequence by hand is how an arm ends up compared against a different
# SCENE rather than a different setting. One script, one sequence, so two runs differ
# only by the environment passed to it.
#
#   tools/cw_hud_capture.sh <outdir> [extra env assignments...]
#   tools/cw_hud_capture.sh cap_control
#   tools/cw_hud_capture.sh cap_arm CW_VK_NO_PACKED_SMALL=1
#
# Timings are the operator's, re-derived at 1-second token granularity after the
# part-4 performance import changed every boot time (their rule: capture 1-3 s after
# the last A). Run it from runtime/build.
set -u
OUT="${1:?usage: cw_hud_capture.sh <outdir> [ENV=VAL ...]}"
shift

PRE="NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,START,NONE,NONE,NONE,NONE,NONE,F2,NONE,NONE,DOWN,NONE,NONE,A,NONE,NONE,NONE,NONE,NONE,START,NONE,NONE,A,NONE,NONE,DOWN,NONE,NONE,A"
TAIL="NONE,NONE,NONE,NONE,START,NONE,NONE,A,NONE,NONE,NONE,NONE,NONE,NONE,NONE,DOWN,NONE,A,NONE,NONE,F9,NONE,NONE,F9,NONE,NONE,F9,NONE,NONE,F9,NONE,NONE"

pgrep -x cw_runtime | while read -r p; do kill "$p"; done
sleep 1
rm -rf "$OUT"

env "$@" CW_DEBUG_MENU=1 CW_FAKE_START_MS=1000 CW_FAKE_PRESS_SEQ="$PRE,$TAIL" \
    CW_CAPTURE_KEY="$OUT" CW_VKDRAW=1 ./cw_runtime 2> "${OUT}.log" &
RPID=$!
while [ "$(ls "$OUT"/capture_*.ppm 2>/dev/null | wc -l)" -lt 4 ] && kill -0 $RPID 2>/dev/null; do
    sleep 5
done
sleep 3
pgrep -x cw_runtime | while read -r p; do kill "$p"; done

python3 - "$OUT" <<'PY'
import glob, sys
from PIL import Image
out = sys.argv[1]
for f in sorted(glob.glob(out + '/capture_*.ppm')):
    Image.open(f).save(f.replace('.ppm', '.png'))
print('%s: %d captures' % (out, len(glob.glob(out + '/capture_*.png'))))
PY
