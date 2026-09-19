/*
  Q Light Controller Plus - Unit test
  vcbutton_test.cpp

  Copyright (c) Massimo Callegari

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

#include "vcbutton_test.h"

#include <QtTest>
#include <QBuffer>
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickView>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <functional>
#include <memory>

#include "contextmanager.h"
#include "doc.h"
#include "fixturemanager.h"
#include "fixture.h"
#include "flowconsole.h"
#include "functionmanager.h"
#include "functionparent.h"
#include "inputoutputmap.h"
#include "mastertimer.h"
#include "qlcinputsource.h"
#include "scene.h"
#define private public
#include "tardis.h"
#undef private
#include "vcbridgev5.h"
#include "vcbutton.h"
#include "vcframe.h"
#include "vcpage.h"
#include "virtualconsole.h"

namespace
{
/** The production widgets need the real VirtualConsole graph: VCButton talks to
 *  Tardis on every configuration change, and VCBridgeV5 resolves widgets through
 *  the console's widget map. */
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
        // VirtualConsole::currentPageItem() resolves the page item through the
        // view's root object, so the view needs one before widgets are added.
        m_rootComponent.reset(new QQmlComponent(m_view.engine()));
        m_rootComponent->setData("import QtQuick 2.0\n"
                                 "Item { Item { objectName: \"vcPage0\" } }",
                                 QUrl(QStringLiteral("qrc:/VCButtonTestRoot.qml")));
        QObject *root = m_rootComponent->create();
        m_view.setContent(QUrl(QStringLiteral("qrc:/VCButtonTestRoot.qml")),
                          m_rootComponent.get(), root);
        m_tardis.reset(new Tardis(&m_view, doc, nullptr, &m_fixtureManager,
                                  &m_functionManager, &m_contextManager,
                                  nullptr, nullptr, &m_vc));
        // Keep the undo thread out of the assertions; configuration still runs.
        m_tardis->m_busy = true;
    }

    ~UiFixture()
    {
        // Delete the page contents through the console's own reset path, while
        // Tardis is still alive to receive the widget-removal actions.
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

QString saveButtonXml(const VCButton &button)
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement("VirtualConsole");
    button.saveXML(&writer);
    writer.writeEndElement();
    return xml;
}

bool loadButtonXml(VCButton &button, const QString &xml)
{
    QBuffer buffer;
    buffer.setData(xml.toUtf8());
    if (!buffer.open(QIODevice::ReadOnly))
        return false;
    QXmlStreamReader reader(&buffer);
    if (!reader.readNextStartElement())
        return false;
    if (!reader.readNextStartElement())
        return false;
    return button.loadXML(reader) && !reader.hasError();
}

/** The production render path: create the widget's QML item and hand it the
 *  button, exactly as VCButton::render()/FlowConsole do. */
QQuickItem *renderButtonItem(QQuickView *view, VCButton *button, const QString &resource)
{
    QQmlComponent component(view->engine(), QUrl(resource));
    if (component.isError())
    {
        qWarning() << resource << component.errors();
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

void VCButton_Test::initTestCase()
{
    // Production QML types that App::startup() registers and the button panels use.
    qmlRegisterUncreatableType<Fixture>("org.qlcplus.classes", 1, 0, "Fixture", "Engine-owned");
    qmlRegisterUncreatableType<Function>("org.qlcplus.classes", 1, 0, "QLCFunction", "Engine-owned");

    // The app-linked target must serve the production QML, not a stale copy.
    const QList<QPair<QString, QString>> resources = {
        {QStringLiteral("../../qml/virtualconsole/VCButtonProperties.qml"), QStringLiteral(":/VCButtonProperties.qml")},
        {QStringLiteral("../../qml/virtualconsole/VCButtonItem.qml"), QStringLiteral(":/VCButtonItem.qml")},
        {QStringLiteral("../../qml/flowconsole/FlowButtonItem.qml"), QStringLiteral(":/FlowButtonItem.qml")},
    };
    for (const auto &entry : resources)
    {
        QFile source(QFINDTESTDATA(entry.first));
        QFile resource(entry.second);
        QVERIFY2(source.open(QIODevice::ReadOnly), qPrintable(entry.first));
        QVERIFY2(resource.open(QIODevice::ReadOnly), qPrintable(entry.second));
        QCOMPARE(resource.readAll(), source.readAll());
    }
}

void VCButton_Test::freezeAction_survivesXmlRoundTrip_data()
{
    QTest::addColumn<int>("action");
    QTest::addColumn<QString>("canonicalXml");

    QTest::newRow("toggle")   << int(VCButton::Toggle)   << QStringLiteral("Toggle");
    QTest::newRow("flash")    << int(VCButton::Flash)    << QStringLiteral("Flash");
    QTest::newRow("blackout") << int(VCButton::Blackout) << QStringLiteral("Blackout");
    QTest::newRow("stopall")  << int(VCButton::StopAll)  << QStringLiteral("StopAll");
    QTest::newRow("freeze")   << int(VCButton::Freeze)   << QStringLiteral("Freeze");
    QTest::newRow("freezehold") << int(VCButton::FreezeHold) << QStringLiteral("FreezeHold");
}

void VCButton_Test::freezeAction_survivesXmlRoundTrip()
{
    QFETCH(int, action);
    QFETCH(QString, canonicalXml);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);

    VCButton button(&doc);
    button.setCaption(QStringLiteral("Hold the look"));
    button.setGeometry(QRect(11, 23, 90, 40));
    button.setActionType(VCButton::ButtonAction(action));
    button.addKeySequence(QKeySequence(QStringLiteral("Ctrl+F")), 0);
    QSharedPointer<QLCInputSource> source(new QLCInputSource(2, 17));
    source->setID(0);
    button.addInputSource(source);

    const QString xml = saveButtonXml(button);
    QVERIFY2(xml.contains(QStringLiteral(">%1<").arg(canonicalXml)), qPrintable(xml));

    VCButton restored(&doc);
    QVERIFY2(loadButtonXml(restored, xml), qPrintable(xml));

    QCOMPARE(int(restored.actionType()), action);
    QCOMPARE(restored.caption(), button.caption());
    QCOMPARE(restored.keySequenceMap().value(QKeySequence(QStringLiteral("Ctrl+F")), 99u), 0u);
    QCOMPARE(restored.inputSources().size(), 1);
    QCOMPARE(restored.inputSources().first()->universe(), quint32(2));
    QCOMPARE(restored.inputSources().first()->channel(), quint32(17));
    // Loading persists configuration only; it must never replay a press.
    QCOMPARE(restored.state(), VCButton::Inactive);
}

void VCButton_Test::mcpBridge_freezeAction_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("action");

    for (const char *path : {"addButton", "setButtonAction", "configureButton"})
        for (const char *action : {"toggle", "flash", "blackout", "stopall", "freeze", "freezehold"})
            QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(path).arg(action)))
                << QString(path) << QString(action);
}

