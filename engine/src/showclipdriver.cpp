/*
  Q Light Controller Plus
  showclipdriver.cpp

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

#include <QMutex>
#include <QSet>

#include "showclipdriver.h"
#include "showfunction.h"
#include "showrunner.h"
#include "function.h"
#include "track.h"
#include "show.h"
#include "doc.h"

namespace
{
const qreal kSameLevel = 1e-9;

/** The levels the clips of each Show wrote on a Function's Intensity
 *  override, the latest last */
class ClipWriters
{
public:
    void wrote(const Function *f, const ShowClipDriver *writer, qreal level)
    {
        QMutexLocker locker(&m_mutex);
        QVector<Write> &writes = m_writes[f];
        remove(writes, writer);
        writes.append({ writer, level });
    }

    /** writer no longer plays f: when it wrote last and another one plays
     *  f on, its own level and the one to write back */
    std::optional<QPair<qreal, qreal>> left(const Function *f, const ShowClipDriver *writer)
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_writes.find(f);
        if (it == m_writes.end())
            return std::nullopt;
        std::optional<QPair<qreal, qreal>> back;
        if (it->count() > 1 && it->last().writer == writer)
            back = qMakePair(it->last().level, it->at(it->count() - 2).level);
        remove(*it, writer);
        if (it->isEmpty())
            m_writes.erase(it);
        return back;
    }

    void forget(const ShowClipDriver *writer)
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_writes.begin(); it != m_writes.end(); )
        {
            remove(*it, writer);
            it = it->isEmpty() ? m_writes.erase(it) : std::next(it);
        }
    }

private:
    struct Write
    {
        const ShowClipDriver *writer;
        qreal level;
    };

    static void remove(QVector<Write> &writes, const ShowClipDriver *writer)
    {
        writes.removeIf([writer](const Write &w) { return w.writer == writer; });
    }

    QMutex m_mutex;
    QHash<const Function *, QVector<Write>> m_writes;
};

ClipWriters &clipWriters()
{
    static ClipWriters writers;
    return writers;
}
}

ShowClipDriver::ShowClipDriver(const Doc *doc, quint32 showId)
    : m_doc(doc)
    , m_show(qobject_cast<Show *>(doc->function(showId)))
{
}

ShowClipDriver::~ShowClipDriver()
{
    clipWriters().forget(this);
}

ShowClipDriver::Clip ShowClipDriver::clipOf(const ShowFunction *clip) const
{
    Clip c;
    c.clip = clip;
    c.automation = clip->clipAutomation();
    c.trackId = Track::invalidId();
    for (Track *track : m_show->tracks())
    {
        for (const ShowFunction *sf : track->showFunctions())
        {
            if (sf == clip)
                c.trackId = track->id();
        }
    }
    return c;
}

ShowClipDriver::Drive &ShowClipDriver::adopt(const ShowFunction *clip, Function *f, quint32 at)
{
    Drive drive;
    drive.function = f;
    drive.clips.append(clipOf(clip));
    drive.at = at;
    drive.nested = f->type() == Function::ShowType ? qobject_cast<Show *>(f) : nullptr;
    // a clip of it that ended this tick has not given anything back yet
    const auto ended = m_drives.constFind(f->id());
    const bool restarting = f->isRunning() && f->stopped();
    if (ended != m_drives.constEnd() && restarting)
    {
        // a clip right behind one that just ended keeps what commands set,
        // while this Show was the one playing it
        for (const Clip &before : ended->clips)
        {
            if (ended->owned && before.clip->startTime() + before.clip->duration(m_doc) == clip->startTime())
                drive.command = ended->command;
        }
    }
    // what this Show's commands set on a Function they already play stays,
    // multiplied by the clip
    else if (drive.nested == nullptr && f->isRunning() && f->hasSource(parent()))
    {
        drive.command = intensityOverride(f);
    }

    if (drive.nested != nullptr)
    {
        if (ended != m_drives.constEnd() && ended->nested != nullptr)
        {
            drive.saved = ended->saved;
            drive.audioSuppressed = ended->audioSuppressed;
        }
        else
        {
            drive.saved.syncSource = drive.nested->syncSource();
            drive.saved.audioSuppressed = drive.nested->performAudioSuppressed();
            drive.saved.masterIntensity = drive.nested->masterIntensity();
            drive.saved.dominant = drive.nested->isDominant();
        }
        drive.nested->setSyncSource(ShowRunner::External);
        // a set whose songs VirtualDJ plays, or a Show Perform plays itself
        if (m_show->nestedAudioSuppressed() || m_show->performAudioSuppressed())
        {
            drive.audioSuppressed = true;
            drive.nested->setPerformAudioSuppressed(true);
        }
    }

    return m_drives.insert(f->id(), drive).value();
}

