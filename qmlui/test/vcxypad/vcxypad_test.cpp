/*
  Q Light Controller Plus - Unit test
  vcxypad_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include <QtTest>

#include "vcxypad_test.h"
#include "vcxypad.h"

#include "doc.h"
#include "fixture.h"
#include "fixturegroup.h"
#include "qlcchannel.h"
#include "qlcfixturedef.h"
#include "qlcfixturehead.h"
#include "qlcfixturemode.h"
#include "qlcphysical.h"

/* VCXYPad exposes two overlapping range APIs:
 *  - the fork's setFixtureRange()/removeHead()/fixtures(), addressing an entry
 *    by (fixtureID, head) and speaking normalized qreal 0.0 - 1.0;
 *  - upstream's headsRangeInfo()/setHeadsRange()/removeHeads(), addressing an
 *    entry by its row index in the QML fixture list and speaking display units.
 * These tests pin both, and their interaction. */

static const QString kDegreeSign = QString(QChar(0x00B0));

namespace
{

/** Doc + VCXYPad + the fixture definitions they borrow, torn down in order */
class Rig
{
public:
    Rig()
        : m_doc(new Doc(nullptr, 4))
        , m_pad(new VCXYPad(m_doc, nullptr))
    {
    }

    ~Rig()
    {
        delete m_pad;
        delete m_doc;
        qDeleteAll(m_defs);
    }

    Doc *doc() const { return m_doc; }
    VCXYPad *pad() const { return m_pad; }

    /** Patch a single-head moving head with the given physical Pan/Tilt spans */
    Fixture *addMover(const QString &name, int panMax, int tiltMax, quint32 address)
    {
        QLCFixtureDef *def = new QLCFixtureDef();
        m_defs.append(def);
        def->setManufacturer("QLC+ Test");
        def->setModel(name);
        def->setType(QLCFixtureDef::MovingHead);

        QLCChannel *pan = new QLCChannel();
        pan->setName("Pan");
        pan->setGroup(QLCChannel::Pan);
        pan->setControlByte(QLCChannel::MSB);
        def->addChannel(pan);

        QLCChannel *tilt = new QLCChannel();
        tilt->setName("Tilt");
        tilt->setGroup(QLCChannel::Tilt);
        tilt->setControlByte(QLCChannel::MSB);
        def->addChannel(tilt);

        QLCFixtureMode *mode = new QLCFixtureMode(def);
        mode->setName("Basic");
        mode->insertChannel(pan, 0);
        mode->insertChannel(tilt, 1);

        QLCPhysical physical;
        physical.setFocusPanMax(panMax);
        physical.setFocusTiltMax(tiltMax);
        mode->setPhysical(physical);

        QLCFixtureHead head;
        head.addChannel(0);
        head.addChannel(1);
        mode->insertHead(-1, head);
        mode->cacheHeads();

        def->addMode(mode);

        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(name);
        fxi->setFixtureDefinition(def, mode);
        fxi->setUniverse(0);
        fxi->setAddress(address);
        m_doc->addFixture(fxi);
        return fxi;
    }

    /** Patch a plain dimmer: it has no Pan/Tilt degrees span at all */
    Fixture *addDimmer(const QString &name, quint32 address)
    {
        Fixture *fxi = new Fixture(m_doc);
        fxi->setName(name);
        fxi->setChannels(4);
        fxi->setUniverse(0);
        fxi->setAddress(address);
        m_doc->addFixture(fxi);
        return fxi;
    }

private:
    Doc *m_doc;
    VCXYPad *m_pad;
    QList<QLCFixtureDef *> m_defs;
};

QVariantList rows(std::initializer_list<int> indexes)
{
    QVariantList list;
    for (int idx : indexes)
        list.append(idx);
    return list;
}

} // namespace

/*********************************************************************
 * fork API -> upstream API
 *********************************************************************/

