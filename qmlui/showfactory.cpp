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
#include "show.h"
#include "track.h"
#include "showfunction.h"

#include <QDebug>
#include <QFileInfo>

ShowFactory::ShowFactory(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
}

void ShowFactory::createShowForSong(const SongLoadTracker::SongInfo &info)
{
    if (!m_doc)
        return;

    const QString &filepath = info.filepath;
    if (filepath.isEmpty())
        return;

    // Session-level dedup
    if (m_createdShows.contains(filepath))
        return;

    // Derive a human-readable name from metadata or filename
    QString showName;
    if (!info.artist.isEmpty() && !info.title.isEmpty())
        showName = info.artist + " - " + info.title;
    else if (!info.title.isEmpty())
        showName = info.title;
    else
        showName = QFileInfo(filepath).completeBaseName();

    // Check if a Show with this name already exists in Doc
    const auto shows = m_doc->functionsByType(Function::ShowType);
    for (Function *f : shows)
    {
        if (f->name() == showName)
        {
            m_createdShows.insert(filepath);
            qDebug() << "[ShowFactory] Show already exists:" << showName;
            return;
        }
    }

    // Create Audio function
    Audio *audio = new Audio(m_doc);
    audio->setName(showName);
    if (!audio->setSourceFileName(filepath))
    {
        qWarning() << "[ShowFactory] Failed to set audio source:" << filepath;
        delete audio;
        return;
    }
    m_doc->addFunction(audio);

    // Create Show function
    Show *show = new Show(m_doc);
    show->setName(showName);
    m_doc->addFunction(show);

    // Add a Track with the Audio as a timeline item
    Track *track = new Track(Function::invalidId(), show);
    track->setName("Audio");
    show->addTrack(track);

    ShowFunction *sf = track->createShowFunction(audio->id());
    sf->setStartTime(0);
    sf->setDuration(audio->totalDuration());
    sf->setColor(ShowFunction::defaultColor(Function::AudioType));

    m_createdShows.insert(filepath);
    qDebug() << "[ShowFactory] Auto-created Show:" << showName
             << "audio ID:" << audio->id()
             << "show ID:" << show->id()
             << "duration:" << audio->totalDuration() << "ms";
}