void VCButton_Test::mcpBridge_freezeAction()
{
    QFETCH(QString, path);
    QFETCH(QString, action);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    VCBridgeV5 bridge(&doc, ui.vc());

    VCPage *page = ui.vc()->page(0);
    QVERIFY(page);
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    QVERIFY(frameID >= 0);

    const QString createAction = path == QStringLiteral("addButton") ? action : QStringLiteral("toggle");
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40),
                                          Function::invalidId(),
                                          QStringLiteral("Freeze"), createAction);
    QVERIFY(buttonID >= 0);

    if (path == QStringLiteral("setButtonAction"))
        QVERIFY(bridge.setButtonAction(buttonID, action));
    else if (path == QStringLiteral("configureButton"))
    {
        VCBridge::ButtonConfig config;
        config.action = action;
        QVERIFY(bridge.configureButton(buttonID, config));
    }

    // The query surface derives its action text from the real widget.
    QCOMPARE(bridge.getWidgetDetails(buttonID).action, action);
}

void VCButton_Test::freezePress_togglesGlobalLatchOnce_data()
{
    QTest::addColumn<QString>("path");

    QTest::newRow("vc-mouse") << QStringLiteral("vc-mouse");
    QTest::newRow("vc-touch") << QStringLiteral("vc-touch");
    QTest::newRow("flow-mouse") << QStringLiteral("flow-mouse");
    QTest::newRow("flow-touch") << QStringLiteral("flow-touch");
    QTest::newRow("mapped-input") << QStringLiteral("mapped-input");
}

void VCButton_Test::freezePress_togglesGlobalLatchOnce()
{
    QFETCH(QString, path);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();
    QVERIFY(!ioMap->isFrozen());

    VCButton button(&doc);
    button.setCaption(QStringLiteral("Freeze"));
    button.setGeometry(QRect(0, 0, 160, 80));
    button.setActionType(VCButton::Freeze);
    ui.vc()->addWidgetToMap(&button);
    const auto unmap = qScopeGuard([&]() { ui.vc()->removeWidgetFromMap(&button); });

    QSignalSpy frozen(ioMap, &InputOutputMap::frozenChanged);
    QVERIFY(frozen.isValid());

    if (path == QStringLiteral("mapped-input"))
    {
        // A physical press is the rising edge. Key auto-repeat and duplicate
        // non-zero pressure arrive as repeated non-zero values.
        button.slotInputValueChanged(0, UCHAR_MAX);
        button.slotInputValueChanged(0, UCHAR_MAX);
        button.slotInputValueChanged(0, 64);
        QCOMPARE(frozen.count(), 1);
        QVERIFY(ioMap->isFrozen());
        QCOMPARE(button.state(), VCButton::Active);

        // Releasing must not toggle.
        button.slotInputValueChanged(0, 0);
        QCOMPARE(frozen.count(), 1);
        QVERIFY(ioMap->isFrozen());

        // The next press releases the latch.
        button.slotInputValueChanged(0, UCHAR_MAX);
        QCOMPARE(frozen.count(), 2);
        QVERIFY(!ioMap->isFrozen());
        QCOMPARE(button.state(), VCButton::Inactive);
        return;
    }

    const bool flow = path.startsWith(QStringLiteral("flow"));
    std::unique_ptr<FlowConsole> flowConsole;
    if (flow)
        flowConsole.reset(new FlowConsole(ui.view(), &doc));

    QQuickView *view = ui.view();
    view->resize(400, 300);
    std::unique_ptr<QQuickItem> item(renderButtonItem(view, &button,
        flow ? QStringLiteral("qrc:/FlowButtonItem.qml")
             : QStringLiteral("qrc:/VCButtonItem.qml")));
    QVERIFY(item);
    if (flow)
    {
        // FlowWidgetItem is laid out by its section; size it directly here.
        item->setWidth(160);
        item->setHeight(80);
    }
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));
    QCOMPARE(item->width(), qreal(160));

    const QPoint center(80, 40);
    QPointingDevice *touch = QTest::createTouchDevice();

    for (int press = 1; press <= 2; ++press)
    {
        // Deliver press and release as separate events. Freezing must capture
        // the look the operator saw when they pressed, so the latch has to
        // turn on the press edge, not when the finger or button comes back up.
        if (path.endsWith(QStringLiteral("touch")))
        {
            QTest::touchEvent(view, touch).press(0, center);
            QCoreApplication::processEvents();
        }
        else
        {
            QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, center);
            QCoreApplication::processEvents();
        }

        QCOMPARE(frozen.count(), press);
        QCOMPARE(ioMap->isFrozen(), press == 1);
        QCOMPARE(button.state(), press == 1 ? VCButton::Active : VCButton::Inactive);

        if (path.endsWith(QStringLiteral("touch")))
        {
            QTest::touchEvent(view, touch).release(0, center);
            QCoreApplication::processEvents();
        }
        else
        {
            QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, center);
            QCoreApplication::processEvents();
        }

        // The release adds nothing: one physical press, one toggle.
        QCOMPARE(frozen.count(), press);
        QCOMPARE(ioMap->isFrozen(), press == 1);
        QCOMPARE(button.state(), press == 1 ? VCButton::Active : VCButton::Inactive);
    }
}

