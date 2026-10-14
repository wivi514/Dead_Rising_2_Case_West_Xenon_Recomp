# Dead Rising 2: Case West — Native PC Port

**To play with online feature download Case West from the [XenonLive launcher](https://github.com/wivi514/XenonLive_Launcher)**

Play **Dead Rising 2: Case West** (Capcom / Blue Castle Games, 2010 — the Xbox
360 exclusive epilogue to Dead Rising 2, where Chuck Greene and Frank West break
into the Phenotrans facility) natively on Windows, Linux, Steam Deck, and Apple
Silicon Macs.

This is not an emulator: the game's Xbox 360 code was translated ahead of time
into a native program ([XenonRecomp](https://github.com/hedge-dev/XenonRecomp) /
[XenosRecomp](https://github.com/hedge-dev/XenosRecomp)), running on a
purpose-built engine with a Vulkan renderer, real XMA audio, and native
keyboard/mouse support.

> **Status: finished. v1.1.2 (October 2026) is my official final build.** The
> single-player game is **playable start to finish**, and two-player online
> co-op works over
> [XenonLive](https://github.com/wivi514/XenonLive), through its
> [launcher](https://github.com/wivi514/XenonLive_Launcher) — the game's own co-op
> menus, the way the 360 version played it. Started outside the launcher the game
> is offline and single-player. I have stopped active development, but I encourage
> others to continue improving the port, fork it, and contribute pull requests.
> Issues and pull requests may go unanswered; the code and development record remain
> here for anyone who wants to carry it forward.

**No game data is included** in this repository or the downloads. You must own
Dead Rising 2: Case West and supply your own copy of the game package. This
project is not affiliated with, or endorsed by, Capcom or Microsoft.

## How to install

1. **Download** the release for your system from the
   [Releases](../../releases) page:
   - Windows: `CaseWestRecomp-windows-x86_64.zip`
   - Linux: `CaseWestRecomp-linux-x86_64.tar.zst`
   - Linux, single file: `CaseWestRecomp-linux-x86_64.AppImage` (same build;
     `chmod +x` it and run it)
   - Steam Deck: `CaseWestRecomp-steamdeck-x86_64.tar.gz`
   - Mac (Apple Silicon): `CaseWestRecomp-macos-arm64.zip`
2. **Unpack it anywhere** (Windows: right-click → Extract All; Linux:
   `tar --zstd -xf CaseWestRecomp-linux-x86_64.tar.zst`; Steam Deck:
   `tar -xzf CaseWestRecomp-steamdeck-x86_64.tar.gz`; Mac: double-click the zip).
   The Mac build is not notarized; before its first launch, run
   `xattr -dr com.apple.quarantine <the unzipped folder>` in Terminal.
3. **Add your copy of the game.** You need the XBLA package file your Xbox 360
   downloaded — about **1.2 GB**, no file extension. On the console's storage
   it is at:
   ```
   Content/0000000000000000/58410B00/000D0000/<a long string of letters and numbers>
   ```
   Copy that file into the `assets/package/` folder inside the game folder you
   just unpacked (copying the whole `58410B00` folder in also works — or just
   drag the file onto the launcher window in step 4).
4. **Run the game** — `cw_runtime.exe` on Windows, `./cw_runtime` on Linux or
   Steam Deck, or `CaseWestRecomp.app` on Mac (keep it beside the `assets` folder).
   The first run sets everything up by itself under a progress bar: it unpacks
   your package, prepares the game's 1,322 shaders (7.5 s on a 16-thread
   desktop, longer on fewer cores — it uses all of them), and generates
   the keyboard prompt art from your data. Later launches go straight into the
   game.

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
  keys for the d-pad. The epilogue's **photo camera** works fully on keyboard
  too — hold right-click, press 2 or 3 to raise the camera, left-click to take
  the shot, 1/3 or the wheel to zoom. Every on-screen prompt shows real key
  icons, and the exact map is printed in the terminal at startup. Rebindable
  via a `kbmap.txt` file next to the executable.
- **Any controller SDL recognizes** (Xbox layout) works, and the prompts switch
  between keyboard and controller art automatically depending on which one you
  touched last. **Controller vibration** reaches the pad, at the lengths the
  console gave each effect whatever your frame rate (`CW_NO_RUMBLE=1` turns it
  off).

## Features

- The **whole single-player game**: the Phenotrans facility, the story cases,
  combo weapons, the photo camera, cinematics with subtitles, save/load —
  completable start to finish.
- **60 fps** (the engine's own pacing, surfaced) — the original 30 fps stays
  available as a setting, along with higher caps.
- **A graphics menu inside the game's own options** (Help & Options → Visuals):
  resolution — which **applies live, without a restart** — display mode, vsync,
  shadow quality, MSAA (off / 2x / 4x, next launch), frame cap, field of view
  and mouse sensitivity. The launcher carries the same rows.
- **Ultrawide support** — the resolution row lists your monitor's own modes, so
  a 21:9 display can pick e.g. 3440×1440 and the game renders true widescreen,
  and **culls to that width too**, so scenery at the far edges does not pop in
  and out as you turn. **16:10 displays** (1280×800, 1920×1200, 2560×1600)
  render the world taller with the interface letterboxed at full width.
  **Tip:** raise Field of View in the options when playing ultrawide; the stock
  FOV was chosen for 16:9.
- **MSAA 2x anti-aliasing** by default, a settings launcher, and a pipeline
  pre-warm plus background pipeline building, so even a first session plays
  smoothly instead of hitching the first time it sees something new.
- **The launcher works on a controller as well as a keyboard** — D-pad or left
  stick to move, A to select, START to play, B to quit — and lists the 21:9 and
  16:10 sizes alongside the 16:9 ones.
- **Subtitle language from the launcher** — English, French, Italian, Spanish,
  Japanese or Korean, the six the game ships. It is read once at boot, so pick
  it before pressing PLAY.
- **Real Xbox 360 audio** (XMA) through ffmpeg — music, speech, effects, and
  the console's hardware loop behaviour.
- **Built for long sessions**: texture memory recycles over a full playthrough
  — no whitening or slow degradation on marathon runs.
- **A log file and a diagnostic mode** for bug reports: every run writes
  `cw_runtime.log` beside the game folder's `assets/` (the previous run is kept
  as `cw_runtime.log.1`), and `cw_runtime --diag` prints one line per fact
  about your OS, GPU, driver and display path, and writes it to `cw_diag.txt`.
- Bink video plays through the game's own decoder; the port supplies file I/O
  and nothing else.
- Under the hood: **58,345 PowerPC functions** statically recompiled to native
  code, the 360 GPU's command stream executed on Vulkan 1.3, and a first run
  that builds everything it needs from your own copy of the game.

## Requirements

- A GPU and driver with **Vulkan 1.3** support (tested on NVIDIA and AMD).
- **Windows**: Windows 10 or later, x86-64.
- **Linux**: x86-64 with glibc **2.35 or newer** (Ubuntu 22.04, Debian 12,
  Fedora 36 and newer).
- **Steam Deck**: use the dedicated Steam Deck archive; it defaults to the Deck's
  1280×800 display and omits the desktop launcher.
- **macOS**: Apple Silicon (M1 or newer), macOS 13 or later. Intel Macs are not
  supported. The app is not notarized; see the install note above.
- **~3 GB free disk space** after first-run unpacking.
- **Your own copy of the game** (see above).

## Known issues (minor — none affect playability)

- A few spots may shade slightly differently than original hardware.
- **On some AMD GPUs** (seen on our own RX 6600 test machine; NVIDIA is unaffected):
  a **black square can appear in the middle of the screen** during loading screens and
  cutscenes. **Alt-tab out and back, or press Win+PrintScreen** — either clears it. It
  does not affect gameplay or progress. The sibling port shows the same thing on the
  same hardware, which points at the code the two share rather than at this game.
- **Steam Deck**: a dedicated build is available, but direct testing on Deck hardware
  has not been completed. `docs/steam-deck-testing.md` has testing notes; community
  reports and fixes are welcome.
- **Co-op needs the XenonLive launcher** — the game only goes online when the
  launcher starts it (see the status note above); a plain launch is single-player.

## The sibling port

The prologue, **Dead Rising 2: Case Zero**, is ported the same way and shares almost
all of this runtime:
[Dead_Rising_2_Case_Zero_Xenon_Recomp](https://github.com/wivi514/Dead_Rising_2_Case_Zero_Xenon_Recomp).
Fixes flow both ways between the two, which is why a defect seen on both — like the
AMD black square above — is worth more than one seen on either alone.

## Debug menu (optional)

The retail disc still carries Blue Castle's own development menu, and this port can
switch it back on. **F4** opens it; **F2** opens the DebugJump screen, which warps
straight to any case or mission from the title menu.

It is off by default, so pressing F4 in a normal install does nothing. Pick whichever
of these suits you — they all do the same thing:

**A shortcut, if you want to switch back and forth.** Right-click `cw_runtime.exe`
→ **Send to** → **Desktop (create shortcut)**, then right-click the new shortcut →
**Properties**, and add a space and `--debug-menu` to the end of the **Target** box:

```
"C:\Games\CaseWestRecomp\cw_runtime.exe" --debug-menu
```

Keep the normal icon as well and you have one install with two ways to start it —
ordinary play, and play with the menu. On Linux the same flag works from a
terminal: `./cw_runtime --debug-menu`.

**A text file, if you want it on permanently.** Open `cw_defaults.env` (next to the
executable) in any text editor and add one line:

```
CW_DEBUG_MENU=1
```

**An environment variable, if you prefer that.** `CW_DEBUG_MENU=1` anywhere the game
can see it. The command-line flag overrides both of the above; the environment
overrides the file.

Once it is on, the menu takes **Up/Down** to move, **Enter** to use, **Left/Right** to
change a value and **F4** to close. The game window has to have keyboard focus.

> This is retained developer scaffolding, not a feature we wrote: some entries do
> nothing, and a few can leave the game in a strange state. It cannot corrupt a save
> that is not written, but take a backup if you are attached to one.

## Building from source

The repository contains no game data, so a build needs your own package plus
sibling checkouts of the (patched) recompilers — the local patches are vendored
in `tools/ci/`, and the Windows toolchain is documented in the sibling port's
[windows-build-setup.md](https://github.com/wivi514/Dead_Rising_2_Case_Zero_Xenon_Recomp/blob/master/docs/windows-build-setup.md).
The short form (Linux, after unpacking the game and regenerating `ppc/` per
`CLAUDE.md`):

```
cmake -S runtime -B runtime/build -G Ninja
cmake --build runtime/build -j$(nproc)
./runtime/build/cw_runtime --smoke
```

CI (`.github/workflows/build.yml`) builds the host runtime on both platforms
on every push — it proves the host code compiles; it cannot run the game.

## For developers and other porters

`docs/` is this project's full working memory, written for an outside reader
porting a *different* Xbox 360 title with the same pipeline: the numbered
findings ledger, the transferable gotcha list, the `.big`/STFS/XEX format
notes, and the record of how a near-complete sibling port of the *same engine*
was transplanted rather than re-derived — `docs/imported-fixes.md` tracks every
fix taken across, with its source commit and what re-measuring it here proved.
Start with `docs/xenia-capture-analysis.md` and `docs/gotchas.md`. The original
day-1 dev README is preserved at `docs/dev-readme-day1.md`.

**If you want to continue the port**, the notes above and the build instructions
are the hand-off. Community forks and pull requests are encouraged; the original
maintainer may not be available to respond.

Its sibling, **[Dead Rising 2: Case Zero — Native PC Port](https://github.com/wivi514/Dead_Rising_2_Case_Zero_Xenon_Recomp)**,
is also released.

## Support the project

The v1.1.2 build is my final release, and I am no longer maintaining this port. If
you'd like to support my work, [**sponsor me on GitHub**](https://github.com/sponsors/wivi514)
— but please don't sponsor expecting further updates from me on Case West. I welcome
community forks and contributions that carry the project forward.

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
