# Bootstrap findings — 2026-08-15 (session 1)

Day 1 of the Case West port. Everything below was **measured on this title's own image**,
not inherited from Case Zero — which matters precisely because the working assumption of
this port is that Case Zero transfers. An assumption that is never checked is a claim
with no evidence behind it, and two of the four tools reused today were silently wrong on
this title (§7).

Sibling port: `~/GithubRepo/Dead_Rising_2_Case_Zero_Xenon_Recomp`, ~46 parts deep and
close to complete. Its `docs/` are the methodology and are referenced rather than
duplicated, except `gotchas.md` and `reusability.md`, which are copied here (§8).

---

## 1. The container: STFS, same as Case Zero

The package arrives as a single 1.22 GB blob with a hash-shaped filename under
`<TitleID>/000D0000/`, exactly Case Zero's shape. `tools/extract_stfs.py` (copied
verbatim; not one line changed) opened it on the first attempt:

```
package     : D01128ABB9C7F9694DAE26AC591A269F8480E85A58 (LIVE)
display name: 'DEADRISING2:CASE WEST'
content type: 0x000d0000 (Arcade Title)
title id    : 58410B00        media id: 198BA306
version     : 3 (base 3)      disc 0
volume      : STFS
content size: 1,224,118,272 bytes
files       : 305
```

`assets/game/` now holds all 305 files, 1.2 GB. Case Zero was 256 files / 825 MB, so
Case West is roughly 50% more content in the same container format.

**The STFS reader is the first thing proven in BOTH ports.** Under `docs/reusability.md`'s
own rule that is the bar for extraction — noted, not acted on.

## 2. The XEX: devkit key + LZX again, and a second code section

`tools/xex_image_dump` (rebuilt against the local patched XenonRecomp) loaded it
directly. The devkit-AES-key patch Case Zero added to XenonRecomp was required here too
and worked without change.

```
image base : 0x82000000
image size : 0xB40000 (11796480 bytes)     <-- identical to Case Zero
entry point: 0x825AC918                    <-- Case Zero: 0x825D9F30
sections   : 14                            <-- Case Zero: 9
```

| section | base | size | flags |
|---|---|---|---|
| `.rdata` | 0x82000600 | 0x001127C4 | data |
| `.pdata` | 0x82112E00 | 0x00034200 | data |
| `BINKCONS )` | 0x82147000 | 0x00002920 | data |
| **`.text`** | **0x82150000** | **0x0087BC54** | **code** |
| **`BINK`** | **0x829CBE00** | **0x000106B8** | **code** |
| `BINKBSS` | 0x829E0000 | 0x000043A0 | data |
| `.data` | 0x829E4400 | 0x00122C18 | data |
| `.XBMOVIE` | 0x82B07200 | 0x00000008 | data |
| `.tls` | 0x82B07400 | 0x00000015 | data |
| `BINKDATAT=` | 0x82B07600 | 0x00003D54 | data |
| `.XEXID` | 0x82B0B400 | 0x00000004 | data |
| `.idata` | 0x82B10000 | 0x00000486 | data |
| `.XBLD` | 0x82B20000 | 0x00000100 | data |
| `.reloc` | 0x82B20200 | 0x000B758C | data |

`.text` is 0x87BC54 against Case Zero's 0x873564 — 2% larger, i.e. the same engine with
a little more of it.

**The finding that changes tooling: there are TWO code sections.** Case Zero has one.
XenonRecomp itself is fine — `recompiler.cpp` iterates every section carrying
`SectionFlags_Code` and the emitted `PPC_CODE_SIZE` is `0x88C4B8`, which spans `.text`
through the end of `BINK`. Every *analysis* tool that hardcoded one range is not; see §7.

## 3. Register save/restore helpers

`tools/find_save_restore.py` found all eight in one pass, no changes to the tool:

```
savegprlr_14  0x8282E9F0   restgprlr_14  0x8282EA40    (r1-based)
savefpr_14    0x8282EB20   restfpr_14    0x8282EB6C    (r12-based)
savevmx_14    0x82833730   savevmx_64    0x828337C4
restvmx_14    0x828339C8   restvmx_64    0x82833A5C
```

The four vector ladders are contiguous — `0x82833730 + 0x94 = 0x828337C4`,
`+ 0x204 = 0x828339C8`, `+ 0x94 = 0x82833A5C` — which is the cross-check that they are
whole ladders and not an 18-rung suffix of a 64-rung one. Same r1/r12 base split as Case
Zero, i.e. the same compiler and the same CRT.

