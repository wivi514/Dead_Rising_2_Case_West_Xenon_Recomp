# Xenia ground-truth captures needed for Case West — request list (round 1)

**Written 2026-08-15, session 1, before any runtime work. Nothing has been captured yet.**

This is a request to a human at another machine. Xenia is unstable on the Linux box this
repo lives on, so **nothing here can be run or checked from here** — no cvar can be
verified by grepping a local source tree and no "let me just try it" shortcut exists.
Every item is written to be executed without a follow-up question.

**It is deliberately shorter than Case Zero's round 1 was**, because that port already
answered several of the questions a first round normally exists to ask. What it answered,
and therefore what is *not* being requested again, is §0.

Priorities are ordered: **A is enough to start the runtime; B and C unlock the renderer
and the differential debugging that carried all three earlier ports through their hardest
bugs; W is the one genuinely new subsystem in this title.**

Everything lands in `Xenia logs/` (gitignored), with one entry per run appended to
`Xenia logs/Xenia_Run_Content.md` (tracked): what you did, in order, plus anything
unusual — crashes, missing graphics in Xenia itself, skips.

---

## Read this before capturing anything

### Do A1 first, alone, and hand it over before doing the rest

A1 is ~20 minutes. Everything else is hours. Every round-1 request list written in this
workspace has contained an assumption that turned out to be wrong, and Case Zero's round 1
contained two. **Capturing one log, confirming it is readable and says what this document
expects, and only then batching the rest, costs one message and can save an evening.**

### THE NON-NEGOTIABLES

1. **`license_mask = 1`.** This is a paid XBLA arcade title and the default boots the
   **TRIAL**. It bit Case Zero's very first capture exactly as its request predicted: the
   trial's boot differs measurably (one archive reloaded 1,164× against 2×, a 45.8 MB log
   against 13.9 MB), so a trial capture is not a smaller full-game capture — it is a
   different code path. Check early in A1's log for `XamContentGetLicenseMask` and record
   what it returned. **If the game shows any trial/unlock prompt, stop and fix the mask.**
2. **Launch the STFS package directly**, not an extracted `default.xex`:
   `…\58410B00\000D0000\D01128ABB9C7F9694DAE26AC591A269F8480E85A58`.
   The content-mount calls at boot differ, and the package is what the console sees.
   Note in the index which you used. Title ID is **58410B00** (Case Zero was `58410A8D` —
   easy to grab the wrong folder).
3. **Never skip the movies, and say in the notes that you didn't.** Asura's Wrath's B1
   note said "skipped all intro movies", three sessions of GPU diffing went into comparing
   loading-movie frames against title-screen frames, and the packets eventually showed the
   drive had covered the movie era all along — **the prose note was simply wrong**. Let
   every logo/intro/loading movie play in full. If you had to skip something, say exactly
   what. **This matters more here than it did in Case Zero**: those movies are Bink, and
   Bink is §W.
4. **`dump_shaders` ON for every single run in this document.** It costs nothing, it is a
   path cvar rather than a boolean, and every area a run reaches is a shader our cache
   would otherwise not hold. In Case Zero the shader bank grew on *every* session that
   reached new ground — 335 → 435 over the port.
5. **Render at 1280×720**, no resolution scaling, so a screenshot can be compared with
   ours pixel-for-pixel rather than by impression.

### Mechanics that cost Case Zero something

- **Only `--log_level=3` names kernel calls.** Level 2 prints handle churn and file paths
  but *no HLE call names at all*. A level-2 log is not a smaller level-3 log; it is a
  different and much less useful thing.
- **`log_level=3` is still not the whole kernel surface.** Exports tagged `kHighFrequency`
  — `NtReadFile`, most of the synchronisation surface, `VdSwap`, `XamInputGetState` — are
  logged only with `log_high_frequency_kernel_calls = true`, which defaults **off**. That
  is why A5 is a separate item.
- **Keep `flush_log = false` when high-frequency logging is on.** Asura's Wrath deadlocked
  on high-freq **+ flush=true**; Case Zero's A5 with flush off did not hang at all.
- **The fork ignores `log_file`** and always writes `<cwd>\xenia.log` — copy the log out
  after a clean exit. `dump_shaders` with a forward-slash path worked fine, so it is
  `log_file`-specific rather than path escaping.
- **Let the log flush on exit.** Case Zero's A2 lost its final buffer batch because Xenia
  was force-closed before a graceful `taskkill`. A clean exit ends with
  `Cheap-skate exit!` in the log; if that line is missing, say so in the index.
