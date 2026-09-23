/*
  Q Light Controller Plus
  show.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QString>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QList>

#include "showrunner.h"
#include "function.h"
#include "show.h"
#include "doc.h"

#define KXMLQLCShowTimeDivision QStringLiteral("TimeDivision")
#define KXMLQLCShowTimeType     QStringLiteral("Type")
#define KXMLQLCShowTimeBPM      QStringLiteral("BPM")

/*****************************************************************************
 * Initialization
 *****************************************************************************/

Show::Show(Doc* doc) : Function(doc, Function::ShowType)
    , m_timeDivisionType(Time)
    , m_timeDivisionBPM(120)
    , m_syncSource(0) // ShowRunner::Autonomous
    , m_performAudioSuppressed(false)
    , m_latestTrackId(0)
    , m_latestShowFunctionID(0)
    , m_runner(NULL)
{
    setName(tr("New Show"));

    // Clear attributes here. I want attributes to be mapped
    // exactly like the Show tracks
    unregisterAttribute(tr("Intensity"));
}

Show::~Show()
{
    m_tracks.clear();
}

QIcon Show::getIcon() const
{
    return QIcon(":/show.png");
}

quint32 Show::totalDuration()
{
    quint32 totalDuration = 0;

    foreach (Track *track, m_tracks)
    {
        foreach (ShowFunction *sf, track->showFunctions())
        {
            if (sf->startTime() + sf->duration(doc()) > totalDuration)
                totalDuration = sf->startTime() + sf->duration(doc());
        }
    }

    return totalDuration;
}

/*****************************************************************************
 * Copying
 *****************************************************************************/

Function* Show::createCopy(Doc* doc, bool addToDoc)
{
    Q_ASSERT(doc != NULL);

    Function* copy = new Show(doc);
    if (copy->copyFrom(this) == false)
    {
        delete copy;
        copy = NULL;
    }
    if (addToDoc == true && doc->addFunction(copy) == false)
    {
        delete copy;
        copy = NULL;
    }

    return copy;
}

bool Show::copyFrom(const Function* function)
{
    const Show* show = qobject_cast<const Show*> (function);
    if (show == NULL)
        return false;

    m_timeDivisionType = show->m_timeDivisionType;
    m_timeDivisionBPM = show->m_timeDivisionBPM;
    m_latestTrackId = show->m_latestTrackId;
    m_latestShowFunctionID = show->m_latestShowFunctionID;

    /** A commanded target gets one private copy that every one of its clips
     *  shares, so a command reaches each occurrence. An uncommanded target
     *  keeps the existing copy-per-clip behaviour. */
    const QList<quint32> commandedTargets = show->commandTrack().referencedFunctionIds();
    QHash<quint32, quint32> copiedTargets;

    // create a copy of each track
    foreach (Track *track, show->tracks())
    {
        quint32 sceneID = track->getSceneID();
        Track* newTrack = new Track(sceneID, this);
        newTrack->setName(track->name());
        addTrack(newTrack);

        // create a copy of each sequence/audio in a track
        foreach (ShowFunction *sfunc, track->showFunctions())
        {
            Function* function = doc()->function(sfunc->functionID());
            if (function == NULL)
                continue;

            const bool commanded = commandedTargets.contains(function->id());
            Function *copy = NULL;

            if (commanded && copiedTargets.contains(function->id()))
            {
                copy = doc()->function(copiedTargets.value(function->id()));
            }
            else
            {
                /* Attempt to create a copy of the function to Doc */
                copy = function->createCopy(doc());
                if (copy != NULL)
                {
                    copy->setName(tr("Copy of %1").arg(function->name()));
                    if (commanded)
                        copiedTargets.insert(function->id(), copy->id());
                }
            }

            if (copy != NULL)
            {
                ShowFunction *showFunc = newTrack->createShowFunction(copy->id());
                showFunc->setStartTime(sfunc->startTime());
                showFunc->setDuration(sfunc->duration());
                showFunc->setColor(sfunc->color());
                showFunc->setLocked(sfunc->isLocked());
            }
        }
    }

    ShowCommandTrack commands = show->commandTrack();
    for (QHash<quint32, quint32>::const_iterator it = copiedTargets.constBegin();
         it != copiedTargets.constEnd(); ++it)
    {
        commands.remapFunctionId(it.key(), it.value());
    }
    storeCommandTrack(commands);

    return Function::copyFrom(function);
}

