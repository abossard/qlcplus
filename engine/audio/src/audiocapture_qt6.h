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

/** @addtogroup engine_audio Audio
 * @{
 */

class AudioCaptureQt6 final : public AudioCapture
{
    Q_OBJECT
public:
    AudioCaptureQt6(QObject * parent = 0);
    ~AudioCaptureQt6();

    /** @reimpl */
    qint64 latency() const override;

    /** @reimpl */
    void setVolume(qreal volume) override;

    static QAudioFormat selectCaptureFormat(const QAudioDevice &device,
                                            int sampleRate,
                                            int channels);
    static bool convertSamples(QByteArrayView input,
                               const QAudioFormat &format,
                               int sampleCount,
                               int16_t *output);
    static bool readConvertedSamples(QIODevice *input,
                                     QByteArray &pending,
                                     const QAudioFormat &format,
                                     int sampleCount,
                                     int16_t *output);

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

private:
    QAudioSource *m_audioSource;
    QIODevice *m_input;
    QAudioFormat m_format;
    qreal m_volume;
    QByteArray m_currentReadBuffer;
};

/** @} */

#endif // AUDIOCAPTURE_QT6_H
