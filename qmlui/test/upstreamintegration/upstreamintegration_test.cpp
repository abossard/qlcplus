#include <QtTest>
#include <QFileOpenEvent>
#include <QMessageBox>
#include <QBuffer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQmlProperty>
#include <QJSValue>
#include <QQuickItem>
#include <QSettings>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <memory>
#include <cmath>
#include <QScopeGuard>
#include <algorithm>

#include "app.h"
#include "doc.h"
#include "djmanager.h"
#include "fixture.h"
#include "inputoutputmap.h"
#include "mastertimer.h"
#include "performfsm.h"
#include "scene.h"
#include "show.h"
#include "showfunction.h"
#include "showfactory.h"
#include "showmanager.h"
#include "showrunner.h"
#include "tardis.h"
#include "track.h"
#include "universe.h"
#include "vccuelist.h"
#include "vdjbridge.h"

class UpstreamIntegration_Test : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void mixedAuthoring_data();
    void mixedAuthoring();
    void fileOpenGuard_data();
    void fileOpenGuard();
    void cueListWidths_data();
    void cueListWidths();
    void columnResize_data();
    void columnResize();
    void cueListResizeGuard_data();
    void cueListResizeGuard();
    void internalDragOverlay();
    void mixedPasteAndTiming_data();
    void mixedPasteAndTiming();
    void beatAlignment_data();
    void beatAlignment();
    void resizeItems_data();
    void resizeItems();
    void timingControls_data();
    void timingControls();
    void timelineUserSeek_data();
    void timelineUserSeek();
    void showPlayheadVisibilityAndFollow_data();
    void showPlayheadVisibilityAndFollow();
private:
    QTemporaryDir m_settings;
    std::unique_ptr<App> m_app;
};

void UpstreamIntegration_Test::initTestCase()
{
    QVERIFY(m_settings.isValid());
    const QString settings = m_settings.path();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settings);
    QCoreApplication::setOrganizationName("QLCPlusIntegrationTests");
    QCoreApplication::setApplicationName("UpstreamIntegration");
    m_app = std::make_unique<App>();
    m_app->set3dSupported(false);
    m_app->startup();
    m_app->resize(1200, 800);
    m_app->doc()->masterTimer()->stop();
    QVERIFY2(m_app->status() == QQuickView::Ready, qPrintable(m_app->errors().value(0).toString()));
}

void UpstreamIntegration_Test::cleanupTestCase()
{
    m_app.reset();
}

void UpstreamIntegration_Test::mixedAuthoring_data()
{
    QTest::addColumn<int>("division");
    QTest::addColumn<bool>("beatItem");
    QTest::addColumn<double>("zoom");
    for (auto division : {Show::Time, Show::BPM_4_4, Show::VDJBeat})
        for (bool beat : {false, true})
            for (double zoom : {0.5, 2.0})
                QTest::newRow(qPrintable(QString("%1-%2-%3").arg(int(division)).arg(beat).arg(zoom)))
                        << int(division) << beat << zoom;
}

void UpstreamIntegration_Test::mixedAuthoring()
{
    QFETCH(int, division);
    QFETCH(bool, beatItem);
    QFETCH(double, zoom);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    QVERIFY(manager);
    auto *show = new Show(doc);
    doc->addFunction(show);
    show->setTimeDivision(Show::TimeDivision(division), 120);
    manager->setCurrentShowID(show->id());
    manager->setTimeScale(zoom);
    doc->masterTimer()->requestBpmNumber(60);
    auto *scene = new Scene(doc);
    scene->setTempoType(beatItem ? Function::Beats : Function::Time);
    scene->setDuration(beatItem ? 3000 : 750);
    doc->addFunction(scene);
    QQuickItem area;
    const auto reset = qScopeGuard([manager]() { manager->resetView(); });
    manager->addItems(&area, -1, 1500, {scene->id()});
    QCOMPARE(show->tracks().count(), 1);
    auto *sf = show->tracks().first()->showFunctions().first();
    const bool timeRuler = Show::isTimeBasedDivision(Show::TimeDivision(division));
    const uint expectedStart = timeRuler ? (beatItem ? 3000 : 1500) : (beatItem ? 1500 : 750);
    QCOMPARE(sf->startTime(), expectedStart);
    QCOMPARE(scene->tempoType(), beatItem ? Function::Beats : Function::Time);
    QVERIFY(!area.childItems().isEmpty());
    auto *item = area.childItems().last();
    QVERIFY(QMetaObject::invokeMethod(item, "updateGeometry"));
    const double startMs = timeRuler ? 1500 : 750;
    const double expectedX = timeRuler
            ? startMs * manager->tickSize() / (1000 * manager->timeScale())
            : startMs * manager->tickSize() / 2000;
    QVERIFY(qAbs(item->x() - expectedX) < 0.01);
    QCOMPARE(manager->getSnapEdges(Function::invalidId()).size(), 2);
    QVERIFY(qAbs(manager->getSnapEdges(Function::invalidId())[0].toDouble() - expectedX) < 0.01);
    QVERIFY(manager->checkAndMoveItem(sf, 0, 0, expectedStart + 5000));
    QVERIFY(manager->setShowItemDuration(sf, 900));
    manager->setReadOnly(true);
    QVERIFY(!manager->setShowItemStartTime(sf, 0));
    QCOMPARE(sf->startTime(), expectedStart + 5000);
    manager->setReadOnly(false);
    manager->setBpmNumber(0);
    QVERIFY(QMetaObject::invokeMethod(item, "updateGeometry"));
    QVERIFY(std::isfinite(item->x()));
    QVERIFY(std::isfinite(item->width()));
    manager->setCurrentShowID(Function::invalidId());
}

