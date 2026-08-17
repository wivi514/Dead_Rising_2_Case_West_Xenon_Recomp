# Part 3 kickoff — SUPERSEDED, history only

> **⛔ NOT THE LIVE HAND-OFF. `docs/part4-kickoff.md` is.** Superseded 2026-08-16.
> Its §3 said "there is no known defect of our own" — true when written, and part 3 then
> found one: the progress-widget defect, findings 35-47. Sections 1, 2 and 4 (what exists,
> the instruments, the oracle having run out) are still accurate.

**Written at the end of part 2 (2026-08-16). This is the live one.** `docs/part2-kickoff.md`
is **superseded** — read it only as history, and see §5 for why it is also this project's best
cautionary example.

Read this first, then `docs/xenia-capture-analysis.md` (the numbered findings ledger — the
authority on any measured number). `docs/port-plan.md` is the roadmap; `CLAUDE.md` is the
project guide; `docs/imported-fixes.md` is new and tracks everything taken from Case Zero.

---

## 1. THE HEADLINE: THE PORT PLAYS

**Dead Rising 2: Case West boots, renders, and completes Case 1-3** — through new areas, with
zombie combat, cinematics, subtitles, the full HUD, the pause menu, and working save/load.
The operator drove it there on 2026-08-16 and reported **"didn't have any issue except"** two
defects, **both inherited from Case Zero and open there**.

It is also **further than the operator ever drove Xenia**, which has a consequence — §4.

## 2. WHAT ALREADY EXISTS — do not rebuild any of this

**Everything in part 1's kickoff §1 still stands** (clean recompilation, 13 consumed captures,
the transplanted runtime, Bink solved needing no host code, the four parked `port-pending/`
modules). Re-read that section there; it is not repeated here. What part 2 added:

### The renderer works and is on demand

`CW_VKDRAW=1` turns it on — **it is OFF by default and that is deliberate** (it is the control
arm). A run without it presents nothing, which reads exactly like a hang. Part 2 lost its
opening premise to precisely that.

### The instruments you will actually reach for

| env var | what it answers |
|---|---|
| `CW_VKDRAW=1` | turns the renderer on |
| `CW_RING_TRACE=1` | per second: PM4 packets, **frames (XE_SWAP)**, draws, predicated-out, **indirect buffers truncated** |
| `CW_VK_PROFILE=<s>` | frame time, draws/frame, phase split, the guard-promotion line |
| `CW_VK_FRAME_DUMP=<dir>` + `_EVERY=<n>` | writes the **presented** picture as `.ppm` (`grim` does NOT work under this KDE Wayland session) |
| `CW_SHADER_DUMP=<dir>` | microcode dump — **keep it on for every operator session** |
| `CW_NO_WINDOW=1` | headless; presents nothing **by design** |

**`[kcall+]` is NOT a count** — it prints at hit 0 then every 65,536th.

### Gates run and passing at the end of part 2

| gate | state |
|---|---|
| `kernel_call_diff.py` vs A5 `--include-high-frequency` | 5 windows, all permutations, **0 real**, exit 0 |
| `kernel_call_diff.py --derive-mask` | constant agrees with all 11 captures; fails on purpose both ways |
| `xtr_determinism.py` B1 vs B1b | **1.40%** worst aggregate, 0.30% draws |
| `xtr_walk.py stats` on B2 | **INTACT**, 437 M commands, no desyncs |
| `xtr_pm4_census.py --verify` on B2 | 63,037,480 type-3 packets, **0 length mismatches** |
| PM4 opcode coverage | **21/21** opcodes B2 sends are implemented (grep controlled) |
| `build_shader_spv.sh` | **443/443**, zero failures |
| `shader_dim_census.py` | 347 2D / 112 cube, 0 disagreements |
| `cw_runtime --smoke` | 58,695 symbols |
| gap probe | all 7 read 0 over a full play session; control row proved it counts |

Plus part 1's three recompilation gates, unchanged and still clean.

---

## 3. WHERE TO START — and the honest answer is "ask the operator"

**There is no known defect of our own.** That is a genuinely different position from part 2's
start, and it means the next work is a choice rather than a queue. Ranked:

1. **Wait on Case Zero for the two known defects.** Decals (their **00m**, uninvestigated) and
   performance (their **00l**, in progress). Both are filed PENDING in
   `docs/imported-fixes.md` with what to watch. **Do not investigate or fix either here** —
   the operator's standing instruction. When one closes, import it and add a row.
