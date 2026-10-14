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
#include <QAccessible>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <algorithm>

#include "app.h"
#include "contextmanager.h"
#include "doc.h"
#include "djmanager.h"
#include "editorview.h"
#include "fixture.h"
#include "fixtureeditor.h"
#include "fixturegroup.h"
#include "fixturemanager.h"
#include "functionmanager.h"
#include "inputoutputmap.h"
#include "mastertimer.h"
#include "monitorproperties.h"
#include "performfsm.h"
#include "qlccapability.h"
#include "qlcfixturedef.h"
#include "qlcinputsource.h"
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
#include "video.h"
#include "videoeditor.h"
#include "treemodel.h"
#include "treemodelitem.h"
#include "sceneeditor.h"
#include "chasereditor.h"
#include "collectioneditor.h"
#include "efxeditor.h"
#include "vcbutton.h"
#include "virtualconsole.h"
#include "vcframe.h"
#include "vcslider.h"
#include "vcbridgev5.h"
#include "vclayoutadapter.h"
#include "qlcfixturemode.h"
#include "fixtureutils.h"

#define private public
#include "scriptv4.h"
#undef private

class UpstreamIntegration_Test : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void textInputDeletion_data();
    void textInputDeletion();
    void functionDeletionConfirmation_data();
    void functionDeletionConfirmation();
    void actionsSubmenuDismissal_data();
    void actionsSubmenuDismissal();
    void channelWizard_data();
    void channelWizard();
    void videoEditorUrlDialog_data();
    void videoEditorUrlDialog();
    void removedFunctionLeavesNoTreeReference();
    void functionRemovalProjection_data();
    void functionRemovalProjection();
    void mixedAuthoring_data();
    void mixedAuthoring();
    void rulerChangeGeometry_data();
    void rulerChangeGeometry();
    void fileOpenGuard_data();
    void fileOpenGuard();
    void cueListWidths_data();
    void cueListWidths();
    void columnResize_data();
    void columnResize();
    void cueListResizeGuard_data();
    void cueListResizeGuard();
    void fixtureGroupDrag_data();
    void fixtureGroupDrag();
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
    void scriptDurationCache_data();
    void scriptDurationCache();
    void externalFunctionRename_data();
    void externalFunctionRename();
    void timelineViewport_data();
    void timelineViewport();
    void previewRepeatBounds_data();
    void previewRepeatBounds();
    void editorPublication_data();
    void editorPublication();
    void colorSelectionMarker_data();
    void colorSelectionMarker();
    void identicalWidgetFont();
    void selectedFixtureColors_data();
    void selectedFixtureColors();
    void colorPressOwnership();
    void liveFontDialog_data();
    void liveFontDialog();
    void vcCanonicalGeometry_data();
    void vcCanonicalGeometry();
    void vcIdentitySelection();
    void vcStructureEdit_data();
    void vcStructureEdit();
    void vcGeometryAdmission_data();
    void vcGeometryAdmission();
    void vcOutlineInventory();
    void vcPanesCoexist_data();
    void vcPanesCoexist();
    void vcLayoutPreview_data();
    void vcLayoutPreview();
    void vcCreateAdmission_data();
    void vcCreateAdmission();
    void vcSelectionDrop_data();
    void vcSelectionDrop();
    void vcAuthoringAccess_data();
    void vcAuthoringAccess();
    void vcOutlineNotifications_data();
    void vcOutlineNotifications();
    void vcScreenshotConsole();
    void vcLayoutRoundTrip_data();
    void vcLayoutRoundTrip();
    void vcReparentBindings_data();
    void vcReparentBindings();
    void vcZoomedCopy_data();
    void vcZoomedCopy();
    void vcMatrixAdmission_data();
    void vcMatrixAdmission();
    void vcHistoryReplay_data();
    void vcHistoryReplay();
    void vcWorkspacePersistence();
    void vcOutlinePointerSelection_data();
    void vcOutlinePointerSelection();
    void vcBridgeCanonicalGeometry_data();
    void vcBridgeCanonicalGeometry();
    void vcLegacyPlanScope_data();
    void vcLegacyPlanScope();
    void vcLegacyOrderInitialization_data();
    void vcLegacyOrderInitialization();
    void vcPageZoomProjection();
    void vcClipboardAtomicity_data();
    void vcClipboardAtomicity();
    void vcReparentInputMaps_data();
    void vcReparentInputMaps();
    void vcNestedSoloRouting_data();
    void vcNestedSoloRouting();
    void vcClipboardPublication_data();
    void vcClipboardPublication();
    void vcKeyboardPasteOrigin_data();
    void vcKeyboardPasteOrigin();
    void vcLegacyPageColumnOrder_data();
    void vcLegacyPageColumnOrder();
    void vcGeometryArithmetic_data();
    void vcGeometryArithmetic();
    void vcCreationArithmetic_data();
    void vcCreationArithmetic();
    void vcHeaderFitArithmetic_data();
    void vcHeaderFitArithmetic();
    void vcSnappedFarEdgeArithmetic_data();
    void vcSnappedFarEdgeArithmetic();
    void vcFunctionCreationArithmetic_data();
    void vcFunctionCreationArithmetic();
    void vcNestedLeafPointerResize_data();
    void vcNestedLeafPointerResize();
    void vcHeldEdgeMinimum_data();
    void vcHeldEdgeMinimum();
    void vcDefaultHeaderDrop_data();
    void vcDefaultHeaderDrop();
    void vcLegacyLeafPointerResize_data();
    void vcLegacyLeafPointerResize();
    void vcClipboardPointerDrop_data();
    void vcClipboardPointerDrop();
private:
    QTemporaryDir m_settings;
    std::unique_ptr<App> m_app;
};

void UpstreamIntegration_Test::vcCanonicalGeometry_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::newRow("unit") << qreal(1);
    QTest::newRow("zoom-out") << qreal(0.5);
    QTest::newRow("zoom-in") << qreal(2);
}

void UpstreamIntegration_Test::vcCanonicalGeometry()
{
    QFETCH(qreal, scale);
    VirtualConsole *vc = m_app->virtualConsole();
    VCFrame *page = vc->page(0);
    auto *frame = new VCFrame(m_app->doc(), vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    const quint32 frameID = frame->id();
    const bool wasModified = m_app->doc()->isModified();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    auto *button = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setGeometry(QRectF(20, 60, 100, 80));
    button->setScaleFactor(scale);
    QCOMPARE(button->geometry(), QRectF(20 * scale, 60 * scale, 100 * scale, 80 * scale));
    QCOMPARE(button->logicalGeometry(), QRectF(20, 60, 100, 80));

    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    QVERIFY(button->saveXML(&writer));
    QXmlStreamReader reader(xml);
    bool found = false;
    while (!reader.atEnd())
    {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == KXMLQLCWindowState)
        {
            const auto attributes = reader.attributes();
            QCOMPARE(attributes.value(KXMLQLCWindowStateX).toDouble(), qreal(20));
            QCOMPARE(attributes.value(KXMLQLCWindowStateY).toDouble(), qreal(60));
            QCOMPARE(attributes.value(KXMLQLCWindowStateWidth).toDouble(), qreal(100));
            QCOMPARE(attributes.value(KXMLQLCWindowStateHeight).toDouble(), qreal(80));
            found = true;
        }
    }
    QVERIFY(found);
    QVERIFY(!reader.hasError());
}

void UpstreamIntegration_Test::vcIdentitySelection()
{
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldEdit = vc->editMode();
    const QVariantList previousSelection = vc->selectedWidgetIDs();
    auto *button = new VCButton(m_app->doc(), vc->page(0));
    vc->addWidgetToMap(button);
    vc->page(0)->addWidgetToPageMap(button);
    const quint32 id = button->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({id});
        vc->setSelectedWidgetIDs(previousSelection);
        vc->setEditMode(oldEdit);
    });
    vc->setEditMode(true);
    QVERIFY(!button->renderItem());
    vc->setSelectedWidgetIDs({id, id, VCWidget::invalidId()});
    QCOMPARE(vc->selectedWidgetIDs(), QVariantList{id});
    QCOMPARE(vc->selectedWidgetsCount(), 1);
    QCOMPARE(vc->selectedWidget(), button);
    QVERIFY(button->isEditing());
    QQuickItem projection;
    vc->setWidgetSelection(id, &projection, true, true);
    QCOMPARE(vc->selectedWidgetIDs(), QVariantList{id});
    QVERIFY(projection.property("isSelected").toBool());
    vc->resetWidgetSelection();
    QCOMPARE(vc->selectedWidgetsCount(), 0);
    QVERIFY(!button->isEditing());
    QVERIFY(!projection.property("isSelected").toBool());
}

void UpstreamIntegration_Test::vcStructureEdit_data()
{
    QTest::addColumn<QString>("operation");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("multi-root") << QString("move") << true;
    QTest::newRow("deduplicate") << QString("duplicate") << true;
    QTest::newRow("ancestor-selection") << QString("ancestor") << true;
    QTest::newRow("reorder-only") << QString("reorder") << true;
    QTest::newRow("cycle") << QString("cycle") << false;
    QTest::newRow("self") << QString("self") << false;
    QTest::newRow("invalid-subpage") << QString("subpage") << false;
    QTest::newRow("unknown-widget") << QString("missing") << false;
}

void UpstreamIntegration_Test::vcStructureEdit()
{
    QFETCH(QString, operation);
    QFETCH(bool, accepted);
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page = vc->page(0);
    auto *source = new VCFrame(m_app->doc(), vc, page);
    auto *target = new VCFrame(m_app->doc(), vc, page);
    for (VCFrame *frame : {source, target})
    {
        vc->addWidgetToMap(frame);
        page->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setGeometry(QRectF(0, 0, 800, 600));
    }
    target->setGeometry(QRectF(900, 0, 800, 600));
    source->setGeometry(QRectF(0, 0, 500, 400));
    const quint32 sourceID = source->id();
    const quint32 targetID = target->id();
    const bool wasModified = m_app->doc()->isModified();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({sourceID, targetID});
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    auto *first = new VCButton(m_app->doc(), source);
    auto *second = new VCButton(m_app->doc(), source);
    for (VCButton *button : {first, second})
    {
        vc->addWidgetToMap(button);
        source->addWidgetToPageMap(button);
        button->setGeometry(QRectF(button == first ? 200 : 20, 60, 100, 80));
    }
    const QVariantList beforeOrder = source->orderedChildIDs(0);
    QCOMPARE(beforeOrder, (QVariantList{first->id(), second->id()}));
    const QRectF beforeFirst = first->geometry();
    const QRectF beforeSecond = second->geometry();
    m_app->doc()->resetModified();
    QVariantList ids{first->id(), second->id()};
    quint32 destination = targetID;
    int subpage = 0;
    if (operation == "duplicate")
        ids = {first->id(), first->id(), second->id()};
    else if (operation == "ancestor")
        ids = {sourceID, first->id()};
    else if (operation == "reorder")
    {
        ids = {second->id()};
        destination = sourceID;
    }
    else if (operation == "cycle")
    {
        ids = {sourceID};
        destination = first->id();
    }
    else if (operation == "self")
    {
        ids = {sourceID};
        destination = sourceID;
    }
    else if (operation == "subpage")
        subpage = 99;
    else if (operation == "missing")
        ids.append(VCWidget::invalidId());

    const QVariantMap result = vc->editStructure(ids, destination, subpage, 0);
    QCOMPARE(result.value("accepted").toBool(), accepted);
    QCOMPARE(result.value("applied").toBool(), accepted);
    QCOMPARE(first->geometry(), beforeFirst);
    QCOMPARE(second->geometry(), beforeSecond);
    if (!accepted)
    {
        QVERIFY(!result.value("error").toString().isEmpty());
        QCOMPARE(first->parent(), source);
        QCOMPARE(second->parent(), source);
        QCOMPARE(source->orderedChildIDs(0), beforeOrder);
        QVERIFY(!m_app->doc()->isModified());
    }
    else
    {
        QVERIFY(m_app->doc()->isModified());
        if (operation == "ancestor")
        {
            QCOMPARE(source->parent(), target);
            QCOMPARE(first->parent(), source);
            QCOMPARE(result.value("widgetIDs").toList(), QVariantList{sourceID});
        }
        else if (operation == "reorder")
        {
            QCOMPARE(source->orderedChildIDs(0), (QVariantList{second->id(), first->id()}));
        }
        else
        {
            QCOMPARE(first->parent(), target);
            QCOMPARE(second->parent(), target);
            QCOMPARE(target->orderedChildIDs(0), beforeOrder);
            QVERIFY(source->orderedChildIDs(0).isEmpty());
        }
    }
}

void UpstreamIntegration_Test::vcGeometryAdmission_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<QString>("operation");
    for (qreal scale : {qreal(0.5), qreal(0.75), qreal(1), qreal(1.1), qreal(2)})
        for (const QString &operation : {QString("edge"), QString("collision"),
                                        QString("legacy"), QString("minimum"), QString("descendant"),
                                        QString("wrapped-caption"), QString("legacy-plan"), QString("float-move")})
            QTest::newRow(qPrintable(QString("%1-%2").arg(operation).arg(scale))) << scale << operation;
}

void UpstreamIntegration_Test::vcGeometryAdmission()
{
    QFETCH(qreal, scale);
    QFETCH(QString, operation);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setGeometry(QRectF(0, 0, 500, 400));
    frame->setScaleFactor(scale);
    const quint32 frameID = frame->id();
    const bool wasModified = m_app->doc()->isModified();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        if (!wasModified) m_app->doc()->resetModified();
    });
    auto *button = new VCButton(m_app->doc(), frame);
    auto *sibling = new VCButton(m_app->doc(), frame);
    for (VCButton *entry : {button, sibling})
    {
        vc->addWidgetToMap(entry);
        frame->addWidgetToPageMap(entry);
        entry->setLogicalGeometry(QRectF(entry == button ? 20 : 200, 60, 100, 80));
        entry->setScaleFactor(scale);
    }
    if (operation == "wrapped-caption")
        button->setCaption(QString("A long caption which wraps inside its existing control ").repeated(4));
    if (operation == "float-move")
        button->setLogicalGeometry(QRectF(20, 60, 101.2, 80.3));
    m_app->doc()->resetModified();
    const QRectF original = button->logicalGeometry();
    QRectF request(490, 300, 100, 80);
    quint32 editedID = button->id();
    bool accepted = true;
    if (operation == "collision")
    {
        request = QRectF(200, 60, 100, 80);
        accepted = false;
    }
    else if (operation == "legacy" || operation == "legacy-plan")
    {
        sibling->setLogicalGeometry(QRectF(-100, -100, 80, 60));
        m_app->doc()->resetModified();
        request = QRectF(40, 100, 100, 80);
    }
    else if (operation == "minimum")
        request = QRectF(20, 60, 1, 1);
    else if (operation == "wrapped-caption")
        request = QRectF(40, 100, 100, 80);
    else if (operation == "float-move")
        request = QRectF(40, 100, button->geometry().width() / scale, button->geometry().height() / scale);
    else if (operation == "descendant")
    {
        editedID = frameID;
        request = QRectF(0, 0, 150, 150);
        accepted = false;
    }
    const QMap<quint32, QRectF> plan{{editedID, request}, {sibling->id(), sibling->logicalGeometry()}};
    const QVariantMap preview = operation == "legacy-plan" ? vc->previewGeometryPlan(plan)
                                                           : vc->previewGeometry(editedID, request, false);
    QCOMPARE(preview.value("accepted").toBool(), accepted);
    QVERIFY(!preview.value("applied").toBool());
    QCOMPARE(button->logicalGeometry(), original);
    QVERIFY(!m_app->doc()->isModified());
    const QVariantMap result = operation == "legacy-plan" ? vc->applyGeometryPlan(plan)
                                                          : vc->editGeometry(editedID, request, false);
    QCOMPARE(result.value("accepted").toBool(), accepted);
    if (accepted)
    {
        const QRectF rectangle = button->logicalGeometry();
        QVERIFY(frame->contentRect().contains(rectangle));
        QVERIFY(rectangle.width() >= button->minimumSize().width());
        QVERIFY(rectangle.height() >= button->minimumSize().height());
        QVERIFY(!rectangle.intersects(sibling->logicalGeometry()));
        const QRectF previewRect = preview.value("geometries").toList().first().toMap().value("rect").toRectF();
        QCOMPARE(rectangle, previewRect);
        if (operation == "edge")
            QCOMPARE(rectangle, QRectF(400, 300, 100, 80));
        if (operation == "legacy")
            QCOMPARE(rectangle, QRectF(40, 100, 100, 80));
        if (operation == "wrapped-caption")
            QCOMPARE(rectangle, QRectF(40, 100, 100, 80));
        if (operation == "float-move")
        {
            QVERIFY(rectangle.width() == original.width());
            QVERIFY(rectangle.height() == original.height());
        }
    }
    else
    {
        QVERIFY(!result.value("error").toString().isEmpty());
        QCOMPARE(button->logicalGeometry(), original);
        QVERIFY(!m_app->doc()->isModified());
    }
}

void UpstreamIntegration_Test::vcOutlineInventory()
{
    VirtualConsole *vc = m_app->virtualConsole();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(3);
    const quint32 frameID = frame->id();
    const auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({frameID}); });
    auto *button = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(button);
    button->setPage(1);
    button->setCaption("Hidden Head");
    frame->addWidgetToPageMap(button);
    frame->setCollapsed(true);
    QAbstractItemModel *model = vc->outlineModel();
    QVERIFY(model);
    QCoreApplication::processEvents();
    int widgetRole = -1, kindRole = -1;
    const auto roles = model->roleNames();
    for (auto it = roles.constBegin(); it != roles.constEnd(); ++it)
    {
        if (it.value() == "widgetID") widgetRole = it.key();
        if (it.value() == "rowKind") kindRole = it.key();
    }
    QVERIFY(widgetRole >= 0);
    QVERIFY(kindRole >= 0);
    QHash<quint32, int> counts;
    int subpages = 0;
    std::function<void(const QModelIndex &)> visit = [&](const QModelIndex &parent) {
        for (int row = 0; row < model->rowCount(parent); ++row)
        {
            const QModelIndex index = model->index(row, 0, parent);
            const int kind = model->data(index, kindRole).toInt();
            if (kind == 1)
                ++counts[model->data(index, widgetRole).toUInt()];
            if (kind == 2)
                ++subpages;
            visit(index);
        }
    };
    visit({});
    QCOMPARE(counts.value(frameID), 1);
    QCOMPARE(counts.value(button->id()), 1);
    QCOMPARE(subpages, 3);
}

void UpstreamIntegration_Test::vcPanesCoexist_data()
{
    QTest::addColumn<QString>("operation");
    QTest::newRow("open") << QString("open");
    QTest::newRow("mode-toggle") << QString("mode-toggle");
    QTest::newRow("no-match") << QString("no-match");
}

void UpstreamIntegration_Test::vcPanesCoexist()
{
    QFETCH(QString, operation);
    VirtualConsole *vc = m_app->virtualConsole();
    const QString oldFilter = vc->outlineModel()->filterText();
    const bool oldEdit = vc->editMode();
    const bool oldVisible = m_app->isVisible();
    const QSize oldSize = m_app->size();
    const auto cleanup = qScopeGuard([&]() {
        vc->outlineModel()->setFilterText(oldFilter);
        vc->setEditMode(oldEdit);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
    });
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1440, 900);
    QQmlComponent component(m_app->engine(), QUrl("qrc:/VirtualConsole.qml"));
    std::unique_ptr<QObject> object(component.create(qmlContext(m_app->rootObject())));
    auto *console = qobject_cast<QQuickItem *>(object.get());
    QVERIFY2(console, qPrintable(component.errorString()));
    console->setParentItem(m_app->rootObject());
    console->setWidth(1440);
    console->setHeight(900);
    QVERIFY(console->setProperty("outlineOpen", true));
    vc->setEditMode(operation != "mode-toggle");
    auto *outline = console->findChild<QQuickItem *>("vcOutlinePane");
    auto *functions = console->findChild<QQuickItem *>("vcFunctionsPane");
    QVERIFY(outline);
    QVERIFY(functions);
    QVERIFY(functions->setProperty("loaderSource", "qrc:/FunctionManager.qml"));
    QVERIFY(QMetaObject::invokeMethod(functions, "animatePanel", Q_ARG(QVariant, true)));
    QTRY_VERIFY(outline->isVisible() && outline->width() > 100);
    QTRY_VERIFY(functions->isVisible() && functions->width() > 100);
    if (operation == "mode-toggle")
    {
        vc->setEditMode(true);
        QCOMPARE(functions->property("loaderSource").toString(), QString("qrc:/FunctionManager.qml"));
        QVERIFY(functions->property("isOpen").toBool());
    }
    else if (operation == "no-match")
    {
        auto *search = console->findChild<QQuickItem *>("vcOutlineSearch");
        auto *empty = console->findChild<QQuickItem *>("vcOutlineNoMatches");
        QVERIFY(search);
        QVERIFY(empty);
        QVERIFY(search->setProperty("text", "absent-vc-widget-987654321"));
        QTRY_VERIFY(empty->isVisible());
        const QPointF location = empty->mapToItem(outline, QPointF());
        QVERIFY(location.y() >= 0 && location.y() + empty->height() <= outline->height());
    }
    QVERIFY(vc->editMode());
    const QPointF outlineEnd = outline->mapToItem(console, QPointF(outline->width(), 0));
    const QPointF functionsStart = functions->mapToItem(console, QPointF(0, 0));
    QVERIFY(outlineEnd.x() < functionsStart.x());
}

void UpstreamIntegration_Test::vcLayoutPreview_data()
{
    QTest::addColumn<QString>("finish");
    QTest::newRow("confirm") << QString("confirm");
    QTest::newRow("stale") << QString("stale");
    QTest::newRow("cancel") << QString("cancel");
}

void UpstreamIntegration_Test::vcLayoutPreview()
{
    QFETCH(QString, finish);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 400));
    const quint32 frameID = frame->id();
    const bool wasModified = m_app->doc()->isModified();
    const auto cleanup = qScopeGuard([&]() {
        vc->cancelLayout();
        vc->deleteVCWidgets({frameID});
        if (!wasModified) m_app->doc()->resetModified();
    });
    auto *first = new VCButton(m_app->doc(), frame);
    auto *second = new VCButton(m_app->doc(), frame);
    for (VCButton *button : {first, second})
    {
        vc->addWidgetToMap(button);
        frame->addWidgetToPageMap(button);
        button->setLogicalGeometry(QRectF(button == first ? 150 : 350, 100, 100, 80));
    }
    const QRectF originalFirst = first->logicalGeometry();
    const QRectF originalSecond = second->logicalGeometry();
    const QVariantList originalOrder = frame->orderedChildIDs(0);
    m_app->doc()->resetModified();
    QVariantMap preview = vc->previewLayout(frameID, {{"padding", 10}, {"gap", 10}});
    QVERIFY2(preview.value("accepted").toBool(), qPrintable(preview.value("error").toString()));
    QVERIFY(!preview.value("applied").toBool());
    QCOMPARE(first->logicalGeometry(), originalFirst);
    QCOMPARE(second->logicalGeometry(), originalSecond);
    QCOMPARE(frame->orderedChildIDs(0), originalOrder);
    QVERIFY(!m_app->doc()->isModified());
    QVERIFY(!vc->layoutPreview().isEmpty());
    if (finish == "stale")
    {
        first->setLogicalGeometry(QRectF(160, 100, 100, 80));
        m_app->doc()->resetModified();
    }
    else if (finish == "cancel")
        vc->cancelLayout();
    const QRectF beforeConfirm = first->logicalGeometry();
    const QVariantMap result = vc->confirmLayout();
    QCOMPARE(result.value("accepted").toBool(), finish == "confirm");
    if (finish == "confirm")
    {
        QVERIFY(result.value("applied").toBool());
        QVERIFY(frame->contentRect().contains(first->logicalGeometry()));
        QVERIFY(!first->logicalGeometry().intersects(second->logicalGeometry()));
        QCOMPARE(frame->orderedChildIDs(0), originalOrder);
    }
    else
    {
        QVERIFY(!result.value("error").toString().isEmpty());
        QCOMPARE(first->logicalGeometry(), beforeConfirm);
        QCOMPARE(second->logicalGeometry(), originalSecond);
        QVERIFY(!m_app->doc()->isModified());
    }
}

void UpstreamIntegration_Test::vcCreateAdmission_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<bool>("smallParent");
    for (const QString &type : {QString("Button"), QString("Slider"), QString("XYPad"), QString("Frame")})
        QTest::newRow(qPrintable(type)) << type << false;
    QTest::newRow("insufficient-content") << QString("Slider") << true;
}

void UpstreamIntegration_Test::vcCreateAdmission()
{
    QFETCH(QString, type);
    QFETCH(bool, smallParent);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(smallParent ? QRectF(0, 0, 30, 20) : QRectF(0, 0, 600, 500));
    const quint32 id = frame->id();
    const bool wasModified = m_app->doc()->isModified();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({id});
        if (!wasModified) m_app->doc()->resetModified();
    });
    m_app->doc()->resetModified();
    const QPointF position(590, 490);
    const QVariantMap preview = vc->previewCreate(id, type, position);
    QCOMPARE(preview.value("accepted").toBool(), !smallParent);
    QVERIFY(frame->children(false).isEmpty());
    QVERIFY(!m_app->doc()->isModified());
    const QVariantMap result = vc->createWidget(id, type, position);
    QCOMPARE(result.value("accepted").toBool(), !smallParent);
    if (smallParent)
    {
        QVERIFY(!result.value("error").toString().isEmpty());
        QVERIFY(frame->children(false).isEmpty());
        QVERIFY(!m_app->doc()->isModified());
    }
    else
    {
        QCOMPARE(frame->children(false).size(), 1);
        VCWidget *created = frame->children(false).first();
        QCOMPARE(created->logicalGeometry(), preview.value("rect").toRectF());
        QVERIFY(frame->contentRect().contains(created->logicalGeometry()));
        QVERIFY(created->logicalGeometry().width() >= created->minimumSize().width());
        QVERIFY(created->logicalGeometry().height() >= created->minimumSize().height());
    }
}

void UpstreamIntegration_Test::vcSelectionDrop_data()
{
    QTest::addColumn<QString>("operation");
    for (const QString &operation : {QString("same-parent"), QString("cross-parent"), QString("edge"), QString("no-fit")})
        QTest::newRow(qPrintable(operation)) << operation;
}

