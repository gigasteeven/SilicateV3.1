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
static int s_updateCallCount = 0;
static int s_drawCallCount = 0;

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

        log::info("Trakines: PlayLayer::init done — inLevel={}, spoutEnabled={}, mirrorReady={}",
                  g.inLevel, g.spoutEnabled, g.mirrorRenderer.isReady());

        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        auto& g = TrakinesGlobal::get();

        // Aggressive logging for first 5 update calls
        s_updateCallCount++;
        if (s_updateCallCount <= 5) {
            log::info("Trakines: update() #{} — spoutEnabled={}, inLevel={}, mirrorReady={}",
                      s_updateCallCount, g.spoutEnabled, g.inLevel, g.mirrorRenderer.isReady());
        }

        if (g.spoutEnabled && g.inLevel && g.mirrorRenderer.isReady()) {
            g.mirrorRenderer.renderAndSend();
        }
    }

    // Also hook draw() as a backup — log if it's called
    void draw() {
        PlayLayer::draw();

        auto& g = TrakinesGlobal::get();

        s_drawCallCount++;
        if (s_drawCallCount <= 5) {
            log::info("Trakines: draw() #{} — spoutEnabled={}, inLevel={}, mirrorReady={}",
                      s_drawCallCount, g.spoutEnabled, g.inLevel, g.mirrorRenderer.isReady());
        }

        // Also try rendering from draw() as backup
        if (g.spoutEnabled && g.inLevel && g.mirrorRenderer.isReady() && !s_mirrorRendering) {
            g.mirrorRenderer.renderAndSend();
        }
    }

    void onExit() {
        PlayLayer::onExit();

        auto& g = TrakinesGlobal::get();
        log::info("Trakines: onExit() — inLevel was {}, updateCalls={}, drawCalls={}",
                  g.inLevel, s_updateCallCount, s_drawCallCount);

        if (g.inLevel) {
            g.inLevel = false;
            g.mirrorRenderer.cleanup();
        }

        // Reset counters
        s_updateCallCount = 0;
        s_drawCallCount = 0;
    }
};