/*********************************************************************
 * Time division
 *********************************************************************/

void Show::setTimeDivision(Show::TimeDivision type, int BPM)
{
    qDebug() << "[setTimeDivision] type:" << type << ", BPM:" << BPM;
    m_timeDivisionType = type;
    m_timeDivisionBPM = BPM;
}

Show::TimeDivision Show::timeDivisionType() const
{
    return m_timeDivisionType;
}

int Show::beatsDivision() const
{
    switch(m_timeDivisionType)
    {
        case BPM_2_4: return 2;
        case BPM_3_4: return 3;
        case BPM_4_4: return 4;
        default: return 0;
    }
}

void Show::setTimeDivisionType(TimeDivision type)
{
    m_timeDivisionType = type;
}

int Show::timeDivisionBPM() const
{
    return m_timeDivisionBPM;
}

void Show::setTimeDivisionBPM(int BPM)
{
    m_timeDivisionBPM = BPM;
}

QString Show::tempoToString(Show::TimeDivision type)
{
    switch(type)
    {
        case Time: return QString("Time"); break;
        case VDJBeat: return QString("VDJBeat"); break;
        case BPM_4_4: return QString("BPM_4_4"); break;
        case BPM_3_4: return QString("BPM_3_4"); break;
        case BPM_2_4: return QString("BPM_2_4"); break;
        case Invalid:
        default:
            return QString("Invalid"); break;
    }
    return QString();
}

Show::TimeDivision Show::stringToTempo(const QString& tempo)
{
    if (tempo == "Time")
        return Time;
    else if (tempo == "VDJBeat")
        return VDJBeat;
    else if (tempo == "BPM_4_4")
        return BPM_4_4;
    else if (tempo == "BPM_3_4")
        return BPM_3_4;
    else if (tempo == "BPM_2_4")
        return BPM_2_4;
    else
        return Invalid;
}

/*****************************************************************************
 * External sync
 *****************************************************************************/

void Show::setSyncSource(int source)
{
    m_syncSource = source;
    if (m_runner != NULL)
        m_runner->setSyncSource(static_cast<ShowRunner::SyncSource>(source));
}

void Show::setExternalElapsedTime(quint32 ms)
{
    if (m_externalElapsedTime.exchange(ms, std::memory_order_relaxed) != ms)
        emit externalElapsedTimeChanged(ms);
}

void Show::requestSeek(quint32 ms)
{
    m_requestedSeekTime.store(ms, std::memory_order_release);
}

void Show::setPerformAudioSuppressed(bool suppress)
{
    m_performAudioSuppressed.store(suppress, std::memory_order_relaxed);
}

/*****************************************************************************
 * Tracks
 *****************************************************************************/

bool Show::addTrack(Track *track, quint32 id)
{
    Q_ASSERT(track != NULL);

    // No ID given, this method can assign one
    if (id == Track::invalidId())
        id = createTrackId();

     track->setId(id);
     track->setShowId(this->id());
     m_tracks[id] = track;

     registerAttribute(QString("%1-%2").arg(track->name()).arg(track->id()));

     return true;
}

bool Show::removeTrack(quint32 id)
{
    if (m_tracks.contains(id) == true)
    {
        Track* track = m_tracks.take(id);
        Q_ASSERT(track != NULL);

        unregisterAttribute(QString("%1-%2").arg(track->name()).arg(track->id()));

        //emit trackRemoved(id);
        delete track;

        return true;
    }
    else
    {
        qWarning() << Q_FUNC_INFO << "No track found with id" << id;
        return false;
    }
}