## 4. Jump tables: 205 across two code sections

XenonAnalyse was not run. Case Zero's §4 established it finds zero on this compiler's
scheduling, and there is no reason to spend the run to watch it fail again — but see
gotcha 3 before ever *accepting* a zero from it.

`tools/find_jumptables.py`, after the fix in §7:

```
# scanned 82150000..829CBC54        (.text)
# scanned 829CBE00..829DC4B8        (BINK)
absolute jump tables      : 107
offset16 jump tables      :  36
offset8  jump tables      :  62
total tables              : 205
total case labels         : 5791
unhandled                 :   0
```

Case Zero had 232 tables / 6,114 labels, so the mix is comparable. **One of the 107
absolute tables (10 labels) is inside `BINK`** and no `.text`-only scan would ever see
it. Written to `config/CaseWest_switch_tables.toml`.

## 5. First recompilation pass

`XenonRecomp CaseWest.toml`, config with an empty `functions` list:

```
PPC_IMAGE_BASE 0x82000000    PPC_IMAGE_SIZE 0xB40000
PPC_CODE_BASE  0x82150000    PPC_CODE_SIZE  0x88C4B8
```

**58,629 functions, 228 `ppc_recomp.N.cpp` translation units, 156 MB.**
(Case Zero: 57,728 / 227 / 154 MB.)

### Switch-tail repairs — 21 functions, closed in one round

The first pass reported **367** `Switch case at X is trying to jump outside function`
errors. `tools/fix_switch_function_bounds.py --apply` computed size overrides for **21
distinct functions**, and the second pass reports **zero**. Fixpoint in one round, as in
Case Zero. Function count settles at **58,351** — lower than the first pass because the
widened functions absorb the fragments the analyzer had split off.

Case Zero needed 28 functions for 517 errors, so this is the same defect at slightly
smaller scale.

### Dropped direct branches — 8 then 3 functions, driven to zero

`tools/find_dropped_branches.py` reported **18 dropped branches across 8 truncated
functions**, and **zero** backward/split-function cases. `--widen` plus a regeneration
round left 3, another round left zero, and two further rounds confirmed the fixpoint.

Worth stating because the tool's own output asks for it (gotcha 30, *a test that has
never failed has not been shown capable of failing*): **this check demonstrably fails on
this image** — it printed 18, then 3, then 0 across successive rounds, so the final zero
is a result and not a check that cannot fire.

### Unlowered switches — the gate passes, 0 defects

`tools/find_unlowered_switches.py`, after the section-map fix:

```
unlowered dispatches found in ppc/  : 850
bctr sites in .text                 : 1055
lowered to a switch (TOML)          : 205
switch-shaped but NOT lowered       : 2
  of those, DEFECTS (save ladder)   : 0
  of those, benign frameless thunks : 2
```

Exit 0. This is the gate for the class that leaks a callee's non-volatiles into its
caller (gotchas 53–55), and it must be re-run after every config change.

### Final baseline

**58,345 functions, 33 function overrides** (21 switch-tail repairs + 12 truncated-function
widenings), zero `jump outside function`, zero dropped branches, unlowered-switch gate
clean.

### Unrecognized instructions — 39 sites, 6 mnemonics (open work item)

```
14 vminsw     13 vpkshss     8 vavgsw     2 stdux     1 vpkshss128     1 stvebx
```

plus **20 `Unexpected float16_4 pack instruction` sites**, all in one tight cluster
around `0x825E6904–0x825E7490` — a distinct diagnostic from the above and a category
Case Zero never hit. It is one function's worth of addresses, so it is very likely a
single half-float conversion routine. (The 39 unrecognized-instruction sites are a
separate, wider spread: `0x825D7784`–`0x825EDB18`.)

**None of Case Zero's six mnemonics appear here.** Its list was
`lhbrx / stfsux / vsubuws / vspltish / vpkuwum / vadduhs`, all of which were implemented
upstream in the local XenonRecomp during that port, and Case West inherits those fixes
for free. What is left is a fresh, disjoint set of six — mostly VMX signed-integer
min/pack/average. For scale, Asura's Wrath's first pass had 3,192 sites across 32
mnemonics; 39 is tractable and should be closed before any runtime work, because an
unimplemented instruction is a silent wrong-execution trap and not a build failure.

### **The kernel imports are a STRICT SUPERSET of Case Zero's, +3**

This is the most decisive measurement of the day, and it is a two-line diff of the
generated `ppc/ppc_recomp_shared.h` from each port:

