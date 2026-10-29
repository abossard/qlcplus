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

#include <QSet>

#include "showclipdriver.h"
#include "showfunction.h"
#include "showrunner.h"
#include "function.h"
#include "track.h"
#include "show.h"
#include "doc.h"

ShowClipDriver::ShowClipDriver(const Doc *doc, quint32 showId)
    : m_doc(doc)
    , m_show(qobject_cast<Show *>(doc->function(showId)))
{
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

void ShowClipDriver::start(const ShowFunction *clip, Function *f, quint32 at,
                           const QMap<quint32, qreal> &trackIntensities)
{
    Drive drive;
    drive.function = f;
    drive.clips.append(clipOf(clip));
    drive.at = at;
    drive.nested = f->type() == Function::ShowType ? qobject_cast<Show *>(f) : nullptr;
    const ShowClipAutomation &automation = drive.clips.first().automation;

    quint32 startTime = automation.inPoint() + at;
    if (drive.nested != nullptr)
    {
        // a clip of it that ended this tick has not given its settings back yet
        const auto ended = m_drives.constFind(f->id());
        if (ended != m_drives.constEnd() && ended->nested != nullptr)
        {
            drive.saved = ended->saved;
        }
        else
        {
            drive.saved.syncSource = drive.nested->syncSource();
            drive.saved.audioSuppressed = drive.nested->performAudioSuppressed();
            drive.saved.masterIntensity = drive.nested->masterIntensity();
            drive.saved.dominant = drive.nested->isDominant();
        }
        drive.nested->setSyncSource(ShowRunner::External);
        drive.nested->setPerformAudioSuppressed(true);
        startTime = automation.songPositionAt(at);
    }

    Drive &stored = m_drives.insert(f->id(), drive).value();
    applyLevel(stored, trackIntensities);
    // the clock follows in drive(), after every clip of this tick joined: the
    // first sample after the start is where the nested traversal begins
    f->start(m_doc->masterTimer(), FunctionParent(FunctionParent::Function, m_show->id()), startTime);
}

void ShowClipDriver::join(const ShowFunction *clip, Function *f)
{
    auto it = m_drives.find(f->id());
    if (it == m_drives.end())
        return;

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
        if (queued.contains(drive.function) == false)
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
        const Clip &driving = drive.clips.at(drive.driving);
        drive.at = clock > driving.clip->startTime() ? clock - driving.clip->startTime() : 0;

        if (drive.nested != nullptr)
            drive.nested->setExternalElapsedTime(driving.automation.songPositionAt(drive.at));
        applyLevel(drive, trackIntensities);
        ++it;
    }
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
                        driving.automation.levelAt(drive.at);

    if (drive.nested != nullptr)
    {
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
    const bool playing = drive.function->stopped() == false &&
                         drive.function->hasSource(FunctionParent(FunctionParent::Function, m_show->id()));
    if (starting == false && playing == false)
        return;
    drive.function->requestAttributeOverride(Function::Intensity, level);
    drive.level = level;
}

void ShowClipDriver::release(const Drive &drive)
{
    if (drive.nested == nullptr)
        return;

    drive.nested->setSyncSource(drive.saved.syncSource);
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

bool ShowClipDriver::keepsThroughSeek(const Function *f)
{
    return f->type() == Function::ShowType;
}