void UpstreamIntegration_Test::fileOpenGuard_data()
{
    QTest::addColumn<QString>("path");
    QTest::newRow("project") << QDir::current().absoluteFilePath("incoming.qxw");
    QTest::newRow("unsupported") << QDir::current().absoluteFilePath("unsupported.txt");
    QTest::newRow("nonlocal") << QString("https://example.invalid/project.qxw");
}

void UpstreamIntegration_Test::fileOpenGuard()
{
    QFETCH(QString, path);
    auto *doc = m_app->doc();
    if (path.endsWith(".qxw") && !path.startsWith("https:"))
        QVERIFY(m_app->saveWorkspace(path));
    auto *scene = new Scene(doc);
    doc->addFunction(scene);
    const auto id = scene->id();
    doc->setModified();
    QFileOpenEvent event(path.startsWith("https:") ? QUrl(path) : QUrl::fromLocalFile(path));
    QCoreApplication::sendEvent(qApp, &event);
    QCOMPARE(doc->function(id), scene);
    QVERIFY(doc->isModified());
    QQmlExpression menuExpr(qmlContext(m_app->rootObject()), m_app->rootObject(), "actionsMenu");
    QObject *menu = menuExpr.evaluate().value<QObject*>();
    QVERIFY2(menu, qPrintable(menuExpr.error().toString()));
    QObject *popup = menu->findChild<QObject*>("saveFirstPopup");
    QVERIFY(popup);
    QCOMPARE(popup->property("visible").toBool(), path.endsWith(".qxw") && !path.startsWith("https:"));
    QVERIFY(QMetaObject::invokeMethod(popup, "reject"));
    QVERIFY(!popup->property("visible").toBool());
    QCOMPARE(doc->function(id), scene);
    QVERIFY(doc->isModified());
}

void UpstreamIntegration_Test::cueListWidths_data()
{
    QTest::addColumn<QVariantList>("widths");
    QTest::newRow("old-xml-default") << QVariantList{};
    QTest::newRow("unequal-percentages") << QVariantList{5.0, 35.0, 10.0, 15.0, 15.0, 20.0};
}

void UpstreamIntegration_Test::cueListWidths()
{
    QFETCH(QVariantList, widths);
    VCCueList source(m_app->doc()), restored(m_app->doc());
    source.setColumnsWidths(widths);
    std::unique_ptr<VCWidget> copy(source.createCopy(nullptr));
    auto *copied = qobject_cast<VCCueList*>(copy.get());
    QVERIFY(copied);
    QCOMPARE(copied->columnsWidths(), widths);
    QString xml;
    QXmlStreamWriter writer(&xml);
    QVERIFY(source.saveXML(&writer));
    QBuffer buffer;
    buffer.setData(xml.toUtf8());
    QVERIFY(buffer.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&buffer);
    QVERIFY(reader.readNextStartElement());
    QVERIFY(restored.loadXML(reader));
    QCOMPARE(restored.columnsWidths(), widths);
}

void UpstreamIntegration_Test::columnResize_data()
{
    QTest::addColumn<int>("width");
    QTest::newRow("zero-initialization") << 0;
    QTest::newRow("normal") << 800;
    QTest::newRow("resized") << 1200;
}

void UpstreamIntegration_Test::columnResize()
{
    QFETCH(int, width);
    QQmlComponent component(m_app->engine(), QUrl("qrc:/ChaserWidget.qml"));
    std::unique_ptr<QObject> widget(component.create());
    QVERIFY2(widget, qPrintable(component.errorString()));
    const QVariantList widths{5.0, 35.0, 10.0, 15.0, 15.0, 20.0};
    widget->setProperty("width", 0);
    widget->setProperty("columnsWidths", widths);
    widget->setProperty("width", width);
    QSignalSpy resized(widget.get(), SIGNAL(columnsResized(QVariant)));
    QVERIFY(resized.isValid());
    QVERIFY(QMetaObject::invokeMethod(widget.get(), "storeColumnsWidths"));
    QCOMPARE(resized.count(), width ? 1 : 0);
    if (width)
    {
        QVariant value = resized.first().first();
        const QVariantList actual = value.metaType().id() == qMetaTypeId<QJSValue>()
                ? value.value<QJSValue>().toVariant().toList() : value.toList();
        QCOMPARE(actual, widths);
    }
}

void UpstreamIntegration_Test::cueListResizeGuard_data()
{
    QTest::addColumn<QVariantList>("widths");
    QTest::newRow("wide-name") << QVariantList{5.0, 35.0, 10.0, 15.0, 15.0, 20.0};
    QTest::newRow("wide-timing") << QVariantList{5.0, 15.0, 15.0, 20.0, 20.0, 25.0};
}