void VCXYPad_Test::forkRangeReadBackAsDisplayUnits_data()
{
    QTest::addColumn<int>("displayMode");
    QTest::addColumn<qreal>("xMin");
    QTest::addColumn<qreal>("xMax");
    QTest::addColumn<qreal>("yMin");
    QTest::addColumn<qreal>("yMax");
    QTest::addColumn<QString>("units");
    QTest::addColumn<int>("xMaxValue");
    QTest::addColumn<int>("yMaxValue");
    QTest::addColumn<int>("dXMin");
    QTest::addColumn<int>("dXMax");
    QTest::addColumn<int>("dYMin");
    QTest::addColumn<int>("dYMax");

    // 0.333 * 100 = 33.3 -> 33, 0.25 * 100 = 25
    QTest::newRow("percentage")
        << int(VCXYPad::Percentage) << 0.0 << 0.333 << 0.25 << 1.0
        << QString("%") << 100 << 100 << 0 << 33 << 25 << 100;

    // exact .5 quantization boundaries: qRound rounds half away from zero
    QTest::newRow("percentage_half_rounds_up")
        << int(VCXYPad::Percentage) << 0.125 << 0.375 << 0.625 << 0.875
        << QString("%") << 100 << 100 << 13 << 38 << 63 << 88;

    // 0.333 * 255 = 84.915 -> 85, 0.25 * 255 = 63.75 -> 64
    QTest::newRow("dmx")
        << int(VCXYPad::DMX) << 0.0 << 0.333 << 0.25 << 1.0
        << QString() << 255 << 255 << 0 << 85 << 64 << 255;

    // 0.333 * 540 = 179.82 -> 180, 0.25 * 270 = 67.5 -> 68
    QTest::newRow("degrees")
        << int(VCXYPad::Degrees) << 0.0 << 0.333 << 0.25 << 1.0
        << kDegreeSign << 540 << 270 << 0 << 180 << 68 << 270;
}

void VCXYPad_Test::forkRangeReadBackAsDisplayUnits()
{
    QFETCH(int, displayMode);
    QFETCH(qreal, xMin);
    QFETCH(qreal, xMax);
    QFETCH(qreal, yMin);
    QFETCH(qreal, yMax);
    QFETCH(QString, units);
    QFETCH(int, xMaxValue);
    QFETCH(int, yMaxValue);
    QFETCH(int, dXMin);
    QFETCH(int, dXMax);
    QFETCH(int, dYMin);
    QFETCH(int, dYMax);

    Rig rig;
    Fixture *mover = rig.addMover("Mover", 540, 270, 0);
    rig.pad()->addFixture(QVariant::fromValue(mover));
    QCOMPARE(rig.pad()->fixtures().count(), 1);

    QVERIFY(rig.pad()->setFixtureRange(mover->id(), 0, xMin, xMax, false, yMin, yMax, true));

    rig.pad()->setDisplayMode(VCXYPad::DisplayMode(displayMode));
    QVariantMap info = rig.pad()->headsRangeInfo(rows({0}));

    QCOMPARE(info.value("units").toString(), units);
    QCOMPARE(info.value("xMaxValue").toInt(), xMaxValue);
    QCOMPARE(info.value("yMaxValue").toInt(), yMaxValue);
    QCOMPARE(info.value("xMin").toInt(), dXMin);
    QCOMPARE(info.value("xMax").toInt(), dXMax);
    QCOMPARE(info.value("yMin").toInt(), dYMin);
    QCOMPARE(info.value("yMax").toInt(), dYMax);
    QCOMPARE(info.value("xReverse").toBool(), false);
    QCOMPARE(info.value("yReverse").toBool(), true);

    // the fork API leaves the normalized values untouched, whatever the mode
    VCXYPad::XYPadFixture stored = rig.pad()->fixtures().first();
    QCOMPARE(stored.m_xMin, xMin);
    QCOMPARE(stored.m_xMax, xMax);
    QCOMPARE(stored.m_yMin, yMin);
    QCOMPARE(stored.m_yMax, yMax);

    // ... and computeRange() derived the 16 bit offsets from them
    QCOMPARE(stored.m_xOffset, xMin * 65535.0);
    QCOMPARE(stored.m_xRange, (xMax - xMin) * 65535.0);
    QCOMPARE(stored.m_yOffset, yMax * 65535.0);
    QCOMPARE(stored.m_yRange, (yMin - yMax) * 65535.0);
}

