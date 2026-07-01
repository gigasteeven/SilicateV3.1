/// ═══════════════════════════════════════════════════════════════════════════
/// Trakines — Main entry point
/// All Geode hooks: PlayLayer, CCDirector, LevelTools, Keyboard
///
/// Architecture:
///   1. Layout mode applied via addObject hook (from XDBot)
///   2. Each frame: restore originals → render to FBO (Spout2) → reapply layout
///   3. Menu/pause/editor: Spout2 mirrors screen 1:1
/// ═══════════════════════════════════════════════════════════════════════════

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelTools.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

#include "global.hpp"
#include "layout_mode.hpp"
#include "spout/spout_sender.hpp"

using namespace geode::prelude;

// ─── Statics ────────────────────────────────────────────────────────────────

/// Per-object snapshot: stores original visual properties so we can restore
/// them for the Spout2 full-decor render pass and reapply layout after.
struct ObjectSnapshot {
    GameObject* obj         = nullptr;
    int  mainColorID        = 0;
    int  detailColorID      = 0;
    bool detailUsesHSV      = false;
    bool baseUsesHSV        = false;
    bool hasNoGlow          = false;
    bool isHide             = false;
    GLubyte opacity         = 255;
    bool visible            = true;
    bool isDeco             = false;   // true if objectID ∈ decoObjectIDs
    bool isExcludedTrigger  = false;   // true if objectID ∈ excludedTriggerIDs
};

static std::vector<ObjectSnapshot>  s_snapshots;
static TrakinesSpout                s_spout;
static cocos2d::CCRenderTexture*    s_renderTex      = nullptr;
static unsigned int                 s_lastTexWidth   = 0;
static unsigned int                 s_lastTexHeight  = 0;

// Layout mode background overlay (blue)
static cocos2d::CCLayerColor*       s_blueBG         = nullptr;
static constexpr int                BLUE_BG_TAG      = 0x54524B; // "TRK"

// ─── Helpers: restore / apply ───────────────────────────────────────────────

/// Restore every tracked object to its original (full-decor) state.
/// Called BEFORE the Spout2 render pass.
static void restoreFullDecor() {
    for (auto& s : s_snapshots) {
        if (!s.obj) continue;
        s.obj->m_activeMainColorID   = s.mainColorID;
        s.obj->m_activeDetailColorID = s.detailColorID;
        s.obj->m_detailUsesHSV       = s.detailUsesHSV;
        s.obj->m_baseUsesHSV         = s.baseUsesHSV;
        s.obj->m_hasNoGlow           = s.hasNoGlow;
        s.obj->setOpacity(s.opacity);
        s.obj->setVisible(s.visible);
        // NOTE: m_isHide is NOT toggled per-frame (it affects gameplay collision)
    }
    // Hide the blue overlay so Spout2 sees the real background
    if (s_blueBG) s_blueBG->setVisible(false);
}

/// Apply layout mode visuals: white silhouettes, hide deco, show blue BG.
/// Called AFTER the Spout2 render pass (this is what the player sees).
static void applyLayoutMode() {
    for (auto& s : s_snapshots) {
        if (!s.obj) continue;

        // Decoration objects and excluded triggers → hide completely
        if (s.isDeco || s.isExcludedTrigger) {
            s.obj->setVisible(false);
            continue;
        }

        // Gameplay objects → white, no glow, no HSV
        s.obj->m_activeMainColorID   = -1;
        s.obj->m_activeDetailColorID = -1;
        s.obj->m_detailUsesHSV       = false;
        s.obj->m_baseUsesHSV         = false;
        s.obj->m_hasNoGlow           = true;

        // Object 2065 (area trigger visual) should be invisible
        bool isAreaTrigger = (s.obj->m_objectID == 2065);
        s.obj->setOpacity(isAreaTrigger ? 0 : 255);
        s.obj->setVisible(!isAreaTrigger);
    }
    // Show blue background overlay for layout mode
    if (s_blueBG) s_blueBG->setVisible(true);
}

