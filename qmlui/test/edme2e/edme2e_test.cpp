/*
  Q Light Controller Plus - Unit test
  edme2e_test.cpp

  Licensed under the Apache License, Version 2.0

  Song-structure looks end to end: VirtualDJ telemetry as VirtualDJ writes
  it, on the VDJ Bridge plugin's socket, into the real App (VdjBridge ->
  DjFsm -> Perform plays the deck's song Show -> the Song Director block
  under the playhead -> its Collection's look), a workspace loaded from a
  .qxw, analysis files on disk, the blocks created by the App itself, and
  the looks asserted in the universe output. A recorded set is replayed
  from its Show.
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
#include "collection.h"
#include "djsetrecorder.h"
#include "doc.h"
#include "function.h"
#include "functionparent.h"
#include "inputoutputmap.h"
#include "mastertimer.h"
#include "show.h"
#include "showrunner.h"
#include "songdirector.h"
#include "vcbutton.h"
#include "vdjbridge.h"
#include "vdjbridgeplugin.h"
#include "virtualconsole.h"

namespace
{

/** Not VirtualDJ's 8050: a QLC+ running on this machine keeps its port */
const quint16 kPort = 18090;
/** Song A (deck 1) and song B (deck 2), their Shows and analyses */
const QStringList kSongs = {QStringLiteral("/music/edme2e/anthem.mp3"), QStringLiteral("/music/edme2e/banger.mp3")};
const QStringList kIds = {QStringLiteral("e2e0000000000001"), QStringLiteral("e2e0000000000002")};
const quint32 kShowIds[] = {21, 23};
/** 128 BPM, 4 beats per bar, the first downbeat at 0 */
const double kBarMs = 1875.0;
const int kQuietMs = 120;   // three MasterTimer frames: a late start or stop would show
const qint64 kJumpMs = 100; // the recorder clock between two writes that are no play

struct Section
{
    const char *label;
    int startBar;
    int endBar;
};

/** 96 bars, both songs */
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

/** Each look: a Scene, its own HTP channel at full and its value on the shared LTP channel */
struct Look
{
    QString label;
    quint32 function;
    int channel;
    int ltp;
};

const QList<Look> kLooks = {
    {"intro", 0, 0, 10}, {"buildup", 1, 1, 20}, {"drop", 2, 2, 30}, {"breakdown", 3, 3, 40}, {"outro", 4, 4, 50},
};
const quint32 kStrobeFunction = 5;   // the DJ's own look, on a VC button
const int kStrobeChannel = 5;
const quint32 kStrobeWidget = 10;
const quint32 kLegacyWidget = 11;    // an old look button of the drop
const int kLtpChannel = 7;
const int kChannels = 8;

/** Today's defaults: every label its Scene */
QString defaults()
{
    QString looks;
    for (const Look &look : kLooks)
        looks += QString(R"(
  <Look Label="%1" Function="%2"/>)").arg(look.label).arg(look.function);
    return QString(R"(<SongStructure Auto="0">%1
 </SongStructure>)").arg(looks);
}

/** A workspace of the button director: the drop mapped to its VC button, AUTO on */
QString legacyButtons()
{
    return QString(R"(<SongStructure Auto="1">
  <Look Label="drop" Widget="%1"/>
 </SongStructure>)").arg(kLegacyWidget);
}

