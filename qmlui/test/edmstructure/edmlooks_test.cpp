/*
  Q Light Controller Plus - Unit test
  edmlooks_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "edmlooks_test.h"

#include <QtTest>

#include "collection.h"
#include "contextmanager.h"
#include "djfsm.h"
#include "doc.h"
#include "edmanalysisqueue.h"
#include "edmstructure.h"
#include "edmstructuredirector.h"
#include "edmstructuremapping.h"
#include "edmvclooksurface.h"
#include "fixture.h"
#include "fixturemanager.h"
#include "functionmanager.h"
#include "functionparent.h"
#include "inputoutputmap.h"
#include "mastertimer.h"
#include "scene.h"
#include "showeventlog.h"
#include "audio.h"
#include "show.h"
#include "showfunction.h"
#include "track.h"
#define private public
#include "showmanager.h"
#undef private
#define private public
#include "tardis.h"
#undef private
#include "vcbutton.h"
#include "virtualconsole.h"

#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSettings>
#include <QTemporaryDir>

#include <memory>

namespace
{

/** 20 bars of 2 s from 1 s: intro 0-4, buildup 4-8, drop 8-16, outro 16-20 */
const QString kFinal = R"({"schema": "qlcplus-edm-final/2", "track": {"title": "t", "sha256": "aaaaaaaaaaaaaaaa00"},
    "grid": {"bpm": 120.0, "period": 0.5, "beats_per_bar": 4, "first_downbeat": 1.0},
    "sections": [
      {"label": "intro", "start_bar": 0, "end_bar": 4, "start": 0.0, "end": 9.0, "confidence": 100},
      {"label": "buildup", "start_bar": 4, "end_bar": 8, "start": 9.0, "end": 17.0, "confidence": 70},
      {"label": "drop", "start_bar": 8, "end_bar": 16, "start": 17.0, "end": 33.0, "confidence": 90},
      {"label": "outro", "start_bar": 16, "end_bar": 20, "start": 33.0, "end": 41.0, "confidence": 60}],
    "events": [{"kind": "drop", "bar": 8, "time": 17.0, "confidence": 80}]})";

void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

/** The production widgets need the real VirtualConsole graph, as in vcbutton_test */
class UiFixture
{
public:
    explicit UiFixture(Doc *doc)
        : m_fixtureManager(&m_view, doc)
        , m_functionManager(&m_view, doc)
        , m_contextManager(&m_view, doc, &m_fixtureManager, &m_functionManager)
        , m_vc(&m_view, doc, &m_contextManager)
    {
        m_view.rootContext()->setContextProperty("screenPixelDensity", 4.0);
        m_view.rootContext()->setContextProperty("mainView", m_view.contentItem());
        m_rightSidePanel.setWidth(780);
        m_view.rootContext()->setContextProperty("sideLoader", &m_sideLoader);
        m_view.rootContext()->setContextProperty("rightSidePanel", &m_rightSidePanel);
        m_view.setResizeMode(QQuickView::SizeRootObjectToView);
        m_rootComponent.reset(new QQmlComponent(m_view.engine()));
        m_rootComponent->setData("import QtQuick 2.0\n"
                                 "Item { Item { objectName: \"vcPage0\" } }",
                                 QUrl(QStringLiteral("qrc:/EdmLooksTestRoot.qml")));
        QObject *root = m_rootComponent->create();
        m_view.setContent(QUrl(QStringLiteral("qrc:/EdmLooksTestRoot.qml")), m_rootComponent.get(), root);
        m_tardis.reset(new Tardis(&m_view, doc, nullptr, &m_fixtureManager, &m_functionManager, &m_contextManager,
                                  nullptr, nullptr, &m_vc));
        m_tardis->m_busy = true;
    }

    ~UiFixture()
    {
        m_vc.resetContents();
        m_tardis.reset();
        Tardis::s_instance = nullptr;
    }

    QQuickView *view() { return &m_view; }
    VirtualConsole *vc() { return &m_vc; }

private:
    QQuickView m_view;
    QQuickItem m_sideLoader;
    QQuickItem m_rightSidePanel;
    std::unique_ptr<QQmlComponent> m_rootComponent;
    FixtureManager m_fixtureManager;
    FunctionManager m_functionManager;
    ContextManager m_contextManager;
    VirtualConsole m_vc;
    std::unique_ptr<Tardis> m_tardis;
};

/** A Doc with one 4-channel fixture, the Virtual Console, and the director on its buttons; deck songs from a temp analysis dir */
struct Looks
{
    Doc doc;
    UiFixture ui;
    QTemporaryDir root;
    QString dir;
    DjFsm fsm;
    EdmAnalysisQueue queue;
    EdmStructure structure;
    EdmStructureMapping mapping;
    EdmVcLookSurface surface;
    EdmStructureDirector director;
    Fixture *fixture;

