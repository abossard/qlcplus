/*
  Q Light Controller Plus - Unit test
  fixturecontrols_test.cpp

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
#include <QScopedPointer>

#include "fixturecontrols_test.h"
#include "fixturecontrols.h"
#include "qlcfixturemode.h"
#include "qlcfixturehead.h"
#include "qlcfixturedef.h"
#include "qlccapability.h"
#include "qlcchannel.h"

#include "../../../engine/test/common/resource_paths.h"

static const char *kindName(FixtureControls::Kind kind)
{
    switch (kind)
    {
        case FixtureControls::Fader:      return "Fader";
        case FixtureControls::Presets:    return "Presets";
        case FixtureControls::ColorMixer: return "ColorMixer";
        case FixtureControls::PanTilt:    return "PanTilt";
    }
    return "";
}

/** "Fader 0 | ColorMixer@1 2,3,4": kind, @head when in a head, channels */
static QString describe(const QList<FixtureControls::Control> &controls)
{
    QStringList items;
    for (const FixtureControls::Control &control : controls)
    {
        QStringList channels;
        for (quint32 ch : control.channels)
            channels << QString::number(ch);
        items << QString("%1%2 %3").arg(kindName(control.kind),
                                        control.head < 0 ? QString() : QString("@%1").arg(control.head),
                                        channels.join(','));
    }
    return items.join(" | ");
}

/** A one-mode definition from a channel spec and a head spec.
 *  Channels, space separated: a channel preset name ("IntensityRed"),
 *  "Group/N" for a custom channel with N capabilities ("Shutter/3"),
 *  or "Group/fine" for a custom fine channel.
 *  Heads, "|" separated channel ranges: "1-3|4-6" */
static QLCFixtureDef *createDef(const QString &channels, const QString &heads)
{
    QLCFixtureDef *def = new QLCFixtureDef();
    def->setManufacturer("Test");
    def->setModel("Spec");

    QLCFixtureMode *mode = new QLCFixtureMode(def);
    mode->setName("Spec");

    const QStringList tokens = channels.split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i < tokens.size(); ++i)
    {
        QLCChannel *channel = new QLCChannel();
        channel->setName(QString("ch%1").arg(i));

        const QStringList parts = tokens.at(i).split('/');
        if (parts.size() == 1)
        {
            channel->setPreset(QLCChannel::stringToPreset(parts.first()));
            channel->addPresetCapability();
        }
        else
        {
            channel->setGroup(QLCChannel::stringToGroup(parts.first()));
            if (parts.value(1) == "fine")
                channel->setControlByte(QLCChannel::LSB);
            const int caps = parts.value(1) == "fine" ? 1 : parts.value(1, "1").toInt();
            for (int c = 0; c < caps; ++c)
                channel->addCapability(new QLCCapability(c * 256 / caps, (c + 1) * 256 / caps - 1,
                                                         QString("cap%1").arg(c)));
        }
        def->addChannel(channel);
        mode->insertChannel(channel, i);
    }

    for (const QString &range : heads.split('|', Qt::SkipEmptyParts))
    {
        QLCFixtureHead head;
        const QStringList ends = range.split('-');
        for (int ch = ends.first().toInt(); ch <= ends.last().toInt(); ++ch)
            head.addChannel(ch);
        mode->insertHead(-1, head);
    }

    def->addMode(mode);
    return def;
}

void FixtureControls_Test::initTestCase()
{
    const QStringList searchPaths = {
        INTERNAL_FIXTUREDIR,
        "../../../../resources/fixtures/",
        "../../../../../resources/fixtures/"
    };
    for (const QString &path : searchPaths)
    {
        if (m_fixtureDefCache.loadMap(QDir(path)))
            return;
    }
    QFAIL("Fixture library not found");
}

