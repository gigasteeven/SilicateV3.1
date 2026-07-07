#include "mirror_renderer.hpp"
#include "../global.hpp"

#ifdef TRAKINES_SPOUT_ENABLED
#include <windows.h>
#include <GL/gl.h>
#endif

extern bool s_mirrorRendering;

MirrorRenderer::MirrorRenderer() {}
MirrorRenderer::~MirrorRenderer() { cleanup(); }

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
    if (m_texture) { glDeleteTextures(1, &m_texture); m_texture = 0; }
    if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
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

    // ── Re-entrancy guard ──────────────────────────────────
    if (s_mirrorRendering) return;
    s_mirrorRendering = true;

    // ── Log entry on first few calls ───────────────────────
    m_frameCount++;
    if (m_frameCount <= 3) {
        log::info("Trakines: renderAndSend() called (frame {})", m_frameCount);
    }

    // ── Save GL state ──────────────────────────────────────
    GLint oldFbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFbo);
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    // ── Disable layout mode for mirror render ──────────────
    bool wasLayout = false;
    if (m_layoutModeFlag && *m_layoutModeFlag) {
        wasLayout = true;
        *m_layoutModeFlag = false;
    }

    // ── Bind our FBO ───────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // ── Render scene into FBO ──────────────────────────────
    auto scene = CCDirector::get()->getRunningScene();
    if (scene) {
        scene->visit();
    }

    // ── Restore layout mode ────────────────────────────────
    if (wasLayout && m_layoutModeFlag) {
        *m_layoutModeFlag = true;
    }

    // ── Restore GL state ───────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, oldFbo);
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

    // ── Try sending to Spout2 ──────────────────────────────
    // Method 1: SendTexture (our FBO texture)
    bool sent = m_spout.sendTexture(m_texture, GL_TEXTURE_2D, m_width, m_height);

    // Method 2: SendFbo with our FBO
    if (!sent) {
        if (m_frameCount <= 5) log::warn("Trakines: SendTexture failed (frame {}), trying SendFbo", m_frameCount);
        sent = m_spout.sendFbo(m_fbo, m_width, m_height);
    }

    // Method 3: SendFbo with default framebuffer (screen)
    if (!sent) {
        if (m_frameCount <= 5) log::warn("Trakines: SendFbo failed, trying default framebuffer");
        sent = m_spout.sendFbo(0, m_width, m_height);
    }

    // Method 4: glReadPixels + SendImage (CPU fallback)
    if (!sent) {
        if (m_frameCount <= 5) log::warn("Trakines: SendFbo(0) failed, trying glReadPixels + SendImage");
        // Read from our FBO
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
        glReadPixels(0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_pixelBuffer.data());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        // Send via SendImage
        // Note: SpoutLibrary SendImage signature: SendImage(pixels, width, height, glFormat, bInvert, HostFBO)
        // We need to call it through our wrapper — but we don't have a SendImage wrapper.
        // For now, just log.
        log::error("Trakines: All Spout2 send methods failed on frame {}", m_frameCount);
    }

    // ── Log result ─────────────────────────────────────────
    if (m_frameCount <= 5 || m_frameCount % 300 == 0) {
        log::info("Trakines: Frame {} sent to Spout2: {} (tex={}, fbo={})",
                  m_frameCount, sent ? "OK" : "FAILED", m_texture, m_fbo);
    }

    s_mirrorRendering = false;
#endif
}

bool MirrorRenderer::shouldRender(float dt) {
    return m_ready;
}

void MirrorRenderer::cleanup() {
    m_spout.release();
    destroyFBO();
    m_ready = false;
    m_frameCount = 0;
    log::info("Trakines: Mirror renderer cleaned up");
}