void VCButton_Test::freezeHoldGesture_holdsWhilePressed_data()
{
    QTest::addColumn<QString>("path");

    for (const char *path : {"vc-mouse", "vc-touch", "flow-mouse", "flow-touch", "mapped-input"})
        QTest::newRow(path) << QString(path);
}

void VCButton_Test::freezeHoldGesture_holdsWhilePressed()
{
    QFETCH(QString, path);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();

    // A persistent latch is already held. The while-pressed gesture must never
    // disturb it, in either direction.
    ioMap->setFrozen(true);

    VCButton button(&doc);
    button.setCaption(QStringLiteral("Freeze while pressed"));
    button.setGeometry(QRect(0, 0, 160, 80));
    button.setActionType(VCButton::FreezeHold);
    ui.vc()->addWidgetToMap(&button);
    const auto unmap = qScopeGuard([&]() { ui.vc()->removeWidgetFromMap(&button); });
    QCOMPARE(button.state(), VCButton::Inactive);

    QSignalSpy momentary(ioMap, &InputOutputMap::frozenMomentaryChanged);
    QVERIFY(momentary.isValid());

    if (path == QStringLiteral("mapped-input"))
    {
        button.slotInputValueChanged(0, UCHAR_MAX);
        button.slotInputValueChanged(0, UCHAR_MAX);
        button.slotInputValueChanged(0, 64);
        QCOMPARE(momentary.count(), 1);
        QVERIFY(ioMap->frozenMomentary());
        QCOMPARE(button.state(), VCButton::Active);

        button.slotInputValueChanged(0, 0);
        QCOMPARE(momentary.count(), 2);
        QVERIFY(!ioMap->frozenMomentary());
        QCOMPARE(button.state(), VCButton::Inactive);

        QVERIFY(ioMap->frozenLatch());
        return;
    }

    const bool flow = path.startsWith(QStringLiteral("flow"));
    std::unique_ptr<FlowConsole> flowConsole;
    if (flow)
        flowConsole.reset(new FlowConsole(ui.view(), &doc));

    QQuickView *view = ui.view();
    view->resize(400, 300);
    std::unique_ptr<QQuickItem> item(renderButtonItem(view, &button,
        flow ? QStringLiteral("qrc:/FlowButtonItem.qml")
             : QStringLiteral("qrc:/VCButtonItem.qml")));
    QVERIFY(item);
    if (flow)
    {
        item->setWidth(160);
        item->setHeight(80);
    }
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));

    const QPoint center(80, 40);
    QPointingDevice *touch = QTest::createTouchDevice();
    const bool byTouch = path.endsWith(QStringLiteral("touch"));

    if (byTouch)
        QTest::touchEvent(view, touch).press(0, center);
    else
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, center);
    QCoreApplication::processEvents();

    QCOMPARE(momentary.count(), 1);
    QVERIFY(ioMap->frozenMomentary());
    QCOMPARE(button.state(), VCButton::Active);

    if (byTouch)
        QTest::touchEvent(view, touch).release(0, center);
    else
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, center);
    QCoreApplication::processEvents();

    QCOMPARE(momentary.count(), 2);
    QVERIFY(!ioMap->frozenMomentary());
    QCOMPARE(button.state(), VCButton::Inactive);

    // The persistent latch was never touched by the hold gesture.
    QVERIFY(ioMap->frozenLatch());
    QVERIFY(ioMap->isFrozen());
}

void VCButton_Test::freezeRoutesThroughRealInputDispatch_data()
{
    QTest::addColumn<int>("action");
    QTest::addColumn<QString>("route");

    for (int action : {int(VCButton::Freeze), int(VCButton::FreezeHold)})
        for (const char *route : {"keyboard", "mapped-controller"})
            QTest::newRow(qPrintable(QStringLiteral("%1-%2")
                    .arg(action == VCButton::Freeze ? "latch" : "while-pressed").arg(route)))
                << action << QString(route);
}

