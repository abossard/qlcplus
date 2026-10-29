/*
  Q Light Controller Plus
  djsetrecorder.cpp

  Copyright (c) QLC+ contributors

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

#include "djsetrecorder.h"
#include "showfactory.h"

#include "doc.h"
#include "show.h"
#include "track.h"
#include "function.h"
#include "mastertimer.h"
#include "showfunction.h"

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>

#include <algorithm>
#include <limits>
#include <memory>

namespace
{
const quint32 kOpenInterval = std::numeric_limits<quint32>::max();

/** A segment replaces the ones that do not start before it */
void appendSegment(QVector<ShowClipSegment> &segments, quint32 clipMs, quint32 songMs, qreal rate)
{
    while (segments.isEmpty() == false && segments.last().clipMs >= clipMs)
        segments.removeLast();
    ShowClipSegment segment;
    segment.clipMs = clipMs;
    segment.songMs = songMs;
    segment.rate = rate;
    segments.append(segment);
}

/** A sample in the millisecond of the one before replaces it: the later write is the newer view */
template <typename Sample>
void appendSample(QVector<Sample> &samples, const Sample &sample)
{
    if (samples.isEmpty() == false && samples.last().clipMs == sample.clipMs)
        samples.last() = sample;
    else
        samples.append(sample);
}

QString trackName(int deck)
{
    return QStringLiteral("Deck %1").arg(deck);
}
} // namespace

