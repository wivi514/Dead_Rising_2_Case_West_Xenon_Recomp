# Release notes — v1.0.1 (ALL THREE ARTIFACTS BUILT AND GATED)

**This is the text to paste into the GitHub Release body** (everything below the
`---`). The v1.0.0 artifacts stay exactly as tested. All three v1.0.1 artifacts are
staged for upload at `~/Release/Case West/1.0.1/` with a `SHA256SUMS` beside them.

**Built 2026-09-10 at source `be203d5`, on the OLD BASE** (Ubuntu 22.04 in a podman
container, clang 15, with SDL2 and the LGPL ffmpeg compiled inside it):

| artifact | bytes | sha256 |
|---|---|---|
| `CaseWestRecomp-linux-x86_64.tar.zst` | 28,359,146 | `6acf7ec4fb3e7a75cff78274a218072ffbaab6abdb38957ad2c5bb6e72830aac` |
| `CaseWestRecomp-linux-x86_64.AppImage` | 27,355,640 | `478905c77f2b8e4e50fffbb7a943108134c89f89a3edd53c03f8c49d4ae3b016` |
| `CaseWestRecomp-windows-x86_64.zip` | 22,212,742 | `973b136939d114d362e33ef0c320c8bbdcd494aad1357b3cf0ba25ada77d5bba` |

**Gates run on both Linux artifacts:**
* **glibc floor 2.35**, computed per file from the artifact itself. The binding library
  is `libavutil` (2.35); everything else needs only 2.34.
* **Clean-container gate AT THE FLOOR** (`ubuntu:22.04`), both artifacts: **GATE PASSED**,
  including the whole first-run flow — the real package extracted, **1,429 shaders
  translated with 0 failures** (1,322 pixel + 107 from the new vertex recipes), overlay
  generation byte-identical to the Python reference, a boot that read 261 `.big` archives
  from the extracted tree, and the honest refusal from a container with no game.
* **BELOW the floor** (`rockylinux:9-minimal`, glibc 2.34): refuses exactly as documented
  — `GLIBC_2.35 not found (required by libavutil.so.60)`. A demonstration of the floor,
  not a passing gate.
* The AppImage additionally self-checks: `--appimage-extract-and-run` smoke (no FUSE),
  the data root resolving BESIDE the image with `assets/package/` seeded, and the FUSE
  mount path a double-click takes.

What the next build carries beyond v1.0.0, in the order it landed:
* the 688-key pre-warm seed (part 10's harvest, committed and waiting);
* the launcher SUBTITLES row and the CASE ZERO header fix (part 11, §7);
* everything in §8 — the fix round below;
* the launcher round in §9 — the pad, the 21:9 rungs, and the clipped footer.

**The Windows leg is built** (czwin, at the same source commit): the files whose Windows
halves had never been compiled for this port — `fence_wait.cpp` (WaitOnAddress),
`log_file.cpp` (text-mode log), `png_icon.cpp`, `rumble_guest.cpp`, `camera_fov.cpp` —
all compiled, with ONE warning and no errors, and the staged exe passed `--smoke`.

**Owed before this ships**: the operator's play sitting (vibration, the MSAA row, a
16:10 mode if a display offers one, keyboard Q on a Y prompt, the mouse wheel), and the
one check no script here can make — running the staged Windows exe on a machine with no
dev tree.

---

A fix round for both platforms, carrying over the sibling port's player-reported fixes
from its v1.0.2 — every one re-measured on this game where it could be — plus a
lower Linux glibc requirement and an AppImage.

**Upgrading from v1.0.0:** unpack over your existing folder, or anywhere — saves and
settings live outside the game folder and are untouched. Your unpacked game data and
shader cache are reused.

### Both platforms

- **Controller vibration.** The game's rumble now reaches your pad — hits, weapons,
  everything the Xbox 360 version shook the controller for, at the length the console
  gave them whatever your frame rate. Any pad SDL drives with rumble support; the
  game's own vibration option still applies. Set `CW_NO_RUMBLE=1` to switch it off.
- **Lights and glows no longer wrap to the opposite edge of the screen**: textures now
  clamp at their edges the way the game asks, where they used to wrap.
- **Door transitions at 21:9 keep their proportions.** The camera the game uses while
  walking through a door was being rejected by the widescreen patch until you moved.
- **Keyboard: Q acts as the Y button everywhere the game reads it**, and the grapple
  QTE's face buttons follow the on-screen key art (Space / E / left-click / Q) instead
  of the DR2 PC WASD mapping the prompt never showed.
- **Prompts read correctly on a controller.** The on-screen wording now follows the
  device the same way the button art already did — being grabbed by a zombie while
  playing on a pad no longer tells you to MASH, which is the keyboard instruction.
