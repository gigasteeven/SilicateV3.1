/// ═══════════════════════════════════════════════════════════════════════════
/// Trakines — Main entry point
///
/// Player screen  : XDBot-style layout mode (deco hidden, white silhouettes,
///                  authentic blue BG/ground/line via GD color channels).
/// OBS via Spout2 : full original decor for the level + all UI/indicators.
///
/// Rendering:
///   • Spout ON  → dual render. Each frame: restore full decor + original
///                 colors → render offscreen (sent to OBS) → re-apply layout
///                 (shown to the player).
///   • Spout OFF → single render of the layout (no FPS cost).
///   • Menu/pause/editor → single render, framebuffer mirrored 1:1 to OBS so
///                 every indicator / menu / the editor stays intact on stream.
/// ═══════════════════════════════════════════════════════════════════════════

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelTools.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/binding/GJEffectManager.hpp>
#include <Geode/binding/ColorAction.hpp>

#include "global.hpp"
#include "layout_mode.hpp"
#include "spout/spout_sender.hpp"

using namespace geode::prelude;

// ─── Per-object snapshot (original visuals, used to restore full decor) ───────
struct ObjectSnapshot {
    GameObject* obj      = nullptr;
    int  mainColorID     = 0;
    int  detailColorID   = 0;
    bool detailUsesHSV   = false;
    bool baseUsesHSV     = false;
    bool hasNoGlow       = false;
    bool isHide          = false;
    GLubyte opacity      = 255;
    bool visible         = true;
    bool hide            = false;   // hidden in layout (deco / excluded trigger)
};

// ─── Original color-channel snapshot (to restore full-decor colors for OBS) ───
struct ColorSnapshot {
    int id = 0;
    ccColor3B color   = {255, 255, 255};
    ccColor3B from    = {255, 255, 255};
    ccColor3B to      = {255, 255, 255};
    bool blending     = false;
};

static std::vector<ObjectSnapshot> s_snapshots;
static std::vector<ColorSnapshot>  s_colorSnapshots;
static TrakinesSpout               s_spout;
static cocos2d::CCRenderTexture*   s_renderTex     = nullptr;
static unsigned int                s_lastTexWidth  = 0;
static unsigned int                s_lastTexHeight = 0;

// ═══════════════════════════════════════════════════════════════════════════
// Color-channel helpers (authentic XDBot blue via GJEffectManager)
// ═══════════════════════════════════════════════════════════════════════════

/// Snapshot the current colors of every layout channel so we can restore the
/// real decor colors for the OBS render pass.
static void snapshotLevelColors(PlayLayer* pl) {
    s_colorSnapshots.clear();
    if (!pl || !pl->m_effectManager) return;

    for (const auto& ch : layoutColorChannels) {
        ColorAction* action = pl->m_effectManager->getColorAction(ch.id);
        if (!action) continue;
        ColorSnapshot snap;
        snap.id       = ch.id;
        snap.color    = action->m_color;
        snap.from     = action->m_fromColor;
        snap.to       = action->m_toColor;
        snap.blending = action->m_blending;
        s_colorSnapshots.push_back(snap);
    }
}

/// Force the layout channels to XDBot's blue/white palette.
static void applyLayoutColors(PlayLayer* pl) {
    if (!pl || !pl->m_effectManager) return;

    for (const auto& ch : layoutColorChannels) {
        ColorAction* action = pl->m_effectManager->getColorAction(ch.id);
        if (!action) continue;
        ccColor3B col = ccc3(ch.r, ch.g, ch.b);
        action->m_color     = col;
        action->m_fromColor = col;
        action->m_toColor   = col;
        action->m_blending  = ch.blending;
    }
    pl->m_effectManager->calculateBaseActiveColors();
}

/// Restore the level's original color channels (used for the OBS render pass).
static void restoreLevelColors(PlayLayer* pl) {
    if (!pl || !pl->m_effectManager) return;

    for (const auto& snap : s_colorSnapshots) {
        ColorAction* action = pl->m_effectManager->getColorAction(snap.id);
        if (!action) continue;
        action->m_color     = snap.color;
        action->m_fromColor = snap.from;
        action->m_toColor   = snap.to;
        action->m_blending  = snap.blending;
    }
    pl->m_effectManager->calculateBaseActiveColors();
}

