# Dead Rising 2: Case West — Xenia capture run index

One entry per run. Requests: `docs/xenia-capture-requests.md` (round 1).

All captures run on the operator's **Windows** PC on the instrumented **xenia-canary
fork** (Xenia is unstable on the Linux box this repo lives on). Record the fork's
`Build:` line per run rather than assuming it is unchanged.

The STFS **package** is launched directly —
`…\58410B00\000D0000\D01128ABB9C7F9694DAE26AC591A269F8480E85A58` — **not** the extracted
`default.xex`. Title ID **58410B00**. (Case Zero is `58410A8D`; the folders look alike.)

**This file is tracked in git; the captures themselves are not** (`Xenia logs/*` is
gitignored with this file negated out). So this index is the only thing that survives if a
capture is lost, and an unindexed capture is a file nobody can interpret in three weeks.

---

## Entry template

Copy this per run.

```
### <id> — <one-line what it is>  → `<directory>/`
**Delivered <date>.** <what was done, in order. Say what was skipped, or that nothing was.>
Flags: log_level=?, flush_log=?, high_freq=?, dump_shaders=?, trace_* =?, resolution=?
Xenia build: <the `Build:` line>
Clean exit: <did the log end with `Cheap-skate exit!`?>
license_mask / trial: <what XamContentGetLicenseMask returned; any unlock prompt?>

- <findings, surprises, anything that looked wrong IN XENIA ITSELF>
- <anything this run shows that the request did not predict — this is the most
  valuable part of an entry, and two of Case Zero's round-1 premises were refuted
  exactly here>

Files: <names and sizes>
```

---

## Round 1

Round 1 was requested 2026-08-15 (session 1). See `docs/xenia-capture-requests.md`.

### A1 — SHORT boot at max verbosity (boot → main menu → ~menu sit → clean quit)  → `A1_boot_menu_fullgame/`
**Delivered 2026-08-15.** Launched the STFS **package** directly
(`…\58410B00\000D0000\D01128AB…E85A58`), booted to the main menu, sat at the menu, then
graceful `taskkill /IM xenia_canary.exe` (no /F) to flush the buffered log. Handed over
alone, first, per the request. Nothing was intentionally skipped by the operator — **but the
log shows no Bink movie was opened (see below).**
Flags: log_level=3, flush_log=false, high_freq=false, dump_shaders=C:/xenia_logs/cw_shaders_A1/,
store_shaders=false, trace_*=off, apu=any, gpu=any, resolution=1×1 (no scaling), all fork
instruments off.
Xenia build: `canary_experimental@a635ac64f on Jul 22 2026`
Clean exit: **YES** — `Cheap-skate exit!` present (line 340213/340311; trailing lines are
async XMA-audio flush — normal for a buffered log).
license_mask / trial: **FULL GAME.** license_mask=1 in the config header; no trial/unlock
prompt; no trial strings in the log. `XamContentGetLicenseMask` is not logged per-call at L3
(import-table only, ord 0x266) — proven behaviourally + via header, same as Case Zero.
Title ID **58410B00**, Media ID 198BA306, Savegame ID 58410B00.

