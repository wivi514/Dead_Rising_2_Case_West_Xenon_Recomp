# Part 11 kickoff — the live hand-off

**Written at the end of part 10 (2026-09-05, the RELEASE part). THIS IS THE LIVE
ONE.** `part9-kickoff.md` is superseded. (No part10-kickoff was ever written:
part 9 closed informally with the operator's "Good enough" on the KB/M polish,
and part 10 opened on their instruction — *"Let's do the release, look at how
Case Zero did it."*)

Read this, then `docs/release-github-plan.md` — the release plan AND its
execution record, which is most of what part 10 did.

---

## 1. WHERE THE RELEASE STANDS: both artifacts BUILT AND GATED, publish awaits the operator

* **BOTH REBUILT 2026-09-06 to carry the Case Zero parts 98-101 fix round**
  (eight fixes, `docs/imported-fixes.md` §6 — the semaphore boot hang, the AMD
  depth format, the worker floor, the self-firing firearm, async pipelines,
  the pre-warm union, EXIT GAME, MASH in every language).
* `dist/CaseWestRecomp-linux-x86_64.tar.zst` — 26 MB, sha256 `4d3e5005…`.
  Gated: `.text` identity between matched configures; clean-container GATE
  PASSED **including the whole first-run flow** (in-process extract of the real
  package, 1,322-shader disc prebuild, overlay generation byte-identical to
  the Python reference, boot, honest refusal).
* `dist/CaseWestRecomp-windows-x86_64.zip` — 21 MB, sha256 `9b907718…`. Built
  on czwin (C:\cw tree, sharing C:\cz's toolchain/deps); staged exe passed
  `--smoke`; pulled back and hash-verified on arrival.
* `docs/release-notes-v1.0.0.md` is the paste-ready Release body with both
  hashes. The public `README.md` is live at the root (dev README preserved at
  `docs/dev-readme-day1.md`). CI is in (`.github/workflows/build.yml`) — its
  FIRST live runs trigger on part 10's pushes; check the Actions tab.

## 1a. PART 11 IS NOW: WAIT FOR PLAYER ISSUES (operator, 2026-09-06)

Their words: *"I am done for now awaiting people issue."* The release is staged
and tagged; the next work is REACTIVE — whatever the first issue reports say
once it is public. Nothing in the backlog should be started speculatively.

**When an issue does arrive, the machinery is all in place and warm:**
* the picture-bisection order is `CW_VK_MSAA=1` → `CW_VK_NO_PAR_RECORD=1` →
  `CW_VK_NO_DEFERRED_CLEAR=1` → `CW_NO_KB_PROMPTS=1` (bundle README has it too,
  so a reporter can run the arms themselves);
* new arms from the fix round worth knowing: `CW_NO_SEM_LIMIT=1` (semaphore
  limit), `CW_VK_SYNC_PIPELINE=1` (async pipelines off),
  `CW_VK_DEPTH_FLOAT=1` (force D32F), `CW_WORKERS=N` (worker budget);
* three machines are ready: this box, czwin (`C:\cw`, full toolchain) and
  czamd ([[the-amd-test-machine]] — RX 6600, Windows 10, ready-to-play install);
* a fix round is ~15 minutes of machinery: one commit per change, rebuild both
  artifacts, re-gate (text identity + container on Linux, staged --smoke on
  Windows), refresh the hashes in the notes AND the staged `SHA256SUMS`.

**A RICHER PRE-WARM SEED IS ALREADY COMMITTED AND WAITING.** The operator's
capture session harvested **688 keys** (against the shipped 134), all
resolvable, now in `tools/release/prewarm.keys`. It is deliberately NOT in the
staged v1.0.0 artifacts — those stay exactly as tested — so **the very next
artifact build ships it for free** and first-session stutter drops without
anyone doing anything. Re-harvest again after any long sitting: the per-user
file at `~/.cache/cw-recomp/pipeline_shader_spv.bin.keys` is REWRITTEN each
session, so a good harvest is perishable.

## 1b. THE SITTINGS ARE DONE AND THE RELEASE IS STAGED (2026-09-06)

Operator: **"Tested and pretty good."** Tag `v1.0.0` pushed at `9ef2ef1`;
artifacts + SHA256SUMS + paste-ready notes staged at
`~/Release/Case West/1.0.0/`. **All that remains is their GitHub clicks**
(draft the release on the existing tag, paste the notes, attach the two files,
publish, flip visibility). §2 below is kept for the mechanics and for whatever
a later fix round needs.

## 2. WHAT PART 11 IS: the operator's test sittings, then the three clicks

1. **Windows sitting (zero setup).** The play copy is fully staged ON THE
   LAPTOP: `C:\cw\Dead_Rising_2_Case_West_Xenon_Recomp\dist\CaseWestRecomp\`
   — their package is already in its `assets\package\` (1,224,163,328 bytes,
   verified). Double-click `cw_runtime.exe`: launcher → PLAY → the full
   first-run flow runs under the progress window. Verify: KB/M with key-cap
   prompts, a save round-trip (make one, quit, relaunch, load), MSAA look,
   general play.
2. **Linux sitting.** The staged bundle is `dist/CaseWestRecomp/` (clean
   skeleton). Worth testing the launcher's drag-and-drop with their package —
   the one road no gate exercises. Same checks as Windows.
3. **Re-harvest the prewarm seed from a sitting** (Case Zero part 85's road):
   after real play, `~/.cache/cw-recomp/pipeline_shader_spv.bin.keys` (Linux)
   or `%LOCALAPPDATA%\cw-recomp\…keys` (Windows) → `tools/release/prewarm.keys`
   → repackage both artifacts → refresh both hashes in the notes. The shipped
   seed is a 134-key HUD-route harvest; a play harvest will be much richer.
4. **Publish** (the operator's clicks): tag v1.0.0 at the verified commit,
   Draft a new release, title "Dead Rising 2: Case West — Native PC Port
   v1.0.0", paste the notes body (below its `---`), attach both `dist/`
   artifacts, publish; Settings → Change visibility → Public (confirm CI green
   first — no `gh` here, the checks tab is operator-only).

**If a sitting forces a fix**: one change per commit, rebuild BOTH artifacts,
re-run the gates (text identity + container on Linux; staged --smoke on
Windows), refresh both hashes in the notes. Case Zero went through four such
rounds on release day; the machinery makes each ~15 minutes.

## 2a. THE AMD MACHINE IS SET UP AND THE RELEASE RUNS THERE (2026-09-06)

**czamd** (`192.168.0.60`, Windows 10 Pro, Radeon RX 6600, user `lisab`) now
carries a ready-to-play install at **`C:\Users\lisab\cw\CaseWestRecomp\`** —
the gated v1.0.0 zip, unpacked, with the operator's own package already
extracted and all 1,322 shaders built. Just run `cw_runtime.exe` there.

It has NO toolchain (no compiler, CMake, git or Vulkan SDK) and nothing was
built on it — a Windows binary is portable and what that machine offers is its
GPU. Installing a toolchain there is a multi-GB change to a machine that is not
the dev box; say so before doing it.

What running there already proved (`docs/imported-fixes.md` §6 addendum): the
whole first-run flow on a machine with no dev tree (the one check the packaging
script says it cannot make), the AMD depth fallback firing on the device's own
answer, the worker floor giving that 6-core CPU 3 workers instead of 1, and
75,770 log lines with zero faults. It also FOUND the false "saving will fail"
message (`a3d8eb3`).

**Still owed there: a windowed, interactive run** — everything above was
headless over SSH. A GUI launch needs the interactive session (Case Zero used a
`schtasks` task for this on czwin).

## 2b. WHAT THE SITTING SHOULD NOW ALSO CHECK (new, from the fix round)

The fix round added behaviour that only a human can confirm, and each is one
minute of play:
1. **Aim a firearm with RMB and do not fire** — the self-firing assault rifle
   was the sibling's headline bug and the same mapping was here. Then fire with
   LMB, and throw something (hold RMB, click).
2. **EXIT GAME → YES** should quit to the desktop (it did nothing before).
3. **The photo camera** still takes out with 2/3 while RMB is held — the fix
   removed a 70 ms trigger stagger that this path used to depend on.
4. Anything on a non-English language: the struggle prompt should read MASH.

## 3. FOUND-AND-FIXED IN PART 10 (beyond the release machinery itself)

* **The shared pipeline-cache directory** (`cz-recomp` → `cw-recomp`): both
  ports on one machine silently shared one pipeline cache + pre-warm key file
  (keyed on the identical `shader_spv` dir name). Fixed; dev cache migrated.
* **LoadShaders' pre-A.1 fallback walk**: a shipped bundle only found its
  shader cache when the CWD was the bundle root — `HostPaths::ShaderCache()`
  is now the first candidate (Case Zero's A.1 form; the czwin build's
  `readlink` error is what surfaced it).
* **memmem on Windows**: portable FindBytes in `native_kbm.cpp` (their
  d125ec2 imported) and `fe_probe.cpp` (our own file, three callers).

## 3a. DONE IN PART 11 SO FAR (2026-09-09) — post-tag, NOT in the staged artifacts

* **SUBTITLES row in the launcher** (`c00f5cd`, `docs/imported-fixes.md` §7):
  Case Zero's part-99 language selection, with the ID→bank mapping re-measured on
  this image (1=en 2=ja 4=fr 5=es 6=it 7=ko; 3 and 8 fall back to en). New
  `language=` key in `cw_settings.txt`; `CW_LANGUAGE=N` is the dev arm. Gates run:
  one bank per boot, persisted path, loud clamp, A1 kernel-order diff identical to
  the English control. **Operator-confirmed in launcher sessions (French, Korean, Japanese,
  2026-09-09)** — the CJK fonts render.
* **Launcher header said CASE ZERO** (`644c9b7`) — a sibling string literal,
  player-visible in v1.0.0. Fixed.
* Both ride the NEXT artifact build with the 688-key seed; the staged v1.0.0
  stays exactly as tested.
* **Case Zero parts 102-109 imported** (`docs/imported-fixes.md` §8, same day):
  vibration + the 30 Hz rumble tick, sampler clamp modes, the door-camera
  tolerance, MSAA as a setting, 16:10, the wheel/minigame/Q KB/M fixes, the
  glyph-scan and fence-park CPU fixes, 3 workers @ 4 cores, the async
  low-priority pre-warm, vertex-shader recipes (107, byte-identical),
  the stream-store VRAM mirror, the audio timestamp, the log file + `--diag`,
  Wayland-first, the window icon/title, old-base + AppImage scripts. Every guest
  address re-derived here. Gates: engagement of every default and control arm,
  the recipe byte-identity gate, validation at 16:9 and 16:10.
  **OWED**: the operator's sitting (vibration felt; MSAA row; a 16:10 mode;
  Q on a Y prompt; wheel notches; the grapple QTE), the czwin compile (Windows
  halves of `fence_wait.cpp`/`log_file.cpp`), the old-base build
  (`tools/release_build_oldbase.sh` — `podman tag cz-oldbase:jammy
  cw-oldbase:jammy` first), then v1.0.1 (`docs/release-notes-v1.0.1.md`).

## 3g. THE ULTRAWIDE CULLING FIX IS IN AND OPERATOR-VERIFIED (`imported-fixes.md` §12)

`5467a3e` — the game-side fov substitution, Case Zero's mechanism with all three
addresses derived here (binder `sub_8236F648`, param getter `sub_8246D1A0`, call site
`0x8246F730` found by census and confirmed by a second instrument). At 3440x1440 the
game is handed 55.79 deg for 43.0, so it CULLS to the width it renders. **Operator on
their own ultrawide: "Seems to be working perfectly."** 16:9 is untouched.
**Backlog item 1 closed**; skip-intro-logos is the only deferred item left.
The pre-warm seed also grew 688 -> 898 keys from that session (`c463848`).

## 3f. THE PRE-RELEASE AUDIT IS DONE (`imported-fixes.md` §11)

Four checks — every file, every arm, every range boundary, then the gap they exposed.
**One live defect found and fixed**: the prompt WORDING did not follow the device, so on
a controller the struggle prompt said MASH (`db1bab5`). It sat in a numbering gap between
two import ranges. **Backlog item 4 is CLOSED, not deferred**: the sibling's
shadow-distance work landed and does not work, by their own header.

Still absent ON PURPOSE, both needing per-title RE rather than an import: skip-intro-logos
and `camera_fov.cpp`. The second has a player-visible consequence worth knowing — at 21:9
and 16:10 the world is DRAWN wide but still CULLED at 16:9, so the flanks can pop. v1.0.0
does this too.

## 3e. THE LINUX v1.0.1 ARTIFACTS ARE BUILT AND GATED (2026-09-10, `imported-fixes.md` §10)

`dist/CaseWestRecomp-linux-x86_64.{tar.zst,AppImage}` at source `a950870`, built on the
old base. **glibc floor 2.35, down from 2.43** — which is what made v1.0.0 unable to
start on SteamOS. Gated: PASSED at the floor for both, REFUSED below it with the
documented `GLIBC_2.35 not found`. Hashes are in `docs/release-notes-v1.0.1.md`.

**What v1.0.1 still needs**: the Windows zip rebuilt on czwin (nothing here can build
it; `fence_wait.cpp` and `log_file.cpp` have Windows halves never yet compiled for this
port) and the operator's play sitting. Then attach three files and publish.

The Steam Deck audit is §10: all seven of the sibling's deliverables accounted for, the
player guide at `docs/steam-deck-testing.md`, and 1280x800 accepted exactly at the
resolution rule's 16:10 floor. **No Deck has run this** — the rows are causes removed,
not successes observed.

## 3d. THE LAUNCHER ROUND (imported-fixes §9, commit `28189b5`)

The sibling's `8b67e6a`/`e3981ad` — the pad drives the launcher, and the ladder carries
the 21:9 rungs (the operator's own display is 3440x1440). Found beside them and fixed:
OUR launcher window was still 720x420 from when it had seven rows, so with nine the
drop-hint footer drew at y=416 and was clipped. Gate: `CW_LAUNCHER_PAD_TEST`, which
synthesises real SDL pad events so a box with no controller can still test it.
**It rewrites `cw_settings.txt` — back it up and restore byte-identical.**

Also read out of their docs, not their code: their RX 6600 sitting confirms §8's stream
mirror, glyph scan and fence park ON AMD, and their black square now points at the
PRESENT path (it clears on alt-tab), with a bisection order this port should reuse.

## 3c. TWO THINGS THE IMPORT LEFT OPEN (both in `imported-fixes.md` §8's tail)

1. **`config/cw_soak_route.seq` IS STALE — the crowd route no longer reaches the
   crowd.** Three runs peak at ~900 draws against a 4,500 gate, and the PRE-IMPORT
   binary run the same night fails identically at 902, so this predates the import.
   Every crowd measurement on this box is blocked until the route is re-recorded,
   which needs one operator play-through (`tools/cw_route_record.sh`). The mirror's
   crowd benefit is currently the sibling's number, not ours.
2. **An intermittent hang on the EXIT path, seen once, unattributed.** A run printed
   its exit counters then sat until killed; every thread was queued behind one blocked
   `write()` to fd 2, which part 11 turned into the log tee's pipe. Both tee copies of
   that run are byte-identical and complete, which argues the reader was not behind.
   Not reproduced in three later runs. **Settle it before v1.0.1 ships** — a hang on
   exit is player-visible. One-variable test: the same route under `CW_NO_LOG_FILE=1`.

## 3b. THE NEXT ARTIFACT BUILD IS NOW A FULL ROUND, NOT A REPACKAGE

The §1a recipe (rebuild both, re-gate, refresh hashes) still holds, with two
additions from §8: the Linux leg is built INSIDE the old-base container
(`tools/release_build_oldbase.sh`, which also packages; then
`tools/release_package_appimage.sh` for the AppImage), and the Windows leg
must compile `cpu/fence_wait.cpp` (WaitOnAddress, the `synchronization` lib is
on the link line) and `host/log_file.cpp` (text-mode log) — both compiled on
czwin for the sibling, neither yet here.

## 4. THE BACKLOG OTHERWISE (carried from part 9, unchanged)

1. Game-side FOV — recipe transfers, addresses do not.
2. Eight unconsumed B4 capture frames.
3. The F3/DebugEnter boot-logo crash (parked by the operator).
4. Watch the sibling for their shadow-distance work landing.
5. The parked minor-visual-issues list is still the operator's to name.
6. Linux ffmpeg ships without x86 asm (no nasm on the dev box); glibc floor
   2.43; AppImage later; macOS is milestone C, hardware-blocked.

## 5. MEASUREMENT RULES (unchanged from part 9's §4)

One replay chain at a time; no builds during soaks; headless comparisons
against pre-part-8 numbers need `CW_VK_MSAA=1`; verdicts through
`cw_trace_band.py`.
