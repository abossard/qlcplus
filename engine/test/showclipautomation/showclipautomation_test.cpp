/*
  Q Light Controller Plus - Unit test
  showclipautomation_test.cpp

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

#include <QtTest>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <cmath>
#include <limits>

#include "showclipautomation_test.h"
#include "showclipautomation.h"
#include "showfunction.h"

typedef QVector<ShowClipSegment> Segments;
typedef QVector<ShowClipLevel> Levels;
typedef QVector<ShowClipDominance> Steps;
typedef QVector<ShowClipPositionSample> PositionSamples;
typedef QVector<ShowClipLevelSample> LevelSamples;
typedef QVector<ShowClipInterval> Intervals;

Q_DECLARE_METATYPE(ShowClipAutomation)
Q_DECLARE_METATYPE(Segments)
Q_DECLARE_METATYPE(Levels)
Q_DECLARE_METATYPE(Steps)
Q_DECLARE_METATYPE(PositionSamples)
Q_DECLARE_METATYPE(LevelSamples)
Q_DECLARE_METATYPE(Intervals)

char *toString(const ShowClipSegment &s)
{
    return qstrdup(qPrintable(QString("{%1, %2, %3}").arg(s.clipMs).arg(s.songMs).arg(s.rate, 0, 'g', 17)));
}

char *toString(const ShowClipLevel &l)
{
    return qstrdup(qPrintable(QString("{%1, %2}").arg(l.clipMs).arg(l.level, 0, 'g', 17)));
}

char *toString(const ShowClipDominance &d)
{
    return qstrdup(qPrintable(QString("{%1, %2}").arg(d.clipMs).arg(d.dominant)));
}

namespace
{

ShowClipSegment segment(quint32 clipMs, quint32 songMs, qreal rate)
{
    ShowClipSegment s;
    s.clipMs = clipMs;
    s.songMs = songMs;
    s.rate = rate;
    return s;
}

ShowClipLevel keyframe(quint32 clipMs, qreal value)
{
    ShowClipLevel l;
    l.clipMs = clipMs;
    l.level = value;
    return l;
}

ShowClipDominance step(quint32 clipMs, bool dominant)
{
    ShowClipDominance d;
    d.clipMs = clipMs;
    d.dominant = dominant;
    return d;
}

ShowClipInterval interval(quint32 fromMs, quint32 toMs)
{
    ShowClipInterval i;
    i.fromMs = fromMs;
    i.toMs = toMs;
    return i;
}

/** A deck sampled every stepMs from fromMs to toMs inclusive, playing from songMs at rate */
PositionSamples playing(quint32 fromMs, quint32 toMs, quint32 stepMs, quint32 songMs, qreal rate)
{
    PositionSamples samples;
    for (quint32 t = fromMs; t <= toMs; t += stepMs)
    {
        ShowClipPositionSample s;
        s.clipMs = t;
        s.songMs = quint32(std::round(songMs + (t - fromMs) * rate));
        samples.append(s);
    }
    return samples;
}

/** A weight sampled every stepMs from fromMs to toMs inclusive, linear from one level to another */
LevelSamples ramp(quint32 fromMs, quint32 toMs, quint32 stepMs, qreal from, qreal to)
{
    LevelSamples samples;
    for (quint32 t = fromMs; t <= toMs; t += stepMs)
    {
        ShowClipLevelSample s;
        s.clipMs = t;
        s.level = toMs == fromMs ? to : from + (to - from) * qreal(t - fromMs) / qreal(toMs - fromMs);
        samples.append(s);
    }
    return samples;
}

ShowClipAutomation automation(const Segments &segments, const Levels &envelope, const Steps &dominance)
{
    ShowClipAutomation a;
    a.setSegments(segments);
    a.setEnvelope(envelope);
    a.setDominance(dominance);
    return a;
}

QString saved(const ShowClipAutomation &a)
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    a.saveXML(&writer);
    return xml;
}

}

/*********************************************************************
 * Evaluation
 *********************************************************************/

