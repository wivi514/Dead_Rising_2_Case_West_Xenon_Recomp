#!/usr/bin/env bash
# Uninstrumented perf profile at the HEAVY band, on the replayed crowd route.
#
# The HUD-scene profile (finding 63) answered the light-scene regime; every question
# about the crowd frame — where the pump's 11 ms of walk actually goes, whether the
# parallel-record campaign's ceiling holds here — needs samples taken while the route
# soaks at 5,000+ draws. Same discipline as cw_perf_profile.sh: no ProfScope, cycles:u,
# the [fps] line as the phase detector.
#
#   tools/cw_crowd_perf.sh <outtag> [ENV=VAL ...]
# Products in ~/DR2CW-troubleshooting/crowdroute/: <tag>.perf + <tag>.report
set -u
TAG="${1:?usage: cw_crowd_perf.sh <outtag> [ENV=VAL ...]}"
shift
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$HOME/DR2CW-troubleshooting/crowdroute"

( SEQFILE="$ROOT/config/cw_soak_route.seq" SOAK=90 "$ROOT/tools/cw_crowdroute.sh" "$TAG" "$@" ) &
CHAIN=$!
LOG=""
for i in $(seq 1 60); do
    LOG=$(ls -t "$OUT"/crowd_*_"$TAG".log 2>/dev/null | head -1)
    [ -n "$LOG" ] && break
    sleep 2
done
[ -n "$LOG" ] || { echo "!! no log appeared"; exit 2; }
# Wait for the heavy band, then sample for 30 s.
while kill -0 $CHAIN 2>/dev/null; do
    if grep -a '\[fps\]' "$LOG" | tail -1 | grep -qE 'draws med [5-9][0-9]{3}'; then
        PID=$(pgrep -x cw_runtime_crow | head -1)
        [ -n "$PID" ] || break
        perf record -F 1997 -e cycles:u -o "$OUT/$TAG.perf" -p "$PID" -- sleep 30
        perf report -i "$OUT/$TAG.perf" --stdio --percent-limit 0.3 \
            --sort symbol > "$OUT/$TAG.report" 2>/dev/null
        break
    fi
    sleep 3
done
wait $CHAIN
echo "== $TAG gate: $?  report: $(wc -l < "$OUT/$TAG.report" 2>/dev/null || echo 0) lines"