void VCButton_Test::freezeRoutesThroughRealInputDispatch()
{
    QFETCH(int, action);
    QFETCH(QString, route);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();
    VCBridgeV5 bridge(&doc, ui.vc());

    // A real widget living on page 0, reached the way the production dispatch
    // reaches it: through VirtualConsole and VCPage, not by calling the button.
    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    QVERIFY(frameID >= 0);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), Function::invalidId(),
                                          QStringLiteral("Freeze"),
                                          action == VCButton::Freeze ? QStringLiteral("freeze")
                                                                     : QStringLiteral("freezehold"));
    QVERIFY(buttonID >= 0);
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);
    QCOMPARE(int(button->actionType()), action);

    VCPage *page = ui.vc()->page(0);
    QVERIFY(page);

    const bool latch = action == VCButton::Freeze;
    QSignalSpy latchSpy(ioMap, &InputOutputMap::frozenLatchChanged);
    QSignalSpy momentarySpy(ioMap, &InputOutputMap::frozenMomentaryChanged);
    QSignalSpy &spy = latch ? latchSpy : momentarySpy;

    // Deliver a real press, a duplicate, and a release through the production route.
    std::function<void(bool, bool)> deliver;
    if (route == QStringLiteral("keyboard"))
    {
        const QKeySequence seq(QStringLiteral("Ctrl+F"));
        button->addKeySequence(seq, 0);
        page->buildKeySequenceMap();
        deliver = [&](bool pressed, bool autoRepeat) {
            QKeyEvent event(pressed ? QEvent::KeyPress : QEvent::KeyRelease,
                            Qt::Key_F, Qt::ControlModifier, QString(), autoRepeat);
            ui.vc()->handleKeyEvent(&event, pressed);
        };
    }
    else
    {
        QSharedPointer<QLCInputSource> source(new QLCInputSource(2, 17));
        source->setID(0);
        button->addInputSource(source);
        page->mapInputSource(source, button);
        deliver = [&](bool pressed, bool) {
            // Invoke the very slot InputOutputMap's inputValueChanged signal is
            // connected to, through the meta-object, exactly as Qt delivers it.
            QVERIFY(QMetaObject::invokeMethod(ui.vc(), "slotInputValueChanged",
                                              Q_ARG(quint32, 2), Q_ARG(quint32, 17),
                                              Q_ARG(uchar, pressed ? UCHAR_MAX : 0)));
        };
    }

    deliver(true, false);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(latch ? ioMap->frozenLatch() : ioMap->frozenMomentary(), true);
    QCOMPARE(button->state(), VCButton::Active);

    // Keyboard auto-repeat and duplicate controller pressure must not re-fire.
    deliver(true, true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(latch ? ioMap->frozenLatch() : ioMap->frozenMomentary(), true);

    deliver(false, false);
    if (latch)
    {
        // Release is ignored for the latch and re-arms the press edge.
        QCOMPARE(spy.count(), 1);
        QVERIFY(ioMap->frozenLatch());
        deliver(true, false);
        QCOMPARE(spy.count(), 2);
        QVERIFY(!ioMap->frozenLatch());
    }
    else
    {
        // Release is the thaw for the while-pressed action.
        QCOMPARE(spy.count(), 2);
        QVERIFY(!ioMap->frozenMomentary());
        QCOMPARE(button->state(), VCButton::Inactive);
    }

    // Neither route disturbed the other component.
    QCOMPARE(latch ? ioMap->frozenMomentary() : ioMap->frozenLatch(), false);
}

void VCButton_Test::freezeHold_onlyOwningControlReleases_data()
{
    QTest::addColumn<QString>("how");

    QTest::newRow("hidden") << QStringLiteral("hidden");
    QTest::newRow("disabled") << QStringLiteral("disabled");
    QTest::newRow("deleted") << QStringLiteral("deleted");
}

void VCButton_Test::freezeHold_onlyOwningControlReleases()
{
    QFETCH(QString, how);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();

    VCButton holder(&doc);
    holder.setActionType(VCButton::FreezeHold);
    std::unique_ptr<VCButton> bystander(new VCButton(&doc));
    bystander->setActionType(VCButton::FreezeHold);
    ui.vc()->addWidgetToMap(&holder);
    ui.vc()->addWidgetToMap(bystander.get());
    const auto unmap = qScopeGuard([&]() { ui.vc()->removeWidgetFromMap(&holder); });

    holder.requestStateChange(true);
    QVERIFY(ioMap->frozenMomentary());
    // The bystander displays the shared component but never pressed anything.
    QCOMPARE(bystander->state(), VCButton::Active);

    if (how == QStringLiteral("hidden"))
        bystander->setVisible(false);
    else if (how == QStringLiteral("disabled"))
        bystander->setDisabled(true);
    else
    {
        ui.vc()->removeWidgetFromMap(bystander.get());
        bystander.reset();
    }

    // Removing a control that holds nothing must not thaw the rig.
    QVERIFY2(ioMap->frozenMomentary(), "an unpressed control released another control's hold");
    QCOMPARE(holder.state(), VCButton::Active);

    // The owning control still releases.
    holder.setVisible(false);
    QVERIFY(!ioMap->frozenMomentary());

    if (bystander)
        ui.vc()->removeWidgetFromMap(bystander.get());
}

void VCButton_Test::freezeModeSwitch_releasesAndRearms()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();

    VCButton button(&doc);
    button.setActionType(VCButton::FreezeHold);
    ui.vc()->addWidgetToMap(&button);
    const auto unmap = qScopeGuard([&]() { ui.vc()->removeWidgetFromMap(&button); });

    // Reconfiguring a control that is holding must not strand the shared flag.
    button.requestStateChange(true);
    QVERIFY(ioMap->frozenMomentary());
    button.setActionType(VCButton::Toggle);
    QVERIFY2(!ioMap->frozenMomentary(), "switching away from FreezeHold stranded the momentary flag");

    // A latch press whose release is swallowed by an action switch must not
    // leave the press edge armed, or the next press is ignored.
    button.setActionType(VCButton::Freeze);
    button.slotInputValueChanged(0, UCHAR_MAX);
    QVERIFY(ioMap->frozenLatch());
    button.setActionType(VCButton::Toggle);
    button.slotInputValueChanged(0, 0);
    button.setActionType(VCButton::Freeze);
    button.slotInputValueChanged(0, UCHAR_MAX);
    QVERIFY2(!ioMap->frozenLatch(), "the press edge stayed armed across an action switch");
}

