#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelTools.hpp>
#include "../global.hpp"
#include "../layout/layout_mode.hpp"

using namespace geode::prelude;

class $modify(LevelTools) {
    static bool verifyLevelIntegrity(gd::string v1, int v2) {
        if (TrakinesGlobal::get().layoutMode) return true;
        return LevelTools::verifyLevelIntegrity(v1, v2);
    }
};

bool s_mirrorRendering = false;

class $modify(PlayLayer) {
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

    // ── update: THIS HOOK DEFINITELY WORKS (Layout Mode uses it) ──
    // Call renderAndSend() from here. The GL context is valid during
    // the game loop. We render the scene as-is (previous frame state)
    // which is fine — 1 frame lag at 60fps = 16ms, imperceptible.
    void update(float dt) {
        PlayLayer::update(dt);

        auto& g = TrakinesGlobal::get();
        if (g.spoutEnabled && g.inLevel && g.mirrorRenderer.isReady()) {
            g.mirrorRenderer.renderAndSend();
        }
    }

    void onExit() {
        PlayLayer::onExit();

        auto& g = TrakinesGlobal::get();
        if (g.inLevel) {
            g.inLevel = false;
            g.mirrorRenderer.cleanup();
        }
    }
};
