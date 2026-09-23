/*
  Q Light Controller Plus - Unit test
  showcommandrecorder_test.cpp

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

#include "showcommandrecorder_test.h"

// The plugin cache only ever loads plugins from disk. A unit test has no
// control surface attached, so it registers its own I/O plugin instead.
#define private public
#include "ioplugincache.h"
#undef private

#include <QtTest>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickView>
#include <memory>

#include "contextmanager.h"
#include "doc.h"
#include "fixture.h"
#include "fixturemanager.h"
#include "functionmanager.h"
#include "functionparent.h"
#include "mastertimer.h"
#include "universe.h"
#include "inputpatch.h"
#include "inputoutputmap.h"
#include "qlcinputsource.h"
#include "qlcioplugin.h"
#include "scene.h"
#include "show.h"
#include "showcommandrecorder.h"
#include "showcommandtrack.h"
#include "showrunner.h"
#define private public
#include "tardis.h"
#undef private
#include "vcbridgev5.h"
#include "vcbutton.h"
#include "vcpage.h"
#include "vcslider.h"
#include "virtualconsole.h"

namespace
{
/** The production widgets talk to Tardis on every value change and are reached
 *  through the real VirtualConsole/VCPage dispatch, so the recorder has to be
 *  tested against that same graph. */
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
                                 QUrl(QStringLiteral("qrc:/ShowCommandRecorderTestRoot.qml")));
        QObject *root = m_rootComponent->create();
        m_view.setContent(QUrl(QStringLiteral("qrc:/ShowCommandRecorderTestRoot.qml")),
                          m_rootComponent.get(), root);
        m_tardis.reset(new Tardis(&m_view, doc, nullptr, &m_fixtureManager,
                                  &m_functionManager, &m_contextManager,
                                  nullptr, nullptr, &m_vc));
        // Keep the undo thread out of the assertions; configuration still runs.
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

constexpr quint32 kRecordPosition = 4200;
constexpr quint32 kSourceUniverse = 0;
constexpr quint32 kSourceChannel = 17;

/** A stand-in for a patched I/O plugin. It exercises the production patching
 *  and dispatch code; it does not emulate any protocol or hardware. */
class TestIOPlugin : public QLCIOPlugin
{
public:
    explicit TestIOPlugin(const QString &name)
        : m_name(name)
    {
    }

    void init() override {}
    QString name() const override { return m_name; }
    int capabilities() const override { return QLCIOPlugin::Input; }
    QString pluginInfo() const override { return m_name; }
    QStringList inputs() override { return QStringList() << QStringLiteral("Test line"); }
    QString inputInfo(quint32) override { return m_name; }
    bool openInput(quint32, quint32) override { return true; }
    void closeInput(quint32, quint32) override {}

private:
    QString m_name;
};

/** Patch a test plugin to an input universe through the production API. The
 *  cache takes ownership, exactly as it does for a plugin loaded from disk. */
bool patchInputPlugin(Doc *doc, const QString &pluginName, quint32 universe)
{
    doc->ioPluginCache()->m_plugins.append(new TestIOPlugin(pluginName));
    return doc->inputOutputMap()->setInputPatch(universe, pluginName,
                                                QString(), QString(), 0);
}

/** A Show that reports a stable authoritative position, the way an externally
 *  driven (Perform) Show does. The recorder keeps no playhead of its own. */
Show *addRecordingShow(Doc *doc, quint32 positionMs)
{
    Show *show = new Show(doc);
    show->setName(QStringLiteral("Recording show"));
    if (doc->addFunction(show) == false)
        return nullptr;

    show->setSyncSource(ShowRunner::External);
    show->setExternalElapsedTime(positionMs);
    return show;
}

Scene *addTarget(Doc *doc)
{
    Fixture *fixture = new Fixture(doc);
    fixture->setChannels(4);
    fixture->setAddress(0);
    fixture->setUniverse(0);
    if (doc->addFixture(fixture) == false)
        return nullptr;

    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("Recorded target"));
    scene->setValue(SceneValue(fixture->id(), 0, 200));
    if (doc->addFunction(scene) == false)
        return nullptr;

    return scene;
}

/** The production external-input route: a mapped input source delivered through
 *  the very slot InputOutputMap's signal is connected to. No control surface is
 *  patched to this universe here, so the event carries no user provenance. */
bool deliverMappedInput(VirtualConsole *vc, uchar value)
{
    return QMetaObject::invokeMethod(vc, "slotInputValueChanged",
                                     Q_ARG(quint32, kSourceUniverse),
                                     Q_ARG(quint32, kSourceChannel),
                                     Q_ARG(uchar, value));
}

void mapInputSource(VCPage *page, VCWidget *widget, quint8 controlId)
{
    QSharedPointer<QLCInputSource> source(new QLCInputSource(kSourceUniverse, kSourceChannel));
    source->setID(controlId);
    widget->addInputSource(source);
    page->mapInputSource(source, widget);
}

/** Find an item by objectName in the visual tree. List delegates are not
 *  QObject children of the view they appear in, so findChild misses them. */
QQuickItem *findVisualItem(QQuickItem *root, const QString &objectName)
{
    if (root == nullptr)
        return nullptr;
    if (root->objectName() == objectName)
        return root;

    QList<QQuickItem *> children = root->childItems();
    QQuickItem *content = root->property("contentItem").value<QQuickItem *>();
    if (content != nullptr && !children.contains(content))
        children.append(content);

    for (QQuickItem *child : children)
    {
        if (QQuickItem *found = findVisualItem(child, objectName))
            return found;
    }
    return nullptr;
}

/** Type into whatever currently has keyboard focus in the view */
void typeInto(QQuickView *view, const QString &text)
{
    for (const QChar &c : text)
        QTest::keyClick(view, c.toLatin1());
    QTest::keyClick(view, Qt::Key_Return);
}

QQuickItem *renderButtonItem(QQuickView *view, VCButton *button)
{
    QQmlComponent component(view->engine(), QUrl(QStringLiteral("qrc:/VCButtonItem.qml")));
    if (component.isError())
    {
        qWarning() << component.errors();
        return nullptr;
    }
    QQuickItem *item = qobject_cast<QQuickItem *>(component.create());
    if (item == nullptr)
        return nullptr;
    item->setParentItem(view->contentItem());
    item->setProperty("buttonObj", QVariant::fromValue(button));
    return item;
}
}