    Looks()
        : doc(nullptr, 1)
        , ui(&doc)
        , dir(root.filePath("analysis"))
        , queue(dir)
        , structure(&fsm, &queue, dir)
        , surface(ui.vc())
        , director(&structure, &mapping, &surface)
    {
        QDir().mkpath(dir + "/final");
        writeFile(dir + "/final/aaaaaaaaaaaaaaaa.json", kFinal.toUtf8());
        writeFile(dir + "/index.json", R"({"aaaaaaaaaaaaaaaa": {"path": "/m/a.mp3"}})");
        fixture = new Fixture(&doc);
        fixture->setChannels(4);
        fixture->setAddress(0);
        fixture->setUniverse(0);
        doc.addFixture(fixture);
        doc.masterTimer()->start();
    }

    ~Looks()
    {
        doc.masterTimer()->stop();
    }

    /** An empty Scene stops itself, so each holds one channel */
    Scene *scene(quint32 channel)
    {
        Scene *s = new Scene(&doc);
        s->setValue(SceneValue(fixture->id(), channel, 200));
        doc.addFunction(s);
        return s;
    }

    void map(const QString &label, VCButton &button)
    {
        mapping.setAutoEnabled(true);
        mapping.setWidget(label, button.id());
    }

    void load(int deck, const QString &path)
    {
        fsm.onDeckTrigger(deck, "masterdeck", "on");
        fsm.onDeckTrigger(deck, "get_filepath", path);
        fsm.onDeckTrigger(deck, "get_title", "t");
        fsm.onDeckTrigger(deck, "get_artist", "a");
        fsm.onDeckTrigger(deck, "get_bpm", 120.0);
        fsm.onDeckTrigger(deck, "loaded", "on");
        fsm.onTickEnded();
    }
    void at(int deck, double bar)
    {
        fsm.onDeckTrigger(deck, "get_time elapsed absolute", 1000.0 + bar * 2000.0);
        fsm.onTickEnded();
    }
};

void collectItems(QQuickItem *item, QList<QQuickItem *> &out)
{
    out << item;
    for (QQuickItem *child : item->childItems())
        collectItems(child, out);
}

}

void EdmLooks_Test::initTestCase()
{
    QSettings::setDefaultFormat(QSettings::IniFormat);
    static QTemporaryDir settingsDir;
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QCoreApplication::setOrganizationName("edmlooks_test");
    qmlRegisterUncreatableType<VCWidget>("org.qlcplus.classes", 1, 0, "VCWidget", "Console-owned");
}

void EdmLooks_Test::lookPressSkipsNonToggleButtons_data()
{
    QTest::addColumn<int>("action");

    QTest::newRow("StopAll") << int(VCButton::StopAll);
    QTest::newRow("Blackout") << int(VCButton::Blackout);
}

