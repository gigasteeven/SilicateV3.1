#include "spout_sender.hpp"

SpoutSenderWrap::SpoutSenderWrap() {
#ifdef TRAKINES_SPOUT_ENABLED
    m_sender = new SpoutSender();
#endif
}

SpoutSenderWrap::~SpoutSenderWrap() {
    release();
#ifdef TRAKINES_SPOUT_ENABLED
    delete m_sender;
    m_sender = nullptr;
#endif
}

bool SpoutSenderWrap::create(const char* name, unsigned int width, unsigned int height) {
#ifdef TRAKINES_SPOUT_ENABLED
    if (!m_sender) return false;
    m_sender->SetSenderName(name);
    // CreateSender is called automatically by the first SendTexture/SendFbo call
    // But we can also call it explicitly via the Spout class
    // SpoutSender wraps Spout which auto-creates on first send
    m_initialized = true;
    return true;
#else
    return false;
#endif
}

bool SpoutSenderWrap::sendTexture(unsigned int texID, unsigned int texTarget,
                                   unsigned int width, unsigned int height) {
#ifdef TRAKINES_SPOUT_ENABLED
    if (!m_sender || !m_initialized) return false;
    return m_sender->SendTexture(texID, texTarget, width, height, true, 0);
#else
    return false;
#endif
}

bool SpoutSenderWrap::sendFbo(unsigned int fboID, unsigned int width, unsigned int height) {
#ifdef TRAKINES_SPOUT_ENABLED
    if (!m_sender || !m_initialized) return false;
    return m_sender->SendFbo(fboID, width, height, true);
#else
    return false;
#endif
}

void SpoutSenderWrap::release() {
#ifdef TRAKINES_SPOUT_ENABLED
    if (m_sender) {
        m_sender->ReleaseSender();
    }
#endif
    m_initialized = false;
}

bool SpoutSenderWrap::isInitialized() {
#ifdef TRAKINES_SPOUT_ENABLED
    if (m_sender) return m_sender->IsInitialized();
#endif
    return false;
}

const char* SpoutSenderWrap::getName() {
#ifdef TRAKINES_SPOUT_ENABLED
    if (m_sender) return m_sender->GetName();
#endif
    return "Trakines";
}
