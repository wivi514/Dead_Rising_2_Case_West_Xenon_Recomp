# Part 2 kickoff — SUPERSEDED, history only

> **⛔ THIS IS NOT THE LIVE HAND-OFF. `docs/part3-kickoff.md` is.**
> Superseded at the end of part 2 (2026-08-16).
>
> **Keep reading it only for §1 (what already exists) — and even that is now behind
> part 3's own §2.** Its §2 problem statement is FALSE and was refuted by the very
> measurement it asked for; the retraction is in place below. This file is retained
> because it is the project's clearest example of a kickoff stating a hypothesis with
> the confidence of a finding.

**Written at the end of part 1 (2026-08-15).** Part 1 was one long conversation covering
what its documents call "session 1" (bootstrap) and "session 2" (captures + the runtime
transplant); the numbering in those files refers to that split, not to parts. **This is the
first kickoff, and it is the live one.** When part 2 ends, write `part3-kickoff.md` and say
here that this one is superseded.

Read this first, then `docs/xenia-capture-analysis.md` (the numbered findings ledger — the
authority on any measured number). `docs/port-plan.md` is the roadmap; `CLAUDE.md` is the
project guide.

---

## 1. WHAT ALREADY EXISTS — do not rebuild any of this

This is the most valuable section in the file. Everything below is **done, committed and
re-verified at the end of part 1**.

### The recompilation is complete and clean

```
58,448 functions  ·  228 TUs  ·  135 function overrides in config/CaseWest.toml
zero `jump outside function`   ·  zero dropped branches
unlowered-switch gate: exit 0, 0 defects (2 benign frameless thunks)
205 jump tables / 5,791 labels across BOTH code sections (.text and BINK)
```

All three gates re-run clean immediately before this was written. **Do not re-derive the
config.** If you change it, regenerate `ppc/` and re-run all three — the commands are in
`CLAUDE.md`.

### Round 1 captures are COMPLETE and consumed

**13 captures, 17 GB, in `Xenia logs/`** — A1, A2, A3, A4, A5, B1, B1b, B2, C1, C2, and the
W/B4 frame set. All analysed; **findings 1–26** in `docs/xenia-capture-analysis.md`. The
index is `Xenia logs/Xenia_Run_Content.md` (tracked; the captures are not).

**Nothing is outstanding.** Do not write a round-2 request without a specific question —
gotcha 18: *a capture request is a hypothesis with a shelf life.*

### The shader cache is built and translates clean

```
439 distinct shaders  ->  439 translated by XenosRecomp, ZERO failures
shader_dim_census.py: 345 modules 2D / 111 cube, 0 disagreements
```

`assets/shader_spv/` (gitignored, 12 MB). Microcode dumps live in
`~/DR2CW-troubleshooting/ucode-dumps` — **not `/tmp`**, which is a tmpfs. The bank has been
stable across the last four captures.

### The runtime is transplanted, links, and BOOTS

`runtime/`, ~34,500 lines from Case Zero, renamed `CZ_` → `CW_` with a three-way gate
(including a negative control proving the old name does nothing).

```
230 TUs compile, zero errors   ·   cw_runtime --smoke: 58,695 symbols resolved
first boot: 60 s, NO CRASH, 247+13 imports resolved with no warning,
            87 .big archives opened through our VFS, 114 distinct kernel calls,
            3,720 vblanks (~62/s) delivered to the guest's own callback
```

Full record: `docs/w1-transplant-notes.md`. **W1 is complete**, including the save layer —
A3 confirmed `content.cpp` needs no functional change.

**Four modules are parked in `runtime/port-pending/` and must stay parked** until Case
West's own addresses are derived: `guest_probe.cpp`, `debug_tunables.cpp`, `d3d_hooks.cpp`,
`d3d_draw.cpp`. Read that directory's README before touching them — especially the part
about `sub_82475718`, which exists in **both** images and would link silently to an
unrelated function. Two stubs keep the seam and report their own absence.

### Bink is solved and needs no host code

Findings 14, 17, 22, 23 together: the decoder is recompiled guest code (137 `BINK`
functions execute), running on **its own two guest worker threads** — which is what the
four mutants guard — reading input in **252 sequential 128 KB chunks** from inside the STFS
package, writing **three linear `k_8` YUV planes** that **the guest's own pixel shader**
converts. **The host contributes file I/O and nothing else.**

Two requirements that fail silently into a plausible picture: **honour `tiled=0`** (these
planes are linear) and read chroma at its **padded 768 pitch**, not 640.

---

## 2. WHERE TO START — an ordered list, first measurement named

