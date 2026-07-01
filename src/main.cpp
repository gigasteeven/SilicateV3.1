/// ═══════════════════════════════════════════════════════════════════════════
/// Trakines — Main entry point
///
/// Player screen  : XDBot-style layout (deco hidden, white silhouettes, blue
///                  background / ground / line).
/// OBS via Spout2 : the ORIGINAL decorated level, rendered into a dedicated
///                  offscreen texture that the layout pass never touches — so
///                  OBS always shows the real, fully-decorated level.
///
/// "Cache the decorated level" (your idea): every original object stays loaded
/// for the whole session; we render that original scene into an offscreen
/// texture each frame for OBS, and paint the layout only onto the player screen.
///
/// Per frame while playing (layout + Spout on) — 3 GPU passes (RTX 3090):
///   1. CCDirector::drawScene()        → advance game logic + render (original).
///   2. Render original scene → RT     → Spout SendTexture(RT) → OBS (original).
///   3. Apply layout transiently, clear, re-visit scene → player sees layout.
///   4. Restore the original state.
///
/// Menu / pause / editor: single render mirrored 1:1 to OBS (SendFbo), and the
/// Spout sender is created globally so it works outside gameplay too.
/// ═══════════════════════════════════════════════════════════════════════════

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelTools.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/binding/GJGroundLayer.hpp>

#include "global.hpp"
#include "layout_mode.hpp"
#include "spout/spout_sender.hpp"

using namespace geode::prelude;

// ─── XDBot layout palette ─────────────────────────────────────────────────────
static const ccColor3B kBgBlue     = {40, 125, 255};
static const ccColor3B kGroundBlue = {0, 102, 255};
static const ccColor3B kLineWhite  = {255, 255, 255};

// ─── Per-object snapshot ──────────────────────────────────────────────────────
struct ObjectSnapshot {
    GameObject* obj      = nullptr;
    bool hasNoGlow       = false;
    bool visible         = true;
    bool hide            = false;   // deco / excluded trigger — hidden in layout
    ccColor3B liveColor  = {255, 255, 255};
};

// ─── Background/ground color snapshot ─────────────────────────────────────────
struct BgSnapshot {
    bool valid = false;
    ccColor3B bg   = {255, 255, 255};
    ccColor3B g1   = {255, 255, 255};
    ccColor3B g2   = {255, 255, 255};
    ccColor3B line = {255, 255, 255};
    ccColor3B g1b  = {255, 255, 255};
    ccColor3B g2b  = {255, 255, 255};
    ccColor3B lineb= {255, 255, 255};
};

static std::vector<ObjectSnapshot> s_snapshots;
static BgSnapshot                  s_bg;
static TrakinesSpout               s_spout;
static bool                        s_spoutReady   = false;
static cocos2d::CCRenderTexture*   s_renderTex    = nullptr;
static float                       s_rtW          = 0.f;
static float                       s_rtH          = 0.f;

// ═══════════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════════

static void frameSizePixels(unsigned int& w, unsigned int& h) {
    auto view = CCDirector::sharedDirector()->getOpenGLView();
    if (view) {
        auto fs = view->getFrameSize();
        w = static_cast<unsigned int>(fs.width);
        h = static_cast<unsigned int>(fs.height);
    } else {
        auto ps = CCDirector::sharedDirector()->getWinSizeInPixels();
        w = static_cast<unsigned int>(ps.width);
        h = static_cast<unsigned int>(ps.height);
    }
}

/// Create/resize the offscreen texture used to render the ORIGINAL level.
static void ensureRenderTexture() {
    auto win = CCDirector::sharedDirector()->getWinSize();
    if (s_renderTex && s_rtW == win.width && s_rtH == win.height) return;

    if (s_renderTex) { s_renderTex->release(); s_renderTex = nullptr; }

    s_renderTex = CCRenderTexture::create(
        static_cast<int>(win.width), static_cast<int>(win.height));
    if (s_renderTex) {
        s_renderTex->retain();
        s_rtW = win.width;
        s_rtH = win.height;
    }
}

static void colorGroundLayout(GJGroundLayer* gl, ccColor3B& sg1, ccColor3B& sg2, ccColor3B& sln) {
    if (!gl) return;
    if (gl->m_ground1Sprite) { sg1 = gl->m_ground1Sprite->getColor(); gl->m_ground1Sprite->setColor(kGroundBlue); }
    if (gl->m_ground2Sprite) { sg2 = gl->m_ground2Sprite->getColor(); gl->m_ground2Sprite->setColor(kGroundBlue); }
    if (gl->m_lineSprite)    { sln = gl->m_lineSprite->getColor();    gl->m_lineSprite->setColor(kLineWhite); }
}

static void colorGroundRestore(GJGroundLayer* gl, const ccColor3B& sg1, const ccColor3B& sg2, const ccColor3B& sln) {
    if (!gl) return;
    if (gl->m_ground1Sprite) gl->m_ground1Sprite->setColor(sg1);
    if (gl->m_ground2Sprite) gl->m_ground2Sprite->setColor(sg2);
    if (gl->m_lineSprite)    gl->m_lineSprite->setColor(sln);
}