void UpstreamIntegration_Test::cueListResizeGuard()
{
    QFETCH(QVariantList, widths);
    auto *doc = m_app->doc();
    const QString path = QDir::current().absoluteFilePath("cue-resize-incoming.qxw");
    QVERIFY(m_app->saveWorkspace(path));
    QQuickItem area(m_app->rootObject());
    area.setWidth(1000);
    area.setHeight(600);
    VCCueList cue(doc);
    cue.render(m_app.get(), &area);
    QVERIFY(cue.renderItem());
    QQmlExpression expression(qmlContext(cue.renderItem()), cue.renderItem(), "chWidget");
    QObject *widget = expression.evaluate().value<QObject*>();
    QVERIFY2(widget, qPrintable(expression.error().toString()));
    doc->resetModified();
    QVERIFY(QMetaObject::invokeMethod(widget, "columnsResized", Q_ARG(QVariant, widths)));
    QCOMPARE(cue.columnsWidths(), widths);
    QVERIFY(doc->isModified());
    QFileOpenEvent event(QUrl::fromLocalFile(path));
    QCoreApplication::sendEvent(qApp, &event);
    QQmlExpression menuExpr(qmlContext(m_app->rootObject()), m_app->rootObject(), "actionsMenu");
    QObject *menu = menuExpr.evaluate().value<QObject*>();
    QVERIFY2(menu, qPrintable(menuExpr.error().toString()));
    QObject *popup = menu->findChild<QObject*>("saveFirstPopup");
    QVERIFY(popup);
    QVERIFY(popup->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(popup, "clicked", Q_ARG(int, int(QMessageBox::Cancel))));
    QVERIFY(QMetaObject::invokeMethod(popup, "close"));
    QVERIFY(!popup->property("visible").toBool());
    QCOMPARE(cue.columnsWidths(), widths);
    QVERIFY(doc->isModified());

    VCCueList restored(doc);
    QBuffer buffer;
    buffer.setData("<CueList><ColumnsWidths>5,35,10,15,15,20</ColumnsWidths></CueList>");
    QVERIFY(buffer.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&buffer);
    QVERIFY(reader.readNextStartElement());
    doc->resetModified();
    QVERIFY(restored.loadXML(reader));
    QCOMPARE(restored.columnsWidths(), QVariantList({5.0, 35.0, 10.0, 15.0, 15.0, 20.0}));
    QVERIFY(!doc->isModified());
}

void UpstreamIntegration_Test::internalDragOverlay()
{
    QQmlComponent component(m_app->engine());
    component.setData("import QtQuick\nimport \"qrc:/\"\n"
                      "WidgetDragItem { width: 100; function setDrag(value) { Drag.active = value } }",
                      QUrl("qrc:/drag-test.qml"));
    std::unique_ptr<QObject> widget(component.create());
    QVERIFY2(widget, qPrintable(component.errorString()));
    QQmlExpression overlay(qmlContext(m_app->rootObject()), m_app->rootObject(), "fileDropArea.z");
    QVERIFY(QMetaObject::invokeMethod(widget.get(), "setDrag", Q_ARG(QVariant, true)));
    QCOMPARE(overlay.evaluate().toInt(), -1);
    QVERIFY(QMetaObject::invokeMethod(widget.get(), "setDrag", Q_ARG(QVariant, false)));
    QCOMPARE(overlay.evaluate().toInt(), 100);
}

void UpstreamIntegration_Test::mixedPasteAndTiming_data()
{
    QTest::addColumn<int>("division");
    QTest::newRow("time") << int(Show::Time);
    QTest::newRow("beats") << int(Show::BPM_4_4);
    QTest::newRow("vdj") << int(Show::VDJBeat);
}

void UpstreamIntegration_Test::mixedPasteAndTiming()
{
    QFETCH(int, division);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    auto *show = new Show(doc);
    doc->addFunction(show);
    show->setTimeDivision(Show::TimeDivision(division), 120);
    manager->setCurrentShowID(show->id());
    const bool ms = manager->timeBasedDivision();
    QQuickItem area;
    const auto reset = qScopeGuard([manager]() { manager->resetView(); });
    auto *time = new Scene(doc);
    auto *beat = new Scene(doc);
    doc->addFunction(time);
    doc->addFunction(beat);
    beat->setTempoType(Function::Beats);
    time->setDuration(500);
    beat->setDuration(1000);
    manager->addItems(&area, -1, ms ? 1000 : 2000, {time->id(), beat->id()});
    auto items = show->tracks().first()->showFunctions();
    QCOMPARE(items.count(), 2);
    QCOMPARE(items[0]->startTime(), uint(1000));
    QCOMPARE(items[1]->startTime(), uint(3000));
    manager->setItemSelection(0, items[0], area.childItems()[0], true, Qt::ControlModifier);
    manager->setItemSelection(0, items[1], area.childItems()[1], true, Qt::ControlModifier);
    manager->copyToClipboard();
    manager->setCurrentTime(4000);
    QVERIFY(manager->pasteFromClipboard());
    items = show->tracks().first()->showFunctions();
    QCOMPARE(items.count(), 4);
    QCOMPARE(items[2]->startTime(), uint(4000));
    QCOMPARE(items[3]->startTime(), uint(9000));
    QVERIFY(manager->insertTimeAtCursor(ms ? 250 : 500, ms ? 1200 : 2400));
    QCOMPARE(items[0]->duration(), uint(750));
    QCOMPARE(items[1]->startTime(), uint(3500));
    QVERIFY(manager->cutTimeAtCursor(ms ? 250 : 500, ms ? 1200 : 2400));
    QCOMPARE(items[0]->duration(), uint(500));
    QCOMPARE(items[1]->startTime(), uint(3000));
    manager->setCurrentShowID(Function::invalidId());
}

void UpstreamIntegration_Test::beatAlignment_data()
{
    QTest::addColumn<bool>("accept");
    QTest::newRow("cancel") << false;
    QTest::newRow("confirm-and-undo") << true;
}

