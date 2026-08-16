# Xenia capture analysis — the numbered findings ledger

**This file is the authority on any measured number in this port. Where another document
disagrees with it, it wins.** Findings are numbered and never renumbered; a retraction is
written *in place* under the finding it retracts, not deleted.

**ROUND 1 IS COMPLETE**, all delivered 2026-08-15: **A1, A2, A3, A4, A5, B1, B1b, B2, C1,
C2, W1, W2, B4** plus boot-logo screenshots. E (screenshots) is satisfied in substance by
the frame-locked PNGs that came with W/B4. **Nothing is outstanding.**

**Nothing is outstanding, and there is no place gap either.** An earlier version of this
paragraph called a missing "outdoors" frame the likeliest remaining source of new shaders;
**the operator corrected it — Case West has no outdoors. The whole game is set inside the
Phenotrans facility.** See the retraction under finding 20.
What each file is: `Xenia logs/Xenia_Run_Content.md`. What was asked and why:
`docs/xenia-capture-requests.md`.

**Round 1 refuted FOUR claims this project had already written down as likely** — findings
3, 4, 18 and 23. All four were mine; three were inferences from a *count* or a *truncated
listing* rather than measurements, and all are corrected in place in
`docs/bootstrap-2026-08-15.md`, `docs/port-plan.md` and the memory directory.
`docs/part2-kickoff.md` §4 states the common lesson.

---

## 1. The trial trap did not fire, because the request caught it

`license_mask = 1` on every run; no unlock prompt, no trial strings. Title ID `58410B00`,
Media ID `198BA306`, Savegame ID `58410B00`. `XamContentGetLicenseMask` is imported
(ord `0x266`) but not logged per call at L3, so the licence state is established
behaviourally and from the config header — the same standard of proof Case Zero settled
for, and the same conclusion.

Nothing further to do. Recorded because Case Zero's *first* A1 take booted the trial and
its whole round 1 nearly went to the wrong code path.

## 2. From-boot `--trace_function_data` does not crash this title

Same as Case Zero and Asura's Wrath; it is Fable II that crashes. C1 completed cleanly with
a `Cheap-skate exit!`.

## 3. **RETRACTION — the mutants are NOT co-op. They are a hot solo-gameplay lock.**

`docs/port-plan.md` W1 and the request's §X.1 both guessed that `NtCreateMutant` /
`NtReleaseMutant` — two of this title's three imports that Case Zero lacks — "probably
belong to co-op", on the reasoning that Case Zero has no co-op and no mutants. A1 appeared
to support it: **zero** calls to any of the three on the boot→title path.

**A2 refutes it.** In a *solo* gameplay session:

```
NtCreateMutant    =      6
NtReleaseMutant   = 32,382
DbgBreakPoint     =      0
```

Six mutants created and released ~32,000 times across 5–10 minutes is a per-frame or
per-resource lock on the ordinary single-player path — the call volume points at audio/XMA
or the streaming loader. **They must be implemented as real synchronisation primitives.**
A mutant that fakes acquisition is gotcha 5's failure mode with a deadlock at the end of
it, and this one is on the hot path where a deadlock will be blamed on anything else.

`DbgBreakPoint` at 0 calls is consistent with what it is — it fires only on an actual debug
break — and is **safe to stub**.

**The lesson, because it is the reason this was wrong and not just unlucky:** A1's zero was
read as evidence *for* the co-op attribution when it was only evidence that *boot* does not
use them. An absence measured on one path is not an attribution to another path. It is
gotcha 3 wearing a different hat — the zero was a fact about the boot drive, not about the
primitive.

## 4. **RETRACTION — Bink is real, but it is NOT on the boot path.**

`docs/bootstrap-2026-08-15.md` §6, `docs/port-plan.md` W3 and the memory
`case-west-really-uses-bink` all state that `dr2_logo.bik` is on the boot path and that
Bink therefore "cannot be deferred the way a cutscene could". **The first half is wrong.**

A1 opens **zero** `.bik` files on the boot→title path. None of `dr2_logo`, `dr2_logo_b`,
`DR_LOGO_shortLoop`, `800a_intro` or any `*_monitors` appears anywhere in the 31 MB log,
and `NtCreateFile` *is* logged at L3 (the log is full of frontend `.big`/`.tex` opens), so
a movie open would have shown. The four frame-locked PNGs confirm what is actually on
screen: an ESRB rating card, then Capcom and Blue Castle logos, then the title — rendered
from `data/frontend/ratinglogos.big` / `startup.tex`, i.e. **static image assets**.

The inference that produced the error was: the `.bik` files exist, `dr2_logo.bik` is
obviously a boot logo by its name, therefore it plays at boot. **A filename is not a call
site.** This is the same shape as Case Zero's own retracted Bink claim — that port inferred
a codec from a string, this one inferred a *playback moment* from a filename — which is
worth noticing, because the correction to Case Zero's error was already in front of me.

**What is NOT retracted:** Bink genuinely runs in this title, which was the substantive
claim. Finding 5 is the confirmation.

## 5. Bink CONFIRMED in gameplay, streamed out of the STFS package, on no thread of its own

A2 opens two of them with `NtCreateFile`:

```
game:\data\movies\800a_intro.bik     the New Game intro cinematic (33 MB)
game:\data\movies\807_monitors.bik   an in-world MONITOR screen, reached in the opening area
```

Three properties, each of which changes what has to be built:

1. **They resolve through `StfsContainerDevice::ResolvePath(\data\movies)`** — the movies
   are read from **inside the STFS package**, not from the loose `assets/game/data/movies/`
   copy the extractor produced. The VFS must serve them from the container.