- **★ §W DEVIATES FROM THE REQUEST — no boot Bink played.** ZERO `.bik` opened; none of
  dr2_logo / dr2_logo_b / DR_LOGO_shortLoop / 800a_intro / *_monitors appear ANYWHERE in the
  31 MB log; `NtCreateFile` is logged at L3 (log is full of frontend .big/.tex opens) so a
  movie open would have shown. A "Movie Player Object" IS constructed at boot (two named events
  registered) but was fed no file. The 10 .bik DO ship on disk (66 MB, verified). Working
  interpretation: the boot logos seen were the frontend RATING logos
  (`data/frontend/ratinglogos.big`/`startup.tex` = .big/.tex images, not Bink); the intro Bink
  (`800a_intro.bik`) is most likely a New-Game cinematic, and the pre-title `dr2_logo` loop
  either needs a condition not met here or wasn't reached. **§W3 answer for A1: solo boot→menu
  opens no data/movies/*.bik and spawns no thread for one.** ⇒ **W1 (F4 of the boot logo movie)
  may not be capturable on the boot path** — the reliable Bink is likely the New-Game intro (A2)
  or the in-world *_monitors screens (W2). Operator on-screen confirmation pending.
- **§X.1** — NtCreateMutant (ord 0xD4) / NtReleaseMutant (0xF2) / DbgBreakPoint (0x01) are all
  imported but **never called** in solo boot ~~⇒ they belong to co-op, as hypothesised.~~
  **← THE INFERENCE IS RETRACTED (analyst, same day; see A2's entry and finding 3).** The
  zero is correct and stands: boot does not touch them. The attribution does not — A2's
  SOLO gameplay session calls `NtReleaseMutant` **32,382** times. An absence measured on
  one path is not an attribution to another path.  The observation was the operator's;
  the wrong conclusion drawn from it was mine, in the request's §X.1.
- **§X.2** — solo boot **does** touch low-level networking: `NetDll_WSAStartup`×2,
  `NetDll_XNetStartup`×1, `NetDll_XNetGetTitleXnAddr`×405 are actually called; **no**
  XSession*/XOnline*/XLive/matchmaking. Winsock + XNet title-address come up on the solo path;
  the co-op SESSION layer is not entered.
- Shaders: 168 dumped at boot→menu (98 ps + 70 vs). Delivered `cw_shaders_A1.zip`.

Files: `xenia_A1.log.gz` 744 KB (→ 31,313,744 B, 340,311 lines; md5 d3cb227bb4e379ee65b93cc1d33bb792),
`cw_shaders_A1.zip` 299 KB (md5 0ddcd30408f8195dbd625b49600f03fa),
`xenia-canary.config.A1.toml` 67 KB, `A1_NOTES.txt`.

### B1 + B1b — boot → title GPU streams + determinism control  → `gpu_B1_boot/`, `gpu_B1b_boot_repeat/`
**Delivered 2026-08-15.** Two `.xtr` streams over the same A1 boot→title drive (logos in full,
quit promptly at title, no idle). B1b is the identical determinism-control repeat. **No F4 on
either** (kept frame-comparable). A first B1 attempt was contaminated by F4 presses at each logo
→ discarded and re-run clean; those presses yielded 4 logo PNGs (delivered under `boot_logos/`)
but no paired frame `.xtr` (F4-during-stream screenshots only).
Flags: trace_gpu_stream=true, log_level=3 (free same-run correlation log), flush_log=false,
dump_shaders per-run, store_shaders=false, trace_function_data=off, high_freq=off, instruments
off, apu=any, gpu=any, resolution=1×1, license_mask=1.
Xenia build: `canary_experimental@a635ac64f on Jul 22 2026`
Clean exit: **YES** both (`Cheap-skate exit!`).
license_mask / trial: full game (=1), no trial.

