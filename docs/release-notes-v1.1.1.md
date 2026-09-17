# Release notes — v1.1.1 (ALL FOUR ARTIFACTS BUILT AND GATED; GitHub release DRAFTED)

**This is the text to paste into the GitHub Release body** (everything below the
`---`). The four v1.1.1 artifacts are staged for upload at `~/Release/Case West/1.1.1/`
with a `SHA256SUMS` beside them.

**Built 2026-09-17 at the `v1.1.1` tag** (branch `xlive-integration`; the docs commit
after the tag changes no code). Linux on the OLD BASE (Ubuntu 22.04 in a podman
container, clang 15, SDL2 + LGPL ffmpeg + a static libcurl/OpenSSL compiled inside it);
Windows on czwin (clang-cl, curl-for-win's DLL beside the exe). libxlive from XenonLive
at `1034bcc`; the overlay from the XenonLive_Launcher checkout at `e59d3ed` (Linux) /
`761de27` (Windows) — both clean checkouts this time.

| artifact | bytes | sha256 |
|---|---|---|
| `CaseWestRecomp-linux-x86_64.tar.zst` | 31,861,812 | `939da2c0abd1a85e047a47bf09fe97d3b4deb8e647adee2f74cfe58435082cd7` |
| `CaseWestRecomp-linux-x86_64.AppImage` | 30,472,696 | `0db2eebc8108e6880c1f765353a675732c42329dad064764a2b2c222880868d9` |
| `CaseWestRecomp-steamdeck-x86_64.tar.gz` | 31,970,995 | `6782317fa4327fa93946e2476d7390b8b109a19c92b73e8b823da1dacfca5d3a` |
| `CaseWestRecomp-windows-x86_64.zip` | 24,891,116 | `09b45d8456280c2760ec5d1e897faca0d6d63493b250b2f2e65d187c3e4fb073` |

**Gates run:**
* **`.text` identity** between the Release and matched RelWithDebInfo configures, both
  Linux variants: OK (40,954,834 bytes of `.text`, identical).
* **glibc floor 2.35** (libavutil), unchanged.
* **Clean-container gate AT THE FLOOR** (`ubuntu:22.04`): the tarball stage, the
  AppImage and the Steam Deck bundle (`CW_GATE_SYSTEM_CXX=1`) — **all three GATE
  PASSED**, each with the whole first-run flow: the real package extracted, 1,322 pixel
  shaders translated (1,429 with the vertex pass) with 0 failures, overlay generation,
  a boot reading 261 `.big` archives in 45 s, the honest refusal from a container with
  no game.
* **The staged Windows exe passed `--smoke`** and a 70 s headless boot on czwin
  (14c/20t): `timeBeginPeriod(1) -> ok`, the two-core pump and the placement on, both
  guest threads named, 166-174 fps at the title screen; the leaderboard timer and the
  F8/F9 lines present. Zip hash verified after transfer.
* **Host boots of both Linux bundles on the dev box** against the dev tree's game data:
  the desktop tarball at 200 fps (title screen, `CW_FPS_CAP=500`); the Deck bundle
  links the system libstdc++ and honours an existing settings file (its 1280x800 is a
  first-run default).
* **Validation layer**: the same single standing VUID as v1.1.0's binary, nothing new.

**What is measured and what is not** (`docs/imported-fixes.md` §14): every new default
engages on the headless DebugJump route and has a same-binary control arm; the tile
fix for near actors is unexercised on that route (it spawns no near actors); the
exposure fix's PICTURE is not measured on this title — the consumer code is identical
to the sibling's, where it was — so the dark-room before/after is the operator's, on
this build.

---

The fix round after v1.1.0 — the sibling port's lighting and tile-rendering fixes
brought across, the leaderboard syncing when you save, better bug-report captures, and
the launcher in your language.

**Upgrading from v1.1.0:** unpack over your existing folder, or let the XenonLive
launcher install it — settings, saves and the shader cache live outside the game
folder and are untouched.

### Rendering

- **Dark interiors: the game's auto-exposure works now.** The game measures the
  scene's brightness on the GPU and reads the result back on the CPU to set its
  exposure. That read-back never happened in this port (nor in the Xbox 360 emulator
  it was checked against — a real console does it), so the exposure sat on its
  minimum everywhere and dark rooms stayed dark. The result is now written back, and
  the game adapts the way the console did. If a scene looks *brighter* than you are
  used to, that is the fix; `CW_VK_NO_RESOLVE_WRITEBACK=1` in `cw_defaults.env`
  restores the old behaviour if you prefer it or want to compare.
- **Near characters on the right half of the screen** — the v1.1.0 known issue
  "black textures/boxes … when an NPC clips very close to the camera, on the
  right-hand side" — has two mechanisms fixed underneath it. The game renders the
  screen in two halves by replaying one command list per half; (a) the second half
  inherited the first half's last pixel shader for the early depth pass of near
  characters, and (b) a depth clear inside that replay was landing on the wrong
  half. Both are the console's behaviour now. Reported and fixed on the sibling
  port; on this one the mechanism is the same — please say whether the right-hand
  seam is gone for you.
- **Alpha-to-mask (hair, foliage-style cutouts) is rendered as real alpha-to-coverage**
  on the anti-aliased frame, as the console does, instead of a hard cutout.

### Gameplay and online

- **The PP leaderboard updates within seconds of a save**, instead of once every six
  minutes (the console's flush interval — chosen for a service where every write cost
  a round trip; XenonLive takes them one by one).

### Reporting bugs

- **F8 now captures twenty consecutive frames** (F9 stays one). The report the
  launcher's Issues tab shows keeps what fits; the whole burst is also written full-size
  next to the captures folder (`bursts/<name>/frame_NN.png`, ~100 MB, never uploaded,
  delete by hand) — for anything that flickers, a frame-by-frame diff is what finds it.

### Launcher and settings

- **The launcher and the in-game settings panel are labelled in your subtitle
  language** (English, French, Italian, Spanish; Japanese and Korean read as English —
  the small bitmap font is Latin-only; the game's own text is fully localised).

### For the curious

- **F4 opens the game's own developer debug menu** when the game is started with
  `CW_DEBUG_MENU=1` (in `cw_defaults.env` or the environment): the title's shipped
  debug tree (performance charts, thread edit, NPC spawn…) plus curated switches
  (god mode, ghost mode, zombies ignore humans, disable time of day, …). Up/Down
  select, Enter/Right/Left act, F4 closes. Developer scaffolding — it can do things
  the game never expected; save first.

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
- **Steam controllers are not picked up**, even though the rest of the SDL controller
  support works and this should not need Steam Input. Any other controller SDL
  recognises (Xbox layout) works; a Steam controller needs Steam Input as a workaround
  for now.
- **Black textures or black boxes when a lot of fading gore is on screen.** It clears
  on its own as the scene changes. (The related report — an NPC clipping close to the
  camera going wrong on the right-hand side — has the two tile fixes above underneath
  it; tell us if it is still there.)
- **Mouse-wheel scrolling in menus can miss steps.** It looks frame-rate dependent —
  at high frame rates (~120 fps) it registers reliably, lower down it sometimes does
  not. The arrow keys and WASD are unaffected.
- The occasional spot may shade slightly differently than the console.
- **No macOS build yet** — awaits test hardware, nothing structural.

### Legal

This project is not affiliated with, or endorsed by, Capcom or Microsoft.
Dead Rising 2: Case West is © Capcom Co., Ltd. The downloads contain the
recompiled program and this project's own runtime/art only; all game content
is read from, or generated at first run from, the player's own copy.
Project code: PolyForm Noncommercial 1.0.0. Third-party licences:
`THIRD_PARTY.md` inside each bundle (libcurl and OpenSSL static on Linux,
`libcurl-x64.dll` with `LICENSE.CURL` on Windows; miniz for the bug-report PNGs).
Built on hedge-dev's XenonRecomp and XenosRecomp.

### Checksums (SHA-256)

```
939da2c0abd1a85e047a47bf09fe97d3b4deb8e647adee2f74cfe58435082cd7  CaseWestRecomp-linux-x86_64.tar.zst
0db2eebc8108e6880c1f765353a675732c42329dad064764a2b2c222880868d9  CaseWestRecomp-linux-x86_64.AppImage
6782317fa4327fa93946e2476d7390b8b109a19c92b73e8b823da1dacfca5d3a  CaseWestRecomp-steamdeck-x86_64.tar.gz
09b45d8456280c2760ec5d1e897faca0d6d63493b250b2f2e65d187c3e4fb073  CaseWestRecomp-windows-x86_64.zip
```