2. **No dedicated movie or Bink thread is created.** The "Movie Player Object" is
   constructed exactly as it is at boot (two named events: *Request Movie Player Object
   Event Handle*, *Movie Player Object Created Event Handle*) and then fed a file. This is
   consistent with — though it does not yet prove — the working hypothesis that the
   recompiled `BINK` code section does the decode on an existing thread, leaving only I/O
   and the output surface as host work.
3. **The reads themselves are invisible at L3**, because `NtReadFile` is `kHighFrequency`.
   The streaming cadence needs **A5**.

**Consequence for the capture request: W1 as specified is unbuildable and is retargeted.**
There is no boot Bink to take a single frame of. The Bink output-surface capture must be
taken at the New Game intro and at the `807_monitors` screen, with `trace_gpu_stream`
**off** so the F4 frame `.xtr` is actually written.

## 6. Solo boot DOES enter the networking stack — partially

The request's §X.2 asked whether a single-player boot walks through co-op initialisation.
It does not enter the *session* layer, but it is not networking-free either:

```
called   : NetDll_WSAStartup ×2, NetDll_XNetStartup ×1, NetDll_XNetGetTitleXnAddr ×405
NOT seen : XSession*, XOnline*, XLive, any matchmaking
```

**So Winsock and the XNet title-address query are on the single-player critical path and
must work**, even though multiplayer is out of scope. 405 calls to
`XNetGetTitleXnAddr` on a boot is a poll, not an initialisation — something is waiting on
an address to become available, and a stub that returns a fixed answer needs to return one
the caller will accept rather than one that makes it spin forever.

This does not change the scope decision: the co-op session layer stays out. It does mean
"multiplayer is out of scope" cannot be implemented as "make every NetDll call fail".

## 7. The forwards coverage oracle recovered 43 entry points

C1 executed **11,681** functions (Case Zero's C1: 12,278 — same ballpark), over
`0x82150000–0x829CBC14`. `tools/coverage_to_function_overrides.py` against a `ppc/`
regenerated from the committed config:

```
executed functions        : 11,680
already recompiled        : 10,965
skipped, switch labels    :    487
skipped, import thunks    :    112
skipped, helper ladders   :     68
skipped, implausible      :      0
MISSING -> overrides      :     48
```

**Then the disposal pass removed 5 of the 48.** The tool's own docstring predicts this and
names the mechanism: Xenia records any executed branch target as a "function", so loop
headers arrive looking exactly like undiscovered entry points, and adding one splits a real
function so its loop-back edge becomes a silently dropped branch. `find_dropped_branches.py`
reported **6 dropped branches across 5 functions — all 5 config entries added by the
oracle** — and `--prune` removed them. One further round of
`fix_switch_function_bounds.py` closed a switch tail that a widened recovery had exposed.

**Net: 43 real entry points that no static analysis would ever find**, including
**`0x825AC918` — the image's own entry point**, which nothing `bl`s to.

Final state after C1: **58,387 functions, 74 function overrides**, zero `jump outside
function`, zero dropped branches, unlowered-switch gate at 0 defects. **Superseded by
finding 13** once C2 arrived.

*(An earlier version of this paragraph said 76 overrides. That was a `grep -c 'address = '`
count, which also matches two of the eight `save/restgprlr_*_address` config keys — the
ones written without column padding. The entry count is 74.)*

The count of import thunks the tool located — **247, at `0x829CACE4..0x829CBC54`** — matches
the import census exactly and is a free cross-check on both.

## 8. The shader bank is 386, and A2 alone contains all of it

**Read the distinct-blob count, not the file count.** Xenia writes three files per shader
variant (`*.ucode.bin.*` raw microcode, `*.ucode.*` disassembly, `*.d3d12*.bin.*` its own
translated DXBC), so the per-run file counts in the delivery notes — 168, 1,289, 1,062 —
are roughly 3× the number of shaders and are not comparable with Case Zero's blob counts.
Distinct microcode blobs:

| run | distinct shaders |
|---|---|
| A1 (boot→title) | 54 |
| B1 / B1b / C1 (same drive) | 54 each, identical set |
| **A2 (gameplay)** | **386** |
| B2 (gameplay stream) | 321 |
| **union** | **386 — 301 pixel, 85 vertex** |

**A1's 54 ⊂ A2's 386, and B2's 321 ⊂ A2's 386, exactly.** One capture currently holds the
entire bank. Case Zero's equivalent shape was A1 120 ⊂ A2 335.

Treat "the bank is complete" as a claim with a shelf life (gotcha 13). Case Zero's grew
from 335 to 435 and grew on **every** session that reached new ground; these two drives
cover a boot and one opening area, so every part of Case West nobody has walked into is a
shader gap nobody has counted.

> **AND THE SHELF LIFE WAS UNDER AN HOUR. Superseded by finding 15**: C2 arrived with a
> drive that loaded a second zone and brought **49 shaders A2 had never seen**. "A2 alone
> holds the entire bank" was true of the captures in hand and false of the game. The
> sentence above was written as a hedge; it should have been written as a prediction.

## 9. XenosRecomp translates the entire bank with zero failures

The strongest early de-risking result of the round, and it was free:

```
tools/xenia_ucode_to_cache.py <6 dump dirs> ~/DR2CW-troubleshooting/ucode-dumps
    -> 923 blobs in, 386 distinct out, 0 skipped
tools/build_shader_spv.sh ~/DR2CW-troubleshooting/ucode-dumps assets/shader_spv
    -> translated 386 shaders, 0 failures
tools/shader_dim_census.py
    -> 298 modules 2D (950 declared fetch slots), 98 cube (98 slots), 0 disagreements
```

The last is **two-sided by construction**: the per-slot texture dimension is derivable both
from our own microcode parse and from DXC's `OpDecorate ... DescriptorSet` words, so a
disagreement means one of the two decodes is wrong. Zero disagreements over 386 modules.

Three things this establishes before a line of renderer code exists:

- the **XenosRecomp patches Case Zero paid for transfer whole** — not one codegen gap on a
  different title's bank;
- `synth_shader_container.py` carries Case Zero's part-45 fix (a PARTIAL register write is
  not a write; the synth container was dropping PS interpolants and 217 of its 333 pixel
  shaders sampled diffuse at one texel), so **Case West starts on the corrected side of the
  defect that cost that port its whole flat-surface class**;