/// Turn the currently-original scene into the XDBot layout look. Sprite colors
/// are set DIRECTLY so the change is visible for the immediate re-visit.
static void applyLayout(PlayLayer* pl) {
    for (auto& s : s_snapshots) {
        if (!s.obj) continue;
        if (s.hide) { s.obj->setVisible(false); continue; }
        if (s.obj->m_objectID == 2065) { s.obj->setVisible(false); continue; }

        s.liveColor = s.obj->getColor();
        s.obj->setColor(kLineWhite);
        s.obj->m_hasNoGlow = true;
    }

    s_bg = BgSnapshot{};
    if (pl->m_background) { s_bg.bg = pl->m_background->getColor(); pl->m_background->setColor(kBgBlue); }
    colorGroundLayout(pl->m_groundLayer,  s_bg.g1,  s_bg.g2,  s_bg.line);
    colorGroundLayout(pl->m_groundLayer2, s_bg.g1b, s_bg.g2b, s_bg.lineb);
    s_bg.valid = true;
}

/// Restore the true decor so the OBS texture keeps rendering the real level.
static void restoreOriginal(PlayLayer* pl) {
    for (auto& s : s_snapshots) {
        if (!s.obj) continue;
        if (s.hide) { s.obj->setVisible(s.visible); continue; }
        if (s.obj->m_objectID == 2065) { s.obj->setVisible(s.visible); continue; }

        s.obj->setColor(s.liveColor);
        s.obj->m_hasNoGlow = s.hasNoGlow;
    }

    if (!s_bg.valid) return;
    if (pl->m_background) pl->m_background->setColor(s_bg.bg);
    colorGroundRestore(pl->m_groundLayer,  s_bg.g1,  s_bg.g2,  s_bg.line);
    colorGroundRestore(pl->m_groundLayer2, s_bg.g1b, s_bg.g2b, s_bg.lineb);
    s_bg.valid = false;
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
        s_bg = BgSnapshot{};

        // Reserve up-front (levels can have tens of thousands of objects).
        s_snapshots.reserve(level && level->m_objectCount.value() > 0
                                ? static_cast<size_t>(level->m_objectCount.value())
                                : 4096);

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        g.inPlayLayer = true;
        g.playLayer   = this;
        return true;
    }

    void addObject(GameObject* obj) {
        // ALWAYS create the object — this is the "cached" original level that
        // OBS renders. Nothing is deleted, so the decor is always available.
        PlayLayer::addObject(obj);

        auto& g = TrakinesGlobal::get();
        if (!g.layoutMode) return;

        ObjectSnapshot snap;
        snap.obj       = obj;
        snap.hasNoGlow = obj->m_hasNoGlow;
        snap.visible   = obj->isVisible();
        snap.hide      = decoObjectIDs.contains(obj->m_objectID)
                      || excludedTriggerIDs.contains(obj->m_objectID);
        s_snapshots.push_back(snap);
    }

    void onQuit() {
        auto& g = TrakinesGlobal::get();
        g.inPlayLayer = false;
        g.playLayer   = nullptr;
        s_snapshots.clear();
        s_bg = BgSnapshot{};
        PlayLayer::onQuit();
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Hook: CCDirector::drawScene — original → OBS (offscreen), layout → screen
// ═══════════════════════════════════════════════════════════════════════════
class $modify(TrakinesDirector, CCDirector) {

    void drawScene() {
        auto& g = TrakinesGlobal::get();
        PlayLayer* pl = g.playLayer;

        // Create the Spout sender globally so menu / editor also stream.
        if (g.spoutEnabled && !s_spoutReady) {
            unsigned int w, h; frameSizePixels(w, h);
            s_spoutReady = s_spout.init("Trakines", w, h);
        }
        bool spout = g.spoutEnabled && s_spout.isInitialized();

        bool layoutActive = g.inPlayLayer && g.layoutMode && pl != nullptr && !s_snapshots.empty();

        if (!layoutActive) {
            // Menu / pause / editor / layout off — one render, mirror to OBS 1:1.
            CCDirector::drawScene();
            if (spout) {
                unsigned int w, h; frameSizePixels(w, h);
                s_spout.sendFramebuffer(w, h);
            }
            return;
        }

        // 1) Advance game logic + render (state is original).
        CCDirector::drawScene();

        // 2) Render the ORIGINAL scene into the offscreen texture and send that
        //    dedicated texture to OBS — never overwritten by the layout pass.
        if (spout) {
            ensureRenderTexture();
            if (s_renderTex && m_pRunningScene) {
                s_renderTex->beginWithClear(0.f, 0.f, 0.f, 1.f);
                m_pRunningScene->visit();
                s_renderTex->end();

                auto tex = s_renderTex->getSprite()->getTexture();
                if (tex) {
                    GLuint id = tex->getName();
                    auto px   = tex->getContentSizeInPixels();
                    s_spout.sendTexture(id,
                        static_cast<unsigned int>(px.width),
                        static_cast<unsigned int>(px.height));
                }
            }
        }

        // 3) Paint the layout over the screen (visual only — no logic advance).
        applyLayout(pl);
        glClearColor(kBgBlue.r / 255.f, kBgBlue.g / 255.f, kBgBlue.b / 255.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (m_pRunningScene) m_pRunningScene->visit();

        // 4) Restore the original decor for the next frame's OBS render.
        restoreOriginal(pl);
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
                if (!g.layoutMode) restoreOriginal(g.playLayer);

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
