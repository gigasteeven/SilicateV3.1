# Trakines

A Geometry Dash 2.2081 mod (Geode SDK 5.7.1) that provides a dual-output rendering pipeline:

- **Player sees**: Layout mode — blue background, white silhouettes (clean gameplay view)
- **OBS sees**: Full decorated level via Spout2 texture sharing

## Features

- **Layout Mode** (from XDBot): Blue background + white object silhouettes for clean gameplay
- **Spout2 Output**: Full-decor frames sent to OBS via zero-copy GPU texture sharing
- **Menu/Pause/Editor**: Mod inactive — Spout2 mirrors the screen 1:1
- **Hotkey**: Press `U` to toggle layout mode on/off
- **GPU Optimized**: Spout2 uses NVIDIA DX/GL interop for zero-copy texture sharing

## Setup for OBS

1. Install the [Spout2 OBS Plugin](https://github.com/Off-World-Live/obs-spout2-plugin)
2. Add a **Spout2 Capture** source in OBS
3. Select sender name: **Trakines**
4. Play GD with the mod enabled — OBS receives the full decorated level

## Building

```bash
# Requires Geode CLI installed
geode build
```

Or push to GitHub — the Actions workflow builds automatically.

## Credits

- Layout mode logic adapted from [XDBot](https://github.com/NakoMellia/XDBotFork) by Zilko & Camellia
- [Spout2](https://github.com/leadedge/Spout2) by Lynn Jarvis
- Built with [Geode SDK](https://geode-sdk.org/)