void VCButton_Test::freezeHold_pointerInterruptionReleases_data()
{
    QTest::addColumn<QString>("console");
    QTest::addColumn<QString>("interruption");

    for (const char *console : {"vc", "flow"})
        for (const char *interruption : {"grab-cancel", "edit-mode-release"})
            QTest::newRow(qPrintable(QStringLiteral("%1-%2").arg(console).arg(interruption)))
                << QString(console) << QString(interruption);
}

void VCButton_Test::freezeHold_pointerInterruptionReleases()
{
    QFETCH(QString, console);
    QFETCH(QString, interruption);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();

    VCButton button(&doc);
    button.setGeometry(QRect(0, 0, 160, 80));
    button.setActionType(VCButton::FreezeHold);
    ui.vc()->addWidgetToMap(&button);
    const auto unmap = qScopeGuard([&]() { ui.vc()->removeWidgetFromMap(&button); });

    const bool flow = console == QStringLiteral("flow");
    std::unique_ptr<FlowConsole> flowConsole;
    if (flow)
        flowConsole.reset(new FlowConsole(ui.view(), &doc));

    QQuickView *view = ui.view();
    view->resize(400, 300);
    std::unique_ptr<QQuickItem> item(renderButtonItem(view, &button,
        flow ? QStringLiteral("qrc:/FlowButtonItem.qml")
             : QStringLiteral("qrc:/VCButtonItem.qml")));
    QVERIFY(item);
    if (flow)
    {
        item->setWidth(160);
        item->setHeight(80);
    }
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));

    const QPoint center(80, 40);
    QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, center);
    QCoreApplication::processEvents();
    QVERIFY(ioMap->frozenMomentary());

    if (interruption == QStringLiteral("grab-cancel"))
    {
        // Something steals the pointer grab, so no release event ever arrives.
        QQuickItem *grabber = view->mouseGrabberItem();
        QVERIFY(grabber);
        grabber->ungrabMouse();
        QCoreApplication::processEvents();
    }
    else
    {
        // The operator switches to edit mode while still holding the button.
        if (flow)
            flowConsole->setEditMode(true);
        else
            ui.vc()->setEditMode(true);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, center);
        QCoreApplication::processEvents();
    }

    QVERIFY2(!ioMap->frozenMomentary(),
             "an interrupted while-pressed gesture stranded the shared momentary flag");
    QCOMPARE(button.state(), VCButton::Inactive);
}

void VCButton_Test::freezeState_sharedAcrossButtons()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();
    VCBridgeV5 bridge(&doc, ui.vc());

    VCButton first(&doc);
    first.setActionType(VCButton::Freeze);
    VCButton second(&doc);
    second.setActionType(VCButton::Freeze);
    VCButton unrelated(&doc);
    unrelated.setActionType(VCButton::Blackout);
    for (VCButton *b : {&first, &second, &unrelated})
        ui.vc()->addWidgetToMap(b);
    const auto unmap = qScopeGuard([&]() {
        for (VCButton *b : {&first, &second, &unrelated})
            ui.vc()->removeWidgetFromMap(b);
    });

    QSignalSpy frozen(ioMap, &InputOutputMap::frozenChanged);

    first.requestStateChange(true);
    QCOMPARE(frozen.count(), 1);
    QVERIFY(ioMap->isFrozen());
    QCOMPARE(first.state(), VCButton::Active);
    QCOMPARE(second.state(), VCButton::Active);
    QCOMPARE(unrelated.state(), VCButton::Inactive);
    QCOMPARE(bridge.getWidgetDetails(second.id()).buttonState, int(VCButton::Active));

    // A Freeze button configured while the latch is held displays it without
    // touching it.
    VCButton late(&doc);
    ui.vc()->addWidgetToMap(&late);
    late.setActionType(VCButton::Freeze);
    QCOMPARE(late.state(), VCButton::Active);
    QCOMPARE(frozen.count(), 1);

    // A copy reflects the shared state, it does not re-apply it.
    VCFrame parentFrame(&doc, ui.vc(), nullptr);
    std::unique_ptr<VCWidget> copy(first.createCopy(&parentFrame));
    QVERIFY(copy);
    QCOMPARE(copy->type(), VCWidget::ButtonWidget);
    QCOMPARE(qobject_cast<VCButton *>(copy.get())->state(), VCButton::Active);
    QCOMPARE(frozen.count(), 1);

    // Removing controls from view never thaws.
    first.setVisible(false);
    QVERIFY(ioMap->isFrozen());

    // Another Freeze button releases the shared latch.
    second.requestStateChange(false);
    QCOMPARE(frozen.count(), 2);
    QVERIFY(!ioMap->isFrozen());
    QCOMPARE(first.state(), VCButton::Inactive);
    QCOMPARE(late.state(), VCButton::Inactive);
    ui.vc()->removeWidgetFromMap(&late);
}

