# Part 4 kickoff — SUPERSEDED

> **SUPERSEDED 2026-08-23 by `docs/part6-kickoff.md`.** Parts 4 and 5 both ran; §1's one
> active task — the progress-widget defect — is **CLOSED** (finding 60). Its mechanism was
> not any of the ones this document reasons about: it was the small-packed-texture read,
> and `CW_VK_NO_PACKED_SMALL=1` is the arm that proves it. **Finding 59, written during
> part 4, is retracted in place.** Read this file for what was measured, not for what it
> says is broken.


**Written at the end of part 3 (2026-08-16). This is the live one.** `part3-kickoff.md` is
superseded; `part2-kickoff.md` before it is kept only as a cautionary example (its problem
statement was false).

Read this, then `docs/xenia-capture-analysis.md` (the numbered findings ledger — the authority
on any measured number). `docs/imported-fixes.md` tracks everything taken from Case Zero.

---

## 1. THE ONE ACTIVE TASK: the progress-widget defect

**The PP bar, the LIFE meter's empty squares, the mission timer bar and the loading pop-up's
segmented bar do not render.** Present in Case Zero too, and **Case Zero has not started on
it**, so this one is ours to lead and anything found here feeds back.

Findings **35-47** are this investigation. Read 47 first, then 44 (a retraction), then 36.

### What is measured and settled

```
hardware draws them with ONE vertex shader   vs_a4ae7c2b7c1818c4   (f36)
   55 draws in the HUD frame, 27 in the loading pop-up
our runtime, three captured frames            ZERO draws with it   (f37, f38)
the PP bar region                             ours R2 G3 B2 vs hardware R6 G66 B85 (f38)
   with a positive control: LIFE filled pips agree to within 5 units
```

**Every GPU-side explanation is eliminated by measurement** (f35-39): the shader is in the
bank and is real 15-dword microcode; our runtime loads it; topology is mapped; the stream
guard was refuted by A/B; the ALU constant window is read from the registers; and we predicate
out **0.29%** of draws against hardware's **0.3%**. The draws simply never reach the renderer.

**And the guest-side lifecycle is healthy too** (f41-47):

```
cFEMeter created 70 · linked into the widget list 69 · walked 183,190 x
   updated by sub_8281A5E0 (183,190 calls, 100% on meters — the class's own update)
   laid out by sub_82816128 (106 firings, ~1.5 per meter — a one-shot layout completing)
   thirteen of its 32 hookable vtable slots active
```

**So the meter is built, linked, driven and laid out — and never becomes geometry.** What has
not been found is the call that turns a laid-out meter into draws.

### The addresses that are ground truth

Derived by reading the factory table **out of guest memory while the game ran** — not from
static analysis, which was wrong twice:

```
factory table          0x82AF3118, 25 entries of {creator, id, name}
cFEMeter is entry 16   creator 0x82819640 -> 0x82815B80 (alloc 0x2F0) -> ctor 0x8280D300
cFEMeter vtable        0x820BDBE8, 40 methods
   slot 30  0x8280D438  SetNext   (field 0x1CC is a LIST LINK, not a value)
   slot 31  0x8280D440  GetNext
   slot  6  0x8281A5E0  the class's per-widget update
CreateWidget(name)     sub_82784588 — silently returns 0 if the class is unknown
FindClass(name)        sub_82784508 — matches by interned id, generic, also used by the
                       screen parser for property tokens (most of its -1s are normal)
```

### THE NEXT MEASUREMENT, and it is named

**Run the vtable census against a class that DOES draw — `cFEBitmap` or `cFEText` — and diff
it against the meter's.** The filter is `kMeterVtable` in `runtime/cpu/fe_probe.cpp`; pointing
it at another class's vtable is a one-constant change, and that class's vtable address comes
from its constructor the same way the meter's did.

