/*
  Q Light Controller Plus
  vdjdeckmodel.cpp

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

#include "vdjdeckmodel.h"

static const int kThrottleMs = 33; // ~30 Hz

VdjDeckModel::VdjDeckModel(int deckNumber, QObject *parent)
    : QObject(parent)
    , m_deckNumber(deckNumber)
{
    m_throttle.setSingleShot(true);
    m_throttle.setInterval(kThrottleMs);
    connect(&m_throttle, &QTimer::timeout, this, &VdjDeckModel::flushDirty);
}

void VdjDeckModel::markDirty(DirtyFlag flag)
{
    m_dirty |= flag;
    if (!m_throttle.isActive())
        m_throttle.start();
}

void VdjDeckModel::flushDirty()
{
    int d = m_dirty;
    m_dirty = 0;

    if (d & DirtyMetadata)  emit metadataChanged();
    if (d & DirtyBpm)       emit bpmChanged();
    if (d & DirtyTransport) emit transportChanged();
    if (d & DirtyMixer)     emit mixerChanged();
    if (d & DirtyLoop)      emit loopChanged();
}

// --- Metadata setters ---

void VdjDeckModel::setFilepath(const QString &v)
{
    if (m_filepath != v) { m_filepath = v; markDirty(DirtyMetadata); }
}

void VdjDeckModel::setTitle(const QString &v)
{
    if (m_title != v) { m_title = v; markDirty(DirtyMetadata); }
}

void VdjDeckModel::setArtist(const QString &v)
{
    if (m_artist != v) { m_artist = v; markDirty(DirtyMetadata); }
}

void VdjDeckModel::setTitleArtist(const QString &v)
{
    if (m_titleArtist != v) { m_titleArtist = v; markDirty(DirtyMetadata); }
}

void VdjDeckModel::setAlbum(const QString &v)
{
    if (m_album != v) { m_album = v; markDirty(DirtyMetadata); }
}

void VdjDeckModel::setGenre(const QString &v)
{
    if (m_genre != v) { m_genre = v; markDirty(DirtyMetadata); }
}

void VdjDeckModel::setKey(const QString &v)
{
    if (m_key != v) { m_key = v; markDirty(DirtyMetadata); }
}

// --- BPM setters ---

void VdjDeckModel::setBpm(qreal v)
{
    if (!qFuzzyCompare(m_bpm, v)) { m_bpm = v; markDirty(DirtyBpm); }
}

void VdjDeckModel::setFirstBeat(qreal v)
{
    if (!qFuzzyCompare(m_firstBeat, v)) { m_firstBeat = v; markDirty(DirtyBpm); }
}

// --- Transport setters ---

void VdjDeckModel::setPosition(qreal v)
{
    if (!qFuzzyCompare(m_position, v)) { m_position = v; markDirty(DirtyTransport); }
}

void VdjDeckModel::setTimeRemaining(qreal v)
{
    if (!qFuzzyCompare(m_timeRemaining, v)) { m_timeRemaining = v; markDirty(DirtyTransport); }
}

void VdjDeckModel::setTimeTotal(qreal v)
{
    if (!qFuzzyCompare(m_timeTotal, v)) { m_timeTotal = v; markDirty(DirtyTransport); }
}

void VdjDeckModel::setTimeElapsed(qreal v)
{
    if (!qFuzzyCompare(m_timeElapsed, v)) { m_timeElapsed = v; markDirty(DirtyTransport); }
}

void VdjDeckModel::setBeatPos(qreal v)
{
    if (!qFuzzyCompare(m_beatPos, v)) { m_beatPos = v; markDirty(DirtyTransport); }
}

void VdjDeckModel::setPlaying(bool v)
{
    if (m_playing != v) { m_playing = v; markDirty(DirtyTransport); }
}

void VdjDeckModel::setLoaded(bool v)
{
    if (m_loaded != v) { m_loaded = v; markDirty(DirtyTransport); }
}

// --- Mixer setters ---

void VdjDeckModel::setVolume(qreal v)
{
    if (!qFuzzyCompare(m_volume, v)) { m_volume = v; markDirty(DirtyMixer); }
}

void VdjDeckModel::setEqHigh(qreal v)
{
    if (!qFuzzyCompare(m_eqHigh, v)) { m_eqHigh = v; markDirty(DirtyMixer); }
}

void VdjDeckModel::setEqMed(qreal v)
{
    if (!qFuzzyCompare(m_eqMed, v)) { m_eqMed = v; markDirty(DirtyMixer); }
}

void VdjDeckModel::setEqLow(qreal v)
{
    if (!qFuzzyCompare(m_eqLow, v)) { m_eqLow = v; markDirty(DirtyMixer); }
}

void VdjDeckModel::setGain(qreal v)
{
    if (!qFuzzyCompare(m_gain, v)) { m_gain = v; markDirty(DirtyMixer); }
}

void VdjDeckModel::setVu(qreal v)
{
    if (!qFuzzyCompare(m_vu, v)) { m_vu = v; markDirty(DirtyMixer); }
}

void VdjDeckModel::setLevel(qreal v)
{
    if (!qFuzzyCompare(m_level, v)) { m_level = v; markDirty(DirtyMixer); }
}

// --- Loop setters ---

void VdjDeckModel::setLooping(bool v)
{
    if (m_looping != v) { m_looping = v; markDirty(DirtyLoop); }
}

void VdjDeckModel::setLoopLength(qreal v)
{
    if (!qFuzzyCompare(m_loopLength, v)) { m_loopLength = v; markDirty(DirtyLoop); }
}

// --- Reset ---

void VdjDeckModel::reset()
{
    m_filepath.clear();
    m_title.clear();
    m_artist.clear();
    m_titleArtist.clear();
    m_album.clear();
    m_genre.clear();
    m_key.clear();

    m_bpm = 0.0;
    m_firstBeat = 0.0;

    m_position = 0.0;
    m_timeRemaining = 0.0;
    m_timeTotal = 0.0;
    m_timeElapsed = 0.0;
    m_beatPos = 0.0;
    m_playing = false;
    m_loaded = false;

    m_volume = 0.0;
    m_eqHigh = 0.0;
    m_eqMed = 0.0;
    m_eqLow = 0.0;
    m_gain = 0.0;
    m_vu = 0.0;
    m_level = 0.0;

    m_looping = false;
    m_loopLength = 0.0;

    // Flush all groups immediately on reset
    m_dirty = 0;
    m_throttle.stop();
    emit metadataChanged();
    emit bpmChanged();
    emit transportChanged();
    emit mixerChanged();
    emit loopChanged();
}