- **Keep the config-dump header** Xenia prints at the top of every log — never trim it —
  and note the `Build: …` line in the index. If you changed anything in
  `xenia.config.toml`, copy the toml next to the log.
- **The `.xtr` 2 GiB cliff.** Asura's Wrath's gameplay trace overshot 2 GiB by ~15 KB and
  had to be discarded entirely (an `ftell` limit in the writer). You fixed this at source
  for Case Zero's B2 — if that fix is still in your fork, say so in the index and ignore
  this; if you are on a fresh build, stop a GPU capture promptly at the end of its drive
  rather than idling.
- **A `.xtr` must be running from process start**, not attached later. Frame 0 matters
  more than any other frame.

### Single frames beat streams for anything place-anchored

Case Zero's round 1 asked for continuous `trace_gpu_stream` captures and its round 2
asked for "the same method". **You deviated and were right to** — a stream cannot produce
several separately named place-anchored traces in one session, and single-frame F4 traces
can, and they are self-contained besides (an `EdramSnapshot` plus the actual sampled bytes
of every texture the frame touched, so each one replays standalone). Round 3 you improved
the fork further so F4 also captures Xenia's own guest framebuffer at the trace press,
which makes the PNG **frame-locked to its `.xtr`** and usable for pixel questions.

**So: B1/B1b/B2 below are streams because they are about eras and cadence. Everything
place-anchored — §W and §B4 — should be single-frame F4 captures with their frame-locked
PNGs, and if you think a different mechanism answers the question better, use it and say
what you did.** Ask me for outcomes, not flags; you know the tool better than this
document does.

---

## §0. What Case Zero already answered — deliberately NOT requested again

Listed so you do not spend an evening re-capturing something we already hold, and so a
later session does not read the absence as an oversight.

| question | why it is closed | what would reopen it |
|---|---|---|
| **Do the loose disc shader banks hold usable microcode?** `data/shaders/deadrisingepilogue-{vs,ps,vd,pd,sc,sd,ss}.big` | **No.** Case Zero's finding 6: they are `.big` archives of `<hash>.vo` shader *objects* carrying build metadata (including `.updb` debug paths), sharing only background-noise overlap with the microcode the guest actually submits. Its round 1 spent a whole section on this. | nothing — we go straight to `dump_shaders` |
| **The `.big` container format** | **Cracked** (`docs/big-archive-format.md` in the Case Zero repo), and this title's archives read correctly with our existing reader on the first try. | a `.big` this reader chokes on |
| **Does `dump_shaders` work / what does it emit?** | Yes — raw Xenos microcode (`.ucode.bin`) + disassembly + Xenia's translated DXBC. | nothing |
| **Is `--trace_function_data` stripped in Canary?** | No. | nothing |
| **Are the Debug-only GPU cvars real?** | `log_guest_driven_gpu_register_written_values`, `disassemble_pm4` and `log_ringbuffer_kickoff_initiator_bts` are gated at **compile time** on `XE_DEBUG` and are silently compiled out of a Release build — an arm using them returns 0 lines, which reads as "the cvar did nothing" rather than "this build cannot do that". | still not needed yet; see §Z |

**The one Case Zero answer that must NOT be carried over is its finding 7, "there is no
Bink in this game".** That was true of Case Zero and is false here. See §W.

---

## §A. Kernel-call text logs — the priority

Ground truth for boot order, kernel HLE return values, file I/O, thread creation and
content mounting. All three earlier runtimes were written against these from day one, and
Case Zero's `kernel/imports.cpp` is literally ordered by what its A1 called.

```
xenia.exe --log_level=3 --dump_shaders=C:\xenia_logs\cw_shaders_A1\ ^
          "…\58410B00\000D0000\D01128ABB9C7F9694DAE26AC591A269F8480E85A58"
```

### A1 — SHORT boot at maximum verbosity  ★ do this one first, alone

Boot → every logo/intro movie played **in full** → main menu → sit ~60 s → quit cleanly.

This becomes the sequence our runtime must reproduce. It is also where the licence-mask
check happens, and where §W's first question gets answered for free.

### A2 — into gameplay

Boot → New Game → play the opening ~5–10 minutes → quit. Get out of the opening area,
fight some zombies, pick up a weapon, and **let any in-game cutscene play rather than
skipping it**. Adds the gameplay kernel surface: streaming loads, physics/audio thread
creation, XMA context allocation, and the gameplay shader set — which in Case Zero was
about 3× the boot set.

