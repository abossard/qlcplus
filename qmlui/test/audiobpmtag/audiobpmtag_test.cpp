/*
  Q Light Controller Plus - Unit test
  audiobpmtag_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "audiobpmtag_test.h"
#include "audiobpmtag.h"

#include "doc.h"
#include "show.h"

#include <QTemporaryFile>

// ── ID3v2 tag builders (mirror the real on-disk layout) ───────────────────

static QByteArray synchsafe(quint32 v)
{
    QByteArray b;
    b.append(char((v >> 21) & 0x7F));
    b.append(char((v >> 14) & 0x7F));
    b.append(char((v >> 7) & 0x7F));
    b.append(char(v & 0x7F));
    return b;
}

static QByteArray be32(quint32 v)
{
    QByteArray b;
    b.append(char((v >> 24) & 0xFF));
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char(v & 0xFF));
    return b;
}

static QByteArray be24(quint32 v)
{
    QByteArray b;
    b.append(char((v >> 16) & 0xFF));
    b.append(char((v >> 8) & 0xFF));
    b.append(char(v & 0xFF));
    return b;
}

// Wrap a single frame into an ID3 tag header (header size is synchsafe in all versions).
static QByteArray wrapTag(int major, const QByteArray &frame)
{
    QByteArray tag("ID3");
    tag.append(char(major));
    tag.append(char(0));            // minor
    tag.append(char(0));            // flags
    tag.append(synchsafe(frame.size()));
    tag.append(frame);
    return tag;
}

// ID3v2.4 (synchsafe frame size) with a text frame: 1 encoding byte + text.
static QByteArray id3v24(const QByteArray &id4, uchar enc, const QByteArray &text)
{
    QByteArray body;
    body.append(char(enc));
    body.append(text);
    QByteArray frame(id4);          // 4 bytes
    frame.append(synchsafe(body.size()));
    frame.append(char(0));          // flags hi
    frame.append(char(0));          // flags lo
    frame.append(body);
    return wrapTag(4, frame);
}

// ID3v2.3 (plain be32 frame size).
static QByteArray id3v23(const QByteArray &id4, uchar enc, const QByteArray &text)
{
    QByteArray body;
    body.append(char(enc));
    body.append(text);
    QByteArray frame(id4);
    frame.append(be32(body.size()));
    frame.append(char(0));
    frame.append(char(0));
    frame.append(body);
    return wrapTag(3, frame);
}

// ID3v2.2 (3-char id "TBP", 3-byte size, no frame flags).
static QByteArray id3v22(const QByteArray &id3, uchar enc, const QByteArray &text)
{
    QByteArray body;
    body.append(char(enc));
    body.append(text);
    QByteArray frame(id3);          // 3 bytes
    frame.append(be24(body.size()));
    frame.append(body);
    return wrapTag(2, frame);
}

// UTF-16LE bytes with a leading BOM (encoding byte 1).
static QByteArray utf16leWithBom(const QString &s)
{
    QByteArray b;
    b.append(char(0xFF)); b.append(char(0xFE)); // BOM
    for (QChar c : s)
    {
        ushort u = c.unicode();
        b.append(char(u & 0xFF));
        b.append(char((u >> 8) & 0xFF));
    }
    return b;
}

// UTF-16BE bytes without a BOM (encoding byte 2).
static QByteArray utf16beNoBom(const QString &s)
{
    QByteArray b;
    for (QChar c : s)
    {
        ushort u = c.unicode();
        b.append(char((u >> 8) & 0xFF));
        b.append(char(u & 0xFF));
    }
    return b;
}

// ID3v2.3 header with the extended-header flag (0x40) set and a chosen ext-size.
// Used to exercise the malformed/overflowing extended-header path.
static QByteArray id3v23ExtHeader(quint32 extSize, const QByteArray &trailing)
{
    QByteArray tag("ID3");
    tag.append(char(3)); tag.append(char(0));      // v2.3.0
    tag.append(char(0x40));                         // flags: extended header present
    QByteArray after;
    after.append(be32(extSize));                    // extended-header size (be32, excludes itself)
    after.append(trailing);
    tag.append(synchsafe(after.size()));
    tag.append(after);
    return tag;
}

void AudioBpmTag_Test::parsesBpmAcrossId3Variants_data()
{
    QTest::addColumn<QByteArray>("data");
    QTest::addColumn<int>("expected"); // -1 == std::nullopt

    // The real-world case observed in the user's library (ID3v2.4, UTF-8).
    QTest::newRow("v2.4 utf8 128.00")  << id3v24("TBPM", 3, QByteArray("128.00")) << 128;
    QTest::newRow("v2.3 latin1 120")   << id3v23("TBPM", 0, QByteArray("120"))    << 120;
    QTest::newRow("v2.2 TBP 125")      << id3v22("TBP",  0, QByteArray("125"))    << 125;
    QTest::newRow("decimal rounds up") << id3v24("TBPM", 3, QByteArray("124.96")) << 125;
    QTest::newRow("utf16 BOM 130")     << id3v24("TBPM", 1, utf16leWithBom("130")) << 130;
    QTest::newRow("utf16BE 126")       << id3v24("TBPM", 2, utf16beNoBom("126"))  << 126;
    QTest::newRow("no TBPM frame")     << id3v24("TIT2", 3, QByteArray("Title"))  << -1;
    QTest::newRow("not an ID3 tag")    << QByteArray("RIFFxxxxWAVEfmt junk data") << -1;
    QTest::newRow("too short")         << QByteArray("ID3")                       << -1;
    QTest::newRow("empty TBPM")        << id3v24("TBPM", 3, QByteArray())         << -1;
    // Malformed inputs must return none WITHOUT crashing (out-of-bounds guards):
    // a v2.3 extended-header size that, +4, overflows int (regression for a
    // confirmed SIGSEGV in the ext-header skip).
    QTest::newRow("v2.3 overflow extHdr") << id3v23ExtHeader(0x7FFFFFFFu, QByteArray(16, '\0')) << -1;
    QTest::newRow("v2.3 huge extHdr")     << id3v23ExtHeader(0xFFFFFFFFu, QByteArray(16, '\0')) << -1;
}

void AudioBpmTag_Test::parsesBpmAcrossId3Variants()
{
    QFETCH(QByteArray, data);
    QFETCH(int, expected);

    const std::optional<int> got = AudioBpmTag::parseId3v2Bpm(data);
    if (expected < 0)
        QVERIFY2(!got.has_value(), "expected no BPM");
    else
    {
        QVERIFY2(got.has_value(), "expected a BPM");
        QCOMPARE(*got, expected);
    }
}

void AudioBpmTag_Test::readsBpmFromFile()
{
    // A real temp file carrying an ID3v2.4 TBPM tag is read end-to-end.
    QTemporaryFile f;
    QVERIFY(f.open());
    f.write(id3v24("TBPM", 3, QByteArray("128.00")));
    f.write(QByteArray(64, '\0')); // pretend audio bytes after the tag
    f.flush();
    const QString path = f.fileName();

    const std::optional<int> bpm = AudioBpmTag::readFileBpm(path);
    QVERIFY(bpm.has_value());
    QCOMPARE(*bpm, 128);

    // Missing file → no value.
    QVERIFY(!AudioBpmTag::readFileBpm("/no/such/file.mp3").has_value());

    // Existing file with no ID3 tag → no value.
    QTemporaryFile plain;
    QVERIFY(plain.open());
    plain.write(QByteArray("just some bytes, not a tag"));
    plain.flush();
    QVERIFY(!AudioBpmTag::readFileBpm(plain.fileName()).has_value());
}

void AudioBpmTag_Test::appliesFileBpmToShow()
{
    Doc doc(nullptr, 4);

    // File with a BPM tag → the Show gets BPM_4_4 at that BPM.
    QTemporaryFile f;
    QVERIFY(f.open());
    f.write(id3v24("TBPM", 3, QByteArray("128.00")));
    f.flush();

    Show *tagged = new Show(&doc);
    AudioBpmTag::applyFileBpmToShow(tagged, f.fileName());
    QCOMPARE(tagged->timeDivisionType(), Show::BPM_4_4);
    QCOMPARE(tagged->timeDivisionBPM(), 128);

    // File without a tag → the Show keeps its defaults (Time / 120) and no crash.
    Show *plain = new Show(&doc);
    AudioBpmTag::applyFileBpmToShow(plain, "/no/such/file.mp3");
    QCOMPARE(plain->timeDivisionType(), Show::Time);
    QCOMPARE(plain->timeDivisionBPM(), 120);

    delete tagged;
    delete plain;
}

QTEST_MAIN(AudioBpmTag_Test)
