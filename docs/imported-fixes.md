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

## 4. The post-RT performance work — parts 72-81, with ray tracing stubbed inert

| | |
|---|---|
| **Imported** | 2026-08-27 (part 5), commit `03dc55e` |
| **Source** | Case Zero `ef52c7b..HEAD` — 56 runtime commits, everything after their *"PARK the RT shadow rows, and repoint the project at performance"* |
| **Re-measured here?** | **YES** — 215-252 fps at ~1,350 draws/frame **with the profiler running**, `rt 0.0%`, HUD and progress bars unchanged, smoke gate passes |

### Why this one needed a different technique

Parts 1-3 of this table were range cuts: `git diff A..B`, rename the patch text, apply.
**That stopped working here.** This work is written on top of the ray tracing of their parts
62-71, which this port deliberately does not carry, and it calls into it from ~57 sites
across the draw path, the frame roll and the exit census.

```
sequential cherry-pick   FAILS AT COMMIT 1 (it folds the RT-era per-draw hooks)
one flattened diff       73 conflicts
three-way merge          1 conflict in ~26,000 lines, and it was additive
```

The merge that works:

```
base   = sibling@<our last import point>, renamed CZ_ -> CW_
theirs = sibling@HEAD,  renamed, with the unported namespaces replaced by stubs
ours   = our current file
git merge-file ours base theirs
```

### The stubs — keep the seam, drop the feature

```
namespace rtshadow   3,396 lines -> 156      namespace rtfactor   953 lines -> 21
```

Hard-false gates (`Active`, `RouteB`, `MenuOffersRt`), `TierThisFrame()` = 0, no-op
collectors, census globals left at zero so the exit report prints honest zeros, and
`PrintCollectorCensus` says out loud that nothing was collected — a silent census and a
census that found zero are different claims (gotcha 25). `R->rtEnabled` is hard false, so
the device stops requesting ray-query extensions no code uses; the capability **probe**
still runs and still reports.

**Deleting the call sites instead would fork this file from the sibling permanently** and
make every future import a hand-merge. The stubs' headers say exactly what un-parking needs.

### What came with it

A persisted `VkPipelineCache` (this renderer never had one); **texture image
suballocation** — one `vkAllocateMemory` per texture was **71%** of the texture decode;
batched texture-upload submits (2,432 submit-and-waits became one per burst); image-barrier
masks derived from their layouts (−11.9% at crowd load in the sibling); the stream store
starting **at its ceiling** so growth cannot hitch; a first GPU-side pass breakdown
(`CW_VK_GPU_PASSES`); a per-frame CPU/GPU profiler; and **per-entry durations in the
synthetic input sequence** (`NAME@MS`, e.g. `A@300`, `NONE@2000`) — which is what lets a
recipe reproduce a human instead of a metronome.

### The Visuals panel's shadow row

The merge pulled in their six-rung version (our base predates it, so it read as an
addition). **Cut back to LOW / MEDIUM / HIGH**, with the three RT footer reasons dropped —
operator's instruction, and the configuration Case Zero itself ships since their part 71.

---

## 5. The parts 83-93 campaign: performance, real MSAA, native keyboard/mouse, the release infrastructure

| | |
|---|---|
| **Imported** | 2026-09-03 (part 8) |
| **Source** | Case Zero `adca819..ecb6775` (their parts 83-93), plus `2bcf396` (the MSAA 2x default flip, committed there the same day) — ~9,300 runtime lines |
| **Re-measured here?** | Mechanisms YES at first boot (see below); wall-time A/B via the soak pipeline, results in this row's addendum |

### What came across, and its state here

* **Their part-89 parallel recorder** ("resolve serial, record parallel": the pump
  deposits fully-resolved DrawCaptures; 512-draw chunks record on the 3 shared
  guard-pool workers into self-contained dynamic-rendering instances; one ordered
  submit). **This SUPERSEDES our part-7 stack** (per-range secondaries + deferred
  replay): their design measured −1.80 ms at their crowd where ours bought +0.35, and
  our own 2c flip already proved worker recording under a range partition is null
  (finding 70) — their capture/record split is exactly what removes the reason it was
  null. `gpu/parallel_record.cpp` is parked out of the build with a header note;
  `CW_VK_NO_PAR_RECORD=1` is the serial control arm. The part-8 reuse census
  (`af8b5d6`, never run) retired with the DrawTicket it hooked — its question was
  answered by their part-87 census (93-94% of crowd draws differ only in ALU
  constants), which is also why the four-cell census plan was cancelled.
* **Deferred scoped clears** (their part 90, WITH the `6c50716` surface-footprint
  fix): clears latch at resolve time and emit into the next pass's first instance.
  ~978k clears deferred in one title-screen boot here; `CW_VK_NO_DEFERRED_CLEAR=1` /
  `CW_VK_DEFER_FULL_RECT=1` are the bisection pair.
* **Write-extent-bounded dynamic constant gather** (their part 88): first boot here
  moved 4.25 GB where full copies were 23.05 GB (**−81.6%**; they measured −84%), and
  our own shader sidecars confirm the target class: 20 VS with dynamic exprs, ALL
  `vc({8,9,10}+a0)`, zero outliers. `CW_VK_NO_BOUNDED_DYNAMIC=1`, verify/poison/fill
  arms as theirs.
* **Projection-patch memo** (their part 88): 99.9% MRU hit rate here at first boot —
  the same figure as theirs. `CW_VK_NO_PATCH_MEMO=1`.
* **Real MSAA — `CW_VK_MSAA`, DEFAULT 2x** (their part 93 + the operator's default
  decision): truly multisampled EDRAM, resolves at RB_COPY, SAMPLE_ZERO depth
  resolve. Engagement verified both ways here (290k colour + 30k depth resolves at
  2x; `=1` boots the single-sample control, announced). Their verdict transfers as
  THEIR measurement: works as game-wide AA, does not fix their hair flicker.
* **Hair-flicker diagnostic arms** `CW_VK_NO_BLEND_DEPTH_WRITE` / `CW_VK_DEPTH_FLOAT`
  (their part 92) — both off by default, imported as the diagnostic kit.
* **Live internal-resolution apply** (their part 91): the Visuals panel's resolution
  row steps a PENDING value, X applies at the frame boundary (the placement that fixed
  their freeze). Ported by hand into our `pc_options_cw.cpp` (our panel is our own
  RE); rows grew to 8 with MOUSE CAMERA / MOUSE SENS.
* **Native keyboard/mouse** (their parts 91-92): all guest addresses RE-DERIVED on
  this image — the record is `docs/native-kbm-import.md`. First boot: verify OK, 93
  bindings resolved 0 bad, 86 spliced (their exact count). Key-cap prompt icons
  generate and serve (`tools/gen_kbm_icons.py`, part 8).
* **KB/M struggle-prompt flash** (their parts 95-96, `c23f155`/`80596ec`/`67a25a3`,
  imported 2026-09-05): the zombie-grab QTE ("push the zombie off") is
  hud_infobar's w_zombie_grapple, a 3-frame stick-wiggle cFEBitmapList — legending
  `analog_move_left`=A, `_right`=D, `_center`=blank makes it flash A↔D under
  keyboard with no runtime code, and id 4049 "LS "→"MASH" + "LEFT STICK "→"A / D
  KEYS " relabel it. All pieces verified present on THIS image before the port
  (glyphs exist, id 4049 == "LS ", "LEFT STICK " unique, .bcs is the {n;ids;offs}
  model with offs[0]==header). Generator gates pass; bank 25/139 patched, under the
  501,900 pin. `tools/gen_kbm_icons.py`.