void UpstreamIntegration_Test::beatAlignment()
{
    QFETCH(bool, accept);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    auto *show = new Show(doc);
    doc->addFunction(show);
    show->setTimeDivision(Show::Time, 120);
    manager->setCurrentShowID(show->id());
    auto *scene = new Scene(doc);
    doc->addFunction(scene);
    scene->setTempoType(Function::Beats);
    scene->setDuration(750);
    QQuickItem area;
    const auto reset = qScopeGuard([manager]() { manager->resetView(); });
    manager->addItems(&area, -1, 625, {scene->id()});
    auto *sf = show->tracks().first()->showFunctions().first();
    QCOMPARE(sf->startTime(), uint(1250));
    QQmlContext context(m_app->rootContext());
    context.setContextProperty("mainView", m_app->rootObject());
    QQmlComponent component(m_app->engine(), QUrl("qrc:/ShowManager.qml"));
    std::unique_ptr<QObject> view(component.create(&context));
    QVERIFY2(view, qPrintable(component.errorString()));
    auto *combo = view->findChild<QObject*>("markersCombo");
    auto *popup = view->findChild<QObject*>("beatAlignWarningPopup");
    QVERIFY(combo);
    QVERIFY(popup);
    combo->setProperty("currentIndex", 2);
    QVERIFY(popup->property("visible").toBool());
    QCOMPARE(show->timeDivisionType(), Show::Time);
    if (!accept)
    {
        QVERIFY(QMetaObject::invokeMethod(popup, "reject"));
        QCOMPARE(sf->startTime(), uint(1250));
        QCOMPARE(show->timeDivisionType(), Show::Time);
        return;
    }
    QTest::qWait(300);
    QVERIFY(QMetaObject::invokeMethod(popup, "accept"));
    QCOMPARE(show->timeDivisionType(), Show::BPM_4_4);
    QCOMPARE(sf->startTime(), uint(1000));
    QCOMPARE(sf->duration(), uint(1000));
    QTest::qWait(300);
    Tardis::instance()->undoAction();
    QCOMPARE(sf->startTime(), uint(1250));
    QCOMPARE(sf->duration(), uint(750));
}

void UpstreamIntegration_Test::resizeItems_data()
{
    QTest::addColumn<int>("division");
    QTest::addColumn<bool>("beatItem");
    QTest::addColumn<bool>("left");
    for (auto division : {Show::Time, Show::BPM_4_4, Show::VDJBeat})
        for (bool beat : {false, true})
            for (bool left : {false, true})
                QTest::newRow(qPrintable(QString("%1-%2-%3").arg(int(division)).arg(beat).arg(left)))
                        << int(division) << beat << left;
}

void UpstreamIntegration_Test::resizeItems()
{
    QFETCH(int, division);
    QFETCH(bool, beatItem);
    QFETCH(bool, left);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    auto *show = new Show(doc);
    doc->addFunction(show);
    show->setTimeDivision(Show::TimeDivision(division), 120);
    manager->setCurrentShowID(show->id());
    manager->setTimeScale(manager->timeBasedDivision() ? 0.5 : 4.0);
    manager->setGridEnabled(false);
    auto *scene = new Scene(doc);
    doc->addFunction(scene);
    scene->setTempoType(beatItem ? Function::Beats : Function::Time);
    scene->setDuration(beatItem ? 4000 : 2000);
    QQuickItem area(m_app->rootObject());
    area.setY(200);
    area.setZ(1000);
    area.setWidth(800);
    area.setHeight(100);
    const auto reset = qScopeGuard([manager]() { manager->resetView(); });
    manager->addItems(&area, -1, manager->timeBasedDivision() ? 1000 : 2000, {scene->id()});
    auto *sf = show->tracks().first()->showFunctions().first();
    auto *item = area.childItems().first();
    auto *handle = item->findChild<QQuickItem*>(left ? "showItemResizeLeft" : "showItemResizeRight");
    QVERIFY(handle);
    const double originalWidth = item->width();
    const uint originalDuration = sf->duration();
    QPoint point = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2)).toPoint();
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, point);
    const int direction = left ? -1 : 1;
    QTest::mouseMove(m_app.get(), point + QPoint(direction * 15, 0), 20);
    QTest::mouseMove(m_app.get(), point + QPoint(direction * 120, 0), 20);
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, point + QPoint(direction * 120, 0));
    QVERIFY(sf->duration() > originalDuration);
    QVERIFY(item->width() > originalWidth);
    QCOMPARE(scene->tempoType(), beatItem ? Function::Beats : Function::Time);
}

void UpstreamIntegration_Test::timingControls_data()
{
    QTest::addColumn<int>("division");
    QTest::newRow("time") << int(Show::Time);
    QTest::newRow("beats") << int(Show::BPM_4_4);
    QTest::newRow("vdj") << int(Show::VDJBeat);
}

