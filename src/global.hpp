#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include "layout/layout_mode.hpp"
#include "mirror/mirror_renderer.hpp"

using namespace geode::prelude;

// ── Global state for Trakines ──────────────────────────────
// Single singleton that holds all shared state.
class TrakinesGlobal {
    TrakinesGlobal() {}

public:
    static auto& get() {
        static TrakinesGlobal instance;
        return instance;
    }

    // ── Settings (loaded from mod settings) ────────────────
    bool layoutMode = false;          // Is Layout Mode active?
    bool spoutEnabled = false;        // Is Spout2 output enabled?
    std::string spoutName = "Trakines";
    unsigned int mirrorWidth = 1920;
    unsigned int mirrorHeight = 1080;
    unsigned int mirrorFps = 60;

    // ── Runtime state ──────────────────────────────────────
    std::string originalLevelString;  // Saved before Layout Mode transform
    bool inLevel = false;             // Are we currently in a PlayLayer?

    // ── Mirror renderer ────────────────────────────────────
    MirrorRenderer mirrorRenderer;

    // ── UI visibility flag ─────────────────────────────────
    // When true, UI elements (CPS, FPS, etc.) are visible on the mirror render.
    // This is the universal flag: any UI element checks this to decide
    // whether to render itself in the mirror pass.
    // Default: true (show all UI on the Spout2 output)
    bool showUIOnMirror = true;

    // ── Load settings from mod.json ────────────────────────
    void loadSettings() {
        auto mod = Mod::get();
        layoutMode = mod->getSettingValue<bool>("enable_layout_mode");
        spoutEnabled = mod->getSettingValue<bool>("enable_spout_output");
        spoutName = mod->getSettingValue<std::string>("spout_sender_name");
        mirrorWidth = mod->getSettingValue<int64_t>("mirror_resolution_w");
        mirrorHeight = mod->getSettingValue<int64_t>("mirror_resolution_h");
        mirrorFps = mod->getSettingValue<int64_t>("mirror_fps");
    }
};
