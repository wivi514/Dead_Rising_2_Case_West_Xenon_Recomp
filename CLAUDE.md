# Dead Rising 2: Case West Xenon Recomp — project guide

Static recompilation of the Xbox 360 XBLA title **Dead Rising 2: Case West**
(Capcom / Blue Castle Games, 2010) using **XenonRecomp** + **XenosRecomp** (hedge-dev's
faithful recompiler pair, the ones UnleashedRecomp uses).

This is the **fourth** game ported with this pipeline in this workspace, and the first
one whose direct predecessor is a near-complete port of the *same engine*. Read the
three before it before re-deriving anything:

- `~/GithubRepo/Dead_Rising_2_Case_Zero_Xenon_Recomp` — **the one that matters here.**
  Same engine, same studio, same year, same container, same compiler. ~46 parts deep and
  close to complete. Its `CLAUDE.md` is the project journal and its `docs/` are the
  methodology.
- `~/GithubRepo/Fable2XenonRecomp` — the original and the deepest (91k functions → a
  live rendered world). Its `docs/` hold the reusable methodology.
- `~/GithubRepo/Asuras_Wrath_Xenon_Recomp` — the second port, which proved the template
  transfers and consolidated the gotchas into a numbered list.

**Multiplayer / co-op is OUT OF SCOPE for now** (operator's call, 2026-08-15). Case
West's headline feature is two-player co-op and the image is full of it; nothing may
block single-player on it. `docs/port-plan.md` W7 explains why deferring it is
low-risk — the import table says so.

## Status — and where a new conversation starts

> **THE LIVE HAND-OFF IS `docs/part2-kickoff.md`.** Read it first in a new conversation:
> it says what already exists (so it is not rebuilt), names the first measurement, and
> lists the gates that are owed. When a part ends, write the next `part<N>-kickoff.md`,
> demote this pointer to it, and refresh the memory directory.

## Status: part 1 complete (2026-08-15)

Part 1 was one conversation: bootstrap, the full round-1 capture set, and the runtime
transplant. Its documents say "session 1" and "session 2" for the two halves; that split
is internal to part 1.

Day-1 measurements are in **`docs/bootstrap-2026-08-15.md`**; the roadmap is
**`docs/port-plan.md`**; the transplant record is **`docs/w1-transplant-notes.md`**.
**`docs/xenia-capture-analysis.md` is the numbered findings ledger and the authority on
any measured number — where another doc disagrees with it, it wins.**

**Round 1 refuted FOUR claims this project had already written down**, all in part 1 and
all by measurement: the mutants are not co-op (finding 3) and not audio/streaming either —
they are **Bink's** (finding 23); there is **no boot Bink**, a filename read as a call site
(finding 4); and "no video texture in the W1 frame" was my own tooling error, a listing
truncated to the top 12 draws *by vertex count* when every Bink draw is a 4-vertex quad
(finding 18). All are retracted in place wherever they were claimed.

That is the thesis risk in this port arriving on schedule — see "The thesis of this port"
below — and `docs/part2-kickoff.md` §4 draws the common lesson: three of the four were
inferences from a **count** or a **truncated listing**.

| | state |
|---|---|
| Package unpacked | ✅ 305 files, 1.2 GB |
| Image dumped | ✅ 14 sections, **two of them code** |
| Save/restore helpers | ✅ all 8 |
| Jump tables | ✅ 205 tables / 5,791 labels |
| Recompilation | ✅ 58,345 functions, 228 TUs, **zero** `jump outside function`, **zero** dropped branches |
| Import surface | ✅ 247 names = Case Zero's 244 **+ 3** |
| Runtime | ❌ does not exist yet |
| Xenia ground truth | ✅ **round 1 COMPLETE** — 13 captures delivered and consumed; nothing outstanding |
| Coverage oracle | ✅ 104 entry points recovered from C1+C2; config at **135 overrides, 58,448 functions** |
| Shader cache | ✅ **443 shaders, 443 translated, 0 failures**, dim census 0 disagreements — rebuilt 2026-08-16 from OUR OWN runtime's dump (finding 32) |
| Bink | ✅ **SOLVED — the host contributes file I/O and nothing else.** Guest code decodes it on its own 2 threads, a guest shader converts it (findings 14, 17, 22, 23) |
| Runtime (W1) | ✅ **COMPLETE** — links, boots (58,695 symbols; 60 s, 87 `.big`, vblanks delivered), save layer confirmed |
| Kernel-call order | ✅ **set-exact prefix of A5, 0 real divergences** (part 2, finding 27) |
| First picture (W4) | ✅ **RENDERS** — Capcom logo, then the animated title screen at ~31 fps, `CW_VKDRAW=1`. Shader cache covered the whole frontend, 0 misses |
| **Gameplay** | ✅ **IT PLAYS** — 20,765-frame drive: gameplay, cinematics with subtitles, full HUD, pause menu. 26.2 M draws, 0 truncated IBs, only **2** shader-cache misses (finding 28) |
| **Intro → safehouse** | ✅ **playable, with zombie combat**, and "pretty much exactly like Xenia" — **and better in one place: a cutscene Xenia truncates plays through here** (operator, finding 29, gotcha 320) |
| **CASE 1-3 COMPLETE** | ✅ **new areas, no crash, no hang, "didn't have any issue except" two INHERITED defects** — and this is **FURTHER THAN THE OPERATOR EVER DROVE XENIA**, so past here the port has **no capture ground truth at all** (finding 33) |
| Decals | ⏳ minor visual defect — **Case Zero's open item 00m, NOT investigated there or here.** Import when it closes; `docs/imported-fixes.md` |
| Performance | ⏳ **Case Zero's 00l / parts 47-48, actively in progress there.** Not measured here. Import when it lands |
| Save/load | ✅ **behaviourally confirmed** — saved and loaded inside a real session (finding 29). Part 1's confirmation was static only |
| HUD/menu text | ✅ **FIXED** — imported 2026-08-16 from Case Zero `82d181f` (part 46), **confirmed in play by the operator** ("Ui seems to work really well this time"). Costs ~12 MB/frame of extra hashing in gameplay. `docs/imported-fixes.md` |

**~~Part 2's first job is the spin~~ — RETRACTED, and by the measurement it asked for.**
There was no spin to fix. The kernel-call order is a **set-exact prefix of A5's with zero
real divergences**, and the runtime **renders the Capcom logo and the animated title screen
at ~31 fps** — it was sitting on PRESS START waiting for input. The lock storm is the
title's own idiom: **Xenia does 1,465 `RtlEnterCriticalSection` per frame, we do 2,567.**
There was no picture because the renderer is off unless `CW_VKDRAW=1` and the runs behind
that note were headless. **Finding 27**, and `docs/part2-kickoff.md` §2 carries the
retraction in place.

That makes five for five on this port's recurring error — **an absence is a fact about what
was looked at**, here about which flag the run had set — and adds the cheap corollary:
**ask the oracle whether it shows the symptom too.** One grep on A5 would have retired the
whole investigation before it started.

## The thesis of this port, and the risk that comes with it

Session 1 measured that Case West is the **same engine, same compiler, same container,
same asset formats, and a strict superset of Case Zero's kernel imports**. So this is a
**transplant plus a delta**, not a port from scratch.

**The risk is therefore the inverse of Case Zero's.** There, the danger was not knowing
anything. Here it is **assuming a Case Zero answer transfers when it does not** — and
session 1 caught three of those before writing a line of runtime code:

1. **Bink.** Case Zero *retracted* "this game uses Bink" after measuring that no decoder
   ran and no `.bik` shipped. **Case West genuinely uses Bink**: four Bink sections in
   the XEX (one executable), ten `.bik` files, a `binkmovieplayer.cpp` wrapper. Applying
   Case Zero's retraction here would have been exactly backwards.
   **But I then over-corrected**: I wrote that `dr2_logo.bik` plays at boot, purely because
   its name says "logo". Capture A1 opens **zero** `.bik` before the title screen — the boot
   logos are static images. **A filename is not a call site** (finding 4). Bink is real and
   first plays at the New Game intro. C2 then measured **137 `BINK`-section functions
   executing** there (finding 14), and W1 decoded the output surface itself: three linear
   `k_8` YUV planes converted by **the guest's own pixel shader**. **Bink needs no host code
   at all** (finding 17) — the subsystem I called "the one genuinely new thing in this port"
   turned out to be entirely built already.
2. **Two code sections.** Case Zero has one (`.text`). Case West has `.text` *and*
   `BINK`. XenonRecomp handles it; two inherited analysis tools did not, and both
   reported clean runs while skipping a whole section.
3. **Hardcoded sibling constants.** `find_jumptables.py` and
   `fix_switch_function_bounds.py` both carried Case Zero's `.text` bounds as defaults.
   Fixed to read the image's own section map.

That is gotcha 3 in a new dress and it is this port's characteristic failure mode:
**a tool or a conclusion copied from a sibling port carries that port's constants, and a
constant wrong in the safe direction reports a clean run over a smaller image.**

**Session 2 added the second half of that failure mode, from the other direction:** two
claims made here from *this* image — mutants are co-op, Bink is on the boot path — were
refuted by the first captures. Both were inferences dressed as findings. The shared lesson
with the three above is one sentence: **an absence measured on one path is not an
attribution to another, and a name is not a call site.** Both are in
`docs/xenia-capture-analysis.md` as findings 3 and 4, with what produced each error.

## Transferable gotchas

**THE FULL NUMBERED LEDGER IS `docs/gotchas.md` — entries 1–319 copied verbatim from Case
Zero on 2026-08-15, and every "gotcha N" reference resolves there.** New entries from this
port continue at **321**; **320 is the first written here rather than inherited.** (An
earlier version of this paragraph said "315 entries, continue at 316" — the file already ran
to 319. Gotcha 13 applies to this file too.) Read it **before making a measurement claim,
adding an instrument, believing a zero, or trusting a number an earlier session wrote down**;
those four situations produced almost all of it.

Note that entries naming a Case Zero file, env var, address or count are *pointers into
the sibling repo*, not claims about this one.

The ten that bite most often, as one-liners. Each is a summary, not the entry:

**And the sharpest one this port has produced, because it took three tries:**
**when an attribution has been wrong twice, stop refining the estimate.** The Bink mutants
were called co-op (wrong), then audio/streaming (wrong), both from call *counts*. What
settled it was a thread entry address falling inside a named section, plus 100% containment
inside a file handle's lifetime — facts that are true or false rather than large or small.
Findings 3 and 23.

3.  **A zero is a detection failure, not a fact.** XenonAnalyse finds zero jump tables on
    this compiler; our scanner found 205. Applies to every number a detector prints —
    **including the range the detector was pointed at**, which is how session 1's two
    tool defects hid.
5.  **Kernel stubs must fail honestly, never fake success** — and a stub that returns an
    error but leaves its OUT-PARAMETER untouched is worse than no stub. See also 59 and
    201: when a return value is a predicate or a computed value, "fail honestly" has no
    spelling and implementing it is the only correct option.
7.  **A probe expensive enough to stall the game manufactures the stability it reports.**
    Every instrument needs its own control — and see 151, an arm with no counter cannot
    be shown to have engaged, and 223, an instrument on a hot path can cancel the effect
    it is measuring exactly.
13. **A capture request, a plan, and your own status note all have a shelf life.**
    Re-read them against the current ledger before believing their conclusions. **In this
    port that includes every Case Zero document.**
25. **A grep that cannot match is not a clean result.** Check the emitter before
    believing a zero — and 109, a capped or thinned log line is not a count.
30. **A test that has never failed has not been shown capable of failing.** Break the
    implementation on purpose and confirm the test screams. Applies to diagnostics and
    to instruments as much as to tests (94, 158).
50/51/86. **A rate measured once is a fact about that afternoon, and the control is the
    old binary run NOW** — not its remembered numbers. 159: a bimodal arm makes every
    single-run claim a coin flip.
133/127. **One frame of an animated scene is ONE SAMPLE**, and that applies to LOOKING,
    not just to measuring.
172/268. **A retirement is only as good as the ORACLE it was measured on** — and YOUR OWN
    STUB IS AN ORACLE. Re-ask your earlier A/Bs whenever an upstream defect is fixed.
267. **A guest structure handed to a DMA device holds PHYSICAL addresses**, and in a flat
    recompiler map those are not the ones the CPU uses. Cost Case Zero its whole audio
    subsystem for 28 parts. Print the DESTINATION on every file-IO trace.
237/238. **A MEAN frame time measures this title's vblank pacing floor, not your change**
    — read medians and the share of frames pinned to a 16 ms multiple. And **a profiler
    column that falls to zero is not a saving until you find where the replacement work
    got charged.**

## Inherited from the earlier ports: shared-decode cross-checks

Everything below is **hardware-level decode**, not title-specific, so a defect found in
one port is a defect in the others unless this one is written differently. Checking is
minutes; diagnosing the symptom is days. Case Zero verified both and is correct on both —
**re-check them here in one grep once `runtime/gpu/` exists.**

| shared decode | correct form |
|---|---|
| fetch-constant SIZE field — the endian bits occupy the low 2 bits of `fdw1`, so `fdw1 & 0x7FFFFF` swallows them (reads ~4x too large, permits reads past the buffer, and *under*-reports past ~2^21 dwords) | `(fdw1 >> 2) & 0xFFFFFF` |
| `num_format_all` INTEGER semantics — a fetch declaring unsigned/integer on `k_8_8_8_8` bound as `R8G8B8A8_UNORM` delivers 0..1 where the guest asked 0..255 (typically packed bone indices, TEXCOORD-wrapped as 360 titles do) | deliver the integer as its own value into a FLOAT input, via `USCALED`/`SSCALED` |

**Confirmed NON-issues — do not chase these.** The guest requests 8-in-32 endian on 100%
of fetches; `exp_adjust` is declared but zero everywhere; the Xenos compiler emits a
`yxwz`-shaped destination swizzle on ~87% of 16-bit fetches that compensates the 8-in-32
pair transposition, and XenosRecomp already honours it.

**That third one was Case Zero's entire striped-material defect class.** Its
`g_SwappedTexcoords` mask was compensating A SECOND TIME, so 16_16 lightmap UVs arrived
transposed and baked prop shadows painted as hard-edged black blotches. **For Case West:
publish NO texcoord swap mask; trust the microcode's own swizzles.** Start from the
corrected position rather than repeating the fix.

## Layout

- `config/CaseWest.toml` — XenonRecomp main config: helper addresses, plus the function
  overrides. Currently 33 entries — 21 switch-tail repairs plus 11 truncated-function widenings — and it will grow from three sources that
  **merge, never replace** each other (`fix_switch_function_bounds.py`,
  `coverage_to_function_overrides.py`, `find_dropped_branches.py --widen`). Regenerating
  any of them from a stale `ppc/` silently under-reports; always rebuild `ppc/` from the
  committed config first.
- `config/CaseWest_switch_tables.toml` — 205 jump tables (107 absolute, 62 offset8,
  36 offset16, 5,791 labels) from `tools/find_jumptables.py`, **across both code
  sections**. XenonAnalyse finds zero on this compiler — see gotcha 3.
- `assets/package/` — the XBLA STFS package as delivered (gitignored; copyrighted).
- `assets/game/` — what `tools/extract_stfs.py` unpacked out of it: `default.xex` +
  `data/` (gitignored).
- `assets/game/default_image.bin` (+ `.sections`) — the loaded image for offline
  analysis, from `tools/xex_image_dump`. **The `.sections` sidecar is now an input**, not
  just a record: two tools read their code ranges out of it.
- `ppc/` — generated C++ (gitignored; 156 MB, 58,448 functions, regeneratable).
- `assets/shader_spv/` — the SPIR-V cache (gitignored, 12 MB, game-derived): 439 `.spv` +
  439 `.meta.json`, built from the captures' microcode. Rebuild it with the Commands
  section. Microcode dumps live in `~/DR2CW-troubleshooting/ucode-dumps` — **not `/tmp`,
  which is a tmpfs and has silently eaten dumps in the sibling port.**
- `tools/` — analysis scripts, all copied from Case Zero with provenance in their
  headers. `gdis.py` is the guest disassembler and is usually the right first stop for
  any question about what the title's own code does. `import_call_sites.py` is the one to
  reach for when implementing a kernel import: a capture has no return values, so the
  guest code that consumes the result is the specification.
- `docs/` — the project's memory:
  - **`bootstrap-2026-08-15.md`** — day 1. Every number in this file was measured on
    *this* image. Read it first.
  - **`xenia-capture-analysis.md`** — **the numbered findings ledger.** The authority on
    any measured number; read it before believing a number in any other file here.
  - **`part2-kickoff.md`** — **the LIVE hand-off.** Highest-numbered `part<N>-kickoff.md`
    always wins on "where the port is"; older ones are history and go stale fast.
  - **`port-plan.md`** — the roadmap, as items W0–W8 with gates and costs.
  - **`w1-transplant-notes.md`** — how the runtime came across, what was parked and why.
  - **`xenia-capture-requests.md`** — round 1, written 2026-08-15, **open and unfulfilled**.
    Nothing in this port has been checked against hardware. Its §0 records what Case Zero
    already answered and is deliberately not re-captured; §W is Bink and has no Case Zero
    equivalent; §X is three pre-registered questions.
  - **`gotchas.md`** — the 315-entry transferable ledger. Every "gotcha N" resolves here.
  - **`imported-fixes.md`** — **every fix taken from Case Zero, with the source commit and
    the date it landed here**, so a later revision in the sibling can be diffed rather than
    re-derived. The operator asked for this by name.
  - `reusability.md` — the tier list for what may be extracted into shared code, and the
    two rules governing it. Relevant at W6, not before.
- `runtime/` — the host runtime, transplanted from Case Zero in W1 (`docs/w1-transplant-notes.md`).
  **Its instruments are `CW_*`**, renamed at transplant time so an exported variable cannot
  reach the wrong port's binary on a machine that runs both; the gate for that rename was
  three-way, including a negative control proving the old `CZ_` name does nothing.
  Target is `cw_runtime`; the link gate survives as `cw_runtime --smoke`.
  - `runtime/port-pending/` — **four modules deliberately NOT built**: `guest_probe.cpp`,
    `debug_tunables.cpp`, `d3d_hooks.cpp`, `d3d_draw.cpp`. They are Case Zero's per-title
    reverse engineering (the "never shared" tier), and between them they name 156 guest
    addresses of which **155 do not exist here and one, `sub_82475718`, does** — it would
    have linked silently to an unrelated function. Read that directory's README before
    restoring any of them. `debug_tunables_stub.cpp` and `d3d_draw_stub.cpp` keep the seam
    and report their own absence.
- `Xenia logs/` — captures land here (gitignored); the index
  `Xenia logs/Xenia_Run_Content.md` **is** tracked and is the only thing that survives a
  lost capture. Empty so far.
- Recompiler TOOL at `~/GithubRepo/XenonRecomp` (built at `build/`; **carries local
  patches, including the devkit-AES-key fix this title needs** — see Case Zero's
  `docs/xenonrecomp-upstream-bugs.md`). Shader translator at `~/GithubRepo/XenosRecomp`
  (also patched; Case West inherits those fixes for free).

## Commands

Unpack the game (once):
```
python3 tools/extract_stfs.py \
    "assets/package/58410B00/000D0000/D01128ABB9C7F9694DAE26AC591A269F8480E85A58" \
    -o assets/game
./tools/build_xex_image_dump.sh
./tools/xex_image_dump assets/game/default.xex assets/game/default_image.bin
```
The image dump must come **before** any config regeneration: `find_jumptables.py` and
`fix_switch_function_bounds.py` both read their code ranges from the `.sections` sidecar
it writes, and both exit rather than guess if it is missing.

Regenerate the recompiled C++ (from repo root; `ppc/` must exist or XenonRecomp
segfaults in `fwrite`):
```
mkdir -p ppc && cd config && ~/GithubRepo/XenonRecomp/build/XenonRecomp/XenonRecomp \
    CaseWest.toml ~/GithubRepo/XenonRecomp/XenonUtils/ppc_context.h
```

Regenerate the switch tables — **use our scanner, not XenonAnalyse**:
```
python3 tools/find_jumptables.py assets/game/default_image.bin \
    -o config/CaseWest_switch_tables.toml
```

Repair function bounds after any switch-table change, then re-run the recompiler and
confirm the log has zero `jump outside function` lines:
```
python3 tools/fix_switch_function_bounds.py --apply
```

Check for silently dropped direct branches — **not optional after any change to the
function list**, and the only thing that catches the coverage oracle's loop-header splits
(gotcha 28). Regenerate `ppc/` between each step. **Not yet run on this title (W0):**
```
python3 tools/find_dropped_branches.py            # report both classes
python3 tools/find_dropped_branches.py --prune    # backward: remove spurious starts
python3 tools/find_dropped_branches.py --widen    # forward: widen truncated functions
```

Then check that every switch-shaped `bctr` was actually lowered — the gate for the defect
class that leaks a callee's non-volatiles into its caller (gotchas 53-55).
**Exit 1 = a real defect; run it last, and after any config change:**
```
python3 tools/find_unlowered_switches.py          # 0 defects expected
python3 tools/find_unlowered_switches.py --all    # also list benign tail-call thunks
```

Rebuild the SPIR-V shader cache. **`assets/shader_spv/` is gitignored, so a fresh clone
needs this.** Input is the captures' microcode (and, once a runtime exists, its own dump,
which is the authority on the byte range because the cache key hashes it):
```
python3 tools/xenia_ucode_to_cache.py "Xenia logs"/*/cw_shaders_*/ \
    ~/DR2CW-troubleshooting/ucode-dumps          # 439 distinct
tools/build_shader_spv.sh ~/DR2CW-troubleshooting/ucode-dumps assets/shader_spv
python3 tools/shader_dim_census.py               # 0 disagreements expected; exit 1 = defect
```
**The bank grows on every capture that reaches new MATERIAL** — 386 → 435 the moment one
drive loaded a second zone, but only 435 → 439 across nine further world frames, because
those covered new *places* inside a material set already visited. A shader the cache lacks
is one log line and a silent counter, not a failure. Keep `dump_shaders` on for every
capture whatever it was asked for. **There is no outdoors in this game** — Case West is set
entirely inside the Phenotrans facility, so the captured set covers its area classes and the
bank is more likely near-complete than a missing-biome reading would suggest.
```
```
The census is **two-sided by construction** — the per-slot texture dimension is derivable
both from our ucode parse and from DXC's `OpDecorate` words, so a disagreement means one
of the two decodes is wrong. Keep the dumps out of `/tmp`.

