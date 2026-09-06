# Release notes — v1.0.0 (DRAFT until the operator's test sittings pass)

**This is the text to paste into the GitHub Release body** (everything below the
`---`). Drafted 2026-09-05 in the release session; the checksums are refreshed
whenever an artifact is rebuilt — if either is EVER rebuilt, refresh its hash
here before attaching. Windows hash lands when the czwin package is pulled back.

---

Play **Dead Rising 2: Case West** — the Xbox 360 exclusive epilogue to Dead
Rising 2, starring Chuck Greene and Frank West — natively on Windows and Linux.
Not an emulator: the game's code is translated ahead of time and runs directly
on your PC, with a Vulkan renderer, real Xbox 360 audio, and native
keyboard/mouse support.

> **The port is essentially complete, single-player.** The game is **playable
> start to finish** — this port has been completed end to end — and should look
> right in nearly all places. A few minor issues remain (below); none block
> progress. **Two-player co-op is not supported**: Case West's co-op is
> online-only on the 360 and this port runs the single-player game.

**You must own the game.** No Capcom content ships in this repository or in
these downloads — the game runs from your own copy of the XBLA package.

### How to install

1. Download the build for your system below and unpack it anywhere.
2. Copy your own XBLA package file (~1.2 GB, no file extension — on the
   console it lives at
   `Content/0000000000000000/58410B00/000D0000/<long name>`) into the
   unpacked folder's `assets/package/`, or just drag it onto the launcher.
3. Run `cw_runtime.exe` (Windows) or `./cw_runtime` (Linux). The first run
   sets everything up by itself under a progress bar — unpacks your package,
   prepares all 1,322 shaders (~10 s), and generates the key-prompt assets
   from your data. Later launches start straight into the game.

Saves and settings live outside the game folder (Windows:
`Saved Games\Dead Rising 2 Case West\`; Linux:
`~/.local/share/Dead Rising 2 Case West/`), so reinstalling never touches
them. A README inside the bundle covers troubleshooting.

### Highlights

- **The whole single-player game** — the Phenotrans facility, the story
  cases, combo weapons, the photo camera, cinematics with subtitles,
  save/load, completable start to finish.
- **60 fps** by default — the original 30 fps pacing stays available as a
  setting, along with higher caps.
- **Native keyboard/mouse** with the Dead Rising 2 PC control scheme, raw
  mouse look, and real key icons on every prompt — prompts switch between
  key and controller art automatically based on what you touched last, and
  the epilogue's photo camera works fully on keyboard (right-click aim,
  2/3 raise the camera, left-click shoot, 1/3 zoom). Rebindable via
  `kbmap.txt`. Any controller SDL recognizes works too.
- **A native graphics menu** inside the game's own options (Help & Options →
  Visuals) — resolution (applies live, no restart), display mode, vsync,
  shadow quality, frame cap, field of view, mouse sensitivity.
- **MSAA 2x** anti-aliasing by default, a settings launcher, and a pipeline
  pre-warm so even the first session plays smoothly.
- **Real Xbox 360 audio** (XMA) through ffmpeg — music, speech, effects, and
  the console's hardware loop behaviour.
- **Built for marathon sessions**: texture memory recycles over a full
  playthrough — no whitening or slow degradation on long runs.
- Under the hood: 58,345 PowerPC functions statically recompiled to native
  code, the 360 GPU's command stream executed on Vulkan 1.3, Bink video
  through the game's own decoder, and a first run that builds everything it
  needs from your own copy of the game.

### Requirements

- GPU + driver with **Vulkan 1.3**.
- **Windows** 10+ x86-64, or **Linux** x86-64 with **glibc 2.43 or newer**.
- Your own copy of the Dead Rising 2: Case West XBLA package (~1.2 GB).
- ~3 GB free disk after first-run unpacking.

### Known issues (minor)

- The occasional spot may shade slightly differently than the console.
- **Linux glibc floor** (2.43): older distributions refuse to start with a
  `GLIBC_x.yz not found` message. An AppImage-style build is planned.
- **No macOS build yet** — awaits test hardware, nothing structural.
- **No co-op** (see above).

### Legal

This project is not affiliated with, or endorsed by, Capcom or Microsoft.
Dead Rising 2: Case West is © Capcom Co., Ltd. The downloads contain the
recompiled program and this project's own runtime/art only; all game content
is read from, or generated at first run from, the player's own copy.
Project code: PolyForm Noncommercial 1.0.0. Third-party licences:
`THIRD_PARTY.md` inside each bundle. Built on hedge-dev's XenonRecomp and
XenosRecomp.

Also released: the sibling port,
[Dead Rising 2: Case Zero — Native PC Port](https://github.com/wivi514/Dead_Rising_2_Case_Zero_Xenon_Recomp).

### Checksums (SHA-256)

```
c1d81f875da1a199b22a65635b9977dc9a488fd964d1331cc21a48dfc023e40b  CaseWestRecomp-linux-x86_64.tar.zst
PENDING-WINDOWS-BUILD                                             CaseWestRecomp-windows-x86_64.zip
```