void FixtureControls_Test::analyze_data()
{
    QTest::addColumn<QString>("channels");
    QTest::addColumn<QString>("heads");
    QTest::addColumn<QString>("expected");

    QTest::newRow("rgb par with master dimmer")
        << "IntensityMasterDimmer IntensityRed IntensityGreen IntensityBlue" << ""
        << "Fader 0 | ColorMixer 1,2,3";
    QTest::newRow("rgb par without master dimmer")
        << "IntensityRed IntensityGreen IntensityBlue" << ""
        << "ColorMixer 0,1,2";
    QTest::newRow("single colour is a fader")
        << "IntensityMasterDimmer IntensityUV" << ""
        << "Fader 0 | Fader 1";
    QTest::newRow("hsv is a colour mix")
        << "IntensityHue IntensitySaturation IntensityValue" << ""
        << "ColorMixer 0,1,2";
    QTest::newRow("16 bit pan tilt")
        << "PositionPan PositionPanFine PositionTilt PositionTiltFine" << ""
        << "PanTilt 0,1,2,3";
    QTest::newRow("pan without tilt")
        << "PositionPan" << ""
        << "Fader 0";
    QTest::newRow("fine before coarse")
        << "IntensityDimmerFine IntensityDimmer" << ""
        << "Fader 0,1";
    QTest::newRow("fine pairs by preset")
        << "BeamZoomSmallBig BeamFocusNearFar BeamZoomFine BeamFocusFine" << ""
        << "Fader 0,2 | Fader 1,3";
    QTest::newRow("custom fine pairs by group")
        << "Gobo/8 Gobo/fine" << ""
        << "Presets 0,1";
    QTest::newRow("fine without coarse")
        << "IntensityMasterDimmer PositionPanFine" << ""
        << "Fader 0 | Fader 1";
    QTest::newRow("nothing channels are left out")
        << "IntensityMasterDimmer NoFunction" << ""
        << "Fader 0";
    QTest::newRow("multi capability shutter is presets")
        << "Shutter/3" << ""
        << "Presets 0";
    QTest::newRow("multi capability dimmer stays a fader")
        << "Intensity/2" << ""
        << "Fader 0";
    QTest::newRow("multi capability speed stays a fader")
        << "Speed/3" << ""
        << "Fader 0";
    QTest::newRow("moving head")
        << "PositionPan PositionPanFine PositionTilt PositionTiltFine Colour/8 Gobo/8 Shutter/4 IntensityDimmer" << ""
        << "PanTilt 0,1,2,3 | Presets 4 | Presets 5 | Presets 6 | Fader 7";
    QTest::newRow("led bar heads with a shared master")
        << "IntensityMasterDimmer IntensityRed IntensityGreen IntensityBlue IntensityRed IntensityGreen IntensityBlue"
        << "1-3|4-6"
        << "Fader 0 | ColorMixer@0 1,2,3 | ColorMixer@1 4,5,6";
    QTest::newRow("channel in two heads belongs to the first")
        << "IntensityRed IntensityGreen IntensityBlue IntensityMasterDimmer" << "0-3|3-3"
        << "ColorMixer@0 0,1,2 | Fader@0 3";
}

void FixtureControls_Test::analyze()
{
    QFETCH(QString, channels);
    QFETCH(QString, heads);
    QFETCH(QString, expected);

    QScopedPointer<QLCFixtureDef> def(createDef(channels, heads));
    QCOMPARE(describe(FixtureControls::analyze(def->modes().first())), expected);
}

void FixtureControls_Test::library_data()
{
    QTest::addColumn<QString>("manufacturer");
    QTest::addColumn<QString>("model");
    QTest::addColumn<int>("modeIndex");

    QStringList manufacturers = m_fixtureDefCache.manufacturers();
    manufacturers.sort();
    for (const QString &manufacturer : manufacturers)
    {
        QStringList models = m_fixtureDefCache.models(manufacturer);
        models.sort();
        for (const QString &model : models)
        {
            // Loading replaces the map's names with the definition's own,
            // which some definitions spell differently
            QLCFixtureDef *def = m_fixtureDefCache.fixtureDef(manufacturer, model);
            for (int m = 0; m < def->modes().size(); ++m)
                QTest::addRow("%s/%s/%d", qPrintable(def->manufacturer()), qPrintable(def->model()), m)
                    << def->manufacturer() << def->model() << m;
        }
    }
}

