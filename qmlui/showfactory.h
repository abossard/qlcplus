/*
  Q Light Controller Plus
  showfactory.h

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

#ifndef SHOWFACTORY_H
#define SHOWFACTORY_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "djfsm.h"

class Doc;
class Show;
class Audio;

/** Folder path used to group auto-created VDJ song Shows. */
inline const QString kSongFolderPath = QStringLiteral("Songs");

/**
 * The song-to-Show index and the builder of song Shows.
 *
 * A song (a file path) maps to the user's choice (Assign, or the Show New
 * built; Clear removes the choice), else to the first Show in Doc order whose
 * first Audio clip plays it. The choice is saved with the workspace; a choice
 * whose Show is gone is dropped. Path separators are normalized where a key
 * enters the index, so a Windows path and its POSIX form are one song.
 */
class ShowFactory : public QObject
{
    Q_OBJECT

public:
    explicit ShowFactory(Doc *doc, QObject *parent = nullptr);

    /** The Show for a song: the user's choice, else the Doc's; Function::invalidId() for none. */
    quint32 showIdForFilepath(const QString &filepath) const;

    /** The first Audio clip of a Show, nullptr without one */
    static Audio *songAudio(const Doc *doc, const Show *show);

    /**
     * Derive the human-readable Show name from song metadata:
     * "Artist - Title", else "Title", else the filename stem.
     */
    static QString showNameForSong(const DjFsm::DeckSong &info);

    /**
     * Synchronously build an Audio + Show + Track for the song and return the
     * new Show ID (or Function::invalidId() on failure). Always creates a new
     * Show, which the index then maps through its Audio clip.
     */
    quint32 buildShow(const DjFsm::DeckSong &info);

    /** The user's choice for a song: a Show, or Function::invalidId() to drop
     *  the choice. Kept until the Doc is cleared, or the Show removed. */
    void registerMapping(const QString &filepath, quint32 showId);

    /** The choices, a fork-owned element of the workspace; paths relative to its folder like Audio sources */
    void saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &doc);

    /** One spelling for a song path: the key of the index and of the song rows */
    static QString normalizePath(const QString &filepath);

signals:
    /** Emitted after a Show is successfully added to the Doc for a song. */
    void showCreatedForSong(const QString &filepath, quint32 showId);

    /** The index changed: for one song, or (empty filepath) for any song
     *  after the Doc changed. Consumers re-resolve on this. */
    void mappingChanged(const QString &filepath, quint32 showId);

private:
    void docChanged();

    Doc *m_doc;
    QHash<QString, quint32> m_choices;
    mutable QHash<QString, quint32> m_docIndex;
    mutable bool m_docIndexDirty = true;
};

#endif // SHOWFACTORY_H
