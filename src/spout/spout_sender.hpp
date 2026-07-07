#pragma once

#include <Geode/Geode.hpp>
#include <string>

#ifdef TRAKINES_SPOUT_ENABLED
#include "SpoutSender.h"
#endif

using namespace geode::prelude;

// ── Spout2 sender wrapper ──────────────────────────────────
// Manages a Spout2 sender that shares an OpenGL texture with OBS.
// Uses GPU-to-GPU texture sharing (NVIDIA GL/DX interop on RTX 3090).
class SpoutSenderWrap {
public:
    SpoutSenderWrap();
    ~SpoutSenderWrap();

    // Create a Spout2 sender with the given name and dimensions
    bool create(const char* name, unsigned int width, unsigned int height);

    // Send an OpenGL texture to Spout2 (GPU-to-GPU, no CPU copy)
    bool sendTexture(unsigned int texID, unsigned int texTarget,
                     unsigned int width, unsigned int height);

    // Send the currently bound FBO to Spout2
    bool sendFbo(unsigned int fboID, unsigned int width, unsigned int height);

    // Release the sender and free resources
    void release();

    // Check if the sender is initialized
    bool isInitialized();

    // Get sender name
    const char* getName();

private:
#ifdef TRAKINES_SPOUT_ENABLED
    SpoutSender* m_sender = nullptr;
#endif
    bool m_initialized = false;
};
