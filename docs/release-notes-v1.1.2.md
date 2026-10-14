> [!WARNING]
> ## 🌐 WANT ONLINE FEATURES?
>
> **Launch the game through the [XenonLive Launcher](https://github.com/wivi514/XenonLive_Launcher).**
>
> XenonLive is required for:
> - 🎮 Online / co-op
> - 🏆 Achievements
> - 👤 Gamertag
> - 👥 Friends
>
> **If you start the game any other way, it will run in OFFLINE MODE only.**
> Single-player will still work normally.

# Dead Rising 2: Case West — Native PC Port v1.1.2

**v1.1.2 is the maintainer's official final build.** The original maintainer has
stopped active development, but community forks, improvements, and pull requests
are encouraged. Issues and pull requests may go unanswered.

This release brings the project to its final planned state across Windows, Linux,
Steam Deck, and Apple Silicon Mac. It includes the following since v1.1.1:

### Platforms and reliability

- Apple Silicon macOS build, packaged as a self-contained app bundle.
- Dedicated Steam Deck build with Deck-oriented defaults and no desktop launcher.
- Improved Windows and macOS startup, Vulkan setup, and crash/diagnostic reporting.

### Rendering and settings

- Renderer fixes for widescreen fills, depth precision, resolve write-back, cube
  snapshots, and shader/texture resource handling.
- Zombie detail and shadow-bias controls in the visuals settings.
- Static world detail uses the game's original 1.0 scale and is no longer an
  adjustable setting.

### Modding and diagnostics

- Expanded support for community content, including larger zombie and shader
  pools, audio substitutions, and a `game_mods/` overlay.
- Additional frame, vertex-shader, and machine diagnostics for troubleshooting.

## Downloads

- Windows: `CaseWestRecomp-windows-x86_64.zip`
- Linux: `CaseWestRecomp-linux-x86_64.tar.zst` or
  `CaseWestRecomp-linux-x86_64.AppImage`
- Steam Deck: `CaseWestRecomp-steamdeck-x86_64.tar.gz`
- Apple Silicon Mac: `CaseWestRecomp-macos-arm64.zip`

The downloads contain no game data. You must supply your own Dead Rising 2: Case
West XBLA package. See the [README](../README.md) for setup instructions,
requirements, known issues, and troubleshooting. The macOS app is not notarized;
the README explains the first-launch quarantine step.

Two-player online co-op requires starting the game from the
[XenonLive launcher](https://github.com/wivi514/XenonLive_Launcher). A direct
launch remains offline single-player.

## Validation

- Release and matched development builds passed the Linux `.text` identity check
  for both desktop and Steam Deck variants.
- Linux AppImage self-check and Windows, macOS, Linux, and Steam Deck smoke checks
  passed.
- The maintainer confirmed game boots on their Windows, Linux, and macOS systems.
- SHA-256 checksums for all five downloads are included as `SHA256SUMS`.