void VCButton_Test::freezeModes_componentStateAndInteraction()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();
    VCBridgeV5 bridge(&doc, ui.vc());

    VCButton latchA(&doc), latchB(&doc), holdA(&doc), holdB(&doc);
    for (VCButton *b : {&latchA, &latchB})
        b->setActionType(VCButton::Freeze);
    for (VCButton *b : {&holdA, &holdB})
        b->setActionType(VCButton::FreezeHold);
    for (VCButton *b : {&latchA, &latchB, &holdA, &holdB})
        ui.vc()->addWidgetToMap(b);
    const auto unmap = qScopeGuard([&]() {
        for (VCButton *b : {&latchA, &latchB, &holdA, &holdB})
            ui.vc()->removeWidgetFromMap(b);
    });

    // Two while-pressed controls are held. Both mirror the one shared flag,
    // exactly as every Flash button mirrors Function::flashing.
    holdA.requestStateChange(true);
    holdB.requestStateChange(true);
    QVERIFY(ioMap->frozenMomentary());
    QVERIFY(!ioMap->frozenLatch());
    QVERIFY(ioMap->isFrozen());
    QCOMPARE(holdA.state(), VCButton::Active);
    QCOMPARE(holdB.state(), VCButton::Active);

    // Latch buttons show the latch, never the aggregate, so a momentary hold
    // must not make them look latched.
    QCOMPARE(latchA.state(), VCButton::Inactive);
    QCOMPARE(latchB.state(), VCButton::Inactive);
    QCOMPARE(bridge.getWidgetDetails(latchA.id()).buttonState, int(VCButton::Inactive));
    QCOMPARE(bridge.getWidgetDetails(holdA.id()).buttonState, int(VCButton::Active));

    // A latch press while momentary is active inverts the latch independently.
    latchA.requestStateChange(true);
    QVERIFY(ioMap->frozenLatch());
    QVERIFY(ioMap->frozenMomentary());
    QCOMPARE(latchA.state(), VCButton::Active);
    QCOMPARE(latchB.state(), VCButton::Active);
    QCOMPARE(holdA.state(), VCButton::Active);

    // Flash parity: ANY release clears the one shared momentary flag, even
    // though the other control is still held. The latch survives.
    holdA.requestStateChange(false);
    QVERIFY(!ioMap->frozenMomentary());
    QVERIFY(ioMap->frozenLatch());
    QVERIFY(ioMap->isFrozen());
    QCOMPARE(holdA.state(), VCButton::Inactive);
    QCOMPARE(holdB.state(), VCButton::Inactive);
    QCOMPARE(latchA.state(), VCButton::Active);

    // requestStateChange stays a desired-state command for the internal
    // adapters: an explicit false releases the latch.
    latchB.requestStateChange(false);
    QVERIFY(!ioMap->frozenLatch());
    QVERIFY(!ioMap->isFrozen());
    QCOMPARE(latchA.state(), VCButton::Inactive);
}

void VCButton_Test::freezeEditorRow_adoptsActionWithoutActuating_data()
{
    QTest::addColumn<QString>("objectName");
    QTest::addColumn<int>("action");

    QTest::newRow("freeze-toggle")
        << QStringLiteral("freezeActionCheckBox") << int(VCButton::Freeze);
    QTest::newRow("freeze-while-pressed")
        << QStringLiteral("freezeHoldActionCheckBox") << int(VCButton::FreezeHold);
}

void VCButton_Test::freezeHold_releasesWhenHeldControlGoesAway_data()
{
    QTest::addColumn<QString>("how");

    QTest::newRow("hidden") << QStringLiteral("hidden");
    QTest::newRow("disabled") << QStringLiteral("disabled");
    QTest::newRow("deleted") << QStringLiteral("deleted");
}

void VCButton_Test::freezeHold_releasesWhenHeldControlGoesAway()
{
    QFETCH(QString, how);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();
    ioMap->setFrozen(true);

    std::unique_ptr<VCButton> button(new VCButton(&doc));
    button->setActionType(VCButton::FreezeHold);
    ui.vc()->addWidgetToMap(button.get());

    button->requestStateChange(true);
    QVERIFY(ioMap->frozenMomentary());

    // A held while-pressed control that goes away never receives its release,
    // so it must drop its activation itself, exactly as a held Flash button does.
    if (how == QStringLiteral("hidden"))
    {
        button->setVisible(false);
    }
    else if (how == QStringLiteral("disabled"))
    {
        button->setDisabled(true);
    }
    else
    {
        ui.vc()->removeWidgetFromMap(button.get());
        button.reset();
    }

    QVERIFY(!ioMap->frozenMomentary());
    // The persistent latch is untouched by the cleanup.
    QVERIFY(ioMap->frozenLatch());
    QVERIFY(ioMap->isFrozen());

    if (button)
        ui.vc()->removeWidgetFromMap(button.get());
}