void ShowCommandRecorder_Test::externalInput_authorsOnlyThroughMidiPatch_data()
{
    QTest::addColumn<QString>("pluginName");
    QTest::addColumn<int>("expectedCommands");

    // A MIDI control surface is a person moving something.
    QTest::newRow("midi-patch") << QStringLiteral("MIDI") << 1;

    // Every other transport reaches the very same dispatch with the very same
    // value and is not evidence that anybody touched a control.
    QTest::newRow("artnet-patch") << QStringLiteral("ArtNet") << 0;
    QTest::newRow("e131-patch") << QStringLiteral("E1.31") << 0;
    QTest::newRow("loopback-patch") << QStringLiteral("Loopback") << 0;
}

void ShowCommandRecorder_Test::externalInput_authorsOnlyThroughMidiPatch()
{
    QFETCH(QString, pluginName);
    QFETCH(int, expectedCommands);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    QVERIFY(patchInputPlugin(&doc, pluginName, kSourceUniverse));
    QCOMPARE(doc.inputOutputMap()->inputPatch(kSourceUniverse)->pluginName(), pluginName);

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);
    QVERIFY(recorder.isAuthoring());

    VCBridgeV5 bridge(&doc, ui.vc());
    VCPage *page = ui.vc()->page(0);
    QVERIFY(page);
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    QVERIFY(frameID >= 0);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    QVERIFY(buttonID >= 0);
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);
    mapInputSource(page, button, 0);

    QVERIFY(deliverMappedInput(ui.vc(), UCHAR_MAX));

    // The live effect is identical on every transport.
    QCOMPARE(button->state(), VCButton::Active);
    QTRY_VERIFY(scene->isRunning());

    const QVector<ShowCommand> &stored = recorder.track().commands();
    QTRY_COMPARE(stored.count(), qsizetype(expectedCommands));
    QTest::qWait(120);
    QCOMPARE(stored.count(), qsizetype(expectedCommands));

    if (expectedCommands > 0)
    {
        QCOMPARE(int(stored.at(0).action), int(ShowCommandAction::Start));
        QCOMPARE(stored.at(0).functionId, scene->id());
        QCOMPARE(stored.at(0).time, kRecordPosition);
    }
}

void ShowCommandRecorder_Test::onlyUserInputAuthors_data()
{
    QTest::addColumn<QString>("route");
    QTest::addColumn<QVariantList>("actions");
    QTest::addColumn<QVariantList>("intensities");

    const QVariantList none;
    const QVariantList start{int(ShowCommandAction::Start)};
    const QVariantList startThenValue{int(ShowCommandAction::Start),
                                      int(ShowCommandAction::SetIntensity)};

    // Real user intent, each through its own production ingress.
    QTest::newRow("button-pointer") << QStringLiteral("button-pointer")
                                    << start << QVariantList{0.0};
    QTest::newRow("button-keyboard") << QStringLiteral("button-keyboard")
                                     << start << QVariantList{0.0};
    QTest::newRow("slider-pointer") << QStringLiteral("slider-pointer")
                                    << startThenValue << QVariantList{0.0, 1.0};

    // Negative controls: the same live effect, produced by something that is
    // not a person operating a control. A mapped external source on a universe
    // with no MIDI control surface patched to it is one of them, and so is the
    // shared input slot, which synthetic and scripted callers reach directly.
    QTest::newRow("button-external-not-midi") << QStringLiteral("button-external")
                                              << none << QVariantList();
    QTest::newRow("slider-external-not-midi") << QStringLiteral("slider-external")
                                              << none << QVariantList();
    QTest::newRow("button-programmatic") << QStringLiteral("button-programmatic")
                                         << none << QVariantList();
    QTest::newRow("button-slot") << QStringLiteral("button-slot")
                                 << none << QVariantList();
    QTest::newRow("slider-audio") << QStringLiteral("slider-audio")
                                  << none << QVariantList();
}

void ShowCommandRecorder_Test::onlyUserInputAuthors()
{
    QFETCH(QString, route);
    QFETCH(QVariantList, actions);
    QFETCH(QVariantList, intensities);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);
    QCOMPARE(show->commandPosition(), kRecordPosition);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);
    QVERIFY(recorder.isAuthoring());
    QVERIFY(show->commandRecording());

    VCBridgeV5 bridge(&doc, ui.vc());
    VCPage *page = ui.vc()->page(0);
    QVERIFY(page);
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    QVERIFY(frameID >= 0);

    const bool slider = route.startsWith(QStringLiteral("slider"));
    VCButton *button = nullptr;
    VCSlider *fader = nullptr;
    std::unique_ptr<QQuickItem> item;

    if (slider)
    {
        const int sliderID = bridge.addSlider(frameID, QRect(10, 10, 60, 200),
                                              QStringLiteral("level"),
                                              QStringLiteral("Intensity"),
                                              Function::invalidId(), {});
        QVERIFY(sliderID >= 0);
        fader = qobject_cast<VCSlider *>(ui.vc()->widget(sliderID));
        QVERIFY(fader);
        // Level -> Adjust is the transition that arms the function-intensity
        // mode: it registers the DMX source and selects the Intensity attribute.
        fader->setSliderMode(VCSlider::Adjust);
        fader->setControlledFunction(scene->id());
        QCOMPARE(fader->controlledAttribute(), int(Function::Intensity));

        if (route.endsWith(QStringLiteral("external")))
        {
            mapInputSource(page, fader, 0);
            QVERIFY(deliverMappedInput(ui.vc(), UCHAR_MAX));
        }
        else if (route.endsWith(QStringLiteral("pointer")))
        {
            fader->requestUserValue(UCHAR_MAX);
        }
        else
        {
            fader->setValue(UCHAR_MAX, true, true); // the audio trigger path
        }

        // Every route applies the value live exactly once, and the native
        // execution seam runs on the timer, not at input time.
        QCOMPARE(fader->value(), int(UCHAR_MAX));
        QTRY_VERIFY(scene->isRunning());
    }
    else
    {
        const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                              QStringLiteral("Target"), QStringLiteral("toggle"));
        QVERIFY(buttonID >= 0);
        button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
        QVERIFY(button);

        if (route.endsWith(QStringLiteral("pointer")))
        {
            QQuickView *view = ui.view();
            view->resize(400, 300);
            item.reset(renderButtonItem(view, button));
            QVERIFY(item);
            view->show();
            QVERIFY(QTest::qWaitForWindowExposed(view));
            const QPoint center(80, 40);
            QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, center);
            QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, center);
            QCoreApplication::processEvents();
        }
        else if (route.endsWith(QStringLiteral("external")))
        {
            mapInputSource(page, button, 0);
            QVERIFY(deliverMappedInput(ui.vc(), UCHAR_MAX));
        }
        else if (route.endsWith(QStringLiteral("keyboard")))
        {
            const QKeySequence seq(QStringLiteral("Ctrl+F"));
            button->addKeySequence(seq, 0);
            page->buildKeySequenceMap();
            QKeyEvent event(QEvent::KeyPress, Qt::Key_F, Qt::ControlModifier);
            ui.vc()->handleKeyEvent(&event, true);
        }
        else if (route.endsWith(QStringLiteral("slot")))
        {
            // The shared input slot is also reached by synthetic and scripted
            // callers, so it carries no provenance of its own.
            button->slotInputValueChanged(0, UCHAR_MAX);
        }
        else
        {
            button->requestStateChange(true); // scripts, web access, Tardis
        }

        // Every route starts the function live exactly once.
        QCOMPARE(button->state(), VCButton::Active);
        QTRY_VERIFY(scene->isRunning());
    }

    const QVector<ShowCommand> &stored = recorder.track().commands();
    QTRY_COMPARE(stored.count(), actions.count());

    // Three engine ticks later nothing has been added behind the user's back:
    // the negative controls stay empty and no route authors twice.
    QTest::qWait(120);
    QCOMPARE(stored.count(), actions.count());

    for (int i = 0; i < actions.count(); i++)
    {
        const ShowCommand &cmd = stored.at(i);
        QCOMPARE(int(cmd.action), actions.at(i).toInt());
        QCOMPARE(cmd.functionId, scene->id());
        // The timestamp is the Show time the input happened at, never the
        // time some later engine tick executed it.
        QCOMPARE(cmd.time, kRecordPosition);
        if (cmd.action == ShowCommandAction::SetIntensity)
            QCOMPARE(cmd.intensity, intensities.at(i).toReal());
    }

    // What the recorder holds is what the Show publishes: one committed
    // transition, not a private copy that drifts from the host.
    QCOMPARE(show->commandTrack().commands(), stored);

    // Stable, unique ids are what the editor and the runtime address.
    if (stored.count() > 1)
        QVERIFY(stored.at(0).id != stored.at(1).id);
}