void UpstreamIntegration_Test::vcSelectionDrop()
{
    QFETCH(QString, operation);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldSnap = vc->snapping();
    const bool wasModified = m_app->doc()->isModified();
    auto *source = new VCFrame(m_app->doc(), vc, vc->page(0));
    auto *target = new VCFrame(m_app->doc(), vc, vc->page(0));
    for (VCFrame *frame : {source, target})
    {
        vc->addWidgetToMap(frame);
        vc->page(0)->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setLogicalGeometry(QRectF(0, 0, 800, 400));
    }
    target->setLogicalGeometry(QRectF(900, 0, operation == "no-fit" ? 250 : 800, 400));
    const quint32 sourceID = source->id(), targetID = target->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({sourceID, targetID});
        vc->setSnapping(oldSnap);
        if (!wasModified) m_app->doc()->resetModified();
    });
    auto *first = new VCButton(m_app->doc(), source);
    auto *second = new VCButton(m_app->doc(), source);
    for (VCButton *button : {first, second})
    {
        vc->addWidgetToMap(button);
        source->addWidgetToPageMap(button);
        button->setLogicalGeometry(QRectF(button == first ? 20 : 200, 60, 100, 80));
    }
    vc->setSnapping(false);
    m_app->doc()->resetModified();
    const QVariantList ids{first->id(), second->id(), first->id()};
    const QVariantList originalOrder = source->orderedChildIDs(0);
    const quint32 destination = operation == "same-parent" ? sourceID : targetID;
    const QPointF requested(operation == "edge" ? 700 : 50, 100);
    QVariantMap preview = vc->previewSelectionDrop(ids, first->id(), destination, 0, requested);
    QCOMPARE(preview.value("accepted").toBool(), operation != "no-fit");
    QCOMPARE(first->logicalGeometry(), QRectF(20, 60, 100, 80));
    QVERIFY(!m_app->doc()->isModified());
    QVariantMap result = vc->editSelectionDrop(ids, first->id(), destination, 0, requested);
    QCOMPARE(result.value("accepted").toBool(), operation != "no-fit");
    if (operation == "no-fit")
    {
        QCOMPARE(first->parent(), source);
        QCOMPARE(second->parent(), source);
        QVERIFY(!m_app->doc()->isModified());
    }
    else
    {
        VCFrame *destinationFrame = operation == "same-parent" ? source : target;
        QCOMPARE(first->parent(), destinationFrame);
        QCOMPARE(second->parent(), destinationFrame);
        QCOMPARE(second->logicalGeometry().topLeft() - first->logicalGeometry().topLeft(), QPointF(180, 0));
        QVERIFY(destinationFrame->contentRect().contains(second->logicalGeometry()));
        QCOMPARE(destinationFrame->orderedChildIDs(0), originalOrder);
    }
}

void UpstreamIntegration_Test::vcAuthoringAccess_data()
{
    QTest::addColumn<QString>("restriction");
    for (const QString &restriction : {QString("operate-selection"), QString("page-pin"),
                                       QString("temporary-unlock"), QString("access-mask")})
        QTest::newRow(qPrintable(restriction)) << restriction;
}

void UpstreamIntegration_Test::vcAuthoringAccess()
{
    QFETCH(QString, restriction);
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page = vc->page(0);
    const bool oldEdit = vc->editMode(), wasModified = m_app->doc()->isModified();
    const int oldMask = m_app->accessMask(), oldPIN = page->PIN();
    auto *source = new VCFrame(m_app->doc(), vc, page);
    auto *target = new VCFrame(m_app->doc(), vc, page);
    for (VCFrame *frame : {source, target})
    {
        vc->addWidgetToMap(frame);
        page->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setLogicalGeometry(QRectF(frame == source ? 0 : 900, 0, 600, 400));
    }
    const quint32 sourceID = source->id(), targetID = target->id();
    const auto cleanup = qScopeGuard([&]() {
        source->setPIN(0);
        page->setPIN(oldPIN);
        m_app->setAccessMask(oldMask);
        vc->deleteVCWidgets({sourceID, targetID});
        vc->setEditMode(oldEdit);
        if (!wasModified) m_app->doc()->resetModified();
    });
    auto *button = new VCButton(m_app->doc(), source);
    vc->addWidgetToMap(button);
    source->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 60, 100, 80));
    const quint32 buttonID = button->id();
    if (restriction == "operate-selection")
    {
        vc->setEditMode(false);
        m_app->doc()->resetModified();
        vc->setSelectedWidgetIDs({buttonID});
        QVERIFY(vc->selectedWidgetIDs().isEmpty());
        QVERIFY(!button->isEditing());
        QVERIFY(!m_app->doc()->isModified());
        return;
    }
    if (restriction == "page-pin") page->setPIN(1234);
    else if (restriction == "temporary-unlock")
    {
        page->setPIN(1234);
        QVERIFY(vc->validatePagePIN(0, "1234", false));
        QVERIFY(page->requirePIN());
        QVERIFY(vc->editGeometry(buttonID, QRectF(50, 100, 100, 80), false).value("accepted").toBool());
        return;
    }
    else m_app->setAccessMask(oldMask & ~App::AC_VCEditing);
    m_app->doc()->resetModified();
    const QRectF geometry = button->logicalGeometry();
    const QVariantList order = source->orderedChildIDs(0);
    const QVariantMap moved = vc->editGeometry(buttonID, QRectF(50, 100, 100, 80), false);
    QVERIFY(!moved.value("accepted").toBool());
    QVERIFY(!moved.value("error").toString().isEmpty());
    QVERIFY(!vc->editStructureBefore({buttonID}, targetID, 0).value("accepted").toBool());
    QVERIFY(!vc->createWidget(sourceID, "Button", QPointF(200, 100)).value("accepted").toBool());
    QVERIFY(!vc->deleteAuthoringWidgets({buttonID}).value("accepted").toBool());
    QCOMPARE(vc->widget(buttonID), button);
    QCOMPARE(button->logicalGeometry(), geometry);
    QCOMPARE(source->orderedChildIDs(0), order);
    QVERIFY(!m_app->doc()->isModified());
}

void UpstreamIntegration_Test::vcOutlineNotifications_data()
{
    QTest::addColumn<QString>("operation");
    for (const QString &operation : {QString("delete"), QString("lifetime"), QString("selection"), QString("parent-role")})
        QTest::newRow(qPrintable(operation)) << operation;
}

void UpstreamIntegration_Test::vcOutlineNotifications()
{
    QFETCH(QString, operation);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldEdit = vc->editMode();
    vc->setEditMode(true);
    auto *source = new VCFrame(m_app->doc(), vc, vc->page(0));
    auto *target = new VCFrame(m_app->doc(), vc, vc->page(0));
    for (VCFrame *frame : {source, target})
    {
        vc->addWidgetToMap(frame);
        vc->page(0)->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setLogicalGeometry(QRectF(frame == source ? 0 : 900, 0, 600, 400));
    }
    const quint32 sourceID = source->id(), targetID = target->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({sourceID, targetID});
        vc->setEditMode(oldEdit);
    });
    QList<VCButton *> buttons;
    for (int i = 0; i < 3; ++i)
    {
        auto *button = new VCButton(m_app->doc(), source);
        vc->addWidgetToMap(button);
        source->addWidgetToPageMap(button);
        button->setLogicalGeometry(QRectF(20 + i * 150, 60, 100, 80));
        buttons.append(button);
    }
    VCOutlineModel *model = vc->outlineModel();
    const quint32 firstID = buttons[0]->id(), secondID = buttons[1]->id();
    std::function<QModelIndex(quint32, const QModelIndex &)> find =
        [&](quint32 id, const QModelIndex &parent) -> QModelIndex {
        for (int row = 0; row < model->rowCount(parent); ++row)
        {
            const QModelIndex index = model->index(row, 0, parent);
            if (model->data(index, VCOutlineModel::RowKindRole).toInt() == VCOutlineModel::WidgetRow &&
                model->data(index, VCOutlineModel::WidgetIDRole).toUInt() == id)
                return index;
            QModelIndex nested = find(id, index);
            if (nested.isValid()) return nested;
        }
        return {};
    };
    QTRY_VERIFY(find(firstID, {}).isValid());
    QPersistentModelIndex persistent(find(secondID, {}));
    const QModelIndex deletedIndex = find(firstID, {});
    const QString deletedKey = model->data(deletedIndex, VCOutlineModel::RowKeyRole).toString();
    QSignalSpy moves(model, &QAbstractItemModel::rowsMoved);
    QSignalSpy removals(model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy resets(model, &QAbstractItemModel::modelReset);
    QList<quint32> changedIDs;
    QSet<int> changedRoles;
    const auto connection = connect(model, &QAbstractItemModel::dataChanged, this,
        [&](const QModelIndex &first, const QModelIndex &last, const QList<int> &roles) {
        for (int row = first.row(); row <= last.row(); ++row)
        {
            const QModelIndex index = model->index(row, 0, first.parent());
            if (model->data(index, VCOutlineModel::RowKindRole).toInt() == VCOutlineModel::WidgetRow)
                changedIDs.append(model->data(index, VCOutlineModel::WidgetIDRole).toUInt());
        }
        for (int role : roles) changedRoles.insert(role);
    });
    const auto disconnectObserver = qScopeGuard([&]() { disconnect(connection); });
    if (operation == "delete" || operation == "lifetime")
    {
        vc->deleteVCWidgets({firstID});
        if (operation == "lifetime")
            QCOMPARE(model->data(deletedIndex, VCOutlineModel::RowKeyRole).toString(), deletedKey);
        QTRY_VERIFY(removals.count() > 0);
        QTRY_VERIFY(!find(firstID, {}).isValid());
        QCOMPARE(moves.count(), 0);
    }
    else if (operation == "selection")
    {
        vc->setSelectedWidgetIDs({firstID});
        QTRY_VERIFY(!changedIDs.isEmpty());
        QCOMPARE(changedIDs, QList<quint32>{firstID});
    }
    else
    {
        const QVariantMap result = vc->editStructureBefore({firstID}, targetID, 0);
        QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
        QTRY_COMPARE(model->data(find(firstID, {}), VCOutlineModel::ParentIDRole).toUInt(), targetID);
        QVERIFY(changedRoles.contains(VCOutlineModel::ParentIDRole));
        QVERIFY(changedRoles.contains(VCOutlineModel::ParentKeyRole));
    }
    QVERIFY(persistent.isValid());
    QCOMPARE(model->data(persistent, VCOutlineModel::WidgetIDRole).toUInt(), secondID);
    QCOMPARE(resets.count(), 0);
}

void UpstreamIntegration_Test::vcScreenshotConsole()
{
    const QString artifactDirectory = qEnvironmentVariable("QLC_VC_VISUAL_DIR");
    if (artifactDirectory.isEmpty())
        QSKIP("Run explicitly with QLC_VC_VISUAL_DIR for the isolated native fixture");
    QVERIFY(QDir().mkpath(artifactDirectory));
    QFile source(QStringLiteral(QT_TESTCASE_SOURCEDIR "/../../qml/virtualconsole/VirtualConsole.qml"));
    QFile resource(":/VirtualConsole.qml");
    QVERIFY(source.open(QIODevice::ReadOnly));
    QVERIFY(resource.open(QIODevice::ReadOnly));
    QCOMPARE(resource.readAll(), source.readAll());
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const QSize oldSize = m_app->size();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode();
    const bool wasModified = m_app->doc()->isModified();
    const int oldPage = vc->selectedPage();
    QVariantList roots;
    QList<quint32> functionIDs;
    const auto cleanup = qScopeGuard([&]() {
        vc->cancelLayout();
        vc->deleteVCWidgets(roots);
        for (quint32 id : functionIDs) m_app->doc()->deleteFunction(id);
        vc->setSelectedPage(oldPage);
        vc->setEditMode(oldEdit);
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified) m_app->doc()->resetModified();
    });
    for (const QString &caption : {QString("Red"), QString("Blue"), QString("White")})
    {
        auto *scene = new Scene(m_app->doc());
        scene->setName(caption);
        QVERIFY(m_app->doc()->addFunction(scene));
        functionIDs.append(scene->id());
    }
    auto *functionManager = qobject_cast<FunctionManager *>(
        m_app->rootContext()->contextProperty("functionManager").value<QObject *>());
    QVERIFY(functionManager);
    functionManager->updateFunctionsTree();
    qInfo() << "Native fixture metrics" << "density" << vc->pixelDensity()
            << "screenDensity" << m_app->rootContext()->contextProperty("screenPixelDensity")
            << "devicePixelRatio" << m_app->devicePixelRatio();
    auto create = [&](VCFrame *parent, const QString &type, const QRectF &rect, const QString &caption) {
        const QVariantMap result = vc->createWidgetGeometry(parent->id(), type, rect, 0);
        if (!result.value("accepted").toBool())
        {
            qWarning() << "Native fixture creation refused:" << result;
            return static_cast<VCWidget *>(nullptr);
        }
        VCWidget *widget = vc->widget(result.value("widgetIDs").toList().first().toUInt());
        widget->setCaption(caption);
        if (parent == vc->page(0)) roots.append(widget->id());
        return widget;
    };
    auto *head1 = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(20, 20, 360, 340), "Head 1"));
    auto *head2 = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(400, 20, 360, 340), "Head 2"));
    auto *scanner = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(20, 370, 360, 350), "Scanner"));
    auto *laser = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(400, 370, 340, 200), "Laser"));
    auto *led = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(400, 590, 340, 130), "LED"));
    auto *scenes = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(780, 20, 240, 700), "Scenes / Master"));
    QVERIFY(head1 && head2 && scanner && laser && led && scenes);
    for (VCFrame *head : {head1, head2})
    {
        for (int row = 0; row < 2; ++row)
            for (int column = 0; column < 3; ++column)
            {
                auto *button = qobject_cast<VCButton *>(create(head, "Button",
                    QRectF(10 + column * 110, 50 + row * 65, 100, 55),
                    QStringList{"Red", "Blue", "White"}[column]));
                QVERIFY(button);
                button->setFunctionID(functionIDs[column]);
            }
        QVERIFY(create(head, "Slider", QRectF(10, 190, 90, 140), "Intensity"));
        QVERIFY(create(head, "XYPad", QRectF(110, 190, 140, 140), "Position"));
        QVERIFY(create(head, "Button", QRectF(253, 190, 100, 55), "Reset"));
    }
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 3; ++column)
            QVERIFY(create(scanner, "Button", QRectF(10 + column * 110, 50 + row * 65, 100, 55),
                           QString("Gobo %1").arg(row * 3 + column + 1)));
    QVERIFY(create(scanner, "XYPad", QRectF(10, 180, 140, 140), "Position"));
    QVERIFY(create(scanner, "Slider", QRectF(170, 180, 70, 140), "Speed"));
    QVERIFY(create(scanner, "Slider", QRectF(250, 180, 70, 140), "Dimmer"));
    for (VCFrame *frame : {laser, led})
        for (int column = 0; column < 3; ++column)
            QVERIFY(create(frame, "Button", QRectF(10 + column * 110, 50, 100, 60),
                           QStringList{"Red", "Green", "Blue"}[column]));
    for (int row = 0; row < 5; ++row)
        QVERIFY(create(scenes, "Button", QRectF(10, 50 + row * 65, 220, 55),
                       QString("Scene %1").arg(row + 1)));
    QVERIFY(create(scenes, "Slider", QRectF(10, 400, 90, 200), "Master"));
    QVERIFY(create(scenes, "Slider", QRectF(130, 400, 90, 200), "Fade"));

    m_app->doc()->masterTimer()->stop();
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1600, 950);
    contexts->switchToContext("VC");
    vc->setSelectedPage(0);
    QQuickItem *console = nullptr;
    QTRY_VERIFY((console = m_app->rootObject()->findChild<QQuickItem *>("virtualConsole")));
    QVERIFY(console->setProperty("outlineOpen", true));
    vc->setEditMode(true);
    auto *functions = console->findChild<QQuickItem *>("vcFunctionsPane");
    auto *outline = console->findChild<QQuickItem *>("vcOutlinePane");
    QVERIFY(functions && outline);
    QVERIFY(functions->setProperty("loaderSource", "qrc:/FunctionManager.qml"));
    QVERIFY(QMetaObject::invokeMethod(functions, "animatePanel", Q_ARG(QVariant, true)));
    QTRY_VERIFY(functions->width() > 100 && outline->width() > 100);
    QTRY_VERIFY(head1->renderItem());
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    auto rectangle = [](const QRectF &rect) {
        return QJsonObject{{"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}};
    };
    auto globalRect = [&](QQuickItem *item) {
        const QPointF local = item->mapToScene(QPointF());
        const QPoint global = m_app->mapToGlobal(local.toPoint());
        return rectangle(QRectF(global, QSizeF(item->width(), item->height())));
    };
    auto receipt = [&]() {
        QJsonArray widgets, namedItems, functionRows;
        for (VCWidget *widget : vc->page(0)->children(true))
        {
            QJsonObject record{{"id", int(widget->id())}, {"type", VCWidget::typeToString(widget->type())},
                {"caption", widget->caption()}, {"subpage", widget->page()},
                {"logicalRect", rectangle(widget->logicalGeometry())}};
            if (VCWidget *parent = qobject_cast<VCWidget *>(widget->parent()))
                record["parentID"] = int(parent->id());
            if (widget->renderItem()) record["globalRect"] = globalRect(widget->renderItem());
            widgets.append(record);
        }
        std::function<void(QQuickItem *)> visit = [&](QQuickItem *item)
        {
            if (item->isVisible() && item->width() > 0 && item->height() > 0)
            {
            if (item->objectName().startsWith("vc"))
                namedItems.append(QJsonObject{{"name", item->objectName()}, {"globalRect", globalRect(item)}});
            if (item->property("itemType").toInt() == App::FunctionDragItem)
            {
                Function *function = item->property("cRef").value<Function *>();
                if (function && functionIDs.contains(function->id()))
                    functionRows.append(QJsonObject{{"id", int(function->id())}, {"caption", function->name()},
                                                    {"globalRect", globalRect(item)}});
            }
            }
            for (QQuickItem *child : item->childItems()) visit(child);
        };
        visit(m_app->rootObject());
        QJsonArray selection, order;
        for (const QVariant &id : vc->selectedWidgetIDs()) selection.append(int(id.toUInt()));
        QList<VCFrame *> containers{vc->page(0)};
        for (VCWidget *widget : vc->page(0)->children(true))
            if (VCFrame *frame = qobject_cast<VCFrame *>(widget)) containers.append(frame);
        for (VCFrame *frame : containers)
            for (int page = 0; page < frame->totalPagesNumber(); ++page)
            {
                QJsonArray ids;
                for (const QVariant &id : frame->orderedChildIDs(page)) ids.append(int(id.toUInt()));
                order.append(QJsonObject{{"parentID", int(frame->id())}, {"subpage", page}, {"ids", ids}});
            }
        return QJsonObject{{"pid", int(QCoreApplication::applicationPid())},
            {"window", rectangle(QRectF(m_app->position(), m_app->size()))},
            {"devicePixelRatio", m_app->devicePixelRatio()}, {"widgets", widgets},
            {"namedItems", namedItems}, {"functionRows", functionRows}, {"loadedResourceMatchesSource", true},
            {"selectedWidgetIDs", selection}, {"outlineFilterText", vc->outlineModel()->filterText()},
            {"editMode", vc->editMode()}, {"order", order}, {"pageScale", vc->page(0)->pageScale()},
            {"snappingEnabled", vc->snapping()}, {"logicalSnapStep", vc->snappingSize()}};
    };
    QJsonObject initial;
    QTRY_VERIFY(!(initial = receipt()).value("functionRows").toArray().isEmpty());
    QFile ready(QDir(artifactDirectory).filePath("ready.json"));
    QVERIFY(ready.open(QIODevice::WriteOnly));
    ready.write(QJsonDocument(initial).toJson());
    ready.close();
    QVERIFY(m_app->grabWindow().save(QDir(artifactDirectory).filePath("before.png")));
    qInfo() << "Native VC fixture ready" << artifactDirectory;
    bool durationValid = false;
    const int hold = qEnvironmentVariableIntValue("QLC_VC_VISUAL_HOLD_MS", &durationValid);
    if (durationValid && hold > 0) QTest::qWait(hold);
    QVERIFY(m_app->grabWindow().save(QDir(artifactDirectory).filePath("after.png")));
    QFile final(QDir(artifactDirectory).filePath("after.json"));
    QVERIFY(final.open(QIODevice::WriteOnly));
    final.write(QJsonDocument(receipt()).toJson());
    for (quint32 id : functionIDs) QVERIFY(!m_app->doc()->function(id)->isRunning());
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcLayoutRoundTrip_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<bool>("storedOrder");
    for (qreal scale : {qreal(0.75), qreal(1.1), qreal(2)})
        for (bool stored : {true, false})
            QTest::newRow(qPrintable(QString("%1-%2").arg(scale).arg(stored ? "stored" : "legacy")))
                << scale << stored;
}

void UpstreamIntegration_Test::vcLayoutRoundTrip()
{
    QFETCH(qreal, scale);
    QFETCH(bool, storedOrder);
    VirtualConsole *vc = m_app->virtualConsole();
    VCFrame *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(3);
    frame->setLogicalGeometry(QRectF(10.25, 10.5, 600.75, 400.25));
    const quint32 rootID = frame->id();
    const bool wasModified = m_app->doc()->isModified();
    const auto cleanup = qScopeGuard([&]() {
        if (vc->widget(frame->id()) == frame) vc->deleteVCWidgets({frame->id()});
        else delete frame;
        if (!wasModified) m_app->doc()->resetModified();
    });
    QList<quint32> ids;
    const QList<QRectF> rectangles{QRectF(200.25, 60.75, 100.5, 80.25),
                                   QRectF(20.25, 60.75, 90.4, 85.5),
                                   QRectF(10.75, 70.1, 120.5, 90.3)};
    for (int i = 0; i < rectangles.size(); ++i)
    {
        auto *button = new VCButton(m_app->doc(), frame);
        vc->addWidgetToMap(button);
        button->setPage(i == 2 ? 1 : 0);
        frame->addWidgetToPageMap(button);
        button->setLogicalGeometry(rectangles[i]);
        button->setScaleFactor(scale);
        ids.append(button->id());
    }
    frame->setScaleFactor(scale);
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    QVERIFY(frame->saveXML(&writer));
    if (!storedOrder)
        xml.replace(" ChildOrder=\"1\"", "");
    vc->deleteVCWidgets({rootID});
    frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    QBuffer input(&xml);
    QVERIFY(input.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&input);
    QVERIFY(reader.readNextStartElement());
    QVERIFY(frame->loadXML(reader));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    QCOMPARE(frame->logicalGeometry(), QRectF(10.25, 10.5, 600.75, 400.25));
    for (int i = 0; i < ids.size(); ++i)
    {
        VCWidget *button = vc->widget(ids[i]);
        QVERIFY(button);
        QCOMPARE(button->logicalGeometry(), rectangles[i]);
        QCOMPARE(button->page(), i == 2 ? 1 : 0);
    }
    QCOMPARE(frame->orderedChildIDs(0), (storedOrder ? QVariantList{ids[0], ids[1]}
                                                      : QVariantList{ids[1], ids[0]}));
    QCOMPARE(frame->orderedChildIDs(1), QVariantList{ids[2]});
    QVERIFY(frame->orderedChildIDs(2).isEmpty());
}

void UpstreamIntegration_Test::vcReparentBindings_data()
{
    QTest::addColumn<bool>("reorderOnly");
    QTest::newRow("reorder-keeps-gain") << true;
    QTest::newRow("cross-parent-routes-driver") << false;
}

void UpstreamIntegration_Test::vcReparentBindings()
{
    QFETCH(bool, reorderOnly);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *source = new VCFrame(m_app->doc(), vc, vc->page(0));
    auto *target = new VCFrame(m_app->doc(), vc, vc->page(0));
    for (VCFrame *frame : {source, target})
    {
        vc->addWidgetToMap(frame);
        vc->page(0)->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setLogicalGeometry(QRectF(frame == source ? 0 : 900, 0, 600, 400));
    }
    source->adjustIntensity(0.5);
    target->adjustIntensity(0.25);
    const quint32 sourceID = source->id(), targetID = target->id();
    const auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({sourceID, targetID}); });
    const QVariantMap sourceCreation = vc->createWidgetGeometry(sourceID, "Button", QRectF(20, 60, 100, 80));
    const QVariantMap targetCreation = vc->createWidgetGeometry(targetID, "Button", QRectF(20, 60, 100, 80));
    QVERIFY(sourceCreation.value("accepted").toBool());
    QVERIFY(targetCreation.value("accepted").toBool());
    VCWidget *sourceButton = vc->widget(sourceCreation.value("widgetIDs").toList().first().toUInt());
    VCWidget *targetButton = vc->widget(targetCreation.value("widgetIDs").toList().first().toUInt());
    auto *driver = new VCSlider(m_app->doc(), source);
    driver->setSliderMode(VCSlider::Submaster);
    driver->setValue(128);
    driver->setLogicalGeometry(QRectF(200, 60, 90, 200));
    source->addWidget(nullptr, driver, QPoint(200, 60));
    QCOMPARE(driver->intensity(), qreal(1));
    QCOMPARE(sourceButton->intensity(), qreal(128) / 255 * 0.5);
    const quint32 driverID = driver->id();
    const QVariantMap result = vc->editStructure({driverID}, reorderOnly ? sourceID : targetID, 0, 0);
    QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
    QCOMPARE(driver->intensity(), qreal(1));
    if (reorderOnly)
        QCOMPARE(sourceButton->intensity(), qreal(128) / 255 * 0.5);
    else
    {
        QCOMPARE(sourceButton->intensity(), qreal(0.5));
        QCOMPARE(targetButton->intensity(), qreal(128) / 255 * 0.25);
        driver->setValue(64);
        QCOMPARE(sourceButton->intensity(), qreal(0.5));
        QCOMPARE(targetButton->intensity(), qreal(64) / 255 * 0.25);
    }
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcZoomedCopy_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<qreal>("targetScale");
    QTest::addColumn<bool>("nested");
    QTest::newRow("zoom-out") << qreal(0.75) << qreal(1) << false;
    QTest::newRow("fractional-zoom") << qreal(1.1) << qreal(1) << false;
    QTest::newRow("zoom-in") << qreal(2) << qreal(1) << false;
    for (qreal targetScale : {qreal(0.75), qreal(1.1), qreal(2)})
        QTest::newRow(qPrintable(QString("nested-target-%1").arg(targetScale))) << qreal(1.1) << targetScale << true;
}

