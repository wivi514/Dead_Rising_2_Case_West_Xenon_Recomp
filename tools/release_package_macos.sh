#!/usr/bin/env bash
# Assemble the macOS (Apple Silicon) release artifact — release-plan milestone C.4, the
# sibling of tools/release_package_linux.sh. Run ON the Mac (ssh czmac; see
# docs/macos-build-setup.md), against a Release tree configured with the LGPL ffmpeg and
# the real SDL2 built there (that doc's "Release" section has the exact cmake line).
#
# WHY A FOLDER WITH AN .app INSIDE, AND NOT A BARE .app. The runtime finds its data root by
# walking up from the executable to the first directory holding `assets/` (host_paths.cpp,
# up to four levels: Contents/MacOS -> Contents -> .app -> the folder beside it). The
# player's game package and everything first run unpacks from it live in that `assets/`,
# so it must sit BESIDE the bundle: writing inside a signed .app would break its seal, and
# a bundle with no `assets/` anywhere above it resolves the root to its own MacOS dir. So
# the archive is CaseWestRecomp/{CaseWestRecomp.app, assets/package/, README...} — the
# same shape as the Linux tarball, with the .app where the Linux executable is.
#
# WHAT GOES INSIDE THE BUNDLE, and why each one:
#   Contents/MacOS/cw_runtime          the game, and NOTHING else: codesign refuses any
#                                      non-code file in Contents/MacOS ("code object is not
#                                      signed at all"), which the first version of this
#                                      script hit on kbm_chips/
#   Contents/Resources/{cw_defaults.env,prewarm.keys,vs_recipes.bin,kbm_chips/,licenses/}
#                                      HostPaths::ShippedDir(), which is Contents/Resources
#                                      inside a bundle and the exe's directory elsewhere
#   Contents/Frameworks/               SDL2 (real, not sdl2-compat), LGPL libavcodec/
#                                      libavutil, the Vulkan loader, MoltenVK, DXC (the
#                                      translator searches <exe>/../Frameworks on macOS)
#   Contents/Resources/vulkan/icd.d/MoltenVK_icd.json
#                                      the loader's in-bundle ICD search path on macOS;
#                                      without it the loader finds no driver at all on a
#                                      Mac with no Vulkan SDK, i.e. every player's Mac.
# Every dylib's install name becomes @rpath/<name> and the executable gets
# @executable_path/../Frameworks as its rpath, so nothing points back at /opt/homebrew or at the
# build machine's home directory. The script FAILS if any absolute non-system path is
# left after the rewrite.
#
# SIGNING ORDER: strip -> install_name_tool -> codesign, because both of the first two
# edit the Mach-O and any edit invalidates a signature (release-plan C.4). Ad-hoc
# (`codesign -s -`) is not optional: Apple Silicon will not execute an unsigned arm64
# binary at all.
#
# Usage:  tools/release_package_macos.sh [BUILD_DIR] [OUT_DIR]
#   CW_FFMPEG_PREFIX / CW_SDL2_PREFIX  where the LGPL ffmpeg and SDL2 were installed
#   CW_DXC_LIB                         the arm64 libdxcompiler.dylib
#   CW_FFMPEG_TARBALL                  the ffmpeg source tarball, hashed into THIRD_PARTY.md
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${1:-$ROOT/runtime/build-release}
OUT=${2:-$ROOT/dist}
NAME=CaseWestRecomp
STAGE=$OUT/$NAME
APP=$STAGE/$NAME.app
FFPFX=${CW_FFMPEG_PREFIX:-$HOME/cz/thirdparty/ffmpeg-lgpl}
SDL2PFX=${CW_SDL2_PREFIX:-$HOME/cz/thirdparty/sdl2}
DXC_SRC=${CW_DXC_LIB:-$HOME/cz/XenosRecomp/thirdparty/dxc-bin/lib/arm64/libdxcompiler.dylib}
FFTAR=${CW_FFMPEG_TARBALL:-$HOME/cz/tp-work/ffmpeg-8.1.2.tar.xz}
BREW=$(brew --prefix 2>/dev/null || echo /opt/homebrew)

fail() { echo "FAIL: $*" >&2; exit 1; }
[ "$(uname -s)" = Darwin ] || fail "this script runs on the Mac (ssh czmac)"
[ -x "$BUILD/cw_runtime" ] || fail "no executable at $BUILD/cw_runtime"
[ -f "$DXC_SRC" ] || fail "no libdxcompiler.dylib at $DXC_SRC (set CW_DXC_LIB)"