* **The release infrastructure** (their parts 83-86): in-process shader translator
  (`--translate-shaders`; **byte-identity gate run here: 480/480 .spv identical** to
  the Python-built cache; sidecars regenerated with the new aluConsts fields the
  gather needs), first-boot disc shader prebuild, exe-anchored paths, first-run gate,
  in-process STFS extract, **saves in the OS location** (`~/.local/share/Dead Rising 2
  Case West/`, migration verified here on the operator's real save).


### Addendum — the local wall-time A/B (2026-09-03, preliminary)

`p8_parrec_on` (3 accepted) vs `p8_parrec_off` = `CW_VK_NO_PAR_RECORD=1` (2 accepted),
one binary (`sha=8eb792bfb41bd48e`), both arms at the new MSAA-2x default, verdict via
`cw_trace_band.py` (91,822 frames): **frame-weighted +0.7%, NON-monotone** — the
decisive 6,500-7,000 band reads −4.6% and 5,500-6,000 reads +8.2%, mixed signs =
composition noise, not a real change. The GPU column says why: at the crowd, wall ≈
GPU + ~1.4 ms in BOTH arms — **this title's crowd is GPU-BOUND under MSAA 2x**, so a
CPU-side recorder cannot convert (their part-90 flip warning, arrived here on day
one). The recorder stays default on the sibling's −1.80 ms proof at a CPU-bound
crowd plus its own order-gate correctness; the pre-registered follow-up, if anyone
wants a local CPU-regime price, is the same A/B with `CW_VK_MSAA=1` on BOTH arms.
The chain was stopped at 3v2 (operator wanted the machine); 3v3 completion is a
five-minute errand, not a blocker.

### Not imported

* `debug_tunables.cpp` delta (their level-cap-50 / skill-grant work) — Case Zero
  demo-progression specifics; our `debug_tunables_cw.cpp` is untouched.
* `camera_fov.cpp` + `96c3b95`'s property censuses — per-title RE (backlog item).
* Their uncommitted working-tree work (shadow_distance.{cpp,h},
  patch_char_idmap_hlsl.py) — unfinished there; watch for the commit.
* Release packaging scripts / CI / prewarm seed — their bundle's; revisit at release.

### Method

The proven three-way merge, one step further: base = CZ@adca819 renamed with our RT
stubs swapped in; theirs = CZ@ecb6775 same treatment; ours = our file **with the
part-7 record stack reverted** (its 2,318-line restructure is what their recorder
replaces — merging both would have produced 32 conflicts; without it, ZERO). Our
pre-stack local work (fast-retry backoff, mid-walk rptr, WAITWORLD, all instruments)
survived the merge intact.

---

## PENDING — defects known to be Case Zero's, waiting on a fix there

**These are NOT to be investigated in this port.** They were reported here by the operator
(2026-08-16, the Case 1-3 session, finding 33) as pre-existing and already known in the
sibling. Re-deriving them here would duplicate in-flight work and risk landing a different
fix for the same defect. Watch the named item, import when it closes, add a row above.

| defect here | Case Zero item | state there (checked 2026-08-16) | what to watch |
|---|---|---|---|
| ~~**Decals not rendering properly**~~ | ~~**00m**~~ | **FIXED — operator's statement, 2026-08-30** (part 7's close): the decal defect no longer shows. Mechanism not established here — most plausibly resolved by one of the imported/landed renderer changes since 2026-08-16; if the sibling's 00m closes with a named fix, diff rather than assume. | nothing — closed |
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

## Mouse camera ALWAYS ON (part 9, 2026-09-05) — from Case Zero's release session