```
Case West : 247 imports          Case Zero : 244 imports
only in Case West : DbgBreakPoint, NtCreateMutant, NtReleaseMutant
only in Case Zero : (none)
```

**Every kernel and XAM import Case Zero implements, Case West also needs — and Case West
needs exactly three more.** `runtime/kernel/imports.cpp` (4,668 lines, written in the
order Case Zero's A1 capture called them) is therefore portable essentially whole, and
the new work is three names, two of which are a mutant pair.

> **PARTIALLY SUPERSEDED, 2026-08-15 (capture findings 3 and 6).** The census stands —
> 247 against 244, superset, +3 — but two readings taken from it were wrong. The three new
> names are **not** co-op: `NtReleaseMutant` is called 32,382 times in a *solo* gameplay
> session and must be a real primitive (finding 3). And a superset import table does not
> mean the shared 244 are driven the same way — a solo boot calls
> `NetDll_XNetGetTitleXnAddr` 405 times (finding 6).

Two cautions on what this measurement does and does not say. It is a census of *names in
the import table*, so it says the surface matches; it says nothing about whether a given
import is called with the same arguments, in the same order, or at the same time — and
gotcha 5's rule (a stub must fail honestly, and one with an out-parameter must fill it or
not exist) applies to all 247 regardless of where the implementation came from. And
`DbgBreakPoint` importing is not itself proof of a debug build; see §6 for the strings
that are.

## 6. Game intel

- **Internal project name is `deadrisingepilogue`.** 402 build-path strings read
  `c:\bcg\deadrisingepilogue\...` (and exactly one legacy `c:\bcg\deadrising`). The
  shipped shader banks are named to match:
  `data/shaders/deadrisingepilogue-{vs,ps,vd,pd,sc,sd,ss}.big`, where Case Zero's were
  `deadrisingprologue-*`. Anything keyed on that string in a tool needs retargeting.
- **Same engine, same studio, same asset formats.** `.big` containers (154 of them),
  `.bct` textures, `.bcf` fonts. `tools/big_list.py` — copied over with only a name
  change — reads `data/datafile.big` correctly on the first try, including the
  compressed entries, so **`docs/big-archive-format.md` from Case Zero transfers
  verbatim** and did not need re-deriving.
- **Havok** physics is present (779 `hkp*` strings), and the in-house **CrowdEngine**
  (12 strings) for zombie crowds — both as in Case Zero.
- **Zones**: the image still carries full-DR2 zone names (`americana`, `atlantica`,
  `arena_stadium`) it does not ship, exactly as Case Zero did, plus Case West's own
  (`Phenotrans`, `SecureLab`, `StoragePens`, `LivingResearch`, `Safehouse`). **All of the
  shipped ones are parts of the Phenotrans facility: this game has no outdoors at all**
  (operator, 2026-08-15) — which is why the list reads as interiors and why, unlike Case
  Zero's outdoor Still Creek, there is no exterior area class a capture set could miss.
- **A debug build is present in the retail image again** — `DebugMenu`,
  `COMMAND_RENDERDEBUGMENU`, `COMMAND_AIDEBUGMENU`, `ShowInDebugMenu`,
  `DontAutoCompleteOnDebugJump`. Case Zero's whole navigation toolchain (DebugJump,
  AutoChuck) was built on the equivalent, so this is a large lead worth confirming early.
- **Co-op is real and in scope for the engine, not for us.** `COOP PLAYER <%d>`,
  `HudCoopRevivalTimerWarning`, `OUTFIT_COOP_DEFAULT`, `deadrisingepilogue/coopweapons.php`.
  Case West's headline feature is two-player co-op; **the operator has scoped multiplayer
  OUT for now**. Single-player must not be blocked on it.

### **BINK IS REAL HERE. Case Zero's finding 7 does NOT transfer.**

Case Zero retracted its day-1 "Bink video" inference: that image carried the strings
`Bink_1`/`Bink_2` but no decoder ran and no `.bik` file shipped, and the retraction's
stated lesson was that a middleware *name* in an image proves a name exists, not that a
codec runs. Applying that retraction to Case West would be exactly wrong, and the
evidence is not a string this time:

- **Four Bink sections in the XEX**, one of them executable: `BINK` (0x106B8 of code),
  `BINKCONS )`, `BINKBSS`, `BINKDATAT=`, plus `.XBMOVIE`. That is a statically linked
  decoder, not a dead symbol.
- **Ten `.bik` files ship**, 66 MB of them, in `data/movies/` — including
  `800a_intro.bik` at 33 MB and the `dr2_logo*.bik` boot logos.
