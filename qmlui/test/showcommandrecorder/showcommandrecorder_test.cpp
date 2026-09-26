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
#include "mastertimer.h"
#undef private

#include <QtTest>
#include <QAccessible>
#include <QBuffer>
#include <QFileOpenEvent>
#include <QMessageBox>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickView>
#include <QTranslator>
#include <functional>
#include <memory>
#include <thread>
// the catch-up lag is read from the runner itself
#define private public
#define protected public
#include "show.h"
#include "showmanager.h"
#include "showrunner.h"
#undef protected
#undef private

#include "app.h"
#include "collection.h"
#include "efx.h"
#include "contextmanager.h"
#include "djmanager.h"
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
#include "showeventlog.h"
#include "showeventmodel.h"
#include "showfunction.h"
#include "showrunner.h"
#include "track.h"
#define private public
#include "tardis.h"
#undef private
#include "vcbridgev5.h"
#include "vcbutton.h"
#include "vcpage.h"
#include "vcslider.h"
#include "vcsoloframe.h"
#include "vdjbridge.h"
#include "performfsm.h"
#include "virtualconsole.h"
#include "workspacebridgev5.h"
#include "tools/tool_registry.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

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

/** A stand-in feedback line: records what the production feedback path sends */
class FeedbackPlugin : public QLCIOPlugin
{
public:
    explicit FeedbackPlugin(QVector<int> *sent)
        : m_sent(sent)
    {
    }

    void init() override {}
    QString name() const override { return QStringLiteral("Test feedback"); }
    int capabilities() const override { return QLCIOPlugin::Output | QLCIOPlugin::Feedback; }
    QString pluginInfo() const override { return name(); }
    QStringList outputs() override { return QStringList() << QStringLiteral("Feedback line"); }
    QString outputInfo(quint32) override { return name(); }
    bool openOutput(quint32, quint32) override { return true; }
    void closeOutput(quint32, quint32) override {}
    void writeUniverse(quint32, quint32, const QByteArray &, bool) override {}
    void sendFeedBack(quint32, quint32, quint32, uchar value, const QVariant &) override
    {
        m_sent->append(value);
    }

private:
    QVector<int> *m_sent;
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

QQuickItem *renderSliderItem(QQuickView *view, VCSlider *slider, const QString &resource)
{
    QQmlComponent component(view->engine(), QUrl(resource));
    if (component.isError())
    {
        qWarning() << component.errors();
        return nullptr;
    }
    QQuickItem *item = qobject_cast<QQuickItem *>(component.create());
    if (item == nullptr)
        return nullptr;
    item->setParentItem(view->contentItem());
    item->setProperty("sliderObj", QVariant::fromValue(slider));
    return item;
}

/** The visible descendant whose property holds value */
QQuickItem *findVisualItemWith(QQuickItem *root, const char *property, const QVariant &value)
{
    if (root == nullptr)
        return nullptr;
    if (root->isVisible() && root->property(property) == value)
        return root;
    for (QQuickItem *child : root->childItems())
    {
        if (QQuickItem *found = findVisualItemWith(child, property, value))
            return found;
    }
    return nullptr;
}

/** What the GUI learns of native playback: the queued running()/stopped() of
 *  watched functions and the receipts of watched Shows, in delivery order */
class NativeLog : public QObject
{
public:
    void watch(Function *function)
    {
        connect(function, &Function::running, this,
                [this](quint32 id) { m_events.append(qMakePair(id, true)); }, Qt::QueuedConnection);
        connect(function, qOverload<quint32>(&Function::stopped), this,
                [this](quint32 id) { m_events.append(qMakePair(id, false)); }, Qt::QueuedConnection);
    }

    void watchReceipts(Show *show)
    {
        connect(show, &Show::functionReceiptReady, this, [this]() { m_receipts++; },
                Qt::QueuedConnection);
    }

    bool live(quint32 id) const
    {
        for (int i = m_events.count() - 1; i >= 0; i--)
        {
            if (m_events.at(i).first == id)
                return m_events.at(i).second;
        }
        return false;
    }

    int starts(quint32 id) const { return m_events.count(qMakePair(id, true)); }
    int stops(quint32 id) const { return m_events.count(qMakePair(id, false)); }
    /** Delivery position of the first such event, -1 when none arrived */
    int indexOf(quint32 id, bool running) const { return m_events.indexOf(qMakePair(id, running)); }
    int receipts() const { return m_receipts; }

private:
    QVector<QPair<quint32, bool>> m_events;
    int m_receipts = 0;
};

/** One MasterTimer tick on this thread, then the GUI work it queued. Nothing
 *  runs concurrently, so native state is read on the thread that owns it. */
void tickAndDeliver(Doc *doc, int ticks = 1)
{
    for (int i = 0; i < ticks; i++)
    {
        doc->masterTimer()->timerTick();
        QCoreApplication::processEvents();
    }
}

VCButton *addToggle(VCBridgeV5 &bridge, VirtualConsole *vc, int frameID, quint32 functionId,
                    int x, const QString &caption)
{
    return qobject_cast<VCButton *>(vc->widget(
            bridge.addButton(frameID, QRect(x, 10, 80, 40), functionId, caption,
                             QStringLiteral("toggle"))));
}

VCSlider *addAdjustSlider(VCBridgeV5 &bridge, VirtualConsole *vc, int frameID, quint32 functionId)
{
    VCSlider *slider = qobject_cast<VCSlider *>(vc->widget(
            bridge.addSlider(frameID, QRect(200, 10, 60, 200), QStringLiteral("level"),
                             QStringLiteral("Fader"), Function::invalidId(), {})));
    if (slider == nullptr)
        return nullptr;
    slider->setSliderMode(VCSlider::Adjust);
    slider->setControlledFunction(functionId);
    return slider;
}

/** A Collection of the recorded target and a sibling that keeps it alive */
struct LookRig
{
    Scene *child = nullptr;
    Scene *sibling = nullptr;
    Collection *collection = nullptr;
    VCButton *lookButton = nullptr;
    VCButton *childButton = nullptr;
};

LookRig addLookRig(Doc *doc, VCBridgeV5 &bridge, VirtualConsole *vc)
{
    LookRig rig;
    rig.child = addTarget(doc);
    if (rig.child == nullptr)
        return rig;
    rig.sibling = new Scene(doc);
    rig.sibling->setName(QStringLiteral("Sibling"));
    rig.sibling->setValue(SceneValue(rig.child->values().first().fxi, 1, 120));
    doc->addFunction(rig.sibling);
    rig.collection = new Collection(doc);
    rig.collection->setName(QStringLiteral("Look"));
    doc->addFunction(rig.collection);
    rig.collection->addFunction(rig.child->id());
    rig.collection->addFunction(rig.sibling->id());

    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    rig.lookButton = addToggle(bridge, vc, frameID, rig.collection->id(), 10, QStringLiteral("Look"));
    rig.childButton = addToggle(bridge, vc, frameID, rig.child->id(), 100, QStringLiteral("Child"));
    return rig;
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
        QCOMPARE(int(stored.at(0).action), int(ShowCommandAction::SetButtonState));
        QCOMPARE(stored.at(0).controlId, button->recordingId());
        QVERIFY(stored.at(0).on);
        QCOMPARE(stored.at(0).time, kRecordPosition);
    }
}

void ShowCommandRecorder_Test::onlyUserInputAuthors_data()
{
    QTest::addColumn<QString>("route");
    QTest::addColumn<QVariantList>("actions");
    QTest::addColumn<QVariantList>("intensities");

    const QVariantList none;
    const QVariantList on{int(ShowCommandAction::SetButtonState)};
    const QVariantList position{int(ShowCommandAction::SetSliderPosition)};

    // Real user intent, each through its own production ingress.
    QTest::newRow("button-pointer") << QStringLiteral("button-pointer")
                                    << on << QVariantList{1.0};
    QTest::newRow("button-keyboard") << QStringLiteral("button-keyboard")
                                     << on << QVariantList{1.0};
    QTest::newRow("slider-pointer") << QStringLiteral("slider-pointer")
                                    << position << QVariantList{1.0};

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
        if (cmd.action == ShowCommandAction::SetSliderPosition)
        {
            QCOMPARE(cmd.controlId, fader->recordingId());
            QCOMPARE(cmd.position, intensities.at(i).toReal());
        }
        else
        {
            QCOMPARE(cmd.controlId, button->recordingId());
            QCOMPARE(cmd.on, intensities.at(i).toReal() > 0);
        }
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
    QCOMPARE(stored.count(), qsizetype(1));
    QTest::qWait(120);
    QCOMPARE(stored.count(), qsizetype(1));

    // The user's own position is what gets recorded. The audio value that
    // reached the engine authors nothing at all.
    QCOMPARE(int(stored.at(0).action), int(ShowCommandAction::SetSliderPosition));
    QCOMPARE(stored.at(0).controlId, fader->recordingId());
    QCOMPARE(stored.at(0).position, qreal(200) / qreal(UCHAR_MAX));
    QCOMPARE(stored.at(0).time, kRecordPosition);
}


void ShowCommandRecorder_Test::monitoringToggle_firstUserClickAcquires_data()
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

void ShowCommandRecorder_Test::monitoringToggle_firstUserClickAcquires()
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

    // The native click acquires the button's own activation, recording or not
    const QVector<ShowCommand> &stored = recorder.track().commands();
    QCOMPARE(button->state(), VCButton::Active);
    QVERIFY(scene->isRunning());
    if (recording)
    {
        QTRY_COMPARE(stored.count(), qsizetype(1));
        QCOMPARE(int(stored.at(0).action), int(ShowCommandAction::SetButtonState));
        QCOMPARE(stored.at(0).controlId, button->recordingId());
        QVERIFY(stored.at(0).on);
    }
    else
    {
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
    const QUuid control = button->recordingId();
    QVERIFY(!control.isNull());

    // Reassigning the widget afterwards must not rewrite history: commands
    // address the control's identity; its current binding is what replay drives.
    button->setFunctionID(other->id());

    QCOMPARE(stored.count(), qsizetype(1));
    QCOMPARE(stored.at(0).id, recordedId);
    QCOMPARE(stored.at(0).controlId, control);
    QCOMPARE(stored.at(0).functionId, ShowCommand::InvalidId);
    QCOMPARE(show->commandTrack().commands().at(0).controlId, control);
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
    QCOMPARE(int(stored.at(0).action), int(ShowCommandAction::SetButtonState));
    QCOMPARE(int(stored.at(1).action), int(ShowCommandAction::SetButtonState));
    QVERIFY(stored.at(0).on);
    QVERIFY(!stored.at(1).on);

    // Another Show resolving now is an automatic handover: the take on the
    // first Show closes and recording rearms on the second.
    Show *second = new Show(&doc);
    second->setName(QStringLiteral("Other show"));
    QVERIFY(doc.addFunction(second));
    recorder.setResolvedShow(second->id());
    QCOMPARE(recorder.phaseValue(), int(ShowRecordPhase::Bound));
    QCOMPARE(recorder.boundShowId(), second->id());
    QCOMPARE(second->commandTrack().count(), 0);
    QCOMPARE(show->commandTrack().count(), 2);
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

    // legacy function commands, which VC input no longer authors
    ShowCommandInput start;
    start.origin = ShowCommandOrigin::Pointer;
    start.action = ShowCommandAction::Start;
    start.functionId = scene->id();
    ShowCommandInput value = start;
    value.action = ShowCommandAction::SetIntensity;
    value.intensity = 128.0 / 255.0;
    QVERIFY(recorder.submitUserInput(QVector<ShowCommandInput>{start, value}));

    const QVector<ShowCommand> &stored = recorder.track().commands();
    QCOMPARE(stored.count(), qsizetype(2));
    const quint32 valueId = stored.at(1).id;
    QCOMPARE(int(stored.at(1).action), int(ShowCommandAction::SetIntensity));
    // editing happens after the take, with REC off
    QVERIFY(recorder.setRecording(false));

    // Editing goes through the model and reaches the Show, whose end follows it.
    QVERIFY(recorder.retimeCommand(show->id(), valueId, 9000));
    QCOMPARE(recorder.track().commands().at(1).time, quint32(9000));
    QCOMPARE(show->commandTrack().commands().at(1).time, quint32(9000));
    QCOMPARE(show->commandTrack().extent(), quint32(9000));

    QVERIFY(recorder.setCommandValue(show->id(), valueId, 0.25));
    QCOMPARE(recorder.track().commands().at(1).intensity, 0.25);

    // A rejected edit changes nothing and says why.
    QVERIFY(!recorder.setCommandValue(show->id(), valueId, 4.2));
    QVERIFY(!recorder.lastError().isEmpty());
    QCOMPARE(recorder.track().commands().at(1).intensity, 0.25);
    QVERIFY(!recorder.removeCommands(show->id(), QVariantList{ShowCommand::InvalidId}));
    QCOMPARE(recorder.track().count(), 2);

    QVERIFY(recorder.removeCommands(show->id(), QVariantList{valueId}));
    QCOMPARE(recorder.track().count(), 1);
    QCOMPARE(show->commandTrack().count(), 1);
    QCOMPARE(show->commandTrack().extent(), kRecordPosition);
    QVERIFY(recorder.lastError().isEmpty());

    // The editable view the QML list binds to mirrors the model.
    const QVariantList view = recorder.commands();
    QCOMPARE(view.count(), 1);
    QCOMPARE(view.at(0).toMap().value("action").toString(), QStringLiteral("Start"));
    QCOMPARE(view.at(0).toMap().value("functionName").toString(), scene->name());
}

void ShowCommandRecorder_Test::recordStop_finalizesExtentAtLastItem()
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

    // The saved end is the last retained item. Waiting with Record armed kept
    // the Show running, but it is not a duration and it is not persisted.
    QCOMPARE(show->commandTrack().extent(), quint32(10000));
    QCOMPARE(recorder.extent(), 10000);
}

void ShowCommandRecorder_Test::recordRebind_finalizesPreviousShowAtLastItem()
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
    // closed at its last retained item, not where its clock had reached.
    first->setExternalElapsedTime(21000);
    recorder.setResolvedShow(second->id());

    QCOMPARE(first->commandTrack().extent(), quint32(5000));
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
    QCOMPARE(first->commandTrack().count(), 1);

    // Re-arming on the next song must not inherit the old gesture, even when
    // the engine finally gets around to executing it.
    recorder.setResolvedShow(second->id());
    recorder.setRecording(true);
    fader->writeDMX(doc.masterTimer(), QList<Universe *>());
    QCoreApplication::processEvents();

    QCOMPARE(second->commandTrack().count(), 0);
    QCOMPARE(first->commandTrack().count(), 1);
}

void ShowCommandRecorder_Test::sliderPosition_capturesAcceptedRangeAtInput()
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

    // Half submaster: the target receives half of the fader, which is not
    // the position the operator set.
    fader->adjustIntensity(0.5);
    fader->setRangeHighLimit(200);
    fader->requestUserValue(150);

    // The range can be changed by anything at any moment, so the position is
    // taken from the range the value was accepted in.
    fader->adjustIntensity(1.0);
    fader->setRangeHighLimit(50);

    fader->writeDMX(doc.masterTimer(), QList<Universe *>());
    QCoreApplication::processEvents();

    const QVector<ShowCommand> &stored = recorder.track().commands();
    QCOMPARE(stored.count(), qsizetype(1));
    QCOMPARE(int(stored.at(0).action), int(ShowCommandAction::SetSliderPosition));
    QCOMPARE(stored.at(0).position, 0.75);
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
    // the recorded button state replays through the controls
    recorder.setVirtualConsole(ui.vc());
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

    // a legacy function command, which VC input no longer authors
    ShowCommandInput start;
    start.origin = ShowCommandOrigin::Pointer;
    start.action = ShowCommandAction::Start;
    start.functionId = scene->id();
    QVERIFY(recorder.submitUserInput(start));
    QCOMPARE(recorder.track().count(), 1);
    recorder.setRecording(false);

    const quint32 id = recorder.track().commands().at(0).id;

    // An editor that shows action and target has to let the user change them.
    QVERIFY(recorder.setCommandAction(show->id(), id, QStringLiteral("Stop")));
    QCOMPARE(int(recorder.track().commands().at(0).action), int(ShowCommandAction::Stop));
    QCOMPARE(int(show->commandTrack().commands().at(0).action), int(ShowCommandAction::Stop));

    QVERIFY(recorder.setCommandTarget(show->id(), id, other->id()));
    QCOMPARE(recorder.track().commands().at(0).functionId, other->id());
    QCOMPARE(show->commandTrack().commands().at(0).functionId, other->id());

    // Rejected edits leave the take alone and say why.
    QVERIFY(!recorder.setCommandAction(show->id(), id, QStringLiteral("Explode")));
    QVERIFY(!recorder.lastError().isEmpty());
    QCOMPARE(int(recorder.track().commands().at(0).action), int(ShowCommandAction::Stop));

    QVERIFY(!recorder.setCommandTarget(show->id(), id, 12345));
    QCOMPARE(recorder.track().commands().at(0).functionId, other->id());

    // A Show that starts itself is not a target.
    QVERIFY(!recorder.setCommandTarget(show->id(), id, show->id()));
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

void ShowCommandRecorder_Test::finalizeExtent_endsAtLastRetainedItem_data()
{
    QTest::addColumn<quint32>("authoredExtent");
    QTest::addColumn<bool>("record");
    QTest::addColumn<quint32>("expectedExtent");

    // A take ends at its last retained item, not at an empty authored tail.
    QTest::newRow("short take on a long show") << 60000u << true << 10000u;
    // A take that captured nothing still ends at its content: here, none.
    QTest::newRow("no input at all") << 60000u << false << 0u;
    // Nor at the time Record stayed armed.
    QTest::newRow("fresh show") << 0u << true << 10000u;
}

void ShowCommandRecorder_Test::finalizeExtent_endsAtLastRetainedItem()
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

void ShowCommandRecorder_Test::newTake_endsEditedExtentAtContent_data()
{
    QTest::addColumn<bool>("rewindBeforeArming");
    QTest::newRow("edit-selected-show-before-recording") << false;
    QTest::newRow("off-mode-playback-does-not-extend-recording") << true;
}

void ShowCommandRecorder_Test::newTake_endsEditedExtentAtContent()
{
    QFETCH(bool, rewindBeforeArming);
    Doc doc(nullptr, 1);
    Show *show = addRecordingShow(&doc, 10000);
    QVERIFY(show);
    ShowCommandRecorder recorder(&doc);
    QVERIFY(recorder.setResolvedShow(show->id()));
    ShowCommandTrack authored;
    QVERIFY(authored.setExtent(60000));
    QVERIFY(show->setCommandTrack(authored));
    if (rewindBeforeArming)
    {
        show->setExternalElapsedTime(70000);
        show->setExternalElapsedTime(10000);
    }

    // arming keeps the edited end; the take's end is its content
    QVERIFY(recorder.setRecording(true));
    QCOMPARE(show->commandTrack().extent(), quint32(60000));
    show->setExternalElapsedTime(12000);
    QVERIFY(recorder.setRecording(false));

    QCOMPARE(show->commandTrack().extent(), quint32(0));
    QCOMPARE(recorder.extent(), 0);
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

    QCOMPARE(recorder.track().count(), 1);
    QCOMPARE(recorder.track().commands().at(0).time, quint32(20000));

    // The engine finally runs: the recorded gesture must not be recorded twice.
    rig.fader->writeDMX(doc.masterTimer(), QList<Universe *>());
    QCoreApplication::processEvents();
    QCOMPARE(recorder.track().count(), 1);
}

void ShowCommandRecorder_Test::invalidEdits_areRejectedNotClamped_data()
{
    QTest::addColumn<QString>("edit");
    QTest::addColumn<int>("value");

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
    QVERIFY(edit == QStringLiteral("retime"));
    QVERIFY(!recorder.retimeCommand(show->id(), id, value));

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
    ShowCommandInput start;
    start.origin = ShowCommandOrigin::Pointer;
    start.action = ShowCommandAction::Start;
    start.functionId = scene->id();
    QVERIFY(recorder.submitUserInput(start));
    QCOMPARE(recorder.track().count(), 1);

    // The recorded target disappears, so the take can no longer be published:
    // accepted input waits unpublished.
    QVERIFY(doc.deleteFunction(scene->id()));
    show->setExternalElapsedTime(15000);
    rig.fader->setSliderMode(VCSlider::Level);
    rig.fader->requestUserValue(90);
    QCOMPARE(show->commandTrack().count(), 1);

    QVERIFY(!recorder.setRecording(false));

    // Failing to close the take must not throw it away or pretend it stopped.
    QVERIFY(recorder.isRecording());
    QVERIFY(recorder.isAuthoring());
    QVERIFY(!recorder.lastError().isEmpty());
    QCOMPARE(recorder.track().count(), 1);
    QVERIFY(!recorder.checkpoint());
}

void ShowCommandRecorder_Test::replayedButtonOn_startsItsSceneThroughShowPlayback()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    show->setName(QStringLiteral("Replay show"));
    QVERIFY(doc.addFunction(show));

    // Neither selected nor armed: replay belongs to every playing Show.
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), scene->id(),
                                          QStringLiteral("Target"), QStringLiteral("toggle"));
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 10, button->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(scene);
    log.watchReceipts(show);
    const auto stopShow = qScopeGuard([&]() {
        show->stop(FunctionParent::master());
        show->stopAndWait();
    });

    show->start(doc.masterTimer(), FunctionParent::master());

    // the batch is acknowledged once its receipt came back
    QTRY_VERIFY(log.receipts() > 0 && recorder.pendingControlRuns() == 0);
    QVERIFY(log.live(scene->id()));
    QCOMPARE(button->state(), VCButton::Active);
}

void ShowCommandRecorder_Test::replayedCollectionOn_thenChildOff_stopsTheChild_data()
{
    QTest::addColumn<QString>("shape");

    QTest::newRow("direct member") << QStringLiteral("direct member");
    QTest::newRow("nested depth 2") << QStringLiteral("nested depth 2");
    QTest::newRow("nested depth 3") << QStringLiteral("nested depth 3");
    QTest::newRow("shared by two members") << QStringLiteral("shared by two members");
    QTest::newRow("cycle") << QStringLiteral("cycle");
}

void ShowCommandRecorder_Test::replayedCollectionOn_thenChildOff_stopsTheChild()
{
    QFETCH(QString, shape);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *child = addTarget(&doc);
    QVERIFY(child);
    // a second member keeps the Collection alive once the first one stops
    Scene *sibling = new Scene(&doc);
    sibling->setName(QStringLiteral("Sibling"));
    sibling->setValue(SceneValue(child->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(sibling));
    const auto addCollection = [&doc](const QString &name, const QList<quint32> &members)
    {
        Collection *collection = new Collection(&doc);
        collection->setName(name);
        doc.addFunction(collection);
        for (quint32 id : members)
            collection->addFunction(id);
        return collection;
    };
    // the recorded Collection starts the child directly or through nested Collections
    Collection *inner = addCollection(QStringLiteral("Inner"), { child->id(), sibling->id() });
    Collection *collection = inner;
    if (shape == QLatin1String("nested depth 2") || shape == QLatin1String("cycle"))
        collection = addCollection(QStringLiteral("Look"), { inner->id() });
    else if (shape == QLatin1String("nested depth 3"))
        collection = addCollection(QStringLiteral("Look"),
                                   { addCollection(QStringLiteral("Mid"), { inner->id() })->id() });
    else if (shape == QLatin1String("shared by two members"))
        collection = addCollection(QStringLiteral("Look"),
                                   { inner->id(),
                                     addCollection(QStringLiteral("Other"),
                                                   { child->id(), sibling->id() })->id() });
    if (shape == QLatin1String("cycle"))
        QVERIFY(inner->addFunction(collection->id()));
    Show *show = new Show(&doc);
    show->setName(QStringLiteral("Replay show"));
    QVERIFY(doc.addFunction(show));

    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *lookButton = qobject_cast<VCButton *>(ui.vc()->widget(
            bridge.addButton(frameID, QRect(10, 10, 80, 40), collection->id(),
                             QStringLiteral("Look"), QStringLiteral("toggle"))));
    VCButton *childButton = qobject_cast<VCButton *>(ui.vc()->widget(
            bridge.addButton(frameID, QRect(100, 10, 80, 40), child->id(),
                             QStringLiteral("Child"), QStringLiteral("toggle"))));
    QVERIFY(lookButton);
    QVERIFY(childButton);

    // Crossed by the same tick, so they arrive as one batch in this order.
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 10, lookButton->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 11, childButton->ensureRecordingId(), false)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    for (Function *f : std::initializer_list<Function *>{ collection, child, sibling })
        log.watch(f);
    log.watchReceipts(show);
    const auto stopShow = qScopeGuard([&]() {
        show->stop(FunctionParent::master());
        show->stopAndWait();
    });

    show->start(doc.masterTimer(), FunctionParent::master());

    // acknowledged after the child's stop receipt, which follows its stopped()
    QTRY_VERIFY(log.receipts() > 0 && recorder.pendingControlRuns() == 0);
    QCOMPARE(lookButton->state(), VCButton::Active);
    QVERIFY(log.live(collection->id()));
    QVERIFY(log.live(sibling->id()));
    QVERIFY(log.starts(child->id()) > 0);
    QVERIFY(!log.live(child->id()));
    QCOMPARE(childButton->state(), VCButton::Inactive);
}

void ShowCommandRecorder_Test::replay_survivesSelectionChange()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *playing = new Show(&doc);
    QVERIFY(doc.addFunction(playing));
    Show *selected = new Show(&doc);
    QVERIFY(doc.addFunction(selected));

    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    QVERIFY(button);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 10, button->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(playing->setCommandTrack(track));

    QVERIFY(recorder.setResolvedShow(playing->id()));
    playing->start(doc.masterTimer(), FunctionParent::master());
    // the operator selects another Show before the record is crossed
    QVERIFY(recorder.setResolvedShow(selected->id()));
    tickAndDeliver(&doc, 5);

    QCOMPARE(button->state(), VCButton::Active);
    QVERIFY(scene->isRunning());
}

void ShowCommandRecorder_Test::guiCancel_dropsWorkQueuedBeforeIt_data()
{
    QTest::addColumn<QString>("cancel");
    QTest::addColumn<bool>("receiptQueued");

    QTest::newRow("seek, batch queued") << QStringLiteral("seek") << false;
    QTest::newRow("seek, receipt queued") << QStringLiteral("seek") << true;
    QTest::newRow("function stop, batch queued") << QStringLiteral("function stop") << false;
    QTest::newRow("function stop, receipt queued") << QStringLiteral("function stop") << true;
    QTest::newRow("toggle stop, batch queued") << QStringLiteral("toggle stop") << false;
    QTest::newRow("toggle stop, receipt queued") << QStringLiteral("toggle stop") << true;
}

void ShowCommandRecorder_Test::guiCancel_dropsWorkQueuedBeforeIt()
{
    QFETCH(QString, cancel);
    QFETCH(bool, receiptQueued);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const LookRig rig = addLookRig(&doc, bridge, ui.vc());
    QVERIFY(rig.lookButton);
    QVERIFY(rig.childButton);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 10, rig.lookButton->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 11, rig.childButton->ensureRecordingId(), false)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    // a VC Toggle bound to the Show owns its playback
    VCButton *showButton = nullptr;
    if (cancel == QStringLiteral("toggle stop"))
    {
        const int frameID = bridge.addFrame(0, QRect(0, 310, 400, 100), QStringLiteral("Shows"), false);
        showButton = addToggle(bridge, ui.vc(), frameID, show->id(), 10, QStringLiteral("Show"));
        QVERIFY(showButton);
    }

    QSignalSpy batches(show, &Show::controlBatchesReady);
    QSignalSpy receipts(show, &Show::functionReceiptReady);
    NativeLog log;
    log.watch(rig.collection);
    log.watch(rig.child);
    if (showButton != nullptr)
        showButton->requestStateChange(true);
    else
        show->start(timer, FunctionParent::master());
    for (int i = 0; i < 5 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    if (receiptQueued)
    {
        // the Collection ON is applied, the child OFF waits for its receipt
        QCoreApplication::processEvents();
        QCOMPARE(recorder.pendingControlRuns(), 1);
        for (int i = 0; i < 5 && receipts.isEmpty(); i++)
            timer->timerTick();
        QCOMPARE(receipts.count(), 1);
    }

    // the queued work reaches the GUI only after the operator's seek or stop,
    // before the timer ran the stop
    if (cancel == QStringLiteral("seek"))
        show->requestSeek(30000);
    else if (cancel == QStringLiteral("function stop"))
        static_cast<Function *>(show)->stop(FunctionParent::master());
    else
        showButton->requestStateChange(true);
    QCoreApplication::processEvents();
    QCOMPARE(recorder.pendingControlRuns(), 0);
    if (cancel == QStringLiteral("seek"))
    {
        // the cancelled work stopped where it was: nothing of it ran after the seek
        QCOMPARE(rig.lookButton->state(), receiptQueued ? VCButton::Active : VCButton::Inactive);
        QCOMPARE(rig.childButton->state(), receiptQueued ? VCButton::Monitoring : VCButton::Inactive);
        QCOMPARE(log.starts(rig.collection->id()), receiptQueued ? 1 : 0);
        QCOMPARE(log.starts(rig.child->id()), receiptQueued ? 1 : 0);
        QCOMPARE(log.indexOf(rig.child->id(), false), -1);

        // the seek catches up to its destination instead: Look ON, then Child OFF
        const int before = receipts.count();
        for (int i = 0; i < 20 && (receipts.count() == before || recorder.pendingControlRuns() > 0); i++)
            tickAndDeliver(&doc);
        QVERIFY(rig.collection->isRunning());
        QCOMPARE(rig.lookButton->state(), VCButton::Active);
        QVERIFY(!rig.child->isRunning());
        QCOMPARE(rig.childButton->state(), VCButton::Inactive);
        QCOMPARE(log.starts(rig.collection->id()), 1);
        QCOMPARE(log.starts(rig.child->id()), 1);
        return;
    }
    tickAndDeliver(&doc, 5);

    QCOMPARE(rig.collection->isRunning(), receiptQueued);
    QCOMPARE(rig.lookButton->state(), receiptQueued ? VCButton::Active : VCButton::Inactive);
    // the child OFF behind the cancelled work is never applied
    QCOMPARE(rig.child->isRunning(), receiptQueued);
    QCOMPARE(rig.childButton->state(), receiptQueued ? VCButton::Monitoring : VCButton::Inactive);
}

void ShowCommandRecorder_Test::cancel_dropsParkedRunWithoutFurtherBatch_data()
{
    QTest::addColumn<QString>("cancel");

    QTest::newRow("gui seek") << QStringLiteral("gui seek");
    QTest::newRow("function stop") << QStringLiteral("function stop");
    QTest::newRow("stop, then manual tick") << QStringLiteral("stop, then manual tick");
}

void ShowCommandRecorder_Test::cancel_dropsParkedRunWithoutFurtherBatch()
{
    QFETCH(QString, cancel);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    QVERIFY(button);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 10, button->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 5 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    // applied, the acknowledgement waits for the Scene's receipt
    QCoreApplication::processEvents();
    QCOMPARE(recorder.pendingControlRuns(), 1);

    if (cancel == QStringLiteral("gui seek"))
    {
        show->requestSeek(30000);
        QCoreApplication::processEvents();
    }
    else if (cancel == QStringLiteral("function stop"))
    {
        // before the timer ran it
        static_cast<Function *>(show)->stop(FunctionParent::master());
        QCoreApplication::processEvents();
    }
    else
    {
        // stop, then the postRun of a tick stepped on this thread
        show->stop(FunctionParent::master());
        tickAndDeliver(&doc);
    }

    QCOMPARE(recorder.pendingControlRuns(), 0);
}

void ShowCommandRecorder_Test::replayedButtonOn_atContentEnd_runsBeforeShowEnds()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    QVERIFY(button);

    // the last record is the end of the Show's content
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, button->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(100));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(scene);
    log.watch(show);
    const auto stopShow = qScopeGuard([&]() {
        show->stop(FunctionParent::master());
        show->stopAndWait();
    });

    show->start(doc.masterTimer(), FunctionParent::master());

    // it ends by itself, only after the crossed record took native effect
    QTRY_VERIFY(log.indexOf(show->id(), false) >= 0);
    QVERIFY(log.indexOf(scene->id(), true) >= 0);
    QVERIFY(log.indexOf(scene->id(), true) < log.indexOf(show->id(), false));
    QCOMPARE(button->state(), VCButton::Active);
}

void ShowCommandRecorder_Test::replayedPlayingPair_waitsForItsCoupledPredecessor_data()
{
    QTest::addColumn<bool>("solo");
    QTest::addColumn<bool>("secondOn");
    QTest::addColumn<bool>("firstLive");
    QTest::addColumn<bool>("secondLive");

    // B ON in a SoloFrame stops A, and B is started once
    QTest::newRow("solo group") << true << true << false << true;
    // two buttons of one Scene: the OFF lands on the running Scene
    QTest::newRow("same function") << false << false << false << false;
}

void ShowCommandRecorder_Test::replayedPlayingPair_waitsForItsCoupledPredecessor()
{
    QFETCH(bool, solo);
    QFETCH(bool, secondOn);
    QFETCH(bool, firstLive);
    QFETCH(bool, secondLive);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *first = addTarget(&doc);
    QVERIFY(first);
    Scene *second = first;
    if (solo)
    {
        second = new Scene(&doc);
        second->setName(QStringLiteral("Second"));
        second->setValue(SceneValue(first->values().first().fxi, 1, 120));
        QVERIFY(doc.addFunction(second));
    }
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), solo);
    VCButton *firstButton = addToggle(bridge, ui.vc(), frameID, first->id(), 10, QStringLiteral("First"));
    VCButton *secondButton = addToggle(bridge, ui.vc(), frameID, second->id(), 100, QStringLiteral("Second"));
    QVERIFY(firstButton);
    QVERIFY(secondButton);

    // crossed by the same tick, so they arrive as one batch in this order
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 10, firstButton->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 11, secondButton->ensureRecordingId(), secondOn)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(first);
    if (solo)
        log.watch(second);
    log.watchReceipts(show);
    const auto stopShow = qScopeGuard([&]() {
        show->stop(FunctionParent::master());
        show->stopAndWait();
    });

    show->start(doc.masterTimer(), FunctionParent::master());

    QTRY_VERIFY(log.receipts() > 0 && recorder.pendingControlRuns() == 0);
    QTRY_COMPARE(log.live(first->id()), firstLive);
    QTRY_COMPARE(log.live(second->id()), secondLive);
    QCOMPARE(log.starts(second->id()), 1);
    QCOMPARE(firstButton->state(), firstLive ? VCButton::Active : VCButton::Inactive);
    QCOMPARE(secondButton->state(), secondLive ? VCButton::Active : VCButton::Inactive);
}

void ShowCommandRecorder_Test::replayedSliderPosition_thenLegacyIntensity_endsAtLegacyValue()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);

    // the fader already plays its Scene, as it does live
    slider->setValue(100);
    for (int i = 0; i < 10 && !scene->isRunning(); i++)
        tickAndDeliver(&doc);
    QVERIFY(scene->isRunning());

    // the slider record is a batch of its own, the legacy value waits behind it
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, slider->ensureRecordingId(),
                                                        ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.8)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 11, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    // the timer is stepped on this thread, so the values are read where they are written
    QVector<qreal> intensities;
    QObject probe;
    connect(scene, &Function::attributeChanged, &probe, [&intensities](int index, qreal value) {
        if (index == Function::Intensity)
            intensities.append(value);
    });
    const auto first = [&intensities](qreal value) {
        for (int i = 0; i < intensities.count(); i++)
            if (qFuzzyCompare(intensities.at(i), value))
                return i;
        return -1;
    };
    const auto last = [&intensities](qreal value) {
        for (int i = intensities.count() - 1; i >= 0; i--)
            if (qFuzzyCompare(intensities.at(i), value))
                return i;
        return -1;
    };

    show->start(doc.masterTimer(), FunctionParent::master());
    for (int i = 0; i < 10 && (first(0.2) < 0 || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    // whatever the slider still holds is written by the next tick
    tickAndDeliver(&doc);

    QVERIFY2(last(0.8) >= 0, "the replayed slider position never reached the function");
    QVERIFY(first(0.2) > last(0.8));
    QCOMPARE(scene->getAttributeValue(Function::Intensity), qreal(0.2));

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::replayedSliderPosition_supersededByNewerInput_doesNotStall()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);
    slider->setValue(100);
    for (int i = 0; i < 10 && !scene->isRunning(); i++)
        tickAndDeliver(&doc);
    QVERIFY(scene->isRunning());

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, slider->ensureRecordingId(),
                                                        ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.8)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 11, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QVector<qreal> intensities;
    QObject probe;
    connect(scene, &Function::attributeChanged, &probe, [&intensities](int index, qreal value) {
        if (index == Function::Intensity)
            intensities.append(value);
    });
    const auto applied = [&intensities](qreal value) {
        return std::any_of(intensities.cbegin(), intensities.cend(),
                           [value](qreal v) { return qFuzzyCompare(v, value); });
    };
    QSignalSpy retired(slider, &VCSlider::recordedWriteRetired);

    show->start(doc.masterTimer(), FunctionParent::master());
    // an audio mapping moves the fader before every tick, also after the replay
    for (int i = 0; i < 10 && !applied(0.2); i++)
    {
        slider->setValue(140 + i);
        tickAndDeliver(&doc);
    }

    QCOMPARE(retired.count(), 1);
    QCOMPARE(retired.first().at(1).toInt(), int(VCSlider::RecordedWriteSuperseded));
    QVERIFY(!applied(0.8));
    QVERIFY(applied(0.2));
    QCOMPARE(recorder.pendingControlRuns(), 0);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::replayedSliderPosition_cancelledByReconfiguration_data()
{
    QTest::addColumn<QString>("change");
    QTest::addColumn<bool>("written");
    QTest::addColumn<int>("outcome");

    const int cancelled = VCSlider::RecordedWriteCancelled;
    QTest::newRow("mode level") << QStringLiteral("mode level") << false << cancelled;
    QTest::newRow("mode submaster") << QStringLiteral("mode submaster") << false << cancelled;
    QTest::newRow("mode grand master") << QStringLiteral("mode grand master") << false << cancelled;
    QTest::newRow("rebind") << QStringLiteral("rebind") << false << cancelled;
    QTest::newRow("attribute change") << QStringLiteral("attribute change") << false << cancelled;
    QTest::newRow("disable") << QStringLiteral("disable") << false << cancelled;
    QTest::newRow("delete") << QStringLiteral("delete") << false << cancelled;
    // written, the report still queued when the slider goes away
    QTest::newRow("delete after write") << QStringLiteral("delete") << true
                                        << int(VCSlider::RecordedWriteApplied);
}

void ShowCommandRecorder_Test::replayedSliderPosition_cancelledByReconfiguration()
{
    QFETCH(QString, change);
    QFETCH(bool, written);
    QFETCH(int, outcome);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Scene *other = new Scene(&doc);
    other->setValue(SceneValue(scene->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(other));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, slider->ensureRecordingId(),
                                                        ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.8)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 11, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QSignalSpy batches(show, &Show::controlBatchesReady);
    QSignalSpy retired(slider, &VCSlider::recordedWriteRetired);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 5 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    // applied, the batch waits for the write no tick has run yet
    QCoreApplication::processEvents();
    QCOMPARE(recorder.pendingControlRuns(), 1);
    const quint64 traversal = show->commandTraversal();
    if (written)
        timer->timerTick();

    if (change == QStringLiteral("mode level"))
        slider->setSliderMode(VCSlider::Level);
    else if (change == QStringLiteral("mode submaster"))
        slider->setSliderMode(VCSlider::Submaster);
    else if (change == QStringLiteral("mode grand master"))
        slider->setSliderMode(VCSlider::GrandMaster);
    else if (change == QStringLiteral("rebind"))
        slider->setControlledFunction(other->id());
    else if (change == QStringLiteral("attribute change"))
        slider->setControlledAttribute(Function::Intensity + 1);
    else if (change == QStringLiteral("disable"))
        slider->setDisabled(true);
    else
        ui.vc()->deleteVCWidgets(QVariantList{ slider->id() });
    if (written)
        QCoreApplication::processEvents();

    // reported on the spot, without waiting for any write
    QCOMPARE(retired.count(), 1);
    QCOMPARE(retired.first().at(1).toInt(), outcome);
    // that write started the stopped Scene: the op ends once the start is live
    if (written)
    {
        QCOMPARE(retired.first().at(2).toUInt(), scene->id());
        for (int i = 0; i < 5 && recorder.pendingControlRuns() > 0; i++)
            tickAndDeliver(&doc);
    }
    // finished within the same traversal: acknowledged, not abandoned
    QCOMPARE(recorder.pendingControlRuns(), 0);
    QCOMPARE(show->commandTraversal(), traversal);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::replayedSliderPosition_functionDeleted_showStillEnds()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);

    // the batch is the end of the Show's content
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, slider->ensureRecordingId(),
                                                        ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.8)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 11, scene->id(), 0.2)));
    QVERIFY(track.setExtent(11));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(show);

    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 5 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    QCoreApplication::processEvents();
    QCOMPARE(recorder.pendingControlRuns(), 1);

    // no write can ever reach a Function that is gone
    QVERIFY(doc.deleteFunction(scene->id()));
    QCOMPARE(recorder.pendingControlRuns(), 0);

    for (int i = 0; i < 10 && log.indexOf(show->id(), false) < 0; i++)
        tickAndDeliver(&doc);
    QVERIFY(log.indexOf(show->id(), false) >= 0);
}

void ShowCommandRecorder_Test::cancelledSliderReplay_neverWritesLate()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);
    slider->setValue(100);
    for (int i = 0; i < 10 && !scene->isRunning(); i++)
        tickAndDeliver(&doc);
    QVERIFY(scene->isRunning());

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, slider->ensureRecordingId(),
                                                        ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.8)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 11, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QVector<qreal> intensities;
    QObject probe;
    connect(scene, &Function::attributeChanged, &probe, [&intensities](int index, qreal value) {
        if (index == Function::Intensity)
            intensities.append(value);
    });
    const auto applied = [&intensities](qreal value) {
        return std::any_of(intensities.cbegin(), intensities.cend(),
                           [value](qreal v) { return qFuzzyCompare(v, value); });
    };

    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 5 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    QCoreApplication::processEvents();
    QCOMPARE(recorder.pendingControlRuns(), 1);

    // cancelled before any write, so the legacy value follows at once
    slider->setDisabled(true);
    QCOMPARE(recorder.pendingControlRuns(), 0);
    for (int i = 0; i < 10 && !applied(0.2); i++)
        tickAndDeliver(&doc);
    QVERIFY(applied(0.2));

    // and nothing the cancelled replay left behind lands after it
    tickAndDeliver(&doc, 3);
    QVERIFY(!applied(0.8));
    QCOMPARE(scene->getAttributeValue(Function::Intensity), qreal(0.2));

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::cancelledSliderReplay_laterSameValueStillWrites_data()
{
    QTest::addColumn<QString>("after");
    QTest::addColumn<int>("finalValue");

    QTest::newRow("same replay after re-enable") << QStringLiteral("same replay") << 204;
    QTest::newRow("same input after re-enable") << QStringLiteral("same input") << 204;
    // an audio value that arrived before the cancel is not the replay's
    QTest::newRow("newer input in the cancel window") << QStringLiteral("newer input") << 150;
}

void ShowCommandRecorder_Test::cancelledSliderReplay_laterSameValueStillWrites()
{
    QFETCH(QString, after);
    QFETCH(int, finalValue);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);
    slider->setValue(100);
    for (int i = 0; i < 10 && !scene->isRunning(); i++)
        tickAndDeliver(&doc);
    QVERIFY(scene->isRunning());

    ShowCommandTrack track;
    const QUuid id = slider->ensureRecordingId();
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, id, ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.8)));
    if (after == QStringLiteral("same replay"))
        QVERIFY(track.insert(ShowCommand::setSliderPosition(1, 200, id, ShowControlRole::AdjustSlider,
                                                            QStringLiteral("Intensity"), 0.8)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 5 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    QCoreApplication::processEvents();
    QCOMPARE(recorder.pendingControlRuns(), 1);

    if (after == QStringLiteral("newer input"))
        slider->setValue(150);
    slider->setDisabled(true);
    slider->setDisabled(false);
    if (after == QStringLiteral("same input"))
        slider->setValue(204);

    const qreal expected = qreal(finalValue) / qreal(UCHAR_MAX);
    for (int i = 0; i < 20 && (!qFuzzyCompare(scene->getAttributeValue(Function::Intensity), expected) ||
                               recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    QCOMPARE(scene->getAttributeValue(Function::Intensity), expected);
    QCOMPARE(recorder.pendingControlRuns(), 0);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::cancelledLevelReplay_restoresPriorControlState_data()
{
    QTest::addColumn<bool>("monitorReading");
    QTest::addColumn<int>("finalValue");

    QTest::newRow("no newer reading") << false << 0;
    // a channel monitor reading delivered after the replay is newer than it
    QTest::newRow("monitor reading after the replay") << true << 50;
}

void ShowCommandRecorder_Test::cancelledLevelReplay_restoresPriorControlState()
{
    QFETCH(bool, monitorReading);
    QFETCH(int, finalValue);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    QVector<int> feedback;
    doc.ioPluginCache()->m_plugins.append(new FeedbackPlugin(&feedback));
    QVERIFY(doc.inputOutputMap()->setOutputPatch(kSourceUniverse, QStringLiteral("Test feedback"),
                                                 QString(), QStringLiteral("Feedback line"), 0, true));
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    QVERIFY(slider);
    slider->setSliderMode(VCSlider::Level);
    slider->setMonitorEnabled(true);
    mapInputSource(ui.vc()->page(0), slider, 0);

    QVERIFY(slider->applyRecordedPosition(0.8) != 0);
    QVERIFY(slider->isOverriding());
    if (monitorReading)
        slider->setValue(50, false, true);

    slider->setDisabled(true);
    slider->setDisabled(false);

    QCOMPARE(slider->value(), finalValue);
    QVERIFY(!slider->isOverriding());
    QVERIFY(!feedback.isEmpty());
    QCOMPARE(feedback.last(), finalValue);
}

void ShowCommandRecorder_Test::replayedSliderStart_isLiveBeforeLegacyIntensity()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);

    // the target is stopped: the replayed position is what starts it
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, slider->ensureRecordingId(),
                                                        ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.8)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 11, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QVector<qreal> intensities;
    QObject probe;
    connect(scene, &Function::attributeChanged, &probe, [&intensities](int index, qreal value) {
        if (index == Function::Intensity)
            intensities.append(value);
    });
    const auto applied = [&intensities](qreal value) {
        return std::any_of(intensities.cbegin(), intensities.cend(),
                           [value](qreal v) { return qFuzzyCompare(v, value); });
    };

    show->start(doc.masterTimer(), FunctionParent::master());
    for (int i = 0; i < 10 && (!applied(0.2) || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    tickAndDeliver(&doc);

    QVERIFY(applied(0.8));
    QVERIFY2(applied(0.2), "the legacy value found its target not started yet");
    QVERIFY(scene->isRunning());
    QCOMPARE(scene->getAttributeValue(Function::Intensity), qreal(0.2));

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::replayedSliderStart_racingOwnWrite_isLiveBeforeLegacyIntensity_data()
{
    QTest::addColumn<QString>("race");
    QTest::addColumn<int>("reports");

    // the fader already holds the recorded value and its write started the
    // Scene in the tick that crossed the record: no action, but the start
    // must be live before the legacy value
    QTest::newRow("matching value written, start queued") << QStringLiteral("written") << 0;
    // the fader already holds the recorded value, and its own pending write
    // is what starts the Scene: the replay waits for that write, no other
    QTest::newRow("matching value, pending write starts it") << QStringLiteral("same value") << 1;
    // the replay's own write started the Scene, then the fader was rebound
    // before that report reached the GUI
    QTest::newRow("rebound before the report") << QStringLiteral("rebound") << 1;
}

void ShowCommandRecorder_Test::replayedSliderStart_racingOwnWrite_isLiveBeforeLegacyIntensity()
{
    QFETCH(QString, race);
    QFETCH(int, reports);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Scene *other = new Scene(&doc);
    other->setValue(SceneValue(scene->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(other));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 0, slider->ensureRecordingId(),
                                                        ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.8)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 1, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QVector<qreal> intensities;
    QObject probe;
    connect(scene, &Function::attributeChanged, &probe, [&intensities](int index, qreal value) {
        if (index == Function::Intensity)
            intensities.append(value);
    });
    const auto applied = [&intensities](qreal value) {
        return std::any_of(intensities.cbegin(), intensities.cend(),
                           [value](qreal v) { return qFuzzyCompare(v, value); });
    };

    if (race == QStringLiteral("written"))
        slider->setValue(204);
    QSignalSpy batches(show, &Show::controlBatchesReady);
    QSignalSpy retired(slider, &VCSlider::recordedWriteRetired);
    show->start(timer, FunctionParent::master());
    timer->timerTick();
    QCOMPARE(batches.count(), 1);
    if (race == QStringLiteral("same value"))
        slider->setValue(204);
    QCoreApplication::processEvents();
    if (race == QStringLiteral("rebound"))
    {
        // the write that starts the Scene, its report still queued
        timer->timerTick();
        slider->setControlledFunction(other->id());
    }

    for (int i = 0; i < 10 && (!applied(0.2) || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);

    QVERIFY2(applied(0.2), "the legacy value found its target not started yet");
    QCOMPARE(recorder.pendingControlRuns(), 0);
    QVERIFY(scene->isRunning());
    QCOMPARE(scene->getAttributeValue(Function::Intensity), qreal(0.2));
    // at most the one write that started the Scene, reported with that Scene
    QCOMPARE(retired.count(), reports);
    if (reports > 0)
        QCOMPARE(retired.first().at(2).toUInt(), scene->id());

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::replayedMatchingSliderPosition_takesNoAction_data()
{
    QTest::addColumn<int>("value");
    QTest::addColumn<bool>("live");

    QTest::newRow("matching position already written") << 204 << true;
    // natively a stopped target, so the legacy value after it is skipped
    QTest::newRow("matching zero on a stopped target") << 0 << false;
}

void ShowCommandRecorder_Test::replayedMatchingSliderPosition_takesNoAction()
{
    QFETCH(int, value);
    QFETCH(bool, live);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);
    slider->setValue(value);
    for (int i = 0; i < 10 && scene->isRunning() != live; i++)
        tickAndDeliver(&doc);
    QCOMPARE(scene->isRunning(), live);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, slider->ensureRecordingId(),
                                                        ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"),
                                                        qreal(value) / qreal(UCHAR_MAX))));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 11, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QVector<qreal> intensities;
    QObject probe;
    connect(scene, &Function::attributeChanged, &probe, [&intensities](int index, qreal v) {
        if (index == Function::Intensity)
            intensities.append(v);
    });
    QSignalSpy batches(show, &Show::controlBatchesReady);
    QSignalSpy retired(slider, &VCSlider::recordedWriteRetired);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 5 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    QCoreApplication::processEvents();

    // nothing to do: only the target's liveness is observed, then it acks
    for (int i = 0; i < 5 && recorder.pendingControlRuns() > 0; i++)
        tickAndDeliver(&doc);
    QCOMPARE(recorder.pendingControlRuns(), 0);
    tickAndDeliver(&doc, 2);
    QCOMPARE(retired.count(), 0);
    // the target receives the legacy value only, and only while live
    QCOMPARE(intensities.isEmpty(), !live);
    QVERIFY(std::all_of(intensities.cbegin(), intensities.cend(),
                        [](qreal v) { return qFuzzyCompare(v, 0.2); }));
    QCOMPARE(scene->isRunning(), live);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::replayedChildOff_afterLegacyStart_stopsTheChild_data()
{
    QTest::addColumn<QString>("shape");

    QTest::newRow("direct member") << QStringLiteral("direct member");
    QTest::newRow("nested depth 2") << QStringLiteral("nested depth 2");
    // its own button already runs it: the Collection start adds an owner only
    QTest::newRow("already-live child") << QStringLiteral("already-live child");
    // running but paused: the start resumes nothing and emits nothing
    QTest::newRow("paused collection") << QStringLiteral("paused collection");
    // the legacy Start and the OFF target the same function
    QTest::newRow("same function") << QStringLiteral("same function");
}

void ShowCommandRecorder_Test::replayedChildOff_afterLegacyStart_stopsTheChild()
{
    QFETCH(QString, shape);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const LookRig rig = addLookRig(&doc, bridge, ui.vc());
    QVERIFY(rig.childButton);
    // what the legacy command starts
    Function *started = rig.collection;
    if (shape == QStringLiteral("nested depth 2"))
    {
        Collection *outer = new Collection(&doc);
        QVERIFY(doc.addFunction(outer));
        QVERIFY(outer->addFunction(rig.collection->id()));
        started = outer;
    }
    else if (shape == QStringLiteral("same function"))
    {
        started = rig.child;
    }

    // Crossed by the same tick: the legacy Start runs in the runner, the OFF
    // arrives in the VC batch right behind it.
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 10, started->id())));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 11, rig.childButton->ensureRecordingId(), false)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    for (Function *f : std::initializer_list<Function *>{ started, rig.collection, rig.child, rig.sibling })
        log.watch(f);
    int batches = 0;
    QObject probe;
    connect(show, &Show::controlBatchesReady, &probe, [&batches]() { batches++; }, Qt::QueuedConnection);

    // prepared with the timer stepped on this thread, before it runs on its own
    if (shape == QStringLiteral("already-live child"))
        rig.childButton->requestStateChange(true);
    else if (shape == QStringLiteral("paused collection"))
        rig.collection->start(timer, FunctionParent::master());
    const bool prepared = shape == QStringLiteral("already-live child") ||
                          shape == QStringLiteral("paused collection");
    for (int i = 0; i < 10 && prepared && !rig.child->isRunning(); i++)
        tickAndDeliver(&doc);
    if (shape == QStringLiteral("paused collection"))
        rig.collection->setPause(true);

    timer->start();
    const auto stopTimer = qScopeGuard([&]() { timer->stop(); });
    const auto stopShow = qScopeGuard([&]() {
        show->stop(FunctionParent::master());
        show->stopAndWait();
    });
    show->start(timer, FunctionParent::master());

    // the child ran, and the replayed OFF stopped it again
    QTRY_VERIFY(batches > 0 && recorder.pendingControlRuns() == 0 && log.starts(rig.child->id()) > 0 &&
                !log.live(rig.child->id()) && rig.childButton->state() == VCButton::Inactive);
    if (started != rig.child)
        QVERIFY(log.live(started->id()));
}

void ShowCommandRecorder_Test::replayedSoloSiblingOn_afterCollectionStart_winsLikeNative_data()
{
    QTest::addColumn<QString>("start");

    QTest::newRow("legacy Collection start") << QStringLiteral("legacy collection");
    QTest::newRow("VC Collection button") << QStringLiteral("vc collection");
    // the started function's own button shares the Solo Frame
    QTest::newRow("legacy start of A itself") << QStringLiteral("legacy root");
    // the Collection's own monitoring button shares the Solo Frame
    QTest::newRow("Collection button in the frame") << QStringLiteral("collection in frame");
}

void ShowCommandRecorder_Test::replayedSoloSiblingOn_afterCollectionStart_winsLikeNative()
{
    QFETCH(QString, start);
    const bool collectionInFrame = start == QStringLiteral("collection in frame");

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    Scene *a = addTarget(&doc);
    QVERIFY(a);
    Scene *b = new Scene(&doc);
    b->setValue(SceneValue(a->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(b));
    Collection *collection = new Collection(&doc);
    QVERIFY(doc.addFunction(collection));
    QVERIFY(collection->addFunction(a->id()));

    // B shares a Solo Frame with A's button, or with the Collection's own one
    const int soloID = bridge.addFrame(0, QRect(0, 0, 400, 200), QStringLiteral("Solo"), true);
    const int frameID = bridge.addFrame(0, QRect(0, 210, 400, 100), QStringLiteral("Looks"), false);
    VCButton *aButton = addToggle(bridge, ui.vc(), collectionInFrame ? frameID : soloID, a->id(), 10,
                                  QStringLiteral("A"));
    VCButton *bButton = addToggle(bridge, ui.vc(), soloID, b->id(), 100, QStringLiteral("B"));
    VCButton *collectionButton = addToggle(bridge, ui.vc(), collectionInFrame ? soloID : frameID,
                                           collection->id(), 200, QStringLiteral("Look"));
    // what B's start stops: the Solo Frame member that the first record started
    Function *stopped = collectionInFrame ? static_cast<Function *>(collection) : a;
    QVERIFY(aButton);
    QVERIFY(bButton);
    QVERIFY(collectionButton);

    // crossed by the same tick
    ShowCommandTrack track;
    QVERIFY(track.insert(start == QStringLiteral("vc collection")
                             ? ShowCommand::setButtonState(0, 10, collectionButton->ensureRecordingId(), true)
                             : ShowCommand::start(0, 10, start == QStringLiteral("legacy root") ? a->id()
                                                                                              : collection->id())));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 11, bButton->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    for (Function *f : std::initializer_list<Function *>{ a, b, collection })
        log.watch(f);
    int batches = 0;
    QObject probe;
    connect(show, &Show::controlBatchesReady, &probe, [&batches]() { batches++; }, Qt::QueuedConnection);

    timer->start();
    const auto stopTimer = qScopeGuard([&]() { timer->stop(); });
    const auto stopShow = qScopeGuard([&]() {
        show->stop(FunctionParent::master());
        show->stopAndWait();
    });
    show->start(timer, FunctionParent::master());

    // as by hand once the start settled: B's start stops its Solo Frame sibling
    QTRY_VERIFY(batches > 0 && recorder.pendingControlRuns() == 0 && log.starts(stopped->id()) > 0 &&
                !log.live(stopped->id()) && log.live(b->id()) && bButton->state() == VCButton::Active);
}

void ShowCommandRecorder_Test::replayedButtonOn_afterLegacyStop_endsActive_data()
{
    QTest::addColumn<bool>("catchUp");
    QTest::addColumn<bool>("stopBeforeBatch");

    // the Stop is crossed while the initial serial catch-up still holds the
    // first batch: the Stop settles before the ON is published
    QTest::newRow("initial catch-up") << true << true;
    // the first write at 0 drained the empty prefix: the ON is published in
    // the write that applies the Stop, before the Stop takes effect
    QTest::newRow("ordinary playback") << false << false;
}

void ShowCommandRecorder_Test::replayedButtonOn_afterLegacyStop_endsActive()
{
    QFETCH(bool, catchUp);
    QFETCH(bool, stopBeforeBatch);

    // Native order, seen on the timer thread. Declared first, so they outlive
    // the callbacks, which are disconnected before the timer stops.
    std::atomic<int> sequence{ 0 };
    std::atomic<int> publications{ 0 };
    std::atomic<int> starts{ 0 };
    std::atomic<int> stopAt{ 0 };
    std::atomic<int> secondBatchAt{ 0 };
    std::atomic<int> restartAt{ 0 };
    std::atomic<int> ticks{ 0 };

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    // the harness moves the timeline, nothing crosses before it says so
    show->setSyncSource(ShowRunner::External);
    show->setExternalElapsedTime(catchUp ? 200 : 0);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    QVERIFY(button);
    Scene *other = new Scene(&doc);
    other->setValue(SceneValue(scene->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(other));
    VCButton *otherButton = addToggle(bridge, ui.vc(), frameID, other->id(), 100, QStringLiteral("Other"));
    QVERIFY(otherButton);

    // an unrelated batch in between, then the Stop and the ON in the same tick
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 10, scene->id())));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 200, otherButton->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::stop(2, 500, scene->id())));
    QVERIFY(track.insert(ShowCommand::setButtonState(3, 500, button->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(scene);
    int batches = 0;
    QObject probe;
    connect(show, &Show::controlBatchesReady, &probe, [&batches]() { batches++; }, Qt::QueuedConnection);
    connect(show, &Show::controlBatchesReady, &probe, [&, show]() {
        const int at = ++sequence;
        const int n = ++publications;
        if (n == 2)
            secondBatchAt = at;
        // catching up, the Stop is crossed while this first batch is still held
        if (catchUp && n == 1)
            show->setExternalElapsedTime(500);
    }, Qt::DirectConnection);
    connect(scene, qOverload<quint32>(&Function::stopped), &probe,
            [&]() { stopAt = ++sequence; }, Qt::DirectConnection);
    connect(scene, &Function::running, &probe, [&]() {
        const int at = ++sequence;
        if (++starts == 2)
            restartAt = at;
    }, Qt::DirectConnection);
    connect(timer, &MasterTimer::tickReady, &probe, [&ticks]() { ticks++; }, Qt::DirectConnection);

    timer->start();
    const auto stopAll = qScopeGuard([&]() {
        QObject::disconnect(show, nullptr, &probe, nullptr);
        QObject::disconnect(scene, nullptr, &probe, nullptr);
        QObject::disconnect(timer, nullptr, &probe, nullptr);
        show->stop(FunctionParent::master());
        show->stopAndWait();
        timer->stop();
    });
    show->start(timer, FunctionParent::master());
    const int startTicks = ticks;

    if (!catchUp)
    {
        // a whole tick after Play: the first write ran at 0 and left catch-up
        QTRY_VERIFY(ticks >= startTicks + 2);
        show->setExternalElapsedTime(200);
    }
    // catching up, the ON can be published and applied within the same poll
    QTRY_VERIFY(batches >= 1 && recorder.pendingControlRuns() == 0);
    if (!catchUp)
        show->setExternalElapsedTime(500);

    // A busy GUI once the first batch is applied: whatever the engine does
    // next is still queued when the executor gets to the ON
    for (int i = 0; i < 5000 && stopAt == 0; i++)
        QThread::msleep(1);
    QVERIFY(stopAt > 0);

    // as by hand once the Stop took effect: the click starts it again, owned by the button
    QTRY_VERIFY(batches == 2 && recorder.pendingControlRuns() == 0 && log.starts(scene->id()) >= 2 &&
                log.live(scene->id()) && button->state() == VCButton::Active);
    QCOMPARE(stopAt < secondBatchAt, stopBeforeBatch);
    QVERIFY(stopAt < restartAt);
    QCOMPARE(log.starts(scene->id()), 2);
    QCOMPARE(log.stops(scene->id()), 1);
    QVERIFY(!scene->startedAsChild());
}

void ShowCommandRecorder_Test::replayedSubmaster_thenLegacyIntensity_endsAtLegacyValue()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    // the Submaster scales the fader beside it, which plays the Scene
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *fader = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(fader);
    VCSlider *submaster = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    QVERIFY(submaster);
    submaster->setSliderMode(VCSlider::Submaster);
    fader->setValue(200);
    for (int i = 0; i < 10 && !scene->isRunning(); i++)
        tickAndDeliver(&doc);
    QVERIFY(scene->isRunning());

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, submaster->ensureRecordingId(),
                                                        ShowControlRole::SubmasterSlider, QString(), 0.5)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 11, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QVector<qreal> intensities;
    QObject probe;
    connect(scene, &Function::attributeChanged, &probe, [&intensities](int index, qreal value) {
        if (index == Function::Intensity)
            intensities.append(value);
    });
    const auto first = [&intensities](qreal value) {
        for (int i = 0; i < intensities.count(); i++)
            if (qAbs(intensities.at(i) - value) < 0.005)
                return i;
        return -1;
    };

    show->start(doc.masterTimer(), FunctionParent::master());
    for (int i = 0; i < 10 && (first(0.2) < 0 || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    tickAndDeliver(&doc);

    // the scaled fader value landed first, the legacy value after it and stays
    const qreal scaled = (200.0 / 255.0) * (128.0 / 255.0);
    QVERIFY(first(scaled) >= 0);
    QVERIFY(first(0.2) > first(scaled));
    // within the fader's own one step echo of the legacy value
    QVERIFY(qAbs(scene->getAttributeValue(Function::Intensity) - 0.2) < 0.005);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::replayedSubmaster_retiresPendingChildWriteBeforeLegacy()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *fader = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(fader);
    VCSlider *submaster = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    QVERIFY(submaster);
    submaster->setSliderMode(VCSlider::Submaster);
    fader->setValue(200);
    for (int i = 0; i < 10 && !scene->isRunning(); i++)
        tickAndDeliver(&doc);
    QVERIFY(scene->isRunning());

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, submaster->ensureRecordingId(),
                                                        ShowControlRole::SubmasterSlider, QString(), 0.5)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 11, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 5 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    // the fader moved before the replay reaches it, its write still pending
    fader->setValue(150);
    QCoreApplication::processEvents();

    bool legacyApplied = false;
    QObject probe;
    connect(scene, &Function::attributeChanged, &probe, [&legacyApplied](int index, qreal value) {
        if (index == Function::Intensity && qFuzzyCompare(value, 0.2))
            legacyApplied = true;
    });
    for (int i = 0; i < 10 && (!legacyApplied || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    QVERIFY(legacyApplied);
    tickAndDeliver(&doc, 2);

    // Submaster 128/255: the fader shows 0.2 as qRound(0.2 / (128/255) * 255) = 102
    // but feedback is display only (user-approved), so no requantized write-back
    // replaces it: the legacy 0.2 stays, never the stale 150
    QCOMPARE(scene->getAttributeValue(Function::Intensity), 0.2);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::replayedSubmaster_childStartIsLiveBeforeLegacy()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *fader = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(fader);
    VCSlider *submaster = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    QVERIFY(submaster);
    submaster->setSliderMode(VCSlider::Submaster);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 0, submaster->ensureRecordingId(),
                                                        ShowControlRole::SubmasterSlider, QString(), 0.5)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 1, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    // The fader's write lands in the tick that crossed the record, before the
    // executor registers anything: nothing is owed any more, but the start it
    // made is still queued.
    fader->setValue(200);
    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(timer, FunctionParent::master());
    timer->timerTick();
    QCOMPARE(batches.count(), 1);
    QVERIFY(!scene->isRunning());
    QCoreApplication::processEvents();

    bool legacyApplied = false;
    QObject probe;
    connect(scene, &Function::attributeChanged, &probe, [&legacyApplied](int index, qreal value) {
        if (index == Function::Intensity && qFuzzyCompare(value, 0.2))
            legacyApplied = true;
    });
    for (int i = 0; i < 10 && (!legacyApplied || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    QVERIFY2(legacyApplied, "the legacy value found its target not started yet");
    tickAndDeliver(&doc, 2);

    // Submaster 128/255: the fader shows 0.2 as 102, but feedback is display only
    // (user-approved) and no 102/255 * 128/255 is written back
    QCOMPARE(scene->getAttributeValue(Function::Intensity), 0.2);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::replayedSliderZero_thenOnSameFunction_endsActive_data()
{
    QTest::addColumn<bool>("realTimer");

    QTest::newRow("manual timer") << false;
    QTest::newRow("real timer") << true;
}

void ShowCommandRecorder_Test::replayedSliderZero_thenOnSameFunction_endsActive()
{
    QFETCH(bool, realTimer);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    QVERIFY(slider);
    QVERIFY(button);

    // the fader and the button both play the Scene
    slider->setValue(100);
    button->applyRecordedState(true);
    for (int i = 0; i < 10 && (!scene->isRunning() || button->state() != VCButton::Active); i++)
        tickAndDeliver(&doc);
    QVERIFY(scene->isRunning());
    QCOMPARE(button->state(), VCButton::Active);

    // equal time, authored order: the fader to zero, then the button ON; the
    // ON is the last content of the Show
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, slider->ensureRecordingId(),
                                                        ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.0)));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 10, button->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(40));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(scene);
    log.watch(show);

    // as by hand once the zero took effect: the click starts the Scene again
    if (realTimer)
    {
        timer->start();
        const auto stopTimer = qScopeGuard([&]() {
            show->stop(FunctionParent::master());
            show->stopAndWait();
            timer->stop();
        });
        show->start(timer, FunctionParent::master());
        QTRY_VERIFY(log.indexOf(show->id(), false) >= 0 && recorder.pendingControlRuns() == 0 &&
                    log.starts(scene->id()) > 0 && log.live(scene->id()) &&
                    button->state() == VCButton::Active);
        return;
    }

    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 30 && log.indexOf(show->id(), false) < 0; i++)
        tickAndDeliver(&doc);
    QVERIFY(log.indexOf(show->id(), false) >= 0);
    QCOMPARE(recorder.pendingControlRuns(), 0);
    QVERIFY(log.starts(scene->id()) > 0);
    QVERIFY(scene->isRunning());
    QCOMPARE(button->state(), VCButton::Active);
}

void ShowCommandRecorder_Test::replayedSlider_reconfiguredInWriteWindow_neverRetargets_data()
{
    QTest::addColumn<QString>("change");

    QTest::newRow("rebind") << QStringLiteral("rebind");
    QTest::newRow("mode") << QStringLiteral("mode");
    QTest::newRow("attribute") << QStringLiteral("attribute");
}

void ShowCommandRecorder_Test::replayedSlider_reconfiguredInWriteWindow_neverRetargets()
{
    QFETCH(QString, change);

    // Written on the timer thread only. Declared first, so they outlive the
    // callbacks, which are disconnected before anything else goes away.
    std::atomic<int> ticks{ 0 };
    std::atomic<bool> otherLive{ false };
    std::atomic<bool> otherGotReplay{ false };

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Scene *other = new Scene(&doc);
    other->setValue(SceneValue(scene->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(other));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, slider->ensureRecordingId(),
                                                        ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.8)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QSignalSpy batches(show, &Show::controlBatchesReady);
    QSignalSpy retired(slider, &VCSlider::recordedWriteRetired);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 5 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    // the replayed 0.8 now waits for a write that no tick has run
    QCoreApplication::processEvents();
    QCOMPARE(recorder.pendingControlRuns(), 1);

    QObject probe;
    connect(timer, &MasterTimer::tickReady, &probe, [&]() {
        if (other->isRunning())
            otherLive = true;
        if (qFuzzyCompare(other->getAttributeValue(Function::Intensity), 0.8))
            otherGotReplay = true;
        ticks++;
    }, Qt::DirectConnection);
    // the real timer runs while the setter has published the new configuration
    // to observers, until the engine has written and drained after it
    const auto window = [&]() {
        const int from = ticks;
        timer->start();
        for (int i = 0; i < 5000 && ticks < from + 3; i++)
            QThread::msleep(1);
        timer->stop();
    };
    if (change == QStringLiteral("rebind"))
    {
        connect(slider, &VCSlider::controlledFunctionChanged, &probe, window);
        slider->setControlledFunction(other->id());
    }
    else if (change == QStringLiteral("mode"))
    {
        connect(slider, &VCSlider::sliderModeChanged, &probe, window);
        slider->setSliderMode(VCSlider::Level);
    }
    else
    {
        connect(slider, &VCSlider::controlledAttributeChanged, &probe, window);
        slider->setControlledAttribute(Function::Intensity + 1);
    }
    QObject::disconnect(timer, nullptr, &probe, nullptr);
    QObject::disconnect(slider, nullptr, &probe, nullptr);
    QVERIFY(ticks >= 3);
    QCoreApplication::processEvents();

    // the stale replay never reaches the new configuration: it is cancelled
    QVERIFY(!otherLive);
    QVERIFY(!otherGotReplay);
    QVERIFY(!retired.isEmpty());
    QCOMPARE(retired.first().at(1).toInt(), int(VCSlider::RecordedWriteCancelled));

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::replayedLevelSlider_channelsReplacedInWriteWindow_neverRetargets_data()
{
    QTest::addColumn<QString>("replace");

    // the remap publishes the new channels and notifies, then the timer runs
    QTest::newRow("remap") << QStringLiteral("remap");
    // the new channel is added, the timer runs, then the old one is removed
    QTest::newRow("add, then remove") << QStringLiteral("add then remove");
}

void ShowCommandRecorder_Test::replayedLevelSlider_channelsReplacedInWriteWindow_neverRetargets()
{
    QFETCH(QString, replace);

    // Written on the timer thread only. Declared first, so they outlive the
    // callbacks, which are disconnected before anything else goes away.
    std::atomic<int> ticks{ 0 };

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    const quint32 fixtureID = scene->values().first().fxi;
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    QVERIFY(slider);
    slider->setSliderMode(VCSlider::Level);
    slider->addLevelChannel(fixtureID, 0);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, slider->ensureRecordingId(),
                                                        ShowControlRole::LevelSlider, QString(), 0.8)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QSignalSpy batches(show, &Show::controlBatchesReady);
    QSignalSpy retired(slider, &VCSlider::recordedWriteRetired);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 5 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    // the replayed 0.8 now waits for a write that no tick has run
    QCoreApplication::processEvents();
    QCOMPARE(recorder.pendingControlRuns(), 1);

    QObject probe;
    connect(timer, &MasterTimer::tickReady, &probe, [&]() { ticks++; }, Qt::DirectConnection);
    // the real timer runs once the new channel is published, until the engine
    // has written after it
    const auto window = [&]() {
        const int from = ticks;
        timer->start();
        for (int i = 0; i < 5000 && ticks < from + 3; i++)
            QThread::msleep(1);
        timer->stop();
    };
    if (replace == QStringLiteral("remap"))
    {
        connect(slider, &VCSlider::channelsCountChanged, &probe, window);
        QMap<SceneValue, SceneValue> remap;
        remap.insert(SceneValue(fixtureID, 0), SceneValue(fixtureID, 2));
        slider->remapChannels(remap);
    }
    else
    {
        slider->addLevelChannel(fixtureID, 2);
        window();
        slider->removeLevelChannel(fixtureID, 0);
    }
    QObject::disconnect(timer, nullptr, &probe, nullptr);
    QObject::disconnect(slider, nullptr, &probe, nullptr);
    QVERIFY(ticks >= 3);
    QCoreApplication::processEvents();

    // no write claimed the stale replay for the newly selected channels: it is cancelled
    QVERIFY(!retired.isEmpty());
    QCOMPARE(retired.first().at(1).toInt(), int(VCSlider::RecordedWriteCancelled));

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::replayedGrandMasterPosition_completesOnReturn()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    QVERIFY(slider);
    slider->setSliderMode(VCSlider::GrandMaster);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 10, slider->ensureRecordingId(),
                                                        ShowControlRole::GrandMasterSlider,
                                                        QString(), 0.5)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 5 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    QCoreApplication::processEvents();

    // applied inside the setter, so nothing is left to wait for
    QCOMPARE(recorder.pendingControlRuns(), 0);
    QCOMPARE(int(doc.inputOutputMap()->grandMasterValue()), 128);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::levelSliderValue_outputOnlyWhenMeantForDmx_data()
{
    QTest::addColumn<bool>("setDMX");
    QTest::addColumn<int>("faders");

    // the channel monitor mirrors output back into the slider
    QTest::newRow("monitor update") << false << 0;
    QTest::newRow("live value") << true << 1;
}

void ShowCommandRecorder_Test::levelSliderValue_outputOnlyWhenMeantForDmx()
{
    QFETCH(bool, setDMX);
    QFETCH(int, faders);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    QVERIFY(slider);
    slider->setSliderMode(VCSlider::Level);
    slider->setMonitorEnabled(true);
    slider->addLevelChannel(scene->values().first().fxi, 0);

    QList<Universe *> universes = doc.inputOutputMap()->claimUniverses();
    const auto release = qScopeGuard([&doc]() { doc.inputOutputMap()->releaseUniverses(false); });
    // the value the slider starts from is output once
    slider->writeDMX(doc.masterTimer(), universes);
    const int before = universes.first()->faders().count();

    slider->setValue(200, setDMX, false);
    slider->writeDMX(doc.masterTimer(), universes);

    QCOMPARE(universes.first()->faders().count() - before, faders);
}

void ShowCommandRecorder_Test::adjustFaderFeedback_isShownNotWrittenBack_data()
{
    QTest::addColumn<bool>("solo");
    QTest::addColumn<bool>("adjacent");
    QTest::addColumn<qreal>("finalIntensity");
    QTest::addColumn<int>("finalValue");

    // of two values at the same time the later one wins, as without a fader
    QTest::newRow("adjacent 0.2 then 0.8") << false << true << 0.8 << 204;
    // a value the fader only shows is no fader start its solo frame reacts to
    QTest::newRow("single 0.2 in a solo frame") << true << false << 0.2 << 51;
}

void ShowCommandRecorder_Test::adjustFaderFeedback_isShownNotWrittenBack()
{
    QFETCH(bool, solo);
    QFETCH(bool, adjacent);
    QFETCH(qreal, finalIntensity);
    QFETCH(int, finalValue);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Scene *sibling = new Scene(&doc);
    sibling->setValue(SceneValue(scene->values().first().fxi, 1, 123));
    QVERIFY(doc.addFunction(sibling));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), solo);
    VCSlider *fader = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(fader);
    fader->requestUserValue(204);
    for (int i = 0; i < 10 && !scene->isRunning(); i++)
        tickAndDeliver(&doc);
    QVERIFY(scene->isRunning());
    VCButton *button = addToggle(bridge, ui.vc(), frameID, sibling->id(), 10, QStringLiteral("Sibling"));
    QVERIFY(button);
    button->requestUserStateChange(true);
    for (int i = 0; i < 10 && !sibling->isRunning(); i++)
        tickAndDeliver(&doc);
    QVERIFY(sibling->isRunning());

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setIntensity(0, 10, scene->id(), 0.2)));
    if (adjacent)
        QVERIFY(track.insert(ShowCommand::setIntensity(1, 10, scene->id(), 0.8)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    show->start(doc.masterTimer(), FunctionParent::master());
    for (int i = 0; i < 20 && show->commandPosition() <= 10; i++)
        tickAndDeliver(&doc);
    QVERIFY(show->commandPosition() > 10);
    // a write-back of the fader would land in the next ticks
    tickAndDeliver(&doc, 3);

    QCOMPARE(scene->getAttributeValue(Function::Intensity), finalIntensity);
    QCOMPARE(fader->value(), finalValue);
    QVERIFY(scene->isRunning());
    QVERIFY(sibling->isRunning());
    QCOMPARE(button->state(), VCButton::Active);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::adjustFaderStopFeedback_leavesTheAttributeReset_data()
{
    QTest::addColumn<int>("low");
    QTest::addColumn<int>("high");
    QTest::addColumn<int>("value");

    QTest::newRow("range 100..200") << 100 << 200 << 180;
    QTest::newRow("full range") << 0 << 255 << 204;
}

void ShowCommandRecorder_Test::adjustFaderStopFeedback_leavesTheAttributeReset()
{
    QFETCH(int, low);
    QFETCH(int, high);
    QFETCH(int, value);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *fader = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(fader);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Button"));
    QVERIFY(button);
    fader->setRangeLowLimit(low);
    fader->setRangeHighLimit(high);
    fader->requestUserValue(value);
    for (int i = 0; i < 10 && !scene->isRunning(); i++)
        tickAndDeliver(&doc);
    QVERIFY(scene->isRunning());

    button->requestUserStateChange(true);
    for (int i = 0; i < 10 && button->state() != VCButton::Active; i++)
        tickAndDeliver(&doc);
    QCOMPARE(button->state(), VCButton::Active);
    button->requestUserStateChange(false);
    for (int i = 0; i < 10 && scene->isRunning(); i++)
        tickAndDeliver(&doc);
    QVERIFY(!scene->isRunning());
    // a write-back of the zero the fader shows would land in the next ticks
    tickAndDeliver(&doc, 3);

    QCOMPARE(fader->value(), 0);
    QVERIFY(!scene->isRunning());
    // the stop reset every override (Function::postRun -> resetAttributes),
    // so the Scene is back to its own intensity
    QCOMPARE(scene->getAttributeValue(Function::Intensity), qreal(1.0));
}

void ShowCommandRecorder_Test::adjustFaderFeedback_isWhatACancelRestores()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *fader = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(fader);
    fader->requestUserValue(204);
    for (int i = 0; i < 10 && !qFuzzyCompare(scene->getAttributeValue(Function::Intensity), 0.8); i++)
        tickAndDeliver(&doc);
    QCOMPARE(scene->getAttributeValue(Function::Intensity), 0.8);

    // another writer of the single Intensity override: the fader shows it
    scene->requestAttributeOverride(Function::Intensity, 0.2);
    QCOMPARE(fader->value(), 51);

    QVERIFY(fader->applyRecordedPosition(0.6) != 0);
    QCOMPARE(fader->value(), 153);
    fader->setDisabled(true);

    // the cancel puts back what the fader showed, and writes nothing
    QCOMPARE(fader->value(), 51);
    tickAndDeliver(&doc, 3);
    QCOMPARE(scene->getAttributeValue(Function::Intensity), 0.2);
}

void ShowCommandRecorder_Test::adjustFaderOnFunctionStart_stillWritesItsValue_data()
{
    QTest::addColumn<bool>("masterStart");
    QTest::addColumn<bool>("targetRunning");
    QTest::addColumn<bool>("siblingRunning");
    QTest::addColumn<int>("finalValue");

    // the fader re-applies what it shows, a start its solo frame reacts to
    QTest::newRow("button outside the solo frame starts it") << false << true << false << 255;
    // the fader at zero stops what anything else starts
    QTest::newRow("master start with the fader at zero") << true << false << true << 0;
}

void ShowCommandRecorder_Test::adjustFaderOnFunctionStart_stillWritesItsValue()
{
    QFETCH(bool, masterStart);
    QFETCH(bool, targetRunning);
    QFETCH(bool, siblingRunning);
    QFETCH(int, finalValue);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Scene *sibling = new Scene(&doc);
    sibling->setValue(SceneValue(scene->values().first().fxi, 1, 123));
    QVERIFY(doc.addFunction(sibling));
    VCBridgeV5 bridge(&doc, ui.vc());
    const int soloID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Solo"), true);
    const int outsideID = bridge.addFrame(0, QRect(0, 310, 400, 100), QStringLiteral("Outside"), false);
    VCSlider *fader = addAdjustSlider(bridge, ui.vc(), soloID, scene->id());
    QVERIFY(fader);
    VCButton *siblingButton = addToggle(bridge, ui.vc(), soloID, sibling->id(), 100, QStringLiteral("Sibling"));
    QVERIFY(siblingButton);
    VCButton *button = addToggle(bridge, ui.vc(), outsideID, scene->id(), 10, QStringLiteral("Button"));
    QVERIFY(button);
    siblingButton->requestUserStateChange(true);
    for (int i = 0; i < 10 && !sibling->isRunning(); i++)
        tickAndDeliver(&doc);
    QVERIFY(sibling->isRunning());
    QCOMPARE(fader->value(), 0);

    if (masterStart)
        scene->start(doc.masterTimer(), FunctionParent::master());
    else
        button->requestUserStateChange(true);
    tickAndDeliver(&doc, 10);

    QCOMPARE(scene->isRunning(), targetRunning);
    QCOMPARE(sibling->isRunning(), siblingRunning);
    QCOMPARE(fader->value(), finalValue);

    scene->stop(FunctionParent::master());
    sibling->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::adjustFaderOutputRequest_stillWrites_data()
{
    QTest::addColumn<bool>("audioOrigin");

    QTest::newRow("audio origin ingress") << true;
    // audio triggers, scripts and web access call it this way
    QTest::newRow("programmatic setValue for DMX") << false;
}

void ShowCommandRecorder_Test::adjustFaderOutputRequest_stillWrites()
{
    QFETCH(bool, audioOrigin);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *fader = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(fader);

    if (audioOrigin)
        fader->requestUserValue(153, true, ShowCommandOrigin::Audio);
    else
        fader->setValue(153, true, true);
    for (int i = 0; i < 10 && (!scene->isRunning() ||
                               !qFuzzyCompare(scene->getAttributeValue(Function::Intensity), 0.6)); i++)
        tickAndDeliver(&doc);

    QVERIFY(scene->isRunning());
    QCOMPARE(scene->getAttributeValue(Function::Intensity), 0.6);

    scene->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::seek_restoresRecordedButtonOn()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    QVERIFY(button);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 10000, button->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(scene);
    log.watchReceipts(show);

    show->start(doc.masterTimer(), FunctionParent::master());
    tickAndDeliver(&doc);
    QCOMPARE(button->state(), VCButton::Inactive);

    show->requestSeek(15000);
    for (int i = 0; i < 50 && (log.receipts() == 0 || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);

    QCOMPARE(button->state(), VCButton::Active);
    QVERIFY(log.live(scene->id()));
    QCOMPARE(log.starts(scene->id()), 1);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::seek_restoresMixedHistoryInAuthoredOrder_data()
{
    QTest::addColumn<bool>("legacyFirst");
    QTest::addColumn<qreal>("intensity");

    QTest::newRow("slider .8, then legacy .2") << false << qreal(0.2);
    QTest::newRow("legacy .2, then slider .8") << true << qreal(0.8);
}

void ShowCommandRecorder_Test::seek_restoresMixedHistoryInAuthoredOrder()
{
    QFETCH(bool, legacyFirst);
    QFETCH(qreal, intensity);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);
    QCOMPARE(slider->value(), 0);

    const ShowCommand position = ShowCommand::setSliderPosition(0, legacyFirst ? 11000 : 10000,
                                                                slider->ensureRecordingId(),
                                                                ShowControlRole::AdjustSlider,
                                                                QStringLiteral("Intensity"), 0.8);
    ShowCommandTrack track;
    QVERIFY(track.insert(position));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, legacyFirst ? 10000 : 11000, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QVector<qreal> intensities;
    QObject probe;
    connect(scene, &Function::attributeChanged, &probe, [&intensities](int index, qreal value) {
        if (index == Function::Intensity)
            intensities.append(value);
    });
    const auto applied = [&intensities](qreal value) {
        return std::any_of(intensities.cbegin(), intensities.cend(),
                           [value](qreal v) { return qFuzzyCompare(v, value); });
    };
    QSignalSpy retired(slider, &VCSlider::recordedWriteRetired);
    NativeLog log;
    log.watchReceipts(show);

    show->start(doc.masterTimer(), FunctionParent::master());
    tickAndDeliver(&doc);
    QVERIFY(!scene->isRunning());

    show->requestSeek(15000);
    // the slider write retired and was seen live; the legacy value, when it
    // comes last, reached the running target
    for (int i = 0; i < 50 && (retired.isEmpty() || log.receipts() == 0 || recorder.pendingControlRuns() > 0 ||
                               (!legacyFirst && !applied(0.2))); i++)
        tickAndDeliver(&doc);

    QCOMPARE(retired.count(), 1);
    QVERIFY(scene->isRunning());
    QCOMPARE(scene->getAttributeValue(Function::Intensity), intensity);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::play_fromStoppedCursor_restoresOnce()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Scene *other = new Scene(&doc);
    other->setValue(SceneValue(scene->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(other));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *before = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Before"));
    VCButton *atCursor = addToggle(bridge, ui.vc(), frameID, other->id(), 100, QStringLiteral("At cursor"));
    QVERIFY(before);
    QVERIFY(atCursor);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 10000, before->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 15000, atCursor->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(scene);
    log.watch(other);
    log.watchReceipts(show);
    QSignalSpy batches(show, &Show::controlBatchesReady);

    // a stopped cursor moving over the records has no effect
    show->requestSeek(15000);
    tickAndDeliver(&doc, 3);
    QCOMPARE(batches.count(), 0);
    QCOMPARE(before->state(), VCButton::Inactive);
    QCOMPARE(atCursor->state(), VCButton::Inactive);

    show->start(doc.masterTimer(), FunctionParent::master(), 15000);
    for (int i = 0; i < 50 && (log.receipts() == 0 || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    // one catch-up batch; playing on past the cursor replays neither record
    const int catchUpBatches = batches.count();
    for (int i = 0; i < 50 && show->commandPosition() < 15500; i++)
        tickAndDeliver(&doc);

    QCOMPARE(catchUpBatches, 1);
    QCOMPARE(batches.count(), 1);
    QCOMPARE(before->state(), VCButton::Active);
    QCOMPARE(atCursor->state(), VCButton::Active);
    QCOMPARE(log.starts(scene->id()), 1);
    QCOMPARE(log.starts(other->id()), 1);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::playThrough_reachesLiteralNativeState_data()
{
    QTest::addColumn<QString>("rig");
    QTest::addColumn<quint32>("destination");
    QTest::addColumn<QString>("state");

    // Records at 1000, 2000 and 2400; every rig starts Inactive and stopped,
    // then plays through to the destination.
    const QList<std::tuple<const char *, QString, quint32, QString>> rows{
        { "solo, both off", QStringLiteral("solo"), 2500, QStringLiteral("A=Inactive,B=Inactive;a=off,b=off") },
        { "solo, B wins", QStringLiteral("solo"), 2100, QStringLiteral("A=Inactive,B=Active;a=off,b=on") },
        { "aliases, one off", QStringLiteral("alias"), 2500, QStringLiteral("A=Inactive,B=Inactive;a=off") },
        { "collection, child off, on again", QStringLiteral("collection"), 2500,
          QStringLiteral("Look=Active,X=Inactive;look=on,x=off,y=on") },
        { "monitored sibling stops", QStringLiteral("monitored"), 2500,
          QStringLiteral("Look=Active,X=Inactive,B=Active;look=on,x=off,y=on,b=on") },
        { "monitored sibling excluded", QStringLiteral("excluded"), 2500,
          QStringLiteral("Look=Active,X=Monitoring,B=Active;look=on,x=on,y=on,b=on") },
        { "monitored startup sibling", QStringLiteral("startup"), 2500,
          QStringLiteral("Look=Active,X=Inactive,B=Active;look=on,x=off,y=on,b=on") },
        { "equal intensity", QStringLiteral("intensity"), 2500, QStringLiteral("S=128,B=Active;a=on,b=on") },
        { "shared intensity", QStringLiteral("shared"), 2500, QStringLiteral("S=204,T=204;a=on@0.8") },
        { "fed-back slider write", QStringLiteral("fed back"), 2500,
          QStringLiteral("S=255,A=Active,B=Active;a=on,b=on") },
        { "fed-back zero, button on again", QStringLiteral("zero"), 2500, QStringLiteral("A=Active;a=on") },
        { "nested collection already running", QStringLiteral("nested"), 2500,
          QStringLiteral("Outer=Active,Inner=Active,X=Inactive,B=Active;outer=on,inner=on,x=off,y=on,b=on") },
        { "collection member stops solo sibling", QStringLiteral("member"), 2500,
          QStringLiteral("Look=Inactive,X=Inactive,B=Inactive;look=off,x=off,y=off,b=off") },
    };
    for (const auto &row : rows)
        QTest::newRow(std::get<0>(row)) << std::get<1>(row) << std::get<2>(row) << std::get<3>(row);
}

void ShowCommandRecorder_Test::playThrough_reachesLiteralNativeState()
{
    QFETCH(QString, rig);
    QFETCH(quint32, destination);
    QFETCH(QString, state);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *a = addTarget(&doc);
    QVERIFY(a);
    const auto addScene = [&doc, a](const QString &name, quint32 channel) {
        Scene *scene = new Scene(&doc);
        scene->setName(name);
        scene->setValue(SceneValue(a->values().first().fxi, channel, 120));
        doc.addFunction(scene);
        return scene;
    };
    Scene *b = addScene(QStringLiteral("b"), 1);
    Scene *y = addScene(QStringLiteral("y"), 2);
    Collection *look = new Collection(&doc);
    look->setName(QStringLiteral("look"));
    QVERIFY(doc.addFunction(look));
    look->addFunction(a->id());
    look->addFunction(y->id());
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());

    // name -> control, and name -> function, reported in this order
    QVector<QPair<QString, VCWidget *>> controls;
    QVector<QPair<QString, Function *>> functions;
    ShowCommandTrack track;
    const auto button = [&](int frameID, Function *f, int x, const QString &name) {
        VCButton *control = addToggle(bridge, ui.vc(), frameID, f->id(), x, name);
        controls.append(qMakePair(name, control));
        return control->ensureRecordingId();
    };
    const auto on = [&track](quint32 id, quint32 time, const QUuid &control, bool value) {
        return track.insert(ShowCommand::setButtonState(id, time, control, value));
    };
    if (rig == QStringLiteral("solo") || rig == QStringLiteral("alias"))
    {
        const bool solo = rig == QStringLiteral("solo");
        const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), solo);
        const QUuid first = button(frameID, a, 10, QStringLiteral("A"));
        const QUuid second = button(frameID, solo ? b : a, 100, QStringLiteral("B"));
        functions.append(qMakePair(QStringLiteral("a"), a));
        if (solo)
            functions.append(qMakePair(QStringLiteral("b"), b));
        QVERIFY(on(0, 1000, first, true));
        QVERIFY(on(1, 2000, second, true));
        QVERIFY(on(2, 2400, solo ? second : first, false));
    }
    else if (rig == QStringLiteral("collection"))
    {
        const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
        const QUuid lookButton = button(frameID, look, 10, QStringLiteral("Look"));
        const QUuid child = button(frameID, a, 100, QStringLiteral("X"));
        functions = { qMakePair(QStringLiteral("look"), static_cast<Function *>(look)),
                      qMakePair(QStringLiteral("x"), static_cast<Function *>(a)),
                      qMakePair(QStringLiteral("y"), static_cast<Function *>(y)) };
        QVERIFY(on(0, 1000, lookButton, true));
        QVERIFY(on(1, 2000, child, false));
        QVERIFY(on(2, 2400, lookButton, true));
    }
    else if (rig == QStringLiteral("zero"))
    {
        // S and A share a's intensity; S's zero stops a, A starts it again
        const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
        VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, a->id());
        QVERIFY(slider);
        const QUuid fullButton = button(frameID, a, 10, QStringLiteral("A"));
        functions = { qMakePair(QStringLiteral("a"), static_cast<Function *>(a)) };
        const auto position = [&track, slider](quint32 id, quint32 time, qreal value) {
            return track.insert(ShowCommand::setSliderPosition(id, time, slider->ensureRecordingId(),
                                                               ShowControlRole::AdjustSlider,
                                                               QStringLiteral("Intensity"), value));
        };
        QVERIFY(position(0, 1000, 0.8));
        QVERIFY(on(1, 2000, fullButton, true));
        QVERIFY(position(2, 2200, 0.0));
        QVERIFY(on(3, 2400, fullButton, true));
    }
    else if (rig == QStringLiteral("nested"))
    {
        // Outer holds look (X's a and y); y keeps look running once X stopped
        Collection *outer = new Collection(&doc);
        outer->setName(QStringLiteral("outer"));
        QVERIFY(doc.addFunction(outer));
        outer->addFunction(look->id());
        const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 100), QStringLiteral("Frame"), false);
        const QUuid outerButton = button(frameID, outer, 10, QStringLiteral("Outer"));
        const QUuid innerButton = button(frameID, look, 100, QStringLiteral("Inner"));
        const int soloID = bridge.addFrame(0, QRect(0, 110, 400, 100), QStringLiteral("Solo"), true);
        const QUuid child = button(soloID, a, 10, QStringLiteral("X"));
        const QUuid other = button(soloID, b, 100, QStringLiteral("B"));
        functions = { qMakePair(QStringLiteral("outer"), static_cast<Function *>(outer)),
                      qMakePair(QStringLiteral("inner"), static_cast<Function *>(look)),
                      qMakePair(QStringLiteral("x"), static_cast<Function *>(a)),
                      qMakePair(QStringLiteral("y"), static_cast<Function *>(y)),
                      qMakePair(QStringLiteral("b"), static_cast<Function *>(b)) };
        QVERIFY(on(0, 1000, innerButton, true));
        QVERIFY(on(1, 1500, child, false));
        QVERIFY(on(2, 2000, other, true));
        QVERIFY(on(3, 2400, outerButton, true));
    }
    else if (rig == QStringLiteral("fed back"))
    {
        // S shares B's Solo Frame; A plays S's function from outside it
        const int soloID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Solo"), true);
        VCSlider *slider = addAdjustSlider(bridge, ui.vc(), soloID, a->id());
        QVERIFY(slider);
        controls.append(qMakePair(QStringLiteral("S"), static_cast<VCWidget *>(slider)));
        const int frameID = bridge.addFrame(0, QRect(0, 310, 400, 100), QStringLiteral("Frame"), false);
        const QUuid outside = button(frameID, a, 10, QStringLiteral("A"));
        const QUuid other = button(soloID, b, 10, QStringLiteral("B"));
        functions = { qMakePair(QStringLiteral("a"), static_cast<Function *>(a)),
                      qMakePair(QStringLiteral("b"), static_cast<Function *>(b)) };
        const auto full = [&track, slider](quint32 id, quint32 time) {
            return track.insert(ShowCommand::setSliderPosition(id, time, slider->ensureRecordingId(),
                                                               ShowControlRole::AdjustSlider,
                                                               QStringLiteral("Intensity"), 1.0));
        };
        QVERIFY(full(0, 1000));
        QVERIFY(on(1, 2000, outside, true));
        QVERIFY(on(2, 2200, other, true));
        QVERIFY(full(3, 2400));
    }
    else if (rig == QStringLiteral("member"))
    {
        // B plays, then the Collection starts X in B's Solo Frame, then stops
        const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 100), QStringLiteral("Frame"), false);
        const QUuid lookButton = button(frameID, look, 10, QStringLiteral("Look"));
        const int soloID = bridge.addFrame(0, QRect(0, 110, 400, 100), QStringLiteral("Solo"), true);
        button(soloID, a, 10, QStringLiteral("X"));
        const QUuid other = button(soloID, b, 100, QStringLiteral("B"));
        functions = { qMakePair(QStringLiteral("look"), static_cast<Function *>(look)),
                      qMakePair(QStringLiteral("x"), static_cast<Function *>(a)),
                      qMakePair(QStringLiteral("y"), static_cast<Function *>(y)),
                      qMakePair(QStringLiteral("b"), static_cast<Function *>(b)) };
        QVERIFY(on(0, 1000, other, true));
        QVERIFY(on(1, 2000, lookButton, true));
        QVERIFY(on(2, 2400, lookButton, false));
    }
    else if (rig == QStringLiteral("shared"))
    {
        // two faders of one intensity: each write moves the other one's feedback
        const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
        functions = { qMakePair(QStringLiteral("a"), static_cast<Function *>(a)) };
        quint32 id = 0;
        for (const auto &record : { qMakePair(QStringLiteral("S"), qreal(0.8)), qMakePair(QStringLiteral("T"), qreal(0.2)),
                                    qMakePair(QStringLiteral("S"), qreal(0.8)) })
        {
            if (controls.count() < 2 && std::none_of(controls.cbegin(), controls.cend(),
                                                     [&record](const auto &c) { return c.first == record.first; }))
            {
                VCSlider *added = addAdjustSlider(bridge, ui.vc(), frameID, a->id());
                QVERIFY(added);
                controls.append(qMakePair(record.first, static_cast<VCWidget *>(added)));
            }
            VCWidget *fader = std::find_if(controls.cbegin(), controls.cend(),
                                           [&record](const auto &c) { return c.first == record.first; })->second;
            QVERIFY(track.insert(ShowCommand::setSliderPosition(id, id == 0 ? 1000 : id == 1 ? 2000 : 2400,
                                                                fader->ensureRecordingId(),
                                                                ShowControlRole::AdjustSlider,
                                                                QStringLiteral("Intensity"), record.second)));
            id++;
        }
    }
    else if (rig == QStringLiteral("intensity"))
    {
        const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), true);
        VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, a->id());
        QVERIFY(slider);
        controls.append(qMakePair(QStringLiteral("S"), static_cast<VCWidget *>(slider)));
        const QUuid other = button(frameID, b, 10, QStringLiteral("B"));
        functions = { qMakePair(QStringLiteral("a"), static_cast<Function *>(a)),
                      qMakePair(QStringLiteral("b"), static_cast<Function *>(b)) };
        const auto position = [&track, slider](quint32 id, quint32 time, qreal value) {
            return track.insert(ShowCommand::setSliderPosition(id, time, slider->ensureRecordingId(),
                                                               ShowControlRole::AdjustSlider,
                                                               QStringLiteral("Intensity"), value));
        };
        QVERIFY(position(0, 1000, 0.5));
        QVERIFY(on(1, 2000, other, true));
        QVERIFY(position(2, 2400, 0.5));
    }
    else
    {
        // the Collection makes X monitor its member; B starts in X's Solo Frame
        const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 100), QStringLiteral("Frame"), false);
        const QUuid lookButton = button(frameID, look, 10, QStringLiteral("Look"));
        const int soloID = bridge.addFrame(0, QRect(0, 110, 400, 100), QStringLiteral("Solo"), true);
        const QUuid child = button(soloID, a, 10, QStringLiteral("X"));
        const QUuid other = button(soloID, b, 100, QStringLiteral("B"));
        Q_UNUSED(child)
        qobject_cast<VCSoloFrame *>(ui.vc()->widget(soloID))->setExcludeMonitoredFunctions(rig != QStringLiteral("monitored"));
        if (rig == QStringLiteral("startup"))
            doc.setStartupFunction(a->id());
        functions = { qMakePair(QStringLiteral("look"), static_cast<Function *>(look)),
                      qMakePair(QStringLiteral("x"), static_cast<Function *>(a)),
                      qMakePair(QStringLiteral("y"), static_cast<Function *>(y)),
                      qMakePair(QStringLiteral("b"), static_cast<Function *>(b)) };
        QVERIFY(on(0, 1000, lookButton, true));
        QVERIFY(on(1, 2000, other, true));
    }
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    for (const auto &control : controls)
        QVERIFY(control.second != nullptr);

    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(doc.masterTimer(), FunctionParent::master());
    tickAndDeliver(&doc);
    QCOMPARE(batches.count(), 0);
    for (int i = 0; i < 400 && (show->commandPosition() < destination || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    // settled before the next record
    QVERIFY(show->commandPosition() < (destination < 2400 ? 2400u : 60000u));

    QStringList buttons;
    for (const auto &control : controls)
    {
        VCSlider *slider = qobject_cast<VCSlider *>(control.second);
        buttons.append(control.first + QLatin1Char('=') +
                       (slider != nullptr ? QString::number(slider->value())
                                          : QString::fromLatin1(QMetaEnum::fromType<VCButton::ButtonState>()
                                                .valueToKey(qobject_cast<VCButton *>(control.second)->state()))));
    }
    QStringList running;
    for (const auto &function : functions)
    {
        running.append(function.first + (function.second->isRunning() ? QStringLiteral("=on") : QStringLiteral("=off")));
        if (rig == QStringLiteral("shared"))
            running.last() += QLatin1Char('@') + QString::number(function.second->getAttributeValue(Function::Intensity));
    }
    QCOMPARE(buttons.join(QLatin1Char(',')) + QLatin1Char(';') + running.join(QLatin1Char(',')), state);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::catchUp_soloPrefixRunsSerially()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *a = addTarget(&doc);
    QVERIFY(a);
    Scene *b = new Scene(&doc);
    b->setValue(SceneValue(a->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(b));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int soloID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Solo"), true);
    VCButton *buttonA = addToggle(bridge, ui.vc(), soloID, a->id(), 10, QStringLiteral("A"));
    VCButton *buttonB = addToggle(bridge, ui.vc(), soloID, b->id(), 100, QStringLiteral("B"));
    QVERIFY(buttonA);
    QVERIFY(buttonB);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 10000, buttonA->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 20000, buttonB->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setButtonState(2, 22000, buttonB->ensureRecordingId(), false)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(a);
    log.watch(b);

    // Play from a stopped cursor past all three records
    show->start(doc.masterTimer(), FunctionParent::master(), 25000);
    for (int i = 0; i < 100 && (log.indexOf(b->id(), false) < 0 || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    tickAndDeliver(&doc, 5);

    QCOMPARE(buttonA->state(), VCButton::Inactive);
    QCOMPARE(buttonB->state(), VCButton::Inactive);
    QCOMPARE(log.starts(a->id()), 1);
    QCOMPARE(log.stops(a->id()), 1);
    QCOMPARE(log.starts(b->id()), 1);
    QCOMPARE(log.stops(b->id()), 1);
    // A ON; B ON stops A through the Solo Frame as it starts B; then B OFF
    QVERIFY(log.indexOf(a->id(), true) < log.indexOf(a->id(), false));
    QVERIFY(log.indexOf(a->id(), false) < log.indexOf(b->id(), true));
    QVERIFY(log.indexOf(b->id(), true) < log.indexOf(b->id(), false));

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::catchUp_backwardSeekRunsPrefixOnce()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *a = addTarget(&doc);
    QVERIFY(a);
    const auto addScene = [&doc, a](quint32 channel) {
        Scene *scene = new Scene(&doc);
        scene->setValue(SceneValue(a->values().first().fxi, channel, 120));
        doc.addFunction(scene);
        return scene;
    };
    Scene *b = addScene(1);
    Scene *c = addScene(2);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *buttonA = addToggle(bridge, ui.vc(), frameID, a->id(), 10, QStringLiteral("A"));
    VCButton *buttonB = addToggle(bridge, ui.vc(), frameID, b->id(), 100, QStringLiteral("B"));
    VCButton *buttonC = addToggle(bridge, ui.vc(), frameID, c->id(), 190, QStringLiteral("C"));
    QVERIFY(buttonA && buttonB && buttonC);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 5000, buttonC->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 10000, buttonA->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setButtonState(2, 20000, buttonB->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setButtonState(3, 22000, buttonA->ensureRecordingId(), false)));
    QVERIFY(track.insert(ShowCommand::setButtonState(4, 28000, buttonC->ensureRecordingId(), false)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(a);
    log.watch(b);
    log.watch(c);
    log.watchReceipts(show);
    const auto settle = [&]() {
        for (int i = 0; i < 50; i++)
        {
            const int receipts = log.receipts();
            tickAndDeliver(&doc);
            if (receipts == log.receipts() && recorder.pendingControlRuns() == 0)
                break;
        }
    };

    show->start(doc.masterTimer(), FunctionParent::master(), 30000);
    settle();
    QCOMPARE(buttonC->state(), VCButton::Inactive);
    QCOMPARE(log.stops(c->id()), 1);

    // back while Playing: the prefix up to 25s again, the OFF at 28s only when crossed
    show->requestSeek(25000);
    settle();
    QVERIFY(show->commandPosition() < 28000);
    QCOMPARE(buttonA->state(), VCButton::Inactive);
    QCOMPARE(buttonB->state(), VCButton::Active);
    QCOMPARE(buttonC->state(), VCButton::Active);
    QCOMPARE(log.starts(a->id()), 2);
    QCOMPARE(log.stops(a->id()), 2);
    QCOMPARE(log.starts(b->id()), 1);
    QCOMPARE(log.stops(b->id()), 0);
    QCOMPARE(log.starts(c->id()), 2);
    QCOMPARE(log.stops(c->id()), 1);

    for (int i = 0; i < 400 && show->commandPosition() < 28500; i++)
        tickAndDeliver(&doc);
    settle();
    QCOMPARE(buttonC->state(), VCButton::Inactive);
    QCOMPARE(log.starts(c->id()), 2);
    QCOMPARE(log.stops(c->id()), 2);
    QCOMPARE(log.starts(a->id()), 2);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::catchUp_mixedTieAtDestinationRunsOnceInOrder_data()
{
    QTest::addColumn<bool>("legacyFirst");
    QTest::addColumn<QString>("intensities");
    QTest::addColumn<int>("sliderValue");

    // .4 twice: the slider's write, then its reapply once the function runs
    QTest::newRow("legacy .2, then slider .8") << true << QStringLiteral("0.4,0.4,0.2,0.8") << 204;
    QTest::newRow("slider .8, then legacy .2") << false << QStringLiteral("0.4,0.4,0.8,0.2") << 51;
}

void ShowCommandRecorder_Test::catchUp_mixedTieAtDestinationRunsOnceInOrder()
{
    QFETCH(bool, legacyFirst);
    QFETCH(QString, intensities);
    QFETCH(int, sliderValue);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(slider);
    const auto position = [slider](quint32 id, quint32 time, qreal value) {
        return ShowCommand::setSliderPosition(id, time, slider->ensureRecordingId(), ShowControlRole::AdjustSlider,
                                              QStringLiteral("Intensity"), value);
    };

    ShowCommandTrack track;
    QVERIFY(track.insert(position(0, 500, 0.4)));
    if (legacyFirst)
        QVERIFY(track.insert(ShowCommand::setIntensity(1, 2000, scene->id(), 0.2)));
    QVERIFY(track.insert(position(2, 2000, 0.8)));
    if (!legacyFirst)
        QVERIFY(track.insert(ShowCommand::setIntensity(1, 2000, scene->id(), 0.2)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));

    QStringList applied;
    QObject probe;
    connect(scene, &Function::attributeChanged, &probe, [&applied](int index, qreal value) {
        if (index == Function::Intensity)
            applied.append(QString::number(value));
    });
    QSignalSpy retired(slider, &VCSlider::recordedWriteRetired);

    show->start(doc.masterTimer(), FunctionParent::master(), 2000);
    for (int i = 0; i < 50 && (applied.count() < 4 || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    tickAndDeliver(&doc, 10);

    QCOMPARE(applied.join(QLatin1Char(',')), intensities);
    QCOMPARE(retired.count(), 2);
    QVERIFY(scene->isRunning());
    QCOMPARE(slider->value(), sliderValue);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::catchUp_soloSliderHistoryEndsLikeNative()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *a = addTarget(&doc);
    QVERIFY(a);
    Scene *b = new Scene(&doc);
    b->setValue(SceneValue(a->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(b));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    // S shares B's Solo Frame
    const int soloID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Solo"), true);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), soloID, a->id());
    VCButton *buttonB = addToggle(bridge, ui.vc(), soloID, b->id(), 10, QStringLiteral("B"));
    QVERIFY(slider);
    QVERIFY(buttonB);
    const auto position = [slider](quint32 id, quint32 time, qreal value) {
        return ShowCommand::setSliderPosition(id, time, slider->ensureRecordingId(), ShowControlRole::AdjustSlider,
                                              QStringLiteral("Intensity"), value);
    };

    ShowCommandTrack track;
    QVERIFY(track.insert(position(0, 1000, 0.8)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 1500, a->id(), 0.2)));
    QVERIFY(track.insert(ShowCommand::setButtonState(2, 2000, buttonB->ensureRecordingId(), true)));
    QVERIFY(track.insert(position(3, 2400, 0.8)));
    QVERIFY(track.insert(ShowCommand::setIntensity(4, 2600, a->id(), 0.8)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(b);
    QSignalSpy retired(slider, &VCSlider::recordedWriteRetired);

    show->start(doc.masterTimer(), FunctionParent::master(), 2800);
    for (int i = 0; i < 100 && (retired.count() < 2 || log.stops(b->id()) == 0 ||
                                recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    tickAndDeliver(&doc, 10);

    QCOMPARE(buttonB->state(), VCButton::Inactive);
    QCOMPARE(log.starts(b->id()), 1);
    QCOMPARE(log.stops(b->id()), 1);
    QVERIFY(a->isRunning());
    QCOMPARE(a->getAttributeValue(Function::Intensity), 0.8);
    QCOMPARE(slider->value(), 204);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::catchUp_prefixLag_data()
{
    QTest::addColumn<bool>("legacy");
    QTest::addColumn<int>("records");

    QTest::newRow("legacy Start/Stop, 10") << true << 10;
    QTest::newRow("legacy Start/Stop, 200") << true << 200;
    QTest::newRow("button ON/OFF, 10") << false << 10;
    QTest::newRow("button ON/OFF, 200") << false << 200;
}

void ShowCommandRecorder_Test::catchUp_prefixLag()
{
    QFETCH(bool, legacy);
    QFETCH(int, records);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *a = addTarget(&doc);
    QVERIFY(a);
    Scene *last = new Scene(&doc);
    last->setValue(SceneValue(a->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(last));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *buttonA = addToggle(bridge, ui.vc(), frameID, a->id(), 10, QStringLiteral("A"));
    VCButton *lastButton = addToggle(bridge, ui.vc(), frameID, last->id(), 100, QStringLiteral("Last"));
    QVERIFY(buttonA && lastButton);

    // alternating on/off every 10 ms, then the ON that ends the prefix
    ShowCommandTrack track;
    for (int i = 0; i < records; i++)
    {
        const quint32 time = quint32(10 + i * 10);
        const bool on = i % 2 == 0;
        QVERIFY(track.insert(legacy ? (on ? ShowCommand::start(quint32(i), time, a->id())
                                          : ShowCommand::stop(quint32(i), time, a->id()))
                                    : ShowCommand::setButtonState(quint32(i), time, buttonA->ensureRecordingId(), on)));
    }
    const quint32 destination = quint32(records * 10 + 100);
    QVERIFY(track.insert(ShowCommand::setButtonState(quint32(records), destination - 50,
                                                     lastButton->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(600000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(a);

    show->start(doc.masterTimer(), FunctionParent::master(), destination);
    // drained: the runner consumed the final acknowledgement and left catch-up
    const auto drained = [show]() {
        return show->m_runner != nullptr && !show->m_runner->m_commandCatchUp &&
               show->m_runner->m_heldControlBatch == 0 && show->m_runner->m_commandRemainder.isEmpty();
    };
    int ticks = 0;
    int activationTicks = -1;
    quint32 activationClock = 0;
    while (ticks < 4000 && (activationTicks < 0 || !drained()))
    {
        tickAndDeliver(&doc);
        ticks++;
        if (activationTicks < 0 && lastButton->state() == VCButton::Active)
        {
            activationTicks = ticks;
            activationClock = show->commandPosition() - destination;
        }
    }
    const quint32 drainClock = show->commandPosition() - destination;
    qInfo().noquote() << QStringLiteral("[catch-up lag] %1 x%2: last button Active after %3 ticks (%4 ms of Show "
                                        "clock), prefix drained after %5 ticks (%6 ms)")
                             .arg(legacy ? QStringLiteral("legacy Start/Stop") : QStringLiteral("button ON/OFF"))
                             .arg(records).arg(activationTicks).arg(activationClock).arg(ticks).arg(drainClock);
    QVERIFY(drained());
    QCOMPARE(show->m_ackedControlSeq, show->m_lastControlSeq);
    tickAndDeliver(&doc, 3);

    QCOMPARE(lastButton->state(), VCButton::Active);
    QCOMPARE(log.starts(a->id()), records / 2);
    QCOMPARE(log.stops(a->id()), records / 2);
    QCOMPARE(buttonA->state(), VCButton::Inactive);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::catchUp_stoppedCaptureReplaysOnPlay()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    Scene *other = new Scene(&doc);
    other->setValue(SceneValue(scene->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(other));
    VCButton *later = addToggle(bridge, ui.vc(), frameID, other->id(), 100, QStringLiteral("Later"));
    QVERIFY(button && later);

    // a VC record far ahead makes Play a catch-up traversal
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 50000, later->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(scene);

    // captured at the stopped cursor: ON, then OFF, executed live
    recorder.setRecording(true);
    recorder.setResolvedShow(show->id());
    QCOMPARE(recorder.phaseValue(), int(ShowRecordPhase::Bound));
    button->requestUserStateChange(true);
    tickAndDeliver(&doc);
    button->requestUserStateChange(false);
    tickAndDeliver(&doc);
    for (int i = 0; i < 20 && show->commandTrack().count() < 3; i++)
        tickAndDeliver(&doc);
    QCOMPARE(show->commandTrack().count(), 3);
    QVERIFY(recorder.setRecording(false));
    QCOMPARE(log.starts(scene->id()), 1);
    QCOMPARE(log.stops(scene->id()), 1);

    // Play from that cursor replays both, once
    show->start(doc.masterTimer(), FunctionParent::master());
    for (int i = 0; i < 20 && log.stops(scene->id()) < 2; i++)
        tickAndDeliver(&doc);
    tickAndDeliver(&doc, 5);

    QCOMPARE(log.starts(scene->id()), 2);
    QCOMPARE(log.stops(scene->id()), 2);
    QVERIFY(!scene->isRunning());

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::catchUp_liveInputAfterColdPlayDoesNotEcho()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *later = addToggle(bridge, ui.vc(), frameID, scene->id(), 100, QStringLiteral("Later"));
    QVERIFY(later);

    // a VC record far ahead makes Play a catch-up traversal
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 50000, later->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    Scene *target = new Scene(&doc);
    target->setValue(SceneValue(scene->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(target));
    NativeLog log;
    log.watch(target);
    recorder.setRecording(true);
    recorder.setResolvedShow(show->id());
    QCOMPARE(recorder.phaseValue(), int(ShowRecordPhase::Bound));

    // cold Play is accepted, then a live Start is executed and recorded
    // before the timer has run the Show at all
    show->start(doc.masterTimer(), FunctionParent::master());
    const FunctionParent owner(FunctionParent::AutoVCWidget, 42);
    target->start(doc.masterTimer(), owner);
    ShowCommandInput input;
    input.origin = ShowCommandOrigin::Pointer;
    input.action = ShowCommandAction::Start;
    input.functionId = target->id();
    QVERIFY(recorder.submitUserInput(input));
    tickAndDeliver(&doc, 5);

    // echoed, the Show would own the target too and keep it running
    target->stop(owner);
    tickAndDeliver(&doc, 3);
    QVERIFY(!target->isRunning());
    QCOMPARE(log.starts(target->id()), 1);
    QCOMPARE(log.stops(target->id()), 1);

    QVERIFY(recorder.setRecording(false));
    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::catchUp_liveInputAfterRunningRestartDoesNotEcho_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("restarts");

    QTest::newRow("button after Stop/Play of a running Show") << QStringLiteral("button") << 1;
    QTest::newRow("owned input after Stop/Play of a running Show") << QStringLiteral("owner") << 1;
    QTest::newRow("button after two rapid Stop/Play") << QStringLiteral("button") << 2;
}

void ShowCommandRecorder_Test::catchUp_liveInputAfterRunningRestartDoesNotEcho()
{
    QFETCH(QString, input);
    QFETCH(int, restarts);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *later = addToggle(bridge, ui.vc(), frameID, scene->id(), 100, QStringLiteral("Later"));
    Scene *target = new Scene(&doc);
    target->setValue(SceneValue(scene->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(target));
    VCButton *button = addToggle(bridge, ui.vc(), frameID, target->id(), 10, QStringLiteral("Target"));
    QVERIFY(later && button);

    // a VC record far ahead makes Play a catch-up traversal
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 50000, later->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(target);
    recorder.setRecording(true);
    recorder.setResolvedShow(show->id());
    QCOMPARE(recorder.phaseValue(), int(ShowRecordPhase::Bound));

    // the Show runs, then Stop and Play are accepted before its next tick
    show->start(doc.masterTimer(), FunctionParent::master());
    tickAndDeliver(&doc, 5);
    for (int i = 0; i < restarts; i++)
    {
        show->stop(FunctionParent::master());
        show->start(doc.masterTimer(), FunctionParent::master());
    }

    // a live Start is executed and recorded before the retired runner is replaced
    const FunctionParent owner(FunctionParent::AutoVCWidget, 4242);
    if (input == QStringLiteral("button"))
    {
        button->requestUserStateChange(true);
    }
    else
    {
        target->start(doc.masterTimer(), owner);
        ShowCommandInput command;
        command.origin = ShowCommandOrigin::Pointer;
        command.action = ShowCommandAction::Start;
        command.functionId = target->id();
        QVERIFY(recorder.submitUserInput(command));
    }
    tickAndDeliver(&doc, 20);

    // echoed, the Show would own the target too and keep it running
    QVERIFY(!target->startedAsChild());
    if (input == QStringLiteral("button"))
        button->requestUserStateChange(false);
    else
        target->stop(owner);
    tickAndDeliver(&doc, 5);
    QVERIFY(!target->isRunning());
    QCOMPARE(log.starts(target->id()), 1);
    QCOMPARE(log.stops(target->id()), 1);

    QVERIFY(recorder.setRecording(false));
    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::catchUp_liveInputAfterRequestedSeekDoesNotEcho_data()
{
    QTest::addColumn<bool>("pausedResume");

    QTest::newRow("playing") << false;
    QTest::newRow("paused, moved, resumed") << true;
}

void ShowCommandRecorder_Test::catchUp_liveInputAfterRequestedSeekDoesNotEcho()
{
    QFETCH(bool, pausedResume);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *later = addToggle(bridge, ui.vc(), frameID, scene->id(), 190, QStringLiteral("Later"));
    const auto addScene = [&](int channel) {
        Scene *function = new Scene(&doc);
        function->setValue(SceneValue(scene->values().first().fxi, channel, 120));
        return doc.addFunction(function) ? function : nullptr;
    };
    Scene *prefix = addScene(1);
    Scene *before = addScene(2);
    Scene *target = addScene(3);
    QVERIFY(prefix && before && target);
    VCButton *beforeButton = addToggle(bridge, ui.vc(), frameID, before->id(), 10, QStringLiteral("Before"));
    VCButton *button = addToggle(bridge, ui.vc(), frameID, target->id(), 100, QStringLiteral("Target"));
    QVERIFY(later && beforeButton && button);

    // a VC record far ahead makes the seek a catch-up traversal
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 3000, prefix->id())));
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 50000, later->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(prefix);
    log.watch(target);
    recorder.setRecording(true);
    recorder.setResolvedShow(show->id());
    QCOMPARE(recorder.phaseValue(), int(ShowRecordPhase::Bound));

    // a live legacy Start: executed natively, authored as a function command
    const auto liveStart = [&recorder](VCButton *pressed, Function *function) {
        pressed->requestUserStateChange(true, ShowCommandOrigin::Programmatic);
        ShowCommandInput input;
        input.origin = ShowCommandOrigin::Pointer;
        input.action = ShowCommandAction::Start;
        input.functionId = function->id();
        return recorder.submitUserInput(input);
    };

    // Playing: a live Start of the traversal the seek ends
    show->start(doc.masterTimer(), FunctionParent::master());
    tickAndDeliver(&doc, 5);
    QVERIFY(liveStart(beforeButton, before));
    tickAndDeliver(&doc, 3);

    // a live Start accepted after the seek request, before the next tick
    if (pausedResume)
    {
        show->setPause(true);
        tickAndDeliver(&doc, 3);
    }
    show->requestSeek(5000);
    if (pausedResume)
        show->setPause(false);
    QVERIFY(liveStart(button, target));
    tickAndDeliver(&doc, 20);

    // the prefix runs once, the earlier id replays as history, the later one does not echo
    QCOMPARE(log.starts(prefix->id()), 1);
    QVERIFY(before->startedAsChild());
    QVERIFY(!target->startedAsChild());
    button->requestUserStateChange(false, ShowCommandOrigin::Programmatic);
    tickAndDeliver(&doc, 5);
    QVERIFY(!target->isRunning());
    QCOMPARE(log.starts(target->id()), 1);
    QCOMPARE(log.stops(target->id()), 1);

    QVERIFY(recorder.setRecording(false));
    show->stop(FunctionParent::master());
    before->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::retiredRunner_endsAtItsClaimedOperation_data()
{
    QTest::addColumn<bool>("catchUp");
    QTest::addColumn<bool>("controlNext");

    QTest::newRow("catch-up, legacy Start next") << true << false;
    QTest::newRow("catch-up, VC batch next") << true << true;
    QTest::newRow("ordinary playback, legacy Start next") << false << false;
}

void ShowCommandRecorder_Test::retiredRunner_endsAtItsClaimedOperation()
{
    QFETCH(bool, catchUp);
    QFETCH(bool, controlNext);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *live = addTarget(&doc);
    QVERIFY(live);
    const auto addScene = [&](int channel) {
        Scene *function = new Scene(&doc);
        function->setValue(SceneValue(live->values().first().fxi, channel, 120));
        return doc.addFunction(function) ? function : nullptr;
    };
    Scene *old = addScene(1);
    Scene *claimed = addScene(2);
    QVERIFY(old && claimed);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int solo = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Solo"), true);
    const int frameID = bridge.addFrame(0, QRect(0, 310, 400, 300), QStringLiteral("Frame"), false);
    VCButton *liveButton = addToggle(bridge, ui.vc(), solo, live->id(), 10, QStringLiteral("Live"));
    VCButton *oldButton = addToggle(bridge, ui.vc(), solo, old->id(), 100, QStringLiteral("Old"));
    VCButton *later = addToggle(bridge, ui.vc(), frameID, claimed->id(), 10, QStringLiteral("Later"));
    QVERIFY(liveButton && oldButton && later);

    // the old traversal claims the intensity; whatever follows it is its remainder
    const quint32 claimAt = catchUp ? 2000 : 1000;
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, catchUp ? 1000 : 10, claimed->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, claimAt, claimed->id(), 0.4)));
    QVERIFY(track.insert(controlNext ? ShowCommand::setButtonState(2, claimAt, oldButton->ensureRecordingId(), true)
                                     : ShowCommand::start(2, claimAt, old->id())));
    // a VC record far ahead makes Play at 10000 a catch-up traversal
    if (catchUp)
        QVERIFY(track.insert(ShowCommand::setButtonState(3, 150000, later->ensureRecordingId(), false)));
    QVERIFY(track.setExtent(200000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(old);
    recorder.setRecording(true);
    recorder.setResolvedShow(show->id());
    QCOMPARE(recorder.phaseValue(), int(ShowRecordPhase::Bound));

    // while the claimed intensity executes: Stop, Play from 0 and a live Start
    const FunctionParent owner(FunctionParent::AutoVCWidget, 4242);
    bool boundary = false;
    bool accepted = false;
    int laterBatches = 0;
    QObject probe;
    connect(show, &Show::controlBatchesReady, &probe, [&]() { if (boundary) laterBatches++; }, Qt::DirectConnection);
    connect(claimed, &Function::attributeChanged, &probe, [&](int index, qreal value) {
        if (boundary || index != Function::Intensity || !qFuzzyCompare(value, 0.4))
            return;
        boundary = true;
        show->stop(FunctionParent::master());
        show->start(doc.masterTimer(), FunctionParent::master(), 0);
        live->start(doc.masterTimer(), owner);
        ShowCommandInput input;
        input.origin = ShowCommandOrigin::Pointer;
        input.action = ShowCommandAction::Start;
        input.functionId = live->id();
        accepted = recorder.submitUserInput(input);
    }, Qt::DirectConnection);
    show->start(doc.masterTimer(), FunctionParent::master(), catchUp ? 10000 : 0);
    for (int i = 0; i < 80 && !boundary; i++)
        tickAndDeliver(&doc);
    QVERIFY(boundary && accepted);
    tickAndDeliver(&doc, 5);

    // the new traversal is not there yet: the retired one ran nothing past its claim
    QVERIFY(show->commandPosition() < 200);
    QCOMPARE(laterBatches, 0);
    QCOMPARE(log.starts(old->id()), 0);
    QCOMPARE(oldButton->state(), VCButton::Inactive);
    QVERIFY(live->isRunning());
    QVERIFY(!live->startedAsChild());
    live->stop(owner);
    tickAndDeliver(&doc, 4);
    QVERIFY(!live->isRunning());

    QVERIFY(recorder.setRecording(false));
    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::retiredRunner_guiRestartDuringClaimedOperation_realTimer()
{
    // Seen on the timer thread only. Declared first, so they outlive the
    // callback, which is disconnected before the timer stops.
    std::atomic<bool> claimed{ false };
    std::atomic<bool> released{ false };

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *live = addTarget(&doc);
    QVERIFY(live);
    const auto addScene = [&](int channel) {
        Scene *function = new Scene(&doc);
        function->setValue(SceneValue(live->values().first().fxi, channel, 120));
        return doc.addFunction(function) ? function : nullptr;
    };
    Scene *old = addScene(1);
    Scene *adjusted = addScene(2);
    QVERIFY(old && adjusted);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int solo = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Solo"), true);
    const int frameID = bridge.addFrame(0, QRect(0, 310, 400, 300), QStringLiteral("Frame"), false);
    VCButton *liveButton = addToggle(bridge, ui.vc(), solo, live->id(), 10, QStringLiteral("Live"));
    VCButton *oldButton = addToggle(bridge, ui.vc(), solo, old->id(), 100, QStringLiteral("Old"));
    VCButton *later = addToggle(bridge, ui.vc(), frameID, adjusted->id(), 10, QStringLiteral("Later"));
    QVERIFY(liveButton && oldButton && later);

    // a VC record far ahead makes Play at 10000 a catch-up traversal
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 1000, adjusted->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 2000, adjusted->id(), 0.4)));
    QVERIFY(track.insert(ShowCommand::start(2, 3000, old->id())));
    QVERIFY(track.insert(ShowCommand::setButtonState(3, 150000, later->ensureRecordingId(), false)));
    QVERIFY(track.setExtent(200000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(old);
    log.watch(live);
    recorder.setRecording(true);
    recorder.setResolvedShow(show->id());
    QCOMPARE(recorder.phaseValue(), int(ShowRecordPhase::Bound));

    // the timer thread holds in the claimed intensity until the GUI has acted
    QObject probe;
    connect(adjusted, &Function::attributeChanged, &probe, [&](int index, qreal value) {
        if (index != Function::Intensity || !qFuzzyCompare(value, 0.4) || claimed.exchange(true))
            return;
        for (int i = 0; i < 5000 && !released; i++)
            QThread::msleep(1);
    }, Qt::DirectConnection);

    timer->start();
    const auto stopAll = qScopeGuard([&]() {
        released = true;
        QObject::disconnect(adjusted, nullptr, &probe, nullptr);
        show->stop(FunctionParent::master());
        show->stopAndWait();
        timer->stop();
    });
    show->start(timer, FunctionParent::master(), 10000);
    QTRY_VERIFY(claimed);

    // the GUI stops, plays from 0 and executes a live Start meanwhile
    show->stop(FunctionParent::master());
    show->start(timer, FunctionParent::master(), 0);
    const FunctionParent owner(FunctionParent::AutoVCWidget, 4242);
    live->start(timer, owner);
    ShowCommandInput input;
    input.origin = ShowCommandOrigin::Pointer;
    input.action = ShowCommandAction::Start;
    input.functionId = live->id();
    QVERIFY(recorder.submitUserInput(input));
    released = true;

    // well before the new traversal reaches 1000: the retired one ran nothing past its claim
    QTRY_VERIFY(log.starts(live->id()) > 0);
    QTest::qWait(300);
    QVERIFY(show->commandPosition() < 1000);
    QCOMPARE(log.starts(old->id()), 0);
    QCOMPARE(oldButton->state(), VCButton::Inactive);
    QVERIFY(log.live(live->id()));
    QVERIFY(!live->startedAsChild());
    live->stop(owner);
    QTRY_VERIFY(!log.live(live->id()));
    QVERIFY(recorder.setRecording(false));
}

void ShowCommandRecorder_Test::retiredRunner_legacySeekEndsAtItsClaimedValue()
{
    Doc doc(nullptr, 1);
    Scene *u = addTarget(&doc);
    QVERIFY(u);
    Scene *b = new Scene(&doc);
    b->setValue(SceneValue(u->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(b));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));

    // legacy-only, no executor: a seek restores both historical values inline
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setIntensity(0, 1000, u->id(), 0.4)));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 2000, b->id(), 0.7)));
    QVERIFY(track.setExtent(200000));
    QVERIFY(show->setCommandTrack(track));

    const FunctionParent owner(FunctionParent::AutoVCWidget, 4242);
    u->start(doc.masterTimer(), owner);
    b->start(doc.masterTimer(), owner);
    tickAndDeliver(&doc);
    QVERIFY(u->isRunning() && b->isRunning());

    // while U's restored value executes: Stop, Play from 0 and a live B value
    bool boundary = false;
    QObject probe;
    connect(u, &Function::attributeChanged, &probe, [&](int index, qreal value) {
        if (boundary || index != Function::Intensity || !qFuzzyCompare(value, 0.4))
            return;
        boundary = true;
        show->stop(FunctionParent::master());
        show->start(doc.masterTimer(), FunctionParent::master(), 0);
        b->requestAttributeOverride(Function::Intensity, 0.9);
    }, Qt::DirectConnection);
    show->start(doc.masterTimer(), FunctionParent::master(), 10000);
    for (int i = 0; i < 10 && !boundary; i++)
        tickAndDeliver(&doc);
    QVERIFY(boundary);
    tickAndDeliver(&doc, 5);

    // the new traversal is not at 2000 yet: the retired one restored nothing past U
    QVERIFY(show->commandPosition() < 2000);
    QCOMPARE(u->getAttributeValue(Function::Intensity), qreal(0.4));
    QCOMPARE(b->getAttributeValue(Function::Intensity), qreal(0.9));

    show->stop(FunctionParent::master());
    u->stop(owner);
    b->stop(owner);
    tickAndDeliver(&doc, 4);
}

void ShowCommandRecorder_Test::catchUp_armedAcrossPlayReplaysOnlyHistory()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Scene *target = new Scene(&doc);
    target->setValue(SceneValue(scene->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(target));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("A"));
    VCButton *later = addToggle(bridge, ui.vc(), frameID, target->id(), 100, QStringLiteral("Later"));
    QVERIFY(button && later);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 50000, later->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(scene);
    log.watch(target);

    // stopped and armed: A ON, then OFF, executed live
    recorder.setRecording(true);
    recorder.setResolvedShow(show->id());
    QCOMPARE(recorder.phaseValue(), int(ShowRecordPhase::Bound));
    button->requestUserStateChange(true);
    tickAndDeliver(&doc);
    button->requestUserStateChange(false);
    tickAndDeliver(&doc);
    for (int i = 0; i < 20 && show->commandTrack().count() < 3; i++)
        tickAndDeliver(&doc);
    QCOMPARE(show->commandTrack().count(), 3);

    // still armed: Play is accepted, then G is executed and recorded before the first tick
    show->start(doc.masterTimer(), FunctionParent::master());
    const FunctionParent owner(FunctionParent::AutoVCWidget, 42);
    target->start(doc.masterTimer(), owner);
    ShowCommandInput input;
    input.origin = ShowCommandOrigin::Pointer;
    input.action = ShowCommandAction::Start;
    input.functionId = target->id();
    QVERIFY(recorder.submitUserInput(input));
    for (int i = 0; i < 20 && log.stops(scene->id()) < 2; i++)
        tickAndDeliver(&doc);
    tickAndDeliver(&doc, 3);
    target->stop(owner);
    tickAndDeliver(&doc, 3);

    // A's history replays once; G does not echo
    QCOMPARE(log.starts(scene->id()), 2);
    QCOMPARE(log.stops(scene->id()), 2);
    QVERIFY(!target->isRunning());
    QCOMPARE(log.starts(target->id()), 1);

    QVERIFY(recorder.setRecording(false));
    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::pause_keepsVcStartedFunctions_resumeRunsNoPrefix()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Scene *clip = new Scene(&doc);
    clip->setValue(SceneValue(scene->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(clip));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    auto *clips = new Track(Function::invalidId());
    auto *item = new ShowFunction(show->getLatestShowFunctionId());
    item->setFunctionID(clip->id());
    item->setStartTime(0);
    item->setDuration(60000);
    clips->addShowFunction(item);
    QVERIFY(show->addTrack(clips));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    QVERIFY(button);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, button->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(scene);
    QSignalSpy batches(show, &Show::controlBatchesReady);

    show->start(doc.masterTimer(), FunctionParent::master());
    for (int i = 0; i < 50 && (!log.live(scene->id()) || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    QCOMPARE(button->state(), VCButton::Active);
    QVERIFY(clip->isRunning());
    const int crossed = batches.count();
    QCOMPARE(crossed, 1);

    show->setPause(true);
    tickAndDeliver(&doc, 10);
    QVERIFY(clip->isPaused());
    QVERIFY(scene->isRunning());
    QVERIFY(!scene->isPaused());
    QCOMPARE(button->state(), VCButton::Active);

    // Resume without repositioning continues the traversal
    show->setPause(false);
    tickAndDeliver(&doc, 10);
    QVERIFY(!clip->isPaused());
    QCOMPARE(batches.count(), crossed);
    QCOMPARE(log.starts(scene->id()), 1);
    QCOMPARE(log.stops(scene->id()), 0);
    QCOMPARE(button->state(), VCButton::Active);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::resume_afterPausedMove_catchesUpWithoutStopping_data()
{
    QTest::addColumn<QString>("shape");

    QTest::newRow("time show") << QStringLiteral("time");
    QTest::newRow("beats show") << QStringLiteral("beats");
    QTest::newRow("legacy-only track") << QStringLiteral("legacy");
}

void ShowCommandRecorder_Test::resume_afterPausedMove_catchesUpWithoutStopping()
{
    QFETCH(QString, shape);
    const bool legacy = shape == QLatin1String("legacy");

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *a = addTarget(&doc);
    QVERIFY(a);
    const auto addScene = [&doc, a](quint32 channel) {
        Scene *scene = new Scene(&doc);
        scene->setValue(SceneValue(a->values().first().fxi, channel, 120));
        doc.addFunction(scene);
        return scene;
    };
    Scene *b = addScene(1);
    Scene *owned = addScene(2);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    if (shape == QLatin1String("beats"))
    {
        show->setTimeDivision(Show::BPM_4_4, 120);
        show->setTempoType(Function::Beats);
    }
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *buttonA = addToggle(bridge, ui.vc(), frameID, a->id(), 10, QStringLiteral("A"));
    VCButton *buttonB = addToggle(bridge, ui.vc(), frameID, b->id(), 100, QStringLiteral("B"));
    QVERIFY(buttonA && buttonB);

    ShowCommandTrack track;
    if (legacy)
    {
        // a is someone else's, the Show only sets its value; owned is the Show's
        QVERIFY(track.insert(ShowCommand::start(0, 500, owned->id())));
        QVERIFY(track.insert(ShowCommand::setIntensity(1, 1000, a->id(), 0.3)));
        QVERIFY(track.insert(ShowCommand::setIntensity(2, 2000, a->id(), 0.6)));
        a->start(doc.masterTimer(), FunctionParent::master());
    }
    else
    {
        QVERIFY(track.insert(ShowCommand::setButtonState(0, 1000, buttonA->ensureRecordingId(), true)));
        QVERIFY(track.insert(ShowCommand::setButtonState(1, 2000, buttonA->ensureRecordingId(), false)));
        QVERIFY(track.insert(ShowCommand::setButtonState(2, 6000, buttonB->ensureRecordingId(), true)));
    }
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(a);
    log.watch(b);
    log.watch(owned);
    log.watchReceipts(show);
    const auto settle = [&]() {
        for (int i = 0; i < 20; i++)
        {
            const int receipts = log.receipts();
            tickAndDeliver(&doc);
            if (receipts == log.receipts() && recorder.pendingControlRuns() == 0)
                break;
        }
    };

    ShowManager manager(ui.view(), &doc);
    // no App window: the editor geometry reads the context's own density
    manager.m_detached = true;
    manager.setCurrentShowID(int(show->id()));
    manager.playShow();
    for (int i = 0; i < 200 && show->commandPosition() < 1500; i++)
        tickAndDeliver(&doc);
    settle();
    if (!legacy)
        QCOMPARE(buttonA->state(), VCButton::Active);

    // Pause, move the cursor: nothing is applied until Play
    manager.playShow();
    QVERIFY(manager.isPaused());
    tickAndDeliver(&doc, 3);
    manager.setCurrentTime(2500);
    tickAndDeliver(&doc, 3);
    if (!legacy)
        QCOMPARE(buttonA->state(), VCButton::Active);
    const ShowRunner *runner = show->m_runner;
    QSignalSpy showStopped(show, qOverload<quint32>(&Function::stopped));
    QSignalSpy finished(show, &Show::showFinished);

    manager.playShow();
    QVERIFY(!manager.isPaused());
    tickAndDeliver(&doc);
    settle();

    QVERIFY(show->isRunning());
    QCOMPARE(showStopped.count(), 0);
    QCOMPARE(finished.count(), 0);
    QCOMPARE(show->m_runner, runner);
    QVERIFY(show->commandPosition() >= 2500);
    QVERIFY(show->commandPosition() < 6000);
    if (legacy)
    {
        // the legacy seek: values restored, historical Start/Stop not replayed
        QCOMPARE(a->getAttributeValue(Function::Intensity), qreal(0.6));
        QCOMPARE(log.starts(owned->id()), 1);
        QCOMPARE(log.stops(owned->id()), 1);
    }
    else
    {
        // the prefix once: A ON (already on), then A OFF; B ON is later
        QCOMPARE(buttonA->state(), VCButton::Inactive);
        QCOMPARE(log.starts(a->id()), 1);
        QCOMPARE(log.stops(a->id()), 1);
        QCOMPARE(log.starts(b->id()), 0);
    }
    if (shape == QLatin1String("beats"))
    {
        QVERIFY(show->m_runner->beatSynced);
        QVERIFY(show->m_runner->m_elapsedBeats >= 5000);
    }

    show->stop(FunctionParent::master());
    a->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::userRequest_waitsBehindCrossedReplayOnItsControl()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    const quint32 fixtureID = scene->values().first().fxi;
    const auto addScene = [&doc, fixtureID](quint32 channel) {
        Scene *function = new Scene(&doc);
        function->setValue(SceneValue(fixtureID, channel, 120));
        doc.addFunction(function);
        return function;
    };
    Scene *legacy = addScene(1);
    Scene *unrelated = addScene(2);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    recorder.setResolvedShow(show->id());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *replayed = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Replayed"));
    VCButton *other = addToggle(bridge, ui.vc(), frameID, unrelated->id(), 100, QStringLiteral("Other"));
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    QVERIFY(replayed && other && slider);
    slider->setSliderMode(VCSlider::Level);
    slider->addLevelChannel(fixtureID, 3);

    // the 30% on S waits behind the button batch and a legacy Start
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, replayed->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::start(1, 105, legacy->id())));
    QVERIFY(track.insert(ShowCommand::setSliderPosition(2, 110, slider->ensureRecordingId(),
                                                        ShowControlRole::LevelSlider, QString(), 0.3)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    QSignalSpy batches(show, &Show::controlBatchesReady);

    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 20 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    timer->timerTick();

    show->setPause(true);
    const quint32 pausedAt = show->commandPosition();
    // the operator moves S to 55% while its crossed 30% is still owed
    slider->requestUserValue(140);
    const int valueAtAcceptance = slider->value();
    const QVector<ShowControlRequest> queued = recorder.pendingUserRequests();
    // an unrelated control responds at once
    other->requestUserStateChange(true);
    const VCButton::ButtonState otherAtOnce = other->state();

    for (int i = 0; i < 20 && (!recorder.pendingUserRequests().isEmpty() ||
                               recorder.pendingControlRuns() > 0 || show->commandWorkPending()); i++)
        tickAndDeliver(&doc);
    const int drainedValue = slider->value();
    show->setPause(false);
    tickAndDeliver(&doc, 10);

    // older work first, the accepted value last, never the stale 30% after it
    QCOMPARE(slider->value(), 140);
    QCOMPARE(drainedValue, 140);
    QCOMPARE(valueAtAcceptance, 0);
    QCOMPARE(queued.count(), 1);
    QCOMPARE(queued.first().value, 140);
    QCOMPARE(queued.first().acceptedTimeMs, pausedAt);
    QCOMPARE(otherAtOnce, VCButton::Active);
    QCOMPARE(replayed->state(), VCButton::Active);
    QVERIFY(legacy->isRunning());
    QCOMPARE(show->commandPosition() > pausedAt, true);

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::userRequest_independentSliderRespondsAtOnce_data()
{
    QTest::addColumn<int>("mode");

    QTest::newRow("second level slider") << int(VCSlider::Level);
    QTest::newRow("grand master slider") << int(VCSlider::GrandMaster);
}

void ShowCommandRecorder_Test::userRequest_independentSliderRespondsAtOnce()
{
    QFETCH(int, mode);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    const quint32 fixtureID = scene->values().first().fxi;
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *replayed = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Replayed"));
    VCSlider *first = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    VCSlider *second = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    QVERIFY(replayed && first && second);
    first->setSliderMode(VCSlider::Level);
    first->addLevelChannel(fixtureID, 1);
    second->setSliderMode(VCSlider::SliderMode(mode));
    if (second->sliderMode() == VCSlider::Level)
        second->addLevelChannel(fixtureID, 2);

    // the first slider's 30% waits behind a button batch
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, replayed->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setSliderPosition(1, 100, first->ensureRecordingId(),
                                                        ShowControlRole::LevelSlider, QString(), 0.3)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 20 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);

    second->requestUserValue(140);

    QCOMPARE(second->value(), 140);
    QVERIFY(recorder.pendingUserRequests().isEmpty());

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::userClick_normalizedOnceToNativeDesiredState_data()
{
    QTest::addColumn<QString>("initial");
    QTest::addColumn<bool>("recording");
    QTest::addColumn<QString>("replayed");
    QTest::addColumn<bool>("expectOn");

    QTest::newRow("inactive") << QStringLiteral("inactive") << false << QString() << true;
    QTest::newRow("active") << QStringLiteral("active") << false << QString() << false;
    QTest::newRow("monitoring") << QStringLiteral("monitoring") << false << QString() << true;
    QTest::newRow("monitoring, recording") << QStringLiteral("monitoring") << true << QString() << true;
    // accepted, then waiting behind a replay reaching the same state: never toggled again
    QTest::newRow("inactive, behind replayed ON") << QStringLiteral("inactive") << false
                                                  << QStringLiteral("on") << true;
    QTest::newRow("active, behind replayed OFF") << QStringLiteral("active") << false
                                                 << QStringLiteral("off") << false;
}

void ShowCommandRecorder_Test::userClick_normalizedOnceToNativeDesiredState()
{
    QFETCH(QString, initial);
    QFETCH(bool, recording);
    QFETCH(QString, replayed);
    QFETCH(bool, expectOn);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    recorder.setResolvedShow(show->id());
    VCBridgeV5 bridge(&doc, ui.vc());
    const LookRig rig = addLookRig(&doc, bridge, ui.vc());
    QVERIFY(rig.childButton);
    VCButton *button = rig.childButton;

    if (initial == QLatin1String("active"))
        button->requestStateChange(true);
    else if (initial == QLatin1String("monitoring"))
        rig.collection->start(timer, FunctionParent::master());
    tickAndDeliver(&doc, 3);
    QCOMPARE(button->state(), initial == QLatin1String("active") ? VCButton::Active
                              : initial == QLatin1String("monitoring") ? VCButton::Monitoring
                                                                       : VCButton::Inactive);
    if (recording)
        QVERIFY(recorder.setRecording(true));

    if (replayed.isEmpty() == false)
    {
        ShowCommandTrack track;
        QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, button->ensureRecordingId(),
                                                         replayed == QLatin1String("on"))));
        QVERIFY(track.setExtent(60000));
        QVERIFY(show->setCommandTrack(track));
        QSignalSpy batches(show, &Show::controlBatchesReady);
        show->start(timer, FunctionParent::master());
        for (int i = 0; i < 20 && batches.isEmpty(); i++)
            timer->timerTick();
        QCOMPARE(batches.count(), 1);
    }

    button->requestUserStateChange(true);
    const QVector<ShowControlRequest> queued = recorder.pendingUserRequests();
    for (int i = 0; i < 20 && (!recorder.pendingUserRequests().isEmpty() || recorder.pendingControlRuns() > 0); i++)
        tickAndDeliver(&doc);
    tickAndDeliver(&doc, 2);

    QCOMPARE(button->state(), expectOn ? VCButton::Active : VCButton::Inactive);
    QCOMPARE(rig.child->isRunning(), expectOn);
    QCOMPARE(queued.count(), replayed.isEmpty() ? 0 : 1);
    if (!queued.isEmpty())
        QCOMPARE(queued.first().on, expectOn);
    if (recording)
    {
        QCOMPARE(recorder.track().commands().count(), qsizetype(1));
        QCOMPARE(int(recorder.track().commands().first().action), int(ShowCommandAction::SetButtonState));
        QCOMPARE(recorder.track().commands().first().on, expectOn);
    }
    if (initial == QLatin1String("monitoring"))
    {
        // the button owns the Scene now, the Collection stopping leaves it on
        rig.collection->stop(FunctionParent::master());
        tickAndDeliver(&doc, 3);
        QVERIFY(!rig.collection->isRunning());
        QVERIFY(rig.child->isRunning());
        QCOMPARE(button->state(), VCButton::Active);
    }

    if (recording)
        QVERIFY(recorder.setRecording(false));
    show->stop(FunctionParent::master());
    rig.child->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::userRequest_survivesReplayCancelNotWorkspaceReset_data()
{
    QTest::addColumn<QString>("cancel");
    QTest::addColumn<bool>("executed");

    QTest::newRow("seek") << QStringLiteral("seek") << true;
    QTest::newRow("stop") << QStringLiteral("stop") << true;
    QTest::newRow("workspace reset") << QStringLiteral("reset") << false;
    QTest::newRow("show deleted") << QStringLiteral("delete") << false;
}

void ShowCommandRecorder_Test::userRequest_survivesReplayCancelNotWorkspaceReset()
{
    QFETCH(QString, cancel);
    QFETCH(bool, executed);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    QVERIFY(slider);
    slider->setSliderMode(VCSlider::Level);
    slider->addLevelChannel(scene->values().first().fxi, 3);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 100, slider->ensureRecordingId(),
                                                        ShowControlRole::LevelSlider, QString(), 0.3)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 20 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);
    // the 30% is applied, its run waits for the write
    QCoreApplication::processEvents();
    QCOMPARE(recorder.pendingControlRuns(), 1);
    QCOMPARE(slider->value(), 77);

    slider->requestUserValue(140);
    QCOMPARE(recorder.pendingUserRequests().count(), 1);

    if (cancel == QLatin1String("seek"))
        show->requestSeek(50);
    else if (cancel == QLatin1String("stop"))
        show->stop(FunctionParent::master());
    else if (cancel == QLatin1String("reset"))
        doc.clearContents();
    else
        QVERIFY(doc.deleteFunction(show->id()));
    QCoreApplication::processEvents();

    QCOMPARE(slider->value(), executed ? 140 : 77);
    QVERIFY(recorder.pendingUserRequests().isEmpty());

    if (executed)
    {
        show->stop(FunctionParent::master());
        tickAndDeliver(&doc);
    }
}

void ShowCommandRecorder_Test::recArmedUserInput_waitsBehindCoupledReplay_data()
{
    QTest::addColumn<bool>("button");
    QTest::addColumn<bool>("waits");

    QTest::newRow("adjust slider") << false << true;
    QTest::newRow("toggle button") << true << true;
}

void ShowCommandRecorder_Test::recArmedUserInput_waitsBehindCoupledReplay()
{
    QFETCH(bool, button);
    QFETCH(bool, waits);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *toggle = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, scene->id());
    QVERIFY(toggle && slider);

    // a crossed replay on the same control, not taken by the executor yet
    ShowCommandTrack track;
    if (button)
        QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, toggle->ensureRecordingId(), true)));
    else
        QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 100, slider->ensureRecordingId(),
                                                            ShowControlRole::AdjustSlider,
                                                            QStringLiteral("Intensity"), 0.3)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    recorder.setResolvedShow(show->id());
    QVERIFY(recorder.setRecording(true));
    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 20 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);

    // recorded at acceptance, then ordered behind the crossed replay like any request
    if (button)
        toggle->requestUserStateChange(true);
    else
        slider->requestUserValue(140);
    QCOMPARE(recorder.pendingUserRequests().count(), waits ? 1 : 0);
    QCOMPARE(show->commandTrack().count(), 2);

    tickAndDeliver(&doc, 8);
    QVERIFY(recorder.pendingUserRequests().isEmpty());
    if (button)
        QCOMPARE(toggle->state(), VCButton::Active);
    else
        QCOMPARE(slider->value(), 140);
    QCOMPARE(show->commandTrack().count(), 2);

    QVERIFY(recorder.setRecording(false));
    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::resume_externalShowAfterPausedMove_keepsItsTraversal()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Scene *legacy = new Scene(&doc);
    legacy->setValue(SceneValue(scene->values().first().fxi, 1, 120));
    QVERIFY(doc.addFunction(legacy));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    show->setSyncSource(ShowRunner::External);
    show->setExternalElapsedTime(200);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    QVERIFY(button);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, button->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::start(1, 150, legacy->id())));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    QSignalSpy batches(show, &Show::controlBatchesReady);

    ShowManager manager(ui.view(), &doc);
    manager.m_detached = true;
    manager.setCurrentShowID(int(show->id()));
    manager.playShow();
    for (int i = 0; i < 20 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);

    // the legacy Start waits behind the batch; the external clock owns the position
    manager.playShow();
    QVERIFY(manager.isPaused());
    manager.setCurrentTime(5000);
    manager.playShow();
    QVERIFY(!manager.isPaused());
    // the catch-up Start settles one tick after it ran
    for (int i = 0; i < 20 && (!legacy->isRunning() || recorder.pendingControlRuns() > 0 ||
                               show->commandWorkPending()); i++)
        tickAndDeliver(&doc);

    QCOMPARE(button->state(), VCButton::Active);
    QVERIFY(legacy->isRunning());
    QVERIFY(!show->commandWorkPending());

    show->stop(FunctionParent::master());
    legacy->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::pausing_showsOwedCrossedWorkUntilDrained_data()
{
    QTest::addColumn<bool>("owed");
    QTest::addColumn<QString>("then");

    QTest::newRow("owed, drained") << true << QString();
    // the pending work is read when pausing: nothing owed never shows pausing
    QTest::newRow("nothing owed") << false << QString();
    QTest::newRow("owed, resumed") << true << QStringLiteral("resume");
    QTest::newRow("owed, stopped") << true << QStringLiteral("stop");
    QTest::newRow("owed, other show selected") << true << QStringLiteral("select");
}

void ShowCommandRecorder_Test::pausing_showsOwedCrossedWorkUntilDrained()
{
    QFETCH(bool, owed);
    QFETCH(QString, then);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    Show *other = new Show(&doc);
    QVERIFY(doc.addFunction(other));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    QVERIFY(button);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, owed ? 100 : 50000, button->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    QSignalSpy batches(show, &Show::controlBatchesReady);

    ShowManager manager(ui.view(), &doc);
    manager.m_detached = true;
    manager.setCurrentShowID(int(show->id()));
    QSignalSpy pausingSpy(&manager, &ShowManager::pausingChanged);
    manager.playShow();
    for (int i = 0; i < 11; i++)
        timer->timerTick();
    QCOMPARE(batches.count(), owed ? 1 : 0);

    // Pause while the crossed batch is still owed to the GUI
    manager.playShow();
    const quint32 pausedAt = show->commandPosition();
    const bool pausingAtPause = manager.pausing();

    if (then == QLatin1String("resume"))
        manager.playShow();
    else if (then == QLatin1String("stop"))
        manager.stopShow();
    else if (then == QLatin1String("select"))
        manager.setCurrentShowID(int(other->id()));
    const bool pausingAfterThen = manager.pausing();
    const int changesBeforeDrain = pausingSpy.count();

    // the owed work drains natively, paused or not
    for (int i = 0; i < 20 && (recorder.pendingControlRuns() > 0 || show->commandWorkPending()); i++)
        tickAndDeliver(&doc);
    tickAndDeliver(&doc, 2);

    QCOMPARE(pausingAtPause, owed);
    QVERIFY(!manager.pausing());
    // a Stop cancels the owed batch instead
    QCOMPARE(button->state(), owed && then != QLatin1String("stop") ? VCButton::Active : VCButton::Inactive);
    if (then.isEmpty())
    {
        QCOMPARE(show->commandPosition(), pausedAt);
        QCOMPARE(pausingSpy.count(), owed ? 2 : 0);
    }
    else
    {
        // cleared by the transport, a later drain of the old work changes nothing
        QVERIFY(!pausingAfterThen);
        QCOMPARE(pausingSpy.count(), changesBeforeDrain);
        QCOMPARE(pausingSpy.count(), 2);
    }

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

void ShowCommandRecorder_Test::ignoredExternalSeek_keepsCrossedWork_data()
{
    QTest::addColumn<QString>("entry");

    QTest::newRow("show manager seek") << QStringLiteral("manager");
    QTest::newRow("direct Show seek") << QStringLiteral("direct");
    // an Autonomous seek made after the switch still cancels and catches up
    QTest::newRow("switched to autonomous") << QStringLiteral("autonomous");
    // cancelled under Autonomous, discarded by the switch back: the owed work is dropped
    QTest::newRow("switched back to external after the request") << QStringLiteral("switched");
}

void ShowCommandRecorder_Test::ignoredExternalSeek_keepsCrossedWork()
{
    QFETCH(QString, entry);
    const bool autonomous = entry == QLatin1String("autonomous");
    const bool switched = entry == QLatin1String("switched");

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    QVERIFY(scene);
    const auto addScene = [&doc, scene](quint32 channel) {
        Scene *function = new Scene(&doc);
        function->setValue(SceneValue(scene->values().first().fxi, channel, 120));
        doc.addFunction(function);
        return function;
    };
    Scene *legacy = addScene(1);
    Scene *later = addScene(2);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    show->setSyncSource(ShowRunner::External);
    show->setExternalElapsedTime(200);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Target"));
    VCButton *laterButton = addToggle(bridge, ui.vc(), frameID, later->id(), 100, QStringLiteral("Later"));
    QVERIFY(button && laterButton);

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, button->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::start(1, 150, legacy->id())));
    QVERIFY(track.insert(ShowCommand::setButtonState(2, 400, laterButton->ensureRecordingId(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    NativeLog log;
    log.watch(scene);
    QSignalSpy batches(show, &Show::controlBatchesReady);

    ShowManager manager(ui.view(), &doc);
    manager.m_detached = true;
    manager.setCurrentShowID(int(show->id()));
    manager.playShow();
    for (int i = 0; i < 20 && batches.isEmpty(); i++)
        timer->timerTick();
    QCOMPARE(batches.count(), 1);

    // the local seek, while the button batch and the legacy Start are still owed
    if (switched)
    {
        // an accepted manual request waits behind the owed batch
        button->requestUserStateChange(true);
        QCOMPARE(recorder.pendingUserRequests().count(), 1);
        show->setSyncSource(ShowRunner::Autonomous);
        show->requestSeek(5000);
        show->setSyncSource(ShowRunner::External);
    }
    else if (autonomous)
    {
        show->setSyncSource(ShowRunner::Autonomous);
        manager.requestSeek(300);
    }
    else if (entry == QLatin1String("manager"))
    {
        manager.requestSeek(5000);
    }
    else
    {
        show->requestSeek(5000);
    }
    const auto settle = [&]() {
        for (int i = 0; i < 30 && (recorder.pendingControlRuns() > 0 || show->commandWorkPending() ||
                                   !legacy->isRunning()); i++)
            tickAndDeliver(&doc);
    };
    settle();

    QCOMPARE(button->state(), VCButton::Active);
    QVERIFY(recorder.pendingUserRequests().isEmpty());
    // the cancelled legacy Start never runs, no prefix replays it
    QCOMPARE(legacy->isRunning(), !switched);
    QVERIFY(!show->commandWorkPending());
    if (autonomous)
    {
        // cancelled, then the prefix to 300 ran once
        QVERIFY(show->commandPosition() >= 300);
        QCOMPARE(log.starts(scene->id()), 1);
    }
    else
    {
        // ignored: the external clock still owns the position
        QCOMPARE(show->commandPosition(), quint32(200));
        // later crossings still reach the GUI
        show->setExternalElapsedTime(450);
        for (int i = 0; i < 20 && laterButton->state() != VCButton::Active; i++)
            tickAndDeliver(&doc);
        QCOMPARE(laterButton->state(), VCButton::Active);
    }

    show->stop(FunctionParent::master());
    legacy->stop(FunctionParent::master());
    tickAndDeliver(&doc);
}

namespace
{
/** Toggles on A and B and an Adjust slider on A, against one Show */
struct RequestRig
{
    Doc doc{nullptr, 1};
    UiFixture ui{&doc};
    VCBridgeV5 bridge{&doc, ui.vc()};
    ShowCommandRecorder recorder{&doc};
    Scene *a = addTarget(&doc);
    Scene *b = new Scene(&doc);
    Scene *later = new Scene(&doc);
    Show *show = new Show(&doc);
    int frame = -1;
    VCButton *ba = nullptr;
    VCButton *bb = nullptr;
    VCSlider *slider = nullptr;

    RequestRig()
    {
        b->setValue(SceneValue(a->values().first().fxi, 1, 120));
        later->setValue(SceneValue(a->values().first().fxi, 2, 90));
        doc.addFunction(b);
        doc.addFunction(later);
        doc.addFunction(show);
        recorder.setVirtualConsole(ui.vc());
        recorder.setResolvedShow(show->id());
        frame = bridge.addFrame(0, QRect(0, 0, 600, 300), QStringLiteral("Requests"), false);
        ba = addToggle(bridge, ui.vc(), frame, a->id(), 10, QStringLiteral("A"));
        bb = addToggle(bridge, ui.vc(), frame, b->id(), 100, QStringLiteral("B"));
        slider = addAdjustSlider(bridge, ui.vc(), frame, a->id());
    }
    ~RequestRig()
    {
        show->stop(FunctionParent::master());
        for (Scene *f : {a, b, later})
            f->stop(FunctionParent::master());
        tickAndDeliver(&doc, 5);
    }
};
}

void ShowCommandRecorder_Test::claimedOperation_sourceSwitchRunsNoStaleWork_data()
{
    QTest::addColumn<bool>("catchUp");
    QTest::addColumn<QString>("next");
    QTest::addColumn<QString>("boundary");

    for (bool catchUp : {true, false})
        for (const QString &next : {QStringLiteral("legacy"), QStringLiteral("vc")})
            for (const QString &boundary : {QStringLiteral("none"), QStringLiteral("seek"), QStringLiteral("switch")})
                QTest::newRow(qPrintable((catchUp ? QStringLiteral("catch-up, ") : QStringLiteral("ordinary, ")) +
                                         next + QStringLiteral(" next, ") + boundary))
                        << catchUp << next << boundary;
}

void ShowCommandRecorder_Test::claimedOperation_sourceSwitchRunsNoStaleWork()
{
    QFETCH(bool, catchUp);
    QFETCH(QString, next);
    QFETCH(QString, boundary);

    RequestRig r;
    r.slider->setControlledFunction(Function::invalidId());
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 100, r.a->id(), 0.4)));
    QVERIFY(track.insert(next == QLatin1String("legacy")
                         ? ShowCommand::start(2, 100, r.b->id())
                         : ShowCommand::setButtonState(2, 100, r.bb->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::start(3, 800, r.later->id())));
    QVERIFY(track.insert(ShowCommand::setButtonState(4, 4500, r.ba->ensureRecordingId(), false)));
    QVERIFY(track.setExtent(6000) && r.show->setCommandTrack(track));
    r.a->start(r.doc.masterTimer(), FunctionParent::master());
    tickAndDeliver(&r.doc, 2);
    QSignalSpy starts(r.b, &Function::running);
    QSignalSpy laterStarts(r.later, &Function::running);
    QSignalSpy batches(r.show, &Show::controlBatchesReady);

    // inside the claimed intensity: a seek request, which the source switch discards
    bool crossed = false;
    QObject observer;
    connect(r.a, &Function::attributeChanged, &observer, [&](int attribute, qreal value) {
        if (crossed || attribute != Function::Intensity || value != 0.4)
            return;
        crossed = true;
        if (boundary == QLatin1String("none"))
            return;
        r.show->requestSeek(50);
        if (boundary == QLatin1String("switch"))
        {
            r.show->setSyncSource(ShowRunner::External);
            r.show->setExternalElapsedTime(200);
        }
    }, Qt::DirectConnection);
    r.show->start(r.doc.masterTimer(), FunctionParent::master(), catchUp ? 200 : 0);
    if (catchUp)
        tickAndDeliver(&r.doc, 3);
    else
    {
        for (int i = 0; i < 20 && !crossed; i++)
            tickAndDeliver(&r.doc);
        tickAndDeliver(&r.doc, 2);
    }

    // the claimed intensity finished, nothing after it ran
    QVERIFY(crossed);
    QCOMPARE(starts.count(), boundary == QLatin1String("none") ? 1 : 0);
    QCOMPARE(batches.count(), next == QLatin1String("vc") && boundary == QLatin1String("none") ? 1 : 0);
    if (boundary == QLatin1String("switch"))
    {
        // later crossings of the external clock still apply
        r.show->setExternalElapsedTime(900);
        tickAndDeliver(&r.doc, 6);
        QCOMPARE(laterStarts.count(), 1);
        QCOMPARE(r.show->commandTrack().commands().count(), 4);
    }
    QObject::disconnect(r.a, nullptr, &observer, nullptr);
}

void ShowCommandRecorder_Test::userRequest_keepsItsAcceptedDestination_data()
{
    QTest::addColumn<QString>("control");
    QTest::addColumn<QString>("change");

    for (const QString &control : {QStringLiteral("button"), QStringLiteral("slider")})
        for (const QString &change : {QStringLiteral("none"), QStringLiteral("rebind"), QStringLiteral("remove"),
                                      QStringLiteral("mode")})
            QTest::newRow(qPrintable(control + QStringLiteral(", ") + change)) << control << change;
}

void ShowCommandRecorder_Test::userRequest_keepsItsAcceptedDestination()
{
    QFETCH(QString, control);
    QFETCH(QString, change);
    const bool button = control == QLatin1String("button");

    RequestRig r;
    ShowCommandTrack track;
    QVERIFY(track.insert(button ? ShowCommand::setButtonState(1, 100, r.ba->ensureRecordingId(), true)
                                : ShowCommand::setSliderPosition(1, 100, r.slider->ensureRecordingId(),
                                                                 ShowControlRole::AdjustSlider,
                                                                 QStringLiteral("Intensity"), 0.3)));
    QVERIFY(track.setExtent(5000) && r.show->setCommandTrack(track));
    QSignalSpy batches(r.show, &Show::controlBatchesReady);
    QSignalSpy newStarts(r.b, &Function::running);
    r.show->start(r.doc.masterTimer(), FunctionParent::master());
    for (int i = 0; i < 20 && batches.isEmpty(); i++)
        r.doc.masterTimer()->timerTick();
    QCOMPARE(batches.count(), 1);

    // accepted against A while the crossed replay on the control is owed
    r.show->setPause(true);
    if (button)
        r.ba->requestUserStateChange(true);
    else
        r.slider->requestUserValue(140);
    QCOMPARE(r.recorder.pendingUserRequests().count(), 1);
    QSignalSpy reported(&r.recorder, &ShowCommandRecorder::lastErrorChanged);
    if (change == QLatin1String("rebind"))
    {
        if (button)
            r.ba->setFunctionID(r.b->id());
        else
            r.slider->setControlledFunction(r.b->id());
    }
    else if (change == QLatin1String("mode"))
    {
        if (button)
            r.ba->setActionType(VCButton::Flash);
        else
            r.slider->setSliderMode(VCSlider::Level);
    }
    else if (change == QLatin1String("remove"))
    {
        QVERIFY(r.bridge.removeWidget(button ? r.ba->id() : r.slider->id()));
        r.ba = button ? nullptr : r.ba;
        r.slider = button ? r.slider : nullptr;
    }
    // Stop retires the replay before its callback is delivered: only the request is left
    r.show->stop(FunctionParent::master());
    tickAndDeliver(&r.doc, 8);

    QVERIFY(r.recorder.pendingUserRequests().isEmpty());
    QCOMPARE(newStarts.count(), 0);
    QVERIFY(!r.b->isRunning());
    QCOMPARE(r.a->isRunning(), change == QLatin1String("none"));
    // a request whose destination changed is cancelled and reported, never retargeted
    const bool reconfigured = change == QLatin1String("rebind") || change == QLatin1String("mode");
    QCOMPARE(reported.isEmpty(), !reconfigured);
    QCOMPARE(r.recorder.lastError().isEmpty(), !reconfigured);
}

void ShowCommandRecorder_Test::levelChannelSnapshot_sharesNoTimerStorage()
{
    RequestRig r;
    r.slider->setSliderMode(VCSlider::Level);
    r.slider->addLevelChannel(r.a->values().first().fxi, 3);

    // the timer iterates the channels mutably: a shared copy would make it detach mid-loop
    const QList<SceneValue> first = r.slider->levelChannels();
    const QList<SceneValue> second = r.slider->levelChannels();
    QCOMPARE(first, second);
    QVERIFY(first.constData() != second.constData());
}

void ShowCommandRecorder_Test::acceptedSliderInput_recordsEachChangedPosition_data()
{
    QTest::addColumn<QVariantList>("moves");
    QTest::addColumn<QVariantList>("times");
    QTest::addColumn<bool>("queued");
    QTest::addColumn<QVariantList>("positions");
    QTest::addColumn<QVariantList>("stamps");

    // rapid input before any engine tick; the repeated 80 is unchanged
    QTest::newRow("rapid") << QVariantList{20, 80, 80, 40} << QVariantList{1000, 1500, 1500, 2750} << false
                           << QVariantList{0.2, 0.8, 0.4} << QVariantList{1000, 1500, 2750};
    // the native value is unchanged; returning to an earlier position is a change
    QTest::newRow("from native value") << QVariantList{0, 30, 90, 30} << QVariantList{3000, 3000, 4100, 5200}
                                       << false << QVariantList{0.3, 0.9, 0.3} << QVariantList{3000, 4100, 5200};
    // queued behind crossed replay: compared with the latest queued request, not the native 0
    QTest::newRow("queued behind replay") << QVariantList{20, 0, 0, 65} << QVariantList{6000, 6300, 6300, 7000}
                                          << true << QVariantList{0.2, 0.0, 0.65} << QVariantList{6000, 6300, 7000};
}

void ShowCommandRecorder_Test::acceptedSliderInput_recordsEachChangedPosition()
{
    QFETCH(QVariantList, moves);
    QFETCH(QVariantList, times);
    QFETCH(bool, queued);
    QFETCH(QVariantList, positions);
    QFETCH(QVariantList, stamps);

    RequestRig r;
    r.slider->setRangeLowLimit(0);
    r.slider->setRangeHighLimit(100);
    r.show->setSyncSource(ShowRunner::External);
    r.show->setExternalElapsedTime(200);
    ShowCommandTrack track;
    if (queued)
        QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 100, r.slider->ensureRecordingId(),
                                                            ShowControlRole::AdjustSlider,
                                                            QStringLiteral("Intensity"), 0.5)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(r.show->setCommandTrack(track));
    QVERIFY(r.recorder.setRecording(true));
    QVERIFY(r.recorder.isAuthoring());
    if (queued)
    {
        QSignalSpy batches(r.show, &Show::controlBatchesReady);
        r.show->start(r.doc.masterTimer(), FunctionParent::master());
        for (int i = 0; i < 20 && batches.isEmpty(); i++)
            r.doc.masterTimer()->timerTick();
        QCOMPARE(batches.count(), 1);
    }

    for (int i = 0; i < moves.count(); i++)
    {
        r.show->setExternalElapsedTime(times.at(i).toUInt());
        r.slider->requestUserValue(moves.at(i).toInt());
    }

    const auto captured = [&r]()
    {
        QVector<ShowCommand> list;
        for (const ShowCommand &cmd : r.show->commandTrack().commands())
        {
            if (cmd.time >= 1000)
                list.append(cmd);
        }
        return list;
    };
    const auto verify = [&]()
    {
        const QVector<ShowCommand> records = captured();
        QCOMPARE(records.count(), positions.count());
        for (int i = 0; i < records.count(); i++)
        {
            QCOMPARE(int(records.at(i).action), int(ShowCommandAction::SetSliderPosition));
            QCOMPARE(records.at(i).controlId, r.slider->recordingId());
            QCOMPARE(int(records.at(i).role), int(ShowControlRole::AdjustSlider));
            QCOMPARE(records.at(i).attribute, QStringLiteral("Intensity"));
            QVERIFY(qFuzzyCompare(1.0 + records.at(i).position, 1.0 + positions.at(i).toReal()));
            QCOMPARE(records.at(i).time, stamps.at(i).toUInt());
        }
        // accepted input records its desired position, never a function command
        for (const ShowCommand &cmd : r.show->commandTrack().commands())
            QVERIFY(cmd.action == ShowCommandAction::SetSliderPosition);
    };

    // recorded at acceptance, before the engine executed any of it
    verify();
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(r.recorder.track().commands(), r.show->commandTrack().commands());
    QCOMPARE(r.recorder.pendingUserRequests().isEmpty(), !queued);

    tickAndDeliver(&r.doc, 8);

    // executing the requests records nothing more
    verify();
    QVERIFY(r.recorder.pendingUserRequests().isEmpty());
    QCOMPARE(r.slider->value(), moves.last().toInt());
}

void ShowCommandRecorder_Test::movedHostCursor_stampsAcceptedInput_data()
{
    QTest::addColumn<bool>("paused");
    QTest::addColumn<bool>("external");
    QTest::addColumn<int>("cursor");
    QTest::addColumn<bool>("stampsCursor");
    QTest::addColumn<bool>("adoptedByExternal");

    QTest::newRow("stopped") << false << false << 8000 << true << false;
    QTest::newRow("paused") << true << false << 13500 << true << false;
    // the external clock owns the position: a moved cursor stays where it is shown
    QTest::newRow("external stopped") << false << true << 9000 << false << false;
    // moved while autonomous, then an external clock takes the Show at 0 without a tick
    QTest::newRow("adopted by external clock") << false << false << 8000 << false << true;
}

void ShowCommandRecorder_Test::movedHostCursor_stampsAcceptedInput()
{
    QFETCH(bool, paused);
    QFETCH(bool, external);
    QFETCH(int, cursor);
    QFETCH(bool, stampsCursor);
    QFETCH(bool, adoptedByExternal);

    RequestRig r;
    MasterTimer *timer = r.doc.masterTimer();
    if (external)
    {
        r.show->setSyncSource(ShowRunner::External);
        r.show->setExternalElapsedTime(3000);
    }
    ShowCommandTrack track;
    QVERIFY(track.setExtent(60000));
    QVERIFY(r.show->setCommandTrack(track));
    ShowManager manager(r.ui.view(), &r.doc);
    manager.m_detached = true;
    manager.setCurrentShowID(int(r.show->id()));
    QVERIFY(r.recorder.setRecording(true));
    QVERIFY(r.recorder.isAuthoring());
    if (paused)
    {
        manager.playShow();
        for (int i = 0; i < 40; i++)
            timer->timerTick();
        manager.playShow();
        tickAndDeliver(&r.doc, 2);
        QVERIFY(r.show->isPaused());
    }
    const quint32 clockBefore = r.show->commandPosition();
    QSignalSpy clock(r.show, &Show::timeChanged);
    QSignalSpy batches(r.show, &Show::controlBatchesReady);

    manager.requestSeek(cursor);
    if (adoptedByExternal)
    {
        r.show->setSyncSource(ShowRunner::External);
        r.show->setExternalElapsedTime(0);
        QCOMPARE(r.show->commandPosition(), clockBefore);
    }
    r.slider->requestUserValue(90);
    tickAndDeliver(&r.doc, 3);

    const QVector<ShowCommand> records = r.show->commandTrack().commands();
    QCOMPARE(records.count(), 1);
    QCOMPARE(records.first().time, stampsCursor ? quint32(cursor) : clockBefore);
    // the cursor move seeks, dispatches and advances nothing
    QCOMPARE(r.show->commandPosition(), clockBefore);
    QCOMPARE(clock.count(), 0);
    QCOMPARE(batches.count(), 0);
    QCOMPARE(r.show->isRunning(), paused);
    QCOMPARE(r.slider->value(), 90);

    if (paused)
    {
        // once the clock runs again, input is stamped where it is
        manager.playShow();
        tickAndDeliver(&r.doc, 10);
        const quint32 running = r.show->commandPosition();
        QVERIFY(running > quint32(cursor));
        r.slider->requestUserValue(40);
        QCOMPARE(r.show->commandTrack().commands().last().time, running);
    }
}

void ShowCommandRecorder_Test::acceptedButtonInput_recordsDesiredState_data()
{
    QTest::addColumn<QString>("rig");
    QTest::addColumn<QStringList>("clicks");
    QTest::addColumn<QVariantList>("times");
    QTest::addColumn<QVariantList>("ons");
    QTest::addColumn<int>("finalA");
    QTest::addColumn<int>("finalB");

    QTest::newRow("inactive, then active") << QStringLiteral("plain") << QStringList{"a", "a"}
                                           << QVariantList{1000, 2500} << QVariantList{true, false}
                                           << int(VCButton::Inactive) << int(VCButton::Inactive);
    // a Monitoring click is native ON: the button acquires its own activation
    QTest::newRow("monitoring acquires") << QStringLiteral("monitoring") << QStringList{"a"}
                                         << QVariantList{1800} << QVariantList{true}
                                         << int(VCButton::Active) << int(VCButton::Inactive);
    // the Solo Frame stops A natively; only the accepted requests are recorded
    QTest::newRow("solo frame") << QStringLiteral("solo") << QStringList{"a", "b", "b"}
                                << QVariantList{1000, 1200, 3100} << QVariantList{true, true, false}
                                << int(VCButton::Inactive) << int(VCButton::Inactive);
}

void ShowCommandRecorder_Test::acceptedButtonInput_recordsDesiredState()
{
    QFETCH(QString, rig);
    QFETCH(QStringList, clicks);
    QFETCH(QVariantList, times);
    QFETCH(QVariantList, ons);
    QFETCH(int, finalA);
    QFETCH(int, finalB);

    RequestRig r;
    // the rig's fader at 0 on A would stop it natively
    r.slider->setSliderMode(VCSlider::Level);
    r.show->setSyncSource(ShowRunner::External);
    r.show->setExternalElapsedTime(200);
    VCButton *a = r.ba;
    VCButton *b = r.bb;
    if (rig == QLatin1String("solo"))
    {
        const int soloID = r.bridge.addFrame(0, QRect(0, 310, 400, 100), QStringLiteral("Solo"), true);
        a = addToggle(r.bridge, r.ui.vc(), soloID, r.a->id(), 10, QStringLiteral("Solo A"));
        b = addToggle(r.bridge, r.ui.vc(), soloID, r.b->id(), 100, QStringLiteral("Solo B"));
        QVERIFY(a && b);
    }
    if (rig == QLatin1String("monitoring"))
    {
        r.a->start(r.doc.masterTimer(), FunctionParent::master());
        tickAndDeliver(&r.doc, 2);
        QCOMPARE(a->state(), VCButton::Monitoring);
    }
    QVERIFY(r.recorder.setRecording(true));

    for (int i = 0; i < clicks.count(); i++)
    {
        VCButton *button = clicks.at(i) == QLatin1String("a") ? a : b;
        r.show->setExternalElapsedTime(times.at(i).toUInt());
        button->requestUserStateChange(button->state() != VCButton::Active);
        tickAndDeliver(&r.doc, 2);
    }
    tickAndDeliver(&r.doc, 3);

    const QVector<ShowCommand> records = r.show->commandTrack().commands();
    QCOMPARE(records.count(), clicks.count());
    for (int i = 0; i < records.count(); i++)
    {
        VCButton *button = clicks.at(i) == QLatin1String("a") ? a : b;
        QCOMPARE(int(records.at(i).action), int(ShowCommandAction::SetButtonState));
        QCOMPARE(int(records.at(i).role), int(ShowControlRole::ToggleButton));
        QCOMPARE(records.at(i).controlId, button->recordingId());
        QCOMPARE(records.at(i).on, ons.at(i).toBool());
        QCOMPARE(records.at(i).time, times.at(i).toUInt());
    }
    QVERIFY(r.recorder.lastError().isEmpty());
    QCOMPARE(int(a->state()), finalA);
    QCOMPARE(int(b->state()), finalB);
    QCOMPARE(r.a->isRunning(), finalA == int(VCButton::Active));
}

void ShowCommandRecorder_Test::acceptedSliderModes_recordPrimaryValue_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("attribute");
    QTest::addColumn<int>("role");
    QTest::addColumn<QString>("key");
    QTest::addColumn<int>("value");
    QTest::addColumn<qreal>("high");

    QTest::newRow("level") << int(VCSlider::Level) << 0 << int(ShowControlRole::LevelSlider) << QString()
                           << 51 << 255.0;
    // an EFX width runs 0..127
    QTest::newRow("adjust attribute") << int(VCSlider::Adjust) << 1 << int(ShowControlRole::AdjustSlider)
                                      << QStringLiteral("EFX:1") << 102 << 127.0;
    QTest::newRow("submaster") << int(VCSlider::Submaster) << 0 << int(ShowControlRole::SubmasterSlider)
                               << QString() << 204 << 255.0;
    QTest::newRow("grand master") << int(VCSlider::GrandMaster) << 0 << int(ShowControlRole::GrandMasterSlider)
                                  << QString() << 153 << 255.0;
}

void ShowCommandRecorder_Test::acceptedSliderModes_recordPrimaryValue()
{
    QFETCH(int, mode);
    QFETCH(int, attribute);
    QFETCH(int, role);
    QFETCH(QString, key);
    QFETCH(int, value);
    QFETCH(qreal, high);

    RequestRig r;
    r.show->setSyncSource(ShowRunner::External);
    r.show->setExternalElapsedTime(2600);
    r.slider->setSliderMode(VCSlider::SliderMode(mode));
    if (mode == VCSlider::Level)
        r.slider->addLevelChannel(r.a->values().first().fxi, 3);
    if (mode == VCSlider::Adjust)
    {
        EFX *efx = new EFX(&r.doc);
        QVERIFY(r.doc.addFunction(efx));
        r.slider->setControlledFunction(efx->id());
        r.slider->setControlledAttribute(attribute);
    }
    QCOMPARE(r.slider->rangeHighLimit(), high);
    QVERIFY(r.recorder.setRecording(true));

    r.slider->requestUserValue(value);
    tickAndDeliver(&r.doc, 2);

    const QVector<ShowCommand> records = r.show->commandTrack().commands();
    QCOMPARE(records.count(), 1);
    QCOMPARE(int(records.first().action), int(ShowCommandAction::SetSliderPosition));
    QCOMPARE(int(records.first().role), role);
    QCOMPARE(records.first().attribute, key);
    QCOMPARE(records.first().position, qreal(value) / high);
    QCOMPARE(records.first().time, quint32(2600));
    QCOMPARE(r.slider->value(), value);
    QVERIFY(r.recorder.lastError().isEmpty());
}

void ShowCommandRecorder_Test::unsupportedInput_reportsAndStaysLive_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("item");
    QTest::addColumn<bool>("reported");

    QTest::newRow("flash button") << QStringLiteral("flash button") << QString() << true;
    QTest::newRow("blackout button") << QStringLiteral("blackout button") << QString() << true;
    // inner boundary: delivered to the widget as its mapped input id
    QTest::newRow("slider reset input") << QStringLiteral("slider reset input") << QString() << true;
    QTest::newRow("slider flash input") << QStringLiteral("slider flash input") << QString() << true;
    // a real click on the production slider item's button
    QTest::newRow("slider reset button") << QStringLiteral("reset button")
                                         << QStringLiteral("qrc:/VCSliderItem.qml") << true;
    QTest::newRow("slider flash button") << QStringLiteral("flash button press")
                                         << QStringLiteral("qrc:/VCSliderItem.qml") << true;
    QTest::newRow("flow slider reset button") << QStringLiteral("reset button")
                                              << QStringLiteral("qrc:/FlowSliderItem.qml") << true;
    QTest::newRow("flow slider flash button") << QStringLiteral("flash button press")
                                              << QStringLiteral("qrc:/FlowSliderItem.qml") << true;
    // programmatic use is nobody's input
    QTest::newRow("programmatic reset") << QStringLiteral("programmatic reset") << QString() << false;
    QTest::newRow("programmatic flash") << QStringLiteral("programmatic flash") << QString() << false;
}

void ShowCommandRecorder_Test::unsupportedInput_reportsAndStaysLive()
{
    QFETCH(QString, input);
    QFETCH(QString, item);
    QFETCH(bool, reported);

    RequestRig r;
    r.show->setSyncSource(ShowRunner::External);
    r.show->setExternalElapsedTime(2600);
    r.slider->setMonitorEnabled(true);
    r.slider->setAdjustFlashEnabled(true);
    QVERIFY(r.recorder.setRecording(true));
    std::unique_ptr<QQuickItem> sliderItem;
    QQuickItem *button = nullptr;
    if (item.isEmpty() == false)
    {
        QQuickView *view = r.ui.view();
        view->resize(600, 400);
        sliderItem.reset(renderSliderItem(view, r.slider, item));
        QVERIFY(sliderItem);
        sliderItem->setSize(QSizeF(120, 300));
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view));
        QQmlExpression glyph(qmlContext(sliderItem.get()), sliderItem.get(), QStringLiteral("FontAwesome.fa_xmark"));
        button = input == QLatin1String("reset button")
                     ? findVisualItemWith(sliderItem.get(), "faSource", glyph.evaluate())
                     : findVisualItemWith(sliderItem.get(), "tooltip", QStringLiteral("Flash the controlled Function"));
        QVERIFY(button);
    }
    const auto buttonCenter = [&button]() {
        return button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();
    };
    QSignalSpy errors(&r.recorder, &ShowCommandRecorder::lastErrorChanged);

    bool live = false;
    if (input == QLatin1String("reset button"))
    {
        r.slider->setIsOverriding(true);
        QTest::mouseClick(r.ui.view(), Qt::LeftButton, Qt::NoModifier, buttonCenter());
        live = r.slider->isOverriding() == false;
    }
    else if (input == QLatin1String("flash button press"))
    {
        // from the slider's level, press flashes to full, release restores it
        r.slider->setValue(102);
        tickAndDeliver(&r.doc, 2);
        const qreal level = r.a->getAttributeValue(Function::Intensity);
        QVERIFY(qFuzzyCompare(level, qreal(0.4)));
        QTest::mousePress(r.ui.view(), Qt::LeftButton, Qt::NoModifier, buttonCenter());
        live = qFuzzyCompare(r.a->getAttributeValue(Function::Intensity), qreal(1.0));
        QTest::mouseRelease(r.ui.view(), Qt::LeftButton, Qt::NoModifier, buttonCenter());
        live = live && qFuzzyCompare(r.a->getAttributeValue(Function::Intensity), level);
    }
    else if (input == QLatin1String("programmatic reset"))
    {
        r.slider->setIsOverriding(true);
        r.slider->setIsOverriding(false);
        live = r.slider->isOverriding() == false;
    }
    else if (input == QLatin1String("programmatic flash"))
    {
        r.slider->flashFunction(true);
        live = qFuzzyCompare(r.a->getAttributeValue(Function::Intensity), qreal(1.0));
        r.slider->flashFunction(false);
    }
    else if (input == QLatin1String("flash button"))
    {
        VCButton *flash = qobject_cast<VCButton *>(r.ui.vc()->widget(
            r.bridge.addButton(r.frame, QRect(300, 10, 80, 40), r.a->id(), QStringLiteral("Flash"),
                               QStringLiteral("flash"))));
        QVERIFY(flash);
        flash->requestUserStateChange(true);
        live = flash->state() == VCButton::Active;
        flash->requestUserStateChange(false);
    }
    else if (input == QLatin1String("blackout button"))
    {
        VCButton *blackout = qobject_cast<VCButton *>(r.ui.vc()->widget(
            r.bridge.addButton(r.frame, QRect(300, 10, 80, 40), Function::invalidId(),
                               QStringLiteral("Blackout"), QStringLiteral("blackout"))));
        QVERIFY(blackout);
        blackout->requestUserStateChange(true);
        tickAndDeliver(&r.doc, 2);
        live = r.doc.inputOutputMap()->blackout();
        r.doc.inputOutputMap()->setBlackout(false);
    }
    else if (input == QLatin1String("slider reset input"))
    {
        r.slider->setIsOverriding(true);
        r.slider->deliverInput(1, UCHAR_MAX, ShowCommandOrigin::Midi);
        live = r.slider->isOverriding() == false;
    }
    else
    {
        r.slider->deliverInput(2, UCHAR_MAX, ShowCommandOrigin::Midi);
        live = qFuzzyCompare(r.a->getAttributeValue(Function::Intensity), qreal(1.0));
        r.slider->deliverInput(2, 0, ShowCommandOrigin::Midi);
    }
    tickAndDeliver(&r.doc, 2);

    QVERIFY(live);
    QCOMPARE(r.show->commandTrack().count(), 0);
    QCOMPARE(errors.count(), reported ? 1 : 0);
    QCOMPARE(r.recorder.lastError().startsWith(QStringLiteral("Not recorded")), reported);
}

void ShowCommandRecorder_Test::patchedInput_recordsOnlyUserRoutes_data()
{
    QTest::addColumn<QString>("plugin");
    QTest::addColumn<bool>("button");
    QTest::addColumn<bool>("relative");
    QTest::addColumn<bool>("pickup");
    QTest::addColumn<QVariantList>("inputs");
    QTest::addColumn<QVariantList>("recorded");
    QTest::addColumn<int>("finalValue");

    // patched plugin -> console input slot -> page mapping -> pickup -> VC request
    QTest::newRow("osc button") << QStringLiteral("OSC") << true << false << false << QVariantList{255}
                                << QVariantList{1.0} << 0;
    QTest::newRow("osc slider") << QStringLiteral("OSC") << false << false << false << QVariantList{204, 51}
                                << QVariantList{0.8, 0.2} << 51;
    // an encoder steps the value by its sensitivity: +20, +20, -20
    QTest::newRow("osc relative slider") << QStringLiteral("OSC") << false << true << false
                                         << QVariantList{70, 80, 60}
                                         << QVariantList{20 / 255.0, 40 / 255.0, 20 / 255.0} << 20;
    // from 100, pickup rejects 204 and 150, then follows 101 and 60
    QTest::newRow("osc pickup") << QStringLiteral("OSC") << false << false << true
                                << QVariantList{204, 150, 101, 60} << QVariantList{101 / 255.0, 60 / 255.0} << 60;
    QTest::newRow("midi slider") << QStringLiteral("MIDI") << false << false << false << QVariantList{102}
                                 << QVariantList{0.4} << 102;
    QTest::newRow("artnet slider") << QStringLiteral("ArtNet") << false << false << false << QVariantList{204}
                                   << QVariantList() << 204;
    QTest::newRow("e131 button") << QStringLiteral("E1.31") << true << false << false << QVariantList{255}
                                 << QVariantList() << 0;
}

void ShowCommandRecorder_Test::patchedInput_recordsOnlyUserRoutes()
{
    QFETCH(QString, plugin);
    QFETCH(bool, button);
    QFETCH(bool, relative);
    QFETCH(bool, pickup);
    QFETCH(QVariantList, inputs);
    QFETCH(QVariantList, recorded);
    QFETCH(int, finalValue);

    RequestRig r;
    QVERIFY(patchInputPlugin(&r.doc, plugin, kSourceUniverse));
    r.show->setSyncSource(ShowRunner::External);
    r.show->setExternalElapsedTime(kRecordPosition);
    VCWidget *widget = button ? static_cast<VCWidget *>(r.ba) : static_cast<VCWidget *>(r.slider);
    if (pickup)
    {
        r.slider->setValue(100, true, true);
        r.slider->setCatchValues(true);
    }
    QSharedPointer<QLCInputSource> source(new QLCInputSource(kSourceUniverse, kSourceChannel));
    source->setID(0);
    if (relative)
    {
        source->setWorkingMode(QLCInputSource::Encoder);
        source->setSensitivity(20);
        QVERIFY(QObject::connect(source.data(), SIGNAL(inputValueChanged(quint32,quint32,uchar)),
                                 widget, SLOT(slotInputSourceValueChanged(quint32,quint32,uchar))));
    }
    widget->addInputSource(source);
    r.ui.vc()->page(0)->mapInputSource(source, widget);
    QVERIFY(r.recorder.setRecording(true));

    for (const QVariant &value : inputs)
        QVERIFY(deliverMappedInput(r.ui.vc(), uchar(value.toInt())));
    tickAndDeliver(&r.doc, 3);

    const QVector<ShowCommand> records = r.show->commandTrack().commands();
    QCOMPARE(records.count(), recorded.count());
    for (int i = 0; i < records.count(); i++)
    {
        QCOMPARE(records.at(i).controlId, widget->recordingId());
        QCOMPARE(records.at(i).time, kRecordPosition);
        if (button)
        {
            QCOMPARE(int(records.at(i).action), int(ShowCommandAction::SetButtonState));
            QVERIFY(records.at(i).on);
        }
        else
        {
            QCOMPARE(int(records.at(i).action), int(ShowCommandAction::SetSliderPosition));
            QVERIFY(qFuzzyCompare(1.0 + records.at(i).position, 1.0 + recorded.at(i).toReal()));
        }
    }
    // the live effect is the same on every route
    if (button)
        QCOMPARE(r.ba->state(), VCButton::Active);
    else
        QCOMPARE(r.slider->value(), finalValue);
}

void ShowCommandRecorder_Test::nonUserChanges_recordNothing_data()
{
    QTest::addColumn<QString>("change");
    QTest::addColumn<int>("records");

    // inner boundary: the widget request with that origin
    QTest::newRow("audio request") << QStringLiteral("audio request") << 0;
    QTest::newRow("programmatic request") << QStringLiteral("programmatic request") << 0;
    QTest::newRow("replay") << QStringLiteral("replay") << 1;
    QTest::newRow("function feedback") << QStringLiteral("function feedback") << 1;
    // only the Collection's own button was operated
    QTest::newRow("collection child") << QStringLiteral("collection child") << 1;
    // with the replayed record: accepted while recording, executed after disarm, once;
    // accepted after disarm, never
    QTest::newRow("after disarm") << QStringLiteral("after disarm") << 2;
}

void ShowCommandRecorder_Test::nonUserChanges_recordNothing()
{
    QFETCH(QString, change);
    QFETCH(int, records);

    RequestRig r;
    r.show->setSyncSource(ShowRunner::External);
    r.show->setExternalElapsedTime(2600);
    ShowCommandTrack track;
    if (change == QLatin1String("replay") || change == QLatin1String("after disarm"))
        QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, r.ba->ensureRecordingId(), true)));
    if (change == QLatin1String("function feedback"))
        QVERIFY(track.insert(ShowCommand::setIntensity(0, 100, r.a->id(), 0.4)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(r.show->setCommandTrack(track));
    if (change == QLatin1String("function feedback"))
    {
        r.slider->requestUserValue(204);
        tickAndDeliver(&r.doc, 3);
        QVERIFY(r.a->isRunning());
    }
    VCButton *look = nullptr;
    if (change == QLatin1String("collection child"))
    {
        r.slider->setSliderMode(VCSlider::Level);
        Collection *collection = new Collection(&r.doc);
        QVERIFY(r.doc.addFunction(collection));
        collection->addFunction(r.a->id());
        look = addToggle(r.bridge, r.ui.vc(), r.frame, collection->id(), 300, QStringLiteral("Look"));
        QVERIFY(look);
    }
    QVERIFY(r.recorder.setRecording(true));

    if (change == QLatin1String("audio request"))
    {
        r.slider->requestUserValue(153, true, ShowCommandOrigin::Audio);
        QCOMPARE(r.slider->value(), 153);
    }
    else if (change == QLatin1String("programmatic request"))
    {
        r.ba->requestUserStateChange(true, ShowCommandOrigin::Programmatic);
        QCOMPARE(r.ba->state(), VCButton::Active);
    }
    else if (change == QLatin1String("replay") || change == QLatin1String("function feedback"))
    {
        r.show->start(r.doc.masterTimer(), FunctionParent::master());
        tickAndDeliver(&r.doc, 8);
        if (change == QLatin1String("replay"))
            QCOMPARE(r.ba->state(), VCButton::Active);
        else
            QCOMPARE(r.slider->value(), 102);
    }
    else if (change == QLatin1String("collection child"))
    {
        look->requestUserStateChange(true);
        tickAndDeliver(&r.doc, 3);
        QCOMPARE(r.ba->state(), VCButton::Monitoring);
    }
    else
    {
        // the replayed ON is owed, so A's click waits behind it
        QSignalSpy batches(r.show, &Show::controlBatchesReady);
        r.show->start(r.doc.masterTimer(), FunctionParent::master());
        for (int i = 0; i < 20 && batches.isEmpty(); i++)
            r.doc.masterTimer()->timerTick();
        r.ba->requestUserStateChange(true);
        QCOMPARE(r.recorder.pendingUserRequests().count(), 1);
        QVERIFY(r.recorder.setRecording(false));
        tickAndDeliver(&r.doc, 8);
        QVERIFY(r.recorder.pendingUserRequests().isEmpty());
        r.slider->requestUserValue(77);
        tickAndDeliver(&r.doc, 2);
        QCOMPARE(r.slider->value(), 77);
    }
    tickAndDeliver(&r.doc, 3);

    QCOMPARE(r.show->commandTrack().count(), records);
    if (change == QLatin1String("collection child"))
        QCOMPARE(r.show->commandTrack().commands().first().controlId, look->recordingId());
}

namespace
{
/** The Show as it is saved to workspace XML */
QByteArray savedShowXml(Show *show)
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    show->saveXML(&writer);
    writer.writeEndDocument();
    return buffer.data();
}

/** The Show's command track as saved to and loaded back from workspace XML */
ShowCommandTrack savedCommandTrack(Doc *doc, Show *show)
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    show->saveXML(&writer);
    writer.writeEndDocument();

    QXmlStreamReader reader(buffer.data());
    reader.readNextStartElement();
    std::unique_ptr<Show> loaded(new Show(doc));
    if (loaded->loadXML(reader) == false)
        return ShowCommandTrack();
    return loaded->commandTrack();
}
}

void ShowCommandRecorder_Test::checkpoint_savesAcceptedContentEnd_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<int>("savedCount");
    QTest::addColumn<quint32>("savedExtent");

    // last item at 40 s, REC clock at 55 s
    QTest::newRow("checkpoint while armed") << QStringLiteral("checkpoint") << 2 << 40000u;
    QTest::newRow("disarm") << QStringLiteral("disarm") << 2 << 40000u;
    QTest::newRow("shorter loop pass") << QStringLiteral("loop") << 2 << 40000u;
    QTest::newRow("retry after failure") << QStringLiteral("retry") << 3 << 40000u;
    QTest::newRow("discard after failure") << QStringLiteral("discard") << 1 << 5000u;
    // accepted capture is not native execution: nothing waits for the queued request
    QTest::newRow("queued request") << QStringLiteral("queued") << 3 << 40000u;
}

void ShowCommandRecorder_Test::checkpoint_savesAcceptedContentEnd()
{
    QFETCH(QString, scenario);
    QFETCH(int, savedCount);
    QFETCH(quint32, savedExtent);

    RequestRig r;
    r.show->setSyncSource(ShowRunner::External);
    r.show->setExternalElapsedTime(200);
    const bool failing = scenario == QLatin1String("retry") || scenario == QLatin1String("discard");
    ShowCommandTrack track;
    if (scenario == QLatin1String("queued"))
        QVERIFY(track.insert(ShowCommand::setSliderPosition(0, 100, r.slider->ensureRecordingId(),
                                                            ShowControlRole::AdjustSlider,
                                                            QStringLiteral("Intensity"), 0.5)));
    QVERIFY(r.show->setCommandTrack(track));
    QVERIFY(r.recorder.setRecording(true));
    QSignalSpy batches(r.show, &Show::controlBatchesReady);
    if (failing || scenario == QLatin1String("queued"))
    {
        r.show->start(r.doc.masterTimer(), FunctionParent::master());
        for (int i = 0; i < 20 && r.show->isRunning() == false; i++)
            r.doc.masterTimer()->timerTick();
        for (int i = 0; i < 20 && scenario == QLatin1String("queued") && batches.isEmpty(); i++)
            r.doc.masterTimer()->timerTick();
    }
    Scene *gone = new Scene(&r.doc);
    QVERIFY(r.doc.addFunction(gone));
    const quint32 goneId = gone->id();
    if (failing)
    {
        // a function command whose target then disappears makes every publication fail
        r.show->setExternalElapsedTime(5000);
        ShowCommandInput start;
        start.origin = ShowCommandOrigin::Pointer;
        start.action = ShowCommandAction::Start;
        start.functionId = goneId;
        QVERIFY(r.recorder.submitUserInput(start));
        QVERIFY(r.doc.deleteFunction(goneId));
    }

    r.show->setExternalElapsedTime(12000);
    r.slider->requestUserValue(51);
    r.show->setExternalElapsedTime(40000);
    r.ba->requestUserStateChange(true);
    r.show->setExternalElapsedTime(scenario == QLatin1String("loop") ? 50000 : 55000);
    const QVector<ShowCommand> accepted = r.recorder.track().commands();

    if (scenario == QLatin1String("checkpoint") || scenario == QLatin1String("queued"))
    {
        QVERIFY(r.recorder.checkpoint());
        // both requests on A wait behind its crossed replay
        QCOMPARE(r.recorder.pendingUserRequests().count(), scenario == QLatin1String("queued") ? 2 : 0);
    }
    else if (scenario == QLatin1String("disarm"))
    {
        QVERIFY(r.recorder.setRecording(false));
    }
    else if (scenario == QLatin1String("loop"))
    {
        r.show->setExternalElapsedTime(1000);
        r.show->setExternalElapsedTime(8000);
        QVERIFY(r.recorder.setRecording(false));
    }
    else
    {
        QCOMPARE(r.show->commandTrack().count(), 1);
        QVERIFY(!r.recorder.checkpoint());
        QVERIFY(!r.recorder.lastError().isEmpty());
        QVERIFY(r.recorder.isAuthoring());
        if (scenario == QLatin1String("discard"))
            r.recorder.discardUnpublished();
        Scene *back = new Scene(&r.doc);
        QVERIFY(r.doc.addFunction(back, goneId));
        QVERIFY(r.recorder.checkpoint());
        // no echo: accepted input already ran live in this traversal
        tickAndDeliver(&r.doc, 4);
        QCOMPARE(batches.count(), 0);
        QVERIFY(!back->isRunning());
    }

    const ShowCommandTrack saved = savedCommandTrack(&r.doc, r.show);
    QCOMPARE(saved.count(), savedCount);
    QCOMPARE(saved.extent(), savedExtent);
    QCOMPARE(saved.lastCommandTime(), savedExtent);
    // the same ids, order and data that were accepted
    QCOMPARE(saved.commands(), r.show->commandTrack().commands());
    if (failing == false)
        QCOMPARE(saved.commands(), accepted);
    if (scenario == QLatin1String("retry"))
    {
        QCOMPARE(int(saved.commands().at(1).action), int(ShowCommandAction::SetSliderPosition));
        QCOMPARE(saved.commands().at(1).time, 12000u);
        QCOMPARE(int(saved.commands().at(2).action), int(ShowCommandAction::SetButtonState));
        QVERIFY(saved.commands().at(1).id > saved.commands().at(0).id);
        QVERIFY(saved.commands().at(2).id > saved.commands().at(1).id);
    }

    const bool armed = scenario != QLatin1String("disarm") && scenario != QLatin1String("loop");
    QCOMPARE(r.recorder.isRecording(), armed);
    QCOMPARE(r.show->commandRecording(), armed);
    if (armed)
    {
        // input after the checkpoint is unsaved data
        r.doc.resetModified();
        r.show->setExternalElapsedTime(56000);
        r.slider->requestUserValue(102);
        QVERIFY(r.doc.isModified());
        QCOMPARE(r.show->commandTrack().count(), savedCount + 1);
        QCOMPARE(saved.count(), savedCount);
    }
    tickAndDeliver(&r.doc, 8);
}

void ShowCommandRecorder_Test::takeBoundary_savesContentEndWithoutNewInput_data()
{
    QTest::addColumn<QString>("boundary");
    QTest::addColumn<bool>("withClip");
    QTest::addColumn<bool>("capture");

    // retained command at 40 s, authored extent 60 s, REC clock at 55 s;
    // the optional clip ends at 45 s
    for (const QString &boundary : {QStringLiteral("checkpoint"), QStringLiteral("disarm"),
                                    QStringLiteral("handover")})
    {
        for (bool capture : {false, true})
        {
            for (bool withClip : {false, true})
            {
                QTest::newRow(qPrintable(QStringLiteral("%1, %2, %3").arg(boundary,
                              capture ? QStringLiteral("new input") : QStringLiteral("no new input"),
                              withClip ? QStringLiteral("clip") : QStringLiteral("commands only"))))
                    << boundary << withClip << capture;
            }
        }
    }
}

void ShowCommandRecorder_Test::takeBoundary_savesContentEndWithoutNewInput()
{
    QFETCH(QString, boundary);
    QFETCH(bool, withClip);
    QFETCH(bool, capture);

    RequestRig r;
    ShowCommandTrack retained;
    QVERIFY(retained.insert(ShowCommand::setButtonState(1, 40000, r.ba->ensureRecordingId(), true)));
    QVERIFY(retained.setExtent(60000));
    QVERIFY(r.show->setCommandTrack(retained));
    if (withClip)
    {
        auto *track = new Track(Function::invalidId());
        auto *clip = new ShowFunction(r.show->getLatestShowFunctionId());
        clip->setFunctionID(r.b->id());
        clip->setStartTime(1000);
        clip->setDuration(44000);
        track->addShowFunction(clip);
        QVERIFY(r.show->addTrack(track));
    }
    r.show->setSyncSource(ShowRunner::External);
    r.show->setExternalElapsedTime(12000);
    QVERIFY(r.recorder.setRecording(true));
    if (capture)
        r.slider->requestUserValue(51);
    r.show->setExternalElapsedTime(55000);
    const QVector<ShowCommand> accepted = r.recorder.track().commands();
    Show *next = new Show(&r.doc);
    QVERIFY(r.doc.addFunction(next));
    const ShowCommandTrack before = savedCommandTrack(&r.doc, r.show);
    r.doc.resetModified();
    QSignalSpy errors(&r.recorder, &ShowCommandRecorder::lastErrorChanged);
    QSignalSpy dirtied(&r.doc, &Doc::modified);

    if (boundary == QLatin1String("checkpoint"))
        QVERIFY(r.recorder.checkpoint());
    else if (boundary == QLatin1String("disarm"))
        QVERIFY(r.recorder.setRecording(false));
    else
        QVERIFY(r.recorder.setResolvedShow(next->id()));

    // the workspace Save and Load of the Show
    QBuffer buffer;
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    QVERIFY(r.show->saveXML(&writer));
    writer.writeEndDocument();
    QXmlStreamReader reader(buffer.data());
    QVERIFY(reader.readNextStartElement());
    Show loaded(&r.doc);
    QVERIFY(loaded.loadXML(reader));
    const ShowCommandTrack saved = loaded.commandTrack();

    // commands and clips, ending at the last retained item
    QCOMPARE(saved.commands(), accepted);
    QCOMPARE(saved.count(), capture ? 2 : 1);
    QCOMPARE(loaded.totalDuration(), withClip ? 45000u : 0u);
    QCOMPARE(saved.extent(), 40000u);
    QCOMPARE(qMax(loaded.totalDuration(), saved.extent()), withClip ? 45000u : 40000u);
    QCOMPARE(r.show->commandTrack().extent(), 40000u);
    QCOMPARE(errors.count(), 0);
    // captured input already published its end; otherwise the boundary
    // changed what is saved, which is unsaved work until a save
    const bool changed = capture == false;
    QCOMPARE(before.extent() != saved.extent(), changed);
    QCOMPARE(r.doc.isModified(), changed);
    QCOMPARE(dirtied.count(), changed ? 1 : 0);
    QCOMPARE(r.recorder.isRecording(), boundary != QLatin1String("disarm"));
    QCOMPARE(r.show->commandRecording(), boundary == QLatin1String("checkpoint"));
    if (boundary == QLatin1String("handover"))
        QCOMPARE(r.recorder.boundShowId(), next->id());
}

void ShowCommandRecorder_Test::takeBoundary_marksPersistedChangeModified_data()
{
    QTest::addColumn<QString>("boundary");
    QTest::addColumn<QString>("setup");
    QTest::addColumn<bool>("dirtyBefore");
    QTest::addColumn<bool>("published");
    QTest::addColumn<bool>("changed");

    // a clean saved snapshot with a 60 s tail after its last command at 40 s
    QTest::newRow("disarm-unsaved") << QStringLiteral("disarm") << QStringLiteral("tail") << false << true << true;
    QTest::newRow("handover-unsaved") << QStringLiteral("handover") << QStringLiteral("tail") << false << true << true;
    // preparing a snapshot changes it: dirty until a save clears it
    QTest::newRow("changed checkpoint") << QStringLiteral("checkpoint") << QStringLiteral("tail") << false << true
                                        << true;
    QTest::newRow("repeated checkpoint") << QStringLiteral("checkpoint") << QStringLiteral("repeat") << false
                                         << true << false;
    QTest::newRow("already normalized disarm") << QStringLiteral("disarm") << QStringLiteral("normalized") << false
                                               << true << false;
    QTest::newRow("REC off checkpoint") << QStringLiteral("checkpoint") << QStringLiteral("off") << false << true
                                        << false;
    // pending input no Show takes: nothing is published, the dirty state stays
    QTest::newRow("failed disarm, clean") << QStringLiteral("disarm") << QStringLiteral("failing") << false << false
                                          << false;
    QTest::newRow("failed handover, dirty") << QStringLiteral("handover") << QStringLiteral("failing") << true
                                            << false << false;
}

void ShowCommandRecorder_Test::takeBoundary_marksPersistedChangeModified()
{
    QFETCH(QString, boundary);
    QFETCH(QString, setup);
    QFETCH(bool, dirtyBefore);
    QFETCH(bool, published);
    QFETCH(bool, changed);

    RequestRig r;
    Show *next = new Show(&r.doc);
    QVERIFY(r.doc.addFunction(next));
    ShowCommandTrack retained;
    QVERIFY(retained.insert(ShowCommand::setButtonState(17, 40000, r.ba->ensureRecordingId(), true)));
    QVERIFY(retained.setExtent(setup == QLatin1String("normalized") ? 40000 : 60000));
    QVERIFY(r.show->setCommandTrack(retained));
    r.show->setSyncSource(ShowRunner::External);
    r.show->setExternalElapsedTime(55000);
    QVERIFY(r.recorder.setRecording(setup != QLatin1String("off")));
    if (setup == QLatin1String("repeat"))
        QVERIFY(r.recorder.checkpoint());
    if (setup == QLatin1String("failing"))
    {
        // a published command whose target then disappears: later input stays pending
        Scene *gone = new Scene(&r.doc);
        QVERIFY(r.doc.addFunction(gone));
        ShowCommandInput start;
        start.origin = ShowCommandOrigin::Pointer;
        start.action = ShowCommandAction::Start;
        start.functionId = gone->id();
        QVERIFY(r.recorder.submitUserInput(start));
        QVERIFY(r.doc.deleteFunction(gone->id()));
        r.slider->requestUserValue(51);
        QVERIFY(!r.recorder.checkpoint());
    }
    const QByteArray before = savedShowXml(r.show);
    if (dirtyBefore)
        r.doc.setModified();
    else
        r.doc.resetModified();
    QSignalSpy dirtied(&r.doc, &Doc::modified);

    bool ok = false;
    if (boundary == QLatin1String("checkpoint"))
        ok = r.recorder.checkpoint();
    else if (boundary == QLatin1String("disarm"))
        ok = r.recorder.setRecording(false);
    else
        ok = r.recorder.setResolvedShow(next->id());
    QCoreApplication::processEvents();

    const QByteArray after = savedShowXml(r.show);
    QCOMPARE(ok, published);
    QCOMPARE(after != before, changed);
    QCOMPARE(savedCommandTrack(&r.doc, r.show).extent(),
             changed || setup == QLatin1String("normalized") || setup == QLatin1String("repeat") ? 40000u
             : setup == QLatin1String("failing")                                               ? 55000u
                                                                                               : 60000u);
    QCOMPARE(r.doc.isModified(), dirtyBefore || changed);
    QCOMPARE(dirtied.count(), changed ? 1 : 0);

    if (changed && boundary == QLatin1String("checkpoint"))
    {
        // a modeled successful save; the next input is unsaved again
        r.doc.resetModified();
        r.show->setExternalElapsedTime(56000);
        r.slider->requestUserValue(102);
        QVERIFY(r.doc.isModified());
        QVERIFY(r.recorder.isRecording());
        QCOMPARE(r.show->commandTrack().count(), 2);
    }
}

void ShowCommandRecorder_Test::automaticShowChange_handsRecordingOver_data()
{
    QTest::addColumn<QString>("change");

    QTest::newRow("to another show") << QStringLiteral("to another show");
    // no Show resolved in between: armed, then the next one binds
    QTest::newRow("through a gap") << QStringLiteral("through a gap");
    QTest::newRow("previous cannot finalize") << QStringLiteral("previous cannot finalize");
    // an earlier, different report is still shown when the handover fails
    QTest::newRow("previous cannot finalize after another report")
        << QStringLiteral("previous cannot finalize after another report");
}

void ShowCommandRecorder_Test::automaticShowChange_handsRecordingOver()
{
    QFETCH(QString, change);

    RequestRig r;
    Show *a = r.show;
    a->setSyncSource(ShowRunner::External);
    a->setExternalElapsedTime(4000);
    const auto addShow = [&r](quint32 at) {
        Show *show = new Show(&r.doc);
        r.doc.addFunction(show);
        show->setSyncSource(ShowRunner::External);
        show->setExternalElapsedTime(at);
        return show;
    };
    Show *b = addShow(900);
    Show *c = addShow(300);
    Scene *gone = new Scene(&r.doc);
    QVERIFY(r.doc.addFunction(gone));
    const quint32 goneId = gone->id();
    QVERIFY(r.recorder.setRecording(true));
    QVERIFY(r.recorder.isAuthoring());
    const bool failing = change.startsWith(QLatin1String("previous cannot finalize"));
    if (failing)
    {
        // a function command whose target then disappears makes A's publication fail
        ShowCommandInput start;
        start.origin = ShowCommandOrigin::Pointer;
        start.action = ShowCommandAction::Start;
        start.functionId = goneId;
        QVERIFY(r.recorder.submitUserInput(start));
        QVERIFY(r.doc.deleteFunction(goneId));
        // B has a minute to play
        ShowCommandTrack minute;
        QVERIFY(minute.setExtent(60000));
        QVERIFY(b->setCommandTrack(minute));
        b->start(r.doc.masterTimer(), FunctionParent::master());
        tickAndDeliver(&r.doc, 2);
        QVERIFY(b->isRunning());
    }
    a->setExternalElapsedTime(4700);
    r.slider->requestUserValue(51);
    // failing: the function command alone, the slider input waits unpublished
    QCOMPARE(a->commandTrack().count(), 1);
    if (change.endsWith(QLatin1String("after another report")))
        r.recorder.reportUnsupported(QStringLiteral("flash button"), ShowCommandOrigin::Pointer);

    QSignalSpy reported(&r.recorder, &ShowCommandRecorder::lastErrorChanged);
    const bool handed = r.recorder.setResolvedShow(change == QLatin1String("through a gap") ? Function::invalidId()
                                                                                           : b->id());
    QCOMPARE(handed, !failing);
    QVERIFY(!a->commandRecording());

    if (change == QLatin1String("to another show"))
    {
        // A closed at its last item; recording goes on in B with its own clock
        QCOMPARE(a->commandTrack().extent(), 4700u);
        QVERIFY(r.recorder.isAuthoring());
        QCOMPARE(r.recorder.boundShowId(), b->id());
        QVERIFY(b->commandRecording());
        r.slider->requestUserValue(153);
        QCOMPARE(b->commandTrack().count(), 1);
        QCOMPARE(b->commandTrack().commands().first().time, 900u);
        QCOMPARE(a->commandTrack().count(), 1);
    }
    else if (change == QLatin1String("through a gap"))
    {
        QCOMPARE(r.recorder.phaseValue(), int(ShowRecordPhase::Armed));
        QVERIFY(r.recorder.isRecording());
        r.slider->requestUserValue(153);
        QCOMPARE(a->commandTrack().count(), 1);
        QVERIFY(r.recorder.setResolvedShow(c->id()));
        QVERIFY(r.recorder.isAuthoring());
        QCOMPARE(r.recorder.boundShowId(), c->id());
        r.slider->requestUserValue(204);
        QCOMPARE(c->commandTrack().count(), 1);
        QCOMPARE(c->commandTrack().commands().first().time, 300u);
    }
    else
    {
        // disarmed and reported; B plays and its controls work, unrecorded
        QVERIFY(!r.recorder.isRecording());
        QCOMPARE(reported.count(), 1);
        QVERIFY2(r.recorder.lastError().contains(QStringLiteral("Unknown command target")),
                 qPrintable(r.recorder.lastError()));
        QVERIFY(!b->commandRecording());
        r.slider->requestUserValue(153);
        tickAndDeliver(&r.doc, 3);
        QCOMPARE(r.slider->value(), 153);
        QVERIFY(b->isRunning());
        QCOMPARE(b->commandTrack().count(), 0);
        // a later automatic change reports nothing more
        QVERIFY(r.recorder.setResolvedShow(c->id()));
        QCOMPARE(reported.count(), 1);
        // A's accepted input is all still there
        Scene *back = new Scene(&r.doc);
        QVERIFY(r.doc.addFunction(back, goneId));
        QVERIFY(r.recorder.checkpoint());
        QCOMPARE(a->commandTrack().count(), 2);
        QCOMPARE(a->commandTrack().commands().last().time, 4700u);
        QCOMPARE(int(a->commandTrack().commands().last().action), int(ShowCommandAction::SetSliderPosition));
        b->stop(FunctionParent::master());
    }
    tickAndDeliver(&r.doc, 3);
}

void ShowCommandRecorder_Test::manualShowSelection_lockedWhileArmed_data()
{
    QTest::addColumn<QString>("state");
    QTest::addColumn<bool>("accepted");

    QTest::newRow("armed, another show") << QStringLiteral("armed") << false;
    // the automatic handover already moved the take there
    QTest::newRow("armed, after automatic handover") << QStringLiteral("handed over") << true;
    // nothing bound yet: the first resolved Show binds
    QTest::newRow("armed without a show") << QStringLiteral("unbound") << true;
    QTest::newRow("not armed") << QStringLiteral("off") << true;
}

void ShowCommandRecorder_Test::manualShowSelection_lockedWhileArmed()
{
    QFETCH(QString, state);
    QFETCH(bool, accepted);

    RequestRig r;
    Show *b = new Show(&r.doc);
    QVERIFY(r.doc.addFunction(b));
    ShowManager manager(r.ui.view(), &r.doc);
    manager.m_detached = true;
    // the application's selection wiring
    QObject::connect(&manager, &ShowManager::currentShowIDChanged, &r.recorder,
                     [&r](int showID) { r.recorder.setResolvedShow(quint32(showID)); });
    if (state == QLatin1String("unbound"))
        QVERIFY(r.recorder.setResolvedShow(Function::invalidId()));
    else
        manager.setCurrentShowID(int(r.show->id()));
    QVERIFY(r.recorder.setRecording(state != QLatin1String("off")));
    if (state == QLatin1String("handed over"))
        QVERIFY(r.recorder.setResolvedShow(b->id()));
    const int before = manager.currentShowID();

    QSignalSpy selected(&manager, &ShowManager::currentShowIDChanged);
    manager.setCurrentShowID(int(b->id()));

    QCOMPARE(manager.currentShowID(), accepted ? int(b->id()) : before);
    QCOMPARE(selected.count(), accepted && before != int(b->id()) ? 1 : 0);
    QCOMPARE(r.recorder.isRecording(), state != QLatin1String("off"));
    if (state != QLatin1String("off"))
        QCOMPARE(r.recorder.boundShowId(), accepted ? b->id() : r.show->id());
}

void ShowCommandRecorder_Test::performFollow_handsOverOnlyWhileEngaged_data()
{
    QTest::addColumn<bool>("enabledFirst");

    QTest::newRow("deck change while engaged") << true;
    // the deck resolved B while Perform was off; enabling it changes only its state
    QTest::newRow("enabled with the deck already on B") << false;
}

void ShowCommandRecorder_Test::performFollow_handsOverOnlyWhileEngaged()
{
    QFETCH(bool, enabledFirst);

    RequestRig r;
    Show *b = new Show(&r.doc);
    QVERIFY(r.doc.addFunction(b));
    ShowManager manager(r.ui.view(), &r.doc);
    manager.m_detached = true;
    // the application's wiring, in its order
    QObject::connect(&manager, &ShowManager::currentShowIDChanged, &r.recorder,
                     [&r](int showID) { r.recorder.setResolvedShow(quint32(showID)); });
    VdjBridge bridge;
    bridge.setDoc(&r.doc);
    r.recorder.followPerform(bridge.performFsm());
    DjManager dj(r.ui.view(), &r.doc, &bridge, bridge.showFactory());
    QObject::connect(&dj, &DjManager::showLoadRequested, &manager, &ShowManager::setCurrentShowID);
    manager.setCurrentShowID(int(r.show->id()));
    QVERIFY(r.recorder.setRecording(true));
    QCOMPARE(r.recorder.boundShowId(), r.show->id());
    PerformFsm *fsm = bridge.performFsm();

    if (enabledFirst)
    {
        fsm->setPerformEnabled(true);
        fsm->setActiveShow(b->id());
    }
    else
    {
        // Perform off is not an automatic source: the take stays where it is
        fsm->setActiveShow(b->id());
        QCOMPARE(r.recorder.boundShowId(), r.show->id());
        QCOMPARE(manager.currentShowID(), int(r.show->id()));
        fsm->setPerformEnabled(true);
    }

    QVERIFY(r.recorder.isAuthoring());
    QCOMPARE(r.recorder.boundShowId(), b->id());
    QCOMPARE(manager.currentShowID(), int(b->id()));
    fsm->setPerformEnabled(false);
}

void ShowCommandRecorder_Test::appPerform_handsRecordingOver_data()
{
    QTest::addColumn<QString>("change");

    // the deck resolved B while Perform was off
    QTest::newRow("engaged on an unchanged deck") << QStringLiteral("engaged on an unchanged deck");
    QTest::newRow("deck change while engaged") << QStringLiteral("deck change while engaged");
    QTest::newRow("through a gap") << QStringLiteral("through a gap");
    QTest::newRow("previous cannot finalize") << QStringLiteral("previous cannot finalize");
}

void ShowCommandRecorder_Test::appPerform_handsRecordingOver()
{
    QFETCH(QString, change);

    // the production application and its startup wiring, offline and with
    // settings of its own
    QTemporaryDir settings(QDir::currentPath() + QStringLiteral("/app-settings-XXXXXX"));
    QVERIFY(settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settings.path());
    const auto nativeSettings = qScopeGuard([]() { QSettings::setDefaultFormat(QSettings::NativeFormat); });
    auto app = std::make_unique<App>();
    // the application leaves its undo instance behind; a failed row may
    // leave a Show running
    const auto appGone = qScopeGuard([&app]() {
        for (Function *function : app->doc()->functions())
        {
            if (function->isRunning())
                function->stop(FunctionParent::master());
        }
        tickAndDeliver(app->doc(), 3);
        app.reset();
        Tardis::s_instance = nullptr;
    });
    app->set3dSupported(false);
    app->startup();
    Doc *doc = app->doc();
    doc->masterTimer()->stop();
    QVERIFY2(app->status() == QQuickView::Ready, qPrintable(app->errors().value(0).toString()));
    const auto context = [&app](const char *name) {
        return app->rootContext()->contextProperty(QString::fromLatin1(name)).value<QObject *>();
    };
    auto *recorder = qobject_cast<ShowCommandRecorder *>(context("showCommandRecorder"));
    auto *manager = qobject_cast<ShowManager *>(context("showManager"));
    auto *bridge = qobject_cast<VdjBridge *>(context("vdjBridge"));
    QVERIFY(recorder && manager && bridge);
    PerformFsm *fsm = bridge->performFsm();

    const auto addShow = [doc](quint32 at) {
        Show *show = new Show(doc);
        doc->addFunction(show);
        show->setSyncSource(ShowRunner::External);
        show->setExternalElapsedTime(at);
        return show;
    };
    Show *a = addShow(4000);
    Show *b = addShow(0);
    Show *c = addShow(0);
    Scene *target = new Scene(doc);
    QVERIFY(doc->addFunction(target));
    Scene *gone = new Scene(doc);
    QVERIFY(doc->addFunction(gone));
    const quint32 goneId = gone->id();
    const auto start = [recorder](quint32 functionId) {
        ShowCommandInput input;
        input.origin = ShowCommandOrigin::Pointer;
        input.action = ShowCommandAction::Start;
        input.functionId = functionId;
        return recorder->submitUserInput(input);
    };

    manager->setCurrentShowID(int(a->id()));
    QVERIFY(recorder->setRecording(true));
    QCOMPARE(recorder->boundShowId(), a->id());
    const bool failing = change == QLatin1String("previous cannot finalize");
    if (failing)
    {
        // a published command whose target then disappears: A's later input
        // stays unpublished, and so A cannot close
        QVERIFY(start(goneId));
        QVERIFY(doc->deleteFunction(goneId));
        a->setExternalElapsedTime(4700);
        QVERIFY(!start(target->id()));
        QCOMPARE(a->commandTrack().count(), 1);
        ShowCommandTrack minute;
        QVERIFY(minute.setExtent(60000));
        QVERIFY(b->setCommandTrack(minute));
        b->start(doc->masterTimer(), FunctionParent::master());
        tickAndDeliver(doc, 2);
        QVERIFY(b->isRunning());
    }
    else
    {
        a->setExternalElapsedTime(4700);
        QVERIFY(start(target->id()));
    }

    if (change == QLatin1String("engaged on an unchanged deck"))
    {
        fsm->setActiveShow(b->id());
        QCOMPARE(recorder->boundShowId(), a->id());
        QCOMPARE(manager->currentShowID(), int(a->id()));
    }
    else
    {
        fsm->setActiveShow(a->id());
    }
    QSignalSpy reported(recorder, &ShowCommandRecorder::lastErrorChanged);
    fsm->setPerformEnabled(true);
    QCOMPARE(recorder->boundShowId(), change == QLatin1String("engaged on an unchanged deck") ? b->id() : a->id());
    if (change == QLatin1String("through a gap"))
    {
        fsm->setActiveShow(PerformFsm::InvalidShowId);
        QCOMPARE(recorder->phaseValue(), int(ShowRecordPhase::Armed));
        QVERIFY(recorder->isRecording());
        QVERIFY(!a->commandRecording());
        fsm->setActiveShow(c->id());
    }
    else if (change != QLatin1String("engaged on an unchanged deck"))
    {
        fsm->setActiveShow(b->id());
    }
    Show *next = change == QLatin1String("through a gap") ? c : b;

    // the displayed Show follows the deck either way
    QCOMPARE(manager->currentShowID(), int(next->id()));
    QVERIFY(!a->commandRecording());
    if (failing)
    {
        // disarmed and reported once; B plays unrecorded; A keeps all it accepted
        QVERIFY(!recorder->isRecording());
        QCOMPARE(reported.count(), 1);
        QVERIFY(!b->commandRecording());
        tickAndDeliver(doc, 3);
        QVERIFY(b->isRunning());
        QCOMPARE(b->commandTrack().count(), 0);
        Scene *back = new Scene(doc);
        QVERIFY(doc->addFunction(back, goneId));
        QVERIFY(recorder->checkpoint());
        QCOMPARE(a->commandTrack().count(), 2);
        QCOMPARE(a->commandTrack().commands().last().functionId, target->id());
        b->stop(FunctionParent::master());
    }
    else
    {
        // A closed at its last item; the take goes on in the next Show at the
        // clock the deck pushes
        QCOMPARE(reported.count(), 0);
        QCOMPARE(a->commandTrack().count(), 1);
        QCOMPARE(a->commandTrack().extent(), 4700u);
        QVERIFY(recorder->isAuthoring());
        QCOMPARE(recorder->boundShowId(), next->id());
        QVERIFY(next->commandRecording());
        next->setExternalElapsedTime(next == c ? 300 : 900);
        QVERIFY(start(target->id()));
        QCOMPARE(next->commandTrack().count(), 1);
        QCOMPARE(next->commandTrack().commands().first().time, next == c ? 300u : 900u);
    }
    fsm->setPerformEnabled(false);
    QVERIFY(recorder->setRecording(false));
    tickAndDeliver(doc, 3);
}

namespace
{
/** The production application and its startup wiring, offline and with
 *  settings of its own */
class AppRig
{
public:
    AppRig()
        : m_settings(QDir::currentPath() + QStringLiteral("/app-settings-XXXXXX"))
    {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, m_settings.path());
        app = std::make_unique<App>();
        app->set3dSupported(false);
        app->startup();
        doc = app->doc();
        doc->masterTimer()->stop();
        recorder = qobject_cast<ShowCommandRecorder *>(context("showCommandRecorder"));
        manager = qobject_cast<ShowManager *>(context("showManager"));
    }

    ~AppRig()
    {
        // a failed row may leave a Show running; the application leaves its
        // undo instance behind
        if (realTimer)
        {
            // its stop notifications are delivered while the Doc still exists
            doc->masterTimer()->stop();
            QCoreApplication::processEvents();
        }
        else
        {
            for (Function *function : app->doc()->functions())
            {
                if (function->isRunning())
                    function->stop(FunctionParent::master());
            }
            tickAndDeliver(app->doc(), 3);
        }
        app.reset();
        Tardis::s_instance = nullptr;
        QSettings::setDefaultFormat(QSettings::NativeFormat);
    }

    QObject *context(const char *name) const
    {
        return app->rootContext()->contextProperty(QString::fromLatin1(name)).value<QObject *>();
    }

    /** A production QML object, named in the main view's scope */
    QObject *qml(const QString &expression) const
    {
        QQmlExpression e(qmlContext(app->rootObject()), app->rootObject(), expression);
        return e.evaluate().value<QObject *>();
    }

    Show *addShow(quint32 at) const
    {
        Show *show = new Show(doc);
        doc->addFunction(show);
        show->setSyncSource(ShowRunner::External);
        show->setExternalElapsedTime(at);
        return show;
    }

    bool start(quint32 functionId) const
    {
        ShowCommandInput input;
        input.origin = ShowCommandOrigin::Pointer;
        input.action = ShowCommandAction::Start;
        input.functionId = functionId;
        return recorder->submitUserInput(input);
    }

    QTemporaryDir m_settings;
    std::unique_ptr<App> app;
    Doc *doc = nullptr;
    ShowCommandRecorder *recorder = nullptr;
    ShowManager *manager = nullptr;
    /** The application's own timer thread runs: replacing a workspace stops
     *  its functions through it, and restarts it */
    bool realTimer = false;
};

QByteArray fileBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/** A Show exactly as a workspace file stores it, loaded back in isolation */
std::unique_ptr<Show> showFromWorkspaceFile(Doc *doc, const QString &path, quint32 showId)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) == false)
        return nullptr;
    QXmlStreamReader reader(&file);
    while (reader.atEnd() == false)
    {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("Function") &&
            reader.attributes().value(QLatin1String("ID")).toUInt() == showId)
        {
            auto loaded = std::make_unique<Show>(doc);
            return loaded->loadXML(reader) ? std::move(loaded) : nullptr;
        }
    }
    return nullptr;
}

/** A recording at 55 s on a Show whose last retained command is at 40 s,
 *  with a clip ending at 45 s and a 60 s authored tail. Input accepted at
 *  30 s still awaits publication: its Show rejected it while an earlier
 *  command's target was missing. */
struct SavedTake
{
    Show *show = nullptr;
    Scene *target = nullptr;
    quint32 goneId = Function::invalidId();
};

SavedTake recordPendingTake(AppRig &rig, const QString &transport)
{
    SavedTake take;
    take.show = rig.addShow(12000);
    take.show->setName(QStringLiteral("Take A"));
    take.target = new Scene(rig.doc);
    rig.doc->addFunction(take.target);
    Scene *gone = new Scene(rig.doc);
    rig.doc->addFunction(gone);
    take.goneId = gone->id();

    ShowCommandTrack retained;
    retained.insert(ShowCommand::start(1, 40000, take.target->id()));
    retained.setExtent(60000);
    take.show->setCommandTrack(retained);
    auto *track = new Track(Function::invalidId());
    auto *clip = new ShowFunction(take.show->getLatestShowFunctionId());
    clip->setFunctionID(take.target->id());
    clip->setStartTime(1000);
    clip->setDuration(44000);
    track->addShowFunction(clip);
    take.show->addTrack(track);

    rig.manager->setCurrentShowID(int(take.show->id()));
    rig.recorder->setRecording(true);
    if (transport != QLatin1String("stopped"))
    {
        take.show->start(rig.doc->masterTimer(), FunctionParent::master());
        for (int i = 0; i < 20 && take.show->isRunning() == false; i++)
            tickAndDeliver(rig.doc);
        if (transport == QLatin1String("paused"))
        {
            take.show->setPause(true);
            tickAndDeliver(rig.doc, 2);
        }
    }
    take.show->setExternalElapsedTime(20000);
    rig.start(take.goneId);
    rig.doc->deleteFunction(take.goneId);
    take.show->setExternalElapsedTime(30000);
    rig.start(take.target->id());
    take.show->setExternalElapsedTime(55000);
    return take;
}
} // namespace

void ShowCommandRecorder_Test::appSave_checkpointsRecording_data()
{
    QTest::addColumn<QString>("transport");
    QTest::addColumn<QString>("failure");

    QTest::newRow("stopped") << QStringLiteral("stopped") << QString();
    QTest::newRow("playing") << QStringLiteral("playing") << QString();
    QTest::newRow("paused") << QStringLiteral("paused") << QString();
    // the missing target is still missing: the Show rejects the pending input
    QTest::newRow("playing, rejected checkpoint") << QStringLiteral("playing") << QStringLiteral("checkpoint");
    // an existing project in a directory nothing can be written to
    QTest::newRow("paused, unwritable destination") << QStringLiteral("paused") << QStringLiteral("destination");
}

void ShowCommandRecorder_Test::appSave_checkpointsRecording()
{
    QFETCH(QString, transport);
    QFETCH(QString, failure);

    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString original = files.filePath(QStringLiteral("original.qxw"));
    const QString saveAs = files.filePath(QStringLiteral("saved.qxw"));
    const QString readOnlyDir = files.filePath(QStringLiteral("locked"));
    const QString locked = readOnlyDir + QStringLiteral("/other.qxw");
    QVERIFY(QDir().mkpath(readOnlyDir));
    {
        QFile other(locked);
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.write("another project");
    }
    QVERIFY(QFile::setPermissions(readOnlyDir, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    const auto unlock = qScopeGuard([&readOnlyDir]() {
        QFile::setPermissions(readOnlyDir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    });

    AppRig rig;
    QVERIFY(rig.recorder && rig.manager);
    QVERIFY(rig.app->saveWorkspace(original));
    const QByteArray originalBytes = fileBytes(original);
    const QString workspacePath = rig.doc->workspacePath();
    const SavedTake take = recordPendingTake(rig, transport);
    QVERIFY(take.show->commandTrack().count() == 2);
    QVERIFY(rig.doc->isModified());
    if (failure != QLatin1String("checkpoint"))
    {
        Scene *back = new Scene(rig.doc);
        QVERIFY(rig.doc->addFunction(back, take.goneId));
    }
    const bool running = take.show->isRunning();
    const bool paused = take.show->isPaused();
    QCOMPARE(running, transport != QLatin1String("stopped"));
    QCOMPARE(paused, transport == QLatin1String("paused"));
    QVERIFY(rig.recorder->isAuthoring());
    QCOMPARE(rig.recorder->position(), 55000);

    const QString destination = failure.isEmpty() ? saveAs : failure == QLatin1String("checkpoint") ? original : locked;
    const bool saved = rig.app->saveWorkspace(destination);

    // REC and playback go on either way
    QVERIFY(rig.recorder->isAuthoring());
    QCOMPARE(rig.recorder->boundShowId(), take.show->id());
    QVERIFY(take.show->commandRecording());
    QCOMPARE(take.show->isRunning(), running);
    QCOMPARE(take.show->isPaused(), paused);
    QCOMPARE(rig.recorder->position(), 55000);

    if (failure.isEmpty() == false)
    {
        QVERIFY(!saved);
        QVERIFY(rig.doc->isModified());
        QCOMPARE(rig.app->fileName(), original);
        QCOMPARE(rig.doc->workspacePath(), workspacePath);
        QCOMPARE(fileBytes(original), originalBytes);
        QCOMPARE(fileBytes(locked), QByteArray("another project"));
        QCOMPARE(QDir(readOnlyDir).entryList(QDir::Files), QStringList{QStringLiteral("other.qxw")});
        if (failure == QLatin1String("checkpoint"))
        {
            QVERIFY(!rig.recorder->lastError().isEmpty());
            QCOMPARE(take.show->commandTrack().count(), 2);
            Scene *back = new Scene(rig.doc);
            QVERIFY(rig.doc->addFunction(back, take.goneId));
        }
        // nothing accepted was lost: a later save takes it
        QVERIFY(rig.app->saveWorkspace(saveAs));
    }
    else
    {
        QVERIFY(saved);
    }

    // everything accepted through the boundary, ending at the content
    QVERIFY(!rig.doc->isModified());
    QCOMPARE(rig.app->fileName(), saveAs);
    std::unique_ptr<Show> reopened = showFromWorkspaceFile(rig.doc, saveAs, take.show->id());
    QVERIFY(reopened);
    const ShowCommandTrack stored = reopened->commandTrack();
    QCOMPARE(stored.count(), 3);
    QCOMPARE(stored.commands(), take.show->commandTrack().commands());
    QCOMPARE(stored.commands().at(0).functionId, take.goneId);
    QCOMPARE(stored.commands().at(1).time, 30000u);
    QCOMPARE(stored.commands().at(1).functionId, take.target->id());
    QCOMPARE(stored.lastCommandTime(), 40000u);
    QCOMPARE(stored.extent(), 40000u);
    QCOMPARE(reopened->totalDuration(), 45000u);
    QVERIFY(!reopened->commandRecording());

    // later input is unsaved again and does not reach the saved file
    const QByteArray savedBytes = fileBytes(saveAs);
    take.show->setExternalElapsedTime(56000);
    QVERIFY(rig.start(take.target->id()));
    QVERIFY(rig.doc->isModified());
    QCOMPARE(take.show->commandTrack().count(), 4);
    QCOMPARE(fileBytes(saveAs), savedBytes);
    QVERIFY(rig.recorder->isAuthoring());
    QCOMPARE(take.show->isRunning(), running);
    QVERIFY(rig.recorder->setRecording(false));
}

namespace
{
/** Click a standard button of a production dialog, as the user does */
bool clickDialogButton(QQuickView *window, QObject *dialog, int button)
{
    QQmlExpression find(qmlContext(dialog), dialog, QStringLiteral("standardButton(%1)").arg(button));
    auto *item = qobject_cast<QQuickItem *>(find.evaluate().value<QObject *>());
    if (item == nullptr)
        return false;
    // the popup takes input once it has opened and its button row shows the button
    QQuickItem *row = item->parentItem();
    while (row != nullptr && row->inherits("QQuickFlickable") == false)
        row = row->parentItem();
    if (row == nullptr || !QTest::qWaitFor([&]() {
            const QPointF center = item->mapToItem(row, QPointF(item->width() / 2, item->height() / 2));
            return dialog->property("opened").toBool() && item->isVisible() && item->width() > 0 &&
                   row->contains(center);
        }))
        return false;
    QSignalSpy clicked(item, SIGNAL(clicked()));
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
    return clicked.count() == 1;
}
} // namespace

void ShowCommandRecorder_Test::appReplacement_confirmsRecording_data()
{
    QTest::addColumn<QString>("route");
    QTest::addColumn<QString>("setup");
    QTest::addColumn<bool>("hasFile");
    QTest::addColumn<QString>("choice");

    // a saved, clean workspace with REC armed still asks, and Cancel keeps it all
    QTest::newRow("new, clean, cancel") << QStringLiteral("new") << QStringLiteral("clean") << true
                                        << QStringLiteral("cancel");
    QTest::newRow("open, clean, cancel") << QStringLiteral("open") << QStringLiteral("clean") << true
                                         << QStringLiteral("cancel");
    QTest::newRow("quit, clean, cancel") << QStringLiteral("quit") << QStringLiteral("clean") << true
                                         << QStringLiteral("cancel");
    QTest::newRow("close, clean, cancel") << QStringLiteral("close") << QStringLiteral("clean") << true
                                          << QStringLiteral("cancel");
    QTest::newRow("new, pending, save") << QStringLiteral("new") << QStringLiteral("pending") << true
                                        << QStringLiteral("save");
    // Discard does not need the recording to validate
    QTest::newRow("file open, rejected, discard") << QStringLiteral("file open") << QStringLiteral("rejected") << true
                                                  << QStringLiteral("discard");
    QTest::newRow("new, rejected, save fails") << QStringLiteral("new") << QStringLiteral("rejected") << true
                                               << QStringLiteral("save");
    QTest::newRow("new, pending, save as canceled") << QStringLiteral("new") << QStringLiteral("pending") << false
                                                    << QStringLiteral("save as canceled");
    QTest::newRow("open, pending, save as fails") << QStringLiteral("open") << QStringLiteral("pending") << false
                                                  << QStringLiteral("save as fails");
    QTest::newRow("quit, pending, save") << QStringLiteral("quit") << QStringLiteral("pending") << true
                                         << QStringLiteral("save");
    // Save was chosen: what REC accepts while the file is chosen is saved too
    QTest::newRow("open, pending, save, input while choosing") << QStringLiteral("open") << QStringLiteral("pending")
                                                               << true << QStringLiteral("save, then input");
}

void ShowCommandRecorder_Test::appReplacement_confirmsRecording()
{
    QFETCH(QString, route);
    QFETCH(QString, setup);
    QFETCH(bool, hasFile);
    QFETCH(QString, choice);

    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString original = files.filePath(QStringLiteral("original.qxw"));
    const QString other = files.filePath(QStringLiteral("other.qxw"));
    const QString saveAs = files.filePath(QStringLiteral("saved.qxw"));
    const QString readOnlyDir = files.filePath(QStringLiteral("locked"));
    QVERIFY(QDir().mkpath(readOnlyDir));
    QVERIFY(QFile::setPermissions(readOnlyDir, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    const auto unlock = qScopeGuard([&readOnlyDir]() {
        QFile::setPermissions(readOnlyDir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    });

    AppRig rig;
    QVERIFY(rig.recorder && rig.manager);
    // a window the user can click in
    rig.app->showNormal();
    rig.app->resize(1200, 800);
    QVERIFY(QTest::qWaitForWindowExposed(rig.app.get()));
    QTRY_VERIFY(rig.app->rootObject()->width() > 1000);
    QObject *menu = rig.qml(QStringLiteral("actionsMenu"));
    QVERIFY(menu);
    QObject *popup = menu->findChild<QObject *>(QStringLiteral("saveFirstPopup"));
    QVERIFY(popup);
    QObject *saveError = menu->findChild<QObject *>(QStringLiteral("saveErrorPopup"));
    // the menu's own ids live in the context its children were created in
    const auto menuQml = [menu, popup](const QString &expression) {
        QQmlExpression e(qmlContext(popup), menu, expression);
        return e.evaluate();
    };
    // the file dialog the menu opens on this platform
    QObject *fileDialog = nullptr;
    const bool customDialog = menuQml(QStringLiteral("Qt.platform.os === 'linux'")).toBool();
    for (QObject *child : menu->findChildren<QObject *>())
    {
        const QString type = QString::fromLatin1(child->metaObject()->className());
        if (customDialog ? type.startsWith(QLatin1String("PopupFolderBrowser")) : child->inherits("QQuickFileDialog"))
            fileDialog = child;
    }
    QVERIFY(fileDialog);
    // The platform dialog does not run offscreen: the file is chosen or the
    // choice canceled through the dialog's own signals, into the menu's handlers
    const auto acceptFile = [&](const QString &path) {
        if (customDialog == false)
            fileDialog->setProperty("selectedFile", QUrl::fromLocalFile(path));
        else
        {
            fileDialog->setProperty("currentFolder", QFileInfo(path).absolutePath());
            fileDialog->setProperty("selectedFile", QFileInfo(path).fileName());
        }
        return QMetaObject::invokeMethod(fileDialog, "accepted");
    };
    const auto pendingAction = [popup]() { return popup->property("action").toString(); };

    // another project to open, then the current one
    QVERIFY(rig.app->saveWorkspace(other));
    if (hasFile)
        QVERIFY(rig.app->saveWorkspace(original));
    else
        rig.app->setFileName(QString());
    SavedTake take = recordPendingTake(rig, QStringLiteral("playing"));
    if (setup != QLatin1String("rejected"))
        QVERIFY(rig.doc->addFunction(new Scene(rig.doc), take.goneId));
    if (setup == QLatin1String("clean"))
    {
        QVERIFY(rig.app->saveWorkspace(original));
        QVERIFY(!rig.doc->isModified());
    }
    QVERIFY(rig.recorder->isAuthoring());
    const quint32 showId = take.show->id();
    const QString fileBefore = rig.app->fileName();
    const bool dirtyBefore = rig.doc->isModified();
    int functionsBefore = rig.doc->functions().count();
    const QVector<ShowCommand> commandsBefore = take.show->commandTrack().commands();
    // from here the application's own timer runs: replacing a workspace stops
    // its functions through it. Playback is read through its signals.
    QVERIFY(rig.manager->isPlaying());
    rig.doc->masterTimer()->start();
    rig.realTimer = true;

    if (route == QLatin1String("new"))
        QVERIFY(QMetaObject::invokeMethod(menu, "handleNewAction"));
    else if (route == QLatin1String("open"))
        QVERIFY(QMetaObject::invokeMethod(menu, "handleOpenAction"));
    else if (route == QLatin1String("file open"))
    {
        QFileOpenEvent event(QUrl::fromLocalFile(other));
        QCoreApplication::sendEvent(qApp, &event);
    }
    else if (route == QLatin1String("quit"))
    {
        QEvent event(QEvent::Quit);
        QCoreApplication::sendEvent(qApp, &event);
    }
    else
    {
        QCloseEvent event;
        QCoreApplication::sendEvent(rig.app.get(), &event);
        QVERIFY(!event.isAccepted());
    }
    QCoreApplication::processEvents();

    // the existing dialog, telling what is being recorded
    QVERIFY(popup->property("visible").toBool());
    QVERIFY(popup->property("message").toString().contains(QLatin1String("Take A")));
    QVERIFY(rig.recorder->isAuthoring());

    const int button = choice == QLatin1String("cancel")    ? int(QMessageBox::Cancel)
                       : choice == QLatin1String("discard") ? int(QMessageBox::No)
                                                            : int(QMessageBox::Yes);
    QVERIFY(clickDialogButton(rig.app.get(), popup, button));
    QCoreApplication::processEvents();
    QTRY_VERIFY(!popup->property("visible").toBool());

    const auto sessionKept = [&]() {
        QCOMPARE(rig.doc->function(showId), take.show);
        QCOMPARE(rig.doc->functions().count(), functionsBefore);
        QVERIFY(rig.recorder->isAuthoring());
        QCOMPARE(rig.recorder->boundShowId(), showId);
        QVERIFY(take.show->commandRecording());
        QVERIFY(rig.manager->isPlaying());
        QVERIFY(rig.doc->masterTimer()->runningFunctions() > 0);
        QCOMPARE(rig.recorder->position(), 55000);
    };

    if (choice == QLatin1String("cancel"))
    {
        sessionKept();
        QCOMPARE(rig.doc->isModified(), dirtyBefore);
        QCOMPARE(rig.app->fileName(), fileBefore);
        QCOMPARE(take.show->commandTrack().commands(), commandsBefore);
        return;
    }

    if (choice == QLatin1String("save as canceled") || choice == QLatin1String("save as fails"))
    {
        // Save As on a never saved project: nothing proceeds until it saves
        QCOMPARE(menuQml(QStringLiteral("dialogOpMode")).toInt(), int(App::SaveMode));
        QCOMPARE(pendingAction(), route == QLatin1String("new") ? QStringLiteral("#NEW") : QStringLiteral("#OPEN"));
        sessionKept();
        if (choice == QLatin1String("save as canceled"))
            QVERIFY(QMetaObject::invokeMethod(fileDialog, "rejected"));
        else
            QVERIFY(acceptFile(readOnlyDir + QStringLiteral("/project.qxw")));
        QCoreApplication::processEvents();
        QVERIFY(pendingAction().isEmpty());
        QCOMPARE(saveError && saveError->property("visible").toBool(), choice == QLatin1String("save as fails"));
        sessionKept();
        QVERIFY(rig.doc->isModified());
        QVERIFY(rig.app->fileName().isEmpty());
        // a later, unrelated Save As does not resume the abandoned action
        QVERIFY(QMetaObject::invokeMethod(menu, "openDialog", Q_ARG(QVariant, int(App::SaveAsMode))));
        QVERIFY(acceptFile(saveAs));
        QCoreApplication::processEvents();
        QCOMPARE(rig.app->fileName(), saveAs);
        QVERIFY(!rig.doc->isModified());
        QCOMPARE(menuQml(QStringLiteral("dialogOpMode")).toInt(), int(App::SaveAsMode));
        QVERIFY(!popup->property("visible").toBool());
        sessionKept();
        return;
    }

    if (setup == QLatin1String("rejected") && choice == QLatin1String("save"))
    {
        // the checkpoint fails: nothing is replaced or lost, and it says why
        sessionKept();
        QVERIFY(saveError && saveError->property("visible").toBool());
        QVERIFY(rig.doc->isModified());
        QVERIFY(!rig.recorder->lastError().isEmpty());
        QCOMPARE(rig.app->fileName(), original);
        QCOMPARE(take.show->commandTrack().commands(), commandsBefore);
        // once it can be saved, a plain Save does not resume the New
        QVERIFY(rig.doc->addFunction(new Scene(rig.doc), take.goneId));
        functionsBefore++;
        QVERIFY(QMetaObject::invokeMethod(menu, "handleSaveAction"));
        QCoreApplication::processEvents();
        QVERIFY(!rig.doc->isModified());
        QCOMPARE(take.show->commandTrack().count(), 3);
        sessionKept();
        return;
    }

    const bool inputWhileChoosing = choice == QLatin1String("save, then input");
    if (inputWhileChoosing)
    {
        QCOMPARE(menuQml(QStringLiteral("dialogOpMode")).toInt(), int(App::OpenMode));
        sessionKept();
        take.show->setExternalElapsedTime(56000);
        QVERIFY(rig.start(take.target->id()));
        QVERIFY(acceptFile(other));
        QCoreApplication::processEvents();
    }

    if (choice == QLatin1String("save") || inputWhileChoosing)
    {
        // the take was saved whole, ending at its content, before anything proceeded
        std::unique_ptr<Show> saved = showFromWorkspaceFile(rig.doc, original, showId);
        QVERIFY(saved);
        QCOMPARE(saved->commandTrack().count(), inputWhileChoosing ? 4 : 3);
        QCOMPARE(saved->commandTrack().extent(), inputWhileChoosing ? 56000u : 40000u);
    }
    else
    {
        // discarded: the saved project is what it was
        std::unique_ptr<Show> saved = showFromWorkspaceFile(rig.doc, original, showId);
        QVERIFY(saved == nullptr || saved->commandTrack().count() < 2);
    }

    if (route == QLatin1String("quit"))
    {
        // the application exits through its existing lifecycle
        QCOMPARE(rig.doc->masterTimer()->runningFunctions(), 0);
        return;
    }

    // replaced: nothing of the old take is armed or reaches the new workspace
    QVERIFY(!rig.recorder->isRecording());
    QCOMPARE(rig.recorder->boundShowId(), quint32(Function::invalidId()));
    QCOMPARE(rig.app->fileName(), route == QLatin1String("file open") || inputWhileChoosing ? other : QString());
    QVERIFY(rig.doc->function(showId) == nullptr);
    // arming again waits for a Show of the new workspace, even one reusing the old ID
    Show *next = rig.addShow(3000);
    QVERIFY(rig.recorder->setRecording(true));
    QCOMPARE(rig.recorder->phaseValue(), int(ShowRecordPhase::Armed));
    QVERIFY(!next->commandRecording());
    QVERIFY(rig.recorder->setRecording(false));
    rig.manager->setCurrentShowID(int(next->id()));
    QTest::qWait(100);
    QVERIFY(!rig.recorder->isRecording());
    QVERIFY(!next->commandRecording());
    Scene *scene = new Scene(rig.doc);
    QVERIFY(rig.doc->addFunction(scene));
    QVERIFY(!rig.start(scene->id()));
    QCOMPARE(next->commandTrack().count(), 0);
}

namespace
{
/** The shown production item of that name, in whichever view is loaded */
QQuickItem *shownItem(QQuickItem *root, const QString &name)
{
    for (QQuickItem *item : root->findChildren<QQuickItem *>(name))
    {
        if (item->isVisible())
            return item;
    }
    return nullptr;
}
} // namespace

void ShowCommandRecorder_Test::appRecordControls_shareOneRecorder_data()
{
    QTest::addColumn<QString>("context");
    QTest::addColumn<QString>("resource");
    QTest::addColumn<QString>("other");
    QTest::addColumn<QString>("otherResource");
    QTest::addColumn<bool>("keyboard");

    const QString show = QStringLiteral("SHOWMGR"), dj = QStringLiteral("DJMGR"), vc = QStringLiteral("VC");
    const QString showRes = QStringLiteral("qrc:/ShowManager.qml"), djRes = QStringLiteral("qrc:/DjManager.qml"),
                  vcRes = QStringLiteral("qrc:/VirtualConsole.qml");
    // the Show editor's Space plays the Show; a focused REC keeps it
    QTest::newRow("show editor, keyboard") << show << showRes << vc << vcRes << true;
    QTest::newRow("show editor, mouse") << show << showRes << dj << djRes << false;
    QTest::newRow("dj, mouse") << dj << djRes << show << showRes << false;
    QTest::newRow("dj, keyboard") << dj << djRes << vc << vcRes << true;
    QTest::newRow("virtual console, mouse") << vc << vcRes << show << showRes << false;
    QTest::newRow("virtual console, keyboard") << vc << vcRes << dj << djRes << true;
}

void ShowCommandRecorder_Test::appRecordControls_shareOneRecorder()
{
    QFETCH(QString, context);
    QFETCH(QString, resource);
    QFETCH(QString, other);
    QFETCH(QString, otherResource);
    QFETCH(bool, keyboard);

    AppRig rig;
    QVERIFY(rig.recorder && rig.manager);
    rig.app->showNormal();
    rig.app->resize(1600, 900);
    QVERIFY(QTest::qWaitForWindowExposed(rig.app.get()));
    QTRY_VERIFY(rig.app->rootObject()->width() > 1000);
    QQuickItem *root = rig.app->rootObject();
    MasterTimer *timer = rig.doc->masterTimer();

    // A owes its GUI a crossed batch at 100 ms: its control no longer exists
    Show *a = new Show(rig.doc);
    a->setName(QStringLiteral("Take A"));
    QVERIFY(rig.doc->addFunction(a));
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, QUuid::createUuid(), true)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(a->setCommandTrack(track));
    Show *b = new Show(rig.doc);
    b->setName(QStringLiteral("Take B"));
    QVERIFY(rig.doc->addFunction(b));
    Scene *scene = new Scene(rig.doc);
    QVERIFY(rig.doc->addFunction(scene));

    const auto view = [&](const QString &name, const QString &qml) {
        QVERIFY(QMetaObject::invokeMethod(root, "switchToContext", Q_ARG(QVariant, name), Q_ARG(QVariant, qml)));
    };
    const auto rec = [root]() { return shownItem(root, QStringLiteral("recordSwitch")); };
    const auto status = [&rec]() {
        QQuickItem *control = rec() ? rec()->parentItem() : nullptr;
        QQuickItem *text = control ? control->findChild<QQuickItem *>(QStringLiteral("recordStatus")) : nullptr;
        return text && text->isVisible() ? text->property("text").toString() : QString();
    };
    const auto toggle = [&]() {
        QQuickItem *item = rec();
        QVERIFY(item);
        if (keyboard)
        {
            item->forceActiveFocus();
            QTRY_VERIFY(item->hasActiveFocus());
            QTest::keyClick(rig.app.get(), Qt::Key_Space);
        }
        else
        {
            QTest::mouseClick(rig.app.get(), Qt::LeftButton, Qt::NoModifier,
                              item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
        }
        QCoreApplication::processEvents();
    };

    view(context, resource);
    QTRY_VERIFY(rec() != nullptr);
    QVERIFY(!rec()->property("checked").toBool());

    // armed with no Show: capture only, nothing plays
    toggle();
    QVERIFY(rig.recorder->isRecording());
    QCOMPARE(rig.recorder->phaseValue(), int(ShowRecordPhase::Armed));
    QVERIFY(rec()->property("checked").toBool());
    QVERIFY(status().contains(QLatin1String("waiting for a show")));
    QVERIFY(!rig.manager->isPlaying());
    QCOMPARE(timer->runningFunctions(), 0);

    // the first manual selection binds, then the take keeps its Show
    rig.manager->setCurrentShowID(int(a->id()));
    QVERIFY(rig.recorder->isAuthoring());
    QCOMPARE(rig.recorder->boundShowId(), a->id());
    rig.manager->setCurrentShowID(int(b->id()));
    QCOMPARE(rig.manager->currentShowID(), int(a->id()));
    QCOMPARE(status(), QStringLiteral("REC: Recording - Take A - Stopped"));

    // another view shows the same take and state
    view(other, otherResource);
    QTRY_VERIFY(rec() != nullptr);
    QVERIFY(rec()->property("checked").toBool());
    QCOMPARE(status(), QStringLiteral("REC: Recording - Take A - Stopped"));

    // transport is the Show's own: playing, pausing while crossed work is owed, paused
    rig.manager->playShow();
    for (int i = 0; i < 11; i++)
        timer->timerTick();
    QVERIFY(rig.manager->isPlaying());
    rig.manager->playShow();
    QVERIFY(rig.manager->pausing());
    QCOMPARE(status(), QStringLiteral("REC: Recording - Take A - Pausing"));
    for (int i = 0; i < 20 && rig.manager->pausing(); i++)
        tickAndDeliver(rig.doc);
    QCOMPARE(status(), QStringLiteral("REC: Recording - Take A - Paused"));
    const quint32 pausedAt = a->commandPosition();

    // back in the first view, paused capture still records at the frozen cursor
    view(context, resource);
    QTRY_VERIFY(rec() != nullptr);
    QCOMPARE(status(), QStringLiteral("REC: Recording - Take A - Paused"));
    QVERIFY(rig.start(scene->id()));
    QCOMPARE(a->commandTrack().commands().last().time, pausedAt);

    // REC off leaves playback alone and closes the take at its content
    toggle();
    QVERIFY(!rig.recorder->isRecording());
    QVERIFY(!rec()->property("checked").toBool());
    QVERIFY(status().isEmpty());
    QVERIFY(rig.manager->isPlaying());
    QVERIFY(rig.manager->isPaused());
    QCOMPARE(a->commandPosition(), pausedAt);
    QCOMPARE(a->commandTrack().extent(), pausedAt);

    // selection is free again once REC is off
    rig.manager->setCurrentShowID(int(b->id()));
    QCOMPARE(rig.manager->currentShowID(), int(b->id()));
    a->stop(FunctionParent::master());
    tickAndDeliver(rig.doc, 3);
}

void ShowCommandRecorder_Test::appRecordControls_followTargetRename_data()
{
    QTest::addColumn<QString>("context");
    QTest::addColumn<QString>("resource");
    QTest::addColumn<bool>("previousTarget");

    const QStringList contexts{QStringLiteral("SHOWMGR"), QStringLiteral("DJMGR"), QStringLiteral("VC")};
    const QStringList resources{QStringLiteral("qrc:/ShowManager.qml"), QStringLiteral("qrc:/DjManager.qml"),
                                QStringLiteral("qrc:/VirtualConsole.qml")};
    for (int i = 0; i < contexts.count(); i++)
    {
        QTest::newRow(qPrintable(contexts.at(i) + QStringLiteral(", bound Show renamed")))
            << contexts.at(i) << resources.at(i) << false;
        // the take moved on: renaming its old Show says nothing about the new one
        QTest::newRow(qPrintable(contexts.at(i) + QStringLiteral(", previous target renamed")))
            << contexts.at(i) << resources.at(i) << true;
    }
}

void ShowCommandRecorder_Test::appRecordControls_followTargetRename()
{
    QFETCH(QString, context);
    QFETCH(QString, resource);
    QFETCH(bool, previousTarget);

    AppRig rig;
    QVERIFY(rig.recorder && rig.manager);
    rig.app->showNormal();
    rig.app->resize(1600, 900);
    QVERIFY(QTest::qWaitForWindowExposed(rig.app.get()));
    QQuickItem *root = rig.app->rootObject();
    Show *a = rig.addShow(3000);
    a->setName(QStringLiteral("Before rename"));
    Show *b = rig.addShow(6000);
    b->setName(QStringLiteral("Take B"));
    QVERIFY(QMetaObject::invokeMethod(root, "switchToContext", Q_ARG(QVariant, context), Q_ARG(QVariant, resource)));
    QTRY_VERIFY(shownItem(root, QStringLiteral("recordSwitch")) != nullptr);
    const auto status = [root]() {
        QQuickItem *text = shownItem(root, QStringLiteral("recordStatus"));
        return text ? text->property("text").toString() : QString();
    };
    QObject *menu = rig.qml(QStringLiteral("actionsMenu"));
    QObject *notice = menu ? menu->findChild<QObject *>(QStringLiteral("saveFirstPopup")) : nullptr;
    QVERIFY(notice);

    rig.manager->setCurrentShowID(int(a->id()));
    QVERIFY(rig.recorder->setRecording(true));
    QCOMPARE(status(), QStringLiteral("REC: Recording - Before rename - Stopped"));
    if (previousTarget)
    {
        QVERIFY(rig.recorder->setRecording(false));
        rig.manager->setCurrentShowID(int(b->id()));
        QVERIFY(rig.recorder->setRecording(true));
        QCOMPARE(rig.recorder->boundShowId(), b->id());
    }
    const QString target = previousTarget ? QStringLiteral("Take B") : QStringLiteral("After rename");
    QSignalSpy states(rig.recorder, &ShowCommandRecorder::stateChanged);

    // the Show editor's name field, or any other rename of the old Show
    if (previousTarget)
        a->setName(QStringLiteral("After rename"));
    else
        rig.manager->setShowName(QStringLiteral("After rename"));
    QCoreApplication::processEvents();

    QCOMPARE(rig.recorder->targetName(), target);
    QCOMPARE(status(), QStringLiteral("REC: Recording - %1 - Stopped").arg(target));
    QVERIFY(notice->property("message").toString().contains(target));
    // a name is not a recording transition
    QCOMPARE(states.count(), 0);
    QVERIFY(rig.recorder->isAuthoring());
    QVERIFY(!rig.manager->isPlaying());
    QVERIFY(rig.recorder->setRecording(false));
}

void ShowCommandRecorder_Test::appPerformGap_firstSelectionBinds_data()
{
    QTest::addColumn<QString>("selection");

    // after Perform resolved nothing, the Show Manager still displays A
    QTest::newRow("still displayed A") << QStringLiteral("A");
    QTest::newRow("different B") << QStringLiteral("B");
    // a bound take keeps its Show; selecting it again changes nothing
    QTest::newRow("bound, select B") << QStringLiteral("bound B");
    QTest::newRow("bound, select A again") << QStringLiteral("bound A");
    // not recording: selecting the displayed Show stays a no-op
    QTest::newRow("REC off, A again") << QStringLiteral("off A");
}

void ShowCommandRecorder_Test::appPerformGap_firstSelectionBinds()
{
    QFETCH(QString, selection);

    AppRig rig;
    QVERIFY(rig.recorder && rig.manager);
    auto *bridge = qobject_cast<VdjBridge *>(rig.context("vdjBridge"));
    QVERIFY(bridge);
    PerformFsm *fsm = bridge->performFsm();
    Show *a = rig.addShow(3000);
    Show *b = rig.addShow(6000);

    rig.manager->setCurrentShowID(int(a->id()));
    if (selection.startsWith(QLatin1String("off")) == false)
    {
        QVERIFY(rig.recorder->setRecording(true));
        fsm->setActiveShow(a->id());
        fsm->setPerformEnabled(true);
        QCOMPARE(rig.recorder->boundShowId(), a->id());
        if (selection.startsWith(QLatin1String("bound")) == false)
        {
            // an automatic gap: the take on A closes, REC waits armed
            fsm->setActiveShow(PerformFsm::InvalidShowId);
            QCOMPARE(rig.recorder->phaseValue(), int(ShowRecordPhase::Armed));
        }
        fsm->setPerformEnabled(false);
        QVERIFY(!rig.manager->readOnly());
    }
    QCOMPARE(rig.manager->currentShowID(), int(a->id()));
    const quint32 boundBefore = rig.recorder->boundShowId();
    QSignalSpy states(rig.recorder, &ShowCommandRecorder::stateChanged);
    QSignalSpy selected(rig.manager, &ShowManager::currentShowIDChanged);

    Show *chosen = selection.endsWith(QLatin1String("B")) ? b : a;
    // the public setter the function list and Show editor select through
    rig.manager->setCurrentShowID(int(chosen->id()));

    if (selection == QLatin1String("A") || selection == QLatin1String("B"))
    {
        // the first manual selection binds, then locks
        QCOMPARE(rig.manager->currentShowID(), int(chosen->id()));
        QVERIFY(rig.recorder->isAuthoring());
        QCOMPARE(rig.recorder->boundShowId(), chosen->id());
        QVERIFY(chosen->commandRecording());
        rig.manager->setCurrentShowID(int((chosen == a ? b : a)->id()));
        QCOMPARE(rig.manager->currentShowID(), int(chosen->id()));
        const int transitions = states.count();
        rig.manager->setCurrentShowID(int(chosen->id()));
        QCOMPARE(states.count(), transitions);
    }
    else
    {
        // nothing moves: no selection, target or transport change
        QCOMPARE(rig.manager->currentShowID(), int(a->id()));
        QCOMPARE(rig.recorder->boundShowId(), boundBefore);
        QCOMPARE(states.count(), 0);
        QCOMPARE(selected.count(), 0);
        QCOMPARE(rig.recorder->isRecording(), selection.startsWith(QLatin1String("bound")));
    }
    QCOMPARE(rig.doc->masterTimer()->runningFunctions(), 0);
    rig.recorder->setRecording(false);
}

void ShowCommandRecorder_Test::appMcpReplacement_needsDiscardWhileRecording_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QString>("rec");
    QTest::addColumn<bool>("discard");

    for (const QString &tool : {QStringLiteral("new_workspace"), QStringLiteral("load_workspace")})
    {
        for (const QString &rec : {QStringLiteral("armed"), QStringLiteral("bound")})
        {
            for (bool discard : {false, true})
                QTest::newRow(qPrintable(QStringLiteral("%1, %2, %3").arg(tool, rec,
                              discard ? QStringLiteral("discardUnsaved") : QStringLiteral("no flag"))))
                    << tool << rec << discard;
        }
        // REC off and saved: nothing to confirm
        QTest::newRow(qPrintable(tool + QStringLiteral(", off, no flag"))) << tool << QStringLiteral("off") << false;
    }
}

void ShowCommandRecorder_Test::appMcpReplacement_needsDiscardWhileRecording()
{
    QFETCH(QString, tool);
    QFETCH(QString, rec);
    QFETCH(bool, discard);

    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString current = files.filePath(QStringLiteral("current.qxw"));
    const QString other = files.filePath(QStringLiteral("other.qxw"));
    AppRig rig;
    QVERIFY(rig.recorder && rig.manager);
    // the production bridge and tools the MCP server registers, driven offline
    WorkspaceBridgeV5 bridge(rig.app.get());
    fastmcpp::tools::ToolManager tools;
    registerWorkspaceTools(tools, rig.doc, &bridge);
    const auto invoke = [&](const Json &args) {
        const Json result = tools.invoke(tool.toStdString(), args);
        return result.is_string() ? Json::parse(result.get<std::string>()) : result;
    };

    QVERIFY(rig.app->saveWorkspace(other));
    Show *take = rig.addShow(12000);
    take->setName(QStringLiteral("Take A"));
    Scene *scene = new Scene(rig.doc);
    QVERIFY(rig.doc->addFunction(scene));
    ShowCommandTrack retained;
    QVERIFY(retained.insert(ShowCommand::start(1, 4000, scene->id())));
    QVERIFY(take->setCommandTrack(retained));
    if (rec == QLatin1String("bound"))
        rig.manager->setCurrentShowID(int(take->id()));
    if (rec != QLatin1String("off"))
        QVERIFY(rig.recorder->setRecording(true));
    // a saved, clean project: only the recording is at stake
    QVERIFY(rig.app->saveWorkspace(current));
    QVERIFY(!rig.doc->isModified());
    QCOMPARE(rig.recorder->isRecording(), rec != QLatin1String("off"));
    QCOMPARE(rig.recorder->phaseValue(), rec == QLatin1String("bound")   ? int(ShowRecordPhase::Bound)
                                         : rec == QLatin1String("armed") ? int(ShowRecordPhase::Armed)
                                                                         : int(ShowRecordPhase::Off));
    const quint32 takeId = take->id();

    Json args = Json::object();
    if (tool == QLatin1String("load_workspace"))
        args["path"] = other.toStdString();
    if (discard)
        args["discardUnsaved"] = true;
    const Json result = invoke(args);

    const bool proceeds = discard || rec == QLatin1String("off");
    if (proceeds == false)
    {
        // refused, saying why; project and REC as they were
        QVERIFY2(result.contains("error"), result.dump().c_str());
        QCOMPARE(result.value("recording", false), true);
        QCOMPARE(rig.doc->function(takeId), take);
        QCOMPARE(rig.app->fileName(), current);
        QVERIFY(rig.recorder->isRecording());
        QCOMPARE(rig.recorder->boundShowId(),
                 rec == QLatin1String("bound") ? takeId : quint32(Function::invalidId()));
        QCOMPARE(take->commandTrack().count(), 1);
        QVERIFY(rig.recorder->setRecording(false));
        return;
    }

    // replaced once; nothing of the old take is armed or resolved any more
    QVERIFY2(!result.contains("error"), result.dump().c_str());
    rig.realTimer = true;
    QVERIFY(rig.doc->function(takeId) == nullptr);
    QCOMPARE(rig.app->fileName(), tool == QLatin1String("load_workspace") ? other : QString());
    QVERIFY(!rig.recorder->isRecording());
    Show *next = rig.addShow(3000);
    QVERIFY(rig.recorder->setRecording(true));
    QCOMPARE(rig.recorder->phaseValue(), int(ShowRecordPhase::Armed));
    QVERIFY(!next->commandRecording());
    QVERIFY(rig.recorder->setRecording(false));
}

namespace
{
/** A stopped Show in the production Show editor with a mixed take: legacy
 *  function commands and VC state records, with equal times */
struct EditorRig
{
    AppRig rig;
    Show *show = nullptr;
    Scene *scene = nullptr;
    VCButton *button = nullptr;
    VCSlider *fader = nullptr;
    VCSlider *dimmer = nullptr;
    QUuid missing = QUuid::createUuid();

    bool setUp()
    {
        if (rig.recorder == nullptr || rig.manager == nullptr)
            return false;
        rig.app->showNormal();
        rig.app->resize(1600, 900);
        if (!QTest::qWaitForWindowExposed(rig.app.get()))
            return false;

        scene = new Scene(rig.doc);
        scene->setName(QStringLiteral("Blue wash"));
        rig.doc->addFunction(scene);
        auto *vc = qobject_cast<VirtualConsole *>(rig.context("virtualConsole"));
        if (vc == nullptr)
            return false;
        VCBridgeV5 bridge(rig.doc, vc);
        const int frame = bridge.addFrame(0, QRect(0, 0, 600, 300), QStringLiteral("Frame"), false);
        button = addToggle(bridge, vc, frame, scene->id(), 10, QStringLiteral("Blue button"));
        fader = qobject_cast<VCSlider *>(vc->widget(bridge.addSlider(frame, QRect(100, 10, 60, 200),
            QStringLiteral("level"), QStringLiteral("Fader"), Function::invalidId(), {})));
        dimmer = qobject_cast<VCSlider *>(vc->widget(bridge.addSlider(frame, QRect(200, 10, 60, 200),
            QStringLiteral("level"), QStringLiteral("Dimmer"), Function::invalidId(), {})));
        if (button == nullptr || fader == nullptr || dimmer == nullptr)
            return false;
        fader->setSliderMode(VCSlider::Adjust);
        fader->setControlledFunction(scene->id());
        dimmer->setDisabled(true);

        show = new Show(rig.doc);
        show->setName(QStringLiteral("Edited show"));
        rig.doc->addFunction(show);
        ShowCommandTrack track;
        track.insert(ShowCommand::start(0, 1200, scene->id()));
        track.insert(ShowCommand::setButtonState(1, 1200, button->ensureRecordingId(), true));
        track.insert(ShowCommand::setSliderPosition(2, 1700, fader->ensureRecordingId(),
                                                    ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), 0.4));
        track.insert(ShowCommand::setIntensity(3, 1700, scene->id(), 0.5));
        track.insert(ShowCommand::setButtonState(4, 2500, missing, false));
        track.insert(ShowCommand::setSliderPosition(5, 2500, dimmer->ensureRecordingId(),
                                                    ShowControlRole::LevelSlider, QString(), 0.75));
        track.insert(ShowCommand::stop(6, 4000, scene->id()));
        track.setExtent(4000);
        if (!show->setCommandTrack(track))
            return false;

        if (!QMetaObject::invokeMethod(root(), "switchToContext", Q_ARG(QVariant, QStringLiteral("SHOWMGR")),
                                       Q_ARG(QVariant, QStringLiteral("qrc:/ShowManager.qml"))))
            return false;
        rig.manager->setCurrentShowID(int(show->id()));
        QCoreApplication::processEvents();
        rig.doc->resetModified();
        return true;
    }

    QQuickItem *root() const { return rig.app->rootObject(); }
    QQuickItem *item(const QString &name) const { return shownItem(root(), name); }

    /** The shown rows, top to bottom */
    QList<QQuickItem *> rows() const
    {
        QList<QQuickItem *> shown;
        QList<QQuickItem *> pending{item(QStringLiteral("commandView"))};
        while (!pending.isEmpty())
        {
            QQuickItem *next = pending.takeFirst();
            if (next == nullptr)
                continue;
            if (next->objectName() == QLatin1String("recordingRow"))
            {
                if (next->isVisible())
                    shown.append(next);
                continue;
            }
            pending += next->childItems();
        }
        std::sort(shown.begin(), shown.end(), [](QQuickItem *a, QQuickItem *b)
                  { return a->mapToScene(QPointF()).y() < b->mapToScene(QPointF()).y(); });
        return shown;
    }
    QVector<quint32> rowIds() const
    {
        QVector<quint32> ids;
        for (QQuickItem *row : rows())
            ids.append(row->property("commandId").toUInt());
        return ids;
    }
    QVector<quint32> selectedIds() const
    {
        QVector<quint32> ids;
        for (QQuickItem *row : rows())
        {
            if (row->property("selected").toBool())
                ids.append(row->property("commandId").toUInt());
        }
        return ids;
    }
    QQuickItem *row(quint32 id) const
    {
        for (QQuickItem *row : rows())
        {
            if (row->property("commandId").toUInt() == id)
                return row;
        }
        return nullptr;
    }
    static QString cell(QQuickItem *row, const char *name)
    {
        QQuickItem *text = row ? findVisualItem(row, QString::fromLatin1(name)) : nullptr;
        if (text == nullptr)
            return QStringLiteral("<no %1>").arg(QLatin1String(name));
        const QVariant label = text->property("label");
        return (label.isValid() ? label : text->property("text")).toString();
    }
    QPoint centerOf(QQuickItem *item) const
    {
        return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
    }
    void click(QQuickItem *item, Qt::KeyboardModifiers modifiers = Qt::NoModifier) const
    {
        QTest::mouseClick(rig.app.get(), Qt::LeftButton, modifiers, centerOf(item));
        QCoreApplication::processEvents();
    }
    /** A real double click on the named cell of the row */
    bool doubleClick(quint32 id, const char *name) const
    {
        QQuickItem *text = findVisualItem(row(id), QString::fromLatin1(name));
        if (text == nullptr)
            return false;
        const QPoint at = centerOf(text);
        QTest::mouseDClick(rig.app.get(), Qt::LeftButton, Qt::NoModifier, at);
        QCoreApplication::processEvents();
        return true;
    }
    QQuickItem *editor() const
    {
        QQuickItem *found = findVisualItem(item(QStringLiteral("commandView")), QStringLiteral("cellEditor"));
        return found != nullptr && found->isVisible() ? found : nullptr;
    }
    void type(const QString &text, Qt::Key finish = Qt::Key_Return) const
    {
        QTest::keyClick(rig.app.get(), Qt::Key_A, Qt::ControlModifier);
        for (const QChar &c : text)
            QTest::keyClick(rig.app.get(), c.toLatin1());
        if (finish != Qt::Key_unknown)
            QTest::keyClick(rig.app.get(), finish);
        QCoreApplication::processEvents();
    }
    bool openRecordings() const
    {
        QQuickItem *tab = item(QStringLiteral("recordingsTab"));
        if (tab == nullptr)
            return false;
        click(tab);
        return item(QStringLiteral("recordingsView")) != nullptr;
    }
    QVector<quint32> trackIds() const
    {
        QVector<quint32> ids;
        for (const ShowCommand &cmd : show->commandTrack().commands())
            ids.append(cmd.id);
        return ids;
    }
    Tardis *tardis() const { return qobject_cast<Tardis *>(rig.context("tardis")); }
    /** The undo position once the history thread took every queued action */
    int settledHistory() const
    {
        QTest::qWait(200);
        return tardis()->m_historyIndex;
    }
    QString shownText(const char *name) const
    {
        QQuickItem *text = item(QString::fromLatin1(name));
        return text ? text->property("label").toString() : QString();
    }
    const ShowCommand &command(quint32 id) const
    {
        const ShowCommandTrack &track = show->commandTrack();
        static const ShowCommand none;
        const int index = track.indexOfId(id);
        return index >= 0 ? track.commands().at(index) : none;
    }
};
} // namespace

void ShowCommandRecorder_Test::recordingsTab_listsAndEditsTheTake()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    const ShowCommandTrack before = ed.show->commandTrack();

    // the Show editor offers its recordings as a tab, next to the timeline
    QVERIFY2(ed.openRecordings(), "the Show editor has no Recordings tab");
    QVERIFY(ed.item(QStringLiteral("recordControl")) != nullptr);
    QVERIFY(ed.item(QStringLiteral("showItemsArea")) == nullptr);
    QTRY_COMPARE(ed.rowIds(), (QVector<quint32>{0, 1, 2, 3, 4, 5, 6}));

    // one ordered list: time, control, action, value; function commands and
    // VC states are told apart, controls by identity, caption and binding
    const QString fader = ed.fader->recordingId().toString(QUuid::WithoutBraces);
    QCOMPARE(EditorRig::cell(ed.row(0), "timeCell"), QStringLiteral("00:01.200"));
    QCOMPARE(EditorRig::cell(ed.row(0), "controlCell"), QStringLiteral("Blue wash"));
    QCOMPARE(EditorRig::cell(ed.row(0), "actionCell"), QStringLiteral("Start function"));
    QCOMPARE(EditorRig::cell(ed.row(0), "valueCell"), QString());
    QCOMPARE(EditorRig::cell(ed.row(1), "controlCell"), QStringLiteral("Blue button"));
    QVERIFY(EditorRig::cell(ed.row(1), "controlDetail").contains(ed.button->recordingId().toString(QUuid::WithoutBraces)));
    QVERIFY(EditorRig::cell(ed.row(1), "controlDetail").contains(QStringLiteral("Blue wash")));
    QCOMPARE(EditorRig::cell(ed.row(1), "actionCell"), QStringLiteral("Button state"));
    QCOMPARE(EditorRig::cell(ed.row(1), "valueCell"), QStringLiteral("On"));
    QCOMPARE(EditorRig::cell(ed.row(2), "controlCell"), QStringLiteral("Fader"));
    QVERIFY(EditorRig::cell(ed.row(2), "controlDetail").contains(fader));
    QVERIFY(EditorRig::cell(ed.row(2), "controlDetail").contains(QStringLiteral("Blue wash")));
    QCOMPARE(EditorRig::cell(ed.row(2), "actionCell"), QStringLiteral("Slider position"));
    QCOMPARE(EditorRig::cell(ed.row(2), "valueCell"), QStringLiteral("40%"));
    QCOMPARE(EditorRig::cell(ed.row(3), "actionCell"), QStringLiteral("Set intensity"));
    QCOMPARE(EditorRig::cell(ed.row(3), "valueCell"), QStringLiteral("50%"));
    // a control that no longer exists or is disabled stays in the list, saying so
    QVERIFY(EditorRig::cell(ed.row(4), "controlDetail").contains(ed.missing.toString(QUuid::WithoutBraces)));
    QVERIFY(EditorRig::cell(ed.row(4), "controlDetail").contains(QStringLiteral("Missing")));
    QCOMPARE(EditorRig::cell(ed.row(4), "valueCell"), QStringLiteral("Off"));
    QCOMPARE(EditorRig::cell(ed.row(5), "controlCell"), QStringLiteral("Dimmer"));
    QVERIFY(EditorRig::cell(ed.row(5), "controlDetail").contains(QStringLiteral("Disabled")));
    QCOMPARE(EditorRig::cell(ed.row(5), "valueCell"), QStringLiteral("75%"));
    QCOMPARE(EditorRig::cell(ed.row(6), "actionCell"), QStringLiteral("Stop function"));

    // filtering hides rows, it never reorders or changes them
    QQuickItem *filter = ed.item(QStringLiteral("recordingsFilter"));
    QVERIFY(filter);
    ed.click(filter);
    ed.type(QStringLiteral("button"), Qt::Key_unknown);
    QTRY_COMPARE(ed.rowIds(), (QVector<quint32>{1, 4}));
    ed.type(QString(), Qt::Key_Backspace);
    QTRY_COMPARE(ed.rowIds(), (QVector<quint32>{0, 1, 2, 3, 4, 5, 6}));
    QCOMPARE(ed.show->commandTrack().commands(), before.commands());

    // a click selects; Ctrl adds; Shift extends from the last clicked row
    ed.click(ed.row(2));
    QCOMPARE(ed.selectedIds(), QVector<quint32>{2});
    QVERIFY(ed.editor() == nullptr);
    ed.click(ed.row(4), Qt::ControlModifier);
    QCOMPARE(ed.selectedIds(), (QVector<quint32>{2, 4}));
    ed.click(ed.row(6), Qt::ShiftModifier);
    QCOMPARE(ed.selectedIds(), (QVector<quint32>{4, 5, 6}));
    QVERIFY(ed.editor() == nullptr);

    // a double click edits; Escape cancels without touching anything
    QVERIFY(ed.doubleClick(2, "timeCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    QCOMPARE(ed.editor()->property("text").toString(), QStringLiteral("1.700"));
    ed.type(QStringLiteral("9"), Qt::Key_Escape);
    QTRY_VERIFY(ed.editor() == nullptr);
    QCOMPARE(ed.show->commandTrack().commands(), before.commands());
    QVERIFY(!ed.rig.doc->isModified());

    // Enter commits the value, the saved data changes and the project is dirty
    QVERIFY(ed.doubleClick(2, "valueCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    QCOMPARE(ed.editor()->property("text").toString(), QStringLiteral("40"));
    ed.type(QStringLiteral("55"));
    QTRY_VERIFY(ed.editor() == nullptr);
    QCOMPARE(ed.command(2).position, 0.55);
    QVERIFY(ed.rig.doc->isModified());
    QTRY_COMPARE(EditorRig::cell(ed.row(2), "valueCell"), QStringLiteral("55%"));

    // the state of a button, and a time onto later ties: the event takes the
    // place its equal-time order gives it there (it was recorded before them)
    QVERIFY(ed.doubleClick(1, "valueCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    ed.type(QStringLiteral("off"));
    QTRY_VERIFY(ed.editor() == nullptr);
    QCOMPARE(ed.command(1).on, false);
    QVERIFY(ed.doubleClick(1, "timeCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    ed.type(QStringLiteral("1.7"));
    QTRY_VERIFY(ed.editor() == nullptr);
    QCOMPARE(ed.command(1).time, 1700u);
    QCOMPARE(ed.trackIds(), (QVector<quint32>{0, 1, 2, 3, 4, 5, 6}));
    QVERIFY(ed.doubleClick(1, "timeCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    ed.type(QStringLiteral("1.9"));
    QTRY_VERIFY(ed.editor() == nullptr);
    QCOMPARE(ed.trackIds(), (QVector<quint32>{0, 2, 3, 1, 4, 5, 6}));
    QTRY_COMPARE(ed.rowIds(), (QVector<quint32>{0, 2, 3, 1, 4, 5, 6}));
    // a trigger has no value to edit, and nothing converts it
    QVERIFY(ed.doubleClick(0, "valueCell"));
    QVERIFY(ed.editor() == nullptr);
    QCOMPARE(int(ed.command(0).action), int(ShowCommandAction::Start));

    // what was edited is what a saved project holds
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    QVERIFY(ed.show->commandTrack().saveXML(&writer));
    QVERIFY(xml.contains("0.55"));
}

void ShowCommandRecorder_Test::recordingsEditor_needsStoppedPlaybackAndRecOff_data()
{
    QTest::addColumn<QString>("state");

    QTest::newRow("REC on") << QStringLiteral("rec");
    QTest::newRow("start queued") << QStringLiteral("queued");
    QTest::newRow("playing") << QStringLiteral("playing");
    QTest::newRow("paused") << QStringLiteral("paused");
    QTest::newRow("stop not finished") << QStringLiteral("stopping");
    QTest::newRow("playback started during a cell edit") << QStringLiteral("during edit");
    QTest::newRow("started by a Collection on the timer") << QStringLiteral("collection");
}

void ShowCommandRecorder_Test::recordingsEditor_needsStoppedPlaybackAndRecOff()
{
    QFETCH(QString, state);

    EditorRig ed;
    QVERIFY(ed.setUp());
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);
    MasterTimer *timer = ed.rig.doc->masterTimer();
    const quint32 showId = ed.show->id();

    // one step to undo, made while everything is stopped
    const int base = ed.settledHistory();
    QVERIFY(ed.rig.recorder->retimeCommand(showId, 6, 4500));
    QTRY_COMPARE(ed.tardis()->m_historyIndex, base + 1);
    ed.rig.doc->resetModified();
    const ShowCommandTrack before = ed.show->commandTrack();

    if (state == QLatin1String("during edit"))
    {
        QVERIFY(ed.doubleClick(2, "valueCell"));
        QTRY_VERIFY(ed.editor() != nullptr);
    }
    Collection *collection = nullptr;
    if (state == QLatin1String("rec"))
    {
        QVERIFY(ed.rig.recorder->setRecording(true));
    }
    else if (state == QLatin1String("collection"))
    {
        collection = new Collection(ed.rig.doc);
        QVERIFY(ed.rig.doc->addFunction(collection));
        QVERIFY(collection->addFunction(showId));
        collection->start(timer, FunctionParent::master());
        tickAndDeliver(ed.rig.doc, 1);
        QVERIFY(!ed.show->commandPlaybackStopped());
        // adding the Collection is a project change of its own
        ed.rig.doc->resetModified();
    }
    else
    {
        ed.rig.manager->playShow();
        QVERIFY(timer->isStartQueued(ed.show));
        if (state != QLatin1String("queued") && state != QLatin1String("during edit"))
            tickAndDeliver(ed.rig.doc, 2);
        if (state == QLatin1String("paused") || state == QLatin1String("stopping"))
            ed.rig.manager->playShow();
        if (state == QLatin1String("stopping"))
        {
            ed.rig.manager->stopShow();
            QVERIFY(!ed.rig.manager->isPlaying());
        }
    }

    // the view stays readable and says why it cannot be edited
    QCOMPARE(ed.rowIds().count(), 7);
    QTRY_VERIFY(!ed.shownText("recordingsBlocked").isEmpty());
    const QString reason = ed.shownText("recordingsBlocked");
    QVERIFY(reason.contains(state == QLatin1String("rec") ? QLatin1String("REC") : QLatin1String("stopped")));

    // every path is refused at its commit, the UI's and the model's
    if (state == QLatin1String("during edit"))
    {
        ed.type(QStringLiteral("80"));
        QVERIFY(ed.editor() != nullptr);
        QVERIFY(ed.shownText("recordingsError").contains(QLatin1String("stopped")));
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Escape);
        QTRY_VERIFY(ed.editor() == nullptr);
    }
    else
    {
        QVERIFY(ed.doubleClick(2, "valueCell"));
        QVERIFY(ed.editor() == nullptr);
    }
    ed.click(ed.row(1));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Delete);
    QCoreApplication::processEvents();
    QVERIFY(!ed.rig.recorder->retimeCommand(showId, 2, 3000));
    QVERIFY(!ed.rig.recorder->setCommandValue(showId, 2, 0.9));
    QVERIFY(!ed.rig.recorder->setCommandState(showId, 1, false));
    QVERIFY(!ed.rig.recorder->removeCommands(showId, {1, 3}));
    QVERIFY(!ed.rig.recorder->moveCommands(showId, {1, 2}, 1, 2000));
    QVERIFY(!ed.rig.recorder->snapCommands(showId, {1, 2}, 500, 0));
    QVERIFY(!ed.rig.recorder->editCommandText(showId, 2, QStringLiteral("value"), QStringLiteral("10")));
    ed.tardis()->undoAction();
    QVERIFY(!ed.rig.recorder->lastError().isEmpty());
    QCOMPARE(ed.tardis()->m_historyIndex, base + 1);
    QCOMPARE(ed.show->commandTrack().commands(), before.commands());
    QVERIFY(!ed.rig.doc->isModified());

    // fully stopped with REC off, the same undo goes through
    if (state == QLatin1String("rec"))
    {
        QVERIFY(ed.rig.recorder->setRecording(false));
        ed.rig.doc->resetModified();
    }
    else if (collection != nullptr)
    {
        collection->stop(FunctionParent::master());
        ed.show->stop(FunctionParent::master());
        tickAndDeliver(ed.rig.doc, 3);
    }
    else
    {
        // a Stop before the timer ran the queued start does not stop it
        // (native Show Manager behaviour): the Show plays, so stop it again
        for (int i = 0; i < 3 && ed.rig.recorder->editBlockedReason().isEmpty() == false; i++)
        {
            tickAndDeliver(ed.rig.doc, 1);
            ed.rig.manager->stopShow();
            tickAndDeliver(ed.rig.doc, 2);
        }
    }
    QTRY_VERIFY(ed.shownText("recordingsBlocked").isEmpty());
    ed.tardis()->undoAction();
    QCOMPARE(ed.tardis()->m_historyIndex, base);
    QCOMPARE(ed.command(6).time, 4000u);
    QVERIFY(ed.rig.doc->isModified());
}

void ShowCommandRecorder_Test::recordingsEditor_rejectsInvalidEditsWhole_data()
{
    QTest::addColumn<quint32>("id");
    QTest::addColumn<QString>("field");
    QTest::addColumn<QString>("text");

    QTest::newRow("time not a number") << 2u << QStringLiteral("time") << QStringLiteral("soon");
    QTest::newRow("negative time") << 2u << QStringLiteral("time") << QStringLiteral("-0.5");
    QTest::newRow("time past the end of time") << 2u << QStringLiteral("time") << QStringLiteral("5000000");
    QTest::newRow("position above 100%") << 2u << QStringLiteral("value") << QStringLiteral("150");
    QTest::newRow("negative intensity") << 3u << QStringLiteral("value") << QStringLiteral("-5");
    QTest::newRow("value not a number") << 3u << QStringLiteral("value") << QStringLiteral("half");
    QTest::newRow("state neither On nor Off") << 1u << QStringLiteral("value") << QStringLiteral("maybe");
}

void ShowCommandRecorder_Test::recordingsEditor_rejectsInvalidEditsWhole()
{
    QFETCH(quint32, id);
    QFETCH(QString, field);
    QFETCH(QString, text);

    EditorRig ed;
    QVERIFY(ed.setUp());
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);
    const int base = ed.settledHistory();
    const ShowCommandTrack before = ed.show->commandTrack();

    QVERIFY(ed.doubleClick(id, field == QLatin1String("time") ? "timeCell" : "valueCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    ed.type(text);

    // nothing is clamped or half applied: the editor stays open with the reason
    QVERIFY(ed.editor() != nullptr);
    QVERIFY(!ed.shownText("recordingsError").isEmpty());
    QCOMPARE(ed.show->commandTrack().commands(), before.commands());
    QCOMPARE(ed.show->commandTrack().extent(), before.extent());
    QVERIFY(!ed.rig.doc->isModified());
    QCOMPARE(ed.settledHistory(), base);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Escape);
    QTRY_VERIFY(ed.editor() == nullptr);
    QCOMPARE(ed.show->commandTrack().commands(), before.commands());
}

void ShowCommandRecorder_Test::recordingsEditor_deleteUndoRedo_keepsLaterCapture()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);
    Tardis *tardis = ed.tardis();
    const int base = ed.settledHistory();
    const ShowCommandTrack original = ed.show->commandTrack();
    const quint32 a = ed.show->id();

    // three events of three ties, deleted at once without asking
    ed.click(ed.row(1));
    ed.click(ed.row(3), Qt::ControlModifier);
    ed.click(ed.row(4), Qt::ControlModifier);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Delete);
    QCoreApplication::processEvents();
    QCOMPARE(ed.trackIds(), (QVector<quint32>{0, 2, 5, 6}));
    QVERIFY(ed.rig.doc->isModified());
    QTRY_VERIFY(ed.item(QStringLiteral("deleteNotice")) != nullptr);
    QCOMPARE(ed.shownText("deleteNoticeText"), QStringLiteral("Deleted 3 event(s)"));
    QTRY_COMPARE(tardis->m_historyIndex, base + 1);

    // five more are recorded afterwards, three of them sharing those times
    QVERIFY(ed.rig.recorder->setRecording(true));
    const auto at = [&](int ms) { ed.rig.manager->setCurrentTime(ms); };
    at(1200); ed.button->requestUserStateChange(true);
    at(1700); ed.fader->requestUserValue(128);
    at(1700); ed.button->requestUserStateChange(true);
    at(2500); ed.fader->requestUserValue(30);
    at(5000); ed.button->requestUserStateChange(true);
    QVERIFY(ed.rig.recorder->setRecording(false));
    QCOMPARE(ed.trackIds(), (QVector<quint32>{0, 7, 2, 8, 9, 5, 10, 6, 11}));
    const ShowCommandTrack recorded = ed.show->commandTrack();
    ed.rig.doc->resetModified();

    // Undo restores exactly the three, in their places, and keeps the five
    QQuickItem *undo = ed.item(QStringLiteral("deleteUndo"));
    QVERIFY(undo);
    ed.click(undo);
    QCOMPARE(ed.trackIds(), (QVector<quint32>{0, 1, 7, 2, 3, 8, 9, 4, 5, 10, 6, 11}));
    for (quint32 id : {1u, 3u, 4u})
        QCOMPARE(ed.command(id), original.commands().at(original.indexOfId(id)));
    for (quint32 id : {7u, 8u, 9u, 10u, 11u})
        QCOMPARE(ed.command(id), recorded.commands().at(recorded.indexOfId(id)));
    QCOMPARE(ed.show->commandTrack().extent(), 5000u);
    QVERIFY(ed.rig.doc->isModified());
    QCOMPARE(tardis->m_historyIndex, base);
    QVERIFY(ed.item(QStringLiteral("deleteNotice")) == nullptr);

    // Redo deletes the same three again
    tardis->redoAction();
    QCOMPARE(ed.trackIds(), (QVector<quint32>{0, 7, 2, 8, 9, 5, 10, 6, 11}));
    QCOMPARE(tardis->m_historyIndex, base + 1);

    // the step belongs to its Show, whichever Show is selected now
    Show *other = new Show(ed.rig.doc);
    other->setName(QStringLiteral("Other show"));
    QVERIFY(ed.rig.doc->addFunction(other));
    ed.rig.manager->setCurrentShowID(int(other->id()));
    QTRY_COMPARE(ed.rowIds().count(), 0);
    QVERIFY(!ed.rig.recorder->removeCommands(a, {0}));
    QCOMPARE(ed.show->commandTrack().count(), 9);
    tardis->undoAction();
    QCOMPARE(ed.trackIds(), (QVector<quint32>{0, 1, 7, 2, 3, 8, 9, 4, 5, 10, 6, 11}));
    QCOMPARE(other->commandTrack().count(), 0);
    QCOMPARE(tardis->m_historyIndex, base);
    ed.rig.manager->setCurrentShowID(int(a));
    QTRY_COMPARE(ed.rowIds().count(), 12);

    // an event the step names changed meanwhile: refused, nothing consumed
    ShowCommandTrack changed = ed.show->commandTrack();
    ShowCommand three = ed.command(3);
    three.intensity = 0.9;
    QVERIFY(changed.replace(three));
    QVERIFY(ed.show->setCommandTrack(changed));
    ed.rig.doc->resetModified();
    tardis->redoAction();
    QCOMPARE(tardis->m_historyIndex, base);
    QCOMPARE(ed.show->commandTrack().commands(), changed.commands());
    QVERIFY(ed.rig.recorder->lastError().contains(QLatin1String("3")));
    QVERIFY(!ed.rig.doc->isModified());
    three.intensity = 0.5;
    QVERIFY(changed.replace(three));
    QVERIFY(ed.show->setCommandTrack(changed));
    tardis->redoAction();
    QCOMPARE(tardis->m_historyIndex, base + 1);
    QCOMPARE(ed.trackIds(), (QVector<quint32>{0, 7, 2, 8, 9, 5, 10, 6, 11}));

    // what a saved project holds, reloaded
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("edited.qxw"));
    QVERIFY(ed.rig.app->saveWorkspace(path));
    std::unique_ptr<Show> saved = showFromWorkspaceFile(ed.rig.doc, path, a);
    QVERIFY(saved);
    QCOMPARE(saved->commandTrack().commands(), ed.show->commandTrack().commands());
    QCOMPARE(saved->commandTrack().extent(), 5000u);
}

void ShowCommandRecorder_Test::recordingsEditor_movesGroupByMusicalSteps_data()
{
    QTest::addColumn<int>("division");
    QTest::addColumn<int>("bpm");
    QTest::addColumn<int>("step");
    QTest::addColumn<int>("key");
    QTest::addColumn<QVector<quint32>>("times");

    // events 1, 2 and 3 at 1.2 s, 1.7 s and 1.7 s (2 and 3 tied), moved together
    QTest::newRow("4/4, one bar later") << int(Show::BPM_4_4) << 120 << 0 << int(Qt::Key_Right)
                                        << QVector<quint32>{3200, 3700, 3700};
    QTest::newRow("4/4, half a bar earlier") << int(Show::BPM_4_4) << 120 << 1 << int(Qt::Key_Left)
                                             << QVector<quint32>{200, 700, 700};
    QTest::newRow("3/4, a quarter bar later") << int(Show::BPM_3_4) << 120 << 2 << int(Qt::Key_Right)
                                              << QVector<quint32>{1575, 2075, 2075};
    QTest::newRow("2/4 at 100 BPM, one bar later") << int(Show::BPM_2_4) << 100 << 0 << int(Qt::Key_Right)
                                                   << QVector<quint32>{2400, 2900, 2900};
    // one event would land before the Show starts: nothing moves
    QTest::newRow("4/4, one bar earlier is out of range") << int(Show::BPM_4_4) << 120 << 0 << int(Qt::Key_Left)
                                                          << QVector<quint32>{1200, 1700, 1700};
}

void ShowCommandRecorder_Test::recordingsEditor_movesGroupByMusicalSteps()
{
    QFETCH(int, division);
    QFETCH(int, bpm);
    QFETCH(int, step);
    QFETCH(int, key);
    QFETCH(QVector<quint32>, times);

    EditorRig ed;
    QVERIFY(ed.setUp());
    ed.rig.manager->setBpmNumber(bpm);
    ed.rig.manager->setTimeDivision(Show::TimeDivision(division));
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);
    const int base = ed.settledHistory();
    ed.rig.doc->resetModified();
    const ShowCommandTrack before = ed.show->commandTrack();
    const bool moves = times.first() != 1200;

    QQuickItem *stepChooser = ed.item(QStringLiteral("moveStep"));
    QVERIFY(stepChooser);
    stepChooser->setProperty("currentIndex", step);
    QVERIFY(ed.item(QStringLiteral("musicalUnavailable")) == nullptr);

    // a cell editor keeps its arrow keys: they move its text cursor only
    QVERIFY(ed.doubleClick(1, "timeCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    ed.editor()->setProperty("cursorPosition", 0);
    QTest::keyClick(ed.rig.app.get(), Qt::Key(key));
    QTest::keyClick(ed.rig.app.get(), Qt::Key(key));
    QCOMPARE(ed.show->commandTrack().commands(), before.commands());
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Escape);
    QTRY_VERIFY(ed.editor() == nullptr);

    // the selected group moves by the key, with one shared delta
    ed.click(ed.row(1));
    ed.click(ed.row(3), Qt::ShiftModifier);
    QCOMPARE(ed.selectedIds(), (QVector<quint32>{1, 2, 3}));
    QTest::keyClick(ed.rig.app.get(), Qt::Key(key));
    QCoreApplication::processEvents();
    QCOMPARE(ed.command(1).time, times.at(0));
    QCOMPARE(ed.command(2).time, times.at(1));
    QCOMPARE(ed.command(3).time, times.at(2));
    // values, targets and the tie order stay
    const QVector<quint32> ids = ed.trackIds();
    QVERIFY(ids.indexOf(2) < ids.indexOf(3));
    for (quint32 id : {1u, 2u, 3u})
    {
        ShowCommand expected = before.commands().at(before.indexOfId(id));
        expected.time = ed.command(id).time;
        QCOMPARE(ed.command(id), expected);
    }
    QCOMPARE(ed.rig.doc->isModified(), moves);
    QCOMPARE(ed.selectedIds(), (QVector<quint32>{1, 2, 3}));
    if (!moves)
    {
        QVERIFY(!ed.shownText("recordingsError").isEmpty());
        QCOMPARE(ed.show->commandTrack().commands(), before.commands());
        QCOMPARE(ed.settledHistory(), base);
        return;
    }

    // one move is one undo step, back to the exact places
    QTRY_COMPARE(ed.tardis()->m_historyIndex, base + 1);
    ed.tardis()->undoAction();
    QCOMPARE(ed.show->commandTrack().commands(), before.commands());
    QCOMPARE(ed.tardis()->m_historyIndex, base);
}

void ShowCommandRecorder_Test::recordingsEditor_snapsGroupToGrid_data()
{
    QTest::addColumn<QString>("grid");
    QTest::addColumn<QVector<quint32>>("times");

    // events 1 and 2 at 1.2 s and 1.7 s: the earliest goes to the nearest beat
    QTest::newRow("VDJ grid with its anchor") << QStringLiteral("vdj") << QVector<quint32>{1131, 1631};
    QTest::newRow("4/4 at 128 BPM") << QStringLiteral("128") << QVector<quint32>{1406, 1906};
    QTest::newRow("4/4 at 120 BPM") << QStringLiteral("120") << QVector<quint32>{1000, 1500};
    QTest::newRow("no tempo") << QStringLiteral("time") << QVector<quint32>{1200, 1700};
}

void ShowCommandRecorder_Test::recordingsEditor_snapsGroupToGrid()
{
    QFETCH(QString, grid);
    QFETCH(QVector<quint32>, times);

    EditorRig ed;
    QVERIFY(ed.setUp());
    if (grid == QLatin1String("vdj"))
    {
        ed.rig.manager->setTimeDivision(Show::VDJBeat);
        ed.rig.manager->m_vdjGrid.valid = true;
        ed.rig.manager->m_vdjGrid.beatPeriodMs = 500.0;
        ed.rig.manager->m_vdjGrid.anchorMs = 130.5;
        emit ed.rig.manager->vdjGridChanged();
    }
    else if (grid != QLatin1String("time"))
    {
        ed.rig.manager->setBpmNumber(grid.toInt());
        ed.rig.manager->setTimeDivision(Show::BPM_4_4);
    }
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);
    const int base = ed.settledHistory();
    ed.rig.doc->resetModified();
    const ShowCommandTrack before = ed.show->commandTrack();

    ed.click(ed.row(1));
    ed.click(ed.row(2), Qt::ControlModifier);
    QQuickItem *snap = ed.item(QStringLiteral("snapButton"));
    QVERIFY(snap);
    if (grid == QLatin1String("time"))
    {
        // no musical grid: said so, Snap and moves unavailable, times still editable
        QVERIFY(ed.item(QStringLiteral("musicalUnavailable")) != nullptr);
        QVERIFY(!snap->isEnabled());
        QVERIFY(!ed.item(QStringLiteral("moveLater"))->isEnabled());
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Right);
        QCOMPARE(ed.show->commandTrack().commands(), before.commands());
        QVERIFY(ed.doubleClick(1, "timeCell"));
        QTRY_VERIFY(ed.editor() != nullptr);
        ed.type(QStringLiteral("1.25"));
        QCOMPARE(ed.command(1).time, 1250u);
        return;
    }

    QVERIFY(snap->isEnabled());
    ed.click(snap);
    QCOMPARE(ed.command(1).time, times.at(0));
    QCOMPARE(ed.command(2).time, times.at(1));
    QCOMPARE(ed.command(2).position, 0.4);
    QVERIFY(ed.rig.doc->isModified());
    QTRY_COMPARE(ed.tardis()->m_historyIndex, base + 1);
    ed.tardis()->undoAction();
    QCOMPARE(ed.show->commandTrack().commands(), before.commands());
}

void ShowCommandRecorder_Test::recordingsEditor_groupEditsRejectWhole()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    ed.rig.manager->setBpmNumber(128);
    ed.rig.manager->setTimeDivision(Show::BPM_4_4);
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);
    ShowCommandRecorder *recorder = ed.rig.recorder;
    const quint32 showId = ed.show->id();
    const qreal halfBar = 0.5 * 4 * 60000.0 / 128; // 937.5 ms
    QVERIFY(recorder->retimeCommand(showId, 6, ShowCommand::MaxTime - 100));
    QVERIFY(recorder->retimeCommand(showId, 0, 50));
    const ShowCommandTrack before = ed.show->commandTrack();
    ed.rig.doc->resetModified();

    // repeated nudges forward and back come back exactly, times and the
    // order of 2 before its unselected tie 3: one rounded step, one order
    for (int i = 0; i < 3; i++)
        QVERIFY(recorder->moveCommands(showId, {1, 2}, 1, halfBar));
    QCOMPARE(ed.command(1).time, 1200u + 3 * 938);
    QCOMPARE(ed.command(2).time - ed.command(1).time, 500u);
    for (int i = 0; i < 3; i++)
        QVERIFY(recorder->moveCommands(showId, {1, 2}, -1, halfBar));
    QCOMPARE(ed.show->commandTrack().commands(), before.commands());

    // any result out of range, or a selection that went stale, refuses it all
    ed.rig.doc->resetModified();
    QVERIFY(!recorder->moveCommands(showId, {5, 6}, 1, 2 * halfBar));    // past the end of time
    QVERIFY(!recorder->snapCommands(showId, {0, 2}, 500, 400));        // 50 ms snaps to -100 ms
    QVERIFY(!recorder->snapCommands(showId, {1, 2, 77}, 500, 0));      // 77 is gone
    QVERIFY(!recorder->moveCommands(showId, {1, 77}, 1, halfBar));
    QVERIFY(!recorder->removeCommands(showId, {1, 77}));
    QVERIFY(!recorder->moveCommands(showId, {1, 2}, 1, qQNaN()));
    QVERIFY(!recorder->snapCommands(showId, {1, 2}, 0, 0));
    QCOMPARE(ed.show->commandTrack().commands(), before.commands());
    QVERIFY(!ed.rig.doc->isModified());
}

void ShowCommandRecorder_Test::recordingsEditor_staleEditorNeverEditsAnotherShow()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    Show *other = new Show(ed.rig.doc);
    other->setName(QStringLiteral("Other show"));
    QVERIFY(ed.rig.doc->addFunction(other));
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 800, ed.scene->id())));
    QVERIFY(other->setCommandTrack(track));
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);
    ed.rig.doc->resetModified();

    // an edit begun on A's event 0 is A's, even once B, with its own event 0, is shown
    QVERIFY(ed.doubleClick(0, "timeCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    ed.rig.manager->setCurrentShowID(int(other->id()));
    QTRY_COMPARE(ed.rowIds(), QVector<quint32>{0});
    ed.type(QStringLiteral("9.9"));
    QCOMPARE(other->commandTrack().commands().at(0).time, 800u);
    QCOMPARE(ed.command(0).time, 1200u);
    QVERIFY(ed.editor() == nullptr);
    QVERIFY(!ed.rig.doc->isModified());
}

void ShowCommandRecorder_Test::recordingsEditor_undoStepsKeepNativeGrouping()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);
    Tardis *tardis = ed.tardis();
    const int base = ed.settledHistory();
    const QString buttonCaption = ed.button->caption();
    const QString faderCaption = ed.fader->caption();

    // rapid editor steps, each one its own step
    for (int i = 1; i <= 10; i++)
        QVERIFY(ed.rig.recorder->retimeCommand(ed.show->id(), 6, 4000 + i * 10));
    QTRY_COMPARE(tardis->m_historyIndex, base + 10);

    // then two unrelated native edits well apart in time stay two steps
    ed.button->setCaption(QStringLiteral("First"));
    QTest::qWait(400);
    ed.fader->setCaption(QStringLiteral("Second"));
    QTRY_COMPARE(tardis->m_historyIndex, base + 12);
    tardis->undoAction();
    QCOMPARE(ed.fader->caption(), faderCaption);
    QCOMPARE(ed.button->caption(), QStringLiteral("First"));
    tardis->undoAction();
    QCOMPARE(ed.button->caption(), buttonCaption);
    QCOMPARE(ed.command(6).time, 4100u);
    tardis->undoAction();
    QCOMPARE(ed.command(6).time, 4090u);
}

void ShowCommandRecorder_Test::recordingsEditor_restoredShowNeverReusesHistoryIds()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);
    Tardis *tardis = ed.tardis();
    const int base = ed.settledHistory();
    const quint32 a = ed.show->id();

    // the highest event goes, then its Show, which native undo brings back
    QVERIFY(ed.rig.recorder->removeCommands(a, {6}));
    auto *functions = qobject_cast<FunctionManager *>(ed.rig.context("functionManager"));
    QVERIFY(functions);
    // the Show Manager keeps a plain pointer to its current Show: show another first
    Show *other = new Show(ed.rig.doc);
    QVERIFY(ed.rig.doc->addFunction(other));
    ed.rig.manager->setCurrentShowID(int(other->id()));
    functions->deleteFunctions({a});
    QVERIFY(ed.rig.doc->function(a) == nullptr);
    QTRY_COMPARE(tardis->m_historyIndex, base + 2);
    tardis->undoAction();
    Show *restored = qobject_cast<Show *>(ed.rig.doc->function(a));
    QVERIFY(restored);
    QCOMPARE(restored->commandTrack().count(), 6);
    ed.rig.manager->setCurrentShowID(int(a));

    // a new capture gets an id the history cannot mean
    QVERIFY(ed.rig.recorder->setRecording(true));
    ed.rig.manager->setCurrentTime(3000);
    ed.button->requestUserStateChange(true);
    QVERIFY(ed.rig.recorder->setRecording(false));
    QCOMPARE(restored->commandTrack().count(), 7);
    const quint32 captured = restored->commandTrack().commands().last().id;
    QVERIFY2(captured > 6, qPrintable(QString::number(captured)));

    // so the deletion still undoes, keeping the capture
    tardis->undoAction();
    QCOMPARE(tardis->m_historyIndex, base);
    QCOMPARE(restored->commandTrack().count(), 8);
    QVERIFY(restored->commandTrack().contains(6));
    QVERIFY(restored->commandTrack().contains(captured));
}

void ShowCommandRecorder_Test::recordingsEditor_awayAndBackKeepsTieOrder_data()
{
    QTest::addColumn<quint32>("moved");
    QTest::addColumn<QString>("path");

    // A (id 9) before B (id 2) at 1.7 s: the ids run against their order
    QTest::newRow("A, selected before its tie, keys") << 9u << QStringLiteral("keys");
    QTest::newRow("B, selected after its tie, keys") << 2u << QStringLiteral("keys");
    QTest::newRow("A, cell time edits") << 9u << QStringLiteral("cell");
    QTest::newRow("B, cell time edits") << 2u << QStringLiteral("cell");
    QTest::newRow("A, Snap then its time back") << 9u << QStringLiteral("snap");
    QTest::newRow("A, undo and redo on the way") << 9u << QStringLiteral("undo");
    QTest::newRow("A, saved and reloaded on the way") << 9u << QStringLiteral("save");
}

void ShowCommandRecorder_Test::recordingsEditor_awayAndBackKeepsTieOrder()
{
    QFETCH(quint32, moved);
    QFETCH(QString, path);

    EditorRig ed;
    QVERIFY(ed.setUp());
    ShowCommandTrack ties;
    QVERIFY(ties.insert(ShowCommand::start(0, 1200, ed.scene->id())));
    QVERIFY(ties.insert(ShowCommand::setSliderPosition(9, 1700, ed.fader->recordingId(),
                                                       ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), 0.3)));
    QVERIFY(ties.insert(ShowCommand::setIntensity(2, 1700, ed.scene->id(), 0.6)));
    QVERIFY(ties.insert(ShowCommand::setButtonState(5, 2500, ed.button->recordingId(), true)));
    QVERIFY(ed.show->setCommandTrack(ties));
    ed.rig.manager->setBpmNumber(120);
    ed.rig.manager->setTimeDivision(Show::BPM_4_4);
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds(), (QVector<quint32>{0, 9, 2, 5}));
    const QVector<quint32> original{0, 9, 2, 5};
    const int base = ed.settledHistory();
    const auto key = [&](Qt::Key k)
    {
        ed.click(ed.row(moved));
        QTest::keyClick(ed.rig.app.get(), k);
        QCoreApplication::processEvents();
    };
    const auto cellTime = [&](const QString &seconds)
    {
        QVERIFY(ed.doubleClick(moved, "timeCell"));
        QTRY_VERIFY(ed.editor() != nullptr);
        ed.type(seconds);
        QTRY_VERIFY(ed.editor() == nullptr);
    };

    if (path == QLatin1String("keys"))
    {
        key(Qt::Key_Right);
        QCOMPARE(ed.command(moved).time, 3700u);
        key(Qt::Key_Left);
    }
    else if (path == QLatin1String("cell"))
    {
        cellTime(QStringLiteral("2.5"));
        QCOMPARE(ed.command(moved).time, 2500u);
        cellTime(QStringLiteral("1.7"));
    }
    else if (path == QLatin1String("snap"))
    {
        ed.click(ed.row(moved));
        QQuickItem *snap = ed.item(QStringLiteral("snapButton"));
        QVERIFY(snap);
        ed.click(snap);
        QCOMPARE(ed.command(moved).time, 1500u);
        cellTime(QStringLiteral("1.7"));
    }
    else if (path == QLatin1String("undo"))
    {
        key(Qt::Key_Right);
        QTRY_COMPARE(ed.tardis()->m_historyIndex, base + 1);
        ed.tardis()->undoAction();
        QCOMPARE(ed.trackIds(), original);
        ed.tardis()->redoAction();
        QCOMPARE(ed.command(moved).time, 3700u);
        key(Qt::Key_Left);
    }
    else
    {
        key(Qt::Key_Right);
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("ties.qxw"));
        QVERIFY(ed.rig.app->saveWorkspace(file));
        std::unique_ptr<Show> saved = showFromWorkspaceFile(ed.rig.doc, file, ed.show->id());
        QVERIFY(saved);
        ShowCommandTrack reloaded = saved->commandTrack();
        QVERIFY(reloaded.retime(moved, 1700));
        QVector<quint32> ids;
        for (const ShowCommand &cmd : reloaded.commands())
            ids.append(cmd.id);
        QCOMPARE(ids, original);
        key(Qt::Key_Left);
    }

    // back at 1.7 s, A is before B again: moving back is not an undo
    QCOMPARE(ed.command(moved).time, 1700u);
    QCOMPARE(ed.trackIds(), original);
    QTRY_COMPARE(ed.rowIds(), original);
}

void ShowCommandRecorder_Test::recordingsEditor_visibleRowsFollowTheirControls()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    Scene *other = new Scene(ed.rig.doc);
    other->setName(QStringLiteral("Amber wash"));
    QVERIFY(ed.rig.doc->addFunction(other));
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);

    // an edit is under way when the controls change
    QVERIFY(ed.doubleClick(2, "valueCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    ed.type(QStringLiteral("5"), Qt::Key_unknown);
    QQuickItem *editor = ed.editor();

    // renamed, rebound, re-enabled and a renamed function: shown as they are now
    ed.button->setCaption(QStringLiteral("Renamed button"));
    ed.button->setFunctionID(other->id());
    ed.dimmer->setDisabled(false);
    ed.scene->setName(QStringLiteral("Deep blue"));
    QTRY_COMPARE(EditorRig::cell(ed.row(1), "controlCell"), QStringLiteral("Renamed button"));
    QTRY_VERIFY(EditorRig::cell(ed.row(1), "controlDetail").contains(QStringLiteral("Amber wash")));
    QTRY_VERIFY(!EditorRig::cell(ed.row(5), "controlDetail").contains(QStringLiteral("Disabled")));
    QTRY_COMPARE(EditorRig::cell(ed.row(0), "controlCell"), QStringLiteral("Deep blue"));
    QTRY_VERIFY(EditorRig::cell(ed.row(2), "controlDetail").contains(QStringLiteral("Deep blue")));

    // the open edit kept its field, its text and its keyboard focus
    QCOMPARE(ed.editor(), editor);
    QVERIFY(editor->hasActiveFocus());
    QCOMPARE(editor->property("text").toString(), QStringLiteral("5"));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_5);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Return);
    QCoreApplication::processEvents();
    QCOMPARE(ed.command(2).position, 0.55);

    // a control that goes away is shown missing, not dropped
    const QUuid fader = ed.fader->recordingId();
    auto *vc = qobject_cast<VirtualConsole *>(ed.rig.context("virtualConsole"));
    vc->deleteVCWidgets({ed.fader->id()});
    ed.fader = nullptr;
    QTRY_VERIFY(EditorRig::cell(ed.row(2), "controlDetail").contains(QStringLiteral("Missing")));
    QVERIFY(EditorRig::cell(ed.row(2), "controlDetail").contains(fader.toString(QUuid::WithoutBraces)));
    QCOMPARE(ed.rowIds().count(), 7);
}

void ShowCommandRecorder_Test::recordingsEditor_restoredControlsReturnToTheirRows()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);
    auto *vc = qobject_cast<VirtualConsole *>(ed.rig.context("virtualConsole"));
    auto *functions = qobject_cast<FunctionManager *>(ed.rig.context("functionManager"));
    QVERIFY(vc && functions);
    Tardis *tardis = ed.tardis();
    const int base = ed.settledHistory();
    const QByteArray saved = [&]()
    {
        QByteArray xml;
        QXmlStreamWriter writer(&xml);
        ed.show->commandTrack().saveXML(&writer);
        return xml;
    }();

    // an edit is under way on the legacy value while its controls come and go
    QVERIFY(ed.doubleClick(3, "valueCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    ed.type(QStringLiteral("7"), Qt::Key_unknown);
    QQuickItem *editor = ed.editor();

    // the slider and the legacy target go, natively, as two undo steps
    const quint32 sceneId = ed.scene->id();
    vc->deleteVCWidgets({ed.fader->id()});
    ed.fader = nullptr;
    QTRY_VERIFY(EditorRig::cell(ed.row(2), "controlDetail").contains(QStringLiteral("Missing")));
    QTest::qWait(300);
    functions->deleteFunctions({sceneId});
    ed.scene = nullptr;
    QTRY_COMPARE(EditorRig::cell(ed.row(0), "controlCell"), QStringLiteral("Missing function"));
    QCOMPARE(ed.settledHistory(), base + 2);

    // native undo brings each back with its identity: the open rows show them
    tardis->undoAction();
    QVERIFY(ed.rig.doc->function(sceneId) != nullptr);
    QTRY_COMPARE(EditorRig::cell(ed.row(0), "controlCell"), QStringLiteral("Blue wash"));
    QVERIFY(EditorRig::cell(ed.row(2), "controlDetail").contains(QStringLiteral("Missing")));
    tardis->undoAction();
    QCOMPARE(tardis->m_historyIndex, base);
    QTRY_COMPARE(EditorRig::cell(ed.row(2), "controlCell"), QStringLiteral("Fader"));
    QVERIFY(!EditorRig::cell(ed.row(2), "controlDetail").contains(QStringLiteral("Missing")));
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    ed.show->commandTrack().saveXML(&writer);
    QCOMPARE(xml, saved);

    // the open edit kept its text, focus and target
    QCOMPARE(ed.editor(), editor);
    QVERIFY(editor->hasActiveFocus());
    QCOMPARE(editor->property("text").toString(), QStringLiteral("7"));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_5);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Return);
    QCoreApplication::processEvents();
    QCOMPARE(ed.command(3).intensity, 0.75);

    // Timeline and Recordings share the parent editor's observation lease.
    ed.click(ed.item(QStringLiteral("timelineTab")));
    QTRY_VERIFY(ed.item(QStringLiteral("recordingsView")) == nullptr);
    QSignalSpy refreshed(ed.rig.recorder, &ShowCommandRecorder::commandsChanged);
    vc->deleteVCWidgets({ed.button->id()});
    ed.button = nullptr;
    QTest::qWait(300);
    tardis->undoAction();
    QTest::qWait(300);
    QVERIFY(refreshed.count() > 0);
    QVERIFY(QMetaObject::invokeMethod(ed.root(), "switchToContext", Q_ARG(QVariant, QStringLiteral("VC")),
                                      Q_ARG(QVariant, QStringLiteral("qrc:/VirtualConsole.qml"))));
    QCoreApplication::processEvents();
    refreshed.clear();
    auto *restored = qobject_cast<VCButton *>(vc->widgetsByRecordingId(ed.command(1).controlId).value(0));
    QVERIFY(restored);
    restored->setCaption(QStringLiteral("Outside the Show editor"));
    QTest::qWait(100);
    QCOMPARE(refreshed.count(), 0);
}

void ShowCommandRecorder_Test::recordingsEditor_handlesTheWholeEventIdRange_data()
{
    QTest::addColumn<quint32>("id");

    QTest::newRow("largest signed") << 2147483647u;
    QTest::newRow("smallest beyond signed") << 2147483648u;
    QTest::newRow("largest valid") << 4294967294u;
}

void ShowCommandRecorder_Test::recordingsEditor_handlesTheWholeEventIdRange()
{
    QFETCH(quint32, id);

    EditorRig ed;
    QVERIFY(ed.setUp());
    ShowCommandTrack take;
    QVERIFY(take.insert(ShowCommand::setIntensity(id, 1700, ed.scene->id(), 0.4)));
    QVERIFY(take.insert(ShowCommand::setSliderPosition(1, 1700, ed.fader->recordingId(),
                                                       ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), 0.3)));
    QVERIFY(take.insert(ShowCommand::start(5, 1200, ed.scene->id())));
    QVERIFY(ed.show->setCommandTrack(take));
    ed.rig.manager->setBpmNumber(120);
    ed.rig.manager->setTimeDivision(Show::BPM_4_4);
    QVERIFY(ed.openRecordings());
    const QVector<quint32> order{5, id, 1};
    QTRY_COMPARE(ed.rowIds(), order);

    // selection, Ctrl and a Shift range from an anchor, both ways
    ed.click(ed.row(id));
    QCOMPARE(ed.selectedIds(), QVector<quint32>{id});
    ed.click(ed.row(1), Qt::ControlModifier);
    QCOMPARE(ed.selectedIds(), (QVector<quint32>{id, 1}));
    ed.click(ed.row(5));
    ed.click(ed.row(1), Qt::ShiftModifier);
    QCOMPARE(ed.selectedIds(), order);
    ed.click(ed.row(id));
    ed.click(ed.row(5), Qt::ShiftModifier);
    QCOMPARE(ed.selectedIds(), (QVector<quint32>{5, id}));

    // a cell edit reaches the event
    QVERIFY(ed.doubleClick(id, "valueCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    ed.type(QStringLiteral("65"));
    QTRY_VERIFY(ed.editor() == nullptr);
    QCOMPARE(ed.command(id).intensity, 0.65);

    // moved away and back by the keys, it is before its tie again
    ed.click(ed.row(id));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Right);
    QCOMPARE(ed.command(id).time, 3700u);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Left);
    QCOMPARE(ed.trackIds(), order);
    QCOMPARE(ed.selectedIds(), QVector<quint32>{id});

    // deleted and brought back through the notice, with its value and place
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Delete);
    QCoreApplication::processEvents();
    QCOMPARE(ed.trackIds(), (QVector<quint32>{5, 1}));
    QTRY_VERIFY(ed.item(QStringLiteral("deleteUndo")) != nullptr);
    QTest::qWait(200);
    ed.click(ed.item(QStringLiteral("deleteUndo")));
    QCOMPARE(ed.trackIds(), order);
    QCOMPARE(ed.command(id).intensity, 0.65);
}

void ShowCommandRecorder_Test::recordingsEditor_historyKeepsNewestStepsAtCapacity_data()
{
    QTest::addColumn<bool>("branch");

    QTest::newRow("101 edits in one native batch") << false;
    QTest::newRow("native step, undo and a new branch") << true;
}

void ShowCommandRecorder_Test::recordingsEditor_historyEvictsWholeNativeSteps()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    Tardis *tardis = ed.tardis();
    ed.settledHistory();
    tardis->resetHistory();

    // one native step of two actions, then 100 editor steps: the native step goes whole
    ed.button->setCaption(QStringLiteral("Pair A"));
    ed.fader->setCaption(QStringLiteral("Pair B"));
    QCOMPARE(ed.settledHistory(), 1);
    tardis->beginBatch(QStringLiteral("editor steps"));
    for (int i = 1; i <= 100; i++)
        QVERIFY(ed.rig.recorder->retimeCommand(ed.show->id(), 6, 4000 + i));
    tardis->endBatch();
    QCOMPARE(ed.settledHistory(), 99);
    QCOMPARE(tardis->m_history.count(), 100);
    QCOMPARE(tardis->m_historyCount, 100);
    for (int i = 0; i < 101; i++)
        tardis->undoAction();
    QCOMPARE(ed.command(6).time, 4000u);
    QCOMPARE(ed.button->caption(), QStringLiteral("Pair A"));
    QCOMPARE(ed.fader->caption(), QStringLiteral("Pair B"));
}

void ShowCommandRecorder_Test::recordingsEditor_historyKeepsNewestStepsAtCapacity()
{
    QFETCH(bool, branch);

    EditorRig ed;
    QVERIFY(ed.setUp());
    Tardis *tardis = ed.tardis();
    ed.settledHistory();
    tardis->resetHistory();
    const quint32 showId = ed.show->id();
    const QString caption = ed.button->caption();
    const auto edits = [&](int from, int to)
    {
        tardis->beginBatch(QStringLiteral("editor steps"));
        for (int i = from; i <= to; i++)
            QVERIFY(ed.rig.recorder->retimeCommand(showId, 6, 4000 + i));
        tardis->endBatch();
    };

    int newest = 4101;
    if (branch)
    {
        // 60 edits, a native step, then back over it and nine edits, and a new branch of 50
        edits(1, 60);
        ed.button->setCaption(QStringLiteral("Native step"));
        QCOMPARE(ed.settledHistory(), 60);
        for (int i = 0; i < 10; i++)
            tardis->undoAction();
        QCOMPARE(ed.button->caption(), caption);
        QCOMPARE(ed.command(6).time, 4051u);
        edits(200, 249);
        newest = 4249;
    }
    else
    {
        edits(1, 101);
    }

    // 100 steps kept, all single edits: only the oldest went
    QCOMPARE(ed.settledHistory(), 99);
    QCOMPARE(tardis->m_history.count(), 100);
    QCOMPARE(ed.command(6).time, quint32(newest));
    tardis->undoAction();
    QCOMPARE(ed.command(6).time, quint32(newest - 1));
    for (int i = 1; i < 100; i++)
        tardis->undoAction();
    QCOMPARE(tardis->m_historyIndex, -1);
    QCOMPARE(ed.command(6).time, 4001u);
    tardis->undoAction();
    QCOMPARE(ed.command(6).time, 4001u);
    QCOMPARE(ed.button->caption(), caption);
    tardis->redoAction();
    QCOMPARE(ed.command(6).time, 4002u);
}

void ShowCommandRecorder_Test::recordingsEditor_nativeUndoKeepsItsNewestWindow_data()
{
    QTest::addColumn<QString>("sequence");

    // three different captions 80 ms apart: 160 ms from first to last
    QTest::newRow("rapid chain of different controls") << QStringLiteral("chain");
    // A, B, C and A again 80 ms apart, then back over it and one editor edit
    QTest::newRow("same control again, then an editor branch") << QStringLiteral("again");
    // the same, undone twice, then one editor edit: its step is counted
    QTest::newRow("editor branch after undoing it all") << QStringLiteral("branch");
    // A, B and A again 50 ms apart: one window, merged into one step
    QTest::newRow("same control again within the window") << QStringLiteral("merge");
}

void ShowCommandRecorder_Test::recordingsEditor_nativeUndoKeepsItsNewestWindow()
{
    QFETCH(QString, sequence);

    EditorRig ed;
    QVERIFY(ed.setUp());
    Tardis *tardis = ed.tardis();
    ed.settledHistory();
    tardis->resetHistory();
    VCWidget *a = ed.button, *b = ed.fader, *c = ed.dimmer;
    const QString a0 = a->caption(), b0 = b->caption(), c0 = c->caption();
    const auto captions = [&]() { return QStringList{a->caption(), b->caption(), c->caption()}; };

    if (sequence == QLatin1String("merge"))
    {
        a->setCaption(QStringLiteral("A1"));
        QTest::qWait(50);
        b->setCaption(QStringLiteral("B1"));
        QTest::qWait(50);
        a->setCaption(QStringLiteral("A2"));
        ed.settledHistory();
        QCOMPARE(tardis->m_historyCount, 1);
        tardis->undoAction();
        QCOMPARE(captions(), (QStringList{a0, b0, c0}));
        QCOMPARE(tardis->m_historyIndex, -1);
        tardis->redoAction();
        QCOMPARE(captions(), (QStringList{QStringLiteral("A2"), QStringLiteral("B1"), c0}));
        return;
    }

    a->setCaption(QStringLiteral("A1"));
    QTest::qWait(80);
    b->setCaption(QStringLiteral("B1"));
    QTest::qWait(80);
    c->setCaption(QStringLiteral("C1"));
    if (sequence != QLatin1String("chain"))
    {
        QTest::qWait(80);
        a->setCaption(QStringLiteral("A2"));
    }
    ed.settledHistory();
    if (sequence == QLatin1String("branch"))
    {
        tardis->undoAction();
        tardis->undoAction();
        QVERIFY(ed.rig.recorder->retimeCommand(ed.show->id(), 6, 4321));
        QCOMPARE(ed.settledHistory(), 0);
        QCOMPARE(tardis->m_history.count(), 1);
        QCOMPARE(tardis->m_historyCount, 1);
        tardis->undoAction();
        QCOMPARE(ed.command(6).time, 4000u);
        return;
    }

    // Undo takes what lies within 150 ms of the newest action, as before
    const int steps = tardis->m_historyCount;
    tardis->undoAction();
    if (sequence == QLatin1String("chain"))
    {
        QCOMPARE(captions(), (QStringList{QStringLiteral("A1"), b0, c0}));
        tardis->undoAction();
        QCOMPARE(captions(), (QStringList{a0, b0, c0}));
        tardis->redoAction();
        QCOMPARE(captions(), (QStringList{QStringLiteral("A1"), b0, c0}));
        tardis->redoAction();
        QCOMPARE(captions(), (QStringList{QStringLiteral("A1"), QStringLiteral("B1"), QStringLiteral("C1")}));
        QCOMPARE(steps, 2);
        return;
    }
    QCOMPARE(captions(), (QStringList{QStringLiteral("A1"), QStringLiteral("B1"), c0}));
    tardis->undoAction();
    QCOMPARE(captions(), (QStringList{a0, b0, c0}));
    QCOMPARE(tardis->m_historyIndex, -1);
    QCOMPARE(steps, 2);

    // a new branch after undoing everything: one editor step, counted and undoable
    QVERIFY(ed.rig.recorder->retimeCommand(ed.show->id(), 6, 4321));
    QCOMPARE(ed.settledHistory(), 0);
    QCOMPARE(tardis->m_history.count(), 1);
    QCOMPARE(tardis->m_historyCount, 1);
    tardis->redoAction();
    QCOMPARE(captions(), (QStringList{a0, b0, c0}));
    tardis->undoAction();
    QCOMPARE(ed.command(6).time, 4000u);
    tardis->redoAction();
    QCOMPARE(ed.command(6).time, 4321u);
}

void ShowCommandRecorder_Test::recordingsEditor_workspaceReuseDropsOldEditsAndCallbacks()
{
    // no row is ever drawn from data it no longer has
    QTest::failOnWarning(QRegularExpression(QStringLiteral("ShowCommandList\\.qml.*Unable to assign")));
    EditorRig ed;
    QVERIFY(ed.setUp());
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(ed.rowIds().count(), 7);
    const quint32 originalShow = ed.show->id();
    QVERIFY(ed.doubleClick(2, "valueCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    ed.type(QStringLiteral("77"), Qt::Key_unknown);

    // a control change queued just before the workspace goes
    qInfo() << "PHASE queue old refresh";
    ed.button->setCaption(QStringLiteral("Old queued availability"));
    qInfo() << "PHASE clear";
    // the application's own reset stops and restarts its timer, which the
    // rig then stops at teardown
    ed.rig.realTimer = true;
    ed.rig.app->clearDocument();
    ed.button = nullptr;
    ed.fader = nullptr;
    ed.dimmer = nullptr;
    ed.scene = nullptr;
    qInfo() << "PHASE reuse";
    Scene *scene = new Scene(ed.rig.doc);
    scene->setName(QStringLiteral("Replacement scene"));
    QVERIFY(ed.rig.doc->addFunction(scene));
    Show *show = new Show(ed.rig.doc);
    QVERIFY(ed.rig.doc->addFunction(show));
    QCOMPARE(show->id(), originalShow);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setIntensity(2, 900, scene->id(), 0.6)));
    QVERIFY(show->setCommandTrack(track));
    ed.show = show;
    ed.rig.manager->setCurrentShowID(int(show->id()));
    QCoreApplication::processEvents();
    if (ed.item(QStringLiteral("recordingsView")) == nullptr)
        QVERIFY(ed.openRecordings());
    qInfo() << "PHASE check rows";

    // the new workspace's event 2 is shown as it is; the old edit is gone
    QTRY_COMPARE(ed.rowIds(), QVector<quint32>{2});
    QCOMPARE(EditorRig::cell(ed.row(2), "controlCell"), QStringLiteral("Replacement scene"));
    QCOMPARE(EditorRig::cell(ed.row(2), "valueCell"), QStringLiteral("60%"));
    QVERIFY(ed.editor() == nullptr);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Return);
    QCoreApplication::processEvents();
    QCOMPARE(ed.command(2).intensity, 0.6);

    // hidden, nothing is observed; shown again, rows are current
    qInfo() << "PHASE hidden";
    ed.click(ed.item(QStringLiteral("timelineTab")));
    QTRY_VERIFY(ed.item(QStringLiteral("recordingsView")) == nullptr);
    QSignalSpy refreshed(ed.rig.recorder, &ShowCommandRecorder::commandsChanged);
    auto *functions = qobject_cast<FunctionManager *>(ed.rig.context("functionManager"));
    functions->deleteFunctions({scene->id()});
    ed.settledHistory();
    ed.tardis()->undoAction();
    QTest::qWait(100);
    QCOMPARE(refreshed.count(), 0);
    QVERIFY(ed.openRecordings());
    QTRY_COMPARE(EditorRig::cell(ed.row(2), "controlCell"), QStringLiteral("Replacement scene"));
    qInfo() << "PHASE done";
}

/*********************************************************************
 * Shared debug panel
 *********************************************************************/

namespace
{
ShowEventLog::Entry diagEntry(quint64 generation, const QString &reason)
{
    ShowEventLog::Entry e;
    e.generation = generation;
    e.reason = reason;
    return e;
}

QStringList rowReasons(const ShowEventModel &model)
{
    QStringList reasons;
    for (const ShowEventLog::Entry &e : model.rows())
        reasons.append(e.reason);
    return reasons;
}
} // namespace

void ShowCommandRecorder_Test::debugPanel_capturesOnlyWhileOpen()
{
    Doc doc(nullptr, 1);
    ShowEventModel model(&doc, nullptr);
    ShowEventLog::resetStatsForTest();

    // closed from the start: nothing observes
    QVERIFY(!model.isActive());
    QCOMPARE(ShowEventLog::generation(), quint64(0));

    model.setOpen(true);
    QVERIFY(model.isActive());
    const quint64 a = ShowEventLog::generation();
    QVERIFY(a != 0);
    // a producer started under A finishes later
    const ShowEventLog::Entry lateA = diagEntry(a, QStringLiteral("A late"));
    ShowEventLog::append(diagEntry(a, QStringLiteral("A")));
    QCoreApplication::processEvents();
    QCOMPARE(rowReasons(model), QStringList{QStringLiteral("A")});

    // closing drops the rows; a producer gated on generation() builds nothing
    model.setOpen(false);
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(ShowEventLog::generation(), quint64(0));
    const quint64 offeredClosed = ShowEventLog::stats().offered;
    if (const quint64 g = ShowEventLog::generation())
        ShowEventLog::append(diagEntry(g, QStringLiteral("B")));
    QCOMPARE(ShowEventLog::stats().offered, offeredClosed);

    // reopened: a fresh observation, the late A completion is refused
    model.setOpen(true);
    const quint64 c = ShowEventLog::generation();
    QVERIFY(c != 0 && c != a);
    ShowEventLog::append(lateA);
    ShowEventLog::append(diagEntry(c, QStringLiteral("C")));
    QCoreApplication::processEvents();
    QCOMPARE(rowReasons(model), QStringList{QStringLiteral("C")});
    QCOMPARE(ShowEventLog::stats().stale, quint64(1));

    // a hidden window ends it too, and showing it again starts empty
    model.setWindowVisible(false);
    QVERIFY(!model.isActive());
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(ShowEventLog::generation(), quint64(0));
    model.setWindowVisible(true);
    QVERIFY(ShowEventLog::generation() > c);
    QCOMPARE(model.rowCount(), 0);

    // tab changes keep the observation
    const quint64 d = ShowEventLog::generation();
    ShowEventLog::append(diagEntry(d, QStringLiteral("D")));
    model.setTab(ShowEventModel::ReferencesTab);
    QCoreApplication::processEvents();
    QCOMPARE(ShowEventLog::generation(), d);
    QCOMPARE(rowReasons(model), QStringList{QStringLiteral("D")});

    // a workspace reset closes the panel, and keeps a posted wake the only one
    ShowEventLog::append(diagEntry(d, QStringLiteral("pending")));
    const quint64 wakes = ShowEventLog::stats().wakesPosted;
    doc.clearContents();
    QVERIFY(!model.isOpen());
    QCOMPARE(ShowEventLog::generation(), quint64(0));
    QCOMPARE(model.rowCount(), 0);
    model.setOpen(true);
    ShowEventLog::append(diagEntry(ShowEventLog::generation(), QStringLiteral("after reset")));
    QCOMPARE(ShowEventLog::stats().wakesPosted, wakes);
    QCoreApplication::processEvents();
    QCOMPARE(rowReasons(model), QStringList{QStringLiteral("after reset")});
    model.setOpen(false);
}

void ShowCommandRecorder_Test::debugPanel_boundsHistoryAndWakes()
{
    Doc doc(nullptr, 1);
    ShowEventModel model(&doc, nullptr);
    QCoreApplication::processEvents();
    ShowEventLog::resetStatsForTest();

    // the GUI stalls while producers outrun it, across rapid reopen cycles
    model.setOpen(true);
    for (int cycle = 0; cycle < 5; cycle++)
    {
        const quint64 g = ShowEventLog::generation();
        for (int i = 0; i < 2600 + cycle; i++)
            ShowEventLog::append(diagEntry(g, QString::number(i)));
        if (cycle < 4)
        {
            model.setOpen(false);
            model.setOpen(true);
        }
    }
    QCOMPARE(ShowEventLog::stats().wakesPosted, quint64(1));

    // one drain presents the current observation only, bounded, with its evictions
    QCoreApplication::processEvents();
    QCOMPARE(model.rowCount(), ShowEventLog::Capacity);
    QCOMPARE(model.rows().first().reason, QString::number(604));
    QCOMPARE(model.rows().last().reason, QString::number(2603));
    QCOMPARE(model.evicted(), 604.0);
    QCOMPARE(model.dropped(), 0.0);
    for (int i = 1; i < model.rowCount(); i++)
        QVERIFY(model.rows().at(i).seq > model.rows().at(i - 1).seq);

    // a later burst rolls the presentation without growing it
    const quint64 g = ShowEventLog::generation();
    for (int i = 0; i < 10; i++)
        ShowEventLog::append(diagEntry(g, QStringLiteral("late %1").arg(i)));
    QCOMPARE(ShowEventLog::stats().wakesPosted, quint64(2));
    QCoreApplication::processEvents();
    QCOMPARE(model.rowCount(), ShowEventLog::Capacity);
    QCOMPARE(model.rows().last().reason, QStringLiteral("late 9"));
    QCOMPARE(model.evicted(), 614.0);
    QCOMPARE(model.rowOfSeq(model.rows().first().seq), 0);
    QCOMPARE(model.rowOfSeq(model.rows().first().seq - 1), -1);

    // texts are bounded as they are appended
    ShowEventLog::Entry big = diagEntry(g, QString(1000, QLatin1Char('r')));
    big.control = QString(1000, QLatin1Char('c'));
    ShowEventLog::append(big);
    QCoreApplication::processEvents();
    QCOMPARE(model.rows().last().reason.size(), ShowEventLog::ReasonLength);
    QCOMPARE(model.rows().last().control.size(), ShowEventLog::LabelLength);
    model.setOpen(false);
}

namespace
{
/** Everything a mixed recording and replay session leaves behind */
struct FlowRun
{
    QVector<ShowCommand> commands;
    QVector<quint32> orders;
    quint64 revision = 0;
    int buttonState = -1;
    int faderValue = -1;
    bool aRunning = false;
    bool bRunning = false;
    qreal bIntensity = -1;
    QVector<ShowEventLog::Entry> rows;
    ShowEventLog::Summary summary;
    quint64 offered = 0;
    QUuid missing;
    quint32 bId = Function::invalidId();
};

/** REC off, recorded button/slider input at repeated and rewound Show
 *  times, not recorded origins and duplicates, an unsupported Flash, a
 *  refused edit, then a replay mixing VC state, a missing control and
 *  legacy commands */
FlowRun runFlow(bool open)
{
    FlowRun run;
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    ShowEventModel model(&doc, nullptr);
    ShowEventLog::resetSummary();
    QCoreApplication::processEvents();
    ShowEventLog::resetStatsForTest();
    model.setOpen(open);

    Scene *a = addTarget(&doc);
    Scene *b = new Scene(&doc);
    b->setName(QStringLiteral("B"));
    b->setValue(SceneValue(a->values().first().fxi, 1, 120));
    doc.addFunction(b);
    run.bId = b->id();
    Scene *idle = new Scene(&doc);
    idle->setValue(SceneValue(a->values().first().fxi, 2, 90));
    doc.addFunction(idle);
    Show *take = addRecordingShow(&doc, kRecordPosition);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frameID, a->id(), 10, QStringLiteral("Button A"));
    VCSlider *fader = addAdjustSlider(bridge, ui.vc(), frameID, b->id());
    VCButton *flash = qobject_cast<VCButton *>(ui.vc()->widget(
            bridge.addButton(frameID, QRect(300, 10, 80, 40), a->id(), QStringLiteral("Flash"),
                             QStringLiteral("flash"))));

    // REC off: live only
    button->requestUserStateChange(true, ShowCommandOrigin::Pointer);
    tickAndDeliver(&doc, 2);

    recorder.setResolvedShow(take->id());
    recorder.setRecording(true);
    button->requestUserStateChange(true, ShowCommandOrigin::Pointer);
    fader->requestUserValue(153, true, ShowCommandOrigin::Midi);
    fader->requestUserValue(153, true, ShowCommandOrigin::Pointer);
    fader->requestUserValue(60, true, ShowCommandOrigin::Audio);
    fader->requestUserValue(90, true, ShowCommandOrigin::Programmatic);
    flash->requestUserStateChange(true, ShowCommandOrigin::Pointer);
    flash->requestUserStateChange(false, ShowCommandOrigin::Pointer);
    tickAndDeliver(&doc, 2);
    take->setExternalElapsedTime(5000);
    button->requestUserStateChange(true, ShowCommandOrigin::Keyboard);
    // rewound: authored earlier on the Show, arriving later
    take->setExternalElapsedTime(3000);
    button->requestUserStateChange(true, ShowCommandOrigin::Osc);
    recorder.retimeCommand(take->id(), 0, 100);
    tickAndDeliver(&doc, 2);
    recorder.setRecording(false);

    // replay: tied VC states, a control nobody carries, legacy commands
    Show *replay = new Show(&doc);
    replay->setName(QStringLiteral("Replay"));
    doc.addFunction(replay);
    run.missing = QUuid::createUuid();
    ShowCommandTrack track;
    track.insert(ShowCommand::setButtonState(0, 20, button->ensureRecordingId(), true));
    track.insert(ShowCommand::setSliderPosition(1, 20, fader->ensureRecordingId(),
                                                ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), 0.25));
    track.insert(ShowCommand::setButtonState(2, 40, run.missing, true));
    track.insert(ShowCommand::start(3, 60, b->id()));
    track.insert(ShowCommand::setIntensity(4, 60, b->id(), 0.5));
    track.insert(ShowCommand::setIntensity(5, 60, idle->id(), 0.3));
    track.setExtent(400);
    replay->setCommandTrack(track);
    replay->start(doc.masterTimer(), FunctionParent::master());
    for (int i = 0; i < 40; i++)
        tickAndDeliver(&doc);

    // control identities are random per run; the order and value data are compared
    run.commands = take->commandTrack().commands();
    for (ShowCommand &cmd : run.commands)
    {
        cmd.controlId = QUuid();
        run.orders.append(cmd.order);
    }
    run.revision = take->commandTrackRevision();
    run.buttonState = int(button->state());
    run.faderValue = fader->value();
    run.aRunning = a->isRunning();
    run.bRunning = b->isRunning();
    run.bIntensity = b->getAttributeValue(Function::Intensity);
    QCoreApplication::processEvents();
    run.rows = model.rows();
    run.summary = ShowEventLog::summary();
    run.offered = ShowEventLog::stats().offered;

    replay->stop(FunctionParent::master());
    tickAndDeliver(&doc, 3);
    for (Function *function : doc.functions())
        function->stop(FunctionParent::master());
    tickAndDeliver(&doc, 3);
    model.setOpen(false);
    return run;
}

int findRow(const QVector<ShowEventLog::Entry> &rows, ShowEventLog::Phase phase, ShowEventLog::Outcome outcome,
            const QString &control, int from = 0)
{
    for (int i = from; i < rows.count(); i++)
    {
        const ShowEventLog::Entry &e = rows.at(i);
        if (e.phase == phase && e.outcome == outcome && (control.isEmpty() || e.control == control))
            return i;
    }
    return -1;
}
} // namespace

void ShowCommandRecorder_Test::debugPanel_tracesFlowDecisionsAndOutcomes()
{
    using P = ShowEventLog::Phase;
    using O = ShowEventLog::Outcome;
    const FlowRun run = runFlow(true);
    const QVector<ShowEventLog::Entry> &rows = run.rows;
    const QString button = QStringLiteral("Button A"), fader = QStringLiteral("Fader");
    for (int i = 1; i < rows.count(); i++)
        QVERIFY(rows.at(i).seq > rows.at(i - 1).seq);

    // REC off: seen with its decision, executed live, nothing committed
    const int off = findRow(rows, P::Decision, O::Ignored, button);
    QVERIFY(off >= 0);
    QCOMPARE(rows.at(off).origin, int(ShowCommandOrigin::Pointer));
    QVERIFY(rows.at(off).reason.contains(QLatin1String("REC")));
    const int offLive = findRow(rows, P::Execute, O::Requested, button, off);
    QVERIFY(offLive > off);
    QCOMPARE(rows.at(offLive).trace, rows.at(off).trace);
    QVERIFY(findRow(rows, P::Transport, O::Applied, QString(), off) > offLive);

    // recorded input: decision, then the Show's commit, then its execution, one trace
    const int rec = findRow(rows, P::Decision, O::Recorded, button);
    QVERIFY(rec > off);
    QCOMPARE(rows.at(rec).showTimeMs, qint64(kRecordPosition));
    QCOMPARE(rows.at(rec).value, QStringLiteral("Off"));
    const int pub = findRow(rows, P::Commit, O::Published, QString(), rec);
    QVERIFY(pub > rec);
    QCOMPARE(rows.at(pub).trace, rows.at(rec).trace);
    QCOMPARE(rows.at(pub).commandId, 0u);
    QVERIFY(findRow(rows, P::Execute, O::Requested, button, pub) > pub);

    // the same Show time: its own command, after the first
    const int midi = findRow(rows, P::Decision, O::Recorded, fader);
    QVERIFY(midi > pub);
    QCOMPARE(rows.at(midi).origin, int(ShowCommandOrigin::Midi));
    QCOMPARE(rows.at(midi).showTimeMs, qint64(kRecordPosition));
    const int midiPub = findRow(rows, P::Commit, O::Published, QString(), midi);
    QCOMPARE(rows.at(midiPub).commandId, 1u);
    QCOMPARE(rows.at(midiPub).trace, rows.at(midi).trace);
    QVERIFY(rows.at(midi).trace != rows.at(rec).trace);

    // not recorded, each with its reason, before the recorder's filters hid it
    const int dup = findRow(rows, P::Decision, O::Ignored, fader, midi);
    QCOMPARE(rows.at(dup).origin, int(ShowCommandOrigin::Pointer));
    const int audio = findRow(rows, P::Decision, O::Ignored, fader, dup + 1);
    QCOMPARE(rows.at(audio).origin, int(ShowCommandOrigin::Audio));
    const int script = findRow(rows, P::Decision, O::Ignored, fader, audio + 1);
    QCOMPARE(rows.at(script).origin, int(ShowCommandOrigin::Programmatic));
    QVERIFY(rows.at(dup).reason != rows.at(audio).reason);
    const int flash = findRow(rows, P::Decision, O::Unsupported, QStringLiteral("Flash"), script);
    QVERIFY(flash > script);

    // later Show time, then a rewind: arrival order, not Show time
    const int later = findRow(rows, P::Decision, O::Recorded, button, flash);
    const int rewound = findRow(rows, P::Decision, O::Recorded, button, later + 1);
    QCOMPARE(rows.at(later).showTimeMs, qint64(5000));
    QCOMPARE(rows.at(later).origin, int(ShowCommandOrigin::Keyboard));
    QCOMPARE(rows.at(rewound).showTimeMs, qint64(3000));
    QCOMPARE(rows.at(rewound).origin, int(ShowCommandOrigin::Osc));
    QVERIFY(rows.at(rewound).take != rows.at(later).take);

    // a refused edit changes nothing
    const int refused = findRow(rows, P::Commit, O::Rejected, QString(), rewound);
    QVERIFY(refused > rewound);
    QCOMPARE(rows.at(refused).showId, rows.at(rec).showId);

    // replay: requests, not proof of output; the missing control is skipped
    const int replayOn = findRow(rows, P::Execute, O::Requested, button, refused);
    QVERIFY(replayOn > refused);
    QCOMPARE(rows.at(replayOn).origin, int(ShowCommandOrigin::Replay));
    QCOMPARE(rows.at(replayOn).commandId, 0u);
    const int replayFader = findRow(rows, P::Execute, O::Requested, fader, replayOn);
    QVERIFY(replayFader > replayOn);
    QCOMPARE(rows.at(replayFader).traversal, rows.at(replayOn).traversal);
    const int missing = findRow(rows, P::Execute, O::Skipped, QString(), replayFader);
    QVERIFY(missing > replayFader);
    QCOMPARE(rows.at(missing).controlId, run.missing);
    QVERIFY(rows.at(missing).reason.contains(QLatin1String("no control")));
    const int legacyStart = findRow(rows, P::Execute, O::Requested, QString(), missing);
    QCOMPARE(rows.at(legacyStart).functionId, run.bId);
    QCOMPARE(rows.at(legacyStart).commandId, 3u);
    const int applied = findRow(rows, P::Execute, O::Applied, QString(), legacyStart);
    QCOMPARE(rows.at(applied).commandId, 4u);
    const int inactive = findRow(rows, P::Execute, O::Skipped, QString(), legacyStart);
    QCOMPARE(rows.at(inactive).commandId, 5u);
    QVERIFY(rows.at(inactive).reason.contains(QLatin1String("not running")));
    // an unchanged live value is not reported as a change
    QCOMPARE(rows.at(findRow(rows, P::Execute, O::Skipped, fader, dup)).trace, rows.at(dup).trace);

    // failed operations only: Flash press and release, the refused edit, the missing control
    QCOMPARE(run.summary.count, quint64(4));
    QVERIFY(run.summary.reason.contains(QLatin1String("no control")));
    QCOMPARE(run.summary.view, ShowEventLog::View::References);
}

void ShowCommandRecorder_Test::debugPanel_leavesRecordingAndReplayUnchanged()
{
    const FlowRun closed = runFlow(false);
    const FlowRun open = runFlow(true);

    // closed, no producer built an entry
    QCOMPARE(closed.offered, quint64(0));
    QVERIFY(closed.rows.isEmpty());
    QVERIFY(open.rows.count() > 20);

    QCOMPARE(open.commands.count(), 4);
    QCOMPARE(open.commands, closed.commands);
    QCOMPARE(open.orders, closed.orders);
    QCOMPARE(open.revision, closed.revision);
    QCOMPARE(open.buttonState, closed.buttonState);
    QCOMPARE(open.faderValue, closed.faderValue);
    QCOMPARE(open.aRunning, closed.aRunning);
    QCOMPARE(open.bRunning, closed.bRunning);
    QCOMPARE(open.bIntensity, closed.bIntensity);
    QVERIFY(open.bRunning);
    // the summary does not depend on the panel
    QCOMPARE(open.summary.count, closed.summary.count);
    QCOMPARE(QString(open.summary.reason).remove(open.missing.toString(QUuid::WithoutBraces)),
             QString(closed.summary.reason).remove(closed.missing.toString(QUuid::WithoutBraces)));
}

void ShowCommandRecorder_Test::debugPanel_deferredOutcomeStaysInItsObservation_data()
{
    QTest::addColumn<bool>("openAtAcceptance");
    QTest::addColumn<bool>("reopenWhileWaiting");

    QTest::newRow("same observation") << true << false;
    QTest::newRow("reopened while it waits") << true << true;
    QTest::newRow("opened while it waits") << false << true;
}

void ShowCommandRecorder_Test::debugPanel_deferredOutcomeStaysInItsObservation()
{
    QFETCH(bool, openAtAcceptance);
    QFETCH(bool, reopenWhileWaiting);
    using P = ShowEventLog::Phase;
    using O = ShowEventLog::Outcome;

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    ShowEventModel model(&doc, nullptr);
    MasterTimer *timer = doc.masterTimer();
    Scene *scene = addTarget(&doc);
    const quint32 fixtureID = scene->values().first().fxi;
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *replayed = addToggle(bridge, ui.vc(), frameID, scene->id(), 10, QStringLiteral("Replayed"));
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    slider->setSliderMode(VCSlider::Level);
    slider->addLevelChannel(fixtureID, 3);
    slider->setCaption(QStringLiteral("Level"));

    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, replayed->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setSliderPosition(1, 110, slider->ensureRecordingId(),
                                                        ShowControlRole::LevelSlider, QString(), 0.3)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(timer, FunctionParent::master());
    for (int i = 0; i < 20 && batches.isEmpty(); i++)
        timer->timerTick();
    timer->timerTick();
    show->setPause(true);

    model.setOpen(openAtAcceptance);
    const quint64 accepted = ShowEventLog::generation();
    slider->requestUserValue(140);
    QCOMPARE(recorder.pendingUserRequests().count(), 1);
    QCOMPARE(recorder.pendingUserRequests().first().cause.generation, accepted);
    if (reopenWhileWaiting)
    {
        model.setOpen(false);
        model.setOpen(true);
    }
    const quint64 completed = ShowEventLog::generation();

    for (int i = 0; i < 20 && (!recorder.pendingUserRequests().isEmpty() ||
                               recorder.pendingControlRuns() > 0 || show->commandWorkPending()); i++)
        tickAndDeliver(&doc);
    QCoreApplication::processEvents();

    // native behavior does not depend on the panel
    QCOMPARE(slider->value(), 140);
    QCOMPARE(replayed->state(), VCButton::Active);
    for (const ShowEventLog::Entry &e : model.rows())
        QCOMPARE(e.generation, completed);

    const QVector<ShowEventLog::Entry> &rows = model.rows();
    const int queued = findRow(rows, P::Execute, O::Queued, QStringLiteral("Level"));
    const int applied = findRow(rows, P::Execute, O::Applied, QStringLiteral("Level"));
    if (!reopenWhileWaiting)
    {
        QVERIFY(queued >= 0 && applied > queued);
        QCOMPARE(rows.at(applied).trace, rows.at(queued).trace);
        QCOMPARE(rows.at(applied).showTimeMs, rows.at(queued).showTimeMs);
    }
    else
    {
        // the later observation shows replay it saw, never the earlier input's completion
        QCOMPARE(queued, -1);
        QCOMPARE(applied, -1);
        QVERIFY(findRow(rows, P::Execute, O::Requested, QStringLiteral("Level")) >= 0);
    }

    show->stop(FunctionParent::master());
    tickAndDeliver(&doc, 3);
    model.setOpen(false);
}

namespace
{
/** Gives a widget a recorded identity as a loaded workspace would */
struct RecordingIdLoader : VCWidget
{
    using VCWidget::loadXMLRecordingId;
};

bool loadRecordingId(VCWidget *widget, const QUuid &id)
{
    QXmlStreamReader reader(QStringLiteral("<Button RecordingID=\"%1\"/>").arg(id.toString()));
    reader.readNextStartElement();
    bool (VCWidget::*load)(QXmlStreamReader &) = &RecordingIdLoader::loadXMLRecordingId;
    return (widget->*load)(reader);
}

QVariantMap referenceRow(const QVariantList &rows, const QUuid &id)
{
    for (const QVariant &row : rows)
    {
        if (row.toMap().value("controlId").toString() == id.toString(QUuid::WithoutBraces))
            return row.toMap();
    }
    return QVariantMap();
}

/** Status of the row's role, e.g. "LevelSlider" */
QString roleStatus(const QVariantMap &row, const QString &role)
{
    for (const QVariant &entry : row.value("roles").toList())
    {
        if (entry.toMap().value("role").toString() == role)
            return entry.toMap().value("status").toString();
    }
    return QString();
}
} // namespace

void ShowCommandRecorder_Test::debugPanel_referencedControlsUseReplayResolution()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    ShowEventModel model(&doc, &recorder);
    Scene *s1 = addTarget(&doc);
    const quint32 fixtureID = s1->values().first().fxi;
    Scene *s2 = new Scene(&doc);
    s2->setValue(SceneValue(fixtureID, 1, 90));
    QVERIFY(doc.addFunction(s2));
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    recorder.setResolvedShow(show->id());

    VCBridgeV5 bridge(&doc, ui.vc());
    const int frameID = bridge.addFrame(0, QRect(0, 0, 800, 300), QStringLiteral("Frame"), false);
    VCButton *a = addToggle(bridge, ui.vc(), frameID, s1->id(), 10, QStringLiteral("Button A"));
    // the same caption, another identity
    VCButton *a2 = addToggle(bridge, ui.vc(), frameID, s2->id(), 100, QStringLiteral("Button A"));
    VCButton *off = addToggle(bridge, ui.vc(), frameID, s2->id(), 190, QStringLiteral("Off button"));
    off->setDisabled(true);
    VCButton *twin1 = addToggle(bridge, ui.vc(), frameID, s1->id(), 280, QStringLiteral("Twin 1"));
    VCButton *twin2 = addToggle(bridge, ui.vc(), frameID, s2->id(), 370, QStringLiteral("Twin 2"));
    const QUuid twin = QUuid::createUuid();
    QVERIFY(loadRecordingId(twin1, twin) && loadRecordingId(twin2, twin));
    VCSlider *level = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    level->setSliderMode(VCSlider::Level);
    level->addLevelChannel(fixtureID, 3);
    level->setCaption(QStringLiteral("Level"));
    VCSlider *gm = addAdjustSlider(bridge, ui.vc(), frameID, Function::invalidId());
    gm->setSliderMode(VCSlider::GrandMaster);
    gm->setCaption(QStringLiteral("GM"));
    VCSlider *fader = addAdjustSlider(bridge, ui.vc(), frameID, s2->id());
    fader->setCaption(QStringLiteral("Fader"));
    const QUuid missing = QUuid::createUuid();

    // unequal counts, repeated identities in two roles, a legacy command naming no control
    ShowCommandTrack track;
    quint32 id = 0;
    const auto button = [&](quint32 time, const QUuid &control, bool on)
    { QVERIFY(track.insert(ShowCommand::setButtonState(id++, time, control, on))); };
    const auto slider = [&](quint32 time, const QUuid &control, ShowControlRole role, const QString &attribute)
    { QVERIFY(track.insert(ShowCommand::setSliderPosition(id++, time, control, role, attribute, 0.5))); };
    button(10, a->ensureRecordingId(), true);
    button(12, a->recordingId(), false);
    button(14, a->recordingId(), true);
    button(10, a2->ensureRecordingId(), true);
    button(16, off->ensureRecordingId(), true);
    button(18, twin, true);
    button(20, missing, true);
    button(22, missing, false);
    slider(24, level->ensureRecordingId(), ShowControlRole::LevelSlider, QString());
    slider(26, gm->ensureRecordingId(), ShowControlRole::GrandMasterSlider, QString());
    slider(28, fader->ensureRecordingId(), ShowControlRole::AdjustSlider, QStringLiteral("Intensity"));
    slider(30, fader->recordingId(), ShowControlRole::AdjustSlider, QStringLiteral("Intensity"));
    slider(32, fader->recordingId(), ShowControlRole::LevelSlider, QString());
    QVERIFY(track.insert(ShowCommand::start(id++, 34, s1->id())));
    QVERIFY(track.setExtent(400));
    QVERIFY(show->setCommandTrack(track));
    const ShowCommandTrack authored = show->commandTrack();

    // opened, with a log far past its capacity: the inventory is the whole Show
    model.setOpen(true);
    for (int i = 0; i < ShowEventLog::Capacity + 100; i++)
        ShowEventLog::append(diagEntry(ShowEventLog::generation(), QString::number(i)));
    QCoreApplication::processEvents();
    QVERIFY(model.evicted() >= 100);

    QVariantList rows = recorder.referencedControls();
    QCOMPARE(rows.count(), 8);
    QStringList order;
    for (const QVariant &row : rows)
        order.append(row.toMap().value("status").toString());
    QCOMPARE(order, QStringList({"Missing", "Ambiguous", "Incompatible", "Disabled", "Ready", "Ready", "Ready", "Ready"}));

    const QVariantMap rowA = referenceRow(rows, a->recordingId());
    QCOMPARE(rowA.value("count").toInt(), 3);
    QCOMPARE(rowA.value("caption").toString(), QStringLiteral("Button A"));
    QVERIFY(rowA.value("binding").toString().contains(QStringLiteral("Recorded target")));
    QCOMPARE(referenceRow(rows, a2->recordingId()).value("count").toInt(), 1);
    QCOMPARE(referenceRow(rows, missing).value("count").toInt(), 2);
    QCOMPARE(referenceRow(rows, twin).value("status").toString(), QStringLiteral("Ambiguous"));
    // Level and Grand Master need no Function
    QCOMPARE(referenceRow(rows, level->recordingId()).value("status").toString(), QStringLiteral("Ready"));
    QCOMPARE(referenceRow(rows, gm->recordingId()).value("status").toString(), QStringLiteral("Ready"));
    // both roles of one identity, each with its own status and count
    const QVariantMap rowFader = referenceRow(rows, fader->recordingId());
    QCOMPARE(rowFader.value("count").toInt(), 3);
    QCOMPARE(roleStatus(rowFader, "AdjustSlider"), QStringLiteral("Ready"));
    QCOMPARE(roleStatus(rowFader, "LevelSlider"), QStringLiteral("Incompatible"));
    QCOMPARE(rowFader.value("roles").toList().count(), 2);

    // replay reaches the same verdict for every one it cannot use
    show->start(doc.masterTimer(), FunctionParent::master());
    for (int i = 0; i < 20; i++)
        tickAndDeliver(&doc);
    show->stop(FunctionParent::master());
    tickAndDeliver(&doc, 3);
    QStringList skipped;
    for (const ShowEventLog::Entry &e : model.rows())
    {
        // unresolved, not merely already in the recorded state
        if (e.phase == ShowEventLog::Phase::Execute && e.outcome == ShowEventLog::Outcome::Skipped &&
            e.widgetId == ShowEventLog::NoId && !e.controlId.isNull())
            skipped.append(e.controlId.toString(QUuid::WithoutBraces));
    }
    QStringList problems;
    for (const QVariant &row : rows)
    {
        for (const QVariant &role : row.toMap().value("roles").toList())
        {
            if (role.toMap().value("status").toString() != QStringLiteral("Ready"))
            {
                for (int n = 0; n < role.toMap().value("count").toInt(); n++)
                    problems.append(row.toMap().value("controlId").toString());
            }
        }
    }
    skipped.sort();
    problems.sort();
    QCOMPARE(skipped, problems);
    QCOMPARE(show->commandTrack().commands(), authored.commands());

    // native changes while open, gestures untouched
    QSignalSpy refreshed(&recorder, &ShowCommandRecorder::referencesChanged);
    a->setCaption(QStringLiteral("Renamed A"));
    QTRY_COMPARE(referenceRow(recorder.referencedControls(), a->recordingId()).value("caption").toString(),
                 QStringLiteral("Renamed A"));
    level->setDisabled(true);
    QTRY_COMPARE(referenceRow(recorder.referencedControls(), level->recordingId()).value("status").toString(),
                 QStringLiteral("Disabled"));
    fader->setSliderMode(VCSlider::Level);
    QTRY_COMPARE(roleStatus(referenceRow(recorder.referencedControls(), fader->recordingId()), "LevelSlider"),
                 QStringLiteral("Ready"));
    QCOMPARE(roleStatus(referenceRow(recorder.referencedControls(), fader->recordingId()), "AdjustSlider"),
             QStringLiteral("Incompatible"));
    ShowCommandTrack grown = show->commandTrack();
    QVERIFY(grown.insert(ShowCommand::setButtonState(grown.nextEventId(), 40, missing, true)));
    const int beforeTrack = refreshed.count();
    QVERIFY(show->setCommandTrack(grown));
    QTRY_VERIFY(refreshed.count() > beforeTrack);
    QCOMPARE(referenceRow(recorder.referencedControls(), missing).value("count").toInt(), 3);
    QCOMPARE(show->commandTrack().count(), authored.count() + 1);
    const int beforeRemoval = refreshed.count();
    QVERIFY(doc.deleteFunction(s1->id()));
    QTRY_VERIFY(refreshed.count() > beforeRemoval);
    QCOMPARE(referenceRow(recorder.referencedControls(), a->recordingId()).value("status").toString(),
             QStringLiteral("Unbound"));

    // closed: nothing is followed; reopened: current configuration
    model.setOpen(false);
    QCoreApplication::processEvents();
    const int closedAt = refreshed.count();
    a2->setCaption(QStringLiteral("Changed while closed"));
    off->setDisabled(false);
    QTest::qWait(50);
    QCOMPARE(refreshed.count(), closedAt);
    model.setOpen(true);
    QTRY_VERIFY(refreshed.count() > closedAt);
    QCOMPARE(referenceRow(recorder.referencedControls(), a2->recordingId()).value("caption").toString(),
             QStringLiteral("Changed while closed"));
    QCOMPARE(referenceRow(recorder.referencedControls(), off->recordingId()).value("status").toString(),
             QStringLiteral("Ready"));
    model.setOpen(false);
}

namespace
{
const QString kDiagArtifacts = QString::fromLocal8Bit(qgetenv("QLC_DIAG_ARTIFACTS"));

/** The visible delegates of a list, top to bottom */
QList<QQuickItem *> shownRows(QQuickItem *list, const QString &name)
{
    QList<QQuickItem *> rows;
    if (list == nullptr)
        return rows;
    const QRectF bounds = list->mapRectToScene(QRectF(0, 0, list->width(), list->height()));
    QList<QQuickItem *> pending{list};
    while (!pending.isEmpty())
    {
        QQuickItem *row = pending.takeFirst();
        if (row->objectName() != name)
        {
            pending += row->childItems();
            continue;
        }
        const QRectF r = row->mapRectToScene(QRectF(0, 0, row->width(), row->height()));
        if (row->isVisible() && bounds.contains(r.center()))
            rows.append(row);
    }
    std::sort(rows.begin(), rows.end(), [](QQuickItem *a, QQuickItem *b)
              { return a->mapToScene(QPointF()).y() < b->mapToScene(QPointF()).y(); });
    return rows;
}

void wheelUp(QQuickWindow *window, QQuickItem *item, int notches)
{
    const QPointF at = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
    for (int i = 0; i < notches; i++)
    {
        QWheelEvent wheel(at, window->mapToGlobal(at), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(window, &wheel);
        QCoreApplication::processEvents();
    }
}
} // namespace

void ShowCommandRecorder_Test::debugPanel_appLaunchersShareOnePanel_data()
{
    QTest::addColumn<QString>("context");
    QTest::addColumn<QString>("resource");
    QTest::newRow("show editor") << QStringLiteral("SHOWMGR") << QStringLiteral("qrc:/ShowManager.qml");
    QTest::newRow("dj") << QStringLiteral("DJMGR") << QStringLiteral("qrc:/DjManager.qml");
    QTest::newRow("virtual console") << QStringLiteral("VC") << QStringLiteral("qrc:/VirtualConsole.qml");
}

void ShowCommandRecorder_Test::debugPanel_appLaunchersShareOnePanel()
{
    QFETCH(QString, context);
    QFETCH(QString, resource);
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto *events = qobject_cast<ShowEventModel *>(ed.rig.context("showEvents"));
    QVERIFY(events);
    QVERIFY(!events->isOpen());
    QVERIFY(QMetaObject::invokeMethod(ed.root(), "switchToContext", Q_ARG(QVariant, context), Q_ARG(QVariant, resource)));
    QTRY_VERIFY(ed.item(QStringLiteral("recordDebugButton")) != nullptr);

    ed.click(ed.item(QStringLiteral("recordDebugButton")));
    QTRY_VERIFY(ed.item(QStringLiteral("showDebugPanel")) != nullptr);
    QVERIFY(events->isActive());
    // the one MainView panel, docked below the view, which stays usable
    QCOMPARE(ed.root()->findChildren<QQuickItem *>(QStringLiteral("showDebugPanel")).count(), 1);
    QQuickItem *panel = ed.item(QStringLiteral("showDebugPanel"));
    QCOMPARE(panel->mapToScene(QPointF(0, panel->height())).y(), ed.root()->height());
    QQuickItem *rec = ed.item(QStringLiteral("recordSwitch"));
    QVERIFY(rec);
    QVERIFY(rec->mapToScene(QPointF(0, rec->height())).y() <= panel->mapToScene(QPointF()).y());

    // observed from the moment it opened, in either tab
    ed.click(rec);
    QVERIFY(ed.rig.recorder->isRecording());
    QTRY_VERIFY(events->count() > 0);
    const quint64 observation = ShowEventLog::generation();
    const int before = events->count();
    ed.click(ed.item(QStringLiteral("referencesTab")));
    QCOMPARE(events->tab(), int(ShowEventModel::ReferencesTab));
    QCOMPARE(ShowEventLog::generation(), observation);
    QTRY_COMPARE(ed.item(QStringLiteral("referencesList"))->property("count").toInt(),
                 ed.rig.recorder->referencedControls().count());
    QVERIFY(ed.rig.recorder->referencedControls().count() == 4);
    ed.click(ed.item(QStringLiteral("recordSwitch")));
    QVERIFY(!ed.rig.recorder->isRecording());
    QTRY_VERIFY(events->count() > before);
    QCOMPARE(ShowEventLog::generation(), observation);

    // closing clears; what happens while closed is never shown later
    ed.click(ed.item(QStringLiteral("debugClose")));
    QTRY_VERIFY(ed.item(QStringLiteral("showDebugPanel")) == nullptr);
    QVERIFY(!events->isActive());
    QCOMPARE(events->count(), 0);
    QCOMPARE(ShowEventLog::generation(), quint64(0));
    const quint64 offered = ShowEventLog::stats().offered;
    ed.click(ed.item(QStringLiteral("recordSwitch")));
    QVERIFY(ed.rig.recorder->isRecording());
    QCOMPARE(ShowEventLog::stats().offered, offered);
    ed.click(ed.item(QStringLiteral("recordDebugButton")));
    QTRY_VERIFY(events->isActive());
    QVERIFY(ShowEventLog::generation() > observation);
    QCoreApplication::processEvents();
    QCOMPARE(events->count(), 0);
    // it opens on the tab it had
    QCOMPARE(events->tab(), int(ShowEventModel::ReferencesTab));
    QVERIFY(ed.rig.recorder->setRecording(false));
    events->setOpen(false);
}

void ShowCommandRecorder_Test::debugPanel_appReadingPositionAndProblems()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto *events = qobject_cast<ShowEventModel *>(ed.rig.context("showEvents"));
    auto *vc = qobject_cast<VirtualConsole *>(ed.rig.context("virtualConsole"));
    QVERIFY(events && vc);
    VCBridgeV5 bridge(ed.rig.doc, vc);
    const int frame = bridge.addFrame(0, QRect(0, 320, 300, 100), QStringLiteral("Flash frame"), false);
    auto *flash = qobject_cast<VCButton *>(vc->widget(bridge.addButton(frame, QRect(10, 10, 80, 40), ed.scene->id(),
                                                                       QStringLiteral("Flash"), QStringLiteral("flash"))));
    QVERIFY(flash);
    QVERIFY(ed.rig.recorder->setRecording(true));
    QVERIFY(ed.rig.recorder->isAuthoring());
    const ShowCommandTrack authored = ed.show->commandTrack();
    const quint64 revision = ed.show->commandTrackRevision();

    // closed: two failures become a count and a reason, no event history
    const quint64 offered = ShowEventLog::stats().offered;
    flash->requestUserStateChange(true, ShowCommandOrigin::Pointer);
    flash->requestUserStateChange(false, ShowCommandOrigin::Pointer);
    QTRY_VERIFY(ed.item(QStringLiteral("problemsIndicator")) != nullptr);
    QCOMPARE(ed.item(QStringLiteral("problemsIndicator"))->property("text").toString(), QStringLiteral("Problems: 2"));
    QVERIFY(ed.item(QStringLiteral("problemsReason"))->property("text").toString().contains(QLatin1String("Flash")));
    QCOMPARE(ShowEventLog::stats().offered, offered);
    QCOMPARE(ShowEventLog::generation(), quint64(0));

    // the indicator opens the panel on its view; details begin now
    ed.click(ed.item(QStringLiteral("problemsIndicator")));
    QTRY_VERIFY(events->isActive());
    QCOMPARE(events->tab(), int(ShowEventModel::EventsTab));
    QCoreApplication::processEvents();
    QCOMPARE(events->count(), 0);
    QTRY_VERIFY(ed.item(QStringLiteral("problemsIndicator")) == nullptr);

    const auto press = [&](int times)
    {
        for (int i = 0; i < times; i++)
            flash->requestUserStateChange(i % 2 == 0, ShowCommandOrigin::Pointer);
        QCoreApplication::processEvents();
    };
    press(40);
    QTRY_COMPARE(events->count(), 40);
    QQuickItem *panel = ed.item(QStringLiteral("showDebugPanel"));
    QQuickItem *list = ed.item(QStringLiteral("eventsList"));
    QVERIFY(panel && list);
    QTRY_VERIFY(list->property("atYEnd").toBool());

    // reading older rows: a real wheel scroll stops following
    wheelUp(ed.rig.app.get(), list, 20);
    QTRY_VERIFY(!list->property("atYEnd").toBool());
    QVERIFY(!panel->property("followTail").toBool());
    // a press while it still moves only stops it
    QTRY_VERIFY(!list->property("moving").toBool());
    const QList<QQuickItem *> visible = shownRows(list, QStringLiteral("eventRow"));
    QVERIFY(!visible.isEmpty());
    ed.click(visible.first());
    const qreal selected = visible.first()->property("rowSeq").toReal();
    QCOMPARE(panel->property("selectedSeq").toReal(), selected);
    QVERIFY(ed.item(QStringLiteral("eventDetails"))->property("text").toString()
                .contains(QStringLiteral("Event: %1").arg(selected)));
    list->forceActiveFocus();
    QTRY_VERIFY(list->hasActiveFocus());
    const qreal contentY = list->property("contentY").toReal();

    // new rows neither move the reader nor take the selection or focus
    press(40);
    QTRY_COMPARE(events->count(), 80);
    QCOMPARE(list->property("contentY").toReal(), contentY);
    QCOMPARE(panel->property("selectedSeq").toReal(), selected);
    QVERIFY(list->hasActiveFocus());
    if (!kDiagArtifacts.isEmpty())
        ed.rig.app->grabWindow().save(kDiagArtifacts + QStringLiteral("/debug-panel-events.png"));

    // Live follows again
    ed.click(ed.item(QStringLiteral("eventsLive")));
    QTRY_VERIFY(list->property("atYEnd").toBool());
    QVERIFY(panel->property("followTail").toBool());
    QCOMPARE(panel->property("selectedSeq").toReal(), selected);

    // an evicted selection is cleared and said so
    const quint64 g = ShowEventLog::generation();
    for (int i = 0; i < ShowEventLog::Capacity; i++)
        ShowEventLog::append(diagEntry(g, QStringLiteral("bulk %1").arg(i)));
    QTRY_COMPARE(panel->property("selectedSeq").toReal(), -1.0);
    QVERIFY(ed.item(QStringLiteral("selectionExpired")) != nullptr);
    QCOMPARE(events->count(), ShowEventLog::Capacity);
    QTRY_VERIFY(list->property("atYEnd").toBool());

    // the referenced controls, problems first, filtered on request
    ed.click(ed.item(QStringLiteral("referencesTab")));
    QQuickItem *refs = ed.item(QStringLiteral("referencesList"));
    QTRY_COMPARE(refs->property("count").toInt(), 4);
    ed.click(ed.item(QStringLiteral("referencesProblems")));
    QTRY_COMPARE(refs->property("count").toInt(), 2);
    const QList<QQuickItem *> problemRows = shownRows(refs, QStringLiteral("referenceRow"));
    QCOMPARE(problemRows.count(), 2);
    QVERIFY(ed.item(QStringLiteral("referencesProblems"))->property("text").toString().contains(QLatin1String("(2)")));
    if (!kDiagArtifacts.isEmpty())
        ed.rig.app->grabWindow().save(kDiagArtifacts + QStringLiteral("/debug-panel-references.png"));

    // only presentation changed
    QCOMPARE(ed.show->commandTrackRevision(), revision);
    QCOMPARE(ed.show->commandTrack().commands(), authored.commands());
    QVERIFY(ed.rig.recorder->isAuthoring());

    // hiding the window ends the observation; showing it again starts empty
    ed.rig.app->hide();
    QTRY_VERIFY(!events->isActive());
    QCOMPARE(events->count(), 0);
    QCOMPARE(ShowEventLog::generation(), quint64(0));
    ed.rig.app->showNormal();
    QVERIFY(QTest::qWaitForWindowExposed(ed.rig.app.get()));
    QTRY_VERIFY(events->isActive());
    QCoreApplication::processEvents();
    QCOMPARE(events->count(), 0);
    QVERIFY(ed.rig.recorder->setRecording(false));
    events->setOpen(false);
}

void ShowCommandRecorder_Test::debugPanel_appResetStartsFromTheNewWorkspace()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto *events = qobject_cast<ShowEventModel *>(ed.rig.context("showEvents"));
    QVERIFY(events);
    events->showTab(ShowEventModel::ReferencesTab);
    QTRY_COMPARE(ed.item(QStringLiteral("referencesList"))->property("count").toInt(), 4);
    const quint32 oldShowId = ed.show->id();
    QSignalSpy refreshed(ed.rig.recorder, &ShowCommandRecorder::referencesChanged);

    // a queued control refresh, then the workspace goes
    ed.button->setCaption(QStringLiteral("Old queued caption"));
    ed.rig.realTimer = true;
    ed.rig.app->clearDocument();
    ed.button = nullptr;
    ed.fader = nullptr;
    ed.dimmer = nullptr;
    ed.scene = nullptr;
    QVERIFY(!events->isOpen());
    QCOMPARE(ShowEventLog::generation(), quint64(0));
    QCOMPARE(events->problemCount(), 0.0);

    // the new workspace reuses the Show ID; closed, nothing follows it
    auto *vc = qobject_cast<VirtualConsole *>(ed.rig.context("virtualConsole"));
    VCBridgeV5 bridge(ed.rig.doc, vc);
    Scene *scene = new Scene(ed.rig.doc);
    scene->setName(QStringLiteral("New scene"));
    ed.rig.doc->addFunction(scene);
    const int frame = bridge.addFrame(0, QRect(0, 0, 300, 200), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, vc, frame, scene->id(), 10, QStringLiteral("New button"));
    Show *reused = new Show(ed.rig.doc);
    ed.rig.doc->addFunction(reused, oldShowId);
    QCOMPARE(reused->id(), oldShowId);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 20, button->ensureRecordingId(), true)));
    const QUuid gone = QUuid::createUuid();
    QVERIFY(track.insert(ShowCommand::setButtonState(1, 40, gone, true)));
    QVERIFY(track.setExtent(200));
    QVERIFY(reused->setCommandTrack(track));
    ed.rig.manager->setCurrentShowID(int(reused->id()));
    const int closedAt = refreshed.count();
    QTest::qWait(50);
    QCOMPARE(refreshed.count(), closedAt);

    // closed, playback still validates destinations: the good one runs, the gone one counts
    reused->start(ed.rig.doc->masterTimer(), FunctionParent::master());
    QTRY_COMPARE(button->state(), VCButton::Active);
    QTRY_COMPARE(events->problemCount(), 1.0);
    QVERIFY(events->problemReason().contains(gone.toString(QUuid::WithoutBraces)));
    QCOMPARE(events->problemTab(), int(ShowEventModel::ReferencesTab));
    reused->stop(FunctionParent::master());
    QTRY_VERIFY(!reused->isRunning() && !ed.rig.manager->isPlaying());
    QTest::qWait(50);

    // reopened: only the new workspace's references
    ed.click(ed.item(QStringLiteral("problemsIndicator")));
    QTRY_VERIFY(events->isActive());
    QCOMPARE(events->tab(), int(ShowEventModel::ReferencesTab));
    QTRY_COMPARE(ed.item(QStringLiteral("referencesList"))->property("count").toInt(), 2);
    const QVariantList rows = ed.rig.recorder->referencedControls();
    QCOMPARE(rows.first().toMap().value("status").toString(), QStringLiteral("Missing"));
    QCOMPARE(rows.last().toMap().value("caption").toString(), QStringLiteral("New button"));
    QCoreApplication::processEvents();
    QCOMPARE(events->count(), 0);
    events->setOpen(false);
}

void ShowCommandRecorder_Test::debugPanel_deferredOutcomeKeepsAcceptedIdentity()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    Show *show = new Show(&doc);
    QVERIFY(doc.addFunction(show));
    Show *next = addRecordingShow(&doc, 7777);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    ShowEventModel model(&doc, &recorder);
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frame = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frame, scene->id(), 10, QStringLiteral("Replayed"));
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frame, Function::invalidId());
    slider->setSliderMode(VCSlider::Level);
    slider->addLevelChannel(scene->values().first().fxi, 3);
    slider->setCaption(QStringLiteral("Accepted caption"));
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setButtonState(0, 100, button->ensureRecordingId(), true)));
    QVERIFY(track.insert(ShowCommand::setSliderPosition(1, 110, slider->ensureRecordingId(),
                                                        ShowControlRole::LevelSlider, QString(), 0.3)));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    QSignalSpy batches(show, &Show::controlBatchesReady);
    show->start(doc.masterTimer(), FunctionParent::master());
    for (int i = 0; i < 20 && batches.isEmpty(); i++)
        doc.masterTimer()->timerTick();
    doc.masterTimer()->timerTick();
    show->setPause(true);
    QVERIFY(recorder.setResolvedShow(show->id()));
    QVERIFY(recorder.setRecording(true));
    model.setOpen(true);

    // accepted on the first take; handed over and renamed while it waits
    slider->requestUserValue(140);
    QCOMPARE(recorder.pendingUserRequests().count(), 1);
    const quint64 trace = recorder.pendingUserRequests().first().cause.trace;
    const quint64 acceptedTake = recorder.takeId();
    QVERIFY(recorder.setResolvedShow(next->id()));
    slider->setCaption(QStringLiteral("Renamed after acceptance"));
    for (int i = 0; i < 30 && (!recorder.pendingUserRequests().isEmpty() || recorder.pendingControlRuns() > 0 ||
                               show->commandWorkPending()); i++)
        tickAndDeliver(&doc);
    QCoreApplication::processEvents();
    QCOMPARE(slider->value(), 140);

    QVector<ShowEventLog::Entry> caused;
    for (const ShowEventLog::Entry &e : model.rows())
    {
        if (e.trace == trace)
            caused.append(e);
    }
    QVERIFY(caused.count() >= 2);
    for (const ShowEventLog::Entry &e : caused)
    {
        QCOMPARE(e.showId, show->id());
        QCOMPARE(e.take, acceptedTake);
        QCOMPARE(e.control, QStringLiteral("Accepted caption"));
        // the request's time and value; its authored command shows the
        // recorded position, the extent it moved has no Show time
        if (e.action != QStringLiteral("Extent"))
            QCOMPARE(e.showTimeMs, caused.first().showTimeMs);
        if (e.phase != ShowEventLog::Phase::Commit)
            QCOMPARE(e.value, QStringLiteral("140"));
    }
    QCOMPARE(caused.last().outcome, ShowEventLog::Outcome::Applied);
    model.setOpen(false);
    QVERIFY(recorder.setRecording(false));
    show->stop(FunctionParent::master());
    tickAndDeliver(&doc, 3);
}

namespace
{
/** A take whose accepted slider input is kept unpublished: its Show refused
 *  it while a legacy target was missing, then the target came back */
struct PendingTake
{
    Show *show = nullptr;
    VCSlider *slider = nullptr;
};

PendingTake pendingTake(Doc *doc, UiFixture *ui, ShowCommandRecorder *recorder, bool openAtAcceptance,
                        ShowEventModel *model)
{
    PendingTake take;
    Scene *scene = addTarget(doc);
    Scene *gone = new Scene(doc);
    doc->addFunction(gone);
    const quint32 goneId = gone->id();
    take.show = addRecordingShow(doc, 3000);
    ShowCommandTrack track;
    track.insert(ShowCommand::start(0, 1000, goneId));
    track.setExtent(1000);
    take.show->setCommandTrack(track);
    VCBridgeV5 bridge(doc, ui->vc());
    const int frame = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    take.slider = addAdjustSlider(bridge, ui->vc(), frame, scene->id());
    recorder->setResolvedShow(take.show->id());
    recorder->setRecording(true);
    doc->deleteFunction(goneId);
    model->setOpen(openAtAcceptance);
    take.slider->requestUserValue(133);
    Scene *restored = new Scene(doc);
    doc->addFunction(restored, goneId);
    return take;
}
} // namespace

void ShowCommandRecorder_Test::debugPanel_closedPendingOutcomeIsNotBackfilled_data()
{
    QTest::addColumn<bool>("openAtAcceptance");
    QTest::newRow("accepted while closed") << false;
    QTest::newRow("accepted in an earlier observation") << true;
}

void ShowCommandRecorder_Test::debugPanel_closedPendingOutcomeIsNotBackfilled()
{
    QFETCH(bool, openAtAcceptance);
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    ShowEventModel model(&doc, &recorder);
    const PendingTake take = pendingTake(&doc, &ui, &recorder, openAtAcceptance, &model);
    QCOMPARE(take.show->commandTrack().count(), 1);

    // a new observation after the input was accepted; a later checkpoint publishes it
    model.setOpen(false);
    model.setOpen(true);
    QVERIFY(recorder.checkpoint());
    QCoreApplication::processEvents();
    QCOMPARE(take.show->commandTrack().count(), 2);
    QCOMPARE(take.show->commandTrack().extent(), quint32(3000));

    // owner decision: no row of the old input, nor the extent it derives;
    // the checkpoint itself is its own operation with its own trace
    int commits = 0;
    int checkpoints = 0;
    for (const ShowEventLog::Entry &e : model.rows())
    {
        commits += e.phase == ShowEventLog::Phase::Commit;
        if (e.phase == ShowEventLog::Phase::Transport && e.action == QStringLiteral("Checkpoint"))
        {
            checkpoints++;
            QVERIFY(e.trace != 0);
            QCOMPARE(e.outcome, ShowEventLog::Outcome::Applied);
            QCOMPARE(e.commandId, ShowEventLog::NoId);
        }
    }
    QCOMPARE(commits, 0);
    QCOMPARE(checkpoints, 1);
    model.setOpen(false);
    QVERIFY(recorder.setRecording(false));
    tickAndDeliver(&doc, 3);
}

void ShowCommandRecorder_Test::debugPanel_checkpointIsItsOwnObservedOperation()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    ShowEventModel model(&doc, &recorder);
    Scene *scene = addTarget(&doc);
    Show *show = addRecordingShow(&doc, 2000);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 1000, scene->id())));
    QVERIFY(track.setExtent(60000));
    QVERIFY(show->setCommandTrack(track));
    QVERIFY(recorder.setResolvedShow(show->id()));
    QVERIFY(recorder.setRecording(true));

    // begun after opening: the checkpoint and the extent it ends at its content
    model.setOpen(true);
    QVERIFY(recorder.checkpoint());
    QCoreApplication::processEvents();
    QCOMPARE(show->commandTrack().extent(), quint32(1000));
    const QVector<ShowEventLog::Entry> &rows = model.rows();
    const int extent = findRow(rows, ShowEventLog::Phase::Commit, ShowEventLog::Outcome::Published, QString());
    QVERIFY(extent >= 0);
    QCOMPARE(rows.at(extent).action, QStringLiteral("Extent"));
    QCOMPARE(rows.at(extent).before, QStringLiteral("60.000"));
    QCOMPARE(rows.at(extent).after, QStringLiteral("1.000"));
    const int checkpoint = findRow(rows, ShowEventLog::Phase::Transport, ShowEventLog::Outcome::Applied, QString());
    QVERIFY(checkpoint > extent);
    QCOMPARE(rows.at(checkpoint).action, QStringLiteral("Checkpoint"));
    QVERIFY(rows.at(extent).trace != 0);
    QCOMPARE(rows.at(extent).trace, rows.at(checkpoint).trace);
    model.setOpen(false);
    QVERIFY(recorder.setRecording(false));
}

void ShowCommandRecorder_Test::debugPanel_committedChangesReportCommandAndExtentDeltas()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    Show *show = addRecordingShow(&doc, 0);
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 500, scene->id())));
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 1000, scene->id(), 0.4)));
    QVERIFY(track.setExtent(1000));
    QVERIFY(show->setCommandTrack(track));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    QVERIFY(recorder.setResolvedShow(show->id()));
    ShowEventModel model(&doc, &recorder);
    model.setOpen(true);

    const auto rowsWith = [&](const QString &action) {
        QVector<ShowEventLog::Entry> found;
        for (const ShowEventLog::Entry &e : model.rows())
        {
            if (e.phase == ShowEventLog::Phase::Commit && e.outcome == ShowEventLog::Outcome::Published &&
                e.action == action)
                found.append(e);
        }
        return found;
    };

    // an editor retime moves the last event and with it the extent
    QVERIFY(recorder.retimeCommand(show->id(), 1, 5000));
    QCoreApplication::processEvents();
    QCOMPARE(show->commandTrack().extent(), quint32(5000));
    QCOMPARE(rowsWith(QStringLiteral("Set intensity")).count(), 1);
    QCOMPARE(rowsWith(QStringLiteral("Set intensity")).first().commandId, 1u);
    QVERIFY(rowsWith(QStringLiteral("Set intensity")).first().before.startsWith(QLatin1String("1.000")));
    QVERIFY(rowsWith(QStringLiteral("Set intensity")).first().after.startsWith(QLatin1String("5.000")));
    QCOMPARE(rowsWith(QStringLiteral("Extent")).count(), 1);
    QCOMPARE(rowsWith(QStringLiteral("Extent")).first().before, QStringLiteral("1.000"));
    QCOMPARE(rowsWith(QStringLiteral("Extent")).first().after, QStringLiteral("5.000"));

    // a refused edit reports no publication
    const int published = model.count();
    QVERIFY(!recorder.retimeCommand(show->id(), 99, 100));
    QCoreApplication::processEvents();
    QCOMPARE(findRow(model.rows(), ShowEventLog::Phase::Commit, ShowEventLog::Outcome::Published, QString(),
                     published), -1);

    // another source replaces the track with as many events: its real changes
    model.setOpen(false);
    model.setOpen(true);
    ShowCommandTrack changed = show->commandTrack();
    QVERIFY(changed.retime(1, 4000));
    QVERIFY(changed.remove(0));
    QVERIFY(changed.insert(ShowCommand::stop(2, 4000, scene->id())));
    QVERIFY(changed.setExtent(4000));
    QVERIFY(show->setCommandTrack(changed));
    QCoreApplication::processEvents();
    const QVector<ShowEventLog::Entry> moved = rowsWith(QStringLiteral("Set intensity"));
    QCOMPARE(moved.count(), 1);
    QCOMPARE(moved.first().commandId, 1u);
    QVERIFY(moved.first().before.startsWith(QLatin1String("5.000")));
    QVERIFY(moved.first().after.startsWith(QLatin1String("4.000")));
    const QVector<ShowEventLog::Entry> removed = rowsWith(QStringLiteral("Start function"));
    QCOMPARE(removed.count(), 1);
    QCOMPARE(removed.first().commandId, 0u);
    QVERIFY(removed.first().after.isEmpty());
    const QVector<ShowEventLog::Entry> added = rowsWith(QStringLiteral("Stop function"));
    QCOMPARE(added.count(), 1);
    QCOMPARE(added.first().commandId, 2u);
    QVERIFY(added.first().before.isEmpty());
    QCOMPARE(rowsWith(QStringLiteral("Extent")).count(), 1);
    QCOMPARE(rowsWith(QStringLiteral("Extent")).first().after, QStringLiteral("4.000"));
    // one change, one trace
    QCOMPARE(moved.first().trace, removed.first().trace);
    QVERIFY(moved.first().trace != 0);
    model.setOpen(false);
}

void ShowCommandRecorder_Test::debugPanel_panelHiddenOrUnloadedEndsObservation_data()
{
    QTest::addColumn<bool>("unload");
    QTest::newRow("panel hidden") << false;
    QTest::newRow("main QML unloaded") << true;
}

void ShowCommandRecorder_Test::debugPanel_panelHiddenOrUnloadedEndsObservation()
{
    QFETCH(bool, unload);
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto *events = qobject_cast<ShowEventModel *>(ed.rig.context("showEvents"));
    ed.click(ed.item(QStringLiteral("recordDebugButton")));
    QTRY_VERIFY(events->isActive());
    ed.click(ed.item(QStringLiteral("referencesTab")));
    QVERIFY(events->isActive());
    QSignalSpy refreshed(ed.rig.recorder, &ShowCommandRecorder::referencesChanged);

    if (unload)
        ed.rig.app->setSource(QUrl());
    else
        QVERIFY(ed.item(QStringLiteral("showDebugPanel"))->setProperty("visible", false));
    QCoreApplication::processEvents();

    QVERIFY(!events->isActive());
    QCOMPARE(ShowEventLog::generation(), quint64(0));
    QCOMPARE(events->count(), 0);
    const int closedAt = refreshed.count();
    ed.button->setCaption(QStringLiteral("Changed after the panel went"));
    QTest::qWait(50);
    QCOMPARE(refreshed.count(), closedAt);

    if (!unload)
    {
        // Debug shows it again, fresh
        ed.click(ed.item(QStringLiteral("recordDebugButton")));
        QTRY_VERIFY(ed.item(QStringLiteral("showDebugPanel")) != nullptr);
        QVERIFY(events->isActive());
        QCoreApplication::processEvents();
        QCOMPARE(events->count(), 0);
    }
    events->setOpen(false);
}

void ShowCommandRecorder_Test::debugPanel_soleDroppedDiagnosticWakes()
{
    Doc doc(nullptr, 1);
    ShowEventModel model(&doc, nullptr);
    model.setOpen(true);
    QCoreApplication::processEvents();
    ShowEventLog::resetStatsForTest();
    const quint64 g = ShowEventLog::generation();

    // the GUI holds the ring as the timer offers its last diagnostic
    const auto offerContended = [g]()
    {
        ShowEventLog::lockRingForTest();
        std::thread timer([g]() { ShowEventLog::append(diagEntry(g, QStringLiteral("dropped")), true); });
        timer.join();
        ShowEventLog::unlockRingForTest();
    };
    offerContended();
    QCOMPARE(ShowEventLog::stats().wakesPosted, quint64(1));
    // a second drop while that wake is pending posts nothing more
    offerContended();
    QCOMPARE(ShowEventLog::stats().wakesPosted, quint64(1));
    QCoreApplication::processEvents();
    QCOMPARE(model.dropped(), 2.0);
    QCOMPARE(model.count(), 0);
    // drained, the next drop wakes again; after reopen the count is the new one's
    offerContended();
    QCOMPARE(ShowEventLog::stats().wakesPosted, quint64(2));
    QCoreApplication::processEvents();
    QCOMPARE(model.dropped(), 3.0);
    model.setOpen(false);
    model.setOpen(true);
    QCOMPARE(model.dropped(), 0.0);
    model.setOpen(false);
}

void ShowCommandRecorder_Test::debugPanel_rejectedKeyboardInputIsObserved_data()
{
    QTest::addColumn<bool>("disabled");
    QTest::addColumn<bool>("global");
    QTest::newRow("enabled, page") << false << false;
    QTest::newRow("disabled, page") << true << false;
    QTest::newRow("enabled, global") << false << true;
    QTest::newRow("disabled, global") << true << true;
}

void ShowCommandRecorder_Test::debugPanel_rejectedKeyboardInputIsObserved()
{
    QFETCH(bool, disabled);
    QFETCH(bool, global);
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    Show *show = addRecordingShow(&doc, 1500);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    ShowEventModel model(&doc, &recorder);
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frame = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frame, scene->id(), 10, QStringLiteral("Keyboard button"));
    VCPage *page = ui.vc()->page(0);
    page->setVisible(true);
    button->setDisabled(disabled);
    QKeySequence sequence(Qt::Key_F8);
    page->mapKeySequence(sequence, 0, button);
    QVERIFY(recorder.setResolvedShow(show->id()));
    QVERIFY(recorder.setRecording(true));
    model.setOpen(true);

    if (global)
        page->handleKeyEventGlobal(sequence, true);
    else
        page->handleKeyEvent(sequence, true);
    tickAndDeliver(&doc, 2);

    QVector<ShowEventLog::Entry> keyboard;
    for (const ShowEventLog::Entry &e : model.rows())
    {
        if (e.origin == int(ShowCommandOrigin::Keyboard))
            keyboard.append(e);
    }
    QVERIFY(!keyboard.isEmpty());
    QCOMPARE(keyboard.first().control, QStringLiteral("Keyboard button"));
    if (disabled)
    {
        // refused where it is routed, neither executed nor recorded
        QCOMPARE(keyboard.count(), 1);
        QCOMPARE(keyboard.first().phase, ShowEventLog::Phase::Input);
        QCOMPARE(keyboard.first().outcome, ShowEventLog::Outcome::Ignored);
        QVERIFY(keyboard.first().reason.contains(QLatin1String("disabled")));
        QCOMPARE(button->state(), VCButton::Inactive);
        QCOMPARE(show->commandTrack().count(), 0);
    }
    else
    {
        QCOMPARE(keyboard.first().outcome, ShowEventLog::Outcome::Recorded);
        QCOMPARE(button->state(), VCButton::Active);
        QCOMPARE(show->commandTrack().count(), 1);
    }
    model.setOpen(false);
    QVERIFY(recorder.setRecording(false));
    scene->stop(FunctionParent::master());
    tickAndDeliver(&doc, 3);
}

void ShowCommandRecorder_Test::debugPanel_directProgrammaticRequestsAreObservedOnce_data()
{
    QTest::addColumn<bool>("open");
    QTest::newRow("closed") << false;
    QTest::newRow("open") << true;
}

void ShowCommandRecorder_Test::debugPanel_directProgrammaticRequestsAreObservedOnce()
{
    QFETCH(bool, open);
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    Scene *other = new Scene(&doc);
    other->setValue(SceneValue(scene->values().first().fxi, 1, 90));
    QVERIFY(doc.addFunction(other));
    Show *show = addRecordingShow(&doc, 1000);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    ShowEventModel model(&doc, &recorder);
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frame = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frame, scene->id(), 10, QStringLiteral("Code button"));
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frame, other->id());
    slider->setCaption(QStringLiteral("Code fader"));
    VCButton *flash = qobject_cast<VCButton *>(ui.vc()->widget(
            bridge.addButton(frame, QRect(300, 10, 80, 40), other->id(), QStringLiteral("Flash"),
                             QStringLiteral("flash"))));
    QVERIFY(recorder.setResolvedShow(show->id()));
    QVERIFY(recorder.setRecording(true));
    model.setOpen(open);
    QCoreApplication::processEvents();
    ShowEventLog::resetStatsForTest();

    // what WebAccess and native undo call: live, never recorded
    button->requestStateChange(true);
    slider->setValue(119, true, true);
    // display feedback asks for no output
    slider->setValue(40, false, true);
    // wrapped paths trace themselves only: a user value and a Flash press
    slider->requestUserValue(77, true, ShowCommandOrigin::Pointer);
    flash->requestUserStateChange(true, ShowCommandOrigin::Pointer);
    flash->requestUserStateChange(false, ShowCommandOrigin::Pointer);
    tickAndDeliver(&doc, 3);
    QCoreApplication::processEvents();

    QCOMPARE(button->state(), VCButton::Active);
    QCOMPARE(slider->value(), 77);
    QCOMPARE(show->commandTrack().count(), 1);
    QCOMPARE(show->commandTrack().commands().first().position, 77.0 / 255.0);
    if (!open)
    {
        QCOMPARE(ShowEventLog::stats().offered, quint64(0));
        QCOMPARE(model.count(), 0);
    }
    else
    {
        QVector<ShowEventLog::Entry> programmatic;
        int fader = 0;
        for (const ShowEventLog::Entry &e : model.rows())
        {
            if (e.origin == int(ShowCommandOrigin::Programmatic))
                programmatic.append(e);
            fader += e.control == QStringLiteral("Code fader") && e.origin == int(ShowCommandOrigin::Pointer);
        }
        QCOMPARE(programmatic.count(), 2);
        QCOMPARE(programmatic.at(0).control, QStringLiteral("Code button"));
        QCOMPARE(programmatic.at(0).value, QStringLiteral("1"));
        QCOMPARE(programmatic.at(1).control, QStringLiteral("Code fader"));
        QCOMPARE(programmatic.at(1).value, QStringLiteral("119"));
        for (const ShowEventLog::Entry &e : programmatic)
            QVERIFY(e.reason.contains(QLatin1String("not recorded")));
        // the user value: its decision, added command, extent and execution, no second trace
        QCOMPARE(fader, 4);
    }
    model.setOpen(false);
    QVERIFY(recorder.setRecording(false));
    for (Function *function : doc.functions())
        function->stop(FunctionParent::master());
    tickAndDeliver(&doc, 3);
}

void ShowCommandRecorder_Test::debugPanel_debugRefocusesTheOpenPanel()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto *events = qobject_cast<ShowEventModel *>(ed.rig.context("showEvents"));
    ed.click(ed.item(QStringLiteral("recordDebugButton")));
    QTRY_VERIFY(events->isActive());
    QQuickItem *panel = ed.item(QStringLiteral("showDebugPanel"));
    const quint64 observation = ShowEventLog::generation();
    ed.click(ed.item(QStringLiteral("recordSwitch")));
    QTRY_VERIFY(events->count() > 0);
    const int rows = events->count();
    QVERIFY(!panel->hasActiveFocus());

    ed.click(ed.item(QStringLiteral("recordDebugButton")));
    QTRY_VERIFY(panel->hasActiveFocus());
    QCOMPARE(ShowEventLog::generation(), observation);
    QVERIFY(events->count() >= rows);
    QCOMPARE(ed.root()->findChildren<QQuickItem *>(QStringLiteral("showDebugPanel")).count(), 1);
    QVERIFY(ed.rig.recorder->setRecording(false));
    events->setOpen(false);
}

namespace
{
/** Counts the translations of one diagnostic text, the first thing
 *  building that diagnostic does */
class DiagnosticTextCounter : public QTranslator
{
public:
    explicit DiagnosticTextCounter(const char *source) : m_source(source) {}
    bool isEmpty() const override { return false; }
    QString translate(const char *context, const char *source, const char *, int) const override
    {
        if (QByteArray(context) == "ShowCommandRecorder" && m_source == source)
            count++;
        return QString();
    }
    mutable int count = 0;

private:
    QByteArray m_source;
};
} // namespace

void ShowCommandRecorder_Test::debugPanel_closedCheckpointBuildsNoDiagnostic_data()
{
    QTest::addColumn<bool>("open");
    QTest::addColumn<bool>("fails");
    QTest::newRow("closed, saved") << false << false;
    QTest::newRow("closed, refused") << false << true;
    QTest::newRow("open, saved") << true << false;
}

void ShowCommandRecorder_Test::debugPanel_closedCheckpointBuildsNoDiagnostic()
{
    QFETCH(bool, open);
    QFETCH(bool, fails);
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    ShowEventModel model(&doc, &recorder);
    Show *show = nullptr;
    if (fails)
    {
        // a pending input its Show refuses: the checkpoint fails and says so
        const PendingTake take = pendingTake(&doc, &ui, &recorder, false, &model);
        show = take.show;
        QVERIFY(doc.deleteFunction(show->commandTrack().commands().first().functionId));
    }
    else
    {
        Scene *scene = addTarget(&doc);
        show = addRecordingShow(&doc, 4000);
        ShowCommandTrack track;
        QVERIFY(track.insert(ShowCommand::start(0, 1000, scene->id())));
        QVERIFY(track.setExtent(6000));
        QVERIFY(show->setCommandTrack(track));
        QVERIFY(recorder.setResolvedShow(show->id()));
        QVERIFY(recorder.setRecording(true));
    }
    model.setOpen(open);

    DiagnosticTextCounter saved("accepted input ends at its content");
    DiagnosticTextCounter action("Checkpoint");
    QVERIFY(QCoreApplication::installTranslator(&saved));
    QVERIFY(QCoreApplication::installTranslator(&action));
    const bool result = recorder.checkpoint();
    QCoreApplication::removeTranslator(&action);
    QCoreApplication::removeTranslator(&saved);

    // the checkpoint itself is unchanged; only an open panel builds its entry
    QCOMPARE(result, !fails);
    QCOMPARE(recorder.lastError().isEmpty(), !fails);
    if (!fails)
        QCOMPARE(show->commandTrack().extent(), quint32(1000));
    QCOMPARE(saved.count, open && !fails ? 1 : 0);
    QCOMPARE(action.count, open ? 1 : 0);
    model.setOpen(false);
    recorder.discardRecording();
}

void ShowCommandRecorder_Test::debugPanel_firstRecordedIdentityIsConsistent()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    Show *show = addRecordingShow(&doc, 1234);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    ShowEventModel model(&doc, &recorder);
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frame = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCSlider *slider = addAdjustSlider(bridge, ui.vc(), frame, scene->id());
    VCSlider *idle = addAdjustSlider(bridge, ui.vc(), frame, scene->id());
    QVERIFY(slider->recordingId().isNull() && idle->recordingId().isNull());
    model.setOpen(true);

    // observed but not recorded: no identity is issued for it
    idle->requestUserValue(50, true, ShowCommandOrigin::Osc);
    QVERIFY(recorder.setResolvedShow(show->id()));
    QVERIFY(recorder.setRecording(true));
    idle->requestUserValue(60, true, ShowCommandOrigin::Audio);
    QVERIFY(idle->recordingId().isNull());

    // the first recorded input issues it, and every entry it causes carries it
    slider->requestUserValue(119, true, ShowCommandOrigin::Osc);
    QCoreApplication::processEvents();
    QCOMPARE(show->commandTrack().count(), 1);
    const QUuid assigned = show->commandTrack().commands().first().controlId;
    QVERIFY(!assigned.isNull());
    QCOMPARE(slider->recordingId(), assigned);
    int caused = 0;
    for (const ShowEventLog::Entry &e : model.rows())
    {
        if (e.widgetId == idle->id())
            QVERIFY(e.controlId.isNull());
        if (e.widgetId != slider->id())
            continue;
        caused++;
        QCOMPARE(e.controlId, assigned);
    }
    QCOMPARE(caused, 4);
    model.setOpen(false);
    QVERIFY(recorder.setRecording(false));
    scene->stop(FunctionParent::master());
    tickAndDeliver(&doc, 3);
}

void ShowCommandRecorder_Test::debugPanel_commandDeltaKeepsChangedFacts_data()
{
    QTest::addColumn<QString>("change");
    QTest::newRow("target only") << QStringLiteral("target");
    QTest::newRow("order only") << QStringLiteral("order");
    QTest::newRow("control role only") << QStringLiteral("role");
}

void ShowCommandRecorder_Test::debugPanel_commandDeltaKeepsChangedFacts()
{
    QFETCH(QString, change);
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    Scene *other = new Scene(&doc);
    QVERIFY(doc.addFunction(other));
    Show *show = addRecordingShow(&doc, 0);
    const QUuid control = QUuid::createUuid();
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::start(0, 1000, scene->id())));
    QVERIFY(track.insert(ShowCommand::stop(1, 1000, scene->id())));
    QVERIFY(track.insert(ShowCommand::setSliderPosition(2, 2000, control, ShowControlRole::AdjustSlider,
                                                        QStringLiteral("Intensity"), 0.5)));
    QVERIFY(track.setExtent(2000));
    QVERIFY(show->setCommandTrack(track));
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    QVERIFY(recorder.setResolvedShow(show->id()));
    ShowEventModel model(&doc, &recorder);
    model.setOpen(true);

    QVector<quint32> expected;
    if (change == QLatin1String("target"))
    {
        QVERIFY(recorder.setCommandTarget(show->id(), 0, other->id()));
        expected = {0};
    }
    else
    {
        ShowCommandTrack changed;
        if (change == QLatin1String("order"))
        {
            QVERIFY(changed.insert(ShowCommand::stop(1, 1000, scene->id())));
            QVERIFY(changed.insert(ShowCommand::start(0, 1000, scene->id())));
            QVERIFY(changed.insert(track.commands().at(2)));
            expected = {0, 1};
        }
        else
        {
            QVERIFY(changed.insert(track.commands().at(0)));
            QVERIFY(changed.insert(track.commands().at(1)));
            QVERIFY(changed.insert(ShowCommand::setSliderPosition(2, 2000, control, ShowControlRole::LevelSlider,
                                                                  QString(), 0.5)));
            expected = {2};
        }
        QVERIFY(changed.setExtent(2000));
        QVERIFY(show->setCommandTrack(changed));
    }
    QCoreApplication::processEvents();

    QVector<quint32> changedIds;
    for (const ShowEventLog::Entry &e : model.rows())
    {
        if (e.phase != ShowEventLog::Phase::Commit || e.commandId == ShowEventLog::NoId)
            continue;
        changedIds.append(e.commandId);
        // the stored sides say what changed, not only that something did
        QVERIFY2(e.before != e.after, qPrintable(e.before));
        QVERIFY(!e.before.isEmpty() && !e.after.isEmpty());
        if (change == QLatin1String("target"))
        {
            QVERIFY(e.before.contains(QStringLiteral("function %1").arg(scene->id())));
            QVERIFY(e.after.contains(QStringLiteral("function %1").arg(other->id())));
        }
        else if (change == QLatin1String("order"))
        {
            const quint32 wasOrder = track.commands().at(int(e.commandId)).order;
            QVERIFY(e.before.contains(QStringLiteral("#%1").arg(wasOrder)));
            QVERIFY(e.after.contains(QStringLiteral("#%1").arg(show->commandTrack().commands().at(
                show->commandTrack().indexOfId(e.commandId)).order)));
        }
        else
        {
            QVERIFY(e.before.contains(QLatin1String("AdjustSlider Intensity")));
            QVERIFY(e.after.contains(QLatin1String("LevelSlider")));
            QVERIFY(e.before.contains(control.toString(QUuid::WithoutBraces)));
        }
        QVERIFY(e.before.size() <= ShowEventLog::ReasonLength);
    }
    std::sort(changedIds.begin(), changedIds.end());
    QCOMPARE(changedIds, expected);
    model.setOpen(false);
}

void ShowCommandRecorder_Test::debugPanel_directRequestClaimsNoExecution_data()
{
    QTest::addColumn<bool>("bound");
    QTest::newRow("bound") << true;
    QTest::newRow("unbound") << false;
}

void ShowCommandRecorder_Test::debugPanel_directRequestClaimsNoExecution()
{
    QFETCH(bool, bound);
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    Scene *scene = addTarget(&doc);
    ShowCommandRecorder recorder(&doc);
    recorder.setVirtualConsole(ui.vc());
    ShowEventModel model(&doc, &recorder);
    VCBridgeV5 bridge(&doc, ui.vc());
    const int frame = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    VCButton *button = addToggle(bridge, ui.vc(), frame, bound ? scene->id() : Function::invalidId(), 10,
                                 QStringLiteral("Direct button"));
    model.setOpen(true);
    button->requestStateChange(true);
    tickAndDeliver(&doc, 2);
    QCoreApplication::processEvents();

    QCOMPARE(button->state(), bound ? VCButton::Active : VCButton::Inactive);
    QCOMPARE(model.count(), 1);
    const ShowEventLog::Entry e = model.rows().first();
    QCOMPARE(e.origin, int(ShowCommandOrigin::Programmatic));
    // arrival says nothing about what the control then did
    QCOMPARE(e.outcome, ShowEventLog::Outcome::Received);
    QVERIFY(!e.reason.contains(QLatin1String("executed")));
    QVERIFY(e.reason.contains(QLatin1String("not recorded")));
    model.setOpen(false);
    scene->stop(FunctionParent::master());
    tickAndDeliver(&doc, 3);
}

void ShowCommandRecorder_Test::debugPanel_transportUsesTheAcceptanceClock_data()
{
    QTest::addColumn<QString>("route");
    // where input is accepted, and what the transport rows say
    QTest::addColumn<quint32>("expected");

    QTest::newRow("stopped and rewound") << QStringLiteral("rewound") << 0u;
    QTest::newRow("paused seek") << QStringLiteral("paused seek") << 1500u;
    QTest::newRow("external clock ignores a stale cursor") << QStringLiteral("external") << 4200u;
    QTest::newRow("external clock past INT_MAX") << QStringLiteral("external") << quint32(INT_MAX) + 1u;
    QTest::newRow("external clock at MaxTime") << QStringLiteral("external") << ShowCommand::MaxTime;
}

void ShowCommandRecorder_Test::debugPanel_transportUsesTheAcceptanceClock()
{
    QFETCH(QString, route);
    QFETCH(quint32, expected);

    RequestRig r;
    MasterTimer *timer = r.doc.masterTimer();
    ShowCommandTrack track;
    QVERIFY(track.setExtent(60000));
    QVERIFY(r.show->setCommandTrack(track));
    ShowManager manager(r.ui.view(), &r.doc);
    manager.m_detached = true;
    manager.setCurrentShowID(int(r.show->id()));
    // the application's transport wiring
    QObject::connect(&manager, &ShowManager::isPlayingChanged, &r.recorder,
                     [&](bool playing) { r.recorder.setPlaying(playing && !manager.isPaused()); });
    QObject::connect(&manager, &ShowManager::isPausedChanged, &r.recorder,
                     [&](bool paused) { r.recorder.setPlaying(manager.isPlaying() && !paused); });
    ShowEventModel model(&r.doc, &r.recorder);
    model.setOpen(true);
    const auto playFor = [&](int ticks) {
        manager.playShow();
        for (int i = 0; i < ticks; i++)
            timer->timerTick();
        tickAndDeliver(&r.doc, 1);
    };

    if (route == QLatin1String("rewound"))
    {
        // the runtime consumed a position, then Stop and Stop again rewind
        playFor(40);
        QVERIFY(r.show->commandPosition() > 0);
        manager.stopShow();
        tickAndDeliver(&r.doc, 3);
        QVERIFY(!r.show->isRunning());
        manager.stopShow();
        QCOMPARE(manager.currentTime(), 0);
        QVERIFY(r.show->commandPosition() > 0);
        QVERIFY(r.recorder.setRecording(true));
    }
    else if (route == QLatin1String("paused seek"))
    {
        QVERIFY(r.recorder.setRecording(true));
        playFor(40);
        manager.playShow();
        tickAndDeliver(&r.doc, 2);
        QVERIFY(r.show->isPaused());
        QVERIFY(r.show->commandPosition() != expected);
        manager.requestSeek(int(expected));
    }
    else
    {
        // a cursor set while the Show ran on its own clock, then the host clock
        manager.requestSeek(700);
        r.show->setSyncSource(ShowRunner::External);
        r.show->setExternalElapsedTime(expected);
        QVERIFY(r.recorder.setRecording(true));
    }
    r.ba->requestUserStateChange(true);
    tickAndDeliver(&r.doc, 2);
    if (route == QLatin1String("rewound"))
    {
        // before the timer's first write: the Show has not moved yet
        manager.playShow();
        QCoreApplication::processEvents();
    }

    // the accepted input keeps the clock it was accepted at, over the whole range
    QCOMPARE(r.show->commandTrack().count(), 1);
    QCOMPARE(r.show->commandTrack().commands().first().time, expected);

    const QVector<ShowEventLog::Entry> &rows = model.rows();
    // the latest row of that transport change
    const auto showTimeOf = [&rows](ShowEventLog::Phase phase, const QString &action) {
        for (int i = rows.count() - 1; i >= 0; i--)
        {
            if (rows.at(i).phase == phase && rows.at(i).action == action)
                return rows.at(i).showTimeMs;
        }
        return qint64(-2);
    };
    const int recorded = findRow(rows, ShowEventLog::Phase::Decision, ShowEventLog::Outcome::Recorded,
                                 QStringLiteral("A"));
    QVERIFY(recorded >= 0);
    QCOMPARE(rows.at(recorded).showTimeMs, qint64(expected));
    // the transport rows around it say the same clock; REC went on before Play there
    QCOMPARE(showTimeOf(ShowEventLog::Phase::Transport, QStringLiteral("REC on")),
             qint64(route == QLatin1String("paused seek") ? 0u : expected));
    if (route != QLatin1String("external"))
        QCOMPARE(showTimeOf(ShowEventLog::Phase::Transport, QStringLiteral("Cursor moved")), qint64(expected));
    if (route == QLatin1String("rewound"))
        QCOMPARE(showTimeOf(ShowEventLog::Phase::Transport, QStringLiteral("Playing")), qint64(expected));
    QVERIFY(r.recorder.setRecording(false));
    model.setOpen(false);
}

namespace
{
/** Qt's accessibility tree, walked the way a platform bridge walks it */
QAccessibleInterface *findAccessible(QAccessibleInterface *root,
                                     const std::function<bool(QAccessibleInterface *)> &match)
{
    if (root == nullptr || root->isValid() == false)
        return nullptr;
    if (match(root))
        return root;
    for (int i = 0; i < root->childCount(); i++)
    {
        if (QAccessibleInterface *found = findAccessible(root->child(i), match))
            return found;
    }
    return nullptr;
}

QAccessibleInterface *accessibleById(QWindow *window, const QString &id)
{
    return findAccessible(QAccessible::queryAccessibleInterface(window), [&id](QAccessibleInterface *i) {
        return i->text(QAccessible::Identifier) == id;
    });
}

QAccessibleInterface *accessibleByName(QWindow *window, QAccessible::Role role, const QString &name)
{
    return findAccessible(QAccessible::queryAccessibleInterface(window), [&](QAccessibleInterface *i) {
        return i->role() == role && i->text(QAccessible::Name) == name;
    });
}

/** What a platform bridge does with a named action: it performs only an offered one */
bool performAccessibleAction(QAccessibleInterface *iface, const QString &action)
{
    QAccessibleActionInterface *actions = iface ? iface->actionInterface() : nullptr;
    if (actions == nullptr || actions->actionNames().contains(action) == false)
        return false;
    actions->doAction(action);
    QCoreApplication::processEvents();
    return true;
}

/** As with a screen reader or a UI automation client attached */
struct AccessibilityOn
{
    AccessibilityOn() { QAccessible::setActive(true); }
    ~AccessibilityOn() { QAccessible::setActive(false); }
};
} // namespace

void ShowCommandRecorder_Test::accessibleVcControls_actAsUserInput_data()
{
    QTest::addColumn<QString>("control");
    QTest::addColumn<QString>("action");
    QTest::addColumn<bool>("disabled");
    QTest::addColumn<int>("commands");
    QTest::addColumn<int>("value");

    const QString press = QAccessibleActionInterface::pressAction();
    const QString increase = QAccessibleActionInterface::increaseAction();
    const QString decrease = QAccessibleActionInterface::decreaseAction();
    // value: the button state or the fader value afterwards
    QTest::newRow("toggle press") << QStringLiteral("toggle") << press << false << 1 << int(VCButton::Active);
    QTest::newRow("disabled toggle press") << QStringLiteral("toggle") << press << true << 0 << int(VCButton::Inactive);
    // a flash is momentary: pressed and released, reported as not recordable
    QTest::newRow("flash press") << QStringLiteral("flash") << press << false << 0 << int(VCButton::Inactive);
    QTest::newRow("fader increase") << QStringLiteral("fader") << increase << false << 1 << 101;
    QTest::newRow("fader decrease") << QStringLiteral("fader") << decrease << false << 1 << 99;
    QTest::newRow("disabled fader increase") << QStringLiteral("fader") << increase << true << 0 << 100;
}

void ShowCommandRecorder_Test::accessibleVcControls_actAsUserInput()
{
    QFETCH(QString, control);
    QFETCH(QString, action);
    QFETCH(bool, disabled);
    QFETCH(int, commands);
    QFETCH(int, value);

    AccessibilityOn accessibility;
    RequestRig r;
    r.show->setSyncSource(ShowRunner::External);
    r.show->setExternalElapsedTime(2600);
    const bool fader = control == QLatin1String("fader");
    VCButton *button = r.ba;
    if (control == QLatin1String("flash"))
    {
        button = qobject_cast<VCButton *>(r.ui.vc()->widget(
            r.bridge.addButton(r.frame, QRect(300, 10, 80, 40), r.a->id(), QStringLiteral("Flash"),
                               QStringLiteral("flash"))));
        QVERIFY(button);
    }
    if (fader)
    {
        r.slider->setValue(100);
        tickAndDeliver(&r.doc, 2);
    }
    VCWidget *widget = fader ? static_cast<VCWidget *>(r.slider) : button;
    widget->setDisabled(disabled);
    QVERIFY(r.recorder.setRecording(true));
    QSignalSpy errors(&r.recorder, &ShowCommandRecorder::lastErrorChanged);

    QQuickView *view = r.ui.view();
    view->resize(600, 400);
    std::unique_ptr<QQuickItem> item(fader ? renderSliderItem(view, r.slider, QStringLiteral("qrc:/VCSliderItem.qml"))
                                           : renderButtonItem(view, button));
    QVERIFY(item);
    if (fader)
        item->setSize(QSizeF(120, 300));
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));

    // named by its caption, the way the user sees it
    QAccessibleInterface *iface = accessibleByName(view, fader ? QAccessible::Slider : QAccessible::Button,
                                                   widget->caption());
    QVERIFY2(iface, qPrintable(QStringLiteral("no accessible %1 named %2").arg(control, widget->caption())));
    QCOMPARE(iface->state().disabled, disabled);
    if (fader)
    {
        QVERIFY(iface->valueInterface());
        QCOMPARE(iface->valueInterface()->currentValue().toInt(), 100);
    }
    else
    {
        QCOMPARE(iface->text(QAccessible::Identifier), QStringLiteral("vcButton-%1").arg(button->id()));
        QCOMPARE(iface->state().checkable, control == QLatin1String("toggle"));
        QVERIFY(!iface->state().checked);
    }

    QVERIFY(performAccessibleAction(iface, action));
    tickAndDeliver(&r.doc, 3);

    // exactly one accepted user command, or none when refused
    const QVector<ShowCommand> records = r.show->commandTrack().commands();
    QCOMPARE(records.count(), commands);
    if (fader)
    {
        QCOMPARE(r.slider->value(), value);
        QCOMPARE(iface->valueInterface()->currentValue().toInt(), value);
        if (commands)
        {
            QCOMPARE(int(records.first().action), int(ShowCommandAction::SetSliderPosition));
            QCOMPARE(records.first().position, qreal(value) / 255.0);
        }
        // feedback and code keep their own, unrecorded path
        r.slider->setValue(120, false, true);
        r.slider->setValue(130);
        tickAndDeliver(&r.doc, 3);
        QCOMPARE(iface->valueInterface()->currentValue().toInt(), 130);
    }
    else
    {
        QCOMPARE(int(button->state()), value);
        QCOMPARE(iface->state().checked, value == int(VCButton::Active));
        if (commands)
        {
            QCOMPARE(int(records.first().action), int(ShowCommandAction::SetButtonState));
            QCOMPARE(records.first().controlId, button->recordingId());
            QVERIFY(records.first().on);
        }
        QCOMPARE(errors.count(), control == QLatin1String("flash") ? 1 : 0);
    }
    QCOMPARE(r.show->commandTrack().count(), commands);
    QVERIFY(r.recorder.setRecording(false));
}

void ShowCommandRecorder_Test::accessiblePlayhead_seeksLikeTheRuler_data()
{
    QTest::addColumn<QString>("state");
    QTest::addColumn<int>("bpm");
    QTest::addColumn<bool>("seeks");
    QTest::addColumn<int>("target");
    // one accessible step: a second, or a beat
    QTest::addColumn<int>("step");

    QTest::newRow("stopped") << QStringLiteral("stopped") << 0 << true << 2500 << 1000;
    QTest::newRow("paused") << QStringLiteral("paused") << 0 << true << 2500 << 1000;
    QTest::newRow("read only") << QStringLiteral("read only") << 0 << false << 2500 << 1000;
    QTest::newRow("no show") << QStringLiteral("no show") << 0 << false << 2500 << 1000;
    // the ruler of a slow beat grid ends later than 300000 ms
    QTest::newRow("slow beats") << QStringLiteral("stopped") << 30 << true << 400000 << 2000;
}

void ShowCommandRecorder_Test::accessiblePlayhead_seeksLikeTheRuler()
{
    QFETCH(QString, state);
    QFETCH(int, bpm);
    QFETCH(bool, seeks);
    QFETCH(int, target);
    QFETCH(int, step);

    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    ShowManager *manager = ed.rig.manager;
    if (bpm > 0)
    {
        manager->setBpmNumber(bpm);
        manager->setTimeDivision(Show::BPM_4_4);
    }
    if (state == QLatin1String("paused"))
    {
        manager->playShow();
        tickAndDeliver(ed.rig.doc, 2);
        manager->playShow();
        tickAndDeliver(ed.rig.doc, 2);
        QVERIFY(ed.show->isPaused());
    }
    else if (state == QLatin1String("read only"))
    {
        manager->setReadOnly(true);
    }
    else if (state == QLatin1String("no show"))
    {
        manager->setCurrentShowID(int(Function::invalidId()));
    }
    QCoreApplication::processEvents();
    const int start = manager->currentTime();

    QAccessibleInterface *head = accessibleById(ed.rig.app.get(), QStringLiteral("showPlayheadPosition"));
    QVERIFY2(head, "the playhead has no accessible position");
    QCOMPARE(head->role(), QAccessible::Slider);
    QVERIFY(!head->text(QAccessible::Name).isEmpty());
    QAccessibleValueInterface *position = head->valueInterface();
    QVERIFY(position);
    QCOMPARE(head->state().disabled, !seeks);
    QCOMPARE(position->currentValue().toInt(), start);
    QCOMPARE(position->minimumValue().toInt(), 0);
    QVERIFY(position->maximumValue().toInt() > target + step);

    // what the platform writes for a new value, then one step
    position->setCurrentValue(target);
    QCoreApplication::processEvents();
    QCOMPARE(manager->currentTime(), seeks ? target : start);
    QCOMPARE(position->currentValue().toInt(), manager->currentTime());
    QVERIFY(performAccessibleAction(head, QAccessibleActionInterface::increaseAction()));
    QCOMPARE(manager->currentTime(), seeks ? target + step : start);
    QCOMPARE(position->currentValue().toInt(), manager->currentTime());

    QAccessibleInterface *time = accessibleById(ed.rig.app.get(), QStringLiteral("showTime"));
    QVERIFY(time);
    QCOMPARE(time->role(), QAccessible::StaticText);
    if (seeks && bpm == 0)
    {
        QVERIFY2(time->text(QAccessible::Name).contains(QLatin1String("03.5")), qPrintable(time->text(QAccessible::Name)));
        QVERIFY(head->text(QAccessible::Description).contains(QLatin1String("03.500")));
    }
    // a paused seek moves the cursor only; Resume decides what catches up
    if (state == QLatin1String("paused"))
    {
        QVERIFY(ed.show->isPaused());
        QVERIFY(manager->isPaused());
        QVERIFY(manager->m_cursorMovedDuringPause);
    }
    else
    {
        QVERIFY(!manager->isPlaying());
    }
    QCOMPARE(ed.show->commandTrack().count(), 7);
}

void ShowCommandRecorder_Test::timeline_groupsSelectAndRevealSamples()
{
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto track = ed.show->commandTrack();
    QVERIFY(track.insert(ShowCommand::setSliderPosition(90, 1800, ed.fader->recordingId(),
        ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), .8)));
    QVERIFY(track.insert(ShowCommand::setSliderPosition(8, 2200, ed.fader->recordingId(),
        ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), .2)));
    QVERIFY(ed.show->setCommandTrack(track));
    QCoreApplication::processEvents();
    const auto before = ed.show->commandTrack().commands();
    const QString itemId = QStringLiteral("recordingItem-%1-90").arg(ed.show->id());
    QAccessibleInterface *group = accessibleById(ed.rig.app.get(), itemId);
    QVERIFY2(group, "Timeline must expose its grouped recording object");
    QVERIFY(group->text(QAccessible::Name).contains(QStringLiteral("2 samples")));
    QVERIFY(group->text(QAccessible::Name).contains(QStringLiteral("80%")));
    QVERIFY(group->text(QAccessible::Name).contains(QStringLiteral("20%")));
    QVERIFY(performAccessibleAction(group, QAccessibleActionInterface::pressAction()));
    QVERIFY(group->state().selected);
    QCOMPARE(ed.show->commandTrack().commands(), before);
    auto *item = qobject_cast<QQuickItem *>(group->object());
    QVERIFY(item);
    QTest::mouseDClick(ed.rig.app.get(), Qt::LeftButton, Qt::NoModifier, ed.centerOf(item));
    QTRY_VERIFY(ed.item(QStringLiteral("recordingsView")) != nullptr);
    QCOMPARE(ed.selectedIds(), (QVector<quint32>{90, 8}));
    QVERIFY(ed.doubleClick(8, "valueCell"));
    QTRY_VERIFY(ed.editor() != nullptr);
    ed.type(QStringLiteral("35"));
    QCOMPARE(ed.command(8).position, .35);
    QCOMPARE(ed.command(90).position, .8);
}

void ShowCommandRecorder_Test::timeline_clipSelectionTakesKeyboardFocus_data()
{
    QTest::addColumn<bool>("keyboard");
    QTest::newRow("from recording") << false;
    QTest::newRow("from clip keyboard selection") << true;
}

void ShowCommandRecorder_Test::timeline_clipSelectionTakesKeyboardFocus()
{
    QFETCH(bool, keyboard);
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto track = ed.show->commandTrack();
    QVERIFY(track.insert(ShowCommand::setSliderPosition(90, 3000, ed.fader->recordingId(),
        ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), .8)));
    QVERIFY(ed.show->setCommandTrack(track));
    auto *area = ed.item(QStringLiteral("showItemsArea"));
    QVERIFY(area);
    auto *content = qvariant_cast<QQuickItem *>(area->property("contentItem"));
    QVERIFY(content);
    ShowFunction source(0);
    source.setFunctionID(ed.scene->id());
    source.setDuration(5000);
    ed.rig.manager->addItems(content, -1, 5000, {ed.scene->id()}, &source);
    QCoreApplication::processEvents();
    auto *group = accessibleById(ed.rig.app.get(),
        QStringLiteral("recordingItem-%1-90").arg(ed.show->id()));
    QVERIFY(group);
    auto *groupItem = qobject_cast<QQuickItem *>(group->object());
    QVERIFY(groupItem);
    ed.click(groupItem);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    QVERIFY(group->state().selected);
    QVERIFY(!ed.rig.manager->isPlaying());

    QQuickItem *clip = nullptr;
    for (auto *child : content->childItems())
        if (child->property("sfRef").isValid())
            clip = child;
    QVERIFY(clip);
    if (keyboard)
    {
        auto *selection = findVisualItem(clip, QStringLiteral("clipSelection"));
        QVERIFY(selection);
        selection->forceActiveFocus();
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    }
    ed.click(clip);
    QVERIFY(clip->property("isSelected").toBool());
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    QCoreApplication::processEvents();
    const bool played = ed.rig.manager->isPlaying();
    const bool selected = clip->property("isSelected").toBool();
    if (played)
    {
        ed.rig.manager->stopShow();
        tickAndDeliver(ed.rig.doc, 3);
    }
    QVERIFY2(played, "Space must play after a pointer switches from a recording to an ordinary clip");
    QVERIFY(selected);
    QVERIFY(!group->state().selected);
}

void ShowCommandRecorder_Test::timeline_localKeysDoNotReachVc_data()
{
    QTest::addColumn<bool>("transfer");
    QTest::addColumn<bool>("retarget");
    QTest::addColumn<int>("selectionModifiers");
    QTest::newRow("repeat and release") << false << false << int(Qt::NoModifier);
    QTest::newRow("focus transfer during hold") << true << false << int(Qt::NoModifier);
    for (const auto modifiers : {Qt::NoModifier, Qt::ShiftModifier, Qt::ControlModifier, Qt::MetaModifier})
        QTest::newRow(qPrintable(QStringLiteral("recording retarget %1").arg(int(modifiers))))
            << false << true << int(modifiers);
}

void ShowCommandRecorder_Test::timeline_vcOriginalRelease_data()
{
    QTest::addColumn<bool>("transfer");
    QTest::newRow("modifier released first") << false;
    QTest::newRow("VC press Show release") << true;
}

void ShowCommandRecorder_Test::timeline_vcOriginalRelease()
{
    QFETCH(bool, transfer);
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto *vc = qobject_cast<VirtualConsole *>(ed.rig.context("virtualConsole"));
    vc->setEditMode(false);
    ed.button->setActionType(VCButton::Flash);
    ed.button->addKeySequence(QKeySequence(Qt::ALT | Qt::Key_Right), 0);
    vc->page(vc->selectedPage())->buildKeySequenceMap();
    ed.rig.app->rootObject()->forceActiveFocus();
    QTest::keyPress(ed.rig.app.get(), Qt::Key_Right, Qt::AltModifier);
    tickAndDeliver(ed.rig.doc, 2);
    QCOMPARE(ed.button->state(), VCButton::Active);
    if (transfer)
        ed.item(QStringLiteral("timelineTab"))->forceActiveFocus();
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier);
    QCoreApplication::sendEvent(ed.rig.app.get(), &release);
    tickAndDeliver(ed.rig.doc, 2);
    QCOMPARE(ed.button->state(), VCButton::Inactive);
    QCOMPARE(ed.command(2).time, quint32(1700));
}

void ShowCommandRecorder_Test::timeline_localControlLifecycle_data()
{
    QTest::addColumn<QString>("owner");
    QTest::newRow("field") << QStringLiteral("field");
    QTest::newRow("field command navigation release") << QStringLiteral("fieldNavigation");
    QTest::newRow("record") << QStringLiteral("record");
    QTest::newRow("cancel then return") << QStringLiteral("cancel");
}

void ShowCommandRecorder_Test::timeline_localControlLifecycle()
{
    QFETCH(QString, owner);
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto *vc = qobject_cast<VirtualConsole *>(ed.rig.context("virtualConsole"));
    vc->setEditMode(false);
    ed.button->setActionType(VCButton::Flash);
    const bool navigation = owner == QLatin1String("fieldNavigation");
    const int key = navigation ? Qt::Key_Left
        : owner == QLatin1String("record") ? Qt::Key_Space : Qt::Key_Right;
    ed.button->addKeySequence(QKeySequence(key), 0);
    if (navigation)
        ed.button->addKeySequence(QKeySequence(Qt::CTRL | Qt::Key_Left), 0);
    vc->page(vc->selectedPage())->buildKeySequenceMap();
    if (owner.startsWith(QLatin1String("field")))
    {
        QVERIFY(ed.openRecordings());
        auto *filter = ed.item(QStringLiteral("recordingsFilter"));
        filter->forceActiveFocus();
        ed.type(QStringLiteral("ab"), Qt::Key_unknown);
        if (navigation)
        {
            ed.button->requestUserStateChange(true);
            tickAndDeliver(ed.rig.doc, 2);
            QCOMPARE(ed.button->state(), VCButton::Active);
            QTest::keyPress(ed.rig.app.get(), Qt::Key_Left, Qt::ControlModifier);
            QKeyEvent release(QEvent::KeyRelease, Qt::Key_Left, Qt::NoModifier);
            QCoreApplication::sendEvent(ed.rig.app.get(), &release);
            QCOMPARE(filter->property("cursorPosition").toInt(), 0);
        }
        else
        {
            QTest::keyClick(ed.rig.app.get(), Qt::Key_Home);
            QTest::keyClick(ed.rig.app.get(), Qt::Key_Right);
            QCOMPARE(filter->property("cursorPosition").toInt(), 1);
        }
    }
    else if (owner == QLatin1String("record"))
    {
        auto *rec = ed.item(QStringLiteral("recordSwitch"));
        QVERIFY(rec);
        rec->forceActiveFocus();
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
        QVERIFY(ed.rig.recorder->isRecording());
        QVERIFY(!ed.rig.manager->isPlaying());
        QVERIFY(ed.rig.recorder->setRecording(false));
    }
    else
    {
        ed.rig.manager->setBpmNumber(120);
        ed.rig.manager->setTimeDivision(Show::BPM_4_4);
        auto *group = accessibleById(ed.rig.app.get(),
            QStringLiteral("recordingItem-%1-2").arg(ed.show->id()));
        QVERIFY(group);
        QVERIFY(performAccessibleAction(group, QAccessibleActionInterface::pressAction()));
        QTest::keyPress(ed.rig.app.get(), Qt::Key_Right);
        QCoreApplication::processEvents();
        ed.item(QStringLiteral("timelineTab"))->forceActiveFocus();
        group = accessibleById(ed.rig.app.get(), QStringLiteral("recordingItem-%1-2").arg(ed.show->id()));
        qobject_cast<QQuickItem *>(group->object())->forceActiveFocus();
        QKeyEvent repeat(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, {}, true);
        QCoreApplication::sendEvent(ed.rig.app.get(), &repeat);
        QTest::keyRelease(ed.rig.app.get(), Qt::Key_Right);
        QCOMPARE(ed.command(2).time, quint32(3700));
    }
    tickAndDeliver(ed.rig.doc, 2);
    QCOMPARE(ed.button->state(), navigation ? VCButton::Active : VCButton::Inactive);
    if (navigation)
        ed.button->requestUserStateChange(false);
}

void ShowCommandRecorder_Test::timeline_localKeysDoNotReachVc()
{
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    ed.rig.manager->setBpmNumber(120);
    ed.rig.manager->setTimeDivision(Show::BPM_4_4);
    auto *vc = qobject_cast<VirtualConsole *>(ed.rig.context("virtualConsole"));
    QVERIFY(vc);
    vc->setEditMode(false);
    ed.button->setActionType(VCButton::Flash);
    ed.button->addKeySequence(QKeySequence(Qt::Key_Right), 0);
    vc->page(vc->selectedPage())->buildKeySequenceMap();
    ed.rig.app->rootObject()->forceActiveFocus();
    QTest::keyPress(ed.rig.app.get(), Qt::Key_Right);
    tickAndDeliver(ed.rig.doc, 2);
    QCOMPARE(ed.button->state(), VCButton::Active);
    QTest::keyRelease(ed.rig.app.get(), Qt::Key_Right);
    tickAndDeliver(ed.rig.doc, 2);
    QCOMPARE(ed.button->state(), VCButton::Inactive);
    auto *group = accessibleById(ed.rig.app.get(),
        QStringLiteral("recordingItem-%1-2").arg(ed.show->id()));
    QVERIFY(group);
    QVERIFY(performAccessibleAction(group, QAccessibleActionInterface::pressAction()));
    auto *item = qobject_cast<QQuickItem *>(group->object());
    QVERIFY(item);
    item->forceActiveFocus();
    QFETCH(bool, transfer);
    QFETCH(bool, retarget);
    QFETCH(int, selectionModifiers);
    QKeyEvent overrideEvent(QEvent::ShortcutOverride, Qt::Key_Right, Qt::NoModifier);
    overrideEvent.ignore();
    QCoreApplication::sendEvent(ed.rig.app.get(), &overrideEvent);
    QVERIFY(overrideEvent.isAccepted());
    QTest::keyPress(ed.rig.app.get(), Qt::Key_Right);
    tickAndDeliver(ed.rig.doc, 2);
    QCOMPARE(ed.button->state(), VCButton::Inactive);
    QCOMPARE(ed.command(2).time, quint32(3700));
    if (transfer)
        ed.item(QStringLiteral("timelineTab"))->forceActiveFocus();
    if (retarget)
    {
        auto *other = accessibleById(ed.rig.app.get(),
            QStringLiteral("recordingItem-%1-5").arg(ed.show->id()));
        QVERIFY(other);
        auto *target = qobject_cast<QQuickItem *>(other->object());
        for (int i = 0; i < 15 && !target->hasActiveFocus(); ++i)
            QTest::keyClick(ed.rig.app.get(), Qt::Key_Backtab, Qt::ShiftModifier);
        QVERIFY(target->hasActiveFocus());
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Space, Qt::KeyboardModifiers(selectionModifiers));
    }
    QKeyEvent repeat(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, {}, true);
    QCoreApplication::sendEvent(ed.rig.app.get(), &repeat);
    QTest::keyRelease(ed.rig.app.get(), Qt::Key_Right);
    tickAndDeliver(ed.rig.doc, 2);
    QCOMPARE(ed.button->state(), VCButton::Inactive);
    QCOMPARE(ed.command(5).time, quint32(2500));
    QCOMPARE(ed.command(2).time, quint32(transfer || retarget ? 3700 : 5700));
}

void ShowCommandRecorder_Test::timeline_feedbackRemainsReachable_data()
{
    QTest::addColumn<int>("tracks");
    QTest::addColumn<int>("height");
    QTest::newRow("recordings only") << 0 << 900;
    QTest::newRow("five tracks") << 5 << 900;
    QTest::newRow("seven tracks") << 7 << 900;
    QTest::newRow("seven tracks small viewport") << 7 << 480;
}

void ShowCommandRecorder_Test::timeline_keyboardCellAndReturn_data()
{
    QTest::addColumn<bool>("value");
    QTest::newRow("time field") << false;
    QTest::newRow("value field") << true;
}

void ShowCommandRecorder_Test::timeline_keyboardCellAndReturn()
{
    QFETCH(bool, value);
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto *group = accessibleById(ed.rig.app.get(),
        QStringLiteral("recordingItem-%1-2").arg(ed.show->id()));
    QVERIFY(group);
    QVERIFY(performAccessibleAction(group, QAccessibleActionInterface::pressAction()));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Return);
    QTRY_VERIFY(ed.item(QStringLiteral("recordingsView")));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Tab);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    QCOMPARE(ed.selectedIds(), (QVector<quint32>{2}));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Tab);
    if (value)
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Tab);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_F2);
    QTRY_VERIFY(ed.editor());
    ed.type(value ? QStringLiteral("65") : QStringLiteral("1.875"));
    QCOMPARE(ed.command(2).time, quint32(value ? 1700 : 1875));
    QCOMPARE(ed.command(2).position, value ? .65 : .4);
    auto *tab = ed.item(QStringLiteral("timelineTab"));
    QVERIFY(tab);
    // Follow the real tab chain rather than invoking the tab's click handler.
    for (int i = 0; i < 120 && !tab->hasActiveFocus(); ++i)
    {
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Backtab, Qt::ShiftModifier);
    }
    QVERIFY(tab->hasActiveFocus());
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    QTRY_VERIFY(ed.item(QStringLiteral("recordingLane"))->hasActiveFocus());
    QVERIFY(!ed.rig.manager->isPlaying());
    auto *restored = accessibleById(ed.rig.app.get(),
        QStringLiteral("recordingItem-%1-2").arg(ed.show->id()));
    QVERIFY(restored && restored->state().selected);
}

void ShowCommandRecorder_Test::timeline_numericAuthoredSelection_data()
{
    QTest::addColumn<int>("division");
    QTest::addColumn<bool>("endpoint");
    QTest::addColumn<QString>("refusal");
    for (const int division : {int(Show::Time), int(Show::BPM_4_4), int(Show::VDJBeat)})
        for (const bool endpoint : {false, true})
            QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(division).arg(endpoint)))
                << division << endpoint << QString();
    QTest::newRow("locked final member") << int(Show::Time) << false << QStringLiteral("locked");
    QTest::newRow("overflow final member") << int(Show::Time) << false << QStringLiteral("overflow");
    QTest::newRow("implicit duration") << int(Show::Time) << true << QStringLiteral("implicit");
    QTest::newRow("endpoint overlap") << int(Show::Time) << true << QStringLiteral("overlap");
    QTest::newRow("endpoint minimum") << int(Show::Time) << true << QStringLiteral("minimum");
    QTest::newRow("adjacent selected members") << int(Show::Time) << false << QStringLiteral("adjacent");
    QTest::newRow("nearest native beat units") << int(Show::Time) << false << QStringLiteral("rounding");
    QTest::newRow("missing Function") << int(Show::Time) << false << QStringLiteral("missing");
}

void ShowCommandRecorder_Test::timeline_numericKeyboard_data()
{
    QTest::addColumn<int>("division");
    QTest::addColumn<bool>("ordinary");
    QTest::addColumn<bool>("endpoint");
    for (const int division : {int(Show::Time), int(Show::BPM_4_4), int(Show::VDJBeat)})
        for (const bool ordinary : {false, true})
            for (const bool endpoint : {false, true})
                QTest::newRow(qPrintable(QStringLiteral("%1-%2-%3").arg(division).arg(ordinary).arg(endpoint)))
                    << division << ordinary << endpoint;
}

void ShowCommandRecorder_Test::timeline_timingFieldFocus()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    const auto stop = qScopeGuard([&]() {
        tickAndDeliver(ed.rig.doc, 2);
        ed.rig.manager->stopShow();
        tickAndDeliver(ed.rig.doc, 3);
    });
    auto *area = ed.item(QStringLiteral("showItemsArea"));
    auto *content = qvariant_cast<QQuickItem *>(area->property("contentItem"));
    ShowFunction source(0);
    source.setDuration(2000);
    ed.rig.manager->addItems(content, -1, 5000, {ed.scene->id()}, &source);
    QCoreApplication::processEvents();
    auto *clip = findVisualItem(content, QStringLiteral("clipSelection"));
    QVERIFY(clip);
    clip->forceActiveFocus();
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    auto reach = [&](QQuickItem *target) {
        for (int i = 0; i < 180 && !target->hasActiveFocus(); ++i)
            QTest::keyClick(ed.rig.app.get(), Qt::Key_Tab);
        return target->hasActiveFocus();
    };
    auto *tool = ed.item(QStringLiteral("timingSettingsButton"));
    QVERIFY(tool && reach(tool));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    QTRY_VERIFY(ed.item(QStringLiteral("timingStartButton")));
    auto *start = ed.item(QStringLiteral("timingStartButton"));
    QTRY_VERIFY(start->width() > 0);
    QVERIFY(reach(start));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    auto *hours = ed.item(QStringLiteral("timingHours"));
    QVERIFY(hours);
    QTRY_VERIFY(hours->hasActiveFocus());
    auto *millis = ed.item(QStringLiteral("timingMillis"));
    QVERIFY(millis && reach(millis));
    auto *sf = ed.show->tracks().first()->showFunctions().first();
    const quint32 before = sf->startTime();
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Up);
    QCOMPARE(sf->startTime(), before + 1);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Escape);
    QCOMPARE(sf->startTime(), before + 1);
    QTRY_VERIFY(start->hasActiveFocus());
}

void ShowCommandRecorder_Test::timeline_localModalDoesNotReachVc()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto *vc = qobject_cast<VirtualConsole *>(ed.rig.context("virtualConsole"));
    vc->setEditMode(false);
    ed.button->addKeySequence(QKeySequence(Qt::Key_Escape), 0);
    vc->page(vc->selectedPage())->buildKeySequenceMap();
    auto *area = ed.item(QStringLiteral("showItemsArea"));
    auto *content = qvariant_cast<QQuickItem *>(area->property("contentItem"));
    ShowFunction source(0);
    source.setDuration(2000);
    ed.rig.manager->addItems(content, -1, 5000, {ed.scene->id()}, &source);
    QCoreApplication::processEvents();
    auto *clip = findVisualItem(content, QStringLiteral("clipSelection"));
    QVERIFY(clip);
    clip->forceActiveFocus();
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    auto *remove = findVisualItemWith(ed.root(), "tooltip", QStringLiteral("Remove the selected items"));
    QVERIFY(remove);
    remove->forceActiveFocus();
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    QTest::qWait(150);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Escape);
    tickAndDeliver(ed.rig.doc, 2);
    QCOMPARE(ed.button->state(), VCButton::Inactive);
    QCOMPARE(ed.show->tracks().first()->showFunctions().size(), 1);
}

void ShowCommandRecorder_Test::timeline_contextEntryAndVisibleFocus()
{
    EditorRig ed;
    QVERIFY(ed.setUp());
    QTest::keyClick(ed.rig.app.get(), Qt::Key_2, Qt::AltModifier);
    QCoreApplication::processEvents();
    QTest::keyClick(ed.rig.app.get(), Qt::Key_4, Qt::AltModifier);
    QCoreApplication::processEvents();
    auto *focus = ed.rig.app->activeFocusItem();
    while (focus && !focus->property("showKeyScope").toBool())
        focus = focus->parentItem();
    QVERIFY2(focus, "Entering Show Manager must establish a local keyboard focus owner");
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Tab);
    QVERIFY(ed.rig.app->activeFocusItem() != focus);
    QCOMPARE(ed.show->commandTrack().count(), 7);
}

void ShowCommandRecorder_Test::timeline_keyboardOffscreenFocus_data()
{
    QTest::addColumn<bool>("ordinary");
    QTest::newRow("ordinary seventh track") << true;
    QTest::newRow("recording after seven tracks") << false;
}

void ShowCommandRecorder_Test::timeline_recordingButtonsFromKeyboard_data()
{
    QTest::addColumn<bool>("table");
    QTest::addColumn<bool>("undo");
    QTest::newRow("lane Snap") << false << false;
    QTest::newRow("lane Undo") << false << true;
    QTest::newRow("table Snap") << true << false;
    QTest::newRow("table Undo") << true << true;
}

void ShowCommandRecorder_Test::timeline_recordingButtonsFromKeyboard()
{
    QFETCH(bool, table);
    QFETCH(bool, undo);
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    ed.rig.manager->setBpmNumber(120);
    ed.rig.manager->setTimeDivision(Show::BPM_4_4);
    auto *group = accessibleById(ed.rig.app.get(),
        QStringLiteral("recordingItem-%1-2").arg(ed.show->id()));
    QVERIFY(group);
    QVERIFY(performAccessibleAction(group, QAccessibleActionInterface::pressAction()));
    if (table)
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Return);
    if (undo)
    {
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Delete);
        QCOMPARE(ed.show->commandTrack().count(), 6);
        QTest::qWait(200);
    }
    QQuickItem *target = nullptr;
    if (table)
        target = ed.item(undo ? QStringLiteral("deleteUndo") : QStringLiteral("snapButton"));
    else
    {
        auto *action = accessibleById(ed.rig.app.get(),
            undo ? QStringLiteral("recordingDeleteUndo") : QStringLiteral("recordingSnap"));
        QVERIFY(action);
        target = qobject_cast<QQuickItem *>(action->object());
    }
    QVERIFY(target);
    for (int i = 0; i < 200 && !target->hasActiveFocus(); ++i)
    {
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Tab);
        QCoreApplication::processEvents();
    }
    QVERIFY2(target->hasActiveFocus(), "The existing recording action must be reachable by Tab");
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    QCOMPARE(ed.show->commandTrack().count(), 7);
    QCOMPARE(ed.command(2).time, quint32(undo ? 1700 : 1500));
    QVERIFY(!ed.rig.manager->isPlaying());
}

void ShowCommandRecorder_Test::timeline_keyboardOffscreenFocus()
{
    QFETCH(bool, ordinary);
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    ed.rig.app->resize(1600, 480);
    auto *area = ed.item(QStringLiteral("showItemsArea"));
    auto *content = qvariant_cast<QQuickItem *>(area->property("contentItem"));
    ShowFunction source(0);
    source.setDuration(2000);
    for (int i = 0; i < 7; ++i)
        ed.rig.manager->addItems(content, -1, 20000, {ed.scene->id()}, &source);
    QCoreApplication::processEvents();
    QQuickItem *target = nullptr;
    if (ordinary)
    {
        for (auto *item : content->childItems())
            if (item->property("trackIndex").isValid() && item->property("trackIndex").toInt() == 6)
                target = findVisualItem(item, QStringLiteral("clipSelection"));
    }
    else
    {
        auto *group = accessibleById(ed.rig.app.get(),
            QStringLiteral("recordingItem-%1-2").arg(ed.show->id()));
        QVERIFY(group);
        target = qobject_cast<QQuickItem *>(group->object());
    }
    QVERIFY(target);
    ed.item(QStringLiteral("timelineTab"))->forceActiveFocus();
    for (int i = 0; i < 220 && !target->hasActiveFocus(); ++i)
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Tab);
    QVERIFY(target->hasActiveFocus());
    auto *viewport = area->parentItem();
    while (viewport && !viewport->property("totalTracksHeight").isValid())
        viewport = viewport->parentItem();
    QVERIFY(viewport);
    const QRectF rect = target->mapRectToScene(target->boundingRect());
    QVERIFY2(viewport->mapRectToScene(viewport->boundingRect()).contains(rect),
             "The focused selection control must scroll into the visible timeline");
    QCOMPARE(ed.show->commandTrack().count(), 7);
    QCOMPARE(ed.rig.manager->selectedItemsCount(), 0);
    QVERIFY(!ed.rig.manager->isPlaying());
}

void ShowCommandRecorder_Test::timeline_clipboardAndDeleteKeepTheirDomain_data()
{
    QTest::addColumn<QString>("control");
    for (const auto *control : {"", "recordingMoveEarlier", "recordingMoveLater", "recordingSnap", "recordingDelete"})
        QTest::newRow(*control ? control : "recording delegate") << QString::fromLatin1(control);
}

void ShowCommandRecorder_Test::timeline_clipboardAndDeleteKeepTheirDomain()
{
    QFETCH(QString, control);
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    ed.rig.manager->setBpmNumber(120);
    ed.rig.manager->setTimeDivision(Show::BPM_4_4);
    auto *area = ed.item(QStringLiteral("showItemsArea"));
    auto *content = qvariant_cast<QQuickItem *>(area->property("contentItem"));
    ShowFunction source(0);
    source.setDuration(2000);
    ed.rig.manager->addItems(content, -1, 5000, {ed.scene->id()}, &source);
    QCoreApplication::processEvents();
    auto *clip = findVisualItem(content, QStringLiteral("clipSelection"));
    QVERIFY(clip);
    clip->forceActiveFocus();
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_C, Qt::ControlModifier);
    QCOMPARE(ed.rig.manager->clipboardItemsCount(), 1);
    ed.rig.manager->setSelectedTrackId(ed.show->tracks().first()->id());
    ed.rig.manager->setCurrentTime(10000);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_V, Qt::ControlModifier);
    QCOMPARE(ed.show->tracks().first()->showFunctions().size(), 2);
    ed.rig.manager->setCurrentTime(20000);
    auto *group = accessibleById(ed.rig.app.get(),
        QStringLiteral("recordingItem-%1-2").arg(ed.show->id()));
    QVERIFY(group);
    QVERIFY(performAccessibleAction(group, QAccessibleActionInterface::pressAction()));
    if (!control.isEmpty())
    {
        auto *action = accessibleById(ed.rig.app.get(), control);
        QVERIFY(action);
        auto *target = qobject_cast<QQuickItem *>(action->object());
        QVERIFY(target);
        for (int i = 0; i < 200 && !target->hasActiveFocus(); ++i)
            QTest::keyClick(ed.rig.app.get(), Qt::Key_Tab);
        QVERIFY(target->hasActiveFocus());
        if (control == QLatin1String("recordingMoveLater"))
        {
            QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
            QCOMPARE(ed.command(2).time, quint32(3700));
            QVERIFY(target->hasActiveFocus());
        }
    }
    QCOMPARE(ed.rig.manager->selectedItemsCount(), 0);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_C, Qt::ControlModifier);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_V, Qt::ControlModifier);
    QCOMPARE(ed.rig.manager->clipboardItemsCount(), 1);
    QCOMPARE(ed.show->tracks().first()->showFunctions().size(), 2);
    QCOMPARE(ed.show->commandTrack().count(), 7);
    group = accessibleById(ed.rig.app.get(),
        QStringLiteral("recordingItem-%1-2").arg(ed.show->id()));
    QVERIFY(group);
    qobject_cast<QQuickItem *>(group->object())->forceActiveFocus();
    const quint32 beforeDelete = ed.command(2).time;
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Delete);
    QCOMPARE(ed.show->commandTrack().count(), 6);
    QCOMPARE(ed.show->tracks().first()->showFunctions().size(), 2);
    const int serial = ed.rig.recorder->lastEditSerial();
    QTest::qWait(200);
    QVERIFY(ed.tardis()->undoCommandEdit(serial));
    QCOMPARE(ed.command(2).time, beforeDelete);
}

void ShowCommandRecorder_Test::timeline_numericKeyboard()
{
    QFETCH(int, division);
    QFETCH(bool, ordinary);
    QFETCH(bool, endpoint);
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    const auto stop = qScopeGuard([&]() {
        tickAndDeliver(ed.rig.doc, 2);
        ed.rig.manager->stopShow();
        tickAndDeliver(ed.rig.doc, 3);
    });
    ed.rig.manager->setBpmNumber(120);
    ed.rig.manager->setTimeDivision(Show::TimeDivision(division));
    ShowFunction *sf = nullptr;
    QQuickItem *selection = nullptr;
    if (ordinary)
    {
        auto *area = ed.item(QStringLiteral("showItemsArea"));
        auto *content = qvariant_cast<QQuickItem *>(area->property("contentItem"));
        ShowFunction source(0);
        source.setDuration(2000);
        ed.rig.manager->addItems(content, -1, 5000, {ed.scene->id()}, &source);
        sf = ed.show->tracks().first()->showFunctions().first();
        sf->setStartTime(5000);
        QCoreApplication::processEvents();
        selection = findVisualItem(content, QStringLiteral("clipSelection"));
        QVERIFY2(selection, "An ordinary clip needs an explicit keyboard selection affordance");
    }
    else
    {
        auto *group = accessibleById(ed.rig.app.get(),
            QStringLiteral("recordingItem-%1-2").arg(ed.show->id()));
        QVERIFY(group);
        selection = qobject_cast<QQuickItem *>(group->object());
    }
    auto reach = [&](QQuickItem *target) {
        for (int i = 0; i < 160 && !target->hasActiveFocus(); ++i)
            QTest::keyClick(ed.rig.app.get(), Qt::Key_Tab);
        return target->hasActiveFocus();
    };
    ed.item(QStringLiteral("timelineTab"))->forceActiveFocus();
    QVERIFY(reach(selection));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Space);
    QVERIFY(!ed.rig.manager->isPlaying());
    QCOMPARE(ed.rig.manager->selectedItemsCount(), ordinary ? 1 : 0);
    auto *chooser = ed.item(QStringLiteral("moveStep"));
    QVERIFY2(chooser, "Move by must be visible from the Timeline");
    QVERIFY(reach(chooser));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_End);
    QCOMPARE(chooser->property("currentIndex").toInt(), 3);
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Tab);
    auto *amount = ed.item(QStringLiteral("moveTimeDelta"));
    QVERIFY(amount && amount->hasActiveFocus());
    ed.type(QStringLiteral("0.125"));
    QVERIFY(reach(selection));
    QTest::keyClick(ed.rig.app.get(), Qt::Key_Right, endpoint ? Qt::AltModifier : Qt::NoModifier);
    QCoreApplication::processEvents();
    if (ordinary)
    {
        QCOMPARE(sf->startTime(), quint32(endpoint ? 5000 : 5125));
        QCOMPARE(sf->duration(), quint32(endpoint ? 2125 : 2000));
        QCOMPARE(ed.rig.manager->selectedItemsCount(), 1);
        if (endpoint)
        {
            QTest::keyPress(ed.rig.app.get(), Qt::Key_Right, Qt::AltModifier);
            QKeyEvent modifierUp(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
            QCoreApplication::sendEvent(ed.rig.app.get(), &modifierUp);
            QKeyEvent repeat(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, {}, true);
            QCoreApplication::sendEvent(ed.rig.app.get(), &repeat);
            QTest::keyRelease(ed.rig.app.get(), Qt::Key_Right);
            QCOMPARE(sf->startTime(), quint32(5000));
            QCOMPARE(sf->duration(), quint32(2375));
        }
    }
    else
    {
        QCOMPARE(ed.command(2).time, quint32(endpoint ? 1700 : 1825));
        if (endpoint)
        {
            auto *reason = ed.item(QStringLiteral("timelineKeyboardFeedback"));
            QVERIFY(reason && reason->property("label").toString().contains(QStringLiteral("endpoint")));
        }
    }
}

void ShowCommandRecorder_Test::timeline_numericAuthoredSelection()
{
    QFETCH(int, division);
    QFETCH(bool, endpoint);
    QFETCH(QString, refusal);
    const bool success = refusal.isEmpty() || refusal == QLatin1String("adjacent")
        || refusal == QLatin1String("rounding");
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto *manager = ed.rig.manager;
    manager->setBpmNumber(refusal == QLatin1String("rounding") ? 123 : 120);
    manager->setTimeDivision(Show::TimeDivision(division));
    auto *beats = new Scene(ed.rig.doc);
    beats->setTempoType(Function::Beats);
    QVERIFY(ed.rig.doc->addFunction(beats));
    auto *area = ed.item(QStringLiteral("showItemsArea"));
    QVERIFY(area);
    auto *content = qvariant_cast<QQuickItem *>(area->property("contentItem"));
    ShowFunction source(0);
    source.setDuration(2000);
    manager->addItems(content, -1, 10000, {ed.scene->id()}, &source);
    manager->addItems(content, -1, 10000, {beats->id()}, &source);
    manager->addItems(content, -1, 30000, {beats->id()}, &source);
    auto *first = ed.show->tracks().at(0)->showFunctions().first();
    auto *second = ed.show->tracks().at(1)->showFunctions().first();
    auto *untouched = ed.show->tracks().at(2)->showFunctions().first();
    first->setStartTime(10000);
    first->setDuration(2000);
    second->setStartTime(20000);
    second->setDuration(4000);
    if (refusal == QLatin1String("adjacent"))
    {
        ed.show->tracks().at(1)->removeShowFunction(second, false);
        ed.show->tracks().at(0)->addShowFunction(second);
        second->setStartTime(24000);
    }
    if (refusal == QLatin1String("missing"))
        second->setFunctionID(Function::invalidId());
    if (refusal == QLatin1String("locked"))
        second->setLocked(true);
    if (refusal == QLatin1String("overflow"))
        second->setStartTime(INT_MAX - 100);
    if (refusal == QLatin1String("implicit"))
        second->setDuration(0);
    if (refusal == QLatin1String("minimum"))
        second->setDuration(125);
    if (refusal == QLatin1String("overlap"))
    {
        auto *neighbor = new ShowFunction(99);
        neighbor->setFunctionID(beats->id());
        neighbor->setStartTime(24100);
        neighbor->setDuration(4000);
        ed.show->tracks().at(1)->addShowFunction(neighbor);
    }
    const quint32 oldStart = second->startTime(), oldDuration = second->duration();
    const quint32 otherStart = untouched->startTime(), otherDuration = untouched->duration();
    const quint32 functionDuration = beats->totalDuration();
    manager->resetItemsSelection();
    manager->setItemSelection(0, first, nullptr, true, Qt::ControlModifier);
    manager->setItemSelection(1, second, nullptr, true, Qt::ControlModifier);
    const int base = ed.settledHistory();
    QString error;
    QVERIFY(QMetaObject::invokeMethod(manager, "shiftSelectedItems", Q_RETURN_ARG(QString, error),
        Q_ARG(int, refusal == QLatin1String("minimum") ? -1 : 1), Q_ARG(double, 125.0), Q_ARG(bool, endpoint)));
    QCOMPARE(error.isEmpty(), success);
    QCOMPARE(first->startTime(), quint32(success && !endpoint ? 10125 : 10000));
    QCOMPARE(first->duration(), quint32(success && endpoint ? 2125 : 2000));
    const int nativeDelta = refusal == QLatin1String("rounding") ? 256 : 250;
    QCOMPARE(second->startTime(), oldStart + (success && !endpoint ? nativeDelta : 0));
    QCOMPARE(second->duration(), oldDuration + (success && endpoint ? nativeDelta : 0));
    QCOMPARE(untouched->startTime(), otherStart);
    QCOMPARE(untouched->duration(), otherDuration);
    QCOMPARE(beats->totalDuration(), functionDuration);
    QCOMPARE(manager->selectedItemsCount(), 2);
    if (!success)
        QCOMPARE(ed.settledHistory(), base);
    else
    {
        QTemporaryDir saved(QDir::currentPath() + QStringLiteral("/keyboard-save-XXXXXX"));
        const QString path = saved.filePath(QStringLiteral("numeric.qxw"));
        QVERIFY(ed.rig.app->saveWorkspace(path));
        auto reopened = showFromWorkspaceFile(ed.rig.doc, path, ed.show->id());
        QVERIFY(reopened);
        QCOMPARE(reopened->tracks().at(0)->showFunctions().first()->startTime(), first->startTime());
        auto *savedTrack = reopened->getTrackFromShowFunctionID(second->id());
        QVERIFY(savedTrack);
        ShowFunction *savedSecond = nullptr;
        for (auto *item : savedTrack->showFunctions())
            if (item->id() == second->id())
                savedSecond = item;
        QVERIFY(savedSecond);
        QCOMPARE(savedSecond->duration(), second->duration());
        QTRY_VERIFY(ed.tardis()->m_historyIndex > base);
        while (ed.tardis()->m_historyIndex > base)
            ed.tardis()->undoAction();
        QCOMPARE(first->startTime(), quint32(10000));
        QCOMPARE(first->duration(), quint32(2000));
        QCOMPARE(second->startTime(), oldStart);
        QCOMPARE(second->duration(), oldDuration);
    }
}

void ShowCommandRecorder_Test::timeline_feedbackRemainsReachable()
{
    QFETCH(int, tracks);
    QFETCH(int, height);
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    ed.rig.app->resize(1600, height);
    ed.rig.manager->setBpmNumber(120);
    ed.rig.manager->setTimeDivision(Show::BPM_4_4);
    auto track = ed.show->commandTrack();
    QVERIFY(track.insert(ShowCommand::setSliderPosition(90, 600, ed.fader->recordingId(),
        ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), .8)));
    QVERIFY(track.insert(ShowCommand::setSliderPosition(91, 900, ed.fader->recordingId(),
        ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), .2)));
    QVERIFY(ed.show->setCommandTrack(track));
    const auto before = ed.show->commandTrack().commands();
    auto *area = ed.item(QStringLiteral("showItemsArea"));
    QVERIFY(area);
    auto *content = qvariant_cast<QQuickItem *>(area->property("contentItem"));
    QVERIFY(content);
    ShowFunction source(0);
    source.setFunctionID(ed.scene->id());
    source.setDuration(5000);
    for (int i = 0; i < tracks; i++)
        ed.rig.manager->addItems(content, -1, 5000, {ed.scene->id()}, &source);
    QCoreApplication::processEvents();
    QQuickItem *viewport = area->parentItem();
    while (viewport && !viewport->property("totalTracksHeight").isValid())
        viewport = viewport->parentItem();
    QVERIFY(viewport);
    auto reveal = [&](QQuickItem *item)
    {
        for (int i = 0; i < 4; i++)
        {
            const auto rect = item->mapRectToScene(item->boundingRect());
            const auto bounds = viewport->mapRectToScene(viewport->boundingRect());
            if (bounds.contains(rect))
                return true;
            QQuickItem *scrollbar = nullptr;
            for (auto *child : viewport->childItems())
                if (child->property("position").isValid() && child->property("size").isValid()
                    && child->property("orientation").toInt() == Qt::Vertical)
                    scrollbar = child;
            if (!scrollbar || !scrollbar->isVisible())
                break;
            const qreal handleCenter = scrollbar->property("position").toReal()
                + scrollbar->property("size").toReal() / 2;
            const QPoint start = scrollbar->mapToScene(
                QPointF(scrollbar->width() / 2, handleCenter * scrollbar->height())).toPoint();
            const QPoint end = scrollbar->mapToScene(QPointF(scrollbar->width() / 2,
                rect.bottom() > bounds.bottom() ? scrollbar->height() - 1 : 1)).toPoint();
            QTest::mousePress(ed.rig.app.get(), Qt::LeftButton, Qt::NoModifier, start);
            QTest::mouseMove(ed.rig.app.get(), end, 30);
            QTest::mouseRelease(ed.rig.app.get(), Qt::LeftButton, Qt::NoModifier, end);
            QCoreApplication::processEvents();
        }
        qInfo() << "Unreachable feedback" << item->isVisible()
                << item->mapRectToScene(item->boundingRect())
                << viewport->mapRectToScene(viewport->boundingRect())
                << viewport->property("contentY") << viewport->property("contentHeight");
        return false;
    };
    auto itemFor = [&](const QString &id)
    {
        auto *iface = accessibleById(ed.rig.app.get(), id);
        return iface ? qobject_cast<QQuickItem *>(iface->object()) : nullptr;
    };
    const QString groupId = QStringLiteral("recordingItem-%1-90").arg(ed.show->id());
    auto *group = itemFor(groupId);
    QVERIFY(group);
    QVERIFY(reveal(group));
    ed.click(group);
    auto *remove = itemFor(QStringLiteral("recordingDelete"));
    QVERIFY(remove);
    QVERIFY(reveal(remove));
    ed.click(remove);
    QCOMPARE(ed.show->commandTrack().commands().size(), before.size() - 2);
    auto *feedback = accessibleById(ed.rig.app.get(), QStringLiteral("recordingEditFeedback"));
    QVERIFY(feedback);
    QVERIFY(feedback->text(QAccessible::Name).contains(QStringLiteral("2")));
    auto *undo = itemFor(QStringLiteral("recordingDeleteUndo"));
    QVERIFY(undo);
    QVERIFY2(reveal(undo), "Counted Undo must be reachable by scrolling the visible timeline");
    QTest::qWait(200);
    ed.click(undo);
    QCOMPARE(ed.show->commandTrack().commands(), before);

    group = itemFor(groupId);
    QVERIFY(group);
    QVERIFY(reveal(group));
    ed.click(group);
    auto *earlier = itemFor(QStringLiteral("recordingMoveEarlier"));
    QVERIFY(earlier);
    QVERIFY(reveal(earlier));
    ed.click(earlier);
    QVERIFY(!ed.rig.recorder->lastError().isEmpty());
    auto *error = accessibleById(ed.rig.app.get(), QStringLiteral("recordingEditFeedback"));
    QVERIFY(error);
    QCOMPARE(error->text(QAccessible::Name), ed.rig.recorder->lastError());
    auto *errorItem = qobject_cast<QQuickItem *>(error->object());
    QVERIFY(errorItem);
    QVERIFY2(reveal(errorItem), "Refused-edit feedback must be reachable in the visible timeline");
    ed.click(errorItem);
    QCOMPARE(ed.show->commandTrack().commands(), before);
    QVERIFY(!ed.rig.recorder->lastError().isEmpty());
}

void ShowCommandRecorder_Test::timeline_movesExactSelectionAfterRegroup_data()
{
    QTest::addColumn<bool>("beats");
    QTest::addColumn<bool>("numeric");
    QTest::addColumn<bool>("vdj");
    QTest::newRow("time ruler") << false << false << false;
    QTest::newRow("beat ruler") << true << false << false;
    QTest::newRow("numeric time") << false << true << false;
    QTest::newRow("numeric beats") << true << true << false;
    QTest::newRow("numeric VDJ without grid") << false << true << true;
}

void ShowCommandRecorder_Test::timeline_movesExactSelectionAfterRegroup()
{
    QFETCH(bool, beats);
    QFETCH(bool, numeric);
    QFETCH(bool, vdj);
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    ed.rig.manager->setBpmNumber(120);
    if (beats)
        ed.rig.manager->setTimeDivision(Show::BPM_4_4);
    if (vdj)
        ed.rig.manager->setTimeDivision(Show::VDJBeat);
    auto track = ed.show->commandTrack();
    QVERIFY(track.insert(ShowCommand::setSliderPosition(90, 1800, ed.fader->recordingId(),
        ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), .8)));
    QVERIFY(track.insert(ShowCommand::setSliderPosition(8, 2200, ed.fader->recordingId(),
        ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), .2)));
    QVERIFY(ed.show->setCommandTrack(track));
    QCoreApplication::processEvents();
    auto groupId = [&](int first) { return QStringLiteral("recordingItem-%1-%2").arg(ed.show->id()).arg(first); };
    auto *group = accessibleById(ed.rig.app.get(), groupId(90));
    QVERIFY(group);
    QVERIFY(performAccessibleAction(group, QAccessibleActionInterface::pressAction()));
    QVERIFY(ed.rig.recorder->removeCommands(ed.show->id(), {3}));
    QCoreApplication::processEvents();
    group = accessibleById(ed.rig.app.get(), groupId(2));
    QVERIFY(group);
    QVERIFY(!group->state().selected);
    QVERIFY(group->text(QAccessible::Description).contains(QStringLiteral("2 of 3")));
    if (numeric)
    {
        auto *chooser = ed.item(QStringLiteral("moveStep"));
        QVERIFY(chooser);
        chooser->forceActiveFocus();
        QTest::keyClick(ed.rig.app.get(), Qt::Key_End);
        QTest::keyClick(ed.rig.app.get(), Qt::Key_Tab);
        ed.type(QStringLiteral("0.125"));
    }
    auto *later = accessibleById(ed.rig.app.get(), QStringLiteral("recordingMoveLater"));
    QVERIFY2(later, "Timeline needs a movement action for exact selected sample IDs");
    QVERIFY2(later->rect().width() > 0 && later->rect().height() > 0,
             "Timeline movement controls must have a visible hit area");
    QVERIFY(performAccessibleAction(later, QAccessibleActionInterface::pressAction()));
    if (numeric)
    {
        QVERIFY(!later->state().disabled);
        QCOMPARE(ed.command(90).time, 1925U);
        QCOMPARE(ed.command(8).time, 2325U);
        QCOMPARE(ed.command(2).time, 1700U);
        QCOMPARE(ed.command(8).order, 8U);
        const int serial = ed.rig.recorder->lastEditSerial();
        QTest::qWait(200);
        QVERIFY(ed.tardis()->undoCommandEdit(serial));
        QCOMPARE(ed.command(90).time, 1800U);
        QCOMPARE(ed.command(8).time, 2200U);
        QCOMPARE(ed.command(2).time, 1700U);
        return;
    }
    if (!beats)
    {
        QVERIFY(later->state().disabled);
        QCOMPARE(ed.command(90).time, 1800U);
        QCOMPARE(ed.command(8).time, 2200U);
        return;
    }
    QCOMPARE(ed.command(90).time, 3800U);
    QCOMPARE(ed.command(8).time, 4200U);
    QCOMPARE(ed.command(2).time, 1700U);
    QCOMPARE(ed.command(90).position, .8);
    QCOMPARE(ed.command(8).order, 8U);
    auto *snap = accessibleById(ed.rig.app.get(), QStringLiteral("recordingSnap"));
    QVERIFY(snap);
    QVERIFY(performAccessibleAction(snap, QAccessibleActionInterface::pressAction()));
    QCOMPARE(ed.command(90).time, 4000U);
    QCOMPARE(ed.command(8).time, 4400U);
    QCOMPARE(ed.command(2).time, 1700U);
    const int serial = ed.rig.recorder->lastEditSerial();
    QTest::qWait(200);
    QVERIFY(ed.tardis()->undoCommandEdit(serial));
    QCOMPARE(ed.command(90).time, 3800U);
    QCOMPARE(ed.command(8).time, 4200U);
}

void ShowCommandRecorder_Test::timeline_dragCommitsOnceOrCancels_data()
{
    QTest::addColumn<QString>("cancel");
    QTest::newRow("commit") << QString();
    QTest::newRow("command changes") << QStringLiteral("command");
    QTest::newRow("REC starts") << QStringLiteral("rec");
}

void ShowCommandRecorder_Test::timeline_dragCommitsOnceOrCancels()
{
    QFETCH(QString, cancel);
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    auto track = ed.show->commandTrack();
    QVERIFY(track.insert(ShowCommand::setSliderPosition(90, 1800, ed.fader->recordingId(),
        ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), .8)));
    QVERIFY(track.insert(ShowCommand::setSliderPosition(8, 2200, ed.fader->recordingId(),
        ShowControlRole::AdjustSlider, QStringLiteral("Intensity"), .2)));
    QVERIFY(ed.show->setCommandTrack(track));
    QCoreApplication::processEvents();
    const auto prefix = QStringLiteral("recordingItem-%1-").arg(ed.show->id());
    auto *group = accessibleById(ed.rig.app.get(), prefix + "90");
    auto *destination = accessibleById(ed.rig.app.get(), prefix + "6");
    QVERIFY(group && destination);
    auto *item = qobject_cast<QQuickItem *>(group->object());
    auto *target = qobject_cast<QQuickItem *>(destination->object());
    QVERIFY(item && target);
    const QPoint start = ed.centerOf(item);
    const QPoint end = start + QPoint(qRound(target->x() - item->x()), 0);
    const int serial = ed.rig.recorder->lastEditSerial();
    QTest::mousePress(ed.rig.app.get(), Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(ed.rig.app.get(), end, 30);
    if (cancel == QLatin1String("command"))
        QVERIFY(ed.rig.recorder->setCommandValue(ed.show->id(), 8, .3));
    if (cancel == QLatin1String("rec"))
        QVERIFY(ed.rig.recorder->setRecording(true));
    QTest::mouseRelease(ed.rig.app.get(), Qt::LeftButton, Qt::NoModifier, end);
    QCoreApplication::processEvents();
    // The time ruler quantizes its geometry to pixels, not command times.
    QVERIFY(qAbs(qint64(ed.command(90).time) - (cancel.isEmpty() ? 4000 : 1800)) <= (cancel.isEmpty() ? 20 : 0));
    QCOMPARE(ed.command(8).time - ed.command(90).time, 400U);
    if (cancel.isEmpty())
    {
        QCOMPARE(ed.rig.recorder->lastEditSerial(), serial + 1);
        QTest::qWait(200);
        QVERIFY(ed.tardis()->undoCommandEdit(serial + 1));
        QCOMPARE(ed.command(90).time, 1800U);
        QCOMPARE(ed.command(8).time, 2200U);
    }
    if (cancel == QLatin1String("rec"))
        QVERIFY(ed.rig.recorder->setRecording(false));
}

void ShowCommandRecorder_Test::accessibleRecordings_rowsTabsAndGroupActions()
{
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    QWindow *window = ed.rig.app.get();
    ed.rig.manager->setBpmNumber(120);
    ed.rig.manager->setTimeDivision(Show::BPM_4_4);
    QCoreApplication::processEvents();

    // the editor tabs carry their visible label and switch like a click
    QAccessibleInterface *tab = accessibleByName(window, QAccessible::CheckBox, QStringLiteral("Recordings"));
    QVERIFY2(tab, "the Recordings tab has no accessible name");
    QVERIFY(performAccessibleAction(tab, QAccessibleActionInterface::toggleAction()));
    QTRY_VERIFY(ed.item(QStringLiteral("recordingsView")) != nullptr);
    QTRY_COMPARE(ed.rowIds().count(), 7);
    const int base = ed.settledHistory();

    // one row per event: its id, time, control, action and value
    QAccessibleInterface *row = accessibleById(window, QStringLiteral("recordingRow-1"));
    QVERIFY2(row, "the recorded rows are not accessible");
    QCOMPARE(row->role(), QAccessible::ListItem);
    const QString name = row->text(QAccessible::Name);
    QVERIFY2(name.contains(QLatin1String("Event 1")) && name.contains(QLatin1String("00:01.200")) &&
             name.contains(QLatin1String("Blue button")), qPrintable(name));
    QVERIFY(row->state().selectable);
    QVERIFY(!row->state().selected);
    QVERIFY(findAccessible(row, [](QAccessibleInterface *i) {
        return i->role() == QAccessible::StaticText && i->text(QAccessible::Name) == QLatin1String("00:01.200");
    }));

    QAccessibleInterface *snap = accessibleByName(window, QAccessible::Button, QStringLiteral("Snap"));
    QAccessibleInterface *later = accessibleByName(window, QAccessible::Button,
                                                   QStringLiteral("Move the selection later"));
    QVERIFY2(snap && later, "Snap and Move have no accessible names");
    QVERIFY(snap->state().disabled);
    // refused while nothing is selected: no edit, no history
    QVERIFY(performAccessibleAction(snap, QAccessibleActionInterface::pressAction()));
    QCOMPARE(ed.command(1).time, 1200u);

    // pressing a row selects it alone, like a plain click
    QVERIFY(performAccessibleAction(row, QAccessibleActionInterface::pressAction()));
    QCOMPARE(ed.selectedIds(), (QVector<quint32>{1}));
    QVERIFY(row->state().selected);
    QVERIFY(!snap->state().disabled);
    QVERIFY(!later->state().disabled);

    // Snap is the button's own action: one grouped edit, one undo step
    QVERIFY(performAccessibleAction(snap, QAccessibleActionInterface::pressAction()));
    QCOMPARE(ed.command(1).time, 1000u);
    QCOMPARE(ed.command(2).time, 1700u);
    QTRY_COMPARE(ed.tardis()->m_historyIndex, base + 1);
    QCOMPARE(ed.settledHistory(), base + 1);

    // the debug panel's tabs switch through the accessible action
    QAccessibleInterface *debug = accessibleByName(window, QAccessible::Button, QStringLiteral("Open the debug panel"));
    QVERIFY(debug);
    QVERIFY(performAccessibleAction(debug, QAccessibleActionInterface::pressAction()));
    QTRY_VERIFY(ed.item(QStringLiteral("showDebugPanel")) != nullptr);
    auto *events = qobject_cast<ShowEventModel *>(ed.rig.context("showEvents"));
    QVERIFY(events);
    QAccessibleInterface *references = accessibleByName(window, QAccessible::PageTab,
                                                        QStringLiteral("Referenced controls"));
    QVERIFY(references);
    QVERIFY(performAccessibleAction(references, QAccessibleActionInterface::pressAction()));
    QCOMPARE(events->tab(), int(ShowEventModel::ReferencesTab));
    QAccessibleInterface *eventsTab = accessibleByName(window, QAccessible::PageTab, QStringLiteral("Events"));
    QVERIFY(eventsTab);
    QVERIFY(performAccessibleAction(eventsTab, QAccessibleActionInterface::pressAction()));
    QCOMPARE(events->tab(), int(ShowEventModel::EventsTab));

    // REC through its switch: the status and the observed event read as text
    QAccessibleInterface *rec = accessibleByName(window, QAccessible::CheckBox, QStringLiteral("Record commands"));
    QVERIFY(rec);
    QVERIFY(performAccessibleAction(rec, QAccessibleActionInterface::toggleAction()));
    QVERIFY(ed.rig.recorder->isRecording());
    QAccessibleInterface *status = accessibleById(window, QStringLiteral("recordStatus"));
    QVERIFY2(status, "the REC status is not accessible");
    QCOMPARE(status->role(), QAccessible::StaticText);
    QVERIFY2(status->text(QAccessible::Name).startsWith(QLatin1String("REC: ")), qPrintable(status->text(QAccessible::Name)));
    QTRY_VERIFY(events->count() > 0);
    QAccessibleInterface *eventRow = findAccessible(QAccessible::queryAccessibleInterface(window), [](QAccessibleInterface *i) {
        return i->role() == QAccessible::ListItem && i->text(QAccessible::Identifier).startsWith(QLatin1String("debugEvent-"));
    });
    QVERIFY2(eventRow, "the observed events are not accessible");
    QVERIFY(!eventRow->text(QAccessible::Name).isEmpty());
    QVERIFY(ed.rig.recorder->setRecording(false));
}

void ShowCommandRecorder_Test::accessibleVirtualConsole_pagesAndButtons()
{
    AccessibilityOn accessibility;
    EditorRig ed;
    QVERIFY(ed.setUp());
    QVERIFY(QMetaObject::invokeMethod(ed.root(), "switchToContext", Q_ARG(QVariant, QStringLiteral("VC")),
                                      Q_ARG(QVariant, QStringLiteral("qrc:/VirtualConsole.qml"))));
    QCoreApplication::processEvents();
    auto *vc = qobject_cast<VirtualConsole *>(ed.rig.context("virtualConsole"));
    QVERIFY(vc);

    QAccessibleInterface *page = nullptr;
    QTRY_VERIFY((page = accessibleById(ed.rig.app.get(), QStringLiteral("vcPage-0"))) != nullptr);
    QCOMPARE(page->text(QAccessible::Name), vc->page(0)->caption());

    QAccessibleInterface *button = nullptr;
    QTRY_VERIFY((button = accessibleById(ed.rig.app.get(), QStringLiteral("vcButton-%1").arg(ed.button->id()))) != nullptr);
    QCOMPARE(button->text(QAccessible::Name), QStringLiteral("Blue button"));

    // a press reaches the state a mouse click reaches, and presses again like one
    QQuickItem *area = qobject_cast<QQuickItem *>(button->object());
    QVERIFY(area);
    QList<int> clicked, pressed;
    for (int i = 0; i < 2; i++)
    {
        ed.click(area);
        tickAndDeliver(ed.rig.doc, 2);
        clicked.append(int(ed.button->state()));
    }
    for (int i = 0; i < 2; i++)
    {
        QVERIFY(performAccessibleAction(button, QAccessibleActionInterface::pressAction()));
        tickAndDeliver(ed.rig.doc, 2);
        pressed.append(int(ed.button->state()));
        QCOMPARE(button->state().checked, ed.button->state() == VCButton::Active);
    }
    QCOMPARE(pressed, clicked);
    QVERIFY(clicked.first() != int(VCButton::Inactive));
    QCOMPARE(clicked.last(), int(VCButton::Inactive));

    QAccessibleInterface *fader = accessibleByName(ed.rig.app.get(), QAccessible::Slider, QStringLiteral("Fader"));
    QAccessibleInterface *dimmer = accessibleByName(ed.rig.app.get(), QAccessible::Slider, QStringLiteral("Dimmer"));
    QVERIFY2(fader && dimmer, "the VC faders have no accessible caption");
    QVERIFY(dimmer->state().disabled);
}

void ShowCommandRecorder_Test::accessibleFunctionList_activatesLikeDoubleClick_data()
{
    QTest::addColumn<QString>("resource");
    QTest::addColumn<bool>("folder");

    QTest::newRow("function row") << QStringLiteral("qrc:/FunctionDelegate.qml") << false;
    QTest::newRow("folder row") << QStringLiteral("qrc:/TreeNodeDelegate.qml") << true;
}

void ShowCommandRecorder_Test::accessibleFunctionList_activatesLikeDoubleClick()
{
    QFETCH(QString, resource);
    QFETCH(bool, folder);

    AccessibilityOn accessibility;
    AppRig rig;
    rig.app->showNormal();
    QVERIFY(QTest::qWaitForWindowExposed(rig.app.get()));
    Show *show = rig.addShow(0);
    show->setName(QStringLiteral("Test show"));

    QQmlComponent component(rig.app->engine(), QUrl(resource));
    std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem *>(component.create()));
    QVERIFY2(item, qPrintable(component.errorString()));
    item->setParentItem(rig.app->contentItem());
    item->setSize(QSizeF(300, 30));
    if (folder)
    {
        item->setProperty("textLabel", QStringLiteral("Test folder"));
        item->setProperty("nodePath", QStringLiteral("Test folder"));
    }
    else
    {
        item->setProperty("cRef", QVariant::fromValue(static_cast<Function *>(show)));
    }
    QCoreApplication::processEvents();
    QSignalSpy events(item.get(), SIGNAL(mouseEvent(int,int,int,QVariant,int)));

    const QString label = folder ? QStringLiteral("Test folder") : show->name();
    QAccessibleInterface *row = accessibleByName(rig.app.get(), QAccessible::ListItem, label);
    QVERIFY2(row, qPrintable(QStringLiteral("no accessible row named %1").arg(label)));
    QVERIFY(row->state().selectable);
    QVERIFY(!row->state().selected);
    if (!folder)
        QCOMPARE(row->text(QAccessible::Identifier), QStringLiteral("function-%1").arg(show->id()));

    // select then open, the handlers of a double click
    QVERIFY(performAccessibleAction(row, QAccessibleActionInterface::pressAction()));
    QCOMPARE(events.count(), 2);
    QCOMPARE(events.at(0).at(0).toInt(), int(App::Clicked));
    QCOMPARE(events.at(1).at(0).toInt(), int(App::DoubleClicked));
    for (const QList<QVariant> &e : std::as_const(events))
    {
        QCOMPARE(e.at(1).toInt(), folder ? -1 : int(show->id()));
        QCOMPARE(e.at(4).toInt(), 0);
    }
    item->setProperty("isSelected", true);
    QVERIFY(row->state().selected);
}

QTEST_MAIN(ShowCommandRecorder_Test)
