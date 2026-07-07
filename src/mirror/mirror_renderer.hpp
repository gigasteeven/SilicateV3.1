#pragma once

#include <Geode/Geode.hpp>
#include <string>
#include <vector>
#include "../spout/spout_sender.hpp"

#ifdef TRAKINES_SPOUT_ENABLED
#include <windows.h>
#include <GL/gl.h>
#endif

using namespace geode::prelude;

// ── Mirror Renderer ────────────────────────────────────────
class MirrorRenderer {
public:
    MirrorRenderer();
    ~MirrorRenderer();

    void init(unsigned int width, unsigned int height, const char* spoutName);
    void renderAndSend();
    bool shouldRender(float dt);
    void cleanup();
    bool isReady() const { return m_ready; }
    void setLayoutModeFlag(bool* flag) { m_layoutModeFlag = flag; }

private:
    unsigned int m_width = 1920;
    unsigned int m_height = 1080;
    unsigned int m_fps = 60;

    unsigned int m_fbo = 0;
    unsigned int m_texture = 0;

    SpoutSenderWrap m_spout;

    bool m_ready = false;
    int m_frameCount = 0;
    bool* m_layoutModeFlag = nullptr;

    // CPU fallback pixel buffer (only used if GPU methods fail)
    std::vector<unsigned char> m_pixelBuffer;

    bool createFBO();
    void destroyFBO();
};