void ShowCommandRecorder_Test::sliderUserValue_survivesLaterAudioValue()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);
    QVERIFY(recorder.isAuthoring());

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    QVERIFY(frameID >= 0);
    const int sliderID = bridge.addSlider(frameID, QRect(10, 10, 60, 200),
                                          QStringLiteral("level"), QStringLiteral("Intensity"),
                                          Function::invalidId(), {});
    QVERIFY(sliderID >= 0);
    VCSlider *fader = qobject_cast<VCSlider *>(ui.vc()->widget(sliderID));
    QVERIFY(fader);
    fader->setSliderMode(VCSlider::Adjust);
    fader->setControlledFunction(scene->id());

    // The user moves the fader, then an audio trigger overwrites the value
    // before the engine tick that actually executes the adjustment.
    fader->requestUserValue(200);
    fader->setValue(60, true, true);
    QCOMPARE(fader->value(), 60);

    const QVector<ShowCommand> &stored = recorder.track().commands();
    QTRY_COMPARE(stored.count(), qsizetype(2));
    QTest::qWait(120);
    QCOMPARE(stored.count(), qsizetype(2));

    QCOMPARE(int(stored.at(0).action), int(ShowCommandAction::Start));
    QCOMPARE(stored.at(0).functionId, scene->id());
    QCOMPARE(int(stored.at(1).action), int(ShowCommandAction::SetIntensity));
    // The user's own value is what gets recorded. The audio value that
    // arrived first at the engine authors nothing at all.
    QCOMPARE(stored.at(1).intensity, qreal(200) / qreal(UCHAR_MAX));
    QCOMPARE(stored.at(1).time, kRecordPosition);
}


void ShowCommandRecorder_Test::monitoringToggle_firstUserClickStops_data()
{
    QTest::addColumn<bool>("recording");
    QTest::addColumn<QString>("route");

    // Every ingress a person can press reaches the same resolver. The shared
    // Toggle dispatch guards used to swallow a monitored press before it ever
    // got there, so a MIDI or key press restarted what the operator saw lit.
    for (const char *route : {"pointer", "midi", "keyboard"})
    {
        QTest::newRow(qPrintable(QStringLiteral("recording-%1").arg(route)))
            << true << QString(route);
        QTest::newRow(qPrintable(QStringLiteral("not-recording-%1").arg(route)))
            << false << QString(route);
    }
}

void ShowCommandRecorder_Test::monitoringToggle_firstUserClickStops()
{
    QFETCH(bool, recording);
    QFETCH(QString, route);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    if (route == QStringLiteral("midi"))
        QVERIFY(patchInputPlugin(&doc, QStringLiteral("MIDI"), kSourceUniverse));

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(recording);

    VCBridgeV5 bridge(&doc, ui.vc());
    VCPage *page = ui.vc()->page(0);
    QVERIFY(page);
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    QVERIFY(frameID >= 0);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    QVERIFY(buttonID >= 0);
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);

    // Something else - the Show itself, in production - is already playing the
    // target, so the button displays it as monitored rather than pressed.
    scene->start(doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY(scene->isRunning());
    QTRY_COMPARE(button->state(), VCButton::Monitoring);

    if (route == QStringLiteral("midi"))
    {
        mapInputSource(page, button, 0);
        QVERIFY(deliverMappedInput(ui.vc(), UCHAR_MAX));
    }
    else if (route == QStringLiteral("keyboard"))
    {
        const QKeySequence seq(QStringLiteral("Ctrl+F"));
        button->addKeySequence(seq, 0);
        page->buildKeySequenceMap();
        QKeyEvent event(QEvent::KeyPress, Qt::Key_F, Qt::ControlModifier);
        ui.vc()->handleKeyEvent(&event, true);
    }
    else
    {
        button->requestUserStateChange(true);
    }

    const QVector<ShowCommand> &stored = recorder.track().commands();
    if (recording)
    {
        // What the operator sees is a lit button, so their click means stop.
        QTRY_VERIFY(!scene->isRunning());
        QCOMPARE(button->state(), VCButton::Inactive);
        QTRY_COMPARE(stored.count(), qsizetype(1));
        QCOMPARE(int(stored.at(0).action), int(ShowCommandAction::Stop));
        QCOMPARE(stored.at(0).functionId, scene->id());
    }
    else
    {
        // Ordinary behaviour is untouched: the click restarts it as before.
        QCOMPARE(button->state(), VCButton::Active);
        QVERIFY(scene->isRunning());
        QCOMPARE(stored.count(), qsizetype(0));
    }
}

