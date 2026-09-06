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

* `dist/CaseWestRecomp-linux-x86_64.tar.zst` — 26 MB, sha256 `f7f76eb3…`.
  Gated: `.text` identity between matched configures; clean-container GATE
  PASSED **including the whole first-run flow** (in-process extract of the real
  package, 1,322-shader disc prebuild, overlay generation byte-identical to
  the Python reference, boot, honest refusal).
* `dist/CaseWestRecomp-windows-x86_64.zip` — 21 MB, sha256 `dcac8ca1…`. Built
  on czwin (C:\cw tree, sharing C:\cz's toolchain/deps); staged exe passed
  `--smoke`; pulled back and hash-verified on arrival.
* `docs/release-notes-v1.0.0.md` is the paste-ready Release body with both
  hashes. The public `README.md` is live at the root (dev README preserved at
  `docs/dev-readme-day1.md`). CI is in (`.github/workflows/build.yml`) — its
  FIRST live runs trigger on part 10's pushes; check the Actions tab.

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
