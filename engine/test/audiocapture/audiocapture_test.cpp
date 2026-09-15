/*
  Q Light Controller Plus - Unit test
  audiocapture_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>
#include <samplerate.h>

#include <QSettings>
#include <QSignalSpy>
#include <QIODevice>

#include "audioanalyzer.h"
#include "audiocapture.h"
#include "audiocapture_qt6.h"
#include "audiocapture_test.h"
#include "audiochannel.h"
#include "audioframe.h"
#include "aubioresults.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{

class NegotiatedAudioCapture final : public AudioCapture
{
public:
    NegotiatedAudioCapture(unsigned int sampleRate,
                           unsigned int channels,
                           std::vector<int16_t> samples)
        : m_negotiatedSampleRate(sampleRate)
        , m_negotiatedChannels(channels)
        , m_samples(std::move(samples))
    {
    }

    void setVolume(qreal) override
    {
    }

    const QVector<int> &readSizes() const
    {
        return m_readSizes;
    }

    int uninitializeCount() const
    {
        return m_uninitializeCount;
    }

    void setNegotiatedFormat(unsigned int sampleRate,
                             unsigned int channels,
                             std::vector<int16_t> samples)
    {
        m_negotiatedSampleRate = sampleRate;
        m_negotiatedChannels = channels;
        m_samples = std::move(samples);
        m_offset = 0;
    }

protected:
    bool initialize() override
    {
        // Matches the current Qt fallback: the device mutates the negotiated
        // fields before AudioCapture reads its first block.
        m_sampleRate = m_negotiatedSampleRate;
        m_channels = m_negotiatedChannels;
        return true;
    }

    void uninitialize() override
    {
        m_uninitializeCount++;
    }

    void suspend() override
    {
    }

    void resume() override
    {
    }

    qint64 latency() const override
    {
        return 0;
    }

    bool readAudio(int maxSize) override
    {
        m_readSizes.append(maxSize);
        const int requiredSamples = int(AUDIO_DEFAULT_BUFFER_SIZE * m_negotiatedChannels);
        if (maxSize != requiredSamples
            || m_offset + size_t(maxSize) > m_samples.size())
        {
            m_userStop = true;
            return false;
        }

        std::memcpy(m_audioBuffer, m_samples.data() + m_offset,
                    size_t(maxSize) * sizeof(*m_audioBuffer));
        m_offset += size_t(maxSize);
        if (m_offset == m_samples.size())
            m_userStop = true;
        return true;
    }

private:
    unsigned int m_negotiatedSampleRate;
    unsigned int m_negotiatedChannels;
    std::vector<int16_t> m_samples;
    size_t m_offset = 0;
    QVector<int> m_readSizes;
    int m_uninitializeCount = 0;
};

class ChunkedIODevice final : public QIODevice
{
public:
    ChunkedIODevice()
    {
        open(QIODevice::ReadOnly);
    }

    void append(const QByteArray &data)
    {
        m_data.append(data);
        emit readyRead();
    }

    bool isSequential() const override
    {
        return true;
    }

    qint64 bytesAvailable() const override
    {
        return m_data.size() + QIODevice::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 size = std::min<qint64>(maxSize, m_data.size());
        if (size == 0)
            return 0;
        std::memcpy(data, m_data.constData(), size_t(size));
        m_data.remove(0, size);
        return size;
    }

    qint64 writeData(const char *, qint64) override
    {
        return -1;
    }

private:
    QByteArray m_data;
};

QAudioFormat makeQt6Format(int sampleRate,
                           int channels,
                           QAudioFormat::SampleFormat sampleFormat)
{
    QAudioFormat format;
    format.setSampleRate(sampleRate);
    format.setChannelCount(channels);
    format.setSampleFormat(sampleFormat);
    return format;
}

std::vector<int16_t> makeSine(unsigned int sampleRate,
                              unsigned int channels,
                              double frequency,
                              int blocks,
                              double amplitude = 20000.0)
{
    const int frames = blocks * AUDIO_DEFAULT_BUFFER_SIZE;
    std::vector<int16_t> samples(size_t(frames) * channels);
    for (int frame = 0; frame < frames; frame++)
    {
        const double phase = 2.0 * M_PI * frequency * frame / sampleRate;
        const int16_t sample = int16_t(std::sin(phase) * amplitude);
        for (unsigned int channel = 0; channel < channels; channel++)
            samples[size_t(frame) * channels + channel] = sample;
    }
    return samples;
}

void addKick(std::vector<int16_t> &mono, int startFrame, int sampleRate)
{
    const int length = int(0.09 * sampleRate);
    double phase = 0.0;
    for (int i = 0; i < length; i++)
    {
        const int position = startFrame + i;
        if (position >= int(mono.size()))
            break;
        const double time = double(i) / sampleRate;
        const double frequency = 150.0 * std::exp(-time * 25.0) + 45.0;
        phase += 2.0 * M_PI * frequency / sampleRate;
        mono[position] = int16_t(std::sin(phase) * std::exp(-time * 30.0) * 20000.0);
    }
}

std::vector<int16_t> makeKickTrack(unsigned int sampleRate,
                                   unsigned int channels,
                                   double bpm,
                                   double seconds)
{
    const int frames = int(seconds * sampleRate);
    std::vector<int16_t> mono(frames, 0);
    for (double beat = 0.0; beat < seconds; beat += 60.0 / bpm)
        addKick(mono, int(beat * sampleRate), int(sampleRate));

    std::vector<int16_t> samples(size_t(frames) * channels);
    for (int frame = 0; frame < frames; frame++)
        for (unsigned int channel = 0; channel < channels; channel++)
            samples[size_t(frame) * channels + channel] = mono[frame];
    samples.resize(samples.size() - (samples.size() % (AUDIO_DEFAULT_BUFFER_SIZE * channels)));
    return samples;
}

double bestPitch(const QSignalSpy &spy, int begin = 0)
{
    double pitch = 0.0;
    double confidence = -1.0;
    for (int i = begin; i < spy.size(); i++)
    {
        const QList<QVariant> &arguments = spy.at(i);
        const AubioResults results = qvariant_cast<AubioResults>(arguments.at(0));
        if (results.pitchHz > 0.0 && results.pitchConfidence > confidence)
        {
            confidence = results.pitchConfidence;
            pitch = results.pitchHz;
        }
    }
    return pitch;
}

QByteArray floatBlock(const std::vector<float> &samples)
{
    return QByteArray(reinterpret_cast<const char *>(samples.data()),
                      int(samples.size() * sizeof(float)));
}

class PacketInput final : public QIODevice
{
public:
    QByteArray bytes;
    QVector<int> packets;
    qsizetype offset = 0;
    int packet = 0;
    bool yield = false;

    bool isSequential() const override { return true; }

protected:
    qint64 readData(char *output, qint64 maximum) override
    {
        if (yield || offset == bytes.size())
        {
            yield = false;
            return 0;
        }
        const qint64 count = std::min({ maximum, qint64(bytes.size() - offset),
                                       qint64(packets[packet++ % packets.size()]) });
        std::memcpy(output, bytes.constData() + offset, size_t(count));
        offset += count;
        yield = true;
        return count;
    }
    qint64 writeData(const char *, qint64) override { return -1; }
};

class InjectedInput final : public AudioCaptureQt6::InputBackend
{
public:
    QAudioFormat format;
    QByteArray bytes;
    QVector<int> packets {8192};
    std::atomic<bool> disconnected {false};
    std::atomic<int> opens {0};
    std::atomic<qreal> volume {0.0};
    QByteArray openedId;

    QList<AudioCaptureQt6::Device> devices() const override
    {
        if (disconnected)
            return {{QByteArray("other"), QStringLiteral("Other mic"), true}};
        return {{QByteArray("selected"), QStringLiteral("Selected input"), true},
                {QByteArray("other"), QStringLiteral("Other mic"), false}};
    }
    QIODevice *open(const QByteArray &id, int, int, QAudioFormat &actual) override
    {
        ++opens;
        openedId = id;
        actual = format;
        input = std::make_unique<PacketInput>();
        input->bytes = bytes;
        input->packets = packets;
        input->open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        return input.get();
    }
    bool failed() const override { return disconnected; }
    void close() override { input.reset(); }
    void setVolume(qreal value) override { volume = value; }

private:
    std::unique_ptr<PacketInput> input;
};

struct RecordedFrame
{
    std::vector<float> samples;
    uint64_t sampleTime;
    uint64_t frameIndex;
    uint64_t epoch;
    uint32_t sampleRate;
};

std::vector<float> resampleReference(const std::vector<float> &mono, int rate)
{
    if (rate == 30000)
        return mono;
    int error = 0;
    SRC_STATE *state = src_new(SRC_SINC_FASTEST, 1, &error);
    if (!state)
        qFatal("Reference resampler failed: %s", src_strerror(error));
    std::vector<float> result(size_t(std::ceil(double(mono.size()) * 30000.0 / rate)) + 1000);
    SRC_DATA data {};
    data.data_in = mono.data();
    data.input_frames = long(mono.size());
    data.data_out = result.data();
    data.output_frames = long(result.size());
    data.src_ratio = 30000.0 / rate;
    error = src_process(state, &data);
    if (error || data.input_frames_used != long(mono.size()))
        qFatal("Reference resampler did not consume input");
    result.resize(size_t(data.output_frames_gen));
    src_delete(state);
    return result;
}

} // namespace

void AudioCapture_Test::init()
{
    QSettings settings;
    settings.setValue(SETTINGS_AUDIO_INPUT_SRATE, AUDIO_DEFAULT_SAMPLE_RATE);
    settings.setValue(SETTINGS_AUDIO_INPUT_CHANNELS, AUDIO_DEFAULT_CHANNELS);
}

void AudioCapture_Test::cleanup()
{
    QSettings settings;
    settings.remove(SETTINGS_AUDIO_INPUT_SRATE);
    settings.remove(SETTINGS_AUDIO_INPUT_CHANNELS);
}

void AudioCapture_Test::canonicalSourceCadence_data()
{
    QTest::addColumn<unsigned int>("sampleRate");
    QTest::addColumn<unsigned int>("channels");
    QTest::newRow("44100 mono") << 44100u << 1u;
    QTest::newRow("48000 stereo") << 48000u << 2u;
}

void AudioCapture_Test::canonicalSourceCadence()
{
    QFETCH(unsigned int, sampleRate);
    QFETCH(unsigned int, channels);
    static constexpr int blocks = 100;
    NegotiatedAudioCapture capture(sampleRate, channels,
                                   makeSine(sampleRate, channels, 440.0, blocks));
    QSignalSpy frames(&capture, &AudioCapture::aubioDataReady);
    capture.run();
    const int expectedFrames = int(resampleReference(
        std::vector<float>(blocks * AUDIO_DEFAULT_BUFFER_SIZE, 0.3f), sampleRate).size() / 500);
    QCOMPARE(frames.count(), expectedFrames);
}

void AudioCapture_Test::qt6CanonicalPackets_data()
{
    QTest::addColumn<int>("rate");
    QTest::addColumn<int>("channels");
    QTest::addColumn<int>("encoding");
    QTest::addColumn<QVector<int>>("packets");
    for (int rate : {44100, 48000})
        for (int channels : {1, 2})
            for (int encoding : {int(QAudioFormat::Int16), int(QAudioFormat::Float)})
                for (bool irregular : {false, true})
                {
                    const QByteArray name = QByteArray::number(rate) + "/" + QByteArray::number(channels)
                        + "/" + QByteArray::number(encoding) + (irregular ? "/irregular" : "/regular");
                    QTest::newRow(name.constData()) << rate << channels << encoding
                        << (irregular ? QVector<int>{1, 7, 4091, 3, 8192, 127, 511}
                                      : QVector<int>{2048});
                }
}

void AudioCapture_Test::qt6CanonicalPackets()
{
    QFETCH(int, rate);
    QFETCH(int, channels);
    QFETCH(int, encoding);
    QFETCH(QVector<int>, packets);
    const QAudioFormat format = makeQt6Format(rate, channels, QAudioFormat::SampleFormat(encoding));
    static constexpr int sourceFrames = 512 * 120 + 200;
    QByteArray bytes;
    std::vector<float> mono(sourceFrames);
    for (int i = 0; i < sourceFrames; ++i)
    {
        double mixed = 0;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float value = float(0.31 * std::sin(2.0 * M_PI * (347 + 811 * ch) * i / rate)
                                      + 0.037 * std::cos(2.0 * M_PI * 7913 * i / rate));
            if (encoding == QAudioFormat::Float)
            {
                bytes.append(reinterpret_cast<const char *>(&value), sizeof(value));
                mixed += value;
            }
            else
            {
                const int16_t pcm = int16_t(value * 32768.0f);
                bytes.append(reinterpret_cast<const char *>(&pcm), sizeof(pcm));
                mixed += float(pcm) / 32768.0f;
            }
        }
        mono[size_t(i)] = float(mixed / channels);
    }
    const auto expected = resampleReference(mono, rate);
    auto backend = std::make_unique<InjectedInput>();
    backend->format = format;
    backend->bytes = bytes;
    backend->packets = packets;
    AudioCaptureQt6 capture(std::move(backend));
    capture.setInputDevice(QStringLiteral("Selected input"));
    std::vector<RecordedFrame> observed;
    std::atomic<int> frameCount {0};
    connect(&capture, &AudioCapture::frameReady, &capture, [&](const AudioFrame &frame) {
        observed.push_back({std::vector<float>(frame.samples, frame.samples + frame.sampleCount),
                            frame.sampleTime, frame.frameIndex, frame.sourceEpoch, frame.sampleRate});
        ++frameCount;
    }, Qt::DirectConnection);
    capture.registerBandsNumber(3);
    QTRY_COMPARE_WITH_TIMEOUT(frameCount.load(), int(expected.size() / 500), 10000);
    capture.unregisterBandsNumber(3);
    QCOMPARE(observed.size(), expected.size() / 500);
    QVERIFY(observed.size() > 50);
    double maxError = 0.0;
    for (size_t i = 0; i < observed.size(); ++i)
    {
        QCOMPARE(observed[i].sampleRate, 30000u);
        QCOMPARE(observed[i].samples.size(), size_t(500));
        QCOMPARE(observed[i].sampleTime, uint64_t(i * 500));
        QCOMPARE(observed[i].frameIndex, uint64_t(i + 1));
        QCOMPARE(observed[i].epoch, observed.front().epoch);
        for (size_t j = 0; j < 500; ++j)
            maxError = std::max(maxError, double(std::abs(observed[i].samples[j] - expected[i * 500 + j])));
    }
    QVERIFY2(maxError < 5e-6, qPrintable(QString::number(maxError, 'g', 12)));
    qInfo("CANONICAL rate=%d channels=%d encoding=%d frames=%zu firstTime=%llu lastTime=%llu maxError=%.9g",
          rate, channels, encoding, observed.size(),
          static_cast<unsigned long long>(observed.front().sampleTime),
          static_cast<unsigned long long>(observed.back().sampleTime), maxError);
}

void AudioCapture_Test::qt6CanonicalPoisonRecovery_data()
{
    QTest::addColumn<float>("poison");
    QTest::newRow("NaN") << std::numeric_limits<float>::quiet_NaN();
    QTest::newRow("+Inf") << std::numeric_limits<float>::infinity();
    QTest::newRow("-Inf") << -std::numeric_limits<float>::infinity();
}

void AudioCapture_Test::qt6CanonicalPoisonRecovery()
{
    QFETCH(float, poison);
    std::vector<float> valid(512 * 8);
    for (size_t i = 0; i < valid.size(); ++i)
        valid[i] = float(0.3 * std::sin(i * 0.073));
    std::vector<float> poisoned(512, 0.2f);
    poisoned[333] = poison;
    auto backend = std::make_unique<InjectedInput>();
    backend->format = makeQt6Format(48000, 1, QAudioFormat::Float);
    backend->bytes = floatBlock(valid).first(512 * sizeof(float))
        + floatBlock(poisoned) + floatBlock(valid).sliced(512 * sizeof(float));
    backend->packets = {2048, 2048, 7, 119, 3, 3001, 1, 8192};
    const auto expected = resampleReference(valid, 48000);
    AudioCaptureQt6 capture(std::move(backend));
    capture.setInputDevice(QStringLiteral("Selected input"));
    std::vector<float> samples;
    std::atomic<int> frames {0};
    connect(&capture, &AudioCapture::frameReady, &capture, [&](const AudioFrame &frame) {
        samples.insert(samples.end(), frame.samples, frame.samples + frame.sampleCount);
        ++frames;
    }, Qt::DirectConnection);
    capture.registerBandsNumber(3);
    QTRY_COMPARE_WITH_TIMEOUT(frames.load(), int(expected.size() / 500), 3000);
    capture.unregisterBandsNumber(3);
    QCOMPARE(samples.size(), (expected.size() / 500) * 500);
    for (size_t i = 0; i < samples.size(); ++i)
        QVERIFY(std::abs(samples[i] - expected[i]) < 5e-6f);
}

void AudioCapture_Test::qt6FloatPrecision()
{
    std::vector<float> tiny(512 * 4);
    for (size_t i = 0; i < tiny.size(); ++i)
        tiny[i] = float((int(i % 41) - 20) * 1e-8);
    auto backend = std::make_unique<InjectedInput>();
    backend->format = makeQt6Format(30000, 1, QAudioFormat::Float);
    backend->bytes = floatBlock(tiny);
    backend->packets = {3, 13, 2049};
    AudioCaptureQt6 capture(std::move(backend));
    capture.setInputDevice(QStringLiteral("Selected input"));
    std::vector<float> observed;
    std::atomic<int> frames {0};
    connect(&capture, &AudioCapture::frameReady, &capture, [&](const AudioFrame &frame) {
        observed.insert(observed.end(), frame.samples, frame.samples + frame.sampleCount);
        ++frames;
    }, Qt::DirectConnection);
    capture.registerBandsNumber(3);
    QTRY_COMPARE_WITH_TIMEOUT(frames.load(), 4, 3000);
    capture.unregisterBandsNumber(3);
    QCOMPARE(observed.size(), size_t(2000));
    for (size_t i = 0; i < observed.size(); ++i)
        QCOMPARE(observed[i], tiny[i]);
}

void AudioCapture_Test::qt6SlowPartialPackets()
{
    auto backend = std::make_unique<InjectedInput>();
    backend->format = makeQt6Format(30000, 1, QAudioFormat::Float);
    backend->bytes = floatBlock(std::vector<float>(1000, 0.125f));
    backend->packets = {8};
    AudioCaptureQt6 capture(std::move(backend));
    std::atomic<int> frames {0};
    std::vector<RecordedFrame> observed;
    connect(&capture, &AudioCapture::frameReady, &capture, [&](const AudioFrame &frame) {
        observed.push_back({std::vector<float>(frame.samples, frame.samples + frame.sampleCount),
            frame.sampleTime, frame.frameIndex, frame.sourceEpoch, frame.sampleRate});
        ++frames;
    }, Qt::DirectConnection);
    capture.registerBandsNumber(3);
    QTRY_COMPARE_WITH_TIMEOUT(frames.load(), 2, 8000);
    capture.unregisterBandsNumber(3);
    QCOMPARE(observed[0].samples, std::vector<float>(500, 0.125f));
    QCOMPARE(observed[1].samples, observed[0].samples);
    QCOMPARE(observed[1].sampleTime, uint64_t(500));
    QCOMPARE(observed[1].frameIndex, uint64_t(2));
    QCOMPARE(observed[1].epoch, observed[0].epoch);
}

void AudioCapture_Test::qt6UnavailableRestart()
{
    auto backend = std::make_unique<InjectedInput>();
    InjectedInput *input = backend.get();
    backend->format = makeQt6Format(48000, 2, QAudioFormat::Float);
    backend->bytes = floatBlock(std::vector<float>(512 * 16, 0.125f));
    AudioCaptureQt6 capture(std::move(backend));
    capture.setInputDevice(QStringLiteral("id:c2VsZWN0ZWQ="));
    capture.setVolume(0.37);
    QSignalSpy first(&capture, &AudioCapture::statusChanged);
    QSignalSpy second(&capture, &AudioCapture::statusChanged);
    std::atomic<int> frames {0};
    QElapsedTimer clock;
    clock.start();
    std::atomic<qint64> lastFrameMs {-1}, unavailableMs {-1};
    connect(&capture, &AudioCapture::frameReady, &capture,
            [&](const AudioFrame &) {
                lastFrameMs = clock.elapsed();
                ++frames;
            }, Qt::DirectConnection);
    connect(&capture, &AudioCapture::statusChanged, &capture,
            [&](AudioCapture::Status status, const QString &, quint64) {
                if (status == AudioCapture::Unavailable)
                    unavailableMs = clock.elapsed();
            }, Qt::DirectConnection);
    capture.registerBandsNumber(3);
    capture.registerBandsNumber(16);
    QTRY_VERIFY_WITH_TIMEOUT(frames.load() > 0, 1000);
    const uint64_t firstEpoch = capture.sourceEpoch();
    QTRY_VERIFY_WITH_TIMEOUT(unavailableMs.load() >= 0, 1000);
    QCOMPARE(capture.status(), AudioCapture::Unavailable);
    QVERIFY(unavailableMs.load() - lastFrameMs.load() <= 250);
    QCOMPARE(capture.signalPower(), 0u);
    QVERIFY(capture.sourceEpoch() > firstEpoch);
    const int stalledFrames = frames;
    QTest::qWait(50);
    QCOMPARE(frames.load(), stalledFrames);
    QCOMPARE(first.count(), second.count());
    input->disconnected = true;
    const uint64_t lostEpoch = capture.sourceEpoch();
    capture.restart();
    QTRY_VERIFY_WITH_TIMEOUT(capture.sourceEpoch() > lostEpoch &&
                            capture.status() == AudioCapture::Unavailable, 1000);
    const int opensBeforeReconnect = input->opens;
    QTest::qWait(120);
    QCOMPARE(input->opens.load(), opensBeforeReconnect);
    input->disconnected = false;
    QTRY_VERIFY_WITH_TIMEOUT(frames.load() > stalledFrames, 1000);
    QCOMPARE(input->volume.load(), 0.37);
    capture.unregisterBandsNumber(3);
    QVERIFY(capture.isRunning());
    capture.unregisterBandsNumber(16);
    QCOMPARE(capture.status(), AudioCapture::Stopped);
    QCOMPARE(input->openedId, QByteArray("selected"));
}

void AudioCapture_Test::qt6RejectedNegotiation_data()
{
    QTest::addColumn<int>("rate");
    QTest::addColumn<int>("channels");
    QTest::addColumn<int>("encoding");
    QTest::newRow("zero rate") << 0 << 2 << int(QAudioFormat::Float);
    QTest::newRow("zero channels") << 48000 << 0 << int(QAudioFormat::Float);
    QTest::newRow("UInt8") << 48000 << 2 << int(QAudioFormat::UInt8);
    QTest::newRow("Int32") << 48000 << 1 << int(QAudioFormat::Int32);
    QTest::newRow("Unknown") << 48000 << 2 << int(QAudioFormat::Unknown);
}

void AudioCapture_Test::qt6DeviceReselection()
{
    auto backend = std::make_unique<InjectedInput>();
    backend->format = makeQt6Format(48000, 1, QAudioFormat::Float);
    backend->bytes = floatBlock(std::vector<float>(512 * 4, 0.3f));
    AudioCaptureQt6 capture(std::move(backend));
    capture.setInputDevice(QString());
    std::atomic<int> frames {0};
    std::atomic<uint64_t> firstFrameEpoch {0};
    std::atomic<uint64_t> firstFrameTime {99};
    connect(&capture, &AudioCapture::frameReady, &capture, [&](const AudioFrame &frame) {
        if (frame.frameIndex == 1)
        {
            firstFrameEpoch = frame.sourceEpoch;
            firstFrameTime = frame.sampleTime;
        }
        ++frames;
    }, Qt::DirectConnection);
    capture.registerBandsNumber(3);
    QTRY_VERIFY_WITH_TIMEOUT(frames.load() > 0, 1000);
    QCOMPARE(capture.appliedDevice(), QStringLiteral("id:c2VsZWN0ZWQ="));
    const uint64_t epoch = firstFrameEpoch;
    const int previousCount = frames;
    capture.setInputDevice(QStringLiteral("Other mic"));
    QTRY_VERIFY_WITH_TIMEOUT(firstFrameEpoch.load() > epoch, 1000);
    QVERIFY(frames.load() > previousCount);
    QCOMPARE(firstFrameTime.load(), uint64_t(0));
    QCOMPARE(capture.appliedDevice(), QStringLiteral("id:b3RoZXI="));
    QCOMPARE(capture.captureFormat().sampleRate(), 48000);
    QCOMPARE(capture.channels(), 1u);
    capture.unregisterBandsNumber(3);
}

void AudioCapture_Test::qt6RejectedNegotiation()
{
    QFETCH(int, rate);
    QFETCH(int, channels);
    QFETCH(int, encoding);
    auto backend = std::make_unique<InjectedInput>();
    backend->format = makeQt6Format(rate, channels, QAudioFormat::SampleFormat(encoding));
    backend->bytes = QByteArray(32768, 'x');
    AudioCaptureQt6 capture(std::move(backend));
    capture.setInputDevice(QStringLiteral("Selected input"));
    std::atomic<int> frames {0};
    connect(&capture, &AudioCapture::frameReady, &capture,
            [&](const AudioFrame &) { ++frames; }, Qt::DirectConnection);
    capture.registerBandsNumber(3);
    QTRY_COMPARE_WITH_TIMEOUT(capture.status(), AudioCapture::Error, 250);
    capture.unregisterBandsNumber(3);
    QCOMPARE(frames.load(), 0);
}

void AudioCapture_Test::quietBandsRespectConfiguredGate_data()
{
    QTest::addColumn<double>("frequency");
    QTest::addColumn<double>("rmsDb");
    QTest::addColumn<double>("gateDb");

    QTest::newRow("mids -55 gate -65") << 800.0 << -55.0 << -65.0;
    QTest::newRow("mids -70 gate -80") << 800.0 << -70.0 << -80.0;
    QTest::newRow("highs -55 gate -65") << 5000.0 << -55.0 << -65.0;
    QTest::newRow("loud control -50") << 800.0 << -50.0 << -65.0;
}

void AudioCapture_Test::quietBandsRespectConfiguredGate()
{
    QFETCH(double, frequency);
    QFETCH(double, rmsDb);
    QFETCH(double, gateDb);

    static constexpr unsigned int sampleRate = 44100;
    static constexpr int blocks = 300;
    const double amplitude = 32768.0 * std::sqrt(2.0) * std::pow(10.0, rmsDb / 20.0);
    auto samples = makeSine(sampleRate, 1, frequency, blocks, amplitude);
    AudioAnalyzer analyzer;
    AudioChannelConfig config = AudioChannelConfig::defaults();
    config.noiseGate.thresholdDb = gateDb;
    AudioChannel *channel = analyzer.createChannel(config);
    NegotiatedAudioCapture capture(sampleRate, 1, samples);
    capture.setAnalyzer(&analyzer);
    AudioSnapshot snapshot;
    connect(&capture, &AudioCapture::frameReady, &capture,
            [&](const AudioFrame &) { snapshot = channel->snapshot(); }, Qt::DirectConnection);

    capture.run();

    qInfo("QUIET_BAND_HZ=%.0f RMS_DB=%.2f GATE_DB=%.0f CLOSED=%d MIDS=%.6f HIGHS=%.6f",
          frequency, snapshot.features.rmsDb, gateDb, snapshot.noiseGateClosed,
          snapshot.mids, snapshot.highs);
    QVERIFY(snapshot.features.rmsDb > gateDb);
    QVERIFY(!snapshot.noiseGateClosed);
    QVERIFY((frequency < 3000.0 ? snapshot.mids : snapshot.highs) > 0.0);

    if (frequency == 800.0 && rmsDb == -55.0)
    {
        const auto bass = makeSine(sampleRate, 1, 70.0, blocks);
        for (size_t i = 0; i < samples.size(); i++)
            samples[i] += bass[i];
        capture.setNegotiatedFormat(sampleRate, 1, std::move(samples));
        capture.run();
        const AudioSnapshot withBass = snapshot;
        QVERIFY(!withBass.noiseGateClosed);
        QVERIFY(withBass.mids > 0.0);
    }
}

void AudioCapture_Test::configuredGateStillSuppressesQuietAudio_data()
{
    QTest::addColumn<double>("amplitude");
    QTest::addColumn<int>("dcOffset");

    QTest::newRow("nonzero -75 gate -65")
        << 32768.0 * std::sqrt(2.0) * std::pow(10.0, -75.0 / 20.0) << 0;
    QTest::newRow("digital silence") << 0.0 << 0;
    QTest::newRow("constant DC") << 0.0 << 8192;
}

void AudioCapture_Test::configuredGateStillSuppressesQuietAudio()
{
    QFETCH(double, amplitude);
    QFETCH(int, dcOffset);

    static constexpr unsigned int sampleRate = 44100;
    AudioAnalyzer analyzer;
    AudioChannelConfig config = AudioChannelConfig::defaults();
    config.noiseGate.thresholdDb = -65.0;
    AudioChannel *channel = analyzer.createChannel(config);
    NegotiatedAudioCapture capture(sampleRate, 1, makeSine(sampleRate, 1, 800.0, 300));
    capture.setAnalyzer(&analyzer);
    AudioSnapshot observed;
    connect(&capture, &AudioCapture::frameReady, &capture,
            [&](const AudioFrame &) { observed = channel->snapshot(); }, Qt::DirectConnection);
    capture.run();
    const AudioSnapshot open = observed;
    QVERIFY(!open.noiseGateClosed);
    QVERIFY(open.mids > 0.0);
    QVERIFY(open.volume.normalized > 0.0);
    QVERIFY(open.volume.volumeNorm > 0.0);

    auto quiet = makeSine(sampleRate, 1, 800.0, 8, amplitude);
    for (int16_t &sample : quiet)
        sample += dcOffset;
    capture.setNegotiatedFormat(sampleRate, 1, std::move(quiet));
    capture.run();
    AudioSnapshot closed = observed;
    if (dcOffset != 0)
    {
        // Raw PCM volume includes DC, unlike the former per-packet demeaned meter.
        QVERIFY(closed.features.rmsDb > -13.0);
        QVERIFY(!closed.noiseGateClosed);
        return;
    }
    QVERIFY(closed.noiseGateClosed);
    QVERIFY(closed.features.rmsDb < config.noiseGate.thresholdDb);
    if (amplitude > 0.0)
    {
        capture.setNegotiatedFormat(
            sampleRate, 1, makeSine(sampleRate, 1, 800.0, 600, amplitude));
        capture.run();
        closed = observed;
        QVERIFY(closed.mids <= 1e-6);
        QVERIFY(closed.highs <= 1e-6);
    }
    else
    {
        QCOMPARE(closed.mids, 0.0);
        QCOMPARE(closed.highs, 0.0);
    }

    QVERIFY(closed.noiseGateClosed);
    QCOMPARE(closed.volume.normalized, 0.0);
    const double rawVolume = std::clamp(1.0 + closed.features.rmsDb / 100.0, 0.0, 1.0);
    QVERIFY(std::abs(closed.volume.volumeNorm - rawVolume) < 1e-3);
    for (double value : { closed.features.rmsDb, closed.features.peakDb,
                          closed.features.crestFactor, closed.features.centroidHz,
                          closed.features.spread, closed.features.rolloffHz,
                          closed.features.flux, closed.features.hfc,
                          closed.features.flatness, closed.volume.raw,
                          closed.volume.smoothed, closed.mids, closed.highs })
        QVERIFY(std::isfinite(value));
}

void AudioCapture_Test::negotiatedFormatUpdatesPipeline_data()
{
    QTest::addColumn<unsigned int>("sampleRate");
    QTest::addColumn<unsigned int>("channels");
    QTest::addColumn<int>("requiredSamples");

    QTest::newRow("unchanged 44.1kHz mono")
        << 44100u << 1u << 512;
    QTest::newRow("negotiated 48kHz stereo")
        << 48000u << 2u << 1024;
}

void AudioCapture_Test::negotiatedFormatUpdatesPipeline()
{
    QFETCH(unsigned int, sampleRate);
    QFETCH(unsigned int, channels);
    QFETCH(int, requiredSamples);

    NegotiatedAudioCapture capture(sampleRate, channels,
                                   makeSine(sampleRate, channels, 440.0, 96));
    QSignalSpy aubioSpy(&capture, &AudioCapture::aubioDataReady);
    QVERIFY(aubioSpy.isValid());

    capture.run();

    QVERIFY(!capture.readSizes().isEmpty());
    const int readSamples = capture.readSizes().constFirst();
    const bool captureSizeMatches = std::all_of(
        capture.readSizes().cbegin(), capture.readSizes().cend(),
        [requiredSamples](int size) { return size == requiredSamples; });
    qInfo("READ_AUDIO_SAMPLES=%d", readSamples);
    qInfo("REQUIRED_BLOCK_SAMPLES=%d", requiredSamples);
    qInfo("CAPTURE_SIZE_MATCH=%s", captureSizeMatches ? "true" : "false");

    QCOMPARE(capture.sampleRate(), sampleRate);
    QVERIFY(captureSizeMatches);
    QCOMPARE(capture.uninitializeCount(), 1);
    QVERIFY(!aubioSpy.isEmpty());
}

void AudioCapture_Test::negotiatedFormatTransitions_data()
{
    QTest::addColumn<unsigned int>("firstRate");
    QTest::addColumn<unsigned int>("firstChannels");
    QTest::addColumn<unsigned int>("secondRate");
    QTest::addColumn<unsigned int>("secondChannels");

    QTest::newRow("unchanged restart")
        << 44100u << 1u << 44100u << 1u;
    QTest::newRow("rate only")
        << 44100u << 1u << 48000u << 1u;
    QTest::newRow("channel only")
        << 48000u << 1u << 48000u << 2u;
    QTest::newRow("second rate only")
        << 48000u << 2u << 44100u << 2u;
}

void AudioCapture_Test::negotiatedFormatTransitions()
{
    QFETCH(unsigned int, firstRate);
    QFETCH(unsigned int, firstChannels);
    QFETCH(unsigned int, secondRate);
    QFETCH(unsigned int, secondChannels);

    static constexpr int blocks = 96;
    NegotiatedAudioCapture capture(
        firstRate, firstChannels,
        makeSine(firstRate, firstChannels, 440.0, blocks));
    QSignalSpy aubioSpy(&capture, &AudioCapture::aubioDataReady);
    capture.run();
    const int firstSignalCount = aubioSpy.count();
    const int firstReadCount = capture.readSizes().size();
    const double firstPitch = bestPitch(aubioSpy);

    capture.setNegotiatedFormat(
        secondRate, secondChannels,
        makeSine(secondRate, secondChannels, 440.0, blocks));
    capture.run();
    const double secondPitch = bestPitch(aubioSpy, firstSignalCount);

    QCOMPARE(firstReadCount, blocks);
    QCOMPARE(capture.readSizes().size(), blocks * 2);
    QVERIFY(std::all_of(
        capture.readSizes().cbegin(),
        capture.readSizes().cbegin() + firstReadCount,
        [firstChannels](int size) {
            return size == int(AUDIO_DEFAULT_BUFFER_SIZE * firstChannels);
        }));
    QVERIFY(std::all_of(
        capture.readSizes().cbegin() + firstReadCount,
        capture.readSizes().cend(),
        [secondChannels](int size) {
            return size == int(AUDIO_DEFAULT_BUFFER_SIZE * secondChannels);
        }));
    QCOMPARE(capture.sampleRate(), secondRate);
    const int firstExpected = int(resampleReference(
        std::vector<float>(blocks * AUDIO_DEFAULT_BUFFER_SIZE, 0.3f), firstRate).size() / 500);
    const int secondExpected = int(resampleReference(
        std::vector<float>(blocks * AUDIO_DEFAULT_BUFFER_SIZE, 0.3f), secondRate).size() / 500);
    QCOMPARE(firstSignalCount, firstExpected);
    QCOMPARE(aubioSpy.count(), firstExpected + secondExpected);
    QVERIFY(firstPitch > 0.0);
    QVERIFY(secondPitch > 0.0);
    QVERIFY2(std::fabs(secondPitch - firstPitch) / firstPitch < 0.03,
             qPrintable(QString("transition pitch %1 differs from %2")
                        .arg(secondPitch).arg(firstPitch)));
    qInfo("FORMAT_TRANSITION=%u/%u->%u/%u READ_SAMPLES=%d,%d PITCH=%.2f,%.2f SIGNALS=%d",
          firstRate, firstChannels, secondRate, secondChannels,
          capture.readSizes().at(0), capture.readSizes().at(firstReadCount),
          firstPitch, secondPitch, int(aubioSpy.count()));
}

void AudioCapture_Test::negotiatedFormatUpdatesAubio()
{
    NegotiatedAudioCapture reference(
        44100, 1, makeSine(44100, 1, 440.0, 96));
    QSignalSpy referenceSpy(&reference, &AudioCapture::aubioDataReady);
    reference.run();

    NegotiatedAudioCapture negotiated(
        48000, 2, makeSine(48000, 2, 440.0, 96));
    QSignalSpy negotiatedSpy(&negotiated, &AudioCapture::aubioDataReady);
    negotiated.run();

    QVERIFY(!referenceSpy.isEmpty());
    QVERIFY(!negotiatedSpy.isEmpty());
    const double referencePitch = bestPitch(referenceSpy);
    const double negotiatedPitch = bestPitch(negotiatedSpy);
    qInfo("REFERENCE_AUBIO_PITCH_HZ=%.2f", referencePitch);
    qInfo("NEGOTIATED_AUBIO_PITCH_HZ=%.2f", negotiatedPitch);
    QVERIFY(referencePitch > 0.0);
    QVERIFY2(std::fabs(negotiatedPitch - referencePitch) / referencePitch < 0.03,
             qPrintable(QString("negotiated pitch %1 differs from reference %2")
                        .arg(negotiatedPitch).arg(referencePitch)));
}

void AudioCapture_Test::negotiatedFormatUpdatesTracker()
{
    static constexpr unsigned int sampleRate = 48000;
    static constexpr unsigned int channels = 2;
    static constexpr int requiredSamples = AUDIO_DEFAULT_BUFFER_SIZE * channels;

    NegotiatedAudioCapture capture(
        sampleRate, 1, makeKickTrack(sampleRate, 1, 120.0, 20.0));
    QSignalSpy beatSpy(&capture, &AudioCapture::beatDetected);
    QVERIFY(beatSpy.isValid());

    capture.run();
    QVERIFY(!beatSpy.isEmpty());
    const int firstReadCount = capture.readSizes().size();
    beatSpy.clear();

    capture.setNegotiatedFormat(
        sampleRate, channels, makeKickTrack(sampleRate, channels, 120.0, 20.0));
    capture.run();

    QCOMPARE(firstReadCount, int(20.0 * sampleRate / AUDIO_DEFAULT_BUFFER_SIZE));
    QVERIFY(std::all_of(
        capture.readSizes().cbegin(),
        capture.readSizes().cbegin() + firstReadCount,
        [](int size) { return size == AUDIO_DEFAULT_BUFFER_SIZE; }));
    QVERIFY(std::all_of(
        capture.readSizes().cbegin() + firstReadCount,
        capture.readSizes().cend(),
        [](int size) { return size == requiredSamples; }));
    QVERIFY2(!beatSpy.isEmpty(), "negotiated tracker emitted no beats");

    const int detectedBpm = beatSpy.constLast().at(0).toInt();
    qInfo("NEGOTIATED_TRACKER_BPM=%d", detectedBpm);
    QVERIFY(std::abs(detectedBpm - 120) <= 3);
}

void AudioCapture_Test::invalidNegotiatedFormat_data()
{
    QTest::addColumn<unsigned int>("sampleRate");
    QTest::addColumn<unsigned int>("channels");

    QTest::newRow("zero sample rate") << 0u << 2u;
    QTest::newRow("zero channels") << 48000u << 0u;
    QTest::newRow("capture size overflow")
        << 48000u
        << (std::numeric_limits<unsigned int>::max() / AUDIO_DEFAULT_BUFFER_SIZE) + 1u;
}

void AudioCapture_Test::invalidNegotiatedFormat()
{
    QFETCH(unsigned int, sampleRate);
    QFETCH(unsigned int, channels);

    static constexpr int blocks = 96;
    NegotiatedAudioCapture capture(
        44100, 1, makeSine(44100, 1, 440.0, blocks));
    QSignalSpy aubioSpy(&capture, &AudioCapture::aubioDataReady);
    capture.run();
    const int expectedFrames = int(resampleReference(
        std::vector<float>(blocks * AUDIO_DEFAULT_BUFFER_SIZE, 0.3f), 44100).size() / 500);
    QCOMPARE(aubioSpy.count(), expectedFrames);
    const double beforePitch = bestPitch(aubioSpy);
    const int readsBeforeInvalid = capture.readSizes().size();

    capture.setNegotiatedFormat(sampleRate, channels, {});
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression("\\[AudioCapture\\] Invalid negotiated format.*"));
    capture.run();

    QCOMPARE(capture.readSizes().size(), readsBeforeInvalid);
    QCOMPARE(aubioSpy.count(), expectedFrames);

    capture.setNegotiatedFormat(
        44100, 1, makeSine(44100, 1, 440.0, blocks));
    capture.run();
    QCOMPARE(capture.readSizes().size(), blocks * 2);
    QCOMPARE(aubioSpy.count(), expectedFrames * 2);
    const double afterPitch = bestPitch(aubioSpy, expectedFrames);
    QVERIFY(beforePitch > 0.0);
    QVERIFY(std::fabs(afterPitch - beforePitch) / beforePitch < 0.03);
    QCOMPARE(capture.uninitializeCount(), 3);
    qInfo("INVALID_FORMAT_RECOVERY_PITCH=%.2f,%.2f READS=%d SIGNALS=%d",
          beforePitch, afterPitch, int(capture.readSizes().size()),
          int(aubioSpy.count()));
}

void AudioCapture_Test::qt6SampleConversion_data()
{
    QTest::addColumn<int>("sampleFormat");
    QTest::addColumn<QByteArray>("input");
    QTest::addColumn<QVector<int>>("expected");

    const int16_t int16Samples[] = {
        std::numeric_limits<int16_t>::min(), -12345, 0, 12345,
        std::numeric_limits<int16_t>::max()
    };
    QTest::newRow("Int16 lossless")
        << int(QAudioFormat::Int16)
        << QByteArray(reinterpret_cast<const char *>(int16Samples),
                      int(sizeof(int16Samples)))
        << QVector<int>({ -32768, -12345, 0, 12345, 32767 });

    const float floatSamples[] = { -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f };
    QTest::newRow("Float32 clamp and convert")
        << int(QAudioFormat::Float)
        << QByteArray(reinterpret_cast<const char *>(floatSamples),
                      int(sizeof(floatSamples)))
        << QVector<int>({ -32768, -32768, -16384, 0, 16384, 32767, 32767 });
}

void AudioCapture_Test::qt6SampleConversion()
{
    QFETCH(int, sampleFormat);
    QFETCH(QByteArray, input);
    QFETCH(QVector<int>, expected);

    const QAudioFormat format = makeQt6Format(
        48000, 1, QAudioFormat::SampleFormat(sampleFormat));
    std::vector<int16_t> output(size_t(expected.size()), 1234);

    QVERIFY(AudioCaptureQt6::convertSamples(
        QByteArrayView(input), format, expected.size(), output.data()));
    for (int i = 0; i < expected.size(); i++)
        QCOMPARE(int(output[size_t(i)]), expected.at(i));
    std::vector<float> floating(size_t(expected.size()), 1234.0f);
    QVERIFY(AudioCaptureQt6::convertSamples(
        QByteArrayView(input), format, expected.size(), floating.data()));
    for (int i = 0; i < expected.size(); ++i)
    {
        if (sampleFormat == QAudioFormat::Int16)
            QCOMPARE(floating[size_t(i)], float(expected[i]) / 32768.0f);
        else
        {
            float value;
            std::memcpy(&value, input.constData() + i * sizeof(float), sizeof(value));
            QCOMPARE(floating[size_t(i)], std::clamp(value, -1.0f, 1.0f));
        }
    }
    qInfo("SAMPLE_FORMAT=%d CONVERTED_FIRST=%d CONVERTED_LAST=%d",
          sampleFormat, int(output.front()), int(output.back()));
}

void AudioCapture_Test::qt6UnsupportedSampleFormat_data()
{
    QTest::addColumn<int>("sampleFormat");

    QTest::newRow("UInt8") << int(QAudioFormat::UInt8);
    QTest::newRow("Int32") << int(QAudioFormat::Int32);
    QTest::newRow("Unknown") << int(QAudioFormat::Unknown);
}

void AudioCapture_Test::qt6UnsupportedSampleFormat()
{
    QFETCH(int, sampleFormat);

    const QAudioFormat format = makeQt6Format(
        48000, 1, QAudioFormat::SampleFormat(sampleFormat));
    const QByteArray input(16, char(0x7f));
    int16_t output[] = { 1234, 1234, 1234, 1234 };

    QVERIFY(!AudioCaptureQt6::convertSamples(
        QByteArrayView(input), format, 4, output));
    for (int16_t sample : output)
        QCOMPARE(sample, int16_t(1234));
}

void AudioCapture_Test::qt6PartialFrameReads()
{
    const QAudioFormat format = makeQt6Format(48000, 2, QAudioFormat::Float);
    const float floatSamples[] = { -1.0f, -0.5f, 0.5f, 1.5f };
    const QByteArray raw(reinterpret_cast<const char *>(floatSamples),
                         int(sizeof(floatSamples)));
    ChunkedIODevice input;
    QByteArray pending;
    int16_t output[] = { 1234, 1234, 1234, 1234 };

    input.append(raw.first(7));
    QVERIFY(!AudioCaptureQt6::readConvertedSamples(
        &input, pending, format, 4, output));
    QCOMPARE(pending.size(), 7);
    for (int16_t sample : output)
        QCOMPARE(sample, int16_t(1234));

    input.append(raw.sliced(7));
    QVERIFY(AudioCaptureQt6::readConvertedSamples(
        &input, pending, format, 4, output));
    QCOMPARE(QVector<int>({ output[0], output[1], output[2], output[3] }),
             QVector<int>({ -32768, -16384, 16384, 32767 }));
    qInfo("FLOAT32_KNOWN_WAVEFORM=%d,%d,%d,%d",
          output[0], output[1], output[2], output[3]);
    QVERIFY(pending.isEmpty());

    input.append(raw.first(2));
    std::fill_n(output, 4, int16_t(2345));
    QVERIFY(!AudioCaptureQt6::readConvertedSamples(
        &input, pending, format, 4, output));
    QCOMPARE(pending.size(), 2);
    for (int16_t sample : output)
        QCOMPARE(sample, int16_t(2345));
}

void AudioCapture_Test::qt6PoisonedBlockRecovery_data()
{
    QTest::addColumn<double>("poison");
    QTest::addColumn<int>("firstChunkBytes");

    QTest::newRow("NaN complete")
        << std::numeric_limits<double>::quiet_NaN() << 16;
    QTest::newRow("positive infinity complete")
        << std::numeric_limits<double>::infinity() << 16;
    QTest::newRow("negative infinity complete")
        << -std::numeric_limits<double>::infinity() << 16;
    QTest::newRow("NaN partial")
        << std::numeric_limits<double>::quiet_NaN() << 7;
}

void AudioCapture_Test::qt6PoisonedBlockRecovery()
{
    QFETCH(double, poison);
    QFETCH(int, firstChunkBytes);

    const QAudioFormat format = makeQt6Format(48000, 2, QAudioFormat::Float);
    const QByteArray poisoned = floatBlock({
        float(poison), -0.25f, 0.25f, 0.75f
    });
    const QByteArray valid = floatBlock({
        -1.0f, -0.5f, 0.5f, 1.0f
    });
    ChunkedIODevice input;
    QByteArray pending;
    int16_t output[] = { 1234, 1234, 1234, 1234 };
    float floating[] = {1234.0f, 1234.0f, 1234.0f, 1234.0f};
    QVERIFY(!AudioCaptureQt6::convertSamples(QByteArrayView(poisoned), format, 4, floating));
    for (float sample : floating)
        QCOMPARE(sample, 1234.0f);

    input.append(poisoned.first(firstChunkBytes));
    const bool partialOrRejected = AudioCaptureQt6::readConvertedSamples(
        &input, pending, format, 4, output);
    QVERIFY(!partialOrRejected);
    for (int16_t sample : output)
        QCOMPARE(sample, int16_t(1234));

    if (firstChunkBytes < poisoned.size())
    {
        QCOMPARE(pending.size(), firstChunkBytes);
        input.append(poisoned.sliced(firstChunkBytes));
        QVERIFY(!AudioCaptureQt6::readConvertedSamples(
            &input, pending, format, 4, output));
        for (int16_t sample : output)
            QCOMPARE(sample, int16_t(1234));
    }

    qInfo("POISON_REJECTED_PENDING=%lld", qint64(pending.size()));
    QCOMPARE(pending.size(), 0);

    input.append(valid);
    QVERIFY(AudioCaptureQt6::readConvertedSamples(
        &input, pending, format, 4, output));
    QCOMPARE(QVector<int>({ output[0], output[1], output[2], output[3] }),
             QVector<int>({ -32768, -16384, 16384, 32767 }));
    qInfo("POISON_RECOVERY_OUTPUT=%d,%d,%d,%d PENDING=%lld",
          output[0], output[1], output[2], output[3], qint64(pending.size()));
    QVERIFY(pending.isEmpty());
}

QTEST_GUILESS_MAIN(AudioCapture_Test)