void ShowClipAutomation_Test::songPosition_data()
{
    QTest::addColumn<Segments>("segments");
    QTest::addColumn<quint32>("clipMs");
    QTest::addColumn<quint32>("songMs");

    QTest::newRow("no map is the clip time") << Segments() << 1234u << 1234u;
    QTest::newRow("in-point at the clip start") << Segments{segment(0, 30000, 1.0)} << 0u << 30000u;
    QTest::newRow("in-point plays on") << Segments{segment(0, 30000, 1.0)} << 1500u << 31500u;
    const Segments loop{segment(0, 1000, 1.0), segment(1900, 1000, 1.0)};
    QTest::newRow("loop before the wrap") << loop << 1899u << 2899u;
    QTest::newRow("loop at the wrap") << loop << 1900u << 1000u;
    QTest::newRow("loop after the wrap") << loop << 2000u << 1100u;
    const Segments pause{segment(0, 500, 1.0), segment(1000, 1500, 0.0), segment(3000, 1500, 1.0)};
    QTest::newRow("paused") << pause << 2000u << 1500u;
    QTest::newRow("resumed") << pause << 3500u << 2000u;
    QTest::newRow("pitch is the rate") << Segments{segment(0, 0, 1.08)} << 1000u << 1080u;
    QTest::newRow("rounded to the nearest ms") << Segments{segment(0, 0, 0.5)} << 3u << 2u;
    QTest::newRow("before the first segment it extends back") << Segments{segment(1000, 5000, 1.0)} << 400u << 4400u;
    QTest::newRow("never before the song start") << Segments{segment(1000, 200, 1.0)} << 0u << 0u;
    QTest::newRow("never past the clock range")
        << Segments{segment(0, std::numeric_limits<quint32>::max() - 10, 2.0)} << 100u
        << std::numeric_limits<quint32>::max();
}

void ShowClipAutomation_Test::songPosition()
{
    QFETCH(Segments, segments);
    QFETCH(quint32, clipMs);
    QFETCH(quint32, songMs);

    ShowClipAutomation a;
    QVERIFY(a.setSegments(segments));
    QCOMPARE(a.songPositionAt(clipMs), songMs);
}

void ShowClipAutomation_Test::inPoint_data()
{
    QTest::addColumn<Segments>("segments");
    QTest::addColumn<quint32>("inPoint");

    QTest::newRow("no map starts at 0") << Segments() << 0u;
    QTest::newRow("the first segment's song position")
        << Segments{segment(0, 42000, 1.0), segment(5000, 1000, 1.0)} << 42000u;
}

void ShowClipAutomation_Test::inPoint()
{
    QFETCH(Segments, segments);
    QFETCH(quint32, inPoint);

    ShowClipAutomation a;
    QVERIFY(a.setSegments(segments));
    QCOMPARE(a.inPoint(), inPoint);
}

void ShowClipAutomation_Test::level_data()
{
    QTest::addColumn<Levels>("envelope");
    QTest::addColumn<quint32>("clipMs");
    QTest::addColumn<qreal>("level");

    QTest::newRow("no envelope is full") << Levels() << 500u << 1.0;
    QTest::newRow("one keyframe before it") << Levels{keyframe(500, 0.25)} << 0u << 0.25;
    QTest::newRow("one keyframe after it") << Levels{keyframe(500, 0.25)} << 9000u << 0.25;
    const Levels fadeIn{keyframe(0, 0.0), keyframe(1000, 1.0)};
    QTest::newRow("linear between keyframes") << fadeIn << 250u << 0.25;
    QTest::newRow("at a keyframe") << fadeIn << 1000u << 1.0;
    QTest::newRow("held after the last") << fadeIn << 5000u << 1.0;
    const Levels crossfade{keyframe(0, 1.0), keyframe(1000, 1.0), keyframe(3000, 0.0)};
    QTest::newRow("flat part") << crossfade << 600u << 1.0;
    QTest::newRow("falling part") << crossfade << 2500u << 0.25;
}

void ShowClipAutomation_Test::level()
{
    QFETCH(Levels, envelope);
    QFETCH(quint32, clipMs);
    QFETCH(qreal, level);

    ShowClipAutomation a;
    QVERIFY(a.setEnvelope(envelope));
    QCOMPARE(a.levelAt(clipMs), level);
}

void ShowClipAutomation_Test::dominant_data()
{
    QTest::addColumn<Steps>("dominance");
    QTest::addColumn<quint32>("clipMs");
    QTest::addColumn<bool>("dominant");

    QTest::newRow("no steps is dominant") << Steps() << 700u << true;
    const Steps handover{step(0, false), step(2000, true), step(5000, false)};
    QTest::newRow("before the handover") << handover << 1999u << false;
    QTest::newRow("at the handover") << handover << 2000u << true;
    QTest::newRow("inside") << handover << 4999u << true;
    QTest::newRow("handed back") << handover << 5000u << false;
    QTest::newRow("dominant before the first step") << Steps{step(3000, false)} << 0u << true;
}

