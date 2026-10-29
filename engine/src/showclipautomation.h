/*
  Q Light Controller Plus
  showclipautomation.h

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

#ifndef SHOWCLIPAUTOMATION_H
#define SHOWCLIPAUTOMATION_H

#include <QtGlobal>
#include <QString>
#include <QVector>

class QXmlStreamReader;
class QXmlStreamWriter;

/** @addtogroup engine_functions Functions
 * @{
 */

#define KXMLShowClipAutomation QStringLiteral("ClipAutomation")

/** From clipMs on, the clip plays its function from songMs at rate:
 *  a loop wrap or a seek starts a new segment, a pause is rate 0, pitch is the rate */
struct ShowClipSegment
{
    quint32 clipMs = 0;
    quint32 songMs = 0;
    qreal rate = 1.0;

    bool operator==(const ShowClipSegment &other) const;
};

/** The clip level at clipMs, linear to the next keyframe */
struct ShowClipLevel
{
    quint32 clipMs = 0;
    qreal level = 1.0;

    bool operator==(const ShowClipLevel &other) const;
};

/** From clipMs on the clip is dominant, or not */
struct ShowClipDominance
{
    quint32 clipMs = 0;
    bool dominant = true;

    bool operator==(const ShowClipDominance &other) const;
};

/** The song position a deck reported at a clip time */
struct ShowClipPositionSample
{
    quint32 clipMs = 0;
    quint32 songMs = 0;
};

/** The mix weight a deck had at a clip time */
struct ShowClipLevelSample
{
    quint32 clipMs = 0;
    qreal level = 1.0;
};

/** A clip time span [fromMs, toMs) a deck was the active one; UINT_MAX ends never */
struct ShowClipInterval
{
    quint32 fromMs = 0;
    quint32 toMs = 0;
};

/**
 * How a Show clip plays its function over the clip's own time, in ms from
 * the clip start: the song position (position map), the level (envelope)
 * and whether the clip is the dominant one. A value: every member function
 * is a calculation. Empty parts mean identity from 0, level 1 and dominant.
 */
class ShowClipAutomation
{
public:
    bool isEmpty() const;

    /** Ordered by strictly rising clipMs */
    const QVector<ShowClipSegment> &segments() const;
    /** Refused whole, leaving the map alone, unless clipMs rises strictly
     *  and every rate is finite and not negative */
    bool setSegments(const QVector<ShowClipSegment> &segments);

    const QVector<ShowClipLevel> &envelope() const;
    /** Refused whole unless clipMs rises strictly and every level is in [0, 1] */
    bool setEnvelope(const QVector<ShowClipLevel> &keyframes);

    const QVector<ShowClipDominance> &dominance() const;
    /** Refused whole unless clipMs rises strictly */
    bool setDominance(const QVector<ShowClipDominance> &steps);

    /** The song position at clip time t: songMs + (t - clipMs) x rate of the
     *  last segment with clipMs <= t, the first one before it, never below 0.
     *  t without segments. */
    quint32 songPositionAt(quint32 clipMs) const;

    /** Where the clip starts its function: the first segment's songMs, 0 without */
    quint32 inPoint() const;

    /** Linear between keyframes, the first one's level before it, the last
     *  one's after it, 1.0 without keyframes */
    qreal levelAt(quint32 clipMs) const;

    /** The last step at or before t, dominant before the first one */
    bool dominantAt(quint32 clipMs) const;

    /**
     * Recorder compression. Each builder takes samples in rising clip time
     * and skips one that does not come after the one before it.
     */

    /** Segments every sample lies within toleranceMs of. A segment starts
     *  at the sample that leaves the prediction of the one before it, at
     *  the observed position: a loop wrap, a seek, a pause or a pitch change.
     *  A rate of exactly 1 or 0 is chosen when it fits. */
    static QVector<ShowClipSegment> segmentsFromSamples(const QVector<ShowClipPositionSample> &samples,
                                                        quint32 toleranceMs);

    /** Keyframes, each one a sample, whose lines lie within tolerance of
     *  every sample. The last sample is a keyframe unless holding the one
     *  before it fits. */
    static QVector<ShowClipLevel> envelopeFromSamples(const QVector<ShowClipLevelSample> &samples,
                                                      qreal tolerance);

    /** Steps dominant exactly inside the intervals, overlapping or touching
     *  ones merged: not dominant from 0 when the first starts later */
    static QVector<ShowClipDominance> dominanceFromIntervals(const QVector<ShowClipInterval> &intervals);

    /** Load from a <ClipAutomation> element. An unreadable one leaves this
     *  value empty and returns false with the reason. */
    bool loadXML(QXmlStreamReader &root, QString *error = nullptr);

    /** Save as a <ClipAutomation> element */
    void saveXML(QXmlStreamWriter *doc) const;

    bool operator==(const ShowClipAutomation &other) const;
    bool operator!=(const ShowClipAutomation &other) const { return !(*this == other); }

private:
    QVector<ShowClipSegment> m_segments;
    QVector<ShowClipLevel> m_envelope;
    QVector<ShowClipDominance> m_dominance;
};

/** @} */

#endif