/*********************************************************************
 * upstream API -> fork API
 *********************************************************************/

void VCXYPad_Test::displayRangeStoredAsNormalized_data()
{
    QTest::addColumn<int>("displayMode");
    QTest::addColumn<int>("dXMin");
    QTest::addColumn<int>("dXMax");
    QTest::addColumn<int>("dYMin");
    QTest::addColumn<int>("dYMax");
    QTest::addColumn<qreal>("xMin");
    QTest::addColumn<qreal>("xMax");
    QTest::addColumn<qreal>("yMin");
    QTest::addColumn<qreal>("yMax");

    QTest::newRow("percentage")
        << int(VCXYPad::Percentage) << 25 << 75 << 10 << 90
        << 0.25 << 0.75 << 0.10 << 0.90;

    QTest::newRow("dmx")
        << int(VCXYPad::DMX) << 51 << 204 << 0 << 255
        << 0.2 << 0.8 << 0.0 << 1.0;

    QTest::newRow("degrees")
        << int(VCXYPad::Degrees) << 135 << 405 << 0 << 270
        << 0.25 << 0.75 << 0.0 << 1.0;
}

void VCXYPad_Test::displayRangeStoredAsNormalized()
{
    QFETCH(int, displayMode);
    QFETCH(int, dXMin);
    QFETCH(int, dXMax);
    QFETCH(int, dYMin);
    QFETCH(int, dYMax);
    QFETCH(qreal, xMin);
    QFETCH(qreal, xMax);
    QFETCH(qreal, yMin);
    QFETCH(qreal, yMax);

    Rig rig;
    Fixture *mover = rig.addMover("Mover", 540, 270, 0);
    rig.pad()->addFixture(QVariant::fromValue(mover));

    rig.pad()->setDisplayMode(VCXYPad::DisplayMode(displayMode));
    rig.pad()->setHeadsRange(rows({0}), dXMin, dXMax, true, dYMin, dYMax, false);

    VCXYPad::XYPadFixture stored = rig.pad()->fixtures().first();
    QCOMPARE(stored.m_xMin, xMin);
    QCOMPARE(stored.m_xMax, xMax);
    QCOMPARE(stored.m_yMin, yMin);
    QCOMPARE(stored.m_yMax, yMax);
    QCOMPARE(stored.m_xReverse, true);
    QCOMPARE(stored.m_yReverse, false);

    // reversed X swaps offset and range sign
    QCOMPARE(stored.m_xOffset, xMax * 65535.0);
    QCOMPARE(stored.m_xRange, (xMin - xMax) * 65535.0);
    QCOMPARE(stored.m_yOffset, yMin * 65535.0);
    QCOMPARE(stored.m_yRange, (yMax - yMin) * 65535.0);
}

/*********************************************************************
 * entry addressing
 *********************************************************************/