void ShowClipAutomation_Test::dominant()
{
    QFETCH(Steps, dominance);
    QFETCH(quint32, clipMs);
    QFETCH(bool, dominant);

    ShowClipAutomation a;
    QVERIFY(a.setDominance(dominance));
    QCOMPARE(a.dominantAt(clipMs), dominant);
}

void ShowClipAutomation_Test::refusedValuesKeepThePreviousOnes_data()
{
    QTest::addColumn<Segments>("segments");
    QTest::addColumn<Levels>("envelope");
    QTest::addColumn<Steps>("dominance");
    QTest::addColumn<bool>("accepted");

    const qreal nan = std::numeric_limits<qreal>::quiet_NaN();
    const qreal inf = std::numeric_limits<qreal>::infinity();
    QTest::newRow("valid") << Segments{segment(0, 0, 1.0), segment(10, 0, 0.0)}
                           << Levels{keyframe(0, 0.0), keyframe(10, 1.0)} << Steps{step(0, false), step(10, true)} << true;
    QTest::newRow("segment times repeat") << Segments{segment(5, 0, 1.0), segment(5, 9, 1.0)} << Levels() << Steps()
                                          << false;
    QTest::newRow("segment times fall") << Segments{segment(5, 0, 1.0), segment(4, 9, 1.0)} << Levels() << Steps()
                                        << false;
    QTest::newRow("negative rate") << Segments{segment(0, 0, -1.0)} << Levels() << Steps() << false;
    QTest::newRow("NaN rate") << Segments{segment(0, 0, nan)} << Levels() << Steps() << false;
    QTest::newRow("infinite rate") << Segments{segment(0, 0, inf)} << Levels() << Steps() << false;
    QTest::newRow("level above 1") << Segments() << Levels{keyframe(0, 1.5)} << Steps() << false;
    QTest::newRow("level below 0") << Segments() << Levels{keyframe(0, -0.1)} << Steps() << false;
    QTest::newRow("NaN level") << Segments() << Levels{keyframe(0, nan)} << Steps() << false;
    QTest::newRow("level times repeat") << Segments() << Levels{keyframe(3, 0.0), keyframe(3, 1.0)} << Steps() << false;
    QTest::newRow("dominance times repeat") << Segments() << Levels() << Steps{step(3, false), step(3, true)}
                                            << false;
}

void ShowClipAutomation_Test::refusedValuesKeepThePreviousOnes()
{
    QFETCH(Segments, segments);
    QFETCH(Levels, envelope);
    QFETCH(Steps, dominance);
    QFETCH(bool, accepted);

    const ShowClipAutomation before =
        automation({segment(0, 7000, 1.0)}, {keyframe(0, 0.5)}, {step(0, false)});
    ShowClipAutomation a = before;
    const bool segmentsSet = a.setSegments(segments);
    const bool envelopeSet = a.setEnvelope(envelope);
    const bool dominanceSet = a.setDominance(dominance);

    QCOMPARE(segmentsSet && envelopeSet && dominanceSet, accepted);
    // a refused part stays as it was
    QCOMPARE(a.segments(), segmentsSet ? segments : before.segments());
    QCOMPARE(a.envelope(), envelopeSet ? envelope : before.envelope());
    QCOMPARE(a.dominance(), dominanceSet ? dominance : before.dominance());
}

/*********************************************************************
 * Builders
 *********************************************************************/