QByteArray workspaceXml(const QString &structure)
{
    QString functions;
    for (const Look &look : kLooks)
    {
        functions += QString(R"(
  <Function ID="%1" Type="Scene" Name="%2 look">
   <Speed FadeIn="0" FadeOut="0" Duration="0"/>
   <FixtureVal ID="0">%3,255,%4,%5</FixtureVal>
  </Function>)").arg(look.function).arg(look.label).arg(look.channel).arg(kLtpChannel).arg(look.ltp);
    }
    functions += QString(R"(
  <Function ID="%1" Type="Scene" Name="Strobe">
   <Speed FadeIn="0" FadeOut="0" Duration="0"/>
   <FixtureVal ID="0">%2,255</FixtureVal>
  </Function>)").arg(kStrobeFunction).arg(kStrobeChannel);

    const quint32 songMs = quint32(96 * kBarMs);
    for (int i = 0; i < kSongs.size(); i++)
    {
        functions += QString(R"(
  <Function ID="%1" Type="Audio" Name="%2" Path="Songs">
   <Speed FadeIn="0" FadeOut="0" Duration="%3"/>
   <RunOrder>SingleShot</RunOrder>
   <Source>%4</Source>
  </Function>
  <Function ID="%5" Type="Show" Name="%2" Path="Songs">
   <TimeDivision Type="VDJBeat" BPM="128"/>
   <Track ID="0" Name="Audio" isMute="1">
    <ShowFunction ID="%1" StartTime="0" Duration="%3" Color="#608053"/>
   </Track>
  </Function>)").arg(kShowIds[i] - 1).arg(QFileInfo(kSongs[i]).completeBaseName()).arg(songMs)
                 .arg(kSongs[i]).arg(kShowIds[i]);
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
   <Mode>%1 Channel</Mode>
   <ID>0</ID>
   <Name>Dimmers</Name>
   <Universe>0</Universe>
   <Address>0</Address>
   <Channels>%1</Channels>
   <ForcedLTP>%2</ForcedLTP>
  </Fixture>%3
 </Engine>
 <VirtualConsole>
  <Frame Caption="Looks" ID="3">
   <WindowState Visible="True" X="0" Y="0" Width="1920" Height="1080"/>
   <AllowResize>False</AllowResize>
   <ShowHeader>False</ShowHeader>
   <ShowEnableButton>True</ShowEnableButton>
   <Collapsed>False</Collapsed>
   <Disabled>False</Disabled>
   <Button Caption="Strobe" ID="%4">
    <WindowState Visible="True" X="0" Y="0" Width="120" Height="60"/>
    <Function ID="%5"/>
    <Action>Toggle</Action>
   </Button>
   <Button Caption="Drop look" ID="%6">
    <WindowState Visible="True" X="120" Y="0" Width="120" Height="60"/>
    <Function ID="2"/>
    <Action>Toggle</Action>
   </Button>
  </Frame>
 </VirtualConsole>
 %7
</Workspace>
)").arg(kChannels).arg(kLtpChannel).arg(functions).arg(kStrobeWidget).arg(kStrobeFunction)
   .arg(kLegacyWidget).arg(structure).toUtf8();
}

