# Part 6 kickoff — the live hand-off

**Written at the end of part 5, extended 2026-08-27. THIS IS THE LIVE ONE.**
`part4-kickoff.md` is superseded (its one active task is closed — see §1);
`part3-kickoff.md` before it likewise; `part2-kickoff.md` is kept only as the cautionary
example, because its problem statement was false and part 2 refuted it with the very
measurement that section asked for.

Read this, then `docs/xenia-capture-analysis.md` — the numbered findings ledger and the
authority on any measured number. `docs/imported-fixes.md` tracks everything taken from
Case Zero, with the source commit and date for each.

**Parts 4 and 5 ran in one conversation.** Part 4 was the meter investigation and the first
performance import; part 5 closed the widget defect, landed the Visuals menu, and then took
the sibling's newer performance work as well.

---

## 1. THERE IS NO ACTIVE DEFECT. This part starts from a backlog.

**The port's one defect of its own is CLOSED** (finding 60, 2026-08-23). The operator's
words for what comes next: *"I'll make you add more stuff in a fresh conversation."* So
part 6 has no inherited problem statement — pick from §4, or take whatever the operator
brings.

---

## 2. WHAT ALREADY EXISTS — do not rebuild any of this

This is the section that pays for itself. Everything below is in the tree, committed, and
verified at the date given.

### The port plays, fast, at high resolution

```
Case 1-3 complete, new areas, zombie combat, cinematics, save/load   (finding 33, part 2)
16-24 fps @720p  ->  68-120 fps @2560x1440 internal, no regression   (part 4, f1fdeaa)
then 215-252 fps at ~1,350 draws WITH the profiler on                (part 5, 03dc55e)
```

**Two** of the sibling's performance campaigns are imported: their parts 47-55 (part 4) and
their parts 72-81 (part 5). The second brought a persisted `VkPipelineCache`, texture image
suballocation (one `vkAllocateMemory` per texture had been 71% of the decode), batched
texture-upload submits, image-barrier masks derived from layouts, a stream store that starts
at its ceiling so growth cannot hitch, a GPU-side pass breakdown, and **per-entry durations
in the synthetic input sequence** (`NAME@MS` — `A@300`, `NONE@2000`), which finally makes a
recipe able to reproduce a human rather than a metronome.

Defaults that changed, each with its control arm:

| default now | control arm |
|---|---|
| Vulkan swapchain present path (MAILBOX) | `CW_VK_NO_SWAPCHAIN=1` |
| the title's own 60 fps pacing | `CW_FPS_CAP=30` |
| thread budget sized from PHYSICAL cores | — (reports itself at boot) |
| internal resolution arm | `CW_VK_RES=2560x1440`, `CW_VK_RES_SCALE=N` |

### The progress widgets render (part 5)

Findings 35-59 close with **finding 60**. The mechanism is Case Zero's small-packed-texture
read: a texture whose shorter dimension is ≤ 16 texels packs its whole chain into one
32x32-block tile with `mipAddr=0`, and level 0 was being read at the tile ORIGIN. The bar
strips are **32x1**.

**`CW_VK_NO_PACKED_SMALL=1` is the regression test** — it makes both bars vanish again.

**Finding 59 is RETRACTED in place.** It named `vs_667b04293a65b5ca` as the widget geometry;
that class is still ZERO in our in-game censuses *while the bars render correctly*. Correct
counters, wrong story — the fourth time on this port (gotcha 322). **A draw class present on
hardware and absent here is a DIFFERENCE, not a CAUSE, until an arm ties it to pixels.**

### The Visuals menu (finding 61)

The title's own **Options → Visuals** opens a host-drawn panel; the hub stays alive
underneath and gets its input back on close (B).

```
RESOLUTION · DISPLAY MODE · VSYNC · SHADOW QUALITY · FRAME CAP · FIELD OF VIEW
SHADOW QUALITY = LOW / MEDIUM / HIGH
```

- `runtime/host/settings.{h,cpp}` — the persistent store, imported unmodified.
- `runtime/cpu/pc_options_cw.cpp` — **ours, not Case Zero's.** Only their DEFAULT path
  (~120 of their 959 lines); the rest is their native-screen experiment and carries nearly
  all of that file's guest exposure, including a `.text` bound wrong for this title.
- The `"OptionsVisual"` hash is **computed** by calling the title's own `sub_827815D0` on
  our own image's string at `0x8206D900` — nothing transcribed. It is `21C38544`.
- Arms: `CW_NO_PC_OPTIONS=1` restores the shipped screen; `CW_TEST_PANEL=1` opens the panel
  at boot for display-only questions.

### Also imported in part 5 and not to be re-derived

