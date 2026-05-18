/*
  Q Light Controller Plus
  vdjdeckmodel.h

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

#ifndef VDJDECKMODEL_H
#define VDJDECKMODEL_H

#include <QObject>
#include <QTimer>

/**
 * Per-deck telemetry model for VirtualDJ integration.
 *
 * Holds all per-deck properties received from the DMXDesktop telemetry
 * protocol. NOTIFY signals are grouped to minimize QML binding churn:
 *
 *  - metadataChanged():  title, artist, album, genre, key, filepath, titleArtist
 *  - transportChanged(): position, timeRemaining, timeTotal, timeElapsed,
 *                         beatPos, playing, loaded
 *  - mixerChanged():     volume, eqHigh, eqMed, eqLow, gain, vu, level
 *  - loopChanged():      looping, loopLength
 *  - bpmChanged():       bpm, firstBeat
 *
 * The throttle timer coalesces rapid property writes into a single
 * NOTIFY emission at ~30 Hz per group.
 */
class VdjDeckModel : public QObject
{
    Q_OBJECT

    // --- Metadata (on-load) ---
    Q_PROPERTY(QString filepath READ filepath NOTIFY metadataChanged)
    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(QString titleArtist READ titleArtist NOTIFY metadataChanged)
    Q_PROPERTY(QString album READ album NOTIFY metadataChanged)
    Q_PROPERTY(QString genre READ genre NOTIFY metadataChanged)
    Q_PROPERTY(QString key READ key NOTIFY metadataChanged)

    // --- BPM ---
    Q_PROPERTY(qreal bpm READ bpm NOTIFY bpmChanged)
    Q_PROPERTY(qreal firstBeat READ firstBeat NOTIFY bpmChanged)

    // --- Transport (continuous) ---
    Q_PROPERTY(qreal position READ position NOTIFY transportChanged)
    Q_PROPERTY(qreal timeRemaining READ timeRemaining NOTIFY transportChanged)
    Q_PROPERTY(qreal timeTotal READ timeTotal NOTIFY transportChanged)
    Q_PROPERTY(qreal timeElapsed READ timeElapsed NOTIFY transportChanged)
    Q_PROPERTY(qreal beatPos READ beatPos NOTIFY transportChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY transportChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY transportChanged)

    // --- Mixer ---
    Q_PROPERTY(qreal volume READ volume NOTIFY mixerChanged)
    Q_PROPERTY(qreal eqHigh READ eqHigh NOTIFY mixerChanged)
    Q_PROPERTY(qreal eqMed READ eqMed NOTIFY mixerChanged)
    Q_PROPERTY(qreal eqLow READ eqLow NOTIFY mixerChanged)
    Q_PROPERTY(qreal gain READ gain NOTIFY mixerChanged)
    Q_PROPERTY(qreal vu READ vu NOTIFY mixerChanged)
    Q_PROPERTY(qreal level READ level NOTIFY mixerChanged)

    // --- Loop ---
    Q_PROPERTY(bool looping READ looping NOTIFY loopChanged)
    Q_PROPERTY(qreal loopLength READ loopLength NOTIFY loopChanged)

public:
    explicit VdjDeckModel(int deckNumber, QObject *parent = nullptr);

    int deckNumber() const { return m_deckNumber; }

    // --- Getters ---
    QString filepath() const { return m_filepath; }
    QString title() const { return m_title; }
    QString artist() const { return m_artist; }
    QString titleArtist() const { return m_titleArtist; }
    QString album() const { return m_album; }
    QString genre() const { return m_genre; }
    QString key() const { return m_key; }

    qreal bpm() const { return m_bpm; }
    qreal firstBeat() const { return m_firstBeat; }

    qreal position() const { return m_position; }
    qreal timeRemaining() const { return m_timeRemaining; }
    qreal timeTotal() const { return m_timeTotal; }
    qreal timeElapsed() const { return m_timeElapsed; }
    qreal beatPos() const { return m_beatPos; }
    bool playing() const { return m_playing; }
    bool loaded() const { return m_loaded; }

    qreal volume() const { return m_volume; }
    qreal eqHigh() const { return m_eqHigh; }
    qreal eqMed() const { return m_eqMed; }
    qreal eqLow() const { return m_eqLow; }
    qreal gain() const { return m_gain; }
    qreal vu() const { return m_vu; }
    qreal level() const { return m_level; }

    bool looping() const { return m_looping; }
    qreal loopLength() const { return m_loopLength; }

    // --- Setters (called by VdjTelemetryClient parser) ---
    void setFilepath(const QString &v);
    void setTitle(const QString &v);
    void setArtist(const QString &v);
    void setTitleArtist(const QString &v);
    void setAlbum(const QString &v);
    void setGenre(const QString &v);
    void setKey(const QString &v);

    void setBpm(qreal v);
    void setFirstBeat(qreal v);

    void setPosition(qreal v);
    void setTimeRemaining(qreal v);
    void setTimeTotal(qreal v);
    void setTimeElapsed(qreal v);
    void setBeatPos(qreal v);
    void setPlaying(bool v);
    void setLoaded(bool v);

    void setVolume(qreal v);
    void setEqHigh(qreal v);
    void setEqMed(qreal v);
    void setEqLow(qreal v);
    void setGain(qreal v);
    void setVu(qreal v);
    void setLevel(qreal v);

    void setLooping(bool v);
    void setLoopLength(qreal v);

    /** Reset all fields to defaults (called on client disconnect). */
    void reset();

signals:
    void metadataChanged();
    void bpmChanged();
    void transportChanged();
    void mixerChanged();
    void loopChanged();

private:
    enum DirtyFlag {
        DirtyMetadata  = 0x01,
        DirtyBpm       = 0x02,
        DirtyTransport = 0x04,
        DirtyMixer     = 0x08,
        DirtyLoop      = 0x10,
    };

    void markDirty(DirtyFlag flag);
    void flushDirty();

    int m_deckNumber;
    int m_dirty = 0;
    QTimer m_throttle;

    // Metadata
    QString m_filepath;
    QString m_title;
    QString m_artist;
    QString m_titleArtist;
    QString m_album;
    QString m_genre;
    QString m_key;

    // BPM
    qreal m_bpm = 0.0;
    qreal m_firstBeat = 0.0;

    // Transport
    qreal m_position = 0.0;
    qreal m_timeRemaining = 0.0;
    qreal m_timeTotal = 0.0;
    qreal m_timeElapsed = 0.0;
    qreal m_beatPos = 0.0;
    bool m_playing = false;
    bool m_loaded = false;

    // Mixer
    qreal m_volume = 0.0;
    qreal m_eqHigh = 0.0;
    qreal m_eqMed = 0.0;
    qreal m_eqLow = 0.0;
    qreal m_gain = 0.0;
    qreal m_vu = 0.0;
    qreal m_level = 0.0;

    // Loop
    bool m_looping = false;
    qreal m_loopLength = 0.0;
};

#endif // VDJDECKMODEL_H
