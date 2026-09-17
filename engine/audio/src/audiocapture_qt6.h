/*
  Q Light Controller Plus
  audiocapture_qt6.h

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

#ifndef AUDIOCAPTURE_QT6_H
#define AUDIOCAPTURE_QT6_H

#include "audiocapture.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QAudioFormat>
#include <QByteArrayView>
#include <QVariantMap>

/** @addtogroup engine_audio Audio
 * @{
 */

class AudioCaptureQt6 final : public AudioCapture
{
    Q_OBJECT
public:
    struct Device
    {
        QByteArray id;
        QString description;
        bool isDefault = false;
    };

    class InputBackend
    {
    public:
        virtual ~InputBackend() = default;
        virtual QList<Device> devices() const = 0;
        virtual QIODevice *open(const QByteArray &id, int sampleRate, int channels,
                                QAudioFormat &format) = 0;
        virtual bool failed() const = 0;
        virtual void close() = 0;
        virtual void setVolume(qreal volume) = 0;
        // Optional capability: old backends explicitly report unknown rather
        // than claiming a requested buffer was applied.
        virtual void setBufferRequestMs(int ms) { Q_UNUSED(ms) }
        virtual int appliedBufferRequestMs() const { return -1; }
        virtual qint64 bufferCapacityBytes() const { return -1; }
        virtual qint64 queuedBytes() const { return -1; }
    };

    AudioCaptureQt6(QObject * parent = 0);
    AudioCaptureQt6(std::unique_ptr<InputBackend> backend, QObject *parent = nullptr);
    ~AudioCaptureQt6();

    /** @reimpl */
    qint64 latency() const override;

    /** @reimpl */
    void setVolume(qreal volume) override;
    /** Empty follows the OS default. IDs use "id:" plus base64; descriptions
     *  remain accepted for existing settings. Does not write settings. */
    void setInputDevice(const QString &selection);
    QString inputDevice() const;
    QString appliedDevice() const;
    QAudioFormat captureFormat() const;
    QVariantMap inputDiagnostics() const;

    static QAudioFormat selectCaptureFormat(const QAudioDevice &device,
                                            int sampleRate,
                                            int channels);
    static bool convertSamples(QByteArrayView input,
                               const QAudioFormat &format,
                               int sampleCount,
                               int16_t *output);
    static bool convertSamples(QByteArrayView input, const QAudioFormat &format,
                               int sampleCount, float *output);
    static bool readConvertedSamples(QIODevice *input,
                                     QByteArray &pending,
                                     const QAudioFormat &format,
                                     int sampleCount,
                                     int16_t *output);
    static bool readConvertedSamples(QIODevice *input, QByteArray &pending,
                                     const QAudioFormat &format, int sampleCount,
                                     float *output);

protected:
    /** @reimpl */
    bool initialize() override;

    /** @reimpl */
    virtual void uninitialize() override;

    /** @reimpl */
    void suspend() override;

    /** @reimpl */
    void resume() override;

    /** @reimpl */
    bool readAudio(int maxSize) override;
    bool sourceFailed() const override;
    bool retrySource() const override { return true; }
    void clearPendingInput() override { m_currentReadBuffer.clear(); }

private:
    void updateInputDiagnostics(bool received);
    int m_appliedBufferRequestMs = -1;
    qint64 m_capacityBytes = -1;
    qint64 m_queuedBytes = -1;
    qint64 m_lastPcmNs = 0;
    std::unique_ptr<InputBackend> m_backend;
    QIODevice *m_input = nullptr;
    QAudioFormat m_format;
    std::atomic<qreal> m_volume {1.0};
    qreal m_appliedVolume = -1.0;
    QString m_selection;
    QString m_appliedDevice;
    bool m_selectionOverride = false;
    QByteArray m_currentReadBuffer;
};

/** @} */

#endif // AUDIOCAPTURE_QT6_H
