# Release notes — v1.1.0 (ALL FOUR ARTIFACTS BUILT AND GATED)

**This is the text to paste into the GitHub Release body** (everything below the
`---`). All four v1.1.0 artifacts are staged for upload at
`~/Release/Case West/1.1.0/` with a `SHA256SUMS` beside them.

**Built 2026-09-12 at source `aae9fca`** (rebuilt the same evening after the first AMD sitting found the invisible-overlay bug — the notes below say what; all four artifacts are at that commit) (branch `xlive-integration`; the docs commits
after it change no code). Linux on the OLD BASE (Ubuntu 22.04 in a podman container,
clang 15, SDL2 + LGPL ffmpeg + a static libcurl/OpenSSL compiled inside it); Windows
on czwin (clang-cl, curl-for-win's DLL beside the exe). The overlay comes from the
XenonLive_Launcher checkout at `e718507`; libxlive from XenonLive at `ab2fc80`.
**One provenance note, said rather than hidden**: the Linux overlays were compiled
from that launcher checkout carrying an uncommitted 47-line additive patch (an
`Overlay::Notify/Dismiss` toast API the sibling's co-op call uses; nothing in this
port calls it); the Windows overlay is from the clean `e718507`.

| artifact | bytes | sha256 |
|---|---|---|
| `CaseWestRecomp-linux-x86_64.tar.zst` | 31,609,251 | `ec81e020d2ecdfad385cef04f792befbfeb88b1ce198dceb61c74f128f27b7b9` |
| `CaseWestRecomp-linux-x86_64.AppImage` | 30,218,744 | `0b2b3c93f54795b99f89faa368f2b4994d2f3d40a8e209fd2346d0bb32860c8b` |
| `CaseWestRecomp-steamdeck-x86_64.tar.gz` | 31,710,489 | `995eeaa558df68483b7aff8eb8c4b998b51911f0ff31071a91e4b5f93604cfa4` |
| `CaseWestRecomp-windows-x86_64.zip` | 24,650,170 | `7b969e4bbe61f602b7231678da9fc238651f999a0691716fccd84ced3944d350` |

**Gates run:**
* **`.text` identity** between the Release and matched RelWithDebInfo configures, both
  Linux variants: OK.
* **glibc floor 2.35** (libavutil), unchanged.
* **Clean-container gate AT THE FLOOR** (`ubuntu:22.04`): the tarball stage, the
  AppImage (through its own runtime + AppRun, root beside the image) and the Steam
  Deck bundle (with `CW_GATE_SYSTEM_CXX=1`, since not bundling libstdc++ is the point
  there) — **all three GATE PASSED**, each with the whole first-run flow: the real
  package extracted (305 files), 1,322 pixel shaders translated with 0 failures,
  overlay generation byte-identical to the Python reference, a boot reading 261 `.big`
  archives, the honest refusal from a container with no game.
* **Below the floor** (`rockylinux:9-minimal`, glibc 2.34): refuses exactly as
  documented — a demonstration, not a passing gate.
* **The staged Windows exe passed `--smoke`** and a 70 s headless boot on czwin
  (14c/20t): `timeBeginPeriod(1) -> ok`, the two-core pump and the placement on, both
  guest threads named, 175-185 fps at the title screen.
* **Host boots of both Linux bundles on the dev box** against the dev tree's game data:
  the desktop tarball renders (294 fps at the title screen, CW_FPS_CAP=500); the Deck
  bundle skips the launcher, comes up borderless with `internal resolution 1280x800
  from CW_VK_RES (env wins over cw_settings.txt)`, and links the system libstdc++.

**The AMD sitting is done** (czamd, RX 6600, no dev tree — the check the packaging
script says it cannot make): the first build lost the player's input on Shift+Tab
(fixed, `aae9fca`, below); on the rebuilt one the operator's verdict was "now it's
good" — 77-100 fps median at 4,000-6,600 draws, p99 14-22 ms at 1080p.
**Owed**: a co-op session on these builds, the crowd verdict on an 8-core box.
Nobody here owns a Steam Deck; the Deck build is the sibling's recipe re-run on this
title and gated in a container, not played on a Deck.

---

Two-player online co-op, the XenonLive account underneath it, a Steam Deck build, and
the sibling port's latest performance work — on both platforms.

**Upgrading from v1.0.x:** unpack over your existing folder, or let the XenonLive
launcher install it — settings live outside the game folder and are untouched. Your
unpacked game data and shader cache are reused. **Saves are now per profile** — read
the XenonLive section before you look for yours.

### Co-op and XenonLive

- **Two-player online co-op**, the way the Xbox 360 version played it: the game's own
  co-op menus, one player hosting, the other joining, the host asked before anyone
  comes in. It runs over **[XenonLive](https://github.com/wivi514/XenonLive)** — sign
  in through the [XenonLive launcher](https://github.com/wivi514/XenonLive_Launcher),
  which installs this build for you and starts it signed in.
- **Online is through the launcher, only.** Start the game from the XenonLive launcher
  and you are signed in: your gamertag, achievements, friends, co-op. Start it any
  other way and it is the **default profile, offline** — no account, nothing online,
  the full single-player game exactly as before.
- **Saves are per profile.** Each XenonLive account has its own save folder, named by
  the gamertag, under the saved-games location (`~/.local/share/Dead Rising 2 Case
  West/` on Linux, `Saved Games\Dead Rising 2 Case West\` on Windows); the offline
  default profile keeps the folder you already have. **Your existing saves are the
  default profile's**: to carry them onto your account, copy the save files into the
  account's folder once it exists.
- **The in-game overlay: Shift+Tab.** Friends, invites and notifications over the
  game; the pad and the mouse belong to the overlay while it is open.
- **Shift+Tab does nothing when the game was not started from the launcher** (there
  is no account to show) — and says so in the log. The first v1.1.0 build opened an
  invisible overlay there that took the mouse and pad; fixed before publishing.
- **A brief Live drop no longer ends a session**: the connection is held for 30 s
  before the game hears a sign-out (the token's hourly refresh used to close a co-op
  session on both machines).

### Performance (both platforms)

- **The GPU command stream runs on two cores** on machines with 6+ physical cores (or
  8+ logical): the walk on one, the draws on another, in order. On the sibling title
  this was −1.2 ms a frame at a crowd; at a light scene it costs a fraction of a
  millisecond that the 60 fps cap hides. `CW_PUMP_SPLIT=0` is the switch back.
- **Thread placement** on 8+ physical cores with SMT: the game's two busy threads and
  the two renderer threads each get a physical core to themselves. `CW_GUEST_PIN=0`
  switches it off.
- **Windows: the system timer is asked for 1 ms** — without it every 1 ms wait in the
  frame path was 15.6 ms whenever no other program held the timer (the sibling's AMD
  test machine read a flat 47 ms a frame; 13 ms with this).
- The runtime log is capped at 256 MB; a disabled trace that re-armed itself after
  2^31 draws and halved the frame rate is fixed.

### Steam Deck

- A **Steam Deck build** (`CaseWestRecomp-steamdeck-x86_64.tar.gz`, extracts by
  double-click in Dolphin), the same three changes as Case Zero's: **no launcher**
  (drop your package in `assets/package/` and run `./cw_runtime`; every setting is
  in the in-game menu), **renders at 1280x800**, pinned by a line in `cw_defaults.env`
  you can delete, and **uses SteamOS's own C++ runtime**. Its README says how to add
  it to Game Mode. Untested on a real Deck — a report either way is what we most want.

### Requirements

- GPU + driver with **Vulkan 1.3**.
- **Windows** 10+ x86-64, or **Linux** x86-64 with **glibc 2.35 or newer** (the AppImage
  additionally needs FUSE, as every AppImage does; without it run it with
  `--appimage-extract-and-run`).
- Your own copy of the Dead Rising 2: Case West XBLA package (~1.2 GB).
- ~3 GB free disk after first-run unpacking.
- For co-op: a XenonLive account (free, in the launcher), and both players on this
  version.

### Known issues (minor)

- **On some AMD GPUs** (our own RX 6600 test machine; NVIDIA is unaffected): a **black
  square can appear in the middle of the screen** during loading screens and
  cutscenes. **Alt-tab out and back, or press Win+PrintScreen** — either clears it. It
  does not affect gameplay or progress; the sibling port shows the same on the same
  hardware.
- The occasional spot may shade slightly differently than the console.
- **No macOS build yet** — awaits test hardware, nothing structural.

### Legal

This project is not affiliated with, or endorsed by, Capcom or Microsoft.
Dead Rising 2: Case West is © Capcom Co., Ltd. The downloads contain the
recompiled program and this project's own runtime/art only; all game content
is read from, or generated at first run from, the player's own copy.
Project code: PolyForm Noncommercial 1.0.0. Third-party licences:
`THIRD_PARTY.md` inside each bundle (libcurl and OpenSSL are new in this
version — static on Linux, `libcurl-x64.dll` with `LICENSE.CURL` on Windows).
Built on hedge-dev's XenonRecomp and XenosRecomp.

### Checksums (SHA-256)

```
ec81e020d2ecdfad385cef04f792befbfeb88b1ce198dceb61c74f128f27b7b9  CaseWestRecomp-linux-x86_64.tar.zst
0b2b3c93f54795b99f89faa368f2b4994d2f3d40a8e209fd2346d0bb32860c8b  CaseWestRecomp-linux-x86_64.AppImage
995eeaa558df68483b7aff8eb8c4b998b51911f0ff31071a91e4b5f93604cfa4  CaseWestRecomp-steamdeck-x86_64.tar.gz
7b969e4bbe61f602b7231678da9fc238651f999a0691716fccd84ced3944d350  CaseWestRecomp-windows-x86_64.zip
```
