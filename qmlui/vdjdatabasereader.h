/*
  Q Light Controller Plus
  vdjdatabasereader.h

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

#ifndef VDJDATABASEREADER_H
#define VDJDATABASEREADER_H

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class QIODevice;

/**
 * Read-only access to VirtualDJ's database.xml song analysis data.
 *
 * VDJ stores per-song analysis under each <Song FilePath="..."> entry:
 *  - <Scan Bpm="0.4687..." Phase="6.93..."> — the beat period in SECONDS
 *    (60 / BPM) and the grid phase anchor in seconds
 *  - <Poi Pos="..." Type="beatgrid"> — grid anchor as a POI (same value as
 *    Scan Phase when both are present; some songs only have Phase)
 *  - <Poi Name="Cue 1" Pos="..." Num="1" Color="4291559424" Type="cue"> —
 *    cue points; automix POIs are unnamed but carry a Point attribute
 *    (realStart/realEnd/fadeStart/fadeEnd), plus loop/remix/action markers
 *
 * The database is never modified and nothing is copied into QLC+ structures:
 * the Show editor's VDJBeat mode reads grid + POIs from here at display time.
 *
 * Databases are parsed lazily with QXmlStreamReader and cached per file,
 * invalidated by modification time.
 *
 * Locate algorithm (safe resolution, in priority order):
 *  1. Explicit user override — QSettings key "virtualdj/databasePath"
 *  2. OS-standard locations via QStandardPaths:
 *     GenericDataLocation/VirtualDJ/database.xml (macOS: ~/Library/
 *     Application Support), then DocumentsLocation/VirtualDJ/database.xml
 *     (Windows home, follows OneDrive-redirected Documents)
 *  3. The per-drive database on the volume the song file lives on
 *     (<root>/VirtualDJ/database.xml)
 * Every candidate is validated (readable + <VirtualDJ_Database> root
 * element) before being trusted; lookup() prefers the first database that
 * actually contains the song.
 */
class VdjDatabaseReader
{
public:
    struct Poi
    {
        QString name;   // Name attribute, or Point (automix) as fallback
        double posMs = 0.0;
        QString type;   // "cue", "loop", "remix", "automix", ...
        int num = -1;   // cue number, -1 if absent
        QString color;  // "#RRGGBB" from VDJ's 32-bit Color, empty if absent
    };

    struct SongGrid
    {
        bool valid = false;      // true when a beat period was found
        double beatPeriodMs = 0.0;
        double anchorMs = 0.0;   // position of a beat (grid phase); 0 if absent
        QList<Poi> pois;         // markers, beatgrid anchor excluded
    };

    VdjDatabaseReader() = default;

    /** Look up $songPath in all candidate databases. Returns an invalid
     *  SongGrid if the song is not found or no database exists. */
    SongGrid lookup(const QString &songPath);

    /** Look up $songPath in the database file at $dbPath (cached). */
    SongGrid lookupInDatabase(const QString &dbPath, const QString &songPath);

    /** Return the first candidate that exists and validates as a VirtualDJ
     *  database, or an empty string when none is found. $songPath (optional)
     *  adds the song's drive to the candidates. */
    static QString locateDatabase(const QString &songPath = QString());

    /** Candidate database.xml paths for a song, in priority order:
     *  QSettings override, OS-standard locations, the song's drive.
     *  Exposed for testing. */
    static QStringList databaseCandidates(const QString &songPath);

    /** Cheap validation: the file exists, is readable and its XML root
     *  element is <VirtualDJ_Database>. Exposed for testing. */
    static bool isVdjDatabase(const QString &path);

    /** Parse a whole database from $device into a FilePath-keyed map.
     *  Exposed for testing. */
    static QHash<QString, SongGrid> parseDatabase(QIODevice *device);

private:
    struct DbCache
    {
        QDateTime mtime;
        QHash<QString, SongGrid> songs;
    };

    /** Per-database-file cache, keyed by absolute db path */
    QHash<QString, DbCache> m_cache;
};

#endif // VDJDATABASEREADER_H