/// Ensure the CCRenderTexture matches the current window size (handles resize).
static void ensureRenderTexture(unsigned int w, unsigned int h) {
    if (s_renderTex && s_lastTexWidth == w && s_lastTexHeight == h) return;

    // Release old texture
    if (s_renderTex) {
        s_renderTex->release();
        s_renderTex = nullptr;
    }

    // CCRenderTexture::create takes points; content scale factor handles pixels
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
// Hook: LevelTools — bypass integrity check when layout mode modifies data
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

        // Read settings
        g.layoutMode   = Mod::get()->getSavedValue<bool>("layout_mode", true);
        g.spoutEnabled = Mod::get()->getSavedValue<bool>("spout_enabled", true);

        // Clear previous level's object data
        s_snapshots.clear();
        s_blueBG = nullptr;

        // Call original init (this triggers addObject for every level object)
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        g.inPlayLayer = true;
        g.playLayer   = this;

        if (g.layoutMode) {
            // Create blue background overlay (RGB 40, 125, 255)
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            s_blueBG = CCLayerColor::create(ccc4(40, 125, 255, 255));
            if (s_blueBG) {
                s_blueBG->setContentSize(winSize);
                s_blueBG->setZOrder(-9999);     // behind everything in the game
                s_blueBG->setTag(BLUE_BG_TAG);
                this->addChild(s_blueBG);
            }

            // Apply layout mode visuals to all objects stored during addObject
            applyLayoutMode();
        }

        // Initialize Spout2 sender
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
        // ALWAYS add the object (even deco — we need it for Spout2 full render)
        PlayLayer::addObject(obj);

        auto& g = TrakinesGlobal::get();
        if (!g.layoutMode) return;

        // Snapshot original properties BEFORE any layout modifications
        ObjectSnapshot snap;
        snap.obj              = obj;
        snap.mainColorID      = obj->m_activeMainColorID;
        snap.detailColorID    = obj->m_activeDetailColorID;
        snap.detailUsesHSV    = obj->m_detailUsesHSV;
        snap.baseUsesHSV      = obj->m_baseUsesHSV;
        snap.hasNoGlow        = obj->m_hasNoGlow;
        snap.isHide           = obj->m_isHide;
        snap.opacity          = obj->getOpacity();
        snap.visible          = obj->isVisible();
        snap.isDeco           = decoObjectIDs.contains(obj->m_objectID);
        snap.isExcludedTrigger = excludedTriggerIDs.contains(obj->m_objectID);

        s_snapshots.push_back(snap);

        // Layout mode modifications are applied in bulk after init finishes
        // (see PlayLayer::init hook above calling applyLayoutMode())
    }

    void onQuit() {
        auto& g = TrakinesGlobal::get();
        g.inPlayLayer = false;
        g.playLayer   = nullptr;
        s_snapshots.clear();
        s_blueBG = nullptr;

        // Release Spout2 and render texture
        s_spout.release();
        if (s_renderTex) {
            s_renderTex->release();
            s_renderTex  = nullptr;
            s_lastTexWidth = 0;
            s_lastTexHeight = 0;
        }

        PlayLayer::onQuit();
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Hook: CCDirector::drawScene — dual render pipeline
//
// GAMEPLAY with layout mode + Spout2:
//   1. Restore full-decor state on all objects
//   2. Render the scene to an offscreen CCRenderTexture (full decor)
//   3. Send that texture to Spout2 → OBS gets full decorations
//   4. Reapply layout mode visuals
//   5. Call original drawScene → player sees layout mode on screen
//
// NON-GAMEPLAY or layout mode off:
//   1. Call original drawScene → renders normally
//   2. Send the framebuffer to Spout2 → OBS mirrors screen 1:1
// ═══════════════════════════════════════════════════════════════════════════
class $modify(TrakinesDirector, CCDirector) {

    void drawScene() {
        auto& g = TrakinesGlobal::get();

        bool dualRender = g.inPlayLayer
                       && g.layoutMode
                       && g.spoutEnabled
                       && s_spout.isInitialized()
                       && s_renderTex != nullptr
                       && !s_snapshots.empty();

        if (dualRender) {
            // ── Pass 1: full-decor render for Spout2 ────────────────────

            // Temporarily restore all objects to their original state
            restoreFullDecor();

            // Render the fully-decorated scene into our offscreen texture
            s_renderTex->beginWithClear(0.0f, 0.0f, 0.0f, 1.0f);
            if (m_pRunningScene) {
                m_pRunningScene->visit();
            }
            s_renderTex->end();

            // Send the texture to Spout2 (zero-copy DX/GL interop on NVIDIA)
            GLuint texID = s_renderTex->getSprite()->getTexture()->getName();
            auto pixelSize = CCDirector::sharedDirector()->getWinSizeInPixels();
            unsigned int w = static_cast<unsigned int>(pixelSize.width);
            unsigned int h = static_cast<unsigned int>(pixelSize.height);
            s_spout.sendTexture(texID, w, h);

            // ── Pass 2: layout mode render for player ───────────────────

            // Reapply layout mode (white silhouettes, hide deco, blue bg)
            applyLayoutMode();

            // Normal render to screen — player sees layout mode
            CCDirector::drawScene();

        } else {
            // ── Single render (menu / pause / editor / layout off) ──────

            // Render normally
            CCDirector::drawScene();

            // If Spout2 is active, mirror the framebuffer to OBS
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
// Hook: Keyboard — U key toggles layout mode
// ═══════════════════════════════════════════════════════════════════════════
class $modify(CCKeyboardDispatcher) {

    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat) {
        // 'U' key = virtual key code 0x55 = 85
        constexpr int VK_U = 0x55;

        if (down && !repeat && key == static_cast<enumKeyCodes>(VK_U)) {
            auto& g = TrakinesGlobal::get();

            // Only toggle during gameplay
            if (g.inPlayLayer) {
                g.layoutMode = !g.layoutMode;
                Mod::get()->setSavedValue("layout_mode", g.layoutMode);

                if (g.layoutMode) {
                    applyLayoutMode();
                } else {
                    restoreFullDecor();
                }

                // Brief notification
                Notification::create(
                    g.layoutMode ? "Layout Mode: ON" : "Layout Mode: OFF",
                    NotificationIcon::None,
                    1.0f
                )->show();
            }
        }

        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat);
    }
};
