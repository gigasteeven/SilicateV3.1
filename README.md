# Silicate

This is the main repository for the Silicate bot. 

## Versioning

Currently the bot is in **alpha**, which means we use `1.0.0-alpha.XX` builds.
When releasing a beta build, use `1.0.0-beta.XX` builds.

Upon release, we will migrate to a different versioning system - preferably something like `2026.01-01` for the first build in January 2026.

## Structure

```
src/
    assist/ - Assist features, such as autoclicker or hitboxes.
    bot/ - Core bot components.
    checkpoint/ - The practice fix.
    hooks/ - All of the hooks Silicate uses. Interacts with core game logic.
    label/ - The label system for displaying overlays.
    physics/ - Geometry Dash physics decomp for trajectory.
    render/ - The renderer and DSP recorder.
    replay/ - The replay system.
    settings/ - The bot's settings module.
    shared/ - Shared parts of the code, such as keybind logic.
    trajectory/ - Simulation/trajectory logic.
    ui/ - The interface.
    util/ - Generic utilities, such as midhooking.
lib/
    tabby/ - The UI library, based on ImGUI.
```

## Compiling

1. Clone [`tabby`](https://github.com/silicate-bot/tabby) into lib/tabby. Make sure you're on the `legacy-v1` branch (until we migrate lol).
2. Run `build.bat` or `build-rel.bat` if you're compiling for public use.

## Contributing

Please use feature branches. Use clang-format for formatting your code (unless it makes it horribly unreadable).
Currently we do not have automated testing. Please test the features you're implementing/changing and related components before releasing builds.
