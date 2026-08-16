# Fixes imported from Case Zero — what, when, and from which commit

**Why this file exists.** Case West and Case Zero are two implementations of the *same
engine*, so a defect fixed in one is usually a defect fixed in both — and the right move is
to **import the fix, not re-derive it**. But an import is a snapshot: the sibling keeps
moving, and a fix that arrives here on Monday may be revised there on Friday. Without a
record of exactly which commit was taken and when, the only way to find out what this port
is missing is to re-read both files side by side.

So: **one row per import, naming the source commit, the date, and what was changed at this
end.** When Case Zero revises an imported fix, `git log <commit>..HEAD -- <file>` in the
sibling repo gives the delta directly.

**The operator asked for this file by name** (2026-08-16): *"include when we implemented it
because there might be an updated version later on."*

Rules that apply to every row:

- **State whether the fix was re-measured HERE or is being taken on the sibling's evidence.**
  Case Zero is a genuine cross-check for anything shared and worthless as an oracle for
  anything it got wrong (`CLAUDE.md`, evidence rules).
- **Translate the instrument prefix.** Case Zero's arms are `CZ_*`; ours are `CW_*`. An
  imported `CZ_` name is a switch that silently does nothing.
- **Keep the control arm.** If the sibling's fix shipped with an off switch, the import ships
  with it too, renamed — otherwise this port cannot show the change did anything.

---

## 1. The UI text defect — the stream store's guard

| | |
|---|---|
| **Imported** | 2026-08-16 (part 2) |
| **Source** | Case Zero `82d181f` — *"part 46: the UI text fix — exactness EARNED per stream instead of bought by size, with a bounded bootstrap probe"*, committed 2026-08-15 |
| **Confirmed in Case Zero by** | `f033efd` — *"THE UI TEXT DEFECT IS FIXED — confirmed in play ('Ui stay good the whole time')"* |
| **Diagnosis commit** | Case Zero `3dea7c1` — *"part 45: THE UI TEXT DEFECT IS THE STREAM STORE'S GUARD"* |
| **Files changed here** | `runtime/gpu/vk_renderer.cpp` only |
| **Control arm** | `CW_VK_NO_DYNAMIC_GUARD=1` (Case Zero's `CZ_VK_NO_DYNAMIC_GUARD`) |
| **Re-measured here?** | **The mechanism engages here and is priced here. The DEFECT ITSELF has not been re-confirmed fixed on this port — that needs an operator play session.** |

### The symptom on this port

Recorded as finding 28 before the fix existed here: HUD objective banner and kill counter
render with dark ghosting behind the glyphs, pause-menu labels come out as overlapping
unreadable strings, **cinematic subtitles unaffected**.

### The mechanism, in one paragraph

The stream store caches vertex/index buffers in device memory and re-uses them when the
guest's bytes have not changed. "Have not changed" is decided by a **guard** — a hash that is
exact up to a byte bound and *sampled* above it. The UI text layer is one big vertex buffer
that the guest sub-allocates every run of glyphs out of, and it sits **above** any affordable
bound, so its changes were sampled and missed: the store served last frame's glyphs.

Raising the bound does not work — Case Zero measured the HUD still dropping out at 256 KB,
where exactness already costs 121+ MB/frame. **Size is the wrong discriminator.** The right
one is *dynamic vs static*: world geometry is written once and read all level; the UI buffer
is rewritten every frame. So a stream **caught changing** is hashed exactly from then on, and
everything else keeps the cheap sampled guard.

That policy has one hole by construction — a stream whose *first* change the sampled guard
misses stays sampled, so the defect survives until the first visible change and then
self-heals. Case Zero's operator hit exactly that (*"UI did break at the start of being in
game but then it seems to be good now"*). It is closed by **inverting the presumption for a
new entry**: hashed exactly for its first `kGuardProbes` (3) observations, demoted once it
proves static — under a **per-frame byte budget** (`kGuardProbeBudget`, 4 MB), without which
the probe cost Case Zero 838 streams and 66.8 MB/frame on its outdoor route.

### What was measured HERE, 2026-08-16

Title-screen run, `CW_VKDRAW=1 CW_VK_PROFILE=5`, arm against its own control:

```
arm  (fix on)              guard PROMOTED to exact: 25-30 streams/frame, 1.0 MB/frame
control (NO_DYNAMIC_GUARD) guard PROMOTED to exact:  0 streams/frame,    0.0 MB/frame
```

**The arm visibly engages and the control visibly disables it** — the gate this port requires
for any imported instrument, because the prefix lives in string literals and a `CZ_`-named
switch would compile and do nothing.

**The cost here is 1.0 MB/frame against Case Zero's ~18 MB/frame.** That is the direction
predicted when the budget was ported: Case Zero's figure is from its **outdoor** route, and
**Case West has no outdoors** — a facility interior meets far less new geometry, so the
bootstrap probe has a much smaller population to pay for. The 4 MB budget is kept at Case
Zero's value because it is a ceiling, not a target.

**Frame time: 31.2 fps / 32.0 ms in BOTH arms at matched draws (~750/frame).** This does
**not** establish the fix is free. 32.0 ms is exactly two vblanks — both arms are sitting on
this title's pacing floor, so the comparison cannot resolve any cost smaller than the
headroom (gotcha 237: *a mean frame time measures the vblank pacing floor, not your change*).
A gameplay session at ~1,265 draws/frame is where a real price would show.

### What is still owed on this import

- **Confirm the defect is actually fixed on this port**, in play, by the operator — the same
  way Case Zero confirmed it. Everything measured here says the *mechanism* runs; nothing
  here says the *HUD text* is right, because that needs eyes on a gameplay session.
- If it is NOT fixed, the useful next reading is whether the promotion count rises when the
  HUD is on screen, and `CW_VK_STREAM_GUARD_EXACT=1` remains the unlimited arm that separates
  "the guard is still wrong" from "it was never the guard here".

### Not imported from the same neighbourhood

Case Zero's working tree also carries **uncommitted part-47 performance work** on this file
(a three-way split of the `record` profiler phase, and a four-lane `GuardFold`). **Not
imported**: it is unfinished, it is performance rather than correctness, and this port has no
performance measurements of its own to justify it yet. Revisit when Case Zero commits it.