void EdmLooks_Test::lookPressSkipsNonToggleButtons()
{
    QFETCH(int, action);

    Looks looks;
    Scene *running = looks.scene(0);
    VCButton button(&looks.doc);
    button.setActionType(VCButton::ButtonAction(action));
    looks.ui.vc()->addWidgetToMap(&button);
    const auto unmap = qScopeGuard([&]() { looks.ui.vc()->removeWidgetFromMap(&button); });
    looks.map("drop", button);

    looks.load(1, "/m/a.mp3");
    looks.at(1, 4.5);
    running->start(looks.doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY(running->isRunning());

    looks.at(1, 8.5);   // the drop starts
    QTest::qWait(300);

    QVERIFY2(running->isRunning(), "the drop's \"look\" stopped every function");
    QVERIFY2(looks.doc.inputOutputMap()->blackout() == false, "the drop's \"look\" switched blackout on");
}

void EdmLooks_Test::monitoredLookIsReleased()
{
    Looks looks;
    Scene *build = looks.scene(0);
    Scene *drop = looks.scene(1);
    VCButton buildLook(&looks.doc), dropLook(&looks.doc);
    buildLook.setFunctionID(build->id());
    dropLook.setFunctionID(drop->id());
    looks.ui.vc()->addWidgetToMap(&buildLook);
    looks.ui.vc()->addWidgetToMap(&dropLook);
    const auto unmap = qScopeGuard([&]() {
        looks.ui.vc()->removeWidgetFromMap(&buildLook);
        looks.ui.vc()->removeWidgetFromMap(&dropLook);
    });
    looks.map("buildup", buildLook);
    looks.map("drop", dropLook);

    looks.load(1, "/m/a.mp3");
    looks.at(1, 1.0);   // intro: no look mapped
    Collection *cue = new Collection(&looks.doc);
    cue->addFunction(build->id());
    looks.doc.addFunction(cue);
    cue->start(looks.doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY(build->isRunning());
    QTRY_COMPARE(buildLook.state(), VCButton::Monitoring);

    looks.at(1, 8.5);   // through the buildup into the drop

    QTRY_VERIFY2(build->isRunning() == false, "the buildup look started by a Collection stays on under the drop look");
}

void EdmLooks_Test::noPressWhenBlocked_data()
{
    QTest::addColumn<QString>("block");

    QTest::newRow("edit mode") << "edit mode";
    QTest::newRow("disabled") << "disabled";
}

void EdmLooks_Test::noPressWhenBlocked()
{
    QFETCH(QString, block);

    Looks looks;
    Scene *drop = looks.scene(0);
    VCButton dropLook(&looks.doc);
    dropLook.setFunctionID(drop->id());
    looks.ui.vc()->addWidgetToMap(&dropLook);
    const auto unmap = qScopeGuard([&]() { looks.ui.vc()->removeWidgetFromMap(&dropLook); });
    looks.map("drop", dropLook);
    if (block == "edit mode")
        looks.ui.vc()->setEditMode(true);
    else
        dropLook.setDisabled(true);

    looks.load(1, "/m/a.mp3");
    looks.at(1, 4.5);
    looks.at(1, 8.5);   // the drop starts
    QTest::qWait(200);

    QCOMPARE(dropLook.state(), VCButton::Inactive);
}

void EdmLooks_Test::testMappingCombosFollowWorkspaceLoad()
{
    Looks looks;
    VCButton chill(&looks.doc), dropLook(&looks.doc);
    chill.setCaption("Chill");
    dropLook.setCaption("Drop");
    looks.ui.vc()->addWidgetToMap(&chill);
    looks.ui.vc()->addWidgetToMap(&dropLook);
    const auto unmap = qScopeGuard([&]() {
        looks.ui.vc()->removeWidgetFromMap(&chill);
        looks.ui.vc()->removeWidgetFromMap(&dropLook);
    });
    looks.map("drop", chill);

    QQmlContext *context = looks.ui.view()->rootContext();
    context->setContextProperty("edmMapping", &looks.mapping);
    context->setContextProperty("edmDirector", &looks.director);
    context->setContextProperty("virtualConsole", looks.ui.vc());
    QQmlComponent component(looks.ui.view()->engine(), QUrl("qrc:/StructureMapping.qml"));
    std::unique_ptr<QObject> panel(component.create(context));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));
    panel->setProperty("expanded", true);
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "loadButtons"));

    auto comboTexts = [&]() {
        QList<QQuickItem *> items;
        collectItems(qobject_cast<QQuickItem *>(panel.get()), items);
        QStringList texts;
        for (QQuickItem *item : items)
        {
            if (QString(item->metaObject()->className()).contains("ComboBox"))
                texts << item->property("currentText").toString();
        }
        return texts;
    };
    QVERIFY2(comboTexts().contains("Chill"), qPrintable(comboTexts().join(", ")));

    // another workspace maps the drop to the Drop button
    QXmlStreamReader reader(QString(R"(<SongStructure Auto="1"><Look Label="drop" Widget="%1"/></SongStructure>)").arg(dropLook.id()));
    QVERIFY(reader.readNextStartElement());
    QVERIFY(looks.mapping.loadXML(reader));

    QVERIFY2(comboTexts().contains("Drop"), qPrintable("combos show: " + comboTexts().join(", ")));
}

void EdmLooks_Test::analysisSectionsFollowAudioStart()
{
    Looks looks;
    Audio *audio = new Audio(&looks.doc);
    QVERIFY(looks.doc.addFunction(audio));
    audio->setSourceFileName("/m/a.mp3");
    Show *show = new Show(&looks.doc);
    QVERIFY(looks.doc.addFunction(show));
    Track *track = new Track(Function::invalidId(), show);
    show->addTrack(track);
    ShowFunction *clip = track->createShowFunction(audio->id());
    clip->setStartTime(30000);   // the song starts 30 s into the Show

    ShowManager manager(looks.ui.view(), &looks.doc);
    manager.m_analysisReader = EdmAnalysisReader(looks.dir);   // the test's analysis dir, not the user's
    manager.setCurrentShowID(int(show->id()));

    const QVariantList sections = manager.analysisSections();
    QVERIFY(sections.isEmpty() == false);
    QCOMPARE(sections.first().toMap().value("startMs").toDouble(), 30000.0);
}

