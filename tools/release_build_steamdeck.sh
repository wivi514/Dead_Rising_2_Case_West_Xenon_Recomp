#!/usr/bin/env bash
# Build the STEAM DECK variant of the Linux release artifact.
#
# WHY THIS EXISTS. A player on SteamOS 3.8 reported the v1.0.2 Linux build (tar and
# AppImage alike) dying with SIGSEGV right after the "[settings]" line, with
# CW_VKDRAW=0 and with SDL_VIDEODRIVER=x11 making no difference. Reading the boot order
# places that exactly at the LAUNCHER's window: main.cpp runs Settings_Load and then
# Host_RunLauncher before any boot machinery, and the launcher's SDL_CreateRenderer
# (host/window.cpp, right after the ApplyGameIcon call whose "[icon]" line they quoted)
# is the next thing that runs. That call is also the ONE code path no gate here has ever
# exercised: release_gate_clean_container.sh runs CW_NO_WINDOW=1 CW_LAUNCHER=0, the
# launcher early-returns on the dummy driver anyway, and this project's dev box is
# NVIDIA — whose GL driver, unlike Mesa's, does not link libstdc++ at all.
#
# So this variant removes the launcher from the player's path entirely rather than
# guessing at what inside it faults. The game window itself creates no SDL_Renderer (it
# carries SDL_WINDOW_VULKAN and the renderer presents its own image), so with the
# launcher gone a default run never asks SDL for a GL context at all.
#
# THREE CHANGES, each revertible on its own, so the report that comes back can name
# which one mattered:
#   1. cw_defaults.env says CW_LAUNCHER=0                        (CW_PKG_NO_LAUNCHER)
#   2. settings defaults are 1280x800 fullscreen-desktop, the    (CW_DECK -> CW_DECK_DEFAULTS)
#      Deck's native panel, instead of 1280x720 windowed, AND
#      cw_defaults.env PINS CW_VK_RES=1280x800 so an existing    (CW_PKG_EXTRA_DEFAULTS)
#      or migrated settings file cannot put it back
#   3. libstdc++/libgcc_s are NOT bundled, so Mesa gets SteamOS's  (CW_PKG_SYSTEM_CXX)
#      newer copy instead of our GLIBCXX_3.4.30 one shadowing it
#
# Change 3 is the only one that could make things WORSE, and only on a distribution
# whose libstdc++ is older than the old base's — which SteamOS 3.8's is not. It is in
# here because change 1 alone would not help if the same shadowing also reaches RADV,
# and a delivery that gets past the launcher only to fault at Vulkan init is no use.
#
# The archive is .tar.gz, not the desktop build's .tar.zst: Dolphin on the Deck extracts
# a .gz by double-click.
#
# Everything else — the container, the old base, the identity gate, the packaging — is
# the normal release path with the switches above set. It writes into dist-steamdeck/,
# so it cannot touch the desktop artifact or its staged tree.
#
# Usage:  tools/release_build_steamdeck.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)

export CW_DECK=ON
export CW_BUILD_TAG=-deck
export CW_PKG_SUFFIX=steamdeck-x86_64
export CW_PKG_TARGZ=1
export CW_PKG_NO_LAUNCHER=1
export CW_PKG_SYSTEM_CXX=1
export CW_PKG_README=$ROOT/tools/release/README.steamdeck.md
export CW_PKG_OUT=dist-steamdeck
# THE RESOLUTION PIN (operator instruction). CW_DECK's 1280x800 is only a DEFAULT — it
# applies on a run that finds no cw_settings.txt, and the first test launch showed
# exactly how that is not enough: the save relocation carried a settings file in from an
# existing install and the run came up at 3440x1440. CW_VK_RES is the lever that wins
# over the settings file (the env-wins rule every consumer enforces), so the shipped
# defaults file pins it and a Deck starts at its native panel whatever a migrated or
# hand-edited settings file says.
#
# WHAT IT COSTS, said out loud: while this line is present the in-game settings screen's
# RESOLUTION row does nothing — env beats the file, and the renderer says so on stdout
# ("internal resolution 1280x800 from CW_VK_RES (env wins over ...)"). The README tells
# the player to delete the line if they want that row back, which is a plain-text edit
# next to the executable and not a rebuild.
export CW_PKG_EXTRA_DEFAULTS='CW_VK_RES=1280x800'
# The SDL2/ffmpeg/XenonUtils prefixes under thirdparty/oldbase are the desktop build's
# and are variant-independent — rebuilding them would cost 20 minutes and change nothing.
export CW_OLDBASE_SKIP_DEPS=${CW_OLDBASE_SKIP_DEPS:-1}

exec "$ROOT/tools/release_build_oldbase.sh" "$@"
