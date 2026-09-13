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
#   2. settings DEFAULTS are 1280x800 fullscreen-desktop, the    (CW_DECK -> CW_DECK_DEFAULTS)
#      Deck's native panel, instead of 1280x720 windowed. A
#      DEFAULT ONLY: the in-game RESOLUTION row still works, so
#      a player can pick 1920x1080 or anything else the Deck
#      supports and it is remembered (no CW_VK_RES pin)
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
# applies on a run that finds no cw_settings.txt — which is exactly what we want here
# (operator's call, 2026-09-13): the Deck's FIRST launch comes up at its native
# 1280x800, and the in-game settings menu can then change it to anything the Deck
# supports (1920x1080 and the rest), with the choice remembered in cw_settings.txt.
#
# NO CW_VK_RES PIN. An earlier build shipped `CW_VK_RES=1280x800` in cw_defaults.env so
# a settings file migrated from a PC install could not bring a desktop resolution with
# it — but env beats the settings file everywhere, so that line ALSO made the in-game
# RESOLUTION row do nothing, and a Deck owner could not raise or lower it without
# editing a file. The default alone gives the same good first launch and keeps the row
# working, so the pin is gone.
# The SDL2/ffmpeg/XenonUtils prefixes under thirdparty/oldbase are the desktop build's
# and are variant-independent — rebuilding them would cost 20 minutes and change nothing.
export CW_OLDBASE_SKIP_DEPS=${CW_OLDBASE_SKIP_DEPS:-1}

exec "$ROOT/tools/release_build_oldbase.sh" "$@"
