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

}

AudioCaptureQt6::AudioCaptureQt6(QObject * parent)
    : AudioCapture(parent)
    , m_audioSource(NULL)
    , m_input(NULL)
{
}

AudioCaptureQt6::~AudioCaptureQt6()
{
    stop();
    Q_ASSERT(m_audioSource == NULL);
}

bool AudioCaptureQt6::initialize()
{
    QSettings settings;
    QString devName = "";
    QAudioDevice audioDevice = QMediaDevices::defaultAudioInput();

    QVariant var = settings.value(SETTINGS_AUDIO_INPUT_DEVICE);
    if (var.isValid() == true)
    {
        devName = var.toString();
        foreach (const QAudioDevice &deviceInfo, QMediaDevices::audioInputs())
        {
            if (deviceInfo.description() == devName)
            {
                audioDevice = deviceInfo;
                break;
            }
        }
    }

    QAudioFormat requestedFormat;
    requestedFormat.setSampleRate(m_sampleRate);
    requestedFormat.setChannelCount(m_channels);
    requestedFormat.setSampleFormat(QAudioFormat::Int16);
    m_format = selectCaptureFormat(audioDevice, m_sampleRate, m_channels);
    if (!m_format.isValid())
    {
        qWarning() << "No supported audio capture format for device"
                   << audioDevice.description();
        return false;
    }
    if (m_format != requestedFormat)
        qWarning() << "Requested format not supported - using" << m_format;

    m_channels = m_format.channelCount();
    m_sampleRate = m_format.sampleRate();

    Q_ASSERT(m_audioSource == NULL);

    m_audioSource = new QAudioSource(audioDevice, m_format);

    if (m_audioSource == NULL)
    {
        qWarning() << "Cannot open audio input stream from device" << audioDevice.description();
        return false;
    }

    m_input = m_audioSource->start();

    if (m_audioSource->state() == QAudio::StoppedState)
    {
        qWarning() << "Could not start input capture on device" << audioDevice.description();
        delete m_audioSource;
        m_audioSource = NULL;
        m_input = NULL;
        return false;
    }

    m_currentReadBuffer.clear();

    return true;
}

void AudioCaptureQt6::uninitialize()
{
    Q_ASSERT(m_audioSource != NULL);

    m_audioSource->stop();
    delete m_audioSource;
    m_audioSource = NULL;
}

qint64 AudioCaptureQt6::latency() const
{
    return 0; // TODO
}

void AudioCaptureQt6::setVolume(qreal volume)
{
    if (volume == m_volume)
        return;

    m_volume = volume;
    if (m_audioSource != NULL)
        m_audioSource->setVolume(volume);

    emit volumeChanged(volume * 100.0);
}

void AudioCaptureQt6::suspend()
{
}

void AudioCaptureQt6::resume()
{
}

bool AudioCaptureQt6::readAudio(int maxSize)
{
    if (m_audioSource == NULL || m_input == NULL)
        return false;

    return readConvertedSamples(m_input, m_currentReadBuffer, m_format,
                                maxSize, m_audioBuffer);
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

    if (preferred.isValid() && isConvertibleSampleFormat(preferred.sampleFormat()))
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
    if (input == nullptr || output == nullptr || !format.isValid()
        || sampleCount <= 0 || format.channelCount() <= 0
        || sampleCount % format.channelCount() != 0
        || !isConvertibleSampleFormat(format.sampleFormat()))
    {
        return false;
    }

    const int frames = sampleCount / format.channelCount();
    const int requiredBytes = format.bytesForFrames(frames);
    if (requiredBytes <= 0
        || qint64(requiredBytes)
            != qint64(sampleCount) * format.bytesPerSample())
    {
        return false;
    }

    pending.append(input->readAll());
    if (pending.size() < requiredBytes)
        return false;

    const QByteArrayView block(pending.constData(), requiredBytes);
    const bool converted = convertSamples(block, format, sampleCount, output);
    pending.remove(0, requiredBytes);
    return converted;
}
