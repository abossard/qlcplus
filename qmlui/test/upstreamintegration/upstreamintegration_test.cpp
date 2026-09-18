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
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <memory>
#include <cmath>
#include <QScopeGuard>

#include "app.h"
#include "doc.h"
#include "mastertimer.h"
#include "scene.h"
#include "show.h"
#include "showmanager.h"
#include "tardis.h"
#include "vccuelist.h"

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
private:
    std::unique_ptr<App> m_app;
};

void UpstreamIntegration_Test::initTestCase()
{
    const QString settings = QDir::current().absoluteFilePath("isolated-settings");
    QVERIFY(QDir().mkpath(settings));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settings);
    QCoreApplication::setOrganizationName("QLCPlusIntegrationTests");
    QCoreApplication::setApplicationName("UpstreamIntegration");
    m_app = std::make_unique<App>();
    m_app->set3dSupported(false);
    m_app->startup();
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

QTEST_MAIN(UpstreamIntegration_Test)
#include "upstreamintegration_test.moc"