2. **The eight unanalysed B4 frames** — the only unconsumed capture data left, and the
   reference set for the hardest surface classes (monitor bank, bathroom mirror, monitor wall,
   two StoragePens crowds, lab glass, two in-world monitors). This is the highest-value
   *self-servable* work: it needs no operator and no new capture.
3. **Play further.** Case 1-3 is done; the rest of the game is unmeasured. Every session is
   free evidence if `CW_SHADER_DUMP` is on, and the gap probe reports on window close.
4. **W0.3, the recompiler gaps — NOT urgent.** The seven functions holding all 59
   untranslatable instruction sites read **zero** across a full play session (finding 32). It
   is shared XenonRecomp work, so it is the operator's call, and nothing is waiting on it.

### If a defect does turn up

**Check whether an oracle exists for it before spending anything** (gotcha 321, §4). And ask
the cheap question first: *does Xenia show this too?* — that retired two whole investigations
in part 2, one of them in a single `grep -c`.

---

## 4. THE ORACLE HAS RUN OUT — the one thing part 3 must not forget

**Past Case 1-3, this port has no Xenia ground truth at all.** Every capture stops short of
where the runtime now goes, because the operator never drove Xenia that far.

- **For content the captures cover**, guest-side bytes are still ground truth: kernel call
  order, file reads, PM4 packets, shader microcode.
- **Past them there is no capture, no hardware A/B, and no "does Xenia show this too?"**

Two consequences, written up as **gotchas 320 and 321** — the first two authored in this port
rather than inherited:

1. **Xenia is not a ceiling.** It already loses to us on one axis: **it truncates a cutscene's
   dialogue that our runtime plays through.** So a divergence from a capture is a *question,
   not a verdict*, and **an A/B scored as "closer to Xenia = better" will actively regress
   what we already get right.**
2. **Split the defect list by whether an oracle exists for it.** A defect reproducing on the
   title screen or in a menu is worth several found in late content, because it can be
   adjudicated at all. This promotes gotcha 319 from a convenience to a **selection rule** —
   and it is the reason Case Zero's own decal note says to check the menu backdrop first.

---

## 5. WHAT PART 2 GOT WRONG — and it was the kickoff itself

**Part 2's opening premise was false, and part 2 refuted it with the very measurement its own
kickoff asked for.** `part2-kickoff.md` §2 said the runtime "does not present a frame" and
named a 6.8 M/minute `RtlEnterCriticalSection` storm as the thing to diagnose.

The runtime was rendering the title screen at ~31 fps and **waiting for someone to press
START**. There was no picture because `CW_VKDRAW` was unset and the runs behind the note were
headless. The lock storm is the title's own idiom — **Xenia does 1,465 lock enters per frame;
we do 2,567.**

That is this port's recurring error for the **fifth** time: **an absence is a fact about what
was looked at** — here, about which flag the run had set. The other four were a boot-path
zero read as an attribution, a filename read as a call site, a listing truncated by vertex
count, and a missing capture read as a missing area.

**So: read a kickoff for what already exists. Re-measure what it says is broken.** A problem
statement is a hypothesis with a shelf life (gotcha 13), and this file is not exempt either.

One more from part 2, smaller but live: **`kernel_call_diff.py` carried Case Zero's derivation
of its kHighFrequency mask**, which on this title was wrong in both directions at once. It now
has `--derive-mask` to regenerate the constant from the captures. **Grep any copied tool for
the sibling's constants before trusting one run** — that is three tools caught this way now.

---

## 6. OPERATIONAL

- **Commit with plain `git commit`.** Never `-c user.email` / `-c user.name`. The repo config
  already holds `wivi514 <wivi514@hotmail.com>`.
- **The operator drives.** Launch the game for them with instruments wired; do not run it
  headlessly and do not stop at a recipe. Their reports have now refuted this project's
  premises five times and have repeatedly been better than what was asked for.
- **Instruments are `CW_*`.** Translate the prefix when quoting any Case Zero doc — a `CZ_`
  name compiles and silently does nothing.
- **`/tmp` is a 32 GB tmpfs and it has been cleared mid-session.** Use the session scratchpad;
  keep microcode dumps in `~/DR2CW-troubleshooting/ucode-dumps`.
- **The coverage traces have a 124 KB hole** (`0x825D1028`–`0x825F0000`, finding 31). Never
  answer "does this execute?" from C1/C2 in that range, and **run a positive control before
  believing any trace-derived zero** — that is what caught it.
- Xenia captures run on the operator's Windows PC and land under
  `/mnt/ideapad3/Ideapad3Server/GithubRepo/<repo>/Xenia logs/`.
