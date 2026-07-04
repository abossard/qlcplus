/*
  Q Light Controller Plus
  vdjdatabasereader.cpp

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

#include "vdjdatabasereader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QXmlStreamReader>

VdjDatabaseReader::SongGrid VdjDatabaseReader::lookup(const QString &songPath)
{
    // prefer the first VALID database that actually contains the song:
    // a wrong-but-existing candidate simply won't have the FilePath entry
    for (const QString &dbPath : databaseCandidates(songPath))
    {
        if (isVdjDatabase(dbPath) == false)
            continue;

        SongGrid grid = lookupInDatabase(dbPath, songPath);
        if (grid.valid)
            return grid;
    }
    return SongGrid();
}

VdjDatabaseReader::SongGrid VdjDatabaseReader::lookupInDatabase(const QString &dbPath,
                                                                const QString &songPath)
{
    QFileInfo dbInfo(dbPath);
    if (dbInfo.exists() == false)
    {
        m_cache.remove(dbPath);
        return SongGrid();
    }

    auto it = m_cache.find(dbPath);
    if (it == m_cache.end() || it->mtime != dbInfo.lastModified())
    {
        QFile file(dbPath);
        if (file.open(QIODevice::ReadOnly) == false)
        {
            qWarning() << "[VdjDatabaseReader] cannot open" << dbPath;
            return SongGrid();
        }

        DbCache cache;
        cache.mtime = dbInfo.lastModified();
        cache.songs = parseDatabase(&file);
        it = m_cache.insert(dbPath, cache);
        qDebug() << "[VdjDatabaseReader] parsed" << dbPath
                 << "-" << it->songs.count() << "songs";
    }

    return it->songs.value(songPath);
}

QString VdjDatabaseReader::locateDatabase(const QString &songPath)
{
    for (const QString &dbPath : databaseCandidates(songPath))
    {
        if (isVdjDatabase(dbPath))
            return dbPath;
    }
    return QString();
}

QStringList VdjDatabaseReader::databaseCandidates(const QString &songPath)
{
    QStringList candidates;

    // 1. explicit user override always wins (portable/nonstandard installs).
    // Guarded: QSettings needs the application identity set by main().
    if (QCoreApplication::organizationName().isEmpty() == false)
    {
        const QString overridePath =
            QSettings().value(QStringLiteral("virtualdj/databasePath")).toString();
        if (overridePath.isEmpty() == false)
            candidates << overridePath;
    }

    // 2. OS-standard locations. GenericDataLocation covers macOS
    // (~/Library/Application Support); DocumentsLocation covers the classic
    // Windows home and follows OneDrive-redirected Documents folders.
    const QList<QStandardPaths::StandardLocation> locations = {
        QStandardPaths::GenericDataLocation,
        QStandardPaths::DocumentsLocation,
    };
    for (QStandardPaths::StandardLocation location : locations)
    {
        for (const QString &base : QStandardPaths::standardLocations(location))
        {
            const QString dbPath = base + QStringLiteral("/VirtualDJ/database.xml");
            if (candidates.contains(dbPath) == false)
                candidates << dbPath;
        }
    }

    // 3. VDJ keeps a separate database on each drive:
    // <volume root>/VirtualDJ/database.xml
    if (songPath.isEmpty() == false)
    {
        QStorageInfo storage(QFileInfo(songPath).absolutePath());
        if (storage.isValid())
        {
            QString driveDb = storage.rootPath();
            if (driveDb.endsWith(QLatin1Char('/')) == false)
                driveDb += QLatin1Char('/');
            driveDb += QStringLiteral("VirtualDJ/database.xml");
            if (candidates.contains(driveDb) == false)
                candidates << driveDb;
        }
    }

    return candidates;
}

bool VdjDatabaseReader::isVdjDatabase(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) == false)
        return false;

    // stream only as far as the first element: a few bytes, no full parse
    QXmlStreamReader xml(&file);
    while (xml.atEnd() == false)
    {
        if (xml.readNext() == QXmlStreamReader::StartElement)
            return xml.name() == QStringLiteral("VirtualDJ_Database");
    }
    return false;
}

QHash<QString, VdjDatabaseReader::SongGrid> VdjDatabaseReader::parseDatabase(QIODevice *device)
{
    QHash<QString, SongGrid> songs;
    QXmlStreamReader xml(device);

    QString currentPath;
    SongGrid currentGrid;
    bool hasGridPoi = false;
    double scanPhaseMs = -1.0;

    while (xml.atEnd() == false)
    {
        switch (xml.readNext())
        {
            case QXmlStreamReader::StartElement:
            {
                const auto attrs = xml.attributes();

                if (xml.name() == QStringLiteral("Song"))
                {
                    currentPath = attrs.value(QStringLiteral("FilePath")).toString();
                    currentGrid = SongGrid();
                    hasGridPoi = false;
                    scanPhaseMs = -1.0;
                }
                else if (currentPath.isEmpty())
                {
                    break;
                }
                else if (xml.name() == QStringLiteral("Scan"))
                {
                    bool ok = false;
                    const double bpmAttr = attrs.value(QStringLiteral("Bpm")).toDouble(&ok);
                    if (ok && bpmAttr > 0.0)
                    {
                        // VDJ stores the beat period in seconds (60 / BPM):
                        // 0.1..5 s covers 600..12 BPM. A plain BPM value
                        // (defensive) is >= 20. Anything in neither range is
                        // rejected so a corrupt entry cannot produce an absurd
                        // grid (a near-zero period would stall the painter).
                        double periodSec = 0.0;
                        if (bpmAttr >= 0.1 && bpmAttr <= 5.0)
                            periodSec = bpmAttr;
                        else if (bpmAttr >= 20.0 && bpmAttr <= 600.0)
                            periodSec = 60.0 / bpmAttr;

                        if (periodSec > 0.0)
                        {
                            currentGrid.beatPeriodMs = periodSec * 1000.0;
                            currentGrid.valid = true;
                        }
                    }

                    // some songs carry the grid phase only here, without a
                    // beatgrid POI (used as anchor fallback at Song end)
                    ok = false;
                    const double phaseSec = attrs.value(QStringLiteral("Phase")).toDouble(&ok);
                    if (ok && phaseSec >= 0.0)
                        scanPhaseMs = phaseSec * 1000.0;
                }
                else if (xml.name() == QStringLiteral("Poi"))
                {
                    bool ok = false;
                    const double posSec = attrs.value(QStringLiteral("Pos")).toDouble(&ok);
                    if (ok == false)
                        break;

                    const QString type = attrs.value(QStringLiteral("Type")).toString();
                    if (type == QStringLiteral("beatgrid"))
                    {
                        // first anchor wins (multi-segment grids are rare and
                        // the first segment covers the song start)
                        if (hasGridPoi == false)
                        {
                            currentGrid.anchorMs = posSec * 1000.0;
                            hasGridPoi = true;
                        }
                    }
                    else
                    {
                        Poi poi;
                        poi.posMs = posSec * 1000.0;
                        poi.name = attrs.value(QStringLiteral("Name")).toString();
                        // automix POIs are unnamed but carry a Point attribute
                        if (poi.name.isEmpty())
                            poi.name = attrs.value(QStringLiteral("Point")).toString();
                        poi.type = type.isEmpty() ? QStringLiteral("cue") : type;
                        bool numOk = false;
                        const int num = attrs.value(QStringLiteral("Num")).toInt(&numOk);
                        poi.num = numOk ? num : -1;

                        // VDJ colors are 32-bit ARGB decimals
                        bool colorOk = false;
                        const quint32 argb = attrs.value(QStringLiteral("Color")).toUInt(&colorOk);
                        if (colorOk)
                            poi.color = QStringLiteral("#%1")
                                            .arg(argb & 0xFFFFFF, 6, 16, QLatin1Char('0'));

                        currentGrid.pois.append(poi);
                    }
                }
                break;
            }
            case QXmlStreamReader::EndElement:
            {
                if (xml.name() == QStringLiteral("Song") && currentPath.isEmpty() == false)
                {
                    if (hasGridPoi == false && scanPhaseMs >= 0.0)
                        currentGrid.anchorMs = scanPhaseMs;
                    songs.insert(currentPath, currentGrid);
                    currentPath.clear();
                }
                break;
            }
            default:
                break;
        }
    }

    if (xml.hasError())
        qWarning() << "[VdjDatabaseReader] XML error:" << xml.errorString();

    return songs;
}
