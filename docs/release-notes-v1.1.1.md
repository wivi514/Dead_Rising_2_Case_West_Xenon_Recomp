# Release notes — v1.1.1 (DRAFT — hashes filled in when the artifacts are built)

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
| `CaseWestRecomp-linux-x86_64.tar.zst` | TBD | `TBD` |
| `CaseWestRecomp-linux-x86_64.AppImage` | TBD | `TBD` |
| `CaseWestRecomp-steamdeck-x86_64.tar.gz` | TBD | `TBD` |
| `CaseWestRecomp-windows-x86_64.zip` | TBD | `TBD` |

**Gates run:** TBD (filled at build time — `.text` identity, glibc floor, the
clean-container gate at the floor for all three Linux bundles, `--smoke` on the staged
Windows exe).

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
TBD  CaseWestRecomp-linux-x86_64.tar.zst
TBD  CaseWestRecomp-linux-x86_64.AppImage
TBD  CaseWestRecomp-steamdeck-x86_64.tar.gz
TBD  CaseWestRecomp-windows-x86_64.zip
```
