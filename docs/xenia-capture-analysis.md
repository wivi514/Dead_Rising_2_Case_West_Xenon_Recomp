# Xenia capture analysis — the numbered findings ledger

**This file is the authority on any measured number in this port. Where another document
disagrees with it, it wins.** Findings are numbered and never renumbered; a retraction is
written *in place* under the finding it retracts, not deleted.

Round 1 partial delivery: **2026-08-15**, session 2. Delivered: **A1, B1, B1b, C1, A2, B2,
C2** plus a set of boot-logo screenshots. Still outstanding: **A3, A4, A5, B4, W1/W2, E**.
What each file is: `Xenia logs/Xenia_Run_Content.md`. What was asked and why:
`docs/xenia-capture-requests.md`.

**Round 1 refuted two claims this project had already written down as likely.** Both were
mine, both were inferences from the image rather than measurements, and both are corrected
in place in `docs/port-plan.md` and in the memory directory. They are findings 3 and 4.

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
