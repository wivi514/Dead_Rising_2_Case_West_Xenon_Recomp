# Case West port plan

Written 2026-08-15, at the end of session 1. The evidence behind every claim here is
`docs/bootstrap-2026-08-15.md`; where the two disagree, the bootstrap doc wins because it
is the one with measurements in it.

**Scope note from the operator: multiplayer / co-op is OUT for now.** Case West's
headline feature is two-player co-op and the image is full of it, but nothing in this
plan may block single-player on it. See W7.

---

## The strategy, in one paragraph

Case Zero is ~46 parts deep and close to complete, and session 1 measured that Case West
is the **same engine, same compiler, same container, same asset formats, and a strict
superset of the same 244 kernel imports**. So this is not a port from scratch: it is a
**transplant plus a delta**. The plan is to lift Case Zero's runtime essentially whole,
get to a picture as fast as possible, and spend the real effort only on the four things
that are genuinely different — Bink, the fresh instruction set, this title's own shader
bank, and its own content. The risk to manage is the opposite of Case Zero's: there, the
danger was not knowing anything; here, it is **assuming a Case Zero answer transfers when
it does not**. Session 1 already caught three of those (§7 of the bootstrap, and the Bink
retraction that must NOT be applied).

## Where the port is now

Done in session 1 — reproducible from a clean clone with the Commands section of
`CLAUDE.md`:

| | state |
|---|---|
| Package unpacked | ✅ 305 files, 1.2 GB in `assets/game/` |
| Image dumped | ✅ `assets/game/default_image.bin` (+ `.sections`), 14 sections |
| Save/restore helpers | ✅ all 8, in `config/CaseWest.toml` |
| Jump tables | ✅ 205 tables / 5,791 labels, both code sections |
| Recompilation | ✅ **58,448 functions**, 228 TUs, **zero** `jump outside function`, **zero** dropped branches |
| Coverage oracle | ✅ **104 entry points** recovered from C1+C2; config at 135 overrides |
| Shader cache | ✅ **439 shaders, 439 translated, 0 failures** — W4's hardest input already exists |
| Runtime (W1) | ✅ **links and boots**; 4 per-title modules parked, save layer awaiting A3 |
| Bink (W3) | ✅ **SOLVED — no host code needed** (finding 17); honour `tiled=0` and the padded chroma pitch |
| Kernel import surface | ✅ 247 names, measured against Case Zero's 244 |
| Recompiler gates | ✅ dropped branches and unlowered switches both clean (W0.1, W0.2) |
| Runtime | ❌ nothing yet — `runtime/` does not exist |

Nothing has ever been compiled by a C++ compiler, and nothing has been checked against
hardware. **There is no Xenia capture of this title.**

---

## W0 — close the recompiler gates (no host code yet)

The three config-maintenance passes Case Zero ran, in order, regenerating `ppc/` between
each. **Items 1 and 2 were completed in session 1**; item 3 is what is left, and it is
the one that touches a shared tool.

1. ~~`find_dropped_branches.py`~~ — **DONE in session 1.** 18 branches across 8
   truncated functions, driven to zero over three widen/regenerate rounds; two further
   rounds confirmed the fixpoint. Zero backward/split cases. Re-run after any change to
   the function list — it is the only check that catches the coverage oracle's
   loop-header splits (gotcha 28).
2. ~~`find_unlowered_switches.py`~~ — **DONE in session 1, exit 0.** 205 lowered, 2
   switch-shaped-but-not-lowered, both benign frameless thunks, **0 defects**. Still a
   gate, not a repair (gotchas 53–55): re-run last and after every config change.
3. Implement the **6 missing mnemonics** — `vminsw`, `vpkshss`, `vavgsw`, `stdux`,
   `vpkshss128`, `stvebx` — plus the **20 `float16_4` pack sites** clustered around
   `0x825E6904–0x825E7490`, in `~/GithubRepo/XenonRecomp`, then regenerate and confirm
   zero. Upstream them: they are hardware instructions, not title-specific.

   *An unimplemented instruction is not a build failure. It is a silent wrong-execution
   trap.* Close this before any runtime work, exactly as Case Zero did.