- the naming glob (`*.ucode.bin.{vert,frag}`) matched without modification, so the fork's
  dump format is unchanged from Case Zero's.

`assets/shader_spv/` is 386 `.spv` + 386 `.meta.json`, 10 MB, gitignored and regeneratable
from the command above.

## 10. `.big` streaming, threads and the audio surface — scale only

From A2, recorded so a later measurement has a baseline rather than as anything to act on:

- **95 distinct `.big` archives** opened over the gameplay drive (Case Zero's A2: 433 — but
  that was a much longer drive, so this is not a comparison);
- **29 `ExCreateThread`** total;
- **7,880,619 `XmaContext` log lines**, which is why the raw A2 log is ~970 MB. The XMA
  context lifecycle and the decode pump are fully exercised, so the audio phase has what it
  needs to start without a dedicated capture.

Nothing looked wrong inside Xenia on any run. The only "unimplemented" strings in the logs
are the import-coverage banner and a config echo.

## 11. B2 is 14.8 GiB and the 2 GiB cliff fix holds at that size

`58410B00_stream.xtr` = 15,527,183,189 B, header `01 00 00 00` + GUID, finalized on a clean
exit. That is ~2× Case Zero's B2 (7.95 GiB) and re-proves the operator's source fix for the
`ftell` limit that cost Asura's Wrath a whole capture.

It also overshot the request's "stop promptly" guidance — the stream grows ~1 GB/min.
**Future gameplay streams: stop 1–2 minutes after combat starts.** Not a defect in this
capture; a note for the next one.

## 12. Determinism between B1 and B1b is NOT yet established

B1 is 521,700,871 B and B1b is 555,355,676 B, a ratio of 1.064. **Do not read that 6% as a
determinism verdict in either direction.** Stream total size is not a determinism metric
and a byte-diff is meaningless — host fields differ run to run. The operator flagged this
themselves rather than reporting a number that looked like an answer.

A real check needs the `.xtr` decoder aligned per frame over the fixed boot+logo prefix,
which is `tools/xtr_determinism.py` in the Case Zero repo and has not been brought across
or run here. **Open.** Until it runs, this port has no established noise floor for GPU
stream comparison, and Asura's Wrath spent real time treating noise as signal for exactly
that reason.


---

*Findings 13-16 added the same day, from capture **C2** (gameplay function coverage,
driven further than A2/B2 — the operator reached and loaded a whole new zone).*

## 13. C2 is a strict superset of C1, and adds 61 more entry points

```
C1 executed : 11,472 functions          (operator's count 11,681 — see the note below)
C2 executed : 19,195 functions          (operator's count 19,487)
C1 addresses absent from C2 : 0         <- strict superset, as predicted
```

*(The two counts differ because I additionally require `end > start` when reading the
48-byte records, which drops ~210 and ~290 degenerate entries respectively. Neither count
is wrong; they measure slightly different things, and nothing downstream depends on the
difference.)*

Re-running `coverage_to_function_overrides.py` over **both** traces proposed **67** further
overrides. The disposal cycle removed 5, and one `fix_switch_function_bounds` round cleared
the 11 `jump outside function` errors the splits had caused — all of which came from the 5,
since the repair tool itself reported "0 new this round".

**Net +61.** Final state: **58,448 functions, 135 function overrides**, zero
`jump outside function`, zero dropped branches, unlowered-switch gate at 0 defects.

**Two of the 5 pruned addresses had already been pruned in the C1 round**
(`0x825F6F9C`, `0x82837740`). That is not a new defect and not a regression: the oracle
reads the trace and the config, the pruned entries are by definition not in the config, so
fresh coverage data re-proposes them. `coverage_to_function_overrides.py`'s docstring
predicts exactly this loop. **Expect it on every future coverage round, and do not treat a
recurring prune as a problem** — the prune is the disposal step, not a repair that should
have stuck.

## 14. **The Bink decoder RUNS, as recompiled guest code — 137 functions of it**

The strongest result in this delivery, and it was free — nobody asked for it.

```
functions executed inside the BINK section (0x829CBE00..0x829DC4B8):
  C1 (boot -> title)            :   0
  C2 (gameplay, intro played)   : 137      range 0x829CBE88..0x829DAC50
```

C2's drive plays the New Game intro (`800a_intro.bik`); C1's stops at the title. So the
zero and the 137 are the same measurement taken either side of the only event that
distinguishes them.

**This is direct evidence for W3's working hypothesis**, which until now was an argument
from section flags: the statically linked RAD decoder is ordinary PowerPC code, XenonRecomp
translated it along with everything else (`PPC_CODE_SIZE` covers it), and on hardware it
executes as 137 distinct guest functions on an existing thread — consistent with A2 showing
no dedicated movie thread (finding 5).

**What it does not establish**, stated so the next session does not over-read it: that our
*translation* of those 137 is correct, or that the output surface works. It establishes
that the decode is guest code we already generate, which is what decides whether W3 is "wire
up I/O and a texture upload" or "write a Bink decoder". It says the former.