> ### ⛔ RETRACTED IN FULL — there was no problem. See finding 27.
>
> **The premise below is wrong and part 2 refuted it with the very measurement this
> section asked for.** The runtime was never stalled: it renders the Capcom logo and the
> animated Case West title screen, at ~31 fps, and was sitting on **PRESS START waiting for
> input**. The kernel-call order is a set-exact prefix of A5's with **zero** real
> divergences. The `RtlEnterCriticalSection` storm is the title's own idiom — **Xenia does
> 1,465 lock enters per frame; we do 2,567**, on a runtime running at half A5's frame rate.
>
> There was no picture because **the renderer is off unless `CW_VKDRAW=1`**, and the runs
> behind this note were `CW_NO_WINDOW=1` headless ones. Every symptom listed below is a
> correct observation of a healthy title screen.
>
> **Item 3 below ("once it presents: W4, first picture") is therefore already DONE**, and
> its gate passed untouched on the first run: `no translated shader` = 0, the 439-shader
> cache covered the entire frontend.
>
> The lesson is this port's fifth of the same kind and is written out in finding 27:
> *an absence is a fact about what was looked at* — here, about which flag the run had set.
> The cheap half is cheaper still: **ask the oracle whether it shows the symptom too.** One
> `grep -c RtlEnterCriticalSection` on A5 would have retired this before it started.

### ~~The problem to solve~~ (retracted — kept verbatim as the record of the error)

The boot **does not present a frame**. It settles into a loop polling
`XamInputGetState`/`XamInputSetState` while a worker spins hard on
`RtlEnterCriticalSection`/`RtlLeaveCriticalSection` — **6.8 M enters and 8.4 M leaves in
one minute**. Something is waiting for something that never happens.

### **THE FIRST MEASUREMENT: diff our kernel-call order against A1's.**

Not a debugger, not a guess at the lock. This is the method that carried all three earlier
ports and it is now fully supported here, because round 1 delivered the captures it needs.

```
cd runtime/build && CW_NO_WINDOW=1 timeout 120 ./cw_runtime > /tmp/ours.log 2>&1
zcat "../../Xenia logs/A1_boot_menu_fullgame/xenia_A1.log.gz" > /tmp/a1.log
python3 ../../tools/kernel_call_diff.py --xenia /tmp/a1.log --ours /tmp/ours.log
```

`tools/kernel_call_diff.py` was copied from Case Zero and retargeted at the end of part 1
but **has never been run here** — treat its first output with the suspicion any unrun tool
deserves, and read its docstring first (it compares **first-occurrence order**, not the raw
stream, and it explains why). Gotcha 30: confirm it can fail before believing a pass.

**What the answer means.** A divergence in the order the title first touches each kernel
entry point means we steered the guest down a different path, and the first divergence is
the defect — not the twentieth. If the order matches all the way, the spin is not a kernel
sequencing problem and the next probe is the guest side: find which guest function the
spinning thread is in.

### Then, in order

2. **`/tmp` is a 32 GB tmpfs.** `zcat`-ing A5 (909 MB) plus frame dumps will fill it and
   the symptom is the shell appearing to die. `df -h /tmp` before long runs; prefer the
   session scratchpad and delete as you go.
3. **Once it presents: W4, first picture.** `CW_VKDRAW=1` has **never been run** here. The
   439-shader cache exists, so the renderer's hardest input is already in hand. Check
   `grep -c "no translated shader" run.log` — it must be 0, and it is one log line and a
   silent counter otherwise.
4. **Publish NO texcoord swap mask.** Trust the microcode's own destination swizzles. That
   was Case Zero's entire striped-material defect class; start from the corrected side.
5. **W0.3 — the recompiler gaps.** 39 unrecognized-instruction sites over 6 mnemonics
   (`vminsw`, `vpkshss`, `vavgsw`, `stdux`, `vpkshss128`, `stvebx`) plus **20 `float16_4`
   pack sites** clustered at `0x825E6904–0x825E7490`. This is work in
   `~/GithubRepo/XenonRecomp`, **shared with the other three ports** — not a change to make
   unilaterally. An unimplemented instruction is a silent wrong-execution trap, not a build
   failure. Disassemble the `float16_4` cluster with `tools/gdis.py` first; it is probably
   one half-float conversion routine.

---

## 3. GATES — run, and still owed

**Run and passing** (re-verified at the end of part 1):

| gate | state |
|---|---|
| `find_dropped_branches.py` | no dropped branches |
| `find_unlowered_switches.py` | exit 0, 0 defects |
| recompiler log | 0 `jump outside function` |
| `build_shader_spv.sh` | 439/439, zero failures |
| `shader_dim_census.py` | 0 disagreements |
| `cw_runtime --smoke` | 58,695 symbols, all mappings sane |
| `xtr_draw_bindings.py --self-test` | 439/439 hash to their own filenames |
| the `CZ_`→`CW_` rename | three-way, with a negative control |

**Run and passing, added in part 2:**