void VCXYPad_Test::groupEntryMatchedByGroupId()
{
    Rig rig;
    Fixture *wide = rig.addMover("Wide", 540, 270, 0);
    Fixture *narrow = rig.addMover("Narrow", 360, 180, 2);

    FixtureGroup *group = new FixtureGroup(rig.doc());
    group->setName("Movers");
    rig.doc()->addFixtureGroup(group);
    group->assignFixture(wide->id());
    group->assignFixture(narrow->id());
    QCOMPARE(group->headList().count(), 2);

    rig.pad()->addGroup(QVariant::fromValue(group));
    QCOMPARE(rig.pad()->fixtures().count(), 1);

    // a group entry keeps the group id and leaves m_head unused
    VCXYPad::XYPadFixture entry = rig.pad()->fixtures().first();
    QCOMPARE(entry.m_groupID, group->id());
    QCOMPARE(entry.m_head.fxi, Fixture::invalidId());

    // the group's span is the smallest among its members
    rig.pad()->setDisplayMode(VCXYPad::Degrees);
    QVariantMap info = rig.pad()->headsRangeInfo(rows({0}));
    QCOMPARE(info.value("xMaxValue").toInt(), 360);
    QCOMPARE(info.value("yMaxValue").toInt(), 180);

    rig.pad()->setHeadsRange(rows({0}), 90, 270, false, 45, 135, false);
    entry = rig.pad()->fixtures().first();
    QCOMPARE(entry.m_xMin, 0.25);
    QCOMPARE(entry.m_xMax, 0.75);
    QCOMPARE(entry.m_yMin, 0.25);
    QCOMPARE(entry.m_yMax, 0.75);
}

void VCXYPad_Test::plainHeadMatchedByFixtureAndHead()
{
    Rig rig;
    Fixture *first = rig.addMover("First", 540, 270, 0);
    Fixture *second = rig.addMover("Second", 540, 270, 2);
    rig.pad()->addFixture(QVariant::fromValue(first));
    rig.pad()->addFixture(QVariant::fromValue(second));
    QCOMPARE(rig.pad()->fixtures().count(), 2);

    rig.pad()->setDisplayMode(VCXYPad::Percentage);

    // upstream addresses by row, so row 1 must reach the second fixture only
    rig.pad()->setHeadsRange(rows({1}), 10, 20, false, 30, 40, false);
    QCOMPARE(rig.pad()->fixtures().at(0).m_xMin, 0.0);
    QCOMPARE(rig.pad()->fixtures().at(0).m_xMax, 1.0);
    QCOMPARE(rig.pad()->fixtures().at(1).m_xMin, 0.10);
    QCOMPARE(rig.pad()->fixtures().at(1).m_xMax, 0.20);

    // the fork addresses by (fixtureID, head) and must reach the first only
    QVERIFY(rig.pad()->setFixtureRange(first->id(), 0, 0.5, 0.6, false, 0.7, 0.8, false));
    QCOMPARE(rig.pad()->fixtures().at(0).m_xMin, 0.5);
    QCOMPARE(rig.pad()->fixtures().at(1).m_xMin, 0.10);

    // an unpatched head matches nothing
    QVERIFY(rig.pad()->setFixtureRange(first->id(), 3, 0.0, 1.0, false, 0.0, 1.0, false) == false);
    QVERIFY(rig.pad()->setFixtureRange(second->id() + 100, 0, 0.0, 1.0, false, 0.0, 1.0, false) == false);
}

void VCXYPad_Test::forkApiIgnoresGroupIdGuard()
{
    Rig rig;
    Fixture *mover = rig.addMover("Mover", 540, 270, 0);

    FixtureGroup *group = new FixtureGroup(rig.doc());
    group->setName("Movers");
    rig.doc()->addFixtureGroup(group);
    group->assignFixture(mover->id());

    rig.pad()->addGroup(QVariant::fromValue(group));
    QCOMPARE(rig.pad()->fixtures().count(), 1);

    /* A group entry carries m_head = (Fixture::invalidId(), 0). Upstream's
     * matchers guard on m_groupID first, the fork's do not, so the fork API
     * reaches a group entry through that placeholder head. Pinned so that a
     * refactor sharing one predicate cannot change it unnoticed. */
    QVERIFY(rig.pad()->setFixtureRange(Fixture::invalidId(), 0, 0.2, 0.8, false, 0.3, 0.7, false));
    QCOMPARE(rig.pad()->fixtures().first().m_xMin, 0.2);
    QCOMPARE(rig.pad()->fixtures().first().m_yMax, 0.7);

    QVERIFY(rig.pad()->removeHead(Fixture::invalidId(), 0));
    QCOMPARE(rig.pad()->fixtures().count(), 0);
}