// ═══════════════════════════════════════════════════════════════════════════
// Object visual helpers
// ═══════════════════════════════════════════════════════════════════════════

/// Restore every tracked object to its original (full-decor) state.
static void restoreFullDecor() {
    for (auto& s : s_snapshots) {
        if (!s.obj) continue;
        s.obj->m_activeMainColorID   = s.mainColorID;
        s.obj->m_activeDetailColorID = s.detailColorID;
        s.obj->m_detailUsesHSV       = s.detailUsesHSV;
        s.obj->m_baseUsesHSV         = s.baseUsesHSV;
        s.obj->m_hasNoGlow           = s.hasNoGlow;
        s.obj->m_isHide              = s.isHide;
        s.obj->setOpacity(s.opacity);
        s.obj->setVisible(s.visible);
    }
}

/// Apply the XDBot layout look: deco/excluded hidden, everything else a clean
/// white silhouette (matches XDBot's deco-removal result visually).
static void applyLayoutMode() {
    for (auto& s : s_snapshots) {
        if (!s.obj) continue;

        // Decoration + excluded triggers: hidden (mimics XDBot's deletion).
        if (s.hide) {
            s.obj->setVisible(false);
            continue;
        }

        // Object 2065 (area-color visual) — invisible, marked hidden (XDBot).
        bool is2065 = (s.obj->m_objectID == 2065);

        // Gameplay/solid objects → white silhouette, no glow, no HSV (XDBot).
        s.obj->m_activeMainColorID   = -1;
        s.obj->m_activeDetailColorID = -1;
        s.obj->m_detailUsesHSV       = false;
        s.obj->m_baseUsesHSV         = false;
        s.obj->m_hasNoGlow           = true;
        s.obj->m_isHide              = is2065;
        s.obj->setOpacity(is2065 ? 0 : 255);
        s.obj->setVisible(!is2065);
    }
}