void ShowClipDriver::start(const ShowFunction *clip, Function *f, quint32 at,
                           const QMap<quint32, qreal> &trackIntensities)
{
    // the timer restarts it at the end of this tick: the level survives that
    const bool nested = f->type() == Function::ShowType;
    if (nested == false && f->isRunning() && f->stopped())
        f->stop(parent(), true);
    if (nested && performed(f))
        return;

    Drive &drive = adopt(clip, f, at);
    applyLevel(drive, trackIntensities);
    const ShowClipAutomation &automation = drive.clips.first().automation;
    // the clock follows in drive(), after every clip of this tick joined: the
    // first sample after the start is where the nested traversal begins
    f->start(m_doc->masterTimer(), parent(), nested ? automation.songPositionAt(at) : automation.inPoint() + at);
}

void ShowClipDriver::join(const ShowFunction *clip, Function *f)
{
    auto it = m_drives.find(f->id());
    // one this Show plays again, through a command, after somebody else stopped it
    if (it == m_drives.end())
    {
        if (f->hasSource(parent()))
            adopt(clip, f, 0);
        return;
    }

    for (const Clip &c : std::as_const(it->clips))
    {
        if (c.clip == clip)
            return;
    }
    it->clips.append(clipOf(clip));
}

void ShowClipDriver::drive(quint32 elapsedTime, quint32 elapsedBeats, const QMap<quint32, qreal> &trackIntensities,
                           const QList<QPair<Function *, quint32>> &running)
{
    QSet<const Function *> queued;
    for (const QPair<Function *, quint32> &entry : running)
        queued.insert(entry.first);

    for (auto it = m_drives.begin(); it != m_drives.end(); )
    {
        Drive &drive = it.value();
        // with what they set on it, paused by no clip
        if (drive.nested != nullptr && performed(drive.nested))
        {
            drive.nested->setClipPaused(false);
            it = m_drives.erase(it);
            continue;
        }
        // one that ended itself inside its clip waits for the map to move below its end
        const std::optional<quint32> ended = drive.nested != nullptr && drive.nested->hasSource(parent()) == false
                                             ? drive.nested->finishedAt() : std::nullopt;
        // a nested Show somebody else stopped is theirs from now on
        if (queued.contains(drive.function) == false ||
            (drive.nested != nullptr && drive.nested->hasSource(parent()) == false && ended == std::nullopt))
        {
            release(drive);
            it = m_drives.erase(it);
            continue;
        }

        // of the clips active now, the one that starts last drives
        const quint32 clock = drive.function->tempoType() == Function::Time ? elapsedTime : elapsedBeats;
        int latest = -1;
        for (int i = 0; i < drive.clips.count(); i++)
        {
            const ShowFunction *clip = drive.clips.at(i).clip;
            if (clip->startTime() <= clock && clock < clip->startTime() + clip->duration(m_doc) &&
                (latest < 0 || clip->startTime() >= drive.clips.at(latest).clip->startTime()))
                latest = i;
        }
        if (latest >= 0)
            drive.driving = latest;
        drive.owned = drive.function->stopped() == false && drive.function->hasSource(parent());
        const Clip &driving = drive.clips.at(drive.driving);
        drive.at = clock > driving.clip->startTime() ? clock - driving.clip->startTime() : 0;

        if (drive.nested != nullptr)
        {
            const quint32 position = driving.automation.songPositionAt(drive.at);
            // a move below its end plays it again from there, as live Perform does
            if (ended && position < *ended)
                drive.nested->start(m_doc->masterTimer(), parent(), position);
            drive.nested->setExternalElapsedTime(position);
            drive.nested->setClipPaused(driving.automation.pausedAt(drive.at));
        }
        applyLevel(drive, trackIntensities);
        ++it;
    }
}