- **Mouse wheel: one notch, one item.** Every notch counts now.
- **MSAA is a setting** — off / 2x / 4x in the launcher and in Help & Options → Visuals
  (applies at the next launch). Turning it off is the lever for lower-end GPUs.
- **16:10 resolutions** (1280×800, 1920×1200, 2560×1600): the world renders taller
  with the interface letterboxed at full width, and a windowed window follows the
  internal resolution you pick.
- **Ultrawide stops popping at the edges.** On a 21:9 or 16:10 display the game now
  CULLS to the width it renders, where before it still decided what to draw using the
  16:9 view — so scenery at the far left and right no longer appears and disappears as
  you turn. The Field of View slider moves the game's own camera the same way, rather
  than only the projection.
- **No first-session pop-in from vertex shaders**: the first run now prepares the
  vertex shaders too (107 recipes over the disc's templates), so the shipped pipeline
  pre-warm can build every pipeline before the first frame, and that warm now runs in
  the background at low priority instead of holding the boot for tens of seconds on a
  cold driver cache.
- **Lower CPU cost at the crowd**: the keyboard/mouse prompt-art scan that used to
  sweep memory for two minutes on a full core after every launch now finishes in a
  tenth of a second, and the game's GPU-fence wait parks the core instead of spinning.
  Four-core CPUs get three worker threads.
- **Geometry lives in video memory** (a device-local mirror of the vertex/index store),
  which roughly halves the GPU frame at 1080p on cards limited by PCIe fetch.
- **A quieter log**: the audio decoder no longer writes a warning line thirty times a
  second.
- **A log file and a diagnostic mode** for bug reports: `cw_runtime.log` is written
  beside the game folder's `assets/` on every run (the previous run kept as
  `cw_runtime.log.1`), and `cw_runtime --diag` prints one line per fact about your OS,
  GPU, driver, Vulkan features and display path. Attach both to an issue.
- **The window title is the game's name and the frame rate**, and the window wears the
  game's own tile icon read from your unpacked copy (Windows and X11; Wayland keeps the
  desktop's icon).
- **Subtitle language from the launcher** — English, French, Italian, Spanish, Japanese
  or Korean (pick it before PLAY; it is read once at boot).
- **The launcher takes a controller.** D-pad or left stick to move, A to select, START to
  play from any row, B to quit — so a machine with no keyboard to hand is no longer stuck
  at the first window. The launcher also lists the **21:9 sizes** (2560x1080, 3440x1440,
  3840x1600), which is how you turn on ultrawide rendering without editing anything, and
  its window is tall enough again to show the line telling you where to drop your game
  file.

### Linux

- **The glibc requirement drops from 2.43 to 2.35** — Ubuntu 22.04, Debian 12, Fedora 36
  and anything newer — and there is an **AppImage**: one file, put it anywhere, make it
  executable, run it; it creates `assets/package/` next to itself on first launch.
- **Wayland desktops**: the game prefers SDL's Wayland driver when the session offers
  one (the sibling port measured exactly one frame per second through XWayland on
  NVIDIA); `SDL_VIDEODRIVER` set by you still wins.

### Requirements

- GPU + driver with **Vulkan 1.3**.
- **Windows** 10+ x86-64, or **Linux** x86-64 with **glibc 2.35 or newer** (the AppImage
  additionally needs FUSE, as every AppImage does; without it run it with
  `--appimage-extract-and-run`).
- Your own copy of the Dead Rising 2: Case West XBLA package (~1.2 GB).
- ~3 GB free disk after first-run unpacking.

### Known issues (minor)

- The occasional spot may shade slightly differently than the console.
- **No macOS build yet** — awaits test hardware, nothing structural.
- **No co-op** — Case West's second player is online-only on the 360.

### Legal

This project is not affiliated with, or endorsed by, Capcom or Microsoft.
Dead Rising 2: Case West is © Capcom Co., Ltd. The downloads contain the
recompiled program and this project's own runtime/art only; all game content
is read from, or generated at first run from, the player's own copy.
Project code: PolyForm Noncommercial 1.0.0. Third-party licences:
`THIRD_PARTY.md` inside each bundle. Built on hedge-dev's XenonRecomp and
XenosRecomp.

### Checksums (SHA-256)

```
6acf7ec4fb3e7a75cff78274a218072ffbaab6abdb38957ad2c5bb6e72830aac  CaseWestRecomp-linux-x86_64.tar.zst
478905c77f2b8e4e50fffbb7a943108134c89f89a3edd53c03f8c49d4ae3b016  CaseWestRecomp-linux-x86_64.AppImage
973b136939d114d362e33ef0c320c8bbdcd494aad1357b3cf0ba25ada77d5bba  CaseWestRecomp-windows-x86_64.zip
```