DjSetRecorder::DjSetRecorder(Doc *doc, DjFsm *fsm, ShowFactory *factory, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
    , m_fsm(fsm)
    , m_factory(factory)
    , m_setId(Function::invalidId())
{
    auto started = std::make_shared<QElapsedTimer>();
    started->start();
    m_clock = [started]() { return started->elapsed(); };

    m_updateTimer.setInterval(kUpdateIntervalMs);
    connect(&m_updateTimer, &QTimer::timeout, this, &DjSetRecorder::grow);
    connect(m_fsm, &DjFsm::settled, this, &DjSetRecorder::follow);
    // a workspace clear removes every Function too
    connect(m_doc, &Doc::functionRemoved, this, [this](quint32 id) {
        // its clips go with it: the id can come back as another Function
        m_closed.erase(std::remove_if(m_closed.begin(), m_closed.end(),
                                      [id](const ClosedClip &closed) { return closed.setId == id; }),
                       m_closed.end());
        if (id == m_setId)
            forgetSet();
        updateTimer();
    });
}

void DjSetRecorder::setClock(const Clock &clock)
{
    m_clock = clock;
}

qint64 DjSetRecorder::now() const
{
    return m_clock();
}

QString DjSetRecorder::setName(const QDateTime &start)
{
    return QStringLiteral("Set ") + start.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

void DjSetRecorder::setRecording(bool on)
{
    if (on == m_recording)
        return;
    m_recording = on;
    // the next settled view opens the playing decks' clips
    if (on)
        return;
    const qint64 clock = now();
    for (int i = 0; i < DjFsm::MaxDecks; ++i)
    {
        if (m_clips[i])
            close(i, clock);
    }
    write();
    updateTimer();
    if (m_setId == Function::invalidId())
        return;
    // the set is complete: the next Perform records another one
    m_setId = Function::invalidId();
    m_doc->setModified();
}

void DjSetRecorder::follow()
{
    const qint64 clock = now();
    bool changed = false;
    for (int i = 0; i < DjFsm::MaxDecks; ++i)
    {
        const DjFsm::Deck &deck = m_fsm->deckAt(i);
        const bool tracked = i < m_fsm->deckCount();
        const QString filepath = deck.song.isValid() ? ShowFactory::normalizePath(deck.song.filepath) : QString();
        const quint32 showId = m_factory->showIdForFilepath(filepath);
        std::optional<DeckClip> &clip = m_clips[i];

        // a song stay ends with its song, the song's Show (as live Perform switches), its deck or Perform
        if (clip && (m_recording == false || tracked == false || filepath != clip->filepath
                     || showId != clip->showId))
        {
            close(i, clock);
            changed = true;
        }

        const bool active = m_fsm->activeDeck() == i + 1;
        if (clip)
        {
            sample(*clip, deck, active, quint32(clock - clip->startClock));
            continue;
        }

        // a stay starts at its song's play, from the song's own first sample
        if (m_recording == false || tracked == false || deck.playing == false || deck.positionStale)
            continue;
        const Show *song = qobject_cast<Show *>(m_doc->function(showId));
        // a song that plays the set would hold the set in itself
        if (song == nullptr || (m_setId != Function::invalidId() && song->contains(m_setId))
            || ensureSet(clock) == nullptr)
            continue;

        DeckClip opened;
        opened.filepath = filepath;
        opened.showId = showId;
        opened.startClock = clock;
        opened.startMs = quint32(clock - m_setStartClock);
        opened.run.append({ 0, quint32(qMax(0, deck.elapsedMs)) });
        opened.levels.append({ 0, deck.weight });
        if (active)
            opened.active.append({ 0, kOpenInterval });
        clip = opened;
        changed = true;
    }

    // a clip's growth waits for the timer; an opened or closed one goes at once
    if (changed)
        write();
    updateTimer();
}

void DjSetRecorder::grow()
{
    // the clock only: the decks are read at settled views
    const qint64 clock = now();
    for (std::optional<DeckClip> &clip : m_clips)
    {
        if (clip)
            clip->endMs = quint32(clock - clip->startClock);
    }
    write();
    updateTimer();
}

void DjSetRecorder::close(int deck, qint64 clock)
{
    DeckClip &clip = *m_clips[deck];
    clip.endMs = quint32(clock - clip.startClock);
    closeRun(clip);
    m_closed.append({ m_setId, deck, clip });
    m_clips[deck].reset();
}

bool DjSetRecorder::pending() const
{
    return m_closed.isEmpty() == false
           || std::any_of(m_clips.begin(), m_clips.end(), [](const auto &c) { return c.has_value(); });
}

void DjSetRecorder::updateTimer()
{
    if (pending() && m_updateTimer.isActive() == false)
        m_updateTimer.start();
    else if (pending() == false)
        m_updateTimer.stop();
}

void DjSetRecorder::sample(DeckClip &clip, const DjFsm::Deck &deck, bool active, quint32 clipMs) const
{
    clip.endMs = clipMs;
    const quint32 position = quint32(qMax(0, deck.elapsedMs));
    if (deck.playing)
    {
        // a resume starts a play run where the pause held; a still
        // position while playing (a held jog) holds as well
        clip.playing = true;
        appendSample(clip.run, ShowClipPositionSample{ clipMs, position });
    }
    else if (clip.playing)
    {
        // a pause: the run ends at the deck's position, held from here on
        appendSample(clip.run, ShowClipPositionSample{ clipMs, position });
        closeRun(clip);
        appendSegment(clip.segments, clipMs, position, 0.0);
        clip.playing = false;
    }
    else if (position != clip.segments.last().songMs)
    {
        // a cue or a seek while paused
        appendSegment(clip.segments, clipMs, position, 0.0);
    }

    // the weight steps at a write: the old one holds up to it
    const qreal level = deck.weight;
    if (level != clip.levels.last().level)
    {
        const ShowClipLevelSample last = clip.levels.last();
        if (last.clipMs + 1 < clipMs)
            clip.levels.append({ clipMs - 1, last.level });
        appendSample(clip.levels, ShowClipLevelSample{ clipMs, level });
    }

    const bool open = clip.active.isEmpty() == false && clip.active.last().toMs == kOpenInterval;
    if (active && open == false)
        clip.active.append({ clipMs, kOpenInterval });
    else if (active == false && open)
        clip.active.last().toMs = clipMs;
}

void DjSetRecorder::closeRun(DeckClip &clip) const
{
    for (const ShowClipSegment &segment : ShowClipAutomation::segmentsFromSamples(clip.run, kPositionToleranceMs))
        appendSegment(clip.segments, segment.clipMs, segment.songMs, segment.rate);
    clip.run.clear();
}

ShowClipAutomation DjSetRecorder::automation(const DeckClip &clip) const
{
    QVector<ShowClipSegment> segments = clip.segments;
    for (const ShowClipSegment &segment : ShowClipAutomation::segmentsFromSamples(clip.run, kPositionToleranceMs))
        appendSegment(segments, segment.clipMs, segment.songMs, segment.rate);

    ShowClipAutomation automation;
    automation.setSegments(segments);
    automation.setEnvelope(ShowClipAutomation::envelopeFromSamples(clip.levels, kLevelTolerance));
    automation.setDominance(ShowClipAutomation::dominanceFromIntervals(clip.active));
    return automation;
}

Show *DjSetRecorder::set() const
{
    return qobject_cast<Show *>(m_doc->function(m_setId));
}

Show *DjSetRecorder::ensureSet(qint64 clock)
{
    if (Show *existing = set())
        return existing;

    Show *show = new Show(m_doc);
    show->setName(setName(QDateTime::currentDateTime()));
    show->setPath(kSetFolderPath);
    // the songs' Audio is VirtualDJ's: the set plays their light only
    show->setNestedAudioSuppressed(true);
    if (m_doc->addFunction(show) == false)
    {
        delete show;
        return nullptr;
    }
    m_setId = show->id();
    m_setStartClock = clock;
    for (int deck = 1; deck <= m_fsm->deckCount(); ++deck)
        deckTrack(show, deck);
    qDebug() << "[DjSetRecorder] recording" << show->name();
    return show;
}

Track *DjSetRecorder::deckTrack(Show *set, int deck) const
{
    for (Track *track : set->tracks())
    {
        if (track->name() == trackName(deck))
            return track;
    }
    // the decks before it come first
    if (deck > 1)
        deckTrack(set, deck - 1);
    Track *track = new Track(Function::invalidId(), set);
    track->setName(trackName(deck));
    if (set->addTrack(track) == false)
    {
        delete track;
        return nullptr;
    }
    return track;
}

bool DjSetRecorder::writeClip(quint32 setId, int deck, DeckClip &clip)
{
    Show *set = qobject_cast<Show *>(m_doc->function(setId));
    // the set left the Doc, or the clip the set: nothing to write
    if (set == nullptr)
        return true;
    ShowFunction *sf = clip.uid ? set->showFunction(*clip.uid) : nullptr;
    if (clip.uid && sf == nullptr)
        return true;

    if (sf == nullptr)
    {
        // a running set's clips are its runner's: a new one waits for the stop
        MasterTimer *timer = m_doc->masterTimer();
        MasterTimer::EditAdmission admission(timer, timer->beginFunctionEdit(set));
        if (!admission)
            return false;
        Track *track = deckTrack(set, deck + 1);
        if (track == nullptr)
            return true;
        sf = track->createShowFunction(clip.showId);
        sf->setStartTime(clip.startMs);
        sf->setColor(ShowFunction::defaultColor(Function::ShowType));
        clip.uid = sf->id();
    }
    // 0 would be the song Show's own duration
    sf->setDuration(qMax(1u, clip.endMs));
    sf->setClipAutomation(automation(clip));
    return true;
}

void DjSetRecorder::write()
{
    if (pending() == false)
        return;
    for (auto it = m_closed.begin(); it != m_closed.end();)
    {
        if (writeClip(it->setId, it->deck, it->clip))
            it = m_closed.erase(it);
        else
            ++it;
    }
    for (int i = 0; i < DjFsm::MaxDecks; ++i)
    {
        if (m_clips[i])
            writeClip(m_setId, i, *m_clips[i]);
    }
    // the set changed since a save; a modified Doc keeps its autosave timer
    if (m_doc->isModified() == false)
        m_doc->setModified();
}

void DjSetRecorder::forgetSet()
{
    m_setId = Function::invalidId();
    for (std::optional<DeckClip> &clip : m_clips)
        clip.reset();
}