| gate | state |
|---|---|
| `kernel_call_diff.py --derive-mask` | constant agrees with all 11 captures; fails on purpose both ways |
| `kernel_call_diff.py` vs A5, `--include-high-frequency` | 5 windows, all permutations, **0 real**, exit 0 |
| `grep -c "no translated shader"` with `CW_VKDRAW=1` | **0** — the 439-shader cache covers the whole frontend |
| ring trace `indirect buffers truncated` | **0** |
| the picture itself | Capcom logo, then the animated title screen, correct |

**Still owed:**

- ~~**B1/B1b determinism is UNESTABLISHED**~~ — **DONE, finding 30.** The baseline is
  **1.40%** on the worst aggregate and 0.30% on draws. Also: a frame-INDEXED GPU gate is
  **not viable** on this title (87.9% frame agreement, +2 phase lag on 77% of frames) — gate
  on per-era aggregates.
- ~~**`kernel_call_diff.py` has never been run here**~~ — **DONE, finding 27**, and its
  inherited kHighFrequency mask was wrong and is fixed.
- **B2's 14.8 GiB gameplay stream has never been decoded.** It is the gameplay PM4 ground
  truth, waiting on a renderer to compare against.
- **Eight of the nine B4 frames are unanalysed** — monitor bank, bathroom mirror, monitor
  wall, two StoragePens crowds, lab glass, and the two in-world monitor frames. They are
  the reference set for the renderer's hardest surface classes.

---

## 4. WHAT PART 1 GOT WRONG, so part 2 does not inherit it

Four claims were made and then refuted **by measurement, in the same conversation**. They
are retracted in place wherever they were written, but the pattern matters more than the
list:

1. **"The mutants are co-op"** — refuted by A2. A1's zero calls at boot was evidence about
   *the boot path*, not an attribution to another path.
2. **"The mutants are audio/streaming"** (the replacement guess) — refuted by A5. They are
   **Bink's**.
3. **"`dr2_logo.bik` is on the boot path"** — refuted by A1. Zero `.bik` opened before the
   title screen. A filename is not a call site.
4. **"No video-sized texture in the W1 frame"** — my own tooling error: the listing shows
   the top 12 draws **by vertex count**, and every Bink draw is a 4-vertex fullscreen quad.

**The lesson, and it is the one thing to carry forward:** 1, 2 and 4 were all inferences
from a *count* or a *truncated listing*. What settled the mutants was a thread entry address
falling inside a named section plus 100% containment in a file handle's lifetime — facts
that are **true or false rather than large or small**. When an attribution has been wrong
twice, stop refining the estimate.

---

## 5. OPERATIONAL

- **Commit with plain `git commit`.** Never pass `-c user.email` / `-c user.name` — the
  repo's config already holds `wivi514 <wivi514@hotmail.com>`, the address the GitHub
  account uses. Getting this wrong once in part 1 cost a six-commit `filter-repo` rewrite,
  and after a push the fix is a force-push that breaks every clone.
- **The repo is about to be published.** No remote is configured yet. 1.6 MB tracked; the
  game data, the 17 GB of captures, `ppc/` and `assets/shader_spv/` are all gitignored.
- **Instruments are `CW_*`.** When quoting a Case Zero doc or memory at the operator,
  translate the prefix — otherwise they run a command that silently does nothing.
- **The operator drives.** When a test needs the game played, launch it for them with the
  instruments already wired; do not run it headlessly and do not stop at a recipe. Their
  capture notes have refuted this project's premises four times now and have twice been
  better than what was asked for — **ask for outcomes, not flags.**
- **Xenia captures run on their Windows PC** and land under
  `/mnt/ideapad3/Ideapad3Server/GithubRepo/<repo>/Xenia logs/`. Copy into the repo and
  index in `Xenia_Run_Content.md`.

## 6. THERE IS NO COVERAGE GAP — and how I got that wrong

An earlier draft of this section said "no drive has ever gone outdoors" and called it the
likeliest remaining source of new shaders. **That was wrong, and the operator corrected it:
Case West has no outdoors at all. The entire game is set inside the Phenotrans facility.**

So the 13 captures did not *miss* an area class — they covered the game's area classes,
and the B4 set's "big open receiving/industrial area" is a large interior, which is as close
to an exterior as this title gets. **Round 1's place coverage is complete**, and the shader
bank at 439 — stable across the last four captures — is correspondingly more likely to be
near-complete than "one whole biome is missing" implied.

**The error is worth keeping because it is this port's recurring one.** The B4 notes said
*"NOT captured: a clearly OUTDOORS frame — the drive stayed in Phenotrans interiors"*, and
I read that absence as a gap in the **capture** when it was a fact about the **game**. Same
class as reading A1's zero mutant calls as an attribution, and a filename as a call site:
**an absence is a fact about what was looked at, not about what exists.** When an absence
is about to become a work item, ask whether the thing can exist at all — and here, one
question to the operator settled it in a sentence.

Finding 20's actual lesson still stands and is the useful half: **new geometry is not new
shaders; a new material set is.** Nine world frames across new places added 3 shaders;
C2's one new *zone* added 49.