/// Ensure the offscreen render texture matches the current window size.
static void ensureRenderTexture(unsigned int w, unsigned int h) {
    if (s_renderTex && s_lastTexWidth == w && s_lastTexHeight == h) return;

    if (s_renderTex) {
        s_renderTex->release();
        s_renderTex = nullptr;
    }

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    s_renderTex = CCRenderTexture::create(
        static_cast<int>(winSize.width),
        static_cast<int>(winSize.height)
    );

    if (s_renderTex) {
        s_renderTex->retain();
        s_lastTexWidth  = w;
        s_lastTexHeight = h;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Hook: LevelTools — bypass integrity check while layout mode is on (XDBot)
// ═══════════════════════════════════════════════════════════════════════════
class $modify(LevelTools) {
    static bool verifyLevelIntegrity(gd::string levelString, int levelID) {
        if (TrakinesGlobal::get().layoutMode) return true;
        return LevelTools::verifyLevelIntegrity(levelString, levelID);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Hook: PlayLayer — init / addObject / onQuit
// ═══════════════════════════════════════════════════════════════════════════
class $modify(TrakinesPlayLayer, PlayLayer) {

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        auto& g = TrakinesGlobal::get();

        g.layoutMode   = Mod::get()->getSavedValue<bool>("layout_mode", true);
        g.spoutEnabled = Mod::get()->getSavedValue<bool>("spout_enabled", true);

        s_snapshots.clear();
        s_colorSnapshots.clear();

        // Original init triggers addObject for every level object.
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        g.inPlayLayer = true;
        g.playLayer   = this;

        if (g.layoutMode) {
            // Snapshot the real colors first, then paint the layout palette.
            snapshotLevelColors(this);
            applyLayoutMode();
            applyLayoutColors(this);
        }

        if (g.spoutEnabled) {
            auto pixelSize = CCDirector::sharedDirector()->getWinSizeInPixels();
            unsigned int w = static_cast<unsigned int>(pixelSize.width);
            unsigned int h = static_cast<unsigned int>(pixelSize.height);
            s_spout.init("Trakines", w, h);
            ensureRenderTexture(w, h);
        }

        return true;
    }

    void addObject(GameObject* obj) {
        // ALWAYS add the object — even deco — so the OBS pass can render it.
        PlayLayer::addObject(obj);

        auto& g = TrakinesGlobal::get();
        if (!g.layoutMode) return;

        ObjectSnapshot snap;
        snap.obj           = obj;
        snap.mainColorID   = obj->m_activeMainColorID;
        snap.detailColorID = obj->m_activeDetailColorID;
        snap.detailUsesHSV = obj->m_detailUsesHSV;
        snap.baseUsesHSV   = obj->m_baseUsesHSV;
        snap.hasNoGlow     = obj->m_hasNoGlow;
        snap.isHide        = obj->m_isHide;
        snap.opacity       = obj->getOpacity();
        snap.visible       = obj->isVisible();
        // Hidden in layout view: decorations and excluded triggers.
        snap.hide          = decoObjectIDs.contains(obj->m_objectID)
                          || excludedTriggerIDs.contains(obj->m_objectID);

        s_snapshots.push_back(snap);
    }

    void onQuit() {
        auto& g = TrakinesGlobal::get();
        g.inPlayLayer = false;
        g.playLayer   = nullptr;
        s_snapshots.clear();
        s_colorSnapshots.clear();

        s_spout.release();
        if (s_renderTex) {
            s_renderTex->release();
            s_renderTex     = nullptr;
            s_lastTexWidth  = 0;
            s_lastTexHeight = 0;
        }

        PlayLayer::onQuit();
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Hook: CCDirector::drawScene — dual render pipeline
// ═══════════════════════════════════════════════════════════════════════════
class $modify(TrakinesDirector, CCDirector) {

    void drawScene() {
        auto& g = TrakinesGlobal::get();

        bool dualRender = g.inPlayLayer
                       && g.layoutMode
                       && g.spoutEnabled
                       && s_spout.isInitialized()
                       && s_renderTex != nullptr
                       && g.playLayer != nullptr
                       && !s_snapshots.empty();

        if (dualRender) {
            // ── Pass 1: full-decor render → OBS ────────────────────────────
            restoreFullDecor();
            restoreLevelColors(g.playLayer);

            s_renderTex->beginWithClear(0.0f, 0.0f, 0.0f, 1.0f);
            if (m_pRunningScene) {
                m_pRunningScene->visit();
            }
            s_renderTex->end();

            GLuint texID = s_renderTex->getSprite()->getTexture()->getName();
            auto pixelSize = CCDirector::sharedDirector()->getWinSizeInPixels();
            unsigned int w = static_cast<unsigned int>(pixelSize.width);
            unsigned int h = static_cast<unsigned int>(pixelSize.height);
            s_spout.sendTexture(texID, w, h);

            // ── Pass 2: layout render → player screen ──────────────────────
            applyLayoutMode();
            applyLayoutColors(g.playLayer);

            CCDirector::drawScene();

        } else {
            // ── Single render (menu / pause / editor / layout or spout off) ─
            CCDirector::drawScene();

            // Mirror the screen 1:1 to OBS (indicators / menus / editor intact)
            if (g.spoutEnabled && s_spout.isInitialized()) {
                auto pixelSize = CCDirector::sharedDirector()->getWinSizeInPixels();
                unsigned int w = static_cast<unsigned int>(pixelSize.width);
                unsigned int h = static_cast<unsigned int>(pixelSize.height);
                s_spout.sendFramebuffer(w, h);
            }
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Hook: Keyboard — 'U' toggles layout mode live
// ═══════════════════════════════════════════════════════════════════════════
class $modify(CCKeyboardDispatcher) {

    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double delta) {
        constexpr int VK_U = 0x55;  // 'U'

        if (down && !repeat && key == static_cast<enumKeyCodes>(VK_U)) {
            auto& g = TrakinesGlobal::get();

            if (g.inPlayLayer && g.playLayer) {
                g.layoutMode = !g.layoutMode;
                Mod::get()->setSavedValue("layout_mode", g.layoutMode);

                if (g.layoutMode) {
                    snapshotLevelColors(g.playLayer);
                    applyLayoutMode();
                    applyLayoutColors(g.playLayer);
                } else {
                    restoreFullDecor();
                    restoreLevelColors(g.playLayer);
                }

                Notification::create(
                    g.layoutMode ? "Layout Mode: ON" : "Layout Mode: OFF",
                    NotificationIcon::None,
                    1.0f
                )->show();
            }
        }

        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, delta);
    }
};