void UpstreamIntegration_Test::timingControls()
{
    QFETCH(int, division);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    auto *show = new Show(doc);
    doc->addFunction(show);
    show->setTimeDivision(Show::TimeDivision(division), 120);
    manager->setCurrentShowID(show->id());
    QQuickItem area;
    const auto reset = qScopeGuard([manager]() { manager->resetView(); });
    auto *time = new Scene(doc);
    auto *beat = new Scene(doc);
    doc->addFunction(time);
    doc->addFunction(beat);
    beat->setTempoType(Function::Beats);
    time->setDuration(1500);
    beat->setDuration(4000);
    manager->addItems(&area, -1, 0, {time->id()});
    manager->addItems(&area, -1, 0, {beat->id()});
    auto *timeItem = show->tracks()[0]->showFunctions().first();
    auto *beatItem = show->tracks()[1]->showFunctions().first();
    manager->setItemSelection(0, timeItem, area.childItems()[0], true, Qt::ControlModifier);
    manager->setItemSelection(1, beatItem, area.childItems()[1], true, Qt::ControlModifier);
    QQmlContext context(m_app->rootContext());
    context.setContextProperty("mainView", m_app->rootObject());
    QQmlComponent component(m_app->engine(), QUrl("qrc:/TimingUtils.qml"));
    std::unique_ptr<QObject> panel(component.create(&context));
    QVERIFY2(panel, qPrintable(component.errorString()));
    manager->setCurrentTime(1500);
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "alignStartToCursor"));
    QCOMPARE(timeItem->startTime(), uint(1500));
    QCOMPARE(beatItem->startTime(), uint(3000));
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "applyRelativeValue",
                                      Q_ARG(QVariant, 1),
                                      Q_ARG(QVariant, manager->timeBasedDivision() ? 500 : 1000)));
    QCOMPARE(timeItem->startTime(), uint(2000));
    QCOMPARE(beatItem->startTime(), uint(4000));
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "applyRelativeValue",
                                      Q_ARG(QVariant, 3), Q_ARG(QVariant, -10000)));
    QCOMPARE(timeItem->duration(), uint(1));
    QCOMPARE(beatItem->duration(), uint(125));
    manager->resetItemsSelection();
    manager->setItemSelection(1, beatItem, area.childItems()[1], true, Qt::ControlModifier);
    QCOMPARE(panel->property("isBeatBased").toBool(), true);
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "applyAbsoluteValue",
                                      Q_ARG(QVariant, 3), Q_ARG(QVariant, 2500)));
    QCOMPARE(beatItem->duration(), uint(2500));
    manager->setReadOnly(true);
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "applyAbsoluteValue",
                                      Q_ARG(QVariant, 3), Q_ARG(QVariant, 5000)));
    QCOMPARE(beatItem->duration(), uint(2500));
    manager->setReadOnly(false);
}

void UpstreamIntegration_Test::timelineUserSeek_data()
{
    QTest::addColumn<bool>("readOnly");
    QTest::addColumn<bool>("drag");
    QTest::addColumn<quint32>("startTime");
    QTest::addColumn<quint32>("destination");
    QTest::addColumn<int>("activeCue");

    QTest::newRow("forward-click") << false << false << quint32(500) << quint32(1500) << 1;
    QTest::newRow("backward-drag") << false << true << quint32(1500) << quint32(500) << 0;
    QTest::newRow("perform-click") << true << false << quint32(500) << quint32(1500) << 0;
    QTest::newRow("perform-drag") << true << true << quint32(500) << quint32(1500) << 0;
}