Track* Show::track(quint32 id) const
{
    return m_tracks.value(id, NULL);
}

Track* Show::getTrackFromSceneID(quint32 id) const
{
    foreach (Track *track, m_tracks)
    {
        if (track->getSceneID() == id)
            return track;
    }
    return NULL;
}

Track *Show::getTrackFromShowFunctionID(quint32 id) const
{
    foreach (Track *track, m_tracks)
        if (track->showFunction(id) != NULL)
            return track;

    return NULL;
}

int Show::getTracksCount() const
{
    return m_tracks.size();
}

void Show::moveTrack(Track *track, int direction)
{
    if (track == NULL)
        return;

    qint32 trkID = track->id();
    if (trkID == 0 && direction == -1)
        return;
    qint32 maxID = -1;
    Track *swapTrack = NULL;
    qint32 swapID = -1;
    if (direction > 0) swapID = INT_MAX;

    foreach (quint32 id, m_tracks.keys())
    {
        qint32 signedID = (qint32)id;
        if (signedID > maxID) maxID = signedID;
        if (direction == -1 && signedID > swapID && signedID < trkID)
            swapID = signedID;
        else if (direction == 1 && signedID < swapID && signedID > trkID)
            swapID = signedID;
    }

    qDebug() << Q_FUNC_INFO << "Direction:" << direction << ", trackID:" << trkID << ", swapID:" << swapID;
    if (swapID == trkID || (direction > 0 && trkID == maxID))
        return;

    swapTrack = m_tracks[swapID];
    m_tracks[swapID] = track;
    m_tracks[trkID] = swapTrack;
    track->setId(swapID);
    swapTrack->setId(trkID);
}

QList <Track*> Show::tracks() const
{
    return m_tracks.values();
}

quint32 Show::createTrackId()
{
    while (m_tracks.contains(m_latestTrackId) == true ||
           m_latestTrackId == Track::invalidId())
    {
        m_latestTrackId++;
    }

    return m_latestTrackId;
}

/*********************************************************************
 * Show Functions
 *********************************************************************/

quint32 Show::getLatestShowFunctionId()
{
    return m_latestShowFunctionID++;
}

ShowFunction *Show::showFunction(quint32 id) const
{
    foreach (Track *track, m_tracks)
    {
        ShowFunction *sf = track->showFunction(id);
        if (sf != NULL)
            return sf;
    }

    return NULL;
}

/*********************************************************************
 * Command track
 *********************************************************************/

ShowCommandTrack Show::commandTrack() const
{
    QMutexLocker locker(&m_commandTrackMutex);
    return m_commandTrack;
}

QString Show::commandTargetError(const ShowCommandTrack &track) const
{
    Doc *document = doc();

    foreach (quint32 functionId, track.referencedFunctionIds())
    {
        if (id() != Function::invalidId() && functionId == id())
            return tr("A Show cannot command itself");

        if (document != NULL && document->function(functionId) == NULL)
            return tr("Unknown command target %1").arg(functionId);
    }

    return QString();
}

bool Show::setCommandTrack(const ShowCommandTrack &track, const QSet<quint32> &alreadyApplied,
                           QString *error)
{
    const QString reason = commandTargetError(track);
    if (reason.isEmpty() == false)
    {
        if (error != NULL)
            *error = reason;
        return false;
    }

    ShowCommandTrack accepted = track;
    /** The extent is authored, but it cannot cut authored commands short.
     *  Nothing is appended past the last command: a trailing Start at the
     *  extent ends the Show right there, which is the user's choice */
    if (accepted.extent() < accepted.lastCommandTime())
        accepted.setExtent(accepted.lastCommandTime());

    storeCommandTrack(accepted, alreadyApplied);

    if (error != NULL)
        error->clear();

    emit commandTrackChanged();
    return true;
}

