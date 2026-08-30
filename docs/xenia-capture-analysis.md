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

---

## 29. **Intro to safehouse, playable — and in one place BETTER than the oracle** — operator, 2026-08-16

**Provenance: this is the operator's own play session, reported directly.** It is not a
number measured off a log, and it is recorded as what it is — a behavioural observation from
the person driving, which in this port has repeatedly been better evidence than what was
asked for.

What they did and what happened:

- **Played the whole intro through to the safehouse**, including **zombie combat**.
- **"Runs pretty much exactly like Xenia"** — the first end-to-end behavioural comparison
  this port has against its reference, and it is a pass.
- **Saved and loaded, and it worked perfectly.** Part 1 confirmed the save layer *statically*
  from A3 (`content.cpp` needs no functional change, finding 24). This is the first
  **behavioural** confirmation: a real save written and read back inside a real session.
- **In one place it is BETTER than Xenia: a cutscene whose dialogue Xenia CUTS OFF partway
  plays through correctly on our runtime.**

### That last point is a methodological change, and it is why this finding matters

Every measurement discipline in this project treats the captures as ground truth. That is
still right **for what the guest did** — kernel call order, file reads, PM4 packets, shader
microcode — because those are the guest's own bytes, recompiled from the same image.

It is **not** automatically right for the **output**. Audio continuity, timing and
presentation are the emulator's own work, and the emulator can be wrong. Here it is: Xenia
truncates dialogue that our runtime plays in full.

Three consequences, now written up as **gotcha 320**:

1. **"It differs from the capture" is a question, not a verdict.** Ask whether the capture is
   the thing that is wrong before spending a part on the difference.
2. **Keep the two kinds of ground truth apart.** Guest-side bytes: the capture wins. Emulator
   output: it does not automatically win.
3. **Do not fix a pass into a match.** An A/B that scores "closer to Xenia" as better will
   regress the dialogue we already get right. Score against the *game's* intended behaviour,
   and record the axis where the oracle is known-worse so a later part does not helpfully
   undo it.

This also retires, in passing, a worry the port never got to test: the **XMA audio** path and
the **cinematic** path both work well enough to carry a full intro — and gotcha 267, the
physical-address DMA trap that cost Case Zero its entire audio subsystem for 28 parts, did
not bite here.

### Still open after this session

- **The HUD/menu text defect** (finding 28) — the operator's own work in Case Zero; not to be
  investigated here.
- **Two missing pixel shaders** (finding 28), to be picked up by rebuilding the bank from our
  own runtime's dump.
- Everything past the safehouse is unmeasured.

---

## 30. **The GPU determinism baseline: 1.40%.** The noise floor exists at last — part 2, 2026-08-16

`docs/part2-kickoff.md` §3 listed this as owed and said why it blocks everything downstream:
*"Until it runs, this port has no noise floor for GPU stream comparison — and Asura's Wrath
spent real time treating noise as signal for exactly that reason."* `tools/xtr_determinism.py`
had been copied across but **never run here**. It has now been run, on B1 vs B1b — two
hardware captures of the same boot drive.

```
B1    866 frames  0.49 GiB (11.4 s)
B1b   911 frames  0.52 GiB (12.1 s)
```

### The baseline

```
comparison window: FIXED PREFIX  B1[0:257]  B1b[0:258]   (final era excluded)

worst aggregate delta : 1.40%  (op2D, SET_CONSTANT)
draws delta           : 0.30%
indirect buffers      : 0.09%
```

**Two hardware runs of one drive differ by 1.40% on the worst aggregate.** Our runtime
landing inside that band is **not** evidence of correctness; landing outside it is evidence
of a defect. That is the number every later GPU gate is measured against.

### Three things this settles that were open

**1. The ~6% size delta was never a verdict — confirmed.** Finding 12 recorded the delta and
refused to call it. The size ratio here is 0.939 and it is **not a determinism metric**: a
continuous stream emits frames for as long as the run lasts, so size mostly measures how long
the operator sat on a menu. The final era is 609 frames in B1 and 653 in B1b — that is when
each run was exited, which is why the tool excludes it. Including it reads 42.7% instead of
80.0%.

**2. A frame-INDEXED GPU gate is not viable on this title.** Frame-exact agreement is only
**87.9%** even over the fixed window, and **69.6%** naively:

```
naive frame-i vs frame-i : 179/257 (69.6%)
aligned (insert/delete)  : 226/257 (87.9%)
longest identical run    : 128 frames
phase lag: +2 on 77.0% of frames, +0 on 8.8%, +1 on 7.1%
```

A small set of small lags covering most frames is **drift, not divergence** — deterministic
in content, jittery in phase. So future GPU gates must compare **per-era aggregates**, which
are robust to drift, and must not compare frame *i* to frame *i*.

**3. `MemoryRead`/`MemoryWrite` counts must stay out of the fingerprint.** They are Xenia
deciding which guest memory still needs recording — its own dirty-tracking, not the guest's
behaviour. Folding them in manufactures non-determinism, measured at **42.7% → 16.0%** frame
agreement. They are reported and explicitly excluded from the verdict.

### The era segmentation, and a sibling constant that actually transferred

The tool segments a run into draw-count regimes and its `--era-threshold` default is
`50 130 600`, **measured on Case Zero's boot** — another inherited constant of exactly the
kind that has bitten this port twice (`find_jumptables.py`, `fix_switch_function_bounds.py`).
So it was checked rather than assumed.

The eras land almost identically on the two runs, which is itself the property that matters:

```
era mean draws/frame   B1 : 26.0  26.0  56.0  80.0  48.0  87.6  140.0  194.0  455.2  797.5
                      B1b : 26.9  26.0  46.0  80.0  52.4  87.6  140.0  194.0  457.2  796.3
```

And the baseline is **insensitive to the threshold**. Re-run at `40 100 500`, `60 150 700`
and `30 90 300 700`, the comparison window is `[0:257]/[0:258]` **every time** and the verdict
is 1.40% / 0.30% / 87.9% unchanged. So Case Zero's constant is harmless here — and this is the
**first inherited constant in this port that has been shown to transfer rather than assumed
to**. That is the whole difference between the two tools that were silently wrong in part 1
and this one: somebody varied it.

### What is still owed on the GPU side

- **B2's 14.8 GiB gameplay stream has never been decoded.** It is the gameplay PM4 ground
  truth and it is *newly* actionable, because there is now a renderer to compare it against.
- **Eight of the nine B4 frames are unanalysed** — the reference set for the hardest surface
  classes.

---

## 31. **The coverage traces have a 124 KB hole — and a positive control is the only reason we know** — part 2, 2026-08-16

### The question that led here

W0.3 lists 59 sites XenonRecomp could not translate: 39 unrecognized instructions over six
mnemonics plus 20 `float16_4` pack sites. Re-measured on the current `ppc/`, both counts hold
**exactly**:

```
14 vminsw   13 vpkshss   8 vavgsw   2 stdux   1 vpkshss128   1 stvebx   = 39
20 x "Unexpected float16_4 pack instruction"
```

An unimplemented instruction is not a build failure and not a crash — the emitted function is
missing that instruction's effect and runs anyway, so it is a **silent wrong-execution trap**.

**But 59 sites are not 59 problems: they land in exactly SEVEN functions.**

```
sub_825D75B0  28 sites  vminsw / vpkshss / vpkshss128   saturating vector pack
sub_825D9B50   1 site   stvebx
sub_825E0290   4 sites  vavgsw    (references vperm byte tables at 0x82095120)
sub_825E05D0   4 sites  vavgsw    (sibling of the above)
sub_825E6808  20 sites  float16_4 pack — the whole cluster is ONE function
sub_825EB5C0   1 site   stdux
sub_825ED6C0   1 site   stdux
```

`docs/port-plan.md` guessed the `float16_4` cluster was "probably one guest function". It is.
`sub_825E6808` gates on `r6 == 4` and `r4 == 2` before doing its work — a component-count and
type check, i.e. a **vertex/element format converter**, which is what a half-float packer
should look like.

So the real question is not "how do we implement six mnemonics" but **"do these seven
functions ever run?"** A function that never executes carries no risk, and XenonRecomp is
**shared with the other three ports**, so changing it is not a unilateral call.

### The answer the coverage traces give is WRONG

Asked of C1 (11,680 executed functions) and C2 (19,484), all seven come back **never
executed**. That is a clean, plausible, decisive-looking answer, and acting on it would have
retired W0.3 entirely.

**The positive control refutes it.** Functions with independent evidence that they execute:

```
sub_825B5B90   EXEC in C1 and C2   vblank callback — our own log shows it called 8,060x
sub_825D7D20   NOT FOUND           XGI user-context builder — kernel/imports.cpp derived its
                                   struct layout by READING THIS FUNCTION, and our runtime
                                   sets XGI contexts at boot every run
sub_825D9358   NOT FOUND           the XamTaskSchedule callback A1 explicitly schedules
sub_825D91E0   NOT FOUND           pre-arms the overlapped (imports.cpp)
sub_825C4400   NOT FOUND           XUserWriteAchievements setup (imports.cpp)
```

Four of five controls are missing from a trace that holds 19,484 functions. So the traces are
not a census of what executed in that address range.

### The hole, localised

Executed-function density per 64 KB of `.text` in C2:

```
0x825A0000   189
0x825B0000   236
0x825C0000    82
0x825D0000     8      <- and all 8 are at 825D0A60..825D1028, the very start
0x825E0000     0      <- nothing at all
0x825F0000    81
```

**Zero executed functions between 0x825D1028 and 0x825F0000** — a ~124 KB stretch that
contains **all seven** gap functions *and* the XAM wrapper code that demonstrably runs.

### Why this matters beyond W0.3

**The 104 recovered entry points came from these traces.** `coverage_to_function_overrides.py`
uses hardware execution as a function-discovery oracle, and an oracle with a 124 KB blind spot
cannot have discovered anything there. Whatever indirect-call targets live in that region were
never proposed, and nothing has noticed because the tool reports what it found, not what it
could not see. That is gotcha 3 in its purest form — **a zero is a detection failure, not a
fact** — and it is the third time in this port that a detector's *range* was the thing that
was wrong.

The cause is not established. The region's low-address neighbours are covered normally, so it
is not a truncated trace; a 32 MiB preallocated buffer that the tool already trims suggests
capacity is not it either. **Not chased further, because the question it was blocking now has
a better instrument.**

### The instrument that replaces it

`runtime/cpu/gap_probe.cpp` — a strong `PPC_FUNC(sub_X)` over each of the seven, forwarding to
`__imp__sub_X` after one relaxed atomic increment, reported at exit from **both** shutdown
paths (the SIGTERM handler and the window-close path, which is the one an operator session
actually takes and which `window.cpp`'s own comment records losing a whole evening's census
to).

**It has been shown capable of counting (gotcha 30):** adding `sub_825B5B90` as an eighth row
made it report **11,267** calls on a 70 s boot while all seven real rows stayed 0. The control
row was then removed.

**Current reading, boot to title screen: all seven are 0.** That is an absence about a drive
that never left the title screen — exactly the kind this port has misread five times — so the
report says so in its own output rather than claiming a clearance. **The number that matters
comes from a gameplay drive**, and the probe is always on, so the next one produces it for
free.

---

## 32. **W0.3's seven functions did not execute in a full play session — and the bank is 443** — part 2, 2026-08-16

The operator's session with the imported UI fix carried three instruments at once. Two of
them answered questions that were open.

### The gap probe, on a real gameplay drive: still all zero

`runtime/cpu/gap_probe.cpp` (finding 31) counts calls into the seven functions holding the 59
instructions XenonRecomp cannot translate. On a session that reached **20,164 frames with
content** and peaked at **3,029 draws/frame**:

```
sub_825D75B0   0    vminsw/vpkshss/vpkshss128   28 sites
sub_825D9B50   0    stvebx                       1
sub_825E0290   0    vavgsw                       4
sub_825E05D0   0    vavgsw                       4
sub_825E6808   0    float16_4 pack              20
sub_825EB5C0   0    stdux                        1
sub_825ED6C0   0    stdux                        1
                                          TOTAL  0
```

**This is still an absence and it is still not a clearance** — the probe says so in its own
output, deliberately. But it is a *much better covered* absence than the title-screen reading
it replaces: a play session through gameplay, cinematics, the HUD and the pause menu, at
gameplay draw rates, touched none of the seven.

**What that changes:** W0.3's 59 sites are not a live corruption risk on the paths played so
far, so the shared-XenonRecomp work they imply is **not urgent** and does not block anything.
It is not retired — the honest statement is "not reached yet", and the probe is always on, so
every future session tests it again for free. The instrument was shown capable of counting
before this reading was taken (an eighth control row reported 11,267 calls); a zero here is
the absence of the call, not the absence of an instrument.

### The shader bank: 439 → 443, and the misses are closed

The same session ran `CW_SHADER_DUMP`, which is the first time this port's bank has been
rebuilt from **our own runtime's** microcode rather than the captures':

```
before          439 .spv
3 misses this session:  PS 0af111338e3bbec4   PS 439de904cbd3264f   VS cb8791c5c3e244df
after rebuild   443 .spv, 443 translated, ZERO failures
shader_dim_census.py: 347 2D / 112 cube, "agree on every shader", exit 0
```

All three missed hashes are now present. Note the misses were **different hashes from the two
in finding 28's session** — so this is finding 20's rule still holding: a drive that reaches
new material adds a few shaders, and the count creeps rather than jumps. Five distinct misses
across two full play sessions, against a bank of 443, is a bank that is very nearly complete.

**Why rebuilding from our own dump matters, and not just for the count.** `CLAUDE.md` records
that the runtime's own dump is the authority on the byte range, because the cache key hashes
it — a bank built only from Xenia's dumps could in principle key on a different span and miss
on shaders it actually holds. This rebuild is the first time that authority has been used
here, and the census agreeing on all 443 says the two decodes do not disagree.

### The third instrument

`CW_VK_FRAME_DUMP` also ran; its output is what priced the imported UI fix at 12 MB/frame in
gameplay against 1.0 at the title screen (`docs/imported-fixes.md`).

---

## 33. **CASE 1-3 COMPLETE, NEW AREAS, AND PAST THE ORACLE** — operator, 2026-08-16

**Provenance: the operator's own play session**, the one that carried the imported UI fix and
three instruments (findings 32 and `docs/imported-fixes.md`).

> *"In this run I reached new areas and completed case 1-3 which is further then what I
> reached in xenia and didn't have any issue except [decals and performance]."*

### What this establishes

**The port completes Case 1-3.** Not "boots", not "renders", not "plays the intro" — a
multi-case progression through new areas, with the HUD working, on a build whose only
reported defects are both **inherited from Case Zero and already known there**.

**And it went FURTHER THAN THE CAPTURE SET.** This is the consequence that matters for
method, and it is new:

