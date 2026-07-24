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

#include <QSettings>
#include <QSignalSpy>
#include <QIODevice>

#include "audiocapture.h"
#include "audiocapture_qt6.h"
#include "audiocapture_test.h"
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
                              int blocks)
{
    const int frames = blocks * AUDIO_DEFAULT_BUFFER_SIZE;
    std::vector<int16_t> samples(size_t(frames) * channels);
    for (int frame = 0; frame < frames; frame++)
    {
        const double phase = 2.0 * M_PI * frequency * frame / sampleRate;
        const int16_t sample = int16_t(std::sin(phase) * 20000.0);
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
        if (results.pitchConfidence > confidence)
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
    QCOMPARE(firstSignalCount, blocks);
    QCOMPARE(aubioSpy.count(), blocks * 2);
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
    QCOMPARE(aubioSpy.count(), blocks);
    const double beforePitch = bestPitch(aubioSpy);
    const int readsBeforeInvalid = capture.readSizes().size();

    capture.setNegotiatedFormat(sampleRate, channels, {});
    QTest::ignoreMessage(QtWarningMsg,
                         QRegularExpression("\\[AudioCapture\\] Invalid negotiated format.*"));
    capture.run();

    QCOMPARE(capture.readSizes().size(), readsBeforeInvalid);
    QCOMPARE(aubioSpy.count(), blocks);

    capture.setNegotiatedFormat(
        44100, 1, makeSine(44100, 1, 440.0, blocks));
    capture.run();
    QCOMPARE(capture.readSizes().size(), blocks * 2);
    QCOMPARE(aubioSpy.count(), blocks * 2);
    const double afterPitch = bestPitch(aubioSpy, blocks);
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
