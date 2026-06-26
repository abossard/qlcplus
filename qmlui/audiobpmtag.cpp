/*
  Q Light Controller Plus
  audiobpmtag.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "audiobpmtag.h"
#include "show.h"

#include <QFile>
#include <QtGlobal>

namespace
{

// Synchsafe 32-bit integer (7 bits per byte), big-endian. Used for the ID3v2
// tag size (all versions) and ID3v2.4 frame sizes.
quint32 synchsafe32(const uchar *p)
{
    return (quint32(p[0] & 0x7F) << 21) | (quint32(p[1] & 0x7F) << 14)
         | (quint32(p[2] & 0x7F) << 7)  |  quint32(p[3] & 0x7F);
}

// Plain 32-bit big-endian (ID3v2.3 frame size).
quint32 be32(const uchar *p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16)
         | (quint32(p[2]) << 8)  |  quint32(p[3]);
}

// 24-bit big-endian (ID3v2.2 frame size).
quint32 be24(const uchar *p)
{
    return (quint32(p[0]) << 16) | (quint32(p[1]) << 8) | quint32(p[2]);
}

// Decode an ID3 text frame body (the bytes AFTER the frame header) into a
// QString, honoring the leading text-encoding byte.
QString decodeTextFrame(const QByteArray &body)
{
    if (body.isEmpty())
        return QString();

    const uchar enc = uchar(body.at(0));
    const QByteArray text = body.mid(1);

    switch (enc)
    {
    case 0: // ISO-8859-1
        return QString::fromLatin1(text);
    case 1: // UTF-16 with BOM
        return QString::fromUtf16(reinterpret_cast<const char16_t *>(text.constData()),
                                  text.size() / 2);
    case 2: // UTF-16BE without BOM
    {
        QString s;
        for (int i = 0; i + 1 < text.size(); i += 2)
            s.append(QChar((uchar(text[i]) << 8) | uchar(text[i + 1])));
        return s;
    }
    case 3: // UTF-8
    default:
        return QString::fromUtf8(text);
    }
}

// Parse a leading numeric BPM from frame text ("128", "128.00", "124.96"),
// skipping any leading junk (e.g. a UTF-16 BOM). Returns the rounded value.
std::optional<int> bpmFromText(const QString &text)
{
    const int n = text.size();
    int i = 0;
    while (i < n && !text[i].isDigit())
        ++i; // skip BOM / stray bytes before the number

    const int start = i;
    bool seenDot = false;
    while (i < n)
    {
        const QChar c = text[i];
        if (c.isDigit()) { ++i; continue; }
        if (c == '.' && !seenDot) { seenDot = true; ++i; continue; }
        break;
    }
    if (i == start)
        return std::nullopt;

    bool ok = false;
    const double v = text.mid(start, i - start).toDouble(&ok);
    if (!ok || v <= 0.0)
        return std::nullopt;
    return qRound(v);
}

} // namespace

std::optional<int> AudioBpmTag::parseId3v2Bpm(const QByteArray &data)
{
    if (data.size() < 10)
        return std::nullopt;

    const uchar *d = reinterpret_cast<const uchar *>(data.constData());
    if (d[0] != 'I' || d[1] != 'D' || d[2] != '3')
        return std::nullopt;

    const int major = d[3]; // 2, 3 or 4
    if (major < 2 || major > 4)
        return std::nullopt;

    const uchar flags = d[5];
    const quint32 tagSize = synchsafe32(d + 6);
    const int tagEnd = qMin(int(10 + tagSize), data.size());
    int pos = 10;

    // Skip an extended header if present (ID3v2.3/2.4 flag bit 0x40). Validate
    // the (untrusted) size so a bogus value can't push pos out of bounds.
    if ((major == 3 || major == 4) && (flags & 0x40))
    {
        if (pos + 4 > tagEnd)
            return std::nullopt;
        const quint32 extSize = (major == 4) ? synchsafe32(d + pos) : be32(d + pos);
        // v2.4: synchsafe size includes the 4 size bytes; v2.3: size excludes them.
        const qint64 newPos = qint64(pos) + ((major == 4) ? qint64(extSize)
                                                          : qint64(4) + extSize);
        if (newPos <= pos || newPos > tagEnd)
            return std::nullopt;
        pos = int(newPos);
    }

    const int idLen = (major == 2) ? 3 : 4;
    const int frameHeader = (major == 2) ? 6 : 10; // id + size (+ 2 flag bytes)
    const char *wantId = (major == 2) ? "TBP" : "TBPM";

    while (pos + frameHeader <= tagEnd)
    {
        if (d[pos] == 0) // padding region reached
            break;

        const char *fid = reinterpret_cast<const char *>(d + pos);
        quint32 frameSize;
        if (major == 2)      frameSize = be24(d + pos + idLen);
        else if (major == 3) frameSize = be32(d + pos + idLen);
        else                 frameSize = synchsafe32(d + pos + idLen); // v2.4

        const int bodyPos = pos + frameHeader;
        // 64-bit compare: a corrupt frameSize must not overflow int and slip
        // past the bounds check.
        if (frameSize == 0 || qint64(bodyPos) + qint64(frameSize) > tagEnd)
            break;

        if (qstrncmp(fid, wantId, idLen) == 0)
        {
            const QByteArray body(reinterpret_cast<const char *>(d + bodyPos), int(frameSize));
            return bpmFromText(decodeTextFrame(body));
        }
        pos = bodyPos + int(frameSize);
    }
    return std::nullopt;
}

std::optional<int> AudioBpmTag::readFileBpm(const QString &filePath)
{
    QFile f(filePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return std::nullopt;

    const QByteArray head = f.read(10);
    if (head.size() < 10 || head[0] != 'I' || head[1] != 'D' || head[2] != '3')
        return std::nullopt;

    const uchar *h = reinterpret_cast<const uchar *>(head.constData());
    const quint32 tagSize = synchsafe32(h + 6);
    // Cap the read so a huge embedded artwork tag doesn't force a large read;
    // TBPM frames are front-loaded in practice.
    const quint32 cap = 5u * 1024u * 1024u;
    const QByteArray rest = f.read(qMin(tagSize, cap));
    return parseId3v2Bpm(head + rest);
}

void AudioBpmTag::applyFileBpmToShow(Show *show, const QString &filePath)
{
    if (show == nullptr)
        return;
    if (const std::optional<int> bpm = readFileBpm(filePath))
        show->setTimeDivision(Show::BPM_4_4, *bpm);
}