Two of the 137 needed entry-point recovery (`0x829D3810`, `0x829D38F0`) and are now in the
config — i.e. the decoder reaches at least two of its own functions indirectly, and without
C2 they would have been indirect-call misses the first time a movie played.

## 15. The shader bank grew 386 → 435 on one new zone

```
C2 distinct shaders          : 415
  of which NEW (not in A2)   :  49
A2 shaders absent from C2    :  20      <- neither drive is a subset of the other
union, all seven dumps       : 435      (was 386)
```

Rebuilt: 1,338 blobs in → **435 distinct → 435 translated, zero failures**;
`shader_dim_census.py` reports 341 modules 2D (1,097 declared fetch slots) and 110 cube,
**0 disagreements**. `assets/shader_spv/` is 12 MB.

This is the sibling port's rule arriving on schedule — *the cache grows on every session
that reaches new ground* — and it supersedes finding 8's "A2 alone holds the bank". Two
drives through the same era are not subsets of each other in either direction, which is the
same shape Case Zero recorded for its A1/A5 pair.

**Carry `dump_shaders` on every future capture, including ones requested for other
reasons.** C2 was asked for as a coverage trace; 49 shaders came along for free.

## 16. The sub-image addresses in C2 are the kernel module, not new-zone code

C2's notes read its executed range as `0x80050030–0x829DAC50` and attribute the low end to
"code reached only on the new-zone path". **It is two records at `0x80050030..0x80050038`**,
which is below this title's image base (`0x82000000`) entirely — that address range is
`xboxkrnl.exe`, which Xenia traces as another loaded module.

No action: `coverage_to_function_overrides.py` bounds the executed set to our image, so they
were never candidates. Recorded only so the range is not read as evidence about the title
next time. The *upper* end of that range is the interesting half, and it is finding 14.

---

*Findings 17-21 added the same day, from captures **A4** (title idle) and **W_B4** (nine
place-anchored single-frame F4 captures with frame-locked PNGs).*

## 17. **BINK IS SOLVED. It needs no new host subsystem at all.**

This is the most consequential finding in the round, and it collapses W3 from a subsystem
to an already-satisfied requirement. Frame `01_W1_intro_800a_bik__f4412.xtr` — the New Game
intro, fullscreen — decodes as follows.

**Draw 150** is a 4-vertex fullscreen quad, `vs_d6aceb8914b9cbe9` / `ps_a9f83f703af104b5`,
binding five textures:

```
 s0  0DF87000  1280x720  fmt=2 (k_8)      tiled=0  pitchBlk=40  -> Y  (luma)
 s1  0DEF5000   640x360  fmt=2 (k_8)      tiled=0  pitchBlk=24  -> U  (chroma)
 s2  0DF3E000   640x360  fmt=2 (k_8)      tiled=0  pitchBlk=24  -> V  (chroma)
 s3  17CBE000    80x45   fmt=6 (k_8_8_8_8) tiled=1              -> tone-map / luminance
 s4  1783C000  1024x32   fmt=6 (k_8_8_8_8) tiled=1              -> 32^3 colour LUT, unwrapped
```

A 1280×720 luma with two 640×360 chroma planes is **4:2:0 YUV**. And the pixel shader is
144 bytes of microcode that does exactly what that implies:

```
tfetch2D r3.x___, r0.xy, tf2        ; V
tfetch2D r3._x__, r0.xy, tf1        ; U
tfetch2D r3.__x_, r0.xy, tf0        ; Y
mad r2.xyz_, r3.zyxx, c254.zxyy, c255.xyzz    ; YUV -> RGB, coefficients in constants
```

**Five independent lines of evidence, so this is not one reading:**

1. the plane **dimensions** are 4:2:0 (1× luma, 2× quarter-area chroma);
2. the **shader** fetches three single-channel textures and applies a colour matrix;
3. the luma's **byte histogram clusters hard at 16** — 475,892 of 921,600 texels, i.e.
   studio-swing black level (Y ∈ 16..235). A texture that was not video luma has no reason
   to pile up on exactly 16;
4. the dump is **exactly 921,600 bytes** = 1280×720×1, confirming k_8 at one byte/texel;
5. **rendering the plane reproduces the on-screen frame pixel-for-pixel** — read *linear*,
   the luma is the intro text, scanlines and all.

And the provenance gate passes: the trace issues **no RESOLVE** to `0DF87000`, so these are
guest memory the title uploaded — a sound oracle, not a snapshot from before the surface
existed (gotcha: the shadow-atlas trap in the sibling port).

### What this means for W3

Put together with finding 14 (the decoder runs as recompiled guest code, 137 functions):

| layer | who does it | status |
|---|---|---|
| container read | VFS, from inside the STFS package | already built for `.big` |
| video decode | **guest code** — the recompiled `BINK` section | free (finding 14) |
| output surface | three ordinary **linear `k_8`** textures in guest memory | nothing special |
| YUV → RGB | **the guest's own pixel shader** `ps_a9f83f703af104b5` | already in our cache |
| composite | the title's normal tone-map + colour-LUT post chain | same as every frame |

**There is no host-side movie player to write, no colour conversion, and no ffmpeg
fallback needed.** W3 reduces to "do not break any of the above" — and its one real
requirement is that the renderer **honours `tiled=0`**, because these are the frames'
linear planes and detiling them produces exactly the scrambled block pattern this analysis
produced on its first, wrong attempt.

Two details a renderer will need and nothing else records:
- **chroma pitch is padded**: `pitchBlk=24` → 768 texels for a 640-wide plane; luma
  `pitchBlk=40` → 1280, i.e. exact. Reading chroma at width-as-pitch will shear it.