> **From Case 1-3 onward, this port has NO Xenia ground truth, because the operator never
> drove Xenia that far.** Every capture in round 1 — A2, B2, C2 and the rest — stops short of
> where the runtime now goes.

That is not a gap to fill by asking for captures. It is a **permanent change in what evidence
is available for late content**, and it compounds gotcha 320 (*Xenia is not a ceiling*) into
something sharper:

- **For content the captures cover**, the guest-side bytes are still ground truth: kernel
  call order, file reads, PM4 packets, shader microcode.
- **For content past them, there is no oracle at all.** No capture, no A/B against hardware,
  no "does Xenia show this too?". The only evidence is the game's own intended behaviour,
  the engine's internal consistency, and the operator's eyes.
- So **a defect found in late content cannot be triaged the cheap way** the earlier ones were.
  Findings 27 and 29 were both settled in one grep against a capture; that move is gone past
  Case 1-3. Budget accordingly, and prefer defects that reproduce EARLY — gotcha 319, *prefer
  a repro that already has an oracle in the repository*, is now load-bearing rather than a
  convenience.

### The two defects, and neither is ours

Both were reported as pre-existing and **both are open items in Case Zero**, so neither is to
be investigated here (see `docs/imported-fixes.md`):

| defect | Case Zero item | state there |
|---|---|---|
| **Decals not showing properly** — minor visual | **00m** | operator-reported at part 47, **explicitly NOT investigated**, no captures requested; theirs to characterise after the performance work |
| **Performance** | **00l / parts 47-48** | actively being worked; part 47's A/B reached the two-vblank pacing floor on the headless route |

**"No issue except" those two, across new areas and a full case, is the headline.** No crash,
no hang, no missing subsystem, no unsupported-format complaint.

### One thing this session already banked

The new areas meant new material, and `CW_SHADER_DUMP` was on — so the bank rebuild in
finding 32 (**439 → 443**) already includes them. That is why a session reaching new places
produced only **three** misses: the rebuild consumed the very dumps that session wrote. Any
future drive into genuinely new material should keep the dump on for the same reason.

---

## 34. **B2 DECODED — 14.46 GiB of gameplay PM4, and our command processor covers all of it** — part 2, 2026-08-16

B2 was captured on 2026-08-15 and had **never been decoded**. `docs/part2-kickoff.md` §3 listed
it as owed and said why it was waiting: *"it is the gameplay PM4 ground truth, waiting on a
renderer to compare against."* There is a renderer now, and a noise floor (finding 30), so it
was decoded.

### The stream is intact

`tools/xtr_walk.py stats`, 221 s:

```
15,527,183,189 bytes (14.46 GiB)   header version 1, title 58410B00
437,132,465 commands   ·   12,766 frames   ·   1,187.8 KiB of stream per frame
   208,864,949 PacketStart      14,559,128 MemoryRead      372,822 IndirectBufferStart
     4,028,736 MemoryWrite          28,147 PrimaryBufferStart
             1 EdramSnapshot            1 Registers            1 GammaRamp

integrity: head walkable from 48, NO DESYNCS — the whole stream parsed as a
           single sequence.  verdict: INTACT
```

A 14.46 GiB capture parsing end to end with zero desyncs also re-proves the 2 GiB `.xtr`
cliff fix at ~7x the old limit, which B2's notes flagged as a side benefit.

### The PM4 census, and the decoder's own self-check

`tools/xtr_pm4_census.py --verify`, 291 s:

```
208,864,949 packets  =  95,285,671 type0 (register write runs)
                      + 50,541,798 type2 (nop)
                      + 63,037,480 type3 (opcode)

21 DISTINCT TYPE-3 OPCODES, and ZERO the decoder cannot name.
```

```
16,556,491  0x60 SET_BIN_MASK_LO      2,653,182  0x36 DRAW_INDX_2      50,342  0x62 SET_BIN_SELECT_LO
12,681,319  0x22 DRAW_INDX            2,457,680  0x3B INVALIDATE_STATE 12,769  0x21 REG_RMW
 8,619,389  0x46 EVENT_WRITE          1,048,128  0x3C WAIT_REG_MEM     12,767  0x63 SET_BIN_SELECT_HI
 7,840,564  0x5A EVENT_WRITE_EXT        423,855  0x61 SET_BIN_MASK_HI  12,766  0x64 XE_SWAP
 3,674,341  0x27 IM_LOAD                411,088  0x2D SET_CONSTANT        256  0x45 COND_WRITE
 3,340,647  0x2F LOAD_ALU_CONSTANT      372,816  0x3F INDIRECT_BUFFER        1  0x48 ME_INIT
 2,706,085  0x2B IM_LOAD_IMMEDIATE      108,454  0x58 EVENT_WRITE_SHD
                                         54,540  0x54 INTERRUPT
```

**The self-check passed on 63,037,480 type-3 packets with 0 unexplained length mismatches.**
Two independent things encode the same length — the trace's own `PacketStart.count` and the
PM4 header's count-1 field — so this is the decoder's bit layout being validated against
Xenia's, on this title, at scale. That matters because a wrong shift would land opcodes on
*other real opcodes* and produce a confidently wrong histogram (gotcha 30 applied to a
decoder). The one allowance is documented and narrow: Xenia records `0x3F INDIRECT_BUFFER`
one dword short.

### **The result that closes a scope question: we implement all 21**

Every opcode B2 sends has a case in `runtime/gpu/pm4.cpp`:

```
0x21 0x22 0x27 0x2B 0x2D 0x2F 0x36 0x3B 0x3C 0x3F 0x45 0x46 0x48
0x54 0x58 0x5A 0x60 0x61 0x62 0x63 0x64      -> 21/21 HANDLED
```

**The check was controlled** (gotcha 25 — a grep that cannot match is not a clean result):
`0x99`, `0x7F` and `0x13` all report NOT FOUND, so the test can fail.

So **the command processor's opcode surface for gameplay is closed**, measured rather than
assumed. Nothing the guest sends over a full gameplay drive is unimplemented. That is the
question B2 was captured to answer.

### The per-frame draw profile

```
12,731 of 12,766 frames draw (99.7%)
p5 176 · p25 800 · p50 1,169 · p75 1,511 · p90 1,949 · p95 2,621 · p99 4,692 · max 5,944
mean over drawing frames: 1,204
```

**Our runtime sits in the same distribution.** Finding 28's session was 26,276,829 draws over
20,765 frames = **1,265/frame mean against hardware's 1,204** — within ~5% — and the Case 1-3
session's sampled windows (1,384 / 1,443 / 3,029 draws/frame) all fall between hardware's p50
and p95.

**This is a scale agreement, NOT a matched A/B, and must not be quoted as one.** The evidence
rules require two arms of ONE renderer producing the SAME draw set; these are different
drives of different content on different implementations. What it establishes is that our
renderer's per-frame workload is the same *shape* as hardware's — which is the sanity check
that would have caught a renderer dropping or duplicating whole classes of draw, and it
passes. Finding 30's 1.40% noise floor does **not** apply here; it is a same-drive figure.

### Two things worth noting for later

- **`SET_BIN_MASK_LO` is the most common type-3 opcode in the game — 16.5 M, more than
  `DRAW_INDX`'s 12.7 M**, i.e. ~1.08 bin-mask writes per draw. The two-tile binning is a
  per-draw decision at gameplay scale, not an occasional one, which is the context
  `gpu/vd.cpp`'s ring trace already argues for reading `predicated out=` next to the draw
  count rather than alone.
- **50.5 M type-2 NOPs** — a quarter of all packets. Consistent with finding 39's swap
  padding idiom, which the title uses deliberately and which our `VdSwap` reproduces.

### Now decoded, and what is still not

`docs/part2-kickoff.md` §3's GPU list is down to one item: **eight of the nine B4 frames
remain unanalysed** — the reference set for the hardest surface classes.

---

## 35. **THE PROGRESS-WIDGET DEFECT: a headless repro, and the guard refuted** — part 2, 2026-08-16

**Operator, 2026-08-16:** progress widgets do not work — *"the pp bar and time bar for mission
doesn't work as well as the little square showing progress when loading a save or something
like that that shows up in pop-up or when entering the main menu from the title screen."*
**Present in Case Zero too, and Case Zero has NOT started work on it**, so unlike decals and
performance (`docs/imported-fixes.md`) this one is ours to lead — and anything found here
feeds back to the sibling.

### The repro, and it is self-servable and headless — 40 seconds, no operator

This is the first thing to exist for this defect class in either port. Case Zero's own notes
record it as unreproducible without a human.

```
SEQ=$(python3 -c "print(','.join(['NONE']*17+['START','F9']))")
cd runtime/build && CW_VKDRAW=1 CW_NO_WINDOW=1 \
  CW_FAKE_START_MS=2000 CW_FAKE_PRESS_SEQ="$SEQ" \
  CW_CAPTURE_KEY=<dir> CW_VK_FRAME_DUMP=<dir> CW_VK_FRAME_DUMP_EVERY=5 \
  timeout 55 ./cw_runtime
```

START fires at 36 s, the **"Loading content. Please do not turn off your console."** dialog is
up for roughly frames **1110-1210**, and F9 at 38 s lands inside it and writes the picture,
the pose and an 816-draw per-draw census. Frame-dump stats identify the window without
looking: the title screen samples ~820 distinct colours, the dialog ~355.

**The dialog renders correctly — panel, drop shadow, both text lines, YES/NO — and the little
progress square is simply absent.**

### What is ruled out

**1. Not a missing shader.** `grep -c "no translated shader"` = **0** on the repro run.

**2. Not the ALU constant window.** The renderer reads `SQ_VS_CONST`/`SQ_PS_CONST` bases from
the registers rather than assuming 0/256; the counter that fires is informational.

**3. NOT THE STREAM STORE'S GUARD — refuted by A/B with a stated prediction.** This was the
strongest suspect: it is the mechanism behind the UI *text* defect (`docs/imported-fixes.md`),
and both widget families rewrite their geometry every frame, so stale streams would freeze a
meter and stop a flipbook animating.

> **Prediction stated before the run:** if the square is stale-stream, then
> `CW_VK_STREAM_GUARD_EXACT=1` — the unlimited arm, which hashes every byte of every stream
> and therefore cannot miss any edit — makes it appear.

It does not. The arm's dialog frame is **pixel-indistinguishable** from the control's, and the
inter-frame change analysis is identical (134 vs 137 changed samples over the same bounding
box). **The guard is not the mechanism here**, which also means this is a *different* defect
from the UI text one despite both being UI.

**4. Nothing square-shaped is animating at all.** Between consecutive dialog frames the
changed pixels are sparse and spread over the whole 448x418 dialog area — that is the animated
title-screen sky showing through a translucent panel, not a widget. So the square is not
"animating wrongly"; it is not being drawn.

### What the guest calls these widgets

The frontend is a named-widget system — `c:\bcg\deadrisingepilogue\source\Common\fe\screen\...`
— and the image names **39 `cFE*` classes**. Three matter:

| class | almost certainly |
|---|---|
| **`cFEMeter`** | the PP bar and the case-timer bar |
| **`cFEFlipBook`** / **`cFEFlipFrame`** / **`cFEAnim`** | frame-by-frame animated widgets — the loading square |

The widget instance is named **`loading_ind`**, sitting beside `w_loading`, `cFEText` and
`button_cancel` in the frontend string block at `0x820712E0`. It is **not** an entry in any of
the 154 `.big` archives (0 of 13,833 entries match), so it is an internal widget name, not a
packed asset.

**Note the two families are different classes.** The operator's report groups them because
they fail together, but `cFEMeter` and `cFEFlipBook` are not the same code — so "one defect"
is a hypothesis, not a finding, and it should be tested rather than assumed. **When an
attribution has been wrong twice, stop refining the estimate** (finding 23) — this one has not
been wrong yet, and the way to keep it that way is to make the next step a fact rather than a
mechanism.

### There is no oracle for this frame, and that was checked

Per gotcha 321 the first question is whether hardware evidence exists. **It does not:**

- **B1's boot stream tops out at 814 draws/frame and its last 30 frames sit at 787-805 with no
  transition** — its drive ends at the title screen, so the dialog is not in the capture. Our
  dialog frame is 816 draws, *above* B1's maximum.
- **The four boot screenshots are the ESRB, Capcom, Blue Castle and Dolby logo cards** — none
  is the dialog.

So an early, cheap-to-reach screen is nonetheless past the oracle. **This is the case for
asking the operator for a capture**, and the drive is trivial: boot, press START, F4 at the
"Loading content" dialog, plus one F4 on a gameplay frame with the PP bar and case timer
visible. That would give the first hardware picture of what the widgets should look like AND a
frame `.xtr` to diff draw-for-draw.

### Next steps, in order

1. **Ask for the capture above.** Everything else is cheaper with it and some of it is
   impossible without it.
2. **Decide "is it issued at all?" as a true-or-false fact**, not from a picture: the
   `CW_VK_DRAW_ID` pass paints each draw its own index for one armed frame, so a draw covering
   the square's rectangle either exists or does not. It needs the square's screen rectangle,
   which is what the hardware capture supplies.
3. **`cFEMeter` is the better first target than the square** — the PP bar has a *number* behind
   it, and Case Zero already recorded that a valid headless metric must watch a **HUD number
   change**, not a widget's presence (its part 24 built the presence metric and had to retract
   it: it tracked where Chuck was standing). A meter whose backing value is known is testable
   in a way an animation is not.

---

## 36. **ONE VERTEX SHADER DRAWS ALL THE BROKEN WIDGETS — `vs_a4ae7c2b7c1818c4`** — part 2, 2026-08-16

Finding 35 ended by asking the operator for hardware pictures of the widgets, because B1 tops
out below the dialog and no screenshot existed. **They delivered `R2_ui_bars` the same day:**
two single-frame F4 `.xtr` traces with frame-locked PNGs, md5-verified on copy-in.

### First: the symptom is sharper than "a square is missing"

The hardware PNGs show what these widgets actually are:

- **Loading pop-up:** a **SEGMENTED bar** — roughly 28 small grey squares in a row with **one
  blue segment lit**, caught mid-fill. Not a spinner, and not a solid bar.
- **HUD:** `LIFE` is **5 filled yellow squares plus 2 EMPTY outlined squares**; the PP bar is a
  partially-filled cyan bar; the mission bar has a **thin progress line** under
  "Case 1-2: Access Codes".

The operator's own words for the part I had missed: *"for the empty health point you can also
see a empty square that we do not have on our end."* **We draw the filled pips and omit the
empty ones.** So the class is not "progress bars are broken" — it is closer to **the unlit /
track / empty segments of segmented widgets are absent**, plus the lit segment in the pop-up.

### The measurement that unifies them

Both hardware frames were decoded (`xtr_draw_bindings.py --csv` — `--csv`, not the top-12
default, which is the finding-18 trap) and diffed against our own per-draw census of the
matching screen.