Recover entry points from a Xenia coverage trace, then **always** run the disposal pass —
the oracle proposes loop headers that split real functions, and only the dropped-branch
check can tell those from genuine indirect targets:
```
python3 tools/coverage_to_function_overrides.py --trace <trace.0> --ppc-dir ppc/ \
    --image assets/game/default_image.bin --config config/CaseWest.toml --apply
#   regenerate ppc/, then:
python3 tools/find_dropped_branches.py --prune    # regenerate again
python3 tools/fix_switch_function_bounds.py --apply
```

Look inside the game's `.big` archives — 154 of them:
```
python3 tools/big_list.py --all --find cc_        # search every archive by entry name
python3 tools/big_list.py <a.big> --extract <name> --out DIR
```
The format is Case Zero's `docs/big-archive-format.md`, confirmed unchanged on this
title. Read that doc **with its two retractions**: the name table is NOT fixed-width
outside the shader banks, and a substantial fraction of entries ARE compressed.

## The recompilation contract (identical to all three earlier ports)

- Every guest function → `PPC_FUNC_IMPL(__imp__sub_XXXXXXXX)` taking
  `(PPCContext& ctx, uint8_t* base)`.
- Guest 32-bit addresses index into `base`; `PPC_LOAD/STORE_*` swap endianness.
- Hooks: define a strong `PPC_FUNC(sub_X)` calling `__imp__sub_X(ctx, base)` pre/post.