void UpstreamIntegration_Test::vcZoomedCopy()
{
    QFETCH(qreal, scale);
    QFETCH(qreal, targetScale);
    QFETCH(bool, nested);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *source = new VCFrame(m_app->doc(), vc, vc->page(0));
    auto *target = new VCFrame(m_app->doc(), vc, vc->page(0));
    for (VCFrame *frame : {source, target})
    {
        vc->addWidgetToMap(frame);
        vc->page(0)->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setLogicalGeometry(QRectF(frame == source ? 0 : 900, 0, 600, 400));
    }
    const quint32 sourceID = source->id(), targetID = target->id();
    const bool oldSnap = vc->snapping();
    const qreal oldStep = vc->snappingSize();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({sourceID, targetID});
        vc->setSnapping(oldSnap);
        vc->setSnappingSize(oldStep);
    });
    auto *button = new VCButton(m_app->doc(), source);
    vc->addWidgetToMap(button);
    source->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20.25, 60.5, 101.2, 80.3));
    source->setScaleFactor(scale);
    button->setScaleFactor(scale);
    if (nested) source->setLogicalGeometry(QRectF(0, 0, 250.25, 200.75));
    target->setScaleFactor(targetScale);
    vc->setSnapping(nested);
    vc->setSnappingSize(15);
    target->addWidgetsFromClipboard(nullptr, {nested ? sourceID : button->id()},
                                   QPointF(200 * targetScale, 100 * targetScale).toPoint());
    QCOMPARE(target->children(false).size(), 1);
    VCWidget *copy = target->children(false).first();
    QCOMPARE(copy->logicalGeometry(), nested ? QRectF(195, 105, 250.25, 200.75) : QRectF(200, 100, 101.2, 80.3));
    QCOMPARE(copy->scaleFactor(), target->scaleFactor());
    QVERIFY(copy->recordingId().isNull());
    if (nested)
    {
        auto *frameCopy = qobject_cast<VCFrame *>(copy);
        QVERIFY(frameCopy);
        QCOMPARE(frameCopy->children(false).size(), 1);
        VCWidget *child = frameCopy->children(false).first();
        QCOMPARE(child->logicalGeometry(), QRectF(20.25, 60.5, 101.2, 80.3));
        QCOMPARE(child->scaleFactor(), targetScale);
    }
}

void UpstreamIntegration_Test::vcMatrixAdmission_data()
{
    QTest::addColumn<bool>("fits");
    QTest::newRow("contained") << true;
    QTest::newRow("insufficient-parent") << false;
}

void UpstreamIntegration_Test::vcMatrixAdmission()
{
    QFETCH(bool, fits);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(fits ? QRectF(0, 0, 500, 400) : QRectF(0, 0, 150, 100));
    const quint32 id = frame->id();
    const bool wasModified = m_app->doc()->isModified();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({id});
        if (!wasModified) m_app->doc()->resetModified();
    });
    m_app->doc()->resetModified();
    frame->addWidgetMatrix(nullptr, "buttonmatrix", QPoint(10, 10), QSize(2, 2), QSize(100, 80));
    QCOMPARE(frame->children(false).size(), fits ? 1 : 0);
    if (!fits)
        QVERIFY(!m_app->doc()->isModified());
    else
    {
        auto *matrix = qobject_cast<VCFrame *>(frame->children(false).first());
        QVERIFY(matrix);
        QCOMPARE(matrix->children(false).size(), 4);
        QVERIFY(frame->contentRect().contains(matrix->logicalGeometry()));
        for (VCWidget *widget : matrix->children(false))
            QVERIFY(matrix->contentRect().contains(widget->logicalGeometry()));
    }
}

void UpstreamIntegration_Test::vcHistoryReplay_data()
{
    QTest::addColumn<QString>("format");
    QTest::newRow("legacy-scalar") << QString("scalar");
    QTest::newRow("page-order-map") << QString("map");
    QTest::newRow("missing-target") << QString("missing");
}

void UpstreamIntegration_Test::vcHistoryReplay()
{
    QFETCH(QString, format);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *source = new VCFrame(m_app->doc(), vc, vc->page(0));
    auto *target = new VCFrame(m_app->doc(), vc, vc->page(0));
    for (VCFrame *frame : {source, target})
    {
        vc->addWidgetToMap(frame);
        vc->page(0)->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setLogicalGeometry(QRectF(frame == source ? 0 : 900, 0, 600, 400));
    }
    source->setMultiPageMode(true);
    source->setTotalPagesNumber(2);
    const quint32 sourceID = source->id(), targetID = target->id();
    const auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({sourceID, targetID}); });
    auto *neighbor = new VCButton(m_app->doc(), source);
    vc->addWidgetToMap(neighbor);
    neighbor->setPage(1);
    source->addWidgetToPageMap(neighbor);
    neighbor->setLogicalGeometry(QRectF(20, 100, 100, 80));
    auto *button = new VCButton(m_app->doc(), target);
    vc->addWidgetToMap(button);
    target->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(800, 60, 100, 80));
    TardisAction action;
    action.m_action = Tardis::VCWidgetReparent;
    action.m_timestamp = 0;
    action.m_objID = button->id();
    action.m_oldValue = format == "map"
        ? QVariant(QVariantMap{{"parentID", sourceID}, {"subpage", 1}, {"orderIndex", 0}})
        : QVariant(format == "missing" ? VCWidget::invalidId() : sourceID);
    action.m_newValue = targetID;
    const int code = Tardis::instance()->processAction(action, true);
    if (format == "missing")
    {
        QVERIFY(code < 0);
        QCOMPARE(button->parent(), target);
    }
    else
    {
        QCOMPARE(code, int(Tardis::VCWidgetReparent));
        QCOMPARE(button->parent(), source);
        QCOMPARE(button->logicalGeometry(), QRectF(800, 60, 100, 80));
        if (format == "map")
        {
            QCOMPARE(button->page(), 1);
            QCOMPARE(source->orderedChildIDs(1), (QVariantList{button->id(), neighbor->id()}));
        }
    }
}

void UpstreamIntegration_Test::vcWorkspacePersistence()
{
    QTemporaryDir files(QDir(QCoreApplication::applicationDirPath()).filePath("vc-workspace-XXXXXX"));
    QVERIFY(files.isValid());
    const QString original = files.filePath("original.qxw");
    const QString workspace = files.filePath("edited.qxw");
    const QString oldWorkingPath = m_app->workingPath();
    const bool wasModified = m_app->doc()->isModified();
    QCOMPARE(m_app->saveXML(original), QFile::NoError);
    const auto cleanup = qScopeGuard([&]() {
        QVERIFY(m_app->loadWorkspace(original));
        m_app->doc()->masterTimer()->stop();
        m_app->setWorkingPath(oldWorkingPath);
        if (wasModified) m_app->doc()->setModified();
        else m_app->doc()->resetModified();
    });
    VirtualConsole *vc = m_app->virtualConsole();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(10.25, 20.5, 600.75, 400.25));
    const quint32 frameID = frame->id();
    auto *first = new VCButton(m_app->doc(), frame);
    auto *second = new VCButton(m_app->doc(), frame);
    for (VCButton *button : {first, second})
    {
        vc->addWidgetToMap(button);
        frame->addWidgetToPageMap(button);
        button->setLogicalGeometry(QRectF(button == first ? 20.25 : 200.75, 60.5, 100.5, 80.25));
    }
    const quint32 firstID = first->id(), secondID = second->id();
    vc->page(0)->setPageScale(0.1);
    QVERIFY(vc->editStructure({secondID}, frameID, 0, 0).value("accepted").toBool());
    QVERIFY(m_app->doc()->isModified());
    QCOMPARE(m_app->saveXML(workspace), QFile::NoError);
    QVERIFY(!m_app->doc()->isModified());
    QVERIFY(m_app->loadWorkspace(workspace));
    m_app->doc()->masterTimer()->stop();
    frame = qobject_cast<VCFrame *>(vc->widget(frameID));
    QVERIFY(frame);
    QCOMPARE(frame->logicalGeometry(), QRectF(10.25, 20.5, 600.75, 400.25));
    QCOMPARE(frame->orderedChildIDs(0), (QVariantList{secondID, firstID}));
    QCOMPARE(vc->widget(firstID)->logicalGeometry(), QRectF(20.25, 60.5, 100.5, 80.25));
    QCOMPARE(vc->widget(secondID)->logicalGeometry(), QRectF(200.75, 60.5, 100.5, 80.25));
    for (int index = 0; index < vc->pagesCount(); ++index)
        QCOMPARE(vc->widget(vc->page(index)->id()), vc->page(index));
    m_app->doc()->setModified();
    QVERIFY(m_app->saveXML(files.filePath("missing-parent/failed.qxw")) != QFile::NoError);
    QVERIFY(m_app->doc()->isModified());
}

void UpstreamIntegration_Test::vcOutlinePointerSelection_data()
{
    QTest::addColumn<int>("modifier");
    QTest::addColumn<bool>("smallMove");
    for (int modifier : {int(Qt::NoModifier), int(Qt::ControlModifier), int(Qt::MetaModifier)})
        for (bool move : {false, true})
            QTest::newRow(qPrintable(QString("%1-%2").arg(modifier).arg(move)))
                << modifier << move;
}

void UpstreamIntegration_Test::vcOutlinePointerSelection()
{
    QFETCH(int, modifier);
    QFETCH(bool, smallMove);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode();
    const QSize oldSize = m_app->size();
    const bool wasModified = m_app->doc()->isModified();
    QVariantList roots;
    for (int index = 0; index < 2; ++index)
    {
        const QVariantMap result = vc->createWidgetGeometry(vc->page(0)->id(), "Frame",
            QRectF(20 + index * 350, 20, 300, 200));
        QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
        roots.append(result.value("widgetIDs").toList().first());
    }
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets(roots);
        vc->setEditMode(oldEdit);
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified) m_app->doc()->resetModified();
    });
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1440, 900);
    contexts->switchToContext("VC");
    QQuickItem *console = nullptr;
    QTRY_VERIFY((console = m_app->rootObject()->findChild<QQuickItem *>("virtualConsole")));
    QVERIFY(console->setProperty("outlineOpen", true));
    vc->setEditMode(true);
    auto findRow = [&](quint32 id) {
        QQuickItem *found = nullptr;
        std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
            if (item->objectName() == QString("vcOutlineRow-%1").arg(id)) found = item;
            for (QQuickItem *child : item->childItems()) visit(child);
        };
        visit(console);
        return found;
    };
    QQuickItem *first = nullptr, *second = nullptr;
    QTRY_VERIFY((first = findRow(roots[0].toUInt())));
    QTRY_VERIFY((second = findRow(roots[1].toUInt())));
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    const QPoint firstPoint = first->mapToScene(QPointF(first->width() * 0.45, first->height() / 2)).toPoint();
    const QPoint secondPoint = second->mapToScene(QPointF(second->width() * 0.45, second->height() / 2)).toPoint();
    QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, firstPoint);
    QTRY_COMPARE(vc->selectedWidgetIDs(), QVariantList{roots[0]});
    const QVariantList order = vc->page(0)->orderedChildIDs(0);
    m_app->doc()->resetModified();
    QSignalSpy changed(vc, &VirtualConsole::structureChanged);
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::KeyboardModifiers(modifier), secondPoint);
    if (smallMove) QTest::mouseMove(m_app.get(), secondPoint + QPoint(2, 0));
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::KeyboardModifiers(modifier),
                        secondPoint + (smallMove ? QPoint(2, 0) : QPoint()));
    const QVariantList expected = modifier == Qt::NoModifier ? QVariantList{roots[1]} : roots;
    QTRY_COMPARE(vc->selectedWidgetIDs(), expected);
    QCOMPARE(vc->page(0)->orderedChildIDs(0), order);
    QCOMPARE(changed.count(), 0);
    QVERIFY(!m_app->doc()->isModified());
    QObject *error = console->findChild<QObject *>("vcOutlineError");
    QVERIFY(error);
    QVERIFY(!error->property("visible").toBool());
}

void UpstreamIntegration_Test::vcBridgeCanonicalGeometry_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<bool>("createHidden");
    for (qreal scale : {qreal(0.75), qreal(1.1), qreal(2)})
        for (bool create : {false, true})
            QTest::newRow(qPrintable(QString("%1-%2").arg(scale).arg(create))) << scale << create;
}

void UpstreamIntegration_Test::vcBridgeCanonicalGeometry()
{
    QFETCH(qreal, scale);
    QFETCH(bool, createHidden);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldSnap = vc->snapping();
    const qreal oldStep = vc->snappingSize();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(2);
    frame->setLogicalGeometry(QRectF(0, 0, 800, 600));
    frame->setScaleFactor(scale);
    const quint32 frameID = frame->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        vc->setSnapping(oldSnap);
        vc->setSnappingSize(oldStep);
    });
    auto *button = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 60, 120, 100));
    button->setScaleFactor(scale);
    vc->setSnapping(false);
    vc->setSnappingSize(15);
    VCBridgeV5 bridge(m_app->doc(), vc);
    const QRect requested(37, 83, 137, 103);
    const QRectF expected(qreal(37) / scale, qreal(83) / scale,
                          qreal(137) / scale, qreal(103) / scale);
    if (createHidden)
    {
        const int id = bridge.createWidgetOnPage(frameID, "Button", requested, "Hidden", 1);
        QVERIFY(id >= 0);
        VCWidget *created = vc->widget(id);
        QVERIFY(created);
        QCOMPARE(created->page(), 1);
        QCOMPARE(created->logicalGeometry(), expected);
        QCOMPARE(frame->currentPage(), 0);
        QCOMPARE(frame->children(false).size(), 2);
    }
    else
    {
        const QString error = bridge.setWidgetGeometryChecked(button->id(), requested);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(button->logicalGeometry(), expected);
    }
}

void UpstreamIntegration_Test::vcLegacyPlanScope_data()
{
    QTest::addColumn<bool>("bridgePath");
    QTest::newRow("shared-legacy-adapter") << false;
    QTest::newRow("production-bridge") << true;
}

void UpstreamIntegration_Test::vcLegacyPlanScope()
{
    QFETCH(bool, bridgePath);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 400));
    frame->setScaleFactor(1.1);
    const quint32 frameID = frame->id();
    const auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({frameID}); });
    auto *edited = new VCButton(m_app->doc(), frame);
    auto *legacy = new VCButton(m_app->doc(), frame);
    for (VCButton *button : {edited, legacy})
    {
        vc->addWidgetToMap(button);
        frame->addWidgetToPageMap(button);
        button->setScaleFactor(1.1);
    }
    edited->setLogicalGeometry(QRectF(20.25, 60.5, 100.5, 80.25));
    legacy->setLogicalGeometry(QRectF(-100.25, -60.5, 80.3, 70.7));
    const QRectF original = legacy->logicalGeometry();
    VCBridge::LayoutPlan plan;
    plan.geometries.insert(edited->id(), QRect(100, 150, 120, 100));
    plan.geometries.insert(legacy->id(), legacy->geometry().toRect());
    if (bridgePath)
    {
        VCBridgeV5 bridge(m_app->doc(), vc);
        const QString error = bridge.applyLayoutPlanChecked(plan, true);
        QVERIFY2(error.isEmpty(), qPrintable(error));
    }
    else
    {
        const QVariantMap result = VCLayoutAdapter::applyPlan(vc, plan);
        QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
    }
    QVERIFY(legacy->logicalGeometry().x() == original.x());
    QVERIFY(legacy->logicalGeometry().y() == original.y());
    QVERIFY(legacy->logicalGeometry().width() == original.width());
    QVERIFY(legacy->logicalGeometry().height() == original.height());
    QCOMPARE(edited->geometry().toRect(), QRect(100, 150, 120, 100));
}

void UpstreamIntegration_Test::vcLegacyOrderInitialization_data()
{
    QTest::addColumn<qreal>("step");
    QTest::newRow("small-live-grid") << qreal(2);
    QTest::newRow("large-live-grid") << qreal(20);
}

void UpstreamIntegration_Test::vcLegacyOrderInitialization()
{
    QFETCH(qreal, step);
    VirtualConsole *vc = m_app->virtualConsole();
    const qreal oldStep = vc->snappingSize();
    const bool oldSnap = vc->snapping();
    VCFrame *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 400));
    const quint32 rootID = frame->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frame->id()});
        vc->setSnappingSize(oldStep);
        vc->setSnapping(oldSnap);
    });
    QList<quint32> ids;
    for (int index = 0; index < 2; ++index)
    {
        auto *button = new VCButton(m_app->doc(), frame);
        vc->addWidgetToMap(button);
        frame->addWidgetToPageMap(button);
        button->setLogicalGeometry(QRectF(index == 0 ? 200 : 20, index == 0 ? 40 : 44, 100, 80));
        ids.append(button->id());
    }
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    QVERIFY(frame->saveXML(&writer));
    xml.replace(" ChildOrder=\"1\"", "");
    vc->deleteVCWidgets({rootID});
    vc->setSnapping(true);
    vc->setSnappingSize(step);
    frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    QBuffer buffer(&xml);
    QVERIFY(buffer.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&buffer);
    QVERIFY(reader.readNextStartElement());
    QVERIFY(frame->loadXML(reader));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    QCOMPARE(frame->orderedChildIDs(0), (QVariantList{ids[0], ids[1]}));
    QCOMPARE(vc->widget(ids[0])->logicalGeometry(), QRectF(200, 40, 100, 80));
    QCOMPARE(vc->widget(ids[1])->logicalGeometry(), QRectF(20, 44, 100, 80));
}

void UpstreamIntegration_Test::vcPageZoomProjection()
{
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page = vc->page(0);
    const qreal oldScale = page->pageScale();
    const QRectF logical = page->logicalGeometry();
    const auto restore = qScopeGuard([&]() { page->setPageScale(oldScale - page->pageScale()); });
    page->setPageScale(2 - page->pageScale());
    QCOMPARE(page->pageScale(), qreal(2));
    QCOMPARE(page->geometry().size(), logical.size() * 2);
    const QVariantMap created = vc->createWidgetGeometry(page->id(), "Button", QRectF(1600, 800, 100, 80));
    QVERIFY(created.value("accepted").toBool());
    const quint32 id = created.value("widgetIDs").toList().first().toUInt();
    const auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({id}); });
    QCOMPARE(vc->widget(id)->scaleFactor(), qreal(2));
    QCOMPARE(vc->widget(id)->logicalGeometry(), QRectF(1600, 800, 100, 80));
    QVERIFY(page->geometry().contains(vc->widget(id)->geometry()));
}

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

void UpstreamIntegration_Test::videoEditorUrlDialog_data()
{
    QTest::addColumn<bool>("escape");
    QTest::newRow("cancel") << false;
    QTest::newRow("escape") << true;
}

void UpstreamIntegration_Test::removedFunctionLeavesNoTreeReference()
{
    auto *scene = new Scene(m_app->doc());
    scene->setName("Removed function projection regression");
    QVERIFY(m_app->doc()->addFunction(scene));
    auto *tree = m_app->functionManager()->functionsList().value<TreeModel *>();
    QVERIFY(tree);
    const QString path = scene->name();
    QVERIFY(tree->itemAtPath(path));
    const quint32 id = scene->id();
    QVERIFY(m_app->doc()->deleteFunction(id));
    QVERIFY2(!tree->itemAtPath(path), "FunctionManager retained a row for a deleted Function");
}

void UpstreamIntegration_Test::functionRemovalProjection_data()
{
    QTest::addColumn<bool>("clear");
    QTest::addColumn<bool>("duplicate");
    QTest::newRow("managed-single") << false << false;
    QTest::newRow("document-clear") << true << false;
    QTest::newRow("duplicate-name") << false << true;
}

void UpstreamIntegration_Test::functionRemovalProjection()
{
    QFETCH(bool, clear);
    QFETCH(bool, duplicate);
    auto *view2D = m_app->rootObject()->findChild<QQuickItem *>("twoDView");
    QVERIFY(view2D);
    QQmlExpression popupExpression(qmlContext(view2D), view2D, "monitorPOVPopup");
    QPointer<QObject> monitorPopup = popupExpression.evaluate().value<QObject *>();
    QVERIFY2(monitorPopup, qPrintable(popupExpression.error().toString()));
    const bool wasPopupVisible = monitorPopup->property("visible").toBool();
    const auto oldPointOfView = m_app->doc()->monitorProperties()->pointOfView();
    const QString original = m_settings.filePath("function-removal-original.qxw");
    const QString oldWorkingPath = m_app->workingPath();
    const bool wasModified = m_app->doc()->isModified();
    QCOMPARE(m_app->saveXML(original), QFile::NoError);
    const auto cleanup = qScopeGuard([&]() {
        QSignalSpy ticks(m_app->doc()->masterTimer(), &MasterTimer::tickReady);
        QVERIFY(m_app->loadWorkspace(original));
        m_app->doc()->monitorProperties()->setPointOfView(oldPointOfView);
        QTRY_VERIFY(!ticks.isEmpty());
        m_app->doc()->masterTimer()->stop();
        QVERIFY(monitorPopup);
        QVERIFY(QMetaObject::invokeMethod(monitorPopup.data(), wasPopupVisible ? "open" : "close"));
        QTRY_COMPARE(monitorPopup->property("visible").toBool(), wasPopupVisible);
        m_app->setWorkingPath(oldWorkingPath);
        if (wasModified) m_app->doc()->setModified();
        else m_app->doc()->resetModified();
    });
    FunctionManager *manager = m_app->functionManager();
    auto *tree = manager->functionsList().value<TreeModel *>();
    QVERIFY(tree);
    Function *survivor = new Scene(m_app->doc());
    survivor->setName("Function removal survivor");
    QVERIFY(m_app->doc()->addFunction(survivor));
    QPointer<TreeModelItem> survivorRow(tree->itemAtPath(survivor->name()));
    QVERIFY(survivorRow);
    QVariantList removedIDs;
    for (int index = 0; index < (clear ? 32 : 1); ++index)
    {
        auto *scene = new Scene(m_app->doc());
        scene->setName(duplicate ? survivor->name() : QString("Function removal target %1").arg(index));
        QVERIFY(m_app->doc()->addFunction(scene));
        removedIDs.append(scene->id());
    }
    if (duplicate)
    {
        TreeModelItem *first = tree->itemAtPath(survivor->name());
        Function *firstFunction = first->data(0).value<Function *>();
        if (firstFunction != survivor)
        {
            removedIDs = {survivor->id()};
            survivor = firstFunction;
            survivorRow = first;
        }
    }
    manager->selectFunctionID(removedIDs.first().toUInt(), false);
    QSignalSpy inserts(tree, &QAbstractItemModel::rowsInserted);
    QSignalSpy removals(tree, &QAbstractItemModel::rowsRemoved);
    QSignalSpy resets(tree, &QAbstractItemModel::modelReset);
    QSignalSpy publications(manager, &FunctionManager::functionsListChanged);
    QSignalSpy starts(m_app->doc()->masterTimer(), &MasterTimer::functionStarted);
    if (clear)
    {
        QSignalSpy ticks(m_app->doc()->masterTimer(), &MasterTimer::tickReady);
        m_app->clearDocument();
        QTRY_VERIFY(!ticks.isEmpty());
        m_app->doc()->masterTimer()->stop();
    }
    else
        manager->deleteFunctions(removedIDs);
    if (!duplicate)
        QCOMPARE(inserts.count(), 0);
    QCOMPARE(removals.count(), 1);
    QCOMPARE(resets.count(), 0);
    QCOMPARE(manager->selectedFunctionCount(), 0);
    QCOMPARE(starts.count(), 0);
    if (clear)
    {
        QCOMPARE(tree->rowCount(), 0);
        QCOMPARE(manager->sceneCount(), 0);
        QCOMPARE(publications.count(), 1);
    }
    else
    {
        TreeModelItem *remaining = tree->itemAtPath(survivor->name());
        QVERIFY(remaining);
        QVERIFY(remaining->data(0).value<Function *>() == survivor);
        if (!duplicate)
        {
            QVERIFY(survivorRow);
            QCOMPARE(remaining, survivorRow.data());
        }
        QCOMPARE(publications.count(), duplicate ? 1 : 0);
    }
}

void UpstreamIntegration_Test::videoEditorUrlDialog()
{
    QFETCH(bool, escape);
    const QString originalUrl = "https://example.invalid/original.png";
    const QString cancelledUrl = "https://example.invalid/cancelled.png";
    const QString acceptedUrl = "https://example.invalid/accepted.png";
    auto *video = new Video(m_app->doc());
    video->setSourceUrl(originalUrl);
    QVERIFY(m_app->doc()->addFunction(video));
    const quint32 id = video->id();
    const bool wasVisible = m_app->isVisible();
    const auto cleanup = qScopeGuard([&]() {
        m_app->doc()->deleteFunction(id);
        m_app->setVisible(wasVisible);
    });
    const QVariant previousEditor = m_app->rootContext()->contextProperty("videoEditor");
    VideoEditor backend(m_app.get(), m_app->doc());
    backend.setFunctionID(id);
    const auto restoreEditor = qScopeGuard([&]() {
        m_app->rootContext()->setContextProperty("videoEditor", previousEditor);
    });
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->rootObject()->forceActiveFocus();
    QQmlContext context(qmlContext(m_app->rootObject()));
    context.setContextProperty("mainView", m_app->rootObject());
    QQmlComponent component(m_app->engine(), QUrl("qrc:/VideoEditor.qml"));
    std::unique_ptr<QObject> object(component.create(&context));
    auto *editor = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(editor, qPrintable(component.errorString()));
    editor->setParentItem(m_app->rootObject());
    QQmlExpression expression(qmlContext(editor), editor, "getUrlDialog");
    QObject *popup = expression.evaluate().value<QObject*>();
    QVERIFY2(popup, qPrintable(expression.error().toString()));
    const auto closePopup = qScopeGuard([&]() { QMetaObject::invokeMethod(popup, "close"); });
    auto *input = popup->property("contentItem").value<QQuickItem*>();
    QVERIFY(input);
    QVERIFY(!popup->property("visible").toBool());
    QVERIFY(m_app->activeFocusItem() != input);

    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    QTRY_COMPARE(m_app->activeFocusItem(), input);
    QCOMPARE(input->property("selectedText").toString(), QString("http://"));
    QVERIFY(input->setProperty("text", cancelledUrl));
    QVERIFY(input->setProperty("cursorPosition", cancelledUrl.size()));
    if (escape)
        QTest::keyClick(m_app.get(), Qt::Key_Escape);
    else
        QVERIFY(QMetaObject::invokeMethod(popup, "reject"));
    QTRY_VERIFY(!popup->property("visible").toBool());
    QCOMPARE(video->sourceUrl(), originalUrl);

    m_app->rootObject()->forceActiveFocus();
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("opened").toBool());
    QTRY_COMPARE(m_app->activeFocusItem(), input);
    QCOMPARE(input->property("selectedText").toString(), cancelledUrl);
    QVERIFY(input->setProperty("text", acceptedUrl));
    QVERIFY(QMetaObject::invokeMethod(popup, "accept"));
    QTRY_VERIFY(!popup->property("visible").toBool());
    QCOMPARE(video->sourceUrl(), acceptedUrl);
}

void UpstreamIntegration_Test::textInputDeletion_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<int>("key");
    for (const QString &type : {QString("TextInput"), QString("TextEdit")})
    {
        QTest::newRow(qPrintable(type + "-delete")) << type << int(Qt::Key_Delete);
        QTest::newRow(qPrintable(type + "-backspace")) << type << int(Qt::Key_Backspace);
    }
}

void UpstreamIntegration_Test::textInputDeletion()
{
    QFETCH(QString, type);
    QFETCH(int, key);
    auto *context = m_app->rootContext()->contextProperty("contextManager").value<ContextManager*>();
    QVERIFY(context);
    context->setLastClickedType(App::FunctionDragItem);
    QSignalSpy deletion(context, &ContextManager::requestFunctionsDeletion);
    QQmlComponent component(m_app->engine());
    component.setData(("import QtQuick\n" + type + " { text: \"ABCDE\" }").toUtf8(), QUrl());
    std::unique_ptr<QObject> object(component.create());
    auto *input = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(input, qPrintable(component.errorString()));
    input->setParentItem(m_app->rootObject());
    input->forceActiveFocus();
    QCOMPARE(m_app->activeFocusItem(), input);
    QVERIFY(input->setProperty("cursorPosition", 2));

    QTest::keyClick(m_app.get(), Qt::Key(key));

    QCOMPARE(input->property("text").toString(), key == Qt::Key_Delete ? QString("ABDE") : QString("ACDE"));
    QCOMPARE(deletion.count(), 0);
}

