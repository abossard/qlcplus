/*
  Q Light Controller Plus - Unit test
  edme2e_test.cpp

  Licensed under the Apache License, Version 2.0

  The song-structure look director end to end: VirtualDJ telemetry as
  VirtualDJ writes it, on the VDJ Bridge plugin's socket, into the real App
  (VdjBridge -> DjFsm -> EdmStructure -> EdmStructureDirector -> Virtual
  Console), a workspace loaded from a .qxw, analysis files on disk, and the
  looks asserted on the Virtual Console buttons and in the universe output.
*/

#include "edme2e_test.h"

#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QSettings>
#include <QTcpSocket>

#include "app.h"
#include "doc.h"
#include "edmstructuremapping.h"
#include "function.h"
#include "functionparent.h"
#include "inputoutputmap.h"
#include "mastertimer.h"
#include "vcbutton.h"
#include "vdjbridge.h"
#include "vdjbridgeplugin.h"
#include "virtualconsole.h"

namespace
{

/** Not VirtualDJ's 8050: a QLC+ running on this machine keeps its port */
const quint16 kPort = 18090;
const QString kSong = QStringLiteral("/music/edme2e/anthem.mp3");
const QString kId = QStringLiteral("e2e0000000000001");
/** 128 BPM, 4 beats per bar, the first downbeat at 0 */
const double kBarMs = 1875.0;
const int kQuietMs = 120;   // three MasterTimer frames: a late press would show

struct Section
{
    const char *label;
    int startBar;
    int endBar;
};

/** 96 bars */
const Section kSections[] = {
    {"intro", 0, 16}, {"buildup", 16, 24}, {"drop", 24, 40}, {"breakdown", 40, 56},
    {"buildup", 56, 64}, {"drop", 64, 80}, {"outro", 80, 96},
};

QString sectionAt(double bar)
{
    for (const Section &s : kSections)
    {
        if (bar >= s.startBar && bar < s.endBar)
            return s.label;
    }
    return QString();
}

/** Each look: its Toggle button and the one channel its Scene sets */
struct Look
{
    QString label;
    quint32 widget;
    int channel;
};

const QList<Look> kLooks = {
    {"intro", 10, 0}, {"buildup", 11, 1}, {"drop", 12, 2}, {"breakdown", 13, 3}, {"outro", 14, 4},
    {"disabled", 17, 6},   // a look button in a disabled frame
};
const quint32 kStopAllWidget = 15;
const quint32 kHouseFunction = 5;   // a Scene no look button runs: a StopAll press would end it
const int kHouseChannel = 5;
const int kChannels = 7;

QString songStructure(bool autoOn, quint32 buildupWidget = 11)
{
    return QString(R"(<SongStructure Auto="%1">
  <Look Label="intro" Widget="10"/>
  <Look Label="buildup" Widget="%2"/>
  <Look Label="drop" Widget="12"/>
  <Look Label="breakdown" Widget="13"/>
  <Look Label="outro" Widget="14"/>
 </SongStructure>)").arg(autoOn ? 1 : 0).arg(buildupWidget);
}

QByteArray workspaceXml(const QString &structure)
{
    QString functions;
    const QStringList names = {"Intro look", "Buildup look", "Drop look", "Breakdown look", "Outro look",
                               "House light", "Disabled look"};
    for (int i = 0; i < names.size(); i++)
    {
        functions += QString(R"(
  <Function ID="%1" Type="Scene" Name="%2">
   <Speed FadeIn="0" FadeOut="0" Duration="0"/>
   <FixtureVal ID="0">%1,255</FixtureVal>
  </Function>)").arg(i).arg(names[i]);
    }

    QString buttons;
    for (int i = 0; i < 5; i++)
    {
        buttons += QString(R"(
   <Button Caption="%1" ID="%2">
    <WindowState Visible="True" X="%3" Y="0" Width="120" Height="60"/>
    <Function ID="%4"/>
    <Action>Toggle</Action>
   </Button>)").arg(names[i]).arg(kLooks[i].widget).arg(i * 120).arg(i);
    }

    return QString(R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE Workspace>
<Workspace xmlns="http://www.qlcplus.org/Workspace" CurrentWindow="VC">
 <Creator>
  <Name>Q Light Controller Plus</Name>
  <Version>5.2.2 GIT</Version>
  <Author>edme2e_test</Author>
 </Creator>
 <Engine>
  <InputOutputMap>
   <Universe Name="Universe 1" ID="0"/>
  </InputOutputMap>
  <Fixture>
   <Manufacturer>Generic</Manufacturer>
   <Model>Generic</Model>
   <Mode>7 Channel</Mode>
   <ID>0</ID>
   <Name>Dimmers</Name>
   <Universe>0</Universe>
   <Address>0</Address>
   <Channels>%1</Channels>
  </Fixture>%2
 </Engine>
 <VirtualConsole>
  <Frame Caption="Looks" ID="3">
   <WindowState Visible="True" X="0" Y="0" Width="1920" Height="1080"/>
   <AllowResize>False</AllowResize>
   <ShowHeader>False</ShowHeader>
   <ShowEnableButton>True</ShowEnableButton>
   <Collapsed>False</Collapsed>
   <Disabled>False</Disabled>%3
   <Button Caption="Stop all" ID="%4">
    <WindowState Visible="True" X="0" Y="80" Width="120" Height="60"/>
    <Action>StopAll</Action>
   </Button>
   <Frame Caption="Disabled" ID="16">
    <WindowState Visible="True" X="0" Y="160" Width="300" Height="100"/>
    <ShowHeader>True</ShowHeader>
    <ShowEnableButton>True</ShowEnableButton>
    <Collapsed>False</Collapsed>
    <Disabled>True</Disabled>
    <Button Caption="Disabled look" ID="17">
     <WindowState Visible="True" X="0" Y="40" Width="120" Height="60"/>
     <Function ID="6"/>
     <Action>Toggle</Action>
    </Button>
   </Frame>
  </Frame>
 </VirtualConsole>
 %5
</Workspace>
)").arg(kChannels).arg(functions, buttons).arg(kStopAllWidget).arg(structure).toUtf8();
}

/** The tool's final file, schema 3: buildups unsure */
QByteArray finalJson()
{
    QJsonArray sections;
    for (const Section &s : kSections)
    {
        QJsonObject o{{"label", s.label}, {"start_bar", s.startBar}, {"end_bar", s.endBar},
                      {"start", s.startBar * kBarMs / 1000.0}, {"end", s.endBar * kBarMs / 1000.0},
                      {"confidence", 80}};
        if (QString(s.label) == "buildup")
            o["unsure"] = true;
        sections.append(o);
    }
    const QJsonArray events = {
        QJsonObject{{"kind", "drop"}, {"bar", 24}, {"time", 24 * kBarMs / 1000.0}, {"confidence", 85}},
        QJsonObject{{"kind", "drop"}, {"bar", 64}, {"time", 64 * kBarMs / 1000.0}, {"confidence", 85}},
    };
    const QJsonObject root{
        {"schema", "qlcplus-edm-final/3"},
        {"track", QJsonObject{{"title", "Anthem"}, {"sha256", kId + QString(48, QChar('0'))}}},
        {"grid", QJsonObject{{"bpm", 128.0}, {"period", 60.0 / 128.0}, {"beats_per_bar", 4}, {"first_downbeat", 0.0}}},
        {"sections", sections},
        {"events", events},
    };
    return QJsonDocument(root).toJson();
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

/** One action of the DJ (or the console) and the looks on after it */
struct Step
{
    enum Op { Load, Master, At, Play, Follow, Hand, Edit };
    Op op = Load;
    int deck = 1;
    double from = 0;
    double to = 0;
    double by = 0;
    QString label;
    bool flag = false;
    QStringList on;
};
using Steps = QList<Step>;

/** VirtualDJ loads the song (and makes the deck the master), then sends its first position */
Step load(int deck, bool master, double bar, const QStringList &on)
{
    Step s;
    s.op = Step::Load;
    s.deck = deck;
    s.flag = master;
    s.from = bar;
    s.on = on;
    return s;
}
/** The crossfader moves to the deck's side: VirtualDJ makes it the master deck */
Step master(int deck, const QStringList &on)
{
    Step s;
    s.op = Step::Master;
    s.deck = deck;
    s.on = on;
    return s;
}
/** One position: a play tick, a cue or a jump */
Step at(int deck, double bar, const QStringList &on)
{
    Step s;
    s.op = Step::At;
    s.deck = deck;
    s.from = bar;
    s.on = on;
    return s;
}
/** A position every `by` bars from `from` to `to`, the same looks after each */
Step play(int deck, double from, double to, double by, const QStringList &on)
{
    Step s;
    s.op = Step::Play;
    s.deck = deck;
    s.from = from;
    s.to = to;
    s.by = by;
    s.on = on;
    return s;
}
/** Same, the look of the section played after each */
Step follow(int deck, double from, double to, double by)
{
    Step s;
    s.op = Step::Follow;
    s.deck = deck;
    s.from = from;
    s.to = to;
    s.by = by;
    return s;
}
/** The DJ clicks a look button on the console */
Step hand(const QString &label, const QStringList &on)
{
    Step s;
    s.op = Step::Hand;
    s.label = label;
    s.on = on;
    return s;
}
/** The Virtual Console's edit mode */
Step edit(bool editMode, const QStringList &on)
{
    Step s;
    s.op = Step::Edit;
    s.flag = editMode;
    s.on = on;
    return s;
}

/** VirtualDJ on the other end of the telemetry socket */
class Vdj
{
public:
    explicit Vdj(QObject *plugin)
        : m_ticks(plugin, SIGNAL(tickEnded()))
    {
    }

    bool connectToQlc()
    {
        m_socket.connectToHost(QHostAddress::LocalHost, kPort);
        return m_socket.waitForConnected(2000);
    }

    void disconnectFromQlc()
    {
        m_socket.disconnectFromHost();
        if (m_socket.state() != QAbstractSocket::UnconnectedState)
            m_socket.waitForDisconnected(2000);
    }

    /** One VirtualDJ write: its changed triggers, one object per line; true once QLC+ ended the tick */
    bool write(const QList<QPair<QString, QJsonValue>> &triggers)
    {
        QByteArrayList objects;
        for (const auto &t : triggers)
        {
            objects << QJsonDocument(QJsonObject{{"evt", "subscribed"}, {"trigger", t.first}, {"value", t.second}})
                           .toJson(QJsonDocument::Compact);
        }
        const int before = m_ticks.count();
        m_socket.write(objects.join('\n'));
        m_socket.flush();
        return QTest::qWaitFor([&]() { return m_ticks.count() > before; }, 2000);
    }

    bool load(int deck, bool master)
    {
        const QString d = QString("deck %1 ").arg(deck);
        QList<QPair<QString, QJsonValue>> triggers = {
            {d + "get_filepath", kSong}, {d + "get_title", "Anthem"}, {d + "get_artist", "E2E"},
            {d + "get_bpm", 128.0}, {d + "get_time total", 96 * kBarMs}, {d + "loaded", "on"}, {d + "play", "on"},
        };
        if (master)
            triggers << qMakePair(d + "masterdeck", QJsonValue("on"));
        return write(triggers);
    }

    bool master(int deck)
    {
        return write({{"crossfader", deck == 1 ? 0.0 : 1.0},
                      {"deck 1 masterdeck", deck == 1 ? "on" : "off"},
                      {"deck 2 masterdeck", deck == 2 ? "on" : "off"}});
    }

    bool at(int deck, double bar)
    {
        return write({{QString("deck %1 get_time elapsed absolute").arg(deck), qRound(bar * kBarMs)}});
    }

private:
    QTcpSocket m_socket;
    QSignalSpy m_ticks;
};

}

Q_DECLARE_METATYPE(Steps)

void EdmE2E_Test::initTestCase()
{
    QVERIFY(m_root.isValid());
    const QString analysis = m_root.filePath("analysis");
    QVERIFY(QDir().mkpath(analysis + "/final"));
    QVERIFY(writeFile(analysis + "/final/" + kId + ".json", finalJson()));
    QVERIFY(writeFile(analysis + "/index.json",
                      QJsonDocument(QJsonObject{{kId, QJsonObject{{"path", kSong}}}}).toJson()));

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_root.filePath("settings"));
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, m_root.filePath("settings"));
    QCoreApplication::setOrganizationName("QLCPlusEdmE2ETests");
    QCoreApplication::setApplicationName("EdmE2E");
    QSettings().setValue("edmanalysis/outputDir", analysis);   // the App's analysis folder

    m_app = std::make_unique<App>();
    m_app->set3dSupported(false);
    m_app->startup();
    m_app->resize(1200, 800);
    QVERIFY2(m_app->status() == QQuickView::Ready, qPrintable(m_app->errors().value(0).toString()));
    QQmlExpression startupNotice(qmlContext(m_app->rootObject()), m_app->rootObject(), "scalingInfoPopup");
    auto *notice = startupNotice.evaluate().value<QObject *>();
    QVERIFY2(notice, qPrintable(startupNotice.error().toString()));
    if (notice->property("visible").toBool())
    {
        QVERIFY(QMetaObject::invokeMethod(notice, "accept"));
        QTRY_VERIFY(!notice->property("visible").toBool());
    }

    // the VDJ Bridge input as App attaches it, on a test port and without Bonjour
    auto *bridge = qobject_cast<VdjBridge *>(m_app->rootContext()->contextProperty("vdjBridge").value<QObject *>());
    QVERIFY(bridge);
    m_plugin = std::make_unique<VdjBridgePlugin>();
    m_plugin->setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("hostPort"), kPort);
    m_plugin->setParameter(0, 0, QLCIOPlugin::Input, QStringLiteral("bonjourEnabled"), false);
    QVERIFY(m_plugin->openInput(0, 0));
    bridge->attachVdjPlugin(m_plugin.get());
}

