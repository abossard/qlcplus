/*
  Q Light Controller Plus
  showfactory.cpp

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

#include "showfactory.h"

#include "doc.h"
#include "audio.h"
#include "function.h"
#include "show.h"
#include "track.h"
#include "audiobpmtag.h"

#include <QTimer>
#include "showfunction.h"

#include <QDebug>
#include <QFileInfo>

ShowFactory::ShowFactory(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
}

void ShowFactory::createShowForSong(const DjFsm::DeckSong &info)
{
    if (!m_doc)
        return;

    const QString &filepath = info.filepath;
    if (filepath.isEmpty())
        return;

    // Session-level dedup
    if (m_createdShows.contains(filepath))
        return;
    m_createdShows.insert(filepath);

    // Defer show creation to let the telemetry burst and QML initialization
    // complete fully. Creating Audio+Show triggers Doc::functionAdded which
    // updates QML models — doing this too early crashes the QML engine.
    DjFsm::DeckSong copy = info;
    QTimer::singleShot(3000, this, [this, copy]() { createShowDeferred(copy); });
}

void ShowFactory::createShowDeferred(const DjFsm::DeckSong &info)
{
    if (!m_doc)
        return;

    const QString &filepath = info.filepath;
    const QString showName = showNameForSong(info);

    // Check if a Show with this name already exists in Doc
    const auto shows = m_doc->functionsByType(Function::ShowType);
    for (Function *f : shows)
    {
        if (f->name() == showName)
        {
            m_createdShows.insert(filepath);
            m_filepathToShowId.insert(filepath, f->id());
            qDebug() << "[ShowFactory] Show already exists:" << showName;
            emit showCreatedForSong(filepath, f->id());
            return;
        }
    }

    quint32 showId = buildShowFunctions(info, showName);
    if (showId == Function::invalidId())
        return;

    qDebug() << "[ShowFactory] Auto-created Show:" << showName
             << "show ID:" << showId;

    emit showCreatedForSong(filepath, showId);
}

QString ShowFactory::showNameForSong(const DjFsm::DeckSong &info)
{
    if (!info.artist.isEmpty() && !info.title.isEmpty())
        return info.artist + " - " + info.title;
    if (!info.title.isEmpty())
        return info.title;
    return QFileInfo(info.filepath).completeBaseName();
}

quint32 ShowFactory::buildShowFunctions(const DjFsm::DeckSong &info, const QString &showName)
{
    if (!m_doc)
        return Function::invalidId();

    const QString &filepath = info.filepath;

    // Create Audio function
    Audio *audio = new Audio(m_doc);
    audio->setName(showName);
    audio->setPath(kSongFolderPath);
    if (!audio->setSourceFileName(filepath))
    {
        qWarning() << "[ShowFactory] Failed to set audio source:" << filepath;
        delete audio;
        return Function::invalidId();
    }
    m_doc->addFunction(audio);

    // Create Show function
    Show *show = new Show(m_doc);
    show->setName(showName);
    show->setPath(kSongFolderPath);
    // NOTE: the sync source is NOT set here. It is adopted at runtime by
    // VdjBridge when Perform drives this show (External while adopted,
    // restored afterwards). Setting it permanently would freeze manual
    // playback at 0 and would be lost on workspace reload anyway (the
    // sync source is not persisted in the .qxw).
    m_doc->addFunction(show);

    // Set the show's timing BPM from the music file's own ID3 tag (the file's
    // BPM, independent of VDJ's pitch-affected playing BPM). No-op if absent.
    AudioBpmTag::applyFileBpmToShow(show, filepath);

    // VDJ-tracked songs default to the VDJ Beat division: item positions stay
    // in milliseconds (matching the external position sync) while the editor
    // reads the accurate beat grid and cue markers from VDJ's database.xml.
    // The BPM number set above remains as a fallback for the BPM modes.
    show->setTimeDivisionType(Show::VDJBeat);

    // Add a Track with the Audio as a timeline item
    Track *track = new Track(Function::invalidId(), show);
    track->setName("Audio");
    show->addTrack(track);

    ShowFunction *sf = track->createShowFunction(audio->id());
    sf->setStartTime(0);
    sf->setDuration(audio->totalDuration());
    sf->setColor(ShowFunction::defaultColor(Function::AudioType));

    if (!filepath.isEmpty())
    {
        m_createdShows.insert(filepath);
        m_filepathToShowId.insert(filepath, show->id());
    }

    return show->id();
}

quint32 ShowFactory::buildShow(const DjFsm::DeckSong &info)
{
    if (!m_doc || info.filepath.isEmpty())
        return Function::invalidId();

    const QString showName = showNameForSong(info);
    quint32 showId = buildShowFunctions(info, showName);
    if (showId == Function::invalidId())
        return Function::invalidId();

    qDebug() << "[ShowFactory] Built Show on request:" << showName
             << "show ID:" << showId;
    emit showCreatedForSong(info.filepath, showId);
    return showId;
}

void ShowFactory::registerMapping(const QString &filepath, quint32 showId)
{
    if (filepath.isEmpty())
        return;
    if (showId == Function::invalidId())
    {
        m_filepathToShowId.remove(filepath);
        emit mappingChanged(filepath, showId);
        return;
    }
    m_createdShows.insert(filepath);
    m_filepathToShowId.insert(filepath, showId);
    emit mappingChanged(filepath, showId);
}

void ShowFactory::registerMappings(const QList<QPair<QString, quint32>> &mappings)
{
    if (mappings.isEmpty())
        return;

    for (const auto &mapping : mappings)
    {
        if (mapping.first.isEmpty() || mapping.second == Function::invalidId())
            continue;
        m_createdShows.insert(mapping.first);
        m_filepathToShowId.insert(mapping.first, mapping.second);
    }

    // one bulk notification instead of O(mappings) emissions
    emit mappingChanged(QString(), Function::invalidId());
}

quint32 ShowFactory::showIdForFilepath(const QString &filepath) const
{
    return m_filepathToShowId.value(filepath, Function::invalidId());
}

