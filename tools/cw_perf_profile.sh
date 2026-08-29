#!/usr/bin/env bash
# Drive the game in-game (same route as cw_hud_capture.sh) and take an UNINSTRUMENTED
# perf profile of the process during a long stationary hold.
#
# WHY THIS EXISTS: CW_VK_PROFILE's own bill is ~777 ns/draw — the same order as the
# per-draw costs it reports — so a question about where the REAL frame goes must be
# answered by a sampling profiler on a run that does not carry ProfScope at all
# (Case Zero gotcha 360: a hot path can be too hot to instrument with a ProfScope).
# CW_FPS_LOG is the only instrument armed: it costs one clock read per frame and its
# `draws med` column is how this script detects that the run reached in-game load.
#
#   tools/cw_perf_profile.sh <outtag> [extra env assignments...]
#
# Products, in runtime/build:
#   <outtag>.log        the runtime's stderr (CW_FPS_LOG lines included)
#   <outtag>.perf       perf.data, ~25 s of cycles:u over the whole process, in-game
#   <outtag>.report     perf report, flat by symbol
#   <outtag>/           the four HUD captures (the picture gate: the scene rendered)
#
# Run it from runtime/build. Timings are the operator's (see cw_hud_capture.sh); the
# only change is a 60 s NONE hold inserted before the F9 captures, which is the
# window perf samples in.
set -u
OUT="${1:?usage: cw_perf_profile.sh <outtag> [ENV=VAL ...]}"
shift

PRE="NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,NONE,START,NONE,NONE,NONE,NONE,NONE,F2,NONE,NONE,DOWN,NONE,NONE,A,NONE,NONE,NONE,NONE,NONE,START,NONE,NONE,A,NONE,NONE,DOWN,NONE,NONE,A"
# The save-prompt dismissal from cw_hud_capture.sh's TAIL, then the 60 s hold perf
# samples in, then the captures.
TAIL="NONE,NONE,NONE,NONE,START,NONE,NONE,A,NONE,NONE,NONE,NONE,NONE,NONE,NONE,DOWN,NONE,A,NONE,NONE,NONE@60000,F9,NONE,NONE,F9,NONE,NONE,F9,NONE,NONE,F9,NONE,NONE"

pgrep -x cw_runtime | while read -r p; do kill "$p"; done
sleep 1
rm -rf "$OUT" "$OUT.log" "$OUT.perf" "$OUT.report"

env "$@" CW_DEBUG_MENU=1 CW_FAKE_START_MS=1000 CW_FAKE_PRESS_SEQ="$PRE,$TAIL" \
    CW_FPS_LOG=2 CW_CAPTURE_KEY="$OUT" CW_VKDRAW=1 ./cw_runtime 2> "${OUT}.log" &
RPID=$!

# Wait for in-game load: an [fps] line whose median draw count clears 1000. The title
# screen sits at a few hundred draws, the HUD scene at ~1,345, so the threshold
# separates them with margin on both sides.
while kill -0 $RPID 2>/dev/null; do
    if grep '\[fps\]' "${OUT}.log" | tail -1 | grep -qE 'draws med [0-9]{4}'; then
        break
    fi
    sleep 2
done

if kill -0 $RPID 2>/dev/null; then
    # 3 s of settling (the first over-1000 window may straddle the loading screen),
    # then 25 s of samples well inside the 60 s hold.
    sleep 3
    perf record -F 1997 -e cycles:u -o "${OUT}.perf" -p $RPID -- sleep 25
    perf report -i "${OUT}.perf" --stdio --percent-limit 0.3 \
        --sort comm,dso,symbol > "${OUT}.report" 2>/dev/null
fi

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
grep '\[fps\]' "${OUT}.log" | tail -6