**Play it SOLO.** Co-op is out of scope for this port (see §Y) and a co-op session would
add a subsystem we are deliberately not building.

### A3 — save / content round trip

Boot → New Game → reach the first save point → save → back to the menu → load that save →
quit. **Please also copy the physical save file out** and note where it lived.

Case Zero derived its whole save layer from the title's own statically linked
`XamEnumerate`, and that derivation should transfer — but the title ID differs, the save
shape differs, and Asura's Wrath needed `savedrive:` → `\Device\Content\N\` where a plain
file write would not have worked. This is what tells us the exact shape *here*.

### A4 — idle at the title screen, long

Boot → title screen → leave it alone ~5 minutes → quit. A quiet log makes the per-frame
steady state legible against A1's noisy boot. **Note whether the title screen has an
attract loop or an idle Bink playing** — if it does, that is a §W capture for free.

### A5 — high-frequency kernel calls (separate run)

A1's drive with `--log_high_frequency_kernel_calls=true` and **`flush_log=false`**. Expect
it to be large. Case Zero's equivalent did **not** hang with flush off; if this one does,
say so and keep however far it got — even a boot-only prefix is the only view of the
synchronisation surface, `VdSwap` and `XamInputGetState`.

**Lower priority than it was for Case Zero**, because its main job there was cracking the
`.big` format and that is done (§0). It is still the only place the synchronisation
surface appears — and this title imports **`NtCreateMutant` / `NtReleaseMutant`, which
Case Zero does not** (§X), so A5 is where we find out what they guard.

---

## §B. GPU command-stream traces (`.xtr`)

Stock Canary strips the `.xtr` writer in Release; your instrumented fork forces it on.
All custom instruments in the fork **off**, vanilla settings otherwise.

### B1 — boot → title screen

**Same drive as A1**, movies played in full. Gives the draw profile per frame, the shader
set, the EDRAM layout and the swap cadence for everything up to the menu.

### B1b — B1 again, unchanged

Identical drive, identical settings, second run. **This is the determinism control and it
is not optional.** Asura's Wrath only got a repeat capture in round 3 and it immediately
paid for itself: it established that a GPU stream is deterministic in content and jittery
in phase (±2 frames at era boundaries, 0.38% on totals). Without it there is no way to
tell a real defect from run-to-run noise, and that port spent time treating noise as
signal. One extra run now.

### B2 — gameplay

**Same drive as A2.** Expect an order of magnitude more draws per frame. Watch the 2 GiB
cliff — stop promptly at the end of the drive rather than idling.

### B4 — place-anchored single frames  ★ the ones that will matter most later

**Single-frame F4 captures with their frame-locked PNGs**, standing still, a few seconds
each. Case West's areas, as far as the drive reaches — the Phenotrans facility interiors
are the bulk of the game and are a different visual problem from Case Zero's outdoor Still
Creek:

- the **Safehouse**;
- a **SecureLab** interior;
- the **StoragePens** (this is the zombie-crowd area — the CrowdEngine's worst case);
- a **LivingResearch** interior;
- **outdoors**, if the drive reaches it;
- anywhere with **a lot of glass, a mirror, or a monitor bank** — those were Case Zero's
  hardest surfaces and a monitor bank is also a §W target.

If you cannot reach some of these in one sitting, capture what you can and say which are
missing. **Partial is fine; unlabelled is not.**

### Same-run correlation log — please treat as required, not optional

If the fork will emit both at once, take a `--log_level=3` log **from the same run** as
each `.xtr`, so a frame index can be tied to a file open. Asura's Wrath's paired captures
were worth more than either artifact alone; its unpaired B-series is part of why the
movie-era confusion above went unnoticed for three sessions. If pairing costs a second run
instead, **the `.xtr` is the one that matters**.

---

## §C. Function-coverage traces (`--trace_function_data`)

```
xenia.exe --trace_function_data=true --log_level=3 … "path\to\package"
```

### C1 — boot → title.  C2 — gameplay.

Same drives as A1 and A2.

This is a **two-sided** oracle and most ports use only one side:

- **Forwards** — *hardware ran an address we have no function for* — recovers missing
  entry points for `config/CaseWest.toml`'s `functions` list: vtable slots and
  runtime-built function-pointer tables that no `bl` points at and that carry no `.pdata`
  record, so nothing static will ever find them. Without them there is no `sub_<addr>` and
  `PPC_CALL_INDIRECT_FUNC` dies at runtime. Asura's Wrath recovered 215 this way, Case
  Zero 110. **This is the item in §C that pays off immediately, before any runtime
  exists** — which is why §C is not "later" work despite the second half being.
- **Backwards** — *we ran a function hardware never ran* — localises a control-flow
  divergence to a single function, with no debugger and no reproduction. Needs a running
  runtime, so it is for later.

Treat the capture's function boundaries as **ranges, never identities**: the emulator's
function analysis will not agree with the recompiler's, and 4-byte single-instruction
functions are not comparable at all — they were 52 of 52 first-pass false "divergences" on
Asura's Wrath.

---

## §D. Shader dumps — not a separate capture

`--dump_shaders=<dir>` on **every** run in this document, into a **per-run directory**.
Do not overwrite one directory across runs; the value is in knowing which area produced
which shader.

This is the renderer's only input. It is not optional and it is not a section you can
skip if you are short on time — it is a flag on captures you are taking anyway.

---

## §W. BINK — the one genuinely new subsystem, and the reason this round exists at all

**Case Zero has no Bink. Case West does, and it is on the boot path.** Its round-1
document inferred Bink from two strings and later retracted it after measuring that no
decoder ran and no `.bik` shipped. Do not carry that retraction here — the evidence in
this title is not strings:

- **four Bink sections in the XEX, one of them executable** (`BINK`, 67 KB of PowerPC
  code, plus `BINKCONS )`, `BINKBSS`, `BINKDATAT=` and `.XBMOVIE`);
- **ten `.bik` files ship**, 66 MB, in `data/movies/` — `dr2_logo.bik`, `dr2_logo_b.bik`,
  `DR_LOGO_shortLoop.bik`, `800a_intro.bik` (33 MB), and six `*_monitors.bik`;
- a wrapper with its own source path,
  `c:\bcg\deadrisingepilogue\library\movielib\Common\binkmovieplayer.cpp`;
- runtime path construction `data/movies/%s`, naming in-world screens that do **not** ship
  as loose files (`factslaptop.bik`, `cine_loungescreen_mov.bik`,
  `securityroomB_monitors_mov.bik`) — so this is not only the intro.

The working hypothesis here is that **the decoder is already recompiled** — `BINK` is a
code section, XenonRecomp translated it, and if the decode is pure computation over guest
memory it should simply run, leaving only the I/O and the output surface as host work. The
two guest strings that locate that seam are
`ERROR!! You are passing a write combined memory address to Bink!` and
`cached memory for the Bink texture pointers - see BinkTextures.cpp`, both about
GPU-visible memory. **These captures are what tests that hypothesis.**

### W1 — the boot logos, as a single-frame trace **while a movie is on screen**

The boot Bink is unmissable and free: A1/B1's drive already plays it. What is needed
beyond that is **one F4 single-frame capture with the logo movie actually on screen**,
plus its frame-locked PNG. That one frame tells us the output surface format, its
dimensions, how it is bound, and what the draw that presents it looks like — which is
exactly the seam above.

### W2 — an in-world monitor screen

Same thing, at a **security-room monitor bank or the lounge screen** in the Phenotrans
facility, if the drive reaches one. These are the `*_monitors.bik` / `cine_*_mov.bik`
paths, and they are the interesting case: a movie playing as a **texture on geometry**
rather than fullscreen, which is a different binding and possibly a different format.

**If you can only get one of W1 and W2, get W1** — it is on the boot path, so it blocks
earlier.

### W3 — the kernel/file view, which comes free with A1

Nothing to capture. Just **tell me what A1's log shows around the movie**: whether
`data/movies/*.bik` is opened with `NtCreateFile`, whether the reads are streamed or
whole-file, and whether any thread is created for it. A5's high-frequency log is where the
reads will actually be visible (§A5).

---

## §X. Three pre-registered questions, stated before the captures exist

So these are answers to questions rather than stories told afterwards. Each is cheap and
each changes what gets built.

1. **`NtCreateMutant` / `NtReleaseMutant` / `DbgBreakPoint`.** These are the *only* three
   kernel imports Case West has that Case Zero does not — the import table is otherwise a
   strict superset. **Does A1 (or A5) show them being called, and around what?** A mutant
   is a real synchronisation primitive that must be implemented properly rather than
   stubbed, and knowing what it guards decides how much care it needs. If they never
   appear in a solo boot, that is also an answer, and it probably means they belong to
   co-op (§Y).
2. **Does a solo boot touch co-op initialisation?** Multiplayer is out of scope, and the
   import census says deferring it cannot introduce an unimplemented import. What it does
   not rule out is single-player walking through co-op setup on its way past. **Grep A1
   for `Coop`/`Xn`/session-shaped calls and say what you see.**
3. **Does the title screen or main menu differ from Case Zero's structurally** — a
   different number of swaps per second, an attract loop, a Bink idling behind it? Case
   Zero's title screen was the cheapest place to test almost everything, and it will be
   here too if it behaves the same way.

---

## §Y. Explicitly NOT requested

- **Any co-op / multiplayer capture.** Out of scope for the port (operator's call). Play
  everything solo. If the game forces a networking path even in single-player, that will
  show up in A1 and we will deal with it then — see §X.2.
- **A dedicated audio capture.** A2 at level 3 already shows XMA context allocation and
  the `XAudio*` pump, which is what the audio phase needs to start. A targeted capture
  should wait until there is a specific question for it to answer. Asura's Wrath produced
  two rounds of requests it later had to retract because they were written before the
  question was sharp — *a capture request is a hypothesis with a shelf life*.
- **Debug-build GPU register captures.** See §Z.
- **`trace_function_coverage`** (distinct from `trace_function_data`). It writes a
  *per-instruction* branch oracle in a different, self-describing record format, and
  Asura's Wrath's reader misread it as zero functions and 762,193 resyncs — i.e. as an
  inert flag — when it was the most detailed oracle available. Worth remembering it
  exists; not worth capturing until there is a divergence to chase with it.
- **A `.big` seek-order capture.** The format is cracked and this title's archives read
  with our existing tool (§0).

---

## §Z. If you are rebuilding the fork anyway

**Build Debug alongside Release. Do not capture with it yet; just have it.**

Three GPU cvars — `log_guest_driven_gpu_register_written_values`, `disassemble_pm4`,
`log_ringbuffer_kickoff_initiator_bts` — are gated at compile time on `XE_DEBUG`. In a
Release build they are silently compiled out, so an arm using them returns zero lines,
which reads as "the cvar did nothing" rather than "this build cannot do that".

We do not need those captures yet — they belong to the renderer phase and the first is
only useful once we know what to ask it. But if a build is happening now, producing the
Debug binary costs a compile and removes a future round trip.

For scale on what it eventually gives: on Asura's Wrath that register log was 4.9 M writes
over 2,473 registers, which reads as "port the whole Xenos register file" — until it is
reduced by *distinct value count*, at which point 2,255 registers never change and **48**
are the actual render state. A 50:1 compression, and the difference between a month and a
week.

---

## Delivery and indexing

Copy captures into this repo's `Xenia logs/` and append one entry per run to
`Xenia logs/Xenia_Run_Content.md`, which **is** tracked in git and is the only thing that
survives if a file is later lost. An unindexed capture is a file nobody can interpret in
three weeks.

Each entry should say, briefly:

- what you did, **in order**, and anything you skipped;
- the flags and any `xenia.config.toml` deviation;
- the Xenia `Build:` line;
- whether the log ended with `Cheap-skate exit!`;
- what `XamContentGetLicenseMask` returned / whether any trial prompt appeared;
- anything that looked wrong **in Xenia itself** — a surface Xenia renders badly is not a
  target for us to match, and knowing that early has retired whole days of measurement in
  the sibling port.

**And say when something in this document turns out to be wrong.** Two of Case Zero's
round-1 premises were false and both were caught by the operator's notes rather than by
the request. That is the most valuable thing a capture round produces.

---

## Priority, if the whole list is too much for one sitting

1. **A1** alone, handed over first — with `dump_shaders`. (~20 min)
2. **W1** — one Bink frame. It is on the boot path and it is the only genuinely unknown
   subsystem in the port. (minutes, bolted onto B1)
3. **B1 + B1b** — the boot stream and its determinism control.
4. **C1** — the forwards coverage oracle; pays off before any runtime exists.
5. **A2 + B2 + C2** — gameplay, the big ones.
6. **A3** — save round trip, plus the physical save file.
7. **B4** — the place-anchored frames.
8. **A5** — high-frequency, mainly for §X.1.
9. **A4, W2** — nice to have.
