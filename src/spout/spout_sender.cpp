#include "spout_sender.hpp"

#ifdef _WIN32

#include "Spout.h"
#include <GL/gl.h>

TrakinesSpout::TrakinesSpout() : m_spout(new Spout()) {}

TrakinesSpout::~TrakinesSpout() {
    if (m_spout) {
        if (m_initialized) {
            m_spout->ReleaseSender();
        }
        delete m_spout;
        m_spout = nullptr;
    }
}

bool TrakinesSpout::init(const char* senderName, unsigned int width, unsigned int height) {
    if (m_initialized) return true;

    m_width = width;
    m_height = height;

    // Set the sender name that OBS will see (e.g. "Trakines")
    m_spout->SetSenderName(senderName);

    m_initialized = true;
    return true;
}

void TrakinesSpout::sendTexture(unsigned int textureID, unsigned int width, unsigned int height) {
    if (!m_initialized) return;

    // Update dimensions if changed (window resize)
    if (width != m_width || height != m_height) {
        updateSize(width, height);
    }

    // SendTexture shares the OpenGL texture via DX/GL interop (zero-copy on NVIDIA)
    // bInvert = true to flip Y axis (Cocos2d-x has inverted Y vs Spout convention)
    // HostFBO = 0 means use the default framebuffer context
    m_spout->SendTexture(textureID, GL_TEXTURE_2D, width, height, true, 0);
}

void TrakinesSpout::sendFramebuffer(unsigned int width, unsigned int height) {
    if (!m_initialized) return;

    if (width != m_width || height != m_height) {
        updateSize(width, height);
    }

    // SendFbo reads from the specified FBO and sends it via Spout
    // FBO 0 = default framebuffer (what's currently on screen)
    m_spout->SendFbo(0, width, height, true);
}

void TrakinesSpout::updateSize(unsigned int width, unsigned int height) {
    m_width = width;
    m_height = height;
}

void TrakinesSpout::release() {
    if (!m_initialized) return;

    m_spout->ReleaseSender();
    m_initialized = false;
    m_width = 0;
    m_height = 0;
}

#else

// ─── Stub implementations for non-Windows (Spout2 is Windows-only) ──────────
TrakinesSpout::TrakinesSpout() {}
TrakinesSpout::~TrakinesSpout() {}
bool TrakinesSpout::init(const char*, unsigned int, unsigned int) { return false; }
void TrakinesSpout::sendTexture(unsigned int, unsigned int, unsigned int) {}
void TrakinesSpout::sendFramebuffer(unsigned int, unsigned int) {}
void TrakinesSpout::updateSize(unsigned int, unsigned int) {}
void TrakinesSpout::release() {}

#endif
