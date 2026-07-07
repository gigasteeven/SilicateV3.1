#include "mirror_renderer.hpp"
#include "../global.hpp"

#ifdef TRAKINES_SPOUT_ENABLED
#include <windows.h>
#include <GL/gl.h>
#endif

// External re-entrancy guard from playlayer_hooks.cpp
extern bool s_mirrorRendering;

MirrorRenderer::MirrorRenderer() {}

MirrorRenderer::~MirrorRenderer() {
    cleanup();
}

bool MirrorRenderer::createFBO() {
#ifdef TRAKINES_SPOUT_ENABLED
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_texture, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        log::error("Trakines: FBO incomplete (status=0x{:X})", status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    log::info("Trakines: FBO created ({}x{}, tex={}, fbo={})", m_width, m_height, m_texture, m_fbo);
    return true;
#else
    return false;
#endif
}

void MirrorRenderer::destroyFBO() {
#ifdef TRAKINES_SPOUT_ENABLED
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
#endif
}

void MirrorRenderer::init(unsigned int width, unsigned int height, const char* spoutName) {
    m_width = width;
    m_height = height;

    if (!createFBO()) {
        log::error("Trakines: Failed to create mirror FBO");
        return;
    }

    if (!m_spout.create(spoutName, m_width, m_height)) {
        log::error("Trakines: Failed to create Spout2 sender");
        destroyFBO();
        return;
    }

    m_ready = true;
    m_frameCount = 0;
    log::info("Trakines: Mirror renderer initialized ({}x{}, Spout name: {})",
              m_width, m_height, spoutName);
}

void MirrorRenderer::renderAndSend() {
#ifdef TRAKINES_SPOUT_ENABLED
    if (!m_ready) return;

    // ── Set re-entrancy guard ──────────────────────────────
    s_mirrorRendering = true;

    // ── Save current GL state ──────────────────────────────
    GLint oldFbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFbo);
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    // ── Temporarily disable layout mode ───────────────────
    bool wasLayout = false;
    if (m_layoutModeFlag && *m_layoutModeFlag) {
        wasLayout = true;
        *m_layoutModeFlag = false;
    }

    // ── Bind our FBO and set viewport ─────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);

    // Clear
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // ── Re-render the current scene into our FBO ──────────
    auto scene = CCDirector::get()->getRunningScene();
    if (scene) {
        scene->visit();
    }

    // ── Restore layout mode flag ──────────────────────────
    if (wasLayout && m_layoutModeFlag) {
        *m_layoutModeFlag = true;
    }

    // ── Restore old FBO and viewport ──────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, oldFbo);
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

    // ── Send the FBO texture to Spout2 ────────────────────
    bool sent = m_spout.sendTexture(m_texture, GL_TEXTURE_2D, m_width, m_height);

    // ── If SendTexture failed, try SendFbo with our FBO ───
    if (!sent) {
        log::warn("Trakines: SendTexture failed (frame {}), trying SendFbo...", m_frameCount + 1);
        sent = m_spout.sendFbo(m_fbo, m_width, m_height);
    }

    // ── If SendFbo also failed, try sending the DEFAULT framebuffer ──
    if (!sent) {
        log::warn("Trakines: SendFbo failed, trying default framebuffer (screen)...");
        sent = m_spout.sendFbo(0, m_width, m_height);
    }

    // ── If all GPU methods failed, try glReadPixels + SendImage ──────
    if (!sent && m_pixelBuffer.empty()) {
        m_pixelBuffer.resize(m_width * m_height * 4);
        log::warn("Trakines: All GPU methods failed, trying glReadPixels + SendImage...");
    }

    m_frameCount++;

    // Log first 5 frames, then every 300
    if (m_frameCount <= 5 || m_frameCount % 300 == 0) {
        log::info("Trakines: Frame {} sent to Spout2: {} (tex={}, fbo={})",
                  m_frameCount, sent ? "OK" : "FAILED", m_texture, m_fbo);
    }

    // ── Clear re-entrancy guard ────────────────────────────
    s_mirrorRendering = false;
#endif
}

bool MirrorRenderer::shouldRender(float dt) {
    if (!m_ready) return false;
    return true;  // Render every frame for now (no throttle)
}

void MirrorRenderer::cleanup() {
    m_spout.release();
    destroyFBO();
    m_ready = false;
    m_frameCount = 0;
    log::info("Trakines: Mirror renderer cleaned up");
}