void UpstreamIntegration_Test::functionDeletionConfirmation_data()
{
    QTest::addColumn<bool>("folder");
    QTest::addColumn<bool>("accept");
    for (bool folder : {false, true})
        for (bool accept : {false, true})
            QTest::newRow(qPrintable(QString("%1-%2").arg(folder).arg(accept))) << folder << accept;
}

void UpstreamIntegration_Test::functionDeletionConfirmation()
{
    QFETCH(bool, folder);
    QFETCH(bool, accept);
    auto *context = m_app->rootContext()->contextProperty("contextManager").value<ContextManager*>();
    auto *manager = m_app->rootContext()->contextProperty("functionManager").value<FunctionManager*>();
    QVERIFY(context);
    QVERIFY(manager);
    QVERIFY(QMetaObject::invokeMethod(m_app->rootObject(), "enableContext",
                                     Q_ARG(QVariant, "SHOWMGR"), Q_ARG(QVariant, true)));
    QTRY_COMPARE(m_app->rootObject()->findChildren<QObject*>("funcRightPanel").size(), 2);
    QVERIFY(QMetaObject::invokeMethod(m_app->rootObject(), "enableContext",
                                     Q_ARG(QVariant, "FIXANDFUNC"), Q_ARG(QVariant, true)));
    QObject *panel = nullptr;
    const auto panels = m_app->rootObject()->findChildren<QObject*>("funcRightPanel");
    for (QObject *candidate : panels)
        if (!candidate->property("inShowManager").toBool())
            panel = candidate;
    QVERIFY(panel);
    auto *scene = new Scene(m_app->doc());
    scene->setName("Delete confirmation");
    if (folder)
        scene->setPath("Delete folder");
    QVERIFY(m_app->doc()->addFunction(scene));
    const quint32 id = scene->id();
    manager->updateFunctionsTree();
    if (folder)
        manager->selectFolder("Delete folder", false);
    else
        manager->selectFunctionID(id, false);
    context->setLastClickedType(folder ? App::FolderDragItem : App::FunctionDragItem);
    m_app->rootObject()->forceActiveFocus();
    QObject *popup = panel->findChild<QObject*>("deleteItemsPopup");
    QVERIFY(popup);
    const auto cleanup = qScopeGuard([&]() {
        QMetaObject::invokeMethod(popup, "close");
        manager->deleteFunctions({id});
    });
    QSignalSpy opened(popup, SIGNAL(opened()));
    QSignalSpy deletion(context, &ContextManager::requestFunctionsDeletion);
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);

    context->handleKeyPress(&event);

    QCOMPARE(deletion.count(), 1);
    QTRY_COMPARE(opened.count(), 1);
    QCOMPARE(m_app->doc()->function(id), scene);
    for (QObject *candidate : panels)
    {
        QObject *candidatePopup = candidate->findChild<QObject*>("deleteItemsPopup");
        QVERIFY(candidatePopup);
        QCOMPARE(candidatePopup->property("visible").toBool(), candidate == panel);
    }
    QVERIFY(QMetaObject::invokeMethod(popup, accept ? "accept" : "reject"));
    QCOMPARE(m_app->doc()->function(id) == nullptr, accept);
}

void UpstreamIntegration_Test::actionsSubmenuDismissal_data()
{
    QTest::addColumn<bool>("compact");
    QTest::newRow("normal") << false;
    QTest::newRow("compact") << true;
}

void UpstreamIntegration_Test::actionsSubmenuDismissal()
{
    QFETCH(bool, compact);
    QQmlExpression expression(qmlContext(m_app->rootObject()), m_app->rootObject(), "actionsMenu");
    QObject *menu = expression.evaluate().value<QObject*>();
    QVERIFY2(menu, qPrintable(expression.error().toString()));
    const QVariant originalCompact = menu->property("compactMode");
    const QSize originalSize = m_app->size();
    const bool wasVisible = m_app->isVisible();
    QVERIFY(menu->setProperty("compactMode", compact));
    const auto cleanup = qScopeGuard([&]() {
        QMetaObject::invokeMethod(menu, "close");
        menu->setProperty("compactMode", originalCompact);
        m_app->setVisible(wasVisible);
        m_app->resize(originalSize);
    });
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1200, 800);
    QTRY_COMPARE(m_app->size(), QSize(1200, 800));
    QVERIFY(QMetaObject::invokeMethod(menu, "open"));
    QTRY_VERIFY(menu->property("opened").toBool());
    QObject *submenu = menu->findChild<QObject*>(compact ? "actionsFileMenu" : "actionsNetworkMenu");
    QVERIFY(submenu);
    QVERIFY(menu->setProperty("submenuItem", QVariant::fromValue(submenu)));
    QTRY_VERIFY(submenu->property("opened").toBool());
    QVERIFY(submenu->property("z").toDouble() > menu->property("z").toDouble());
    QVERIFY(submenu->property("width").toDouble() > 0);
    QVERIFY(submenu->property("height").toDouble() > 0);
    QSignalSpy frame(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QVERIFY(frame.wait());
    QCOMPARE(m_app->size(), QSize(1200, 800));

    QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier,
                      QPoint(m_app->width() - 2, m_app->height() - 2));

    QTRY_VERIFY(!menu->property("visible").toBool());
    QTRY_VERIFY(!submenu->property("visible").toBool());
}

void UpstreamIntegration_Test::channelWizard_data()
{
    QTest::addColumn<QString>("type");
    QTest::addColumn<int>("amount");
    QTest::addColumn<QStringList>("components");
    QTest::addColumn<QList<int>>("colours");
    QTest::addColumn<QList<int>>("presets");
    QTest::newRow("Lime-2") << QString("Lime") << 2 << QStringList{"Channel"}
        << QList<int>{QLCChannel::Lime} << QList<int>{QLCChannel::IntensityLime};
    QTest::newRow("Indigo-3") << QString("Indigo") << 3 << QStringList{"Channel"}
        << QList<int>{QLCChannel::Indigo} << QList<int>{QLCChannel::IntensityIndigo};
    QTest::newRow("RGBA-2") << QString("RGBA") << 2 << QStringList{"Red", "Green", "Blue", "Amber"}
        << QList<int>{QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue, QLCChannel::Amber}
        << QList<int>{QLCChannel::IntensityRed, QLCChannel::IntensityGreen, QLCChannel::IntensityBlue, QLCChannel::IntensityAmber};
    QTest::newRow("RGBL-3") << QString("RGBL") << 3 << QStringList{"Red", "Green", "Blue", "Lime"}
        << QList<int>{QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue, QLCChannel::Lime}
        << QList<int>{QLCChannel::IntensityRed, QLCChannel::IntensityGreen, QLCChannel::IntensityBlue, QLCChannel::IntensityLime};
    QTest::newRow("RGB-2") << QString("RGB") << 2 << QStringList{"Red", "Green", "Blue"}
        << QList<int>{QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue}
        << QList<int>{QLCChannel::IntensityRed, QLCChannel::IntensityGreen, QLCChannel::IntensityBlue};
    QTest::newRow("RGBW-2") << QString("RGBW") << 2 << QStringList{"Red", "Green", "Blue", "White"}
        << QList<int>{QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue, QLCChannel::White}
        << QList<int>{QLCChannel::IntensityRed, QLCChannel::IntensityGreen, QLCChannel::IntensityBlue, QLCChannel::IntensityWhite};
    QTest::newRow("RGBAW-2") << QString("RGBAW") << 2 << QStringList{"Red", "Green", "Blue", "Amber", "White"}
        << QList<int>{QLCChannel::Red, QLCChannel::Green, QLCChannel::Blue, QLCChannel::Amber, QLCChannel::White}
        << QList<int>{QLCChannel::IntensityRed, QLCChannel::IntensityGreen, QLCChannel::IntensityBlue, QLCChannel::IntensityAmber, QLCChannel::IntensityWhite};
    QTest::newRow("UV-2") << QString("UV") << 2 << QStringList{"Channel"}
        << QList<int>{QLCChannel::UV} << QList<int>{QLCChannel::IntensityUV};
    QTest::newRow("Dimmer-2") << QString("Dimmer") << 2 << QStringList{"Channel"}
        << QList<int>{QLCChannel::NoColour} << QList<int>{QLCChannel::IntensityDimmer};
}

void UpstreamIntegration_Test::channelWizard()
{
    QFETCH(QString, type);
    QFETCH(int, amount);
    QFETCH(QStringList, components);
    QFETCH(QList<int>, colours);
    QFETCH(QList<int>, presets);
    QLCFixtureDef original;
    auto *seed = new QLCChannel();
    seed->setName("Existing " + type);
    seed->setGroup(QLCChannel::Shutter);
    seed->setControlByte(QLCChannel::LSB);
    seed->setDefaultValue(17);
    QVERIFY(seed->addCapability(new QLCCapability(0, 31, "Closed")));
    QVERIFY(seed->addCapability(new QLCCapability(32, 255, "Open")));
    QVERIFY(original.addChannel(seed));
    QString seedXML;
    QXmlStreamWriter seedWriter(&seedXML);
    QVERIFY(seed->saveXML(&seedWriter));
    const QVariant previousEditor = m_app->rootContext()->contextProperty("fixtureEditor");
    FixtureEditor fixtureEditor(m_app.get(), m_app->doc());
    const auto restoreEditor = qScopeGuard([&]() {
        m_app->rootContext()->setContextProperty("fixtureEditor", previousEditor);
    });
    // Destroy the copied definition after its editor.
    std::unique_ptr<QLCFixtureDef> edited;
    EditorView editor(m_app.get(), 0, &original);
    edited.reset(editor.fixtureDefinition());
    QVERIFY(!editor.isModified());
    QSignalSpy changed(&editor, &EditorView::hasChanged);
    QSignalSpy channelsChanged(&editor, &EditorView::channelsChanged);
    QQmlContext context(qmlContext(m_app->rootObject()));
    context.setContextProperty("fixtureEditorView", m_app->rootObject());
    context.setContextProperty("mainView", m_app->rootObject());
    QQmlComponent component(m_app->engine(), QUrl("qrc:/PopupChannelWizard.qml"));
    std::unique_ptr<QObject> popup(component.create(&context));
    QVERIFY2(popup, qPrintable(component.errorString()));
    QVERIFY(popup->setProperty("editorView", QVariant::fromValue(&editor)));
    const auto cleanup = qScopeGuard([&]() { QMetaObject::invokeMethod(popup.get(), "close"); });
    QQmlExpression comboExpression(qmlContext(popup.get()), popup.get(), "chTypesCombo");
    QObject *combo = comboExpression.evaluate().value<QObject*>();
    QVERIFY2(combo, qPrintable(comboExpression.error().toString()));
    QQmlExpression amountExpression(qmlContext(popup.get()), popup.get(), "amountSpin");
    QObject *spin = amountExpression.evaluate().value<QObject*>();
    QVERIFY2(spin, qPrintable(amountExpression.error().toString()));
    QQmlExpression selectionExpression(qmlContext(popup.get()), popup.get(),
        QString("chTypesCombo.model.findIndex(item => item.mLabel === '%1')").arg(type));
    const int selection = selectionExpression.evaluate().toInt();
    QVERIFY2(!selectionExpression.hasError(), qPrintable(selectionExpression.error().toString()));
    QVERIFY2(selection >= 0, qPrintable("Missing wizard choice: " + type));
    QStringList names;
    for (int i = 1; i <= amount; ++i)
        for (const QString &name : components)
            names.append(QString("%1 %2").arg(name).arg(i));

    for (bool accept : {false, true})
    {
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "open"));
        QTRY_VERIFY(popup->property("opened").toBool());
        QVERIFY(combo->setProperty("currentIndex", selection));
        QVERIFY(spin->setProperty("value", amount));
        QQmlExpression previewExpression(qmlContext(popup.get()), popup.get(),
                                        "itemsList.map(item => item.name)");
        const QStringList preview = previewExpression.evaluate().toStringList();
        QVERIFY2(!previewExpression.hasError(), qPrintable(previewExpression.error().toString()));
        QCOMPARE(preview, names);
        QCOMPARE(edited->channels().size(), 1);
        QVERIFY(!editor.isModified());
        QCOMPARE(changed.count(), 0);
        QCOMPARE(channelsChanged.count(), 0);
        QString previewXML;
        QXmlStreamWriter previewWriter(&previewXML);
        QVERIFY(edited->channels().first()->saveXML(&previewWriter));
        QCOMPARE(previewXML, seedXML);
        QVERIFY(QMetaObject::invokeMethod(popup.get(), accept ? "accept" : "reject"));
        QTRY_VERIFY(!popup->property("visible").toBool());
        QCOMPARE(edited->channels().size(), accept ? names.size() + 1 : 1);
        QCOMPARE(editor.isModified(), accept);
        QString preservedXML;
        QXmlStreamWriter preservedWriter(&preservedXML);
        QVERIFY(edited->channels().first()->saveXML(&preservedWriter));
        QCOMPARE(preservedXML, seedXML);
    }

    QCOMPARE(channelsChanged.count(), names.size());
    QVERIFY(changed.count() > 0);
    for (int i = 0; i < names.size(); ++i)
    {
        const QLCChannel *channel = edited->channels().at(i + 1);
        QString xml;
        QXmlStreamWriter writer(&xml);
        QVERIFY(channel->saveXML(&writer));
        QXmlStreamReader reader(xml);
        QVERIFY(reader.readNextStartElement());
        QLCChannel reloaded;
        QVERIFY(reloaded.loadXML(reader));
        QVERIFY(!reader.hasError());
        for (const QLCChannel *actual : {channel, static_cast<const QLCChannel *>(&reloaded)})
        {
            QCOMPARE(actual->name(), names.at(i));
            QCOMPARE(actual->group(), QLCChannel::Intensity);
            QCOMPARE(actual->colour(), colours.at(i % components.size()));
            QCOMPARE(actual->preset(), presets.at(i % components.size()));
            QCOMPARE(actual->controlByte(), QLCChannel::MSB);
            QCOMPARE(actual->capabilities().size(), 1);
            const QLCCapability *cap = actual->capabilities().first();
            QCOMPARE(cap->min(), 0);
            QCOMPARE(cap->max(), 255);
            QCOMPARE(cap->preset(), QLCCapability::Custom);
            QCOMPARE(cap->name(), names.at(i) + (type == "Dimmer" ? " (0 - 100%)" : " intensity (0 - 100%)"));
        }
    }
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

void UpstreamIntegration_Test::rulerChangeGeometry_data()
{
    QTest::addColumn<int>("fromIndex");
    QTest::addColumn<int>("toIndex");
    QTest::addColumn<double>("timeZoom");
    QTest::addColumn<double>("beatZoom");
    QTest::addColumn<bool>("mixed");
    QTest::newRow("native-bpm-to-time") << 2 << 0 << 5.0 << 1.0 << false;
    for (int from : {0, 1, 2})
        for (int to : {0, 1, 2})
            if (from != to)
                for (const auto &zoom : {QPair<double, double>{5.0, 1.0}, {0.5, 0.5}, {2.0, 2.0}})
                    QTest::newRow(qPrintable(QString("mixed-%1-to-%2-zoom-%3-%4")
                        .arg(from).arg(to).arg(zoom.first).arg(zoom.second)))
                        << from << to << zoom.first << zoom.second << true;
}

void UpstreamIntegration_Test::rulerChangeGeometry()
{
    QFETCH(int, fromIndex);
    QFETCH(int, toIndex);
    QFETCH(double, timeZoom);
    QFETCH(double, beatZoom);
    QFETCH(bool, mixed);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    QVERIFY(manager);
    static const Show::TimeDivision divisions[] = {Show::Time, Show::VDJBeat, Show::BPM_4_4};
    static const uint starts[] = {2500, 6000, 7000, 8000, 9000, 10000, 11000};
    static const uint mixedDurations[] = {5000, 2000, 3000, 6000, 1000, 4000, 5000};
    static const double mixedStartSeconds[] = {2.5, 3.0, 7.0, 4.0, 9.0, 5.0, 11.0};
    static const double mixedDurationSeconds[] = {5.0, 1.0, 3.0, 3.0, 1.0, 2.0, 5.0};
    auto *show = new Show(doc);
    QVERIFY(doc->addFunction(show));
    show->setTimeDivision(Show::Time, 120);
    QList<quint32> functions;
    for (int i = 0; i < 7; ++i)
    {
        auto *scene = new Scene(doc);
        scene->setTempoType(mixed && i % 2 ? Function::Beats : Function::Time);
        QVERIFY(doc->addFunction(scene));
        functions.append(scene->id());
        auto *track = new Track(Function::invalidId(), show);
        auto *sf = new ShowFunction(show->getLatestShowFunctionId());
        sf->setFunctionID(scene->id());
        sf->setStartTime(starts[i]);
        sf->setDuration(mixed ? mixedDurations[i] : 5000);
        QVERIFY(track->addShowFunction(sf));
        QVERIFY(show->addTrack(track));
    }
    const bool wasVisible = m_app->isVisible();
    const auto cleanup = qScopeGuard([&]() {
        manager->resetContents();
        doc->deleteFunction(show->id());
        for (quint32 id : functions)
            doc->deleteFunction(id);
        m_app->setVisible(wasVisible);
    });
    m_app->showNormal();
    m_app->resize(1200, 800);
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    QVERIFY(QMetaObject::invokeMethod(m_app->rootObject(), "switchToContext",
                                     Q_ARG(QVariant, QString("SHOWMGR")),
                                     Q_ARG(QVariant, QString("qrc:/ShowManager.qml"))));
    auto *combo = m_app->rootObject()->findChild<QQuickItem*>("markersCombo");
    QVERIFY(combo);
    manager->setCurrentShowID(show->id());
    QCoreApplication::processEvents();
    QTRY_COMPARE(combo->property("currentIndex").toInt(), 0);
    manager->setTimeScale(timeZoom);
    manager->setTimeDivision(Show::BPM_4_4);
    QCoreApplication::processEvents();
    QTRY_COMPARE(combo->property("currentIndex").toInt(), 2);
    manager->setTimeScale(beatZoom);
    manager->setTimeDivision(divisions[fromIndex]);
    manager->setGridEnabled(false);

    QCoreApplication::processEvents();
    QTRY_COMPARE(combo->property("currentIndex").toInt(), fromIndex);

    for (int index : {fromIndex, toIndex, fromIndex})
    {
        if (combo->property("currentIndex").toInt() != index)
        {
            combo->forceActiveFocus();
            QTest::keyClick(m_app.get(), Qt::Key_Space);
            auto *popup = combo->property("popup").value<QObject*>();
            QVERIFY(popup);
            QTRY_VERIFY(popup->property("visible").toBool());
            QTest::keyClick(m_app.get(), Qt::Key_Home);
            for (int n = 0; n < index; ++n)
                QTest::keyClick(m_app.get(), Qt::Key_Down);
            QTest::keyClick(m_app.get(), Qt::Key_Return);
            QTRY_VERIFY(!popup->property("visible").toBool());
        }
        auto *alignment = m_app->rootObject()->findChild<QObject*>("beatAlignWarningPopup");
        QVERIFY(alignment);
        if (alignment->property("visible").toBool())
        {
            QVERIFY(QMetaObject::invokeMethod(alignment, "accept"));
            QTRY_VERIFY(!alignment->property("visible").toBool());
        }
        QTRY_COMPARE(manager->timeDivision(), divisions[index]);
        QCoreApplication::processEvents();
        QList<QQuickItem*> controls;
        for (auto *item : manager->contextItem()->childItems())
            if (auto *control = item->findChild<QQuickItem*>("clipSelection"))
                controls.append(control);
        QCOMPARE(controls.size(), 7);
        for (auto *control : controls)
        {
            auto *item = control->parentItem();
            auto *sf = item->property("sfRef").value<ShowFunction*>();
            QVERIFY(sf);
            const int trackIndex = item->property("trackIndex").toInt();
            QCOMPARE(sf->startTime(), starts[trackIndex]);
            QCOMPARE(sf->duration(), mixed ? mixedDurations[trackIndex] : uint(5000));
            QCOMPARE(doc->function(sf->functionID())->tempoType(),
                     mixed && trackIndex % 2 ? Function::Beats : Function::Time);
            QVERIFY(item->isVisible());
            QVERIFY(control->isVisible());
            QVERIFY2(control->width() > 0, "The production ruler switch collapsed the visible clip selection target");
            const double pixelsPerSecond = index == 2 ? manager->tickSize() / 2.0
                                                      : manager->tickSize() / timeZoom;
            const double startSeconds = mixed ? mixedStartSeconds[trackIndex] : starts[trackIndex] / 1000.0;
            const double durationSeconds = mixed ? mixedDurationSeconds[trackIndex] : 5.0;
            QVERIFY(qAbs(item->x() - startSeconds * pixelsPerSecond) < 0.01);
            QVERIFY(qAbs(item->width() - durationSeconds * pixelsPerSecond) < 0.01);
            control->forceActiveFocus();
            QTRY_COMPARE(m_app->activeFocusItem(), control);
            auto *accessible = QAccessible::queryAccessibleInterface(control);
            QVERIFY(accessible);
            QVERIFY(!accessible->rect().isEmpty());
            const QPoint point = control->mapToScene(QPointF(control->width() / 2, control->height() / 2)).toPoint();
            QVERIFY2(QRect(QPoint(), m_app->size()).contains(point),
                     qPrintable(QString("point %1,%2 window %3,%4 track %5")
                         .arg(point.x()).arg(point.y()).arg(m_app->width()).arg(m_app->height())
                         .arg(trackIndex)));
            if (mixed)
                QTest::keyClick(m_app.get(), Qt::Key_Space);
            else
                QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, point);
            QVERIFY(item->property("isSelected").toBool());
            QCOMPARE(manager->selectedItemsCount(), 1);
            QCOMPARE(sf->startTime(), starts[trackIndex]);
            QCOMPARE(sf->duration(), mixed ? mixedDurations[trackIndex] : uint(5000));
        }
        QVERIFY(!m_app->grabWindow().isNull());
    }
}