**Loading pop-up, hardware vs ours:**

```
shader pairs: hardware 39, ours 31, SHARED 31, ours-only 0
pairs hardware draws that we never draw:  8, totalling 27 draws
  and ALL EIGHT SHARE ONE VERTEX SHADER: vs_a4ae7c2b7c1818c4
```

Every pair we draw, hardware draws. The entire difference is 27 draws on **one vertex
shader** — against a bar with ~28 grey segments plus one lit.

**And the same shader draws the HUD widgets.** In the PP/mission frame,
`vs_a4ae7c2b7c1818c4` accounts for **55 of 2,175 draws**, and the indices cluster exactly as
the picture does — **draws 887-893 are seven consecutive draws**, and `LIFE` on screen is
**5 filled + 2 empty = 7 squares**.

**So one vertex shader draws the loading bar, the LIFE squares, the PP bar and the mission
line.** That is a true-or-false fact about a named object rather than an estimate about a
count — which is the standard finding 23 set after the mutants were mis-attributed twice.

### What is NOT the cause

- **Not a missing shader.** `vs_a4ae7c2b7c1818c4.spv` and its `.meta.json` **are in our bank**,
  and `grep -c "no translated shader"` is 0.
- **Not an untranslated primitive.** Line list and line strip are both mapped; no unsupported
  primitive fired; our stream contains **zero** line primitives.
- **Not the stream store's guard** — refuted by A/B in finding 35.
- **Our runtime does load this shader**: `[imload] VS va=00000000 hash=a4ae7c2b7c1818c4
  size=15`. The `va=00000000` is expected for `IM_LOAD_IMMEDIATE`, which carries its microcode
  inline (2.7 M of them in B2), and the 60-byte `.ucode` in our dumps is dated **16 Aug**, i.e.
  written by **our own runtime**, not by a Xenia capture.

**So the shader exists, is translated, and is loaded — and in our frame not one draw uses it.**

### The confound, stated rather than buried

**The two loading-pop-up frames are NOT a matched pair.** The operator's drive **loaded a
progressed save**; my headless repro presses START with **no profile**, which raises an extra
"You are not currently signed into a gamer" panel. Different guest state can legitimately
produce a different draw set, so the loading-frame comparison is **suggestive, not
admissible** under this project's own A/B rule.

**The HUD frame does not have that problem as evidence of the defect** — the operator sees the
PP bar, the empty LIFE squares and the mission line broken in ordinary play, and hardware
renders them with this shader. What is still missing is **our side of that same frame**.

### The next measurement, and it is one keypress

**Our own per-draw census of a gameplay frame with the HUD on screen.** If
`vs_a4ae7c2b7c1818c4` appears there with ~55 draws, the draws are issued and the defect is
downstream (geometry, constants, blend, or the pixel shaders paired with it — note the eight
missing pairs use *eight different* pixel shaders, which argues the vertex side is the common
factor). If it appears with **zero**, the draws are never issued and the cause is upstream of
the renderer entirely.

That is one F9 press in a normal session:

```
cd runtime/build && CW_VKDRAW=1 CW_CAPTURE_KEY=<dir> ./cw_runtime
#   play to anywhere the HUD shows LIFE + PP, then press F9
```

It writes the picture, the pose and a full per-draw census. **The same press at the safehouse
bathroom save point makes it a matched pair with `pp_mission_bar.xtr`.**

---

## 37. **THE DRAWS ARE NEVER ISSUED — and the frame that proved it nearly proved nothing** — part 2, 2026-08-16

Finding 36 asked for one thing: our own per-draw census of a gameplay frame with the HUD up,
to decide whether `vs_a4ae7c2b7c1818c4`'s draws are *issued and broken downstream* or *never
issued at all*. The operator pressed F9 and delivered frame **2387**.

```
our frame 2387 : 2,783 draws  ·  draws using vs_a4ae7c2b7c1818c4:  0
hardware frame : 2,175 draws  ·  draws using vs_a4ae7c2b7c1818c4: 55
```

### The trap in this frame, which is the port's own recurring one

**Our frame 2387 does not contain the LV/PP/LIFE cluster at all.** Not "renders it wrongly" —
the top-left is bare wall. Measured rather than eyeballed, over the region the cluster occupies
on hardware:

```
region x70-390 y45-95      hardware mean 63, MAX 248   (bright yellow LIFE pips, cyan PP bar)
                           ours     mean 85, MAX 115   (wall; no bright element anywhere)
```

Case Zero already recorded why: **partial HUD is context-dependent** — its part 24 built a
headless metric counting frames where the LIFE pips were absent, got a beautifully
reproducible 69%, and had to retract the whole thing because the metric tracked *where Chuck
was standing*.

So a "0 draws" reading taken from this frame alone would have been **exactly this port's
characteristic error for the sixth time**: an absence that is a fact about what was looked at.
The claim "the draws are never issued" was one step from being published off a frame that
contains no such widget.

### What rescues it: the mission bar IS in both frames

`Case 2-1: Chuck's Evidence` is on screen in ours; `Case 1-2: Access Codes` in hardware's.
Same widget, both frames. Scanning brightness per row across the bar's track:

```
HARDWARE  rows 257-262   mean 191-193   <- a bright, full-width progress line
OURS      rows 256-263   mean  61- 63   <- flat dark; no line at all
```

**So there is a widget demonstrably present on our screen whose progress line hardware draws
and we do not — in the same frame whose census contains zero `vs_a4ae7c2b7c1818c4` draws.**
That makes the reading admissible for the mission bar, and the LIFE/PP half is simply not
answered by this capture.

### The answer, scoped to what was actually measured

**For the mission progress line: the draws never reach the renderer.** They are not issued
wrongly, not culled by topology, not missing a shader, and not served a stale stream — they
are absent from a per-draw census that lists every draw reaching `DoDraw`.

**One distinction this does NOT yet make**, and it matters: the census is written *inside* the
renderer, so a draw dropped earlier — predicated out by the bin mask in `gpu/pm4.cpp` — never
reaches it and is indistinguishable here from a draw the guest never sent. B2 measured
`SET_BIN_MASK_LO` as the **most common type-3 opcode in the game** (16.5 M, more than
`DRAW_INDX`; finding 34), so this is not a remote possibility.

### The next measurement, and it separates those two in one run

```
cd runtime/build && CW_VKDRAW=1 CW_RING_TRACE=1 CW_PM4_BIN_CENSUS=1 \
  CW_CAPTURE_KEY=<dir> ./cw_runtime  2> run.log
#   play to ANY frame where LV / PP / LIFE are visible top-left, then press F9
```

- `ring: pm4 ... (predicated out=N)` and the bin census say whether draws are being discarded
  before the renderer, and by which bin-mask pair.
- **A frame with the top-left HUD actually raised** also settles the LIFE/PP half, which frame
  2387 could not.

Both readings come from one session. If `predicated out` is large and the bin census names a
pair, the defect is ours in `pm4.cpp`; if it is ~0, the guest is not submitting these draws and
the cause is upstream of the GPU entirely.

---

## 38. **CONFIRMED ON A FRAME THAT HAS THE WIDGETS: zero draws, and the bar renders BLACK** — 2026-08-16

Finding 37 could not answer the LIFE/PP half, because the frame it was given did not contain
that cluster. The operator took a second capture — **frame 8724** — and this one does.

```
HUD region x70-390 y45-95     hardware MAX 248 · frame 2387 MAX 115 · FRAME 8724 MAX 249
frame 8724: 2,625 draws   ·   draws using vs_a4ae7c2b7c1818c4:  0
hardware  : 2,175 draws   ·   draws using vs_a4ae7c2b7c1818c4: 55
```

