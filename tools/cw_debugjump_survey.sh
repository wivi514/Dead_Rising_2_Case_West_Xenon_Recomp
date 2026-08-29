#!/usr/bin/env bash
# Photograph the DebugJump menu, one capture per highlighted entry.
#
# WHY THIS EXISTS: every autonomous run so far uses ONE DebugJump entry (the second —
# F2, DOWN, A), which lands at ~1,350 draws. The operator plays scenes several times
# heavier, and Case Zero's part 80 established that a CPU change must be measured at
# the heavy band or it reads as a null (their gotchas 453/466). Their survey needed the
# operator because only a human could see the entry names; here F9 captures the guest's
# own menu render, so the names can be read from the PNGs afterwards and the whole
# survey is autonomous.
#
#   tools/cw_debugjump_survey.sh <outdir> [N_ENTRIES (default 24)]
#
# Run from runtime/build. Product: <outdir>/capture_NNNN.png, one per DOWN step —
# capture k shows the menu with entry k highlighted.
set -u
OUT="${1:?usage: cw_debugjump_survey.sh <outdir> [n]}"
N="${2:-24}"

SEQ="NONE@21000,START,NONE@5000,F2,NONE@1500"
for ((i=0; i<N; i++)); do
    SEQ="$SEQ,F9@500,NONE@300,DOWN@300,NONE@400"
done
SEQ="$SEQ,F9@500,NONE@2000"

pgrep -x cw_runtime | while read -r p; do kill "$p"; done
sleep 1
rm -rf "$OUT"

env CW_DEBUG_MENU=1 CW_FAKE_START_MS=1000 CW_FAKE_PRESS_SEQ="$SEQ" \
    CW_FPS_LOG=3 CW_CAPTURE_KEY="$OUT" CW_VKDRAW=1 ./cw_runtime 2> "${OUT}.log" &
RPID=$!
want=$((N + 1))
while [ "$(ls "$OUT"/capture_*.ppm 2>/dev/null | wc -l)" -lt $want ] && kill -0 $RPID 2>/dev/null; do
    sleep 5
done
sleep 2
pgrep -x cw_runtime | while read -r p; do kill "$p"; done

python3 - "$OUT" <<'PY'
import glob, sys
from PIL import Image
out = sys.argv[1]
for f in sorted(glob.glob(out + '/capture_*.ppm')):
    Image.open(f).save(f.replace('.ppm', '.png'))
print('%s: %d captures' % (out, len(glob.glob(out + '/capture_*.png'))))
PY
