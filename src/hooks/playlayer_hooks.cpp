#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelTools.hpp>
#include "../global.hpp"
#include "../layout/layout_mode.hpp"

using namespace geode::prelude;

// ── LevelTools: bypass integrity check when Layout Mode is on ──
class $modify(LevelTools) {
    static bool verifyLevelIntegrity(gd::string v1, int v2) {
        if (TrakinesGlobal::get().layoutMode) return true;
        return LevelTools::verifyLevelIntegrity(v1, v2);
    }
};

// ── PlayLayer hooks ────────────────────────────────────────
class $modify(PlayLayer) {
    // ── addObject: strip colors/glow in Layout Mode ────────
    void addObject(GameObject* obj) {
        if (!TrakinesGlobal::get().layoutMode)
            return PlayLayer::addObject(obj);

        // Skip excluded triggers
        if (excludedTriggerIDs.contains(obj->m_objectID))
            return;

        PlayLayer::addObject(obj);

        // Strip visual properties
        obj->m_activeMainColorID = -1;
        obj->m_activeDetailColorID = -1;
        obj->m_detailUsesHSV = false;
        obj->m_baseUsesHSV = false;
        obj->m_hasNoGlow = true;
        obj->m_isHide = (obj->m_objectID == 2065);
        obj->setOpacity(obj->m_objectID == 2065 ? 0 : 255);
        obj->setVisible(obj->m_objectID != 2065);
    }

    // ── init: apply Layout Mode transform + init mirror ────
    bool init(GJGameLevel* level, bool b1, bool b2) {
        auto& g = TrakinesGlobal::get();
        g.loadSettings();

        // Save original level string for mirror renderer
        g.originalLevelString = level->m_levelString;

        // Apply Layout Mode transform
        if (g.layoutMode) {
            level->m_levelString = LayoutMode::getModifiedString(level->m_levelString);
        }

        if (!PlayLayer::init(level, b1, b2)) {
            if (g.layoutMode)
                level->m_levelString = g.originalLevelString;
            return false;
        }

        // Restore original level string (so it's not permanently modified)
        if (g.layoutMode)
            level->m_levelString = g.originalLevelString;

        // Initialize mirror renderer + Spout2
        g.inLevel = true;
        if (g.spoutEnabled) {
            g.mirrorRenderer.setLayoutModeFlag(&g.layoutMode);
            g.mirrorRenderer.init(g.mirrorWidth, g.mirrorHeight, g.spoutName.c_str());
        }

        return true;
    }

    // ── update: throttle check only (no GL calls here) ─────
    void update(float dt) {
        PlayLayer::update(dt);

        auto& g = TrakinesGlobal::get();
        if (g.spoutEnabled && g.inLevel) {
            // Just update the throttle timer — actual render happens in draw()
            g.mirrorRenderer.shouldRender(dt);
        }
    }

    // ── draw: render mirror AFTER the screen draw ──────────
    // This is the correct place for GL operations — the GL context
    // is ready and the scene has just been drawn to the screen.
    void draw() {
        // First, let the game draw normally (with Layout Mode)
        PlayLayer::draw();

        auto& g = TrakinesGlobal::get();
        if (g.spoutEnabled && g.inLevel && g.mirrorRenderer.isReady()) {
            // Check if it's time to render a mirror frame (throttled)
            // We use a simple frame counter approach since draw() doesn't get dt
            g.mirrorRenderer.renderAndSend();
        }
    }

    // ── onExit: cleanup when leaving level ─────────────────
    void onExit() {
        PlayLayer::onExit();

        auto& g = TrakinesGlobal::get();
        if (g.inLevel) {
            g.inLevel = false;
            g.mirrorRenderer.cleanup();
        }
    }
};