void Show::storeCommandTrack(const ShowCommandTrack &track, const QSet<quint32> &alreadyApplied)
{
    {
        QMutexLocker locker(&m_commandTrackMutex);
        m_commandTrack = track;
        /** Ids published earlier in this traversal are still waiting for the
         *  cursor to reach them, so they must survive a newer publication */
        m_commandAppliedIds.unite(alreadyApplied);
    }

    m_commandTrackRevision.fetch_add(1, std::memory_order_release);
}

quint64 Show::commandTrackRevision() const
{
    return m_commandTrackRevision.load(std::memory_order_acquire);
}

ShowCommandTrack Show::commandTrackSnapshot(QSet<quint32> *alreadyApplied) const
{
    QMutexLocker locker(&m_commandTrackMutex);
    if (alreadyApplied != NULL)
        *alreadyApplied = m_commandAppliedIds;

    return m_commandTrack;
}

void Show::commandTraversalRestarted()
{
    QMutexLocker locker(&m_commandTrackMutex);
    m_commandAppliedIds.clear();
}

void Show::setCommandRecording(bool enabled)
{
    m_commandRecording.store(enabled, std::memory_order_relaxed);
}

bool Show::commandRecording() const
{
    return m_commandRecording.load(std::memory_order_relaxed);
}

void Show::setCommandPosition(quint32 ms)
{
    m_commandPosition.store(ms, std::memory_order_relaxed);
}

quint32 Show::commandPosition() const
{
    /** An externally driven Show has exactly one authoritative clock: the one
     *  the host pushes. It stays readable while the Show is idle or paused,
     *  and it is what the runtime consumes anyway, so the two never disagree.
     *  Function::isRunning() is deliberately not consulted here - it belongs
     *  to the MasterTimer, not to callers on the UI thread. */
    if (m_syncSource != 0)
        return externalElapsedTime();

    return m_commandPosition.load(std::memory_order_relaxed);
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

bool Show::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    /* Function tag */
    doc->writeStartElement(KXMLQLCFunction);

    /* Common attributes */
    saveXMLCommon(doc);

    doc->writeStartElement(KXMLQLCShowTimeDivision);
    doc->writeAttribute(KXMLQLCShowTimeType, tempoToString(m_timeDivisionType));
    doc->writeAttribute(KXMLQLCShowTimeBPM, QString::number(m_timeDivisionBPM));
    doc->writeEndElement();

    foreach (Track *track, m_tracks)
        track->saveXML(doc);

    const ShowCommandTrack commands = commandTrack();
    if (commands.isEmpty() == false || commands.extent() > 0)
        commands.saveXML(doc);

    /* End the <Function> tag */
    doc->writeEndElement();

    return true;
}

bool Show::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCFunction)
    {
        qWarning() << Q_FUNC_INFO << "Function node not found";
        return false;
    }

    if (root.attributes().value(KXMLQLCFunctionType).toString() != typeToString(Function::ShowType))
    {
        qWarning() << Q_FUNC_INFO << root.attributes().value(KXMLQLCFunctionType).toString()
                   << "is not a show";
        return false;
    }

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCShowTimeDivision)
        {
            QString type = root.attributes().value(KXMLQLCShowTimeType).toString();
            int bpm = root.attributes().value(KXMLQLCShowTimeBPM).toString().toInt();
            setTimeDivision(stringToTempo(type), bpm);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCTrack)
        {
            Track *trk = new Track(Function::invalidId(), this);
            if (trk->loadXML(root) == true)
                addTrack(trk, trk->id());
        }
        else if (root.name() == KXMLShowCommandTrack)
        {
            ShowCommandTrack parsed;
            QString error;
            if (parsed.loadXML(root, &error) == false)
            {
                qWarning() << Q_FUNC_INFO << "Invalid command track:" << error;
                // leave the reader at the end of this Show, not inside it
                while (root.readNextStartElement())
                    root.skipCurrentElement();
                return false;
            }

            /** Targets are not resolved here: the functions a Show commands may
             *  still be further down the workspace. postLoad() diagnoses them. */
            storeCommandTrack(parsed);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Show tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    return true;
}

