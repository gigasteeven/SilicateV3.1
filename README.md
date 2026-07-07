# Trakines

A Geode mod for Geometry Dash 2.2081 that lets you play with **Layout Mode** while OBS captures the **normal (non-layout) render** via **Spout2**.

## How it works

- **Layout Mode** (ported from XDBotFork): Simplifies the level by removing decorative objects, stripping colors, and keeping only solid hitboxes. The player sees a clean layout.
- **Mirror Renderer**: Re-renders the same scene **without** Layout Mode into an FBO, then sends the texture to Spout2 via GPU-to-GPU sharing (no CPU copy).
- **Spout2**: OBS captures the Spout2 sender as a video source. The viewer sees the full normal level with all decorations and colors.

## Setup

### Prerequisites
- Geometry Dash 2.2081 (Windows)
- Geode Loader 5.7.1+
- OBS Studio with [Spout2 plugin](https://github.com/Off-World-Live/obs-spout2-plugin)

### Installation
1. Download `kaiser.trakines.geode` from [Releases](../../releases)
2. Place it in your `geode/mods/` folder
3. Launch Geometry Dash

### OBS Configuration
1. Install the Spout2 plugin for OBS
2. Add a **Spout2 Capture** source to your scene
3. Select the sender name **"Trakines"** (or whatever you set in mod settings)

## Settings

| Setting | Default | Description |
|---------|---------|-------------|
| Layout Mode | ON | Enables Layout Mode for gameplay |
| Spout2 Output | ON | Sends normal render to OBS via Spout2 |
| Spout Sender Name | "Trakines" | Name OBS will look for |
| Mirror Resolution | 1920x1080 | Resolution of the Spout2 output |
| Mirror FPS | 60 | Throttled render FPS for Spout2 output |

## Building

The mod builds via GitHub Actions using `geode-sdk/build-geode-mod`. Spout2 SDK is statically linked (no external DLLs needed).

## Credits

- Layout Mode logic: [XDBotFork](https://github.com/NakoMellia/XDBotFork) by Zilko & Camellia
- Spout2 SDK: [leadedge/Spout2](https://github.com/leadedge/Spout2) by Lynn Jarvis
- Mod developer: kaiser wilgeim
