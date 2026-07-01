#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

/// Global singleton for Trakines mod state
class TrakinesGlobal {
public:
    static TrakinesGlobal& get() {
        static TrakinesGlobal instance;
        return instance;
    }

    /// Whether layout mode is active (blue bg, white silhouettes)
    bool layoutMode = true;

    /// Whether Spout2 output is active
    bool spoutEnabled = true;

    /// True when inside PlayLayer (gameplay)
    bool inPlayLayer = false;

    /// True during the Spout2 offscreen render pass (layout hooks are bypassed)
    bool renderingForSpout = false;

    /// Pointer to active PlayLayer (nullptr when not in gameplay)
    PlayLayer* playLayer = nullptr;

private:
    TrakinesGlobal() = default;
    TrakinesGlobal(const TrakinesGlobal&) = delete;
    TrakinesGlobal& operator=(const TrakinesGlobal&) = delete;
};