- the **text and the typewriter cursor are baked into the video**, not drawn live. The
  on-screen frame and the luma plane agree glyph for glyph.

## 18. The top-N-by-vertex-count default hid all of it — a tooling trap

`xtr_draw_bindings.py` lists only the **top 12 draws by vertex count**. Every Bink draw is
a **4-vertex fullscreen quad**, which sorts to the *bottom* of that order, so the first pass
over this frame reported "no video-sized texture in the frame — 13 distinct textures, all
render targets or post-process" and was **completely wrong**: the full CSV has 20 distinct
textures over 155 bindings, and the three that matter were among the 7 the cap dropped.

**A vertex-count sort is exactly backwards for post-process, UI and video work**, which is
where the interesting draws are small and the boring ones are large. The tool now takes
`--max-draws` (default unchanged) with that warning in its help text.

*The general form, and it is gotcha 3 again: a listing that truncates is a detector with a
threshold, and its silence is a detection failure, not a fact. Use `--csv` before concluding
anything is absent.*

## 19. A4: no idle Bink, and the title screen is an animated 3D scene

The title screen runs no attract-loop movie — **zero `.bik` opened across a 5-minute idle**,
confirming the operator's on-screen observation, and closing the "free §W capture" the
request had speculated A4 might offer.

But it is not a static image either: the character periodically plays a photo-taking
animation, and the idle log is **GPU-dominated — 804,080 `G>` lines of 1,249,855 (64%)**,
with 255,996 `XmaContext` lines (20%). Case Zero's A4 was ~69% GPU: **the same shape**.