**Gate:** recompiler log is clean on all three categories. Two of the three are already
clean; **only item 3 is left**, and it is work in `~/GithubRepo/XenonRecomp`, which is
shared with the other three ports — so it is not a change to make unilaterally.
**Cost:** most of a session. The mnemonics are mechanical against XenonRecomp's existing VMX
emitters; the `float16_4` cluster is the unknown and is probably one guest function —
disassemble it with `tools/gdis.py` first and find out what it converts.

## W1 — the runtime transplant

Copy `~/GithubRepo/Dead_Rising_2_Case_Zero_Xenon_Recomp/runtime/` (~34,000 lines across
CPU, kernel, GPU, audio and host) and retarget it. **Copy, do not abstract.**
`docs/reusability.md`'s rule is that you extract after the second implementation forces
the seam, not in anticipation of it — and the seam is not visible until this thing boots.
Revisit extraction at W6, with two working ports to diff.

What has to change, in rough order of certainty:

- `PPC_IMAGE_*`/entry point and every `CaseZero` path → `CaseWest`.
- **The env-var prefix → `CW_*`. DECIDED by the operator 2026-08-15, not an open item.**
  Case Zero's instruments are all `CZ_*` (`CZ_VKDRAW`, `CZ_SHADER_DUMP`, `CZ_AUTOCHUCK`,
  ~100 of them, documented in its `docs/instruments.md`); they are renamed mechanically at
  transplant time. The reason is that the operator runs both ports on the same machine and
  an exported `CZ_VKDRAW` reaching the wrong binary is a debugging session lost to a cause
  nobody would look for. The accepted cost is that every instrument reference in Case
  Zero's docs reads one letter off here.

  Three things this touches beyond `sed`, and all three are the kind of thing a bulk
  rename misses silently:

  1. **`CZ_` appears inside string literals**, not only in identifiers — `getenv("CZ_…")`
     is the whole mechanism. A rename that only catches identifiers changes the code and
     not the behaviour, and the arm then reads as "no effect" rather than as "not wired".
  2. **The binary and the C++ symbol prefix change too** — `cz_runtime` → `cw_runtime`,
     `CZ_TIMEBASE_HZ`, `CZ_UNIMPLEMENTED_IMPORT`, `CZ_HAVE_SDL`. `tools/gen_import_stubs.py`
     already emits `CW_UNIMPLEMENTED_IMPORT` in this repo.
  3. **Every arm must be shown to engage.** Gotcha 151: an arm with no counter cannot be
     shown to have engaged, and a renamed-but-unread variable is exactly that failure. The
     gate for this step is not "it compiles" — it is that a known arm still visibly changes
     what it always changed. Pick one with an obvious effect and check both directions.
- `kernel/imports.cpp` — the 244 shared imports transfer; add the **three new ones**.
  Round 1 measured what they are (findings 3 and 6), which changes this item:
  - **`NtCreateMutant` / `NtReleaseMutant` are a HOT SOLO-GAMEPLAY LOCK, not co-op.**
    6 created, **32,382 released** in a 5–10 minute solo session — per-frame or
    per-resource, volume pointing at audio/XMA or the streaming loader. Real primitives,
    properly implemented; a mutant that fakes acquisition is gotcha 5 with a deadlock at
    the end, on the hot path, where it will be blamed on anything else.
    *(This retracts the plan's earlier guess that they were co-op. A1's zero calls at boot
    was evidence that boot does not use them — not an attribution to another path.)*
  - **`DbgBreakPoint` is never called** (0 in both captures). Safe to stub.
  - **And the shared 244 are not all driven as Case Zero drives them.** A solo boot calls
    `NetDll_WSAStartup` ×2, `NetDll_XNetStartup` ×1 and `NetDll_XNetGetTitleXnAddr`
    **×405** — a poll, not an initialisation. Winsock and the XNet title-address query are
    on the single-player critical path, so "multiplayer is out of scope" cannot be
    implemented as "make every NetDll call fail" (finding 6).