- B1 `58410B00_stream.xtr` = 521,700,871 B; B1b = 555,355,676 B; both header `01 00 00 00`+GUID,
  finalized. 168 shaders each (identical to A1's boot set).
- **Determinism: do NOT read the ~6% size delta (B1b/B1=1.064) as a verdict** — per the AW E1/E1b
  lesson, stream total size is not a determinism metric and a byte-diff is meaningless (ASLR host
  fields differ run-to-run). Same drive, same reached-title, clean quit both; a real check needs
  the analyst's decoder aligned per-frame over the fixed boot+logo prefix.
- Boot logos render as **static images, not Bink** (consistent with A1 §W — zero .bik opens).

Files: `gpu_B1_boot/xtr/58410B00_stream.xtr` 521.7 MB, `xenia_B1.log.gz` 263 KB (→8.6 MB corr. log),
`cw_shaders_B1.zip`, `xenia-canary.config.B1.toml`, `B1_B1b_NOTES.txt`;
`gpu_B1b_boot_repeat/xtr/58410B00_stream.xtr` 555.4 MB, `xenia_B1b.log.gz` 268 KB, `cw_shaders_B1b.zip`.

### C1 — boot → title function coverage (forwards oracle)  → `C1_coverage/`
**Delivered 2026-08-15.** `trace_function_data=true` over the A1 boot→title drive. From-boot
coverage did **not** crash this title (as Case Zero / AW).
Flags: trace_function_data=true, path=C:/xenia_logs/cw_C1/trace.0, log_level=3, flush_log=false,
dump_shaders per-run, store_shaders=false, trace_gpu_stream=off, instruments off, 1×1, license_mask=1.
Xenia build: `canary_experimental@a635ac64f on Jul 22 2026`
Clean exit: **YES**.

- **11,681 executed functions** (call_count≠0), addr range 0x82150000–0x829CBC14 (Case Zero C1 was
  12,278 — same ballpark). 48-byte boolean-coverage records (Case Zero layout: +4 start, +8 end,
  +24 call_count-as-boolean). Treat boundaries as RANGES, not identities; 4-byte fns not comparable.
- This is the forwards oracle for `config/CaseWest.toml`'s `functions` list (recovers indirect/vtable
  entry points no static pass finds). Gameplay C2 will be a superset.

Files: `C1_coverage/trace.0.gz` 110 KB (→32 MiB), `xenia_C1.log.gz` 295 KB (corr. log),
`cw_shaders_C1.zip`, `C1_NOTES.txt`.

### boot_logos — 4 frame-locked PNGs of the boot logo sequence (relates to §W / W1)  → `boot_logos/`
**Delivered 2026-08-15.** F4 screenshots captured during the discarded first B1 attempt. ESRB
MATURE card → Capcom/Blue Castle logos → title. All render with **no movie file opened** ⇒ static
image/texture assets, **not Bink**. Images only (no paired `.xtr` — F4-during-stream is screenshot-
only). **W1 as specced (F4 of a boot Bink) is not achievable — no boot Bink exists**; the real Bink
output-surface capture is retargeted to the New-Game intro (`800a_intro.bik`, in A2) and W2 monitors.

Files: 4× `58410B00 - <ISO>.png` (1280×720), `BOOT_LOGOS_NOTES.txt`.

### A2 — gameplay kernel log (boot → New Game → opening ~5–10 min solo)  → `A2_gameplay/`
**Delivered 2026-08-15.** New-Game intro played in full, then opening area solo — zombies, weapon
pickup, cutscene let play — graceful quit. Nothing skipped.
Flags: log_level=3, flush_log=false, high_freq=false, trace_gpu_stream=off, trace_function_data=off,
dump_shaders=cw_shaders_A2, store_shaders=false, instruments off, apu=any, gpu=any, 1×1, license_mask=1.
Xenia build: `canary_experimental@a635ac64f on Jul 22 2026`
Clean exit: **YES**. license_mask/trial: full game (=1), no trial.

- **★ §W CONFIRMED — Bink plays in gameplay.** `game:\data\movies\800a_intro.bik` (New-Game intro) and
  `807_monitors.bik` (an in-world **monitor screen**, reached in the opening) both open via NtCreateFile,
  resolved by `StfsContainerDevice::ResolvePath(\data\movies)` = streamed **from the STFS package**, not
  the loose copy. Movie Player Object built as at boot; **no dedicated movie thread**. Read cadence is
  kHighFrequency (needs A5). ⇒ W1/W2 GPU capture achievable at the intro + 807_monitors (F4, stream OFF).
- **★ §X.1 REFUTES the request.** Mutants are NOT co-op — in this SOLO session `NtCreateMutant`×6,
  **`NtReleaseMutant`×32,382**, `DbgBreakPoint`×0. Six mutants, released ~32k× = a hot solo-gameplay lock
  (likely audio/streaming). Must be implemented properly, not stubbed. (A1 right that boot never touches
  them; the co-op guess was wrong.) DbgBreakPoint safe to stub (never called).