void ShowCommandRecorder_Test::recordedTarget_survivesWidgetRetarget()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Scene *other = new Scene(&doc);
    other->setName(QStringLiteral("Other target"));
    QVERIFY(doc.addFunction(other));
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);

    button->requestUserStateChange(true);
    const QVector<ShowCommand> &stored = recorder.track().commands();
    QTRY_COMPARE(stored.count(), qsizetype(1));
    const quint32 recordedId = stored.at(0).id;

    // Reassigning the widget afterwards must not rewrite history: commands
    // address stable Function ids, not the control that happened to fire them.
    button->setFunctionID(other->id());

    QCOMPARE(stored.count(), qsizetype(1));
    QCOMPARE(stored.at(0).id, recordedId);
    QCOMPARE(stored.at(0).functionId, scene->id());
    QCOMPARE(show->commandTrack().commands().at(0).functionId, scene->id());
}

void ShowCommandRecorder_Test::armedRecorder_bindsFirstShowAndAuthorsWhileFrozen()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);

    ShowCommandRecorder recorder(&doc);
    recorder.setRecording(true);

    // Armed without a Show: it waits instead of guessing a target.
    QCOMPARE(recorder.phaseValue(), int(ShowRecordPhase::Armed));
    QVERIFY(!recorder.isAuthoring());

    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);
    recorder.setResolvedShow(show->id());
    QCOMPARE(recorder.phaseValue(), int(ShowRecordPhase::Bound));
    QCOMPARE(recorder.targetName(), show->name());

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);

    // A paused transport keeps reporting the same time; recording stays live
    // and stamps both commands at that frozen position.
    recorder.setPlaying(false);
    button->requestUserStateChange(true);
    button->requestUserStateChange(false);

    const QVector<ShowCommand> &stored = recorder.track().commands();
    QTRY_COMPARE(stored.count(), qsizetype(2));
    QCOMPARE(stored.at(0).time, kRecordPosition);
    QCOMPARE(stored.at(1).time, kRecordPosition);
    QCOMPARE(int(stored.at(0).action), int(ShowCommandAction::Start));
    QCOMPARE(int(stored.at(1).action), int(ShowCommandAction::Stop));

    // Another Show resolving now suspends recording rather than retargeting it.
    Show *second = new Show(&doc);
    second->setName(QStringLiteral("Other show"));
    QVERIFY(doc.addFunction(second));
    recorder.setResolvedShow(second->id());
    QCOMPARE(recorder.phaseValue(), int(ShowRecordPhase::Suspended));
    QVERIFY(!recorder.isAuthoring());
    QCOMPARE(second->commandTrack().count(), 0);
}

void ShowCommandRecorder_Test::recordingExtent_outlivesLastCommand()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);
    // Recording intent keeps a command-only Show playing past its clips.
    QVERIFY(show->commandRecording());

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);
    button->requestUserStateChange(true);

    const QVector<ShowCommand> &stored = recorder.track().commands();
    QTRY_COMPARE(stored.count(), qsizetype(1));

    // The take reaches as far as the capture got, with nothing padded on:
    // the Show's own recording flag is what keeps a short take playing.
    QCOMPARE(show->commandTrack().extent(), stored.at(0).time);
    QCOMPARE(show->commandTrack().extent(), recorder.track().extent());

    // Stopping replaces that live keepalive with the captured duration: a
    // fixed tail is not a duration and is not what gets saved.
    recorder.setRecording(false);
    QVERIFY(!show->commandRecording());
    QCOMPARE(show->commandTrack().extent(), show->commandPosition());
    QVERIFY(show->commandTrack().extent() >= stored.at(0).time);
}

void ShowCommandRecorder_Test::editor_retimeValueDeleteAndExtent()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int sliderID = bridge.addSlider(frameID, QRect(10, 10, 60, 200),
                                          QStringLiteral("level"), QStringLiteral("Intensity"),
                                          Function::invalidId(), {});
    VCSlider *fader = qobject_cast<VCSlider *>(ui.vc()->widget(sliderID));
    QVERIFY(fader);
    fader->setSliderMode(VCSlider::Adjust);
    fader->setControlledFunction(scene->id());
    fader->requestUserValue(128);

    const QVector<ShowCommand> &stored = recorder.track().commands();
    QTRY_COMPARE(stored.count(), qsizetype(2));
    const quint32 valueId = stored.at(1).id;
    QCOMPARE(int(stored.at(1).action), int(ShowCommandAction::SetIntensity));

    // Editing goes through the model and reaches the Show.
    QVERIFY(recorder.retimeCommand(valueId, 9000));
    QCOMPARE(recorder.track().commands().at(1).time, quint32(9000));
    QCOMPARE(show->commandTrack().commands().at(1).time, quint32(9000));

    QVERIFY(recorder.setCommandIntensity(valueId, 0.25));
    QCOMPARE(recorder.track().commands().at(1).intensity, 0.25);

    // A rejected edit changes nothing and says why.
    QVERIFY(!recorder.setCommandIntensity(valueId, 4.2));
    QVERIFY(!recorder.lastError().isEmpty());
    QCOMPARE(recorder.track().commands().at(1).intensity, 0.25);
    QVERIFY(!recorder.removeCommand(ShowCommand::InvalidId));
    QCOMPARE(recorder.track().count(), 2);

    QVERIFY(recorder.setExtent(30000));
    QCOMPARE(recorder.extent(), 30000);
    QCOMPARE(show->commandTrack().extent(), quint32(30000));

    QVERIFY(recorder.removeCommand(valueId));
    QCOMPARE(recorder.track().count(), 1);
    QCOMPARE(show->commandTrack().count(), 1);
    QVERIFY(recorder.lastError().isEmpty());

    // The editable view the QML list binds to mirrors the model.
    const QVariantList view = recorder.commands();
    QCOMPARE(view.count(), 1);
    QCOMPARE(view.at(0).toMap().value("action").toString(), QStringLiteral("Start"));
    QCOMPARE(view.at(0).toMap().value("functionName").toString(), scene->name());
}

void ShowCommandRecorder_Test::commandEditorQml_rendersTheModel()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    ui.view()->rootContext()->setContextProperty("showCommandRecorder", &recorder);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);
    button->requestUserStateChange(true);
    QTRY_COMPARE(recorder.track().count(), 1);

    // The production editor, loaded exactly as the Show Manager loads it.
    QQmlComponent component(ui.view()->engine(), QUrl(QStringLiteral("qrc:/ShowCommandList.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    std::unique_ptr<QQuickItem> panel(qobject_cast<QQuickItem *>(component.create()));
    QVERIFY(panel);
    panel->setParentItem(ui.view()->contentItem());
    panel->setWidth(400);
    panel->setHeight(300);

    QQuickItem *list = panel->findChild<QQuickItem *>(QStringLiteral("commandView"));
    QVERIFY(list);
    QCOMPARE(list->property("count").toInt(), 1);

    // Deleting through the editor's own model interface empties the view.
    QVERIFY(recorder.removeCommand(recorder.track().commands().at(0).id));
    QTRY_COMPARE(list->property("count").toInt(), 0);
}

void ShowCommandRecorder_Test::recordStop_finalizesExtentAtCaptureEnd()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, 10000);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);

    // A single Start ten seconds in, then twenty more seconds of capture.
    button->requestUserStateChange(true);
    QTRY_COMPARE(recorder.track().count(), 1);
    show->setExternalElapsedTime(30000);

    recorder.setRecording(false);

    // The take lasted until Record was switched off. A fixed tail after the
    // last command is not a duration, and it is not what gets persisted.
    QCOMPARE(show->commandTrack().extent(), quint32(30000));
    QCOMPARE(recorder.extent(), 30000);
}