Two consequences, both good: the title screen should be as cheap and stable a test bed here
as it was there (it is where most of that port's instrument work was validated), and it
introduces **no new shaders** — A4's set is 54 distinct, identical to the A1/B1 boot set, so
the idle scene is the same frontend material already compiled at boot.

This answers the request's §X.3.

## 20. The shader bank is 439, and B4's nine world frames added only 3

```
W session (9 F4 frames across the facility) : 416 distinct
A4 (title idle)                             :  54 distinct, = the boot set exactly
union of all nine dumps                     : 439   (was 435)
```

Rebuilt: 1,808 blobs in → **439 distinct → 439 translated, zero failures**; dim census 345
2D / 111 cube, **0 disagreements**.

**+3 is a much smaller step than C2's +49**, and that is informative rather than
disappointing: the W drive covered the security office, a bathroom, a control room, two
StoragePens crowd scenes and a lab — i.e. a lot of *places* but all within the Phenotrans
material set that A2 and C2 had already visited. **New geometry is not new shaders; a new
material set is.**

> **RETRACTED, 2026-08-15 (operator):** this finding ended by naming **outdoors** as the
> part of the map that would still move the number, and the capture notes flagged the same
> thing as a gap. **Case West has no outdoors** — the entire game takes place inside the
> Phenotrans facility, so there is no unvisited area class and B4's "big open receiving
> area" is a large interior, the closest this title comes to an exterior.
>
> The error: the delivery *lacked* an outdoors frame, and I read that absence as a gap in
> the **capture** rather than as a fact about the **game**. It is the same shape as reading
> A1's zero mutant calls as an attribution (finding 3) and a filename as a call site
> (finding 4) — **an absence is a fact about what was looked at, not about what exists.**
> One question to the operator settled it. The +3-vs-+49 lesson above is unaffected.

## 21. B4 delivered eight world frames, and what they are for

Not yet analysed beyond the Bink pair — recorded so the next session knows what is on disk
and does not re-request it. All are **self-contained single-frame `.xtr` + frame-locked
1280×720 PNG**, `trace_gpu_stream` off so the frame `.xtr` actually writes:

| # | frame | why it was asked for |
|---|---|---|
| 02, 03 | `807_monitors.bik` on an in-world monitor | Bink as a **texture on geometry** — a different binding from W1's fullscreen |
| 04 | security office, camera-feed monitor bank | many small screens |
| 05 | bathroom **mirror** with two figures reflected | planar reflection — one of the sibling port's hardest surface classes |
| 06 | control room, full **wall** of monitors | the monitor worst case |
| 07 | StoragePens crowd from a catwalk | CrowdEngine + grating transparency |
| 08 | lab interior with **glass** observation windows | interior + glass |
| 09 | receiving area, **large** zombie crowd | the CrowdEngine worst case |

**The notes flag "no clearly outdoors frame" — and that is not a gap.** Case West has no
outdoors at all (operator, correcting finding 20's reading); the drive stayed in Phenotrans
interiors because that is the whole game. Frame 09's big open receiving area is the closest
this title comes to an exterior, and it is captured.

---

*Findings 22-26 added from captures **A3** (save round trip) and **A5** (high-frequency),
which complete round 1. The operator deviated from A5's written spec — it asks for A1's
boot drive, and they drove through to gameplay instead, because A1 had already proved the
boot path has neither mutants nor Bink. That deviation is the reason findings 22 and 23
exist at all.*

## 22. **Bink is STREAMED, sequentially, in 128 KB chunks — and the VFS must hold the position**

A5's `log_high_frequency_kernel_calls` makes `NtReadFile` visible for the first time
(1,453 calls in the drive; it is `kHighFrequency` and invisible at plain L3). Tracing
`800a_intro.bik`'s handle end to end:

```
NtCreateFile(game:\data\movies\800a_intro.bik) -> handle F800025C
    via StfsContainerDevice::ResolvePath(\data\movies)     i.e. from INSIDE the package
257 reads, then NtClose

    1 x 0x0000002C  (44)         the Bink header
    1 x 0x00001284  (4,740)
    1 x 0x00002CD4  (11,476)
    1 x 0x00009E78  (40,568)
    1 x 0x00014ED8  (85,720)
  252 x 0x00020000  (131,072)    <- the streaming chunk

  total = 33,172,692 bytes = the file's size on disk, EXACTLY, to the byte
```

**Every one of the 257 reads passes a NULL `ByteOffset` pointer** — not one explicit
offset in the set. So they are sequential reads from the handle's own file position, and
**the VFS must maintain a per-handle position**; a layer that expected an explicit offset
would read the header 257 times.

Three things this settles:

- **streamed, not whole-file** — 252 fixed 128 KB chunks paced across playback, so a
  runtime that tried to slurp a 33 MB movie up front would change the memory profile and
  the timing of everything around it;
- **read exactly once, front to back**, with no seeking back — the byte total matching the
  file size to zero difference is what proves there is no re-read;
- **served from inside the STFS container**, not the loose extracted copy, confirming
  finding 5's `ResolvePath` observation at the read level rather than the open level.

The handle is recycled 11 times across the log, so the window had to be bounded by its
`Added handle` / `Removed handle` pair before counting. Counting every `F800025C` line
would have mixed four other files into the total.

## 23. **The mutants are BINK's. Third attribution, and the first two were both wrong.**

Finding 3 refuted "the mutants are co-op" using A2. The replacement guess — recorded in
A2's own notes and repeated in this ledger — was audio/XMA or the streaming loader, from
the call volume. **That is also wrong.**

A5 shows all four mutants created in one 20-line burst, and what surrounds them names the
owner outright:

```
ExCreateThread(..., xapi=82847D38, start=829D0318, context=829E012C, ...)
                                        ^^^^^^^^             ^^^^^^^^
                                        in BINK              in BINKBSS
KeSetAffinityThread(..., 0x20)          hardware thread 5
NtCreateMutant -> F8000274
NtCreateEvent  -> F8000278
NtResumeThread
NtCreateMutant -> F800027C
```

Twice, identically: **two worker threads, each with two mutants and one event, whose entry
point is inside the `BINK` code section and whose context argument is in `BINKBSS`.**

And the interval test is unambiguous:

```
NtReleaseMutant total            : 14,614
  before the .bik is opened      :      1   (the import-table declaration, not a call)
  DURING the .bik handle's life  : 14,613   = 100.0%
  after the .bik is closed       :      0
```

**Zero releases outside the movie.** The mutants exist only while a movie plays.

### Why this one took three attempts, stated because the pattern is the lesson

Each wrong attribution was an inference from a *count* — "Case Zero has no mutants and no
co-op, so mutants are co-op"; "32,382 calls is a lot, so it must be audio or streaming".
What settled it was not a bigger count. It was **a structural fact** (a thread entry
address that falls inside a named section) and **an interval test** (100% containment
within a file handle's lifetime), neither of which can be produced by staring at a
frequency table. When an attribution has been wrong twice, stop refining the estimate and
find something that is true or false rather than large or small.

### What it means for the port

`runtime/kernel/imports.cpp`'s recursive `Mutant` — restored in W1 — sits on the **movie
decode critical path**, guarding two guest worker threads that run recompiled `BINK` code.
W1's note that a faked mutant would deadlock "on the hot path, where it will be blamed on
anything else" is more specific than it knew: it would present as *the intro cinematic
hangs*, and the Bink decoder would be the last place anyone looked, because finding 17
established Bink needs no host code.

It also completes the Bink picture. The decoder is guest code (14), on **its own two guest
threads** (23), reading its input in 128 KB sequential chunks from the STFS container (22),
writing three linear `k_8` planes that a guest shader converts (17). Every layer is the
title's own. **The host contributes file I/O and nothing else.**

## 24. A3: the save shape — same mechanism, three differences, slots are internal

The mechanism is Case Zero's exactly, including the `XamContentCreateEx` flags
(`0x00001012`) and the `\Device\Content\N\` symbolic link that increments per mount.

```
XamContentCreateEx(0, "save", ..., flags=0x00001012, ...)
  -> save: => \Device\Content\N\
  -> NtCreateFile(save:\DR2E000.DSF)
  -> NtWriteFile(length = 0x134600 = 1,263,104)  one whole-file write
  -> unregister
```

| | Case Zero | Case West |
|---|---|---|
| file | `DR2P000.DSF` | **`DR2E000.DSF`** — E for Epilogue, P for Prologue |
| payload | 303,104 B (0x4A000) | **1,263,104 B (0x134600)**, ~4.2x, and FIXED |
| on disk | a bare file | a folder-package (`DR2E000.DSF/DR2E000.DSF`) — Xenia's STFS-content representation |

**The finding that changes design, and it is the operator's:** they saved to **slot 1 and
then slot 3**, and there is still exactly **one** `DR2E000.DSF` on disk, name and size
unchanged, with only its contents differing (md5 `21731f11…` → `31202c5d…`). **The save
slots live inside the single container.** A save layer that mapped slots onto filenames
would be inventing a scheme the title does not use.

`runtime/kernel/content.cpp` needed **no functional change** — it is filename-agnostic and
stores whatever blob the guest writes, so the internal-slot structure costs it nothing.
Its comments were updated to describe this title's measured shape rather than Case Zero's.

## 25. A3 and A5 added no shaders — the bank stays at 439

A3 dumped 330 distinct and A5 dumped 283; the union across all eleven dumps is unchanged
at **439**. Consistent with finding 20: both drives covered material A2/C2/W had already
visited. No rebuild was needed.

The bank has now been stable across four consecutive captures, which is the first time
that has been true — and since **Case West has no outdoors** (see finding 20's retraction),
there is no unvisited area class left to move it. That makes 439 a plausible near-complete
bank rather than one with a whole biome missing. Still a claim with a shelf life
(gotcha 13): any run reaching new ground should carry `dump_shaders` regardless.

## 26. VdSwap is visible at last, and the boot's sync surface has a scale

Two numbers A5 makes available that no other capture in this round could, recorded for
whenever they are needed rather than because anything needs them now:

- **`VdSwap` × 5,062**, with the 1280×720 swap parameters — the flip/vsync cadence ground
  truth. Asura's Wrath could not get this from kernel logs at all.
- **`NtWaitForSingleObjectEx` × 173,311** and `XamInputGetState` × 34,545 — the whole
  synchronisation and polling surface, all `kHighFrequency` and therefore absent from
  A1/A2/A3.

`DbgBreakPoint` is still never called, across every capture in round 1. It stays stubbed.

---

## 27. **THE RUNTIME WAS NEVER STALLED. It renders the title screen.** — part 2, 2026-08-15

**This retracts part 2's own premise before any work was done on it.**
`docs/part2-kickoff.md` §2 opened with "the boot **does not present a frame**", named the
`RtlEnterCriticalSection` spin as the thing to explain, and set the first measurement as a
kernel-call-order diff. The diff was run. It passed — and running it is what exposed that
there was nothing to diagnose.

### What was measured, in order

**1. The kernel-call order matches the ground truth.** Against A5 with
`--include-high-frequency` (the authority on the synchronisation surface, and the right
oracle for a lock-shaped symptom):

```
ours 114 distinct calls  ·  A5 129  ·  5 mismatch windows, ALL same-set permutations
0 REAL divergences  ·  exit 0
```

Our sequence is a **set-exact prefix** of A5's. The five permutation windows are thread
races at startup (`KeSetAffinityThread` against the `Vd*` block, `XamUserCheckPrivilege`
against `XamUserGetXUID`), which is what the script's window analysis exists to classify.

**2. The A1 comparison's two "ours-only" names are A1 being the outlier, not us.**
Against A1 the masked gate reports a divergence at position 26 and two names we call that
A1 never does: `XamInputSetState` and `XMsgStartIORequest`. Both appear in **seven** of the
other captures, and both appear in A5 at the same positions we call them (104 and 114). A1
is the only capture of the eleven that lacks either. This is gotcha 172/268 in its exact
form — *an oracle is only as good as its coverage* — and the reason the tool's docstring
says to run both captures rather than either.

**3. The guest's render loop was running the whole time.** `CW_RING_TRACE=1`, which reports
the counters the command processor keeps, over a 90 s headless run:

```
pm4 packets=34,015,150   frames(XE_SWAP)=2,783   draws=2,059,284
predicated out=5,504     interrupts=8,930        indirect buffers truncated=0
```

**2,783 frames in 90 s is ~31 fps.** The title was submitting draws and swapping the whole
time. `truncated=0` is the specific counter that would be nonzero if findings 37-39's
dropped-fence class had come back; it did not.

**4. The spin is the title's own idiom, and Xenia does it too.** A5 logs
`RtlEnterCriticalSection` **7,416,110** times across its **5,062** frames — **1,465 enters
per frame on the emulator**. Ours is 7,143,424 across 2,783 frames, **2,567 per frame**:
**1.75x**, the same order of magnitude, on a runtime rendering at half A5's frame rate.
"6.8 M enters in a minute" was never a hang signature; it is what this title's worker
threads cost per frame. A5 shows the identical `RtlEnter`/`RtlLeave` pair storm on thread
`F80000E4` in the hundred lines immediately after `XMsgStartIORequest`.

**5. With the renderer switched on, it draws the game.** `CW_VKDRAW=1` had **never been
run** on this port. First run, no changes to any renderer source:

```
[vk] device: NVIDIA GeForce RTX 3070 (Vulkan 1.4.341)
[host] first present: front buffer A0826000, guest says 1280x720
frame: has content 2,243   ·   frame: uniformly black 6
draw: handed to the renderer 922,541   ·   grep -c "no translated shader" = 0
```

The 439-shader cache covered **every** shader the frontend asked for on the first run —
that gate (`part2-kickoff.md` §2 item 3) passes untouched.

`CW_VK_FRAME_DUMP` writes the presented picture, which is how this was checked rather than
by a compositor screenshot (`grim` does not work under this KDE session, and the runtime's
own dump is the better instrument anyway — it captures what was *presented*, not what the
window manager composited). The dumps show:

- **frame 200: the Capcom logo**, correct colours, on black.
- **frames 800 onward: the Case West title screen** — Chuck and Frank on the clifftop over
  the lit Phenotrans facility, purple dusk sky, "PRESS START", the 2010 Capcom copyright
  line. Correct.
- **It animates.** ~26% of sampled pixels change between consecutive dumps, steady across
  fifteen dumps. That confirms finding 19's "the title screen is an animated 3D scene" from
  our own renderer, and rules out a frozen framebuffer being mistaken for a picture
  (gotcha 133: one frame is one sample — so this was measured over fifteen).

### What actually happened, and the lesson

The runtime was sitting on the **title screen waiting for someone to press START**. Polling
`XamInputGetState`/`XamInputSetState` in a loop while worker threads take and release locks
is *exactly* what that looks like from a kernel log. Every symptom in the kickoff's problem
statement was a correct observation of a healthy title screen.

The picture was missing because **the renderer is off by default** — `CW_VKDRAW` is unset
unless asked for, deliberately, so that it stays a true control arm — and the runs that
produced the "does not present a frame" note were `CW_NO_WINDOW=1` headless runs, which
print that they present nothing.

**This is finding 18's shape exactly, one part later.** There, a listing truncated to the
top 12 draws *by vertex count* hid every 4-vertex Bink quad, and the absence was read as
"no video texture". Here an absence of pixels was read as "the guest never gets there",
when it was a fact about **which flag was set on the run that was looked at**. The port's
recurring error is now five for five:

> **An absence is a fact about what was looked at, not about what exists.** Before an
> absence becomes a work item, ask what would have had to be switched on for the thing to
> appear at all.

And the corollary this one adds, which is the cheap half: **ask the oracle whether it shows
the symptom too.** A5 answered "the emulator does 1,465 lock enters per frame" in one grep,
and that single number would have retired the whole investigation before it started.

### What this changes

- **W4 "first picture" is DONE**, and was already done when part 2 opened — it needed a
  flag, not a fix.
- The kickoff's ordered list in §2 is void from item 1. The next real work is downstream of
  a rendering frontend, not upstream of a hang.
- **The `float16_4` / unrecognized-instruction gaps (W0.3) did not block the frontend.**
  They remain silent wrong-execution traps and still need doing, but they are not on the
  critical path they were assumed to be on.
- Nothing here has been checked past the title screen. **Press START and the measurements
  start over** — this finding covers the frontend and says nothing about gameplay.

---

## 28. **THE PORT PLAYS.** Gameplay, cinematics, HUD — from the operator's own drive — part 2, 2026-08-16

Finding 27 established that the frontend renders and closed with "nothing here has been
checked past the title screen." The operator then drove one, and it went the whole way.

**One session, 20,765 frames, exited cleanly (code 0):**

```
frames(XE_SWAP)=20,765   draws=26,276,829 (~1,265/frame)   predicated out=110,708
pm4 packets=363,011,078  interrupts=68,691  indirect buffers truncated=0
frame: has content 20,448  ·  uniformly black 152  ·  draws handed to renderer 26,236,678
```

**173 frame dumps sampled every 120 frames. Four are uniformly black**, each an isolated
single dump with a full scene in the dump either side of it — load/transition moments, not
a rendering failure. The rest carry 400–1,700 distinct sampled colours.

What the dumps show, and it is the whole game loop:

- **Cinematics** — Frank West and Chuck Greene in the caged corridor, with **subtitles
  rendering correctly**: *"Well heads up, rookie. We've got more on the way."*
- **Gameplay** — Chuck in the Phenotrans offices, third-person, with the **complete HUD**:
  LV/PP/LIFE, the weapon wheel, the partner portrait, the kill counter, the objective
  banner, and a context button prompt.
- **The pause menu** — the Phenotrans terminal with its chemical-structure overlay, ID
  badge, USB stick and syringe, all correct.

Draw load is **~1,265 per frame in gameplay against ~410 on the title screen**, which is the
scale difference the renderer was never previously exercised at.

### What is NOT clean, and what each thing means

**1. Two pixel shaders missing from the cache.**

```
[vk] no translated shader for PS e996eabf1264e235 — draws skipped
[vk] no translated shader for PS 37a7720a1f1710ec — draws skipped
```

Exactly two, out of a 439-shader bank, across a full gameplay session. This is finding 20's
rule holding: **new geometry is not new shaders; a new material set is** — and a drive that
reached material the 13 captures never did found two. It is one log line and a silent
counter by design, so it must be grepped for and never assumed. The bank is rebuilt from our
own runtime's dump now that one exists, which `CLAUDE.md`'s Commands section already says is
the authority on the byte range.

**2. HUD and menu text is damaged — PARKED, NOT DIAGNOSED HERE.**

The objective banner and kill counter render with dark ghosting behind the glyphs, and the
pause menu's labels come out as overlapping, unreadable strings. **Cinematic subtitles are
unaffected and render perfectly**, so it is specific to the HUD/menu text path.

**This is not to be investigated or fixed in this port right now.** The operator is working
the same defect in Case Zero (2026-08-16) and will say when to import the fix. Recorded here
so the symptom is on the record with the session that showed it, and so nobody re-derives it.

**A candidate mechanism was considered and is NOT being claimed.** `vk_renderer.cpp`'s
`CW_VK_TEX_REFRESH` comment says it was built for "a font atlas the CPU keeps writing", which
fits overlapping glyphs well — but the texture guard was **already on and already repairing**
in this very run:

```
texture guard: 49,098,926 cache hits checked, 4,847 served an image whose guest bytes had
CHANGED (0.01%), 4,847 RE-UPLOADED
```

Guard + revalidate have been default-on since Case Zero part 38, so every stale hit in this
session was repaired. That **weakens** the font-atlas explanation rather than supporting it,
and naming it as the cause would be this port's characteristic error one more time —
attributing from a plausible mechanism instead of a measurement (findings 3, 23). It stays an
unattributed symptom until the Case Zero work says otherwise.

**3. Nothing else failed loudly.** A grep for `unsupported|unimplemented|refus|FATAL` returns
682 lines, and **all 682 are false positives**: 681 are the ring trace's own
`fenceRegressionsRefused=` counter and one is the dispatch table's `refused` count. No
unsupported packet, format or import fired — which is the gotcha-5 design working, since any
of those would have named itself.

### Where this leaves the port

The single-player game boots, renders, plays, cinematics run, saves were already confirmed in
part 1, and Bink needs no host code. **The gap between here and "playable end to end" is now
a defect list, not a bring-up list** — and the first item on it belongs to Case Zero.

`truncated=0` across 363 M packets is the counter that would have caught findings 37-39's
dropped-fence class returning at gameplay scale. It did not return.