- **A player wrapper with its own source path**:
  `c:\bcg\deadrisingepilogue\library\movielib\Common\binkmovieplayer.cpp`, with live
  diagnostics (`*** BINK MOVIE failed to start... early exit!`,
  `ERROR!! You are passing a write combined memory address to Bink!`).
- **Runtime path construction**: `data/movies/%s`, and named files well beyond the ten
  that ship (`factslaptop.bik`, `cine_loungescreen_mov.bik`,
  `securityroomB_monitors_mov.bik`) — in-world screens, so this is not only the intro.

This is the one genuinely new subsystem in the port. See the plan's item **W3**.

> **RETRACTED IN PART, 2026-08-15 (capture finding 4): this section said Bink "lands on
> the boot path (`dr2_logo.bik`)".** It does not. Capture A1 opens **zero** `.bik` files
> between boot and the title screen, and the four frame-locked PNGs show the boot logos
> are static images out of `data/frontend/ratinglogos.big` / `startup.tex`. The inference
> was: the file exists, its name says "logo", therefore it plays at boot — **a filename is
> not a call site.**
>
> **Everything else in this section stands**, and finding 5 confirms the substantive
> claim: Bink really does run, at the New Game intro (`800a_intro.bik`) and on in-world
> monitor screens (`807_monitors.bik`), streamed **out of the STFS package** rather than
> the loose files, with no thread of its own.

## 7. Two inherited tools were silently wrong on this title

Both were found by reading a tool's own output header rather than its summary, and
neither would have produced an error. Recorded because the class matters more than the
two instances: **a tool copied from a sibling port carries that port's constants, and a
constant that is wrong in the safe direction reports a clean run over a smaller image.**

1. **`find_jumptables.py` had Case Zero's `.text` bounds as its `--lo`/`--hi` defaults**
   (`0x82150000..0x829C3564`). Run unchanged here it printed a confident
   `204 tables, unhandled: 0` for a range that stopped 0x86F0 short of this title's
   `.text` *and* omitted `BINK` entirely. The only place the range appears in the output
   at all is the `# scanned ...` header line. Fixed: it now reads the code sections out
   of `default_image.bin.sections` and scans every one. The honest number is 205.
2. **`fix_switch_function_bounds.py` had the same range hardcoded** as its
   `valid_code_addr()` gate, which decides whether a case label is real or a
   data-as-code artifact to be skipped. It would have discarded all ten of `BINK`'s
   labels, silently, while reporting success. Fixed the same way.

And one documentation defect in `xex_image_dump.cpp`, which is how the section flags were
misread in the first place: its sidecar legend said `1=code 2=data 4=bss`, but
`XenonUtils/section.h` defines `SectionFlags_Data = 1` and `SectionFlags_Code = 2` and
nothing else. Case Zero never noticed because it has exactly one code section, `.text`,
which prints `2` — read as "data" by the legend and as "the code section" by every human
looking at it, so the two errors cancelled. On a title with two code sections they stop
cancelling. Corrected in this repo's copy.

## 8. What was copied, and what was only referenced

Copied into this repo (with provenance headers intact, and retargeted where noted):

- `tools/` — 17 scripts. `extract_stfs.py`, `find_save_restore.py`, `xex_image_dump.cpp`,
  `decrypt_xex.py` (kept only for its address-mapping helpers; it cannot read an LZX
  devkit XEX and fails *silently* — see its header), `gdis.py`, `big_list.py`,
  `big_decompress.cpp`, `gen_import_stubs.py`, `import_call_sites.py`,
  `guest_callers.py`, and the four config-maintenance tools
  (`find_jumptables`, `fix_switch_function_bounds`, `find_dropped_branches`,
  `find_unlowered_switches`, `coverage_to_function_overrides`).
- `docs/gotchas.md` — the 315-entry transferable ledger, verbatim. New entries continue
  at 316. It is copied rather than referenced so this repo stays independently
  preservable, which is `reusability.md`'s own rule for code.
- `docs/reusability.md` — the tier list, verbatim.

Referenced, not copied — read them in Case Zero's repo:

- `docs/big-archive-format.md`, `docs/xtr-decoder.md`, `docs/xenonrecomp-upstream-bugs.md`,
  `docs/measurement.md`, `docs/instruments.md`, `docs/d3d-translation-plan.md`, and the
  phase notes. These are records of *that* port's measurements. Copying them would create
  a second copy of numbers that were measured once, which is gotcha 13 with extra steps.
  Lift a section into this repo when this title's own measurement confirms it.

## 9. Immediate next steps

`docs/port-plan.md`.
