// The first-run overlay generator (imported from Case Zero's release §0, adapted).
//
// WHY THIS EXISTS. The keyboard-prompt overlay — assets/game_kbm (all 26 keyboard
// prompt icons, the PRESS ENTER / A÷D / MASH string edits, the camera-bar glyph
// retarget and the device-follow sidecar) — is generated in the dev tree by
// tools/gen_kbm_icons.py FROM the game data. It carries Capcom-derived bytes
// (repacked banks are mostly Capcom's own entries), so it can neither ship in the
// release artifact nor be regenerated on a player's machine that has no Python:
// without this module a shipped bundle silently shows pad art on every prompt
// (kernel/vfs.cpp falls through to the shipped files without a word — the
// gotcha-5 shape).
//
// So the transforms live HERE, in the first-run flow, on the road host/stfs_extract
// proved: an in-process C++ port whose output is BYTE-IDENTICAL to the Python
// reference, which stays the dev tool and the oracle. The one asset the transforms
// need that is OURS and not derivable — the 26 key-cap chip images — ships
// pre-baked as raw DXT5 texel blobs (tools/release/kbm_chips/*.dxt, exported by
// gen_kbm_icons.py --export-chips; no Capcom byte, and PIL stays dev-only).
//
// WHAT CASE ZERO'S VERSION HAS THAT THIS ONE DELIBERATELY DOES NOT: the whole
// game_patched layer (options_pc.txt rewrite, preload4 eviction, layout.bin
// re-pins, per-language value strings). Case West's Visuals panel is a pure code
// hook (cpu/pc_options_cw.cpp — "no repacked asset"), this title reads its
// frontend banks LOOSE, and no game_patched directory has ever existed in this
// tree. What this one has that Case Zero's does not: the camera-bar glyph
// retarget in ingame.big (part 9, 4532e50) and the size-pinned str_en.bcs.
//
// The identity gate is free and exact: run the Python tool, run
// `cw_runtime --gen-overlays`, diff the trees. It is what makes this a port
// rather than a reimplementation.
//
// CW_NO_OVERLAY_GEN=1 is the off switch (every automatic first-run step has one);
// CW_NO_PATCHED_ASSETS=1 / CW_NO_NATIVE_KBM=1 already disable the overlay's USE
// and are honoured here too — a run that asked for the shipped data byte-for-byte
// should not spend a first-run generating files it will then ignore.
#pragma once

#include <functional>
#include <string>

namespace OverlayGen
{
// True when generation should run at boot: the game is unpacked, the chip blobs
// are found, no off switch is set, and any overlay output is missing or was
// written by an older generator (a version stamp guards against a shipped update
// silently serving stale banks).
bool WantedAtBoot();

// Generate everything that is missing or stale. `progress` receives a label and a
// 0..1 fraction for the first-run window; it may be null. On failure returns false
// with `err` naming the first gate that refused — the caller prints it loudly and
// boots on: the VFS then serves the shipped data, which is degraded but honest.
bool Generate(const std::function<void(const char*, float)>& progress, std::string& err);
}