void VCButton_Test::freezeEditorRow_adoptsActionWithoutActuating()
{
    QFETCH(QString, objectName);
    QFETCH(int, action);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();

    Scene *scene = new Scene(&doc);
    QVERIFY(doc.addFunction(scene));

    VCButton button(&doc);
    button.setCaption(QStringLiteral("Hold"));
    button.setFunctionID(scene->id());
    ui.vc()->addWidgetToMap(&button);
    const auto unmap = qScopeGuard([&]() { ui.vc()->removeWidgetFromMap(&button); });
    QCOMPARE(button.actionType(), VCButton::Toggle);

    QQuickView *view = ui.view();
    view->resize(800, 900);
    QQmlComponent component(view->engine(), QUrl(QStringLiteral("qrc:/VCButtonProperties.qml")));
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {"width", 780}, {"widgetRef", QVariant::fromValue(&button)}
    }));
    QVERIFY2(panel, qPrintable(component.errorString()));
    QQuickItem *panelItem = qobject_cast<QQuickItem *>(panel.get());
    QVERIFY(panelItem);
    panelItem->setParentItem(view->contentItem());
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));

    QQuickItem *freezeRow = panelItem->findChild<QQuickItem *>(objectName);
    QVERIFY2(freezeRow, qPrintable(QStringLiteral("the pressure editor offers no %1").arg(objectName)));
    QVERIFY(!freezeRow->property("checked").toBool());

    QSignalSpy frozen(ioMap, &InputOutputMap::frozenChanged);
    const bool blackoutBefore = ioMap->blackout();
    const QPointF center = freezeRow->mapToScene(
        QPointF(freezeRow->width() / 2, freezeRow->height() / 2));
    QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, center.toPoint());
    QCoreApplication::processEvents();

    // Selecting the behaviour configures the button, nothing else.
    QCOMPARE(int(button.actionType()), action);
    QVERIFY(freezeRow->property("checked").toBool());
    QCOMPARE(frozen.count(), 0);
    QVERIFY(!ioMap->isFrozen());
    QVERIFY(!ioMap->frozenLatch());
    QVERIFY(!ioMap->frozenMomentary());
    QCOMPARE(ioMap->blackout(), blackoutBefore);
    QVERIFY(!scene->isRunning());
    QCOMPARE(button.state(), VCButton::Inactive);
}

void VCButton_Test::freezeButton_leavesAttachedFunctionAlone()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();

    Scene *scene = new Scene(&doc);
    QVERIFY(doc.addFunction(scene));

    VCButton button(&doc);
    button.setFunctionID(scene->id());
    button.setActionType(VCButton::Freeze);
    ui.vc()->addWidgetToMap(&button);
    const auto unmap = qScopeGuard([&]() { ui.vc()->removeWidgetFromMap(&button); });

    // A Function left attached from an earlier action must not become the
    // Freeze implementation.
    button.requestStateChange(true);
    QVERIFY(ioMap->isFrozen());
    QVERIFY(!scene->isRunning());

    button.requestStateChange(false);
    QVERIFY(!ioMap->isFrozen());
    QVERIFY(!scene->isRunning());
    QCOMPARE(button.functionID(), scene->id());
}

void VCButton_Test::freezeWorkspace_survivesClearAndLoad()
{
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();
    VCBridgeV5 bridge(&doc, ui.vc());

    const int frameID = bridge.addFrame(0, QRect(0, 0, 400, 300), QStringLiteral("Frame"), false);
    QVERIFY(frameID >= 0);
    const int buttonID = bridge.addButton(frameID, QRect(10, 10, 80, 40), Function::invalidId(),
                                          QStringLiteral("Hold the look"), QStringLiteral("freeze"));
    QVERIFY(buttonID >= 0);
    VCButton *button = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(button);
    button->addKeySequence(QKeySequence(QStringLiteral("Ctrl+F")), 0);
    QSharedPointer<QLCInputSource> source(new QLCInputSource(3, 21));
    source->setID(0);
    button->addInputSource(source);

    QString xml;
    QXmlStreamWriter writer(&xml);
    QVERIFY(ui.vc()->saveXML(&writer));

    // Hold the look, then replace the document the way App::clearDocument()
    // does (qmlui/app.cpp): reset the console, then the universes.
    button->requestStateChange(true);
    QVERIFY(ioMap->isFrozen());

    ui.vc()->resetContents();
    ioMap->resetUniverses();
    QVERIFY(!ioMap->isFrozen());

    QBuffer buffer;
    buffer.setData(xml.toUtf8());
    QVERIFY(buffer.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&buffer);
    QVERIFY(reader.readNextStartElement());

    QSignalSpy frozen(ioMap, &InputOutputMap::frozenChanged);
    QVERIFY(ui.vc()->loadXML(reader));
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));

    VCButton *restored = qobject_cast<VCButton *>(ui.vc()->widget(buttonID));
    QVERIFY(restored);
    QCOMPARE(restored->actionType(), VCButton::Freeze);
    QCOMPARE(restored->caption(), QStringLiteral("Hold the look"));
    QCOMPARE(restored->keySequenceMap().value(QKeySequence(QStringLiteral("Ctrl+F")), 99u), 0u);
    QCOMPARE(restored->inputSources().size(), 1);
    QCOMPARE(restored->inputSources().first()->universe(), quint32(3));
    QCOMPARE(restored->inputSources().first()->channel(), quint32(21));
    QCOMPARE(bridge.getWidgetDetails(buttonID).action, QStringLiteral("freeze"));

    // Loading is configuration, never a press.
    QCOMPARE(frozen.count(), 0);
    QVERIFY(!ioMap->isFrozen());
    QCOMPARE(restored->state(), VCButton::Inactive);
}

void VCButton_Test::existingActions_preserved_data()
{
    QTest::addColumn<int>("action");

    QTest::newRow("toggle")   << int(VCButton::Toggle);
    QTest::newRow("flash")    << int(VCButton::Flash);
    QTest::newRow("blackout") << int(VCButton::Blackout);
    QTest::newRow("stopall")  << int(VCButton::StopAll);
}