- Surface: 7.88M XmaContext log lines (audio dominates the 970 MB log), 29 threads, 95 distinct `.big`,
  **1,289 shaders** (7.7× the 168 boot set — larger multiple than Case Zero's ~3×). Nothing wrong in Xenia
  (the only "unimplemented" strings are the import-coverage banner).

Files: `xenia_A2.log.gz` 75 MB (→ ~970 MB; md5 a6c47ea5c4f508f1c9f0b237e22af96e),
`cw_shaders_A2.zip` 3.5 MB (1289 shaders; md5 95a1987eae2d46ecd9ea628e051396bf),
`xenia-canary.config.A2.toml`, `A2_NOTES.txt`.

### B2 — gameplay GPU stream  → `B2_gameplay/`
**Delivered 2026-08-15.** Continuous `.xtr` over a gameplay drive (boot → New Game → intro →
gameplay/combat, solo). Graceful quit finalized the stream. No F4 (clean stream).
Flags: trace_gpu_stream=true, log_level=3 (free correlation log), flush_log=false,
dump_shaders=cw_shaders_B2, store_shaders=false, trace_function_data=off, instruments off,
apu=any, gpu=any, 1×1, license_mask=1.
Xenia build: `canary_experimental@a635ac64f on Jul 22 2026`
Clean exit: **YES**.

- **★ LARGE: 15,527,183,189 B = 14.8 GiB** — the drive ran past the "stop promptly" guidance
  (stream grows ~1 GB/min). VALID/finalized (header `01 00 00 00`+GUID, clean exit). Re-proves the
  2 GiB cliff fix at ~2× the prior max (DR2 CZ B2 was 7.95 GiB). Future gameplay streams: stop
  ~1–2 min after combat starts.
- 1,062 shaders; `800a_intro.bik` is inside the captured era. Same-run L3 correlation log included.

Files: `B2_gameplay/xtr/58410B00_stream.xtr` **14.8 GiB** (15,527,183,189 B),
`xenia_B2.log.gz` 26.8 MB (→393 MB corr. log; md5 8b57dd4bcb5574aecf78f2aff8ca0350),
`cw_shaders_B2.zip` 2.8 MB (1062 shaders; md5 2cecff9144598b7842f6c11d528e28c2), `B2_NOTES.txt`.

---

## Analyst pass — 2026-08-15, session 2

Read after copying all six runs into this repo. Integrity: every `md5`/size in the entries
above verified against the copies; `B2_gameplay/xtr/58410B00_stream.xtr` is exactly
15,527,183,189 B. **The numbered findings are `docs/xenia-capture-analysis.md`**, which is
the authority on any measured number; this section only records what was *done* with the
captures.

**Two claims of mine were refuted by this delivery** — findings 3 (mutants are a hot solo
lock, not co-op) and 4 (there is no boot Bink; a filename is not a call site). Both are
retracted in place in `docs/bootstrap-2026-08-15.md`, `docs/port-plan.md` and the memory
directory.

### Shader counts: read blobs, not files

The per-run counts in the entries above (168 / 1,289 / 1,062) are **file** counts. Xenia
writes three files per shader variant — `*.ucode.bin.*` (raw microcode), `*.ucode.*`
(disassembly), `*.d3d12*.bin.*` (Xenia's own translated DXBC) — so they are ~3× the number
of shaders and are not comparable with Case Zero's blob counts. Distinct microcode blobs:

| run | distinct |
|---|---|
| A1 / B1 / B1b / C1 | **54** each, identical set |
| **A2** | **386** |
| B2 | 321 |
| union | **386** (301 pixel, 85 vertex) |

A1's 54 ⊂ A2's 386 and B2's 321 ⊂ A2's 386, exactly — **A2 alone currently holds the whole
bank**. (Case Zero's shape was A1 120 ⊂ A2 335.)

### What was built from these captures

- **C1 → 43 recovered entry points.** `coverage_to_function_overrides.py` proposed 48;
  `find_dropped_branches.py --prune` removed 5 loop headers that had split real functions;
  one `fix_switch_function_bounds.py` round closed the switch tail that exposed. Config is
  now **76 overrides / 58,387 functions**, with zero `jump outside function`, zero dropped
  branches and the unlowered-switch gate at 0 defects. One of the 43 is `0x825AC918` — the
  image's own entry point, which nothing `bl`s to.
- **All six shader dumps → `assets/shader_spv/`.** 923 blobs in → 386 distinct →
  **386 translated, zero failures**, and `shader_dim_census.py` reports 298 modules 2D /
  98 cube with **0 disagreements** between our microcode parse and DXC's decoration words.
  Microcode kept in `~/DR2CW-troubleshooting/ucode-dumps` (NOT `/tmp`, which is a tmpfs).

### Not yet used

- **`B1`/`B1b` determinism is still open** (finding 12). The ~6% size delta is not a
  verdict in either direction, exactly as the entry says; a real check needs the per-frame
  `.xtr` decoder, which has not been brought across from the Case Zero repo. Until it runs
  this port has no noise floor for GPU stream comparison.
- **B2's 14.8 GiB stream** has not been decoded at all yet — it is the gameplay PM4 ground
  truth and is waiting on a renderer to compare against.

### Still outstanding from round 1

~~**C2**~~ **delivered — see the C2 section below.** Remaining: **A3** (save round trip +
the physical save file), **A4** (title idle), **A5** (high-frequency — now the most
valuable of them: it is the only place the mutant call sites of finding 3 and the Bink read
cadence of finding 5 become visible), **B4** (place-anchored single frames), **W1/W2**
(retargeted — see below), **E** (screenshots).

**W1 is retargeted and the request document is updated.** There is no boot Bink to
photograph. The Bink output-surface frame must be taken at the **New Game intro**
(`800a_intro.bik`) and at the **`807_monitors` screen** in the opening area, as single-frame
F4 captures with `trace_gpu_stream` **OFF** so the frame `.xtr` is actually written.


---

## C2 — gameplay function coverage, driven into a second zone  → `C2_coverage/`

**Delivered 2026-08-15**, shortly after the rest of round 1. Same drive family as A2/B2 but
driven further: the operator reached and **loaded a whole new zone**, which is why C2's
executed set jumps well above C1's. `trace_function_data` from boot did not crash, as in C1.
Flags as C1 plus `dump_shaders=cw_shaders_C2`. Clean exit, `license_mask=1`.

### Analyst pass

Copied and consumed the same day. Findings **13–16** in
`docs/xenia-capture-analysis.md`. Headlines:

- **C2 ⊃ C1 exactly** — zero C1 addresses missing from C2, as the notes predicted.
- **+61 net entry points.** The oracle proposed 67 over both traces; the disposal cycle
  pruned 5, which also cleared the 11 `jump outside function` errors those splits caused.
  Config is now **135 overrides / 58,448 functions**, all gates clean.
  **Two of the 5 pruned had already been pruned in the C1 round** — expected, not a
  regression: fresh coverage re-proposes anything not currently in the config, and the
  oracle's own docstring predicts the loop. Do not chase it next time.
- **★ THE BINK DECODER RUNS AS GUEST CODE — 137 functions inside the `BINK` section
  executed in C2, and ZERO in C1.** The only thing separating those two drives is that C2
  plays the New Game intro. This turns W3's central hypothesis from an argument about
  section flags into a measurement, and it points at "wire up the I/O and the output
  surface" rather than "write a Bink decoder". Two of the 137 needed entry-point recovery
  and are now in the config; without C2 they would have been indirect-call misses the first
  time a movie played.
- **Shaders: the bank grew 386 → 435.** C2 has 415 distinct, **49 of them new**, and A2 has
  20 that C2 lacks — neither drive is a subset of the other. Rebuilt to
  **435 translated / 0 failures / 0 dim-census disagreements**. C2 was requested as a
  *coverage* trace; the 49 shaders came free because `dump_shaders` was on. **Keep it on
  for every capture, whatever the capture is for.**

### One attribution corrected

The notes read the executed range's low end (`0x80050030`) as "code reached only on the
new-zone path". It is **two records at `0x80050030..0x80050038`**, below this title's image
base — that is `xboxkrnl.exe`, another module Xenia traces. Harmless: the oracle bounds
candidates to our image, so they were never proposed. Noted so the range is not read as
evidence about the title. The *upper* end of the same range is the real story, and it is the
BINK result above.

Files: `trace.0.gz` 167 KB (→32 MiB, 19,487 executed by the operator's count / 19,195 by
mine — I additionally require `end > start`), `xenia_C2.log.gz` 126 MB (correlation log),
`cw_shaders_C2.zip` (415 distinct shaders), `C2_NOTES.txt`.


---

## A4 + W1/W2/B4 — analyst pass, 2026-08-15

Findings **17–21** in `docs/xenia-capture-analysis.md`.

### ★ BINK IS SOLVED — and it needs no host code (finding 17)

Frame `01_W1_intro_800a_bik__f4412` decodes as a 4-vertex fullscreen quad binding:

```
 s0 0DF87000 1280x720 k_8 tiled=0 pitchBlk=40  -> Y
 s1 0DEF5000  640x360 k_8 tiled=0 pitchBlk=24  -> U        (pitch 768, PADDED)
 s2 0DF3E000  640x360 k_8 tiled=0 pitchBlk=24  -> V
 s3 17CBE000    80x45 k_8_8_8_8  -> tone map     s4 1783C000 1024x32 -> 32^3 colour LUT
```

converted by the guest's own 144-byte pixel shader `ps_a9f83f703af104b5` (three `tfetch2D`
+ a `mad` against constant coefficients = a YUV→RGB matrix), which is **already in our
cache**. Confirmed five ways: 4:2:0 dimensions; the shader's colour matrix; a luma
histogram piling up on **16** (studio-swing black level); a dump of exactly 1280×720×1
bytes; and **the plane rendering pixel-for-pixel identical to the frame-locked PNG** —
including the typewriter cursor, so the text is baked into the video, not drawn live.
The provenance gate passes (no RESOLVE to the address ⇒ guest-uploaded, a sound oracle).

**Your attribution on this frame was right and my first read of it was wrong** — see the
next item. With finding 14 (137 `BINK` functions execute as guest code), the entire Bink
stack is already ours: no movie player to write, no colour conversion, no ffmpeg fallback.
The only requirements are to **honour `tiled=0`** and to read chroma at its **padded 768
pitch**.

### A tooling trap that nearly buried it (finding 18)

`xtr_draw_bindings.py` lists the **top 12 draws by vertex count**. Every Bink draw is a
**4-vertex fullscreen quad**, so all of them sort to the bottom and the first pass over this
frame reported "no video-sized texture in the frame". A vertex-count sort is backwards for
post-process, UI and video work. The tool now takes `--max-draws`; **use `--csv` before
concluding any texture is absent.**

### A4 (finding 19)

No idle Bink — zero `.bik` over the 5-minute idle, as you observed on screen. The title is
an animated 3D scene, and the log is **64% GPU lines** against Case Zero's ~69%: the same
shape, so the title screen should be as cheap a test bed here as it was there. §X.3
answered. No new shaders — A4's 54 distinct is exactly the boot set.

### Shaders (finding 20)

W session 416 distinct, A4 54; **union now 439** (was 435). Rebuilt: 439 translated, zero
failures, dim census 0 disagreements.

**+3 from nine world frames, against +49 from C2's one new zone.** That is the useful
lesson rather than a disappointment: the W drive covered many new *places* inside a
material set A2/C2 had already visited. **New geometry is not new shaders; a new material
set is.** Which makes the gap you flagged — **no outdoors frame** — the one most likely to
move the number.

### B4's eight world frames

On disk, indexed, not yet analysed beyond the Bink pair: monitor bank, bathroom mirror,
monitor wall, two StoragePens crowds, lab interior with glass, plus the two in-world
monitor frames. They are the reference set for the renderer's hardest surface classes and
they will be read against our output once there is one.