- `kernel/content.{h,cpp}` — the save layer. Its header comment derives the whole XAM
  enumerate protocol from the title's own statically-linked `XamEnumerate`; that
  derivation should transfer, but **re-run it against this image** rather than trusting
  it, because the save *content* differs (different title ID, different save shape).
- `kernel/xex_imports.cpp` — 247 IAT slots now, and the kernel-variable count needs
  re-reading from this XEX rather than inheriting Case Zero's 13.
- `cpu/guest_thread.cpp` — the TLS slot count and stack size come from **this** XEX's
  header, not Case Zero's. Read them; do not copy the constants.

**Gate:** `cw_runtime --smoke` links. **PASSED 2026-08-15** — 230 TUs compiled with zero
errors, 58,695 symbols resolved, and a first boot ran 60 s without crashing, opened 87
`.big` archives through the VFS and delivered 3,720 vblanks to the guest's own callback.
Full record: **`docs/w1-transplant-notes.md`**.

The surprise was not link scale: it was that four modules carry 156 hardcoded Case Zero
guest addresses, of which **one exists in this image too** and would have linked silently
to an unrelated function. They are parked in `runtime/port-pending/`.

**Still open in W1:** `kernel/content.cpp`'s save layer, which needs capture A3.

## W2 — Xenia ground truth (operator-dependent, START IT EARLY)

**This is the long pole and it is not on the critical path of W0/W1, so request it
now.** Every port in this workspace has been carried by these captures, and the renderer
literally cannot start without the shader microcode dump. Captures run on Windows and
only the operator can produce them (memory: `xenia-captures-run-on-windows`).

> **WRITTEN AND READY: `docs/xenia-capture-requests.md` (round 1, 2026-08-15).** It is
> deliberately shorter than Case Zero's round 1 because that port closed several of the
> questions a first round exists to ask — its §0 lists what is NOT being re-captured and
> why. It adds a Bink section (§W) that has no Case Zero equivalent, three pre-registered
> questions (§X), and asks for A1 alone first. The index captures land in is
> `Xenia logs/Xenia_Run_Content.md`, which is tracked.

The summary of that request, for reference:

| id | what | why it is the authority |
|---|---|---|
| **A1** | boot → title, L3 log — **delivered alone, first** | the kernel-call order `imports.cpp` is written against, the licence-mask check, and the first shader bank |
| **A2** | gameplay, solo | the shaders that reach a rendered world (A1's are a subset), plus streaming, threads and the XMA lifecycle |
| **A3** | save round trip **+ the physical save file** | the save shape; the title ID and layout differ from Case Zero even though the protocol should transfer |
| **A4** | long title-screen idle | makes the per-frame steady state legible against A1's noisy boot |
| **A5** | A1's drive at high frequency, `flush_log=false` | the only view of the synchronisation surface — and therefore of `NtCreateMutant`/`NtReleaseMutant`, two of this title's three new imports |
| **B1** | GPU `.xtr` boot → title (+ same-run L3 log) | the PM4 stream ground truth |
| **B1b** | B1 again, unchanged | the determinism control. Without it there is no way to tell a real defect from run-to-run noise, and a sibling port spent time treating noise as signal |
| **B2** | GPU `.xtr` gameplay | ditto, for a real scene. Watch the 2 GiB cliff |
| **B4** | place-anchored **single-frame** F4 traces + frame-locked PNGs | Safehouse, SecureLab, StoragePens (the crowd worst case), LivingResearch, glass/monitors. Self-contained: each holds the actual bytes of every texture the frame sampled |
| **C1/C2** | `--trace_function_data` coverage, boot and gameplay | recovers guest functions reached only through vtables — no `bl` points at them and they carry no `.pdata`, so `PPC_CALL_INDIRECT_FUNC` dies at runtime without this. **Pays off before any runtime exists** |
| **D** | `dump_shaders` | not a separate run — a flag on every run above. The renderer's only input |
| **W1/W2** | one frame with a **Bink logo on screen**; one with an **in-world monitor** | the output-surface seam, which is where the Bink work actually is (W3 below) |
| **E** | screenshots at known points | the only evidence channel for "does it look right" |

**Two traps stated in the request itself**, both learned the hard way in Case Zero:

- **The trial trap.** `license_mask` defaults to 0, so Xenia boots the *trial*, whose
  boot differs measurably. Every run must be the full game (`license_mask = 1`).
- **A capture of the Bink boot logo is asked for specifically** (§W1, and W3 here),
  because it is the first thing the title does and nothing else in the set exercises it.

Priority if the list is too much for one sitting: A1 alone first, then the one Bink
frame, then B1+B1b, then C1.

## W3 — Bink: ~~the one genuinely new subsystem~~ **SOLVED, and it needs no host code**

> **CLOSED 2026-08-15 by capture W1 (finding 17).** Everything below is kept as the record
> of how it was reasoned about; the answer is this box.
>
> The intro frame decodes as a 4-vertex fullscreen quad binding **three linear `k_8`
> planes** — Y 1280×720 (pitch 1280) and U/V 640×360 (pitch **768**, padded) — plus the
> title's usual tone-map and colour-LUT textures, converted to RGB by **the guest's own
> pixel shader** `ps_a9f83f703af104b5` (144 bytes: three `tfetch2D` and a `mad` against
> constant coefficients). That shader is already in our cache.
>
> So the whole stack is already ours: container read = the VFS (from **inside the STFS
> package**), decode = recompiled guest code (finding 14, 137 `BINK` functions execute),
> output = ordinary linear textures, conversion = a guest shader we translate, composite =
> the normal post chain. **There is no movie player to write and no ffmpeg fallback.**
>
> **The one real requirement: honour `tiled=0`.** These planes are linear; detiling them
> produces a scrambled block pattern (which is exactly what this analysis produced on its
> first, wrong attempt). And read chroma at its **padded 768 pitch**, not at 640, or it
> shears.
>
> Confirmed five ways — 4:2:0 dimensions, the shader's colour matrix, a luma histogram
> piling up on **16** (studio-swing black), a dump of exactly 1280×720×1 bytes, and the
> plane rendering **pixel-for-pixel identical to the on-screen frame**. The text and
> typewriter cursor are baked into the video, not drawn live.

## W3 (historical) — Bink: the one genuinely new subsystem

The evidence is in bootstrap §6 and it is not a string inference this time: four Bink
sections (one executable), ten `.bik` files totalling 66 MB, a
`binkmovieplayer.cpp` wrapper with live diagnostics, and runtime path construction
naming in-world screens beyond the ten shipped files. ~~**`dr2_logo.bik` is on the boot path**, so this cannot be deferred to the end the way a
cutscene could.~~ **RETRACTED by finding 4 — there is no boot Bink.** A1 opens zero `.bik`
between boot and title; the boot logos are static images from
`data/frontend/ratinglogos.big` / `startup.tex`. Bink first plays at the **New Game
intro** (`800a_intro.bik`) and on **in-world monitor screens** (`807_monitors.bik`,
reached in the opening area), streamed **out of the STFS package** via
`StfsContainerDevice::ResolvePath(\data\movies)` rather than from the loose files — so
the VFS must serve them from the container. No dedicated movie thread is created, which is
consistent with the recompiled-decoder hypothesis below.

**This lowers W3's urgency but not its size**: it is off the boot path, so a first picture
(W4) can be reached without it — but it is the first cinematic in the game, so it is
between the title screen and all gameplay.

**~~The lead worth testing first~~ — CONFIRMED by C2 (finding 14), and it nearly halves
this item: the decoder IS already recompiled and it RUNS.** `BINK` is a
`SectionFlags_Code` section, XenonRecomp translated it along with everything else,
`PPC_CODE_SIZE` covers it — and on hardware **137 distinct functions inside that section
execute** when the New Game intro plays. C1, whose drive stops at the title, ran **zero**
of them, and playing the intro is the only difference between the two drives.

So the decode is guest code we already generate, and W3 is **"wire up the I/O and the
output surface"** rather than "write a Bink decoder".

**What this does NOT establish**, so it is not over-read later: that our *translation* of
those 137 functions is correct, or that the output surface works. Only that the decoder is
not a hole we have to fill. Two of the 137 are reached indirectly and needed entry-point
recovery from C2 (`0x829D3810`, `0x829D38F0`) — without that capture they would have been
indirect-call misses the first time a movie played.

The two strings that say where the host boundary is:
`ERROR!! You are passing a write combined memory address to Bink!` and
`cached memory for the Bink texture pointers - see BinkTextures.cpp` — both are about
GPU-visible memory, i.e. the seam is at the *output surface*, not the decode.

Order of work:
1. `tools/gdis.py` over the **137 executed** `BINK` functions + the wrapper: find the seam,
   i.e. which calls leave the recompiled code. C2's trace supplies that executed set, which
   is a far better starting list than the whole section.
2. Decide the strategy from what that shows. The fallback now looks unlikely to be needed,
   but it stays cheap if it is — the runtime already links ffmpeg for XMA, and ffmpeg
   decodes Bink video and both Bink audio variants.
3. **Fail honestly in the meantime.** A movie player that silently reports success will
   hang the boot on a frame that never arrives. Case Zero's own diagnostic exists in the
   guest (`*** BINK MOVIE waiting for heap to free up: %3.2f secs have passed...`) and
   will tell you which way it went.

**Do not apply Case Zero's finding 7 here.** That port retracted "Bink video" after
measuring that no decoder ran and no `.bik` shipped. Both halves of that measurement come
out the other way in Case West, and the retraction's own stated lesson — a middleware
name proves a name, not a codec — cuts against reusing its conclusion, not for it.

## W4 — first picture

With W1 linked and W2's A1/A2 shader dumps in hand, this is Case Zero's phases 3–5
compressed, because the code already exists and works:

1. Window, input, present seam (`host/window.cpp`) — transplanted.
2. PM4 command processor (`gpu/pm4.cpp`) — transplanted. **Do not delete it**; it is the
   boot engine and the same-binary control arm for every claim the D3D arm makes.
3. Build the SPIR-V shader cache from this title's microcode:
   `tools/xenia_ucode_to_cache.py` → `tools/build_shader_spv.sh`. Case Zero's bank grew
   from 335 to 435 across the whole port and grew on **every** session that reached new
   ground; expect the same here and carry `CW_SHADER_DUMP` on every run that goes
   somewhere new.
4. The renderer (`gpu/vk_renderer.cpp`, 10,310 lines) — transplanted.

**This is the step where the transplant thesis gets tested for real**, and the honest
expectation is that it mostly works and then breaks somewhere specific. Budget for
"mostly works, then a picture defect hunt" rather than for either extreme.

**One thing to publish differently from Case Zero, decided there and stated for here:**
publish **no texcoord swap mask** — trust the microcode's own destination swizzles. Case
Zero's `g_SwappedTexcoords` was compensating a second time for a transposition the fetch
pipe already handles, and it was the whole striped-material defect class (its parts 37
and 45). Start from the corrected position.

## W5 — audio

`kernel/audio.cpp` + `audio/{xma_decoder,audio_out}.cpp` transplanted. The XMA path is
the first non-graphical thing `reusability.md` rates as proven in both ports.

**Carry the one warning that cost Case Zero 28 parts** (gotcha 267): the XMA context's
three buffer pointers are **physical** addresses, because the APU is a DMA device. In a
flat recompiler map those are not the addresses the CPU uses, and reading them literally
yields a page of zeros that decodes to **silence** — indistinguishable from the "no
audio" symptom you are there to fix. Case Zero's window is `virt = 0xA0000000 | phys`;
verify the base on this title rather than assuming it.

Cinematics in Case Zero were in-engine scripted scenes (29 `.txt` scripts naming camera
and actor animations), not video, and Case West ships the same `data/cinematics/`
(`cinematics.big` + `permanent.big`) and `data/anim/cinematic/` — 22 animation
archives, named `800_main_menu` through `825_epi_teaser`. So the
cinematic system transfers; the Bink files are a **separate** path (logos and in-world
screens), and the two should not be conflated.

## W6 — the extraction question, deliberately here and not earlier

With two working ports there is finally evidence for what the shared seam actually is,
and `docs/reusability.md`'s tier list can be applied for real rather than predicted.
Revisit it then — tier 1 (hardware decode, XMA, STFS/XEX loading) is the candidate set,
and the rule stays "static-link into each port, no shared runtime DLL".

Nothing about this is urgent and doing it earlier is explicitly a mistake.

## W7 — multiplayer (OUT OF SCOPE, and why that is safe)

Deferred by the operator. The measurement that makes deferral low-risk: **Case West's
import table is a superset of Case Zero's by exactly three names, none of them
networking.** Whatever session/XNet surface co-op uses was already in the 244 Case Zero
implemented, so a co-op subsystem cannot introduce an unimplemented *import* — only
unexercised code paths behind ones that already exist.

What that does not cover, and should be watched for: the single-player boot touching
co-op initialisation on its way past. If a hang lands somewhere with `Coop` in the name,
that is this item arriving early.

## W8 — this title's own content

The tail: Case West's zones (`Phenotrans`, `SecureLab`, `StoragePens`,
`LivingResearch`, `Safehouse`), its UI, its save shape, its achievements
(`GAME1.spa`, 76 KB), and the two asset directories Case Zero does not have:
`data/controls/padmap.txt` (a pad mapping in plain text — read it before writing any
input glue, it may specify the thing `host/window.cpp` currently hardcodes) and
`data/dynamicprops/special/`.

