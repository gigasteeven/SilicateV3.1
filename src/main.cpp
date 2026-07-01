/// ═══════════════════════════════════════════════════════════════════════════
/// Trakines — Main entry point
///
/// Player screen  : XDBot-style layout (deco hidden, white silhouettes, blue
///                  background / ground / line).
/// OBS via Spout2 : the ORIGINAL decorated level + all UI/indicators, captured
///                  straight from the real framebuffer at the true resolution.
///
/// Per frame while playing (layout + Spout on):
///   1. CCDirector::drawScene()  → renders the ORIGINAL level to the screen and
///      advances game logic exactly once.
///   2. Spout SendFbo(0)         → OBS receives that original frame (real res).
///   3. Apply layout transiently (object colors → white, deco hidden, blue BG)
///      and re-visit the scene → the PLAYER sees the layout.
///   4. Restore the original state so the next frame's step 1 is original again.
///
/// The game state is never permanently modified, so GD's effect manager keeps
/// running normally (no pulsing) and OBS always gets the true level.
/// Menu / pause / editor use a single render mirrored 1:1 to OBS.
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
// `hide`/`visible`/`hasNoGlow` are captured once (static level data). `liveColor`
// / `liveOpacity` are captured fresh every frame right before the layout render
// (so we restore the exact post-effect-manager color for the next OBS frame).
struct ObjectSnapshot {
    GameObject* obj      = nullptr;
    bool hasNoGlow       = false;
    bool visible         = true;
    bool hide            = false;   // deco / excluded trigger — hidden in layout
    ccColor3B liveColor  = {255, 255, 255};
    GLubyte   liveOpacity= 255;
};

// ─── Background/ground color snapshot (restored for the OBS frame) ────────────
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

// ═══════════════════════════════════════════════════════════════════════════
// Layout apply / restore (transient — only around the on-screen re-render)
// ═══════════════════════════════════════════════════════════════════════════

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

/// Turn the currently-original scene into the XDBot layout look. We set sprite
/// colors DIRECTLY (not the logical color ID) so the change is visible for the
/// immediate re-visit — the effect manager already colored the sprites this
/// frame, so a logical-ID change alone would be ignored until next frame.
static void applyLayout(PlayLayer* pl) {
    for (auto& s : s_snapshots) {
        if (!s.obj) continue;

        // Decoration + excluded triggers → hidden (matches XDBot's deletion).
        if (s.hide) { s.obj->setVisible(false); continue; }

        // 2065 (area-color visual) → invisible in layout (XDBot).
        if (s.obj->m_objectID == 2065) { s.obj->setVisible(false); continue; }

        // Gameplay/solid objects → clean white silhouette.
        s.liveColor   = s.obj->getColor();
        s.liveOpacity = s.obj->getOpacity();
        s.obj->setColor(kLineWhite);
        s.obj->m_hasNoGlow = true;
    }

    // Blue background / ground / white line (set sprite colors directly).
    s_bg = BgSnapshot{};
    if (pl->m_background) { s_bg.bg = pl->m_background->getColor(); pl->m_background->setColor(kBgBlue); }
    colorGroundLayout(pl->m_groundLayer,  s_bg.g1,  s_bg.g2,  s_bg.line);
    colorGroundLayout(pl->m_groundLayer2, s_bg.g1b, s_bg.g2b, s_bg.lineb);
    s_bg.valid = true;
}

/// Restore the true decor so the next frame's OBS capture is the real level.
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

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        g.inPlayLayer = true;
        g.playLayer   = this;

        if (g.spoutEnabled) {
            unsigned int w, h;
            frameSizePixels(w, h);
            s_spout.init("Trakines", w, h);
        }

        return true;
    }

    void addObject(GameObject* obj) {
        // ALWAYS create the object so the OBS pass renders the full decor.
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
        s_spout.release();

        PlayLayer::onQuit();
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Hook: CCDirector::drawScene — original → OBS, layout → screen
// ═══════════════════════════════════════════════════════════════════════════
class $modify(TrakinesDirector, CCDirector) {

    void drawScene() {
        auto& g = TrakinesGlobal::get();
        PlayLayer* pl = g.playLayer;

        bool layoutActive = g.inPlayLayer && g.layoutMode && pl != nullptr && !s_snapshots.empty();
        bool spout        = g.spoutEnabled && s_spout.isInitialized();

        if (!layoutActive) {
            // Menu / pause / editor / layout off — single render, mirror 1:1.
            CCDirector::drawScene();
            if (spout) {
                unsigned int w, h; frameSizePixels(w, h);
                s_spout.sendFramebuffer(w, h);
            }
            return;
        }

        // 1) Original render + game logic (state is untouched = original).
        CCDirector::drawScene();

        // 2) Send that original frame to OBS at the true resolution.
        if (spout) {
            unsigned int w, h; frameSizePixels(w, h);
            s_spout.sendFramebuffer(w, h);
        }

        // 3) Draw the layout over the screen (visual only — no logic advance).
        applyLayout(pl);
        glClearColor(kBgBlue.r / 255.f, kBgBlue.g / 255.f, kBgBlue.b / 255.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (m_pRunningScene) m_pRunningScene->visit();

        // 4) Restore the true decor for the next frame's OBS capture.
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

                // If turning layout off, make sure any transient state is undone.
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