void ShowCommandRecorder_Test::recordRebind_finalizesPreviousShowExtent()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *first = addRecordingShow(&doc, 5000);
    QVERIFY(first);
    Show *second = new Show(&doc);
    second->setName(QStringLiteral("Next song"));
    QVERIFY(doc.addFunction(second));

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(first->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);
    button->requestUserStateChange(true);
    QTRY_COMPARE(recorder.track().count(), 1);

    // The song changes while Record is still on: the take on the old Show is
    // closed at the point its clock had reached.
    first->setExternalElapsedTime(21000);
    recorder.setResolvedShow(second->id());

    QCOMPARE(first->commandTrack().extent(), quint32(21000));
    QCOMPARE(second->commandTrack().count(), 0);
    QCOMPARE(second->commandTrack().extent(), quint32(0));
}

void ShowCommandRecorder_Test::pendingSliderGesture_settlesIntoItsOwnShow()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    // No engine tick here: the gesture stays unexecuted on purpose.

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *first = addRecordingShow(&doc, 4000);
    QVERIFY(first);
    Show *second = new Show(&doc);
    second->setName(QStringLiteral("Next song"));
    QVERIFY(doc.addFunction(second));
    second->setSyncSource(ShowRunner::External);
    second->setExternalElapsedTime(900);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(first->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int sliderID = bridge.addSlider(frameID, QRect(10, 10, 60, 200),
                                          QStringLiteral("level"), QStringLiteral("Intensity"),
                                          Function::invalidId(), {});
    VCSlider *fader = qobject_cast<VCSlider *>(ui.vc()->widget(sliderID));
    QVERIFY(fader);
    fader->setSliderMode(VCSlider::Adjust);
    fader->setControlledFunction(scene->id());

    fader->requestUserValue(255);

    // Record goes off before the engine ever executed that movement. It was
    // accepted while recording, so it belongs to this take and nowhere else.
    recorder.setRecording(false);
    QCOMPARE(first->commandTrack().count(), 2);

    // Re-arming on the next song must not inherit the old gesture, even when
    // the engine finally gets around to executing it.
    recorder.setResolvedShow(second->id());
    recorder.setRecording(true);
    fader->writeDMX(doc.masterTimer(), QList<Universe *>());
    QCoreApplication::processEvents();

    QCOMPARE(second->commandTrack().count(), 0);
    QCOMPARE(first->commandTrack().count(), 2);
}

void ShowCommandRecorder_Test::sliderCoalescing_matchesNativeExecution_data()
{
    QTest::addColumn<QVariantList>("moves");
    QTest::addColumn<QVariantList>("actions");

    const QVariantList startThenValue{int(ShowCommandAction::Start),
                                      int(ShowCommandAction::SetIntensity)};

    // One movement on a stopped target: it starts and takes the value.
    QTest::newRow("up") << QVariantList{200} << startThenValue;

    // Up and back to zero before the engine ran: nothing was ever started, so
    // there is nothing to record. Recording a Start here would make replay
    // leave the target running.
    QTest::newRow("up-then-zero") << QVariantList{200, 0} << QVariantList();

    // Zero on an already stopped target is not a Stop.
    QTest::newRow("zero-on-stopped") << QVariantList{0} << QVariantList();

    // Zero first, then up: the take has to follow the order of the gestures.
    QTest::newRow("zero-then-up") << QVariantList{0, 200} << startThenValue;

    // A repeated identical value changes nothing natively and records nothing
    // extra: the engine coalesces it away, and so does the take.
    QTest::newRow("duplicate") << QVariantList{200, 200} << startThenValue;
}

void ShowCommandRecorder_Test::sliderCoalescing_matchesNativeExecution()
{
    QFETCH(QVariantList, moves);
    QFETCH(QVariantList, actions);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int sliderID = bridge.addSlider(frameID, QRect(10, 10, 60, 200),
                                          QStringLiteral("level"), QStringLiteral("Intensity"),
                                          Function::invalidId(), {});
    VCSlider *fader = qobject_cast<VCSlider *>(ui.vc()->widget(sliderID));
    QVERIFY(fader);
    fader->setSliderMode(VCSlider::Adjust);
    fader->setControlledFunction(scene->id());

    for (const QVariant &move : moves)
        fader->requestUserValue(move.toInt());

    // One engine execution, exactly as the 25 Hz tick would do it.
    fader->writeDMX(doc.masterTimer(), QList<Universe *>());
    QCoreApplication::processEvents();

    const QVector<ShowCommand> &stored = recorder.track().commands();
    QCOMPARE(stored.count(), actions.count());
    for (int i = 0; i < actions.count(); i++)
        QCOMPARE(int(stored.at(i).action), actions.at(i).toInt());

    // Live behaviour follows the engine, not the take.
    QCOMPARE(scene->stopped(), moves.last().toInt() == 0);
}

void ShowCommandRecorder_Test::sliderIntensity_capturesEffectiveValueAtInput()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int sliderID = bridge.addSlider(frameID, QRect(10, 10, 60, 200),
                                          QStringLiteral("level"), QStringLiteral("Intensity"),
                                          Function::invalidId(), {});
    VCSlider *fader = qobject_cast<VCSlider *>(ui.vc()->widget(sliderID));
    QVERIFY(fader);
    fader->setSliderMode(VCSlider::Adjust);
    fader->setControlledFunction(scene->id());

    // Half submaster: what the target actually receives is half of the fader.
    fader->adjustIntensity(0.5);
    fader->requestUserValue(UCHAR_MAX);

    // Both the widget intensity and the range can be changed by anything at
    // any moment, so the effective value has to be captured with the gesture.
    fader->adjustIntensity(1.0);
    fader->setRangeHighLimit(50);

    fader->writeDMX(doc.masterTimer(), QList<Universe *>());
    QCoreApplication::processEvents();

    const QVector<ShowCommand> &stored = recorder.track().commands();
    QCOMPARE(stored.count(), qsizetype(2));
    QCOMPARE(int(stored.at(1).action), int(ShowCommandAction::SetIntensity));
    QCOMPARE(stored.at(1).intensity, 0.5);
}