void UpstreamIntegration_Test::fileOpenGuard_data()
{
    QTest::addColumn<QString>("path");
    QTest::newRow("project") << m_settings.filePath("incoming.qxw");
    QTest::newRow("unsupported") << m_settings.filePath("unsupported.txt");
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
    const QString path = m_settings.filePath("cue-resize-incoming.qxw");
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

void UpstreamIntegration_Test::fixtureGroupDrag_data()
{
    QTest::addColumn<QString>("delegateFile");
    QTest::addColumn<bool>("selected");
    QTest::addColumn<bool>("present");
    QTest::addColumn<bool>("control");
    QTest::newRow("selected-absent-fixture") << "FixtureNodeDelegate.qml" << true << false << false;
    QTest::newRow("selected-absent-group-ctrl") << "TreeNodeDelegate.qml" << true << false << true;
    QTest::newRow("selected-present-head") << "FixtureHeadDelegate.qml" << true << true << false;
    QTest::newRow("selected-present-fixture-ctrl") << "FixtureNodeDelegate.qml" << true << true << true;
    QTest::newRow("unselected-replacement-group") << "TreeNodeDelegate.qml" << false << false << false;
    QTest::newRow("same-id-distinct-heads-ctrl") << "FixtureHeadDelegate.qml" << true << false << true;
}

void UpstreamIntegration_Test::fixtureGroupDrag()
{
    QFETCH(QString, delegateFile);
    QFETCH(bool, selected);
    QFETCH(bool, present);
    QFETCH(bool, control);
    auto *doc = m_app->doc();
    auto *manager = m_app->rootContext()->contextProperty("fixtureManager").value<FixtureManager*>();
    QVERIFY(manager);
    manager->groupsTreeModel();
    auto *fixture = new Fixture(doc);
    fixture->setChannels(3);
    QVERIFY(doc->addFixture(fixture));
    FixtureGroup group(doc);
    group.setId(42);
    group.setName("Drag group");
    const auto cleanup = qScopeGuard([&]() { doc->deleteFixture(fixture->id()); });

    QQmlContext context(m_app->rootContext());
    context.setContextProperty("mainView", m_app->rootObject());
    QQmlComponent component(m_app->engine(), QUrl("qrc:/FixtureGroupManager.qml"));
    std::unique_ptr<QObject> panel(component.create(&context));
    QVERIFY2(panel, qPrintable(component.errorString()));
    qobject_cast<QQuickItem*>(panel.get())->setParentItem(m_app->rootObject());
    QQmlExpression rootExpression(qmlContext(panel.get()), panel.get(),
            "groupListView.forceLayout(); groupListView.itemAtIndex(0).item");
    QObject *root = nullptr;
    QTRY_VERIFY2((root = rootExpression.evaluate().value<QObject*>()),
                 qPrintable(rootExpression.error().toString()));

    QQmlComponent delegate(m_app->engine(), QUrl("qrc:/" + delegateFile));
    std::unique_ptr<QObject> subject(delegate.create(&context)), other(delegate.create(&context));
    QVERIFY2(subject && other, qPrintable(delegate.errorString()));
    const int type = delegateFile == "TreeNodeDelegate.qml" ? App::FixtureGroupDragItem
            : delegateFile == "FixtureHeadDelegate.qml" ? App::HeadDragItem : App::FixtureDragItem;
    const quint32 itemID = type == App::FixtureGroupDragItem ? group.id() : fixture->id();
    for (QObject *item : {subject.get(), other.get()})
    {
        QVERIFY(item->setProperty("itemType", type));
        QVERIFY(item->setProperty("itemID", itemID));
        if (type == App::FixtureGroupDragItem)
            QVERIFY(item->setProperty("cRef", QVariant::fromValue(&group)));
        if (type == App::FixtureDragItem)
            QVERIFY(item->setProperty("cRef", QVariant::fromValue(fixture)));
        if (type == App::HeadDragItem)
            QVERIFY(item->setProperty("headIndex", item == subject.get() ? 2 : 1));
    }
    QVERIFY(subject->setProperty("isSelected", selected));
    context.setContextProperty("subject", subject.get());
    context.setContextProperty("other", other.get());
    QQmlExpression payload(qmlContext(panel.get()), panel.get(),
            present ? "gfhcDragItem.itemsList = [other, subject]"
                    : "gfhcDragItem.itemsList = [other]");
    payload.evaluate();
    QVERIFY2(!payload.hasError(), qPrintable(payload.error().toString()));

    QVERIFY(QMetaObject::invokeMethod(root, "mouseEvent",
            Q_ARG(int, int(App::Pressed)), Q_ARG(int, itemID), Q_ARG(int, type),
            Q_ARG(QVariant, QVariant::fromValue(subject.get())),
            Q_ARG(int, control ? int(Qt::ControlModifier) : 0)));

    payload.setExpression("gfhcDragItem.itemsList");
    const auto items = payload.evaluate().value<QJSValue>();
    const bool retainOther = present || control;
    QCOMPARE(items.property("length").toInt(), retainOther ? 2 : 1);
    if (retainOther)
        QCOMPARE(items.property(0).toQObject(), other.get());
    QObject *actual = items.property(retainOther ? 1 : 0).toQObject();
    QCOMPARE(actual, subject.get());
    QCOMPARE(actual->property("itemType").toInt(), type);
    QCOMPARE(actual->property("itemID").toUInt(), itemID);
    if (type == App::HeadDragItem)
        QCOMPARE(actual->property("headIndex").toInt(), 2);
    else if (type == App::FixtureGroupDragItem)
        QCOMPARE(actual->property("cRef").value<FixtureGroup*>(), &group);
    else
        QCOMPARE(actual->property("cRef").value<Fixture*>(), fixture);
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

    QQmlContext context(m_app->rootContext());
    context.setContextProperty("mainView", m_app->rootObject());
    QQmlComponent fixtures(m_app->engine(), QUrl("qrc:/FixtureGroupManager.qml"));
    std::unique_ptr<QObject> panel(fixtures.create(&context));
    QVERIFY2(panel, qPrintable(fixtures.errorString()));
    qobject_cast<QQuickItem*>(panel.get())->setParentItem(m_app->rootObject());
    QQmlExpression drag(qmlContext(panel.get()), panel.get(), QString());
    const auto cleanup = qScopeGuard([&]() {
        drag.setExpression("groupListView.dragActive = false; UISettings.internalDragActive = false");
        drag.evaluate();
    });
    for (const QString &finish : {QString("drop"), QString("cancel")})
    {
        drag.setExpression("groupListView.dragActive = true");
        drag.evaluate();
        QVERIFY2(!drag.hasError(), qPrintable(drag.error().toString()));
        QCOMPARE(overlay.evaluate().toInt(), -1);
        drag.setExpression("gfhcDragItem.Drag." + finish + "(); groupListView.dragActive = false");
        drag.evaluate();
        QVERIFY2(!drag.hasError(), qPrintable(drag.error().toString()));
        QCOMPARE(overlay.evaluate().toInt(), 100);
    }
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
    QVERIFY(!cursor->isVisible());
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

void UpstreamIntegration_Test::scriptDurationCache_data()
{
    QTest::addColumn<QString>("operation");
    QTest::addColumn<uint>("expected");
    QTest::newRow("empty") << QString("empty") << 0u;
    QTest::newRow("zero") << QString("zero") << 0u;
    QTest::newRow("same-content") << QString("same") << 1000u;
    QTest::newRow("changed-data") << QString("changed") << 2000u;
    QTest::newRow("append") << QString("append") << 3000u;
    QTest::newRow("copy") << QString("copy") << 2000u;
    QTest::newRow("xml") << QString("xml") << 3000u;
}

void UpstreamIntegration_Test::scriptDurationCache()
{
    QFETCH(QString, operation);
    QFETCH(uint, expected);
    Script script(m_app->doc());
    QVERIFY(!script.m_durationValid);
    QCOMPARE(script.totalDuration(), 0u);
    QVERIFY(script.m_durationValid);
    if (operation != "empty")
    {
        QVERIFY(script.setData("Engine.waitTime(1000);\n"));
        QVERIFY(!script.m_durationValid);
        QCOMPARE(script.totalDuration(), 1000u);
        QVERIFY(script.m_durationValid);
        if (operation == "same")
        {
            QVERIFY(!script.setData(script.data()));
            QVERIFY(script.m_durationValid);
        }
        else
        {
            if (operation == "zero" || operation == "changed")
                QVERIFY(script.setData(operation == "zero" ? "Engine.waitTime(0);\n" : "Engine.waitTime(2000);\n"));
            else if (operation == "append")
                QVERIFY(script.appendData("wait:2s"));
            else if (operation == "copy")
            {
                Script source(m_app->doc());
                QVERIFY(source.setData("Engine.waitTime(2000);\n"));
                QCOMPARE(source.totalDuration(), 2000u);
                QVERIFY(script.copyFrom(&source));
                QCOMPARE(source.data(), QString("Engine.waitTime(2000);\n"));
            }
            else
            {
                QXmlStreamReader xml("<Function ID=\"0\" Type=\"Script\" Name=\"Cache\" Version=\"2\">"
                                     "<Command>Engine.waitTime(2000);</Command></Function>");
                QVERIFY(xml.readNextStartElement());
                QVERIFY(script.loadXML(xml));
                QVERIFY(!xml.hasError());
            }
            QVERIFY(!script.m_durationValid);
        }
    }
    QCOMPARE(script.totalDuration(), expected);
    QVERIFY(script.m_durationValid);
    QCOMPARE(script.m_cachedDuration, expected);
    QCOMPARE(script.totalDuration(), expected);
}

void UpstreamIntegration_Test::externalFunctionRename_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("running");
    QTest::addColumn<bool>("clone");
    for (const QString &path : {QString(), QString("Folder/Nested")})
        for (bool running : {false, true})
            for (bool clone : {false, true})
                QTest::newRow(qPrintable(QString("%1-%2-%3").arg(path).arg(running).arg(clone)))
                        << path << running << clone;
}

void UpstreamIntegration_Test::externalFunctionRename()
{
    QFETCH(QString, path);
    QFETCH(bool, running);
    QFETCH(bool, clone);
    auto *doc = m_app->doc();
    auto *manager = m_app->rootContext()->contextProperty("functionManager").value<FunctionManager*>();
    QVERIFY(manager);
    manager->setPreviewEnabled(false);
    manager->setShowRunningOnly(false);
    manager->setSearchFilter("");
    auto *scene = new Scene(doc);
    scene->setName("Rename original");
    scene->setPath(path);
    QVERIFY(doc->addFunction(scene));
    QList<quint32> ids{scene->id()};
    Function *function = scene;
    const auto cleanup = qScopeGuard([&]() {
        if (function->isRunning())
            function->Function::postRun(doc->masterTimer(), {});
        manager->selectFunctionID(Function::invalidId(), false);
        for (quint32 id : ids)
            doc->deleteFunction(id);
    });
    if (clone)
    {
        function = scene->createCopy(doc);
        QVERIFY(function);
        ids.append(function->id());
        function->setName("Rename clone");
    }
    manager->selectFunctionID(function->id(), false);
    if (running)
        function->Function::preRun(doc->masterTimer());
    manager->updateFunctionsTree();
    manager->updateFunctionsTree();
    auto *tree = manager->functionsList().value<TreeModel*>();
    QVERIFY(tree);
    const QString prefix = path.isEmpty() ? QString() : QString(path).replace('/', TreeModel::separator()) + TreeModel::separator();
    QString previous = function->name();
    for (const QString &name : {QString("Renamed once"), QString("Renamed twice")})
    {
        function->setName(name);
        QVERIFY(!tree->itemAtPath(prefix + previous));
        auto *item = tree->itemAtPath(prefix + name);
        QVERIFY(item);
        QCOMPARE(item->data().size(), 3);
        QCOMPARE(item->data(0).value<Function*>(), function);
        QCOMPARE(item->data(1).toInt(), int(App::FunctionDragItem));
        QCOMPARE(item->data(2).toBool(), running);
        QVERIFY(item->flags() & TreeModel::Selected);
        QVERIFY(manager->selectedFunctionsID().contains(function->id()));
        manager->slotFunctionNameChanged(function->id());
        QCOMPARE(tree->itemAtPath(prefix + name), item);
        previous = name;
    }
    manager->slotFunctionNameChanged(Function::invalidId());
    QVERIFY(!tree->removeItem(prefix + "missing"));
    if (!path.isEmpty())
        QVERIFY(!tree->removeItem(prefix + "missing/deeper"));
    QVERIFY(tree->removeItem(prefix + previous));
    function->setName("Missing tree entry");
    QVERIFY(!tree->itemAtPath(prefix + function->name()));
}

void UpstreamIntegration_Test::timelineViewport_data()
{
    QTest::addColumn<int>("division");
    QTest::addColumn<QString>("state");
    for (auto division : {Show::Time, Show::BPM_4_4, Show::VDJBeat})
        for (const QString &state : {QString("playing"), QString("paused"), QString("stopped"),
                                    QString("external"), QString("held"),
                                    QString("queued-start"), QString("queued-resume")})
            QTest::newRow(qPrintable(QString("%1-%2").arg(int(division)).arg(state)))
                    << int(division) << state;
    for (const QString &state : {QString("explicit-generic"), QString("generic-explicit"),
                                QString("paused"), QString("stopped"), QString("held"),
                                QString("collapsed"), QString("inactive-paused"),
                                QString("inactive-stopped"), QString("manual")})
        QTest::newRow(qPrintable("coalesced-" + state))
                << int(Show::Time) << "coalesced-" + state;
}

void UpstreamIntegration_Test::timelineViewport()
{
    QFETCH(int, division);
    QFETCH(QString, state);
    auto *doc = m_app->doc();
    auto *manager = m_app->rootContext()->contextProperty("showManager").value<ShowManager*>();
    QVERIFY(manager);
    doc->masterTimer()->stop();
    manager->resetContents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    auto *show = new Show(doc);
    QVERIFY(doc->addFunction(show));
    auto *scene = new Scene(doc);
    scene->setDuration(40);
    QVERIFY(doc->addFunction(scene));
    auto *track = new Track(Function::invalidId(), show);
    auto *clip = new ShowFunction(show->getLatestShowFunctionId());
    clip->setFunctionID(scene->id());
    clip->setDuration(2000000000);
    QVERIFY(track->addShowFunction(clip));
    QVERIFY(show->addTrack(track));
    show->setTimeDivision(Show::TimeDivision(division), 90);
    manager->setCurrentShowID(show->id());
    manager->setTimeScale(1.0);
    manager->setCurrentTime(0);
    QQuickItem container(m_app->contentItem());
    container.setSize(QSizeF(1200, 600));
    QQmlContext context(m_app->rootContext());
    context.setContextProperty("mainView", m_app->rootObject());
    QQmlComponent component(m_app->engine(), QUrl("qrc:/ShowManager.qml"));
    std::unique_ptr<QObject> object(component.create(&context));
    auto *panel = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(panel, qPrintable(component.errorString()));
    panel->setParentItem(&container);
    const auto cleanup = qScopeGuard([&]() {
        manager->setReadOnly(false);
        if (show->isRunning())
            show->Function::postRun(doc->masterTimer(), {});
        manager->resetContents();
        object.reset();
        doc->deleteFunction(show->id());
        doc->deleteFunction(scene->id());
    });
    const auto item = [panel](const char *name) {
        QQmlExpression expression(qmlContext(panel), panel, QString::fromLatin1(name));
        return qobject_cast<QQuickItem*>(expression.evaluate().value<QObject*>());
    };
    auto *header = item("hdrItem");
    auto *timeline = item("timelineHeader");
    auto *items = item("itemsArea");
    auto *lane = item("recordingLane");
    QVERIFY(header && timeline && items && lane);
    auto *content = items->property("contentItem").value<QQuickItem*>();
    QVERIFY(content);
    QQuickItem *preview = nullptr;
    for (auto *child : content->childItems())
        if (child->property("sfRef").isValid())
            for (auto *candidate : child->childItems())
                if (candidate->property("contextType").isValid())
                    preview = candidate;
    QVERIFY(preview);
    auto *cursor = header->findChild<QQuickItem*>("showPlayhead");
    QVERIFY(cursor);
    QQmlExpression scrollbarExpression(qmlContext(panel), panel, "showContents.ScrollBar.vertical");
    auto *scrollbar = scrollbarExpression.evaluate().value<QObject*>();
    QVERIFY2(scrollbar, qPrintable(scrollbarExpression.error().toString()));
    scrollbar->setProperty("visible", false);
    const qreal unobscuredWidth = panel->property("timelineViewportWidth").toReal();
    scrollbar->setProperty("visible", true);
    QVERIFY(qAbs(unobscuredWidth - panel->property("timelineViewportWidth").toReal()
                 - scrollbar->property("width").toReal()) < 0.01);
    if (state.startsWith("queued-"))
    {
        QCoreApplication::processEvents();
        manager->setCurrentTime(1000);
        if (state == "queued-resume")
        {
            show->Function::preRun(doc->masterTimer());
            manager->playShow();
            QCoreApplication::processEvents();
            QVERIFY(manager->isPaused());
        }
        const qreal width = panel->property("timelineViewportWidth").toReal();
        QVERIFY(width > 0);
        panel->setProperty("xViewOffset", width * 2);
        lane->setProperty("pointerHeld", true);
        if (state == "queued-start")
            show->Function::preRun(doc->masterTimer());
        else
            manager->playShow();
        QCoreApplication::sendPostedEvents(manager, QEvent::MetaCall);
        QVERIFY(manager->isPlaying() && !manager->isPaused());
        QCOMPARE(panel->property("xViewOffset").toReal(), width * 2);
        lane->setProperty("pointerHeld", false);
        QCoreApplication::processEvents();
        QVERIFY2(cursor->isVisible(), "a queued generic release must not discard explicit playback reveal");
        QCOMPARE(timeline->property("contentX").toReal(), items->property("contentX").toReal());
        return;
    }
    if (state.startsWith("coalesced-"))
    {
        show->Function::preRun(doc->masterTimer());
        QCoreApplication::processEvents();
        manager->setCurrentTime(1000);
        QCoreApplication::processEvents();
        const qreal width = panel->property("timelineViewportWidth").toReal();
        QVERIFY(width > 0);
        panel->setProperty("xViewOffset", width * 2);
        if (state == "coalesced-held")
            lane->setProperty("pointerHeld", true);
        if (state == "coalesced-collapsed")
        {
            container.setWidth(10);
            QCOMPARE(panel->property("timelineViewportWidth").toReal(), 0);
        }
        if (state == "coalesced-inactive-paused")
            manager->playShow();
        if (state == "coalesced-inactive-stopped")
            show->Function::postRun(doc->masterTimer(), {});
        QCoreApplication::processEvents();
        const bool requested = state != "coalesced-manual" && !state.startsWith("coalesced-inactive-");
        const QString queue = state == "coalesced-manual"
                ? "Qt.callLater(showMgrContainer.followPlayhead)"
                : state == "coalesced-generic-explicit"
                  ? "Qt.callLater(showMgrContainer.followPlayhead); requestPlayheadReveal()"
                  : "requestPlayheadReveal(); Qt.callLater(showMgrContainer.followPlayhead)";
        QQmlExpression expression(qmlContext(panel), panel, queue);
        expression.evaluate();
        QVERIFY2(!expression.hasError(), qPrintable(expression.error().toString()));
        QCOMPARE(panel->property("playheadRevealRequested").toBool(), requested);
        if (state == "coalesced-paused")
            manager->playShow();
        if (state == "coalesced-stopped")
            show->Function::postRun(doc->masterTimer(), {});
        QCoreApplication::processEvents();
        const bool deferred = state == "coalesced-paused" || state == "coalesced-stopped"
                || state == "coalesced-held" || state == "coalesced-collapsed";
        if (deferred)
        {
            QCOMPARE(panel->property("xViewOffset").toReal(), width * 2);
            QVERIFY(panel->property("playheadRevealRequested").toBool());
            if (state == "coalesced-paused")
                manager->playShow();
            if (state == "coalesced-stopped")
                show->Function::preRun(doc->masterTimer());
            if (state == "coalesced-held")
                lane->setProperty("pointerHeld", false);
            if (state == "coalesced-collapsed")
                container.setWidth(1200);
            QCoreApplication::processEvents();
        }
        if (requested)
            QVERIFY(cursor->isVisible());
        else
            QCOMPARE(panel->property("xViewOffset").toReal(), width * 2);
        QVERIFY(!panel->property("playheadRevealRequested").toBool());
        QCOMPARE(timeline->property("contentX").toReal(), items->property("contentX").toReal());
        panel->setProperty("xViewOffset", width * 2);
        expression.setExpression("Qt.callLater(showMgrContainer.followPlayhead)");
        expression.evaluate();
        QVERIFY2(!expression.hasError(), qPrintable(expression.error().toString()));
        QCoreApplication::processEvents();
        QCOMPARE(panel->property("xViewOffset").toReal(), width * 2);
        return;
    }
    if (state != "stopped")
        show->Function::preRun(doc->masterTimer());
    if (state == "paused")
        manager->playShow();
    manager->setReadOnly(state == "external");
    lane->setProperty("pointerHeld", state == "held");
    QCoreApplication::processEvents();
    const qreal width = panel->property("timelineViewportWidth").toReal();
    QVERIFY(width > 0);
    QCOMPARE(timeline->width(), width);
    QCOMPARE(items->width(), width);
    QVERIFY(preview->width() <= width);
    QCOMPARE(clip->duration(), 2000000000u);
    panel->setProperty("xViewOffset", 0);
    const double pixelsPerMs = manager->timeBasedDivision()
            ? manager->tickSize() / (1000.0 * manager->timeScale())
            : manager->bpmNumber() * manager->tickSize() / (60000.0 * manager->beatsDivision());
    manager->setCurrentTime(int(std::ceil(width * 0.995 / pixelsPerMs)));
    QCoreApplication::processEvents();
    const bool follows = state == "playing" || state == "external";
    const qreal expected = follows ? header->property("cursorPosition").toReal() - width * 0.01 : 0;
    QVERIFY2(qAbs(panel->property("xViewOffset").toReal() - expected) < 0.5,
             qPrintable(QString("offset=%1 expected=%2 width=%3 cursor=%4 playing=%5")
                        .arg(panel->property("xViewOffset").toReal()).arg(expected).arg(width)
                        .arg(header->property("cursorPosition").toReal()).arg(manager->isPlaying())));
    QCOMPARE(timeline->property("contentX").toReal(), items->property("contentX").toReal());
    panel->setProperty("xViewOffset", width * 2);
    QVERIFY(preview->width() <= width);
    QVERIFY(preview->x() >= 0);
    manager->setCurrentTime(1000);
    QCoreApplication::processEvents();
    if (state == "external")
    {
        QVERIFY(panel->property("xViewOffset").toReal() < width * 2);
        QVERIFY(cursor->isVisible());
    }
    else
    {
        QCOMPARE(panel->property("xViewOffset").toReal(), width * 2);
        QVERIFY(!cursor->isVisible());
    }
    manager->setReadOnly(false);
    if (show->isRunning())
        show->Function::postRun(doc->masterTimer(), {});
    panel->setProperty("xViewOffset", 0);
    QVERIFY(cursor->isVisible());
    QQuickItem *canvas = nullptr;
    for (auto *child : header->childItems())
        if (child->property("contextType").isValid())
            canvas = child;
    QVERIFY(canvas);
    container.setWidth(10);
    QCoreApplication::processEvents();
    QCOMPARE(panel->property("timelineViewportWidth").toReal(), 0);
    QVERIFY(std::isfinite(canvas->x()));
    QCOMPARE(canvas->width(), 0);
    container.setWidth(1200);
    QCoreApplication::processEvents();
    const qreal restoredWidth = panel->property("timelineViewportWidth").toReal();
    QVERIFY(restoredWidth > 0);
    QCOMPARE(timeline->width(), restoredWidth);
    QCOMPARE(items->width(), restoredWidth);
    QVERIFY(std::isfinite(canvas->x()));
    QVERIFY(canvas->width() <= restoredWidth * 3);
}

void UpstreamIntegration_Test::previewRepeatBounds_data()
{
    QTest::addColumn<double>("left");
    QTest::addColumn<double>("pixelsPerUnit");
    for (double left : {0.0, 1000000.0})
        for (double scale : {0.01, 1.0, 100.0})
            QTest::newRow(qPrintable(QString("%1-%2").arg(left).arg(scale))) << left << scale;
}

void UpstreamIntegration_Test::previewRepeatBounds()
{
    QFETCH(double, left);
    QFETCH(double, pixelsPerUnit);
    QFile file(QFINDTESTDATA("../../qml/showmanager/ShowItem.qml"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString source = QString::fromUtf8(file.readAll());
    const int start = source.indexOf('{', source.indexOf("        onPaint:"));
    QVERIFY(start >= 0);
    int depth = 1;
    int end = start + 1;
    for (; end < source.size() && depth; ++end)
    {
        if (source[end] == '{')
            ++depth;
        else if (source[end] == '}')
            --depth;
    }
    QCOMPARE(depth, 0);
    QJSEngine engine;
    const QString setup = QString(R"(
        var calls = 0, marks = [];
        var context = { reset:function(){}, clearRect:function(){}, save:function(){},
            translate:function(){}, beginPath:function(){}, stroke:function(){}, restore:function(){},
            moveTo:function(x,y){ marks.push([x,y]); }, lineTo:function(){} };
        var sfRef = {duration:1000000000}, funcRef = {totalDuration:1};
        var ShowManager = {RepeatingDuration:1, FadeIn:2, StepDivider:3, FadeOut:4};
        var showManager = {previewData:function(){ return [1,1]; }};
        var prCanvas = {x:%1, width:400}, itemRoot = {height:100, width:1000000000};
        var width=400, height=100;
        function timeValueToPixels(t) {
            if (++calls > 1000) throw new Error("Offscreen or subpixel repeat budget exceeded");
            return t * %2;
        }
    )").arg(left, 0, 'f').arg(pixelsPerUnit, 0, 'f');
    QVERIFY(!engine.evaluate(setup).isError());
    const auto result = engine.evaluate("(function() " + source.mid(start, end - start) + ")()");
    QVERIFY2(!result.isError(), qPrintable(result.toString()));
    QVERIFY(engine.globalObject().property("calls").toInt() <= 1000);
    const QJSValue marks = engine.globalObject().property("marks");
    QVERIFY(marks.property("length").toInt() > 0);
    for (int i = 0; i < marks.property("length").toInt(); ++i)
    {
        const auto mark = marks.property(i);
        QVERIFY(mark.property(0).toNumber() >= left);
        QVERIFY(mark.property(0).toNumber() <= left + 400);
        QCOMPARE(mark.property(1).toNumber(), 80.0);
    }
}

void UpstreamIntegration_Test::editorPublication_data()
{
    QTest::addColumn<QString>("editor");
    QTest::addColumn<QString>("model");
    QTest::newRow("scene") << QString("sceneEditor") << QString("fixtureList");
    QTest::newRow("chaser") << QString("chaserEditor") << QString("stepsList");
    QTest::newRow("collection") << QString("collectionEditor") << QString("functionsList");
    QTest::newRow("efx") << QString("efxEditor") << QString("fixtureList");
}

void UpstreamIntegration_Test::editorPublication()
{
    QFETCH(QString, editor);
    QFETCH(QString, model);
    const QVariant previous = m_app->rootContext()->contextProperty(editor);
    const auto restore = qScopeGuard([&]() { m_app->rootContext()->setContextProperty(editor, previous); });
    const auto create = [&]() -> std::unique_ptr<FunctionEditor> {
        if (editor == "sceneEditor")
            return std::make_unique<SceneEditor>(m_app.get(), m_app->doc());
        if (editor == "chaserEditor")
            return std::make_unique<ChaserEditor>(m_app.get(), m_app->doc());
        if (editor == "collectionEditor")
            return std::make_unique<CollectionEditor>(m_app.get(), m_app->doc());
        return std::make_unique<EFXEditor>(m_app.get(), m_app->doc());
    };
    auto first = create();
    QQmlComponent component(m_app->engine());
    component.setData(QString("import QtQuick\nQtObject { property var editorModel: %1.%2 }")
                      .arg(editor, model).toUtf8(), QUrl());
    std::unique_ptr<QObject> binding(component.create());
    QVERIFY2(binding, qPrintable(component.errorString()));
    auto *firstModel = binding->property("editorModel").value<QObject*>();
    QVERIFY(firstModel);
    auto second = create();
    auto *secondModel = binding->property("editorModel").value<QObject*>();
    QVERIFY(secondModel);
    QVERIFY(secondModel != firstModel);
    QVERIFY(qobject_cast<QAbstractItemModel*>(secondModel));
}

void UpstreamIntegration_Test::colorSelectionMarker_data()
{
    QTest::addColumn<QColor>("color");
    QTest::addColumn<double>("expectedX");
    QTest::addColumn<double>("expectedY");
    QTest::newRow("black") << QColor(Qt::black) << 0.0 << 0.0;
    QTest::newRow("white") << QColor(Qt::white) << 0.0 << 255.0;
    QTest::newRow("red") << QColor(Qt::red) << 0.0 << 127.5;
    QTest::newRow("green") << QColor(Qt::green) << 84.0 << 127.5;
    QTest::newRow("blue") << QColor(Qt::blue) << 168.0 << 127.5;
}

void UpstreamIntegration_Test::colorSelectionMarker()
{
    QFETCH(QColor, color);
    QFETCH(double, expectedX);
    QFETCH(double, expectedY);
    QQmlComponent component(m_app->engine(), QUrl("qrc:/ColorToolFull.qml"));
    std::unique_ptr<QObject> full(component.create());
    QVERIFY2(full, qPrintable(component.errorString()));
    QSignalSpy changed(full.get(), SIGNAL(toolColorChanged(double,double,double,double,double,double)));
    QVERIFY(full->setProperty("currentRGB", color));
    QVERIFY(qAbs(full->property("selectedColorX").toDouble() - expectedX) < 0.01);
    QVERIFY(qAbs(full->property("selectedColorY").toDouble() - expectedY) < 0.01);
    QCOMPARE(changed.count(), 0);
    QQmlComponent basicComponent(m_app->engine(), QUrl("qrc:/ColorToolBasic.qml"));
    std::unique_ptr<QObject> basic(basicComponent.create());
    QVERIFY2(basic, qPrintable(basicComponent.errorString()));
    basic->setProperty("currentRGB", color);
    QQmlExpression matches(qmlContext(basic.get()), basic.get(), "colorsMatch(currentRGB, currentRGB)");
    QVERIFY(matches.evaluate().toBool());
    QVERIFY(!matches.hasError());
    QQmlExpression outline(qmlContext(basic.get()), basic.get(), "selectionBorderColor(currentRGB)");
    const QString expected = color == Qt::white || color == Qt::green ? "black" : "white";
    QCOMPARE(outline.evaluate().toString(), expected);
}

void UpstreamIntegration_Test::identicalWidgetFont()
{
    VCButton widget(m_app->doc());
    QSignalSpy changed(&widget, &VCWidget::fontChanged);
    QFont font("Sans Serif", 14);
    widget.setFont(font);
    QCOMPARE(changed.count(), 1);
    widget.setFont(font);
    QCOMPARE(changed.count(), 1);
    font.setPointSize(18);
    widget.setFont(font);
    QCOMPARE(changed.count(), 2);
    QCOMPARE(widget.font(), font);
}

void UpstreamIntegration_Test::selectedFixtureColors_data()
{
    QTest::addColumn<bool>("rgb");
    QTest::addColumn<bool>("wauv");
    QTest::addColumn<bool>("mixed");
    for (bool rgb : {false, true})
        for (bool wauv : {false, true})
            for (bool mixed : {false, true})
                QTest::newRow(qPrintable(QString("%1-%2-%3").arg(rgb).arg(wauv).arg(mixed)))
                        << rgb << wauv << mixed;
}

void UpstreamIntegration_Test::selectedFixtureColors()
{
    QFETCH(bool, rgb);
    QFETCH(bool, wauv);
    QFETCH(bool, mixed);
    auto *doc = m_app->doc();
    auto *context = m_app->rootContext()->contextProperty("contextManager").value<ContextManager*>();
    QVERIFY(context);
    context->resetFixtureSelection();
    QLCFixtureDef definition;
    auto *mode = new QLCFixtureMode(&definition);
    QVERIFY(definition.addMode(mode));
    QList<QLCChannel::PrimaryColour> colors;
    if (rgb)
        colors << QLCChannel::Red << QLCChannel::Green << QLCChannel::Blue;
    if (wauv)
        colors << QLCChannel::White << QLCChannel::Amber << QLCChannel::UV;
    if (colors.isEmpty())
        colors << QLCChannel::NoColour;
    for (const auto color : colors)
    {
        auto *channel = new QLCChannel;
        channel->setName(QString::number(int(color)));
        channel->setGroup(QLCChannel::Intensity);
        channel->setColour(color);
        QVERIFY(definition.addChannel(channel));
        QVERIFY(mode->insertChannel(channel, definition.channels().size() - 1));
    }
    QList<quint32> ids;
    const auto cleanup = qScopeGuard([&]() {
        context->resetFixtureSelection();
        for (quint32 id : ids)
            doc->deleteFixture(id);
    });
    for (int i = 0; i < 2; ++i)
    {
        auto *fixture = new Fixture(doc);
        fixture->setFixtureDefinition(&definition, mode);
        fixture->setAddress(i * colors.size());
        QVERIFY(doc->addFixture(fixture));
        ids.append(fixture->id());
        fixture->setChannelValues(QByteArray(512, char(mixed && i ? 128 : 64)));
        context->setFixtureSelection(FixtureUtils::fixtureItemID(fixture->id(), 0, 0), -1, true);
    }
    QQmlComponent component(m_app->engine());
    component.setData(R"(import QtQuick
        Item {
            property bool rgbValid: false
            property bool wauvValid: false
            property color rgbColor
            property color wauvColor
            function updateColors(rgb, color, wauv, white) {
                rgbValid=rgb; rgbColor=color; wauvValid=wauv; wauvColor=white
            }
        })", QUrl());
    std::unique_ptr<QObject> receiver(component.create());
    QVERIFY2(receiver, qPrintable(component.errorString()));
    context->getCurrentColors(qobject_cast<QQuickItem*>(receiver.get()));
    QCOMPARE(receiver->property("rgbValid").toBool(), rgb && !mixed);
    QCOMPARE(receiver->property("wauvValid").toBool(), wauv && !mixed);
    if (rgb && !mixed)
        QCOMPARE(receiver->property("rgbColor").value<QColor>(), QColor(64, 64, 64));
    if (wauv && !mixed)
        QCOMPARE(receiver->property("wauvColor").value<QColor>(), QColor(64, 64, 64));
}

void UpstreamIntegration_Test::colorPressOwnership()
{
    const QRect previousGeometry = m_app->geometry();
    const QWindow::Visibility previousVisibility = m_app->visibility();
    const auto restore = qScopeGuard([&]() {
        m_app->setVisibility(previousVisibility);
        m_app->setGeometry(previousGeometry);
        QCoreApplication::processEvents();
    });
    QTest::failOnWarning(QRegularExpression(".*outside target window.*"));
    QTest::failOnWarning(QRegularExpression(".*ColorToolFull.qml.*(TypeError|ReferenceError).*"));
    m_app->showNormal();
    m_app->resize(1200, 800);
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    QTRY_COMPARE(m_app->size(), QSize(1200, 800));
    QQmlContext context(m_app->rootContext());
    context.setContextProperty("mainView", m_app->rootObject());
    QQmlComponent component(m_app->engine());
    component.setData(R"(import QtQuick
        import "."
        Flickable {
            width:700; height:500; contentWidth:1500; contentHeight:1200
            ColorTool {
                objectName:"picker"; x:150; y:100; width:330; showPalette:false
                colorToolQML:"qrc:/ColorToolFull.qml"
            }
        })", QUrl("qrc:/ColorOwnershipTest.qml"));
    std::unique_ptr<QObject> object(component.create(&context));
    auto *flick = qobject_cast<QQuickItem*>(object.get());
    QVERIFY2(flick, qPrintable(component.errorString()));
    flick->setParentItem(m_app->contentItem());
    flick->setZ(10000);
    auto *tool = flick->findChild<QQuickItem*>("picker");
    QVERIFY(tool);
    QQuickItem *full = nullptr;
    for (auto *child : tool->findChildren<QQuickItem*>())
        if (child->property("selectedColorX").isValid())
            full = child;
    QVERIFY(full);
    QQuickItem *canvas = nullptr;
    for (auto *child : full->childItems())
        if (child->property("contextType").isValid())
            canvas = child;
    QVERIFY(canvas);
    QTRY_VERIFY(canvas->property("available").toBool());
    QSignalSpy painted(canvas, SIGNAL(painted()));
    QVERIFY(painted.isValid());
    QVERIFY(QMetaObject::invokeMethod(canvas, "requestPaint"));
    QTRY_VERIFY(painted.count() > 0);
    QQmlExpression ready(qmlContext(canvas), canvas,
                        "context !== undefined && context !== null && typeof context.getImageData === 'function'");
    QTRY_VERIFY(ready.evaluate().toBool());
    QVERIFY2(!ready.hasError(), qPrintable(ready.error().toString()));
    QSignalSpy changed(tool, SIGNAL(toolColorChanged(double,double,double,double,double,double)));
    QSignalSpy released(full, SIGNAL(released()));
    const QPoint press = canvas->mapToScene(QPointF(100, 100)).toPoint();
    const QPoint outside = canvas->mapToScene(QPointF(330, 330)).toPoint();
    const QRect windowRect(QPoint(), m_app->size());
    QVERIFY(windowRect.contains(press));
    QVERIFY(windowRect.contains(outside));
    QVERIFY(canvas->contains(canvas->mapFromScene(press)));
    QVERIFY(!canvas->contains(canvas->mapFromScene(outside)));
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
    QTRY_VERIFY(!changed.isEmpty());
    const auto picked = changed.constLast();
    QVERIFY(picked.at(0).toDouble() >= 0 && picked.at(0).toDouble() < 0.25);
    QVERIFY(picked.at(1).toDouble() > 0.5 && picked.at(1).toDouble() <= 1);
    QVERIFY(picked.at(2).toDouble() > 0.7 && picked.at(2).toDouble() <= 1);
    const QColor displayed = tool->property("currentRGB").value<QColor>();
    QVERIFY(qAbs(displayed.redF() - picked.at(0).toDouble()) < 1.0 / 255);
    QVERIFY(qAbs(displayed.greenF() - picked.at(1).toDouble()) < 1.0 / 255);
    QVERIFY(qAbs(displayed.blueF() - picked.at(2).toDouble()) < 1.0 / 255);
    QTest::mouseMove(m_app.get(), press + QPoint(40, 40));
    QTest::mouseMove(m_app.get(), outside);
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, outside);
    QTRY_COMPARE(released.count(), 1);
    QVERIFY(changed.count() >= 2);
    QCOMPARE(flick->property("contentX").toReal(), 0.0);
    QCOMPARE(flick->property("contentY").toReal(), 0.0);
    QWheelEvent wheel(press, m_app->mapToGlobal(press), QPoint(), QPoint(0, -120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(m_app.get(), &wheel);
    QCoreApplication::processEvents();
    QCOMPARE(flick->property("contentY").toReal(), 0.0);
}

void UpstreamIntegration_Test::liveFontDialog_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<int>("selected");
    QTest::newRow("rgb-text") << QString("../../qml/fixturesfunctions/RGBMatrixEditor.qml") << 1;
    QTest::newRow("vc-single") << QString("../../qml/virtualconsole/VCWidgetProperties.qml") << 1;
    QTest::newRow("vc-multiple") << QString("../../qml/virtualconsole/VCWidgetProperties.qml") << 2;
}

void UpstreamIntegration_Test::liveFontDialog()
{
    QFETCH(QString, fileName);
    QFETCH(int, selected);
    QFile file(QFINDTESTDATA(qPrintable(fileName)));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString source = QString::fromUtf8(file.readAll());
    const int dialogStart = source.indexOf("FontDialog\n");
    QVERIFY(dialogStart >= 0);
    const auto block = [&](int start) {
        const int begin = source.indexOf('{', start);
        int depth = 1, end = begin + 1;
        for (; end < source.size() && depth; ++end)
        {
            if (source[end] == '{')
                ++depth;
            else if (source[end] == '}')
                --depth;
        }
        return source.mid(begin + 1, end - begin - 2);
    };
    const QString dialog = block(dialogStart);
    const QString open = block(source.lastIndexOf("onClicked:", dialogStart));
    QQmlComponent component(m_app->engine());
    component.setData(QString(R"(
        import QtQuick
        QtObject {
            id: host
            property int updates: 0
            property int batchUpdates: 0
            property int selectedWidgetsCount: %1
            property QtObject wObj: QtObject {
                property font font: Qt.font({family:"Sans Serif", pointSize:10})
                onFontChanged: host.updates++
            }
            property QtObject virtualConsole: QtObject {
                function setWidgetsFont(font) { host.batchUpdates++; host.wObj.font=font }
            }
            property QtObject rgbMatrixEditor: QtObject {
                property font algoTextFont: Qt.font({family:"Sans Serif", pointSize:10})
                onAlgoTextFontChanged: host.updates++
            }
            property QtObject dialog: QtObject {
                property font selectedFont
                property bool visible
                property string title
                signal accepted()
                %2
            }
            function open() { %3 }
        }
    )").arg(selected).arg(dialog, open).toUtf8(), QUrl());
    std::unique_ptr<QObject> host(component.create());
    QVERIFY2(host, qPrintable(component.errorString()));
    auto *dialogObject = host->property("dialog").value<QObject*>();
    QVERIFY(dialogObject);
    host->setProperty("updates", 0);
    QVERIFY(dialogObject->setProperty("selectedFont", QFont("Serif", 12)));
    QCOMPARE(host->property("updates").toInt(), 0);
    QVERIFY(QMetaObject::invokeMethod(host.get(), "open"));
    QCOMPARE(dialogObject->property("selectedFont").value<QFont>().pointSize(), 10);
    QCOMPARE(host->property("updates").toInt(), 0);
    QVERIFY(dialogObject->setProperty("selectedFont", QFont("Serif", 18)));
    QCOMPARE(host->property("updates").toInt(), 1);
    QVERIFY(QMetaObject::invokeMethod(dialogObject, "accepted"));
    QCOMPARE(host->property("updates").toInt(), 1);
    if (selected > 1)
        QCOMPARE(host->property("batchUpdates").toInt(), 2);
    dialogObject->setProperty("visible", false);
    dialogObject->setProperty("selectedFont", QFont("Serif", 20));
    QCOMPARE(host->property("updates").toInt(), 1);
    QVERIFY(QMetaObject::invokeMethod(host.get(), "open"));
    QCOMPARE(dialogObject->property("selectedFont").value<QFont>().pointSize(), 18);
    QCOMPARE(host->property("updates").toInt(), 1);
}

void UpstreamIntegration_Test::vcClipboardAtomicity_data()
{
    QTest::addColumn<bool>("cut");
    QTest::newRow("copy-invalid-second-root") << false;
    QTest::newRow("cut-invalid-second-root") << true;
}

void UpstreamIntegration_Test::vcClipboardAtomicity()
{
    QFETCH(bool, cut);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldEdit = vc->editMode();
    const bool oldSnap = vc->snapping();
    const bool wasModified = m_app->doc()->isModified();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    const QVariantList oldClipboard = vc->clipboardItemsList();
    const bool oldClipboardCut = vc->clipboardIsCut();
    vc->setEditMode(true);
    vc->setSnapping(false);
    auto *source = new VCFrame(m_app->doc(), vc, vc->page(0));
    auto *target = new VCFrame(m_app->doc(), vc, vc->page(0));
    for (VCFrame *frame : {source, target})
    {
        vc->addWidgetToMap(frame);
        vc->page(0)->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setLogicalGeometry(frame == source ? QRectF(0, 0, 600, 400)
                                                  : QRectF(900, 0, 250, 200));
    }
    const quint32 sourceID = source->id(), targetID = target->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({sourceID, targetID});
        vc->setSelectedWidgetIDs(oldClipboard);
        if (oldClipboardCut)
            vc->cutToClipboard();
        else
            vc->copyToClipboard();
        vc->setSelectedWidgetIDs(oldSelection);
        vc->setSnapping(oldSnap);
        vc->setEditMode(oldEdit);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    QVariantList ids;
    for (const QRectF &rect : {QRectF(10, 10, 100, 80), QRectF(120, 10, 350, 80)})
    {
        auto *button = new VCButton(m_app->doc(), source);
        vc->addWidgetToMap(button);
        source->addWidgetToPageMap(button);
        button->setLogicalGeometry(rect);
        ids.append(button->id());
    }
    vc->setSelectedWidgetIDs(ids);
    if (cut)
        vc->cutToClipboard();
    else
        vc->copyToClipboard();
    QCOMPARE(vc->clipboardItemsList(), ids);
    const QVariantList sourceOrder = source->orderedChildIDs(0);
    const QVariantList targetOrder = target->orderedChildIDs(0);
    QSignalSpy structure(vc, &VirtualConsole::structureChanged);
    QSignalSpy rejected(vc, &VirtualConsole::editRejected);
    m_app->doc()->resetModified();

    target->addWidgetsFromClipboard(nullptr, vc->clipboardItemsList(), QPoint(20, 20));

    QCOMPARE(target->orderedChildIDs(0), targetOrder);
    QCOMPARE(source->orderedChildIDs(0), sourceOrder);
    for (const QVariant &id : ids)
    {
        QVERIFY(vc->widget(id.toUInt()));
        QCOMPARE(qobject_cast<VCFrame *>(vc->widget(id.toUInt())->parent()), source);
    }
    QCOMPARE(vc->clipboardItemsList(), ids);
    QCOMPARE(vc->clipboardIsCut(), cut);
    QCOMPARE(vc->selectedWidgetIDs(), ids);
    QCOMPARE(structure.count(), 0);
    QCOMPARE(rejected.count(), 1);
    QVERIFY(!m_app->doc()->isModified());
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcReparentInputMaps_data()
{
    QTest::addColumn<bool>("crossRootPage");
    QTest::addColumn<bool>("keyboard");
    QTest::newRow("controller-same-root") << false << false;
    QTest::newRow("controller-cross-root") << true << false;
    QTest::newRow("keyboard-same-root") << false << true;
    QTest::newRow("keyboard-cross-root") << true << true;
}

void UpstreamIntegration_Test::vcReparentInputMaps()
{
    QFETCH(bool, crossRootPage);
    QFETCH(bool, keyboard);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldEdit = vc->editMode();
    const bool oldSnap = vc->snapping();
    const bool wasModified = m_app->doc()->isModified();
    const bool pageWasVisible = vc->page(0)->isVisible();
    const int oldPage = vc->selectedPage();
    const int extraPageIndex = vc->pagesCount();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    vc->setEditMode(true);
    vc->setSnapping(false);
    vc->resetWidgetSelection();
    if (crossRootPage)
        vc->addPage(extraPageIndex);
    VCPage *sourcePage = vc->page(0);
    VCPage *targetPage = vc->page(crossRootPage ? extraPageIndex : 0);
    sourcePage->setVisible(true);
    targetPage->setVisible(true);
    auto *source = new VCFrame(m_app->doc(), vc, sourcePage);
    auto *target = new VCFrame(m_app->doc(), vc, targetPage);
    for (VCFrame *frame : {source, target})
    {
        vc->addWidgetToMap(frame);
        qobject_cast<VCFrame *>(frame->parent())->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setLogicalGeometry(QRectF(frame == source ? 0 : 900, 0, 600, 400));
    }
    target->setMultiPageMode(true);
    target->setTotalPagesNumber(2);
    auto *nested = new VCFrame(m_app->doc(), vc, source);
    vc->addWidgetToMap(nested);
    source->addWidgetToPageMap(nested);
    nested->setShowHeader(false);
    nested->setLogicalGeometry(QRectF(20, 20, 300, 250));
    auto *slider = new VCSlider(m_app->doc(), nested);
    vc->addWidgetToMap(slider);
    nested->addWidgetToPageMap(slider);
    slider->setLogicalGeometry(QRectF(20, 20, 140, 200));
    slider->setSliderMode(VCSlider::Level);
    slider->setCatchValues(false);
    slider->setRangeLowLimit(0);
    slider->setRangeHighLimit(255);
    auto *scene = new Scene(m_app->doc());
    QVERIFY(m_app->doc()->addFunction(scene));
    const quint32 functionID = scene->id();
    const quint32 sourceID = source->id(), targetID = target->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({sourceID, targetID});
        vc->setSelectedPage(oldPage);
        if (crossRootPage)
            vc->deletePage(extraPageIndex);
        sourcePage->setVisible(pageWasVisible);
        m_app->doc()->deleteFunction(functionID);
        vc->setSnapping(oldSnap);
        vc->setEditMode(oldEdit);
        vc->setSelectedWidgetIDs(oldSelection);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    slider->setControlledFunction(functionID);
    const QUuid recordingID = slider->ensureRecordingId();
    const quint32 sliderID = slider->id();
    const quint32 universe = 0, channel = 497;
    const quint32 inputKey = 497;
    const QKeySequence key(Qt::CTRL | Qt::Key_F11);
    vc->createAndAddInputSource(slider, universe, channel);
    slider->addKeySequence(key, 0);
    sourcePage->buildKeySequenceMap();
    const auto input = slider->inputSource(0, universe, channel);
    QVERIFY(input);
    QCOMPARE(input->id(), quint32(0));
    QCOMPARE(input->page(), ushort(0));
    const auto dispatch = [&](VCPage *page, int value) {
        if (keyboard)
        {
            QKeySequence sequence = key;
            page->handleKeyEvent(sequence, value != 0);
        }
        else
            page->inputValueChanged(inputKey, uchar(value));
    };
    vc->setEditMode(false);
    slider->setValue(0);
    dispatch(sourcePage, keyboard ? 255 : 64);
    QCOMPARE(slider->value(), keyboard ? 255 : 64);
    dispatch(sourcePage, 0);
    QCOMPARE(slider->value(), 0);
    vc->setEditMode(true);
    QSignalSpy started(slider, &VCWidget::functionStarting);

    const QVariantMap result = vc->editStructure({sliderID}, targetID, 1);

    QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
    QCOMPARE(qobject_cast<VCFrame *>(slider->parent()), target);
    QCOMPARE(slider->page(), 1);
    QCOMPARE(input->page(), ushort(1));
    QCOMPARE(input->id(), quint32(0));
    QCOMPARE(input->universe(), universe);
    QCOMPARE(input->channel() & quint32(0xffff), channel);
    QCOMPARE(slider->keySequenceMap().value(key), quint32(0));
    QCOMPARE(slider->id(), sliderID);
    QCOMPARE(slider->recordingId(), recordingID);
    QCOMPARE(slider->controlledFunction(), functionID);
    QCOMPARE(started.count(), 0);
    QVERIFY(scene->stopped());
    QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(scene));
    if (crossRootPage)
    {
        QVERIFY(!sourcePage->hasInputSourceForKey(inputKey));
        QVERIFY(!sourcePage->hasKeySequence(key));
    }
    QVERIFY(targetPage->hasInputSourceForKey(inputKey));
    QVERIFY(targetPage->hasKeySequence(key));
    vc->setEditMode(false);
    dispatch(targetPage, keyboard ? 255 : 128);
    QCOMPARE(slider->value(), 0);
    target->setCurrentPage(1);
    dispatch(targetPage, keyboard ? 255 : 128);
    QCOMPARE(slider->value(), keyboard ? 255 : 128);
    dispatch(targetPage, 0);
    QCOMPARE(slider->value(), 0);
    if (crossRootPage)
    {
        dispatch(sourcePage, keyboard ? 255 : 200);
        QCOMPARE(slider->value(), 0);
    }
    target->setCurrentPage(0);
    dispatch(targetPage, keyboard ? 255 : 200);
    QCOMPARE(slider->value(), 0);
    QCOMPARE(started.count(), 0);
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcNestedSoloRouting_data()
{
    QTest::addColumn<bool>("intoSolo");
    QTest::newRow("normal-to-solo") << true;
    QTest::newRow("solo-to-normal") << false;
}

void UpstreamIntegration_Test::vcNestedSoloRouting()
{
    QFETCH(bool, intoSolo);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldEdit = vc->editMode();
    const bool oldSnap = vc->snapping();
    const bool wasModified = m_app->doc()->isModified();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    vc->setEditMode(true);
    vc->setSnapping(false);
    vc->resetWidgetSelection();
    auto *normal = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(normal);
    vc->page(0)->addWidgetToPageMap(normal);
    normal->setShowHeader(false);
    normal->setLogicalGeometry(QRectF(0, 0, 600, 500));
    const QVariantMap created = vc->createWidgetGeometry(vc->page(0)->id(), "Solo frame",
                                                         QRectF(900, 0, 600, 500));
    QVERIFY2(created.value("accepted").toBool(), qPrintable(created.value("error").toString()));
    auto *solo = qobject_cast<VCFrame *>(vc->widget(created.value("widgetIDs").toList().first().toUInt()));
    QVERIFY(solo);
    solo->setShowHeader(false);
    const quint32 normalID = normal->id(), soloID = solo->id();
    QList<quint32> functionIDs;
    const auto cleanup = qScopeGuard([&]() {
        for (quint32 id : functionIDs)
            m_app->doc()->function(id)->stop(FunctionParent::master());
        m_app->doc()->masterTimer()->stop();
        vc->deleteVCWidgets({normalID, soloID});
        for (quint32 id : functionIDs)
            m_app->doc()->deleteFunction(id);
        vc->setSnapping(oldSnap);
        vc->setEditMode(oldEdit);
        vc->setSelectedWidgetIDs(oldSelection);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    VCFrame *initialParent = intoSolo ? normal : solo;
    auto *outer = new VCFrame(m_app->doc(), vc, initialParent);
    vc->addWidgetToMap(outer);
    initialParent->addWidgetToPageMap(outer);
    outer->setShowHeader(false);
    outer->setLogicalGeometry(QRectF(20, 20, 300, 250));
    auto *inner = new VCFrame(m_app->doc(), vc, outer);
    vc->addWidgetToMap(inner);
    outer->addWidgetToPageMap(inner);
    inner->setShowHeader(false);
    inner->setLogicalGeometry(QRectF(10, 10, 250, 200));
    auto *nestedButton = new VCButton(m_app->doc(), inner);
    auto *soloButton = new VCButton(m_app->doc(), solo);
    for (VCButton *button : {nestedButton, soloButton})
    {
        auto *parent = qobject_cast<VCFrame *>(button->parent());
        vc->addWidgetToMap(button);
        parent->addWidgetToPageMap(button);
        button->setLogicalGeometry(QRectF(button == nestedButton ? 10 : 400, 20, 100, 80));
        auto *scene = new Scene(m_app->doc());
        QVERIFY(m_app->doc()->addFunction(scene));
        functionIDs.append(scene->id());
        button->setFunctionID(scene->id());
        button->setActionType(VCButton::Toggle);
    }
    const QUuid recordingID = nestedButton->ensureRecordingId();
    QSignalSpy nestedStarts(nestedButton, &VCWidget::functionStarting);
    QSignalSpy outerStarts(outer, &VCWidget::functionStarting);
    QSignalSpy innerStarts(inner, &VCWidget::functionStarting);
    QSignalSpy normalStarts(normal, &VCWidget::functionStarting);
    QSignalSpy soloStarts(solo, &VCWidget::functionStarting);

    const QVariantMap result = vc->editStructure({outer->id()}, intoSolo ? soloID : normalID, 0);

    QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
    QCOMPARE(qobject_cast<VCFrame *>(outer->parent()), intoSolo ? solo : normal);
    QCOMPARE(nestedButton->hasSoloParent(), intoSolo);
    QCOMPARE(nestedButton->functionID(), functionIDs[0]);
    QCOMPARE(nestedButton->recordingId(), recordingID);
    QCOMPARE(nestedStarts.count(), 0);
    QCOMPARE(outerStarts.count(), 0);
    QCOMPARE(innerStarts.count(), 0);
    QCOMPARE(m_app->runningFunctionsCount(), 0);
    for (quint32 id : functionIDs)
    {
        QVERIFY(m_app->doc()->function(id)->stopped());
        QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(m_app->doc()->function(id)));
    }
    vc->setEditMode(false);
    soloButton->requestStateChange(true);
    QVERIFY(!m_app->doc()->function(functionIDs[1])->stopped());
    QVERIFY(m_app->doc()->masterTimer()->isStartQueued(m_app->doc()->function(functionIDs[1])));
    nestedButton->requestStateChange(true);
    QCOMPARE(nestedStarts.count(), 1);
    QCOMPARE(innerStarts.count(), intoSolo ? 1 : 0);
    QCOMPARE(outerStarts.count(), intoSolo ? 1 : 0);
    QCOMPARE(normalStarts.count(), 0);
    QCOMPARE(soloStarts.count(), 0);
    QVERIFY(!m_app->doc()->function(functionIDs[0])->stopped());
    QVERIFY(m_app->doc()->masterTimer()->isStartQueued(m_app->doc()->function(functionIDs[0])));
    QCOMPARE(m_app->doc()->function(functionIDs[1])->stopped(), intoSolo);
}

void UpstreamIntegration_Test::vcClipboardPublication_data()
{
    QTest::addColumn<bool>("cut");
    QTest::newRow("copy-complete-subtree") << false;
    QTest::newRow("cut-complete-subtree") << true;
}

void UpstreamIntegration_Test::vcClipboardPublication()
{
    QFETCH(bool, cut);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldEdit = vc->editMode();
    const bool oldSnap = vc->snapping();
    const bool wasModified = m_app->doc()->isModified();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    const QVariantList oldClipboard = vc->clipboardItemsList();
    const bool oldClipboardCut = vc->clipboardIsCut();
    vc->setEditMode(true);
    vc->setSnapping(false);
    auto *source = new VCFrame(m_app->doc(), vc, vc->page(0));
    auto *target = new VCFrame(m_app->doc(), vc, vc->page(0));
    for (VCFrame *frame : {source, target})
    {
        vc->addWidgetToMap(frame);
        vc->page(0)->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setLogicalGeometry(QRectF(frame == source ? 0 : 900, 0, 700, 500));
    }
    const quint32 sourceID = source->id(), targetID = target->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({sourceID, targetID});
        vc->setSelectedWidgetIDs(oldClipboard);
        if (oldClipboardCut)
            vc->cutToClipboard();
        else
            vc->copyToClipboard();
        vc->setSelectedWidgetIDs(oldSelection);
        vc->setSnapping(oldSnap);
        vc->setEditMode(oldEdit);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    auto *frame = new VCFrame(m_app->doc(), vc, source);
    vc->addWidgetToMap(frame);
    source->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(10, 10, 250, 220));
    auto *child = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(child);
    frame->addWidgetToPageMap(child);
    child->setLogicalGeometry(QRectF(10, 10, 100, 80));
    auto *button = new VCButton(m_app->doc(), source);
    vc->addWidgetToMap(button);
    source->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(300, 10, 100, 80));
    const QVariantList ids{frame->id(), button->id()};
    vc->setSelectedWidgetIDs(ids);
    if (cut)
        vc->cutToClipboard();
    else
        vc->copyToClipboard();
    QSignalSpy widgets(vc, &VirtualConsole::widgetsChanged);
    QSignalSpy structure(vc, &VirtualConsole::structureChanged);
    bool coherent = true;
    const auto connection = connect(vc, &VirtualConsole::widgetsChanged, this, [&]() {
        coherent = coherent && target->children(false).size() == 2 &&
            target->children(true).size() == 3 && source->children(false).size() == (cut ? 0 : 2);
        for (VCWidget *entry : target->children(true))
            coherent = coherent && vc->widget(entry->id()) == entry;
    });

    target->addWidgetsFromClipboard(nullptr, vc->clipboardItemsList(), QPoint(20, 20));

    disconnect(connection);
    QCOMPARE(target->children(false).size(), 2);
    QCOMPARE(target->children(true).size(), 3);
    QCOMPARE(source->children(false).size(), cut ? 0 : 2);
    QVERIFY(coherent);
    QCOMPARE(widgets.count(), 1);
    QCOMPARE(structure.count(), 1);
    QCOMPARE(target->children(false)[0]->logicalGeometry(), QRectF(20, 20, 250, 220));
    QCOMPARE(target->children(false)[1]->logicalGeometry(), QRectF(270, 20, 100, 80));
    QVERIFY(m_app->doc()->isModified());
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcKeyboardPasteOrigin_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::newRow("zoom-out") << qreal(0.75);
    QTest::newRow("fractional-zoom") << qreal(1.1);
    QTest::newRow("zoom-in") << qreal(2);
}

void UpstreamIntegration_Test::vcKeyboardPasteOrigin()
{
    QFETCH(qreal, scale);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldEdit = vc->editMode();
    const bool oldSnap = vc->snapping();
    const bool wasModified = m_app->doc()->isModified();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    const QVariantList oldClipboard = vc->clipboardItemsList();
    const bool oldClipboardCut = vc->clipboardIsCut();
    vc->setEditMode(true);
    vc->setSnapping(false);
    auto *source = new VCFrame(m_app->doc(), vc, vc->page(0));
    auto *target = new VCFrame(m_app->doc(), vc, vc->page(0));
    for (VCFrame *frame : {source, target})
    {
        vc->addWidgetToMap(frame);
        vc->page(0)->addWidgetToPageMap(frame);
        frame->setLogicalGeometry(QRectF(frame == source ? 0 : 900, 0, 600, 500));
    }
    source->setShowHeader(false);
    target->setMultiPageMode(true);
    target->setTotalPagesNumber(2);
    target->setCurrentPage(1);
    target->setScaleFactor(scale);
    const quint32 sourceID = source->id(), targetID = target->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({sourceID, targetID});
        vc->setSelectedWidgetIDs(oldClipboard);
        if (oldClipboardCut)
            vc->cutToClipboard();
        else
            vc->copyToClipboard();
        vc->setSelectedWidgetIDs(oldSelection);
        vc->setSnapping(oldSnap);
        vc->setEditMode(oldEdit);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    auto *button = new VCButton(m_app->doc(), source);
    vc->addWidgetToMap(button);
    source->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(10, 10, 140.25, 100.5));
    vc->setSelectedWidgetIDs({button->id()});
    vc->copyToClipboard();
    vc->setSelectedWidgetIDs({targetID});
    const QPointF origin = target->contentRect().topLeft();
    QVERIFY(origin.y() > 0);
    QSignalSpy rejected(vc, &VirtualConsole::editRejected);

    vc->pasteFromClipboard();

    QCOMPARE(target->children(false).size(), 1);
    VCWidget *copy = target->children(false).first();
    QCOMPARE(copy->logicalGeometry(), QRectF(origin, QSizeF(140.25, 100.5)));
    QCOMPARE(copy->page(), 1);
    QCOMPARE(copy->scaleFactor(), scale);
    QCOMPARE(rejected.count(), 0);
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcLegacyPageColumnOrder_data()
{
    QTest::addColumn<bool>("storedOrder");
    QTest::newRow("legacy-disjoint-columns") << false;
    QTest::newRow("stored-order-remains-authoritative") << true;
}

void UpstreamIntegration_Test::vcLegacyPageColumnOrder()
{
    QFETCH(bool, storedOrder);
    VirtualConsole *vc = m_app->virtualConsole();
    const int pageIndex = vc->pagesCount();
    const int oldPage = vc->selectedPage();
    const bool wasModified = m_app->doc()->isModified();
    vc->addPage(pageIndex);
    VCPage *page = vc->page(pageIndex);
    const auto cleanup = qScopeGuard([&]() {
        vc->setSelectedPage(oldPage);
        vc->deletePage(pageIndex);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    const QList<QRectF> rectangles{QRectF(300, 320, 100, 80), QRectF(20, 300, 100, 80),
                                   QRectF(300, 30, 100, 80), QRectF(20, 20, 100, 80)};
    QVariantList ids;
    for (const QRectF &rect : rectangles)
    {
        auto *button = new VCButton(m_app->doc(), page);
        vc->addWidgetToMap(button);
        page->addWidgetToPageMap(button);
        button->setLogicalGeometry(rect);
        ids.append(button->id());
    }
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    QVERIFY(page->saveXML(&writer));
    if (!storedOrder)
        xml.replace(" ChildOrder=\"1\"", "");
    vc->deleteVCWidgets(ids);
    QVERIFY(page->children(false).isEmpty());
    QBuffer input(&xml);
    QVERIFY(input.open(QIODevice::ReadOnly));
    QXmlStreamReader reader(&input);
    QVERIFY(reader.readNextStartElement());

    QVERIFY(page->loadXML(reader));

    QCOMPARE(page->orderedChildIDs(0), storedOrder ? ids : QVariantList({ids[3], ids[1], ids[2], ids[0]}));
    for (int i = 0; i < ids.size(); ++i)
    {
        VCWidget *button = vc->widget(ids[i].toUInt());
        QVERIFY(button);
        QCOMPARE(button->logicalGeometry(), rectangles[i]);
        QCOMPARE(qobject_cast<VCPage *>(button->parent()), page);
    }
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcGeometryArithmetic_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("fractional-exact-fit") << QString("fit") << true;
    QTest::newRow("fractional-neighbor-touch") << QString("touch") << true;
    QTest::newRow("real-overlap-1e-8") << QString("overlap") << false;
    QTest::newRow("real-child-clip-1e-8") << QString("clip") << false;
}

void UpstreamIntegration_Test::vcGeometryArithmetic()
{
    QFETCH(QString, kind);
    QFETCH(bool, accepted);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool wasModified = m_app->doc()->isModified();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, kind == "fit" ? 200.04 : (kind == "clip" ? 200 : 400), 200));
    const quint32 frameID = frame->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    auto *button = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    const bool fractional = kind == "fit" || kind == "touch";
    button->setLogicalGeometry(QRectF(kind == "clip" ? 100 : 0, 20, fractional ? 100.03 : 100, 80));
    if (kind == "touch" || kind == "overlap")
    {
        auto *neighbor = new VCButton(m_app->doc(), frame);
        vc->addWidgetToMap(neighbor);
        frame->addWidgetToPageMap(neighbor);
        neighbor->setLogicalGeometry(QRectF(kind == "touch" ? 200.04 : 110, 20, 100, 80));
    }
    VCWidget *edited = kind == "clip" ? static_cast<VCWidget *>(frame) : button;
    const QRectF before = edited->logicalGeometry();
    const QRectF request = kind == "clip" ? QRectF(0, 0, 199.99999999, 200)
        : (fractional ? QRectF(100.01, 20, 100.03, 80) : QRectF(10.00000001, 20, 100, 80));
    QSignalSpy geometry(edited, &VCWidget::geometryChanged);
    m_app->doc()->resetModified();

    const QVariantMap result = vc->applyGeometryPlan({{edited->id(), request}});

    QCOMPARE(result.value("accepted").toBool(), accepted);
    QCOMPARE(result.value("applied").toBool(), accepted);
    QCOMPARE(edited->logicalGeometry(), accepted ? request : before);
    QCOMPARE(geometry.count(), accepted ? 1 : 0);
    QCOMPARE(m_app->doc()->isModified(), accepted);
    if (!accepted)
        QVERIFY(!result.value("error").toString().isEmpty());
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcCreationArithmetic_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<bool>("accepted");
    QTest::newRow("explicit-fractional-exact-fit") << QString("fit") << true;
    QTest::newRow("explicit-fractional-neighbor-touch") << QString("touch") << true;
    QTest::newRow("explicit-real-overlap-1e-8") << QString("overlap") << false;
    QTest::newRow("default-minimum-ULP-fit") << QString("default") << true;
    QTest::newRow("nonfinite-content-still-refuses") << QString("nonfinite") << false;
}

void UpstreamIntegration_Test::vcCreationArithmetic()
{
    QFETCH(QString, kind);
    QFETCH(bool, accepted);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldSnap = vc->snapping();
    const bool wasModified = m_app->doc()->isModified();
    vc->setSnapping(false);
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, kind == "fit" ? 200.04 :
                                   (kind == "nonfinite" ? std::nan("") : 400), 300));
    const quint32 frameID = frame->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        vc->setSnapping(oldSnap);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    if (kind == "touch" || kind == "overlap")
    {
        auto *neighbor = new VCButton(m_app->doc(), frame);
        vc->addWidgetToMap(neighbor);
        frame->addWidgetToPageMap(neighbor);
        neighbor->setLogicalGeometry(QRectF(kind == "touch" ? 200.04 : 110, 20, 100, 80));
    }
    qreal minimumWidth = 0;
    if (kind == "default")
    {
        const QVariantMap measured = vc->previewCreate(frameID, "Button", QPointF(0, 20));
        QVERIFY(measured.value("accepted").toBool());
        minimumWidth = measured.value("minimumSize").toSizeF().width();
        frame->setLogicalGeometry(QRectF(0, 0, std::nextafter(minimumWidth, 0.0), 300));
    }
    const int count = frame->children(false).size();
    const QRectF request(kind == "overlap" ? 10.00000001 : 100.01, 20,
                         kind == "overlap" ? 100 : 100.03, 80);
    QSignalSpy inventory(vc, &VirtualConsole::widgetsChanged);
    QSignalSpy structure(vc, &VirtualConsole::structureChanged);
    m_app->doc()->resetModified();

    const QVariantMap result = kind == "default" ? vc->createWidget(frameID, "Button", QPointF(0, 20))
        : vc->createWidgetGeometry(frameID, "Button", request);

    QCOMPARE(result.value("accepted").toBool(), accepted);
    QCOMPARE(result.value("applied").toBool(), accepted);
    QCOMPARE(frame->children(false).size(), count + (accepted ? 1 : 0));
    QCOMPARE(inventory.count(), accepted ? 1 : 0);
    QCOMPARE(structure.count(), accepted ? 1 : 0);
    QCOMPARE(m_app->doc()->isModified(), accepted);
    if (accepted)
    {
        VCWidget *created = vc->widget(result.value("widgetIDs").toList().first().toUInt());
        QVERIFY(created);
        if (kind == "default")
        {
            QCOMPARE(created->logicalGeometry().left(), qreal(0));
            QCOMPARE(created->logicalGeometry().width(), minimumWidth);
        }
        else
            QCOMPARE(created->logicalGeometry(), request);
    }
    else
        QVERIFY(!result.value("error").toString().isEmpty());
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcHeaderFitArithmetic_data()
{
    QTest::addColumn<bool>("ulpFit");
    QTest::newRow("fractional-header-ULP-fit") << true;
    QTest::newRow("fractional-header-real-deficit-1e-8") << false;
}

void UpstreamIntegration_Test::vcHeaderFitArithmetic()
{
    QFETCH(bool, ulpFit);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool wasModified = m_app->doc()->isModified();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setLogicalGeometry(QRectF(0, 0, 400, 400));
    const quint32 frameID = frame->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    const qreal header = frame->contentRect().top();
    QVERIFY(header > 0);
    auto *button = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    const QRectF before(0, header, 100, 100.03);
    button->setLogicalGeometry(before);
    const qreal height = ulpFit
        ? std::nextafter(std::nextafter(header + 100.03, 0.0), 0.0) : header + 100.03 - 1e-8;
    frame->setLogicalGeometry(QRectF(0, 0, 400, height));
    QVERIFY(frame->contentRect().height() < 100.03);
    const QRectF request(20, header, 100, 100.03);
    QSignalSpy geometry(button, &VCWidget::geometryChanged);
    m_app->doc()->resetModified();

    const QVariantMap preview = vc->previewGeometry(button->id(), request, false);

    QCOMPARE(preview.value("accepted").toBool(), ulpFit);
    QCOMPARE(button->logicalGeometry(), before);
    QCOMPARE(geometry.count(), 0);
    QVERIFY(!m_app->doc()->isModified());
    const QVariantMap result = vc->editGeometry(button->id(), request, false);
    QCOMPARE(result.value("accepted").toBool(), ulpFit);
    QCOMPARE(button->logicalGeometry(), ulpFit ? request : before);
    QCOMPARE(geometry.count(), ulpFit ? 1 : 0);
    QCOMPARE(m_app->doc()->isModified(), ulpFit);
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcSnappedFarEdgeArithmetic_data()
{
    QTest::addColumn<qreal>("step");
    QTest::newRow("tenth-grid-line") << qreal(5.5118);
}

void UpstreamIntegration_Test::vcSnappedFarEdgeArithmetic()
{
    QFETCH(qreal, step);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldSnap = vc->snapping();
    const qreal oldStep = vc->snappingSize();
    const bool wasModified = m_app->doc()->isModified();
    vc->setSnapping(true);
    vc->setSnappingSize(step);
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 155.148, 300));
    const quint32 frameID = frame->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        vc->setSnapping(oldSnap);
        vc->setSnappingSize(oldStep);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    auto *button = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    const QRectF before(0, 0, 100.03, 80);
    button->setLogicalGeometry(before);
    const QRectF request(55.118, 0, 100.03, 80);
    QSignalSpy geometry(button, &VCWidget::geometryChanged);
    m_app->doc()->resetModified();

    const QVariantMap preview = vc->previewGeometry(button->id(), request, true);

    QVERIFY2(preview.value("accepted").toBool(), qPrintable(preview.value("error").toString()));
    QCOMPARE(preview.value("geometries").toList().first().toMap().value("rect").toRectF(), request);
    QCOMPARE(button->logicalGeometry(), before);
    QCOMPARE(geometry.count(), 0);
    QVERIFY(!m_app->doc()->isModified());
    const QVariantMap result = vc->editGeometry(button->id(), request, true);
    QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
    QCOMPARE(button->logicalGeometry(), request);
    QCOMPARE(geometry.count(), 1);
    QVERIFY(m_app->doc()->isModified());
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcFunctionCreationArithmetic_data()
{
    QTest::addColumn<bool>("wrapped");
    QTest::newRow("fractional-row-exact-fit") << false;
    QTest::newRow("snapped-wrapped-touch") << true;
}

void UpstreamIntegration_Test::vcFunctionCreationArithmetic()
{
    QFETCH(bool, wrapped);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldSnap = vc->snapping();
    const qreal oldStep = vc->snappingSize();
    const bool wasModified = m_app->doc()->isModified();
    vc->setSnapping(wrapped);
    vc->setSnappingSize(5.5118);
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 600));
    const quint32 frameID = frame->id();
    QVariantList functions;
    const auto cleanup = qScopeGuard([&]() {
        for (const QVariant &id : functions)
            m_app->doc()->function(id.toUInt())->stop(FunctionParent::master());
        m_app->doc()->masterTimer()->stop();
        vc->deleteVCWidgets({frameID});
        for (const QVariant &id : functions)
            m_app->doc()->deleteFunction(id.toUInt());
        vc->setSnapping(oldSnap);
        vc->setSnappingSize(oldStep);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    const QVariantMap measured = vc->previewCreate(frameID, "Button", QPointF(0, 0));
    QVERIFY(measured.value("accepted").toBool());
    const QSizeF size = measured.value("rect").toRectF().size();
    const int count = wrapped ? 4 : 2;
    const QPointF origin(wrapped ? 0 : 100.01, 0);
    frame->setLogicalGeometry(QRectF(0, 0,
        wrapped ? size.width() : 100.01 + 2 * size.width() + 5.5118,
        wrapped ? 4 * size.height() : size.height()));
    for (int i = 0; i < count; ++i)
    {
        auto *scene = new Scene(m_app->doc());
        scene->setName(QString("Numeric drop %1").arg(i));
        QVERIFY(m_app->doc()->addFunction(scene));
        functions.append(scene->id());
    }
    QSignalSpy inventory(vc, &VirtualConsole::widgetsChanged);
    QSignalSpy structure(vc, &VirtualConsole::structureChanged);
    m_app->doc()->resetModified();

    const QVariantMap result = vc->createFunctions(frameID, functions, origin, Qt::NoModifier);

    for (const QVariant &id : functions)
    {
        Function *function = m_app->doc()->function(id.toUInt());
        QVERIFY(function->stopped());
        QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(function));
    }
    QCOMPARE(m_app->runningFunctionsCount(), 0);
    if (!result.value("accepted").toBool())
    {
        QVERIFY(frame->children(false).isEmpty());
        QCOMPARE(inventory.count(), 0);
        QCOMPARE(structure.count(), 0);
        QVERIFY(!m_app->doc()->isModified());
    }
    QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
    QCOMPARE(frame->children(false).size(), count);
    QCOMPARE(inventory.count(), 1);
    QCOMPARE(structure.count(), 1);
    for (int i = 0; i < count; ++i)
    {
        auto *button = qobject_cast<VCButton *>(frame->children(false)[i]);
        QVERIFY(button);
        QCOMPARE(button->functionID(), functions[i].toUInt());
        QVERIFY(qAbs(button->logicalGeometry().top() - (wrapped ? i * size.height() : 0)) < 1e-10);
    }
    QVERIFY(m_app->doc()->isModified());
}

void UpstreamIntegration_Test::vcNestedLeafPointerResize_data()
{
    QTest::addColumn<bool>("snapping");
    QTest::addColumn<qreal>("snapStep");
    QTest::addColumn<bool>("horizontal");
    QTest::addColumn<QRectF>("expected");
    QTest::addColumn<int>("boundary");
    const qreal nativeStep = qreal(3 * 1117) / 220;
    QTest::newRow("vertical-unsnapped") << false << qreal(15) << false
        << QRectF(253, 190, 100, 85) << 0;
    QTest::newRow("vertical-snapped") << true << qreal(15) << false
        << QRectF(253, 190, 100, 80) << 0;
    QTest::newRow("vertical-native-grid") << true << nativeStep << false
        << QRectF(253, 190, 100, 84.17272727272723) << 0;
    QTest::newRow("horizontal-unsnapped") << false << qreal(15) << true
        << QRectF(190, 253, 130, 55) << 0;
    QTest::newRow("horizontal-snapped") << true << qreal(15) << true
        << QRectF(190, 253, 125, 55) << 0;
    QTest::newRow("horizontal-native-grid") << true << nativeStep << true
        << QRectF(190, 253, 129.86818181818182, 55) << 0;
    QTest::newRow("vertical-far-edge") << true << qreal(15) << false
        << QRectF(0, 190, 100, 80) << 1;
    QTest::newRow("horizontal-header-edge") << true << qreal(15) << true
        << QRectF(190, 0, 125, 55) << 2;
    QTest::newRow("vertical-header-edge") << true << qreal(15) << false
        << QRectF(253, 0, 100, 120) << 3;
    QTest::newRow("horizontal-far-edge-resize") << true << qreal(15) << true
        << QRectF(253, 190, 107, 55) << 4;
}

void UpstreamIntegration_Test::vcNestedLeafPointerResize()
{
    QFETCH(bool, snapping);
    QFETCH(qreal, snapStep);
    QFETCH(bool, horizontal);
    QFETCH(QRectF, expected);
    QFETCH(int, boundary);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode();
    const bool oldSnap = vc->snapping(), wasModified = m_app->doc()->isModified();
    const qreal oldStep = vc->snappingSize(), oldScale = vc->page(0)->pageScale();
    const QSize oldSize = m_app->size();
    const int oldPage = vc->selectedPage();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    QVariantList roots, functions;
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets(roots);
        for (const QVariant &id : functions)
            m_app->doc()->deleteFunction(id.toUInt());
        vc->page(0)->setPageScale(oldScale - vc->page(0)->pageScale());
        vc->setSnappingSize(oldStep);
        vc->setSnapping(oldSnap);
        vc->setSelectedPage(oldPage);
        vc->setSelectedWidgetIDs(oldSelection);
        vc->setEditMode(oldEdit);
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    vc->page(0)->setPageScale(1 - vc->page(0)->pageScale());
    vc->setSnapping(false);
    const QVariantMap frameResult = vc->createWidgetGeometry(vc->page(0)->id(), "Frame",
        QRectF(20, 20, 360, 340));
    QVERIFY2(frameResult.value("accepted").toBool(), qPrintable(frameResult.value("error").toString()));
    roots.append(frameResult.value("widgetIDs").toList().first());
    auto *frame = qobject_cast<VCFrame *>(vc->widget(roots.first().toUInt()));
    QVERIFY(frame);
    QRectF original = horizontal ? QRectF(190, 253, 100, 55) : QRectF(253, 190, 100, 55);
    if (boundary == 1)
    {
        original.moveLeft(frame->contentRect().right() - original.width());
        expected.moveLeft(original.left());
        QVERIFY(std::fmod(original.left(), snapStep) > 0);
    }
    else if (boundary == 2 || boundary == 3)
    {
        original.moveTop(frame->contentRect().top());
        expected.moveTop(original.top());
        if (boundary == 3)
            expected.setBottom(120);
        QVERIFY(std::fmod(original.top(), snapStep) > 0);
    }
    else if (boundary == 4)
        original = QRectF(253, 190, 100, 55);
    const QVariantMap buttonResult = vc->createWidgetGeometry(frame->id(), "Button", original);
    QVERIFY2(buttonResult.value("accepted").toBool(), qPrintable(buttonResult.value("error").toString()));
    const quint32 buttonID = buttonResult.value("widgetIDs").toList().first().toUInt();
    auto *button = qobject_cast<VCButton *>(vc->widget(buttonID));
    QVERIFY(button);
    auto *scene = new Scene(m_app->doc());
    QVERIFY(m_app->doc()->addFunction(scene));
    functions.append(scene->id());
    button->setFunctionID(scene->id());
    button->setCaption("Reset");
    const QVariantList rootOrder = vc->page(0)->orderedChildIDs(0);
    const QVariantList childOrder = frame->orderedChildIDs(0);
    const QRectF frameGeometry = frame->logicalGeometry();
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1600, 950);
    contexts->switchToContext("VC");
    vc->setSelectedPage(0);
    vc->setEditMode(true);
    vc->page(0)->setPageScale(0.8 - vc->page(0)->pageScale());
    vc->setSnappingSize(snapStep);
    vc->setSnapping(snapping);
    QCOMPARE(vc->snapping(), snapping);
    QCOMPARE(vc->snappingSize(), snapStep);
    QCOMPARE(vc->page(0)->pageScale(), qreal(0.8));
    QCOMPARE(button->scaleFactor(), qreal(0.8));
    QCOMPARE(button->logicalGeometry(), original);
    vc->setSelectedWidgetIDs({buttonID});
    QQuickItem *item = nullptr;
    QTRY_VERIFY((item = button->renderItem()));
    QTRY_VERIFY(item->property("isSelected").toBool());
    QQuickItem *handle = nullptr;
    std::function<void(QQuickItem *)> findHandle = [&](QQuickItem *candidate) {
        if (candidate->property("leftEdge").isValid()
            && !candidate->property("leftEdge").toBool()
            && !candidate->property("topEdge").toBool())
            handle = candidate;
        for (QQuickItem *child : candidate->childItems())
            findHandle(child);
    };
    findHandle(item);
    QVERIFY(handle);
    QVERIFY(handle->isVisible());
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    const QPoint press = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2)).toPoint();
    QVERIFY(handle->contains(handle->mapFromScene(press)));
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
    QVERIFY(item->property("resizing").toBool());
    QVERIFY(!frame->renderItem()->property("resizing").toBool());
    const QPoint delta = horizontal ? QPoint(boundary == 4 ? 4 : 24, 0) : QPoint(0, 24);
    QTest::mouseMove(m_app.get(), press + delta, 20);
    const QRectF requested = item->property("resizeIntent").toRectF();
    QCOMPARE(button->logicalGeometry(), original);
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, press + delta);
    QVERIFY(!item->property("resizing").toBool());
    QCOMPARE(requested, QRectF(original.topLeft(),
        horizontal ? QSizeF(boundary == 4 ? 105 : 130, 55) : QSizeF(100, 85)));
    QCOMPARE(vc->snapping(), snapping);
    QCOMPARE(vc->selectedWidgetIDs(), QVariantList{buttonID});
    QCOMPARE(button->parent(), frame);
    QCOMPARE(button->page(), 0);
    QCOMPARE(button->functionID(), scene->id());
    QCOMPARE(vc->page(0)->orderedChildIDs(0), rootOrder);
    QCOMPARE(frame->orderedChildIDs(0), childOrder);
    QCOMPARE(frame->logicalGeometry(), frameGeometry);
    QVERIFY(scene->stopped());
    QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(scene));
    QCOMPARE(m_app->runningFunctionsCount(), 0);
    const QRectF result = button->logicalGeometry();
    QVERIFY(horizontal ? result.width() > original.width() : result.height() > original.height());
    QVERIFY(frame->contentRect().contains(result));
    QCOMPARE(result.x(), original.x());
    QCOMPARE(result.y(), original.y());
    if (horizontal)
        QCOMPARE(result.height(), original.height());
    else
        QCOMPARE(result.width(), original.width());
    QCOMPARE(result, expected);
}

