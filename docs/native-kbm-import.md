# Native keyboard/mouse — the import and the re-derivation record (part 8)

Case Zero's parts 91-92 built keyboard/mouse support **through the title's own input
layer**: the 360 XEX ships the entire PC input path except the keyboard connect, a
keystroke source, and the mouse. Their `runtime/cpu/native_kbm.cpp` splices key
bindings into port 0's live binding records, feeds SDL key events through the title's
own SetSource, and adds the mouse camera's over-ceiling surplus after the title's own
per-frame source publish. The architecture, the five fix rounds, and the traps are
their `docs/native-kbm-plan.md` / `native-kbm-phaseA.md` — read those first; this file
records only what had to be **re-derived on this image** and how.

## Why re-derivation, not reuse

`runtime/port-pending/README` rule: no Case Zero guest address may be assumed here —
one address existing in both images would link silently to an unrelated function.
Every constant below was derived from THIS title's `default_image.bin`, and
`native_kbm.cpp`'s runtime verify (token table must read KEY_A, command 216 must read
COMMAND_USER_CAM_LEFTRIGHT, SetSource must be a function start) still gates the splice
on every boot.

## The derived addresses (2026-09-03)

| constant | Case Zero | Case West | method |
|---|---|---|---|
| kTokenNames | 0x829F3930 | 0x82A0A860 | pointer-scan for the slot holding &"KEY_A" (entry 1) |
| kTokenValues | 0x829F3AB0 | 0x82A0A9E0 | names+0x180; entry 1 reads VK 0x41 |
| kTokenCats | 0x829F3C30 | 0x82A0AB60 | names+0x300; cats {key=0, pad=1, axis=2, none=3} verified |
| kCmdTable | 0x829DC810 | 0x829EF930 | pointer to &"COMMAND_USER_CAM_LEFTRIGHT" at index 216 |
| kCmdCount | 305 | **316** | walked until a non-COMMAND_ pointer |
| kFnSetSource | 0x828049D8 | 0x827FF3A0 | instruction-shape match (18 instrs, unique) |
| kBindRecords | 0x82AD6CF8 | 0x82AF0EC0 | constant alignment inside the matched bool-query fn |
| kPortMap | 0x82AD65E8 | 0x82AF07B0 | constant alignment inside the matched port-alive fn |
| bool query hook | sub_828053C8 | sub_827FFD48 | shape match, 81 instrs, 1.000 agreement |
| float query hook | sub_82805510 | sub_827FFE90 | shape match, 138 instrs, 1.000 |
| source publish hook | sub_82804AF8 | sub_827FF4C0 | shape match, 24 instrs, 1.000; **0 immediate diffs** (struct offsets identical: records at +0x11D8, effective at +8, 0x30 stride) |
| pad→source conversion hook | sub_828070E0 | sub_82801A90 | the module's only `+0x2498` pad-ring reference, enclosed by a recompiled function start; 0.898 shape agreement over 479 instrs (the function grew with the command registry) |
| (parser, reference) | 0x82803AE0 | 0x827FE480 | shape match, 288 instrs, 1.000 |
| (parse wrapper, reference) | 0x82804248 | 0x827FEBE8 | loose byte match, order-consistent |

**The shape-matching method** (reusable for any sibling-port function): mask every
relative branch and every D-form immediate, keep opcode+register shapes exact, and
compare the sequence until `blr`. Same compiler, same source tree → unique matches at
even 18 instructions. A first attempt that kept load/store displacements exact matched
NOTHING — global displacements differ image to image; wildcard them.

**One trap actually hit:** a loose 24-instruction prefix match placed the conversion at
0x82801E90 — which is not a function start in our recompilation and whose immediates
misaligned. The `+0x2498` pad-ring signature and the ppc/ function map convicted the
real one at 0x82801A90. A match at n=24 with relaxed masking is a candidate, not a
finding.

## What did NOT need changing

* The token vocabulary: **all 95 entries identical, same order** — kbm_default_map.h's
  source tokens resolve unchanged.
* The command registry is identical through index 224 and a superset after (316 vs
  305); the map resolves commands **by name at runtime**, so the growth is invisible.
  All 133 command names in the default map exist here.
* The mode/combiner enums: tables at 0x82A0680C / 0x82A067FC read the same lowercase
  word sets in the same order.
* The engine padmap path here is `data/controls/padmap.txt` (Case Zero:
  `xdata/datafile/tofix/padmap.txt`) — not referenced by the host code, recorded for
  the next person who greps for it.

## First-boot result (2026-09-03)

Image verify OK; the title's own padmap parse settled; **93 key bindings resolved, 40
mouse-side lines folded, 0 bad; 86 of 93 spliced** — the identical count Case Zero
lands. Keyboard drives the game on the first boot of the port.

## Still owed from the Case Zero feature set

* `tools/gen_kbm_icons.py` output — the key-cap prompt art overlay
  (`assets/game_kbm/`) needs this title's own `fecmn.tex`-equivalent bank, layout pin,
  and str_en.bcs markup census before the generator can run. Until then prompts show
  pad glyphs; input is unaffected.
* MOUSE CAMERA defaults OFF (their operator's call so a pad build changes nothing);
  the Visuals panel's MOUSE CAMERA row turns it on, persisted.
