#include "dsp.hpp"

#include <Geode/binding/FMODAudioEngine.hpp>

#include "bot/bot.hpp"
#include "renderer.hpp"

#ifdef SL_DEV_MODE
static int collected = 0;
static int waiting = 0;
#endif

FMOD_RESULT F_CALLBACK AudioRecorder::writeCallback(FMOD_DSP_STATE*,
                                                    float* inBuffer,
                                                    float* outBuffer,
                                                    unsigned int length,
                                                    int inChannels, int*) {
    AudioRecorder* recorder = AudioRecorder::get();

    if (!recorder->m_shouldUpdateFmod) {
        return FMOD_OK;
    }

    // std::memset(outBuffer, 0, length * inChannels * sizeof(float));
    SL_LOG_DEV("length: {}, inChannels: {}, outChannels: {}", length,
               inChannels, *outChannels);

    recorder->m_lastCollectedLength = length;
    recorder->haltWithData(inBuffer, length * inChannels);

    if (recorder->m_audioPreview->inner()) {
        std::memcpy(outBuffer, inBuffer, length * inChannels * sizeof(float));

#pragma omp parallel for
        for (unsigned i = 0; i < length * inChannels; i++) {
            outBuffer[i] *= recorder->m_previousMusicVolume;
        }
    } else {
        std::memset(outBuffer, 0, length * inChannels * sizeof(float));
    }

    return FMOD_OK;
}

void AudioRecorder::haltWithData(float* data, unsigned int length) {
    // std::lock_guard lock(m_lock);

    SL_LOG_DEV("Collected data, unpausing... {}", collected++);

    m_buffer.insert(m_buffer.end(), data, data + length);

#pragma omp parallel for
    for (size_t i = m_buffer.size() - length; i < m_buffer.size(); i++) {
        m_buffer[i] = std::clamp(m_buffer[i], -1.0f, 1.0f);
    }

    SL_LOG_DEV("Setting collected data to true");
    m_collectedData = true;
    // m_master->setPaused(true);
    // m_cv.notify_one();
}

void AudioRecorder::init() {
    FMOD_DSP_DESCRIPTION desc = {};
    strcpy_s(desc.name, "silly dsp");
    desc.version = 0x00020000;
    desc.numinputbuffers = 1;
    desc.numoutputbuffers = 1;
    desc.read = AudioRecorder::writeCallback;
    desc.numparameters = 0;

    auto engine = FMODAudioEngine::get();
    FMOD::System* system = engine->m_system;
    system->getMasterChannelGroup(&m_master);
    system->createDSP(&desc, &m_dsp);
    system->setDSPBufferSize(1024, 2);

    // m_master->setPaused(true);

    m_time = 0.0;
    m_lastPulse = 0.0;
    m_index = 0;
    m_fmodTime = 0.0;
    m_systemTime = 0.0;
    m_buffer.clear();
}

void AudioRecorder::attach(double musicVolume, double sfxVolume) {
    int numDsps;
    m_master->getNumDSPs(&numDsps);
    m_master->addDSP(numDsps, m_dsp);
    m_dsp->setMeteringEnabled(true, false);

    auto engine = FMODAudioEngine::get();

    m_previousMusicVolume = engine->getBackgroundMusicVolume();

    m_previousSFXVolume = engine->getEffectsVolume();
    m_master->setPaused(false);
    FMODAudioEngine::get()->m_system->getSoftwareFormat(&m_sampleRate, nullptr,
                                                        &m_channels);

    engine->setEffectsVolume(sfxVolume);
    engine->setBackgroundMusicVolume(musicVolume);

    engine->m_system->setOutput(FMOD_OUTPUTTYPE_NOSOUND_NRT);

    m_attached = true;
}

void AudioRecorder::detach() {
    m_master->removeDSP(m_dsp);
    m_master->setPaused(false);

    auto engine = FMODAudioEngine::get();

    engine->setEffectsVolume(m_previousSFXVolume);
    engine->setBackgroundMusicVolume(m_previousMusicVolume);
    engine->m_system->setOutput(FMOD_OUTPUTTYPE_AUTODETECT);

    m_attached = false;
}

void AudioRecorder::wait() {
    std::unique_lock lock(m_lock);
    m_cv.wait(lock, [this] { return m_collectedData; });
}

void AudioRecorder::proceed() {
    std::lock_guard lock(m_lock);

    m_collectedData = false;
    m_master->setPaused(false);
    m_cv.notify_one();
}

void AudioRecorder::uninit() { m_dsp->release(); }

float AudioRecorder::calculateTime() {
    int sampleRate, channels;
    FMODAudioEngine::get()->m_system->getSoftwareFormat(&sampleRate, nullptr,
                                                        &channels);

    return (float)m_buffer.size() / ((float)sampleRate * (float)channels);
}

void AudioRecorder::unpause() {
    auto renderer = Renderer::get();
    auto engine = FMODAudioEngine::get();

    m_shouldUpdateFmod = true;
    engine->update(renderer->getDt());
    m_shouldUpdateFmod = false;
}

std::vector<float> AudioRecorder::getBuffer() {
    std::unique_lock lock(m_lock);

    m_cv.wait(lock, [this] { return m_collectedData; });

    return m_buffer;
}
