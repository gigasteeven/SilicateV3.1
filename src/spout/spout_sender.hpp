#pragma once

#include <Geode/Geode.hpp>
#include <string>

#ifdef TRAKINES_SPOUT_ENABLED
#include <windows.h>
// SpoutLibrary.h defines its own GL types (GLuint, GLint, GLenum)
// and does NOT include GL headers — pure virtual interface
#include "SpoutLibrary.h"
#endif

using namespace geode::prelude;

// ── Spout2 sender wrapper ──────────────────────────────────
// Uses SpoutLibrary.h (C++ virtual interface) loaded dynamically.
// The SpoutLibrary.dll is bundled in resources/ and extracted at runtime.
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
    HMODULE m_dll = nullptr;
    SPOUTHANDLE m_spout = nullptr;
    typedef SPOUTHANDLE(WINAPI* GetSpoutFunc)(VOID);
    GetSpoutFunc m_getSpout = nullptr;
#endif
    bool m_initialized = false;
    std::string m_name;

    // Load SpoutLibrary.dll from the mod's resources directory
    bool loadDll();
};
