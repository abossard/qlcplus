/*
  Q Light Controller Plus
  showclipautomation.cpp

  Copyright (c) QLC+ contributors

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QLocale>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <limits>

#include "showclipautomation.h"

#define KXMLShowClipSegment      QStringLiteral("Segment")
#define KXMLShowClipLevel        QStringLiteral("Level")
#define KXMLShowClipDominance    QStringLiteral("Dominance")
#define KXMLShowClipTime         QStringLiteral("Clip")
#define KXMLShowClipSong         QStringLiteral("Song")
#define KXMLShowClipRate         QStringLiteral("Rate")
#define KXMLShowClipValue        QStringLiteral("Value")
#define KXMLShowClipDominant     QStringLiteral("Dominant")

bool ShowClipSegment::operator==(const ShowClipSegment &other) const
{
    return clipMs == other.clipMs && songMs == other.songMs && rate == other.rate;
}

bool ShowClipLevel::operator==(const ShowClipLevel &other) const
{
    return clipMs == other.clipMs && level == other.level;
}

bool ShowClipDominance::operator==(const ShowClipDominance &other) const
{
    return clipMs == other.clipMs && dominant == other.dominant;
}

/** Times rise strictly: one value per clip time */
template <typename T>
static bool risesStrictly(const QVector<T> &items)
{
    for (int i = 1; i < items.count(); i++)
    {
        if (items.at(i).clipMs <= items.at(i - 1).clipMs)
            return false;
    }
    return true;
}

bool ShowClipAutomation::isEmpty() const
{
    return m_segments.isEmpty() && m_envelope.isEmpty() && m_dominance.isEmpty();
}

const QVector<ShowClipSegment> &ShowClipAutomation::segments() const
{
    return m_segments;
}

bool ShowClipAutomation::setSegments(const QVector<ShowClipSegment> &segments)
{
    if (risesStrictly(segments) == false)
        return false;
    for (const ShowClipSegment &segment : segments)
    {
        if (std::isfinite(segment.rate) == false || segment.rate < 0.0)
            return false;
    }
    m_segments = segments;
    return true;
}

const QVector<ShowClipLevel> &ShowClipAutomation::envelope() const
{
    return m_envelope;
}

bool ShowClipAutomation::setEnvelope(const QVector<ShowClipLevel> &keyframes)
{
    if (risesStrictly(keyframes) == false)
        return false;
    for (const ShowClipLevel &keyframe : keyframes)
    {
        // a NaN fails both comparisons
        if (!(keyframe.level >= 0.0 && keyframe.level <= 1.0))
            return false;
    }
    m_envelope = keyframes;
    return true;
}

const QVector<ShowClipDominance> &ShowClipAutomation::dominance() const
{
    return m_dominance;
}

bool ShowClipAutomation::setDominance(const QVector<ShowClipDominance> &steps)
{
    if (risesStrictly(steps) == false)
        return false;
    m_dominance = steps;
    return true;
}

/*********************************************************************
 * Evaluation
 *********************************************************************/

/** Index of the last item at or before clipMs, the first one before it */
template <typename T>
static int governing(const QVector<T> &items, quint32 clipMs)
{
    const auto after = std::upper_bound(items.cbegin(), items.cend(), clipMs,
                                        [](quint32 t, const T &item) { return t < item.clipMs; });
    return qMax(0, int(after - items.cbegin()) - 1);
}

quint32 ShowClipAutomation::songPositionAt(quint32 clipMs) const
{
    if (m_segments.isEmpty())
        return clipMs;

    const ShowClipSegment &segment = m_segments.at(governing(m_segments, clipMs));
    const double position = double(segment.songMs) +
                            (double(clipMs) - double(segment.clipMs)) * segment.rate;
    return quint32(qBound(0.0, std::round(position), double(std::numeric_limits<quint32>::max())));
}

quint32 ShowClipAutomation::inPoint() const
{
    return m_segments.isEmpty() ? 0 : m_segments.first().songMs;
}

qreal ShowClipAutomation::levelAt(quint32 clipMs) const
{
    if (m_envelope.isEmpty())
        return 1.0;

    const int index = governing(m_envelope, clipMs);
    const ShowClipLevel &from = m_envelope.at(index);
    if (clipMs <= from.clipMs || index + 1 == m_envelope.count())
        return from.level;

    const ShowClipLevel &to = m_envelope.at(index + 1);
    const qreal fraction = qreal(clipMs - from.clipMs) / qreal(to.clipMs - from.clipMs);
    return from.level + (to.level - from.level) * fraction;
}

bool ShowClipAutomation::dominantAt(quint32 clipMs) const
{
    if (m_dominance.isEmpty() || clipMs < m_dominance.first().clipMs)
        return true;

    return m_dominance.at(governing(m_dominance, clipMs)).dominant;
}

/*********************************************************************
 * Builders
 *********************************************************************/

/** The slopes from an anchor that keep every sample seen within tolerance */
struct SlopeCone
{
    double low = -std::numeric_limits<double>::infinity();
    double high = std::numeric_limits<double>::infinity();