void UpstreamIntegration_Test::timelineUserSeek()
{
    QFETCH(bool, readOnly);
    QFETCH(bool, drag);
    QFETCH(quint32, startTime);
    QFETCH(quint32, destination);
    QFETCH(int, activeCue);

    auto *doc = m_app->doc();
    auto *timer = doc->masterTimer();
    auto *manager = qobject_cast<ShowManager*>(
            m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    QVERIFY(manager);
    QCOMPARE(doc->inputOutputMap()->outputPatchesCount(0), 0);

    auto *fixture = new Fixture(doc);
    fixture->setUniverse(0);
    fixture->setAddress(0);
    fixture->setChannels(1);
    doc->addFixture(fixture);

    auto *show = new Show(doc);
    show->setTimeDivision(Show::Time, 120);
    doc->addFunction(show);
    auto *track = new Track(Function::invalidId());
    Scene *cues[] = {new Scene(doc), new Scene(doc), new Scene(doc)};
    const quint32 cueStarts[] = {0, 1000, 2000};
    const uchar cueValues[] = {51, 137, 223};
    for (int i = 0; i < 3; ++i)
    {
        cues[i]->setValue(fixture->id(), 0, cueValues[i]);
        doc->addFunction(cues[i]);
        auto *item = new ShowFunction(show->getLatestShowFunctionId());
        item->setFunctionID(cues[i]->id());
        item->setStartTime(cueStarts[i]);
        item->setDuration(1000);
        track->addShowFunction(item);
    }
    show->addTrack(track);
    const quint32 fixtureId = fixture->id();
    const quint32 showId = show->id();
    const QList<quint32> cueIds = {cues[0]->id(), cues[1]->id(), cues[2]->id()};
    std::unique_ptr<QObject> panelObject;
    const auto cleanup = qScopeGuard([&]() {
        manager->setReadOnly(false);
        if (show->isRunning())
            show->stopAndWait();
        timer->stop();
        manager->resetContents();
        panelObject.reset();
        doc->deleteFunction(showId);
        for (quint32 cueId : cueIds)
            doc->deleteFunction(cueId);
        doc->deleteFixture(fixtureId);
        doc->inputOutputMap()->resetUniverses();
    });

    manager->setReadOnly(false);
    manager->setCurrentShowID(show->id());
    manager->setTimeScale(1.0);
    manager->setCurrentTime(int(startTime));

    QQmlContext context(m_app->rootContext());
    context.setContextProperty("mainView", m_app->rootObject());
    QQmlComponent component(m_app->engine(), QUrl("qrc:/ShowManager.qml"));
    panelObject.reset(component.create(&context));
    QVERIFY2(panelObject, qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem*>(panelObject.get());
    QVERIFY(panel);
    panel->setParentItem(m_app->contentItem());
    panel->setZ(10000);
    QCoreApplication::processEvents();

    QQmlExpression headerExpression(qmlContext(panel), panel, "hdrItem");
    QObject *headerObject = headerExpression.evaluate().value<QObject*>();
    QVERIFY2(!headerExpression.hasError(), qPrintable(headerExpression.error().toString()));
    auto *header = qobject_cast<QQuickItem*>(headerObject);
    QVERIFY(header);
    QTRY_VERIFY(header->width() > 0);
    QSignalSpy runnerPosition(show, &Show::timeChanged);
    timer->start();

    if (readOnly)
    {
        show->setSyncSource(ShowRunner::External);
        show->start(timer, FunctionParent::master());
        QTRY_VERIFY(show->isRunning());
        show->setExternalElapsedTime(startTime);
        QTRY_VERIFY(!runnerPosition.isEmpty()
                    && runnerPosition.last().first().toUInt() == startTime);
        QTRY_COMPARE(manager->currentTime(), int(startTime));
        manager->setReadOnly(true);
    }
    else
    {
        show->setSyncSource(ShowRunner::Autonomous);
        manager->playShow();
        QTRY_VERIFY(show->isRunning());
        QTRY_VERIFY(manager->currentTime() >= int(startTime + MasterTimer::tick()));
    }

    runnerPosition.clear();
    const auto timelineX = [manager](quint32 time) {
        return time * manager->tickSize() / (1000.0 * manager->timeScale());
    };
    const qreal targetX = timelineX(destination);
    if (drag)
    {
        const qreal cursorY = header->property("headerHeight").toReal() - 5;
        const QPoint startPoint = header->mapToScene(
                QPointF(timelineX(quint32(manager->currentTime())), cursorY)).toPoint();
        const QPoint targetPoint = header->mapToScene(
                QPointF(targetX, cursorY)).toPoint();
        QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, startPoint);
        QTest::mouseMove(m_app.get(), targetPoint, 20);
        QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, targetPoint);
    }
    else
    {
        const QPoint targetPoint = header->mapToScene(
                QPointF(targetX, header->height() / 2)).toPoint();
        QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, targetPoint);
    }
    QCoreApplication::processEvents();

    if (readOnly)
    {
        QCOMPARE(manager->currentTime(), int(startTime));
        for (const QList<QVariant> &position : runnerPosition)
            QCOMPARE(position.first().toUInt(), startTime);
        show->setExternalElapsedTime(startTime + 40);
        QTRY_COMPARE(manager->currentTime(), int(startTime + 40));
        QCOMPARE(runnerPosition.last().first().toUInt(), startTime + 40);
        QVERIFY(cues[activeCue]->isRunning());
        QVERIFY(!cues[1]->isRunning());
    }
    else
    {
        const int pixelTolerance = int(std::ceil(1000.0 * manager->timeScale()
                                                / manager->tickSize())) + 1;
        QTRY_VERIFY_WITH_TIMEOUT(std::any_of(runnerPosition.cbegin(), runnerPosition.cend(),
                               [destination, pixelTolerance](const QList<QVariant> &position) {
            const int value = position.first().toInt();
            return value >= int(destination) - pixelTolerance
                    && value <= int(destination) + pixelTolerance + int(MasterTimer::tick() * 3);
        }), 200);
        for (int i = 0; i < 3; ++i)
            QCOMPARE(cues[i]->isRunning(), i == activeCue);

        QTRY_VERIFY(cues[2]->isRunning());
    }

    QList<Universe*> universes = doc->inputOutputMap()->claimUniverses();
    QVERIFY(!universes.isEmpty());
    Universe *universe = universes.first();
    doc->inputOutputMap()->releaseUniverses(false);
    const uchar expectedOutput = readOnly ? cueValues[0] : cueValues[2];
    QTRY_COMPARE(universe->preGMValue(0), expectedOutput);
}

void UpstreamIntegration_Test::showPlayheadVisibilityAndFollow_data()
{
    QTest::addColumn<bool>("alreadyRunningPaused");
    QTest::addColumn<bool>("directlyCreated");
    QTest::addColumn<quint32>("pausedPosition");
    QTest::addColumn<quint32>("pausedUpdate");
    QTest::addColumn<quint32>("resumePosition");

    QTest::newRow("stopped-no-runner")
            << false << false << quint32(121000) << quint32(163000) << quint32(327000);
    QTest::newRow("already-running-paused")
            << true << false << quint32(137000) << quint32(179000) << quint32(343000);
    QTest::newRow("directly-created")
            << false << true << quint32(121000) << quint32(163000) << quint32(327000);
}