The stencil test (**never honoured before**; ~18% of a gameplay frame enables it), front
face = CW as the default, guest polygon offset, user clip planes, aspect-correct
presentation with black bars, deferred image retirement, the F8 burst instrument.

### DebugJump works, and the recipe is autonomous

`runtime/cpu/debug_tunables_cw.cpp` — Case West's own, re-derived by fingerprint matching
(finding 57). It reaches in-game with the HUD up, no human. **The timings changed after the
performance import; the current ones are in `tools/cw_hud_capture.sh`** — use that script
rather than retyping a press sequence, so two arms differ only by environment and never by
scene.

### Standing gates — state as of 2026-08-27

| gate | state |
|---|---|
| `cw_runtime --smoke` | ✅ passes, 58,695 symbols |
| `find_unlowered_switches.py` | ✅ **0 defects** (1,055 `bctr`, 205 lowered, 2 benign thunks) |
| Shader cache / dim census | ✅ 443 shaders, 0 disagreements |
| `find_dropped_branches.py` | ⏳ **STILL OWED** — never run on this title (W0). Needs a `ppc/` regeneration cycle between each step; see CLAUDE.md's Commands section |

---

## 3. IMPORTING FROM CASE ZERO — the method, because it changed

Three imports have landed and the fourth needed a different technique; `docs/imported-fixes.md`
has a row per import with its source commit.

**The rename goes on the PATCH TEXT, not the tree** — `sed 's/CZ_/CW_/g'` over the diff, so
string-literal arms (`getenv("CW_...")`) come across as arms and not as dead names.

**Up to part 5 a range cut was enough.** Their commits happened to be ordered so a single
`git diff A..B` took everything wanted and nothing else.

**From part 6 that stops working, and the answer is a three-way merge.** Their newer work is
written on top of subsystems this port does not carry, and calls into them from dozens of
scattered sites, so neither sequential cherry-picking (fails at the first commit) nor one
flattened diff (73 conflicts) survives contact. What does work:

```
base   = the sibling file at OUR last import point, renamed
theirs = the sibling file at HEAD, renamed, with the unported subsystem replaced by a stub
ours   = our current file
git merge-file ours base theirs
```

That produced **one conflict in ~26,000 lines**, and it was additive. **Keep the seam, drop
the feature**: an unported subsystem stays as a namespace of inert stubs — hard-false gates,
no-op collectors, census globals at zero, and a print that says out loud that nothing was
collected. Deleting its call sites instead would fork this file from the sibling permanently
and make every future import a hand-merge.

## 4. THE BACKLOG, roughly by value

1. **Whatever the operator brings.** They said they would.
2. **Decals** — the last known visual defect, and it is **Case Zero's** (their open item
   00m). They have still not investigated it. If it is ever led from here, gotcha 319
   applies with force: past Case 1-3 this port has **no Xenia ground truth at all**
   (finding 33), so a defect that reproduces on the title screen or in the menus is worth
   far more than the same defect found in late content.
3. **The game-side FOV** — the Visuals row currently stores its value and the renderer's
   projection patch applies it, so the world renders wide but does not CULL wide. Case
   Zero's `cpu/camera_fov.cpp` closes that, and is 100% their addresses (including a
   link-register value naming one call site). **The re-derivation recipe transfers even
   though the addresses do not** — it is in that file's own header: a property-name trace
   to find the FOV properties, then a `(lr, value)` census to identify the single roaming-
   camera call site.
4. **`find_dropped_branches.py`** — the owed gate above. It is the only thing that catches
   the coverage oracle's loop-header splits (gotcha 28).
5. **Eight unconsumed B4 capture frames** — the only capture data from round 1 never read.
6. **The F3/DebugEnter boot-logo crash** — parked by the operator ("do not care about the
   crash"), a null vcall from opening a screen on a half-booted game.
7. **Native Visuals screen / `gen_pc_options.py`** — deliberately not ported. Would need
   this title's own `.big` layout and string-id headroom re-measured (Case Zero's first
   pick collided).

---

## 5. HOW THIS PORT KEEPS GETTING THINGS WRONG

Five of this port's claims have fallen to the same shape, and finding 59 is the sixth:
**an absence is a fact about what was looked at.** A boot-path zero, a filename read as a
call site, a listing truncated by vertex count, a missing capture read as a missing area, a
headless run read as a stalled runtime — and now a draw class absent from our frames read as
the cause of a defect it had nothing to do with.

What has worked every single time is **an arm that can turn the effect back on**. Finding 60
was settled in one A/B (`CW_VK_NO_PACKED_SMALL=1`) after five parts of censuses could not
settle it. Before reporting a mechanism, ask what would make it stop.

Gotchas 3, 13, 25, 30, 109, 133, 172/268, 322 and 323 are the ones that bite here; the full
ledger is `docs/gotchas.md`.