void UpstreamIntegration_Test::vcHeldEdgeMinimum_data()
{
    QTest::addColumn<bool>("fixedRight");
    QTest::addColumn<bool>("overhang");
    QTest::newRow("held-right") << true << false;
    QTest::newRow("held-bottom") << false << false;
    QTest::newRow("held-right-overhang") << true << true;
}

void UpstreamIntegration_Test::vcHeldEdgeMinimum()
{
    QFETCH(bool, fixedRight);
    QFETCH(bool, overhang);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldSnap = vc->snapping(), oldEdit = vc->editMode();
    const bool wasModified = m_app->doc()->isModified();
    const qreal oldStep = vc->snappingSize();
    const int oldPage = vc->selectedPage();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 336, 336));
    const quint32 frameID = frame->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        vc->setSnappingSize(oldStep);
        vc->setSnapping(oldSnap);
        vc->setSelectedPage(oldPage);
        vc->setEditMode(oldEdit);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    vc->setSelectedPage(0);
    vc->setEditMode(true);
    vc->setSnapping(false);
    const QRectF seed = fixedRight ? QRectF(135, 60, 200, 100) : QRectF(60, 135, 100, 200);
    const QVariantMap creation = vc->createWidgetGeometry(frameID, "Button", seed);
    QVERIFY2(creation.value("accepted").toBool(), qPrintable(creation.value("error").toString()));
    VCWidget *button = vc->widget(creation.value("widgetIDs").toList().first().toUInt());
    QVERIFY(button);
    QRectF original = seed;
    if (overhang)
    {
        original.moveRight(346);
        button->setLogicalGeometry(original);
    }
    const QSizeF minimum = button->minimumSize();
    QVERIFY(original.width() >= minimum.width());
    QVERIFY(original.height() >= minimum.height());
    if (!overhang)
        QVERIFY(frame->contentRect().contains(original));
    QCOMPARE(button->logicalGeometry(), original);
    const QVariantList order = frame->orderedChildIDs(0);
    vc->setSnappingSize(15);
    vc->setSnapping(true);
    QCOMPARE(vc->snapping(), true);
    QCOMPARE(vc->snappingSize(), qreal(15));
    m_app->doc()->resetModified();
    const qreal heldEnd = overhang ? 346 : 335;
    const QRectF request = fixedRight ? QRectF(heldEnd - 1, 60, 1, 100) : QRectF(60, 334, 100, 1);
    const QVariantMap preview = vc->previewGeometry(button->id(), request);
    QCOMPARE(button->logicalGeometry(), original);
    QVERIFY(!m_app->doc()->isModified());
    const QVariantMap result = vc->editGeometry(button->id(), request);
    if (!result.value("accepted").toBool())
    {
        QCOMPARE(button->logicalGeometry(), original);
        QVERIFY(!m_app->doc()->isModified());
    }
    QCOMPARE(button->parent(), frame);
    QCOMPARE(button->page(), 0);
    QCOMPARE(frame->orderedChildIDs(0), order);
    QCOMPARE(m_app->runningFunctionsCount(), 0);
    QVERIFY2(preview.value("accepted").toBool(), qPrintable(preview.value("error").toString()));
    QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
    const QVariantList geometries = preview.value("geometries").toList();
    QVERIFY(!geometries.isEmpty());
    const QRectF candidate = geometries.first().toMap().value("rect").toRectF();
    const QRectF committed = button->logicalGeometry();
    QVERIFY(candidate.width() >= minimum.width() && candidate.height() >= minimum.height());
    QVERIFY(committed.width() >= minimum.width() && committed.height() >= minimum.height());
    if (overhang)
    {
        QCOMPARE(candidate.right() - frame->contentRect().right(), qreal(10));
        QCOMPARE(committed.right() - frame->contentRect().right(), qreal(10));
        QVERIFY(candidate.left() >= frame->contentRect().left());
        QVERIFY(committed.left() >= frame->contentRect().left());
        QVERIFY(candidate.top() >= frame->contentRect().top());
        QVERIFY(committed.bottom() <= frame->contentRect().bottom());
    }
    else
    {
        QVERIFY(frame->contentRect().contains(candidate));
        QVERIFY(frame->contentRect().contains(committed));
    }
    QCOMPARE(fixedRight ? candidate.right() : candidate.bottom(), heldEnd);
    QCOMPARE(fixedRight ? committed.right() : committed.bottom(), heldEnd);
    const qreal alignedMinimum = std::ceil((fixedRight ? minimum.width() : minimum.height()) / 15) * 15;
    const qreal expectedDragged = std::floor((heldEnd - alignedMinimum) / 15) * 15;
    QCOMPARE(fixedRight ? candidate.left() : candidate.top(), expectedDragged);
    QCOMPARE(fixedRight ? committed.left() : committed.top(), expectedDragged);
    if (overhang)
    {
        QRectF worse = committed;
        worse.setRight(361);
        m_app->doc()->resetModified();
        const QVariantMap rejectedPreview = vc->previewGeometryPlan({{button->id(), worse}});
        const QVariantMap rejectedApply = vc->applyGeometryPlan({{button->id(), worse}});
        QVERIFY(!rejectedPreview.value("accepted").toBool());
        QVERIFY(!rejectedApply.value("accepted").toBool());
        QVERIFY(!rejectedApply.value("error").toString().isEmpty());
        QCOMPARE(button->logicalGeometry(), committed);
        QCOMPARE(frame->orderedChildIDs(0), order);
        QVERIFY(!m_app->doc()->isModified());
        QCOMPARE(m_app->runningFunctionsCount(), 0);
    }
}

