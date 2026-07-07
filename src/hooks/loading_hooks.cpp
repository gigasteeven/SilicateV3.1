#include <Geode/Geode.hpp>
#include "../global.hpp"

// Loading hooks — reserved for preloading textures during level load.
// Currently the mirror renderer initializes in PlayLayer::init which
// is sufficient. This file is for future optimization (preloading
// textures during the loading screen to avoid first-frame stutters).

using namespace geode::prelude;

// Future: hook LevelLoadingLayer to preload textures for both
// layout-mode and normal render before PlayLayer::init is called.