void ShowClipAutomation_Test::segmentsFromSamples_data()
{
    QTest::addColumn<PositionSamples>("samples");
    QTest::addColumn<quint32>("tolerance");
    QTest::addColumn<Segments>("segments");

    QTest::newRow("no samples") << PositionSamples() << 20u << Segments();
    QTest::newRow("one sample") << playing(40, 40, 10, 30000, 1.0) << 20u << Segments{segment(40, 30000, 1.0)};
    QTest::newRow("steady play from an in-point")
        << playing(0, 2000, 40, 30000, 1.0) << 20u << Segments{segment(0, 30000, 1.0)};
    QTest::newRow("loop wrap")
        << playing(0, 1900, 100, 1000, 1.0) + playing(2000, 3000, 100, 1000, 1.0) << 20u
        << Segments{segment(0, 1000, 1.0), segment(2000, 1000, 1.0)};
    QTest::newRow("seek forward")
        << playing(0, 1000, 100, 0, 1.0) + playing(1100, 2000, 100, 60000, 1.0) << 20u
        << Segments{segment(0, 0, 1.0), segment(1100, 60000, 1.0)};
    QTest::newRow("pause and resume")
        << playing(0, 1000, 100, 500, 1.0) + playing(1100, 2000, 100, 1500, 0.0) +
               playing(2100, 3000, 100, 1600, 1.0)
        << 20u << Segments{segment(0, 500, 1.0), segment(1100, 1500, 0.0), segment(2100, 1600, 1.0)};
    QTest::newRow("pitch") << playing(0, 1000, 100, 0, 1.08) << 5u << Segments{segment(0, 0, 1.08)};
    QTest::newRow("a sample that does not come later is skipped")
        << playing(0, 200, 100, 0, 1.0) + playing(200, 200, 10, 9000, 1.0) + playing(300, 400, 100, 300, 1.0)
        << 20u << Segments{segment(0, 0, 1.0)};
}

void ShowClipAutomation_Test::segmentsFromSamples()
{
    QFETCH(PositionSamples, samples);
    QFETCH(quint32, tolerance);
    QFETCH(Segments, segments);

    const Segments built = ShowClipAutomation::segmentsFromSamples(samples, tolerance);
    QCOMPARE(built.count(), segments.count());
    for (int i = 0; i < built.count(); i++)
    {
        QCOMPARE(built.at(i).clipMs, segments.at(i).clipMs);
        QCOMPARE(built.at(i).songMs, segments.at(i).songMs);
        QVERIFY2(qAbs(built.at(i).rate - segments.at(i).rate) < 1e-9, toString(built.at(i)));
    }
    ShowClipAutomation a;
    QVERIFY(a.setSegments(built));
}

void ShowClipAutomation_Test::segmentsKeepJitteredSamplesWithinTolerance_data()
{
    QTest::addColumn<qreal>("rate");
    QTest::addColumn<quint32>("tolerance");
    QTest::addColumn<int>("segmentCount");

    QTest::newRow("at pitch 1") << 1.0 << 20u << 1;
    QTest::newRow("at pitch 0.94") << 0.94 << 20u << 1;
    QTest::newRow("tolerance tighter than the jitter") << 1.0 << 3u << -1;
}

void ShowClipAutomation_Test::segmentsKeepJitteredSamplesWithinTolerance()
{
    QFETCH(qreal, rate);
    QFETCH(quint32, tolerance);
    QFETCH(int, segmentCount);

    // telemetry arrives every 33 ms or so, reporting a position up to 8 ms off
    static const int jitter[] = { 0, 5, -3, 8, -8, 2, -6, 7, -1, 4 };
    PositionSamples samples;
    quint32 t = 0;
    for (int i = 0; i < 600; i++)
    {
        ShowClipPositionSample s;
        s.clipMs = t;
        s.songMs = quint32(qMax(0.0, std::round(10000 + t * rate) + jitter[i % 10]));
        samples.append(s);
        t += 30 + (i % 7);
    }

    ShowClipAutomation a;
    QVERIFY(a.setSegments(ShowClipAutomation::segmentsFromSamples(samples, tolerance)));
    if (segmentCount >= 0)
        QCOMPARE(a.segments().count(), segmentCount);
    else
        QVERIFY(a.segments().count() > 1);
    for (const ShowClipPositionSample &s : std::as_const(samples))
    {
        const qint64 error = qint64(a.songPositionAt(s.clipMs)) - s.songMs;
        QVERIFY2(qAbs(error) <= qint64(tolerance) + 1,
                 qPrintable(QString("%1 ms off at %2").arg(error).arg(s.clipMs)));
    }
}