That converts the question from *"which slot looks like a draw?"* — which has now been guessed
wrong four times — into *"which slot do drawing widgets receive that meters do not?"*, which
is a comparison. **This is the measurement the investigation has been missing.**

Not yet done, and lower priority: `sub_82803308` is the second slot-21 call site and the one
whose guard is real, but its widget is iterated inside the function rather than passed in, so
it needs a loop-body probe. `sub_82803570`, the other site, **runs zero times** — that lead is
untested, not refuted (f47).

## 2. THE INSTRUMENT

`runtime/cpu/fe_probe.cpp` — always on, reports from **both** shutdown paths, so an ordinary
play session produces everything without flags:

```
cd runtime/build && CW_VKDRAW=1 ./cw_runtime 2> run.log    # play, CLOSE THE WINDOW
```

Closing the window matters — killing the process from a terminal loses the report.

It prints: the `CreateWidget` census **by class name**, the classes the parser asks for, the
names that fail to resolve, the cFEMeter lifecycle counters, the per-slot vtable census, and
**the factory table read from guest memory**.

`runtime/cpu/gap_probe.cpp` is the sibling instrument for W0.3 and is also always on.

## 3. THE METHODOLOGICAL LESSON OF PART 3, and it is expensive

**Four readings in a row had correct counters and a wrong story attached** — findings 43, 44,
46, and 47's slot-21 lead. The counters were never wrong.

- **The creator→class map was off by one, twice** (f43, f44), and produced a fully-built probe,
  a plausible chain, and a confident *"cFEMeter is never built"* that was entirely about
  cFEParticleFX. **Every count matched once the label shifted by one.** Gotcha 322.
- **The allocation-size hypothesis** (cFEMeter's 0x8920 looked damning) died to a table:
  `cFELockBox` at 0x2D0 is also unbuilt and `cFEBitmap` at 0x490 builds fine (f43).
- **Field 0x1CC "the meter's value"** is a linked-list next-pointer (f46).

What worked, every time, was **refusing to name things from static reading**: reading the
factory table out of guest memory while the game ran; hooking `CreateWidget` by the class name
it is *passed*; recording the **link register** so the caller identifies itself; and filtering
a shared function by the object's own vtable pointer — something the guest itself wrote.

**Carry that forward. Identify by what the guest wrote, not by where it sits.**

## 4. EVERYTHING ELSE — done, or not ours

| | |
|---|---|
| The port | boots, renders, **completes Case 1-3**, save/load works |
| HUD text | ✅ **FIXED** — imported from Case Zero `82d181f`, confirmed in play |
| Decals | ⏳ Case Zero's open item **00m**, uninvestigated there. **Not ours** |
| Performance | ⏳ Case Zero's **00l**, in progress there. **Not ours** |
| Shader bank | 443, rebuilt from our own dump, census clean |
| GPU determinism | noise floor **1.40%**; a frame-INDEXED gate is not viable |
| B2 | decoded — 21 type-3 opcodes, **we implement all 21** |
| W0.3 | not urgent — all seven functions read zero across a full play session |
| B4 | **eight of nine frames still unanalysed** — the only unconsumed capture data |

**Past Case 1-3 there is no Xenia ground truth at all** (gotcha 321). Split any defect list by
whether an oracle exists for it; a defect that reproduces on the title screen is worth several
found in late content.

## 5. OPERATIONAL

- Plain `git commit`. Never `-c user.email` / `-c user.name`.
- **The operator drives.** Launch the game for them with instruments wired.
- Instruments are `CW_*`; a `CZ_` name compiles and silently does nothing.
- `/tmp` is a tmpfs and **has been cleared mid-session**. Use the session scratchpad.
- The coverage traces have a **124 KB hole** (`0x825D1028`–`0x825F0000`, f31). Run a positive
  control before believing any trace-derived zero.
- Captures land under `/mnt/ideapad3/Ideapad3Server/GithubRepo/<repo>/Xenia logs/`; copy in and
  index in `Xenia_Run_Content.md`.
