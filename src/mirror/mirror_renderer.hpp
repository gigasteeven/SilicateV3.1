#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include "../spout/spout_sender.hpp"

#ifdef TRAKINES_SPOUT_ENABLED
#include <GL/glew.h>
#endif

using namespace geode::prelude;

// ── Mirror Renderer ────────────────────────────────────────
// Renders the game scene WITHOUT Layout Mode into an FBO,
// then sends the FBO texture to Spout2 for OBS capture.
//
// Approach (Variant A): After PlayLayer::draw() renders the
// layout-mode scene to the screen, we re-render the same scene
// into our FBO with layout mode temporarily disabled.
class MirrorRenderer {
public:
    MirrorRenderer();
    ~MirrorRenderer();

    // Initialize FBO + Spout2 sender
    void init(unsigned int width, unsigned int height, const char* spoutName);

    // Render the current game scene (without layout mode) into FBO and send to Spout2
    // Called after PlayLayer::draw() or after any CCScene draw
    void renderAndSend();

    // Update throttle timer (call every frame)
    void update(float dt);

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

    // Pointer to the global layout mode flag
    bool* m_layoutModeFlag = nullptr;

    // Create FBO + texture
    bool createFBO();

    // Destroy FBO
    void destroyFBO();
};