void ShowClipAutomation_Test::envelopeFromSamples_data()
{
    QTest::addColumn<LevelSamples>("samples");
    QTest::addColumn<Levels>("envelope");

    QTest::newRow("no samples") << LevelSamples() << Levels();
    QTest::newRow("full all along") << ramp(0, 2000, 100, 1.0, 1.0) << Levels{keyframe(0, 1.0)};
    QTest::newRow("crossfade out")
        << ramp(0, 1000, 100, 1.0, 1.0) + ramp(1100, 3000, 100, 0.95, 0.0) + ramp(3100, 4000, 100, 0.0, 0.0)
        << Levels{keyframe(0, 1.0), keyframe(1000, 1.0), keyframe(3000, 0.0)};
    QTest::newRow("ends on a slope") << ramp(0, 1000, 100, 0.0, 1.0) << Levels{keyframe(0, 0.0), keyframe(1000, 1.0)};
    QTest::newRow("weights outside [0, 1] are clamped")
        << ramp(0, 0, 100, 1.4, 1.4) << Levels{keyframe(0, 1.0)};
    QTest::newRow("a sample that does not come later is skipped")
        << ramp(0, 500, 100, 0.5, 0.5) + ramp(500, 500, 100, 0.0, 0.0) << Levels{keyframe(0, 0.5)};
}

void ShowClipAutomation_Test::envelopeFromSamples()
{
    QFETCH(LevelSamples, samples);
    QFETCH(Levels, envelope);

    const Levels built = ShowClipAutomation::envelopeFromSamples(samples, 0.01);
    QCOMPARE(built.count(), envelope.count());
    for (int i = 0; i < built.count(); i++)
    {
        QCOMPARE(built.at(i).clipMs, envelope.at(i).clipMs);
        QVERIFY2(qAbs(built.at(i).level - envelope.at(i).level) < 1e-9, toString(built.at(i)));
    }

    // every sample taken lies within the tolerance of the envelope
    ShowClipAutomation a;
    QVERIFY(a.setEnvelope(built));
    for (int i = 0; i < samples.count(); i++)
    {
        const ShowClipLevelSample &s = samples.at(i);
        if (i > 0 && s.clipMs <= samples.at(i - 1).clipMs)
            continue;
        QVERIFY(qAbs(a.levelAt(s.clipMs) - qBound(0.0, s.level, 1.0)) <= 0.01 + 1e-9);
    }
}

void ShowClipAutomation_Test::dominanceFromIntervals_data()
{
    QTest::addColumn<Intervals>("intervals");
    QTest::addColumn<Steps>("steps");

    const quint32 open = std::numeric_limits<quint32>::max();
    QTest::newRow("never active") << Intervals() << Steps{step(0, false)};
    QTest::newRow("active from the start") << Intervals{interval(0, 5000)} << Steps{step(0, true), step(5000, false)};
    QTest::newRow("active in the middle")
        << Intervals{interval(2000, 4000)} << Steps{step(0, false), step(2000, true), step(4000, false)};
    QTest::newRow("active to the end") << Intervals{interval(0, open)} << Steps{step(0, true)};
    QTest::newRow("overlapping merged")
        << Intervals{interval(0, 3000), interval(2000, 5000)} << Steps{step(0, true), step(5000, false)};
    QTest::newRow("touching merged")
        << Intervals{interval(0, 3000), interval(3000, 5000)} << Steps{step(0, true), step(5000, false)};
    QTest::newRow("out of order")
        << Intervals{interval(6000, 7000), interval(1000, 2000)}
        << Steps{step(0, false), step(1000, true), step(2000, false), step(6000, true), step(7000, false)};
    QTest::newRow("empty interval ignored")
        << Intervals{interval(3000, 3000)} << Steps{step(0, false)};
}