void EdmLooks_Test::analysisBandPosition_data()
{
    QTest::addColumn<bool>("msMode");
    QTest::addColumn<double>("ms");
    QTest::addColumn<double>("x");

    // timeScale 2 s per tick, 18 px per tick; BPM ruler: 128 BPM, 4 beats per bar, one bar per tick
    QTest::newRow("time ruler") << true << 60000.0 << 540.0;
    QTest::newRow("BPM ruler: 32 bars at 128 BPM") << false << 60000.0 << 576.0;
}

void EdmLooks_Test::analysisBandPosition()
{
    QFETCH(bool, msMode);
    QFETCH(double, ms);
    QFETCH(double, x);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(import QtQml
import "qrc:/TimeUtils.js" as TimeUtils
QtObject {
    function run(msMode, ms) { return TimeUtils.msToTimelinePos(ms, msMode, 2, 18, 128, 4) }
})", QUrl());
    std::unique_ptr<QObject> probe(component.create());
    QVERIFY2(probe != nullptr, qPrintable(component.errorString()));
    QVariant pos;
    QVERIFY(QMetaObject::invokeMethod(probe.get(), "run", Q_RETURN_ARG(QVariant, pos), Q_ARG(QVariant, msMode), Q_ARG(QVariant, ms)));

    QCOMPARE(pos.toDouble(), x);
}

void EdmLooks_Test::pressReachesALookOnAnotherFramePage()
{
    Looks looks;
    Scene *drop = looks.scene(0);
    VCButton dropLook(&looks.doc);
    dropLook.setFunctionID(drop->id());
    looks.ui.vc()->addWidgetToMap(&dropLook);
    const auto unmap = qScopeGuard([&]() { looks.ui.vc()->removeWidgetFromMap(&dropLook); });
    looks.map("drop", dropLook);
    dropLook.setVisible(false);   // what a multipage frame does to the buttons of the pages not shown

    looks.load(1, "/m/a.mp3");
    looks.at(1, 4.5);
    looks.at(1, 8.5);   // the drop starts

    QCOMPARE(dropLook.state(), VCButton::Active);
}

void EdmLooks_Test::releaseIsAlwaysAllowed_data()
{
    QTest::addColumn<QString>("block");

    QTest::newRow("edit mode") << "edit mode";
    QTest::newRow("disabled") << "disabled";
    QTest::newRow("on another frame page") << "hidden";
}

void EdmLooks_Test::releaseIsAlwaysAllowed()
{
    QFETCH(QString, block);

    Looks looks;
    Scene *build = looks.scene(0);
    Scene *drop = looks.scene(1);
    VCButton buildLook(&looks.doc), dropLook(&looks.doc);
    buildLook.setFunctionID(build->id());
    dropLook.setFunctionID(drop->id());
    looks.ui.vc()->addWidgetToMap(&buildLook);
    looks.ui.vc()->addWidgetToMap(&dropLook);
    const auto unmap = qScopeGuard([&]() {
        looks.ui.vc()->removeWidgetFromMap(&buildLook);
        looks.ui.vc()->removeWidgetFromMap(&dropLook);
    });
    looks.map("buildup", buildLook);
    looks.map("drop", dropLook);
    looks.load(1, "/m/a.mp3");
    looks.at(1, 4.5);
    QCOMPARE(buildLook.state(), VCButton::Active);

    if (block == "edit mode")
        looks.ui.vc()->setEditMode(true);
    else if (block == "disabled")
        buildLook.setDisabled(true);
    else
        buildLook.setVisible(false);
    looks.at(1, 8.5);   // the drop starts: the buildup look goes off

    QCOMPARE(buildLook.state(), VCButton::Inactive);
}

void EdmLooks_Test::lookPressIsTraced()
{
    Looks looks;
    Scene *drop = looks.scene(0);
    VCButton dropLook(&looks.doc);
    dropLook.setFunctionID(drop->id());
    looks.ui.vc()->addWidgetToMap(&dropLook);
    const auto unmap = qScopeGuard([&]() { looks.ui.vc()->removeWidgetFromMap(&dropLook); });
    looks.map("drop", dropLook);
    ShowEventLog::open();
    const auto closeLog = qScopeGuard([]() { ShowEventLog::close(); });

    looks.load(1, "/m/a.mp3");
    looks.at(1, 8.5);
    QCOMPARE(dropLook.state(), VCButton::Active);

    bool traced = false;
    for (const ShowEventLog::Entry &e : ShowEventLog::snapshot(0).entries)
        traced = traced || (e.widgetId == dropLook.id() && e.reason.contains("direct code request"));
    QVERIFY2(traced, "the debug panel does not see the look press");
}

QTEST_MAIN(EdmLooks_Test)