/*********************************************************************
 * degrees scaling
 *********************************************************************/

void VCXYPad_Test::degreesScaleIsSmallestSpanOfSelection()
{
    Rig rig;
    Fixture *wide = rig.addMover("Wide", 540, 270, 0);
    Fixture *narrow = rig.addMover("Narrow", 360, 180, 2);
    rig.pad()->addFixture(QVariant::fromValue(wide));
    rig.pad()->addFixture(QVariant::fromValue(narrow));

    QVERIFY(rig.pad()->setFixtureRange(wide->id(), 0, 0.0, 1.0, false, 0.0, 1.0, false));
    QVERIFY(rig.pad()->setFixtureRange(narrow->id(), 0, 0.0, 0.5, false, 0.0, 0.5, false));

    rig.pad()->setDisplayMode(VCXYPad::Degrees);

    // both rows selected, narrow first: the scale is the smallest span, while
    // the reported range is the first selected entry's only
    QVariantMap info = rig.pad()->headsRangeInfo(rows({1, 0}));
    QCOMPARE(info.value("xMaxValue").toInt(), 360);
    QCOMPARE(info.value("yMaxValue").toInt(), 180);
    QCOMPARE(info.value("xMax").toInt(), 180);
    QCOMPARE(info.value("yMax").toInt(), 90);

    // reversing the selection order changes the reported range, not the scale
    info = rig.pad()->headsRangeInfo(rows({0, 1}));
    QCOMPARE(info.value("xMaxValue").toInt(), 360);
    QCOMPARE(info.value("xMax").toInt(), 360);
    QCOMPARE(info.value("yMax").toInt(), 180);

    // a single row scales on its own span
    info = rig.pad()->headsRangeInfo(rows({0}));
    QCOMPARE(info.value("xMaxValue").toInt(), 540);
    QCOMPARE(info.value("yMaxValue").toInt(), 270);
    QCOMPARE(info.value("xMax").toInt(), 540);
}

void VCXYPad_Test::degreesWriteScaleIsResolvedPerEntry()
{
    Rig rig;
    Fixture *wide = rig.addMover("Wide", 540, 270, 0);
    Fixture *narrow = rig.addMover("Narrow", 360, 180, 2);
    rig.pad()->addFixture(QVariant::fromValue(wide));
    rig.pad()->addFixture(QVariant::fromValue(narrow));

    rig.pad()->setDisplayMode(VCXYPad::Degrees);

    /* headsRangeInfo() resolves the degrees scale once, over the whole
     * selection; setHeadsRange() resolves it again inside its per-entry loop,
     * over that entry's heads only. On a mixed selection the two disagree:
     * 90 deg, entered against the 360 deg maximum the editor was told about,
     * lands on 90/540 for the wide fixture and 90/360 for the narrow one. */
    rig.pad()->setHeadsRange(rows({0, 1}), 90, 270, false, 45, 135, false);
    QCOMPARE(rig.pad()->fixtures().at(0).m_xMin, 90.0 / 540.0);
    QCOMPARE(rig.pad()->fixtures().at(0).m_yMin, 45.0 / 270.0);
    QCOMPARE(rig.pad()->fixtures().at(1).m_xMin, 90.0 / 360.0);
    QCOMPARE(rig.pad()->fixtures().at(1).m_yMin, 45.0 / 180.0);
}

