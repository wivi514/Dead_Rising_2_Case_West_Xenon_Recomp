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
| **Re-measured here?** | **YES — confirmed in play by the operator, 2026-08-16: *"Ui seems to work really well this time."*** Mechanism engages, cost priced, defect confirmed fixed. |

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

### CONFIRMED IN PLAY, 2026-08-16 — and the gameplay price

The operator, on the session launched with this build: **"Ui seems to work really well this
time."** That is the same class of evidence Case Zero closed its own part 46 on (*"Ui stay
good the whole time"*), and it is the evidence that matters — the mechanism engaging was
never in doubt once the counter moved; whether the glyphs are right is a thing only eyes
settle.

**The gameplay cost, which the title-screen A/B could not see:**

```
title screen   25-30 streams/frame promoted,  1.0 MB/frame
gameplay      102-149 streams/frame promoted, 12.0-12.7 MB/frame
```

So the honest price of this fix on Case West is **~12 MB/frame of extra hashing in
gameplay**, not the 1.0 the first measurement suggested — a 12x difference, and a good
reminder that a title-screen arm is not a gameplay arm (gotcha 133: one scene is one sample).
It remains below Case Zero's ~18 MB/frame, which is consistent with this title having no
outdoor streaming route.

**What that price buys is not yet separated from what it costs.** Both title-screen arms sat
on the 32.0 ms two-vblank pacing floor, so no frame-time comparison here has resolved the
fix's cost at all. If gameplay frame time ever becomes the question, `CW_VK_NO_DYNAMIC_GUARD=1`
is the same-binary control arm and the A/B must be run **in gameplay at matched draws**, not
at the title screen.

### If it ever regresses

`CW_VK_STREAM_GUARD_EXACT=1` remains the unlimited arm, which separates "the adaptive guard
is still missing something" from "it was never the guard". `CW_VK_NO_DYNAMIC_GUARD=1` returns
to the pre-import policy and should make the defect come back — that is the test that this
fix is the thing holding it closed.

---

## 2. The performance campaign — parts 47-55, wholesale

| | |
|---|---|
| **Imported** | 2026-08-18 (part 4) |
| **Source** | Case Zero `82d181f..444631f` — the whole performance campaign, parts 47 through 55+ (177 commits), taken at their HEAD of 2026-08-18 |
| **Files changed here** | `runtime/gpu/vk_renderer.cpp`, `gpu/pm4.cpp/.h`, `gpu/vd.cpp/.h`, `host/window.cpp/.h`, `kernel/imports.cpp`, `main.cpp`; NEW `cpu/thread_budget.cpp/.h`, `gpu/pump_stats.h`; `CMakeLists.txt` |
| **Method** | per-file `git diff 82d181f..HEAD` patches with the transplant's textual `CZ_`→`CW_` rename applied to the patch itself; applied clean except one merge conflict (the A2M block, resolved to CZ HEAD's form). Residual diff vs CZ HEAD afterwards is only this port's own instruments (`CW_VK_CENSUS_FRAME`, `CW_VK_VS_CENSUS`, the LT/RT synthetic tokens) plus provenance comments — verified by normalising back and diffing. |
| **Re-measured here?** | **YES — operator, 2026-08-18, in play at 2560x1440 internal: "Framerate is about 4x higher and even more and cannot see any regression."** Their gameplay baseline on the old build was **16-24 fps** (often well under the 31 fps the title-screen measurements showed — one scene is one sample, gotcha 133); the new build runs **68-120 fps**, at 4x the pixels. Profiler snapshot during their session: 67.5-68.0 fps, 14.7-14.8 ms/frame at 6,961 draws/frame — with the profiler's own ~4.3 ms/frame bill included. |

### What came across (the headline items, with their sibling parts)

- **Part 47** — texture content guard runs once per frame per entry under its own budget;
  per-fetch sampler lookup and PM4 register writes go flat/bulk; vertex+index bind state
  cache. Their operator A/B: −21.3 ms at matched draws.
- **Part 48** — `getenv`/`snprintf` off the per-draw and per-packet paths (245→119 ns/draw
  in `other`); PM4 census counters per-thread; the A2M mode read becomes a static
  (`CW_VK_A2M_MODE`, default 2).
- **Part 49** — **the frame-rate ladder fix and 60 fps by default**: vblank period 1 ms,
  present interval pinned at the title's own 2, host vsync explicitly off. `CW_FPS_CAP=30`
  restores shipped pacing and is the control arm.
- **Part 50** — type-2 PM4 filler consumed in one call; the profiler prints its own bill.
- **Part 54** — **the Vulkan swapchain is the DEFAULT present path** (MAILBOX; the readback
  and its two full-frame copies no longer run). `CW_VK_NO_SWAPCHAIN=1` is the control arm.
  Swapchain follows window resizes; `CW_WINDOW_SIZE` / `CW_WINDOW_MAXIMIZED` make the window
  a controlled variable. **`CW_VK_RES=2560x1440` / `CW_VK_RES_SCALE=N`** — internal
  resolution as an integer multiple of the title's 1280x720.
- **Part 55** — **one thread budget for the whole runtime, sized from PHYSICAL cores**
  (`cpu/thread_budget.*`; on the operator's 8-core machine: 3 workers granted, machine
  stays usable); the per-frame stream cache, shader table, cross-frame store index and
  texture cache all become flat open-addressed tables; tables pre-sized with every grow
  counted; per-window ALU constant-copy memo; `CW_VK_VRAM_STREAMS=1` arm (geometry in
  device-local memory — an arm, not the default).

### Gates run here

