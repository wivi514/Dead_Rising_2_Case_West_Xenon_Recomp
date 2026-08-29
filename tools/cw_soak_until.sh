#!/bin/bash
# Run gated crowd-route soaks until N are ACCEPTED, discarding the misses.
#
# WHY THIS EXISTS: the replayed route clears the doorway on some runs and clips the
# frame on others — physics nondeterminism at a narrow gap, the exact class Case
# Zero's operator diagnosed as unfixable on their route. The gate already separates
# the two outcomes (a wall run cannot hold the 4,000-draw band and is renamed
# .rejected); this wrapper turns "sometimes it works" into "K accepted runs, however
# many attempts that takes", which is what an A/B arm actually needs. Attrition costs
# machine minutes and zero operator time.
#
#   tools/cw_soak_until.sh <tag> <accepted-runs> [max-attempts] [ENV=VAL ...]
#     e.g.  tools/cw_soak_until.sh fr_on 3 8 CW_SHADER_DUMP=...
#           BIN_SRC=.../build-o3/cw_runtime tools/cw_soak_until.sh o3 3 8
set -u
TAG="${1:?usage: cw_soak_until.sh <tag> <n-accepted> [max-attempts] [ENV=VAL ...]}"
WANT="${2:?need accepted-run count}"
MAX="${3:-$((WANT * 3))}"
shift 2
[ $# -gt 0 ] && case "$1" in *=*) ;; *) shift ;; esac
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

got=0
for ((try=1; try<=MAX && got<WANT; try++)); do
    echo "== $TAG attempt $try (accepted so far: $got/$WANT)"
    if SEQFILE="${SEQFILE:-$ROOT/config/cw_soak_route.seq}" SOAK="${SOAK:-96}" \
       "$ROOT/tools/cw_crowdroute.sh" "${TAG}_r$try" "$@"; then
        got=$((got + 1))
    fi
    sleep 2
done
echo "== $TAG: $got of $WANT accepted in $((try - 1)) attempts"
[ "$got" -ge "$WANT" ]