**The HUD is raised (MAX 249 against hardware's 248) and not one of 2,625 draws uses the
shader.** The finding-37 caveat is now discharged: this is no longer an absence about what was
looked at.

### The picture, measured with its own positive control

Mean RGB over three strips, ours against hardware:

| strip | hardware | ours | reading |
|---|---|---|---|
| **LIFE filled pips** (x175-245) | R142 G114 B0 | R147 G119 B0 | **CONTROL — agrees.** Our HUD is otherwise correct and the method works |
| **PP bar** (x200-380, y55-66) | R6 **G66 B85** | R2 **G3 B2** | hardware **cyan**, ours **BLACK** |

**The positive control is what makes this admissible** (gotcha 30 applied to a measurement
rather than a test): the filled pips agree to within five units, so a black PP bar is a real
difference and not a mis-registered rectangle or a brightness offset.

So the widget is not displaced or mis-coloured — **the region is simply not painted, and the
HUD's own dark backdrop shows through.** That is exactly what a missing draw looks like over a
dark backing, and it matches the mission line in finding 37 (hardware 191-193, ours 61-63).

**The LIFE empty-square strip is NOT usable as evidence and is excluded**: our capture is LV.41
with six filled pips, hardware's is LV.40 with five filled plus two empty, so the strips hold
different things. Reporting it as a difference would be comparing two different health values.

### The shader is real — that is now checked, not assumed

`vs_a4ae7c2b7c1818c4.ucode` is **60 bytes / 15 dwords of genuine microcode**, not zeros:

```
10011002 00001200 C4000000 00000000 1003C200 22000000 00080000 00253B48
00000002 C80F803E 00000000 E2000000 00000000 00000000 00000000
```

It was worth checking because our loader reports `va=00000000` for it — which had a plausible
failure story behind it (microcode read from an unmapped address would be zeros, hash
consistently, and translate to something that draws nothing). **That story is dead:** the
bytes are real, `va=0` is simply what `IM_LOAD_IMMEDIATE` looks like — the microcode travels
inline in the packet — and B2 counts 2.7 M of those.

### Where the defect is now cornered

Everything downstream is eliminated. The shader exists, is real, is translated, is in the
bank, and is loaded by our runtime. Topology is supported. The stream guard is refuted. The
HUD around it renders correctly, pixel-for-pixel on the control strip. **The draws simply do
not arrive at the renderer.**

**The one remaining fork is unchanged and still needs one reading**, because the per-draw
census is written inside `DoDraw` and cannot see a draw dropped before it:

- **we drop them** — predicated out by the bin mask in `gpu/pm4.cpp`. B2 makes this a live
  possibility: `SET_BIN_MASK_LO` is the most common type-3 opcode in the game (finding 34).
- **the guest never sends them** — the cause is then upstream of the GPU entirely.

```
cd runtime/build && CW_VKDRAW=1 CW_RING_TRACE=1 CW_PM4_BIN_CENSUS=1 \
  CW_CAPTURE_KEY=<dir> ./cw_runtime 2> run.log      # F9 with the HUD up, then send run.log
```

**`run.log` is the artifact this time, not the capture** — the counters live on stderr and the
last two captures did not carry it. `ring: pm4 ... (predicated out=N)` plus the bin census
answer the fork in one line.

---

## 39. **THE GPU LAYER IS EXONERATED — the guest never submits these draws** — 2026-08-16

Finding 38 left one fork: are the widget draws **predicated out by us** in `gpu/pm4.cpp`, or
**never sent by the guest**? The per-draw census cannot tell, because it is written inside
`DoDraw` and a draw dropped earlier never reaches it. The operator ran the session that
answers it and sent `run.log`.

### The answer

```
ring: pm4 packets=25,156,859  frames=1,447  draws=1,699,733  (predicated out=4,918)
                                                    = 0.29%
B1 hardware, for comparison                          = 0.3%
```

**Our bin-mask predication discards 0.29% of draws against hardware's 0.3%.** We are not
dropping a class of draws — we are dropping what hardware drops, to within a rounding error.
The bin census lists **no** (mask, select) pair as discarding anything, and `overflow=0`.

That also retires a worry `gpu/vd.cpp` has carried since the transplant, which said *"a third
of this title's draw packets are discarded here and B1 says hardware discards 0.3%, so the
pair table is where that gap is localised."* **There is no gap. It measures 0.29%.**

Third frame, third confirmation, this one with the HUD raised:

```
frame 1352: 3,069 draws · HUD region MAX 249 (hardware 248) · vs_a4ae7c2b7c1818c4: 0
```

### Everything in the GPU path is now eliminated by measurement

| candidate | verdict |
|---|---|
| missing shader | in the bank; 0 "no translated shader" |
| bad microcode | 60 bytes / 15 dwords, real (finding 38) |
| unsupported topology | line/point/quad/rect all mapped; none fired |
| stale stream store | refuted by A/B (finding 35) |
| ALU constant window | read from the registers, not assumed |
| **bin-mask predication** | **0.29% vs hardware 0.3%** |
| ucode validator rejecting the load | **0 rejections** in the whole session |
| shader binding | `g_boundShaders[stage] = {va, size, hash}` — last-load-wins, which is what `IM_LOAD` means |

And the shader **is loaded during gameplay**: `[imload] VS va=00000000
hash=a4ae7c2b7c1818c4 size=15`. `va=0` is normal — **12 of the session's 45 VS loads are
`IM_LOAD_IMMEDIATE`**, and one of the others (`8bb7e189d92e3def`, also `va=0`) draws 96 times
in a single frame. So immediate loads work.

**So the guest sets the shader up and then never issues a draw while it is bound.** The defect
is upstream of the GPU entirely — in the recompiled guest code's widget path, not in the
renderer or the command processor.

### Where to look next, and it is a different kind of work

This stops being a rendering investigation. The handles that exist:

- **`cFEMeter`** (string at `0x820BEEF0`) and **`cFEFlipBook`** (`0x820BEF18`) are the widget
  classes. The `cFEMeter` string has exactly **one** referencing site, `0x829BC640`, which is
  a class-registration call into `sub_827815D0` — i.e. a factory table, not the draw path.
  The vtable reached from that registration is the way to the render method.
- **`runtime/cpu/gap_probe.cpp` is the instrument already built for this shape of question** —
  a strong `PPC_FUNC(sub_X)` that counts calls and forwards. Point it at the widget's render
  method and the answer is "called N times" or "never called", which is a fact rather than an
  inference.
- W0.3's untranslatable instructions are **not** a candidate: all seven functions holding them
  read zero across a full play session (finding 32).

### One unrelated observation, recorded because it was seen and not chased

`gpu/pm4.cpp`'s draw decode reads
`d.indexed = sourceSelect == 0;   // 0 = DMA; 2 = auto-index; 1 = immediate`

so **`sourceSelect == 1` (indices packed immediately into the packet) is folded into the
not-indexed path** and would be drawn as auto-index. That is *not* this defect — it cannot
remove a draw from the census — but if this title ever emits an immediate-index draw it would
render wrong geometry silently. **Unmeasured: nobody has counted `sourceSelect == 1` in B2.**
Recorded as a lead, not a claim.

---

## 40. **A WIDGET-CONSTRUCTION CENSUS — the instrument exists, and the answer needs one gameplay run** — 2026-08-16

Finding 39 put the defect upstream of the GPU, in guest code. This is the first instrument on
that side of the line: **`runtime/cpu/fe_probe.cpp`**, which hooks the frontend's widget-class
creators and counts constructions.

### How the addresses were derived

`cFEMeter`'s string (`0x820BEEF0`) has exactly one referencing site, and it is one entry in a
run of `{creator, id, name}` triples that registers **22 widget classes** into a factory table.
Extracting the whole run gives every class's creator; `cFEMeter`'s is `0x82819630`, a thunk
tail-calling `0x82817488`, which allocates **0x8920 bytes** and calls the constructor at
**`0x8280F468`**. `cFEText`'s equivalent chain is `0x828194B0 -> 0x82815088 -> 0x8280C028`
(0x400 bytes).

**The first version of this probe hooked the wrong class and got a confident wrong answer.**
The creator is stored into the table *before* its own class name is loaded, so reading the site
by eye attributes the next class's creator to this one — and the first cut hooked
**cFEParticleFX** as "cFEMeter", chased it through a real allocator to a real constructor to a
real 40-method vtable at `0x820BDBE8`, and reported *"no cFEMeter is ever built"*. Every step
was genuine; all of it was the wrong class. Extracting all 22 triples programmatically is what
caught it. **Gotcha 322.**

### The census, on the title screen + loading pop-up

Always-on counters, reported from both shutdown paths, one relaxed atomic each:

```
CONTROL A  widget-name intern   1,044,644   (the frontend ran)

CONSTRUCTED        cFESpinGroup 321 · cFEShape 309 · cFEEBMText 108 · cFEText 56
                   cFEAnim 56 · cFEKeyFrame 4 · cFEBitmap 3
NEVER CONSTRUCTED  cFEMeter 0 · cFEFlipBook 0 · cFEFlipFrame 0 · cFETextList 0
                   cFETextBox 0 · cFEBitmapList 0 · cFETable 0 · cFEParticleFX 0
                   cFEEdit 0 · cFENineGrid 0 · cFEThreeGrid 0 · cFEGenericTable 0
                   cFEMovieBox 0 · cFELockBox 0
```

**Both broken families — `cFEMeter` and `cFEFlipBook` — are never constructed**, on a screen
where seven sibling classes are constructed 857 times between them.

### THIS IS NOT YET THE ANSWER, and saying so is the point

**A title screen plausibly has no meter.** The fourteen zeros include `cFEEdit`, `cFELockBox`,
`cFEMovieBox` and `cFETable` — classes no title screen would ever build — so "zero" is exactly
what a correct runtime would also report for most of that list. The census cannot yet tell
"this class is broken" from "this screen does not use this class".

The one thing that argues it is more than that: **hardware's loading pop-up, on this same
screen, draws a segmented progress bar** (finding 36), so *something* meter-shaped ought to
exist here. But whether that bar is a `cFEMeter` or a composite of `cFEShape`/`cFEBitmap` is
unmeasured, and inferring it from the picture would be exactly the kind of step this port keeps
having to retract.

### The run that settles it — and it is automatic now

**Gameplay, where hardware demonstrably has a PP bar, a LIFE meter and a mission bar.** The
probe is always on and reports on window close, so:

```
cd runtime/build && CW_VKDRAW=1 ./cw_runtime 2> run.log     # play, then close the window
```

- **`cFEMeter` nonzero in gameplay** → meters are built and the defect is downstream, in the
  inherited draw path. The vtable its constructor writes is then the map, and the next hook is
  its draw method.
- **`cFEMeter` still zero while `cFEText` is in the hundreds** → the widgets are never built at
  all, and the defect is in screen construction or the factory — far upstream of anything
  rendering-shaped.

Either way it is one number from one ordinary play session, with a sibling control in the same
run.

---

## 41. **`cFEMeter` IS NEVER CONSTRUCTED — the defect is at widget creation, not drawing** — 2026-08-16

Finding 40 built the census and said it needed one gameplay run to mean anything. The
operator played and closed the window; the probe reported on the way out.

### The result, with nine sibling controls in the same run

```
CONTROL A  widget-name intern   1,168,153

  cFESpinGroup   1,072      cFEAnim         209      cFEKeyFrame   39
  cFEShape       1,000      cFEParticleFX    70      cFEFlipBook   19
  cFEEBMText       247      cFEText          46      cFEBitmap      7      = 2,709 built

  cFEMeter            0
```

**In a gameplay session where hardware demonstrably shows a PP bar, a LIFE meter and a
mission bar, `cFEMeter` is not constructed once — while nine sibling widget classes are
constructed 2,709 times.** Finding 40's caveat ("a title screen plausibly has no meter") is
discharged: this is gameplay, the widgets are on screen on hardware, and the controls are in
the same run.

### And it splits the symptom, which finding 35 warned might happen

Against the title-screen census, two classes changed:

```
                 title screen   gameplay
cFEFlipBook            0    ->      19
cFEParticleFX          0    ->      70
cFEMeter               0    ->       0
```

**`cFEFlipBook` builds fine in gameplay.** So the animated-widget family is *not* broken at
construction, and finding 35's grouping of "meters and flipbooks fail together" was a
hypothesis that has now been **partly refuted**: whatever is wrong with `cFEMeter` is specific
to `cFEMeter`. If the loading pop-up's segmented bar is also missing, either it is a meter too,
or it has a different cause — and that is now a separate question rather than an assumed
shared one.

### The guest's own names for these widgets

The image carries the instance names, and they read exactly like the broken list:

```
meter_life   meter_sub_mission   timermeter   health_meter   w_meter   w_red_meter
w_KO_meter   w_meter_star   meter_attack   meter_group   meter_itemstock
meter_range  meter_speed  meter_throwdistance
```

plus the methods `Meter`, `SetMeter`, `UpdateMeter`. **`meter_life` and `meter_sub_mission`
are the LIFE bar and the mission bar by name.** So the widgets exist in the data, the class is
registered in the factory (its registration is what gave us the address), and it is simply
never instantiated.

### Where this lands

The whole chain is now measured end to end, and every link but one is exonerated:

```
screen data names meter_life / meter_sub_mission        ✓ present in the image
cFEMeter registered in the widget factory               ✓ that is how we found it
cFEMeter CONSTRUCTED                                    ✗  ZERO, in gameplay, with 9 controls
  -> no widget object exists
  -> nothing submits its draws                          ✗  0 draws in 3 captured frames
  -> vs_a4ae7c2b7c1818c4 never bound at draw time       (finding 36-38)
  -> the bar/track/empty segments are simply not painted (finding 38: PP bar reads
     R2 G3 B2 where hardware reads R6 G66 B85)
```

**The defect is at widget creation.** Everything downstream of it — the renderer, the command
processor, the shader bank, the stream store — was measured and cleared in findings 35-39, and
none of it was ever going to matter.

### Next

Find why the factory never produces one. The registration writes `{creator, id, name}` triples
into a table; the creation path looks a class up in that table and calls its creator. So the
question is whether the lookup is asked for a meter and fails, or is never asked. Hooking the
*dispatch* — the function that reads the table and calls a creator — answers it the same way
this finding was reached, and `cFEFlipBook` is now the ideal control, because it goes through
the same dispatch and works.

---

## 42. **THE FACTORY IS NEVER ASKED FOR A METER — and the instrument that decides why** — 2026-08-16

Finding 41 established `cFEMeter` is never constructed in gameplay. This traces the creation
path to the exact branch where a missing widget becomes silence, and instruments it.

### The mechanism, read out of the guest

The widget factory is a global table at **`0x82AF3118`** — **25 entries of
`{creator, id, name}`, 12 bytes each** — filled by `sub_829BC368` and handed to the factory
object by `sub_82784578` (`count -> [obj+0x1C]`, `table -> [obj+0x20]`).

Creation is `sub_82784588`:

```
bl    0x82784508          ; index = FindClass(name)
cmpwi r3, -1
beq   -> li r3,0 ; return ; <-- NO CLASS, NO WIDGET, NO COMPLAINT
lwz   r11, 0x20(r31)      ; the table
mulli r10, r3, 0xc        ; index * 12
lwzx  r11, r10, r11       ; creator = table[index].creator
mtctr r11 ; bctrl         ; call it
```

and `sub_82784508` matches by **interned id**: it interns the requested name through
`sub_827815D0` — the same intern the registration used — and walks the 25 entries comparing
`[entry+4]`. No match returns `-1`. **That `beq` is the whole failure mode: a widget that
cannot be classified is simply not made, and nothing anywhere says so.**

### The instrument

`fe_probe.cpp` now hooks that lookup and reports three things: every `cFE*` class the parser
**asks** for with a count, every bare name that **fails to resolve**, and the construction
census from finding 40. Together they separate three different defects that all present as
"no widget":

| reading | meaning |
|---|---|
| class never **asked** for | the screen data never names it — a data/parse problem |
| asked and **unresolved** | the factory table or the intern is wrong |
| asked, resolved, not **constructed** | the creator itself fails |

### What the title/loading screen says, and why it indicts nothing yet

```
factory lookups 16,771, of which 11,767 returned -1
asked: cFEKeyFrame 2488 · cFEBitmap 618 · cFEAnim 642 · cFEWidget 558 · cFEText 216
       cFEShape 112 · cFEButton 112 · cFEScreen 34 · cFELockBox 8 · cFEEBMText 6
unresolved bare tokens: `}`  (nothing else)
```

**`cFEMeter` is never asked for on this screen** — and no widget type fails to resolve at all.

Two things had to be understood before those numbers meant anything:

1. **`sub_82784508` is a GENERIC name→index lookup**, not a widget-only one. The
   screen-definition parser calls it for every token it meets, including property lines —
   the first run of this probe returned `X=0.18906`, `Font="arialblk18"` and
   `Text="413 IDS_SGF_ONLINE"` as "unresolved classes". **So 11,767 failures are normal**,
   and the probe now drops anything containing `=`, `"` or a space.
2. **This screen plausibly has no meter**, so a zero here is not evidence. Same caveat as
   finding 40, unresolved by the same argument.

### The run that finishes this, and it is the last one needed

**Gameplay, where hardware shows the PP bar, the LIFE meter and the mission bar.** The probe
is always on and prints on window close:

```
cd runtime/build && CW_VKDRAW=1 ./cw_runtime 2> run.log    # play, close the window
```

- **`cFEMeter` never asked for** → the HUD's screen definition never names it. The defect is
  in loading or parsing that screen's data, and the next question is which file it lives in.
- **asked but unresolved** → the intern or the factory table is wrong, and the probe prints
  the name outright.
- **asked, resolved, still 0 constructions** → `sub_82817488` itself is failing, and it
  allocates 0x8920 bytes — by far the largest of any widget class, against `cFEText`'s 0x400.

**That third possibility deserves flagging now**: 0x8920 is 35 KB for one widget, and an
allocation that large failing where a 0x400 one succeeds is a specific, checkable story rather
than a vague one. `sub_82817488` returns 0 on allocation failure, exactly like the missing-class
branch, and just as quietly.

---

## 43. **`cFEMeter` IS REQUESTED AND NEVER BUILT — and my own creator census is partly unreliable** — 2026-08-16

### The gameplay reading that matters

The operator's gameplay run, with finding 42's instrument:

```
factory lookups 53,559   ·   asked for cFEMeter: 140   ·   unresolved bare tokens: only `}`
cFEMeter creator: 0 constructions
```

**`cFEMeter` is asked for 140 times, resolves correctly, and is still never built.** So the
defect is not the screen data failing to name it, and not the factory failing to classify it.
That eliminates two of finding 42's three branches.

### The allocation-size hypothesis is DEAD, and it was mine

Finding 42 flagged `cFEMeter`'s **0x8920-byte** allocation — 35 KB against `cFEText`'s 0x400 —
as "a specific, checkable story". It is checkable, and it is wrong:

```
BUILT      cFEShape 0xF0 · cFEAnim 0xD0 · cFEKeyFrame 0xE0 · cFEEBMText 0x2C0
           cFEFlipBook 0x300 · cFEParticleFX 0x2F0 · cFEText 0x400 · cFEBitmap 0x490
NEVER      cFELockBox 0x2D0 · cFEBitmapList 0x3D0 · cFEMeter 0x8920
```

`cFELockBox` (0x2D0) and `cFEBitmapList` (0x3D0) are **smaller** than `cFEFlipBook` (0x300)
and `cFEBitmap` (0x490), which are built. Size does not separate the two groups. **A large
allocation failing quietly was a good story and it is not what is happening.**

### AND MY CREATOR CENSUS IS PARTLY WRONG — gotcha 322 bit a second time

Hooking `sub_82784588` — `CreateWidget(name)` — by NAME rather than by creator address gives a
different and better-founded picture, and it immediately contradicts the address-based census:

```
CreateWidget, title screen:  cFEKeyFrame made 1244 · cFEBitmap 309 · cFEAnim 321
                             cFEWidget 279 · cFEText 108 · cFEButton 56 · cFEShape 56
                             cFEScreen 17 · cFELockBox 4 · cFEEBMText 3
                             ...and EVERY row reads FAILED 0
```

**`cFELockBox` is made 4 times here, while finding 41's creator hook counted it as ZERO.** The
two cannot both be right, and the name-based one is the trustworthy half: it reads the class
name at the actual creation entry point instead of trusting an address I attributed from a
table. **So the creator→class mapping used in findings 40 and 41 is unreliable for at least one
class, and possibly more** — exactly the failure gotcha 322 was written about, one round after
it was written. The per-class counts in finding 41 should not be quoted; its headline claim
(`cFEMeter` is never constructed) survives because that chain was derived and cross-checked
independently (thunk → allocator of 0x8920 → constructor), and because the meter never appears
in the CreateWidget list at all.

**`CreateWidget` is also not widget-specific** — it makes `cScalabilityProfile`,
`cAnimatedPostFXEvent`, `cFloatJuncture` and more. It is the engine's general named-class
factory, which is why "asked" and "made" counts differ per class and why the address-based
census could drift from it unnoticed.

### What is now known, and the single run that ends it

- The screen data **does** name meters (140 requests in gameplay).
- The factory **does** resolve the name (nothing unresolved).
- **No `CreateWidget` call fails** anywhere — every row reads `FAILED 0`.
- And yet **no meter object exists**, so nothing draws, so the bar is black (findings 38-41).

Those four are only consistent if `CreateWidget` is never actually *called* for `cFEMeter` —
i.e. the 140 lookups come from somewhere that is not creation. **The next run resolves that
directly**, because the probe now records `CreateWidget`'s own argument:

```
cd runtime/build && CW_VKDRAW=1 ./cw_runtime 2> run.log    # play, close the window
```

- **`cFEMeter` appears in the CreateWidget list with `FAILED > 0`** → the creator is returning
  0 and the address to hook next is in the table entry.
- **`cFEMeter` absent from the list entirely** → the meter is never *created*, only looked up,
  and the caller that looks it up 140 times without creating is the thing to find.

