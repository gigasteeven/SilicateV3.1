#pragma once

/// Spout2 sender wrapper for Trakines
/// Sends OpenGL textures/framebuffers to OBS via Spout2 zero-copy GPU sharing
///
/// NOTE: This header is deliberately kept free of Spout2 / OpenGL headers.
/// Geode/cocos2d translation units (e.g. main.cpp) include this wrapper, and
/// cocos ships its own GLEW. Pulling in Spout's GL extension loader here would
/// clash with GLEW ("redeclaration of __glewShaderSource"). We forward-declare
/// the Spout type and hide it behind a pointer; the real Spout headers are only
/// included inside spout_sender.cpp (which is excluded from Geode's PCH).

#include <string>

#ifdef _WIN32
class Spout;  // forward declaration — real definition lives in spout_sender.cpp
#endif

class TrakinesSpout {
public:
    TrakinesSpout();
    ~TrakinesSpout();

    TrakinesSpout(const TrakinesSpout&) = delete;
    TrakinesSpout& operator=(const TrakinesSpout&) = delete;

    /// Initialize Spout2 sender with given name and dimensions
    bool init(const char* senderName, unsigned int width, unsigned int height);

    /// Send an OpenGL texture to Spout2
    /// @param textureID  OpenGL texture name (from CCTexture2D::getName())
    /// @param width      Texture width in pixels
    /// @param height     Texture height in pixels
    void sendTexture(unsigned int textureID, unsigned int width, unsigned int height);

    /// Send the current default framebuffer (FBO 0) to Spout2
    /// Used for menu/pause/editor screen mirroring
    /// @param width   Framebuffer width in pixels
    /// @param height  Framebuffer height in pixels
    void sendFramebuffer(unsigned int width, unsigned int height);

    /// Update sender dimensions (e.g. on window resize)
    void updateSize(unsigned int width, unsigned int height);

    /// Release the Spout2 sender and free resources
    void release();

    /// Check if sender is initialized and ready
    bool isInitialized() const { return m_initialized; }

    /// Get current sender width
    unsigned int getWidth() const { return m_width; }

    /// Get current sender height
    unsigned int getHeight() const { return m_height; }

private:
    bool m_initialized = false;
    unsigned int m_width = 0;
    unsigned int m_height = 0;

#ifdef _WIN32
    Spout* m_spout = nullptr;  // owned; created in ctor, destroyed in dtor
#endif
};