void ShowCommandRecorder_Test::relativeInput_keepsItsProvenance_data()
{
    QTest::addColumn<QString>("pluginName");
    QTest::addColumn<int>("expectedCommands");

    QTest::newRow("midi-relative") << QStringLiteral("MIDI") << 1;
    QTest::newRow("artnet-relative") << QStringLiteral("ArtNet") << 0;
}

void ShowCommandRecorder_Test::relativeInput_keepsItsProvenance()
{
    QFETCH(QString, pluginName);
    QFETCH(int, expectedCommands);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    QVERIFY(patchInputPlugin(&doc, pluginName, kSourceUniverse));

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    VCPage *page = ui.vc()->page(0);
    QVERIFY(page);
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);

    // An encoder is processed by the source itself, which then emits its own
    // value. That detour must not lose where the movement came from.
    QSharedPointer<QLCInputSource> source(new QLCInputSource(kSourceUniverse, kSourceChannel));
    source->setID(0);
    source->setWorkingMode(QLCInputSource::Encoder);
    source->setSensitivity(20);
    button->addInputSource(source);
    QVERIFY(QObject::connect(source.data(), SIGNAL(inputValueChanged(quint32,quint32,uchar)),
                             button, SLOT(slotInputSourceValueChanged(quint32,quint32,uchar))));
    page->mapInputSource(source, button);
    QVERIFY(source->needsUpdate());

    QVERIFY(deliverMappedInput(ui.vc(), UCHAR_MAX));

    QCOMPARE(button->state(), VCButton::Active);
    QTRY_VERIFY(scene->isRunning());

    const QVector<ShowCommand> &stored = recorder.track().commands();
    QTRY_COMPARE(stored.count(), qsizetype(expectedCommands));
    QTest::qWait(120);
    QCOMPARE(stored.count(), qsizetype(expectedCommands));
}

void ShowCommandRecorder_Test::rewoundTake_replaysItsOwnRecordedCommand()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);

    Show *show = new Show(&doc);
    show->setName(QStringLiteral("Recording show"));
    QVERIFY(doc.addFunction(show));

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);

    // A real take on a really running Show.
    show->start(doc.masterTimer(), FunctionParent::master());
    QTRY_VERIFY(show->isRunning());
    QTRY_VERIFY(show->commandPosition() > 200);

    button->requestUserStateChange(true);
    QTRY_COMPARE(recorder.track().count(), 1);
    const quint32 recordedTime = recorder.track().commands().at(0).time;
    QVERIFY(recordedTime > 0);
    QTRY_VERIFY(scene->isRunning());

    // The operator stops the target by hand. Manual interference is allowed to
    // change the output; nothing corrects it and nothing is recorded for it.
    scene->stop(FunctionParent::master());
    QTRY_VERIFY(!scene->isRunning());
    QCOMPARE(recorder.track().count(), 1);

    // The song loops back before that command.
    show->requestSeek(0);
    QTRY_VERIFY(show->commandPosition() < recordedTime);

    // Something else is recorded right after the loop. Publishing it must not
    // drag the previous traversal's live-event identities along, which is what
    // would keep the earlier command silenced.
    Scene *other = new Scene(&doc);
    other->setName(QStringLiteral("Second target"));
    QVERIFY(doc.addFunction(other));
    const int secondID = bridge.addButton(frameID, QRect(100, 10, 80, 40), other->id(),
                                          QStringLiteral("Second"), QStringLiteral("toggle"));
    VCButton *secondButton = qobject_cast<VCButton *>(ui.vc()->widget(secondID));
    QVERIFY(secondButton);
    secondButton->requestUserStateChange(true);
    QTRY_COMPARE(recorder.track().count(), 2);
    QVERIFY(recorder.track().commands().at(0).time < recordedTime);

    // A later traversal is a new traversal: the command recorded live in the
    // previous one must execute now instead of staying silenced by its own
    // stale live-event identity.
    QTRY_VERIFY_WITH_TIMEOUT(show->commandPosition() > recordedTime, 10000);
    QTRY_VERIFY(scene->isRunning());

    show->stop(FunctionParent::master());
    show->stopAndWait();
}

void ShowCommandRecorder_Test::commandEditorQml_editsTimeAndIntensity()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    ui.view()->rootContext()->setContextProperty("showCommandRecorder", &recorder);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int sliderID = bridge.addSlider(frameID, QRect(10, 10, 60, 200),
                                          QStringLiteral("level"), QStringLiteral("Intensity"),
                                          Function::invalidId(), {});
    VCSlider *fader = qobject_cast<VCSlider *>(ui.vc()->widget(sliderID));
    QVERIFY(fader);
    fader->setSliderMode(VCSlider::Adjust);
    fader->setControlledFunction(scene->id());
    fader->requestUserValue(255);
    fader->writeDMX(doc.masterTimer(), QList<Universe *>());
    QCoreApplication::processEvents();
    QCOMPARE(recorder.track().count(), 2);

    // Editing happens after the take, with Record off.
    recorder.setRecording(false);

    QQuickView *view = ui.view();
    view->resize(600, 400);
    QQmlComponent component(view->engine(), QUrl(QStringLiteral("qrc:/ShowCommandList.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    std::unique_ptr<QQuickItem> panel(qobject_cast<QQuickItem *>(component.create()));
    QVERIFY(panel);
    panel->setWidth(600);
    panel->setHeight(400);
    panel->setParentItem(view->contentItem());
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));

    QQuickItem *intensityEdit = nullptr;
    QTRY_VERIFY2((intensityEdit = findVisualItem(panel.get(),
                      QStringLiteral("commandIntensityEdit"))) != nullptr,
                 "the recorded value needs a real control, not a label");
    QMetaObject::invokeMethod(intensityEdit, "selectAndFocus");
    typeInto(view, QStringLiteral("40"));
    QCoreApplication::processEvents();

    const ShowCommand &value = recorder.track().commands().at(1);
    const quint32 valueId = value.id;
    QCOMPARE(int(value.action), int(ShowCommandAction::SetIntensity));
    QCOMPARE(value.intensity, 0.4);
    QCOMPARE(show->commandTrack().commands().at(1).intensity, 0.4);

    QQuickItem *timeEdit = findVisualItem(panel.get(), QStringLiteral("commandTimeEdit"));
    QVERIFY2(timeEdit, "the recorded time needs a real control, not a label");
    const quint32 firstId = recorder.track().commands().at(0).id;
    QMetaObject::invokeMethod(timeEdit, "selectAndFocus");
    typeInto(view, QStringLiteral("7500"));
    QCoreApplication::processEvents();

    // Moving an event in time reorders the take, so follow it by its id.
    const int moved = recorder.track().indexOfId(firstId);
    QVERIFY(moved >= 0);
    QCOMPARE(recorder.track().commands().at(moved).time, quint32(7500));
    QCOMPARE(show->commandTrack().commands().at(moved).time, quint32(7500));

    // Selecting another Show swaps what the editor edits; the old take keeps
    // the edits that were made to it.
    Show *second = new Show(&doc);
    second->setName(QStringLiteral("Other show"));
    QVERIFY(doc.addFunction(second));
    recorder.setResolvedShow(second->id());
    QCOMPARE(recorder.track().count(), 0);
    const int valueIndex = show->commandTrack().indexOfId(valueId);
    QVERIFY(valueIndex >= 0);
    QCOMPARE(show->commandTrack().commands().at(valueIndex).intensity, 0.4);
}