void EdmE2E_Test::cleanupTestCase()
{
    m_app.reset();
    m_plugin.reset();
}

void EdmE2E_Test::lookFollowsTelemetry_data()
{
    QTest::addColumn<QString>("structure");
    QTest::addColumn<Steps>("steps");

    const QString on = songStructure(true);
    const QStringList none;

    QTest::newRow("play from bar 0 through every section start")
        << on << Steps{load(1, true, 0.5, {"intro"}), follow(1, 1.5, 95.5, 1.0)};

    QTest::newRow("cue backward from the drop into the buildup")
        << on << Steps{load(1, true, 0.5, {"intro"}), at(1, 17.0, {"buildup"}), at(1, 24.5, {"drop"}),
                       at(1, 20.0, {"drop"}), play(1, 20.5, 23.5, 1.0, {"drop"}),
                       at(1, 24.5, {"drop"})};   // across the drop start again: it is on

    QTest::newRow("jump forward inside one section")
        << on << Steps{load(1, true, 0.5, {"intro"}), at(1, 24.5, {"drop"}),
                       hand("intro", {"intro", "drop"}), at(1, 38.0, {"intro", "drop"}),
                       at(1, 40.5, {"breakdown"})};

    QTest::newRow("jump forward across several section starts")
        << on << Steps{load(1, true, 0.5, {"intro"}), at(1, 45.0, {"breakdown"}), at(1, 70.0, {"drop"})};

    QTest::newRow("the DJ's look holds until the next section start")
        << on << Steps{load(1, true, 0.5, {"intro"}), at(1, 24.5, {"drop"}),
                       hand("outro", {"drop", "outro"}), hand("drop", {"outro"}),
                       play(1, 25.5, 39.5, 1.0, {"outro"}), at(1, 40.5, {"breakdown"})};

    // 4 beats from bar 23.5: 23.5, 23.75, 24 (the drop starts), 24.25, back to 23.5
    QTest::newRow("a 4-beat loop over the drop start")
        << on << Steps{load(1, true, 0.5, {"intro"}), at(1, 23.5, {"buildup"}), at(1, 23.75, {"buildup"}),
                       at(1, 24.0, {"drop"}), at(1, 24.25, {"drop"}), at(1, 23.5, {"drop"}),
                       hand("drop", none), at(1, 23.75, none), at(1, 24.0, {"drop"}), at(1, 24.25, {"drop"}),
                       at(1, 23.5, {"drop"})};

    QTest::newRow("load the song mid-drop, then the deck becomes active")
        << on << Steps{load(1, false, 30.0, none), at(1, 31.0, none), master(1, {"drop"})};

    QTest::newRow("the crossfader makes the second deck active")
        << on << Steps{load(1, true, 5.0, {"intro"}), load(2, false, 45.0, {"intro"}), master(2, {"breakdown"}),
                       at(1, 17.0, {"breakdown"}), at(2, 56.5, {"buildup"})};

    QTest::newRow("VC edit mode")
        << on << Steps{edit(true, none), load(1, true, 0.5, none), at(1, 16.5, none), edit(false, none),
                       at(1, 24.5, {"drop"})};

    QTest::newRow("AUTO off")
        << songStructure(false) << Steps{load(1, true, 0.5, none), play(1, 1.5, 45.5, 1.0, none)};

    QTest::newRow("a look mapped to StopAll is never pressed")
        << songStructure(true, kStopAllWidget)
        << Steps{load(1, true, 0.5, {"intro"}), at(1, 16.5, none), at(1, 24.5, {"drop"})};

    QTest::newRow("a look mapped to a disabled button is never pressed")
        << songStructure(true, 17) << Steps{load(1, true, 0.5, {"intro"}), at(1, 16.5, none), at(1, 24.5, {"drop"})};
}

