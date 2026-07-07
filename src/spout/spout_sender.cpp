#include "spout_sender.hpp"
#include <Geode/loader/Dirs.hpp>

SpoutSenderWrap::SpoutSenderWrap() {}

SpoutSenderWrap::~SpoutSenderWrap() {
    release();
#ifdef TRAKINES_SPOUT_ENABLED
    if (m_dll) {
        FreeLibrary(m_dll);
        m_dll = nullptr;
    }
#endif
}

#ifdef TRAKINES_SPOUT_ENABLED
bool SpoutSenderWrap::loadDll() {
    // Try to load from mod resources directory
    auto mod = Mod::get();
    auto dllPath = mod->getResourcesDir() / "SpoutLibrary.dll";

    // If not in resources, try the mod's save directory or current directory
    if (!std::filesystem::exists(dllPath)) {
        dllPath = mod->getSaveDir() / "SpoutLibrary.dll";
    }

    // Try loading from system PATH as fallback
    m_dll = LoadLibraryA(dllPath.string().c_str());
    if (!m_dll) {
        // Fallback: try system PATH (if Spout2 is installed system-wide)
        m_dll = LoadLibraryA("SpoutLibrary.dll");
    }

    if (!m_dll) {
        log::error("Trakines: Failed to load SpoutLibrary.dll from {}", dllPath.string());
        return false;
    }

    m_getSpout = (GetSpoutFunc)GetProcAddress(m_dll, "GetSpout");
    if (!m_getSpout) {
        log::error("Trakines: GetSpout not found in SpoutLibrary.dll");
        FreeLibrary(m_dll);
        m_dll = nullptr;
        return false;
    }

    log::info("Trakines: SpoutLibrary.dll loaded from {}", dllPath.string());
    return true;
}
#endif

bool SpoutSenderWrap::create(const char* name, unsigned int width, unsigned int height) {
#ifdef TRAKINES_SPOUT_ENABLED
    m_name = name;

    if (!loadDll()) return false;

    m_spout = m_getSpout();
    if (!m_spout) {
        log::error("Trakines: GetSpout() returned null");
        return false;
    }

    m_spout->SetSenderName(name);
    m_initialized = true;
    log::info("Trakines: Spout2 sender created: \"{}\" ({}x{})", name, width, height);
    return true;
#else
    return false;
#endif
}

bool SpoutSenderWrap::sendTexture(unsigned int texID, unsigned int texTarget,
                                   unsigned int width, unsigned int height) {
#ifdef TRAKINES_SPOUT_ENABLED
    if (!m_spout || !m_initialized) return false;
    return m_spout->SendTexture(texID, texTarget, width, height, true, 0);
#else
    return false;
#endif
}

bool SpoutSenderWrap::sendFbo(unsigned int fboID, unsigned int width, unsigned int height) {
#ifdef TRAKINES_SPOUT_ENABLED
    if (!m_spout || !m_initialized) return false;
    return m_spout->SendFbo(fboID, width, height, true);
#else
    return false;
#endif
}

void SpoutSenderWrap::release() {
#ifdef TRAKINES_SPOUT_ENABLED
    if (m_spout) {
        m_spout->ReleaseSender();
        m_spout->Release();
        m_spout = nullptr;
    }
#endif
    m_initialized = false;
}

bool SpoutSenderWrap::isInitialized() {
#ifdef TRAKINES_SPOUT_ENABLED
    if (m_spout) return m_spout->IsInitialized();
#endif
    return false;
}

const char* SpoutSenderWrap::getName() {
#ifdef TRAKINES_SPOUT_ENABLED
    if (m_spout) return m_spout->GetName();
#endif
    return m_name.c_str();
}
