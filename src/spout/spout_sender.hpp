#pragma once

/// Spout2 sender wrapper for Trakines
/// Sends OpenGL textures/framebuffers to OBS via Spout2 zero-copy GPU sharing

#ifdef _WIN32
#include "Spout.h"
#endif

#include <string>

class TrakinesSpout {
public:
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
    Spout m_spout;
#endif
};