void ShowClipAutomation_Test::dominanceFromIntervals()
{
    QFETCH(Intervals, intervals);
    QFETCH(Steps, steps);

    const Steps built = ShowClipAutomation::dominanceFromIntervals(intervals);
    QCOMPARE(built, steps);
    ShowClipAutomation a;
    QVERIFY(a.setDominance(built));
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

void ShowClipAutomation_Test::xmlRoundTrip_data()
{
    QTest::addColumn<ShowClipAutomation>("automation");

    QTest::newRow("empty") << ShowClipAutomation();
    QTest::newRow("in-point only") << automation({segment(0, 42000, 1.0)}, {}, {});
    QTest::newRow("every part") << automation({segment(0, 1000, 1.0), segment(1933, 1000, 1.0794871794871794),
                                               segment(4000, 3000, 0.0)},
                                              {keyframe(0, 0.0), keyframe(1500, 0.3333333333333333), keyframe(4000, 1.0)},
                                              {step(0, false), step(2500, true)});
}

void ShowClipAutomation_Test::xmlRoundTrip()
{
    QFETCH(ShowClipAutomation, automation);

    QXmlStreamReader reader(saved(automation));
    reader.readNextStartElement();
    QCOMPARE(reader.name().toString(), QString("ClipAutomation"));
    ShowClipAutomation loaded = automation;
    loaded.setSegments({segment(0, 1, 1.0)});
    QString error;
    QVERIFY2(loaded.loadXML(reader, &error), qPrintable(error));
    QCOMPARE(loaded, automation);
    QVERIFY(error.isEmpty());
}

void ShowClipAutomation_Test::xmlUnreadable_data()
{
    QTest::addColumn<QString>("body");

    QTest::newRow("negative rate") << "<Segment Clip=\"0\" Song=\"0\" Rate=\"-1\"/>";
    QTest::newRow("rate not a number") << "<Segment Clip=\"0\" Song=\"0\" Rate=\"fast\"/>";
    QTest::newRow("missing song") << "<Segment Clip=\"0\" Rate=\"1\"/>";
    QTest::newRow("negative clip time") << "<Level Clip=\"-5\" Value=\"0.5\"/>";
    QTest::newRow("level above 1") << "<Level Clip=\"0\" Value=\"2\"/>";
    QTest::newRow("times not rising") << "<Level Clip=\"10\" Value=\"0\"/><Level Clip=\"10\" Value=\"1\"/>";
    QTest::newRow("dominance not a flag") << "<Dominance Clip=\"0\" Dominant=\"2\"/>";
}

void ShowClipAutomation_Test::xmlUnreadable()
{
    QFETCH(QString, body);

    // the element after the unreadable one still loads
    QXmlStreamReader reader(QString("<Clip><ClipAutomation>%1<Unknown/></ClipAutomation><After/></Clip>").arg(body));
    reader.readNextStartElement();
    reader.readNextStartElement();
    ShowClipAutomation a = automation({segment(0, 9, 1.0)}, {}, {});
    QString error;
    QVERIFY(a.loadXML(reader, &error) == false);
    QVERIFY(error.isEmpty() == false);
    QVERIFY(a.isEmpty());
    QVERIFY(reader.readNextStartElement());
    QCOMPARE(reader.name().toString(), QString("After"));
}

void ShowClipAutomation_Test::showFunctionXml_data()
{
    QTest::addColumn<ShowClipAutomation>("automation");
    QTest::addColumn<bool>("written");

    QTest::newRow("no automation writes no element") << ShowClipAutomation() << false;
    QTest::newRow("automation is a child element")
        << automation({segment(0, 30000, 1.0), segment(1900, 30000, 1.0)}, {keyframe(0, 0.0), keyframe(800, 1.0)},
                      {step(0, false), step(400, true)})
        << true;
}

void ShowClipAutomation_Test::showFunctionXml()
{
    QFETCH(ShowClipAutomation, automation);
    QFETCH(bool, written);

    ShowFunction sf(1);
    sf.setFunctionID(12);
    sf.setStartTime(5000);
    sf.setDuration(60000);
    sf.setClipAutomation(automation);
    QCOMPARE(sf.clipAutomation(), automation);

    QString xml;
    {
        QXmlStreamWriter writer(&xml);
        writer.writeStartElement("Track");
        QVERIFY(sf.saveXML(&writer, 3));
        // the next sibling is still read after the clip
        writer.writeEmptyElement("Next");
        writer.writeEndElement();
    }
    QCOMPARE(xml.contains("<ClipAutomation>"), written);

    QXmlStreamReader reader(xml);
    reader.readNextStartElement();
    reader.readNextStartElement();
    ShowFunction loaded(2);
    QVERIFY(loaded.loadXML(reader));
    QCOMPARE(loaded.functionID(), quint32(12));
    QCOMPARE(loaded.startTime(), quint32(5000));
    QCOMPARE(loaded.duration(), quint32(60000));
    QCOMPARE(loaded.clipAutomation(), automation);
    QVERIFY(reader.readNextStartElement());
    QCOMPARE(reader.name().toString(), QString("Next"));
}

QTEST_APPLESS_MAIN(ShowClipAutomation_Test)
