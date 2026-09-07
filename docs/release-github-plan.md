# The GitHub release — plan and execution record

**Operator instruction (2026-09-05):** *"Let's do the release — look at how Case
Zero did it. We did a launcher and all where you could also easily extract the
game."* This plan is Case Zero's `release-github-plan.md` road, re-walked for
Case West — and most of that road was ALREADY HERE, because part 8's import
carried the release runtime wholesale: the launcher (`CW_LAUNCHER`,
`Host_RunLauncher`), the in-process STFS extract, the first-run refusals,
exe-anchored paths, in-process shader translation + the disc prebuild, the
first-run progress window, `cw_defaults.env`, and saves in the OS location.

What was genuinely missing, and what this session built, is below. Case Zero's
plan file remains the methodology reference; this file records only the deltas.

## §0 The overlay gap — the one real code item (DONE)

Same blocker as Case Zero's §0, HALF the size: this port has **no game_patched
layer at all** (the Visuals panel is a code hook — `cpu/pc_options_cw.cpp`, "no
repacked asset"; the frontend banks are read loose). Only `assets/game_kbm`
carries Capcom-derived bytes a bundle cannot ship: the 26 key-cap prompt
glyphs in fecmn.tex, the size-pinned str_en.bcs (PRESS ENTER / A÷D KEYS /
MASH), part 9's camera-bar retarget in ingame.big, and glyph_swap.bin.

Shipped as `runtime/host/overlay_gen.{h,cpp}` — Case Zero's §0 port minus
their patched layer, plus our camera patch and pinned-bank variant — running
at first run and as `cw_runtime --gen-overlays`. Chips ship pre-baked
(`gen_kbm_icons.py --export-chips` → `tools/release/kbm_chips/*.dxt`,
committed, our art only). **Gate, run both ways**: all 4 outputs byte-identical
to the Python reference; a poisoned chip byte IS detected (gotcha 30). The
Python tool carries the reference contract in its docstring, and its inherited
preload4-eviction claim is retracted in place (a Case Zero fact that does not
transfer).

## §0b Found on the way: the shared pipeline-cache directory

The pipeline cache/key file is keyed on the shader-cache DIRECTORY name
(`shader_spv` in both ports) under the inherited `cz-recomp` base dir — so
both titles on one machine silently shared one cache and one pre-warm key
file, and Case Zero's release-day gate runs had overwritten every CW key
(only 9 of the file's 406 keys resolved in our shader cache). Fixed: the base
is `cw-recomp`; the dev cache was migrated; the shipped seed
(`tools/release/prewarm.keys`, 134 keys, 0 unresolvable) was harvested fresh
through `cw_hud_capture.sh`. **The operator's test sitting should re-harvest a
richer seed from real play, exactly Case Zero part 85's road** (their per-user
key file after a sitting → `tools/release/prewarm.keys`, repackage).

## §1 Operator decisions

Inherited from Case Zero's answered set unless they say otherwise: v1.0.0,
attach both artifacts, MSAA 2x stays default, glibc floor is a stated known
limitation. **None of these block the build work; all can be revisited at the
test sitting.**

## §2-§3 Build, package, gate (Linux DONE; Windows this session)

* **CMake**: the Release build type (-O2 -g then split; `CW_SPLIT_DEBUG`),
  `$ORIGIN/lib` RPATH (`CW_BUNDLE_RPATH`), `CW_SDL2_PREFIX` — imported.
* **Deps**: `tools/build_sdl2.sh` + `build_ffmpeg_lgpl.sh` (Case Zero's,
  work-dirs renamed), both built into `thirdparty/`. NOTE: no nasm on this
  machine, so the Linux ffmpeg ships without hand-written x86 asm — the same
  stated limitation Case Zero shipped with.
* **Gates run on Linux, 2026-09-05**:
  - `release_text_identity.sh`: **.text 36,556,402 bytes, byte-identical**
    between matched RelWithDebInfo and Release configures.
  - `release_package_linux.sh`: `dist/CaseWestRecomp-linux-x86_64.tar.zst`,
    26 MB. REBUILT TWICE: once after the czwin POSIX fixes, and again on
    2026-09-06 to carry the parts 98-101 fix round (imported-fixes §6) —
    final sha256 `4d3e5005…`, `.text` 36,577,074 bytes identical between
    matched configures, container gate PASSED on the final bundle.
  - `release_gate_clean_container.sh`: **GATE PASSED** — bundle self-resolves,
    --smoke in-container, DXC dlopen translates, and the whole first-run flow:
    in-process extract (305 files, 1,216,219,768 bytes), disc prebuild
    (**1,322 pixel shaders, 0 refused, 0 failed** — this bank is bigger than
    Case Zero's 1,265), overlay generation byte-identical to the Python
    reference, boot referencing 261 .big archives, honest refusal with no game.
* **Windows** (czwin, the shared laptop): `C:\cw\<repo>` cloned; deps, vc.bat,
  XenonRecomp/XenosRecomp reused from `C:\cz` (title-agnostic on purpose);
  default.xex scp'd (10,825,728 bytes); **ppc/ regenerated on Windows — 233
  files, zero errors** (the recompilation is fully portable, re-confirmed);
  the one predicted MSVC friction was the SAME line as Case Zero's — `memmem`
  in the glyph scan — fixed with their portable FindBytes on both platforms
  BEFORE the first build attempt. `release_package_windows.ps1` ported
  (58410B00 texts, shared C:\cz deps).

## §4 The public front page (DONE)

Root `README.md` rewritten player-first (single-player scope stated up front;
co-op is online-only on the 360 and out of scope here); the day-1 dev README
is preserved verbatim at `docs/dev-readme-day1.md`. CI imported
(`.github/workflows/build.yml` + `tools/ci/` patches — byte-identical to Case
Zero's, same pinned bases, one local patch set serves both ports).

## §5b OPERATOR-ACCEPTED AND STAGED (2026-09-06)

Their verdict after playing the artifacts: **"Tested and pretty good."** That
answers the §3.1/§3.2 sittings as a formal gate — as Case Zero's "we really got
all the fix" did there.

**The release is staged for upload at `~/Release/Case West/1.0.0/`**, matching
their Case Zero 1.0.1 layout:

```
CaseWestRecomp-linux-x86_64.tar.zst    4d3e5005…   (54 entries, tar-verified)
CaseWestRecomp-windows-x86_64.zip      9b907718…   (43 entries, unzip -t clean)
SHA256SUMS                                          (sha256sum -c: both OK)
release-notes-v1.0.0.md                             (paste-ready below its ---)
```

**The tag `v1.0.0` is created and pushed** at commit `9ef2ef1`. What remains is
the operator's own three clicks: Releases → Draft a new release → choose the
existing tag v1.0.0, title "Dead Rising 2: Case West — Native PC Port v1.0.0",
paste the notes body, attach the two artifacts, publish; then Settings →
Change visibility → Public (CI green at the release commit first).

## §5 Publish — what remains, in order

0. **DONE 2026-09-06: the parts 98-101 fix round** (operator: *"Case Zero did a
   bunch of important fixes we should add to v1.0.0"*). Eight fixes, one commit
   each, each gated; both artifacts rebuilt and re-gated on top. Full evidence
   in `docs/imported-fixes.md` §6.
1. ~~**Windows package** off the czwin build~~ — DONE (twice).
2. **The operator's test sittings, both platforms** (the §9.8 lesson: a bundle
   save round-trip and a KB/M-from-the-bundle sitting are the two owed
   verifications; the first-run flow from a bare bundle + their package is the
   third). Re-harvest the prewarm seed from the sitting and repackage.
3. Tag `v1.0.0` at the verified commit; create the GitHub Release; paste
   `docs/release-notes-v1.0.0.md` (below its `---`); attach both artifacts.
4. **Operator flips the repo public** (their account, their click; confirm CI
   green at the release commit first).

## Known residue (stated, not smoothed)

* The Linux ffmpeg has no x86 asm (no nasm here); czwin's has it via MSYS2.
* glibc floor 2.43 on the Linux artifact; AppImage later.
* The prewarm seed is a 134-key HUD-route harvest until the operator sitting
  refreshes it (session one seeds ~0 regardless — vertex shaders are born at
  first sight; Case Zero measured the same).
* macOS: milestone C, hardware-blocked, unchanged.
