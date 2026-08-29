#!/usr/bin/env bash
# Survey ONE DebugJump destination for its draw-count band.
#
# WHY THIS EXISTS: every autonomous measurement so far runs at the recipe's single
# destination (~1,350 draws). Case Zero's part 80 established that a CPU change must be
# measured at the operator's heavy band or it reads as a null. This probe jumps to a
# chosen left-column entry (by DOWN count from the top), plays the same
# cinematic-skip / save-prompt dismissal as cw_hud_capture.sh, then stands for 60 s
# sweeping the camera with the right stick (the camera decides the draw set), logging
# [fps] windows every 3 s. The product is a draw-count band for that destination.
#
#   tools/cw_jump_probe.sh <outtag> <downs> [rights] [extra env...]
#     downs:  DOWN presses within the column
#     rights: RIGHT presses first — 0 = the case column (0=Case1-1, 1=Case2-1 [the
#             known recipe destination], 2=Case3-1, 3=Tutorial), 1 = the survivor
#             scoop column, 2 = the X_AutoChuck column (operator, 2026-08-29:
#             "twice you are in autochuck debug jump which can be really useful to
#             test performance in a crowd of zombies" — AutoChuck plays itself, so
#             it is the heavy-load soak this port lacked)
#
# Run from runtime/build.
set -u
OUT="${1:?usage: cw_jump_probe.sh <outtag> <downs> [rights] [ENV=VAL ...]}"
DOWNS="${2:?need DOWN count}"
RIGHTS="${3:-0}"
shift 2
[ $# -gt 0 ] && case "$1" in *=*) ;; *) shift ;; esac

NAV="NONE@21000,START,NONE@5000,F2,NONE@1500"
# [Y] IGNORE HUMANS — the image's own debug toggle (string at 49492,
# zombies_ignore_all_humans at 436992). Pressed on every jump so an unattended soak
# does not end with Chuck eaten mid-measurement (operator's advice, 2026-08-29).
NAV="$NAV,Y@200,NONE@600"
for ((i=0; i<RIGHTS; i++)); do NAV="$NAV,RIGHT@300,NONE@500"; done
for ((i=0; i<DOWNS; i++)); do NAV="$NAV,DOWN@300,NONE@500"; done
NAV="$NAV,A"
# The arrival sequence from cw_hud_capture.sh, kept verbatim so the scene state matches
# the known-good recipe: skip the cinematic (START..A), dismiss the save prompt (DOWN,A).
ARRIVE="NONE,NONE,NONE,NONE,NONE,START,NONE,NONE,A,NONE,NONE,NONE,NONE,NONE,NONE,NONE,DOWN,NONE,A,NONE,NONE"
# Belt and braces: a jump can land on screens the HUD recipe's timing never met (the
# first survey found Case2-1's CASE FILE screen holding an "A CONTINUE" through the
# whole soak). A few spaced A presses clear any lingering confirm; in gameplay a
# stray A is an attack and does not change what the camera sweep measures.
ARRIVE="$ARRIVE,A,NONE@2500,A,NONE@2500,A,NONE@2500"
# The soak: alternate right-stick sweeps so the camera pans the scene.
# F9 is held for 30 ms only: the capture key fires per FRAME while held (the 24-entry
# menu survey learned that at 893 files), so a short hold means one or two captures.
SOAK="RSRIGHT@6000,RSLEFT@6000,RSRIGHT@6000,RSLEFT@6000,RSRIGHT@6000,RSLEFT@6000,RSRIGHT@6000,RSLEFT@6000,RSRIGHT@6000,RSLEFT@6000,F9@30,NONE@2000"

pgrep -x cw_runtime | while read -r p; do kill "$p"; done
sleep 1
rm -rf "$OUT" "$OUT.log"

env "$@" CW_DEBUG_MENU=1 CW_FAKE_START_MS=1000 CW_FAKE_PRESS_SEQ="$NAV,$ARRIVE,$SOAK" \
    CW_FPS_LOG=3 CW_CAPTURE_KEY="$OUT" CW_VKDRAW=1 ./cw_runtime 2> "${OUT}.log" &
RPID=$!
while [ "$(ls "$OUT"/capture_*.ppm 2>/dev/null | wc -l)" -lt 1 ] && kill -0 $RPID 2>/dev/null; do
    sleep 5
done
sleep 2
pgrep -x cw_runtime | while read -r p; do kill "$p"; done

python3 - "$OUT" <<'PY'
import glob, sys
from PIL import Image
out = sys.argv[1]
for f in sorted(glob.glob(out + '/capture_*.ppm'))[:1]:
    im = Image.open(f)
    im.resize((im.width//2, im.height//2)).save(f.replace('.ppm', '.png'))
PY
echo "== ${OUT}: last 10 fps windows =="
grep '\[fps\]' "${OUT}.log" | tail -10
