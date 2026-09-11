#ifndef AUDIO_TEST_CAPTURE_H
#define AUDIO_TEST_CAPTURE_H

#include "audiocapture.h"
#include <algorithm>
#include <atomic>

class AudioTestCapture final : public AudioCapture
{
public:
    explicit AudioTestCapture(bool stream = false) : m_stream(stream) {}
    ~AudioTestCapture() override { stop(); }
    void setVolume(qreal) override {}
    void run() override { if (m_stream) AudioCapture::run(); }
    std::atomic<bool> missing {false};

protected:
    bool initialize() override
    {
        m_sampleRate = 30000;
        m_channels = 1;
        return true;
    }
    void uninitialize() override {}
    void suspend() override {}
    void resume() override {}
    qint64 latency() const override { return 0; }
    bool readAudio(int) override
    {
        msleep(8);
        if (missing)
            return false;
        for (unsigned int i = 0; i < m_bufferSize; ++i)
            m_audioBuffer[i] = (i % 32 < 16) ? 4096 : -4096;
        return true;
    }

private:
    const bool m_stream;
};

#endif