    bool contains(double slope) const { return slope >= low && slope <= high; }
    bool isEmpty() const { return low > high; }

    /** Narrowed to the slopes from (t0, v0) passing within tolerance of (t, v) */
    void narrow(double t0, double v0, double t, double v, double tolerance)
    {
        low = qMax(low, (v - tolerance - v0) / (t - t0));
        high = qMin(high, (v + tolerance - v0) / (t - t0));
    }
};

QVector<ShowClipSegment> ShowClipAutomation::segmentsFromSamples(const QVector<ShowClipPositionSample> &samples,
                                                                 quint32 toleranceMs)
{
    QVector<ShowClipSegment> segments;
    ShowClipPositionSample anchor;
    ShowClipPositionSample last;
    SlopeCone cone;

    const auto closeSegment = [&]() {
        // the rate of a song at its pitch, or paused, when either fits
        double rate = 1.0;
        if (cone.contains(1.0) == false)
        {
            if (cone.contains(0.0))
                rate = 0.0;
            else if (last.clipMs > anchor.clipMs)
                rate = qBound(cone.low, (double(last.songMs) - anchor.songMs) / (double(last.clipMs) - anchor.clipMs),
                              cone.high);
            rate = qMax(0.0, rate);
        }
        ShowClipSegment segment;
        segment.clipMs = anchor.clipMs;
        segment.songMs = anchor.songMs;
        segment.rate = rate;
        segments.append(segment);
    };

    for (int i = 0; i < samples.count(); i++)
    {
        const ShowClipPositionSample &sample = samples.at(i);
        if (i == 0)
        {
            anchor = last = sample;
            continue;
        }
        if (sample.clipMs <= last.clipMs)
            continue;

        SlopeCone narrowed = cone;
        narrowed.narrow(anchor.clipMs, anchor.songMs, sample.clipMs, sample.songMs, toleranceMs);
        // a rate below 0 is a jump back, never a segment
        if (narrowed.isEmpty() || narrowed.high < 0.0)
        {
            closeSegment();
            anchor = last = sample;
            cone = SlopeCone();
            continue;
        }
        cone = narrowed;
        last = sample;
    }
    if (samples.isEmpty() == false)
        closeSegment();

    return segments;
}

QVector<ShowClipLevel> ShowClipAutomation::envelopeFromSamples(const QVector<ShowClipLevelSample> &samples,
                                                               qreal tolerance)
{
    QVector<ShowClipLevel> keyframes;
    ShowClipLevelSample anchor;
    ShowClipLevelSample last;
    // the slopes from the anchor that pass every sample after it
    SlopeCone cone;
    // every sample since the anchor lies within tolerance of its level
    bool holds = true;

    const auto addKeyframe = [&keyframes](const ShowClipLevelSample &sample) {
        ShowClipLevel keyframe;
        keyframe.clipMs = sample.clipMs;
        keyframe.level = sample.level;
        keyframes.append(keyframe);
    };

    for (int i = 0; i < samples.count(); i++)
    {
        ShowClipLevelSample sample = samples.at(i);
        sample.level = qBound(0.0, sample.level, 1.0);
        if (i == 0)
        {
            anchor = last = sample;
            addKeyframe(anchor);
            continue;
        }
        if (sample.clipMs <= last.clipMs)
            continue;

        const double slope = (sample.level - anchor.level) / (double(sample.clipMs) - anchor.clipMs);
        if (cone.contains(slope) == false)
        {
            // the line to the last sample still passed them all: it ends there
            anchor = last;
            addKeyframe(anchor);
            cone = SlopeCone();
            holds = true;
        }
        cone.narrow(anchor.clipMs, anchor.level, sample.clipMs, sample.level, tolerance);
        holds = holds && qAbs(sample.level - anchor.level) <= tolerance;
        last = sample;
    }
    if (last.clipMs != anchor.clipMs && holds == false)
        addKeyframe(last);

    return keyframes;
}

