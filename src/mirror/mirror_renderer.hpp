#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include "../spout/spout_sender.hpp"

#ifdef TRAKINES_SPOUT_ENABLED
#include <windows.h>
#include <GL/gl.h>
#endif

using namespace geode::prelude;

// ── Mirror Renderer ────────────────────────────────────────
// Renders the game scene WITHOUT Layout Mode into an FBO,
// then sends the FBO texture to Spout2 for OBS capture.
//
// IMPORTANT: renderAndSend() must be called from the DRAW phase
// (after PlayLayer::draw()), NOT from update(). The GL context
// is only ready for rendering during the draw phase.
class MirrorRenderer {
public:
    MirrorRenderer();
    ~MirrorRenderer();

    // Initialize FBO + Spout2 sender
    void init(unsigned int width, unsigned int height, const char* spoutName);

    // Render the current game scene (without layout mode) into FBO and send to Spout2
    // MUST be called from the draw phase (after PlayLayer::draw())
    void renderAndSend();

    // Throttle check — returns true if it's time to render a mirror frame
    bool shouldRender(float dt);

    // Cleanup — called when exiting a level
    void cleanup();

    // Check if initialized
    bool isReady() const { return m_ready; }

    // Set the layout mode flag reference (to temporarily disable during mirror render)
    void setLayoutModeFlag(bool* flag) { m_layoutModeFlag = flag; }

private:
    unsigned int m_width = 1920;
    unsigned int m_height = 1080;
    unsigned int m_fps = 60;

    // FBO
    unsigned int m_fbo = 0;
    unsigned int m_texture = 0;

    // Spout2
    SpoutSenderWrap m_spout;

    // Throttle
    float m_accumulator = 0.0f;

    // State
    bool m_ready = false;
    int m_frameCount = 0;  // for debug logging

    // Pointer to the global layout mode flag
    bool* m_layoutModeFlag = nullptr;

    // Create FBO + texture
    bool createFBO();

    // Destroy FBO
    void destroyFBO();
};
