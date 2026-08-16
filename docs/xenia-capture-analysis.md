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