bool ShowClipDriver::setCommandIntensity(Function *f, qreal value, const QMap<quint32, qreal> &trackIntensities)
{
    auto it = m_drives.find(f->id());
    // one somebody else restarted plays it apart from the clip
    if (it == m_drives.end() || f->hasSource(parent()) == false)
        return false;

    it->command = value;
    applyLevel(it.value(), trackIntensities);
    return true;
}

std::optional<qreal> ShowClipDriver::commandIntensity(const Function *f) const
{
    const auto it = m_drives.constFind(f->id());
    if (it == m_drives.constEnd() || f->hasSource(parent()) == false)
        return std::nullopt;
    return it->command;
}

qreal ShowClipDriver::intensityOverride(const Function *f)
{
    const Attribute intensity = f->attributes().at(Function::Intensity);
    return intensity.m_isOverridden && intensity.m_value > 0.0 ? intensity.m_overrideValue / intensity.m_value : 1.0;
}

void ShowClipDriver::applyLevels(const QMap<quint32, qreal> &trackIntensities)
{
    for (Drive &drive : m_drives)
        applyLevel(drive, trackIntensities);
}

void ShowClipDriver::applyLevel(Drive &drive, const QMap<quint32, qreal> &trackIntensities)
{
    const Clip &driving = drive.clips.at(drive.driving);
    const qreal level = trackIntensities.value(driving.trackId, 1.0) * m_show->masterIntensity() *
                        driving.automation.levelAt(drive.at) * drive.command;

    if (drive.nested != nullptr)
    {
        // paused, this Show still applies levels: Perform's Show is Perform's
        if (performed(drive.nested))
            return;
        drive.nested->setMasterIntensity(level);
        drive.nested->setDominant(driving.automation.dominantAt(drive.at) && m_show->isDominant());
        return;
    }

    // Intensity is a Single override: requesting it again never drives
    // whatever a stale identifier was recycled for. A Function the timer
    // restarts at the end of the tick loses the first request there.
    if (level == drive.level && drive.function->attributes().at(Function::Intensity).m_isOverridden)
        return;
    // only while this Show plays it, a start still queued too: one somebody
    // else restarted keeps its own level
    const bool starting = drive.level < 0.0;
    const bool playing = drive.function->stopped() == false && drive.function->hasSource(parent());
    if (starting == false && playing == false)
        return;
    drive.function->requestAttributeOverride(Function::Intensity, level);
    drive.level = level;
    clipWriters().wrote(drive.function, this, level);
}

void ShowClipDriver::release(const Drive &drive)
{
    if (drive.nested == nullptr)
    {
        // another Show's clip plays it on: its level comes back, unless
        // somebody wrote over this one's since
        const std::optional<QPair<qreal, qreal>> back = clipWriters().left(drive.function, this);
        const Function *f = drive.function;
        if (back && f->isRunning() && f->stopped() == false && f->hasSource(parent()) == false &&
            f->attributes().at(Function::Intensity).m_isOverridden &&
            qAbs(intensityOverride(f) - back->first) <= kSameLevel)
            drive.function->requestAttributeOverride(Function::Intensity, back->second);
        return;
    }
    // Perform took it meanwhile, with its own settings
    if (performed(drive.nested))
        return;

    drive.nested->setClipPaused(false);
    drive.nested->setSyncSource(drive.saved.syncSource);
    if (drive.audioSuppressed)
        drive.nested->setPerformAudioSuppressed(drive.saved.audioSuppressed);
    drive.nested->setMasterIntensity(drive.saved.masterIntensity);
    drive.nested->setDominant(drive.saved.dominant);
}

void ShowClipDriver::releaseAll()
{
    for (const Drive &drive : std::as_const(m_drives))
        release(drive);
    m_drives.clear();
}

FunctionParent ShowClipDriver::parent() const
{
    return FunctionParent(FunctionParent::Function, m_show->id());
}

bool ShowClipDriver::keepsThroughSeek(const Function *f)
{
    return f->type() == Function::ShowType;
}

bool ShowClipDriver::performed(const Function *f)
{
    const Show *show = qobject_cast<const Show *>(f);
    return show != nullptr && show->isPerformDriven();
}