Build + link clean first try; `cw_runtime --smoke` passes; an autonomous DebugJump run
reached in-game (swapchain, thread budget, fps-cap lines all visibly engaged — the rename
gate every imported `CW_` arm must pass); then the operator's own 1440p session, quoted
above. **Not re-measured here**: the individual per-item A/Bs (each has its Case Zero
control arm renamed and kept, so any one of them can be re-litigated on this title if a
regression ever points at it).

### Two defaults changed by this import — what to watch

1. **Pacing**: the title now runs at its 60 fps configuration instead of the 30 fps ladder.
   Case Zero registered and refuted "the cap doubles simulation speed" (their part 49);
   taken on the sibling's evidence — same engine — and nothing in the operator's session
   contradicted it. If sim speed ever looks wrong, `CW_FPS_CAP=30`.
2. **A2M dither** (`CW_VK_A2M_MODE` default 2, per-sample). Case West has no outdoor
   foliage, so the screen-door trade that motivated mode 1 there may never apply here.
   Operator saw no regression.

---

## 3. The non-RT block — parts 56-61, and the progress-widget fix

| | |
|---|---|
| **Imported** | 2026-08-23 (part 5), commit `04f42e3` |
| **Source** | Case Zero `444631f..5b9fbba` — every non-RT commit of parts 56-61 |
| **Method** | per-file `git diff` patches with the `CZ_` -> `CW_` rename applied to the PATCH TEXT (so string-literal arms survive); one include conflict, resolved by keeping both sides |
| **Why that boundary** | **every non-RT commit precedes the first ray-tracing commit** (`4cc4f4a`), so a single cut takes all the fixes and no RT. The operator ruled RT out (2026-08-23); there was no RT code to strip because none was ever brought across |
| **Re-measured here?** | **YES — the progress-widget defect is CLOSED and the mechanism named by A/B.** See finding 60 |

### What it fixed, and the arm that proves it

**The PP bar and the mission progress bar render.** The mechanism is `cf62229`, the
small-packed-texture read: a texture whose shorter dimension is <= 16 texels packs its
whole chain into one tile with `mipAddr = 0`, and level 0 was being read at the tile
origin instead of its packed offset. The bar strips are 32x1.

```
default                   both bars RENDER
CW_VK_NO_PACKED_SMALL=1   both VANISH (the pre-import picture)
```

Also in this block: the **stencil test** (never honoured before — ~18% of a gameplay
frame enables it), **front face = CW** as the default, guest **polygon offset**, **user
clip planes**, **aspect-correct presentation** (black bars instead of stretch),
**deferred image retirement**, and the F8 burst instrument.

### The Visuals menu

`host/settings.{h,cpp}` and the panel layout in `host/window.cpp` came across unmodified.
Case Zero's guest-side half (`cpu/pc_options.cpp`, 959 lines) did **not**: this port
writes its own `cpu/pc_options_cw.cpp` carrying only the default path (~120 lines of it),
because the other ~840 are the native-screen experiment and hold nearly all of that
file's guest exposure — five hooked functions, three data addresses, and **a hardcoded
`.text` bound that is wrong for this title in the safe direction** (gotcha 3).

The one thing it needs is derived rather than transcribed: the `"OptionsVisual"` name
hash, computed by calling **the title's own** `sub_827815D0` on our own image's string.

**Deliberately not ported:** `cpu/camera_fov.cpp` (100% sibling addresses, including a
link-register value identifying one call site) and `tools/gen_pc_options.py` (the
native-screen arm's repacked asset, which also carries Case Zero's string-id and `.big`
layout assumptions).

### Not imported from this range at all

Everything from `4cc4f4a` onward — Case Zero's parts 62-71 ray-tracing work (RT shadow
route (a) and (b), BLAS/TLAS plumbing, the factor pass, the sun oracle, the occluder
census) and the two settings commits that add RT rungs to the shadow row (`403f6c8`,
`f622955`). Operator's instruction: it does not work there yet. Watch those parts and
import if it closes.

---

## PENDING — defects known to be Case Zero's, waiting on a fix there

**These are NOT to be investigated in this port.** They were reported here by the operator
(2026-08-16, the Case 1-3 session, finding 33) as pre-existing and already known in the
sibling. Re-deriving them here would duplicate in-flight work and risk landing a different
fix for the same defect. Watch the named item, import when it closes, add a row above.

| defect here | Case Zero item | state there (checked 2026-08-16) | what to watch |
|---|---|---|---|
| **Decals not rendering properly** — minor visual, operator says fix later | **00m** in `docs/open-items.md` | Reported at their part 47, **explicitly NOT investigated**, no captures requested. Theirs to characterise once the performance work lands. | any commit touching decals / the decal draw pass |
| ~~**Performance**~~ | ~~**00l**, parts 47-48~~ | **IMPORTED 2026-08-18 — row 2 above.** The campaign ran to their part 55+ and came across wholesale. | further perf commits there; `git log 444631f..HEAD -- runtime/` in the sibling gives the delta |

Their likely handle on decals, recorded so it is not re-derived: decals are a separate draw
pass with their own blend state, so a draw census plus draw-ID on a frame containing one
should name the draws in a single capture — and the title screen / menu backdrop is worth
checking first for a self-servable repro (gotcha 319).

**Note for this port specifically:** gotcha 319 matters more here than in Case Zero now.
Past Case 1-3 this port has **no Xenia ground truth at all** (finding 33), so a defect that
reproduces early — on the title screen, in the menus — is worth far more than the same defect
found in late content, where nothing can adjudicate it.

### Not imported from the same neighbourhood

Case Zero's working tree also carries **uncommitted part-47 performance work** on this file
(a three-way split of the `record` profiler phase, and a four-lane `GuardFold`). **Not
imported**: it is unfinished, it is performance rather than correctness, and this port has no
performance measurements of its own to justify it yet. Revisit when Case Zero commits it.