---

## 44. **RETRACTION: `cFEMeter` IS BUILT — 70 times. Finding 41 was wrong, and every count in it was right** — 2026-08-16

### The retraction

**Finding 41 claimed `cFEMeter` is never constructed. That is FALSE.** The name-based
`CreateWidget` probe on the operator's gameplay run:

```
cFEMeter                 made     70   FAILED      0
```

Meters are created, successfully, seventy times. Everything finding 41 built on top of that —
"the defect is at widget creation", "the chain is measured end to end and every link but one is
exonerated" — **is retracted.**

### How it went wrong, and it is gotcha 322 for the third time

The factory table, **read out of guest memory at runtime** rather than inferred from the
registration code:

```
[15] creator 0x82819630  cFETable
[16] creator 0x82819640  cFEMeter      <-- the truth
[17] creator 0x82819650  cFEParticleFX
```

**My static extraction was shifted by exactly one entry, throughout.** Reconciling finding 41's
numbers against `CreateWidget`'s self-labelling counts:

| finding 41 said | count | actually | CreateWidget |
|---|---|---|---|
| cFEParticleFX | 70 | **cFEMeter** | **70** |
| cFEFlipBook | 19 | cFEBitmapList | 19 |
| cFEMeter | **0** | cFETable | 0 |
| cFEText | 46 | cFEButton | 46 |
| cFEShape | 1000 | cFEBitmap | 1000 |
| cFEAnim | 209 | cFEShape | 209 |
| cFEEBMText | 247 | cFEText | 247 |
| cFEKeyFrame | 39 | cFELockBox | 39 |
| cFESpinGroup | 1072 | cFEAnim | 1072 |

**Every single count matches once the label shifts by one.** The instrument was accurate
throughout; only the names attached to it were wrong. That is the nastiest possible failure
shape — the numbers look real because they *are* real.

And note the sequence: my **first** reading by eye gave `0x82819640` for cFEMeter, which is
**correct**. I then "corrected" it to `0x82819630` with a script that read the whole table —
and the script was shifted the other way. Gotcha 322 said *extract the whole table*; doing that
was not sufficient, because a static extraction of a table the compiler builds at runtime is
still an inference. **The fix that actually worked was reading the table out of guest memory
while the game ran.**

Consequently the earlier chain I discarded as cFEParticleFX is cFEMeter's after all:

```
creator 0x82819640 -> 0x82815B80 (alloc 0x2F0) -> ctor 0x8280D300 -> VTABLE 0x820BDBE8
   40 virtual methods; cFEMeter overrides only accessors:
   0x8280D438  stw r4, 0x1CC(r3)    the setter
   0x8280D440  lwz r3, 0x1CC(r3)    the getter
   0x8280D448  lwz r3, 0x248(r3)
```

**So `cFEMeter` has no draw method of its own** — that observation, made in finding 40 and then
disowned, stands.

### What is actually true now

```
the screen data names meters                      ✓  140 lookups in gameplay
the factory resolves the name                     ✓  nothing unresolved
CreateWidget makes them                           ✓  70, with 0 failures
...and no draw in three captured frames uses      ✗  vs_a4ae7c2b7c1818c4
   the shader that paints them
```

**The meter objects exist and are not drawn.** The defect is downstream of construction, in
the meter's own update/draw path — which is where finding 40 pointed before finding 41 sent
this off in the wrong direction for two rounds.

### Next, with addresses that are now ground truth

The probe has been **re-pointed at cFEMeter's real internals**, and the address-based creator
census has been **deleted rather than fixed** — it mislabelled every row twice, and
`CreateWidget` already names its own classes:

```
0x8280D300  ctor          — expect ~70 in gameplay, which also re-validates the mapping
0x8280D438  SET 0x1CC     — is the meter ever given a value?
0x8280D440  get 0x1CC
0x8280D448  get 0x248
```

- **ctor ~70 and setter 0** → meters are built and never driven. A meter with no value
  plausibly draws nothing, and the caller that should be setting it is next.
- **ctor ~70 and setter > 0** → the meter is built and driven, and the defect is in the
  inherited draw path, with the 40-method vtable at `0x820BDBE8` as the map.

---

## 45. **THE METER IS ALIVE: built 70, driven 69, read 10,192 times — and still draws nothing** — 2026-08-16

Finding 44 re-pointed the probe at `cFEMeter`'s true internals and named two outcomes. The
operator played; this is the answer, and it is the second of them.

```
[fe] sub_82815B80      70   cFEMeter creator  [table entry 16]
[fe] sub_8280D300      70   cFEMeter CONSTRUCTOR
[fe] sub_8280D438      69   cFEMeter SET field 0x1CC
[fe] sub_8280D440  10,192   cFEMeter GET field 0x1CC
[fe] sub_8280D448       0   cFEMeter get field 0x248
                     ---
CreateWidget         70 made, 0 FAILED     <- independent, name-based
```

### The address mapping is now confirmed from two directions

**The constructor count is 70 and `CreateWidget`'s name-based count is 70.** Those are two
independent instruments — one hooked by address off the *runtime* factory table, one reading
the class name at the creation entry point — and they agree exactly. **The off-by-one that ran
through findings 40-43 is settled**, and table entry 16 (`creator 0x82819640 -> 0x82815B80 ->
ctor 0x8280D300 -> vtable 0x820BDBE8`) is cFEMeter's real chain.

### The widget is fully alive

- **70 built**, and 70 requested — nothing is lost at creation.
- **69 given a value.** 69 of 70, so essentially every meter is driven; the odd one out is
  plausibly a meter constructed and destroyed without ever being shown.
- **Its value is read 10,192 times** — roughly 146 reads per meter, i.e. per-frame polling.
  **Something is actively asking these meters what to display, every frame, all session.**
- And **not one draw in three captured frames uses the shader that paints them**
  (findings 36-38).

So the meter is constructed, driven, and interrogated — and produces no geometry. **The defect
is in the draw path**, which is exactly where finding 40 pointed before finding 41's
mislabelled census sent this in the wrong direction for two rounds.

`0x248`'s getter is never called at all, which is worth noting only as a fact: whatever that
field is for, this session never asked for it.

### Why the next hook cannot be a per-class one

`cFEMeter` overrides only accessors — **it has no draw method of its own**, so its drawing is
inherited code shared with every other widget class. Hooking that inherited method would count
cFEBitmap and cFEText too and isolate nothing.

**The link register solves it.** The recompiler sets `ctx.lr` on every `bl`, so on entry to the
0x1CC getter it holds the return address of the call site — and the callers of a value that is
read 10,192 times while nothing draws *are* the live meter path. The probe now records the
distinct return addresses with counts.

That turns "somewhere in the inherited draw path" into a list of guest addresses, each of
which resolves to a named function via `ppc/` and can then be disassembled or hooked in turn.

### The run

```
cd runtime/build && CW_VKDRAW=1 ./cw_runtime 2> run.log    # play with the HUD up, close the window
```

One session, and the output is the call-site list.

---

## 46. **The meter IS linked in and traversed — and field 0x1CC is a LIST POINTER, not a value** — 2026-08-16

The link-register probe named the callers of `cFEMeter`'s 0x1CC accessor:

```
cFEMeter get 0x1CC called 12,292 x
  from 0x8281CA8C   9,660 x    (inside sub_8281C9D0)
  from 0x828087AC   2,632 x    (inside sub_82808760)
```

### Correction: 0x1CC is `next`, and those accessors are `GetNext`/`SetNext`

Both call sites are **linked-list traversals**, and both reach the accessor through the
vtable rather than directly:

```
sub_8281C9D0 @ CA78:  lwz r11,0(r30) ; lwz r11,0x7C(r11) ; bctrl   ; r30 = GetNext(r30)
                      or. r30,r3,r3  ; bne <loop>                   ; walk while non-null
sub_82808760 @ 87A0:  lwz r11,0x7C(r11) ; bctrl ; cmplwi r3,0 ; bne ; walk to the TAIL
             @ 87C0:  lwz r11,0x78(r11) ; bctrl                     ; then SetNext(tail, new)
```

`vtable[0x7C]` is index 31 = **`0x8280D440`**, and `vtable[0x78]` is index 30 =
**`0x8280D438`** — the two accessors this probe has been calling "get/set the meter's value".
**They are not.** `0x1CC` is a **next-sibling pointer**, `0x8280D438` is `SetNext` and
`0x8280D440` is `GetNext`.

So finding 45's reading has to be re-stated:

| finding 45 said | actually |
|---|---|
| "69 meters given a value" | **69 meters LINKED into a widget list** (70 built, one never linked) |
| "value read 10,192 times" | **12,292 list traversals stepping over a meter** |

**The conclusion those numbers supported is unchanged and is in fact strengthened**: the meter
objects exist, are inserted into the frontend's widget list, and are walked thousands of times
per session by `sub_8281C9D0`, which calls `vtable[0x18]` (index 6 = `0x8281A5E0`) on each
element before advancing. The widget is not orphaned — **it is in the list and it is visited.**

### Three of my labels have now been wrong, and the pattern is worth naming

1. the creator→class map (off by one entry, twice — findings 43, 44);
2. the allocation-size hypothesis (`cFEMeter`'s 0x8920 looked damning; `cFELockBox` at 0x2D0
   is also unbuilt and `cFEBitmap` at 0x490 builds fine — finding 43);
3. and now `0x1CC` "the meter's value", which is a list link.

Every one was a **semantic guess about a number the instrument reported correctly**. What has
actually worked, every time, is refusing to name things from static reading:

- reading the factory table **out of guest memory while the game ran**;
- hooking `CreateWidget` **by the class name it is passed**;
- and recording **the link register** so the caller identifies itself.

The counts have never been wrong. The stories attached to them have been wrong three times.

### Next

`sub_8281C9D0` walks the widget list and calls **`0x8281A5E0`** on every element — that is the
per-widget operation the meter is receiving 9,660 times while drawing nothing. It sits in the
generic `0x8281xxxx` frontend range, so it is almost certainly shared with the widget classes
that *do* draw, which makes it the right place to look and the wrong place to hook blindly.

Read it before instrumenting it: if it dispatches again per class, the meter's own branch is
the target; if it draws directly, the question becomes what it does differently for a meter.

---

## 47. **The meter's update path is HEALTHY. The vtable census, and a lead that did not survive** — 2026-08-16

Finding 46 named `sub_8281A5E0` as the per-widget operation the meter receives. Measured, with
the class filtered by its own vtable pointer (`0x820BDBE8`, written by its constructor):

```
per-widget update sub_8281A5E0 : 183,190 calls, 183,190 ON A METER   (100%)
  guards        [0x254]!=0 47,106 x   ·   [0x268]>[0x264] 136,034 x
guarded action sub_82816128    :     106 calls,     106 ON A METER
```

**Two things fall out, and both are negative results worth having.**

`sub_8281A5E0` is **cFEMeter-specific**, not shared — 100% of its calls are on meters. That
also validates the vtable filter from the positive side, which the title-screen run could not.

And **the guarded action is not blocked**: 106 firings across 70 meters, ~1.5 each, is what a
one-shot layout looks like rather than something being suppressed. The guards failing on most
calls is normal for an update that only acts on change. **So the update path is healthy and is
not the defect.**

### The vtable census

All 32 hookable slots of `0x820BDBE8`, filtered to meter instances:

```
slot  1   785      slot  4    70      slot  9    396      slot 19  6,726
slot  2     2      slot  5    70      slot 11     70      slot 20  5,007
slot  3   140      slot  8   396      slot 18    840      slot 22      2   ·  slot 32  136
```

Thirteen slots run; the rest read zero. **Slots 19 and 20 are hot and near-identical** (both
test the top bits of the flag word at `[this+0x10]`), and **slot 21 — sitting between them in
memory, structurally different, loading a float and calling out — never runs at all.**

### The lead, and why it is not a finding

Both frontend call sites through vtable slot 21 are gated the same way:

```
lbz  r11, 0x6A(widget) ; cmplwi r11,0 ; beq -> SKIP
lwz  r3, 0xB0(self) ; lwz r11,0(r3) ; lwz r11,0x54(r11) ; bctrl
```

A zero byte at `widget+0x6A` skips the call. That is suggestive and it is **not** established,
for two reasons written down before measuring: the call is made on `[self+0xB0]` **with the
widget as an argument**, so that slot 21 belongs to a *renderer* object rather than the widget
and need not be the same call as cFEMeter's unused slot 21; and at the second site the guard
compares a **constant address** against zero, so that branch can never be taken.

**Measured: `sub_82803570` was called ZERO times** — meters and non-meters alike. That
traversal is not on this title's live path, so the flag was never sampled and **the lead is
neither confirmed nor refuted.** It is recorded as untested, not as evidence.

### Where this leaves the meter

```
created 70 · linked into the widget list 69 · walked 183,190 x · updated (100% meter-specific)
· laid out 106 x · thirteen vtable slots active
...and no draw in any captured frame uses the shader that paints it
```

Everything measurable about the meter's *lifecycle* is healthy. What has not been found is the
call that turns a laid-out meter into geometry.

### Next, and the honest state of it

- **`sub_82803308` is the other slot-21 call site** and the one whose guard is real. It has
  **not** been hooked. Its widget is iterated inside the function rather than passed in, so a
  useful probe needs the loop body, not the entry.
- **"Slot 21 is the draw" remains unproven.** The vtable census says slot 21 never runs on a
  meter; it does not say slot 21 draws.
- The cheapest thing that would settle direction: find which vtable slot the classes that DO
  draw receive and meters do not. That needs the same census run against `cFEBitmap` or
  `cFEText` — the filter is a one-constant change, and the comparison is the measurement this
  investigation has been missing.

**Four readings in a row have now had correct counters and a wrong story attached** (findings
43, 44, 46, and this one's slot-21 lead). The counters have never been wrong. Nothing here is
claimed beyond what the numbers say.

---

## 48. **THE THREE-CLASS VTABLE CENSUS: the draw traversal is slots 8/9, and it nearly skips meters** — part 4, 2026-08-16

Part 4 ran the measurement finding 47 named: the same vtable census on `cFEBitmap`
(1000 built) and `cFEText` (247+7) beside `cFEMeter`, each filtered by the vtable
pointer its own constructor writes. The two new chains were derived from the factory
table **as read out of guest memory** (entries [6] and [4] of the 2026-08-16 dump):

```
cFEBitmap  [ 6] creator 0x828194E0 -> 0x82814A50 (alloc 0xF0)  -> ctor 0x8280B990 -> vt 0x820BD8D8
cFEText    [ 4] creator 0x828194C0 -> 0x828168D0 (alloc 0x2C0) -> ctor 0x8280DCD8 -> vt 0x820BDE50
```

**The census validated itself from three directions** before any number was believed:
each ctor hook re-read `[this+0]` at runtime and all three matched the constants; ctor
counts matched `CreateWidget`'s independent name-based counts exactly (70 = 70,
1000 = 1000, and text ctor 254 = cFEText 247 + cFEEBMText 7, the subclass running the
base ctor); and the static vtable read had already reproduced part 3's runtime meter
census slot for slot.