void EdmE2E_Test::lookFollowsTelemetry()
{
    QFETCH(QString, structure);
    QFETCH(Steps, steps);

    const QString workspace = m_root.filePath("looks.qxw");
    QVERIFY(writeFile(workspace, workspaceXml(structure)));
    Doc *doc = m_app->doc();
    // the universe output: the frame each Universe publishes to its output plugins
    QByteArray output;
    QList<QMetaObject::Connection> watches;
    watches << connect(doc->inputOutputMap(), &InputOutputMap::universeWritten, this,
                       [&output](quint32 index, const QByteArray &frame) {
        if (index == 0)
            output = frame;
    });
    const auto unwatch = qScopeGuard([&watches]() {
        for (const QMetaObject::Connection &c : watches)
            disconnect(c);
    });
    QVERIFY(m_app->loadWorkspace(workspace));

    VirtualConsole *vc = m_app->virtualConsole();
    auto *bridge = qobject_cast<VdjBridge *>(m_app->rootContext()->contextProperty("vdjBridge").value<QObject *>());
    auto *mapping = qobject_cast<EdmStructureMapping *>(m_app->rootContext()->contextProperty("edmMapping").value<QObject *>());
    QVERIFY(bridge && mapping);
    QCOMPARE(mapping->widget("drop"), quint32(12));
    QCOMPARE(doc->fixtures().size(), 1);

    QHash<QString, VCButton *> buttons;
    for (const Look &look : kLooks)
    {
        buttons[look.label] = qobject_cast<VCButton *>(vc->widget(look.widget));
        QVERIFY2(buttons[look.label], qPrintable(look.label));
    }
    VCButton *stopAll = qobject_cast<VCButton *>(vc->widget(kStopAllWidget));
    QVERIFY(stopAll);
    QCOMPARE(stopAll->actionType(), VCButton::StopAll);
    QVERIFY(buttons["disabled"]->isDisabled());

    // which buttons changed state, transient presses included
    QSet<QString> changed;
    for (auto it = buttons.constBegin(); it != buttons.constEnd(); ++it)
    {
        const QString label = it.key();
        watches << connect(it.value(), &VCButton::stateChanged, this, [&changed, label]() { changed.insert(label); });
    }
    watches << connect(stopAll, &VCButton::stateChanged, this, [&changed]() { changed.insert("StopAll"); });

    Function *house = doc->function(kHouseFunction);
    QVERIFY(house);
    house->start(doc->masterTimer(), FunctionParent::master());

    Vdj vdj(m_plugin.get());
    const auto cleanup = qScopeGuard([&]() {
        vdj.disconnectFromQlc();
        (void)QTest::qWaitFor([bridge]() { return bridge->telemetryConnected() == false; }, 2000);
        vc->setEditMode(false);
    });
    QVERIFY(vdj.connectToQlc());
    QTRY_VERIFY(bridge->telemetryConnected());

    // what is observable: the looks' buttons, the universe output, StopAll
    auto observe = [&]() {
        QStringList on;
        for (const Look &look : kLooks)
        {
            if (buttons[look.label]->state() != VCButton::Inactive)
                on << look.label;
        }
        QStringList dmx;
        for (int ch = 0; ch < kChannels; ch++)
            dmx << QString::number(ch < output.size() ? quint8(output.at(ch)) : 0);
        return QString("looks on [%1], dmx [%2], StopAll %3").arg(on.join(","), dmx.join(","))
                                                               .arg(stopAll->state());
    };
    auto expected = [&](const QStringList &expectOn) {
        QStringList on;
        QStringList dmx;
        for (int ch = 0; ch < kChannels; ch++)
            dmx << (ch == kHouseChannel ? "255" : "0");
        for (const Look &look : kLooks)
        {
            if (expectOn.contains(look.label))
            {
                on << look.label;
                dmx[look.channel] = "255";
            }
        }
        return QString("looks on [%1], dmx [%2], StopAll %3").arg(on.join(","), dmx.join(","))
                                                               .arg(int(VCButton::Inactive));
    };

    QStringList before;
    QTRY_VERIFY2_WITH_TIMEOUT(observe() == expected(before), qPrintable(observe()), 2000);

    // after each action: the looks expected, still so a few frames later, and only the buttons that had to change changed
    auto settle = [&](const QStringList &expectOn, const QString &where) {
        const QString want = expected(expectOn);
        QTRY_VERIFY2_WITH_TIMEOUT(observe() == want, qPrintable(where + ": observed " + observe() + ", expected " + want), 5000);
        QTest::qWait(kQuietMs);
        QVERIFY2(observe() == want, qPrintable(where + " (later): observed " + observe() + ", expected " + want));

        QSet<QString> mustChange;
        for (const QString &label : before)
            if (expectOn.contains(label) == false)
                mustChange.insert(label);
        for (const QString &label : expectOn)
            if (before.contains(label) == false)
                mustChange.insert(label);
        QStringList got = changed.values(), want2 = mustChange.values();
        got.sort();
        want2.sort();
        QVERIFY2(got == want2, qPrintable(where + ": buttons changed [" + got.join(",") + "], expected [" + want2.join(",") + "]"));
        before = expectOn;
        changed.clear();
    };

    for (int i = 0; i < steps.size(); i++)
    {
        const Step &s = steps[i];
        const QString where = QString("step %1").arg(i + 1);
        changed.clear();
        switch (s.op)
        {
            case Step::Load:
                QVERIFY2(vdj.load(s.deck, s.flag), qPrintable(where));
                QVERIFY2(vdj.at(s.deck, s.from), qPrintable(where));
                settle(s.on, QString("%1, deck %2 loaded at bar %3").arg(where).arg(s.deck).arg(s.from));
            break;
            case Step::Master:
                QVERIFY2(vdj.master(s.deck), qPrintable(where));
                settle(s.on, QString("%1, deck %2 is the master").arg(where).arg(s.deck));
            break;
            case Step::At:
                QVERIFY2(vdj.at(s.deck, s.from), qPrintable(where));
                settle(s.on, QString("%1, deck %2 at bar %3").arg(where).arg(s.deck).arg(s.from));
            break;
            case Step::Play:
            case Step::Follow:
                for (double bar = s.from; bar <= s.to + 1e-9; bar += s.by)
                {
                    changed.clear();
                    QVERIFY2(vdj.at(s.deck, bar), qPrintable(where));
                    const QStringList on = s.op == Step::Follow ? QStringList{sectionAt(bar)} : s.on;
                    settle(on, QString("%1, deck %2 at bar %3").arg(where).arg(s.deck).arg(bar));
                    if (QTest::currentTestFailed())
                        return;
                }
            break;
            case Step::Hand:
                buttons[s.label]->requestUserStateChange(true);
                settle(s.on, QString("%1, the DJ clicks %2").arg(where, s.label));
            break;
            case Step::Edit:
                vc->setEditMode(s.flag);
                settle(s.on, QString("%1, edit mode %2").arg(where).arg(s.flag));
            break;
        }
        if (QTest::currentTestFailed())
            return;
    }
}

QTEST_MAIN(EdmE2E_Test)