void ShowCommandRecorder_Test::editor_typedActionAndTargetEdits()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Scene *other = new Scene(&doc);
    other->setName(QStringLiteral("Other target"));
    QVERIFY(doc.addFunction(other));
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    recorder.setResolvedShow(show->id());
    recorder.setRecording(true);

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);
    button->requestUserStateChange(true);
    QTRY_COMPARE(recorder.track().count(), 1);
    recorder.setRecording(false);

    const quint32 id = recorder.track().commands().at(0).id;

    // An editor that shows action and target has to let the user change them.
    QVERIFY(recorder.setCommandAction(id, QStringLiteral("Stop")));
    QCOMPARE(int(recorder.track().commands().at(0).action), int(ShowCommandAction::Stop));
    QCOMPARE(int(show->commandTrack().commands().at(0).action), int(ShowCommandAction::Stop));

    QVERIFY(recorder.setCommandTarget(id, other->id()));
    QCOMPARE(recorder.track().commands().at(0).functionId, other->id());
    QCOMPARE(show->commandTrack().commands().at(0).functionId, other->id());

    // Rejected edits leave the take alone and say why.
    QVERIFY(!recorder.setCommandAction(id, QStringLiteral("Explode")));
    QVERIFY(!recorder.lastError().isEmpty());
    QCOMPARE(int(recorder.track().commands().at(0).action), int(ShowCommandAction::Stop));

    QVERIFY(!recorder.setCommandTarget(id, 12345));
    QCOMPARE(recorder.track().commands().at(0).functionId, other->id());

    // A Show that starts itself is not a target.
    QVERIFY(!recorder.setCommandTarget(id, show->id()));
    QCOMPARE(recorder.track().commands().at(0).functionId, other->id());
    QCOMPARE(show->commandTrack().commands().at(0).functionId, other->id());
}

namespace
{
/** A bound recorder with a slider and a button on the current Show */
struct RecordingRig
{
    RecordingRig(Doc *doc, UiFixture *ui, ShowCommandRecorder *recorder, Show *show, Scene *target)
        : bridge(doc, ui->vc())
    {
        recorder->setResolvedShow(show->id());
        recorder->setRecording(true);
        frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
        const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), target->id(),
                                              QStringLiteral("Target"), QStringLiteral("toggle"));
        button = qobject_cast<VCButton *>(ui->vc()->widget(buttonID));
        const int sliderID = bridge.addSlider(frameID, QRect(100, 10, 60, 200),
                                              QStringLiteral("level"), QStringLiteral("Intensity"),
                                              Function::invalidId(), {});
        fader = qobject_cast<VCSlider *>(ui->vc()->widget(sliderID));
        if (fader != nullptr)
        {
            fader->setSliderMode(VCSlider::Adjust);
            fader->setControlledFunction(target->id());
        }
    }

    VCBridgeV5 bridge;
    int frameID = -1;
    VCButton *button = nullptr;
    VCSlider *fader = nullptr;
};
}

void ShowCommandRecorder_Test::finalizeExtent_neverShrinksAuthoredExtent_data()
{
    QTest::addColumn<quint32>("authoredExtent");
    QTest::addColumn<bool>("record");
    QTest::addColumn<quint32>("expectedExtent");

    // A Show that already plays to a minute keeps doing so after a short take.
    QTest::newRow("short take on a long show") << 60000u << true << 60000u;
    QTest::newRow("no input at all") << 60000u << false << 60000u;

    // A fresh Show gets the duration that was actually captured.
    QTest::newRow("fresh show") << 0u << true << 30000u;
}

void ShowCommandRecorder_Test::finalizeExtent_neverShrinksAuthoredExtent()
{
    QFETCH(quint32, authoredExtent);
    QFETCH(bool, record);
    QFETCH(quint32, expectedExtent);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, 10000);
    QVERIFY(show);

    if (authoredExtent > 0)
    {
        ShowCommandTrack authored;
        QVERIFY(authored.setExtent(authoredExtent, nullptr));
        QVERIFY(show->setCommandTrack(authored, QSet<quint32>(), nullptr));
    }

    ShowCommandRecorder recorder(&doc);
    RecordingRig rig(&doc, &ui, &recorder, show, scene);
    QVERIFY(rig.button);

    if (record)
        rig.button->requestUserStateChange(true);

    show->setExternalElapsedTime(authoredExtent > 0 ? 12000 : 30000);
    recorder.setRecording(false);

    QCOMPARE(show->commandTrack().extent(), expectedExtent);
    QCOMPARE(recorder.extent(), int(expectedExtent));
}

void ShowCommandRecorder_Test::newTake_preservesCurrentlyEditedExtent_data()
{
    QTest::addColumn<bool>("rewindBeforeArming");
    QTest::newRow("edit-selected-show-before-recording") << false;
    QTest::newRow("off-mode-playback-does-not-extend-recording") << true;
}

void ShowCommandRecorder_Test::newTake_preservesCurrentlyEditedExtent()
{
    QFETCH(bool, rewindBeforeArming);
    Doc doc(nullptr, 1);
    Show *show = addRecordingShow(&doc, 10000);
    QVERIFY(show);
    ShowCommandRecorder recorder(&doc);
    QVERIFY(recorder.setResolvedShow(show->id()));
    QVERIFY(recorder.setExtent(60000));
    if (rewindBeforeArming)
    {
        show->setExternalElapsedTime(70000);
        show->setExternalElapsedTime(10000);
    }

    QVERIFY(recorder.setRecording(true));
    show->setExternalElapsedTime(12000);
    QVERIFY(recorder.setRecording(false));

    QCOMPARE(show->commandTrack().extent(), quint32(60000));
    QCOMPARE(recorder.extent(), 60000);
}