void UpstreamIntegration_Test::vcDefaultHeaderDrop_data()
{
    QTest::addColumn<bool>("functionDrop");
    QTest::newRow("widget") << false;
    QTest::newRow("function") << true;
}

void UpstreamIntegration_Test::vcDefaultHeaderDrop()
{
    QFETCH(bool, functionDrop);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldSnap = vc->snapping(), oldEdit = vc->editMode();
    const bool wasModified = m_app->doc()->isModified();
    const qreal oldStep = vc->snappingSize();
    const int oldPage = vc->selectedPage();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(true);
    frame->setLogicalGeometry(QRectF(0, 0, 336, 336));
    const quint32 frameID = frame->id();
    auto *scene = new Scene(m_app->doc());
    QVERIFY(m_app->doc()->addFunction(scene));
    const quint32 functionID = scene->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        m_app->doc()->deleteFunction(functionID);
        vc->setSnappingSize(oldStep);
        vc->setSnapping(oldSnap);
        vc->setSelectedPage(oldPage);
        vc->setEditMode(oldEdit);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    vc->setSelectedPage(0);
    vc->setEditMode(true);
    vc->setSnappingSize(15);
    vc->setSnapping(true);
    QCOMPARE(vc->snapping(), true);
    QCOMPARE(vc->snappingSize(), qreal(15));
    QVERIFY(frame->contentRect().top() > 0);
    m_app->doc()->resetModified();
    const QVariantMap preview = vc->previewCreate(frameID, "Button", QPointF(0, 0));
    QVERIFY(frame->children(false).isEmpty());
    QVERIFY(!m_app->doc()->isModified());
    const QVariantMap result = functionDrop
        ? vc->createFunctions(frameID, {functionID}, QPointF(0, 0), Qt::NoModifier)
        : vc->createWidget(frameID, "Button", QPointF(0, 0));
    if (!result.value("accepted").toBool())
    {
        QVERIFY(frame->children(false).isEmpty());
        QVERIFY(!m_app->doc()->isModified());
    }
    QVERIFY(scene->stopped());
    QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(scene));
    QCOMPARE(m_app->runningFunctionsCount(), 0);
    QVERIFY2(preview.value("accepted").toBool(), qPrintable(preview.value("error").toString()));
    QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
    QCOMPARE(frame->children(false).size(), 1);
    auto *created = qobject_cast<VCButton *>(frame->children(false).first());
    QVERIFY(created);
    QCOMPARE(created->parent(), frame);
    QCOMPARE(created->page(), 0);
    const QRectF rectangle = created->logicalGeometry();
    QVERIFY(frame->contentRect().contains(rectangle));
    QVERIFY(rectangle.top() >= frame->contentRect().top());
    QVERIFY(rectangle.width() >= created->minimumSize().width());
    QVERIFY(rectangle.height() >= created->minimumSize().height());
    if (functionDrop)
        QCOMPARE(created->functionID(), functionID);
    else
        QCOMPARE(rectangle, preview.value("rect").toRectF());
}