void FixtureControls_Test::library()
{
    QFETCH(QString, manufacturer);
    QFETCH(QString, model);
    QFETCH(int, modeIndex);

    QLCFixtureDef *def = m_fixtureDefCache.fixtureDef(manufacturer, model);
    QVERIFY(def != nullptr);
    const QLCFixtureMode *mode = def->modes().at(modeIndex);
    const QVector<QLCFixtureHead> &heads = mode->heads();
    QVector<int> uses(mode->channels().size(), 0);

    for (const FixtureControls::Control &control : FixtureControls::analyze(mode))
    {
        QVERIFY(!control.channels.isEmpty());
        QVERIFY(std::is_sorted(control.channels.cbegin(), control.channels.cend()));

        QSet<int> groups;
        int coarse = 0, coarsePan = 0, coarseTilt = 0;
        for (quint32 ch : control.channels)
        {
            QVERIFY(ch < quint32(uses.size()));
            ++uses[ch];

            if (control.head >= 0)
                QVERIFY(heads.at(control.head).channels().contains(ch));
            else
                for (const QLCFixtureHead &head : heads)
                    QVERIFY(!head.channels().contains(ch));

            const QLCChannel *channel = mode->channel(ch);
            groups.insert(channel->group());
            if (channel->controlByte() == QLCChannel::MSB)
            {
                ++coarse;
                coarsePan += channel->group() == QLCChannel::Pan;
                coarseTilt += channel->group() == QLCChannel::Tilt;
            }
        }

        switch (control.kind)
        {
            case FixtureControls::ColorMixer:
                QVERIFY(coarse > 1);
                QCOMPARE(groups, QSet<int>({ QLCChannel::Intensity }));
            break;
            case FixtureControls::PanTilt:
                QCOMPARE(coarsePan, 1);
                QCOMPARE(coarseTilt, 1);
                QCOMPARE(groups, QSet<int>({ QLCChannel::Pan, QLCChannel::Tilt }));
            break;
            default:
                QVERIFY(coarse == 1 || control.channels.size() == 1);
                QCOMPARE(groups.size(), 1);
            break;
        }
    }

    for (int ch = 0; ch < uses.size(); ++ch)
    {
        const int expected = mode->channel(ch)->group() == QLCChannel::Nothing ? 0 : 1;
        QVERIFY2(uses.at(ch) == expected,
                 qPrintable(QString("channel %1 is in %2 controls").arg(ch).arg(uses.at(ch))));
    }
}

void FixtureControls_Test::report()
{
    const QString path = qEnvironmentVariable("QLC_FIXTURE_CONTROLS_REPORT");
    if (path.isEmpty())
        QSKIP("Set QLC_FIXTURE_CONTROLS_REPORT to a CSV path to write the report");

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream out(&file);

    auto field = [](QString text)
    {
        text.replace('"', "\"\"");
        return QString("\"%1\"").arg(text);
    };

    out << "manufacturer,model,mode,modeChannels,modeHeads,head,kind,group,preset,name,"
           "channels,capabilities,taggedCapabilities,visualCapabilities\n";

    QStringList manufacturers = m_fixtureDefCache.manufacturers();
    manufacturers.sort();
    for (const QString &manufacturer : manufacturers)
    {
        QStringList models = m_fixtureDefCache.models(manufacturer);
        models.sort();
        for (const QString &model : models)
        {
            QLCFixtureDef *def = m_fixtureDefCache.fixtureDef(manufacturer, model);
            for (const QLCFixtureMode *mode : def->modes())
            {
                for (const FixtureControls::Control &control : FixtureControls::analyze(mode))
                {
                    // The coarse channel describes the control; a lone fine channel describes itself
                    const QLCChannel *primary = mode->channel(control.channels.first());
                    for (quint32 ch : control.channels)
                    {
                        if (mode->channel(ch)->controlByte() == QLCChannel::MSB)
                        {
                            primary = mode->channel(ch);
                            break;
                        }
                    }

                    int tagged = 0, visual = 0;
                    for (const QLCCapability *cap : primary->capabilities())
                    {
                        tagged += cap->preset() != QLCCapability::Custom;
                        visual += cap->presetType() == QLCCapability::SingleColor ||
                                  cap->presetType() == QLCCapability::DoubleColor ||
                                  cap->presetType() == QLCCapability::Picture;
                    }

                    out << field(def->manufacturer()) << ',' << field(def->model()) << ',' << field(mode->name()) << ','
                        << mode->channels().size() << ',' << mode->heads().size() << ','
                        << control.head << ',' << kindName(control.kind) << ','
                        << QLCChannel::groupToString(primary->group()) << ','
                        << QLCChannel::presetToString(primary->preset()) << ','
                        << field(primary->name()) << ',' << control.channels.size() << ','
                        << primary->capabilities().size() << ',' << tagged << ',' << visual << '\n';
                }
            }
        }
    }
}

QTEST_APPLESS_MAIN(FixtureControls_Test)