/** The tool's final file, schema 3: buildups unsure */
QByteArray finalJson(int song)
{
    QJsonArray sections;
    for (const Section &s : kSections)
    {
        QJsonObject o{{"label", s.label}, {"start_bar", s.startBar}, {"end_bar", s.endBar},
                      {"start", s.startBar * kBarMs / 1000.0}, {"end", s.endBar * kBarMs / 1000.0},
                      {"confidence", 80}, {"label_confidence", 80}};
        if (QString(s.label) == "buildup")
            o["unsure"] = true;
        sections.append(o);
    }
    const QJsonObject root{
        {"schema", "qlcplus-edm-final/3"},
        {"track", QJsonObject{{"title", QFileInfo(kSongs[song]).completeBaseName()},
                              {"sha256", kIds[song] + QString(48, QChar('0'))}}},
        {"grid", QJsonObject{{"bpm", 128.0}, {"period", 60.0 / 128.0}, {"beats_per_bar", 4}, {"first_downbeat", 0.0}}},
        {"sections", sections},
        {"events", QJsonArray()},
    };
    return QJsonDocument(root).toJson();
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

/** What is lit: the looks' HTP channels, the look that holds the LTP channel, the DJ's strobe */
struct Lit
{
    QStringList looks;
    QString ltp;
    bool strobe = false;
};

Lit lit(const QStringList &looks, const QString &ltp = QString(), bool strobe = false)
{
    return Lit{looks, ltp.isEmpty() && looks.size() == 1 ? looks.first() : ltp, strobe};
}

/** One action of the DJ (or the console) and what is lit after it */
struct Step
{
    enum Op { Load, Master, At, Play, Follow, Strobe, Replay };
    Op op = Load;
    int deck = 1;
    double from = 0;
    double to = 0;
    double by = 0;
    bool flag = false;
    Lit after;
};
using Steps = QList<Step>;

/** VirtualDJ loads song A on deck 1, song B on deck 2 (master: the deck becomes active), then sends its first position */
Step load(int deck, bool master, double bar, const Lit &after)
{
    Step s;
    s.op = Step::Load;
    s.deck = deck;
    s.flag = master;
    s.from = bar;
    s.after = after;
    return s;
}
/** VirtualDJ makes the deck the master deck */
Step master(int deck, const Lit &after)
{
    Step s;
    s.op = Step::Master;
    s.deck = deck;
    s.after = after;
    return s;
}
/** One position: a cue or a jump */
Step at(int deck, double bar, const Lit &after)
{
    Step s;
    s.op = Step::At;
    s.deck = deck;
    s.from = bar;
    s.after = after;
    return s;
}
/** Playing: a position every `by` bars from `from` to `to`, the same lit after each */
Step play(int deck, double from, double to, double by, const Lit &after)
{
    Step s;
    s.op = Step::Play;
    s.deck = deck;
    s.from = from;
    s.to = to;
    s.by = by;
    s.after = after;
    return s;
}
/** Same, the look of the section played after each */
Step follow(int deck, double from, double to, double by, bool strobe = false)
{
    Step s;
    s.op = Step::Follow;
    s.deck = deck;
    s.from = from;
    s.to = to;
    s.by = by;
    s.flag = strobe;
    return s;
}
/** The DJ toggles the strobe button on the console */
Step strobe(const Lit &after)
{
    Step s;
    s.op = Step::Strobe;
    s.after = after;
    return s;
}
/** Perform off, then the recorded set Show replays: at each position written before, the same lit */
Step replay()
{
    Step s;
    s.op = Step::Replay;
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
        const QString song = kSongs[deck - 1];
        QList<QPair<QString, QJsonValue>> triggers = {
            {d + "get_filepath", song}, {d + "get_title", QFileInfo(song).completeBaseName()}, {d + "get_artist", "E2E"},
            {d + "get_bpm", 128.0}, {d + "get_time total", 96 * kBarMs}, {d + "volume", 1.0},
            {d + "loaded", "on"}, {d + "play", "on"},
        };
        if (master)
            triggers << qMakePair(d + "masterdeck", QJsonValue("on"));
        return write(triggers);
    }

    bool master(int deck)
    {
        return write({{"deck 1 masterdeck", deck == 1 ? "on" : "off"},
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
    QJsonObject index;
    for (int i = 0; i < kSongs.size(); i++)
    {
        QVERIFY(writeFile(analysis + "/final/" + kIds[i] + ".json", finalJson(i)));
        index[kIds[i]] = QJsonObject{{"path", kSongs[i]}};
    }
    QVERIFY(writeFile(analysis + "/index.json", QJsonDocument(index).toJson()));

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

void EdmE2E_Test::looksFollowSongBlocks_data()
{
    QTest::addColumn<QString>("structure");
    QTest::addColumn<Steps>("steps");

    const QString on = defaults();
    const Lit dark;

    QTest::newRow("play from bar 0 through every section start")
        << on << Steps{load(1, true, 0.5, lit({"intro"})), follow(1, 1.5, 95.5, 1.0)};

    // normal Show behaviour: the block under the playhead runs, backward too
    QTest::newRow("a backward jump starts the earlier section's block")
        << on << Steps{load(1, true, 0.5, lit({"intro"})), at(1, 24.5, lit({"drop"})), at(1, 17.0, lit({"buildup"})),
                       play(1, 17.5, 23.5, 1.0, lit({"buildup"})), at(1, 24.5, lit({"drop"})),
                       at(1, 4.0, lit({"intro"}))};

    QTest::newRow("a jump forward across several section starts")
        << on << Steps{load(1, true, 0.5, lit({"intro"})), at(1, 45.0, lit({"breakdown"})), at(1, 70.0, lit({"drop"})),
                       at(1, 85.0, lit({"outro"}))};

    // 4 beats from bar 23.5: 23.5, 23.75, 24 (the drop starts), 24.25, back to 23.5
    QTest::newRow("a 4-beat loop over the drop start")
        << on << Steps{load(1, true, 0.5, lit({"intro"})), at(1, 23.5, lit({"buildup"})), at(1, 23.75, lit({"buildup"})),
                       at(1, 24.0, lit({"drop"})), at(1, 24.25, lit({"drop"})), at(1, 23.5, lit({"buildup"})),
                       at(1, 23.75, lit({"buildup"})), at(1, 24.0, lit({"drop"}))};

    // the DJ's input keeps running on top, blocks never stop it
    QTest::newRow("the DJ's strobe keeps running across section starts")
        << on << Steps{load(1, true, 14.5, lit({"intro"})), strobe(lit({"intro"}, {}, true)),
                       follow(1, 15.5, 25.5, 1.0, true), strobe(lit({"drop"}))};

    QTest::newRow("a song loaded mid-drop on a deck that is not active")
        << on << Steps{load(1, false, 30.0, lit({"drop"})), at(1, 41.0, lit({"breakdown"})),
                       master(1, lit({"breakdown"}))};

    // both songs' blocks run, LTP with the active deck
    QTest::newRow("two decks overlap")
        << on << Steps{load(1, true, 24.5, lit({"drop"})), load(2, false, 45.0, lit({"drop", "breakdown"}, "drop")),
                       master(2, lit({"drop", "breakdown"}, "breakdown")),
                       at(2, 56.5, lit({"drop", "buildup"}, "buildup")),
                       master(1, lit({"drop", "buildup"}, "drop")),
                       at(1, 41.0, lit({"breakdown", "buildup"}, "breakdown"))};

    // the old button director is gone: its mapping is the drop's default, the button stays off
    QTest::newRow("a workspace of the button director")
        << legacyButtons()
        << Steps{load(1, true, 0.5, dark), at(1, 16.5, dark), at(1, 24.5, lit({"drop"})), at(1, 40.5, dark),
                 at(1, 64.5, lit({"drop"}))};

    // the set nests the song Show: its blocks play through the clip's position map
    QTest::newRow("a recorded set replays the blocks")
        << on << Steps{load(1, true, 14.5, lit({"intro"})), follow(1, 15.0, 26.0, 0.5),
                       at(1, 17.5, lit({"buildup"})), follow(1, 18.0, 20.0, 0.5), at(1, 42.5, lit({"breakdown"})),
                       replay()};
}

void EdmE2E_Test::looksFollowSongBlocks()
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
    auto *director = m_app->findChild<SongDirector *>();
    auto *recorder = m_app->findChild<DjSetRecorder *>();
    QVERIFY(bridge && director && recorder);
    QCOMPARE(doc->fixtures().size(), 1);
    VCButton *strobeButton = qobject_cast<VCButton *>(vc->widget(kStrobeWidget));
    VCButton *legacyButton = qobject_cast<VCButton *>(vc->widget(kLegacyWidget));
    QVERIFY(strobeButton && legacyButton);

    // the App made the blocks: one per section on each song Show, pre-filled from the defaults
    QList<Show *> songs;
    for (quint32 id : kShowIds)
    {
        Show *song = qobject_cast<Show *>(doc->function(id));
        QVERIFY(song);
        QTRY_COMPARE(director->blocks(song).size(), int(std::size(kSections)));
        for (const SongDirectorBlock &block : director->blocks(song))
        {
            auto *collection = qobject_cast<Collection *>(doc->function(block.functionId));
            QVERIFY(collection);
            const QString label = block.generated.label;
            const bool filled = structure == defaults() || label == "drop";
            const auto look = std::find_if(kLooks.begin(), kLooks.end(), [&label](const Look &l) { return l.label == label; });
            QCOMPARE(collection->functions(), filled ? QList<quint32>{look->function} : QList<quint32>());
        }
        songs << song;
    }

    // the recorder's clock: the time VirtualDJ plays, not the test's
    qint64 clock = 1000;
    recorder->setClock([&clock]() { return clock; });
    const auto realClock = qScopeGuard([recorder]() { recorder->setClock(DjSetRecorder::Clock()); });

    Vdj vdj(m_plugin.get());
    const auto cleanup = qScopeGuard([&]() {
        bridge->setPerformMode(false);
        vdj.disconnectFromQlc();
        (void)QTest::qWaitFor([bridge]() { return bridge->telemetryConnected() == false; }, 2000);
        m_app->stopAllFunctions();
    });
    bridge->setPerformMode(true);
    QVERIFY(vdj.connectToQlc());
    QTRY_VERIFY(bridge->telemetryConnected());

    // what is observable: the universe output and the old look button; the running blocks name a failure
    auto observe = [&]() {
        QStringList dmx;
        for (int ch = 0; ch < kChannels; ch++)
            dmx << QString::number(ch < output.size() ? quint8(output.at(ch)) : 0);
        // a button whose Function a block runs shows Monitoring: only a press is Active
        return QString("dmx [%1], look button %2").arg(dmx.join(","), legacyButton->state() == VCButton::Active ? "pressed" : "not pressed");
    };
    auto blocksRunning = [&]() {
        QStringList running;
        for (Show *song : std::as_const(songs))
        {
            for (const SongDirectorBlock &block : director->blocks(song))
            {
                Function *f = doc->function(block.functionId);
                if (f && f->isRunning())
                    running << QString("%1 %2 %3").arg(song->name(), block.generated.label).arg(block.generated.number);
            }
        }
        return running.join(", ");
    };
    auto expected = [&](const Lit &want) {
        QStringList dmx;
        for (int ch = 0; ch < kChannels; ch++)
            dmx << "0";
        for (const Look &look : kLooks)
        {
            if (want.looks.contains(look.label))
                dmx[look.channel] = "255";
            if (want.ltp == look.label)
                dmx[kLtpChannel] = QString::number(look.ltp);
        }
        if (want.strobe)
            dmx[kStrobeChannel] = "255";
        return QString("dmx [%1], look button not pressed").arg(dmx.join(","));
    };

    QTRY_VERIFY2_WITH_TIMEOUT(observe() == expected(Lit()), qPrintable(observe()), 2000);

    // after each action: what is lit, and still so a few frames later; an LTP channel nothing writes keeps its value
    QString heldLtp;
    auto settle = [&](const Lit &asked, const QString &where) {
        Lit want = asked;
        if (want.ltp.isEmpty())
            want.ltp = heldLtp;
        heldLtp = want.ltp;
        const QString target = expected(want);
        QTRY_VERIFY2_WITH_TIMEOUT(observe() == target,
                                  qPrintable(where + ": observed " + observe() + ", expected " + target
                                             + ", blocks running: " + blocksRunning()), 5000);
        QTest::qWait(kQuietMs);
        QVERIFY2(observe() == target, qPrintable(where + " (later): observed " + observe() + ", expected " + target
                                                 + ", blocks running: " + blocksRunning()));
    };

    // the recorder's clock at each position written while deck 1 played alone, and what was lit
    QList<QPair<qint64, Lit>> written;
    auto position = [&](int deck, double bar, qint64 advance, const Lit &want, const QString &where) {
        clock += advance;
        QVERIFY2(vdj.at(deck, bar), qPrintable(where));
        settle(want, where);
        if (deck == 1 && want.strobe == false)
            written << qMakePair(clock, want);
    };

    double lastBar[3] = {0, 0, 0};
    for (int i = 0; i < steps.size(); i++)
    {
        const Step &s = steps[i];
        const QString where = QString("step %1").arg(i + 1);
        switch (s.op)
        {
            case Step::Load:
                clock += kJumpMs;
                QVERIFY2(vdj.load(s.deck, s.flag), qPrintable(where));
                position(s.deck, s.from, 0, s.after, QString("%1, deck %2 loaded at bar %3").arg(where).arg(s.deck).arg(s.from));
                lastBar[s.deck] = s.from;
            break;
            case Step::Master:
                clock += kJumpMs;
                QVERIFY2(vdj.master(s.deck), qPrintable(where));
                settle(s.after, QString("%1, deck %2 is the master").arg(where).arg(s.deck));
            break;
            case Step::At:
                position(s.deck, s.from, kJumpMs, s.after, QString("%1, deck %2 at bar %3").arg(where).arg(s.deck).arg(s.from));
                lastBar[s.deck] = s.from;
            break;
            case Step::Play:
            case Step::Follow:
                for (double bar = s.from; bar <= s.to + 1e-9; bar += s.by)
                {
                    const Lit want = s.op == Step::Follow ? lit({sectionAt(bar)}, {}, s.flag) : s.after;
                    position(s.deck, bar, qRound64((bar - lastBar[s.deck]) * kBarMs), want,
                             QString("%1, deck %2 at bar %3").arg(where).arg(s.deck).arg(bar));
                    lastBar[s.deck] = bar;
                    if (QTest::currentTestFailed())
                        return;
                }
            break;
            case Step::Strobe:
                strobeButton->requestUserStateChange(strobeButton->state() == VCButton::Inactive);
                settle(s.after, QString("%1, the DJ toggles the strobe").arg(where));
            break;
            case Step::Replay:
            {
                // the set closes with Perform, a second after the last write: song Shows stop, the set holds the clip
                QVERIFY2(written.isEmpty() == false, qPrintable(where));
                clock += 1000;
                const quint32 setId = recorder->setShowId();
                bridge->setPerformMode(false);
                QTRY_VERIFY2(songs[0]->isRunning() == false && songs[1]->isRunning() == false, qPrintable(where));
                Show *set = qobject_cast<Show *>(doc->function(setId));
                QVERIFY2(set, qPrintable(where + ": no set recorded"));
                settle(Lit(), where + ", Perform off");

                // the set's time is the recorder's clock from the first write it recorded
                const qint64 setStart = written.first().first;
                set->setSyncSource(ShowRunner::External);
                set->setExternalElapsedTime(0);
                set->start(doc->masterTimer(), FunctionParent::master());
                const auto stopSet = qScopeGuard([set]() { set->stop(FunctionParent::master()); });
                for (const auto &[when, want] : std::as_const(written))
                {
                    const quint32 setMs = quint32(when - setStart + 50);
                    set->setExternalElapsedTime(setMs);
                    settle(want, QString("%1, set at %2 ms").arg(where).arg(setMs));
                    if (QTest::currentTestFailed())
                        return;
                }
            }
            break;
        }
        if (QTest::currentTestFailed())
            return;
    }
}

QTEST_MAIN(EdmE2E_Test)