### The answer is a disproportion, not a hole

The exact-zero "drawers only" slots are small-count lifecycle noise (slot 0 at
294/100 — destructor magnitude; slots 10, 14, 15, 35, 38 all double-digit). The signal
is **slots 8 and 9, called together — identical counts per class, always**:

```
                 slot 8 = slot 9      per created widget
cFEBitmap            32,024                 ~32
cFEText               9,247                 ~37
cFEMeter                118                 ~1.7    <- layout magnitude (cf. 106)
```

A traversal visits drawing widgets at per-frame rates and **visits meters almost
never** — while the *update* walk visits those same meters 4,480 times in the same
session. Slot 9 is a fully per-class override (bitmap `0x82803678`, text `0x82816970`,
meter `0x82804808`); slot 8 is shared by the drawers (`0x8280FF30`) and **overridden by
the meter with its own large, float-heavy body** (`0x82815C18`) — which reads the
guards this investigation has already met (`[this+0x254]`, the `0xB0` object) and ran
those 118 times without producing geometry.

**Both slot-8 bodies open identically: test `[this+0x10] & 0x02000000`, do nothing if
clear.** A visibility-shaped gate, tested in the callee — and possibly also by the
caller, which would explain the skip.

### What this does NOT yet establish

That slot 8/9 **is** the draw is magnitude-plus-structure, not proof; four readings in
part 3 had correct counters and wrong stories. What is measured: the pair runs at
per-frame rates on classes that render and at one-shot rates on the class that does
not. The next probe (committed with this finding) converts the remaining question into
two comparisons: the `0x02000000` flag per class at slot-8 entry **and** in the update
walk, plus the LR at slot-8 entry — which names the walker, and with it the caller-side
gate that decides who gets drawn.

---

## 49. **SLOT 8 IS THE DRAW TREE — and the meter passes every gate of its own** — part 4, 2026-08-16

Two more instrumented drives on the census's heels. Everything below is measured on this
title's own runtime; the addresses came from LR values the recompiler wrote, not from
reading code first.

### The walker has a name and a shape

The slot-8 return addresses land inside `sub_8280FF30` itself — **slot 8 is the recursive
widget Draw**. Decoded (and this reading is *anchored* by the LRs, not free-floating):

```
sub_8280FF30 (drawers' slot 8, cFEWidget::Draw):
  entry:  [this+0x10] & 0x02000000 or bail          ; parent-propagated, per frame
  if own bit 0x00800000 set and [this+0x6C] > eps:
      call OWN vtable slot 9                        ; per-class "render myself"
  for child = [this+0x8]; child; child = [child+0xC]:
      (loop A only) child->flags |= 0x02000000      ; the propagation token
      if child bit 0x00800000 and [child+0x6C] > eps:
          call child->slot8()                        ; recurse
  clear own 0x02000000                               ; token consumed
```

That explains finding 48's identical slot-8/slot-9 counts (8 calls 9 on itself), and it
re-labels the census: **slot 9 is the per-class render** (bitmap `0x82803678`, text
`0x82816970`, meter `0x82804808`), slot 8 the traversal, and `0x82815C18` is
**cFEMeter::Draw**, a real 700-byte float-heavy override.

### The meter's own state is EXONERATED, gate by gate

Update-walk samples (the walk that reaches every widget):

```
                 0x02000000 set   bit 0x00800000 set   [+0x6C] > 0
cFEMeter             97%             8,456 / 9,236       9,236 / 9,236
cFEBitmap           100%             1,247 / 1,286       1,028 / 1,286
cFEText             100%               114 /   117          96 /   117
```

Meters pass the walker's per-child gates **at least as well as the classes that draw** —
and still collect only ~250 slot-8 entries against 9,236 update visits, half of them
arriving without the propagation token (loop B) and dying at the callee's first test.

### What that leaves, and it is two things exactly