void ShowCommandRecorder_Test::loopSegment_keepsDepartureEnd()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, 30000);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    RecordingRig rig(&doc, &ui, &recorder, show, scene);
    QVERIFY(rig.button);

    rig.button->requestUserStateChange(true);
    QTRY_COMPARE(recorder.track().count(), 1);
    const quint64 takeBeforeLoop = recorder.takeId();

    // The song loops. Nobody touches a control afterwards.
    show->setExternalElapsedTime(5000);
    QVERIFY(recorder.takeId() != takeBeforeLoop);

    show->setExternalElapsedTime(8000);
    recorder.setRecording(false);

    // The take reached thirty seconds before the loop. Stopping eight seconds
    // into the second pass must not throw that away.
    QCOMPARE(show->commandTrack().extent(), quint32(30000));
}

void ShowCommandRecorder_Test::loopSegment_settlesPendingGestureBeforeEpoch()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, 20000);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    RecordingRig rig(&doc, &ui, &recorder, show, scene);
    QVERIFY(rig.fader);

    rig.fader->requestUserValue(200);

    // The song loops before the engine executed that movement. It happened at
    // twenty seconds and belongs there, not to the new pass.
    show->setExternalElapsedTime(1000);

    QCOMPARE(recorder.track().count(), 2);
    QCOMPARE(recorder.track().commands().at(0).time, quint32(20000));
    QCOMPARE(recorder.track().commands().at(1).time, quint32(20000));

    // The engine finally runs: the settled gesture must not be recorded twice.
    rig.fader->writeDMX(doc.masterTimer(), QList<Universe *>());
    QCoreApplication::processEvents();
    QCOMPARE(recorder.track().count(), 2);
}

void ShowCommandRecorder_Test::lateInputTimestamp_isNotAClockRewind()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, 10000);
    QVERIFY(show);

    Scene *faderTarget = new Scene(&doc);
    faderTarget->setName(QStringLiteral("Fader target"));
    QVERIFY(doc.addFunction(faderTarget));

    ShowCommandRecorder recorder(&doc);
    RecordingRig rig(&doc, &ui, &recorder, show, scene);
    QVERIFY(rig.fader);
    QVERIFY(rig.button);
    rig.fader->setControlledFunction(faderTarget->id());

    // A fader movement at ten seconds that the engine has not executed yet.
    rig.fader->requestUserValue(200);

    // The clock moves on and a button is pressed at twelve seconds.
    show->setExternalElapsedTime(12000);
    const quint64 takeAfterButton = recorder.takeId();
    rig.button->requestUserStateChange(true);
    QCOMPARE(recorder.track().count(), 1);
    QCOMPARE(recorder.track().commands().at(0).time, quint32(12000));

    // Now the older fader gesture arrives. An input carrying an older
    // timestamp is late, not a rewind: it must not start a new segment.
    rig.fader->writeDMX(doc.masterTimer(), QList<Universe *>());
    QCoreApplication::processEvents();

    QCOMPARE(recorder.takeId(), takeAfterButton);
    QCOMPARE(recorder.track().count(), 3);
    QCOMPARE(recorder.track().commands().at(0).time, quint32(10000));
}

void ShowCommandRecorder_Test::resolvedGesture_survivesDisarmBeforeDelivery()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *first = addRecordingShow(&doc, 4000);
    QVERIFY(first);
    Show *second = new Show(&doc);
    second->setName(QStringLiteral("Next song"));
    QVERIFY(doc.addFunction(second));
    second->setSyncSource(ShowRunner::External);
    second->setExternalElapsedTime(700);

    ShowCommandRecorder recorder(&doc);
    RecordingRig rig(&doc, &ui, &recorder, second == nullptr ? nullptr : first, scene);
    QVERIFY(rig.fader);

    rig.fader->requestUserValue(255);

    // The engine executes the movement, so the commands are resolved, but the
    // main thread has not been given them yet.
    rig.fader->writeDMX(doc.masterTimer(), QList<Universe *>());

    // Record goes off before that delivery happens.
    recorder.setRecording(false);
    QCOMPARE(first->commandTrack().count(), 2);

    // The pending delivery must not arrive in the next take, and must not
    // duplicate what the finalization already drained.
    recorder.setResolvedShow(second->id());
    recorder.setRecording(true);
    QCoreApplication::processEvents();

    QCOMPARE(second->commandTrack().count(), 0);
    QCOMPARE(first->commandTrack().count(), 2);
}

void ShowCommandRecorder_Test::invalidEdits_areRejectedNotClamped_data()
{
    QTest::addColumn<QString>("edit");
    QTest::addColumn<int>("value");

    QTest::newRow("negative extent") << QStringLiteral("extent") << -1;
    QTest::newRow("very negative extent") << QStringLiteral("extent") << -60000;
    QTest::newRow("negative time") << QStringLiteral("retime") << -1;
    QTest::newRow("very negative time") << QStringLiteral("retime") << -5000;
}

void ShowCommandRecorder_Test::invalidEdits_areRejectedNotClamped()
{
    QFETCH(QString, edit);
    QFETCH(int, value);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, kRecordPosition);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    RecordingRig rig(&doc, &ui, &recorder, show, scene);
    QVERIFY(rig.button);
    rig.button->requestUserStateChange(true);
    QCOMPARE(recorder.track().count(), 1);
    recorder.setRecording(false);

    const quint32 id = recorder.track().commands().at(0).id;
    const quint32 timeBefore = recorder.track().commands().at(0).time;
    const quint32 extentBefore = recorder.track().extent();

    // A negative number is not a time. Quietly turning it into zero is a
    // silent success on invalid input.
    if (edit == QStringLiteral("extent"))
        QVERIFY(!recorder.setExtent(value));
    else
        QVERIFY(!recorder.retimeCommand(id, value));

    QVERIFY(!recorder.lastError().isEmpty());
    QCOMPARE(recorder.track().commands().at(0).time, timeBefore);
    QCOMPARE(recorder.track().extent(), extentBefore);
    QCOMPARE(show->commandTrack().extent(), extentBefore);
}

void ShowCommandRecorder_Test::failedFinalization_keepsRecording()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = addRecordingShow(&doc, 9000);
    QVERIFY(show);

    ShowCommandRecorder recorder(&doc);
    RecordingRig rig(&doc, &ui, &recorder, show, scene);
    QVERIFY(rig.button);
    rig.button->requestUserStateChange(true);
    QCOMPARE(recorder.track().count(), 1);

    // The recorded target disappears, so the take can no longer be published.
    QVERIFY(doc.deleteFunction(scene->id()));
    show->setExternalElapsedTime(15000);

    QVERIFY(!recorder.setRecording(false));

    // Failing to close the take must not throw it away or pretend it stopped.
    QVERIFY(recorder.isRecording());
    QVERIFY(recorder.isAuthoring());
    QVERIFY(!recorder.lastError().isEmpty());
    QCOMPARE(recorder.track().count(), 1);
}

QTEST_MAIN(ShowCommandRecorder_Test)