void VCXYPad_Test::degreesWithoutSpanLeavesRangeUntouched()
{
    Rig rig;
    Fixture *dimmer = rig.addDimmer("Dimmer", 0);
    rig.pad()->addHead(int(dimmer->id()), 0);
    QCOMPARE(rig.pad()->fixtures().count(), 1);

    QVERIFY(rig.pad()->setFixtureRange(dimmer->id(), 0, 0.1, 0.9, false, 0.2, 0.8, false));

    rig.pad()->setDisplayMode(VCXYPad::Degrees);

    // no physical Pan/Tilt span: the scale resolves to 0 and is reported as 0
    QVariantMap info = rig.pad()->headsRangeInfo(rows({0}));
    QVERIFY(info.isEmpty() == false);
    QCOMPARE(info.value("xMaxValue").toInt(), 0);
    QCOMPARE(info.value("yMaxValue").toInt(), 0);

    // ... and setHeadsRange silently does nothing rather than dividing by zero
    rig.pad()->setHeadsRange(rows({0}), 90, 270, true, 45, 135, true);
    VCXYPad::XYPadFixture stored = rig.pad()->fixtures().first();
    QCOMPARE(stored.m_xMin, 0.1);
    QCOMPARE(stored.m_xMax, 0.9);
    QCOMPARE(stored.m_yMin, 0.2);
    QCOMPARE(stored.m_yMax, 0.8);
    QCOMPARE(stored.m_xReverse, false);
    QCOMPARE(stored.m_yReverse, false);
}

/*********************************************************************
 * removal
 *********************************************************************/

void VCXYPad_Test::removeHead_data()
{
    QTest::addColumn<int>("headIndex");
    QTest::addColumn<bool>("removed");

    QTest::newRow("patched_head") << 0 << true;
    QTest::newRow("absent_head") << 1 << false;
    QTest::newRow("negative_head") << -1 << false;
}

void VCXYPad_Test::removeHead()
{
    QFETCH(int, headIndex);
    QFETCH(bool, removed);

    Rig rig;
    Fixture *mover = rig.addMover("Mover", 540, 270, 0);
    rig.pad()->addFixture(QVariant::fromValue(mover));
    QCOMPARE(rig.pad()->fixtures().count(), 1);

    QCOMPARE(rig.pad()->removeHead(mover->id(), headIndex), removed);
    QCOMPARE(rig.pad()->fixtures().count(), removed ? 0 : 1);
}

void VCXYPad_Test::removeHeadsRemovesTheSelectedRow()
{
    Rig rig;
    Fixture *first = rig.addMover("First", 540, 270, 0);
    Fixture *second = rig.addMover("Second", 540, 270, 2);

    FixtureGroup *group = new FixtureGroup(rig.doc());
    group->setName("Movers");
    rig.doc()->addFixtureGroup(group);
    group->assignFixture(first->id());

    rig.pad()->addGroup(QVariant::fromValue(group));
    rig.pad()->addFixture(QVariant::fromValue(second));
    QCOMPARE(rig.pad()->fixtures().count(), 2);

    // row 0 is the group entry: it is matched by group id, not by head
    rig.pad()->removeHeads(rows({0}));
    QCOMPARE(rig.pad()->fixtures().count(), 1);
    QCOMPARE(rig.pad()->fixtures().first().m_head.fxi, second->id());

    rig.pad()->removeHeads(rows({0}));
    QCOMPARE(rig.pad()->fixtures().count(), 0);
}

/*********************************************************************
 * selection edge cases
 *********************************************************************/

void VCXYPad_Test::headsRangeInfoOfEmptySelection()
{
    Rig rig;
    // the first fixture is never patched into the pad, so no entry can be
    // reached through the (0, 0) an out of range row falls back to
    rig.addMover("Unpatched", 540, 270, 0);
    Fixture *mover = rig.addMover("Mover", 540, 270, 2);
    rig.pad()->addFixture(QVariant::fromValue(mover));

    rig.pad()->setDisplayMode(VCXYPad::Percentage);

    // QML relies on info.units === undefined to bail out
    QVERIFY(rig.pad()->headsRangeInfo(QVariantList()).isEmpty());
    QVERIFY(rig.pad()->headsRangeInfo(rows({7})).isEmpty());
    QVERIFY(rig.pad()->headsRangeInfo(rows({-1})).isEmpty());

    // a matching row still yields a full map
    QVERIFY(rig.pad()->headsRangeInfo(rows({0})).contains("units"));
}

QTEST_MAIN(VCXYPad_Test)
