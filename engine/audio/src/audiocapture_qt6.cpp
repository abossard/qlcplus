/*
  Q Light Controller Plus
  audiocapture_qt6.cpp

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include <QMediaDevices>
#include <QAudioDevice>
#include <QSettings>
#include <QDebug>
#include <QCoreApplication>

#include "audiocapture_qt6.h"

namespace
{

bool isConvertibleSampleFormat(QAudioFormat::SampleFormat format)
{
    return format == QAudioFormat::Int16 || format == QAudioFormat::Float;
}

class QtInputBackend final : public AudioCaptureQt6::InputBackend
{
public:
    QList<AudioCaptureQt6::Device> devices() const override
    {
        QList<AudioCaptureQt6::Device> result;
        for (const QAudioDevice &device : QMediaDevices::audioInputs())
            result.append({device.id(), device.description(), device.isDefault()});
        return result;
    }

    QIODevice *open(const QByteArray &id, int rate, int channels, QAudioFormat &format) override
    {
        for (const QAudioDevice &device : QMediaDevices::audioInputs())
        {
            if (device.id() != id)
                continue;
            format = AudioCaptureQt6::selectCaptureFormat(device, rate, channels);
            if (!format.isValid())
                return nullptr;
            m_source = std::make_unique<QAudioSource>(device, format);
            QIODevice *input = m_source->start();
            if (!input || m_source->state() == QAudio::StoppedState)
            {
                close();
                return nullptr;
            }
            return input;
        }
        return nullptr;
    }

    bool failed() const override
    {
        return !m_source || m_source->error() != QAudio::NoError
            || m_source->state() == QAudio::StoppedState;
    }
    void close() override
    {
        if (m_source)
            m_source->stop();
        m_source.reset();
    }
    void setVolume(qreal volume) override
    {
        if (m_source)
            m_source->setVolume(volume);
    }

private:
    std::unique_ptr<QAudioSource> m_source;
};

template<typename Sample>
bool readBlock(QIODevice *input, QByteArray &pending, const QAudioFormat &format,
               int sampleCount, Sample *output)
{
    if (!input || !output || !format.isValid() || sampleCount <= 0
        || format.channelCount() <= 0 || sampleCount % format.channelCount() != 0
        || !isConvertibleSampleFormat(format.sampleFormat())
        || sampleCount > std::numeric_limits<int>::max() / format.bytesPerSample())
        return false;

    const int requiredBytes = sampleCount * format.bytesPerSample();
    if (pending.size() < requiredBytes)
        pending.append(input->read(requiredBytes - pending.size()));
    if (pending.size() < requiredBytes)
        return false;
    const bool valid = AudioCaptureQt6::convertSamples(
        QByteArrayView(pending.constData(), requiredBytes), format, sampleCount, output);
    pending.remove(0, requiredBytes);
    return valid;
}

}

AudioCaptureQt6::AudioCaptureQt6(QObject * parent)
    : AudioCaptureQt6(std::make_unique<QtInputBackend>(), parent)
{
    auto *devices = new QMediaDevices(this);
    connect(devices, &QMediaDevices::audioInputsChanged, this, &AudioCapture::restart);
}

AudioCaptureQt6::AudioCaptureQt6(std::unique_ptr<InputBackend> backend, QObject *parent)
    : AudioCapture(parent)
    , m_backend(std::move(backend))
{
}

AudioCaptureQt6::~AudioCaptureQt6()
{
    stop();
    Q_ASSERT(m_input == nullptr);
}

bool AudioCaptureQt6::initialize()
{
    QSettings settings;
    QString selection;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_selectionOverride)
            m_selection = settings.value(SETTINGS_AUDIO_INPUT_DEVICE).toString();
        selection = m_selection;
    }
    const QList<Device> devices = m_backend->devices();
    Device selected;
    for (const Device &device : devices)
    {
        const bool match = selection.isEmpty() ? device.isDefault
            : selection.startsWith(QStringLiteral("id:"))
                ? device.id == QByteArray::fromBase64(selection.mid(3).toLatin1())
                : device.description == selection;
        if (match)
        {
            selected = device;
            break;
        }
    }
    if (selected.id.isEmpty())
    {
        setStatus(Unavailable, selection.isEmpty() ? QStringLiteral("No default audio input")
                  : QStringLiteral("Selected input is unavailable: ") + selection);
        return false;
    }
    QAudioFormat format;
    m_input = m_backend->open(selected.id,
        settings.value(SETTINGS_AUDIO_INPUT_SRATE, AUDIO_DEFAULT_SAMPLE_RATE).toInt(),
        settings.value(SETTINGS_AUDIO_INPUT_CHANNELS, AUDIO_DEFAULT_CHANNELS).toInt(), format);
    if (!m_input || !format.isValid() || !isConvertibleSampleFormat(format.sampleFormat()))
    {
        m_backend->close();
        m_input = nullptr;
        setStatus(Error, QStringLiteral("Cannot open supported PCM on ") + selected.description);
        return false;
    }
    {
        QMutexLocker locker(&m_mutex);
        m_format = format;
        m_appliedDevice = QStringLiteral("id:") + QString::fromLatin1(selected.id.toBase64());
    }
    m_channels = format.channelCount();
    m_sampleRate = format.sampleRate();
    m_floatInput = true;
    m_appliedVolume = m_volume;
    m_backend->setVolume(m_appliedVolume);
    m_currentReadBuffer.clear();
    return true;
}

void AudioCaptureQt6::uninitialize()
{
    m_backend->close();
    m_input = nullptr;
    m_currentReadBuffer.clear();
    QMutexLocker locker(&m_mutex);
    m_appliedDevice.clear();
    m_format = {};
}

qint64 AudioCaptureQt6::latency() const
{
    return 0; // TODO
}

void AudioCaptureQt6::setVolume(qreal volume)
{
    if (!std::isfinite(volume))
        return;
    m_volume = std::clamp(volume, 0.0, 1.0);

    emit volumeChanged(volume * 100.0);
}

void AudioCaptureQt6::setInputDevice(const QString &selection)
{
    {
        QMutexLocker locker(&m_mutex);
        m_selection = selection;
        m_selectionOverride = true;
    }
    restart();
}

QString AudioCaptureQt6::inputDevice() const
{
    QMutexLocker locker(&m_mutex);
    return m_selection;
}

QString AudioCaptureQt6::appliedDevice() const
{
    QMutexLocker locker(&m_mutex);
    return m_appliedDevice;
}

QAudioFormat AudioCaptureQt6::captureFormat() const
{
    QMutexLocker locker(&m_mutex);
    return m_format;
}

void AudioCaptureQt6::suspend()
{
}

void AudioCaptureQt6::resume()
{
}

bool AudioCaptureQt6::readAudio(int maxSize)
{
    if (!m_input)
        return false;
    if (m_appliedVolume != m_volume)
    {
        m_appliedVolume = m_volume;
        m_backend->setVolume(m_appliedVolume);
    }
    const int bytesPerFrame = m_format.bytesPerFrame();
    const int maximumBytes = maxSize * m_format.bytesPerSample();
    m_currentReadBuffer.append(m_input->read(maximumBytes - m_currentReadBuffer.size()));
    m_readFrames = unsigned(m_currentReadBuffer.size() / bytesPerFrame);
    if (!m_readFrames)
        return false;
    const int completeBytes = int(m_readFrames) * bytesPerFrame;
    const bool valid = convertSamples(QByteArrayView(m_currentReadBuffer.constData(), completeBytes),
                                     m_format, int(m_readFrames * m_channels), m_floatBuffer.data());
    m_currentReadBuffer.remove(0, completeBytes);
    return valid;
}

bool AudioCaptureQt6::sourceFailed() const
{
    return m_backend->failed();
}

QAudioFormat AudioCaptureQt6::selectCaptureFormat(const QAudioDevice &device,
                                                  int sampleRate,
                                                  int channels)
{
    if (device.isNull() || sampleRate <= 0 || channels <= 0)
        return {};

    QAudioFormat requested;
    requested.setSampleRate(sampleRate);
    requested.setChannelCount(channels);
    requested.setSampleFormat(QAudioFormat::Int16);
    if (device.isFormatSupported(requested))
        return requested;

    const QAudioFormat preferred = device.preferredFormat();
    QAudioFormat preferredInt16 = preferred;
    preferredInt16.setSampleFormat(QAudioFormat::Int16);
    if (device.isFormatSupported(preferredInt16))
        return preferredInt16;

    if (preferred.isValid() && isConvertibleSampleFormat(preferred.sampleFormat())
        && device.isFormatSupported(preferred))
        return preferred;

    return {};
}

bool AudioCaptureQt6::convertSamples(QByteArrayView input,
                                     const QAudioFormat &format,
                                     int sampleCount,
                                     int16_t *output)
{
    if (output == nullptr || sampleCount <= 0
        || !isConvertibleSampleFormat(format.sampleFormat()))
    {
        return false;
    }

    const int bytesPerSample = format.bytesPerSample();
    if (bytesPerSample <= 0
        || sampleCount > std::numeric_limits<int>::max() / bytesPerSample
        || input.size() != qsizetype(sampleCount * bytesPerSample))
    {
        return false;
    }

    if (format.sampleFormat() == QAudioFormat::Int16)
    {
        std::memcpy(output, input.data(), size_t(input.size()));
        return true;
    }

    std::vector<int16_t> converted(static_cast<size_t>(sampleCount));
    for (int i = 0; i < sampleCount; i++)
    {
        float value;
        std::memcpy(&value, input.data() + qsizetype(i * sizeof(float)),
                    sizeof(value));
        if (!std::isfinite(value))
            return false;

        value = std::clamp(value, -1.0f, 1.0f);
        const double scale = value < 0.0f ? 32768.0 : 32767.0;
        converted[size_t(i)] = int16_t(std::lround(double(value) * scale));
    }

    std::memcpy(output, converted.data(), converted.size() * sizeof(int16_t));
    return true;
}

bool AudioCaptureQt6::readConvertedSamples(QIODevice *input,
                                           QByteArray &pending,
                                           const QAudioFormat &format,
                                           int sampleCount,
                                           int16_t *output)
{
    return readBlock(input, pending, format, sampleCount, output);
}

bool AudioCaptureQt6::convertSamples(QByteArrayView input, const QAudioFormat &format,
                                     int sampleCount, float *output)
{
    if (!output || !format.isValid() || sampleCount <= 0
        || !isConvertibleSampleFormat(format.sampleFormat())
        || qint64(sampleCount) * format.bytesPerSample() != input.size())
        return false;
    if (format.sampleFormat() == QAudioFormat::Float)
    {
        // Validate the complete block before touching the destination.
        for (int i = 0; i < sampleCount; ++i)
        {
            float value;
            std::memcpy(&value, input.data() + size_t(i) * sizeof(float), sizeof(value));
            if (!std::isfinite(value))
                return false;
        }
        for (int i = 0; i < sampleCount; ++i)
        {
            float value;
            std::memcpy(&value, input.data() + size_t(i) * sizeof(float), sizeof(value));
            output[i] = std::clamp(value, -1.0f, 1.0f);
        }
    }
    else
    {
        for (int i = 0; i < sampleCount; ++i)
        {
            int16_t value;
            std::memcpy(&value, input.data() + size_t(i) * sizeof(int16_t), sizeof(value));
            output[i] = float(value) / 32768.0f;
        }
    }
    return true;
}

bool AudioCaptureQt6::readConvertedSamples(QIODevice *input, QByteArray &pending,
                                           const QAudioFormat &format, int sampleCount,
                                           float *output)
{
    return readBlock(input, pending, format, sampleCount, output);
}
