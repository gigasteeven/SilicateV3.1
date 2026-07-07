#include <Geode/Geode.hpp>
#include "global.hpp"
#include "ui/trakines_menu.hpp"

using namespace geode::prelude;

$execute {
    auto& g = TrakinesGlobal::get();
    g.loadSettings();

    log::info("Trakines v1.0.0 loaded");
    log::info("  Layout Mode: {}", g.layoutMode ? "ON" : "OFF");
    log::info("  Spout2 Output: {}", g.spoutEnabled ? "ON" : "OFF");
    if (g.spoutEnabled) {
        log::info("  Mirror: {}x{} @ {} FPS, Spout name: \"{}\"",
                  g.mirrorWidth, g.mirrorHeight, g.mirrorFps, g.spoutName);
    }
}