void Show::postLoad()
{
    foreach (Track* track, m_tracks)
    {
        if (track->postLoad(doc()))
            doc()->setModified();
    }

    /** An unresolved command target is reported and kept: dropping it would
     *  silently destroy authored data that a later workspace fix could use */
    const ShowCommandTrack commands = commandTrack();
    foreach (quint32 functionId, commands.referencedFunctionIds())
    {
        if (doc()->function(functionId) == NULL)
            qWarning() << Q_FUNC_INFO << "Show" << name()
                       << "commands an unknown function:" << functionId;
    }
}

bool Show::contains(quint32 functionId) const
{
    Doc *doc = this->doc();
    Q_ASSERT(doc != NULL);

    if (functionId == id())
        return true;

    foreach (Track* track, m_tracks)
    {
        if (track->contains(doc, functionId))
            return true;
    }

    return commandTrack().referencedFunctionIds().contains(functionId);
}

QList<quint32> Show::components() const
{
    QList<quint32> ids;

    foreach (Track* track, m_tracks)
        ids.append(track->components());

    foreach (quint32 functionId, commandTrack().referencedFunctionIds())
    {
        if (ids.contains(functionId) == false)
            ids.append(functionId);
    }

    return ids;
}

/*****************************************************************************
 * Running
 *****************************************************************************/

void Show::preRun(MasterTimer* timer)
{
    m_requestedSeekTime.store(NoSeekRequested, std::memory_order_relaxed);
    m_commandPosition.store(elapsed(), std::memory_order_relaxed);
    Function::preRun(timer);
    m_runningChildren.clear();
    if (m_runner != NULL)
    {
        m_runner->stop();
        delete m_runner;
    }

    m_runner = new ShowRunner(doc(), this->id(), elapsed());
    m_runner->setSyncSource(static_cast<ShowRunner::SyncSource>(m_syncSource));
    m_runner->setExternalElapsedTime(externalElapsedTime());
    int i = 0;
    foreach (Track *track, m_tracks)
        m_runner->adjustIntensity(getAttributeValue(i++), track);

    connect(m_runner, SIGNAL(timeChanged(quint32)), this, SIGNAL(timeChanged(quint32)));
    connect(m_runner, SIGNAL(showFinished()), this, SIGNAL(showFinished()));
    m_runner->start();
}

void Show::setPause(bool enable)
{
    if (m_runner != NULL)
        m_runner->setPause(enable);
    Function::setPause(enable);
}

void Show::write(MasterTimer* timer, QList<Universe *> universes)
{
    Q_UNUSED(universes);

    if (isPaused())
        return;

    m_runner->setExternalElapsedTime(externalElapsedTime());
    const quint64 seekTime = m_requestedSeekTime.exchange(NoSeekRequested,
                                                         std::memory_order_acquire);
    if (seekTime != NoSeekRequested)
        m_runner->requestSeek(quint32(seekTime));
    m_runner->write(timer);
}

void Show::postRun(MasterTimer* timer, QList<Universe *> universes)
{
    m_requestedSeekTime.store(NoSeekRequested, std::memory_order_relaxed);
    if (m_runner != NULL)
    {
        m_runner->stop();
        delete m_runner;
        m_runner = NULL;
    }
    Function::postRun(timer, universes);
}

void Show::slotChildStopped(quint32 fid)
{
    Q_UNUSED(fid);
}

/*****************************************************************************
 * Attributes
 *****************************************************************************/

int Show::adjustAttribute(qreal fraction, int attributeId)
{
    int attrIndex = Function::adjustAttribute(fraction, attributeId);

    if (m_runner != NULL)
    {
        QList<Track*> trkList = m_tracks.values();
        if (trkList.isEmpty() == false &&
            attrIndex >= 0 && attrIndex < trkList.count())
        {
            Track *track = trkList.at(attrIndex);
            if (track != NULL)
                m_runner->adjustIntensity(getAttributeValue(attrIndex), track);
        }
    }

    return attrIndex;
}
