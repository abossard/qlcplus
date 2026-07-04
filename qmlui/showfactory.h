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
#include <QSet>
#include <QString>

#include "djfsm.h"

class Doc;

/** Folder path used to group auto-created VDJ song Shows. */
inline const QString kSongFolderPath = QStringLiteral("Songs");

/**
 * Creates Audio + Show + Track functions in Doc when a song is ready.
 *
 * Deduplicates against:
 * - Its own internal set (same filepath within a session)
 * - Existing Doc functions by name
 *
 * Separated from VdjBridge/DjFsm so that:
 * - The FSM has no Doc dependency (testable in isolation)
 * - Show creation logic is contained in one place
 */
class ShowFactory : public QObject
{
    Q_OBJECT

public:
    explicit ShowFactory(Doc *doc, QObject *parent = nullptr);

    /** Returns the set of filepaths for which shows were created this session. */
    const QSet<QString> &createdShows() const { return m_createdShows; }

    /** Lookup the showID created for a given filepath, or Function::invalidId() if absent. */
    quint32 showIdForFilepath(const QString &filepath) const;

    /**
     * Derive the human-readable Show name from song metadata:
     * "Artist - Title", else "Title", else the filename stem.
     */
    static QString showNameForSong(const DjFsm::DeckSong &info);

    /**
     * Synchronously build an Audio + Show + Track for the song and return the
     * new Show ID (or Function::invalidId() on failure). Always creates a new
     * Show — used for user-initiated "create fresh show" actions where no
     * telemetry burst is in flight. Registers the filepath→showId mapping.
     */
    quint32 buildShow(const DjFsm::DeckSong &info);

    /** Register an externally-known filepath→showId mapping (e.g. user assignment). */
    void registerMapping(const QString &filepath, quint32 showId);

    /** Bulk mapping restore (workspace load): inserts all pairs and emits a
     *  single mappingChanged(QString(), invalidId) instead of one per song. */
    void registerMappings(const QList<QPair<QString, quint32>> &mappings);

signals:
    /** Emitted after a Show is successfully added to the Doc for a song. */
    void showCreatedForSong(const QString &filepath, quint32 showId);

    /** Emitted whenever the filepath->show mapping changes (assign, clear,
     *  restore-from-Doc). Perform-mode consumers re-resolve on this. */
    void mappingChanged(const QString &filepath, quint32 showId);

public slots:
    /** Create Audio + Show + Track for the given song info. Deduplicates by filepath. */
    void createShowForSong(const DjFsm::DeckSong &info);

private:
    void createShowDeferred(const DjFsm::DeckSong &info);
    quint32 buildShowFunctions(const DjFsm::DeckSong &info, const QString &showName);

    Doc *m_doc;
    QSet<QString> m_createdShows;
    QHash<QString, quint32> m_filepathToShowId;
};

#endif // SHOWFACTORY_H