void UpstreamIntegration_Test::showPlayheadVisibilityAndFollow()
{
    QFETCH(bool, alreadyRunningPaused);
    QFETCH(bool, directlyCreated);
    QFETCH(quint32, pausedPosition);
    QFETCH(quint32, pausedUpdate);
    QFETCH(quint32, resumePosition);

    auto *doc = m_app->doc();
    auto *timer = doc->masterTimer();
    auto *manager = qobject_cast<ShowManager*>(
            m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    QVERIFY(manager);
    auto *djManager = qobject_cast<DjManager*>(
            m_app->rootContext()->contextProperty("djManager").value<QObject*>());
    QVERIFY(djManager);
    auto *bridge = qobject_cast<VdjBridge*>(
            m_app->rootContext()->contextProperty("vdjBridge").value<QObject*>());
    QVERIFY(bridge);

    auto *scene = new Scene(doc);
    doc->addFunction(scene);
    QQuickItem authoringArea(m_app->rootObject());
    Show *show;
    if (directlyCreated)
    {
        manager->resetContents();
        manager->addItems(&authoringArea, -1, 0, {scene->id()});
        show = manager->currentShow();
        QVERIFY(show);
        show->tracks().first()->showFunctions().first()->setDuration(420000);
    }
    else
    {
        show = new Show(doc);
        doc->addFunction(show);
        auto *track = new Track(Function::invalidId());
        auto *item = new ShowFunction(show->getLatestShowFunctionId());
        item->setFunctionID(scene->id());
        item->setStartTime(0);
        item->setDuration(420000);
        track->addShowFunction(item);
        show->addTrack(track);
    }
    show->setTimeDivision(Show::Time, 120);

    const quint32 showId = show->id();
    const quint32 sceneId = scene->id();
    const QString filepath = QString("/c2/playhead-%1.mp3")
            .arg(QString::fromLatin1(QTest::currentDataTag()));
    std::unique_ptr<QObject> panelObject;
    const auto cleanup = qScopeGuard([&]() {
        djManager->setPerformMode(false);
        if (show->isRunning())
            show->stopAndWait();
        timer->stop();
        manager->resetContents();
        panelObject.reset();
        doc->deleteFunction(showId);
        doc->deleteFunction(sceneId);
    });

    djManager->setPerformMode(false);
    QCOMPARE(bridge->performFsm()->state(), PerformFsm::PerformState::Idle);
    QVERIFY(!manager->readOnly());
    manager->setTimeScale(1.0);
    manager->setCurrentTime(7000);

    const auto deckTrigger = [bridge](const QString &trigger, const QVariant &value) {
        return QMetaObject::invokeMethod(bridge, "onDeckTrigger",
                                         Q_ARG(int, 0),
                                         Q_ARG(QString, trigger),
                                         Q_ARG(QVariant, value));
    };
    const auto globalTrigger = [bridge](const QString &trigger, const QVariant &value) {
        return QMetaObject::invokeMethod(bridge, "onGlobalTrigger",
                                         Q_ARG(QString, trigger),
                                         Q_ARG(QVariant, value));
    };

    bridge->showFactory()->registerMapping(filepath, show->id());
    QVERIFY(deckTrigger("get_filepath", filepath));
    QVERIFY(deckTrigger("get_title", "Playhead test"));
    QVERIFY(deckTrigger("get_artist", "QLC+"));
    QVERIFY(deckTrigger("get_bpm", 120.0));
    djManager->assignShow(filepath, int(show->id()));
    QVERIFY(globalTrigger("masterdeck", 1));
    QVERIFY(deckTrigger("get_time elapsed absolute", double(pausedPosition)));
    QVERIFY(deckTrigger("play", "off"));

    timer->start();
    if (alreadyRunningPaused)
    {
        show->setSyncSource(ShowRunner::Autonomous);
        show->start(timer, FunctionParent::master(), 23000);
        QTRY_VERIFY(show->isRunning());
        show->setPause(true);
        QTRY_VERIFY(show->isPaused());
    }

    QQmlContext context(m_app->rootContext());
    context.setContextProperty("mainView", m_app->rootObject());
    QQmlComponent component(m_app->engine(), QUrl("qrc:/ShowManager.qml"));
    panelObject.reset(component.create(&context));
    QVERIFY2(panelObject, qPrintable(component.errorString()));
    auto *panel = qobject_cast<QQuickItem*>(panelObject.get());
    QVERIFY(panel);
    panel->setParentItem(m_app->contentItem());
    panel->setZ(10000);
    QCoreApplication::processEvents();

    QQmlExpression headerExpression(qmlContext(panel), panel, "hdrItem");
    auto *header = qobject_cast<QQuickItem*>(
            headerExpression.evaluate().value<QObject*>());
    QVERIFY2(!headerExpression.hasError(), qPrintable(headerExpression.error().toString()));
    QVERIFY(header);
    QQmlExpression timelineExpression(qmlContext(panel), panel, "timelineHeader");
    auto *timeline = qobject_cast<QQuickItem*>(
            timelineExpression.evaluate().value<QObject*>());
    QVERIFY2(!timelineExpression.hasError(), qPrintable(timelineExpression.error().toString()));
    QVERIFY(timeline);
    QQmlExpression itemsExpression(qmlContext(panel), panel, "itemsArea");
    auto *itemsArea = qobject_cast<QQuickItem*>(
            itemsExpression.evaluate().value<QObject*>());
    QVERIFY2(!itemsExpression.hasError(), qPrintable(itemsExpression.error().toString()));
    QVERIFY(itemsArea);
    auto *cursor = header->findChild<QQuickItem*>("showPlayhead");
    QVERIFY(cursor);

    const auto expectedCursorX = [manager]() {
        if (manager->timeBasedDivision())
            return manager->currentTime() * manager->tickSize()
                    / (1000.0 * manager->timeScale());
        return (manager->bpmNumber() / double(manager->beatsDivision()))
                * manager->tickSize() * (manager->currentTime() / 60000.0);
    };
    const auto cursorIsInViewport = [cursor, timeline]() {
        const qreal viewportX = cursor->mapToItem(timeline, QPointF()).x();
        return viewportX >= 0 && viewportX + cursor->width() <= timeline->width();
    };
    const auto offsetMatches = [panel, timeline, itemsArea](qreal expected) {
        return qAbs(panel->property("xViewOffset").toReal() - expected) < 0.5
                && qAbs(timeline->property("contentX").toReal() - expected) < 0.5
                && qAbs(itemsArea->property("contentX").toReal() - expected) < 0.5;
    };

    QSignalSpy runnerPosition(show, &Show::timeChanged);
    djManager->setPerformMode(true);
    QCOMPARE(bridge->performFsm()->state(), PerformFsm::PerformState::Suspended);
    QTRY_COMPARE(manager->currentShowID(), int(show->id()));
    QTRY_VERIFY(manager->readOnly());
    QTRY_COMPARE(manager->currentTime(), int(pausedPosition));
    QCOMPARE(manager->isPlaying(), alreadyRunningPaused);
    QCOMPARE(manager->isPaused(), alreadyRunningPaused);
    QCOMPARE(show->isRunning(), alreadyRunningPaused);
    QCOMPARE(show->isPaused(), alreadyRunningPaused);

    QTRY_VERIFY(cursor->isVisible());
    QTRY_VERIFY(qAbs(cursor->x() - expectedCursorX()) < 0.5);
    QTRY_VERIFY(cursorIsInViewport());

    QVERIFY(deckTrigger("get_time elapsed absolute", double(pausedUpdate)));
    QTRY_COMPARE(manager->currentTime(), int(pausedUpdate));
    djManager->loadShow(filepath);
    QTRY_COMPARE(manager->currentShowID(), int(show->id()));
    QTRY_COMPARE(manager->currentTime(), int(pausedUpdate));
    QCOMPARE(manager->isPlaying(), alreadyRunningPaused);
    QCOMPARE(manager->isPaused(), alreadyRunningPaused);
    QTRY_VERIFY(qAbs(cursor->x() - expectedCursorX()) < 0.5);
    QTRY_VERIFY(cursorIsInViewport());

    panel->setProperty("xViewOffset", 0);
    QTRY_VERIFY(offsetMatches(0));
    runnerPosition.clear();
    QVERIFY(deckTrigger("get_time elapsed absolute", double(resumePosition)));
    QVERIFY(deckTrigger("play", "on"));
    QCOMPARE(bridge->performFsm()->state(), PerformFsm::PerformState::Live);
    QTRY_VERIFY(show->isRunning() && !show->isPaused());
    QTRY_VERIFY(manager->isPlaying() && !manager->isPaused());
    QTRY_COMPARE(manager->currentTime(), int(resumePosition));
    QTRY_VERIFY_WITH_TIMEOUT(std::any_of(runnerPosition.cbegin(), runnerPosition.cend(),
                             [resumePosition](const QList<QVariant> &position) {
        return position.first().toUInt() == resumePosition;
    }), 1000);
    QTRY_VERIFY(qAbs(cursor->x() - expectedCursorX()) < 0.5);
    QTRY_VERIFY(cursorIsInViewport());
    QVERIFY(panel->property("xViewOffset").toReal() > 0);

    QSignalSpy displayedPositions(manager, &ShowManager::currentTimeChanged);
    const QPoint cursorPoint = cursor->mapToScene(
            QPointF(0, header->property("headerHeight").toReal() / 2)).toPoint();
    const QPoint seekPoint = cursorPoint + QPoint(80, 0);
    QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, seekPoint);
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, cursorPoint);
    QTest::mouseMove(m_app.get(), seekPoint, 20);
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, seekPoint);
    QCoreApplication::processEvents();
    QCOMPARE(bridge->performFsm()->state(), PerformFsm::PerformState::Live);
    QCOMPARE(manager->currentTime(), int(resumePosition));
    for (const QList<QVariant> &position : displayedPositions)
        QCOMPARE(position.first().toUInt(), resumePosition);

    djManager->setPerformMode(false);
    QCOMPARE(bridge->performFsm()->state(), PerformFsm::PerformState::Idle);
    QTRY_VERIFY(!manager->readOnly());
    show->stopAndWait();
    QTRY_VERIFY(!show->isRunning());
    QTRY_VERIFY(!manager->isPlaying());
    QVERIFY(cursor->isVisible());

    manager->setCurrentTime(15000);
    panel->setProperty("xViewOffset", 2000);
    QTRY_VERIFY(offsetMatches(2000));
    manager->setCurrentTime(10000);
    QCoreApplication::processEvents();
    QVERIFY(offsetMatches(2000));
    QVERIFY(cursor->isVisible());
    QVERIFY(!cursorIsInViewport());

    manager->setTimeScale(2.0);
    QTRY_VERIFY(qAbs(cursor->x() - expectedCursorX()) < 0.5);
    QVERIFY(offsetMatches(2000));
    manager->setTimeDivision(Show::BPM_4_4);
    manager->setBpmNumber(90);
    QTRY_VERIFY(qAbs(cursor->x() - expectedCursorX()) < 0.5);
    QVERIFY(offsetMatches(2000));

    manager->setCurrentTime(220000);
    panel->setProperty("xViewOffset", 0);
    QTRY_VERIFY(offsetMatches(0));
    show->setSyncSource(ShowRunner::Autonomous);
    manager->playShow();
    QTRY_VERIFY(show->isRunning() && !show->isPaused());
    QTRY_VERIFY(manager->isPlaying() && !manager->isPaused());
    QTRY_VERIFY(cursorIsInViewport());
    QVERIFY(cursor->isVisible());
    manager->playShow();
    QTRY_VERIFY(manager->isPaused());
    QVERIFY(cursor->isVisible());
    manager->stopShow();
    QTRY_VERIFY(!manager->isPlaying());
    QVERIFY(cursor->isVisible());
}

QTEST_MAIN(UpstreamIntegration_Test)
#include "upstreamintegration_test.moc"