# The two licence checks the Linux script makes, by install name instead of ldd.
otool -L "$BUILD/cw_runtime" | grep -q "$FFPFX/lib/libavcodec" \
    || fail "the binary does not link the LGPL ffmpeg under $FFPFX (Homebrew's is GPL):
$(otool -L "$BUILD/cw_runtime" | grep avcodec)"
# SDL2 links as @rpath/libSDL2-2.0.0.dylib from either source, so the install name cannot
# tell them apart: refuse Homebrew's shim by path here, and the bundled copy is checked for
# libSDL3 below.
otool -L "$BUILD/cw_runtime" | grep -q sdl2-compat \
    && fail "the binary links Homebrew's sdl2-compat; configure with -DCW_SDL2_PREFIX=$SDL2PFX"

# Same play-copy preservation as the Linux script: the stage doubles as a play copy.
KEEP=""
if [ -d "$STAGE/assets" ] && { [ -d "$STAGE/assets/game" ] || [ -d "$STAGE/assets/shader_spv" ] \
    || [ -n "$(find "$STAGE/assets/package" -type f -size +1M 2>/dev/null | head -1)" ]; }; then
    KEEP=$OUT/$NAME.assets.keep
    [ -e "$KEEP" ] && fail "leftover $KEEP exists — resolve by hand"
    mv "$STAGE/assets" "$KEEP"
    echo "    preserving play-copy assets ($(ls "$KEEP" | tr '\n' ' '))"
fi

echo "==> staging $STAGE"
rm -rf "$STAGE"
MACOS=$APP/Contents/MacOS
LIB=$APP/Contents/Frameworks
RES=$APP/Contents/Resources
mkdir -p "$LIB" "$APP/Contents/Resources/vulkan/icd.d" "$STAGE/assets/package" "$STAGE/tools"
cp "$BUILD/cw_runtime" "$MACOS/"
strip -S -x "$MACOS/cw_runtime"

echo "==> bundling libraries"
bundle() { # $1 = source dylib (symlinks resolved), $2 = the name to ship it under
    cp -L "$1" "$LIB/$2"
    chmod u+w "$LIB/$2"
    # A universal (x86_64 + arm64) library carries a slice no Apple Silicon player runs;
    # DXC's is ~50% of the download. Keep the arm64 slice only.
    if lipo -archs "$LIB/$2" 2>/dev/null | grep -q x86_64; then
        lipo -thin arm64 "$LIB/$2" -output "$LIB/$2.thin" && mv "$LIB/$2.thin" "$LIB/$2"
    fi
    install_name_tool -id "@rpath/$2" "$LIB/$2" 2>/dev/null
    printf '    %-28s %8s KB\n' "$2" "$(( $(stat -f%z "$LIB/$2") / 1024 ))"
}
bundle "$SDL2PFX/lib/libSDL2-2.0.0.dylib" libSDL2-2.0.0.dylib
bundle "$FFPFX/lib/libavcodec.62.dylib" libavcodec.62.dylib
bundle "$FFPFX/lib/libavutil.60.dylib" libavutil.60.dylib
bundle "$BREW/lib/libvulkan.1.dylib" libvulkan.1.dylib
bundle "$BREW/lib/libMoltenVK.dylib" libMoltenVK.dylib
bundle "$DXC_SRC" libdxcompiler.dylib
strings "$LIB/libSDL2-2.0.0.dylib" | grep -q libSDL3 && fail "the bundled libSDL2 is sdl2-compat"