void UpstreamIntegration_Test::vcLegacyLeafPointerResize_data()
{
    QTest::addColumn<bool>("snapping");
    QTest::newRow("unsnapped") << false;
    QTest::newRow("snapped") << true;
}

void UpstreamIntegration_Test::vcLegacyLeafPointerResize()
{
    QFETCH(bool, snapping);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const QSize oldSize = m_app->size();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode();
    const bool oldSnap = vc->snapping(), wasModified = m_app->doc()->isModified();
    const qreal oldStep = vc->snappingSize(), oldScale = vc->page(0)->pageScale();
    const int oldPage = vc->selectedPage();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setLogicalGeometry(QRectF(20, 20, 360, 340));
    const quint32 frameID = frame->id();
    auto *scene = new Scene(m_app->doc());
    QVERIFY(m_app->doc()->addFunction(scene));
    const quint32 functionID = scene->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        m_app->doc()->deleteFunction(functionID);
        vc->page(0)->setPageScale(oldScale - vc->page(0)->pageScale());
        vc->setSnappingSize(oldStep);
        vc->setSnapping(oldSnap);
        vc->setSelectedPage(oldPage);
        vc->setSelectedWidgetIDs(oldSelection);
        vc->setEditMode(oldEdit);
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    vc->setSnapping(false);
    const QVariantMap creation = vc->createWidgetGeometry(frameID, "Button", QRectF(10, 60, 100, 55));
    QVERIFY2(creation.value("accepted").toBool(), qPrintable(creation.value("error").toString()));
    auto *button = qobject_cast<VCButton *>(
        vc->widget(creation.value("widgetIDs").toList().first().toUInt()));
    QVERIFY(button);
    button->setFunctionID(functionID);
    const QRectF original(-10, 60, 100, 55);
    button->setLogicalGeometry(original);
    QVERIFY(original.width() >= button->minimumSize().width());
    QVERIFY(original.height() >= button->minimumSize().height());
    const QVariantList order = frame->orderedChildIDs(0);
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1600, 950);
    contexts->switchToContext("VC");
    vc->setSelectedPage(0);
    vc->setEditMode(true);
    vc->page(0)->setPageScale(0.8 - vc->page(0)->pageScale());
    vc->setSnappingSize(15);
    vc->setSnapping(snapping);
    QCOMPARE(vc->snapping(), snapping);
    QCOMPARE(button->scaleFactor(), qreal(0.8));
    QCOMPARE(button->logicalGeometry(), original);
    vc->setSelectedWidgetIDs({button->id()});
    QQuickItem *item = nullptr, *handle = nullptr;
    QTRY_VERIFY((item = button->renderItem()));
    QTRY_VERIFY(item->property("isSelected").toBool());
    std::function<void(QQuickItem *)> findHandle = [&](QQuickItem *candidate) {
        if (candidate->property("leftEdge").isValid()
            && !candidate->property("leftEdge").toBool()
            && !candidate->property("topEdge").toBool())
            handle = candidate;
        for (QQuickItem *child : candidate->childItems())
            findHandle(child);
    };
    findHandle(item);
    QVERIFY(handle);
    QVERIFY(handle->isVisible());
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    const QPoint press = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2)).toPoint();
    QVERIFY(frame->renderItem()->contains(frame->renderItem()->mapFromScene(press)));
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
    QVERIFY(item->property("resizing").toBool());
    QVERIFY(!frame->renderItem()->property("resizing").toBool());
    QTest::mouseMove(m_app.get(), press + QPoint(0, 24), 20);
    QCOMPARE(item->property("resizeIntent").toRectF(), QRectF(-10, 60, 100, 85));
    QCOMPARE(button->logicalGeometry(), original);
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, press + QPoint(0, 24));
    QVERIFY(!item->property("resizing").toBool());
    QCOMPARE(vc->selectedWidgetIDs(), QVariantList{button->id()});
    QCOMPARE(button->parent(), frame);
    QCOMPARE(button->page(), 0);
    QCOMPARE(frame->orderedChildIDs(0), order);
    QCOMPARE(button->functionID(), functionID);
    QVERIFY(scene->stopped());
    QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(scene));
    QCOMPARE(m_app->runningFunctionsCount(), 0);
    QCOMPARE(button->logicalGeometry(), QRectF(-10, 60, 100, snapping ? 90 : 85));
}

void UpstreamIntegration_Test::vcClipboardPointerDrop_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<bool>("fits");
    for (qreal scale : {qreal(1), qreal(0.8), qreal(2), qreal(1.1)})
        for (bool fits : {true, false})
            QTest::newRow(qPrintable(QString("%1-%2").arg(scale).arg(fits ? "contained" : "edge")))
                << scale << fits;
}

void UpstreamIntegration_Test::vcClipboardPointerDrop()
{
    QFETCH(qreal, scale);
    QFETCH(bool, fits);
    for (const QString &name : {QString("VCFrameItem"), QString("VCRightPanel")})
    {
        QFile source(QStringLiteral(QT_TESTCASE_SOURCEDIR "/../../qml/virtualconsole/") + name + ".qml");
        QFile resource(":/" + name + ".qml");
        QVERIFY(source.open(QIODevice::ReadOnly));
        QVERIFY(resource.open(QIODevice::ReadOnly));
        QCOMPARE(resource.readAll(), source.readAll());
    }
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const QSize oldSize = m_app->size();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode();
    const bool oldSnap = vc->snapping(), wasModified = m_app->doc()->isModified();
    const int oldPage = vc->selectedPage(), pageIndex = vc->pagesCount();
    const QVariantList oldSelection = vc->selectedWidgetIDs(), oldClipboard = vc->clipboardItemsList();
    const bool oldClipboardCut = vc->clipboardIsCut();
    QList<quint32> functionIDs;
    vc->addPage(pageIndex);
    const auto cleanup = qScopeGuard([&]() {
        vc->setSelectedPage(oldPage);
        vc->deletePage(pageIndex);
        for (quint32 id : functionIDs)
            m_app->doc()->deleteFunction(id);
        vc->setSelectedWidgetIDs(oldClipboard);
        if (oldClipboardCut)
            vc->cutToClipboard();
        else
            vc->copyToClipboard();
        vc->setSelectedWidgetIDs(oldSelection);
        vc->setSnapping(oldSnap);
        vc->setEditMode(oldEdit);
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified)
            m_app->doc()->resetModified();
    });
    vc->setSnapping(false);
    vc->setSelectedPage(pageIndex);
    VCPage *page = vc->page(pageIndex);
    page->setLogicalGeometry(QRectF(0, 0, 3000, 1800));
    const QVariantMap outerResult = vc->createWidgetGeometry(page->id(), "Frame",
        QRectF(220, 70, 680, 560));
    QVERIFY2(outerResult.value("accepted").toBool(), qPrintable(outerResult.value("error").toString()));
    auto *outer = qobject_cast<VCFrame *>(vc->widget(outerResult.value("widgetIDs").toList().first().toUInt()));
    QVERIFY(outer);
    const QVariantMap targetResult = vc->createWidgetGeometry(outer->id(), "Frame",
        QRectF(40, 60, 600, 430));
    QVERIFY2(targetResult.value("accepted").toBool(), qPrintable(targetResult.value("error").toString()));
    auto *target = qobject_cast<VCFrame *>(vc->widget(targetResult.value("widgetIDs").toList().first().toUInt()));
    QVERIFY(target);
    target->setMultiPageMode(true);
    target->setTotalPagesNumber(2);
    target->setCurrentPage(1);
    auto *source = new VCFrame(m_app->doc(), vc, page);
    vc->addWidgetToMap(source);
    page->addWidgetToPageMap(source);
    source->setShowHeader(false);
    source->setLogicalGeometry(QRectF(1000, 30, 500, 400));
    auto *group = new VCFrame(m_app->doc(), vc, source);
    vc->addWidgetToMap(group);
    source->addWidgetToPageMap(group);
    group->setShowHeader(false);
    group->setLogicalGeometry(QRectF(10, 40, 250.25, 220.5));
    auto *child = new VCButton(m_app->doc(), group);
    auto *button = new VCButton(m_app->doc(), source);
    for (VCButton *entry : {child, button})
    {
        auto *parent = qobject_cast<VCFrame *>(entry->parent());
        vc->addWidgetToMap(entry);
        parent->addWidgetToPageMap(entry);
        entry->setLogicalGeometry(entry == child ? QRectF(10, 40, 100.5, 80.25)
                                                 : QRectF(260.25, 40, 110.25, 90.5));
        auto *scene = new Scene(m_app->doc());
        QVERIFY(m_app->doc()->addFunction(scene));
        functionIDs.append(scene->id());
        entry->setFunctionID(scene->id());
        entry->setActionType(VCButton::Toggle);
    }
    const QVariantList sourceOrder = source->orderedChildIDs(0), groupOrder = group->orderedChildIDs(0);
    const QVariantList pageOrder = page->orderedChildIDs(0), outerOrder = outer->orderedChildIDs(0);
    const QRectF outerGeometry = outer->logicalGeometry(), targetGeometry = target->logicalGeometry();
    const QRectF groupGeometry = group->logicalGeometry(), buttonGeometry = button->logicalGeometry();
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(2200, 1400);
    contexts->switchToContext("VC");
    vc->setSelectedPage(pageIndex);
    vc->setEditMode(true);
    page->setPageScale(scale - page->pageScale());
    QCOMPARE(page->pageScale(), scale);
    QCOMPARE(target->scaleFactor(), scale);
    QCOMPARE(target->currentPage(), 1);
    QVERIFY(target->showHeader());
    QVERIFY(target->contentRect().top() > 0);
    QCOMPARE(vc->selectedPage(), pageIndex);
    QQuickItem *area = nullptr, *pane = nullptr, *viewport = nullptr;
    QTRY_VERIFY(target->renderItem());
    QTRY_VERIFY((area = target->renderItem()->findChild<QQuickItem *>(
        QString("frameDropArea%1").arg(target->id()))));
    QTRY_VERIFY((pane = m_app->rootObject()->findChild<QQuickItem *>("vcFunctionsPane")));
    QTRY_VERIFY((viewport = vc->currentPageItem()));
    const qreal oldX = viewport->property("contentX").toReal(), oldY = viewport->property("contentY").toReal();
    const auto restoreScroll = qScopeGuard([&]() {
        viewport->setProperty("contentX", oldX);
        viewport->setProperty("contentY", oldY);
    });
    QVERIFY(viewport->setProperty("contentX", 40 * scale));
    QVERIFY(viewport->setProperty("contentY", 20 * scale));
    QCOMPARE(viewport->property("contentX").toReal(), 40 * scale);
    QCOMPARE(viewport->property("contentY").toReal(), 20 * scale);
    auto *toggle = pane->findChild<QQuickItem *>("vcEditModeToggle");
    QVERIFY(toggle);
    QQmlExpression pasteExpression(qmlContext(toggle), toggle, "pasteButton");
    auto *pasteButton = qobject_cast<QQuickItem *>(pasteExpression.evaluate().value<QObject *>());
    QVERIFY2(pasteButton, qPrintable(pasteExpression.error().toString()));
    QQmlExpression dragExpression(qmlContext(toggle), toggle, "dumpDragArea");
    QObject *dragArea = dragExpression.evaluate().value<QObject *>();
    QVERIFY2(dragArea, qPrintable(dragExpression.error().toString()));
    QQmlExpression sourceExpression(qmlContext(toggle), toggle, "pasteDragItem");
    auto *dragSource = qobject_cast<QQuickItem *>(sourceExpression.evaluate().value<QObject *>());
    QVERIFY2(dragSource, qPrintable(sourceExpression.error().toString()));
    vc->setSelectedWidgetIDs({group->id(), button->id()});
    vc->copyToClipboard();
    QCOMPARE(vc->clipboardItemsList(), (QVariantList{group->id(), button->id()}));
    QVERIFY(pasteButton->isEnabled());
    QTRY_COMPARE(pane->width(), pane->property(pane->property("isOpen").toBool()
        ? "expandedWidth" : "collapseWidth").toReal());
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    // Align the scrolled DropArea origin for QTest's integer mouse coordinates.
    const QPointF areaOrigin = area->mapToScene(QPointF());
    viewport->setProperty("contentX", viewport->property("contentX").toReal()
                          + areaOrigin.x() - std::round(areaOrigin.x()));
    viewport->setProperty("contentY", viewport->property("contentY").toReal()
                          + areaOrigin.y() - std::round(areaOrigin.y()));
    QCOMPARE(area->mapToScene(QPointF()), QPointF(std::round(areaOrigin.x()), std::round(areaOrigin.y())));
    const QPoint press = pasteButton->mapToScene(QPointF(pasteButton->width() / 2,
                                                        pasteButton->height() / 2)).toPoint();
    const QPointF logicalDrop = fits ? QPointF(100, 100) : QPointF(590, 420);
    const QPointF dropScene = area->mapToScene(logicalDrop * scale);
    QPoint drop = (dropScene + press - dragSource->mapToScene(QPointF())).toPoint();
    const QRect windowRect(QPoint(), m_app->size());
    QVERIFY(windowRect.contains(press));
    QVERIFY(windowRect.contains(drop));
    QVERIFY(area->contains(area->mapFromScene(dropScene)));
    QCOMPARE(area->mapFromScene(dropScene), logicalDrop * scale);
    QSignalSpy dropped(area, SIGNAL(dropped(QQuickDragEvent*)));
    QSignalSpy widgets(vc, &VirtualConsole::widgetsChanged);
    QSignalSpy structure(vc, &VirtualConsole::structureChanged);
    QSignalSpy rejected(vc, &VirtualConsole::editRejected);
    QSignalSpy starts(m_app->doc()->masterTimer(), SIGNAL(functionStarted(quint32)));
    QVERIFY(dropped.isValid());
    QVERIFY(starts.isValid());
    m_app->doc()->resetModified();
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
    const auto releaseMouse = qScopeGuard([&]() {
        if (dragArea->property("dragActive").toBool())
            QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, drop);
    });
    QTest::mouseMove(m_app.get(), press + QPoint(-20, 0), 20);
    QTest::mouseMove(m_app.get(), press + QPoint(-40, 0), 20);
    QTRY_VERIFY(dragArea->property("dragActive").toBool());
    QTest::qWait(20);
    drop = (area->mapToScene(logicalDrop * scale) + press + QPoint(-40, 0)
            - dragSource->mapToScene(QPointF())).toPoint();
    QTest::mouseMove(m_app.get(), drop, 20);
    QTest::qWait(20);
    QObject *targetDrag = area->property("drag").value<QObject *>();
    QVERIFY(targetDrag);
    QTRY_COMPARE(targetDrag->property("source").value<QObject *>(), dragSource);
    QTRY_VERIFY(area->property("containsDrag").toBool());
    QCOMPARE(QPointF(targetDrag->property("x").toReal(), targetDrag->property("y").toReal()),
             logicalDrop * scale);
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, drop);
    QTRY_VERIFY(!dragArea->property("dragActive").toBool());
    QTRY_COMPARE(dropped.count(), 1);
    QCOMPARE(starts.count(), 0);
    QCOMPARE(m_app->runningFunctionsCount(), 0);
    for (quint32 id : functionIDs)
    {
        QVERIFY(m_app->doc()->function(id)->stopped());
        QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(m_app->doc()->function(id)));
    }
    QCOMPARE(source->orderedChildIDs(0), sourceOrder);
    QCOMPARE(group->orderedChildIDs(0), groupOrder);
    QCOMPARE(page->orderedChildIDs(0), pageOrder);
    QCOMPARE(outer->orderedChildIDs(0), outerOrder);
    QCOMPARE(outer->logicalGeometry(), outerGeometry);
    QCOMPARE(target->logicalGeometry(), targetGeometry);
    QCOMPARE(group->logicalGeometry(), groupGeometry);
    QCOMPARE(button->logicalGeometry(), buttonGeometry);
    QCOMPARE(child->logicalGeometry(), QRectF(10, 40, 100.5, 80.25));
    QCOMPARE(vc->clipboardItemsList(), (QVariantList{group->id(), button->id()}));
    QVERIFY(target->orderedChildIDs(0).isEmpty());
    QCOMPARE(target->currentPage(), 1);
    QCOMPARE(target->scaleFactor(), scale);
    if (!fits)
    {
        QVERIFY(target->children(true).isEmpty());
        QVERIFY(!m_app->doc()->isModified());
        QCOMPARE(widgets.count(), 0);
        QCOMPARE(structure.count(), 0);
        QCOMPARE(rejected.count(), 1);
        QVERIFY(!rejected.first().first().toString().isEmpty());
        auto *popup = m_app->rootObject()->findChild<QObject *>("vcLayoutError");
        QVERIFY(popup);
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        return;
    }
    QCOMPARE(rejected.count(), 0);
    QCOMPARE(widgets.count(), 1);
    QCOMPARE(structure.count(), 1);
    QVERIFY(m_app->doc()->isModified());
    QCOMPARE(target->children(false).size(), 2);
    QCOMPARE(target->children(true).size(), 3);
    const QVariantList copies = target->orderedChildIDs(1);
    QCOMPARE(copies.size(), 2);
    auto *groupCopy = qobject_cast<VCFrame *>(vc->widget(copies[0].toUInt()));
    auto *buttonCopy = qobject_cast<VCButton *>(vc->widget(copies[1].toUInt()));
    QVERIFY(groupCopy);
    QVERIFY(buttonCopy);
    QCOMPARE(groupCopy->logicalGeometry().size(), QSizeF(250.25, 220.5));
    QCOMPARE(buttonCopy->logicalGeometry().size(), QSizeF(110.25, 90.5));
    QCOMPARE(buttonCopy->logicalGeometry().topLeft() - groupCopy->logicalGeometry().topLeft(), QPointF(250.25, 0));
    for (VCWidget *copy : {static_cast<VCWidget *>(groupCopy), static_cast<VCWidget *>(buttonCopy)})
    {
        QCOMPARE(copy->parent(), target);
        QCOMPARE(copy->page(), 1);
        QCOMPARE(copy->scaleFactor(), scale);
        QVERIFY(target->contentRect().contains(copy->logicalGeometry()));
        QVERIFY(copy->renderItem());
        QCOMPARE(copy->renderItem()->parentItem(), area);
    }
    QCOMPARE(buttonCopy->functionID(), functionIDs[1]);
    QCOMPARE(buttonCopy->actionType(), VCButton::Toggle);
    QCOMPARE(groupCopy->orderedChildIDs(0).size(), 1);
    auto *childCopy = qobject_cast<VCButton *>(vc->widget(groupCopy->orderedChildIDs(0).first().toUInt()));
    QVERIFY(childCopy);
    QCOMPARE(childCopy->logicalGeometry(), QRectF(10, 40, 100.5, 80.25));
    QCOMPARE(childCopy->scaleFactor(), scale);
    QCOMPARE(childCopy->functionID(), functionIDs[0]);
    QCOMPARE(childCopy->actionType(), VCButton::Toggle);
    QCOMPARE(groupCopy->logicalGeometry(), QRectF(100, 100, 250.25, 220.5));
    QCOMPARE(buttonCopy->logicalGeometry(), QRectF(350.25, 100, 110.25, 90.5));
}

QTEST_MAIN(UpstreamIntegration_Test)
#include "upstreamintegration_test.moc"