**And the navigation toolchain, which is worth building early rather than late.** The
retail image carries a debug build again — `DebugMenu`, `COMMAND_RENDERDEBUGMENU`,
`COMMAND_AIDEBUGMENU`, `DontAutoCompleteOnDebugJump`. Case Zero's DebugJump/AutoChuck
instruments were what made the whole back half of that port measurable, because they let
a headless run reach real gameplay instead of stopping at the title screen. Finding the
equivalent switches here pays for itself immediately.

---

## Ordering, and what can run in parallel

```
W2 (capture request)  ── operator, weeks of latency ──────────────┐
                                                                  ▼
W0 (recompiler gates) ─► W1 (transplant, link gate) ─► W4 (picture) ─► W5 (audio) ─► W8
                              │                                              ▲
                              └─► W3 (Bink: gdis first, cheap) ──────────────┘
                                                          W6 (extraction) after W4/W5
```

**Send the capture request first.** It has the longest latency, it is the only item
nobody here can unblock, and W4 cannot start without the shader dumps it carries.

## What would refute the transplant thesis

Stated in advance, so it is a prediction and not a story told afterwards:

- The link gate (W1) failing for reasons other than scale.
- A boot that diverges from Case Zero's kernel-call order early — which would mean the
  same import *surface* is being driven by a differently-ordered engine, and
  `imports.cpp`'s call-order structure is worth less than it looks.
- A shader bank whose intersection with Case Zero's is small. The banks are per-title
  microcode and there is no reason to expect overlap, but a *large* overlap would be a
  strong second confirmation and is free to measure.

The first two are cheap to check and both land in W1. Check them there rather than
discovering them in W4.
