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
#include "showfunction.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

ShowFactory::ShowFactory(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    if (!m_doc)
        return;
    connect(m_doc, &Doc::cleared, this, [this]() {
        m_choices.clear();
        docChanged();
    });
    // a Show's clips and an Audio's source make the index; their changes reach it
    const auto songFunctionChanged = [this](quint32 id) {
        const Function *function = m_doc->function(id);
        if (function && (function->type() == Function::ShowType || function->type() == Function::AudioType))
            docChanged();
    };
    connect(m_doc, &Doc::functionAdded, this, songFunctionChanged);
    connect(m_doc, &Doc::functionChanged, this, songFunctionChanged);
    connect(m_doc, &Doc::functionNameChanged, this, songFunctionChanged);
    connect(m_doc, &Doc::functionRemoved, this, [this](quint32 id) {
        // the user's choice goes with its Show; its id is handed to the next Function
        for (auto it = m_choices.begin(); it != m_choices.end();)
        {
            if (it.value() == id)
                it = m_choices.erase(it);
            else
                ++it;
        }
        docChanged();
    });
    // a clip attached to a track announces nothing but the Doc's modification
    connect(m_doc, &Doc::modified, this, [this]() { m_docIndexDirty = true; });
}

void ShowFactory::docChanged()
{
    m_docIndexDirty = true;
    emit mappingChanged(QString(), Function::invalidId());
}

QString ShowFactory::normalizePath(const QString &filepath)
{
    return QDir::fromNativeSeparators(filepath);
}

Audio *ShowFactory::songAudio(const Doc *doc, const Show *show)
{
    if (!doc || !show)
        return nullptr;
    for (Track *track : show->tracks())
    {
        for (ShowFunction *sf : track->showFunctions())
        {
            if (Audio *audio = qobject_cast<Audio *>(doc->function(sf->functionID())))
                return audio;
        }
    }
    return nullptr;
}

quint32 ShowFactory::showIdForFilepath(const QString &filepath) const
{
    const QString key = normalizePath(filepath);
    if (key.isEmpty() || !m_doc)
        return Function::invalidId();
    if (m_docIndexDirty)
    {
        m_docIndex.clear();
        // a Show's song is its first Audio clip's; several Shows of one song: the first in Doc order
        for (Function *f : m_doc->functionsByType(Function::ShowType))
        {
            Audio *audio = songAudio(m_doc, qobject_cast<Show *>(f));
            if (audio && !m_docIndex.contains(normalizePath(audio->getSourceFileName())))
                m_docIndex.insert(normalizePath(audio->getSourceFileName()), f->id());
        }
        m_docIndexDirty = false;
    }
    return m_choices.value(key, m_docIndex.value(key, Function::invalidId()));
}

QString ShowFactory::showNameForSong(const DjFsm::DeckSong &info)
{
    if (!info.artist.isEmpty() && !info.title.isEmpty())
        return info.artist + " - " + info.title;
    if (!info.title.isEmpty())
        return info.title;
    return QFileInfo(info.filepath).completeBaseName();
}

quint32 ShowFactory::buildShow(const DjFsm::DeckSong &info)
{
    if (!m_doc || info.filepath.isEmpty())
        return Function::invalidId();

    const QString &filepath = info.filepath;
    const QString showName = showNameForSong(info);

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

    qDebug() << "[ShowFactory] Built Show:" << showName << "show ID:" << show->id();
    // the clip went in after the Show was added: the index reads it now
    docChanged();
    emit showCreatedForSong(filepath, show->id());
    return show->id();
}

void ShowFactory::registerMapping(const QString &filepath, quint32 showId)
{
    const QString key = normalizePath(filepath);
    if (key.isEmpty())
        return;
    if (showId == Function::invalidId())
        m_choices.remove(key);
    else
        m_choices.insert(key, showId);
    if (m_doc)
        m_doc->setModified();
    emit mappingChanged(filepath, showId);
}

void ShowFactory::saveXML(QXmlStreamWriter *doc) const
{
    if (m_choices.isEmpty())
        return;
    doc->writeStartElement(QStringLiteral("SongShows"));
    for (auto it = m_choices.constBegin(); it != m_choices.constEnd(); ++it)
    {
        doc->writeStartElement(QStringLiteral("Song"));
        doc->writeAttribute(QStringLiteral("Path"), it.key());
        doc->writeAttribute(QStringLiteral("Show"), QString::number(it.value()));
        doc->writeEndElement();
    }
    doc->writeEndElement();
}

bool ShowFactory::loadXML(QXmlStreamReader &doc)
{
    if (doc.name() != QLatin1String("SongShows"))
        return false;
    while (doc.readNextStartElement())
    {
        const QXmlStreamAttributes attrs = doc.attributes();
        const quint32 showId = attrs.value(QStringLiteral("Show")).toUInt();
        // the Shows were read before this element: a choice whose Show is gone is dropped
        if (doc.name() == QLatin1String("Song") && m_doc && qobject_cast<Show *>(m_doc->function(showId)))
            m_choices.insert(normalizePath(attrs.value(QStringLiteral("Path")).toString()), showId);
        doc.skipCurrentElement();
    }
    docChanged();
    return true;
}