The draw is a **tree** recursion (children at `[this+0x8]`, siblings `[+0xC]`) pruned at
every level, while the update walk is a **different structure** (the virtual
Set/GetNext — `[+0x1CC]` on meters, finding 46's "list link"). A widget healthy in its own
right is lost by the tree in only two ways: **an ancestor fails the gates** (the recursion
prunes whole subtrees), or **it is not linked under a drawn parent at all**. The committed
round-4 probe measures both, per class, and records the first failing ancestor's vtable
pointer — identity by what the guest wrote.

Also measured: nine `[+0x10] |= 0x00800000` "Show" sites exist in the frontend band
(`0x82805908/48, 0x828065E8, 0x828096DC, 0x8281809C, 0x8281A944, 0x8281AF7C, 0x8281D3A4,
0x8281D628`) — the map for whichever widget round 4 names as wrongly hidden.

---

## 50. **THE METER'S CONTAINER IS ACTIVELY HIDDEN — a base-cFEWidget ancestor fails the shown bit in 92% of samples** — part 4, 2026-08-16

Round 4 of the part-4 instrument (the draw-tree membership census). Per class, sampled in
the update walk:

```
             parent=0   in-chain   NOT-in-chain   ancFAILbit8   ancFAILalpha   all-pass
cFEMeter        0        13,016         0           10,492          1,464        1,060
cFEBitmap       0         1,292         0              212            713          367
cFEText         0           117         0               19             95            3
```

**Tree membership is perfect** — every sampled widget of all three classes is in its
parent's `[+0x8]/[+0xC]` child chain, which also validates the `+0xB0` parent model from
the control side. The meters' loss is an **ancestor failing bit `0x00800000`** (the
walker's "shown" gate) in 81% of samples, plus 11% alpha — and the first failing ancestor
is the **base `cFEWidget`** (vtable `0x820BD310`, confirmed as the vtable the shared base
ctor `sub_82805C80` writes) in 11,952 of 11,956 failing samples. The drawing classes also
sample ancestor-fails — widgets in currently-hidden screens, dominated by *alpha* — but at
far lower rates and with hundreds of all-passes.

**And the direction of causation is now known: every widget is BORN shown.** The base ctor
sets `0x03C00000` into `[this+0x10]` — both the shown bit and the propagation bit. So the
meters' grouping containers did not miss a Show; **something actively hides them and
nothing shows them again** (or hides them wrongly).

The frontend band contains exactly ten clear-bit8 sites and nine set-bit8 sites,
resolving to twelve functions — pager idioms (hide-all / show-selected over an array at
`[this+0x640]`) and HUD-wrapper objects owning a widget at `[this+0x3C4]` with a bool at
`+0x3CC`. The round-5 probe (committed) records every call into those twelve with caller
LR and target, and dumps each distinct meter-blocking ancestor from live guest memory —
so the hider and the hidden can be matched by address within one run. A ~300-site
game-side band (0x824C–0x8250) exists as the fallback if no frontend row matches.

---

## 51. **THE INTERVENTION RENDERS THE WIDGETS — findings 48-50 are the mechanism, and much of what was hidden is co-op UI** — part 4, 2026-08-16

Round 5 first: the meter-blocking containers were dumped from live guest memory with
their full parent chains and children, and **none of the twelve frontend hide/show
functions ever ran** — the actor writes the flag word through some idiom the static
scans did not cover. The dumps showed the shape plainly: two containers whose only
children are **two cFEMeters each** (blocked 228 samples apiece, `bit8=0`, with a second
hidden ancestor higher in the same chain), five identical sibling rows blocked at
`alpha=0`, one mid-fade at `alpha=0.5`.

Then the intervention. `CW_FE_FORCESHOW=1` forces `bit8` set and alpha positive on every
node of every meter's ancestor chain, per update-walk visit; the same binary without the
variable is the control, and the arm proved engagement: **13,881 forced bit8 writes, 17
forced alphas** — and meter draw-tree entries went from ~250 to **15,302**, with
`cFEMeter`'s own render slot running **8,892 times**.

**The operator's screenshot confirms it on screen.** With the containers forced visible,
previously-invisible HUD renders: the **PP and LIFE labels**, the item-belt's row of
empty squares — and with them a crowd of things that "shouldn't be here": **five PLAY
ONLINE menu rows** (the five alpha-0 siblings), **Frank's kill counter and the
second-player item belt** — the co-op UI, correctly hidden in single-player, forced
visible by a deliberately blunt arm. Operator: *"A lot of thing that shouldn't be here."*

### What this settles and what it opens

- The draw pipeline for these widgets is **fully functional end to end** — shader,
  geometry, traversal, meter code. Findings 35-39's GPU exoneration plus 48-50's
  guest-side chain are the complete mechanism.
- The defect is now exactly this: **the game-side show/hide state for the single-player
  meter groups is wrong** — their containers stay hidden (or transparent) while on
  hardware they are shown. Everything else about them is healthy.
- The round-7 instrument names the containers: `[widget+0x4]` is the interned name id
  (slot 18, FindChildById, compares its argument against it), CONTROL A now doubles as
  the name↔id dictionary, and FindChildById records {id, caller LR} — whoever looks up a
  blocked container by name is the game code that manipulates it.

---

## 52. **EVERY BLOCKER HAS A NAME — the HUD tree is legible, and one contradiction remains** — part 4, 2026-08-16

`[widget+0x4]` is the interned name id (slot 18 compares its argument against it), so with
CONTROL A doubling as an id→name dictionary (36k ids recorded) the whole picture reads in
plain text. The player HUD chain:

```
IGOverlay → HUD → HUD → Main → player → c_hud_player → hud_player
  → cFELockBox_healthbar → { small_healthbar → health_pp_meter → pp_meter_container,
                              health_pips, w_KO_bar (HIDDEN), in, out }
```

(`in` / `out` / `enter` are class-0x820BD848 children — named ANIMATIONS on the groups.)

**The blocked-ancestor census, by name.** Contextual UI that is plausibly *correctly*
hidden: `npc2-10`, `prop1-5`, `w_boss_health`, `w_coop_health`, `w_KO_bar`,
`reticle_crosshair`, `w_slot6-12`, inactive `mission1-5`, `w_weapon_slots` (alpha 0). The
defect candidates: **six `meter_sub_mission` containers, bit8=0 — one per mission row
including the ACTIVE `mission0`** (whose row is alpha 0.5 and whose text renders — that is
the missing mission-timer bar exactly), and `health_meter` under `npc1`.

**The game band manipulates widgets by name.** FindChildById rows put game code at
`0x824D7F40-0x824D8510` resolving `w_coop_health`/`w_npc_health`/`w_boss_health`/
`health_meter`/`w_red_meter`/`w_meter`/`w_meter_star`, and `0x824E93xx-95xx` resolving
`w_missions`/`w_mission_text`/`meter_group`/`w_meter`/`meter_sub_mission` — the mission-HUD
controller knows the blocked containers by name.

### The contradiction that names the next measurement

The pp/health chain passes bit8+alpha in essentially every update-walk sample
(`health_pp_meter` blocked once all session), every class on that chain uses the standard
recursive slot 8 (base `cFEWidget` and `cFELockBox` share `0x8280FF30`; the screen-node
class `0x820BE440` wraps it), and yet meters collect ~250 draw-walk entries a session.
**The only reading that fits all counters: the gates flip within the frame** — healthy when
the update walk samples them, hidden when the draw recursion arrives. The round-9
instrument (committed) measures at draw time, by name: slot-8 reach per widget id, and
every child's bit8/alpha sampled at the walker's own decision point.

---

## 53. **THE RENDER GATE: a meter draws only if `[this+0x244] != -1` — and the draw model was wrong all along** — part 4, 2026-08-16

Runs 9-10 (the draw-time frontier, per named widget) rewrote the model twice:

**1. Nothing draws per frame through slot 8.** The busiest widget in a session sees ~1,800
slot-8 entries; a minutes-long session has hundreds of thousands of frames' worth of HUD on
screen. The widget tree walk is a **retained rebuild** — it runs when something is dirty,
and whatever it builds is re-submitted each frame by a separate path. (Screen-node widgets
double-count in the frontier — wrapper plus base hook — so printed "entered" is 2x reach
for that class.)

**2. The pp and mission chains are walked all the way down.** In a gameplay session the
recursion entered, 171 times each: `small_healthbar → health_pp_meter →
pp_meter_container → meter_group → w_meter → pp_meter` — the meters themselves included.
The only draw-time bit8 failures on those chains are `w_KO_bar` (co-op — correctly hidden),
`health_meter` (npc1), and `meter_sub_mission` (the timer rows). **So for the PP bar, every
container theory is dead: its meter's render runs and emits nothing.**

**3. The gate is one field.** `cFEMeter`'s render (slot 9, `sub_82804808`) is four
instructions of gate and a tail call:

```
if ([this+0x244] == -1) return;                       // silently draw nothing
submit([global+0x1498], [this+0x1D0], [this+0x244], 2);   // -> sub_8273A510
```

`[this+0x244]` is written in exactly two frontend-band places: the ctor (initial value)
and property-setter slot 1 (`sub_82816368`), which for one specific property copies the
value string into `[this+0x2C0]` and resolves it through global registry `[0x82AF3028]`
via **`sub_82813940(registry, name) -> id`**, storing the result — **-1 on lookup
failure**, upon which the meter never draws anything, ever, with no complaint.

So the last question in the chain is measurable and named: per meter, what is the handle,
and what did the registry answer for its name? The round-11 instrument records both.

---

## 54. **The handles are all VALID, slot 9 never runs — and the meter's real submit chain is decoded** — part 4, 2026-08-16

Run 11 refuted finding 53's suspect cleanly: **every named meter carries a valid retained
handle** (`w_meter` 0x3B, `pp1` 0x1E8, `up/down` 0x4F, `left/right` 0x50 — none -1), the
value fields at `[+0x1D0]` are sequential item indexes, and the registry lookup
`sub_82813940` never ran — the handle has another writer outside the scanned band. The -1
gate is real but never the blocker.

Run 12 then showed `sub_8273A510` — slot 9's unconditional tail call — **never runs**, so
slot 9 itself never runs, so the meter's emission is not that path at all. Reading
`cFEMeter::Draw` (`0x82815C18`) end to end: it computes the transform, stores the matrix
into `[this+0x20..0x4C]`, propagates the visibility token to children — and its EMITTING
branch, taken only when **`[this+0x254] == 0`**, goes:

```
sub_8272EB40(batch, 0, -1)                      ; begin
id = sub_8275CD58(global, name)                 ; resolve — names held at
                                                ;   [0x82AF363C/40], RUNTIME-populated
sub_8273C870(batch, name, id, 1, &this+0x26C)   ; submit, twice
sub_8274A698 ; sub_8272EC50(batch, r, 0x28)     ; end
```

`[this+0x254]` has been in the probe since finding 47 — the update-walk histogram says it
is 0 in ~74% of samples — but its DRAW-TIME value on the 126 token-passing draw entries
per session is unmeasured. Round 13 hooks all four functions: begin==0 means the branch is
never taken at draw time (and `[0x254]`'s draw-time value is the block); failing resolve
rows name missing resources; healthy submits push the loss into the flusher below.

---

## 55. **RETRACTION: six rounds of hooks were dead code — an unclosed anonymous namespace, gotcha 323** — part 4, 2026-08-16

`nm` on the binary showed every hook added in rounds 11-13 as `W` at `__imp__`'s own
address — the recompiled weak alias, not the override. An unclosed `namespace {` in
`fe_probe.cpp` (opened before the slot-8 probe's counters, balanced by a stray close 600
lines later) gave internal linkage to every `PPC_FUNC` override defined in between. They
compiled, were dead, and their "(never called)" rows were **the instrument reporting its
own absence**.

**VOID:** finding 51's "none of the twelve hide/show functions ever ran"; run 11's
"registry lookup never called"; run 12's "`sub_8273A510` never runs"; finding 54's
"slot 9 never runs / bails at -1". **STILL VALID** (from hooks proven live by their
nonzero data): all vtable censuses, the draw-time frontier, the flag/gate/tree sampling,
and the draw-time meter state — **the drawn meters (`w_meter`, `pp1`) enter and exit
`cFEMeter::Draw` with mode 0 (the emitting mode) and a valid handle 0x1E8**, so on the
surviving evidence the submit chain is plausibly healthy and simply unmeasured below the
draw body.

The catch-it-in-seconds gate is now gotcha 323: after adding any hook, `nm` the binary
and require `T` at its own address — a clean build proves nothing (gotcha 30 in linkage
costume). The counters were never wrong; they were never wired.

---

## 56. **The guest submit chain is HEALTHY end to end — and the zero is re-measured TODAY** — part 4, 2026-08-16

Runs 16-18, the first with every hook proven live (`nm`-verified, gotcha 323):

```
cFEMeter::Draw  mode 0 (emitting), handle valid          178-248 entries/session
  begin sub_8272EB40                                      runs (95,727 total)
  resolve sub_8275CD58: the registry is a SPRITE ATLAS —  meterBit -> 0x3B
     HUD_meter -> 0x1E8, HUD_timer_bit -> 0x1EE,          all meter names RESOLVE
     HUD_healthcube -> 0x39; fails are unrelated one-offs
  submit sub_8273C870 -> enqueue sub_8273C3D0             all batches cap 4, 0 DROPPED
  finalize sub_8274A698: shader-constant-set stack        4 stacks, 168k allocs, 0 FAIL
     (the retail-silent assert "Shader constant set          (one peaks 6,220/13,824;
      overflow.", shaderconstantsetstack.cpp:168,             another 1,336/1,340)
      NEVER fires)
  end sub_8272EC50(batch, constant-set slot, 0x28)        runs
```

The finalize only **snapshots** the constant set and hands back a slot; the draw itself is
issued later by a per-frame consumer of the batches. And that consumer's output is freshly
re-measured: **run 19's F9 draw census — 3,206 draws of frame 1239, captured on TODAY'S
binary during gameplay with the HUD up — contains ZERO draws with `vs_a4ae7c2b7c1818c4`**,
while listing dozens of other vertex shaders (so the detector provably can match). The
part-2 measurement was not stale: the meter bits are enqueued, constant-set-stamped, and
then never turned into a draw.

What remains is one stage: the per-frame batch consumer. Also in hand from run 16: the
type-2 retained-item update's function-pointer target is `sub_8275DFB8` (1,593 calls, id
"blank"), and the batch manager `[global+0xDB4]` has its readers mapped — the flush lives
among `sub_8275Cxxx-8275Fxxx` / `sub_8274Cxxx-8274Dxxx`.

---

## 57. **THE DEBUG JUMP IS PORTED AND CONFIRMED WORKING — Case West's own dev scaffolding, re-derived** — part 4, 2026-08-16

The operator asked for Case Zero's debug-jump toolchain here ("it'll be easier that way").
Ported by re-derivation, never by copied constants (the port-pending rule):

- **The tunables loader is `sub_824A4C90`**, get-bool-by-name `sub_82786708`, and
  `tools/find_debug_tunables.py` machine-extracts the (name → byte) table, modelling the
  pipelined store that produced Case Zero's off-by-one and confirming every byte by its
  `lbz` readers: **401 confirmed tunables** (`enable_debug_jump_menu → 0x82A744FF`,
  `chuck_in_god_mode → 0x82A74557`, and Case West additions like `test_frank`). Checked in
  as `runtime/cpu/debug_tunables_table.inc`.
- **The screen-name hash is `sub_827815D0(name, len)`** — Case Zero's `0x8276E398`
  fingerprint-matched uniquely, and independently already known as fe_probe's CONTROL A.
- **The transition request is `sub_82812410(manager, hash, 0)`** — Case Zero's
  `0x827F6D40` fingerprint-matched uniquely; the manager is captured live from the
  title's own transitions.
- `DebugJump` (0x8206D8C0) / `DebugEnter` (0x8206E284) are the image's own strings, and
  `debugjump.txt` ships in `mainmenu.big` at 4,144 bytes — the same size as Case Zero's.

**Confirmed by the operator**: enter the main menu, press F2, and the title's own
DebugJump screen opens and navigates. (One crash on the way: F3/DebugEnter serviced
during the BOOT LOGOS dies on a null virtual call — screens opened onto a half-booted
game are unsafe; noted, parked at the operator's request.)

The working recipe is now automated: `CW_FAKE_START_MS=3000` with
`...,START,NONE,F2,DOWN,A,...` walks boot → main menu → DebugJump → first entry → jump,
landing in-game with no human and no save-file dependency — the fixed point Case Zero's
whole navigation toolchain existed to provide. `CW_SCREEN_TRACE=1` logs every screen
hash; `CW_DEBUG_TUNABLES=name[=v],...` sets any of the 401 by name, failing loudly on
typos. The F4 overlay / AutoChuck surface is deliberately not ported yet.

Also learned on the way (from run 23's shutdown report): the "flush chain" counters of
finding 56's framing — dispatch 506 / list-exec 506 / per-item 2,516 — are IDENTICAL
across sessions of different lengths, so `sub_82763900/82753FF8/8274C488` are a
BOOT-TIME build pass, not the per-frame flush; only the bits-write counter scales with
session length. The per-frame consumer of the bits batches remains the open question in
the progress-widget investigation.

## 58. **vs_a4ae7c2b7c1818c4 IS THE RESOLVE-RECT SHADER — the widgets are made by RESOLVES, and our guest emits them all** — part 4, 2026-08-19

**Finding 36's interpretation is RETRACTED IN PART, in place.** "One vertex shader draws all
the broken widgets" was read as *the widget geometry's* vertex shader. It is not: it is the
title's **resolve-rectangle shader**. Measured three ways today:

- **Our PM4 walk** (`CW_PM4_METER_TRACE=1`, new): >1,048,576 draws executed with a4ae bound
  in one session — **every one with RB_MODECONTROL edram_mode=6 (kCopy)**, ~6% predicated,
  the rest handed to the renderer, where **all take the RESOLVE door**. That is why the
  pipeline-level VS census (`CW_VK_VS_CENSUS=1`, run today: 37 distinct VS, no a4ae) never
  sees it — resolves do not build pipelines.
- **Hardware says the same** (`tools/xtr_meter_resolves.py`, new, over the operator's
  `R2_ui_bars` traces): in `pp_mission_bar.xtr` ALL 59 a4ae draws are modecontrol=6 — and
  they are the frame's ONLY mode-6 draws; every real draw uses other shaders at modes 4/5.
  So even in hardware's frame the "55 widget draws" were resolves.
- **The widget mechanism**: paint into EDRAM with ordinary draws → **convert-resolve**
  (RB_COPY_CONTROL copy_command=CONVERT, colour+depth **clear-after**, ctl=0x0010030x) into a
  small texture (destPitch encodes it: 64x64 per LIFE square, 128x64, 512x256...), composite
  that texture later with the normal FE shaders. Draws 887-893 = seven consecutive 64x64
  resolves = the 7 LIFE squares.

**What is measured HEALTHY in our runtime:** the resolves run (snapshots created, regions
correct, `entered==accepted` on every pass — a new `drawsEnteredThisPass` counter separates
"guest emitted nothing" from "we dropped it in DoDraw", and NOTHING is dropped).

**The open divergence, where the next session starts:** our resolve trace at the boot
loading popup shows the 64x64 widget resolves each preceded by exactly ONE 3-vertex paint
draw (plausibly healthy), but the 128x128/128x64 resolves preceded by **ZERO paint draws**
— they copy out EDRAM that the PREVIOUS resolve's clear-after just wiped, i.e. blank.
Hardware's `loading_popup.xtr` ALSO shows zero-paint resolves (`since=0` runs at draws
36-41), so "zero paint draws" is not by itself the defect — the question is what those
zero-paint resolves COPY on hardware: EDRAM content that survives from an earlier pass
because the clear-after only wipes the copy region / uses different extents, versus our
EDRAM stand-in whose state at that point differs. **Next measurement: run
`tools/xtr_meter_resolves.py` against our own resolve sequence at the same screen and find
the first dest whose sequence (paint count, region, clear extent) diverges from hardware's
— then check what our clear-after actually clears** (whole stand-in? copy region only?).
Nothing GPU-side remains unexplained above this point: the loss is inside the
resolve/EDRAM-state semantics, not in draw delivery.

## 59. **THE WIDGET GEOMETRY IS `vs_667b04293a65b5ca`, AND THE GUEST STOPS EMITTING IT IN-GAME** — part 4, 2026-08-19

Finding 58's open edge is closed and the defect has a name. Measured today, all on one
binary, with pictures:

- **The missing draw class**: hardware's in-game HUD frame carries **60 draws** of
  `vs_667b04293a65b5ca` / `ps_6a348c839d760e93` — quad lists (prim 13), auto-indexed,
  1,560-2,396 verts, sampling the resolved 32x1/1x1 strip textures. **Six of our in-game
  frame censuses contain ZERO.** That class IS the widget segments.
- **It is not a renderer drop**: `CW_PM4_METER_TRACE=1` now traces this hash too
  (`[mtrbits]`). The draws execute unpredicated by the tens of thousands and enter
  DoDraw with valid shaders and topology — **but only during the TITLE/MENU era**
  (frames ~2,162-4,871 of a 1s-token run); the stream then stops. In-game: zero. So
  **the guest itself stops emitting the widget draws in-game in our runtime**, where
  hardware emits 60 per frame. The loss is a guest-side per-frame emission gate.
- **Snapshot content, photographed** (poison run, `CW_VK_CLEAR_POISON=1` + F9 capture):
  the 128-class widget textures are pure clear-color copies (solid magenta under
  poison), and of the nine 64x64 square textures the first holds real gold art while
  the other eight are painted SOLID BLACK by their one paint draw each. No magenta
  appears in the HUD, so nothing composites those textures as-is.
- **The HUD itself photographed in-game for the first time on this port**
  (`cap_poison6/capture_016793`): LIFE's filled squares render, the PP bar and the
  mission progress line are absent — the defect exactly as the operator reported it.

**Recipe note:** the perf import changed all boot timings; the operator's DebugJump
recipe re-derived at `CW_FAKE_START_MS=1000` granularity (same wall-clock seconds,
1s tokens), and the operator's capture guidance is F9 at +1 s and +3 s after the final
A, no LT needed.

**THE ARMED NEXT MEASUREMENT — the two-era counter diff.** The title screen is a
same-binary CONTROL where the emission works. A title-era fe report (35 s idle) reads:
flush dispatch 506 / bits-write 1,657,908 / begin 388,018 / submit 592,379 /
end 2,612,622, batches pass ~2,900 with high-water 0 and DROPPED 0 (`run_eraT.log`).
Take the matching IN-GAME report (DebugJump recipe + `CW_FE_AUTOREPORT=75`), normalise
to rates, and the first chain stage that is active at the title and flat in-game names
the gate that stops the widget draws. (Note the title batches' high-water reading 0
while bit draws demonstrably fire — read that counter's meaning before trusting it,
gotcha 30.)

## 60. **THE PROGRESS-WIDGET DEFECT IS CLOSED — small packed textures, named by A/B** — part 5, 2026-08-23

**The port's one open defect of its own is fixed**, by importing Case Zero's non-RT
block (`444631f..5b9fbba`, commit `04f42e3`). The operator reported it had been fixed
accidentally in the sibling while fixing something else; they were right.

### The mechanism, named by measurement rather than inference

`cf62229` — **small packed textures**. A texture whose SHORTER dimension is ≤ 16 texels
packs its whole mip chain, level 0 included, into one 32x32-block tile with
`mipAddr = 0`. The renderer read level 0 at the tile ORIGIN — the scrap region — and
skipped the chain upload entirely. **The bar strips are 32x1**, so every one of them
sampled the wrong texels.

**The A/B, same binary, same DebugJump destination, same frame:**

```
default                        PP bar RENDERS, mission progress bar RENDERS
CW_VK_NO_PACKED_SMALL=1        both VANISH — the pre-import picture exactly
```

That is the whole finding: one arm, one flip, both bars. Before/after crops of the HUD
region are in `runtime/build/hud_compare.png` and `mission_compare.png`.

### RETRACTION — finding 59's ATTRIBUTION was wrong (its measurement was not)

Finding 59 identified `vs_667b04293a65b5ca` as "the widget geometry" and concluded the
guest stops emitting it in-game. **The counts were right and the story attached to them
was wrong** — this port's characteristic error, and the fourth time (gotcha 322).
`vs_667b` is still **zero** in our in-game censuses *while the bars render correctly*,
so it was never the class that draws them. Hardware's HUD frame does carry that class;
what it draws there remains unidentified, and it is no longer interesting.

The lesson is the one this project keeps paying for: a draw class that is present on
hardware and absent here is a **difference**, not a **cause**, until something ties it
to the pixels. What tied this defect to its cause was an arm that could turn it back on.

Finding 58's mechanism (the widgets are render-to-texture via convert-resolves) still
stands and is what made the small-texture path the load-bearing one.

## 61. **THE VISUALS MENU — the title's own options row opens a host panel** — part 5, 2026-08-23

Imported from Case Zero part 60, **default path only**. Selecting **Visuals** in the
title's own Options hub is intercepted at the frontend transition (`sub_82812410`, the
hook the DebugJump port already owns), the transition is SWALLOWED, and a host-drawn
panel opens with the hub alive underneath as its backdrop.

Six rows: RESOLUTION / DISPLAY MODE / VSYNC / **SHADOW QUALITY (LOW, MEDIUM, HIGH)** /
FRAME CAP / FIELD OF VIEW. **No ray-tracing entries** — operator's instruction, and by
construction rather than by editing: the import boundary stops before Case Zero's first
RT commit, so no RT code was ever brought across to remove.

**Nothing is transcribed from the sibling.** Case Zero compares against two interned
name-hash globals it located by hand; we instead hash our own image's `"OptionsVisual"`
string (0x8206D900) with **the title's own hash function** `sub_827815D0` and compare
that. Derived, not inherited — and it cannot rot if a rebuild moves the interned table.
The computed hash is `21C38544`.

Verified end to end on 2026-08-23: main menu → HELP & OPTIONS → Visuals opens the panel,
and a further press stepped RESOLUTION to 1368x768 (`[pcopt]` log lines, screenshot).
`CW_NO_PC_OPTIONS=1` restores the shipped screen; `CW_TEST_PANEL=1` opens the panel at
boot for display-only questions.

**Not ported:** `cpu/camera_fov.cpp` (the game-side FOV substitution is entirely Case
Zero addresses, including a link-register value naming one call site) and
`gen_pc_options.py` (the native-screen arm's repacked asset). The FOV row stores its
value and the renderer's own projection patch applies it; re-deriving the game-side half
needs this title's own camera census, and the sibling's file header carries the recipe.

## 62. **A NEAR-COMPLETE PLAYTHROUGH, ZERO FAULTS — and 37 shaders the bank did not have** — part 5, 2026-08-27/28

Two long operator sessions on the post-import build (`03dc55e`), both with
`CW_SHADER_DUMP` armed. The second was an attempt to finish the game and got, in the
operator's words, *"almost"* there — **further into Case West than this port or Xenia
has ever been driven.**

### The run itself is the headline

```
~1,454,000 vblanks in one session   (the longest run this port has taken)
crashes / SIGSEGV / SIGBUS      0
unsupported packet / format / import  0     (all three fail LOUDLY by design)
truncated index buffers         0
dispatch table                  58,695 entries, 0 refused
```

Nothing in the runtime complained, at a depth where there is **no capture ground truth
at all** (finding 33). Combined with 2560x1440 internal and the two imported performance
campaigns, the port now sustains a multi-hour session at high resolution without a fault.

### The shader bank grew, and every miss was invisible

```
session 1 (2026-08-27)   +18 new   15 reached a draw and were SKIPPED
session 2 (2026-08-28)   +19 new   19 reached a draw and were SKIPPED
bank: 443 -> 461 -> 480, all translated, dim census 0 disagreements
```

**The operator reported "everything looks good" during the session in which 15 draw
classes were being declined.** That is the finding, and it is now **gotcha 324**: a
shader-cache miss is silent — the draw is declined, and the result is a world with
something quietly absent that a player has no reference for. Past the captures there is
no oracle either, so the dump plus the renderer's own `no translated shader` line is the
only instrument that can catch it.

**The growth curve is itself a measurement.** ~18 new per session is a bank still
tracking new material. **A repeat run over covered ground returning ZERO new shaders is
what would establish the cache is complete** rather than merely larger — that run is
owed and has not been done.

### How it was nearly not measured at all

The first launch of 2026-08-27 went out with the dump OFF. It was caught by the operator
asking whether it had been armed, two minutes in, and relaunched before anything was
lost. Everything above exists because they asked. **Arm it on every session with a human
at the controls, whatever the session was for.**

## 63. **THE FIRST UNINSTRUMENTED PROFILE — the guest render thread paces the frame, and the two biggest symbols are its ring-space spin** — part 6, 2026-08-28

`tools/cw_perf_profile.sh` (new): the HUD-scene recipe with a 60 s in-game hold, sampled
by `perf` (cycles:u) with NO ProfScope armed — the profiler's own bill is ~777 ns/draw at
this scene, the same order as the per-draw costs it reports, so this run is the first
per-symbol truth this port has. Operator config throughout (2560x1440 internal from
cw_settings.txt, shadows HIGH, fps cap off). 165K samples over 25 s, ~3.3 cores busy.

```
thread                         share    what it is
guest render thread            34.1%    SATURATED (~1.1 core-equivalents)
the pump                       23.3%    77% duty — idles 23% of the frame
3 guard prehash workers        16.4%    GuardFold, off the critical path
second guest thread            15.9%    real sim work
3 guest job threads             7.0%    sub_82741F98, real work
```

Top symbols: `GuardFold` 16.5% (nearly all on the workers), then **`sub_825B7668`
(10.3%) and `sub_825B5FB8` (6.4%) — both on the guest render thread, and they are one
mechanism**: 825B5FB8 compares the ring write offset against a POINTER to the read
pointer (fields +0x2a90/+0x2a9c) and 825B7668 is its nop-backoff spin body with a
5000-unit bound before falling back to a real wait. The busiest code in the process is
the title's Draw Thread waiting to SEE our ring consumption — Case Zero's finding 38,
re-derived here from this image's own bytes.

**The regime differs from Case Zero's:** their pump owned the frame; here the pump idles
23% while the guest render thread saturates. A pump-side per-draw saving therefore does
not convert 1:1 at this scene — the guest side is live in this port in a way it never
was there (their §6dm ruled it out for THEM, at their load, and says nothing about ours).

## 64. **THE RING-LATENCY PAIR: engagement proven, latency bound moved as predicted, frame rate NULL** — part 6, 2026-08-29

Two items built on finding 63, each with its control arm and engagement counter:
mid-walk rptr publication (`CW_PM4_NO_MIDWALK_RPTR=1` restores end-of-walk-only) and
the eager tick (`CW_PM4_NO_EAGER_TICK=1` restores the unconditional 100 us sleep).

One run per arm, HUD scene, matched draws 1330-1470:

```
control   220.0/225.5/224.7 fps   sleep-before-progress <= 0.47 ms/frame   counters 0/0
arm       225.5/224.9/223.4 fps   sleep-before-progress <= 0.31 ms/frame   eager 60%, 16 stores/frame
```

The pre-registered latency bound moved; the frame rate did not. **NULL at this scene** —
the pump's tick latency is not on the frame's critical path here. Kept as defaults (cost
nil, closer to hardware's continuous rptr update, scales with ring traffic), and the
mid-walk census taught one structural fact: **the ring carries only ~16 top-level packets
a frame** — the command mass is inside INDIRECT_BUFFERs, so ring-granularity items on
this title have ~16 latency points a frame to work with, not thousands.

Also learned: the four DebugJump left-column destinations all land at 1,350-1,530 draws
(`tools/cw_jump_probe.sh`) — none reaches the operator's heavy band, so a heavy
autonomous route needs their played input trace (Case Zero part 80's transcription
pipeline), which does not exist here yet.

## 65. **THE HEAVY SCENE EXISTS AND IS AUTONOMOUS — X_AutoChuck, named by the operator** — part 6, 2026-08-29

The operator, mid-session: RIGHT from the DebugJump case column reaches the survivor
scoop, and further right the AutoChuck column — *"which can be really useful to test
performance in a crowd of zombies."* Two corrections the tooling had to learn:

* **The grid has two BLANK columns** between the survivors and X_AutoChuck, and a blank
  column is selectable: RIGHT x2 + A jumps into a 64-draw void that an earlier probe
  had scored as "destination is light". RIGHT x4 is the AutoChuck column. The photo
  survey (F9 after each press) is what caught it — a draw count alone could not have.
* **[Y] IGNORE HUMANS** is a real toggle on the jump screen (string at image+49492,
  property `zombies_ignore_all_humans`); pressed before every unattended jump so the
  soak cannot end with Chuck eaten.

First soak band: **3,100-4,800 draws at 9-12 ms** in the heavy stretches, unattended,
reproducible from a cold boot in ~75 s. `tools/cw_autochuck_soak.sh` +
`tools/cw_trace_band.py` (Case Zero's banding reader, imported). Every heavy-band claim
this port makes from here on has this scene as its floor.

## 66. **THE AUTONOMOUS HEAVY-BAND PIPELINE WORKS — the operator's route, replayed at 100% this evening** — part 6, 2026-08-29

The full chain: the operator records once (`cw_route_record.sh`, with the pad's resting
bias CALIBRATED out — three designs, the third is per-stick median below half
deflection); the transcriber emits compound entries (buttons ride sticks — a dropped
START cost a whole evening) at 40 ms knots the replay LERPS between; `WAITWORLD` parks
the sequence emitting nothing until the renderer sustains a world-sized frame (the
save-load's seconds of variance was the drift that no input fidelity could fix); the
harness appends camera pans and GATES every run, and `cw_soak_until.sh` retries until K
runs are accepted. Six of six accepted in the evening's A/B session, 6,800-7,000 draw
peaks, 43-57 heavy windows per run, every run under the operator's 3-minute cap.

What it is for: pump-side CPU A/Bs. What it is NOT: the crowd's guest-side load — the
operator's manual soak at their crowd save remains the oracle for whole-frame claims.

## 67. **THE HEAVY-BAND VERDICTS: the ring package banks −0.19 ms, −O3 is null, the pump IS the frame, and the order gate is proven** — part 6, 2026-08-29

Three accepted soaks per arm, ~90k frames each, banded medians:

* **The ring-latency package** (eager tick + mid-walk rptr + fast-retry backoff) is
  the session's banked item: **−0.19 ms/frame (+1.7%) at 6,500-7,000 draws**, every
  heavy band in its favour, light bands flat. Mechanism: pump sleep 0.47 → 0.15
  ms/frame. The fast-retry backoff was designed against the crowd decomposition
  (walk 8.2 + SLEEP 1.25 of a 9.55 ms frame) and the naps turned out to be
  drained-ring idles, not WAIT holds (4 holds in a 10-minute soak).
* **CW_PPC_O3 is NULL at the heavy band** (+0.7% frame-weighted, mixed sign). The
  guest render thread's light-scene saturation is ring-space spin, not codegen-bound
  work; the default build stays -O2.
* **The pump IS the crowd frame**: wall 11.1 ms, walk 10.97 of it. GPU 7.3 concurrent,
  fence 0. GuardFold is 22.9% of the process but ALL on the three prehash workers.
  Pump split: our code ~8.9 ms, driver ~2.1 ms, libc ~1.4 ms.
* **The order gate is ALIVE both ways**: serial 20,063 frames / 0 failed / 19.3M draws
  logged; `CW_VK_ORDER_POISON=100` fails it by name. The parallel-record campaign's
  technical prerequisite is met; what remains is the THREAD-BUDGET decision (the
  campaign's workers are the guard pool's), which is the operator's.

## 68. **PARALLEL RECORD STAGE 1: the skeleton is CORRECT AND FREE — after its own A/B refuted the first version** — part 7, 2026-08-29

The campaign's agreed stage 1 (a worker-pool skeleton that partitions frames into
contiguous draw ranges, dispatches them to the SHARED guard-pool workers, and rebuilds
the order gate's submitted sequence from the concatenation — execution untouched) is
in, as `runtime/gpu/parallel_record.{h,cpp}` plus call sites, staged for the first
export that flows TOWARD Case Zero.

* **Correct, at scale, twice**: heavy-band gate runs on both versions — 99.1M draws /
  0 failed (v1, `8ebe410`), **103.9M draws / 32,576 frames / 0 failed** (v2,
  `d37ed23`), peak frame 7,329 draws in 96 ranges, drain never blocked. Five positive
  controls all scream on cue: draw-transposition poison (11,546/11,546), the new
  range-transposition poison `CW_VK_PREC_POISON=1` (13,172/13,184 — the remainder are
  single-range frames it cannot touch), the worker-hash poison `=2` (470,818 mismatches
  with the gate correctly still passing), and the control arm announces itself.
* **The v1 null FAILED — the pre-registration did its job.** The first version logged
  the full draw identity and kicked all three workers per range close in every run:
  −3.6% / −0.41 ms at the decisive 6,500-7,000 band, every heavy band the same sign,
  dose-response with draw count. ~60 ns/draw of bookkeeping nobody consumed, and the
  mechanisms were all boring: notify_all for nanosecond jobs (~67/frame), a mutex in
  the workers' wake predicate, a static-init guard on the per-draw path (gotcha 453's
  exact shape, put back on the path it was once taken off), an unconsumed per-draw
  hash.
* **The v2 null HOLDS.** Two modes: id mode (gate armed) keeps the full concurrency
  proof as a diagnostic arm; count mode (every default run) tracks a count — no
  per-draw hash, no per-range wake (**8 kicks in 13,385 frames** against id mode's
  ~468,000), lock-free HasWork. A/B (3 accepted alternated runs per arm): decisive
  band **−0.5% / −0.05 ms**, non-monotone, mixed sign — the pipeline's own definition
  of noise. A concurrent Case Zero session overlapped the v1 GATE run only (operator
  flagged it); a correctness gate under scheduler contention is a harder test, not a
  contaminated one, and the timing arms ran on a clean machine.

The general lesson, added to the campaign notes rather than re-argued each stage:
**per-range wakes are priced by the job they wake for** — stage 2's jobs are tens of
microseconds and can afford them; stage 1's nanosecond jobs could not.

## 69. **STAGE 2b: THE TICKET SPLIT IS FREE, AND DEFERRED REPLAY IS FASTER — the range's locality pays back the secondaries' cost** — part 7, 2026-08-30

The record core was severed behind a ~750-byte `DrawTicket` (decode at capture,
recording from the ticket alone), gated the same way every stage has been:

* **Capture completeness, at title scale**: 8.28 M draws deferred through 471,594
  ranges with **0 replay early-outs, 0 dropped, 0 new validation complaints, order
  gate 0 failed** — the property stage 2c's workers cannot debug after the fact,
  proven while execution is still serial.
* **The restructure null HOLDS** (3+3 accepted runs, 2b binary vs `4f1993c` control,
  default config): frame-weighted −0.0%, mixed sign, decisive band −0.9%.
* **The defer arm is not free — it is a WIN**: +6.0% / **+0.64 ms at 6,500-7,000**
  (26,802 vs 23,245 frames), +5.8%/+6.8% in the neighbour bands, dose-response with
  draw count, GPU medians identical — the same signature shape that condemned stage
  1's first version, with the sign flipped. Mechanism (stated as hypothesis, one A/B
  old): replaying a range's ~128 draws back-to-back keeps the record path's code and
  state hot, where inline recording interleaves it with decode, constants and texture
  work per draw.
* **The stack nets out**: sec+defer vs the pre-campaign baseline reads +0.8%
  frame-weighted, +1.2% at the decisive band (cross-chain, indicative) — 2a's ~0.6 ms
  secondary overhead is paid back by 2b's locality, so the worker flip starts from a
  scaffold that costs nothing.

Also banked: the range-size knob is noise at the decisive band (512 vs 128, +0.4%);
128 stays default for worker balance. And the harness now stamps every run's binary
sha256 — a mid-chain rebuild nearly relabelled an A/B's back half tonight, and the
class is now detectable in the log rather than by mtime archaeology.
