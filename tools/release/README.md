# Dead Rising 2: Case West — native PC recompilation

This is a native port of the Xbox 360 XBLA title *Dead Rising 2: Case West*
(Capcom / Blue Castle Games, 2010), produced by static recompilation. It is not an
emulator: the game's code was translated ahead of time and runs directly on your CPU,
with the Xbox 360's GPU commands translated to Vulkan.

**This build ships no game data.** You supply your own copy of the game, and the first
run turns it into everything else it needs.

**The port is essentially complete**: the single-player game is playable start to
finish and should look right in nearly all places. A few minor known issues remain;
none block progress. **Two-player co-op is not supported** — Case West's co-op is
online-only on the 360 and this port is single-player (Frank still appears in the
story as the game directs).

## Requirements

* An x86-64 PC (Windows 10+ or Linux) with a working **Vulkan** driver (on Linux: if
  `vulkaninfo` works, you are fine).
* Your own copy of the **Dead Rising 2: Case West** XBLA package (title ID `58410B00`).
  It is the ~1.2 GB file your Xbox 360 downloaded; on the console's storage it lives at
  `Content/0000000000000000/58410B00/000D0000/<a long hash, no file extension>`.
* **Keyboard/mouse or a controller — both are first-class.** The keyboard uses the
  Dead Rising 2 PC scheme (WASD + mouse look, left-click attack, right-click aim,
  Space jump, E use; the full map prints in the terminal at startup, and `kbmap.txt`
  next to the executable rebinds it). Any controller SDL recognises (Xbox layout)
  works too, and on-screen prompts switch between key and button art automatically
  depending on which device you touched last.

## Quick start

1. Run the game — `cw_runtime.exe` on Windows, `./cw_runtime` on Linux. The
   **launcher** opens: pick your display mode, resolution and
   other settings, and **drag your XBLA package file onto the window** to install the
   game (putting it in `assets/package/` by hand works too).
2. Press PLAY.
3. The first run sets everything up, once, with progress shown as it goes:
   * unpacks the package (~1.2 GB in, ~1.2 GB out),
   * prepares the game's shaders from its own disc data (pixel shaders, and the
     vertex shaders through `vs_recipes.bin`),
   * generates the key-prompt assets from your own game data,
   * builds the graphics pipelines in the background while the game starts, so
     the first session plays like the second. A place no one has recorded yet may
     still translate a shader on the fly (a fraction of a second, in the
     background; the log says `first-sight translation` when it happens).
4. Subsequent launches skip all of that and start straight into the game.

Settings (resolution, display mode, shadows, anti-aliasing) are in the in-game
settings menu (Help & Options → Visuals). Your save games and settings live
**outside this folder**, so reinstalling or deleting the game can never touch them:

* Windows: `Saved Games\Dead Rising 2 Case West\` (next to your Documents folder)
* Linux: `~/.local/share/Dead Rising 2 Case West/`

A build that finds saves in the old in-folder location (`assets/save/`) copies them
over automatically on its first launch and leaves the originals as a backup.

## If something is missing

The runtime refuses to start with a message that says what is missing, where it goes,
and what produces it — a black screen is never the intended failure. If it starts but
misbehaves, read on.

## Troubleshooting

All of these are environment variables — run e.g. `CW_FPS_CAP=30 ./cw_runtime`.
Defaults for a shipped build come from `cw_defaults.env` next to the executable (a
plain text file you can edit); anything you set in the environment overrides it.

**Wrong-looking or missing graphics** — try these one at a time, in this order, and
if one fixes it, please report that along with your GPU and driver:

| variable | what it bisects |
|---|---|
| `CW_VK_MSAA=1` | turns anti-aliasing off (2x is the default) |
| `CW_VK_NO_PAR_RECORD=1` | records GPU commands on one thread instead of several |
| `CW_VK_NO_DEFERRED_CLEAR=1` | turns off deferred scoped clears |
| `CW_NO_KB_PROMPTS=1` | restores the pad button art on prompts |
| `CW_VK_VALIDATION=1` | runs the Vulkan validation layer and prints what it finds |

**Stutter in the first minutes of a session** is mostly shader/pipeline warm-up and
fades as the caches fill. It should be far milder from the second launch on. If it
never fades: `CW_VK_NO_PREWARM=1` disables the pipeline pre-warm as a test.

**Performance**: the game's own frame pacing targets 30 fps on the 360;
`CW_FPS_CAP=60` runs the mode the game itself ships for higher refresh. Lowering the
resolution in the settings menu helps most in crowds.

**Sound**: `CW_NO_AUDIO_OUT=1` disables audio output entirely, `CW_NO_XMA_DECODE=1`
disables the decoder — useful to tell a sound problem from a game problem when
reporting an issue.

**Starting over**: delete `assets/game/` and/or `assets/shader_spv/` and the first-run
steps run again — your saves are unaffected, they live in the saved-games location
above. Deleting THAT folder removes your saves and settings; the game never does this
itself.

**A log of everything** is written to `cw_runtime.log` next to the game folder's
`assets/` (for the AppImage: next to the `.AppImage` file), and the previous run's log
is kept as `cw_runtime.log.1`. When reporting a problem, attach that file. To describe
your machine — OS, GPU, driver, which Vulkan features it has, which display driver the
window uses — run `cw_runtime --diag` (Windows: `cw_runtime.exe --diag` from a command
prompt): it prints one line per fact, writes the same to `cw_diag.txt`, and exits.
Paste both into the issue. Say what you SAW — "closed after the progress bar", "black
window", "slow" — rather than "didn't work"; the log usually says the rest.

## What is in this bundle

* `cw_runtime` / `cw_runtime.exe` — the game: recompiled code plus the host runtime.
* `lib/` — bundled libraries (SDL2, an LGPL ffmpeg build for the 360's XMA audio, the
  DirectX Shader Compiler used to translate shaders). Licenses are alongside, and
  `THIRD_PARTY.md` lists everything with provenance.
* `kbm_chips/` — the keyboard prompt icons (this project's own art), composed into
  the game's menus at first run.
* `tools/extract_stfs.py` — a reference unpacker; the runtime normally unpacks your
  package itself, this is for doing it by hand (`python3 tools/extract_stfs.py -h`).
* `cw_defaults.env` — default settings applied when not set in your environment.

The Vulkan loader is deliberately *not* bundled — your GPU driver supplies it.

## Legal

This project ships no game content and cannot supply any; it loads the package you
own. The recompilation and runtime are licensed PolyForm Noncommercial 1.0.0 (see
`LICENSE`); third-party components and their licenses are listed in `THIRD_PARTY.md`.