## Game intel (established 2026-08-15)

Full detail and evidence in `docs/bootstrap-2026-08-15.md` §6.

- **Package**: XContent `LIVE`, content type `0x000D0000` (Arcade Title), STFS volume,
  305 files, 1.22 GB. Display name `DEADRISING2:CASE WEST`. Title ID `58410B00`,
  media ID `198BA306`.
- **XEX**: image base `0x82000000`, entry `0x825AC918`, image size `0xB40000`,
  `.text 0x82150000 + 0x87BC54` **and `BINK 0x829CBE00 + 0x106B8`**. Encryption 1
  (**devkit key**), compression 2 (**LZX**). 247 imports.
- **Internal project name is `deadrisingepilogue`** — 402 build-path strings say
  `c:\bcg\deadrisingepilogue\`. Shader banks are
  `data/shaders/deadrisingepilogue-{vs,ps,vd,pd,sc,sd,ss}.big` where Case Zero's were
  `deadrisingprologue-*`. **Retarget anything keyed on that string.** (And note Case Zero
  established those banks are `.vo` shader *objects* with build metadata, NOT usable
  microcode — the renderer's input is Xenia's `dump_shaders`.)
- **Engine**: Blue Castle Games' in-house engine, shared with the full Dead Rising 2 —
  the image still carries DR2's zone names (`americana`, `atlantica`, `arena_stadium`)
  it does not ship, alongside Case West's own (`Phenotrans`, `SecureLab`, `StoragePens`,
  `LivingResearch`, `Safehouse`).
- **THE WHOLE GAME IS SET INSIDE THE PHENOTRANS FACILITY — there is no outdoors.** Every
  shipped zone name above is a part of that facility, which is why they read as interiors.
  This matters for capture planning and for reading the shader bank: unlike Case Zero's
  outdoor Still Creek there is no exterior area class to be missing, so a capture set that
  covers the facility covers the game. (Operator, 2026-08-15, correcting a session-2 note
  that had called a missing "outdoors" frame a coverage gap.)
- **Middleware**: Havok physics (779 `hkp*` strings), XMA audio, in-house "CrowdEngine"
  for zombie crowds, **and Bink video — really, this time.**
- **Assets**: 154 `.big` archives, `.bct` textures, `.bcf` fonts. Runtime path
  construction (`data/movies/%s`), so the VFS must handle arbitrary paths rather than a
  fixed manifest.
- **A debug build is present in the retail image** — `DebugMenu`,
  `COMMAND_RENDERDEBUGMENU`, `COMMAND_AIDEBUGMENU`, `DontAutoCompleteOnDebugJump`. Case
  Zero's navigation instruments were built on the equivalent and made the back half of
  that port measurable; find these early.

## Conventions (same as the three template ports)

- No copyright/license headers in new files (user's own repo — ask before adding any).
- **Commit proactively** — whenever a change is useful on its own or important
  information was learned. End commit messages with:
  `Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>`
- **Use plain `git commit`. Never pass `-c user.email` / `-c user.name`.** The repo's git
  config already holds the right identity (`wivi514 <wivi514@hotmail.com>`, the address
  the GitHub account uses); overriding it can only make it wrong. Doing so once cost a
  six-commit `filter-repo` rewrite before this repo could be published — and after a push
  the fix is a force-push that breaks every clone. `git var GIT_AUTHOR_IDENT` is the
  one-line check.
- **Document everything** in `docs/` for an outside reader — findings, dead ends,
  formats, retractions. Write it so someone porting a *different* Xbox 360 game can lift
  the technique: say what the idiom or format was, not just what we changed.
- **Comment code for humans.** Every tool opens with a docstring answering *why it
  exists* — what went wrong without it. Inline comments explain the non-obvious
  bit-twiddling and every deliberate exclusion. Generated config files carry a header
  saying which tool produced them and how to regenerate.
- **Retract in place.** When a stated finding turns out to be an artifact, say so where
  it was claimed and explain the artifact.
- **State the provenance of a copied conclusion.** In this port specifically: when
  something is believed because Case Zero measured it, say so, and say whether it has
  been re-measured here. The three session-1 traps were all unmarked inheritance.
- Measurement discipline from day one: A/B with same-binary arms, gate comparisons,
  pre-register capture questions.

### Evidence rules (non-negotiable)

- **Measure before inferring.** A hypothesis about guest behaviour is tested against a
  census over the image, the shader bank or the capture — never argued from
  documentation, from model knowledge, **or from the sibling port**. Report counts, not
  impressions.
- **One change per experiment.** Fixes with distinct predictions land in separate commits
  and are verified separately.
- **State the prediction before running it.** Every fix commit records the falsifiable
  claim it makes, so a run can refute it.
- **A/B ADMISSIBILITY.** Two configurations are comparable at matched indices only if
  they are two states of ONE renderer producing the SAME draw set. If one arm renders
  less — or more — the comparison is inadmissible; say so and fall back to within-run
  evidence.
- **Refutation by compensation beats refutation by absence.** When a mechanism is real
  but compensated somewhere else, record BOTH.
- **An untrusted path is not an oracle**, and **an oracle must be something you did not
  write** (gotcha 172/268). This port has an unusual luxury and an unusual hazard here:
  Case Zero is a *second implementation of the same engine*, which makes it a genuine
  cross-check for anything shared — and makes it worthless as an oracle for anything it
  got wrong, which is a list nobody has finished writing.
- **AND XENIA IS NOT A CEILING (gotcha 320, finding 29).** Separate the two kinds of ground
  truth. For **what the guest did** — kernel call order, file reads, PM4 packets, shader
  microcode — the capture wins, because those are the guest's own bytes. For the emulator's
  **output** — audio continuity, timing, presentation — it does not automatically win, and
  this port has already passed it: **Xenia truncates a cutscene's dialogue that our runtime
  plays through.** So "it differs from the capture" is a question, not a verdict, and an A/B
  that scores "closer to Xenia" as better will actively regress what we already get right.

### Things not to do

- **Do not extract a library from these two ports yet.** `docs/reusability.md`'s rule is
  to extract after the second implementation forces the seam, and the seam is not visible
  until this title boots. W6, not before.
- **Do not bundle independent fixes into one commit.**
- **Do not treat documentation or prior model output as ground truth over a census** —
  including this file and including every Case Zero document. Every number in them was
  measured once and has a shelf life (gotcha 13).
- **Do not add speculative Xenos coverage.** An unsupported packet, format or import
  fails LOUDLY with its identifier; it never guesses (gotcha 5).
- **Do not copy external code before its licence is recorded** in `THIRD_PARTY.md`.
- **Do not delete the PM4 command processor** when the runtime is transplanted. It is the
  boot engine and the same-binary control arm for every claim the D3D arm makes.
