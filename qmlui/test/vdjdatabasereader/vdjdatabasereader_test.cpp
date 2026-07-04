/*
  Q Light Controller Plus - Unit test
  vdjdatabasereader_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "vdjdatabasereader_test.h"
#include "vdjdatabasereader.h"

#include <QBuffer>
#include <QDir>
#include <QTemporaryDir>

// shaped like a real VDJ 2023 database (verified against a live install):
// Scan carries Bpm (seconds per beat) + Phase; the beatgrid POI repeats the
// phase; cues have Type="cue", Num and a 32-bit ARGB Color; automix POIs are
// unnamed but carry a Point attribute
static const char *kSampleDb = R"(<?xml version="1.0" encoding="UTF-8"?>
<VirtualDJ_Database Version="2023">
 <Song FilePath="/Music/Artist - Title.mp3" FileSize="123">
  <Tags Author="Artist" Title="Title" />
  <Infos SongLength="180.5" />
  <Scan Version="801" Bpm="0.468750" Phase="0.435374" Key="Am" />
  <Poi Pos="0.435374" Type="beatgrid" />
  <Poi Name="Intro" Pos="15.25" Num="1" Color="4291559424" Type="cue" />
  <Poi Name="Drop" Pos="60.5" Num="2" Type="cue" />
  <Poi Name="Loop 8" Pos="120.0" Type="loop" />
  <Poi Pos="0.023220" Type="automix" Point="realStart" />
 </Song>
 <Song FilePath="/Music/NoScan.mp3">
  <Tags Author="X" Title="NoScan" />
  <Poi Name="Cue" Pos="10.0" Num="1" Type="cue" />
 </Song>
 <Song FilePath="/Music/PlainBpm.mp3">
  <Scan Version="801" Bpm="128.0" />
 </Song>
 <Song FilePath="/Music/CorruptTiny.mp3">
  <Scan Version="801" Bpm="0.0001" />
 </Song>
 <Song FilePath="/Music/CorruptHuge.mp3">
  <Scan Version="801" Bpm="99999" />
 </Song>
 <Song FilePath="/Music/PhaseOnly.mp3">
  <Scan Version="801" Bpm="0.500000" Phase="6.933515" />
 </Song>
</VirtualDJ_Database>
)";

static QHash<QString, VdjDatabaseReader::SongGrid> parseSample(const QByteArray &xml)
{
    QBuffer buffer;
    buffer.setData(xml);
    buffer.open(QIODevice::ReadOnly);
    return VdjDatabaseReader::parseDatabase(&buffer);
}

void VdjDatabaseReader_Test::parsesSongGridAndPois()
{
    const auto songs = parseSample(kSampleDb);
    QCOMPARE(songs.count(), 6);

    const VdjDatabaseReader::SongGrid grid = songs.value("/Music/Artist - Title.mp3");
    QVERIFY(grid.valid);
    // Bpm attribute is the beat period in seconds: 0.46875 s == 128 BPM
    QCOMPARE(grid.beatPeriodMs, 468.75);
    QCOMPARE(grid.anchorMs, 435.374);

    // the beatgrid POI becomes the anchor, not a marker
    QCOMPARE(grid.pois.count(), 4);
    QCOMPARE(grid.pois[0].name, QString("Intro"));
    QCOMPARE(grid.pois[0].posMs, 15250.0);
    QCOMPARE(grid.pois[0].type, QString("cue"));
    QCOMPARE(grid.pois[0].num, 1);
    // 4291559424 == 0xFFCC0000 -> #cc0000
    QCOMPARE(grid.pois[0].color, QString("#cc0000"));
    QCOMPARE(grid.pois[1].name, QString("Drop"));
    QCOMPARE(grid.pois[1].num, 2);
    QVERIFY(grid.pois[1].color.isEmpty());
    QCOMPARE(grid.pois[2].type, QString("loop"));
    QCOMPARE(grid.pois[2].num, -1);
    // unnamed automix POI falls back to its Point attribute
    QCOMPARE(grid.pois[3].type, QString("automix"));
    QCOMPARE(grid.pois[3].name, QString("realStart"));
}

void VdjDatabaseReader_Test::scanPhaseUsedWhenNoBeatgridPoi()
{
    const auto songs = parseSample(kSampleDb);

    const VdjDatabaseReader::SongGrid grid = songs.value("/Music/PhaseOnly.mp3");
    QVERIFY(grid.valid);
    QCOMPARE(grid.beatPeriodMs, 500.0);
    QCOMPARE(grid.anchorMs, 6933.515);
}

void VdjDatabaseReader_Test::beatPeriodStoredAsSecondsOrBpm()
{
    const auto songs = parseSample(kSampleDb);

    // a database carrying a plain BPM value (>= 20) is converted to a period
    const VdjDatabaseReader::SongGrid grid = songs.value("/Music/PlainBpm.mp3");
    QVERIFY(grid.valid);
    QCOMPARE(grid.beatPeriodMs, 468.75); // 60 / 128 * 1000

    // corrupt/implausible Bpm values are rejected instead of producing an
    // absurd grid (a near-zero period would stall the beat painter)
    QVERIFY(songs.value("/Music/CorruptTiny.mp3").valid == false);
    QVERIFY(songs.value("/Music/CorruptHuge.mp3").valid == false);
}

void VdjDatabaseReader_Test::songWithoutScanIsInvalid()
{
    const auto songs = parseSample(kSampleDb);

    const VdjDatabaseReader::SongGrid grid = songs.value("/Music/NoScan.mp3");
    QVERIFY(grid.valid == false);
    QCOMPARE(grid.beatPeriodMs, 0.0);
    // POIs are still collected even without a grid
    QCOMPARE(grid.pois.count(), 1);
}

void VdjDatabaseReader_Test::unknownSongReturnsInvalid()
{
    const auto songs = parseSample(kSampleDb);
    QVERIFY(songs.value("/Music/Unknown.mp3").valid == false);
}

void VdjDatabaseReader_Test::lookupInDatabaseCachesAndRefreshesOnMtime()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath("database.xml");

    {
        QFile file(dbPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(kSampleDb);
    }

    VdjDatabaseReader reader;
    VdjDatabaseReader::SongGrid grid = reader.lookupInDatabase(dbPath, "/Music/Artist - Title.mp3");
    QVERIFY(grid.valid);
    QCOMPARE(grid.beatPeriodMs, 468.75);

    // rewrite the database with a different grid and a bumped mtime
    QByteArray updated(kSampleDb);
    updated.replace("0.468750", "0.500000");
    {
        QFile file(dbPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(updated);
    }
    // ensure the mtime actually differs (fs timestamp resolution)
    {
        QFile file(dbPath);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(2),
                                 QFileDevice::FileModificationTime));
    }

    grid = reader.lookupInDatabase(dbPath, "/Music/Artist - Title.mp3");
    QVERIFY(grid.valid);
    QCOMPARE(grid.beatPeriodMs, 500.0);
}

void VdjDatabaseReader_Test::missingDatabaseFileReturnsInvalid()
{
    VdjDatabaseReader reader;
    const VdjDatabaseReader::SongGrid grid =
        reader.lookupInDatabase("/nonexistent/database.xml", "/Music/Artist - Title.mp3");
    QVERIFY(grid.valid == false);
}

void VdjDatabaseReader_Test::databaseCandidatesIncludeHomeAndDrive()
{
    const QStringList candidates = VdjDatabaseReader::databaseCandidates("/Music/Artist - Title.mp3");
    QVERIFY(candidates.count() >= 1);
#ifdef Q_OS_MACOS
    QCOMPARE(candidates.first(),
             QDir::homePath() + "/Library/Application Support/VirtualDJ/database.xml");
#else
    QCOMPARE(candidates.first(), QDir::homePath() + "/Documents/VirtualDJ/database.xml");
#endif
    QVERIFY(candidates.contains(QDir::homePath() + "/Documents/VirtualDJ/database.xml"));

    for (const QString &c : candidates)
        QVERIFY(c.endsWith("VirtualDJ/database.xml"));
}

void VdjDatabaseReader_Test::validatesDatabaseRootElement()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto writeFile = [&dir](const QString &name, const QByteArray &content) {
        const QString path = dir.filePath(name);
        QFile file(path);
        if (file.open(QIODevice::WriteOnly) == false)
            return QString();
        file.write(content);
        return path;
    };

    // a real VDJ database validates
    QVERIFY(VdjDatabaseReader::isVdjDatabase(writeFile("database.xml", kSampleDb)));

    // some other application's database.xml is rejected
    QVERIFY(VdjDatabaseReader::isVdjDatabase(
        writeFile("other.xml", "<?xml version=\"1.0\"?>\n<SomeOtherDatabase/>\n")) == false);

    // non-XML content and missing files are rejected
    QVERIFY(VdjDatabaseReader::isVdjDatabase(writeFile("junk.xml", "not xml at all")) == false);
    QVERIFY(VdjDatabaseReader::isVdjDatabase(dir.filePath("missing.xml")) == false);
    QVERIFY(VdjDatabaseReader::isVdjDatabase(QString()) == false);
}

void VdjDatabaseReader_Test::malformedXmlDoesNotCrash()
{
    const auto songs = parseSample("<VirtualDJ_Database><Song FilePath=\"/a.mp3\"><Scan Bpm=\"0.5\"");
    // truncated input: whatever was fully parsed may or may not be present,
    // the only contract is not crashing and not inventing entries
    QVERIFY(songs.count() <= 1);
}

QTEST_GUILESS_MAIN(VdjDatabaseReader_Test)