**Source**: Case Zero retired its `mouse_cam` toggle ("the mouse camera is always
on now") — settings.cpp's retired-key comment and window.cpp's focus-only `wantRel`.
**Landed here**: operator instruction 2026-09-05, "put the mouse always on from Case
Zero in here."

The mouse camera no longer needs the Visuals MOUSE CAMERA row switched on; capture
follows window **focus** alone (released while a panel wants a visible cursor or
focus leaves — a pad player who never moves the mouse feeds zero deltas, so a pad
build is unchanged). Changes, mirroring Case Zero:

* `runtime/host/window.cpp` — `wantRel` drops `Settings_MouseCam()`; the Visuals
  panel loses its MOUSE CAMERA row (8 rows → 7, panel height 460 → 420); the
  keyboard-help line and the part-91 comment updated.
* `runtime/cpu/pc_options_cw.cpp` — the host-panel input handler drops the MOUSE
  CAMERA case (case 6), MOUSE SENS becomes case 6, nav modulo 8 → 7.
* `runtime/host/settings.{cpp,h}` — `mouse_cam` is a retired key (an old settings
  file carrying it parses as an ignored unknown); the `mouseCam` field,
  `Settings_MouseCam`/`Settings_SetMouseCam`, and the save-line are gone.
* `runtime/cpu/native_kbm.cpp` — the direct-camera hook (`sub_82470DC0`) gates on
  `NativeKbm_Active() && MouseDeviceActive()` alone, no longer on the toggle.

The MOUSE SENS row and `mouse_sens` persistence are unchanged. No env-var control
was added (Case Zero didn't); `CW_NO_NATIVE_KBM=1` still disables the whole KB/M
path including the mouse.

---

## §6 — Case Zero parts 98-101: the pre-release fix round (2026-09-06)

| | |
|---|---|
| **Imported** | 2026-09-06 (part 10, the release part), eight separate commits |
| **Source** | Case Zero `55a9d4e`, `0c50a4d`, `49c895c`, `684bff1`, `aea4292`, `42d558d`, `d78ebf6`, `cc05d05`, `0fbc8db`, `95611b9` — their parts 98-101, i.e. everything behind their v1.0.1 |
| **Why now** | The operator's call: *"Case Zero did a bunch of important fixes we should add to v1.0.0."* Our v1.0.0 artifacts existed but predated all of it |
| **Method** | Per-fix, one commit each, each gated separately. The async-pipeline trio applied as one patch (`git apply`, zero conflicts); everything else hand-ported with the defect **re-measured on this image first** |
| **Re-measured here?** | **YES for every one that could be** — see the per-row evidence below |

### What came across, and what proved it here

| fix | source | the defect here | gate run here |
|---|---|---|---|
| **NtReleaseSemaphore honours `maximum`** | `d78ebf6` | **Present, identical code.** Their unbounded count WAS a boot hang on a second machine (66 M releases against a guest maximum of 0x10). Our `Semaphore::Release` was theirs character for character | Headless boot reaches the title and keeps polling; 0 refusals in normal play; **POISON control** (maximum forced to 1) fires the branch and returns cleanly (gotcha 30). Added beyond theirs: a counted, announced refusal and `CW_NO_SEM_LIMIT=1` as the control arm |
| **EDRAM depth auto-negotiation** | `0fbc8db` | **Present.** AMD does not advertise SAMPLED_IMAGE for D24S8, so a sampled depth resolve is UNDEFINED there. This port has only ever run on NVIDIA — the AMD path is untested here and taken on their evidence, but the query is the device's own answer | RTX 3070 still picks D24_UNORM_S8_UINT, no warning; `CW_VK_DEPTH_FLOAT=1` picks D32F and boots; **POISON control** inverting the sampleable test fires the AMD branch. The snapshot dump is format-aware with it |
| **Worker-budget core floor** | `cc05d05` | **Present.** The bare formula gave a 6-core machine 1 worker and a 4-core 0 | Verified by affinity mask, matching their table: 4c→2 (was 0), 6c→3 (was 1), 8c→3 unchanged |
| **RMB aims with LT, not RT** | `684bff1` | **Present** — the self-firing firearm. **Re-measured on THIS image** (`CW_KBM_TRACE=1 CW_KBM_CMD_CENSUS=1` over a gameplay route, read with the guest's own combiner enum `C_AND=1 C_OR=3`): `RAPID_FIRE_RT = X HELD **OR** R2 HELD`, so RMB's R2 satisfied it continuously | build + `--smoke`; **behaviour owed to the operator** (a mouse button cannot be synthesised headlessly). The 70 ms trigger stagger is deleted with it, which also makes the camera's "hold LT, press RB" instant |
| **Async pipeline creation** (+ pre-warm chaining, two-tier FIFO) | `55a9d4e` `0c50a4d` `49c895c` | **Present** — pipelines were created on the frame thread on a miss. Worth MORE here than there: our shipped seed is 134 keys against their 1,365 | Route with it ON: engages, 18.5 M draws, HUD intact; `CW_VK_SYNC_PIPELINE=1` control: 0 async lines; **13 draws of 18.5 M** deferred, honestly counted; validation exactly the standing 6 `topology-08773`. **No frame-time delta claimed** — the two route runs differ 1.8% in draws, above the 1.40% floor, so they are not matched arms |
| **Pre-warm seed unioned, not shadowed** | `95611b9` | **Present.** A session that parks early saved a tiny per-user file that hid the shipped seed forever after | Demonstrated rather than argued: a boot printed `128 per-user + shipped seed -> 134 keys after union`, and that same short run then saved a **32-key** file — exactly what would have shadowed the seed under the old code |
| **EXIT GAME quits to desktop** | `aea4292` | **Present** — `XamLoaderLaunchTitle` was an honest-failure stub, so the menu item did nothing. Both exits now share one sequence (dump counters, save the pipeline cache, `_Exit`) | build + `--smoke`; `import_stubs.cpp` REGENERATED (77 stubs, 170 real), not hand-edited. **CONFIRMED IN PLAY 2026-09-06**: the operator's own capture session ended through it — `XamLoaderLaunchTitle(NULL (dashboard), 0x100) — title requested exit; quitting to desktop`, and the pipeline cache was written on the way out (72.8 MB), which is the half of that fix nothing else exercises |
| **MASH in every language bank** | `42d558d` | **Present and BIGGER here**: we ship **eight** banks, not six, and the overlay carried only `str_en.bcs` | Byte-identity gate re-run across all eight outputs: Python reference vs `--gen-overlays`, only the version stamp differs. `kGeneratorVersion` 1→2 |

### Measured differences from the sibling, recorded so nobody assumes

* **Eight string banks, not six** — and two are not what a naive loop expects. `str_id.bcs` is an **identifier** bank (id 4049 reads `IDS_HUD_LS`, a QA aid with no prose), and `str_lg.bcs` has **zero tail slack** (its blob fills the 120,418-byte pin exactly), so MASH's extra byte cannot fit. Both are skipped **by their own evidence** — a value check and a pin check — not by a hardcoded name list. Six banks are written. `fr` ships `LS` without the trailing space, the same quirk they found.
* **`padmap.txt` is EMPTY on this image** (4 bytes, decompresses to 0), so the bindings had to be read from the live records rather than a file — which is what the census instrument is for.

### Not imported from this range, and why

* **`42f99bc` skip-intro-logos** and **`e1da647` subtitle language selection** — launcher FEATURES, not fixes. Both need this title's own data re-measured (`intro.txt` exists in our `fecmn.big`; the language IDs differ with eight banks). Deferred deliberately, not forgotten. **UPDATE 2026-09-09: `e1da647` is now imported — §7 below, with the mapping re-measured here (it did NOT differ: eight banks on disk, but the guest's ID table addresses the same six). `42f99bc` stays deferred.**
* **`082fff5` uncapped mouse-look** — that is Case Zero re-deriving **our** feature on their image, with their addresses. Nothing flows back.
* **`a1bdff6` gas-station rooftop / texture-LOD thumbnail** — a Case Zero LOCATION and a Case Zero asset hash. The mechanism may generalise and may even touch the operator's parked minor-visual list; not hunted, because that list is theirs to open.
* **The part-99/100 kernel probes** (`CZ_APC_TRACE`, `CZ_KOBJ_DUMP`, `CZ_KCALL_WHO` milestones, the streaming listeners) — diagnostics built to find the boot hang whose FIX we took. Worth having if a player reports a hang; not needed to ship.

### Addendum — the AMD machine, and what running there found (2026-09-06)

The operator pointed this session at **czamd** (`192.168.0.60`, Windows 10 Pro,
**Radeon RX 6600**, 6 physical cores) — the machine Case Zero's boot hang and
AMD depth bug were found on. It has **no toolchain at all** (no compiler, CMake,
git or Vulkan SDK), so nothing was built there: a Windows x86-64 binary is
portable, and what that machine uniquely offers is its **GPU**. The gated
`CaseWestRecomp-windows-x86_64.zip` was transferred (hash verified on arrival),
unpacked, given the operator's own package, and run.

**This is the check `release_package_windows.ps1` says it cannot make** — the
first-run flow on a machine with no dev tree — and it passed end to end:

* `[extract] done: 305 files, 1216219768 bytes` — the in-process STFS unpack;
* `[prebuild] 1322 translated, 0 already present, 0 failed` — DXC on AMD;
* overlay generation ran and the VFS served the generated banks (no Python on
  that machine, which is the whole point of the §0 road);
* boot to the title screen, polling input, **75,770 log lines with zero faults,
  zero unsupported packets/formats/imports, zero VK_ERROR, zero semaphore
  refusals**.

**The two fixes that had never run on AMD both fired, on the device's own
answer rather than a poison:**

```
[vk] device: AMD Radeon(TM) Graphics (Vulkan 1.4.315)
[vk] EDRAM depth format: D24_UNORM_S8_UINT is NOT sampleable on this device
     (AMD) - using D32_SFLOAT_S8_UINT so depth resolves are readable
[threads] machine: 6 physical cores, 12 logical cpus -> budget 3 workers
     (reserve 2, committed 3, floor 3@6c/2@4c, cap 6)
```

The depth line is the §6 import validated on the hardware it was written for —
without it this machine would have had undefined depth resolves. The threads
line is the core floor doing its job on a real player-class CPU: **that machine
would have got 1 worker** under the old formula.

**And running there found a defect nothing else would have** — a first boot
announced `saves live in ... (COULD NOT BE CREATED - saving will fail)` while
the directory had in fact been created. One shared `std::error_code`: the
migration block's `is_directory(oldRoot, ec)` overwrites the create result, and
a fresh install never HAS an old save tree, so **every clean first boot claimed
saving was broken**. Fixed in `a3d8eb3`, with the message now reporting the
directory's real state and a negative control proving it can still fire. **Case
Zero has the same bug character for character** — and since they shipped a
release with a genuine Windows save failure, this is precisely the sentence
someone hunting that would grep for.

## §7 — Case Zero part 99: subtitle language from the launcher (2026-09-09, part 11)

| | |
|---|---|
| **Imported** | 2026-09-09 (part 11), commit `c00f5cd` |
| **Source** | Case Zero `e1da647` (their part 99; the companion `42d558d` — MASH in every bank — was already here as §6's last row) |
| **Why now** | Operator: *"Add localization subtitle to the launcher like how case zero did."* |
| **Method** | Hand-ported (four files, same shape as theirs), with the ID→bank mapping **re-derived on this image** before the launcher row was ordered |
| **Re-measured here?** | **YES** — the whole point. §6 had deferred it precisely because the mapping was not known to transfer |

### What it is

Both HLE sites that answer the console language — `ExGetXConfigSetting(3, 9)`
and `XGetLanguage` — hardcoded English (`1`). They now answer from one
`CwLanguage()` helper (`runtime/kernel/imports.cpp`) that reads the new persisted
`language` key in `cw_settings.txt` (the Xbox ID; default 1), with `CW_LANGUAGE=N`
the dev arm that wins over the file. The launcher gained a **SUBTITLES** row
(ENGLISH / FRANCAIS / ITALIANO / ESPANOL / JAPANESE / KOREAN — ASCII, the 5x7
launcher font has neither accents nor CJK; the in-game text is what gets
localized). The settings loader clamps an unshipped ID to English **loudly**.

There is deliberately no in-game Visuals row: A1 shows the title asking setting 9
**once, at boot** (capture line 5861), so a live row would silently not apply.
The launcher runs before the guest and is the only honest home.

### What proved it here

* **The mapping experiment** (the sibling's §1.1, repeated on this image): one
  headless boot per `CW_LANGUAGE=N`, N=1..8, with `CW_FILE_TRACE=1`. Every run
  opened exactly ONE `str_XX.bcs`, always at `NtCreateFile #40`:

  | ID | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
  |---|---|---|---|---|---|---|---|---|
  | bank | en | ja | **en** | fr | es | it | ko | **en** |

  3 (German) and 8 (Chinese) are Xbox IDs with no bank on this disc and fall back
  to English, so the launcher offers six. The image's own suffix table at
  `0x8206CFC4` reads `ko ja lg es it fr en` and the format string `%s/str_%s%s`
  is at `0x820BD107` — recorded, not used: the mapping came from the experiment.
* **The persisted path, not just the env**: `language=4` written into
  `cw_settings.txt` with no env set opened `str_fr.bcs`; `language=3` printed
  `[settings] language=3 is not one the disc ships (1/2/4/5/6/7) — using ENGLISH`
  and opened `str_en.bcs`. The operator's settings file was restored byte-identical
  (sha256 checked) afterwards.
* **The kernel-order gate**: `tools/kernel_call_diff.py` against A1 for the
  `language=4` boot and for a same-binary English control — the two reports are
  identical except for the log filename. The setting changes a VALUE, not the call
  sequence.
* **The overlay interaction**: the ja and fr boots both served their bank from the
  KB-PROMPT overlay (`assets/game_kbm/data/frontend/str_{ja,fr}.bcs`), so a
  non-English player gets the MASH rewrite §6 already put in every bank.

### Measured differences from the sibling

None in the mapping. The disk has **eight** banks here against their six, but
the guest's ID table names the same six languages; `str_id` and `str_lg` are not
reachable through a console-language ID. Same file number (#40) is a coincidence
of a shared boot order, not a claim.

### Found beside it

The launcher header and the debug-menu heading still read **CASE ZERO** — a
sibling string literal the transplant's `CZ_`→`CW_` rename gate never covered
(it checked instruments, not display strings). Fixed in `644c9b7`, its own commit.
Gotcha 325's class: grep transplanted code for the sibling's string literals.

### Still owed

* ~~**The eye pass**~~ — **OPERATOR-CONFIRMED 2026-09-09 ("Yeah it works")**: a
  launcher session picked FRANCAIS, the run opened `str_fr.bcs` and nothing else,
  the setting persisted as `language=4`, zero faults in the log. **Then KOREAN and
  JAPANESE, one launcher session each (same day): "korean sign worked well",
  "Japanese also works"** — each run opened only its own bank, zero faults. The
  CJK glyph question the sibling left open is CLOSED here: `arialko.bcf` /
  `arialutf.bcf` in `data/system/{480,720}/` render them. Three of six languages
  eyeballed; fr/ko/ja cover both font paths (Latin and CJK).
* **Unreleased**: this lands AFTER the v1.0.0 tag. It ships with the next artifact
  build alongside the 688-key pre-warm seed. Neither is in the staged artifacts.
* `42f99bc` (skip-intro-logos) remains deferred — different mechanism (a data
  patch on `intro.txt`), needs its own recon here.

## §8 — Case Zero parts 102-109: the post-release fix round (2026-09-09, part 11)

| | |
|---|---|
| **Imported** | 2026-09-09 (part 11), the commits listed at the end of this section |
| **Source** | Case Zero `95611b9..28de2f8` — 83 commits, their parts 102-109, everything behind their v1.0.2 and the three player-report fixes after it. Runtime delta ~6,000 lines over 56 files |
| **Why now** | Operator: *"Did a lot of fix on Case Zero implement them here."* |
| **Method** | The part-8 three-way merge again: base = CZ@`95611b9` renamed (`CZ_`→`CW_`, `cz_`→`cw_`, `cz-recomp`→`cw-recomp`, the display strings), theirs = CZ@HEAD same treatment, ours = the working tree. `git merge-file` per changed file: **~30 conflicts in ~6,000 lines**, every one at a seam this port keeps deliberately (the RT stubs, the absent golden texture store, the absent skip-intro-logos and shadow-distance features, our own address blocks, our own save-diagnostic fix). Per-title addresses RE-DERIVED here by byte-shape search before any hook was wired |
| **Re-measured here?** | Engagement of every default and every control arm, one headless boot each; the vertex-recipe pass gated by byte identity against the runtime-dumped cache; validation clean at 16:9 and 16:10. **Behaviour owed to the operator** for the input fixes (a pad and a mouse cannot be synthesised headlessly) |

### What came across, and what proved it here

| fix / feature | source | state on this image | gate run here |
|---|---|---|---|
| **Controller vibration** (XamInputSetState → the window thread → `SDL_GameControllerRumble`; change at once, held level refreshed every 250 ms) | `d6a967c` | **Present, identical stub**: our `XamInputSetState_x` logged the motor words and discarded them | Boot with a pad attached: `[host] rumble: change -> SDL_GameControllerRumble(0, 0) = 0 (has-rumble: yes)`. `CW_NO_RUMBLE=1` prints its OFF line. **Felt behaviour owed to the operator** |
| **The rumble tick at 30 Hz of real time** (effect durations are counts of 30-fps frames; at 110 fps every hit was 27 ms) | `e8cad35` | **Present — and the four functions RE-DERIVED here**: the tick `sub_828003D8` (theirs `sub_82805A58`), SetMotor `sub_82801130`, Send `sub_82801160`, StopAll `sub_82801210`, each found ONCE by its instruction bytes and read instruction for instruction; effect table `0x82AF0880`; the pad vtable has one extra slot here (SetMotor vt+0x58, Send vt+0x5C). Chain above the tick: `sub_828008C8 ← sub_82491868 ← sub_824A46A8` | `[rumble] the title's rumble tick (sub_828003D8) runs at 30 Hz of real time`; `CW_RUMBLE_TICK_HZ=0` prints the per-frame control line. `CW_RUMBLE_TRACE=1` is the bounded probe (their 248 GB lesson kept) |
| **Samplers honour the fetch constants' clamp modes** (address modes were REPEAT since their part 41 — a screen-space blur read the far edge; their "light's glow on the opposite side" report) | `3f557d4` | **Present, shared decode** (`SamplerIndexForFetch` keyed on filter/aniso only) — Case Zero's characteristic "experiment deferred on purpose", inherited whole | First boot: 7 distinct samplers, **four of them clamp/clamp** (`#2 #4 #5 #6`), three wrap/wrap — so the title DOES ask for clamping here and was not getting it. `CW_VK_NO_FETCH_CLAMP=1` prints `forced wrap`. Validation: no new VUID |
| **Scene-transform classifier admits the door-transition camera** (unit-row tolerance 0.004 → 0.01; their 21:9 door stretch) | `63913ab` | **Present, shared code** (`SceneXformForm`). The 1.0024 view-row norm is a Case Zero measurement of the same engine's door camera; not re-measured here (no headless door), taken with its arm | `CW_VK_XFORM_STRICT=1` restores 0.004. The F9 census gains `xf= bEff= n0= n1= n3=` per draw and `[fov-composite]` change lines under `CW_VK_FOV_CENSUS` |
| **MSAA as a SETTING** (`msaa=0|2|4` in `cw_settings.txt`, default 2; a row in the panel — starred until relaunch — and in the launcher; `CW_VK_MSAA` still wins) | `1467d7b` | **Ported by hand into `pc_options_cw.cpp`** (our panel file; theirs is `pc_options.cpp`): row 4, panel now **8 rows**, `%8` cycling; launcher row from the merge | `[vk] msaa in cw_settings.txt — EDRAM is MULTISAMPLED at 2x ... [the 2x default]`; with `CW_VK_MSAA=1` the env wins and says so |
| **16:10 resolutions — NARROW MODE** (world vert-plus inside a widened frustum, UI letterboxed at full width, clip planes mirrored; aspect floor 16:9 → 16:10; launcher ladder + 1280x800/1920x1200/2560x1600) | `a084700` | **Renderer + settings halves present.** The GAME-SIDE half (`camera_fov.cpp`, the roaming camera widened by 1/k for culling) is **absent here as it is for 21:9** — backlog item 1, recipe transfers, addresses do not. So 16:10 here has the same culling caveat 21:9 already has | **Engaged and counted** at `CW_VK_RES=1920x1200`: `draw: raw projection letterboxed to 16:10 (narrow) 475450`. The COMPOSITE form's counter is ZERO in this boot and that is not a failure — it is the world's view-projection form, and a headless title-screen run never reaches the world. **The composite half of narrow mode is therefore UNEXERCISED here** and rides on the sibling's evidence until someone plays at 16:10 |
| **A windowed window follows the internal resolution** | `c0ff3d1` | Present (merge) | Not exercised headlessly beyond the boot; the log's window line |
| **Mouse-wheel notch: the release is carried to the tick after its press** (press+release in one level-sampled tick was no press — "two notches per item") | `390f09d` | **Present, shared feed code** | build; `CW_KBM_NO_TAP_SPLIT=1` control. **Owed to the operator** |
| **Minigame face buttons follow the button art** (MINIGAME_Y on Q, A on SPACE, B on E) | `b92880d` | **Present**: `kbm_default_map.h` is the shared DR2-PC keymap; the chips draw the same caps here | build; **owed to the operator** (the grapple QTE) |
| **Q drives the controller's BUTTON_4 source on key edges** (their "tell the survivor to wait here": a padmap record already full of two sources is unreachable from a key line) | `28de2f8` | **Mechanism present** (`LookupName(..."BUTTON_4")`, resolved at run time on our token table). Case West has no survivors to send anywhere, but every padmap line reading Y now works from Q | `[kbm] Q drives the controller's BUTTON_4 source (edges only): token N`; `CW_KBM_NO_KEY_BUTTONS=1` control |
| **The KB/M glyph scan was the busiest thread in the process** (150 s of memchr+memcmp on a core beside the pump in every run since their part 92 — ours since part 8): 64-aligned multi-probe pass, rarest-byte memchr finder, physical arena first, worker at low priority | `bbba9f6` `f08cdf3` | **Present, identical scan.** Every crowd number this port has quoted since part 8 was taken with that sweep running (their gotcha 535 applies here verbatim). The string-bank half of their change (`ScanForStrBank`) does NOT exist here — our struggle-prompt fix went through the string-bank overlay, not a live memory swap — so that function was dropped at the merge | `[kbm] device-follow scan: aligned pass located 26 of 26 glyphs (29 copies) in 63.4 ms` / `END, 0.063 s`. `CW_KBM_SCAN_LEGACY=1` is the old sweep |
| **The Draw Thread's FENCE wait parks on a futex** instead of spinning (their part 107 item 2; woken from the executor's store site) | `c324bfa` | **Present — both functions RE-DERIVED here**: the loop `sub_825B5FB8` (theirs `sub_82845160`) and the body `sub_825B7668` (`sub_8283C6C8`), each found ONCE by byte shape; every device-struct offset (0x2A90/0x2A9C/0x2ABD/0x2A88/0x2B00/0x3460) and the 0x1388 hang check identical; nested helper `sub_825B6DC0` | `[fencewait] the Draw Thread's fence wait PARKS ...`; `CW_FENCE_PARK=0` prints the SPINS line. **Engagement gate PASSED** (`CW_FPS_LOG=5`, title screen, 320-355 fps): per frame `body 2.9-3.4 | parks 2.9-3.4 (woken 1.9-2.4, timeouts 1.0, **MISSED 0.0**, eagain 0.0) | contended 0.0 passthrough 0.0 | stores seen 23.8-30.2, wakes 2.9-3.3` — the sibling's shape (their crowd: ~6 parks, ~2 woken, ~4 timeouts, 0 missed). Zero missed is the one that matters: no park ever slept through a fence that had already passed |
| **Four-core machines get THREE workers** (floor 3@4c+; their operator's instruction for the Ryzen 3 3100 class — the same operator) | `4d9cfc6` | Present (merge; our §6 floor comment updated in place) | `[threads] ... floor 3@4c+` in every thread report |

### The validation verdict, against the old binary run NOW (gotcha 50/51/86)

Not against a remembered number: `runtime/build-release/cw_runtime`, the **pre-import**
binary from 2026-09-06, was re-run today under `CW_VK_VALIDATION=1` beside the new one.
Both report **4 validation errors, all `VUID-VkGraphicsPipelineCreateInfo-topology-08773`**
— the standing baseline §6 recorded. The 1920x1200 (16:10) boot reports the same 4 and
nothing else, which is the gate the sibling's narrow mode asked for: *clean of anything
new*, not clean.

**Stated exactly, because the arm's own reach is part of the result**: the validation layer
slows the boot enough that all three runs were killed by their `timeout` before the exit
counter block, so each covers instance/device bring-up, swapchain and pipeline creation —
where all four errors live — and the frames it reached, not a full session. The engagement
counts quoted in the table above come from the unvalidated boots, which do dump.

| **Async boot pre-warm on 1..4 workers, BELOW_NORMAL priority for the spare tier**; the thread report names every pool (pipeline / translate / audio / xma) | `fcfd4ef` `ab80b87` | Present. Worth more here: our seed is 688 keys | `pipeline pre-warm: 696 of 696 queued to the background worker (async boot warm)`; `pipeline 4 outside the budget (... BELOW_NORMAL priority for the speculative warm ...)`. `CW_VK_SYNC_PREWARM=1` / `CW_NO_LOW_PRIORITY=1` controls |
| **Vertex-shader RECIPES + the first-run VERTEX pass** (102/104 runtime VS are a disc template + 2-32 patched dwords there; the pass reproduces them at first run so the seed can build every pipeline before the first frame — their session-one pop-in) | `0cd57ff` `370d02c` `c43fdc2` | **Re-derived on THIS title's data**: `tools/vs_recipes.py` over our `deadrisingepilogue-vs.big` (145 templates) and our 109 dumped runtime VS → **107 recipes, 2-31 dwords each, the same two engine-synthesised shaders without a template** (`vs_539ea9e0…`, `vs_a4ae7c2b…`), **0 orphans** in our 688-key seed (74 distinct VS, 74 producible). `tools/release/vs_recipes.bin` is ours, 11,276 bytes | **Byte-identity gate**: `--build-shader-cache` into a scratch dir → `107 runtime vertex shaders reproduced, 0 refused`, and all **107 `.spv` byte-identical** to the cache translated from the runtime dumps (389/389 pixel shaders present in both also identical). `CW_NO_VS_RECIPES=1` is the pixel-only control |
| **The log file** (`cw_runtime.log` beside the data root, a descriptor-level stderr tee drained on every exit path incl. the crash reporter; text mode on Windows) and **`cw_runtime --diag`** | `5b56039` `31e035c` `88fca99` | Present (new `host/log_file.{h,cpp}`; the packaging scripts strip the logs from the stage) | `--diag` exit 0, 69 lines: glibc 2.43, Wayland, both displays, the RTX 3070 with the required-feature table, D24S8 sampleable, 2x MSAA; `cw_diag.txt` written. Every boot: `[log] writing a copy of this output to .../cw_runtime.log`. `CW_NO_LOG_FILE=1` control. Both files gitignored |
| **Prefer SDL's Wayland driver** when the session offers one (their Wayland+NVIDIA XWayland path presented at exactly 1 fps) | `79ef1b7` | Present | `[diag] sdl: video-driver hint: wayland,x11 / video driver that took: wayland`; `[host] window ... on SDL video driver 'wayland'` |
| **Window title = the game's name + fps; the window wears the title's own dashboard tile** (`X_IMAGEID_GAME.PNG`, decoded by a new self-contained PNG reader; never shipped) | `482b47f` | Present (new `host/png_icon.{h,cpp}`); the file exists in our unpacked package too | `[icon] window icon: .../X_IMAGEID_GAME.PNG (64x64)`; on Wayland SDL2 takes no icon and the log says so |
| **The cross-frame stream store gets a DEVICE-LOCAL MIRROR** (their part 106: the crowd's device frame at 1080p was ~4.5 ms of vertex/index fetch over PCIe; 8.84 → 4.0 ms there) | `544ccf2` | Present, ON by default | `stream store MIRROR: 1024 MB of the 1024 MB store twinned in video memory`; exit: `persist hits bound the MIRROR 100.0% of the time (5,774,480 dev, 0 host)`. `CW_VK_NO_STORE_MIRROR=1` control. **No frame-time claim here** — not measured on this box this part |
| **GPU-decomposition instruments** (`CW_VK_GPU_STATS`, `CW_VK_NULL_PS`, `CW_VK_SCISSOR_1PX`, `CW_VK_TRI1`, `CW_VK_VRAM_STORE`) **and the pass-extent census fix** (blind since their part 89 under parallel record — ours too, since part 8) | `ff286b9` `e4a59e8` | Present (`gpu/null_ps_spv.h`, `tools/null_ps.hlsl`, `tools/gen_null_ps_shader.sh`, `tools/gpu_split_window.py`) | build; not exercised |
| **Audio packets carry a timestamp** (no more `Could not update timestamps` 30x/s); the audio threads named `cw-xma-decode` / `cw-audio-pump` | `c21f6e6` | Present (the thread names were a sibling literal in the merge — `cz-` — fixed by hand) | 0 `Could not update timestamps` lines in the boot log |
| **AppImage data root** (`$APPIMAGE` beside the file, guarded by the exe being inside `$APPDIR`), the **old-base Linux build** (Ubuntu 22.04 / glibc 2.35 in a podman container, SDL2 + LGPL ffmpeg with nasm built inside), **`release_package_appimage.sh`**, the icon | `7d1f279` `f610d6c` `c7ee332` | Scripts present and renamed; `tools/release/icon/cw_runtime.png` drawn by `make_icon.py` with the letters CW (our art, no Capcom byte). The `cz-oldbase:jammy` image already on this box is byte-identical in Containerfile after the rename; `podman tag` reuses it | **The old-base build itself is NOT run in this part** — it is the next artifact build's first step (`docs/release-notes-v1.0.1.md`) |
| **Issue templates** (bug report, Steam Deck) | `9e60979` | Present, renamed | — |
| **`--diag`'s depth-format decision without side effects** (`PickEdramDepthFormat`) | `5b56039` | Present — the §6 AMD row's logic, factored | `--diag` names the format it would pick |

### Measured differences from the sibling, recorded so nobody assumes

* **Every guest address differs and every one was re-derived**, none by arithmetic: the
  sibling's instruction sequences were searched for as bytes in `default_image.bin`
  (each found exactly once), then disassembled and compared line by line. The rumble
  pad's vtable has one more slot here than there. `tools/guest_callers.py` gave the call
  chains. The provenance is in each file's header.
* **107 vertex recipes here against their 102** — this title has 145 disc templates to
  their 142 and 109 runtime vertex shaders to their 104; the two template-less
  engine-synthesised shaders are the SAME two hashes on both titles.
* **Seven distinct samplers at the title screen, four clamping** — the clamp census
  is this image's own, and it is the evidence that the sampler change is not inert here.

### Not imported from this range, and why

* **The golden texture store's part-102/104 work** (background writer, `golden.pack`,
  XDG dir) — refinements of a store this port never took (§6: a Case Zero location and a
  Case Zero asset hash). Every hunk touching it was resolved to OURS and the leaked
  references removed; the decode-split profiler keeps a zeroed `golden` column so the
  next merge stays three-way.
* **`camera_fov.cpp`'s 16:10 line** — the file does not exist here (backlog item 1).
* **The RT blobs** (`rt_factor_spv.h`, `rt_shadow_spv.h`) — the stubs stay stubs.
* **Skip-intro-logos** (`boot_skip.cpp`, the settings key, the launcher row) — still
  deferred (§6); every merge hunk carrying it was dropped.
* **`tools/czamd/*.ps1`** — their czamd campaign scripts with `C:\Users\lisab\cz` paths.
* **Their `prewarm.keys`** — ours is the 688-key harvest from this title.
* **Their part 102-108 docs** (`part102-no-popin-plan.md`, `perf-plan-part106/107.md`,
  `picture-plan-part109.md`, `steam-deck-plan.md`, gotchas 514-542) — referenced, not
  copied; `docs/gotchas.md` here continues at 326.

### Found beside it

* The merge introduced **three sibling string literals** that the rename sed could not
  see — `deadrisingprologue-vs.big` (twice in `main.cpp`, once in `shader_prebuild.h`),
  the `cz-xma-decode`/`cz-audio-pump` thread names, `/var/tmp/cz-ffmpeg-build` — and
  `make_icon.py` drew "CZ". All fixed by a grep for `cz\b|CZ_|prologue|DR2CZ` over the
  merged tree BEFORE the first build (gotcha 325's rule, applied at merge time).
* `main.cpp`'s first line still said "Case Zero" from the original transplant. Fixed.

### The performance audit the operator asked for (2026-09-09)

*"You got all the performance improvement like mirror for crowd?"* — so every
performance-bearing commit in the range was checked individually against this tree by a
distinctive marker rather than assumed from the merge. **15 of 15 present:**

| item | marker checked | present |
|---|---|---|
| stream-store device-local mirror (`544ccf2`) | `NO_STORE_MIRROR`, `vkCmdCopyBuffer` at the frame top, `TRANSFER_SRC` on the persist usage | yes |
| `CW_VK_TRI1` / `CW_VK_VRAM_STORE` arms (`e4a59e8`) | both env names | yes |
| `CW_VK_GPU_STATS` / `NULL_PS` / `SCISSOR_1PX` (`ff286b9`) | all three env names | yes |
| the pass-extent census fix, blind since part 8 | reads `scissor.extent`, NOT `R->bound.scissor` | yes |
| fence wait parked (`c324bfa`) | `CW_FENCE_PARK` in `fence_wait.cpp` | yes |
| glyph scan finder + aligned pass (`bbba9f6`, `f08cdf3`) | `CW_KBM_SCAN_LEGACY`, `ScanAligned64` | yes |
| three workers at four cores (`4d9cfc6`) | `floor 3@4c` | yes |
| warm workers BELOW_NORMAL (`ab80b87`) | `ThreadBudget_SetLowPriority` | yes |
| async boot pre-warm (`fcfd4ef`) | `CW_VK_SYNC_PREWARM` | yes |
| first-run vertex pass (`370d02c`) | `CW_NO_VS_RECIPES` | yes |
| MSAA as a setting (`1467d7b`) | `Settings_Msaa` | yes |
| the clang-15 structured-binding fix (`c7ee332`) | `snapBinding` — the old-base build needs it | yes |

**The first version of that audit printed MISSING on all fifteen.** The checker was
broken, not the tree — a shell function whose test could never be true. Gotcha 25 at
small scale, caught only because a row it printed MISSING had been read as present ten
minutes earlier. A detector's own output is a claim about the detector first.

The only performance work NOT taken is the golden texture store's (`200f5b9`, `1501bb0`,
`ff48698`, `5871178`): a store this port never had, so its background writer, its pack
file and its cache-dir fix have nothing here to improve.

### The crowd A/B could NOT be run, and the reason is not the import

The mirror is ON and BINDING — `persist hits bound the MIRROR 100.0% of the time
(5,774,480 dev, 0 host)` — but **no frame-time number is claimed here**, because the
replay route no longer reaches the crowd:

| arm | peak windowed draws med | gate (needs >= 4500) |
|---|---|---|
| new binary, attempt 1 | 930 | REJECTED |
| new binary, attempt 2 | 900 | REJECTED |
| **PRE-IMPORT binary (`build-release`, 2026-09-06), run tonight** | **902** | **REJECTED** |

The control is the old binary run NOW (gotchas 50/51/86), and it fails identically. So
`config/cw_soak_route.seq` is STALE — the recorded press sequence desynchronises in the
frontend and the run never leaves the menus — and it was stale before this import. The
memory note [[debug-jump-recipe]] warns about exactly this class: recorded timings move
under the runtime and a retyped press sequence stops landing.

**What that costs**: the mirror's crowd benefit is taken on the sibling's measurement
(8.84 -> 4.00 ms GPU at 1080p there), not on ours. Re-recording the route needs the
operator to play it once; until then there is no admissible crowd A/B on this box.

### OPEN — an intermittent hang on the EXIT path, seen once, NOT attributed

The first route run did not die at its `timeout`: it printed its exit counter dump and
then sat for 18 minutes until killed. A backtrace of the live process showed the jam:

* one thread inside `write()` from stdio, holding stderr's FILE lock;
* another blocked on that same lock inside `fprintf` (the graphics interrupt pump);
* the guest's own threads blocked behind them.

So everything was queued behind one blocked write to descriptor 2 — which part 11 turned
into a PIPE feeding the new log tee. What argues against the tee being at fault: the
tee's two copies of that run (the console redirect and `cw_runtime.log`) are
**byte-identical and the same length**, so the reader was not behind when it stopped.

**Not reproduced since** — the next three runs, including the pre-import control, all
exited normally. Recorded rather than diagnosed because one occurrence is one sample.
**The cheap next test is a one-variable arm**: run the route with `CW_NO_LOG_FILE=1`
until it either hangs (the tee is innocent) or a hundred runs pass (it is not). A hang
on exit would be player-visible, so this must be settled before v1.0.1 ships.

## §9 — the launcher round, and what the sibling's AMD sittings say about §8 (2026-09-09, part 11)

| | |
|---|---|
| **Imported** | 2026-09-09, commit `28189b5` |
| **Source** | Case Zero `8b67e6a` (21:9 launcher rungs) and `e3981ad` (the pad drives the launcher), both landed after §8's snapshot |
| **Method** | Three-way merge again, base = CZ@`a9e7d95`. ONE conflict: the launcher window's height, because their launcher has ten rows and ours has nine |

* **The pad drives the launcher** — D-pad and left stick move, A selects, START plays
  from any row, B quits, each press becoming the key it stands for so the row behaviour
  is one implementation. Written for a handheld in game mode, where a keyboard-only
  modal window reads as "the game doesn't start".
* **21:9 rungs in the ladder** (2560x1080, 3440x1440, 3840x1600), ordered by height then
  width. **This one matters more here than there**: the operator's own main display is a
  3440x1440 ultrawide, and before this the only way to reach it from the launcher was to
  have the desktop already at that size.
* **AND A DEFECT OF OURS THE SAME FILE REVEALED**: the launcher window has been 720x420
  since it had seven rows. MSAA (§8) and SUBTITLES (§7) made it nine without growing it,
  so the drop-hint footer was drawn at y=416 in a 420-tall window — the line telling a
  new player where to put their game file, clipped. Now sized from the layout's own
  arithmetic (458 for nine rows) with the formula in the comment.

**Gate**: `CW_LAUNCHER_PAD_TEST` — the sibling's own idea, and the reason a machine with
no pad can still gate this. It pushes real SDL controller events through the same cases a
physical pad delivers. Run here: two DOWNs reach RESOLUTION; seven RIGHTs walk
`2560x1440 -> 3440x1440 -> 2560x1600 -> 3840x1600 -> 3840x2160 -> 1280x720 -> 1280x800 ->
1600x900`; `LSDOWN`/`LSUP` move the selection (the stick's edge-trigger); `B` quits, exit 0.

**A hazard, recorded so the next session does not learn it the hard way**: the launcher's
setters persist IMMEDIATELY, so a pad test rewrites the operator's `cw_settings.txt` (this
one walked their resolution to 1600x900). Back it up first and restore it byte-identical
after, sha256 checked — the discipline §7's language experiment used.

### What the sibling's AMD sittings say about §8's performance items

Their `8abdcda` is the closest thing this port has to third-party evidence for the work
§8 imported, and it is worth reading precisely rather than as reassurance. On an **RX
6600 at 1080p with MSAA 2x**, from a cold shader cache, their operator measured a crowd
of 7,861 draws at **70 fps median, p99 19-21 ms, 0.1% of frames above twice the median**,
and confirmed the three part-106/107 items ON AMD: the **stream store in VRAM**, the
**glyph scan at 0.128 s**, and the **fence park with MISSED 0.0** — the same three
engagement facts §8 gates here on NVIDIA, now seen on the other vendor.

**What it is NOT**: a number for this title. It is the sibling's game, their route, their
machine. It raises confidence that the imported code behaves on AMD; it does not stand in
for the crowd A/B this port still cannot run (§8's stale-route note).

### A live AMD lead worth knowing before a player reports it here

Their item 0af (`2b34148`): the **black square on AMD survives v1.0.2 and CLEARS ON
ALT-TAB / Win+PrintScreen**, which points at the **present path** rather than the rendered
image, and their "old driver" theory is refuted by their own log (26.8.1, Vulkan 1.4.315).
This port shares that present path. If an AMD player reports it here, the bisection is
theirs and is already ordered: `CW_VK_NO_SWAPCHAIN=1` first, then the present mode — do
not start from the renderer.

## §10 — the Steam Deck audit, and the glibc floor actually lowered (2026-09-10)

The operator asked whether the sibling's Steam Deck work was done here, naming the glibc
floor. Their plan is `docs/steam-deck-plan.md` §3, seven deliverables. Audited item by
item against this tree:

| their §3 deliverable | here |
|---|---|
| 1. a log file on every platform, plus `--diag` | **in** (§8) — `cw_runtime.log`, `cw_diag.txt`, gated |
| 2. the Wine / lavapipe probes | their investigation, not code; nothing to import |
| 3. REQUIRED features as a checked list, named in the log and `--diag` | **in** (§8) |
| 4. gamescope-awareness for the video-driver hint | **in** (§8) — under gamescope the Wayland hint is NOT set and the log says why |
| 5. the operator's RADV live-USB test | an operator action, not code |
| 6. a Deck test request written for a player | **in** — `.github/ISSUE_TEMPLATE/steam-deck-report.md` (§8) and now `docs/steam-deck-testing.md` |
| 7. a Deck row in the README's requirements and known issues | **in**, 2026-09-10 |

Plus the two things that make a Deck able to run this at all, both already in from §8:
**the glibc floor work** (`tools/release_build_oldbase.sh`, the AppImage script, the
`$APPIMAGE` data root, the clang-15 fix) and **16:10 narrow mode** — the Deck's panel is
**1280x800**, which this port's own resolution rule accepts exactly at its floor
(`w*10 >= h*16` is 12,800 >= 12,800) and which the launcher ladder lists.

### The floor was CODE-complete and ARTIFACT-incomplete, which is not the same thing

Everything above was in the tree after §8, and a player would still have met **glibc
2.43**, because no artifact had ever been built on the old base. That gap is the whole
of what "did you do the glibc thing" was really asking, and it is now closed:

| | before | after |
|---|---|---|
| Linux glibc floor | 2.43 (this machine's) | **2.35** (`libavutil` binds it; everything else 2.34) |
| AppImage | none | `CaseWestRecomp-linux-x86_64.AppImage`, 27,343,352 B |
| ffmpeg x86 assembly | **absent** (no nasm on the dev box) | **present** (the container has nasm) |

Built at source `7d5f42e` by `tools/release_build_oldbase.sh`, which compiles SDL2, the
LGPL ffmpeg and XenonRecomp's static libs inside the container and packages there too —
packaging inside matters, because an `ldd` on the host resolves the bundle's libraries to
Fedora's and would ship this machine's floor straight back. The container image is the
sibling's (`Containerfile` byte-identical after the rename), so `podman tag
cz-oldbase:jammy cw-oldbase:jammy` saved a 1 GB rebuild.

**Gates, and the third is what makes the first two mean anything:**

| gate | verdict |
|---|---|
| clean container AT THE FLOOR (`ubuntu:22.04`), tarball | **GATE PASSED** — full first-run flow, **1,429 shaders, 0 failures**, overlay byte-identical to the Python reference, 261 `.big` archives read, honest refusal with no game |
| clean container AT THE FLOOR, AppImage | **GATE PASSED** |
| clean container BELOW THE FLOOR (`rockylinux:9-minimal`, glibc 2.34) | **REFUSED**, exactly as documented: `GLIBC_2.35 not found (required by libavutil.so.60)` |

A gate that only ever passes has not been shown capable of failing (gotcha 30); the Rocky
9 run is that demonstration and is deliberately NOT counted as a pass.

The AppImage also self-checks three paths of its own: `--appimage-extract-and-run` with no
FUSE, the data root resolving BESIDE the image with `assets/package/` seeded, and the FUSE
mount a double-click takes.

**Still owed for v1.0.1**: the Windows leg on czwin (nothing here can build it), and the
operator's play sitting. Neither is a Deck item. **And no Deck has run this** — every row
above is a cause removed, not a success observed.

## §11 — the pre-release completeness audit (2026-09-10)

The operator's instruction: *"Make sure everything is good from case zero before we
release v1.0.1."* Not a commit sweep this time — commit sweeps are what let the defect
below hide — but four checks that come at it from different directions.

**1. Every file.** Sixteen files exist in the sibling's runtime and not here. All
sixteen are deliberate: `boot_skip` (deferred), `camera_fov` (backlog item 1),
`shadow_distance` (below), `guest_probe`/`debug_tunables`/`d3d_hooks`/`d3d_draw`
(port-pending per-title RE), `pc_options.{cpp,h}` (we have `pc_options_cw`),
`pit_gravel_tex.h` and the four `rt_*` files (a Case Zero asset; ray tracing).

**2. Every arm.** 458 `CW_*` names in their runtime against 412 here. All 46 absent ones
fall in four groups and every group is intentional: ray tracing (40), the golden texture
store (6 — a store this port never took), the parked shadow-distance experiment (2), and
per-title probes including the native-options experiment. **`CW_VK_NO_DECK_SKIP` is not
what it looks like**: it is the gas-station rooftop DECK, a Case Zero location keyed on a
Case Zero shader hash, not the Steam Deck.

**3. Every range boundary.** This port imported the sibling in ranges, and the ranges do
not meet. Each junction checked for runtime commits that fell between:

| junction | verdict |
|---|---|
| the RT era (`5b9fbba..ef52c7b`) | three non-RT commits, all the game-side FOV/culling work — the renderer half is here, the CPU half is `camera_fov.cpp`, backlog item 1 |
| parts 72-81 into `adca819` | that IS §4's own range; the release A.1-A.4 infrastructure is here |
| `ecb6775..55a9d4e` | **THE GAP THAT MATTERED — see below** |
| `55a9d4e..95611b9` | fully accounted for: §6's list, §7's language row, the deferred skip-intro toggle, and the part-99/100 boot-hang probes |

**4. The one that was actually missing.** `74ab694` — the prompt WORDING device-follow —
landed at their part 97, between the range this port took at their part 93 and the range
that resumed at their part 98. Nothing was skipped on purpose; the ranges simply did not
meet there. The defect was live here in identical form: **on a controller, the struggle
prompt said MASH**. Fixed in `db1bab5`, with two divergences their version needed here
(the bank follows the SELECTED LANGUAGE — the operator's own setting is Japanese, so
their hardcoded `str_en.bcs` would have followed nothing — and a located bank must
recognise its own regions, because two languages' id tables agree past 4 KB).

### Checked and deliberately NOT taken

* **`shadow_distance.cpp`** — CLAUDE.md's backlog item 4 said to watch for this landing.
  **It has landed, and it does not work**: their own header calls it "a NON-WORKING
  experiment", two hook targets tried, scaling the globals did not move the shadows,
  operator-confirmed, shipped off and bit-identical. Nine per-title addresses for a
  feature that does nothing. **Backlog item 4 is closed by this, not deferred.**
* **The golden texture store's part-102/104 refinements** — a store this port never had.
* **Skip-intro-logos (`42f99bc`)** — still deferred; a data patch on this title's own
  `intro.txt`, which needs its own recon here.
* **`camera_fov.cpp`** — backlog item 1, and the honest statement is that this is NEW
  per-title reverse engineering, not an import: two hooked functions and several data
  addresses, none of which exist at those addresses here. **Consequence while it is
  absent**: at 21:9 and 16:10 the renderer widens what is DRAWN but the game still CULLS
  to its own 16:9 frustum, so objects at the extreme flanks can pop. v1.0.0 has this too;
  it is a standing limitation, not a regression.
* **The part-99/100 kernel probes** — diagnostics for a boot hang whose FIX is here.

**Nothing else is outstanding.** Every other commit in every range is either imported,
inapplicable by construction, or flowed the other way (three sibling commits are imports
FROM this port).