void VCButton_Test::existingActions_preserved()
{
    QFETCH(int, action);

    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = new Scene(&doc);
    // An empty Scene stops itself on its first write (engine/src/scene.cpp:823),
    // so give it a real channel to hold.
    Fixture *fixture = new Fixture(&doc);
    fixture->setChannels(4);
    fixture->setAddress(0);
    fixture->setUniverse(0);
    QVERIFY(doc.addFixture(fixture));
    scene->setValue(SceneValue(fixture->id(), 0, 200));
    QVERIFY(doc.addFunction(scene));

    VCButton button(&doc);
    button.setFunctionID(scene->id());
    button.setActionType(VCButton::ButtonAction(action));
    ui.vc()->addWidgetToMap(&button);
    const auto unmap = qScopeGuard([&]() { ui.vc()->removeWidgetFromMap(&button); });

    QSignalSpy frozen(ioMap, &InputOutputMap::frozenChanged);

    switch (action)
    {
        case VCButton::Toggle:
            button.slotInputValueChanged(0, UCHAR_MAX);
            QCOMPARE(button.state(), VCButton::Active);
            QTRY_VERIFY(scene->isRunning());
            button.slotInputValueChanged(0, UCHAR_MAX);
            QCOMPARE(button.state(), VCButton::Inactive);
            QTRY_VERIFY(!scene->isRunning());
        break;
        case VCButton::Flash:
            button.slotInputValueChanged(0, UCHAR_MAX);
            QCOMPARE(button.state(), VCButton::Active);
            // Release unflashes: the existing hold-to-flash gesture is intact.
            button.slotInputValueChanged(0, 0);
            QCOMPARE(button.state(), VCButton::Inactive);
        break;
        case VCButton::Blackout:
            QVERIFY(!ioMap->blackout());
            button.slotInputValueChanged(0, UCHAR_MAX);
            QVERIFY(ioMap->blackout());
            QCOMPARE(button.state(), VCButton::Active);
            button.slotInputValueChanged(0, UCHAR_MAX);
            QVERIFY(!ioMap->blackout());
            QCOMPARE(button.state(), VCButton::Inactive);
        break;
        case VCButton::StopAll:
            scene->start(doc.masterTimer(), FunctionParent::master());
            QTRY_VERIFY(scene->isRunning());
            button.slotInputValueChanged(0, UCHAR_MAX);
            QTRY_VERIFY(!scene->isRunning());
        break;
    }

    // None of the pre-existing actions reaches the Freeze latch.
    QCOMPARE(frozen.count(), 0);
    QVERIFY(!ioMap->isFrozen());
}

void VCButton_Test::existingActions_qmlPointerPathUnchanged_data()
{
    QTest::addColumn<int>("action");

    QTest::newRow("toggle")   << int(VCButton::Toggle);
    QTest::newRow("flash")    << int(VCButton::Flash);
    QTest::newRow("blackout") << int(VCButton::Blackout);
    QTest::newRow("stopall")  << int(VCButton::StopAll);
}

void VCButton_Test::existingActions_qmlPointerPathUnchanged()
{
    QFETCH(int, action);

    // Freeze shares VCButtonItem's MouseArea handlers with every other action,
    // so moving it onto the press edge must leave the others exactly where they
    // were: Flash on press/release, Toggle/Blackout/StopAll on the click.
    Doc doc(nullptr, 1);
    UiFixture ui(&doc);
    InputOutputMap *ioMap = doc.inputOutputMap();
    doc.masterTimer()->start();
    const auto stopTimer = qScopeGuard([&]() { doc.masterTimer()->stop(); });

    Scene *scene = new Scene(&doc);
    Fixture *fixture = new Fixture(&doc);
    fixture->setChannels(4);
    fixture->setAddress(0);
    fixture->setUniverse(0);
    QVERIFY(doc.addFixture(fixture));
    scene->setValue(SceneValue(fixture->id(), 0, 200));
    QVERIFY(doc.addFunction(scene));

    VCButton button(&doc);
    button.setFunctionID(scene->id());
    button.setGeometry(QRect(0, 0, 160, 80));
    button.setActionType(VCButton::ButtonAction(action));
    ui.vc()->addWidgetToMap(&button);
    const auto unmap = qScopeGuard([&]() { ui.vc()->removeWidgetFromMap(&button); });

    if (action == VCButton::StopAll)
    {
        scene->start(doc.masterTimer(), FunctionParent::master());
        QTRY_VERIFY(scene->isRunning());
    }

    QQuickView *view = ui.view();
    view->resize(400, 300);
    std::unique_ptr<QQuickItem> item(
        renderButtonItem(view, &button, QStringLiteral("qrc:/VCButtonItem.qml")));
    QVERIFY(item);
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));

    QSignalSpy frozen(ioMap, &InputOutputMap::frozenChanged);
    const QPoint center(80, 40);

    QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, center);
    QCoreApplication::processEvents();

    // Only Flash acts on the press edge, as before.
    QCOMPARE(button.state(), action == VCButton::Flash ? VCButton::Active : VCButton::Inactive);
    QVERIFY(!ioMap->blackout());

    QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, center);
    QCoreApplication::processEvents();

    switch (action)
    {
        case VCButton::Toggle:
            QCOMPARE(button.state(), VCButton::Active);
            QTRY_VERIFY(scene->isRunning());
        break;
        case VCButton::Flash:
            QCOMPARE(button.state(), VCButton::Inactive);
        break;
        case VCButton::Blackout:
            QVERIFY(ioMap->blackout());
            QCOMPARE(button.state(), VCButton::Active);
        break;
        case VCButton::StopAll:
            QTRY_VERIFY(!scene->isRunning());
        break;
    }

    QCOMPARE(frozen.count(), 0);
    QVERIFY(!ioMap->isFrozen());
}

QTEST_MAIN(VCButton_Test)
