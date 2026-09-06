# Dead Rising 2: Case West — Native PC Port

Play **Dead Rising 2: Case West** (Capcom / Blue Castle Games, 2010 — the Xbox
360 exclusive epilogue to Dead Rising 2, starring Chuck Greene and Frank West
inside the Phenotrans facility) natively on your Windows or Linux PC.

This is not an emulator: the game's Xbox 360 code was translated ahead of time
into a native program ([XenonRecomp](https://github.com/hedge-dev/XenonRecomp) /
[XenosRecomp](https://github.com/hedge-dev/XenosRecomp)), running on a
purpose-built engine with a Vulkan renderer, real XMA audio, and native
keyboard/mouse support.

> **Status: essentially complete, single-player.** The game is **playable start
> to finish** — it has been completed end to end on this port — and should look
> right in nearly all places. A few minor issues remain (listed below); none
> block progress. **Two-player co-op is not supported**: Case West's co-op is
> online-only on the 360 and this port runs the single-player game.

**No game data is included** in this repository or the downloads. You must own
Dead Rising 2: Case West and supply your own copy of the game package. This
project is not affiliated with, or endorsed by, Capcom or Microsoft.

## How to install

1. **Download** the release for your system from the
   [Releases](../../releases) page:
   - Windows: `CaseWestRecomp-windows-x86_64.zip`
   - Linux: `CaseWestRecomp-linux-x86_64.tar.zst`
2. **Unpack it anywhere** (Windows: right-click → Extract All; Linux:
   `tar --zstd -xf CaseWestRecomp-linux-x86_64.tar.zst`).
3. **Add your copy of the game.** You need the XBLA package file your Xbox 360
   downloaded — about **1.2 GB**, no file extension. On the console's storage
   it is at:
   ```
   Content/0000000000000000/58410B00/000D0000/<a long string of letters and numbers>
   ```
   Copy that file into the `assets/package/` folder inside the game folder you
   just unpacked (copying the whole `58410B00` folder in also works — or just
   drag the file onto the launcher window in step 4).
4. **Run the game** — `cw_runtime.exe` on Windows, `./cw_runtime` on Linux.
   The first run sets everything up by itself under a progress bar: it unpacks
   your package, prepares the game's shaders from its own data, and generates
   the key-prompt assets. Later launches go straight into the game.

If anything needed is missing, the game tells you exactly what and where — it
never fails with a blank screen on purpose. A `README.md` inside the bundle has
a troubleshooting section.

Your **saves and settings live outside the game folder** (Windows:
`Saved Games\Dead Rising 2 Case West\`; Linux:
`~/.local/share/Dead Rising 2 Case West/`), so you can delete or replace the
game folder at any time without losing progress.

## Controls

- **Keyboard/mouse** works out of the box with the Dead Rising 2 PC control
  scheme: WASD to move, mouse to look, left-click attack, right-click aim,
  Space jump, E to use/pick up, Tab for the map, 1/3 to cycle items, arrow
  keys for the d-pad — and the epilogue's photo camera on the same keys
  (right-click aim, 2/3 to raise the camera, left-click to shoot, 1/3 zoom).
  Every on-screen prompt shows real key icons, and the exact map is printed in
  the terminal at startup. Rebindable via a `kbmap.txt` file next to the
  executable.
- **Any controller SDL recognizes** (Xbox layout) works, and the prompts switch
  between keyboard and controller art automatically depending on which one you
  touched last.

## Features

- The **whole single-player game**: the Phenotrans facility, the story cases,
  combo weapons, the photo camera, cinematics with subtitles, save/load —
  completable start to finish.
- **60 fps** by default (the engine's own pacing, surfaced) — the original
  30 fps remains available as a setting, along with higher caps.
- **A native graphics menu** inside the game's own options (Help & Options →
  Visuals): resolution (applies live, no restart), display mode, vsync, shadow
  quality, frame cap, field of view, mouse sensitivity.
- **MSAA 2x anti-aliasing** by default, a settings launcher, and a pipeline
  pre-warm so even your first session plays smoothly.
- **Real Xbox 360 audio** (XMA) through ffmpeg — music, speech, effects, and
  the console's hardware loop behaviour.
- **Built for long sessions**: texture memory recycles over a full
  playthrough — no slow degradation on marathon runs.
- Bink video plays through the game's own decoder — the port adds file I/O and
  nothing else.

## Requirements

- A GPU and driver with **Vulkan 1.3** support.
- **Windows**: Windows 10 or later, x86-64.
- **Linux**: x86-64 with glibc **2.43 or newer** (see known issues).
- **~3 GB free disk space** after first-run unpacking.
- **Your own copy of the game** (see above).

## Known issues (minor — none affect playability)

- The occasional spot may shade slightly differently than original hardware;
  everything is being tracked and refined.
- **Linux glibc floor**: distributions older than the build base (glibc 2.43)
  refuse to start with a `GLIBC_x.yz not found` message. An AppImage-style
  build is planned.
- **No macOS build yet** — nothing blocks it in principle; it awaits test
  hardware.
- **No co-op** (see status above).

## Building from source

The repository contains no game data, so a build needs your own package plus
sibling checkouts of the (patched) recompilers — the local patches are vendored
in `tools/ci/` and the toolchain notes live in the sibling Case Zero
repository's `docs/windows-build-setup.md`. The short form (Linux, after
unpacking the game and regenerating `ppc/` per `CLAUDE.md`):

```
cmake -S runtime -B runtime/build -G Ninja
cmake --build runtime/build -j$(nproc)
./runtime/build/cw_runtime --smoke
```

CI (`.github/workflows/build.yml`) builds the host runtime on both platforms
on every push — it proves the host code compiles; it cannot run the game.

## For developers and other porters

`docs/` is this project's full working memory, written for an outside reader
porting a *different* Xbox 360 title with the same pipeline: the findings
ledger, the transferable gotcha list, the capture methodology, and the record
of how a near-complete sibling port (Dead Rising 2: Case Zero, same engine)
was transplanted rather than re-derived — `docs/imported-fixes.md` tracks
every fix taken across with its source commit. Start with
`docs/xenia-capture-analysis.md` and `docs/gotchas.md`. The original day-1 dev
README is preserved at `docs/dev-readme-day1.md`.

This port's sibling, **[Dead Rising 2: Case Zero — Native PC Port](https://github.com/wivi514/Dead_Rising_2_Case_Zero_Xenon_Recomp)**,
is also released.

## Support the project

If this port made your day and you'd like to support the work,
[**sponsor me on GitHub**](https://github.com/sponsors/wivi514) — it helps
keep improvements coming. Bug reports and issues are just as valuable.

## Credits and licensing

- **[hedge-dev](https://github.com/hedge-dev)** — XenonRecomp and XenosRecomp,
  the recompiler pair this port is built on, and UnleashedRecomp for proving
  the shape (used as a structural reference only; no GPL code is copied).
- Third-party components and their licences are enumerated in
  `THIRD_PARTY.md`, generated into every release bundle.
- This repository's own code is licensed under **PolyForm Noncommercial
  1.0.0** (see `LICENSE`).
- Dead Rising 2: Case West is © Capcom Co., Ltd. This project ships none of
  its content; everything the game needs is read from, or generated at first
  run from, the player's own copy.
