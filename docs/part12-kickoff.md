# Part 12 kickoff — the live hand-off

**Written at the end of part 12 (2026-09-12). THIS IS THE LIVE ONE.**
`part11-kickoff.md` is superseded; its §1a standing instruction ("wait for player
issues") was overtaken by two things: the operator opened XenonLive/co-op work on
the `xlive-integration` branch (2026-09-10, no part number — its record lives in
the XenonLive repo, `~/GithubRepo/XenonLive/docs/co-op.md` and
`integrating-a-port.md`, because that work is the service's, not this port's),
and then asked for the sibling's next round: *"Did a bunch of thing on case zero
need you to do it here too."* Part 12 is that import, and it is
`docs/imported-fixes.md` §13.

Read this, then §13, then the sibling's `docs/part118-kickoff.md` if a performance
number is going to be quoted — theirs are the measured ones.

---

## 1. WHERE THE PORT IS

* **Branch: `xlive-integration`**, 7 commits ahead of `master` at the end of
  part 12 (3 co-op/overlay commits from the previous session + part 12's 4). Master
  is v1.0.1's tree plus nothing. Whether the branch merges to master before the next
  artifact build is the operator's call; the sibling merged theirs before v1.1.0.
* **Released**: v1.0.0 (tagged) and v1.0.1 (built, gated, staged at
  `~/Release/Case West/1.0.1/`, operator-tested on NVIDIA ultrawide and the RX 6600).
  The GitHub clicks for 1.0.1 were still the operator's as of part 11's close.
* **Unreleased, on this branch**: everything in §13 (the two-core pump, thread
  placement, the wake, huge pages, the log cap, the trace re-arm fix, the guest
  thread instruments, XenonLive launcher-only + per-profile saves, the sign-in
  grace, the release scripts carrying libcurl), plus the co-op/overlay work that
  preceded it. **The sibling shipped the equivalent as v1.1.0** (co-op over
  XenonLive, the overlay, launcher-only online, per-profile saves — their
  `docs/release-notes-v1.1.0.md` is the template for ours).

## 2. WHAT PART 12 MEASURED, AND WHAT IT DID NOT

Every default and every control arm engaged in a headless boot (§13's table). Two
things beyond that:

**2a. The divergence that changed code.** This title does not name its threads —
its A1 has two `SetThreadName` raises (both `HavokWorkerThread`) against the
sibling's 19 — so the placement and the per-thread instruments had nothing to bind
to. Main Thread and Draw Thread are now bound BY IDENTITY (the entry thread; the
thread created at `0x8276FAC8`), with a first wrong attempt recorded in §13 and in
`fence_wait.cpp`. **A transplanted comment in `imports.cpp` had asserted the
sibling's 19 raises as this title's** — part 1's inheritance, never re-measured.
Retracted in place. Gotcha 326 below.

**2b. A retraction made the same day.** Three foreground engagement boots reached a
~5,000-6,000-draw scene ~50 s in, and for an hour this file and §13 called it "the
attract demo — a crowd reachable headlessly". The kcall trace of those boots has
`XamShowDeviceSelectorUI`: PRESS START had been accepted — the window was focused
with a controller attached, so the scene was reached by input, not by the title.
Six background runs with identical env never left the title screen. **There is still
no headless crowd on this port**; the soak route is stale (§8). Retracted in place
in §13 too. (The per-thread shape read off that scene — Main 7.8 ms CPU, Draw 4.1
CPU + 5.6 waiting — is a real measurement of it; only the "reachable on its own"
claim is withdrawn.)

**2c. The A/B that was run, for the record.** Three runs a side, 110 s, background,
new defaults vs `CW_PUMP_SPLIT=0 CW_GUEST_PIN=0 CW_WAITANY_WAKE=0` — every window
at the title screen (~751 draws). Numbers in the addendum at the end of this file.
At that scene the pump is ~1.3 ms of a core and the frame ~2.5 ms; there is nothing
for a second core or a placement to move, so the comparison is a null by
construction and says nothing about the crowd. **Read any future one with the
sibling's gotcha 572**: under `CW_FPS_CAP=500` the 1 ms vblank quantises every
presented frame, so the wall median cannot read a sub-millisecond change — the
per-stage CPU columns can.

**Not measured here**: the frame-time verdict is the sibling's (−1.19 ms wall
median at their crowd from the split, a further −0.35..−0.58 ms per stage from the
pin, operator-verified there). Windows (czwin compile of the placement's Windows
spelling). The old-base build with the static curl. A co-op session long enough to
exercise the sign-in grace (over an hour).

## 3. WHAT IS OWED, IN ORDER

1. **The operator's sitting on this build** — the thing every import owes. What
   to look for: smoother at the crowd (the sibling: "above 100 fps almost all the
   time"); the `[fps]` line's `guest main / draw` columns non-negative; `[pin]`
   lines for all four threads; no `[fencewait] … THIRD` surprises. **First
   bisection if anything is off: `CW_GUEST_PIN=0`, then `CW_PUMP_SPLIT=0`, then
   `CW_WAITANY_WAKE=0`** — each announces itself.
2. **A co-op session on this build** — the Live surface changed (launcher-only
   online, per-profile saves, the sign-in grace). A game started outside the
   XenonLive launcher is now the OFFLINE default profile by design; make sure the
   operator knows that before they report "my gamertag is gone".
3. **czwin**: pull, build, `--smoke`, a boot. The Windows spelling of the pin and
   the thread clocks came across untested.
4. **The old-base Linux build** (`tools/release_build_oldbase.sh` now builds a
   static libcurl+OpenSSL and REQUIRES the launcher checkout for the overlay) —
   then the artifacts. The notes: start from the sibling's v1.1.0 notes. **The
   public README still says "Two-player co-op is not supported" (lines 17 and
   144)** — true of v1.0.x, false of what this branch would ship; rewrite it with
   the operator's wording for the co-op's status, as the sibling's README did.
5. **Merge to master** (operator's call) — the branch is what would ship.

## 4. GOTCHAS WRITTEN IN PART 12

* **326. A sibling's capture is not this title's capture, even when the code is
  the same.** The `SetThreadName` list in `imports.cpp` was Case Zero's A1; ours
  has two entries. The tell was an instrument reading −1.00 — a *bound* number
  whose absence had a spelling. Grep transplanted comments for "A1 shows" and
  re-run the grep on OUR capture before believing the sentence. (`docs/gotchas.md`)
* **327. Name a thread by what it does, then check the census.** "The first
  thread to reach the ring-space wait" was a fact about the boot's ORDER (Main
  Thread first, then F08, then the Draw Thread), not about identity. The entry
  point is identity; the fence census is the check. (`docs/gotchas.md`)

## 5. MEASUREMENT RULES (unchanged from part 9's §4, plus one)

Same-binary arms; state the prediction; one change per experiment; ask the oracle;
an arm without an announcement line has not been shown to engage. **Plus**: under
`CW_FPS_CAP=500` read per-stage CPU, not the wall median (sibling's gotcha 572).