# Point every reference at @rpath, then give the executable its rpath.
relink() { # $1 = Mach-O to rewrite
    otool -L "$1" | tail -n +2 | awk '{print $1}' | while read -r dep; do
        base=$(basename "$dep")
        case "$base" in
            libSDL2-2.0.0.dylib|libavcodec.62.dylib|libavutil.60.dylib|libvulkan.1.dylib|libMoltenVK.dylib)
                [ "$dep" = "@rpath/$base" ] || install_name_tool -change "$dep" "@rpath/$base" "$1" ;;
        esac
    done
}
relink "$MACOS/cw_runtime"
for f in "$LIB"/*.dylib; do relink "$f"; done
# Drop any build-machine rpath (absolute) and add ours.
otool -l "$MACOS/cw_runtime" | awk '/LC_RPATH/ {r=1} r && /path / {print $2; r=0}' | sort -u | while read -r rp; do
    install_name_tool -delete_rpath "$rp" "$MACOS/cw_runtime"
done
install_name_tool -add_rpath "@executable_path/../Frameworks" "$MACOS/cw_runtime"
for f in "$LIB"/*.dylib; do
    otool -l "$f" | awk '/LC_RPATH/ {r=1} r && /path / {print $2; r=0}' | sort -u | while read -r rp; do
        install_name_tool -delete_rpath "$rp" "$f"
    done
    install_name_tool -add_rpath "@loader_path" "$f" 2>/dev/null || true
done

# The gate on the rewrite: no absolute reference may survive outside /usr/lib and
# /System — each one is a library that exists on this Mac and on no player's.
LEAK=$(for f in "$MACOS/cw_runtime" "$LIB"/*.dylib; do
    otool -L "$f" | tail -n +2 | awk '{print $1}' \
        | grep -v -E '^(@rpath|@loader_path|@executable_path|/usr/lib/|/System/)' \
        | sed "s|^|$(basename "$f"): |" || true   # grep -v exits 1 on a clean library
done)
[ -z "$LEAK" ] || fail "references to build-machine libraries survive the rewrite:
$LEAK"

# MoltenVK's ICD manifest, relative to the manifest's own directory.
cat > "$APP/Contents/Resources/vulkan/icd.d/MoltenVK_icd.json" <<'JSON'
{
    "file_format_version": "1.0.0",
    "ICD": {
        "library_path": "../../../Frameworks/libMoltenVK.dylib",
        "api_version": "1.4.0",
        "is_portability_driver": true
    }
}
JSON

echo "==> data files"
python3 - "$ROOT/tools/release/prewarm.keys" <<'PY' || fail "prewarm.keys failed its header check"
import struct, sys
d = open(sys.argv[1], 'rb').read()
m, v, n = struct.unpack_from('<III', d, 0)
assert m == 0x5750435A and v == 1 and n > 0 and len(d) == 12 + n * 56, (hex(m), v, n, len(d))
print(f"    prewarm.keys                 {n} keys")
PY
cp "$ROOT/tools/release/prewarm.keys" "$ROOT/tools/release/vs_recipes.bin" "$RES/"
CHIPS=$(ls "$ROOT/tools/release/kbm_chips"/*.dxt | wc -l | tr -d ' ')
[ "$CHIPS" -eq 26 ] || fail "tools/release/kbm_chips has $CHIPS of 26 chip blobs"
mkdir -p "$RES/kbm_chips" && cp "$ROOT/tools/release/kbm_chips"/*.dxt "$RES/kbm_chips/"
printf 'CW_VKDRAW=1\nCW_LAUNCHER=1\n' > "$RES/cw_defaults.env"

cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key><string>cw_runtime</string>
    <key>CFBundleIdentifier</key><string>io.github.wivi514.casezerorecomp</string>
    <key>CFBundleName</key><string>Case West Recomp</string>
    <key>CFBundleDisplayName</key><string>Dead Rising 2: Case West (Recomp)</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>1.1.3</string>
    <key>CFBundleVersion</key><string>1.1.3</string>
    <key>LSMinimumSystemVersion</key><string>13.0</string>
    <key>LSArchitecturePriority</key><array><string>arm64</string></array>
    <key>NSHighResolutionCapable</key><true/>
    <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
    <key>GCSupportsControllerUserInteraction</key><true/>
</dict>
</plist>
PLIST

cp "$ROOT/tools/extract_stfs.py" "$STAGE/tools/"
cat > "$STAGE/assets/package/PUT_YOUR_GAME_HERE.txt" <<'TXT'
Put your own copy of the Dead Rising 2: Case West XBLA package in this directory.
It is the file your Xbox 360 downloaded. On the console's storage it lives at
    Content/0000000000000000/58410A8D/000D0000/<a long hash, no file extension>
and it is about 825 MB. Copying the whole 58410A8D folder in here works too — the
runtime looks recursively.

This build ships no game data and cannot supply it. Nothing else goes in this
directory; the unpacked files are written to ../game/ on first run.

Keep this assets folder next to CaseWestRecomp.app: the app finds it by looking in the
folder it sits in.
TXT
cp "$ROOT/LICENSE" "$STAGE/"
cp "${CW_PKG_README:-$ROOT/tools/release/README.md}" "$STAGE/README.md"

mkdir -p "$RES/licenses"
echo "==> THIRD_PARTY.md"
{
    echo "# Third-party components in this build"
    echo
    echo "Generated by \`tools/release_package_macos.sh\`. Do not edit by hand."
    echo
    echo '| component | licence | how it is here |'
    echo '|---|---|---|'
    echo '| XenonRecomp / XenosRecomp (hedge-dev) | MIT | the recompiled image and the translated shaders are their output |'
    echo '| SDL2 | zlib | `CaseWestRecomp.app/Contents/Frameworks/` |'
    echo '| ffmpeg — libavcodec, libavutil | LGPL 2.1 or later | `.../Frameworks/`; see below |'
    echo '| Vulkan loader | Apache 2.0 | `.../Frameworks/libvulkan.1.dylib` |'
    echo '| MoltenVK | Apache 2.0 | `.../Frameworks/libMoltenVK.dylib`, the Vulkan driver over Metal |'
    echo '| DirectX Shader Compiler (DXC) | University of Illinois/NCSA | `.../Frameworks/libdxcompiler.dylib`, loaded at run time to translate shaders |'
    echo '| o1heap | MIT | compiled in |'
    echo '| SIMDe | MIT | compiled in |'
    echo '| miniz | public domain / unlicense | compiled in |'
    echo '| XenonLive client library (libxlive) | see the XenonLive repository | compiled in |'
    echo '| curl (libcurl) | curl licence | the copy macOS ships (`/usr/lib/libcurl.4.dylib`) |'
    echo
    echo '## ffmpeg (LGPL)'
    echo
    echo 'Built from unmodified upstream ffmpeg 8.1.2 with GPL and non-free components'
    echo 'disabled, configured for the two XMA decoders only (the flags of'
    echo '`tools/build_ffmpeg_lgpl.sh`). Dynamically linked and replaceable.'
    if [ -f "$FFTAR" ]; then
        echo
        echo '```'
        echo 'https://ffmpeg.org/releases/ffmpeg-8.1.2.tar.xz'
        shasum -a 256 "$FFTAR" | awk '{print "sha256 " $1}'
        echo '```'
    fi
    echo
    echo '## Libraries the executable links'
    echo
    echo '```'
    otool -L "$MACOS/cw_runtime" | tail -n +2 | sed 's/^\t//'
    echo '```'
} > "$STAGE/THIRD_PARTY.md"
[ -f "$(dirname "$FFTAR")/ffmpeg-8.1.2/COPYING.LGPLv2.1" ] \
    && cp "$(dirname "$FFTAR")/ffmpeg-8.1.2/COPYING.LGPLv2.1" "$RES/licenses/LICENSE.ffmpeg"
[ -f "$SDL2PFX/share/licenses/SDL2/LICENSE.txt" ] && cp "$SDL2PFX/share/licenses/SDL2/LICENSE.txt" "$RES/licenses/LICENSE.SDL2"
[ -f "$ROOT/tools/licenses/LICENSE.DXC.txt" ] && cp "$ROOT/tools/licenses/LICENSE.DXC.txt" "$RES/licenses/LICENSE.DXC"

echo "==> ad-hoc signing (after every edit to the binaries)"
for f in "$LIB"/*.dylib; do codesign --force --sign - --timestamp=none "$f"; done
codesign --force --sign - --timestamp=none "$APP"
codesign --verify --strict --verbose=2 "$APP" 2>&1 | sed 's/^/    /'

echo "==> gate: --smoke on the STAGED executable"
"$MACOS/cw_runtime" --smoke 2>&1 | tail -3 | sed 's/^/    /'
"$MACOS/cw_runtime" --smoke >/dev/null 2>&1 || fail "--smoke failed on the staged executable"

echo "==> archive"
ZIP=$OUT/$NAME-macos-arm64.zip
rm -f "$ZIP"
# ditto, not zip: it keeps the bundle's symlinks, extended attributes and code signature
# exactly as Finder's own "Compress" does.
(cd "$OUT" && ditto -c -k --sequesterRsrc --keepParent "$NAME" "$ZIP")
printf '    %s  %s MB\n' "$(basename "$ZIP")" "$(( $(stat -f%z "$ZIP") / 1048576 ))"
shasum -a 256 "$ZIP" | sed 's/^/    sha256 /'

if [ -n "$KEEP" ]; then
    rm -rf "$STAGE/assets" && mv "$KEEP" "$STAGE/assets"
    echo "    play-copy assets restored into the stage (the archive carries the clean skeleton)"
fi