QVector<ShowClipDominance> ShowClipAutomation::dominanceFromIntervals(const QVector<ShowClipInterval> &intervals)
{
    QVector<ShowClipInterval> sorted;
    for (const ShowClipInterval &interval : intervals)
    {
        if (interval.toMs > interval.fromMs)
            sorted.append(interval);
    }
    std::sort(sorted.begin(), sorted.end(), [](const ShowClipInterval &a, const ShowClipInterval &b) {
        return a.fromMs < b.fromMs;
    });

    // merged, overlapping or touching
    QVector<ShowClipInterval> merged;
    for (const ShowClipInterval &interval : std::as_const(sorted))
    {
        if (merged.isEmpty() == false && interval.fromMs <= merged.last().toMs)
            merged.last().toMs = qMax(merged.last().toMs, interval.toMs);
        else
            merged.append(interval);
    }

    QVector<ShowClipDominance> steps;
    const auto step = [&steps](quint32 clipMs, bool dominant) {
        ShowClipDominance s;
        s.clipMs = clipMs;
        s.dominant = dominant;
        steps.append(s);
    };
    if (merged.isEmpty() || merged.first().fromMs > 0)
        step(0, false);
    for (const ShowClipInterval &interval : std::as_const(merged))
    {
        step(interval.fromMs, true);
        if (interval.toMs != std::numeric_limits<quint32>::max())
            step(interval.toMs, false);
    }

    return steps;
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

/** An unsigned 32 bit attribute, false when missing or not a number */
static bool readTime(const QXmlStreamAttributes &attrs, const QString &name, quint32 *value)
{
    bool ok = false;
    *value = attrs.value(name).toUInt(&ok);
    return ok;
}

static bool readReal(const QXmlStreamAttributes &attrs, const QString &name, qreal *value)
{
    bool ok = false;
    *value = attrs.value(name).toDouble(&ok);
    return ok;
}

static QString realToString(qreal value)
{
    return QString::number(value, 'g', QLocale::FloatingPointShortest);
}

bool ShowClipAutomation::loadXML(QXmlStreamReader &root, QString *error)
{
    *this = ShowClipAutomation();
    if (root.name() != KXMLShowClipAutomation)
    {
        if (error != nullptr)
            *error = QStringLiteral("ClipAutomation node not found");
        return false;
    }

    QVector<ShowClipSegment> segments;
    QVector<ShowClipLevel> envelope;
    QVector<ShowClipDominance> dominance;
    QString reason;

    while (root.readNextStartElement())
    {
        const QXmlStreamAttributes attrs = root.attributes();
        if (root.name() == KXMLShowClipSegment)
        {
            ShowClipSegment segment;
            if (!readTime(attrs, KXMLShowClipTime, &segment.clipMs) ||
                !readTime(attrs, KXMLShowClipSong, &segment.songMs) ||
                !readReal(attrs, KXMLShowClipRate, &segment.rate))
                reason = QStringLiteral("unreadable Segment");
            segments.append(segment);
        }
        else if (root.name() == KXMLShowClipLevel)
        {
            ShowClipLevel keyframe;
            if (!readTime(attrs, KXMLShowClipTime, &keyframe.clipMs) ||
                !readReal(attrs, KXMLShowClipValue, &keyframe.level))
                reason = QStringLiteral("unreadable Level");
            envelope.append(keyframe);
        }
        else if (root.name() == KXMLShowClipDominance)
        {
            ShowClipDominance step;
            quint32 dominant = 1;
            if (!readTime(attrs, KXMLShowClipTime, &step.clipMs) ||
                !readTime(attrs, KXMLShowClipDominant, &dominant) || dominant > 1)
                reason = QStringLiteral("unreadable Dominance");
            step.dominant = dominant == 1;
            dominance.append(step);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown ClipAutomation tag:" << root.name();
        }
        root.skipCurrentElement();
    }

    ShowClipAutomation loaded;
    if (reason.isEmpty() && loaded.setSegments(segments) == false)
        reason = QStringLiteral("Segments need rising clip times and finite rates of at least 0");
    if (reason.isEmpty() && loaded.setEnvelope(envelope) == false)
        reason = QStringLiteral("Levels need rising clip times and values in [0, 1]");
    if (reason.isEmpty() && loaded.setDominance(dominance) == false)
        reason = QStringLiteral("Dominance steps need rising clip times");

    if (error != nullptr)
        *error = reason;
    if (reason.isEmpty() == false)
        return false;

    *this = loaded;
    return true;
}

void ShowClipAutomation::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);

    doc->writeStartElement(KXMLShowClipAutomation);
    for (const ShowClipSegment &segment : m_segments)
    {
        doc->writeStartElement(KXMLShowClipSegment);
        doc->writeAttribute(KXMLShowClipTime, QString::number(segment.clipMs));
        doc->writeAttribute(KXMLShowClipSong, QString::number(segment.songMs));
        doc->writeAttribute(KXMLShowClipRate, realToString(segment.rate));
        doc->writeEndElement();
    }
    for (const ShowClipLevel &keyframe : m_envelope)
    {
        doc->writeStartElement(KXMLShowClipLevel);
        doc->writeAttribute(KXMLShowClipTime, QString::number(keyframe.clipMs));
        doc->writeAttribute(KXMLShowClipValue, realToString(keyframe.level));
        doc->writeEndElement();
    }
    for (const ShowClipDominance &step : m_dominance)
    {
        doc->writeStartElement(KXMLShowClipDominance);
        doc->writeAttribute(KXMLShowClipTime, QString::number(step.clipMs));
        doc->writeAttribute(KXMLShowClipDominant, QString::number(step.dominant ? 1 : 0));
        doc->writeEndElement();
    }
    doc->writeEndElement();
}

bool ShowClipAutomation::operator==(const ShowClipAutomation &other) const
{
    return m_segments == other.m_segments && m_envelope == other.m_envelope && m_dominance == other.m_dominance;
}
