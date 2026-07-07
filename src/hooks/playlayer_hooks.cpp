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

// Re-entrancy guard for visit() — defined here, declared extern in mirror_renderer.cpp
bool s_mirrorRendering = false;

// ── PlayLayer hooks ────────────────────────────────────────
class $modify(PlayLayer) {
    // ── addObject: strip colors/glow in Layout Mode ────────
    void addObject(GameObject* obj) {
        if (!TrakinesGlobal::get().layoutMode)
            return PlayLayer::addObject(obj);

        if (excludedTriggerIDs.contains(obj->m_objectID))
            return;

        PlayLayer::addObject(obj);

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

        g.originalLevelString = level->m_levelString;

        if (g.layoutMode) {
            level->m_levelString = LayoutMode::getModifiedString(level->m_levelString);
        }

        if (!PlayLayer::init(level, b1, b2)) {
            if (g.layoutMode)
                level->m_levelString = g.originalLevelString;
            return false;
        }

        if (g.layoutMode)
            level->m_levelString = g.originalLevelString;

        g.inLevel = true;
        if (g.spoutEnabled) {
            g.mirrorRenderer.setLayoutModeFlag(&g.layoutMode);
            g.mirrorRenderer.init(g.mirrorWidth, g.mirrorHeight, g.spoutName.c_str());
        }

        return true;
    }

    // ── visit: called every frame by the scene graph ───────
    // This is MORE reliable than draw() — visit() is always
    // called for every CCNode in the scene graph traversal.
    void visit() {
        PlayLayer::visit();

        // Re-entrancy guard — prevent infinite recursion when
        // renderAndSend() calls scene->visit() internally
        if (s_mirrorRendering) return;

        auto& g = TrakinesGlobal::get();
        if (g.spoutEnabled && g.inLevel && g.mirrorRenderer.isReady()) {
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
