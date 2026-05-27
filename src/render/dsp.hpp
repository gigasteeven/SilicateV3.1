#ifndef RECORDER_DSP_HPP
#define RECORDER_DSP_HPP

#include <Geode/Geode.hpp>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <vector>

#include "settings/settings.hpp"

class AudioRecorder {
   public:
    static AudioRecorder* get() {
        static AudioRecorder instance;

        return &instance;
    }

    void init();
    void attach(double musicVolume, double sfxVolume);
    void detach();
    void uninit();

    void wait();
    void proceed();

    float calculateTime();
    void haltWithData(float* data, unsigned int length);
    void gatherOne();

    void unpause();

    std::vector<float> getBuffer();
    std::vector<float> collectData(double videoTime);

    static FMOD_RESULT F_CALLBACK
    writeCallback(FMOD_DSP_STATE* dspState, float* inBuffer, float* outBuffer,
                  unsigned int length, int inChannels, int* outChannels);

    SLValuePtr<bool> m_audioPreview = SLValue<bool>::create(
        "renderer.audio_preview", &SLSettings::get()->previewAudio);
    bool m_attached = false;
    bool m_collectedData = false;
    std::mutex m_lock;
    std::condition_variable m_cv;

    FMOD::ChannelGroup* m_master;

    float m_pulse1 = 0.0;
    float m_pulse2 = 0.0;
    float m_pulse3 = 0.0;
    float m_pulseCounter = 0.0;

    float m_lastPulse = 0.0;
    bool m_collectPulses = false;
    std::atomic_bool m_shouldUpdateFmod = false;

    double m_systemTime = 0.0;
    double m_fmodTime = 0.0;
    double m_time = 0.0;
    uint32_t m_index = 0;
    int m_sampleRate = 0;
    int m_channels = 0;
    size_t m_lastCollectedLength = 0;

    std::vector<float> m_buffer;

    struct FmodQueuedFn {
        float time;
        std::function<void()> fn;
    };

    std::vector<FmodQueuedFn> m_queuedFmod;

   private:
    FMOD::DSP* m_dsp;
    size_t m_lastBufferSize = 0;

    float m_previousMusicVolume = 0.0;
    float m_previousSFXVolume = 0.0;
};

#endif  // RECORDER_DSP_HPP
