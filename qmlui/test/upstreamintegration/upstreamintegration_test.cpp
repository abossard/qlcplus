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
#include <QQuickItemGrabResult>
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
#include <QSaveFile>
#include <QTimer>
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QUuid>
#include <algorithm>
#include <functional>

#define private public
#include "app.h"
#include "ioplugincache.h"
#undef private
#include "contextmanager.h"
#include "chaser.h"
#include "doc.h"
#include "efx.h"
#include "djmanager.h"
#include "editorview.h"
#include "fixture.h"
#include "fixtureeditor.h"
#include "fixturegroup.h"
#include "fixturegroupeditor.h"
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
#include "showcommandrecorder.h"
#include "showclipeditor.h"
#include "showrunner.h"
#define private public
#include "tardis.h"
#undef private
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
#include "qlcinputchannel.h"
#include "fixtureutils.h"
#include "uimanager.h"
#include "listmodel.h"
#include "simpledesk.h"

#define private public
#include "scriptv4.h"
#include "scriptrunner.h"
#undef private

Q_DECLARE_METATYPE(ShowClipAutomation)

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
    void showClipCycleGuard_data();
    void showClipCycleGuard();
    void nestedClipTrim_data();
    void nestedClipTrim();
    void nestedClipTrimUndoSkipsARemovedClip_data();
    void nestedClipTrimUndoSkipsARemovedClip();
    void nestedClipPaste_data();
    void nestedClipPaste();
    void nestedClipOpen_data();
    void nestedClipOpen();
    void nestedClipOverlay_data();
    void nestedClipOverlay();
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
    void fixtureGroupSelection_data();
    void fixtureGroupSelection();
    void defaultUiScaling_data();
    void defaultUiScaling();
    void uiSettingsInitialization_data();
    void uiSettingsInitialization();
    void emptyVirtualConsole_data();
    void emptyVirtualConsole();
    void stoppedCueNavigation_data();
    void stoppedCueNavigation();
    void tardisHistory_data();
    void tardisHistory();
    void tardisRecordingHistory_data();
    void tardisRecordingHistory();
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
    void incomingListModel_data();
    void incomingListModel();
    void incomingDeskBytes_data();
    void incomingDeskBytes();
    void incomingDistribution_data();
    void incomingDistribution();
    void incomingChaserTempo_data();
    void incomingChaserTempo();
    void C3_beatEditing_data();
    void C3_beatEditing();
    void incomingChaserCommonHistory_data();
    void incomingChaserCommonHistory();
    void incomingSettingsScroll_data();
    void incomingSettingsScroll();
    void incomingScriptNotifications_data();
    void incomingScriptNotifications();
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
    void vcSliderCopy_data();
    void vcSliderCopy();
    void faderHandleScaling_data();
    void faderHandleScaling();
    void selectedFixtureColors_data();
    void selectedFixtureColors();
    void C3_additiveColors_data();
    void C3_additiveColors();
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
    void vcOutlineAssignmentSummary_data();
    void vcOutlineAssignmentSummary();
    void vcOutlineScopeFallback_data();
    void vcOutlineScopeFallback();
    void vcOutlineRouteResolution_data();
    void vcOutlineRouteResolution();
    void vcOutlineAssignmentPaneLifecycle();
    void vcOutlineAssignmentSummaryClipping_data();
    void vcOutlineAssignmentSummaryClipping();
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
    void vcOutlineMovesKeepOtherRows_data();
    void vcOutlineMovesKeepOtherRows();
    void vcScreenshotConsole_data();
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
    void vcDerivedWorkspacePersistence_data();
    void vcDerivedWorkspacePersistence();
    void vcPublicationLifetime_data();
    void vcPublicationLifetime();
    void vcSelectedChrome_data();
    void vcSelectedChrome();
    void vcScrollbarChromeReplay_data();
    void vcScrollbarChromeReplay();
    void vcWidgetPointerDrop_data();
    void vcWidgetPointerDrop();
    void vcDeferredReveal_data();
    void vcDeferredReveal();
    void vcPageColorDeletion();
    void vcFrameLegacyClip();
    void vcFramePresentation_data();
    void vcFramePresentation();
private:
    QTemporaryDir m_settings;
    std::unique_ptr<App> m_app;
};

namespace
{
class OutlineRoutePlugin final : public QLCIOPlugin
{
public:
    explicit OutlineRoutePlugin(const QString &name)
        : m_name(name)
    {
    }

    void init() override {}
    QString name() const override { return m_name; }
    int capabilities() const override { return QLCIOPlugin::Input | QLCIOPlugin::Output | QLCIOPlugin::Feedback; }
    QString pluginInfo() const override { return m_name; }
    QStringList inputs() override { return {QStringLiteral("Input A"), QStringLiteral("Input B")}; }
    QStringList inputsUID() override { return {QStringLiteral("input-uid-a"), QStringLiteral("input-uid-b")}; }
    QString inputInfo(quint32 line) override { return QStringLiteral("Input %1").arg(line); }
    bool openInput(quint32 input, quint32 universe) override
    {
        addToMap(universe, input, QLCIOPlugin::Input);
        return true;
    }
    void closeInput(quint32 input, quint32 universe) override
    {
        removeFromMap(universe, input, QLCIOPlugin::Input);
    }
    QStringList outputs() override { return {QStringLiteral("Output A"), QStringLiteral("Output B")}; }
    QStringList outputsUID() override { return {QStringLiteral("output-uid-a"), QStringLiteral("output-uid-b")}; }
    QString outputInfo(quint32 line) override { return QStringLiteral("Output %1").arg(line); }
    bool openOutput(quint32 output, quint32 universe) override
    {
        addToMap(universe, output, QLCIOPlugin::Output);
        return true;
    }
    void closeOutput(quint32 output, quint32 universe) override
    {
        removeFromMap(universe, output, QLCIOPlugin::Output);
    }
    void writeUniverse(quint32, quint32, const QByteArray &, bool) override {}
    void setParameter(quint32 universe, quint32 line, Capability type, QString name, QVariant value) override
    {
        ++setParameterCalls;
        QLCIOPlugin::setParameter(universe, line, type, name, value);
    }

    int setParameterCalls = 0;

private:
    QString m_name;
};

OutlineRoutePlugin *installOutlineRoutePlugin(Doc *doc, const QString &name)
{
    auto *plugin = new OutlineRoutePlugin(name);
    doc->ioPluginCache()->m_plugins.append(plugin);
    return plugin;
}
}

/** A refused save opens the modal save error popup; a test that refuses one on purpose closes it */
static void closeSaveErrorPopup(QQuickItem *root)
{
    QObject *popup = root ? root->findChild<QObject *>(QStringLiteral("saveErrorPopup")) : nullptr;
    if (popup)
        QMetaObject::invokeMethod(popup, "close");
}

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
    QTest::newRow("descendant-frame") << QString("descendant-frame") << false;
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
    quint32 descendantID = VCWidget::invalidId();
    if (operation == "descendant-frame")
    {
        auto *descendant = new VCFrame(m_app->doc(), vc, source);
        vc->addWidgetToMap(descendant);
        source->addWidgetToPageMap(descendant);
        descendant->setShowHeader(false);
        descendant->setLogicalGeometry(QRectF(0, 200, 300, 150));
        descendantID = descendant->id();
    }
    const QVariantList beforeOrder = source->orderedChildIDs(0);
    QCOMPARE(beforeOrder, (operation == "descendant-frame" ? QVariantList{first->id(), second->id(), descendantID}
                                                           : QVariantList{first->id(), second->id()}));
    const QRectF beforeFirst = first->geometry();
    const QRectF beforeSecond = second->geometry();
    m_app->doc()->resetModified();
    QByteArray beforeXML;
    QXmlStreamWriter beforeWriter(&beforeXML);
    vc->saveXML(&beforeWriter);
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
    else if (operation == "descendant-frame")
    {
        ids = {sourceID, first->id()};
        destination = descendantID;
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
        if (operation == "descendant-frame")
            QVERIFY(result.value("error").toString().contains("descendant"));
        QCOMPARE(first->parent(), source);
        QCOMPARE(second->parent(), source);
        QCOMPARE(source->orderedChildIDs(0), beforeOrder);
        QByteArray afterXML;
        QXmlStreamWriter afterWriter(&afterXML);
        vc->saveXML(&afterWriter);
        QCOMPARE(afterXML, beforeXML);
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

void UpstreamIntegration_Test::vcOutlineAssignmentSummary_data()
{
    QTest::addColumn<QString>("scopeKind");
    QTest::newRow("page") << QString("page");
    QTest::newRow("frame") << QString("frame");
    QTest::newRow("subpage") << QString("subpage");
    QTest::newRow("leaf") << QString("leaf");
}

void UpstreamIntegration_Test::vcOutlineAssignmentSummary()
{
    QFETCH(QString, scopeKind);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(2);
    const quint32 frameID = frame->id();
    auto *first = new VCButton(m_app->doc(), frame);
    auto *second = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(first);
    vc->addWidgetToMap(second);
    frame->addWidgetToPageMap(first);
    frame->addWidgetToPageMap(second);
    first->setPage(0);
    second->setPage(1);
    first->setCaption("Alpha");
    second->setCaption("Beta");
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        vc->outlineModel()->setDiagnosticsActive(false);
        vc->outlineModel()->setFilterText(QString());
        vc->outlineModel()->setInspectionScopeKey(QString());
    });

    vc->createAndAddInputSource(vc->page(0), 0, 9);
    vc->createAndAddInputSource(first, 0, 10);
    vc->createAndAddInputSource(first, 0, 11);
    vc->createAndAddInputSource(second, 0, (1u << 16) | 12u);

    VCOutlineModel *model = vc->outlineModel();
    model->setFilterText(QString());
    model->setDiagnosticsActive(true);

    const QString pageKey = QString("widget:%1").arg(vc->page(0)->id());
    const QString frameKey = QString("widget:%1").arg(frameID);
    const QString firstKey = QString("widget:%1").arg(first->id());
    const QString subpageKey = QString("subpage:%1:0").arg(frameID);
    const QString scopeKey =
        scopeKind == "page" ? pageKey : scopeKind == "frame" ? frameKey : scopeKind == "subpage" ? subpageKey : firstKey;
    QTRY_VERIFY(model->indexForKey(scopeKey).isValid());
    model->setInspectionScopeKey(scopeKey);
    QTRY_COMPARE(model->inspectionScopeKey(), scopeKey);

    QVariantMap summary;
    if (scopeKind == "page")
    {
        QTRY_COMPARE(model->assignmentSummary().value("inputBindingCount").toInt(), 4);
        summary = model->assignmentSummary();
        QCOMPARE(summary.value("inputBindingCount").toInt(), 4);
        QCOMPARE(summary.value("assignedWidgetCount").toInt(), 3);
        QCOMPARE(summary.value("ownInputBindingCount").toInt(), 1);
        QCOMPARE(summary.value("descendantInputBindingCount").toInt(), 3);
    }
    else if (scopeKind == "frame")
    {
        QTRY_COMPARE(model->assignmentSummary().value("inputBindingCount").toInt(), 3);
        summary = model->assignmentSummary();
        QCOMPARE(summary.value("inputBindingCount").toInt(), 3);
        QCOMPARE(summary.value("assignedWidgetCount").toInt(), 2);
        QCOMPARE(summary.value("ownInputBindingCount").toInt(), 0);
        QCOMPARE(summary.value("descendantInputBindingCount").toInt(), 3);
    }
    else if (scopeKind == "subpage")
    {
        QTRY_COMPARE(model->assignmentSummary().value("inputBindingCount").toInt(), 2);
        summary = model->assignmentSummary();
        QCOMPARE(summary.value("inputBindingCount").toInt(), 2);
        QCOMPARE(summary.value("assignedWidgetCount").toInt(), 1);
        QCOMPARE(summary.value("ownInputBindingCount").toInt(), 0);
        QCOMPARE(summary.value("descendantInputBindingCount").toInt(), 2);
    }
    else
    {
        QTRY_COMPARE(model->assignmentSummary().value("inputBindingCount").toInt(), 2);
        summary = model->assignmentSummary();
        QCOMPARE(summary.value("inputBindingCount").toInt(), 2);
        QCOMPARE(summary.value("assignedWidgetCount").toInt(), 1);
        QCOMPARE(summary.value("ownInputBindingCount").toInt(), 2);
        QCOMPARE(summary.value("descendantInputBindingCount").toInt(), 0);
    }

    QAbstractItemModel *rows = model->assignmentRows();
    QVERIFY(rows != nullptr);
    QVERIFY(rows->rowCount() > 0);
    for (int index = 0; index < rows->rowCount(); ++index)
    {
        const QVariantMap row = rows->index(index, 0).data().toMap();
        QVERIFY(row.contains("sourceChannelRaw"));
        QVERIFY(row.contains("inputRouteAvailability"));
        QCOMPARE(row.value("inputRouteAvailability").toString(), QString("unresolved"));
        QVERIFY(row.contains("deviceGroupKey"));
        QVERIFY(row.contains("deviceGroupLabel"));
        QVERIFY(row.contains("deviceGroupStart"));
    }

    const QVariantMap beforeFilter = model->assignmentSummary();
    model->setFilterText("Alpha");
    QTRY_VERIFY(model->indexForKey(firstKey).isValid());
    QCOMPARE(model->inspectionScopeKey(), scopeKey);
    QCOMPARE(model->assignmentSummary(), beforeFilter);
}

void UpstreamIntegration_Test::vcOutlineScopeFallback_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::newRow("subpage-removed") << QString("subpage-removed");
    QTest::newRow("widget-deleted") << QString("widget-deleted");
    QTest::newRow("reparent-then-delete") << QString("reparent-then-delete");
}

void UpstreamIntegration_Test::vcOutlineScopeFallback()
{
    QFETCH(QString, scenario);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(2);
    const quint32 frameID = frame->id();
    auto *child = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(child);
    frame->addWidgetToPageMap(child);
    child->setPage(1);
    const quint32 childID = child->id();
    VCFrame *target = nullptr;
    const auto cleanup = qScopeGuard([&]() {
        QVariantList ids{frameID};
        if (target != nullptr)
            ids.append(target->id());
        vc->deleteVCWidgets(ids);
        vc->outlineModel()->setDiagnosticsActive(false);
        vc->outlineModel()->setInspectionScopeKey(QString());
    });

    VCOutlineModel *model = vc->outlineModel();
    model->setDiagnosticsActive(true);
    QTRY_VERIFY(model->indexForKey(QString("widget:%1").arg(frameID)).isValid());
    QTRY_VERIFY(model->indexForKey(QString("widget:%1").arg(childID)).isValid());
    const QString frameKey = QString("widget:%1").arg(frameID);

    if (scenario == "subpage-removed")
    {
        model->setInspectionScopeKey(QString("subpage:%1:1").arg(frameID));
        QTRY_COMPARE(model->inspectionScopeKey(), QString("subpage:%1:1").arg(frameID));
        frame->setMultiPageMode(false);
        QTRY_COMPARE(model->inspectionScopeKey(), frameKey);
    }
    else
    {
        if (scenario == "widget-deleted")
        {
            model->setInspectionScopeKey(QString("widget:%1").arg(childID));
            QTRY_COMPARE(model->inspectionScopeKey(), QString("widget:%1").arg(childID));
            vc->deleteVCWidgets({childID});
            QTRY_COMPARE(model->inspectionScopeKey(), QString("subpage:%1:1").arg(frameID));
        }
        else
        {
            target = new VCFrame(m_app->doc(), vc, vc->page(0));
            vc->addWidgetToMap(target);
            vc->page(0)->addWidgetToPageMap(target);
            const quint32 targetID = target->id();
            QTRY_VERIFY(model->indexForKey(QString("widget:%1").arg(targetID)).isValid());
            QVERIFY(vc->reparentWidget(child, target));
            QCoreApplication::processEvents();
            model->setInspectionScopeKey(QString("widget:%1").arg(childID));
            QTRY_COMPARE(model->inspectionScopeKey(), QString("widget:%1").arg(childID));
            vc->deleteVCWidgets({childID});
            QTRY_COMPARE(model->inspectionScopeKey(), QString("widget:%1").arg(targetID));
        }
    }
}

void UpstreamIntegration_Test::vcOutlineRouteResolution_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::newRow("route-facts") << QString("route-facts");
    QTest::newRow("poll-route-change") << QString("poll-route-change");
    QTest::newRow("filtered-group-start") << QString("filtered-group-start");
    QTest::newRow("direction-specific-feedback") << QString("direction-specific-feedback");
    QTest::newRow("shared-across-slots") << QString("shared-across-slots");
    QTest::newRow("search-output-device") << QString("search-output-device");
    QTest::newRow("search-widget-type") << QString("search-widget-type");
    QTest::newRow("non-midi-message") << QString("non-midi-message");
    QTest::newRow("midi-plugin-no-profile-decodes") << QString("midi-plugin-no-profile-decodes");
    QTest::newRow("non-midi-plugin-midi-profile-no-decode") << QString("non-midi-plugin-midi-profile-no-decode");
    QTest::newRow("malformed-midi-channel-raw-retained") << QString("malformed-midi-channel-raw-retained");
    QTest::newRow("midi-input-non-midi-feedback") << QString("midi-input-non-midi-feedback");
    QTest::newRow("non-midi-input-midi-feedback") << QString("non-midi-input-midi-feedback");
    QTest::newRow("omni-packed-nonzero-subpage-label") << QString("omni-packed-nonzero-subpage-label");
    QTest::newRow("stable-values-role") << QString("stable-values-role");
}

void UpstreamIntegration_Test::vcOutlineRouteResolution()
{
    QFETCH(QString, scenario);
    VirtualConsole *vc = m_app->virtualConsole();
    VCOutlineModel *model = vc->outlineModel();
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    const quint32 frameID = frame->id();

    auto *alpha = new VCButton(m_app->doc(), frame);
    auto *beta = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(alpha);
    vc->addWidgetToMap(beta);
    frame->addWidgetToPageMap(alpha);
    frame->addWidgetToPageMap(beta);
    alpha->setCaption("Alpha");
    beta->setCaption("Beta");

    InputOutputMap *ioMap = m_app->doc()->inputOutputMap();
    const QString midiPluginName = QStringLiteral("Outline MIDI %1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString oscPluginName = QStringLiteral("Outline OSC %1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    OutlineRoutePlugin *midiPlugin = installOutlineRoutePlugin(m_app->doc(), midiPluginName);
    OutlineRoutePlugin *oscPlugin = installOutlineRoutePlugin(m_app->doc(), oscPluginName);

    const QString profileSuffix = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto *midiProfile = new QLCInputProfile();
    midiProfile->setManufacturer(QStringLiteral("Tests"));
    midiProfile->setModel(QStringLiteral("MIDI Surface %1").arg(profileSuffix));
    midiProfile->setType(QLCInputProfile::MIDI);
    auto *midiChannel = new QLCInputChannel();
    midiChannel->setName(QStringLiteral("Fader 8"));
    midiProfile->insertChannel(7, midiChannel);
    QVERIFY(ioMap->addProfile(midiProfile));

    auto *oscProfile = new QLCInputProfile();
    oscProfile->setManufacturer(QStringLiteral("Tests"));
    oscProfile->setModel(QStringLiteral("OSC Surface %1").arg(profileSuffix));
    oscProfile->setType(QLCInputProfile::OSC);
    auto *oscChannel = new QLCInputChannel();
    oscChannel->setName(QStringLiteral("OSC 1"));
    oscProfile->insertChannel(12, oscChannel);
    QVERIFY(ioMap->addProfile(oscProfile));

    const auto cleanup = qScopeGuard([&]() {
        model->setDiagnosticsActive(false);
        model->setFilterText(QString());
        model->setInspectionScopeKey(QString());
        vc->deleteVCWidgets({frameID});
    });

    QString input0Plugin = midiPluginName;
    QString input0ProfileName = midiProfile->name();
    QString output0Plugin = midiPluginName;
    quint32 output0Line = 1;
    QString input1ProfileName = oscProfile->name();

    if (scenario == "midi-plugin-no-profile-decodes")
        input0ProfileName.clear();
    else if (scenario == "non-midi-plugin-midi-profile-no-decode")
        input1ProfileName = midiProfile->name();
    else if (scenario == "midi-input-non-midi-feedback")
    {
        output0Plugin = oscPluginName;
        output0Line = 0;
    }
    else if (scenario == "non-midi-input-midi-feedback")
    {
        input0Plugin = oscPluginName;
        input0ProfileName = oscProfile->name();
    }

    QVERIFY(ioMap->setInputPatch(0, input0Plugin, QString(), QString(), 0, input0ProfileName));
    QVERIFY(ioMap->setOutputPatch(0, output0Plugin, QString(), QString(), output0Line, true));
    InputPatch *inputPatch = ioMap->inputPatch(0);
    QVERIFY(inputPatch != nullptr);
    const QVariant malformedMidiChannel = QStringLiteral("bad");
    inputPatch->setPluginParameter(QStringLiteral("midichannel"),
                                   scenario == "malformed-midi-channel-raw-retained" ? malformedMidiChannel : QVariant(16));
    OutputPatch *feedbackPatch = ioMap->feedbackPatch(0);
    QVERIFY(feedbackPatch != nullptr);
    const QVariant malformedOutputMidiChannel = QStringLiteral("broken");
    feedbackPatch->setPluginParameter(QStringLiteral("midichannel"),
                                      scenario == "malformed-midi-channel-raw-retained" ? malformedOutputMidiChannel : QVariant(3));

    QVERIFY(ioMap->setInputPatch(1, oscPluginName, QString(), QString(), 1, input1ProfileName));

    const quint32 midiRawChannel = (2u << 12) | 7u;
    vc->createAndAddInputSource(alpha, 0, midiRawChannel);
    vc->createAndAddInputSource(beta, 1, 12);
    vc->createAndAddInputSource(beta, 99, 42);
    const QList<QSharedPointer<QLCInputSource>> alphaSources = alpha->inputSources();
    QVERIFY(!alphaSources.isEmpty());
    QSharedPointer<QLCInputSource> midiSource = alphaSources.first();
    QVERIFY(!midiSource.isNull());
    midiSource->setFeedbackExtraParams(QLCInputFeedback::UpperValue, 2);

    model->setDiagnosticsActive(true);
    const QString frameKey = QStringLiteral("widget:%1").arg(frameID);
    QTRY_VERIFY(model->indexForKey(frameKey).isValid());
    model->setInspectionScopeKey(frameKey);
    QTRY_COMPARE(model->inspectionScopeKey(), frameKey);

    auto rowsByUniverse = [&]() {
        QHash<int, QVariantMap> rows;
        QAbstractItemModel *rowsModel = model->assignmentRows();
        for (int rowIndex = 0; rowIndex < rowsModel->rowCount(); ++rowIndex)
        {
            const QVariantMap row = rowsModel->index(rowIndex, 0).data().toMap();
            rows.insert(row.value(QStringLiteral("sourceUniverse")).toInt(), row);
        }
        return rows;
    };

    if (scenario == "route-facts")
    {
        QHash<int, QVariantMap> rows;
        QTRY_VERIFY((rows = rowsByUniverse()).contains(0));
        QVERIFY(rows.contains(1));
        QVERIFY(rows.contains(99));

        const QVariantMap midiRow = rows.value(0);
        QCOMPARE(midiRow.value(QStringLiteral("inputRouteAvailability")).toString(), QStringLiteral("resolved"));
        QVERIFY(!midiRow.value(QStringLiteral("inputDeviceName")).toString().isEmpty());
        QCOMPARE(midiRow.value(QStringLiteral("inputPlugin")).toString(), midiPluginName);
        QCOMPARE(midiRow.value(QStringLiteral("inputLine")).toInt(), 0);
        QVERIFY(!midiRow.value(QStringLiteral("inputUID")).toString().isEmpty());
        QVERIFY(!midiRow.value(QStringLiteral("outputDeviceName")).toString().isEmpty());
        QCOMPARE(midiRow.value(QStringLiteral("outputPlugin")).toString(), midiPluginName);
        QCOMPARE(midiRow.value(QStringLiteral("outputLine")).toInt(), 1);
        QVERIFY(!midiRow.value(QStringLiteral("outputUID")).toString().isEmpty());
        QCOMPARE(midiRow.value(QStringLiteral("midiRouteResolved")).toBool(), true);
        QVERIFY(midiRow.value(QStringLiteral("midiSelectionState")).toString() != QStringLiteral("unknown"));
        QCOMPARE(midiRow.value(QStringLiteral("sourceLabel")).toString().contains(QStringLiteral("Fader 8")), true);
        QVERIFY(midiRow.value(QStringLiteral("inputParameters")).canConvert<QVariantMap>());
        QVERIFY(midiRow.value(QStringLiteral("outputParameters")).canConvert<QVariantMap>());

        const QVariantMap oscRow = rows.value(1);
        QCOMPARE(oscRow.value(QStringLiteral("inputRouteAvailability")).toString(), QStringLiteral("resolved"));
        QCOMPARE(oscRow.value(QStringLiteral("inputPlugin")).toString(), oscPluginName);
        QCOMPARE(oscRow.value(QStringLiteral("midiRouteResolved")).toBool(), false);
        QCOMPARE(oscRow.value(QStringLiteral("midiSelectionState")).toString(), QStringLiteral("unknown"));

        const QVariantMap invalidRow = rows.value(99);
        QCOMPARE(invalidRow.value(QStringLiteral("inputRouteAvailability")).toString(), QStringLiteral("invalidUniverse"));
        QCOMPARE(invalidRow.value(QStringLiteral("deviceGroupLabel")).toString(), QStringLiteral("Invalid universe U100"));
    }
    else if (scenario == "poll-route-change")
    {
        QHash<int, QVariantMap> rows;
        QTRY_VERIFY((rows = rowsByUniverse()).contains(0));
        const QString initialUid = rows.value(0).value(QStringLiteral("inputUID")).toString();
        QVERIFY(ioMap->setInputPatch(0, midiPluginName, QString(), QString(), 1, midiProfile->name()));
        QTRY_VERIFY((rows = rowsByUniverse()).value(0).value(QStringLiteral("inputUID")).toString() != initialUid);
    }
    else
    {
        if (scenario == "filtered-group-start")
        {
            vc->createAndAddInputSource(beta, 0, midiRawChannel + 1);
            QTRY_VERIFY(model->assignmentRows()->rowCount() >= 3);
            model->setFilterText(QStringLiteral("Beta"));
            QTRY_VERIFY(model->assignmentRows()->rowCount() >= 1);
            bool checked = false;
            for (int rowIndex = 0; rowIndex < model->assignmentRows()->rowCount(); ++rowIndex)
            {
                const QVariantMap row = model->assignmentRows()->index(rowIndex, 0).data().toMap();
                if (row.value(QStringLiteral("widgetPath")).toString().contains(QStringLiteral("Beta")) &&
                    row.value(QStringLiteral("sourceUniverse")).toInt() == 0)
                {
                    QCOMPARE(row.value(QStringLiteral("deviceGroupStart")).toBool(), true);
                    checked = true;
                    break;
                }
            }
            QVERIFY(checked);
        }
        else if (scenario == "direction-specific-feedback")
        {
            QHash<int, QVariantMap> rows;
            QTRY_VERIFY((rows = rowsByUniverse()).contains(0));
            const QVariantMap midiRow = rows.value(0);
            QCOMPARE(midiRow.value(QStringLiteral("midiSelectionState")).toString(), QStringLiteral("omni"));
            QCOMPARE(midiRow.value(QStringLiteral("midiFeedbackUpperState")).toString(), QStringLiteral("fixed"));
            QCOMPARE(midiRow.value(QStringLiteral("midiFeedbackUpperChannelIndex")).toInt(), 5);
            QCOMPARE(midiRow.value(QStringLiteral("midiFeedbackUpperLabel")).toString(), QStringLiteral("Channel 6"));
            QCOMPARE(midiRow.value(QStringLiteral("midiFeedbackState")).toString(), QStringLiteral("unknown"));
            QVERIFY(midiRow.value(QStringLiteral("midiSelectionLabel")).toString().startsWith(QStringLiteral("OMNI")));
        }
        else if (scenario == "shared-across-slots")
        {
            QSharedPointer<QLCInputSource> sameAddressDifferentSlot(new QLCInputSource());
            sameAddressDifferentSlot->setID(1);
            sameAddressDifferentSlot->setUniverse(0);
            sameAddressDifferentSlot->setChannel(midiRawChannel);
            alpha->addInputSource(sameAddressDifferentSlot);
            QTRY_VERIFY(model->assignmentRows()->rowCount() >= 4);
            int sharedRows = 0;
            for (int rowIndex = 0; rowIndex < model->assignmentRows()->rowCount(); ++rowIndex)
            {
                const QVariantMap row = model->assignmentRows()->index(rowIndex, 0).data().toMap();
                if (row.value(QStringLiteral("sourceUniverse")).toInt() == 0 &&
                    row.value(QStringLiteral("sourceChannelRaw")).toUInt() == midiRawChannel)
                {
                    QVERIFY(row.value(QStringLiteral("sharedAssignment")).toBool());
                    QCOMPARE(row.value(QStringLiteral("sharedAssignmentLabel")).toString(),
                             QStringLiteral("Shared physical input address"));
                    ++sharedRows;
                }
            }
            QCOMPARE(sharedRows, 2);
        }
        else if (scenario == "search-output-device")
        {
            QHash<int, QVariantMap> rows;
            QTRY_VERIFY((rows = rowsByUniverse()).contains(0));
            const QString outputName = rows.value(0).value(QStringLiteral("outputDeviceName")).toString();
            QVERIFY(!outputName.isEmpty());
            model->setFilterText(outputName);
            QTRY_VERIFY(model->assignmentRows()->rowCount() >= 1);
        }
        else if (scenario == "search-widget-type")
        {
            model->setFilterText(QStringLiteral("Button"));
            QTRY_VERIFY(model->assignmentRows()->rowCount() >= 1);
        }
        else if (scenario == "non-midi-message")
        {
            QHash<int, QVariantMap> rows;
            QTRY_VERIFY((rows = rowsByUniverse()).contains(1));
            const QVariantMap oscRow = rows.value(1);
            QCOMPARE(oscRow.value(QStringLiteral("midiRouteResolved")).toBool(), false);
            QCOMPARE(oscRow.value(QStringLiteral("midiMessageType")).toString(), QStringLiteral("unknown"));
            QCOMPARE(oscRow.value(QStringLiteral("midiMessageLabel")).toString(), QStringLiteral("Unknown"));
            QCOMPARE(oscRow.value(QStringLiteral("midiSelectionState")).toString(), QStringLiteral("unknown"));
        }
        else if (scenario == "midi-plugin-no-profile-decodes")
        {
            QHash<int, QVariantMap> rows;
            QTRY_VERIFY((rows = rowsByUniverse()).contains(0));
            const QVariantMap row = rows.value(0);
            QCOMPARE(row.value(QStringLiteral("inputPlugin")).toString(), midiPluginName);
            QCOMPARE(row.value(QStringLiteral("inputProfileName")).toString(), QStringLiteral("None"));
            QCOMPARE(row.value(QStringLiteral("midiRouteResolved")).toBool(), true);
            QCOMPARE(row.value(QStringLiteral("midiMessageType")).toString(), QStringLiteral("controlChange"));
            QVERIFY(row.value(QStringLiteral("midiMessageLabel")).toString() != QStringLiteral("Unknown"));
        }
        else if (scenario == "non-midi-plugin-midi-profile-no-decode")
        {
            QHash<int, QVariantMap> rows;
            QTRY_VERIFY((rows = rowsByUniverse()).contains(1));
            const QVariantMap row = rows.value(1);
            QCOMPARE(row.value(QStringLiteral("inputPlugin")).toString(), oscPluginName);
            QCOMPARE(row.value(QStringLiteral("inputProfileName")).toString(), midiProfile->name());
            QCOMPARE(row.value(QStringLiteral("midiRouteResolved")).toBool(), false);
            QCOMPARE(row.value(QStringLiteral("midiMessageType")).toString(), QStringLiteral("unknown"));
            QCOMPARE(row.value(QStringLiteral("midiMessageLabel")).toString(), QStringLiteral("Unknown"));
        }
        else if (scenario == "malformed-midi-channel-raw-retained")
        {
            QHash<int, QVariantMap> rows;
            QTRY_VERIFY((rows = rowsByUniverse()).contains(0));
            const QVariantMap row = rows.value(0);
            QCOMPARE(row.value(QStringLiteral("midiRouteResolved")).toBool(), true);
            QCOMPARE(row.value(QStringLiteral("midiParameterPresent")).toBool(), true);
            QCOMPARE(row.value(QStringLiteral("midiParameterValid")).toBool(), false);
            QCOMPARE(row.value(QStringLiteral("midiParameterValue")).toString(), malformedMidiChannel.toString());
            QCOMPARE(row.value(QStringLiteral("midiSelectionState")).toString(), QStringLiteral("invalid"));
            QCOMPARE(row.value(QStringLiteral("midiOutputParameterPresent")).toBool(), true);
            QCOMPARE(row.value(QStringLiteral("midiOutputParameterValid")).toBool(), false);
            QCOMPARE(row.value(QStringLiteral("midiOutputParameterValue")).toString(), malformedOutputMidiChannel.toString());
            QCOMPARE(row.value(QStringLiteral("midiOutputSelectionState")).toString(), QStringLiteral("invalid"));
        }
        else if (scenario == "midi-input-non-midi-feedback")
        {
            QHash<int, QVariantMap> rows;
            QTRY_VERIFY((rows = rowsByUniverse()).contains(0));
            const QVariantMap row = rows.value(0);
            QCOMPARE(row.value(QStringLiteral("midiRouteResolved")).toBool(), true);
            QCOMPARE(row.value(QStringLiteral("midiOutputRouteResolved")).toBool(), false);
            QCOMPARE(row.value(QStringLiteral("midiMessageType")).toString(), QStringLiteral("controlChange"));
            QCOMPARE(row.value(QStringLiteral("midiFeedbackLowerState")).toString(), QStringLiteral("unknown"));
            QCOMPARE(row.value(QStringLiteral("midiFeedbackUpperState")).toString(), QStringLiteral("unknown"));
            QCOMPARE(row.value(QStringLiteral("midiFeedbackMonitorState")).toString(), QStringLiteral("unknown"));
        }
        else if (scenario == "non-midi-input-midi-feedback")
        {
            QHash<int, QVariantMap> rows;
            QTRY_VERIFY((rows = rowsByUniverse()).contains(0));
            const QVariantMap row = rows.value(0);
            QCOMPARE(row.value(QStringLiteral("midiRouteResolved")).toBool(), false);
            QCOMPARE(row.value(QStringLiteral("midiOutputRouteResolved")).toBool(), true);
            QCOMPARE(row.value(QStringLiteral("midiMessageType")).toString(), QStringLiteral("unknown"));
            QCOMPARE(row.value(QStringLiteral("midiOutputSelectionState")).toString(), QStringLiteral("fixed"));
            QCOMPARE(row.value(QStringLiteral("midiOutputSelectionLabel")).toString(), QStringLiteral("Channel 4"));
        }
        else if (scenario == "omni-packed-nonzero-subpage-label")
        {
            const quint32 omniPackedWithSubpage = (5u << 16) | (3u << 12) | 7u;
            vc->createAndAddInputSource(alpha, 0, omniPackedWithSubpage);
            QTRY_VERIFY(model->assignmentRows()->rowCount() >= 4);
            QVariantMap omniRow;
            for (int rowIndex = 0; rowIndex < model->assignmentRows()->rowCount(); ++rowIndex)
            {
                const QVariantMap row = model->assignmentRows()->index(rowIndex, 0).data().toMap();
                if (row.value(QStringLiteral("sourceChannelRaw")).toUInt() == omniPackedWithSubpage)
                {
                    omniRow = row;
                    break;
                }
            }
            QVERIFY(!omniRow.isEmpty());
            QCOMPARE(omniRow.value(QStringLiteral("midiSelectionState")).toString(), QStringLiteral("omni"));
            QCOMPARE(omniRow.value(QStringLiteral("midiPackedChannelIndex")).toInt(), 3);
            QVERIFY(omniRow.value(QStringLiteral("midiSelectionLabel")).toString().contains(QStringLiteral("assigned channel 4")));
            QVERIFY(!omniRow.value(QStringLiteral("midiSelectionLabel")).toString().contains(QStringLiteral("assigned channel 1")));
        }
        else
        {
            QAbstractItemModel *rowsModel = model->assignmentRows();
            QVERIFY(rowsModel != nullptr);
            auto valuesRoleName = [&]() {
                return QString::fromLatin1(rowsModel->roleNames().value(Qt::UserRole + 1));
            };
            QTRY_VERIFY(rowsModel->rowCount() > 0);
            QCOMPARE(valuesRoleName(), QStringLiteral("values"));
            QVERIFY(rowsModel->index(0, 0).data(Qt::UserRole + 1).canConvert<QVariantMap>());
            model->setFilterText(QStringLiteral("No matching route token"));
            QTRY_COMPARE(rowsModel->rowCount(), 0);
            QCOMPARE(valuesRoleName(), QStringLiteral("values"));
            model->setFilterText(QString());
            QTRY_VERIFY(rowsModel->rowCount() > 0);
            QCOMPARE(valuesRoleName(), QStringLiteral("values"));
        }
    }

    QVERIFY(midiPlugin->setParameterCalls > 0);
    const int writesBefore = midiPlugin->setParameterCalls + oscPlugin->setParameterCalls;
    QTest::qWait(1100);
    QCoreApplication::processEvents();
    QCOMPARE(midiPlugin->setParameterCalls + oscPlugin->setParameterCalls, writesBefore);
}

void UpstreamIntegration_Test::vcOutlineAssignmentPaneLifecycle()
{
    VirtualConsole *vc = m_app->virtualConsole();
    VCOutlineModel *model = vc->outlineModel();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);

    const QString oldContext = contexts->currentContext();
    const bool oldVisible = m_app->isVisible();
    const QSize oldSize = m_app->size();
    const bool oldEdit = vc->editMode();
    const int oldMask = m_app->accessMask();
    const QString oldFilter = model->filterText();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    const bool wasModified = m_app->doc()->isModified();
    const int oldPagePin = vc->page(0)->PIN();
    InputOutputMap *ioMap = m_app->doc()->inputOutputMap();
    InputPatch *oldInputPatch = ioMap->inputPatch(0);
    const QString oldInputPlugin = oldInputPatch ? oldInputPatch->pluginName() : QStringLiteral("None");
    const QString oldInputUid = oldInputPatch ? oldInputPatch->inputUID() : QString();
    const QString oldInputName = oldInputPatch ? oldInputPatch->inputName() : QString();
    const quint32 oldInputLine = oldInputPatch ? oldInputPatch->input() : 0;
    const QString oldInputProfile = oldInputPatch ? oldInputPatch->profileName() : QString();
    const QMap<QString, QVariant> oldInputParameters =
        oldInputPatch ? oldInputPatch->getPluginParameters() : QMap<QString, QVariant>();
    OutputPatch *oldFeedbackPatch = ioMap->feedbackPatch(0);
    const QString oldFeedbackPlugin = oldFeedbackPatch ? oldFeedbackPatch->pluginName() : QStringLiteral("None");
    const QString oldFeedbackUid = oldFeedbackPatch ? oldFeedbackPatch->outputUID() : QString();
    const QString oldFeedbackName = oldFeedbackPatch ? oldFeedbackPatch->outputName() : QString();
    const quint32 oldFeedbackLine = oldFeedbackPatch ? oldFeedbackPatch->output() : 0;
    const QMap<QString, QVariant> oldFeedbackParameters =
        oldFeedbackPatch ? oldFeedbackPatch->getPluginParameters() : QMap<QString, QVariant>();

    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    const quint32 frameID = frame->id();
    auto *button = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setCaption(QString("Alpha Route with wrapped route identity ").repeated(8));
    const quint32 buttonID = button->id();

    const QString pluginName = QStringLiteral("Outline Lifecycle MIDI %1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    installOutlineRoutePlugin(m_app->doc(), pluginName);

    const QString profileSuffix = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto *profile = new QLCInputProfile();
    profile->setManufacturer(QStringLiteral("Tests"));
    profile->setModel(QStringLiteral("MIDI Surface %1").arg(profileSuffix));
    profile->setType(QLCInputProfile::MIDI);
    auto *channel = new QLCInputChannel();
    channel->setName(QStringLiteral("Fader 7"));
    profile->insertChannel(6, channel);
    QVERIFY(ioMap->addProfile(profile));
    QVERIFY(ioMap->setInputPatch(0, pluginName, QString(), QString(), 0, profile->name()));
    QVERIFY(ioMap->setOutputPatch(0, pluginName, QString(), QString(), 1, true));

    const quint32 midiRawChannel = (3u << 12) | 6u;
    vc->createAndAddInputSource(button, 0, midiRawChannel);
    QSharedPointer<QLCInputSource> sharedAddress(new QLCInputSource());
    sharedAddress->setID(1);
    sharedAddress->setUniverse(0);
    sharedAddress->setChannel(midiRawChannel);
    button->addInputSource(sharedAddress);

    QQmlComponent component(m_app->engine(), QUrl("qrc:/VirtualConsole.qml"));
    std::unique_ptr<QObject> object(component.create(qmlContext(m_app->rootObject())));
    auto *console = qobject_cast<QQuickItem *>(object.get());
    QVERIFY2(console, qPrintable(component.errorString()));
    console->setParentItem(m_app->rootObject());
    console->setWidth(1280);
    console->setHeight(820);
    QVERIFY(console->setProperty("outlineOpen", true));

    const auto cleanup = qScopeGuard([&]() {
        model->setInspectionScopeKey(QString());
        model->setFilterText(oldFilter);
        model->setDiagnosticsActive(false);
        vc->setSelectedWidgetIDs(oldSelection);
        vc->setEditMode(oldEdit);
        vc->page(0)->setPIN(oldPagePin);
        m_app->setAccessMask(oldMask);
        ioMap->setInputPatch(0, oldInputPlugin, oldInputUid, oldInputName, oldInputLine, oldInputProfile);
        InputPatch *restoredInputPatch = ioMap->inputPatch(0);
        if (restoredInputPatch != nullptr)
        {
            const QMap<QString, QVariant> currentInputParameters = restoredInputPatch->getPluginParameters();
            for (auto it = currentInputParameters.begin(); it != currentInputParameters.end(); ++it)
            {
                if (!oldInputParameters.contains(it.key()))
                    restoredInputPatch->setPluginParameter(it.key(), QVariant());
            }
            for (auto it = oldInputParameters.begin(); it != oldInputParameters.end(); ++it)
                restoredInputPatch->setPluginParameter(it.key(), it.value());
        }
        ioMap->setOutputPatch(0, oldFeedbackPlugin, oldFeedbackUid, oldFeedbackName, oldFeedbackLine, true);
        OutputPatch *restoredFeedbackPatch = ioMap->feedbackPatch(0);
        if (restoredFeedbackPatch != nullptr)
        {
            const QMap<QString, QVariant> currentFeedbackParameters = restoredFeedbackPatch->getPluginParameters();
            for (auto it = currentFeedbackParameters.begin(); it != currentFeedbackParameters.end(); ++it)
            {
                if (!oldFeedbackParameters.contains(it.key()))
                    restoredFeedbackPatch->setPluginParameter(it.key(), QVariant());
            }
            for (auto it = oldFeedbackParameters.begin(); it != oldFeedbackParameters.end(); ++it)
                restoredFeedbackPatch->setPluginParameter(it.key(), it.value());
        }
        vc->deleteVCWidgets({frameID});
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified)
            m_app->doc()->resetModified();
    });

    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1280, 820);
    m_app->setAccessMask(oldMask | App::AC_VCEditing);
    contexts->switchToContext("VC");
    vc->setEditMode(false);
    vc->setSelectedWidgetIDs({});
    m_app->doc()->resetModified();

    auto *outline = console->findChild<QQuickItem *>("vcOutlinePane");
    auto *assignmentPane = console->findChild<QQuickItem *>("vcAssignmentSummaryPane");
    auto *assignmentScroll = console->findChild<QQuickItem *>("vcAssignmentSummaryScroll");
    auto *totalsText = console->findChild<QQuickItem *>("vcAssignmentSummaryTotals");
    auto *search = console->findChild<QQuickItem *>("vcOutlineSearch");
    auto *errorDialog = console->findChild<QObject *>("vcOutlineError");
    QVERIFY(outline);
    QVERIFY(assignmentPane);
    QVERIFY(assignmentScroll);
    QVERIFY(totalsText);
    QVERIFY(search);
    QVERIFY(errorDialog);
    QTRY_VERIFY(outline->isVisible());
    QTRY_VERIFY(assignmentPane->isVisible());
    QTRY_VERIFY(outline->property("diagnosticsEligible").toBool());
    model->setDiagnosticsActive(true);

    const QString buttonKey = QStringLiteral("widget:%1").arg(buttonID);
    QTRY_VERIFY(model->indexForKey(buttonKey).isValid());
    QVERIFY(QMetaObject::invokeMethod(outline, "selectKey", Q_ARG(QVariant, buttonKey), Q_ARG(QVariant, 0)));
    QTRY_COMPARE(model->inspectionScopeKey(), buttonKey);
    QCOMPARE(vc->selectedWidgetIDs(), QVariantList());
    QVERIFY(!m_app->doc()->isModified());

    QVariantMap summary;
    QTRY_VERIFY((summary = model->assignmentSummary()).value("inputBindingCount").toInt() > 0);
    QCOMPARE(summary.value("assignedWidgetCount").toInt(), 1);
    QCOMPARE(summary.value("ownAssignedWidgetCount").toInt(), 1);
    QCOMPARE(summary.value("descendantAssignedWidgetCount").toInt(), 0);
    QCOMPARE(summary.value("ownInputBindingCount").toInt(), 2);
    QCOMPARE(summary.value("descendantInputBindingCount").toInt(), 0);

    const QString totalsLabel = totalsText->property("text").toString();
    QVERIFY(totalsLabel.contains("Widgets 1"));
    QVERIFY(totalsLabel.contains("Inputs 2"));
    QVERIFY(assignmentPane->width() > 0);

    const auto textItemContaining = [](QQuickItem *root, const QString &needle) -> QQuickItem * {
        if (root == nullptr)
            return nullptr;
        QList<QQuickItem *> pending{root};
        while (!pending.isEmpty())
        {
            QQuickItem *candidate = pending.takeLast();
            if (candidate->property("text").isValid() &&
                candidate->property("text").toString().contains(needle))
                return candidate;
            const auto children = candidate->childItems();
            for (QQuickItem *child : children)
                pending.append(child);
        }
        return nullptr;
    };

    auto rowForUniverse = [&](int universe) {
        QVariantMap row;
        QAbstractItemModel *rowsModel = model->assignmentRows();
        for (int index = 0; index < rowsModel->rowCount(); ++index)
        {
            const QVariantMap candidate = rowsModel->index(index, 0).data().toMap();
            if (candidate.value(QStringLiteral("sourceUniverse")).toInt() == universe)
            {
                row = candidate;
                break;
            }
        }
        return row;
    };

    QVariantMap midiRow;
    QTRY_VERIFY(!(midiRow = rowForUniverse(0)).isEmpty());
    QVERIFY(!midiRow.value(QStringLiteral("inputDeviceName")).toString().isEmpty());
    QCOMPARE(midiRow.value(QStringLiteral("inputPlugin")).toString(), pluginName);
    QCOMPARE(midiRow.value(QStringLiteral("deviceGroupStart")).toBool(), true);
    QVERIFY(!midiRow.value(QStringLiteral("deviceGroupLabel")).toString().isEmpty());
    QVERIFY(midiRow.value(QStringLiteral("sharedAssignment")).toBool());
    QCOMPARE(midiRow.value(QStringLiteral("sharedAssignmentLabel")).toString(),
             QStringLiteral("Shared physical input address"));

    QQuickItem *groupHeadingText = nullptr;
    QTRY_VERIFY((groupHeadingText = textItemContaining(
                    assignmentPane, midiRow.value(QStringLiteral("deviceGroupLabel")).toString())) != nullptr);
    QVERIFY(groupHeadingText->height() > 0);

    QQuickItem *widgetPathText = nullptr;
    QTRY_VERIFY((widgetPathText = textItemContaining(
                    assignmentPane, midiRow.value(QStringLiteral("widgetPath")).toString())) != nullptr);
    QVERIFY(widgetPathText->height() > 0);

    const auto findVisibleShowButton = [&](const QString &requiredInputUid) -> QQuickItem * {
        QList<QQuickItem *> pending{assignmentPane};
        while (!pending.isEmpty())
        {
            QQuickItem *candidate = pending.takeLast();
            if (candidate->objectName() == QStringLiteral("vcAssignmentShowWidget") &&
                candidate->isVisible() &&
                candidate->width() > 0 &&
                candidate->height() > 0 &&
                candidate->window() == m_app.get())
            {
                const QString candidateDetails = candidate->property("detailsText").toString();
                if (candidateDetails.contains(QStringLiteral("UID %1").arg(requiredInputUid)))
                    return candidate;
            }
            const auto children = candidate->childItems();
            for (QQuickItem *child : children)
                pending.append(child);
        }
        return nullptr;
    };

    QQuickItem *showButton = nullptr;
    QTRY_VERIFY((showButton = findVisibleShowButton(midiRow.value(QStringLiteral("inputUID")).toString())) != nullptr);
    const QString detailsText = showButton->property("detailsText").toString();
    QVERIFY(detailsText.contains(QStringLiteral("Input route:")));
    QVERIFY(detailsText.contains(QStringLiteral("UID %1").arg(midiRow.value(QStringLiteral("inputUID")).toString())));
    QVERIFY(detailsText.contains(QStringLiteral("Shared physical input address")));
    QVERIFY(detailsText.contains(midiRow.value(QStringLiteral("widgetPath")).toString()));
    QVERIFY(detailsText.contains(midiRow.value(QStringLiteral("midiMessageLabel")).toString()));
    QVERIFY(detailsText.contains(midiRow.value(QStringLiteral("midiSelectionLabel")).toString()));
    QVERIFY(detailsText.contains(midiRow.value(QStringLiteral("midiOutputSelectionLabel")).toString()));
    QVERIFY(detailsText.contains(midiRow.value(QStringLiteral("midiFeedbackLowerLabel")).toString()));
    QVERIFY(detailsText.contains(midiRow.value(QStringLiteral("midiFeedbackUpperLabel")).toString()));
    QVERIFY(detailsText.contains(midiRow.value(QStringLiteral("midiFeedbackMonitorLabel")).toString()));

    const auto focusAncestorByObjectName = [](QQuickItem *item, const QString &objectName) -> QQuickItem * {
        QQuickItem *current = item;
        while (current != nullptr)
        {
            if (current->objectName() == objectName)
                return current;
            current = current->parentItem();
        }
        return nullptr;
    };
    const auto tabStopsInSubtree = [](QQuickItem *root) -> int {
        if (root == nullptr)
            return 0;
        int count = 0;
        QList<QQuickItem *> pending{root};
        while (!pending.isEmpty())
        {
            QQuickItem *candidate = pending.takeLast();
            if (candidate->property("activeFocusOnTab").toBool() &&
                candidate->isVisible() &&
                candidate->width() > 0 &&
                candidate->height() > 0)
            {
                ++count;
            }
            const auto children = candidate->childItems();
            for (QQuickItem *child : children)
                pending.append(child);
        }
        return count;
    };
    const int maxTabSteps = qMax(8, tabStopsInSubtree(console) + 4);
    search->forceActiveFocus();
    QTRY_VERIFY(search->hasActiveFocus());

    QQuickItem *focusedAssignmentRow = nullptr;
    for (int step = 0; step < maxTabSteps && focusedAssignmentRow == nullptr; ++step)
    {
        QTest::keyClick(m_app.get(), Qt::Key_Tab);
        QCoreApplication::processEvents();
        QQuickItem *activeFocusItem = m_app->activeFocusItem();
        focusedAssignmentRow = focusAncestorByObjectName(activeFocusItem, QStringLiteral("vcAssignmentTableRow"));
    }
    QVERIFY2(focusedAssignmentRow != nullptr, "Tab traversal from search did not reach assignment row focus");
    QTRY_VERIFY(focusedAssignmentRow->property("activeFocus").toBool());
    const QString focusedDetails = focusedAssignmentRow->property("detailsText").toString();
    QVERIFY(focusedDetails.contains(QStringLiteral("Widget path:")));
    QVERIFY(focusedDetails.contains(midiRow.value(QStringLiteral("widgetPath")).toString()));
    QVERIFY(focusedDetails.contains(midiRow.value(QStringLiteral("midiMessageLabel")).toString()));
    QVERIFY(focusedDetails.contains(midiRow.value(QStringLiteral("midiSelectionLabel")).toString()));

    QQuickItem *focusedShowButton = nullptr;
    for (int step = 0; step < maxTabSteps && focusedShowButton == nullptr; ++step)
    {
        QTest::keyClick(m_app.get(), Qt::Key_Tab);
        QCoreApplication::processEvents();
        focusedShowButton = focusAncestorByObjectName(
            m_app->activeFocusItem(), QStringLiteral("vcAssignmentShowWidget"));
    }
    QVERIFY2(focusedShowButton != nullptr, "Tab traversal from assignment row did not reach Show button");
    QVERIFY(focusedShowButton->property("activeFocusOnTab").toBool());

    bool searchReachedByBacktab = false;
    for (int step = 0; step < maxTabSteps; ++step)
    {
        QTest::keyClick(m_app.get(), Qt::Key_Backtab, Qt::ShiftModifier);
        QCoreApplication::processEvents();
        if (search->hasActiveFocus())
        {
            searchReachedByBacktab = true;
            break;
        }
    }
    QVERIFY2(searchReachedByBacktab, "Shift+Tab traversal from table did not return focus to search");

    QVERIFY(search->setProperty("text", "Alpha Route"));
    QTRY_COMPARE(model->filterText(), QString("Alpha Route"));
    QTRY_COMPARE(model->assignmentRows()->rowCount(), 2);

    QVERIFY(assignmentScroll->isVisible());
    const QVariantMap rowBeforeIdlePoll = rowForUniverse(0);
    QVERIFY(!rowBeforeIdlePoll.value(QStringLiteral("assignmentKey")).toString().isEmpty());
    QTest::qWait(1200);
    QCoreApplication::processEvents();
    QCOMPARE(rowForUniverse(0).value(QStringLiteral("assignmentKey")).toString(),
             rowBeforeIdlePoll.value(QStringLiteral("assignmentKey")).toString());

    vc->page(0)->setPIN(1234);
    QCOMPARE(showButton->property("text").toString(), QStringLiteral("Show"));

    QQuickItem *showWidgetButton = nullptr;
    QTRY_VERIFY((showWidgetButton = findVisibleShowButton(rowBeforeIdlePoll.value(QStringLiteral("inputUID")).toString())) != nullptr);
    QVERIFY(showWidgetButton != nullptr);
    QVERIFY(showWidgetButton->window() == m_app.get());

    const QPointF buttonCenterScene = showWidgetButton->mapToScene(
        QPointF(showWidgetButton->width() / 2.0, showWidgetButton->height() / 2.0));
    QPointF buttonCenterInScroll = assignmentScroll->mapFromScene(buttonCenterScene);
    if (buttonCenterInScroll.y() < 0 || buttonCenterInScroll.y() > assignmentScroll->height())
    {
        const qreal targetY = assignmentScroll->property("contentY").toReal() +
                              buttonCenterInScroll.y() -
                              (assignmentScroll->height() / 2.0);
        assignmentScroll->setProperty("contentY", qMax<qreal>(0.0, targetY));
        QCoreApplication::processEvents();
        const QPointF recenteredScene = showWidgetButton->mapToScene(
            QPointF(showWidgetButton->width() / 2.0, showWidgetButton->height() / 2.0));
        buttonCenterInScroll = assignmentScroll->mapFromScene(recenteredScene);
    }

    const QPointF localClickF = showWidgetButton->mapToItem(
        m_app->rootObject(), QPointF(showWidgetButton->width() / 2.0, showWidgetButton->height() / 2.0));
    const QPoint localClick = localClickF.toPoint();
    QVERIFY(localClick.x() >= 0 && localClick.x() < m_app->width());
    QVERIFY(localClick.y() >= 0 && localClick.y() < m_app->height());
    QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, localClick);
    QTRY_COMPARE(search->property("text").toString(), QString());
    QTRY_VERIFY(errorDialog->property("visible").toBool());
    QVERIFY(!errorDialog->property("message").toString().isEmpty());
    QMetaObject::invokeMethod(errorDialog, "close");
    vc->page(0)->setPIN(0);

    const QString uidBeforeStop = rowForUniverse(0).value(QStringLiteral("inputUID")).toString();
    QVERIFY(!uidBeforeStop.isEmpty());
    m_app->setAccessMask(oldMask & ~App::AC_VCEditing);
    model->setDiagnosticsActive(false);
    QVERIFY(ioMap->setInputPatch(0, pluginName, QString(), QString(), 1, profile->name()));
    QTest::qWait(1200);
    QCoreApplication::processEvents();
    QCOMPARE(rowForUniverse(0).value(QStringLiteral("inputUID")).toString(), uidBeforeStop);

    m_app->setAccessMask(oldMask);
    QTRY_VERIFY(outline->property("diagnosticsEligible").toBool());
    model->setDiagnosticsActive(true);
    QTRY_VERIFY(rowForUniverse(0).value(QStringLiteral("inputUID")).toString() != uidBeforeStop);

    QVERIFY(console->setProperty("outlineOpen", false));
    QTRY_VERIFY(!outline->property("diagnosticsEligible").toBool());
    QVERIFY(console->setProperty("outlineOpen", true));
    QTRY_VERIFY(outline->property("diagnosticsEligible").toBool());

    const QString artifactsDir = qEnvironmentVariable("MINIME_ARTIFACTS_DIR");
    if (!artifactsDir.isEmpty())
    {
        QDir().mkpath(artifactsDir);
        const QString screenshotPath = artifactsDir + "/vc-assignment-summary-pane.png";
        const QImage image = m_app->grabWindow();
        image.save(screenshotPath);
    }
}

void UpstreamIntegration_Test::vcOutlineAssignmentSummaryClipping_data()
{
    QTest::addColumn<int>("appWidth");
    QTest::addColumn<int>("appHeight");
    QTest::addColumn<int>("outlineWidth");
    QTest::addColumn<bool>("expectHorizontalOverflow");
    QTest::newRow("default-pane") << 1656 << 970 << 226 << true;
    QTest::newRow("narrow-overflow-pane") << 1656 << 970 << 150 << true;
    QTest::newRow("wide-pane") << 1800 << 970 << 400 << false;
}

void UpstreamIntegration_Test::vcOutlineAssignmentSummaryClipping()
{
    QFETCH(int, appWidth);
    QFETCH(int, appHeight);
    QFETCH(int, outlineWidth);
    QFETCH(bool, expectHorizontalOverflow);

    VirtualConsole *vc = m_app->virtualConsole();
    VCOutlineModel *model = vc->outlineModel();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);

    const QString oldContext = contexts->currentContext();
    const bool oldVisible = m_app->isVisible();
    const QSize oldSize = m_app->size();
    const bool oldEdit = vc->editMode();
    const int oldMask = m_app->accessMask();
    const QString oldFilter = model->filterText();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    const bool wasModified = m_app->doc()->isModified();

    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(2);
    const quint32 frameID = frame->id();
    auto *first = new VCButton(m_app->doc(), frame);
    auto *second = new VCButton(m_app->doc(), frame);
    vc->addWidgetToMap(first);
    vc->addWidgetToMap(second);
    frame->addWidgetToPageMap(first);
    frame->addWidgetToPageMap(second);
    first->setPage(0);
    second->setPage(1);
    first->setCaption(QStringLiteral("EXP-Frau-Button-A"));
    second->setCaption(QStringLiteral("EXP-Frau-Button-B"));

    const auto cleanup = qScopeGuard([&]() {
        model->setInspectionScopeKey(QString());
        model->setFilterText(oldFilter);
        model->setDiagnosticsActive(false);
        vc->setSelectedWidgetIDs(oldSelection);
        vc->setEditMode(oldEdit);
        vc->deleteVCWidgets({frameID});
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified)
            m_app->doc()->resetModified();
    });

    InputOutputMap *ioMap = m_app->doc()->inputOutputMap();
    const QString pluginName = QStringLiteral("Outline Clip MIDI %1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    installOutlineRoutePlugin(m_app->doc(), pluginName);

    auto *profile = new QLCInputProfile();
    profile->setManufacturer(QStringLiteral("Tests"));
    profile->setModel(QStringLiteral("Clipping Surface %1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    profile->setType(QLCInputProfile::MIDI);
    QVERIFY(ioMap->addProfile(profile));

    QVERIFY(ioMap->setInputPatch(0, pluginName, QString(), QString(), 0, profile->name()));
    QVERIFY(ioMap->setOutputPatch(0, pluginName, QString(), QString(), 1, true));
    InputPatch *inputPatch = ioMap->inputPatch(0);
    QVERIFY(inputPatch != nullptr);
    inputPatch->setPluginParameter(QStringLiteral("midichannel"), QVariant(16));
    OutputPatch *feedbackPatch = ioMap->feedbackPatch(0);
    QVERIFY(feedbackPatch != nullptr);
    feedbackPatch->setPluginParameter(QStringLiteral("midichannel"), QVariant(2));

    for (quint32 control = 7; control < 28; ++control)
    {
        const quint32 packed = (3u << 12) | control;
        vc->createAndAddInputSource(second, 0, packed);
    }

    QQmlComponent component(m_app->engine(), QUrl("qrc:/VirtualConsole.qml"));
    std::unique_ptr<QObject> object(component.create(qmlContext(m_app->rootObject())));
    auto *console = qobject_cast<QQuickItem *>(object.get());
    QVERIFY2(console, qPrintable(component.errorString()));
    console->setParentItem(m_app->rootObject());
    console->setWidth(appWidth);
    console->setHeight(appHeight);
    QVERIFY(console->setProperty("outlineOpen", true));

    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(appWidth, appHeight);
    m_app->setAccessMask(oldMask | App::AC_VCEditing);
    contexts->switchToContext("VC");
    vc->setEditMode(false);
    vc->setSelectedWidgetIDs({});
    m_app->doc()->resetModified();

    auto *outline = console->findChild<QQuickItem *>("vcOutlinePane");
    auto *assignmentPane = console->findChild<QQuickItem *>("vcAssignmentSummaryPane");
    auto *assignmentScroll = console->findChild<QQuickItem *>("vcAssignmentSummaryScroll");
    auto *assignmentRowsColumn = console->findChild<QQuickItem *>("vcAssignmentRowsColumn");
    auto *search = console->findChild<QQuickItem *>("vcOutlineSearch");
    auto *scrollBar = console->findChild<QQuickItem *>("vcAssignmentSummaryScrollBar");
    auto *horizontalScrollBar = console->findChild<QQuickItem *>("vcAssignmentSummaryScrollBarHorizontal");
    auto *errorDialog = console->findChild<QObject *>("vcOutlineError");
    auto *headerWidget = console->findChild<QQuickItem *>("vcAssignmentHeaderWidget");
    auto *headerInput = console->findChild<QQuickItem *>("vcAssignmentHeaderInput");
    auto *headerFeedback = console->findChild<QQuickItem *>("vcAssignmentHeaderFeedback");
    QVERIFY(outline);
    QVERIFY(assignmentPane);
    QVERIFY(assignmentScroll);
    QVERIFY(assignmentRowsColumn);
    QVERIFY(search);
    QVERIFY(scrollBar);
    QVERIFY(errorDialog);
    QVERIFY(headerWidget);
    QVERIFY(headerInput);
    QVERIFY(headerFeedback);
    QTRY_VERIFY(outline->isVisible());
    QTRY_VERIFY(assignmentPane->isVisible());
    if (outlineWidth > 0)
    {
        QVERIFY(console->setProperty("outlineWidth", outlineWidth));
        QCoreApplication::processEvents();
    }

    const QString leafKey = QStringLiteral("widget:%1").arg(second->id());
    model->setDiagnosticsActive(true);
    QVERIFY(search->setProperty("text", QStringLiteral("Button-B")));
    QTRY_COMPARE(model->filterText(), QStringLiteral("Button-B"));
    QTRY_VERIFY(model->indexForKey(leafKey).isValid());
    QVERIFY(QMetaObject::invokeMethod(outline, "selectKey", Q_ARG(QVariant, leafKey), Q_ARG(QVariant, 0)));
    QTRY_COMPARE(model->inspectionScopeKey(), leafKey);
    QTRY_VERIFY(model->assignmentSummary().value("inputBindingCount").toInt() > 0);

    QTRY_VERIFY(model->assignmentRows()->rowCount() > 0);

    const auto itemWithObjectName = [](QQuickItem *root, const QString &targetName) -> QQuickItem * {
        if (root == nullptr)
            return nullptr;
        QList<QQuickItem *> pending{root};
        while (!pending.isEmpty())
        {
            QQuickItem *candidate = pending.takeLast();
            if (candidate->property("objectName").toString() == targetName)
                return candidate;
            const auto children = candidate->childItems();
            for (QQuickItem *child : children)
                pending.append(child);
        }
        return nullptr;
    };
    const auto countItemsWithObjectName = [](QQuickItem *root, const QString &targetName) -> int {
        if (root == nullptr)
            return 0;
        int count = 0;
        QList<QQuickItem *> pending{root};
        while (!pending.isEmpty())
        {
            QQuickItem *candidate = pending.takeLast();
            if (candidate->property("objectName").toString() == targetName)
                ++count;
            const auto children = candidate->childItems();
            for (QQuickItem *child : children)
                pending.append(child);
        }
        return count;
    };

    QTRY_VERIFY(assignmentScroll->isVisible());
    QTRY_COMPARE(countItemsWithObjectName(assignmentPane, QStringLiteral("vcAssignmentSummaryRow")),
                 model->assignmentRows()->rowCount());
    const QVariantMap firstRow = model->assignmentRows()->index(0, 0).data().toMap();
    QVERIFY(!firstRow.isEmpty());

    QQuickItem *widgetCell = nullptr;
    QTRY_VERIFY((widgetCell = itemWithObjectName(assignmentPane, QStringLiteral("vcAssignmentCellWidget"))) != nullptr);
    QVERIFY(widgetCell->property("text").toString().contains(QStringLiteral("Button-B")));

    QQuickItem *inputCell = nullptr;
    QTRY_VERIFY((inputCell = itemWithObjectName(assignmentPane, QStringLiteral("vcAssignmentCellInput"))) != nullptr);
    QVERIFY(!inputCell->property("text").toString().isEmpty());

    QQuickItem *feedbackCell = nullptr;
    QTRY_VERIFY((feedbackCell = itemWithObjectName(assignmentPane, QStringLiteral("vcAssignmentCellFeedback"))) != nullptr);
    QVERIFY(!feedbackCell->property("text").toString().isEmpty());

    QQuickItem *showButton = nullptr;
    QTRY_VERIFY((showButton = itemWithObjectName(assignmentPane, QStringLiteral("vcAssignmentShowWidget"))) != nullptr);
    QVERIFY(showButton->property("text").toString() == QStringLiteral("Show"));
    QVERIFY(showButton->property("activeFocusOnTab").toBool());
    const QString detailsText = showButton->property("detailsText").toString();
    QVERIFY(detailsText.contains(QStringLiteral("Source raw:")));
    QVERIFY(detailsText.contains(QStringLiteral("Route parameters: input")));
    QVERIFY(detailsText.contains(QStringLiteral("Widget path:")));
    QVERIFY(detailsText.contains(QStringLiteral("Input labels:")));
    QVERIFY(detailsText.contains(QStringLiteral("Output labels:")));
    QVERIFY(detailsText.contains(QStringLiteral("Feedback labels:")));

    QVariantMap detailsRow;
    for (int rowIndex = 0; rowIndex < model->assignmentRows()->rowCount(); ++rowIndex)
    {
        const QVariantMap candidate = model->assignmentRows()->index(rowIndex, 0).data().toMap();
        const QString uid = candidate.value(QStringLiteral("inputUID")).toString();
        const QString channel = QString::number(candidate.value(QStringLiteral("sourceChannel")).toInt());
        const QString packed = QString::number(candidate.value(QStringLiteral("sourceChannelRaw")).toInt());
        if (!uid.isEmpty() &&
            detailsText.contains(QStringLiteral("UID %1").arg(uid)) &&
            detailsText.contains(QStringLiteral("channel %1").arg(channel)) &&
            detailsText.contains(QStringLiteral("packed %1").arg(packed)))
        {
            detailsRow = candidate;
            break;
        }
    }
    QVERIFY(!detailsRow.isEmpty());
    QVERIFY(detailsText.contains(detailsRow.value(QStringLiteral("widgetPath")).toString()));
    QVERIFY(detailsText.contains(detailsRow.value(QStringLiteral("midiMessageLabel")).toString()));
    QVERIFY(detailsText.contains(detailsRow.value(QStringLiteral("midiSelectionLabel")).toString()));
    QVERIFY(detailsText.contains(detailsRow.value(QStringLiteral("midiOutputSelectionLabel")).toString()));
    QVERIFY(detailsText.contains(detailsRow.value(QStringLiteral("midiFeedbackLowerLabel")).toString()));
    QVERIFY(detailsText.contains(detailsRow.value(QStringLiteral("midiFeedbackUpperLabel")).toString()));
    QVERIFY(detailsText.contains(detailsRow.value(QStringLiteral("midiFeedbackMonitorLabel")).toString()));

    QCOMPARE(headerWidget->property("text").toString(), QStringLiteral("Widget"));
    QCOMPARE(headerInput->property("text").toString(), QStringLiteral("Input"));
    QCOMPARE(headerFeedback->property("text").toString(), QStringLiteral("Feedback"));
    QCOMPARE(headerWidget->property("elide").toInt(), static_cast<int>(Qt::ElideRight));
    QCOMPARE(headerInput->property("elide").toInt(), static_cast<int>(Qt::ElideRight));
    QCOMPARE(headerFeedback->property("elide").toInt(), static_cast<int>(Qt::ElideRight));

    const auto horizontalBounds = [&](QQuickItem *item) {
        const QPointF left = item->mapToItem(assignmentScroll, QPointF(0, 0));
        return QPair<qreal, qreal>(left.x(), left.x() + item->width());
    };

    const auto widgetHeaderBounds = horizontalBounds(headerWidget);
    const auto inputHeaderBounds = horizontalBounds(headerInput);
    const auto feedbackHeaderBounds = horizontalBounds(headerFeedback);
    const auto widgetCellBounds = horizontalBounds(widgetCell);
    const auto inputCellBounds = horizontalBounds(inputCell);
    const auto feedbackCellBounds = horizontalBounds(feedbackCell);
    const qreal tolerance = 1.0;
    QVERIFY(std::abs(widgetHeaderBounds.first - widgetCellBounds.first) <= tolerance);
    QVERIFY(std::abs(inputHeaderBounds.first - inputCellBounds.first) <= tolerance);
    QVERIFY(std::abs(feedbackHeaderBounds.first - feedbackCellBounds.first) <= tolerance);
    QVERIFY(std::abs((widgetHeaderBounds.second - widgetHeaderBounds.first) -
                     (widgetCellBounds.second - widgetCellBounds.first)) <= tolerance);
    QVERIFY(std::abs((inputHeaderBounds.second - inputHeaderBounds.first) -
                     (inputCellBounds.second - inputCellBounds.first)) <= tolerance);
    QVERIFY(std::abs((feedbackHeaderBounds.second - feedbackHeaderBounds.first) -
                     (feedbackCellBounds.second - feedbackCellBounds.first)) <= tolerance);

    QVERIFY(headerWidget->width() >= (headerWidget->property("implicitWidth").toReal() * 0.8));
    QVERIFY(headerInput->width() >= (headerInput->property("implicitWidth").toReal() * 0.8));
    QVERIFY(headerFeedback->width() >= (headerFeedback->property("implicitWidth").toReal() * 0.8));

    QTRY_VERIFY(scrollBar->isVisible());

    qreal viewportWidth = assignmentScroll->width();
    qreal contentWidth = assignmentScroll->property("contentWidth").toReal();
    const qreal viewportHeight = assignmentScroll->height();
    qreal contentHeight = assignmentScroll->property("contentHeight").toReal();
    if (outlineWidth >= 0)
    {
        QVERIFY(console->setProperty("outlineWidth", outlineWidth));
        QCoreApplication::processEvents();
    }

    viewportWidth = assignmentScroll->width();
    contentWidth = assignmentScroll->property("contentWidth").toReal();
    contentHeight = assignmentScroll->property("contentHeight").toReal();
    if (expectHorizontalOverflow)
    {
        viewportWidth = assignmentScroll->width();
        contentWidth = assignmentScroll->property("contentWidth").toReal();
        contentHeight = assignmentScroll->property("contentHeight").toReal();
        const QString overflowMsg = QStringLiteral("forced outline %1, actual outline %2, viewport %3, content %4")
                                        .arg(outlineWidth)
                                        .arg(outline->width())
                                        .arg(viewportWidth)
                                        .arg(contentWidth);
        QVERIFY2(contentWidth > (viewportWidth + 0.5), qPrintable(overflowMsg));
        QVERIFY2(contentHeight > (viewportHeight + 0.5),
                 qPrintable(QStringLiteral("vertical overflow missing: viewport=%1 content=%2")
                                .arg(viewportHeight)
                                .arg(contentHeight)));
        QVERIFY(horizontalScrollBar != nullptr);
        QTRY_VERIFY(horizontalScrollBar->isVisible());
        QVERIFY(horizontalScrollBar->property("position").isValid());
        assignmentScroll->setProperty("contentX", 0.0);
        QCoreApplication::processEvents();
        const qreal startX = assignmentScroll->property("contentX").toReal();
        QVERIFY(horizontalScrollBar->setProperty("position", 1.0));
        QCoreApplication::processEvents();
        QTRY_VERIFY(assignmentScroll->property("contentX").toReal() > startX + 0.5);

        assignmentScroll->setFocus(true);
        const qreal beforeKeyX = assignmentScroll->property("contentX").toReal();
        QTest::keyClick(m_app.get(), Qt::Key_Right);
        QCoreApplication::processEvents();
        const qreal afterKeyX = assignmentScroll->property("contentX").toReal();
        QVERIFY(afterKeyX >= beforeKeyX - 0.5);
    }
    else if (horizontalScrollBar != nullptr)
    {
        QVERIFY2(contentWidth <= (viewportWidth + 0.5),
                 qPrintable(QStringLiteral("unexpected horizontal overflow: viewportWidth=%1 contentWidth=%2 maxX=%3")
                                .arg(viewportWidth)
                                .arg(contentWidth)
                                .arg(qMax<qreal>(0.0, contentWidth - viewportWidth))));
    }

    const qreal maxX = qMax<qreal>(0.0, contentWidth - viewportWidth);
    assignmentScroll->setProperty("contentX", maxX);
    const qreal maxY = qMax<qreal>(0.0, contentHeight - viewportHeight);
    assignmentScroll->setProperty("contentY", maxY);
    QCoreApplication::processEvents();

    const auto scrollBarBounds = horizontalBounds(scrollBar);
    const auto showBounds = horizontalBounds(showButton);
    const QString showBarMsg = QStringLiteral("showRight=%1 barLeft=%2 maxX=%3 contentWidth=%4 viewportWidth=%5")
                                   .arg(showBounds.second)
                                   .arg(scrollBarBounds.first)
                                   .arg(maxX)
                                   .arg(contentWidth)
                                   .arg(viewportWidth);
    const auto verticalBounds = [&](QQuickItem *item) {
        const QPointF top = item->mapToItem(assignmentScroll, QPointF(0, 0));
        return QPair<qreal, qreal>(top.y(), top.y() + item->height());
    };
    const auto ancestorByName = [](QQuickItem *item, const QString &targetName) -> QQuickItem * {
        QQuickItem *current = item;
        while (current != nullptr)
        {
            if (current->objectName() == targetName)
                return current;
            current = current->parentItem();
        }
        return nullptr;
    };

    if (expectHorizontalOverflow)
    {
        const auto horizontalBarBounds = verticalBounds(horizontalScrollBar);
        QQuickItem *bottomShowButton = nullptr;
        qreal bestBottom = -1.0;
        QList<QQuickItem *> pending{assignmentPane};
        while (!pending.isEmpty())
        {
            QQuickItem *candidate = pending.takeLast();
            if (candidate->objectName() == QStringLiteral("vcAssignmentShowWidget") &&
                candidate->isVisible() &&
                candidate->width() > 0 &&
                candidate->height() > 0)
            {
                const auto candidateBounds = verticalBounds(candidate);
                const bool intersectsVisiblePane = candidateBounds.second > 0.0 &&
                                                   candidateBounds.first < assignmentScroll->height();
                if (intersectsVisiblePane &&
                    candidateBounds.second > bestBottom)
                {
                    bestBottom = candidateBounds.second;
                    bottomShowButton = candidate;
                }
            }
            const auto children = candidate->childItems();
            for (QQuickItem *child : children)
                pending.append(child);
        }
        QVERIFY(bottomShowButton != nullptr);
        QQuickItem *bottomRow = ancestorByName(bottomShowButton, QStringLiteral("vcAssignmentTableRow"));
        QVERIFY(bottomRow != nullptr);
        const auto bottomRowBounds = verticalBounds(bottomRow);
        const auto bottomShowBounds = verticalBounds(bottomShowButton);
        const auto bottomShowHorizontal = horizontalBounds(bottomShowButton);
        qDebug() << "ASSIGNMENT_OVERFLOW_GEOMETRY"
                 << "outline" << outline->width()
                 << "viewport" << viewportWidth
                 << "content" << contentWidth
                 << "maxX" << maxX
                 << "maxY" << maxY
                 << "showRight" << bottomShowHorizontal.second
                 << "barLeft" << scrollBarBounds.first
                 << "rowBottom" << bottomRowBounds.second
                 << "showBottom" << bottomShowBounds.second
                 << "hBarTop" << horizontalBarBounds.first;

        QVERIFY(bottomShowBounds.first >= bottomRowBounds.first - tolerance);
        QVERIFY(bottomShowBounds.second <= bottomRowBounds.second + tolerance);
        QVERIFY(bottomShowBounds.second <= horizontalBarBounds.first + tolerance);
        QVERIFY(bottomShowHorizontal.second <= scrollBarBounds.first + tolerance);

        vc->page(0)->setPIN(1234);
        const QPointF localClickF = bottomShowButton->mapToItem(
            m_app->rootObject(), QPointF(bottomShowButton->width() / 2.0, bottomShowButton->height() / 2.0));
        const QPoint localClick = localClickF.toPoint();
        QVERIFY(localClick.x() >= 0 && localClick.x() < m_app->width());
        QVERIFY(localClick.y() >= 0 && localClick.y() < m_app->height());
        QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, localClick);
        QTRY_VERIFY(errorDialog->property("visible").toBool());
        QVERIFY(!errorDialog->property("message").toString().isEmpty());
        QMetaObject::invokeMethod(errorDialog, "close");
        vc->page(0)->setPIN(0);
    }
    QVERIFY2(showBounds.second <= (scrollBarBounds.first + tolerance), qPrintable(showBarMsg));

    const auto widgetHeaderAtMax = horizontalBounds(headerWidget);
    const auto inputHeaderAtMax = horizontalBounds(headerInput);
    const auto feedbackHeaderAtMax = horizontalBounds(headerFeedback);
    const auto widgetCellAtMax = horizontalBounds(widgetCell);
    const auto inputCellAtMax = horizontalBounds(inputCell);
    const auto feedbackCellAtMax = horizontalBounds(feedbackCell);
    QVERIFY(std::abs(widgetHeaderAtMax.first - widgetCellAtMax.first) <= tolerance);
    QVERIFY(std::abs(inputHeaderAtMax.first - inputCellAtMax.first) <= tolerance);
    QVERIFY(std::abs(feedbackHeaderAtMax.first - feedbackCellAtMax.first) <= tolerance);
}

void UpstreamIntegration_Test::vcPanesCoexist_data()
{
    QTest::addColumn<QString>("operation");
    QTest::newRow("open") << QString("open");
    QTest::newRow("mode-toggle") << QString("mode-toggle");
    QTest::newRow("no-match") << QString("no-match");
    QTest::newRow("properties-selection") << QString("properties-selection");
    QTest::newRow("narrow-selection") << QString("narrow-selection");
}

void UpstreamIntegration_Test::vcPanesCoexist()
{
    QFETCH(QString, operation);
    VirtualConsole *vc = m_app->virtualConsole();
    const QString oldFilter = vc->outlineModel()->filterText();
    const bool oldEdit = vc->editMode();
    const bool oldVisible = m_app->isVisible();
    const QSize oldSize = m_app->size();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    const bool wasModified = m_app->doc()->isModified();
    QVariantList createdIDs;
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets(createdIDs);
        vc->setSelectedWidgetIDs(oldSelection);
        vc->outlineModel()->setFilterText(oldFilter);
        vc->setEditMode(oldEdit);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified) m_app->doc()->resetModified();
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
    if (operation.endsWith("selection"))
    {
        auto *button = new VCButton(m_app->doc(), vc->page(0));
        vc->addWidgetToMap(button);
        vc->page(0)->addWidgetToPageMap(button);
        createdIDs.append(button->id());
        const QString key = QString("widget:%1").arg(button->id());
        QTRY_VERIFY(vc->outlineModel()->indexForKey(key).isValid());
        QVERIFY(QMetaObject::invokeMethod(outline, "selectKey", Q_ARG(QVariant, key), Q_ARG(QVariant, 0)));
        QCOMPARE(vc->selectedWidgetIDs(), createdIDs);
        if (operation == "properties-selection")
        {
            auto *toggle = console->findChild<QQuickItem *>("vcPropertiesToggle");
            QVERIFY(toggle);
            QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
            m_app->update();
            QTRY_VERIFY(!rendered.isEmpty());
            QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier,
                toggle->mapToScene(QPointF(toggle->width() / 2, toggle->height() / 2)).toPoint());
            QTRY_COMPARE(functions->property("loaderSource").toString(), QString("qrc:/VCWidgetProperties.qml"));
            auto *functionsToggle = console->findChild<QQuickItem *>("vcFunctionsToggle");
            QVERIFY(functionsToggle);
            QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier,
                functionsToggle->mapToScene(QPointF(functionsToggle->width() / 2, functionsToggle->height() / 2)).toPoint());
            QTRY_COMPARE(functions->property("loaderSource").toString(), QString("qrc:/FunctionManager.qml"));
        }
        else
        {
            m_app->resize(960, 700);
            QTRY_VERIFY(console->width() <= 960);
        }
        QTRY_VERIFY(outline->isVisible() && outline->width() > 100);
        QTRY_VERIFY(functions->isVisible() && functions->width() > 100);
        QCOMPARE(vc->selectedWidgetIDs(), createdIDs);
        QTRY_VERIFY(vc->outlineModel()->indexForKey(key).data(VCOutlineModel::SelectedRole).toBool());
    }
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
    const bool oldEdit = vc->editMode();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    auto *scene = new Scene(m_app->doc());
    scene->setName("Preview binding");
    QVERIFY(m_app->doc()->addFunction(scene));
    const quint32 functionID = scene->id();
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
        m_app->doc()->deleteFunction(functionID);
        vc->setEditMode(oldEdit);
        vc->setSelectedWidgetIDs(oldSelection);
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
    first->setFunctionID(functionID);
    const QRectF originalSecond = second->logicalGeometry();
    const QVariantList originalOrder = frame->orderedChildIDs(0);
    vc->setEditMode(true);
    vc->setSelectedWidgetIDs({first->id(), second->id()});
    m_app->doc()->resetModified();
    auto snapshot = [&]() {
        QByteArray xml;
        QXmlStreamWriter writer(&xml);
        vc->saveXML(&writer);
        QVariantList visibility;
        for (int page = 0; page < vc->pagesCount(); ++page)
            for (VCWidget *widget : vc->page(page)->children(true))
                visibility.append(QVariantMap{{"id", widget->id()}, {"visible", widget->isVisible()},
                    {"editing", widget->isEditing()}, {"intensity", widget->intensity()}});
        return QVariantMap{{"xml", xml}, {"selection", vc->selectedWidgetIDs()}, {"visibility", visibility},
            {"selectedPage", vc->selectedPage()}, {"dirty", m_app->doc()->isModified()}};
    };
    const QVariantMap beforePreview = snapshot();
    QVariantMap preview = vc->previewLayout(frameID, {{"padding", 10}, {"gap", 10}});
    QVERIFY2(preview.value("accepted").toBool(), qPrintable(preview.value("error").toString()));
    QVERIFY(!preview.value("applied").toBool());
    QCOMPARE(first->logicalGeometry(), originalFirst);
    QCOMPARE(second->logicalGeometry(), originalSecond);
    QCOMPARE(frame->orderedChildIDs(0), originalOrder);
    QVERIFY(!m_app->doc()->isModified());
    QCOMPARE(snapshot(), beforePreview);
    QVERIFY(!vc->layoutPreview().isEmpty());
    if (finish == "stale")
    {
        first->setLogicalGeometry(QRectF(160, 100, 100, 80));
        m_app->doc()->resetModified();
    }
    else if (finish == "cancel")
        vc->cancelLayout();
    const QRectF beforeConfirm = first->logicalGeometry();
    const QVariantMap beforeConfirmation = snapshot();
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
        QCOMPARE(snapshot(), beforeConfirmation);
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
                                       QString("temporary-unlock"), QString("access-mask"), QString("detached-unavailable")})
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
        if (page->previewContext()->detached()) page->previewContext()->setDetached(false);
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
    if (restriction == "detached-unavailable")
    {
        page->previewContext()->setDetached(true);
        QPointer<QQuickView> detached(page->previewContext()->view());
        QVERIFY(detached && detached != m_app.get());
        QVERIFY(detached->rootObject());
        detached->setSource(QUrl());
        QVERIFY(!detached->rootObject());
        m_app->doc()->resetModified();
        QByteArray before;
        QXmlStreamWriter writer(&before);
        vc->saveXML(&writer);
        const QVariantList selection = vc->selectedWidgetIDs();
        const int selectedPage = vc->selectedPage();
        const QVariantMap result = vc->revealWidget(buttonID);
        QVERIFY(!result.value("accepted").toBool());
        QVERIFY(!result.value("applied").toBool());
        QVERIFY(result.value("error").toString().contains("Detached page view is unavailable"));
        QByteArray after;
        QXmlStreamWriter afterWriter(&after);
        vc->saveXML(&afterWriter);
        QCOMPARE(after, before);
        QCOMPARE(vc->selectedWidgetIDs(), selection);
        QCOMPARE(vc->selectedPage(), selectedPage);
        QVERIFY(!m_app->doc()->isModified());
        page->previewContext()->setDetached(false);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(detached.isNull());
        return;
    }
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

void UpstreamIntegration_Test::vcOutlineMovesKeepOtherRows_data()
{
    QTest::addColumn<QString>("operation");
    for (const QString &operation : {QString("reorder"), QString("reparent")})
        QTest::newRow(qPrintable(operation)) << operation;
}

void UpstreamIntegration_Test::vcOutlineMovesKeepOtherRows()
{
    QFETCH(QString, operation);
    VirtualConsole *vc = m_app->virtualConsole();
    const bool oldEdit = vc->editMode();
    const bool oldVisible = m_app->isVisible();
    const QSize oldSize = m_app->size();
    QVariantList frameIDs;
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets(frameIDs);
        vc->setEditMode(oldEdit);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
    });
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1440, 900);
    vc->setEditMode(true);
    QQmlComponent component(m_app->engine(), QUrl("qrc:/VirtualConsole.qml"));
    std::unique_ptr<QObject> object(component.create(qmlContext(m_app->rootObject())));
    auto *console = qobject_cast<QQuickItem *>(object.get());
    QVERIFY2(console, qPrintable(component.errorString()));
    console->setParentItem(m_app->rootObject());
    console->setWidth(1440);
    console->setHeight(900);
    QVERIFY(console->setProperty("outlineOpen", true));
    auto *outline = console->findChild<QQuickItem *>("vcOutlinePane");
    QVERIFY(outline);
    QTRY_VERIFY(outline->isVisible() && outline->width() > 100);
    // the edit moves the first frame's buttons, the second frame starts empty, the third only looks on
    QList<VCFrame *> frames;
    QList<QVariantList> buttonIDs;
    for (int f = 0; f < 3; ++f)
    {
        auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
        vc->addWidgetToMap(frame);
        vc->page(0)->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setLogicalGeometry(QRectF(f * 700, 0, 650, 400));
        frameIDs.append(frame->id());
        frames.append(frame);
        QVariantList ids;
        for (int i = 0; i < (f == 1 ? 0 : 6); ++i)
        {
            auto *button = new VCButton(m_app->doc(), frame);
            vc->addWidgetToMap(button);
            frame->addWidgetToPageMap(button);
            button->setLogicalGeometry(QRectF(20 + i * 100, 60, 80, 80));
            ids.append(button->id());
        }
        buttonIDs.append(ids);
    }
    auto locate = [&](const QVariant &id) {
        QQuickItem *found = nullptr;
        const QString name = QString("vcOutlineRow-%1").arg(id.toUInt());
        std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
            if (item->isVisible() && item->objectName() == name) found = item;
            for (QQuickItem *child : item->childItems()) visit(child);
        };
        visit(outline);
        return found;
    };
    for (const QVariantList &ids : {buttonIDs[0], buttonIDs[2]})
    {
        QTRY_VERIFY(vc->outlineModel()->indexForKey(QString("widget:%1").arg(ids[0].toUInt())).isValid());
        QVERIFY(QMetaObject::invokeMethod(outline, "ensureExpandedToKey",
                                          Q_ARG(QVariant, QString("widget:%1").arg(ids[0].toUInt()))));
    }
    QVERIFY(QMetaObject::invokeMethod(outline, "toggleExpanded", Q_ARG(QVariant, QString("widget:%1").arg(frames[1]->id()))));
    QList<QPointer<QQuickItem>> onlookers;
    for (const QVariant &id : buttonIDs[2])
    {
        QQuickItem *row = nullptr;
        QTRY_VERIFY((row = locate(id)));
        onlookers.append(row);
    }
    QSignalSpy proxyLayouts(vc->outlineModel()->filteredModel(), &QAbstractItemModel::layoutChanged);
    // reorder: each button in turn to the front reverses the frame; reparent: all into the next frame
    QVariantList moved = buttonIDs[0];
    VCFrame *owner = frames[operation == "reorder" ? 0 : 1];
    if (operation == "reorder")
    {
        std::reverse(moved.begin(), moved.end());
        for (int i = 1; i < buttonIDs[0].size(); ++i)
        {
            const QVariantMap result = vc->editStructure({buttonIDs[0][i]}, owner->id(), 0, 0);
            QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
        }
    }
    else
    {
        const QVariantMap result = vc->editStructure(moved, owner->id(), 0, 0);
        QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
    }
    QCOMPARE(owner->orderedChildIDs(0).mid(0, moved.size()), moved);
    auto outlineOrder = [&]() {
        QAbstractItemModel *rows = vc->outlineModel()->filteredModel();
        const QModelIndex parent = vc->outlineModel()->indexForKey(QString("widget:%1").arg(owner->id()));
        QVariantList ids;
        for (int row = 0; row < std::min(rows->rowCount(parent), int(moved.size())); ++row)
            ids.append(rows->index(row, 0, parent).data(VCOutlineModel::WidgetIDRole).toUInt());
        return ids;
    };
    QTRY_COMPARE(outlineOrder(), moved);
    for (const QVariant &id : moved)
        QTRY_VERIFY(locate(id));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    for (int i = 0; i < onlookers.size(); ++i)
        QVERIFY2(onlookers[i] && onlookers[i] == locate(buttonIDs[2][i]),
                 qPrintable(QString("outline row of untouched button %1 was rebuilt").arg(buttonIDs[2][i].toUInt())));
    QCOMPARE(proxyLayouts.count(), 0);
}

void UpstreamIntegration_Test::vcScreenshotConsole_data()
{
    QTest::addColumn<QString>("variant");
    QTest::addColumn<QString>("heldGesture");
    QTest::addColumn<QString>("outlineRelease");
    for (const QString &variant : {QString("six-groups"), QString("nested-flush"), QString("nested-near-bottom")})
        QTest::newRow(qPrintable(variant)) << variant << QString() << QString();
    for (const QString &gesture : {QString("outline"), QString("canvas"), QString("resize")})
        QTest::newRow(qPrintable("held-" + gesture)) << QString("six-groups") << gesture << QString();
    for (const QString &probe : {QString("receipt"), QString("screenshot")})
        QTest::newRow(qPrintable("outline-release-" + probe)) << QString("six-groups") << QString() << probe;
}

void UpstreamIntegration_Test::vcScreenshotConsole()
{
    QFETCH(QString, variant);
    QFETCH(QString, heldGesture);
    QFETCH(QString, outlineRelease);
    const QString artifactRoot = qEnvironmentVariable("QLC_VC_VISUAL_DIR");
    if (artifactRoot.isEmpty())
        QSKIP("Run explicitly with QLC_VC_VISUAL_DIR for the isolated native fixture");
    const QString artifactDirectory = QDir(artifactRoot).filePath(QString::fromLatin1(QTest::currentDataTag()));
    QVERIFY(QDir().mkpath(artifactDirectory));
    bool durationValid = false;
    const int hold = qEnvironmentVariableIntValue("QLC_VC_VISUAL_HOLD_MS", &durationValid);
    QVERIFY(!durationValid || hold == 0 || (hold >= 180000 && hold <= 240000));
    QVERIFY(QGuiApplication::platformName() == "offscreen" || (durationValid && hold > 0));
    QVERIFY(heldGesture.isEmpty() || (QGuiApplication::platformName() == "offscreen" && hold == 0));
    QVERIFY(outlineRelease.isEmpty() || (QGuiApplication::platformName() == "offscreen" && hold == 0));
    const QString runID = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QFile executable(QCoreApplication::applicationFilePath());
    QVERIFY(executable.open(QIODevice::ReadOnly));
    QCryptographicHash executableHash(QCryptographicHash::Sha256);
    QVERIFY(executableHash.addData(&executable));
    const QString testSHA = QString::fromLatin1(executableHash.result().toHex());
    if (hold > 0)
        QCOMPARE(testSHA, qEnvironmentVariable("QLC_VC_EXPECT_TEST_SHA256"));
    QJsonObject resourceHashes;
    for (const QString &name : {QString("VirtualConsole.qml"), QString("VCOutline.qml"), QString("VCWidgetItem.qml"),
                               QString("VCFrameItem.qml"), QString("VCPageArea.qml")})
    {
        QFile source(QStringLiteral(QT_TESTCASE_SOURCEDIR "/../../qml/virtualconsole/") + name);
        QFile resource(":/" + name);
        QVERIFY(source.open(QIODevice::ReadOnly));
        QVERIFY(resource.open(QIODevice::ReadOnly));
        const QByteArray sourceBytes = source.readAll(), resourceBytes = resource.readAll();
        QCOMPARE(resourceBytes, sourceBytes);
        resourceHashes[name] = QString::fromLatin1(QCryptographicHash::hash(resourceBytes, QCryptographicHash::Sha256).toHex());
    }
    VirtualConsole *vc = m_app->virtualConsole();
    QCOMPARE(m_app->doc()->fixtures().size(), 0);
    int outputPatches = 0;
    for (Universe *universe : m_app->doc()->inputOutputMap()->universes())
        outputPatches += universe->outputPatchesCount();
    QCOMPARE(outputPatches, 0);
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const QSize oldSize = m_app->size();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode();
    const bool wasModified = m_app->doc()->isModified();
    const int oldPage = vc->selectedPage();
    const QString oldWorkingPath = m_app->workingPath();
    const QVariant oldDensity = m_app->rootContext()->contextProperty("screenPixelDensity");
    QVariantList roots;
    QList<quint32> functionIDs;
    QJsonObject roles;
    const auto cleanup = qScopeGuard([&]() {
        vc->cancelLayout();
        vc->deleteVCWidgets(roots);
        for (quint32 id : functionIDs) m_app->doc()->deleteFunction(id);
        vc->setSelectedPage(oldPage);
        vc->setEditMode(oldEdit);
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        m_app->setWorkingPath(oldWorkingPath);
        if (!outlineRelease.isEmpty()) m_app->rootContext()->setContextProperty("screenPixelDensity", oldDensity);
        if (!wasModified) m_app->doc()->resetModified();
    });
    if (!outlineRelease.isEmpty())
        m_app->rootContext()->setContextProperty("screenPixelDensity", qreal(5.077272727272727));
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
    if (variant != "six-groups")
    {
        auto *a = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(20, 20, 1100, 650),
            variant == "nested-flush" ? QString("Ancestor caption ").repeated(12) : QString("Native A")));
        QVERIFY(a);
        const QRectF rect(variant == "nested-flush" ? a->contentRect().topLeft() : QPointF(80, 308), QSizeF(360, 340));
        auto *b = qobject_cast<VCFrame *>(create(a, "Frame", rect, "Native B"));
        QVERIFY(b);
        VCWidget *intensity = create(b, "Slider", QRectF(10, 190, 90, 140), "Intensity");
        auto *reset = qobject_cast<VCButton *>(create(b, "Button", QRectF(253, 190, 100, 55), "Reset"));
        QVERIFY(intensity && reset);
        reset->setFunctionID(functionIDs[2]);
        roles = QJsonObject{{"A", int(a->id())}, {"B", int(b->id())},
                           {"Intensity", int(intensity->id())}, {"Reset", int(reset->id())}};
        if (variant == "nested-near-bottom")
        {
            auto *adjacent = qobject_cast<VCButton *>(create(vc->page(0), "Button", QRectF(460, 670, 100, 60), "Adjacent"));
            QVERIFY(adjacent);
            adjacent->setFunctionID(functionIDs[0]);
            roles["Adjacent"] = int(adjacent->id());
        }
    }
    else
    {
        auto *head1 = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(20, 20, 360, 340), "Head 1"));
        auto *head2 = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(400, 20, 360, 340), "Head 2"));
        auto *scanner = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(20, 370, 360, 350), "Scanner"));
        auto *laser = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(400, 370, 340, 200), "Laser"));
        auto *led = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(400, 590, 340, 130), "LED"));
        auto *scenes = qobject_cast<VCFrame *>(create(vc->page(0), "Frame", QRectF(780, 20, 240, 700), "Scenes / Master"));
        QVERIFY(head1 && head2 && scanner && laser && led && scenes);
        head2->setMultiPageMode(true);
        head2->setTotalPagesNumber(2);
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
        roles = QJsonObject{{"Head1", int(head1->id())}, {"Head2", int(head2->id())},
                           {"Scanner", int(scanner->id())}, {"Scenes", int(scenes->id())}};
    }

    m_app->doc()->masterTimer()->stop();
    QElapsedTimer clock;
    clock.start();
    QSignalSpy starts(m_app->doc()->masterTimer(), &MasterTimer::functionStarted);
    QSignalSpy authoringTicks(m_app->doc()->masterTimer(), &MasterTimer::tickReady);
    std::vector<std::unique_ptr<QSignalSpy>> dropObservers;
    QSet<QQuickItem *> watchedDrops;
    QSignalSpy published(vc, &VirtualConsole::structureChanged);
    QJsonArray startRequests;
    QJsonArray gestureEvents, queueTransitions, geometryEvents;
    QJsonObject geometryCounts, lastQueue;
    int queueSamples = 0;
    bool queueViolation = false;
    bool inputSealed = false;
    int sealedTickCount = 0;
    quint64 frameSerial = 0;
    const auto paintConnection = connect(m_app.get(), &QQuickWindow::frameSwapped, this, [&]() { ++frameSerial; });
    auto rectangle = [](const QRectF &rect) {
        return QJsonObject{{"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}};
    };
    auto functionState = [&]() {
        QJsonObject state;
        for (quint32 id : functionIDs)
        {
            Function *function = m_app->doc()->function(id);
            const bool queued = function && m_app->doc()->masterTimer()->isStartQueued(function);
            const bool running = function && function->isRunning();
            state[QString::number(id)] = QJsonObject{{"queued", queued}, {"running", running}};
        }
        return state;
    };
    auto sampleQueue = [&]() {
        const QJsonObject state = functionState();
        for (const auto &value : state)
            queueViolation |= value.toObject().value("queued").toBool() || value.toObject().value("running").toBool();
        ++queueSamples;
        if (state != lastQueue)
        {
            queueTransitions.append(QJsonObject{{"ms", clock.elapsed()}, {"state", state}});
            lastQueue = state;
        }
    };
    QSet<quint32> watched;
    QSet<QQuickItem *> watchedItems;
    QList<QPointer<QQmlExpression>> expressions;
    QList<QMetaObject::Connection> requestConnections;
    auto watchRequests = [&]() {
        if (inputSealed) return;
        std::function<void(QQuickItem *)> watchDrops = [&](QQuickItem *item) {
            if (item->property("containsDrag").isValid() && !watchedDrops.contains(item))
            {
                watchedDrops.insert(item);
                dropObservers.push_back(std::make_unique<QSignalSpy>(item, SIGNAL(dropped(QQuickDragEvent*))));
                requestConnections.append(connect(item, &QObject::destroyed, this, [&, item]() { watchedDrops.remove(item); }));
            }
            for (QQuickItem *child : item->childItems()) watchDrops(child);
        };
        watchDrops(m_app->rootObject());
        for (VCWidget *widget : vc->page(0)->children(true))
        {
            if (!watched.contains(widget->id()))
            {
                watched.insert(widget->id());
                requestConnections.append(connect(widget, &VCWidget::functionStarting, this,
                    [&](VCWidget *source, quint32 functionID, qreal intensity) {
                        if (inputSealed) return;
                        startRequests.append(QJsonObject{{"widgetID", int(source->id())},
                            {"functionID", int(functionID)}, {"intensity", intensity}, {"ms", clock.elapsed()}});
                    }));
                QPointer<VCWidget> observed = widget;
                requestConnections.append(connect(widget, &VCWidget::geometryChanged, this, [&, observed]() {
                    if (!observed || inputSealed) return;
                    const QString key = QString::number(observed->id());
                    geometryCounts[key] = geometryCounts.value(key).toInt() + 1;
                    geometryEvents.append(QJsonObject{{"widgetID", int(observed->id())}, {"ms", clock.elapsed()},
                        {"logicalRect", rectangle(observed->logicalGeometry())}});
                }));
            }
            QQuickItem *item = widget->renderItem();
            if (!item || watchedItems.contains(item)) continue;
            watchedItems.insert(item);
            requestConnections.append(connect(item, &QObject::destroyed, this, [&, item]() { watchedItems.remove(item); }));
            for (const QString &state : {QString("moving"), QString("resizing")})
            {
                auto *expression = new QQmlExpression(qmlContext(item), item, state, item);
                expression->setNotifyOnValueChanged(true);
                expression->evaluate();
                expressions.append(expression);
                QPointer<QQuickItem> observed = item;
                const quint32 id = widget->id();
                requestConnections.append(connect(expression, &QQmlExpression::valueChanged, this,
                    [&, expression, observed, id, state]() {
                        if (!observed || inputSealed) return;
                        const bool active = expression->evaluate().toBool();
                        gestureEvents.append(QJsonObject{{"widgetID", int(id)}, {"state", state},
                            {"active", active}, {"ms", clock.elapsed()},
                            {"resizeIntent", rectangle(observed->property("resizeIntent").toRectF())}});
                    }));
            }
        }
    };
    watchRequests();
    const QMetaObject::Connection watchNew = connect(vc, &VirtualConsole::structureChanged, this, watchRequests);
    const auto disconnectRequests = qScopeGuard([&]() {
        disconnect(watchNew);
        disconnect(paintConnection);
        for (const auto &connection : requestConnections) disconnect(connection);
        for (auto expression : expressions) if (expression) delete expression;
    });
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
    QTRY_VERIFY(vc->widget(roots.first().toUInt())->renderItem());
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    auto globalRect = [&](QQuickItem *item) {
        const QRectF scene = item->mapRectToScene(QRectF(QPointF(), QSizeF(item->width(), item->height())));
        return rectangle(QRectF(QPointF(m_app->mapToGlobal(QPoint())) + scene.topLeft(), scene.size()));
    };
    auto liveBoolean = [](QQuickItem *item, const QString &source) {
        QJsonObject state{{"source", source}, {"available", false}, {"value", QJsonValue::Null}};
        if (!item)
        {
            state["error"] = "Live QML object is unavailable";
            return state;
        }
        QQmlExpression expression(qmlContext(item), item, source);
        const QVariant value = expression.evaluate();
        if (expression.hasError() || value.typeId() != QMetaType::Bool)
        {
            state["error"] = expression.hasError() ? expression.error().toString() : "Live QML value is not a boolean";
            return state;
        }
        state["available"] = true;
        state["value"] = value.toBool();
        state["error"] = "";
        return state;
    };
    auto receipt = [&]() {
        QJsonArray widgets, namedItems, functionRows, dropAreas, attachedDrags;
        QJsonArray viewportControls;
        QList<QRectF> viewportControlRects;
        if (QQuickItem *view = vc->currentPageItem())
            for (QQuickItem *control : view->childItems())
                if (control->property("orientation").isValid() && control->property("position").isValid() &&
                    control->property("size").isValid() && control->property("pressed").isValid())
                {
                    const QRectF rect = control->mapRectToScene(QRectF(QPointF(), control->size()))
                        .translated(QPointF(m_app->mapToGlobal(QPoint())));
                    viewportControls.append(QJsonObject{{"kind", control->property("orientation").toInt() == Qt::Horizontal ?
                        "horizontalScrollbar" : "verticalScrollbar"}, {"globalRect", rectangle(rect)},
                        {"visible", control->isVisible()}, {"enabled", control->isEnabled()},
                        {"pressed", control->property("pressed").toBool()}, {"position", control->property("position").toDouble()},
                        {"size", control->property("size").toDouble()}});
                    if (control->isVisible() && control->isEnabled()) viewportControlRects.append(rect);
                }
        QQuickItem *functionRow = nullptr, *outlineRow = nullptr;
        for (VCWidget *widget : vc->page(0)->children(true))
        {
            QJsonObject record{{"id", int(widget->id())}, {"type", VCWidget::typeToString(widget->type())},
                {"caption", widget->caption()}, {"subpage", widget->page()},
                {"logicalRect", rectangle(widget->logicalGeometry())}, {"visible", widget->isVisible()},
                {"minimumWidth", widget->minimumSize().width()}, {"minimumHeight", widget->minimumSize().height()}};
            record["renderItemAvailable"] = widget->renderItem() != nullptr;
            record["movingAvailable"] = false;
            record["resizingAvailable"] = false;
            record["moving"] = QJsonValue::Null;
            record["resizing"] = QJsonValue::Null;
            if (VCWidget *parent = qobject_cast<VCWidget *>(widget->parent()))
                record["parentID"] = int(parent->id());
            if (auto *button = qobject_cast<VCButton *>(widget))
            {
                record["functionID"] = int(button->functionID());
                record["actionType"] = int(button->actionType());
            }
            if (auto *slider = qobject_cast<VCSlider *>(widget))
                record["controlledFunctionID"] = int(slider->controlledFunction());
            if (auto *frame = qobject_cast<VCFrame *>(widget))
            {
                record["currentPage"] = frame->currentPage();
                record["totalPages"] = frame->totalPagesNumber();
                record["collapsed"] = frame->isCollapsed();
                record["contentRect"] = rectangle(frame->contentRect());
            }
            if (widget->renderItem())
            {
                record["globalRect"] = globalRect(widget->renderItem());
                record["renderVisible"] = widget->renderItem()->isVisible();
                for (const QString &name : {QString("moving"), QString("resizing")})
                {
                    const QJsonObject state = liveBoolean(widget->renderItem(), name);
                    record[name] = state.value("value");
                    record[name + "Available"] = state.value("available");
                }
                record["resizeIntent"] = rectangle(widget->renderItem()->property("resizeIntent").toRectF());
                record["previewAccepted"] = widget->renderItem()->property("previewAccepted").toBool();
                record["previewError"] = widget->renderItem()->property("previewError").toString();
                const QPointF hotSpot = widget->renderItem()->property("dragHotSpot").toPointF();
                record["dragHotSpot"] = QJsonObject{{"x", hotSpot.x()}, {"y", hotSpot.y()}};
                QJsonArray ancestors;
                for (QQuickItem *parent = widget->renderItem()->parentItem(); parent; parent = parent->parentItem())
                    if (!parent->objectName().isEmpty()) ancestors.append(parent->objectName());
                record["renderAncestors"] = ancestors;
                QJsonArray captions;
                std::function<void(QQuickItem *)> captureCaption = [&](QQuickItem *item) {
                    if (item->property("text").toString() == widget->caption() &&
                        item->property("paintedWidth").isValid())
                        captions.append(QJsonObject{{"text", item->property("text").toString()},
                            {"globalRect", globalRect(item)}, {"visible", item->isVisible()},
                            {"paintedWidth", item->property("paintedWidth").toReal()},
                            {"paintedHeight", item->property("paintedHeight").toReal()}});
                    for (QQuickItem *child : item->childItems()) captureCaption(child);
                };
                captureCaption(widget->renderItem());
                record["captionItems"] = captions;
            }
            widgets.append(record);
        }
        std::function<void(QQuickItem *)> visit = [&](QQuickItem *item)
        {
            const bool declaredHandle = item->objectName().startsWith("vcResizeHandle-");
            const bool controller = item->objectName().startsWith("vcResizeCorner-");
            const bool fragment = item->objectName().startsWith("vcResizeFragment-");
            if (!outlineRow && item->objectName().startsWith("vcOutlineRow-")) outlineRow = item;
            if (item->objectName().startsWith("vcGestureViewport-"))
            {
                QJsonObject state = liveBoolean(item, "gestureGhost.Drag.active");
                state["kind"] = "canvas";
                state["owner"] = item->objectName();
                state["widgetID"] = item->objectName().mid(QString("vcGestureViewport-").size()).toInt();
                attachedDrags.append(state);
            }
            if (item->property("containsDrag").isValid())
            {
                QObject *drag = item->property("drag").value<QObject *>();
                QObject *source = drag ? drag->property("source").value<QObject *>() : nullptr;
                QJsonObject area{{"name", item->objectName()}, {"globalRect", globalRect(item)},
                    {"containsDrag", item->property("containsDrag").toBool()}, {"visible", item->isVisible()}};
                for (QQuickItem *parent = item->parentItem(); parent; parent = parent->parentItem())
                    if (!parent->objectName().isEmpty()) { area["namedParent"] = parent->objectName(); break; }
                if (drag)
                    for (const QString &key : {QString("x"), QString("y"), QString("keys")})
                        area[key] = QJsonValue::fromVariant(drag->property(qPrintable(key)));
                if (source)
                {
                    area["sourceName"] = source->objectName();
                    if (VCWidget *widget = source->property("wObj").value<VCWidget *>())
                        area["sourceWidgetID"] = int(widget->id());
                    for (const QString &key : {QString("draggedIDs"), QString("widgetIDs"), QString("itemsList")})
                    {
                        QVariant value = source->property(qPrintable(key));
                        if (value.canConvert<QJSValue>()) value = value.value<QJSValue>().toVariant();
                        area[key] = QJsonValue::fromVariant(value);
                    }
                    const QPointF hotSpot = source->property("dragHotSpot").toPointF();
                    area["dragHotSpot"] = QJsonObject{{"x", hotSpot.x()}, {"y", hotSpot.y()}};
                    area["modifiers"] = source->property("modifiers").toInt();
                }
                dropAreas.append(area);
            }
            if (controller)
            {
                QQmlExpression position(qmlContext(item), item, "placement ? placement.rect : null");
                QQmlExpression held(qmlContext(item), item, "heldPlacement ? heldPlacement.rect : null");
                QQmlExpression error(qmlContext(item), item, "placement ? placement.error : ''");
                namedItems.append(QJsonObject{{"name", item->objectName()}, {"kind", "cornerController"},
                    {"index", item->property("index").toInt()}, {"placement", rectangle(position.evaluate().toRectF())},
                    {"heldPlacement", rectangle(held.evaluate().toRectF())}, {"error", error.evaluate().toString()}});
            }
            if ((item->isVisible() || declaredHandle || fragment) && item->width() > 0 && item->height() > 0)
            {
            if (item->objectName().startsWith("vc"))
            {
                QJsonObject named{{"name", item->objectName()}, {"globalRect", globalRect(item)}, {"visible", item->isVisible()},
                    {"kind", fragment ? "cornerFragment" : declaredHandle ? "cornerImage" : "item"}};
                if (fragment)
                {
                    named["visualParent"] = item->parentItem() ? item->parentItem()->objectName() : QString();
                    QRectF painted = item->mapRectToScene(QRectF(QPointF(), QSizeF(item->width(), item->height())));
                    for (QQuickItem *parent = item->parentItem(); parent; parent = parent->parentItem())
                        if (parent->clip())
                            painted = painted.intersected(parent->mapRectToScene(
                                QRectF(QPointF(), QSizeF(parent->width(), parent->height()))));
                    painted.translate(QPointF(m_app->mapToGlobal(QPoint())));
                    named["paintedGlobalRect"] = rectangle(painted);
                    QList<QRectF> regions;
                    if (item->isVisible() && !painted.isEmpty()) regions.append(painted);
                    for (const QRectF &control : viewportControlRects)
                    {
                        QList<QRectF> next;
                        for (const QRectF &region : regions)
                        {
                            const QRectF intersection = region.intersected(control);
                            if (intersection.isEmpty()) { next.append(region); continue; }
                            const QList<QRectF> pieces{
                                QRectF(region.left(), region.top(), region.width(), intersection.top() - region.top()),
                                QRectF(region.left(), intersection.bottom(), region.width(), region.bottom() - intersection.bottom()),
                                QRectF(region.left(), intersection.top(), intersection.left() - region.left(), intersection.height()),
                                QRectF(intersection.right(), intersection.top(), region.right() - intersection.right(), intersection.height())};
                            for (const QRectF &piece : pieces) if (!piece.isEmpty()) next.append(piece);
                        }
                        regions = next;
                    }
                    QJsonArray inputRegions;
                    for (const QRectF &region : regions) inputRegions.append(rectangle(region));
                    named["viewportInputClear"] = regions.size() == 1 && regions.first() == painted;
                    named["viewportInputGlobalRects"] = inputRegions;
                }
                if (item->objectName().startsWith("vcOutlineRow-") && item->parentItem())
                    for (const QString &key : {QString("rowKind"), QString("widgetID"), QString("rowKey"), QString("parentID"),
                                               QString("subpageIndex"), QString("label"), QString("dropIntent")})
                    {
                        QVariant value = item->parentItem()->property(qPrintable(key));
                        if (value.canConvert<QJSValue>()) value = value.value<QJSValue>().toVariant();
                        named[key] = QJsonValue::fromVariant(value);
                    }
                namedItems.append(named);
            }
            if (item->property("itemType").toInt() == App::FunctionDragItem)
            {
                Function *function = item->property("cRef").value<Function *>();
                if (function && functionIDs.contains(function->id()))
                {
                    if (!functionRow) functionRow = item;
                    functionRows.append(QJsonObject{{"id", int(function->id())}, {"caption", function->name()},
                                                    {"globalRect", globalRect(item)}});
                }
            }
            }
            for (QQuickItem *child : item->childItems()) visit(child);
        };
        visit(m_app->rootObject());
        QQuickItem *outlineItem = console->findChild<QQuickItem *>("vcOutlinePane");
        const QJsonObject internal = liveBoolean(console, "UISettings.internalDragActive");
        const QJsonObject outlineState = liveBoolean(outlineItem, "dragging");
        QJsonObject outlineAttached = liveBoolean(outlineRow, "outlineDrag.Drag.active");
        outlineAttached["kind"] = "outline";
        outlineAttached["owner"] = "vcOutlinePane";
        attachedDrags.append(outlineAttached);
        if (functionRow)
        {
            QJsonObject state = liveBoolean(functionRow, "fDragItem.Drag.active");
            state["kind"] = "function";
            state["owner"] = "FunctionManager";
            attachedDrags.append(state);
        }
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
        QJsonArray startEvents;
        for (const auto &event : starts) startEvents.append(int(event.first().toUInt()));
        int dropCount = 0;
        for (const auto &observer : dropObservers) dropCount += observer->count();
        QJsonObject viewport;
        if (QQuickItem *view = vc->currentPageItem())
        {
            viewport["globalRect"] = globalRect(view);
            for (const QString &key : {QString("contentX"), QString("contentY"), QString("contentWidth"), QString("contentHeight"),
                                       QString("leftMargin"), QString("rightMargin"), QString("topMargin"), QString("bottomMargin"),
                                       QString("interactive")})
                viewport[key] = QJsonValue::fromVariant(view->property(qPrintable(key)));
        }
        QJsonArray pages;
        for (int page = 0; page < vc->pagesCount(); ++page)
            pages.append(QJsonObject{{"id", int(vc->page(page)->id())}, {"index", page},
                {"logicalRect", rectangle(vc->page(page)->logicalGeometry())}, {"pageScale", vc->page(page)->pageScale()}});
        QByteArray xml;
        QXmlStreamWriter writer(&xml);
        vc->saveXML(&writer);
        return QJsonObject{{"schemaVersion", 2}, {"runID", runID}, {"variant", variant}, {"ms", clock.elapsed()},
            {"phase", inputSealed ? "sealed" : "authoring"}, {"testSHA256", testSHA}, {"resourceSHA256", resourceHashes},
            {"pid", int(QCoreApplication::applicationPid())}, {"frameSerial", qint64(frameSerial)},
            {"window", rectangle(QRectF(m_app->position(), m_app->size()))},
            {"clientOrigin", QJsonObject{{"x", m_app->mapToGlobal(QPoint()).x()}, {"y", m_app->mapToGlobal(QPoint()).y()}}},
            {"devicePixelRatio", m_app->devicePixelRatio()}, {"widgets", widgets},
            {"internalDragActive", internal.value("value")}, {"internalDragActiveAvailable", internal.value("available")},
            {"internalDragActiveError", internal.value("error")}, {"outlineDragging", outlineState},
            {"attachedDrags", attachedDrags},
            {"namedItems", namedItems}, {"functionRows", functionRows}, {"loadedResourceMatchesSource", true},
            {"selectedWidgetIDs", selection}, {"outlineFilterText", vc->outlineModel()->filterText()},
            {"editMode", vc->editMode()}, {"order", order}, {"pageScale", vc->page(0)->pageScale()},
            {"snappingEnabled", vc->snapping()}, {"logicalSnapStep", vc->snappingSize()},
            {"functionStartEvents", startEvents}, {"functionStartRequests", startRequests},
            {"functionStates", functionState()}, {"queueViolation", queueViolation}, {"queueSamples", queueSamples},
            {"queueTransitions", queueTransitions}, {"authoringTickCount", inputSealed ? sealedTickCount : authoringTicks.count()},
            {"gestureEvents", gestureEvents}, {"geometryEvents", geometryEvents}, {"geometryCounts", geometryCounts},
            {"dropEventCount", dropCount}, {"structureEventCount", published.count()},
            {"dropAreas", dropAreas},
            {"roles", roles}, {"viewport", viewport}, {"pages", pages}, {"selectedPage", vc->selectedPage()},
            {"viewportControls", viewportControls},
            {"context", contexts->currentContext()}, {"workingPath", m_app->workingPath()},
            {"vcXMLSHA256", QString::fromLatin1(QCryptographicHash::hash(xml, QCryptographicHash::Sha256).toHex())},
            {"fixtureCount", m_app->doc()->fixtures().size()}, {"outputPatchCount", outputPatches},
            {"dirty", m_app->doc()->isModified()}};
    };
    auto inactiveError = [](const QJsonObject &state) {
        if (!state.value("internalDragActiveAvailable").toBool() || !state.value("internalDragActive").isBool())
            return QString("Cannot seal input: UISettings.internalDragActive is unavailable");
        if (state.value("internalDragActive").toBool())
            return QString("Cannot seal input: internal QML drag is active");
        const QJsonObject outline = state.value("outlineDragging").toObject();
        if (!outline.value("available").toBool() || !outline.value("value").isBool())
            return QString("Cannot seal input: live outline dragging state is unavailable");
        if (outline.value("value").toBool())
            return QString("Cannot seal input: outline drag is active");
        for (const auto &value : state.value("widgets").toArray())
        {
            const QJsonObject widget = value.toObject();
            if (!widget.value("renderItemAvailable").toBool()) continue;
            for (const QString &name : {QString("moving"), QString("resizing")})
            {
                if (!widget.value(name + "Available").toBool() || !widget.value(name).isBool())
                    return QString("Cannot seal input: widget %1 %2 state is unavailable").arg(widget.value("id").toInt()).arg(name);
                if (widget.value(name).toBool())
                    return QString("Cannot seal input: widget %1 %2 is active").arg(widget.value("id").toInt()).arg(name);
            }
        }
        for (const auto &value : state.value("attachedDrags").toArray())
        {
            const QJsonObject drag = value.toObject();
            if (!drag.value("available").toBool() || !drag.value("value").isBool())
                return QString("Cannot seal input: %1 attached Drag activity is unavailable").arg(drag.value("owner").toString());
            if (drag.value("value").toBool())
                return QString("Cannot seal input: %1 attached Drag is active").arg(drag.value("owner").toString());
        }
        return QString();
    };
    watchRequests();
    sampleQueue();
    m_app->doc()->resetModified();
    QJsonObject initial;
    QTRY_VERIFY(!(initial = receipt()).value("functionRows").toArray().isEmpty());
    QSaveFile ready(QDir(artifactDirectory).filePath("ready.json"));
    QVERIFY(ready.open(QIODevice::WriteOnly));
    ready.write(QJsonDocument(initial).toJson());
    QVERIFY(ready.commit());
    QVERIFY(m_app->grabWindow().save(QDir(artifactDirectory).filePath("before.png")));
    qInfo() << "Native VC fixture ready" << artifactDirectory;
    auto writeJSON = [&](const QString &name, const QJsonObject &value) {
        QSaveFile file(QDir(artifactDirectory).filePath(name));
        return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(value).toJson()) >= 0 && file.commit();
    };
    int sequence = 0;
    QString protocolError;
    auto serviceCheckpoint = [&]() {
        sampleQueue();
        watchRequests();
        QFile request(QDir(artifactDirectory).filePath("checkpoint-request.json"));
        if (!request.exists()) return;
        if (!request.open(QIODevice::ReadOnly) || request.size() > 512)
        {
            protocolError = "Checkpoint request is unreadable or exceeds 512 bytes";
            return;
        }
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(request.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
        {
            protocolError = "Checkpoint request must be a JSON object";
            return;
        }
        const QJsonObject value = document.object();
        const int next = value.value("sequence").toInt(-1);
        for (const QString &key : value.keys())
            if (key != "runID" && key != "sequence" && key != "label" && key != "minFrameSerial" && key != "screenshot")
                protocolError = "Checkpoint requests accept only read-only receipt fields";
        if (value.value("runID").toString() != runID || next <= 0 || next > 64)
            protocolError = "Checkpoint identity or sequence is invalid";
        if (!protocolError.isEmpty() || next == sequence) return;
        if (next != sequence + 1)
        {
            protocolError = "Checkpoint sequences must be contiguous";
            return;
        }
        if (value.value("label").toString().size() > 64)
        {
            protocolError = "Checkpoint label exceeds 64 characters";
            return;
        }
        if (qint64(frameSerial) < value.value("minFrameSerial").toInteger()) return;
        if (!outlineRelease.isEmpty()) qInfo() << "OUTLINE_RELEASE_CHECKPOINT receipt-enter" << clock.elapsed();
        QJsonObject snapshot = receipt();
        if (!outlineRelease.isEmpty()) qInfo() << "OUTLINE_RELEASE_CHECKPOINT receipt-return" << clock.elapsed();
        snapshot["sequence"] = next;
        snapshot["label"] = value.value("label").toString();
        const QString stem = QString("checkpoint-%1").arg(next, 3, 10, QLatin1Char('0'));
        if (!writeJSON(stem + ".json", snapshot) ||
            (value.value("screenshot").toBool() && !m_app->grabWindow().save(QDir(artifactDirectory).filePath(stem + ".png"))))
        {
            protocolError = "Cannot publish checkpoint";
            return;
        }
        sequence = next;
        if (!outlineRelease.isEmpty()) qInfo() << "OUTLINE_RELEASE_CHECKPOINT published" << clock.elapsed();
    };
    if (!outlineRelease.isEmpty())
    {
        QCOMPARE(vc->page(0)->children(true).size(), 46);
        auto *head1 = qobject_cast<VCFrame *>(vc->widget(roles.value("Head1").toInt()));
        auto *head2 = qobject_cast<VCFrame *>(vc->widget(roles.value("Head2").toInt()));
        QVERIFY(head1 && head2);
        QCOMPARE(head2->currentPage(), 0);
        const QVariantList ids = head1->orderedChildIDs(0).mid(0, 2);
        QCOMPARE(ids.size(), 2);
        auto *first = qobject_cast<VCButton *>(vc->widget(ids[0].toUInt()));
        auto *second = qobject_cast<VCButton *>(vc->widget(ids[1].toUInt()));
        QVERIFY(first && second);
        const QRectF firstRect = first->logicalGeometry(), secondRect = second->logicalGeometry();
        const quint32 firstFunction = first->functionID(), secondFunction = second->functionID();
        auto locate = [&](const QString &name) {
            QQuickItem *found = nullptr;
            std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
                if (item->objectName() == name) found = item;
                for (QQuickItem *child : item->childItems()) visit(child);
            };
            visit(m_app->rootObject());
            return found;
        };
        auto center = [](QQuickItem *item) {
            return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
        };
        for (VCFrame *head : {head2, head1})
        {
            QQuickItem *row = locate(QString("vcOutlineRow-%1").arg(head->id()));
            QVERIFY(row && row->isVisible());
            QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, center(row));
            QTest::keyClick(m_app.get(), Qt::Key_Right);
            const quint64 previousFrame = frameSerial;
            m_app->update();
            QTRY_VERIFY(frameSerial > previousFrame);
        }
        QQuickItem *firstRow = nullptr, *secondRow = nullptr, *targetRow = nullptr;
        QTRY_VERIFY((firstRow = locate(QString("vcOutlineRow-%1").arg(first->id()))));
        QTRY_VERIFY((secondRow = locate(QString("vcOutlineRow-%1").arg(second->id()))));
        QTRY_VERIFY((targetRow = locate(QString("vcOutlineRow-subpage:%1:1").arg(head2->id()))));
        QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, center(firstRow));
        QCOMPARE(vc->selectedWidgetIDs(), QVariantList{first->id()});
        QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::ShiftModifier, center(secondRow));
        QCOMPARE(vc->selectedWidgetIDs(), ids);
        const QPoint press = center(firstRow), drop = center(targetRow);
        QVERIFY(QRect(QPoint(), m_app->size()).contains(drop));
        const auto release = qScopeGuard([&]() {
            if (outline->property("dragging").toBool())
            {
                QTest::keyClick(m_app.get(), Qt::Key_Escape);
                QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, drop);
            }
        });
        QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        QTest::mouseMove(m_app.get(), press + QPoint(15, 0), 20);
        QTest::mouseMove(m_app.get(), press + QPoint(35, 0), 20);
        QTRY_VERIFY(outline->property("dragging").toBool());
        QTest::mouseMove(m_app.get(), drop, 20);
        QQmlExpression intent(qmlContext(targetRow), targetRow, "dropIntent");
        const QVariantMap heldIntent = intent.evaluate().value<QJSValue>().toVariant().toMap();
        QCOMPARE(heldIntent.value("targetID").toUInt(), head2->id());
        QCOMPARE(heldIntent.value("subpage").toInt(), 1);
        QVERIFY(heldIntent.value("into").toBool());
        watchRequests();
        sampleQueue();
        QVERIFY(writeJSON("outline-release-held.json", receipt()));
        QSignalSpy proxyLayouts(vc->outlineModel()->filteredModel(), &QAbstractItemModel::layoutChanged);
        QSignalSpy sourceRemovals(vc->outlineModel(), &QAbstractItemModel::rowsRemoved);
        QSignalSpy sourceInserts(vc->outlineModel(), &QAbstractItemModel::rowsInserted);
        QSignalSpy sourceResets(vc->outlineModel(), &QAbstractItemModel::modelReset);
        qint64 commitMS = -1;
        const qint64 releaseStart = clock.elapsed();
        const auto commitConnection = connect(vc, &VirtualConsole::structureChanged, this, [&]() {
            commitMS = clock.elapsed() - releaseStart;
            QVERIFY(writeJSON("outline-release-model-commit.json", QJsonObject{
                {"ms", clock.elapsed()}, {"commitMS", commitMS},
                {"firstParent", int(qobject_cast<VCFrame *>(first->parent())->id())},
                {"secondParent", int(qobject_cast<VCFrame *>(second->parent())->id())},
                {"firstPage", first->page()}, {"secondPage", second->page()},
                {"targetOrder", QJsonArray::fromVariantList(head2->orderedChildIDs(1))}}));
            qInfo() << "OUTLINE_RELEASE model-commit" << clock.elapsed();
        });
        const auto disconnectCommit = qScopeGuard([&]() { disconnect(commitConnection); });
        qInfo() << "OUTLINE_RELEASE begin" << clock.elapsed() << "ids" << ids << "target" << head2->id()
                << "press" << press << "drop" << drop << "screenshot" << outlineRelease;
        QElapsedTimer latency;
        latency.start();
        QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, drop);
        const qint64 releaseMS = latency.elapsed();
        qInfo() << "OUTLINE_RELEASE returned" << releaseMS << "structure" << published.count();
        QVERIFY(writeJSON("checkpoint-request.json", QJsonObject{{"runID", runID}, {"sequence", 1},
            {"label", "into-head2-page1-committed"}, {"screenshot", outlineRelease == "screenshot"}}));
        QTimer observer;
        observer.setInterval(50);
        connect(&observer, &QTimer::timeout, this, serviceCheckpoint);
        observer.start();
        while (sequence < 1 && latency.elapsed() < 2000 && protocolError.isEmpty()) QTest::qWait(5);
        observer.stop();
        const qint64 checkpointMS = latency.elapsed();
        qInfo() << "OUTLINE_RELEASE checkpoint" << checkpointMS << "sequence" << sequence << "error" << protocolError;
        QVERIFY2(protocolError.isEmpty(), qPrintable(protocolError));
        QCOMPARE(sequence, 1);
        QVERIFY2(checkpointMS < 2000, "Actual outline release/checkpoint exceeded the native two-second deadline");
        QVERIFY(QFile::exists(QDir(artifactDirectory).filePath("checkpoint-001.json")));
        if (outlineRelease == "screenshot")
            QVERIFY(QFile::exists(QDir(artifactDirectory).filePath("checkpoint-001.png")));
        QCOMPARE(published.count(), 1);
        // each moved row leaves head1 and arrives in head2's second page, and no other row moves
        int crossParentMoves = 0;
        for (const auto &removal : sourceRemovals)
        {
            QCOMPARE(removal[0].value<QModelIndex>().data(VCOutlineModel::RowKeyRole).toString(), QString("widget:%1").arg(head1->id()));
            QCOMPARE(removal[2].toInt() - removal[1].toInt() + 1, 1);
            ++crossParentMoves;
        }
        for (const auto &insert : sourceInserts)
        {
            QCOMPARE(insert[0].value<QModelIndex>().data(VCOutlineModel::RowKeyRole).toString(), QString("subpage:%1:1").arg(head2->id()));
            QCOMPARE(insert[2].toInt() - insert[1].toInt() + 1, 1);
        }
        QCOMPARE(crossParentMoves, 2);
        QCOMPARE(sourceInserts.count(), 2);
        QCOMPARE(sourceResets.count(), 0);
        QCOMPARE(proxyLayouts.count(), 0);
        QCOMPARE(receipt().value("dropEventCount").toInt(), 1);
        QCOMPARE(first->parent(), head2);
        QCOMPARE(second->parent(), head2);
        QCOMPARE(first->page(), 1);
        QCOMPARE(second->page(), 1);
        QCOMPARE(first->logicalGeometry(), firstRect);
        QCOMPARE(second->logicalGeometry(), secondRect);
        QCOMPARE(first->functionID(), firstFunction);
        QCOMPARE(second->functionID(), secondFunction);
        QCOMPARE(head2->orderedChildIDs(1), ids);
        QCOMPARE(head2->currentPage(), 0);
        QVERIFY(first->renderItem() && !first->renderItem()->isVisible());
        QVERIFY(second->renderItem() && !second->renderItem()->isVisible());
        QCOMPARE(vc->selectedWidgetIDs(), ids);
        QVERIFY(!outline->property("dragging").toBool());
        QVERIFY2(inactiveError(receipt()).isEmpty(), qPrintable(inactiveError(receipt())));
        auto inventoryMatches = [&]() {
            QVariant expanded = outline->property("expandedKeys");
            if (expanded.canConvert<QJSValue>()) expanded = expanded.value<QJSValue>().toVariant();
            QStringList actual;
            std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
                if (item->isVisible() && item->objectName().startsWith("vcOutlineRow-"))
                {
                    QQmlExpression key(qmlContext(item), item, "node.rowKey");
                    actual.append(key.evaluate().toString());
                }
                for (QQuickItem *child : item->childItems()) visit(child);
            };
            visit(outline);
            QStringList expected = vc->outlineModel()->visibleKeys(expanded.toStringList());
            actual.sort();
            expected.sort();
            return actual == expected;
        };
        QTRY_VERIFY(inventoryMatches());
        QCOMPARE(starts.count(), 0);
        QVERIFY(!queueViolation);
        QVERIFY(writeJSON("outline-release-proof.json", QJsonObject{{"releaseMS", releaseMS}, {"checkpointMS", checkpointMS},
            {"commitMS", commitMS}, {"sourceRemovals", sourceRemovals.count()}, {"sourceResets", sourceResets.count()},
            {"crossParentMoves", crossParentMoves}, {"proxyLayouts", proxyLayouts.count()}, {"state", receipt()}}));
        return;
    }
    QPoint heldRelease;
    QJsonObject heldBefore;
    const auto releaseProbe = qScopeGuard([&]() {
        if (heldGesture.isEmpty()) return;
        QTest::keyClick(m_app.get(), Qt::Key_Escape);
        QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, heldRelease);
    });
    if (!heldGesture.isEmpty())
    {
        const quint32 id = roles.value("Scenes").toInt();
        VCWidget *widget = vc->widget(id);
        QVERIFY(widget && widget->renderItem());
        auto locate = [&](const QString &name) {
            QQuickItem *found = nullptr;
            std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
                if (item->objectName() == name) found = item;
                for (QQuickItem *child : item->childItems()) visit(child);
            };
            visit(m_app->rootObject());
            return found;
        };
        QQuickItem *source = heldGesture == "outline" ? locate(QString("vcOutlineRow-%1").arg(id)) : widget->renderItem();
        QVERIFY(source);
        const QPoint selected = heldGesture == "outline" ?
            source->mapToScene(QPointF(source->width() * 0.7, source->height() / 2)).toPoint() :
            source->mapToScene(QPointF(source->width() * 0.95, source->height() * 0.95)).toPoint();
        QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, selected);
        QCOMPARE(vc->selectedWidgetIDs(), QVariantList{id});
        QSignalSpy selectedPaint(m_app.get(), &QQuickWindow::frameSwapped);
        m_app->update();
        QTRY_VERIFY(!selectedPaint.isEmpty());
        if (heldGesture == "resize")
            source = locate(QString("vcResizeHandle-%1-2").arg(id));
        else if (heldGesture == "outline")
            source = locate(QString("vcOutlineRow-%1").arg(id));
        else source = widget->renderItem();
        QVERIFY(source);
        const QPoint press = heldGesture == "resize" ?
            source->mapToScene(QPointF(source->width() / 2, source->height() / 2)).toPoint() :
            heldGesture == "outline" ? source->mapToScene(QPointF(source->width() * 0.7, source->height() / 2)).toPoint() :
            source->mapToScene(QPointF(source->width() * 0.95, source->height() * 0.95)).toPoint();
        heldBefore = receipt();
        heldRelease = press;
        QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        if (heldGesture != "resize")
        {
            QTest::mouseMove(m_app.get(), press + QPoint(12, 8), 20);
            heldRelease = press + QPoint(36, 24);
            QTest::mouseMove(m_app.get(), heldRelease, 20);
        }
        if (heldGesture == "outline")
        {
            QQuickItem *outlineItem = locate("vcOutlinePane");
            QQmlExpression pressed(qmlContext(source), source, "mouseArea.pressed");
            QQmlExpression active(qmlContext(source), source, "mouseArea.drag.active");
            qInfo() << "HELD_OUTLINE_PROBE" << press << heldRelease << "pressed" << pressed.evaluate()
                    << "active" << active.evaluate() << "outlineDragging" << (outlineItem ? outlineItem->property("dragging") : QVariant());
            QVERIFY(outlineItem && outlineItem->property("dragging").toBool());
        }
        else QVERIFY(widget->renderItem()->property(heldGesture == "resize" ? "resizing" : "moving").toBool());
        QVERIFY(writeJSON("held-fixture.json", receipt()));
        QVERIFY(writeJSON("checkpoint-request.json", QJsonObject{{"runID", runID}, {"sequence", 1}, {"label", "held-early-done"}}));
        serviceCheckpoint();
        QCOMPARE(sequence, 1);
        QVERIFY(writeJSON("input-finished.json", QJsonObject{{"runID", runID}, {"lastSequence", 1}}));
    }
    if (hold > 0 || !heldGesture.isEmpty())
    {
        QTimer observer;
        observer.setInterval(50);
        connect(&observer, &QTimer::timeout, this, serviceCheckpoint);
        observer.start();
        QElapsedTimer lease;
        lease.start();
        bool finished = false;
        while (lease.elapsed() < (hold > 0 ? hold : 5000) && protocolError.isEmpty() && !finished)
        {
            QTest::qWait(50);
            QFile done(QDir(artifactDirectory).filePath("input-finished.json"));
            if (!done.exists()) continue;
            if (!done.open(QIODevice::ReadOnly) || done.size() > 512)
            {
                protocolError = "Input-finished fence is unreadable or oversized";
                break;
            }
            const QJsonObject value = QJsonDocument::fromJson(done.readAll()).object();
            finished = value.value("runID").toString() == runID &&
                       value.value("lastSequence").toInt(-1) == sequence && sequence > 0;
            if (!finished) protocolError = "Input-finished identity or sequence is invalid";
            if (finished) protocolError = inactiveError(receipt());
        }
        observer.stop();
        if (!heldGesture.isEmpty())
        {
            QVERIFY2(!protocolError.isEmpty(), "Early input-finished was admitted while the public QQuickView still held an authoring gesture");
            QVERIFY2(protocolError.contains("active"), qPrintable(protocolError));
            QVERIFY(writeJSON("pre-seal-refused.json", QJsonObject{{"reason", protocolError}, {"state", receipt()}}));
            QCOMPARE(receipt().value("vcXMLSHA256"), heldBefore.value("vcXMLSHA256"));
            QCOMPARE(receipt().value("dirty"), heldBefore.value("dirty"));
            QVERIFY(!QFile::exists(QDir(artifactDirectory).filePath("input-seal.json")));
            QVERIFY(!QFile::exists(QDir(artifactDirectory).filePath("console.qxw")));
            return;
        }
        QVERIFY2(protocolError.isEmpty(), qPrintable(protocolError));
        QVERIFY2(finished, "Input lease expired. No save or replacement load is admitted without input-finished.");
    }
    else
    {
        const QJsonObject unavailable = liveBoolean(console, "UISettings.__unavailableNativePreflightProperty");
        QVERIFY(!unavailable.value("available").toBool());
        QVERIFY(unavailable.value("value").isNull());
        QJsonObject missingState = receipt();
        missingState["internalDragActiveAvailable"] = false;
        missingState["internalDragActive"] = QJsonValue::Null;
        QVERIFY(inactiveError(missingState).contains("unavailable"));
        QVERIFY(writeJSON("checkpoint-request.json", QJsonObject{{"runID", runID}, {"sequence", 1}, {"label", "smoke"}}));
        serviceCheckpoint();
        QVERIFY2(protocolError.isEmpty(), qPrintable(protocolError));
        QCOMPARE(sequence, 1);
        QFile checkpoint(QDir(artifactDirectory).filePath("checkpoint-001.json"));
        QVERIFY(checkpoint.open(QIODevice::ReadOnly));
        const QJsonObject value = QJsonDocument::fromJson(checkpoint.readAll()).object();
        QCOMPARE(value.value("vcXMLSHA256"), initial.value("vcXMLSHA256"));
        QCOMPARE(value.value("dirty"), initial.value("dirty"));
        const quint32 observedID = roles.value(variant == "six-groups" ? "Scenes" : "B").toInt();
        VCWidget *observed = vc->widget(observedID);
        QVERIFY(observed && observed->renderItem());
        const QPoint blank = observed->renderItem()->mapToScene(
            QPointF(observed->renderItem()->width() * 0.95, observed->renderItem()->height() * 0.95)).toPoint();
        QVERIFY(QRect(QPoint(), m_app->size()).contains(blank));
        QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, blank);
        QCOMPARE(vc->selectedWidgetIDs(), QVariantList{observedID});
        QSignalSpy selectedPaint(m_app.get(), &QQuickWindow::frameSwapped);
        m_app->update();
        QTRY_VERIFY(!selectedPaint.isEmpty());
        watchRequests();
        QQuickItem *corner = nullptr;
        std::function<void(QQuickItem *)> locate = [&](QQuickItem *item) {
            if (item->objectName() == QString("vcResizeHandle-%1-2").arg(observedID)) corner = item;
            for (QQuickItem *child : item->childItems()) locate(child);
        };
        locate(m_app->rootObject());
        QVERIFY(corner && corner->isVisible());
        const QJsonObject before = receipt();
        const QPoint press = corner->mapToScene(QPointF(corner->width() / 2, corner->height() / 2)).toPoint();
        QVERIFY(QRect(QPoint(), m_app->size()).contains(press));
        QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        QVERIFY(observed->renderItem()->property("resizing").toBool());
        QVERIFY(writeJSON("observer-held.json", receipt()));
        QTest::keyClick(m_app.get(), Qt::Key_Escape);
        QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        QVERIFY(!observed->renderItem()->property("resizing").toBool());
        bool pressed = false, canceled = false;
        for (const auto &event : gestureEvents)
            if (event.toObject().value("widgetID").toInt() == int(observedID) &&
                event.toObject().value("state").toString() == "resizing")
            {
                pressed |= event.toObject().value("active").toBool();
                canceled |= !event.toObject().value("active").toBool();
            }
        QVERIFY(pressed && canceled);
        QCOMPARE(receipt().value("vcXMLSHA256"), before.value("vcXMLSHA256"));
        QCOMPARE(receipt().value("dirty"), before.value("dirty"));
        QVERIFY(writeJSON("observer-canceled.json", receipt()));
    }
    const QString sealError = inactiveError(receipt());
    QVERIFY2(sealError.isEmpty(), qPrintable(sealError));
    sampleQueue();
    sealedTickCount = authoringTicks.count();
    inputSealed = true;
    QVERIFY(m_app->grabWindow().save(QDir(artifactDirectory).filePath("after.png")));
    QFile final(QDir(artifactDirectory).filePath("after.json"));
    QVERIFY(final.open(QIODevice::WriteOnly));
    const QJsonObject beforeReload = receipt();
    final.write(QJsonDocument(beforeReload).toJson());
    final.close();
    QVERIFY(writeJSON("pre-reload.json", beforeReload));
    QSet<int> originalIDs;
    for (const auto &value : initial.value("widgets").toArray())
        originalIDs.insert(value.toObject().value("id").toInt());
    QJsonArray droppedButtons;
    for (const auto &value : beforeReload.value("widgets").toArray())
        if (!originalIDs.contains(value.toObject().value("id").toInt()) &&
            value.toObject().value("type").toString() == "Button")
            droppedButtons.append(value);
    QVERIFY(writeJSON("input-seal.json", QJsonObject{{"runID", runID}, {"lastSequence", sequence},
        {"inputFinished", hold > 0}, {"offscreenSmoke", hold == 0}, {"state", beforeReload},
        {"droppedButtons", droppedButtons}}));
    for (quint32 id : functionIDs)
    {
        QVERIFY(!m_app->doc()->function(id)->isRunning());
        QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(m_app->doc()->function(id)));
    }
    QCOMPARE(starts.count(), 0);
    QVERIFY(startRequests.isEmpty());
    QVERIFY(!queueViolation);
    QCOMPARE(authoringTicks.count(), 0);
    auto canonical = [](const QJsonObject &value) {
        std::vector<QJsonObject> widgets, orders;
        for (const auto &record : value.value("widgets").toArray())
        {
            const QJsonObject source = record.toObject();
            QJsonObject widget;
            for (const QString &key : {QString("id"), QString("type"), QString("caption"), QString("subpage"),
                QString("logicalRect"), QString("parentID"), QString("functionID"), QString("actionType"),
                QString("controlledFunctionID"), QString("currentPage"), QString("totalPages"), QString("collapsed")})
                if (source.contains(key)) widget[key] = source.value(key);
            widgets.push_back(widget);
        }
        std::sort(widgets.begin(), widgets.end(), [](const auto &a, const auto &b) {
            return a.value("id").toInt() < b.value("id").toInt();
        });
        for (const auto &record : value.value("order").toArray()) orders.push_back(record.toObject());
        std::sort(orders.begin(), orders.end(), [](const auto &a, const auto &b) {
            if (a.value("parentID").toInt() != b.value("parentID").toInt())
                return a.value("parentID").toInt() < b.value("parentID").toInt();
            return a.value("subpage").toInt() < b.value("subpage").toInt();
        });
        QJsonArray widgetValues, orderValues;
        for (const auto &widget : widgets) widgetValues.append(widget);
        for (const auto &order : orders) orderValues.append(order);
        return QJsonObject{{"widgets", widgetValues}, {"order", orderValues}};
    };
    const QJsonObject persisted = canonical(beforeReload);
    m_app->doc()->setModified();
    const QJsonObject beforeFailedSave = receipt();
    QVERIFY(!m_app->saveWorkspace(QDir(artifactDirectory).filePath("missing-parent/failed.qxw")));
    closeSaveErrorPopup(m_app->rootObject());
    QVERIFY(m_app->doc()->isModified());
    QCOMPARE(canonical(receipt()), persisted);
    const QJsonObject afterFailedSave = receipt();
    QCOMPARE(afterFailedSave.value("vcXMLSHA256"), beforeFailedSave.value("vcXMLSHA256"));
    QCOMPARE(afterFailedSave.value("selectedWidgetIDs"), beforeFailedSave.value("selectedWidgetIDs"));
    QCOMPARE(afterFailedSave.value("viewport"), beforeFailedSave.value("viewport"));
    QCOMPARE(afterFailedSave.value("workingPath"), beforeFailedSave.value("workingPath"));
    QVERIFY(writeJSON("failed-save.json", QJsonObject{{"before", beforeFailedSave}, {"after", afterFailedSave}}));
    const QString workspace = QDir(artifactDirectory).filePath("console.qxw");
    QVERIFY(m_app->saveWorkspace(workspace));
    QVERIFY(!m_app->doc()->isModified());
    QVERIFY(writeJSON("saved.json", receipt()));
    QPointer<VCWidget> replaced(vc->widget(roots.first().toUInt()));
    QSignalSpy ticks(m_app->doc()->masterTimer(), &MasterTimer::tickReady);
    QVERIFY(m_app->loadWorkspace(workspace));
    QTRY_VERIFY(!ticks.isEmpty());
    m_app->doc()->masterTimer()->stop();
    QVERIFY(replaced.isNull());
    QVERIFY(!m_app->doc()->isModified());
    const QJsonObject reloaded = canonical(receipt());
    QCOMPARE(reloaded, persisted);
    QVERIFY(writeJSON("reloaded.json", receipt()));
    QFile roundtrip(QDir(artifactDirectory).filePath("roundtrip.json"));
    QVERIFY(roundtrip.open(QIODevice::WriteOnly));
    roundtrip.write(QJsonDocument(QJsonObject{{"before", persisted}, {"after", reloaded},
        {"failedSavePreservedStateAndDirty", true}, {"saveClean", true}, {"replacementLoadClean", true},
        {"runID", runID}, {"variant", variant}, {"testSHA256", testSHA}, {"resourceSHA256", resourceHashes},
        {"functionStartEventCount", starts.count()}, {"functionStartRequests", startRequests},
        {"queueViolation", queueViolation}, {"queueSamples", queueSamples}, {"queueTransitions", queueTransitions},
        {"droppedButtons", droppedButtons}}).toJson());
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

void UpstreamIntegration_Test::vcSliderCopy_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("attribute");
    QTest::newRow("level") << int(VCSlider::Level) << int(Function::Intensity);
    QTest::newRow("adjust-intensity") << int(VCSlider::Adjust) << int(Function::Intensity);
    QTest::newRow("adjust-width") << int(VCSlider::Adjust) << int(EFX::Width);
    QTest::newRow("submaster") << int(VCSlider::Submaster) << int(Function::Intensity);
}

void UpstreamIntegration_Test::vcSliderCopy()
{
    QFETCH(int, mode);
    QFETCH(int, attribute);
    Doc *doc = m_app->doc();
    auto *efx = new EFX(doc);
    QVERIFY(doc->addFunction(efx));
    const auto cleanup = qScopeGuard([&]() { QVERIFY(doc->deleteFunction(efx->id())); });
    VCFrame frame(doc, m_app->virtualConsole());
    VCSlider source(doc, &frame);
    source.setSliderMode(static_cast<VCSlider::SliderMode>(mode));
    if (mode == VCSlider::Adjust)
    {
        source.setControlledFunction(efx->id());
        source.setControlledAttribute(attribute);
        source.setAdjustFlashEnabled(true);
    }
    source.setRangeLowLimit(20);
    source.setRangeHighLimit(100);
    source.setClickAndGoType(VCSlider::CnGColors);
    const QColor primary(12, 90, 180), secondary(30, 60, 90);
    source.setClickAndGoColors(primary, secondary);
    source.setValue(72, false, false);
    source.setMonitorEnabled(mode == VCSlider::Level);
    source.addKeySequence(QKeySequence(Qt::Key_F8), 2);
    source.setLogicalGeometry(QRectF(20, 30, 80, 200));
    source.ensureRecordingId();
    std::unique_ptr<VCWidget> copied(source.createCopy(&frame));
    auto *copy = qobject_cast<VCSlider *>(copied.get());
    QVERIFY(copy);
    QCOMPARE(copy->sliderMode(), source.sliderMode());
    QCOMPARE(copy->controlledFunction(), source.controlledFunction());
    QCOMPARE(copy->controlledAttribute(), source.controlledAttribute());
    QCOMPARE(copy->attributeMinValue(), source.attributeMinValue());
    QCOMPARE(copy->attributeMaxValue(), source.attributeMaxValue());
    QCOMPARE(copy->adjustFlashEnabled(), source.adjustFlashEnabled());
    QCOMPARE(copy->rangeLowLimit(), qreal(20));
    QCOMPARE(copy->rangeHighLimit(), qreal(100));
    QCOMPARE(copy->value(), source.value());
    QCOMPARE(copy->clickAndGoType(), VCSlider::CnGColors);
    QCOMPARE(copy->cngPrimaryColor(), primary);
    QCOMPARE(copy->cngSecondaryColor(), secondary);
    QCOMPARE(copy->monitorEnabled(), source.monitorEnabled());
    QCOMPARE(copy->keySequenceMap(), source.keySequenceMap());
    QCOMPARE(copy->logicalGeometry(), source.logicalGeometry());
    QVERIFY(copy->recordingId().isNull());
    QVERIFY(!efx->isRunning());
    QVERIFY(!doc->masterTimer()->isStartQueued(efx));
}

void UpstreamIntegration_Test::faderHandleScaling_data()
{
    QTest::addColumn<qreal>("width");
    QTest::addColumn<qreal>("scale");
    for (qreal width : {qreal(12), qreal(120)})
        for (qreal scale : {qreal(0.5), qreal(1), qreal(2)})
            QTest::newRow(qPrintable(QString("width-%1-scale-%2").arg(width).arg(scale))) << width << scale;
}

void UpstreamIntegration_Test::faderHandleScaling()
{
    QFETCH(qreal, width);
    QFETCH(qreal, scale);
    QQmlComponent component(m_app->engine(), QUrl("qrc:/QLCPlusFader.qml"));
    std::unique_ptr<QObject> object(component.create());
    QVERIFY2(object, qPrintable(component.errorString()));
    QVERIFY(object->setProperty("width", width));
    QVERIFY(object->setProperty("scaleFactor", scale));
    auto *handle = qvariant_cast<QQuickItem *>(object->property("handle"));
    QVERIFY(handle);
    QQmlExpression iconSize(qmlContext(object.get()), object.get(), "UISettings.iconSizeDefault");
    const qreal defaultWidth = iconSize.evaluate().toReal();
    QVERIFY2(!iconSize.hasError(), qPrintable(iconSize.error().toString()));
    QVERIFY(defaultWidth > 0);
    const qreal expected = qMin(defaultWidth * scale, width);
    QTRY_COMPARE(handle->implicitWidth(), expected);
    QCOMPARE(handle->implicitHeight(), expected * qreal(0.75));
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
    closeSaveErrorPopup(m_app->rootObject());
    QVERIFY(m_app->doc()->isModified());
}

void UpstreamIntegration_Test::vcOutlinePointerSelection_data()
{
    QTest::addColumn<int>("modifier");
    QTest::addColumn<bool>("smallMove");
    QTest::addColumn<bool>("filteredRange");
    for (int modifier : {int(Qt::NoModifier), int(Qt::ControlModifier), int(Qt::MetaModifier), int(Qt::ShiftModifier)})
        for (bool move : {false, true})
            QTest::newRow(qPrintable(QString("%1-%2").arg(modifier).arg(move)))
                << modifier << move << false;
    QTest::newRow("filtered-shift-focus-hidden-page") << int(Qt::ShiftModifier) << false << true;
}

void UpstreamIntegration_Test::vcOutlinePointerSelection()
{
    QFETCH(int, modifier);
    QFETCH(bool, smallMove);
    QFETCH(bool, filteredRange);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode();
    const QSize oldSize = m_app->size();
    const bool wasModified = m_app->doc()->isModified();
    const QString oldFilter = vc->outlineModel()->filterText();
    const int oldPage = vc->selectedPage();
    QVariantList roots;
    for (int index = 0; index < (filteredRange ? 3 : 2); ++index)
    {
        const int page = filteredRange && index == 2 ? 1 : 0;
        const QVariantMap result = vc->createWidgetGeometry(vc->page(page)->id(), "Frame",
            QRectF(20 + (page ? 0 : index) * 350, 20, 300, 200));
        QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
        roots.append(result.value("widgetIDs").toList().first());
        if (filteredRange)
            vc->widget(roots.last().toUInt())->setCaption(index == 1 ? "Unmatched" : "Range");
    }
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets(roots);
        vc->outlineModel()->setFilterText(oldFilter);
        vc->setSelectedPage(oldPage);
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
    if (filteredRange)
    {
        vc->setSelectedPage(0);
        QTRY_VERIFY(vc->outlineModel()->indexForKey(QString("widget:%1").arg(roots[0].toUInt())).isValid());
        QTRY_VERIFY(vc->outlineModel()->indexForKey(QString("widget:%1").arg(roots[2].toUInt())).isValid());
        auto *search = console->findChild<QQuickItem *>("vcOutlineSearch");
        QVERIFY(search);
        QVERIFY(search->setProperty("text", "Range"));
        search->forceActiveFocus();
        QTest::keyClick(m_app.get(), Qt::Key_Left);
        QTRY_COMPARE(vc->outlineModel()->filterText(), QString("Range"));
        QVERIFY(!vc->outlineModel()->indexForKey(QString("widget:%1").arg(roots[1].toUInt())).isValid());
    }
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
    const QVariant secondID = roots[filteredRange ? 2 : 1];
    QTRY_VERIFY((second = findRow(secondID.toUInt())));
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    const QPoint firstPoint = first->mapToScene(QPointF(first->width() * 0.45, first->height() / 2)).toPoint();
    const QPoint secondPoint = second->mapToScene(QPointF(second->width() * 0.45, second->height() / 2)).toPoint();
    QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, firstPoint);
    QTRY_COMPARE(vc->selectedWidgetIDs(), QVariantList{roots[0]});
    if (filteredRange)
    {
        console->forceActiveFocus();
        QCOMPARE(vc->selectedWidgetIDs(), QVariantList{roots[0]});
    }
    const QVariantList order = vc->page(0)->orderedChildIDs(0);
    m_app->doc()->resetModified();
    QSignalSpy changed(vc, &VirtualConsole::structureChanged);
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::KeyboardModifiers(modifier), secondPoint);
    if (smallMove) QTest::mouseMove(m_app.get(), secondPoint + QPoint(2, 0));
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::KeyboardModifiers(modifier),
                        secondPoint + (smallMove ? QPoint(2, 0) : QPoint()));
    const QVariantList expected = modifier == Qt::NoModifier ? QVariantList{secondID}
        : filteredRange ? QVariantList{roots[0], secondID} : roots;
    QTRY_COMPARE(vc->selectedWidgetIDs(), expected);
    if (filteredRange)
    {
        QCOMPARE(vc->selectedPage(), 0);
        QVERIFY(!vc->widget(secondID.toUInt())->renderItem());
        QTRY_VERIFY(first->parentItem()->property("selected").toBool());
        QTRY_VERIFY(second->parentItem()->property("selected").toBool());
    }
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
    // Geometry fixtures use a fixed scale, independent of the offscreen screen size.
    m_app->m_uiManager->setModified("scalingFactor", 1.0);
    m_app->resize(1200, 800);
    m_app->doc()->masterTimer()->stop();
    QVERIFY2(m_app->status() == QQuickView::Ready, qPrintable(m_app->errors().value(0).toString()));
    QQmlExpression startupNotice(qmlContext(m_app->rootObject()), m_app->rootObject(), "scalingInfoPopup");
    auto *notice = startupNotice.evaluate().value<QObject *>();
    QVERIFY2(notice, qPrintable(startupNotice.error().toString()));
    // Fresh offscreen settings can open the modal first-use scaling notice.
    if (notice->property("visible").toBool())
    {
        QCOMPARE(notice->property("title").toString(), QString("Small screen detected"));
        QVERIFY(QMetaObject::invokeMethod(notice, "accept"));
        QTRY_VERIFY(!notice->property("visible").toBool());
    }
    QTRY_COMPARE(m_app->rootObject()->property("popupCount").toInt(), 0);
}

void UpstreamIntegration_Test::cleanupTestCase()
{
    m_app.reset();
}

void UpstreamIntegration_Test::fixtureGroupSelection_data()
{
    QTest::addColumn<QString>("operation");
    QTest::addColumn<QVariantList>("expected");
    QTest::newRow("fixture-heads") << QString("fixture") << QVariantList{0, 2, 5};
    QTest::newRow("single-head") << QString("head") << QVariantList{2};
    QTest::newRow("control-add") << QString("control") << QVariantList{0, 2, 5};
    QTest::newRow("shift-range-skips-holes") << QString("range") << QVariantList{0, 2, 5};
    QTest::newRow("nonmember-keeps-selection") << QString("missing") << QVariantList{5};
    QTest::newRow("reset-selection") << QString("reset") << QVariantList{};
}

void UpstreamIntegration_Test::fixtureGroupSelection()
{
    QFETCH(QString, operation);
    QFETCH(QVariantList, expected);
    Doc *doc = m_app->doc();
    auto *fixture = new Fixture(doc);
    fixture->setChannels(3);
    QVERIFY(doc->addFixture(fixture));
    const auto cleanup = qScopeGuard([&]() { doc->deleteFixture(fixture->id()); });
    FixtureGroup group(doc);
    group.setSize(QSize(3, 2));
    QVERIFY(group.assignHead(QLCPoint(0, 0), GroupHead(fixture->id(), 0)));
    QVERIFY(group.assignHead(QLCPoint(2, 0), GroupHead(fixture->id(), 1)));
    QVERIFY(group.assignHead(QLCPoint(2, 1), GroupHead(fixture->id(), 2)));
    FixtureGroupEditor editor(m_app.get(), doc, m_app->m_fixtureManager);
    editor.setEditGroup(QVariant::fromValue(&group));
    QCOMPARE(editor.groupSelection(2, 1, 0), QVariantList{5});
    QSignalSpy changed(&editor, &FixtureGroupEditor::selectionDataChanged);
    if (operation == "range")
    {
        editor.resetSelection();
        QCOMPARE(editor.groupSelection(0, 0, 0), QVariantList{0});
        editor.groupSelection(2, 1, Qt::ShiftModifier);
    }
    else if (operation == "reset")
        editor.resetGroup();
    else
    {
        const int head = operation == "fixture" ? -1 : operation == "control" ? 0 : 1;
        const quint32 id = operation == "missing" ? Fixture::invalidId()
            : FixtureUtils::fixtureItemID(fixture->id(), 0, 0);
        QCOMPARE(editor.selectFixtureHeads(id, head, operation == "control" ? int(Qt::ControlModifier) : 0),
                 operation != "missing");
        if (operation == "control")
            QVERIFY(editor.selectFixtureHeads(FixtureUtils::fixtureItemID(fixture->id(), 0, 0),
                                              1, Qt::ControlModifier));
        if (operation != "missing")
            QVERIFY(!changed.isEmpty());
    }
    QCOMPARE(editor.selectionData(), expected);
}

void UpstreamIntegration_Test::defaultUiScaling_data()
{
    QTest::addColumn<bool>("userModified");
    QTest::newRow("follow-screen-default") << false;
    QTest::newRow("preserve-explicit-scale") << true;
}

void UpstreamIntegration_Test::defaultUiScaling()
{
    QFETCH(bool, userModified);
    UiManager manager(m_app.get(), m_app->doc());
    manager.setDefaultScalingFactor(0.65);
    QCOMPARE(manager.getDefault("scalingFactor").toReal(), 0.65);
    QCOMPARE(manager.getModified("scalingFactor").toReal(), 0.65);
    UiManager *applicationManager = m_app->m_uiManager;
    const qreal previousDefault = applicationManager->getDefault("scalingFactor").toReal();
    const qreal previousScale = applicationManager->getModified("scalingFactor").toReal();
    const auto restore = qScopeGuard([&]() {
        applicationManager->setDefaultScalingFactor(previousDefault);
        applicationManager->setModified("scalingFactor", previousScale);
    });
    applicationManager->setModified("scalingFactor", previousDefault);
    if (userModified)
        applicationManager->setModified("scalingFactor", 1.25);
    applicationManager->setDefaultScalingFactor(0.75);
    QCOMPARE(applicationManager->getDefault("scalingFactor").toReal(), 0.75);
    QCOMPARE(applicationManager->getModified("scalingFactor").toReal(), userModified ? 1.25 : 0.75);
    QQmlExpression scale(qmlContext(m_app->rootObject()), m_app->rootObject(), "UISettings.scalingFactor");
    QCOMPARE(scale.evaluate().toReal(), userModified ? 1.25 : 0.75);
    QVERIFY(!scale.hasError());
}

void UpstreamIntegration_Test::uiSettingsInitialization_data()
{
    QTest::addColumn<double>("defaultScale");
    QTest::addColumn<QString>("preset");
    QTest::newRow("first-small-screen") << 0.65 << QString();
    QTest::newRow("first-desktop-screen") << 1.0 << QString();
    QTest::newRow("saved-classic") << 0.65 << QString("Classic");
    QTest::newRow("saved-dark") << 0.65 << QString("VS Code Dark");
}

void UpstreamIntegration_Test::uiSettingsInitialization()
{
    QFETCH(double, defaultScale);
    QFETCH(QString, preset);
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QByteArray oldHome = qgetenv("HOME");
    const QByteArray oldProfile = qgetenv("USERPROFILE");
    const auto restore = qScopeGuard([&]() {
        if (oldHome.isNull()) qunsetenv("HOME"); else qputenv("HOME", oldHome);
        if (oldProfile.isNull()) qunsetenv("USERPROFILE"); else qputenv("USERPROFILE", oldProfile);
    });
    qputenv("HOME", home.path().toUtf8());
    qputenv("USERPROFILE", home.path().toUtf8());
    QQuickView view;
    view.rootContext()->setContextProperty("screenPixelDensity", 2.0);
    UiManager manager(&view, m_app->doc());
    manager.setDefaultScalingFactor(defaultScale);
    QFile config(manager.userConfFilepath());
    if (!preset.isEmpty())
    {
        QVERIFY(config.open(QIODevice::WriteOnly));
        const QByteArray saved = QJsonDocument(QJsonObject{
            {"preset", preset}, {"sizes", QJsonObject{{"scalingFactor", 1.25}}},
            {"colors", QJsonObject{{"highlight", "#123456"}}}
        }).toJson();
        QCOMPARE(config.write(saved), saved.size());
        config.close();
    }
    QCOMPARE(manager.initialize(), preset.isEmpty() && defaultScale < 1.0);
    QCOMPARE(manager.currentPreset(), preset.isEmpty() ? QString("VS Code Dark") : preset);
    QCOMPARE(manager.getModified("scalingFactor").toReal(), preset.isEmpty() ? defaultScale : 1.25);
    if (preset.isEmpty())
    {
        QVERIFY(config.open(QIODevice::ReadOnly));
        const QJsonObject saved = QJsonDocument::fromJson(config.readAll()).object();
        QCOMPARE(saved.value("sizes").toObject().value("scalingFactor").toDouble(), defaultScale);
        QCOMPARE(saved.value("preset").toString(), QString("VS Code Dark"));
    }
    else
        QCOMPARE(manager.getModified("highlight").toString(), QString("#123456"));
}

void UpstreamIntegration_Test::emptyVirtualConsole_data()
{
    QTest::addColumn<int>("initialPages");
    QTest::newRow("one-page") << 1;
    QTest::newRow("multiple-pages") << 3;
}

void UpstreamIntegration_Test::emptyVirtualConsole()
{
    QFETCH(int, initialPages);
    VirtualConsole *vc = m_app->virtualConsole();
    const int originalPages = vc->pagesCount();
    const int originalSelection = vc->selectedPage();
    QString original;
    QXmlStreamWriter writer(&original);
    QVERIFY(vc->saveXML(&writer));
    const auto restore = qScopeGuard([&]() {
        QBuffer buffer;
        buffer.setData(original.toUtf8());
        QVERIFY(buffer.open(QIODevice::ReadOnly));
        QXmlStreamReader reader(&buffer);
        QVERIFY(reader.readNextStartElement());
        QVERIFY(vc->loadXML(reader));
        QVERIFY(!reader.hasError());
        vc->postLoad();
        QCOMPARE(vc->pagesCount(), originalPages);
        vc->setSelectedPage(originalSelection);
    });
    while (vc->pagesCount() < initialPages)
        vc->addPage(vc->pagesCount());
    while (vc->pagesCount() > initialPages)
        vc->deletePage(vc->pagesCount() - 1);
    vc->page(0)->deleteChildren();
    QXmlStreamReader reader(QStringLiteral("<VirtualConsole/>"));
    QVERIFY(reader.readNextStartElement());
    QVERIFY(vc->loadXML(reader));
    QVERIFY(!reader.hasError());
    vc->postLoad();
    QCOMPARE(vc->pagesCount(), 1);
    QVERIFY(vc->page(0));
    QCOMPARE(vc->widget(vc->page(0)->id()), vc->page(0));
    const QVariantMap created = vc->createWidgetGeometry(vc->page(0)->id(), "Button", QRectF(0, 0, 120, 100));
    QVERIFY2(created.value("accepted").toBool(), qPrintable(created.value("error").toString()));
    QVERIFY(vc->deleteAuthoringWidgets(created.value("widgetIDs").toList()).value("accepted").toBool());
}

void UpstreamIntegration_Test::stoppedCueNavigation_data()
{
    QTest::addColumn<bool>("backward");
    QTest::addColumn<bool>("previous");
    QTest::addColumn<int>("count");
    for (bool backward : {false, true})
        for (bool previous : {false, true})
            for (int count : {1, 3})
                QTest::newRow(qPrintable(QString("%1-%2-%3").arg(backward ? "backward" : "forward",
                    previous ? "previous" : "next").arg(count))) << backward << previous << count;
}

void UpstreamIntegration_Test::stoppedCueNavigation()
{
    QFETCH(bool, backward);
    QFETCH(bool, previous);
    QFETCH(int, count);
    Doc *doc = m_app->doc();
    auto *scene = new Scene(doc);
    QVERIFY(doc->addFunction(scene));
    auto *chaser = new Chaser(doc);
    QVERIFY(doc->addFunction(chaser));
    const auto cleanup = qScopeGuard([&]() {
        doc->deleteFunction(chaser->id());
        doc->deleteFunction(scene->id());
    });
    chaser->setDirection(backward ? Function::Backward : Function::Forward);
    for (int i = 0; i < count; i++)
        chaser->addStep(ChaserStep(scene->id()));
    VCCueList cue(doc);
    cue.setChaserID(chaser->id());
    cue.setNextPrevBehavior(VCCueList::Select);
    QCOMPARE(cue.playbackIndex(), -1);
    if (previous) cue.previousClicked();
    else cue.nextClicked();
    const int first = previous != backward ? count - 1 : 0;
    QCOMPARE(cue.playbackIndex(), first);
    for (int i = 0; i < count; i++)
    {
        if (previous) cue.previousClicked();
        else cue.nextClicked();
    }
    QCOMPARE(cue.playbackIndex(), first);
    QCOMPARE(cue.playbackStatus(), VCCueList::Stopped);
    QVERIFY(!chaser->isRunning());
}

void UpstreamIntegration_Test::tardisHistory_data()
{
    QTest::addColumn<QString>("operation");
    for (const QString &operation : {"roundtrip", "branch", "refused-undo", "refused-redo", "queued"})
        QTest::newRow(qPrintable(operation)) << operation;
}

void UpstreamIntegration_Test::tardisHistory()
{
    QFETCH(QString, operation);
    Tardis *tardis = Tardis::instance();
    VirtualConsole *vc = m_app->virtualConsole();
    VCButton target(m_app->doc());
    vc->addWidgetToMap(&target);
    const auto removeTarget = qScopeGuard([&]() { vc->removeWidgetFromMap(&target); });
    auto *scene = new Scene(m_app->doc());
    scene->setName("three");
    QVERIFY(m_app->doc()->addFunction(scene));
    const auto cleanup = qScopeGuard([&]() {
        tardis->resetHistory();
        m_app->doc()->deleteFunction(scene->id());
    });
    tardis->enqueueAction(Tardis::FunctionSetName, scene->id(), "zero", "settled");
    const auto latest = [&]() {
        QMutexLocker locker(&tardis->m_historyMutex);
        return tardis->m_history.isEmpty() ? QString() : tardis->m_history.last().m_newValue.toString();
    };
    QTRY_COMPARE(latest(), QString("settled"));
    {
        QMutexLocker locker(&tardis->m_historyMutex);
        tardis->m_history = {
            {Tardis::FunctionSetName, 0, scene->id(), "zero", "one"},
            {Tardis::FunctionSetName, 200, scene->id(), "one", "two"},
            {Tardis::FunctionSetName, 201, scene->id(), "two", "three"}
        };
        tardis->m_historyIndex = 2;
        tardis->m_historyCount = 2;
        if (operation.startsWith("refused"))
        {
            tardis->m_history = {{Tardis::VCWidgetReparent, 0, target.id(),
                VCWidget::invalidId(), VCWidget::invalidId()}};
            tardis->m_historyIndex = operation == "refused-undo" ? 0 : -1;
            tardis->m_historyCount = 1;
        }
    }
    if (operation.startsWith("refused"))
    {
        if (operation == "refused-undo") tardis->undoAction();
        else tardis->redoAction();
        QMutexLocker locker(&tardis->m_historyMutex);
        QCOMPARE(tardis->m_historyIndex, operation == "refused-undo" ? 0 : -1);
        return;
    }
    bool callbackLockAvailable = true;
    const auto connection = connect(scene, &Function::nameChanged, this, [&]() {
        if (tardis->m_historyMutex.tryLock())
            tardis->m_historyMutex.unlock();
        else
            callbackLockAvailable = false;
    });
    const auto disconnect = qScopeGuard([&]() { QObject::disconnect(connection); });
    if (operation == "queued")
    {
        {
            QMutexLocker locker(&tardis->m_historyMutex);
            tardis->m_replayingHistory = true;
        }
        tardis->enqueueAction(Tardis::FunctionSetName, scene->id(), "one", "queued");
    }
    tardis->undoAction();
    QCOMPARE(scene->name(), QString("one"));
    QVERIFY(callbackLockAvailable);
    if (operation == "queued")
    {
        QTRY_COMPARE(latest(), QString("queued"));
        QMutexLocker locker(&tardis->m_historyMutex);
        QCOMPARE(tardis->m_history.size(), 2);
        QCOMPARE(tardis->m_historyIndex, 1);
        return;
    }
    tardis->redoAction();
    QCOMPARE(scene->name(), QString("three"));
    tardis->undoAction();
    tardis->undoAction();
    QCOMPARE(scene->name(), QString("zero"));
    if (operation == "branch")
    {
        scene->setName("branch");
        tardis->enqueueAction(Tardis::FunctionSetName, scene->id(), "zero", "branch");
        QTRY_COMPARE(latest(), QString("branch"));
        tardis->redoAction();
        QCOMPARE(scene->name(), QString("branch"));
        tardis->undoAction();
        QCOMPARE(scene->name(), QString("zero"));
    }
    else
    {
        tardis->redoAction();
        QCOMPARE(scene->name(), QString("one"));
        tardis->redoAction();
        QCOMPARE(scene->name(), QString("three"));
    }
    QVERIFY(callbackLockAvailable);
}

void UpstreamIntegration_Test::tardisRecordingHistory_data()
{
    QTest::addColumn<QString>("operation");
    for (const QString &operation : {"roundtrip", "stale", "capacity", "perform-guard"})
        QTest::newRow(qPrintable(operation)) << operation;
}

void UpstreamIntegration_Test::tardisRecordingHistory()
{
    QFETCH(QString, operation);
    Tardis *tardis = Tardis::instance();
    ShowCommandRecorder *recorder = ShowCommandRecorder::instance();
    QVERIFY(recorder);
    auto *manager = m_app->rootContext()->contextProperty("showManager").value<ShowManager *>();
    QVERIFY(manager);
    const int previousShow = manager->currentShowID();
    const bool previousReadOnly = manager->readOnly();
    auto *scene = new Scene(m_app->doc());
    scene->setName("after");
    QVERIFY(m_app->doc()->addFunction(scene));
    auto *show = new Show(m_app->doc());
    QVERIFY(m_app->doc()->addFunction(show));
    const auto cleanup = qScopeGuard([&]() {
        manager->setReadOnly(previousReadOnly);
        manager->setCurrentShowID(previousShow);
        tardis->resetHistory();
        m_app->doc()->deleteFunction(show->id());
        m_app->doc()->deleteFunction(scene->id());
    });
    ShowCommandTrack track;
    QVERIFY(track.insert(ShowCommand::setIntensity(1, 1000, scene->id(), 0.6)));
    QVERIFY(show->setCommandTrack(track));
    manager->setReadOnly(false);
    manager->setCurrentShowID(show->id());
    tardis->enqueueAction(Tardis::FunctionSetName, scene->id(), "before", "after");
    const auto latestSerial = [&]() {
        QMutexLocker locker(&tardis->m_historyMutex);
        return tardis->m_history.isEmpty() ? -1
            : tardis->m_history.last().m_newValue.value<ShowCommandEdit>().serial;
    };
    QTRY_VERIFY(([&]() {
        QMutexLocker locker(&tardis->m_historyMutex);
        return !tardis->m_history.isEmpty() && tardis->m_history.last().m_newValue.toString() == "after";
    })());
    {
        QMutexLocker locker(&tardis->m_historyMutex);
        tardis->m_history = {{Tardis::FunctionSetName, 0, scene->id(), "before", "after"}};
        tardis->m_historyIndex = 0;
        tardis->m_historyCount = 1;
    }
    const int edits = operation == "capacity" ? 101 : 2;
    {
        tardis->beginBatch();
        const auto endBatch = qScopeGuard([&]() { tardis->endBatch(); });
        for (int i = 1; i <= edits; i++)
            QVERIFY2(recorder->retimeCommand(show->id(), 1, 1000 + i), qPrintable(recorder->lastError()));
    }
    const int serial = recorder->lastEditSerial();
    QTRY_COMPARE(latestSerial(), serial);
    const auto time = [&]() { return show->commandTrack().commands().first().time; };
    if (operation == "capacity")
    {
        {
            QMutexLocker locker(&tardis->m_historyMutex);
            QCOMPARE(tardis->m_historyCount, 100);
            QCOMPARE(tardis->m_history.size(), 100);
        }
        for (int i = 0; i < 100; i++) tardis->undoAction();
        QCOMPARE(time(), 1001u);
        tardis->redoAction();
        QCOMPARE(time(), 1002u);
        QCOMPARE(scene->name(), QString("after"));
        return;
    }
    if (operation == "stale")
    {
        QVERIFY(!tardis->undoCommandEdit(serial - 1));
        QVERIFY(tardis->undoCommandEdit(serial));
        QCOMPARE(time(), 1001u);
        tardis->redoAction();
        ShowCommandTrack changed = show->commandTrack();
        QVERIFY(changed.retime(1, 1500));
        QVERIFY(show->setCommandTrack(changed));
        QVERIFY(!tardis->undoCommandEdit(serial));
        QCOMPARE(time(), 1500u);
        QVERIFY(!recorder->lastError().isEmpty());
        QMutexLocker locker(&tardis->m_historyMutex);
        QCOMPARE(tardis->m_historyIndex, 2);
        return;
    }
    if (operation == "perform-guard")
        manager->setReadOnly(true);
    tardis->undoAction();
    QCOMPARE(time(), 1001u);
    tardis->undoAction();
    QCOMPARE(time(), 1000u);
    tardis->undoAction();
    QCOMPARE(scene->name(), operation == "perform-guard" ? QString("after") : QString("before"));
    if (operation != "perform-guard")
        tardis->redoAction();
    tardis->redoAction();
    QCOMPARE(time(), 1001u);
    tardis->redoAction();
    QCOMPARE(time(), 1002u);
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

void UpstreamIntegration_Test::showClipCycleGuard_data()
{
    QTest::addColumn<QString>("item");
    QTest::addColumn<bool>("accepted");

    QTest::newRow("a song show") << "song" << true;
    QTest::newRow("the show itself") << "self" << false;
    QTest::newRow("a show that plays this show") << "wrapper" << false;
    QTest::newRow("a show that plays it through another show") << "outer" << false;
}

void UpstreamIntegration_Test::showClipCycleGuard()
{
    QFETCH(QString, item);
    QFETCH(bool, accepted);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    QVERIFY(manager);
    QMap<QString, Show *> shows;
    for (const char *name : {"set", "song", "wrapper", "outer"})
    {
        auto *show = new Show(doc);
        QVERIFY(doc->addFunction(show));
        auto *track = new Track(Function::invalidId(), show);
        show->addTrack(track);
        shows.insert(name, show);
    }
    shows["self"] = shows["set"];
    // wrapper plays the set, outer plays wrapper
    for (const auto &[parent, child] : {std::pair<const char *, const char *>{"wrapper", "set"}, {"outer", "wrapper"}})
    {
        ShowFunction *clip = shows[parent]->tracks().first()->createShowFunction(shows[child]->id());
        clip->setStartTime(0);
        clip->setDuration(1000);
    }
    const auto cleanup = qScopeGuard([&]() {
        manager->resetView();
        manager->setCurrentShowID(Function::invalidId());
        for (const char *name : {"outer", "wrapper", "song", "set"})
            doc->deleteFunction(shows[name]->id());
    });
    Show *set = shows["set"];
    manager->setCurrentShowID(set->id());
    QQuickItem area;

    manager->addItems(&area, 0, 0, {shows[item]->id()});

    QCOMPARE(set->tracks().first()->showFunctions().count(), accepted ? 1 : 0);
}

namespace
{

ShowClipAutomation clipAutomation(const QVector<ShowClipSegment> &segments, const QVector<ShowClipLevel> &envelope,
                                  const QVector<ShowClipDominance> &dominance)
{
    ShowClipAutomation automation;
    automation.setSegments(segments);
    automation.setEnvelope(envelope);
    automation.setDominance(dominance);
    return automation;
}

/** A recorded play from inPoint: a loop wrap, a fade in, a handover; none for a negative in-point */
ShowClipAutomation recordedPlay(int inPoint)
{
    if (inPoint < 0)
        return ShowClipAutomation();
    return clipAutomation({{0, quint32(inPoint), 1.0}, {4000, quint32(inPoint / 2), 1.0}},
                          {{0, 0.0}, {3000, 1.0}}, {{0, false}, {2500, true}});
}

}

void UpstreamIntegration_Test::nestedClipTrim_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<int>("inPoint");
    QTest::addColumn<int>("dragPx");
    QTest::addColumn<QString>("expect");

    QTest::newRow("a song show trimmed") << "show" << 10000 << 60 << "rebased";
    QTest::newRow("a song show extended into its in-point") << "show" << 10000 << -60 << "rebased";
    QTest::newRow("a song show extension stops at the song start") << "show" << 300 << -200 << "clamped";
    QTest::newRow("an untrimmed song show cannot extend") << "show" << -1 << -60 << "unchanged";
    QTest::newRow("an untrimmed song show trimmed gets an in-point") << "show" << -1 << 60 << "rebased";
    QTest::newRow("a scene with an in-point trimmed") << "scene" << 2000 << 60 << "rebased";
    QTest::newRow("a plain scene keeps the upstream trim") << "scene" << -1 << 60 << "upstream";
    QTest::newRow("read-only refuses") << "show" << 10000 << 0 << "read-only";
    QTest::newRow("a playing show refuses") << "show" << 10000 << 0 << "playing";
    QTest::newRow("undo waits while the show plays") << "show" << 10000 << 60 << "undo-playing";
    QTest::newRow("a chaser with an in-point is not stretched") << "chaser" << 2000 << 60 << "rebased";
}

void UpstreamIntegration_Test::nestedClipTrim()
{
    QFETCH(QString, kind);
    QFETCH(int, inPoint);
    QFETCH(int, dragPx);
    QFETCH(QString, expect);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    auto *editor = qobject_cast<ShowClipEditor*>(m_app->rootContext()->contextProperty("showClipEditor").value<QObject*>());
    QVERIFY(manager);
    QVERIFY(editor);
    auto *set = new Show(doc);
    QVERIFY(doc->addFunction(set));
    set->setTimeDivision(Show::Time, 120);
    Function *function = nullptr;
    if (kind == "show")
    {
        auto *song = new Show(doc);
        song->addTrack(new Track(Function::invalidId(), song));
        function = song;
    }
    else if (kind == "chaser")
    {
        auto *scene = new Scene(doc);
        QVERIFY(doc->addFunction(scene));
        auto *chaser = new Chaser(doc);
        chaser->addStep(ChaserStep(scene->id()));
        chaser->addStep(ChaserStep(scene->id()));
        chaser->setDuration(4000);
        function = chaser;
    }
    else
    {
        auto *scene = new Scene(doc);
        scene->setDuration(8000);
        function = scene;
    }
    QVERIFY(doc->addFunction(function));
    manager->setCurrentShowID(set->id());
    manager->setTimeScale(0.5);
    manager->setGridEnabled(false);
    // a function keeps its length when the clip trims its in-point
    manager->setStretchFunctions(true);
    QQuickItem area(m_app->rootObject());
    area.setY(200);
    area.setZ(1000);
    area.setWidth(800);
    area.setHeight(100);
    const auto reset = qScopeGuard([&]() {
        manager->setReadOnly(false);
        manager->setStretchFunctions(false);
        if (set->isRunning())
            set->stopAndWait();
        doc->masterTimer()->stop();
        manager->resetView();
        manager->setCurrentShowID(Function::invalidId());
    });
    manager->addItems(&area, -1, 4000, {function->id()});
    ShowFunction *sf = set->tracks().first()->showFunctions().first();
    sf->setDuration(8000);
    const ShowClipAutomation before = recordedPlay(inPoint);
    sf->setClipAutomation(before);
    const quint32 functionDuration = function->totalDuration();
    auto *item = area.childItems().first();
    QVERIFY(QMetaObject::invokeMethod(item, "updateGeometry"));
    auto *handle = item->findChild<QQuickItem*>("showItemResizeLeft");
    QVERIFY(handle);
    QTest::qWait(300);

    if (expect == "read-only" || expect == "playing")
    {
        // the runner keeps the automation it started the clip with
        if (expect == "playing")
        {
            doc->masterTimer()->start();
            set->setSyncSource(ShowRunner::Autonomous);
            manager->playShow();
            QTRY_VERIFY(set->isRunning());
        }
        else
        {
            manager->setReadOnly(true);
            QVERIFY(!handle->isVisible());
        }
        QVERIFY(!editor->trimStart(sf, 5000));
        QCOMPARE(sf->startTime(), 4000u);
        QCOMPARE(sf->duration(), 8000u);
        QVERIFY(sf->clipAutomation() == before);
        return;
    }

    const QPoint point = handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2)).toPoint();
    const int direction = dragPx < 0 ? -1 : 1;
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, point);
    QTest::mouseMove(m_app.get(), point + QPoint(direction * 15, 0), 20);
    QTest::mouseMove(m_app.get(), point + QPoint(dragPx, 0), 20);
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, point + QPoint(dragPx, 0));

    const qint64 delta = qint64(sf->startTime()) - 4000;
    if (expect == "unchanged")
    {
        QCOMPARE(sf->startTime(), 4000u);
        QCOMPARE(sf->duration(), 8000u);
        QVERIFY(sf->clipAutomation().isEmpty());
        return;
    }
    if (expect == "upstream")
    {
        QVERIFY(delta > 0);
        QVERIFY(sf->clipAutomation().isEmpty());
        return;
    }
    if (expect == "clamped")
        QCOMPARE(delta, -qint64(before.startExtensionLimit()));
    else
        QCOMPARE(delta > 0, dragPx > 0);
    QVERIFY(delta != 0);
    QCOMPARE(sf->startTime() + sf->duration(), 12000u);
    // other functions play only the in-point, on at rate 1
    ShowClipAutomation played = before;
    if (kind != "show" && !before.segments().isEmpty())
        played.setSegments({{0, before.inPoint(), 1.0}});
    QVERIFY(sf->clipAutomation() == played.withStartMoved(delta));
    QCOMPARE(function->totalDuration(), functionDuration);

    // one undo step gives back the edge and the automation
    QTest::qWait(300);
    if (expect == "undo-playing")
    {
        // the runner keeps the automation it started the clip with
        const quint32 trimmedStart = sf->startTime();
        const ShowClipAutomation trimmed = sf->clipAutomation();
        doc->masterTimer()->start();
        set->setSyncSource(ShowRunner::Autonomous);
        manager->playShow();
        QTRY_VERIFY(set->isRunning());
        Tardis::instance()->undoAction();
        QCOMPARE(sf->startTime(), trimmedStart);
        QVERIFY(sf->clipAutomation() == trimmed);
        // a network peer's edit is not history: it lands at once
        const QString peerEdit = ShowClipEditor::clipXml(set->id(), sf);
        QVERIFY(ShowClipEditor::applyClip(doc, sf->id(), peerEdit, true) == ShowClipEditor::ClipEdit::Waits);
        QVERIFY(ShowClipEditor::applyClip(doc, sf->id(), peerEdit, false) == ShowClipEditor::ClipEdit::Applied);
        set->stopAndWait();
    }
    Tardis::instance()->undoAction();
    QCOMPARE(sf->startTime(), 4000u);
    QCOMPARE(sf->duration(), 8000u);
    QVERIFY(sf->clipAutomation() == before);
}

void UpstreamIntegration_Test::nestedClipTrimUndoSkipsARemovedClip_data()
{
    QTest::addColumn<bool>("trackRemoved");

    // MCP removes items and tracks without history
    QTest::newRow("the trimmed clip removed") << false;
    QTest::newRow("its track removed") << true;
}

void UpstreamIntegration_Test::nestedClipTrimUndoSkipsARemovedClip()
{
    QFETCH(bool, trackRemoved);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    auto *editor = qobject_cast<ShowClipEditor*>(m_app->rootContext()->contextProperty("showClipEditor").value<QObject*>());
    QVERIFY(manager);
    QVERIFY(editor);
    auto *set = new Show(doc);
    QVERIFY(doc->addFunction(set));
    set->setTimeDivision(Show::Time, 120);
    auto *song = new Show(doc);
    song->addTrack(new Track(Function::invalidId(), song));
    QVERIFY(doc->addFunction(song));
    auto *scene = new Scene(doc);
    scene->setDuration(2000);
    QVERIFY(doc->addFunction(scene));
    manager->setCurrentShowID(set->id());
    QQuickItem area(m_app->rootObject());
    const auto reset = qScopeGuard([&]() {
        manager->resetView();
        manager->setCurrentShowID(Function::invalidId());
    });
    manager->addItems(&area, -1, 20000, {scene->id()});
    manager->addItems(&area, -1, 4000, {song->id()});
    QCOMPARE(set->tracks().count(), 2);
    ShowFunction *earlier = set->tracks().first()->showFunctions().first();
    Track *songTrack = set->tracks().last();
    ShowFunction *sf = songTrack->showFunctions().first();
    sf->setDuration(8000);
    sf->setClipAutomation(recordedPlay(10000));

    // an earlier step on another clip, then the trim
    QTest::qWait(300);
    const quint32 earlierDuration = earlier->duration();
    QVERIFY(manager->setShowItemDuration(earlier, int(earlierDuration) + 1000));
    QTest::qWait(300);
    QVERIFY(editor->trimStart(sf, 5000));
    QTest::qWait(300);
    if (trackRemoved)
        QVERIFY(set->removeTrack(songTrack->id()));
    else
        QVERIFY(songTrack->removeShowFunction(sf));

    // the trim's step is passed, then the one before it undone
    Tardis::instance()->undoAction();
    QCOMPARE(earlier->duration(), earlierDuration + 1000);
    Tardis::instance()->undoAction();
    QCOMPARE(earlier->duration(), earlierDuration);
}

void UpstreamIntegration_Test::nestedClipPaste_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<int>("inPoint");

    QTest::newRow("a song show with a recorded play") << "show" << 30000;
    QTest::newRow("a scene with an in-point") << "scene" << 2000;
    QTest::newRow("a plain scene") << "scene" << -1;
}

void UpstreamIntegration_Test::nestedClipPaste()
{
    QFETCH(QString, kind);
    QFETCH(int, inPoint);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    QVERIFY(manager);
    auto *set = new Show(doc);
    QVERIFY(doc->addFunction(set));
    set->setTimeDivision(Show::Time, 120);
    Function *function = kind == "show" ? static_cast<Function*>(new Show(doc)) : new Scene(doc);
    QVERIFY(doc->addFunction(function));
    manager->setCurrentShowID(set->id());
    QQuickItem area;
    const auto reset = qScopeGuard([&]() {
        manager->resetView();
        manager->setCurrentShowID(Function::invalidId());
    });
    manager->addItems(&area, -1, 0, {function->id()});
    ShowFunction *source = set->tracks().first()->showFunctions().first();
    const ShowClipAutomation automation = recordedPlay(inPoint);
    source->setClipAutomation(automation);
    manager->setItemSelection(0, source, area.childItems().first(), true, Qt::NoModifier);
    manager->copyToClipboard();
    manager->setCurrentTime(20000);
    QTest::qWait(300);

    QVERIFY(manager->pasteFromClipboard());

    QList<ShowFunction*> items = set->tracks().first()->showFunctions();
    QCOMPARE(items.count(), 2);
    QCOMPARE(items.last()->startTime(), 20000u);
    QVERIFY(items.last()->clipAutomation() == automation);

    // the pasted clip comes back with its automation on redo
    QTest::qWait(300);
    Tardis::instance()->undoAction();
    QCOMPARE(set->tracks().first()->showFunctions().count(), 1);
    QTest::qWait(300);
    Tardis::instance()->redoAction();
    items = set->tracks().first()->showFunctions();
    QCOMPARE(items.count(), 2);
    QVERIFY(items.last()->clipAutomation() == automation);
}

void UpstreamIntegration_Test::nestedClipOpen_data()
{
    QTest::addColumn<QString>("then");

    QTest::newRow("back to the parent") << "back";
    QTest::newRow("two levels deep and back") << "deeper";
    QTest::newRow("choosing another show forgets the way back") << "other";
    QTest::newRow("a deleted parent leaves no way back") << "deleted";
    QTest::newRow("read-only still navigates") << "read-only";
    QTest::newRow("a scene clip is not a nested show") << "scene";
    QTest::newRow("undo of a trim keeps to its show") << "undo";
}

void UpstreamIntegration_Test::nestedClipOpen()
{
    QFETCH(QString, then);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    auto *editor = qobject_cast<ShowClipEditor*>(m_app->rootContext()->contextProperty("showClipEditor").value<QObject*>());
    QVERIFY(manager);
    QVERIFY(editor);
    QMap<QString, Show *> shows;
    for (const char *name : {"Set", "Song", "Inner", "Other"})
    {
        auto *show = new Show(doc);
        show->setName(name);
        show->addTrack(new Track(Function::invalidId(), show));
        QVERIFY(doc->addFunction(show));
        shows.insert(name, show);
    }
    ShowFunction *inner = shows["Song"]->tracks().first()->createShowFunction(shows["Inner"]->id());
    inner->setStartTime(0);
    inner->setDuration(1000);
    auto *scene = new Scene(doc);
    QVERIFY(doc->addFunction(scene));
    manager->setCurrentShowID(shows["Set"]->id());
    manager->setTimeScale(0.5);
    QQuickItem area(m_app->rootObject());
    area.setY(200);
    area.setZ(1000);
    area.setWidth(800);
    area.setHeight(100);
    // the Show Manager view's flickable, which a press on a clip holds still
    QQuickItem flickable(m_app->rootObject());
    flickable.setObjectName("showItemsArea");
    const auto reset = qScopeGuard([&]() {
        manager->setReadOnly(false);
        manager->resetView();
        manager->setCurrentShowID(Function::invalidId());
    });
    manager->addItems(&area, 0, 0, {then == "scene" ? scene->id() : shows["Song"]->id()});
    ShowFunction *clip = shows["Set"]->tracks().first()->showFunctions().first();
    if (then == "scene")
    {
        QVERIFY(!editor->openNested(clip));
        QCoreApplication::processEvents();
        QCOMPARE(manager->currentShowID(), int(shows["Set"]->id()));
        QVERIFY(!editor->canGoBack());
        return;
    }
    if (then == "read-only")
        manager->setReadOnly(true);
    const uint clipDuration = clip->duration();
    if (then == "undo")
    {
        QTest::qWait(300);
        QVERIFY(editor->trimStart(clip, 500));
        QTest::qWait(300);
    }
    auto *item = area.childItems().first();
    QVERIFY(QMetaObject::invokeMethod(item, "updateGeometry"));
    const QPoint point = item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();

    QTest::mouseDClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, point);

    QTRY_COMPARE(manager->currentShowID(), int(shows["Song"]->id()));
    QVERIFY(editor->canGoBack());
    QCOMPARE(editor->parentShowName(), QString("Set"));

    if (then == "back" || then == "read-only")
    {
        QSignalSpy pathChanged(editor, &ShowClipEditor::pathChanged);
        shows["Set"]->setName("Renamed set");
        QCOMPARE(pathChanged.count(), 1);
        QCOMPARE(editor->parentShowName(), QString("Renamed set"));
        editor->goBack();
        QCOMPARE(manager->currentShowID(), int(shows["Set"]->id()));
        QVERIFY(!editor->canGoBack());
    }
    else if (then == "deeper")
    {
        QVERIFY(editor->openNested(inner));
        QTRY_COMPARE(manager->currentShowID(), int(shows["Inner"]->id()));
        QCOMPARE(editor->parentShowName(), QString("Song"));
        editor->goBack();
        QCOMPARE(manager->currentShowID(), int(shows["Song"]->id()));
        QCOMPARE(editor->parentShowName(), QString("Set"));
        editor->goBack();
        QCOMPARE(manager->currentShowID(), int(shows["Set"]->id()));
        QVERIFY(!editor->canGoBack());
    }
    else if (then == "other")
    {
        manager->setCurrentShowID(shows["Other"]->id());
        QVERIFY(!editor->canGoBack());
        QCOMPARE(editor->parentShowName(), QString());
    }
    else if (then == "undo")
    {
        // the Song shown now has a clip with the same Show-local id
        QCOMPARE(inner->id(), clip->id());
        Tardis::instance()->undoAction();
        QCOMPARE(inner->startTime(), 0u);
        QCOMPARE(inner->duration(), 1000u);
        QVERIFY(inner->clipAutomation().isEmpty());
        QCOMPARE(clip->startTime(), 0u);
        QCOMPARE(clip->duration(), clipDuration);
        QVERIFY(clip->clipAutomation().isEmpty());
    }
    else if (then == "deleted")
    {
        manager->resetView();
        QVERIFY(doc->deleteFunction(shows["Set"]->id()));
        QVERIFY(!editor->canGoBack());
        editor->goBack();
        QCOMPARE(manager->currentShowID(), int(shows["Song"]->id()));
    }
}

void UpstreamIntegration_Test::nestedClipOverlay_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<ShowClipAutomation>("automation");
    // the overlay as JSON; empty when there is nothing to draw
    QTest::addColumn<QString>("expected");

    QTest::newRow("a recorded song play") << "show"
        << clipAutomation({{0, 30000, 1.0}, {1900, 28000, 1.0}, {4000, 30100, 0.0}}, {{0, 0.0}, {2000, 1.0}},
                          {{0, false}, {2500, true}})
        << R"({"boundaries":[1900,4000],"envelope":[{"clipMs":0,"level":0},{"clipMs":2000,"level":1}],)"
           R"("inPoint":30000,"shaded":[{"fromMs":0,"toMs":2500}]})";
    QTest::newRow("an in-point only") << "scene" << clipAutomation({{0, 12000, 1.0}}, {}, {})
        << R"({"boundaries":[],"envelope":[],"inPoint":12000,"shaded":[]})";
    QTest::newRow("a scene plays no loops") << "scene" << clipAutomation({{0, 12000, 1.0}, {3000, 0, 1.0}}, {}, {})
        << R"({"boundaries":[],"envelope":[],"inPoint":12000,"shaded":[]})";
    QTest::newRow("dominance handed back and forth") << "show"
        << clipAutomation({}, {}, {{0, true}, {1000, false}, {3000, true}, {5000, false}})
        << R"({"boundaries":[],"envelope":[],"inPoint":-1,"shaded":[{"fromMs":1000,"toMs":3000},{"fromMs":5000,"toMs":6000}]})";
    QTest::newRow("markers past the clip end are not drawn") << "show"
        << clipAutomation({{0, 0, 1.0}, {7000, 0, 1.0}}, {}, {{7000, false}})
        << R"({"boundaries":[],"envelope":[],"inPoint":0,"shaded":[]})";
    QTest::newRow("a plain scene shows nothing") << "scene" << ShowClipAutomation() << "";
}

void UpstreamIntegration_Test::nestedClipOverlay()
{
    QFETCH(QString, kind);
    QFETCH(ShowClipAutomation, automation);
    QFETCH(QString, expected);
    auto *doc = m_app->doc();
    auto *manager = qobject_cast<ShowManager*>(m_app->rootContext()->contextProperty("showManager").value<QObject*>());
    auto *editor = qobject_cast<ShowClipEditor*>(m_app->rootContext()->contextProperty("showClipEditor").value<QObject*>());
    QVERIFY(manager);
    QVERIFY(editor);
    auto *set = new Show(doc);
    QVERIFY(doc->addFunction(set));
    set->setTimeDivision(Show::Time, 120);
    Function *function = kind == "show" ? static_cast<Function*>(new Show(doc)) : new Scene(doc);
    QVERIFY(doc->addFunction(function));
    manager->setCurrentShowID(set->id());
    manager->setTimeScale(0.5);
    QQuickItem area(m_app->rootObject());
    area.setWidth(800);
    area.setHeight(100);
    const auto reset = qScopeGuard([&]() {
        manager->resetView();
        manager->setCurrentShowID(Function::invalidId());
    });
    manager->addItems(&area, -1, 0, {function->id()});
    ShowFunction *sf = set->tracks().first()->showFunctions().first();
    sf->setDuration(6000);
    auto *item = area.childItems().first();
    auto *overlay = item->findChild<QQuickItem*>("clipAutomationOverlay");
    QVERIFY(overlay);
    QVERIFY(!overlay->isVisible());

    // the overlay follows an automation set after the item exists
    sf->setClipAutomation(automation);

    // Repeater delegates are only in the visual tree
    const auto count = [overlay](const QString &name) {
        int n = 0;
        for (QQuickItem *child : overlay->childItems())
            n += child->objectName() == name && child->isVisible() ? 1 : 0;
        return n;
    };
    if (expected.isEmpty())
    {
        QVERIFY(!overlay->isVisible());
        return;
    }
    const QJsonObject drawn = QJsonObject::fromVariantMap(editor->overlay(sf));
    QCOMPARE(QJsonDocument(drawn).toJson(QJsonDocument::Compact),
             QJsonDocument::fromJson(expected.toUtf8()).toJson(QJsonDocument::Compact));
    QVERIFY(overlay->isVisible());
    QCOMPARE(count("clipBoundary"), int(drawn["boundaries"].toArray().size()));
    QCOMPARE(count("clipShade"), int(drawn["shaded"].toArray().size()));
    QCOMPARE(count("clipEnvelope"), drawn["envelope"].toArray().isEmpty() ? 0 : 1);
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
        const bool sent = QMetaObject::invokeMethod(bridge, "onDeckTrigger",
                                                    Q_ARG(int, 0),
                                                    Q_ARG(QString, trigger),
                                                    Q_ARG(QVariant, value));
        return sent && QMetaObject::invokeMethod(bridge, "onTickEnded");
    };

    bridge->showFactory()->registerMapping(filepath, show->id());
    QVERIFY(deckTrigger("get_filepath", filepath));
    QVERIFY(deckTrigger("loaded", "on"));
    QVERIFY(deckTrigger("get_title", "Playhead test"));
    QVERIFY(deckTrigger("get_artist", "QLC+"));
    QVERIFY(deckTrigger("get_bpm", 120.0));
    djManager->assignShow(filepath, int(show->id()));
    QVERIFY(deckTrigger("masterdeck", "on"));
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

void UpstreamIntegration_Test::incomingListModel_data()
{
    QTest::addColumn<int>("row");
    QTest::addColumn<int>("role");
    QTest::addColumn<bool>("empty");
    QTest::newRow("last-valid-role") << 0 << int(Qt::UserRole + 2) << false;
    QTest::newRow("first-invalid-role") << 0 << int(Qt::UserRole + 3) << false;
    QTest::newRow("negative-row") << -1 << int(Qt::UserRole + 1) << false;
    QTest::newRow("empty-model") << 0 << int(Qt::UserRole + 1) << true;
}

void UpstreamIntegration_Test::incomingListModel()
{
    QFETCH(int, row);
    QFETCH(int, role);
    QFETCH(bool, empty);
    ListModel model;
    model.setRoleNames({"first", "last"});
    const QVariantMap original{{"first", 10}, {"last", 20}};
    if (!empty)
        model.addDataMap(original);
    const QModelIndex index = model.index(row, 0);
    const bool valid = !empty && row == 0 && role == Qt::UserRole + 2;
    QCOMPARE(model.data(index, role).isValid(), valid);
    QCOMPARE(model.setData(index, 42, role), valid);
    if (valid)
        QCOMPARE(model.data(index, role).toInt(), 42);
    model.setDataMap(model.index(-1, 0), {{"first", 99}});
    if (!empty)
        QCOMPARE(model.data(model.index(0, 0), Qt::UserRole + 1).toInt(), 10);
}

void UpstreamIntegration_Test::incomingDeskBytes_data()
{
    QTest::addColumn<int>("value");
    QTest::addColumn<bool>("overridden");
    for (int value : {0, 127, 128, 255})
        for (bool overridden : {false, true})
            QTest::newRow(qPrintable(QString("%1-%2").arg(value).arg(overridden)))
                << value << overridden;
}

void UpstreamIntegration_Test::incomingDeskBytes()
{
    QFETCH(int, value);
    QFETCH(bool, overridden);
    SimpleDesk desk(m_app.get(), m_app->doc(), m_app->m_functionManager);
    desk.setUniverseFilter(0);
    if (overridden)
        desk.setAbsoluteChannelValue(500, 91);
    QByteArray bytes(512, char(0));
    bytes[500] = char(value);
    QVERIFY(QMetaObject::invokeMethod(&desk, "slotUniverseWritten", Qt::DirectConnection,
        Q_ARG(quint32, 0), Q_ARG(QByteArray, bytes)));
    desk.setUniverseFilter(0);
    auto *model = desk.channelList().value<ListModel *>();
    QVERIFY(model);
    const QVariantMap channel = model->itemAt(500).toMap();
    QCOMPARE(channel.value("chValue").toUInt(), overridden ? 91u : uint(value));
    QCOMPARE(channel.value("isOverride").toBool(), overridden);
}

void UpstreamIntegration_Test::C3_beatEditing_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("expected");
    for (int numerator = 1; numerator < 32; ++numerator)
        QTest::newRow(qPrintable(QString("sixteenths-%1").arg(numerator)))
            << QString("%1/16").arg(numerator) << qRound(numerator * 62.5);
    QTest::newRow("decimal") << QString("0.5") << 500;
    QTest::newRow("comma-decimal") << QString("1,25") << 1250;
    QTest::newRow("mixed-fraction") << QString("2 1/2") << 2500;
    QTest::newRow("whitespace") << QString("  2   1/4  ") << 2250;
    QTest::newRow("decimal-sixteenth") << QString("0.0625") << 63;
    QTest::newRow("zero") << QString("0") << 0;
    QTest::newRow("infinite") << QString::fromUtf8("∞") << -2;
    QTest::newRow("invalid") << QString("invalid") << -1;
    QTest::newRow("negative") << QString("-1") << -1;
    QTest::newRow("zero-denominator") << QString("1/0") << -1;
}

void UpstreamIntegration_Test::C3_beatEditing()
{
    QFETCH(QString, input);
    QFETCH(int, expected);
    QSignalSpy warnings(m_app->engine(), &QQmlEngine::warnings);
    QQmlComponent component(m_app->engine(), QUrl("qrc:/TimeEditTool.qml"));
    std::unique_ptr<QObject> tool(component.create());
    QVERIFY2(tool, qPrintable(component.errorString()));
    tool->setProperty("allowFractions", Function::FineFractions);
    tool->setProperty("tempoType", Function::Beats);
    QVERIFY(QMetaObject::invokeMethod(tool.get(), "updateTime",
        Q_ARG(QVariant, 500), Q_ARG(QVariant, QString())));
    QSignalSpy changed(tool.get(), SIGNAL(valueChanged(int)));
    QVERIFY(QMetaObject::invokeMethod(tool.get(), "updateTime",
        Q_ARG(QVariant, -1), Q_ARG(QVariant, input)));
    QCOMPARE(tool->property("timeValue").toInt(), expected == -1 ? 500 : expected);
    QCOMPARE(changed.count(), expected == -1 ? 0 : 1);
    if (expected > 0)
    {
        const QString formatted = tool->property("timeValueString").toString();
        QCOMPARE(formatted, formatted.trimmed());
        tool->setProperty("timeValue", 0);
        QVERIFY(QMetaObject::invokeMethod(tool.get(), "applyBeatValue"));
        QCOMPARE(tool->property("timeValue").toInt(), expected);
        QVERIFY(QMetaObject::invokeMethod(tool.get(), "updateTime",
            Q_ARG(QVariant, 0), Q_ARG(QVariant, QString())));
        QVERIFY(QMetaObject::invokeMethod(tool.get(), "updateTime",
            Q_ARG(QVariant, -1), Q_ARG(QVariant, formatted)));
        QCOMPARE(tool->property("timeValue").toInt(), expected);
    }
    else if (expected == -2)
    {
        tool->setProperty("beatCount", 3);
        tool->setProperty("subdivIdx", 4);
        QVERIFY(QMetaObject::invokeMethod(tool.get(), "applyBeatValue"));
        QCOMPARE(tool->property("timeValue").toInt(), 188);
    }
    QCOMPARE(warnings.count(), 0);
}

void UpstreamIntegration_Test::incomingDistribution_data()
{
    QTest::addColumn<int>("view");
    QTest::addColumn<int>("units");
    QTest::addColumn<int>("direction");
    QTest::addColumn<int>("count");
    for (int view : {int(MonitorProperties::FrontView), int(MonitorProperties::LeftSideView),
                     int(MonitorProperties::RightSideView)})
        for (int units : {int(MonitorProperties::Meters), int(MonitorProperties::Feet)})
            for (int direction : {int(Qt::Horizontal), int(Qt::Vertical)})
                for (int count : {2, 3})
                    QTest::newRow(qPrintable(QString("%1-%2-%3-%4").arg(view).arg(units).arg(direction).arg(count)))
                        << view << units << direction << count;
}

void UpstreamIntegration_Test::incomingDistribution()
{
    QFETCH(int, view);
    QFETCH(int, units);
    QFETCH(int, direction);
    QFETCH(int, count);
    Doc *doc = m_app->doc();
    auto *props = doc->monitorProperties();
    auto *context = m_app->m_contextManager;
    const auto oldView = props->pointOfView();
    const auto oldUnits = props->gridUnits();
    const auto oldGrid = props->gridSize();
    const QVariantList oldSelection = context->selectedItemIDVariantList();
    QList<quint32> ids;
    const auto restore = qScopeGuard([&]() {
        context->resetFixtureSelection();
        for (quint32 id : ids) doc->deleteFixture(id);
        props->setPointOfView(oldView);
        props->setGridUnits(oldUnits);
        props->setGridSize(oldGrid);
        for (const auto &id : oldSelection) context->setFixtureSelection(id.toUInt(), -1, true);
    });
    context->resetFixtureSelection();
    props->setGridUnits(MonitorProperties::GridUnits(units));
    props->setGridSize(QVector3D(12, 8, 10));
    props->setPointOfView(MonitorProperties::PointOfView(view));
    const QList<QVector3D> before{{-900.25f, -1600.5f, -1000.25f},
                                {-100.5f, -1200.25f, -500.5f},
                                {1300.75f, 700.25f, 1400.75f}};
    QList<QPointF> projected;
    for (int i = 0; i < count; ++i)
    {
        auto *fixture = new Fixture(doc);
        fixture->setChannels(1);
        fixture->setAddress(480 + i);
        QVERIFY(doc->addFixture(fixture));
        ids.append(fixture->id());
        props->setFixturePosition(ids.last(), 0, 0, before[i]);
        context->setFixtureSelection(FixtureUtils::fixtureItemID(ids.last(), 0, 0), -1, true);
        projected.append(FixtureUtils::item2DPosition(props, view, before[i]));
    }
    QCOMPARE(context->selectedFixturesCount(), count);
    QList<qreal> originalPositions;
    for (const auto &point : projected)
        originalPositions.append(direction == Qt::Horizontal ? point.x() : point.y());
    std::sort(originalPositions.begin(), originalPositions.end());
    context->setFixturesDistribution(direction);
    QList<qreal> positions;
    for (int i = 0; i < count; ++i)
    {
        const auto after = props->fixturePosition(ids[i], 0, 0);
        if (count == 2)
            QCOMPARE(after, before[i]);
        const auto point = FixtureUtils::item2DPosition(props, view, after);
        positions.append(direction == Qt::Horizontal ? point.x() : point.y());
        const qreal originalPosition = direction == Qt::Horizontal ? projected[i].x() : projected[i].y();
        const qreal expectedPosition = count == 3 && originalPosition != originalPositions.first() &&
                originalPosition != originalPositions.last()
            ? (originalPositions.first() + originalPositions.last()) / 2 : originalPosition;
        QVERIFY(qAbs(positions.last() - expectedPosition) < 0.01);
        const qreal untouched = direction == Qt::Horizontal ? point.y() : point.x();
        const qreal original = direction == Qt::Horizontal ? projected[i].y() : projected[i].x();
        QVERIFY(qAbs(untouched - original) < 0.01);
        if (direction == Qt::Horizontal)
            QCOMPARE(after.y(), before[i].y());
        else
            QCOMPARE(after.z(), before[i].z());
        if (view != MonitorProperties::FrontView)
            QCOMPARE(after.x(), before[i].x());
    }
    if (count == 3)
    {
        std::sort(positions.begin(), positions.end());
        QVERIFY(qAbs(positions[1] - (positions[0] + positions[2]) / 2) < 0.01);
    }
}

void UpstreamIntegration_Test::incomingChaserTempo_data()
{
    QTest::addColumn<uint>("hold");
    QTest::addColumn<int>("from");
    for (uint hold : {Function::infiniteSpeed(), Function::defaultSpeed(), 2000u})
        for (int from : {int(Function::Time), int(Function::Beats)})
            QTest::newRow(qPrintable(QString("%1-%2").arg(hold).arg(from))) << hold << from;
}

void UpstreamIntegration_Test::incomingChaserTempo()
{
    QFETCH(uint, hold);
    QFETCH(int, from);
    auto *scene = new Scene(m_app->doc());
    QVERIFY(m_app->doc()->addFunction(scene));
    auto *chaser = new Chaser(m_app->doc());
    QVERIFY(m_app->doc()->addFunction(chaser));
    const QVariant previous = m_app->rootContext()->contextProperty("chaserEditor");
    const auto cleanup = qScopeGuard([&]() {
        m_app->rootContext()->setContextProperty("chaserEditor", previous);
        m_app->doc()->deleteFunction(chaser->id());
        m_app->doc()->deleteFunction(scene->id());
    });
    chaser->setTempoType(Function::TempoType(from));
    chaser->setFadeInMode(Chaser::PerStep);
    chaser->setFadeOutMode(Chaser::PerStep);
    chaser->setDurationMode(Chaser::PerStep);
    QVERIFY(chaser->addStep(ChaserStep(scene->id(), 500, hold, 250)));
    ChaserEditor editor(m_app.get(), m_app->doc());
    editor.setFunctionID(chaser->id());
    const int to = from == Function::Time ? Function::Beats : Function::Time;
    editor.setTempoType(to);
    const auto converted = chaser->steps().first();
    QCOMPARE(converted.duration, Function::speedAdd(converted.fadeIn, converted.hold));
    if (hold == Function::infiniteSpeed() || hold == Function::defaultSpeed())
        QCOMPARE(converted.hold, hold);
    editor.setTempoType(from);
    const auto restored = chaser->steps().first();
    QCOMPARE(restored.duration, Function::speedAdd(restored.fadeIn, restored.hold));
    QCOMPARE(restored.hold, hold);
}

void UpstreamIntegration_Test::incomingChaserCommonHistory_data()
{
    QTest::addColumn<uint>("hold");
    QTest::addColumn<int>("property");
    for (uint hold : {1000u, Function::infiniteSpeed()})
        for (int property : {int(Function::FadeIn), int(Function::Hold)})
            QTest::newRow(qPrintable(QString("%1-%2").arg(hold).arg(property))) << hold << property;
}

void UpstreamIntegration_Test::incomingChaserCommonHistory()
{
    QFETCH(uint, hold);
    QFETCH(int, property);
    Doc *doc = m_app->doc();
    auto *scene = new Scene(doc);
    auto *chaser = new Chaser(doc);
    QVERIFY(doc->addFunction(scene));
    QVERIFY(doc->addFunction(chaser));
    const QVariant previous = m_app->rootContext()->contextProperty("chaserEditor");
    Tardis *history = Tardis::instance();
    const auto cleanup = qScopeGuard([&]() {
        history->resetHistory();
        m_app->rootContext()->setContextProperty("chaserEditor", previous);
        doc->deleteFunction(chaser->id());
        doc->deleteFunction(scene->id());
    });
    chaser->setFadeInMode(Chaser::Common);
    chaser->setDurationMode(Chaser::Common);
    chaser->setFadeInSpeed(500);
    const uint oldDuration = Function::speedAdd(500, hold);
    chaser->setDuration(oldDuration);
    QVERIFY(chaser->addStep(ChaserStep(scene->id(), 500, hold)));
    ChaserEditor editor(m_app.get(), doc);
    editor.setFunctionID(chaser->id());
    history->enqueueAction(Tardis::FunctionSetName, scene->id(), "fence-before", "fence-after");
    const auto drained = [&]() {
        QMutexLocker locker(&history->m_historyMutex);
        return !history->m_history.isEmpty() &&
            history->m_history.last().m_newValue.toString() == "fence-after";
    };
    QTRY_VERIFY(drained());
    history->resetHistory();
    editor.setStepSpeed(0, 1500, property);
    const uint newFade = property == Function::FadeIn ? 1500u : 500u;
    const uint newHold = property == Function::Hold ? 1500u : hold;
    const uint newDuration = Function::speedAdd(newFade, newHold);
    QCOMPARE(chaser->fadeInSpeed(), newFade);
    QCOMPARE(chaser->duration(), newDuration);
    const auto editRecorded = [&]() {
        QMutexLocker locker(&history->m_historyMutex);
        return !history->m_history.isEmpty() &&
            history->m_history.last().m_action == Tardis::ChaserSetStepDuration &&
            history->m_history.last().m_objID == chaser->id() &&
            history->m_history.last().m_newValue.value<UIntPair>().second == newDuration;
    };
    QTRY_VERIFY(editRecorded());
    history->undoAction();
    QCOMPARE(chaser->fadeInSpeed(), 500u);
    QCOMPARE(chaser->duration(), oldDuration);
    history->redoAction();
    QCOMPARE(chaser->fadeInSpeed(), newFade);
    QCOMPARE(chaser->duration(), newDuration);
}

void UpstreamIntegration_Test::incomingSettingsScroll_data()
{
    QTest::addColumn<int>("height");
    QTest::newRow("short-viewport") << 220;
    QTest::newRow("fitting-viewport") << 1600;
}

void UpstreamIntegration_Test::incomingSettingsScroll()
{
    QFETCH(int, height);
    const QSize oldSize = m_app->size();
    const bool wasVisible = m_app->isVisible();
    const auto restore = qScopeGuard([&]() {
        m_app->resize(oldSize);
        if (!wasVisible) m_app->hide();
    });
    m_app->show();
    QTRY_VERIFY(m_app->isExposed());
    m_app->resize(1200, height);
    QTRY_COMPARE(m_app->height(), height);
    QQmlComponent component(m_app->engine(), QUrl("qrc:/UISettingsEditor.qml"));
    std::unique_ptr<QObject> object(component.create(m_app->rootContext()));
    QVERIFY2(object, qPrintable(component.errorString()));
    auto *item = qobject_cast<QQuickItem *>(object.get());
    QVERIFY(item);
    item->setParentItem(m_app->rootObject());
    item->setZ(1000);
    QQmlExpression flickable(qmlContext(item), item, "editorFlickable");
    auto *flick = flickable.evaluate().value<QObject *>();
    QVERIFY2(flick, qPrintable(flickable.error().toString()));
    flick->setProperty("height", height);
    QTRY_VERIFY(flick->property("contentHeight").toReal() > 0);
    const qreal overflow = qMax(qreal(0), flick->property("contentHeight").toReal() - height);
    if (height == 220) QVERIFY(overflow > 0);
    else QCOMPARE(overflow, 0);
    flick->setProperty("contentY", overflow);
    QCOMPARE(flick->property("contentY").toReal(), overflow);
    QQmlExpression wheel(qmlContext(item), item, "sfSlider.wheelEnabled");
    QVERIFY(!wheel.evaluate().toBool());
    QVERIFY(!wheel.hasError());
    QQmlExpression preset(qmlContext(item), item, "presetCombo.model.indexOf('Classic')");
    QVERIFY(preset.evaluate().toInt() >= 0);
    QVERIFY(!preset.hasError());
    QQmlExpression sliderExpression(qmlContext(item), item, "sfSlider");
    auto *slider = sliderExpression.evaluate().value<QQuickItem *>();
    QVERIFY(slider);
    flick->setProperty("contentY", 0);
    const qreal scale = slider->property("value").toReal();
    QSignalSpy painted(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!painted.isEmpty());
    const QPoint pointer = slider->mapToScene(QPointF(slider->width() / 2, slider->height() / 2)).toPoint();
    QVERIFY(QRect(QPoint(), m_app->size()).contains(pointer));
    QWheelEvent event(pointer, m_app->mapToGlobal(pointer), QPoint(), QPoint(0, -120),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(m_app.get(), &event);
    QCoreApplication::processEvents();
    QCOMPARE(slider->property("value").toReal(), scale);
    if (height == 220) QTRY_VERIFY(flick->property("contentY").toReal() > 0);
    else QCOMPARE(flick->property("contentY").toReal(), 0);
}

void UpstreamIntegration_Test::incomingScriptNotifications_data()
{
    QTest::addColumn<bool>("waitForStart");
    QTest::newRow("main-loop-function-start") << true;
    QTest::newRow("main-loop-function-stop") << false;
}

void UpstreamIntegration_Test::incomingScriptNotifications()
{
    QFETCH(bool, waitForStart);
    Doc document(nullptr);
    Doc *doc = &document;
    auto *fixture = new Fixture(doc);
    fixture->setChannels(2);
    QVERIFY(doc->addFixture(fixture));
    auto *scene = new Scene(doc);
    scene->setValue(fixture->id(), 1, 91);
    auto *script = new Script(doc);
    QVERIFY(doc->addFunction(scene));
    QVERIFY(doc->addFunction(script));
    auto *timer = doc->masterTimer();
    const auto cleanup = qScopeGuard([&]() {
        script->stop(FunctionParent::master());
        scene->stop(FunctionParent::master());
        QTRY_VERIFY(script->stopped() && scene->stopped());
        timer->stop();
        doc->deleteFunction(script->id());
        doc->deleteFunction(scene->id());
    });
    script->setData(QString("Engine.setFixture(%1, 0, 173); Engine.waitFunction%2(%3);")
        .arg(fixture->id()).arg(waitForStart ? "Start" : "Stop").arg(scene->id()));
    doc->inputOutputMap()->startUniverses();
    QTRY_VERIFY(doc->inputOutputMap()->universes().first()->isRunning());
    timer->start();
    if (!waitForStart)
    {
        scene->start(timer, FunctionParent::master());
        QTRY_VERIFY(scene->isRunning());
    }
    script->start(timer, FunctionParent::master());
    const auto awaiting = [&]() {
        if (!script->m_runner)
            return false;
        QMutexLocker lock(&script->m_runner->m_mutex);
        return script->m_runner->m_waitFunctionId == scene->id();
    };
    QTRY_VERIFY(awaiting());
    QCOMPARE(script->m_runner->thread(), QCoreApplication::instance()->thread());
    if (waitForStart)
        scene->start(timer, FunctionParent::master());
    else
        scene->stop(FunctionParent::master());
    QTRY_VERIFY(script->stopped());
    QTRY_VERIFY(script->m_runner == nullptr);
    QCOMPARE(doc->inputOutputMap()->universes().first()->preGMValue(0), uchar(0));
    if (waitForStart)
        QTRY_COMPARE(doc->inputOutputMap()->universes().first()->preGMValue(1), uchar(91));
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

void UpstreamIntegration_Test::C3_additiveColors_data()
{
    QTest::addColumn<int>("emitter");
    QTest::addColumn<QColor>("expected");
    QTest::newRow("white") << int(QLCChannel::White) << QColor(192, 160, 144);
    QTest::newRow("amber") << int(QLCChannel::Amber) << QColor(192, 95, 16);
    QTest::newRow("uv") << int(QLCChannel::UV) << QColor(138, 32, 122);
    QTest::newRow("cyan-emitter") << int(QLCChannel::Cyan) << QColor(64, 160, 144);
    QTest::newRow("magenta-emitter") << int(QLCChannel::Magenta) << QColor(192, 32, 144);
    QTest::newRow("yellow-emitter") << int(QLCChannel::Yellow) << QColor(192, 160, 16);
    QTest::newRow("lime") << int(QLCChannel::Lime) << QColor(151, 160, 39);
    QTest::newRow("indigo") << int(QLCChannel::Indigo) << QColor(101, 32, 81);
}

void UpstreamIntegration_Test::C3_additiveColors()
{
    QFETCH(int, emitter);
    QFETCH(QColor, expected);
    QLCFixtureDef definition;
    auto *mode = new QLCFixtureMode(&definition);
    QVERIFY(definition.addMode(mode));
    for (int color : {int(QLCChannel::Red), int(QLCChannel::Green), int(QLCChannel::Blue), emitter})
    {
        auto *channel = new QLCChannel;
        channel->setName(QString::number(color));
        channel->setGroup(QLCChannel::Intensity);
        channel->setColour(QLCChannel::PrimaryColour(color));
        QVERIFY(definition.addChannel(channel));
        QVERIFY(mode->insertChannel(channel, definition.channels().size() - 1));
    }
    Fixture fixture(m_app->doc());
    fixture.setFixtureDefinition(&definition, mode);
    fixture.setChannelValues(QByteArray::fromHex("40201080"));
    const QColor actual = FixtureUtils::headColor(&fixture, 0).toRgb();
    QCOMPARE(actual.rgb(), expected.rgb());

    if (emitter == QLCChannel::White || emitter == QLCChannel::Amber || emitter == QLCChannel::UV)
    {
        QQmlComponent component(m_app->engine(), QUrl("qrc:/MultiColorBox.qml"));
        std::unique_ptr<QObject> box(component.create());
        QVERIFY2(box, qPrintable(component.errorString()));
        QVERIFY(box->setProperty("primary", QColor(64, 32, 16)));
        const QColor secondary(emitter == QLCChannel::White ? 128 : 0,
                               emitter == QLCChannel::Amber ? 128 : 0,
                               emitter == QLCChannel::UV ? 128 : 0);
        QVERIFY(box->setProperty("secondary", secondary));
        QCOMPARE(box->property("color").value<QColor>().rgb(), expected.rgb());
        QVERIFY(box->setProperty("biColor", true));
        QCOMPARE(box->property("color").value<QColor>(), QColor(64, 32, 16));
    }
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

void UpstreamIntegration_Test::vcWidgetPointerDrop_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<bool>("tree");
    for (qreal scale : {qreal(0.8), qreal(1.1), qreal(2)})
        for (bool cancel : {false, true})
        {
            QTest::newRow(qPrintable(QString("%1-%2").arg(scale).arg(cancel ? "cancel" : "cross-subpage")))
                << scale << cancel << false;
            QTest::newRow(qPrintable(QString("%1-tree-%2").arg(scale).arg(cancel ? "cancel" : "cross-subpage")))
                << scale << cancel << true;
        }
}

void UpstreamIntegration_Test::vcWidgetPointerDrop()
{
    QFETCH(qreal, scale);
    QFETCH(bool, cancel);
    QFETCH(bool, tree);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const QSize oldSize = m_app->size();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode(), oldSnap = vc->snapping();
    const bool wasModified = m_app->doc()->isModified();
    const int oldPage = vc->selectedPage();
    const qreal oldScale = vc->page(0)->pageScale();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    QVariantList roots;
    quint32 functionID = Function::invalidId();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets(roots);
        if (functionID != Function::invalidId()) m_app->doc()->deleteFunction(functionID);
        vc->page(0)->setPageScale(oldScale - vc->page(0)->pageScale());
        vc->setSelectedPage(oldPage);
        vc->setEditMode(oldEdit);
        vc->setSnapping(oldSnap);
        vc->setSelectedWidgetIDs(oldSelection);
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified) m_app->doc()->resetModified();
    });
    for (int index = 0; index < 2; ++index)
    {
        const QVariantMap result = vc->createWidgetGeometry(vc->page(0)->id(), "Frame",
            QRectF(20 + index * 340, 20, 300, 260));
        QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
        roots.append(result.value("widgetIDs").toList().first());
    }
    auto *source = qobject_cast<VCFrame *>(vc->widget(roots[0].toUInt()));
    auto *target = qobject_cast<VCFrame *>(vc->widget(roots[1].toUInt()));
    QVERIFY(source && target);
    target->setMultiPageMode(true);
    target->setTotalPagesNumber(2);
    target->setCurrentPage(1);
    QVariantList ids;
    for (int index = 0; index < 2; ++index)
    {
        const QVariantMap result = vc->createWidgetGeometry(source->id(), "Button",
            QRectF(10 + index * 120, 60, 100, 70));
        QVERIFY(result.value("accepted").toBool());
        ids.append(result.value("widgetIDs").toList().first());
    }
    auto *scene = new Scene(m_app->doc());
    scene->setName("Pointer drop binding");
    QVERIFY(m_app->doc()->addFunction(scene));
    functionID = scene->id();
    auto *first = qobject_cast<VCButton *>(vc->widget(ids[0].toUInt()));
    VCWidget *second = vc->widget(ids[1].toUInt());
    QVERIFY(first && second);
    first->setFunctionID(functionID);
    m_app->doc()->masterTimer()->stop();
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1600, 950);
    contexts->switchToContext("VC");
    vc->setSelectedPage(0);
    vc->setEditMode(true);
    vc->setSnapping(false);
    vc->page(0)->setPageScale(scale - vc->page(0)->pageScale());
    QCOMPARE(vc->page(0)->pageScale(), scale);
    vc->setSelectedWidgetIDs(ids);
    QQuickItem *item = nullptr, *area = nullptr;
    QTRY_VERIFY((item = first->renderItem()));
    QTRY_VERIFY(target->renderItem());
    QTRY_VERIFY((area = target->renderItem()->findChild<QQuickItem *>(QString("frameDropArea%1").arg(target->id()))));
    QQuickItem *sourceRow = nullptr, *treePane = nullptr, *treeDrag = nullptr;
    QObject *treeMouse = nullptr;
    if (tree)
    {
        auto *console = m_app->rootObject()->findChild<QQuickItem *>("virtualConsole");
        QVERIFY(console);
        QVERIFY(console->setProperty("outlineOpen", true));
        treePane = console->findChild<QQuickItem *>("vcOutlinePane");
        QVERIFY(treePane);
        QTRY_VERIFY(vc->outlineModel()->indexForKey(QString("widget:%1").arg(first->id())).isValid());
        QVERIFY(QMetaObject::invokeMethod(treePane, "toggleExpanded",
            Q_ARG(QVariant, QString("widget:%1").arg(source->id()))));
        QVERIFY(QMetaObject::invokeMethod(treePane, "toggleExpanded",
            Q_ARG(QVariant, QString("widget:%1").arg(target->id()))));
        QQuickItem *targetRow = nullptr;
        std::function<void(QQuickItem *)> find = [&](QQuickItem *candidate) {
            if (candidate->objectName() == QString("vcOutlineRow-%1").arg(first->id())) sourceRow = candidate;
            if (candidate->objectName() == QString("vcOutlineRow-subpage:%1:1").arg(target->id())) targetRow = candidate;
            for (QQuickItem *child : candidate->childItems()) find(child);
        };
        QTRY_VERIFY((find(treePane), sourceRow && targetRow));
        QQmlExpression expression(qmlContext(sourceRow), sourceRow, "outlineDrag");
        treeDrag = qobject_cast<QQuickItem *>(expression.evaluate().value<QObject *>());
        QVERIFY2(treeDrag, qPrintable(expression.error().toString()));
        QQmlExpression mouseExpression(qmlContext(sourceRow), sourceRow, "mouseArea");
        treeMouse = mouseExpression.evaluate().value<QObject *>();
        QVERIFY2(treeMouse, qPrintable(mouseExpression.error().toString()));
        area = nullptr;
        for (QQuickItem *child : targetRow->childItems())
            if (child->property("drag").isValid()) area = child;
        QVERIFY(area);
    }
    QQuickItem *viewport = vc->currentPageItem();
    QVERIFY(viewport);
    QVERIFY(viewport->setProperty("contentX", 20 * scale));
    QVERIFY(viewport->setProperty("contentY", 10 * scale));
    QCOMPARE(viewport->property("contentX").toReal(), 20 * scale);
    QCOMPARE(viewport->property("contentY").toReal(), 10 * scale);
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    const QRectF firstRect = first->logicalGeometry(), secondRect = second->logicalGeometry();
    QQuickItem *firstParent = item->parentItem(), *secondParent = second->renderItem()->parentItem();
    QByteArray before;
    QXmlStreamWriter writer(&before);
    vc->saveXML(&writer);
    m_app->doc()->resetModified();
    QSignalSpy dropped(area, SIGNAL(dropped(QQuickDragEvent*)));
    QSignalSpy structure(vc, &VirtualConsole::structureChanged);
    QSignalSpy starts(m_app->doc()->masterTimer(), &MasterTimer::functionStarted);
    QVERIFY(dropped.isValid());
    const QPoint press = tree ? sourceRow->mapToScene(QPointF(sourceRow->width() * 0.7, sourceRow->height() / 2)).toPoint()
        : item->mapToScene(QPointF(item->width() * 0.4, item->height() * 0.4)).toPoint();
    QVERIFY(QRect(QPoint(), m_app->size()).contains(press));
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
    if (tree)
    {
        qInfo() << "TREE_PRESS" << press << "rowVisible" << sourceRow->isVisible()
                << "rowSize" << sourceRow->size() << "pressed" << treeMouse->property("pressed");
        QVERIFY(treeMouse->property("pressed").toBool());
    }
    const auto releaseMouse = qScopeGuard([&]() {
        if (tree ? treePane->property("dragging").toBool() : item->property("moving").toBool())
            QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
    });
    QTest::mouseMove(m_app.get(), press + QPoint(25, 0), 20);
    if (tree) QTest::mouseMove(m_app.get(), press + QPoint(40, 0), 20);
    if (tree)
    {
        QTRY_VERIFY(treePane->property("dragging").toBool());
        QCOMPARE(treeDrag->property("widgetIDs").toList(), ids);
    }
    else
    {
        QTRY_VERIFY(item->property("moving").toBool());
        QCOMPARE(item->property("draggedIDs").toList(), ids);
    }
    const QPointF hotspot = tree ? QPointF() : item->property("dragHotSpot").toPointF();
    const QPoint drop = tree ? area->mapToScene(QPointF(area->width() * 0.7, area->height() / 2)).toPoint()
        : area->mapToScene(QPointF(20 * scale, 70 * scale) + hotspot).toPoint();
    QVERIFY(QRect(QPoint(), m_app->size()).contains(drop));
    const QPointF pointerExpected = (area->mapFromScene(drop) - hotspot) / scale;
    QTest::mouseMove(m_app.get(), drop, 20);
    QObject *drag = area->property("drag").value<QObject *>();
    QVERIFY(drag);
    QTRY_COMPARE(drag->property("source").value<QObject *>(), tree ? treeDrag : item);
    if (!tree) QVERIFY(target->renderItem()->property("dropActive").toBool());
    QVERIFY(drag->property("x").isValid() && drag->property("y").isValid());
    const QPointF expected = (QPointF(drag->property("x").toReal(), drag->property("y").toReal()) - hotspot) / scale;
    if (tree)
    {
        QVERIFY(treePane->property("dropHint").toString().contains(target->caption()));
        QQmlExpression destination(qmlContext(area->parentItem()), area->parentItem(), "dropIntent.targetID");
        QQmlExpression subpage(qmlContext(area->parentItem()), area->parentItem(), "dropIntent.subpage");
        QCOMPARE(destination.evaluate().toUInt(), target->id());
        QCOMPARE(subpage.evaluate().toInt(), 1);
    }
    else
    {
        QVERIFY(std::abs(expected.x() - pointerExpected.x()) <= 1 / scale);
        QVERIFY(std::abs(expected.y() - pointerExpected.y()) <= 1 / scale);
    }
    QCOMPARE(first->logicalGeometry(), firstRect);
    QCOMPARE(second->logicalGeometry(), secondRect);
    if (cancel) QTest::keyClick(m_app.get(), Qt::Key_Escape);
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, drop);
    QVERIFY(!item->property("moving").toBool());
    if (tree) QVERIFY(!treePane->property("dragging").toBool());
    QCOMPARE(vc->selectedWidgetIDs(), ids);
    QCOMPARE(first->functionID(), functionID);
    QCOMPARE(target->currentPage(), 1);
    QCOMPARE(starts.count(), 0);
    QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(scene));
    QVERIFY(!scene->isRunning());
    if (cancel)
    {
        QCOMPARE(dropped.count(), 0);
        QCOMPARE(structure.count(), 0);
        QByteArray after;
        QXmlStreamWriter afterWriter(&after);
        vc->saveXML(&afterWriter);
        QCOMPARE(after, before);
        QCOMPARE(item->parentItem(), firstParent);
        QCOMPARE(second->renderItem()->parentItem(), secondParent);
        QVERIFY(!m_app->doc()->isModified());
    }
    else
    {
        QCOMPARE(dropped.count(), 1);
        QCOMPARE(structure.count(), 1);
        QCOMPARE(first->parent(), target);
        QCOMPARE(second->parent(), target);
        QCOMPARE(first->page(), 1);
        QCOMPARE(second->page(), 1);
        QCOMPARE(first->logicalGeometry(), tree ? firstRect : QRectF(expected, firstRect.size()));
        QCOMPARE(second->logicalGeometry(), tree ? secondRect :
            QRectF(expected + secondRect.topLeft() - firstRect.topLeft(), secondRect.size()));
        QQuickItem *targetArea = target->renderItem()->findChild<QQuickItem *>(QString("frameDropArea%1").arg(target->id()));
        QCOMPARE(item->parentItem(), targetArea);
        QCOMPARE(second->renderItem()->parentItem(), targetArea);
        QVERIFY(source->orderedChildIDs(0).isEmpty());
        QCOMPARE(target->orderedChildIDs(1), ids);
        QVERIFY(m_app->doc()->isModified());
    }
}

void UpstreamIntegration_Test::vcScrollbarChromeReplay_data()
{
    QTest::addColumn<qreal>("density");
    QTest::newRow("native-metrics-1.1") << qreal(5.077272727272727);
}

void UpstreamIntegration_Test::vcScrollbarChromeReplay()
{
    QFETCH(qreal, density);
    auto *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QVariant oldDensity = m_app->rootContext()->contextProperty("screenPixelDensity");
    const QRect oldWindow = m_app->geometry();
    const QString oldContext = contexts->currentContext();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode(), oldSnap = vc->snapping();
    const bool modified = m_app->doc()->isModified();
    const int oldPage = vc->selectedPage();
    const qreal oldScale = vc->page(0)->pageScale();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    QVariantList roots;
    QPointer<QQuickItem> viewport;
    QPointer<QQuickItem> console, functions;
    QVariant oldOutlineOpen, oldOutlineWidth, oldLoader, oldFunctionsOpen;
    qreal oldFunctionsWidth = 0;
    QSizeF oldViewSize;
    QPointF oldViewPosition;
    qreal oldX = 0, oldY = 0;
    const auto cleanup = qScopeGuard([&]() {
        QTest::keyClick(m_app.get(), Qt::Key_Escape);
        vc->deleteVCWidgets(roots);
        m_app->rootContext()->setContextProperty("screenPixelDensity", oldDensity);
        if (console)
        {
            console->setProperty("outlineOpen", oldOutlineOpen);
            console->setProperty("outlineWidth", oldOutlineWidth);
        }
        if (functions)
        {
            functions->setProperty("loaderSource", oldLoader);
            functions->setProperty("isOpen", oldFunctionsOpen);
            functions->setWidth(oldFunctionsWidth);
        }
        if (viewport)
        {
            viewport->setSize(oldViewSize);
            viewport->setPosition(oldViewPosition);
            viewport->setProperty("contentX", oldX);
            viewport->setProperty("contentY", oldY);
        }
        vc->page(0)->setPageScale(oldScale - vc->page(0)->pageScale());
        vc->setSelectedPage(oldPage);
        vc->setEditMode(oldEdit);
        vc->setSnapping(oldSnap);
        vc->setSelectedWidgetIDs(oldSelection);
        contexts->switchToContext(oldContext);
        m_app->setGeometry(oldWindow);
        m_app->setVisible(oldVisible);
        if (!modified) m_app->doc()->resetModified();
    });
    m_app->rootContext()->setContextProperty("screenPixelDensity", density);
    const QVariantMap created = vc->createWidgetGeometry(vc->page(0)->id(), "Frame",
        QRectF(780, 20, 270.99545454545455, 700));
    QVERIFY2(created.value("accepted").toBool(), qPrintable(created.value("error").toString()));
    roots = created.value("widgetIDs").toList();
    auto *scenes = qobject_cast<VCFrame *>(vc->widget(roots.first().toUInt()));
    QVERIFY(scenes);
    scenes->setCaption("Scenes / Master");
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->setGeometry(0, 65, 1600, 950);
    m_app->setPosition(m_app->position() + QPoint(0, 65) - m_app->mapToGlobal(QPoint()));
    contexts->switchToContext("VC");
    console = m_app->rootObject()->findChild<QQuickItem *>("virtualConsole");
    QVERIFY(console);
    oldOutlineOpen = console->property("outlineOpen");
    oldOutlineWidth = console->property("outlineWidth");
    console->setProperty("outlineWidth", 288);
    console->setProperty("outlineOpen", true);
    functions = console->findChild<QQuickItem *>("vcFunctionsPane");
    QVERIFY(functions);
    oldLoader = functions->property("loaderSource");
    oldFunctionsOpen = functions->property("isOpen");
    oldFunctionsWidth = functions->width();
    functions->setProperty("loaderSource", "qrc:/FunctionManager.qml");
    QVERIFY(QMetaObject::invokeMethod(functions, "animatePanel", Q_ARG(QVariant, true)));
    QTRY_VERIFY(functions->width() >= 319);
    vc->setSelectedPage(0);
    vc->setEditMode(true);
    vc->setSnapping(false);
    vc->page(0)->setPageScale(1.1 - vc->page(0)->pageScale());
    QCOMPARE(vc->page(0)->pageScale(), qreal(1.1));
    QTRY_VERIFY(scenes->renderItem());
    viewport = vc->currentPageItem();
    QVERIFY(viewport);
    oldViewSize = viewport->size();
    oldViewPosition = viewport->position();
    oldX = viewport->property("contentX").toReal();
    oldY = viewport->property("contentY").toReal();
    const QRectF nativeView(292.56954545454545, 156.39090909090908, 987.4304545454545, 858.6090909090909);
    const QPointF origin = m_app->mapToGlobal(QPoint());
    QVERIFY(viewport->setProperty("contentX", 818.0));
    QVERIFY(viewport->setProperty("contentY", -40.61818181818182));
    vc->setSelectedWidgetIDs({scenes->id()});
    QSignalSpy painted(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!painted.isEmpty());
    auto globalRect = [&](QQuickItem *item) {
        QRectF rect = item->mapRectToScene(QRectF(QPointF(), item->size()));
        rect.translate(origin);
        return rect;
    };
    qInfo() << "SCROLLBAR_REPLAY_SETUP" << globalRect(viewport) << "wanted" << nativeView
            << "position" << viewport->position() << "parent" << globalRect(viewport->parentItem());
    const QPointF decorationDelta = globalRect(viewport).topLeft() - nativeView.topLeft();
    QVERIFY(std::abs(decorationDelta.x()) <= 2 && std::abs(decorationDelta.y()) <= 2);
    QVERIFY(std::abs(viewport->width() - nativeView.width()) < 0.001);
    QVERIFY(std::abs(viewport->height() - nativeView.height()) < 0.001);
    QVERIFY(std::abs(viewport->property("bottomMargin").toReal() - 40.61818181818182) < 0.001);
    QQuickItem *fragment = nullptr, *bar = nullptr;
    std::function<void(QQuickItem *)> locate = [&](QQuickItem *item) {
        if (item->objectName() == QString("vcResizeFragment-%1-2-0").arg(scenes->id())) fragment = item;
        if (item->property("orientation").isValid() && item->property("position").isValid() &&
            item->property("size").isValid() && item->property("pressed").isValid() &&
            item->property("orientation").toInt() == Qt::Horizontal && viewport->isAncestorOf(item))
            bar = item;
        for (QQuickItem *child : item->childItems()) locate(child);
    };
    locate(m_app->rootObject());
    QVERIFY(fragment && bar && fragment->isVisible() && bar->isVisible());
    QRectF clipped = globalRect(fragment).intersected(globalRect(viewport));
    const QRectF barRect = globalRect(bar);
    const QPointF recordedPress = QPointF(650.6645454545455, 1002.0045454545455) + decorationDelta;
    QVERIFY(barRect.contains(recordedPress));
    const QPointF press = clipped.center();
    const QPointF local = press - origin;
    QVERIFY(QRectF(QPointF(), m_app->size()).contains(local));
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    QVERIFY(vc->saveXML(&writer));
    for (int frame = 0; frame < 3; ++frame)
    {
        QSignalSpy stable(m_app.get(), &QQuickWindow::frameSwapped);
        m_app->update();
        QTRY_VERIFY(!stable.isEmpty());
        QCOMPARE(viewport->property("contentX").toReal(), qreal(818));
    }
    const QImage window = m_app->grabWindow();
    QQmlComponent reference(m_app->engine());
    reference.setData("import QtQuick\nItem { width: 42; height: 42; Image { objectName: \"referenceArrow\"; "
        "width: 40; height: 40; sourceSize: Qt.size(width, height); source: \"qrc:/arrow-corner.svg\" } }", QUrl());
    QVERIFY2(reference.isReady(), qPrintable(reference.errorString()));
    std::unique_ptr<QQuickItem> referenceItem(qobject_cast<QQuickItem *>(reference.create()));
    QVERIFY(referenceItem);
    referenceItem->setParentItem(m_app->contentItem());
    referenceItem->setPosition(QPointF(-100, -100));
    auto *arrow = referenceItem->findChild<QQuickItem *>("referenceArrow");
    QVERIFY(arrow);
    const QRectF imageRect = globalRect(fragment).translated(-origin);
    arrow->setPosition(QPointF(imageRect.x() - qFloor(imageRect.x()), imageRect.y() - qFloor(imageRect.y())));
    auto capture = referenceItem->grabToImage();
    QVERIFY(capture);
    QTRY_VERIFY(!capture->image().isNull());
    const QImage expected = capture->image();
    int opaque = 0, different = 0;
    qInfo() << "SCROLLBAR_PAINT_MAPPING" << "window" << window.size() << window.devicePixelRatio()
            << "app" << m_app->size() << "reference" << expected.size() << expected.devicePixelRatio()
            << "logical" << imageRect;
    for (int y = 1; y < expected.height() - 1; ++y)
        for (int x = 1; x < expected.width() - 1; ++x)
        {
            if (expected.pixelColor(x, y).alpha() < 250 ||
                expected.pixelColor(x - 1, y).alpha() < 250 || expected.pixelColor(x + 1, y).alpha() < 250 ||
                expected.pixelColor(x, y - 1).alpha() < 250 || expected.pixelColor(x, y + 1).alpha() < 250) continue;
            const QPointF mapped((qFloor(imageRect.x()) + (x + 0.5) * referenceItem->width() / expected.width()) *
                                     window.width() / m_app->width(),
                                 (qFloor(imageRect.y()) + (y + 0.5) * referenceItem->height() / expected.height()) *
                                     window.height() / m_app->height());
            const QPoint point(qFloor(mapped.x()), qFloor(mapped.y()));
            QVERIFY(window.rect().contains(point));
            const QColor actual = window.pixelColor(point), wanted = expected.pixelColor(x, y);
            ++opaque;
            if (std::max({std::abs(actual.red() - wanted.red()), std::abs(actual.green() - wanted.green()),
                          std::abs(actual.blue() - wanted.blue())}) > 40)
                ++different;
        }
    qInfo() << "SCROLLBAR_CLEAR_GRAPHIC_PAINT" << "opaque" << opaque << "different" << different;
    QVERIFY(opaque > 100);
    QCOMPARE(different, 0);
    const QString directory = qEnvironmentVariable("QLC_VC_VISUAL_DIR");
    if (!directory.isEmpty())
    {
        QVERIFY(QDir().mkpath(directory));
        QVERIFY(window.save(QDir(directory).filePath(QString("scrollbar-replay-dpr%1.png").arg(m_app->devicePixelRatio()))));
    }
    QMouseEvent down(QEvent::MouseButtonPress, local, press, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QVERIFY(QCoreApplication::sendEvent(m_app.get(), &down));
    qInfo() << "NATIVE_SCROLLBAR_REPLAY" << "roleID" << scenes->id() << "density" << density << "modelDensity" << vc->pixelDensity()
            << "dpr" << m_app->devicePixelRatio() << "decorationDelta" << decorationDelta << "viewport" << globalRect(viewport)
            << "fragment" << globalRect(fragment) << "painted" << clipped << "bar" << barRect
            << "press" << press << "recordedPress" << recordedPress << "barPressed" << bar->property("pressed")
            << "sceneResizing" << scenes->renderItem()->property("resizing")
            << "contentXAfter" << viewport->property("contentX");
    const bool resizing = scenes->renderItem()->property("resizing").toBool();
    QTest::keyClick(m_app.get(), Qt::Key_Escape);
    QMouseEvent up(QEvent::MouseButtonRelease, local, press, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QVERIFY(QCoreApplication::sendEvent(m_app.get(), &up));
    QVERIFY2(resizing, "Reported painted root-corner center must hit Scenes resize, not the viewport scrollbar");
    QVERIFY(clipped.intersected(barRect).isEmpty());
    QCOMPARE(clipped.size(), fragment->size());
    QCOMPARE(viewport->property("contentX").toReal(), qreal(818));
    QByteArray after;
    QXmlStreamWriter afterWriter(&after);
    QVERIFY(vc->saveXML(&afterWriter));
    QCOMPARE(after, xml);
    const QRectF before = scenes->logicalGeometry();
    QSignalSpy changed(scenes, &VCWidget::geometryChanged);
    QMouseEvent resizeDown(QEvent::MouseButtonPress, local, press, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QVERIFY(QCoreApplication::sendEvent(m_app.get(), &resizeDown));
    QVERIFY(scenes->renderItem()->property("resizing").toBool());
    const QPointF end = local + QPointF(0, 4), globalEnd = press + QPointF(0, 4);
    QMouseEvent move(QEvent::MouseMove, end, globalEnd, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QVERIFY(QCoreApplication::sendEvent(m_app.get(), &move));
    QMouseEvent resizeUp(QEvent::MouseButtonRelease, end, globalEnd, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QVERIFY(QCoreApplication::sendEvent(m_app.get(), &resizeUp));
    QCOMPARE(scenes->logicalGeometry(), QRectF(before.topLeft(), QSizeF(before.width(), before.height() + 4 / 1.1)));
    QCOMPARE(changed.count(), 1);
    QVERIFY(!scenes->renderItem()->property("resizing").toBool());
    const QPointF oldLocal = recordedPress - origin;
    QMouseEvent barDown(QEvent::MouseButtonPress, oldLocal, recordedPress, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QVERIFY(QCoreApplication::sendEvent(m_app.get(), &barDown));
    QVERIFY(bar->property("pressed").toBool());
    QVERIFY(!scenes->renderItem()->property("resizing").toBool());
    QVERIFY(std::abs(viewport->property("contentX").toReal() - 259.82596535392094) < 1);
    QMouseEvent barUp(QEvent::MouseButtonRelease, oldLocal, recordedPress, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QVERIFY(QCoreApplication::sendEvent(m_app.get(), &barUp));
}

void UpstreamIntegration_Test::vcSelectedChrome_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<bool>("nested");
    QTest::addColumn<int>("boundary");
    for (qreal scale : {qreal(0.8), qreal(1), qreal(1.1), qreal(2)})
        QTest::newRow(qPrintable(QString::number(scale))) << scale << false << 0;
    for (qreal scale : {qreal(0.8), qreal(1.1), qreal(2)})
    {
        QTest::newRow(qPrintable(QString("%1-nested").arg(scale))) << scale << true << 0;
        QTest::newRow(qPrintable(QString("%1-nested-flush-left").arg(scale))) << scale << true << 1;
        QTest::newRow(qPrintable(QString("%1-nested-near-bottom").arg(scale))) << scale << true << 2;
        QTest::newRow(qPrintable(QString("%1-deeper-flush").arg(scale))) << scale << true << 3;
        QTest::newRow(qPrintable(QString("%1-deeper-near-bottom").arg(scale))) << scale << true << 4;
        QTest::newRow(qPrintable(QString("%1-ancestor-caption").arg(scale))) << scale << true << 5;
        QTest::newRow(qPrintable(QString("%1-ancestor-control").arg(scale))) << scale << true << 6;
        QTest::newRow(qPrintable(QString("%1-clearance-tie").arg(scale))) << scale << true << 7;
        QTest::newRow(qPrintable(QString("%1-scroll-preference").arg(scale))) << scale << true << 8;
    }
}

void UpstreamIntegration_Test::vcSelectedChrome()
{
    QFETCH(qreal, scale);
    QFETCH(bool, nested);
    QFETCH(int, boundary);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const QSize oldSize = m_app->size();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode();
    const bool oldSnap = vc->snapping();
    const bool wasModified = m_app->doc()->isModified();
    const int oldPage = vc->selectedPage();
    const qreal oldScale = vc->page(0)->pageScale();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    QVariantList roots;
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets(roots);
        vc->page(0)->setPageScale(oldScale - vc->page(0)->pageScale());
        vc->setSelectedPage(oldPage);
        vc->setEditMode(oldEdit);
        vc->setSnapping(oldSnap);
        vc->setSelectedWidgetIDs(oldSelection);
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified) m_app->doc()->resetModified();
    });
    quint32 parentID = vc->page(0)->id();
    VCWidget *outer = nullptr;
    if (nested)
    {
        if (boundary == 3 || boundary == 4)
        {
            const QVariantMap grand = vc->createWidgetGeometry(parentID, "Frame", QRectF(20, 20, 1700, 780));
            QVERIFY2(grand.value("accepted").toBool(), qPrintable(grand.value("error").toString()));
            roots = grand.value("widgetIDs").toList();
            parentID = roots.first().toUInt();
            qInfo() << "DEEP_CHROME_PARENT" << vc->widget(parentID)->logicalGeometry()
                    << qobject_cast<VCFrame *>(vc->widget(parentID))->contentRect();
        }
        const QVariantMap ancestor = vc->createWidgetGeometry(parentID, "Frame",
            QRectF(20, boundary == 3 || boundary == 4 ? 40 : 20, boundary == 6 ? 560 : 1100, 650));
        QVERIFY2(ancestor.value("accepted").toBool(), qPrintable(ancestor.value("error").toString()));
        if (roots.isEmpty()) roots = ancestor.value("widgetIDs").toList();
        parentID = ancestor.value("widgetIDs").toList().first().toUInt();
        outer = vc->widget(parentID);
        if (boundary == 5) outer->setCaption(QString("Ancestor caption ").repeated(12));
    }
    QRectF requested = nested ? QRectF(80, 80, 360, 340) : QRectF(20, 20, 360, 340);
    if (boundary && boundary != 7 && boundary != 8)
    {
        const QRectF content = qobject_cast<VCFrame *>(outer)->contentRect();
        requested.moveTopLeft(boundary == 1 || boundary == 3 || boundary == 5 || boundary == 6 ? content.topLeft()
            : QPointF(80, content.bottom() - requested.height() - 2));
        if (boundary == 6)
            requested.moveLeft(content.right() - content.y() + 2 / scale - requested.width());
        QVERIFY(content.contains(requested));
    }
    const QVariantMap created = vc->createWidgetGeometry(parentID, "Frame", requested);
    QVERIFY2(created.value("accepted").toBool(), qPrintable(created.value("error").toString()));
    const quint32 frameID = created.value("widgetIDs").toList().first().toUInt();
    if (!nested) roots.append(frameID);
    const QVariantMap control = vc->createWidgetGeometry(frameID, "Slider", QRectF(10, 190, 90, 140));
    QVERIFY2(control.value("accepted").toBool(), qPrintable(control.value("error").toString()));
    VCWidget *slider = vc->widget(control.value("widgetIDs").toList().first().toUInt());
    slider->setCaption("Intensity");
    VCWidget *frame = vc->widget(frameID);
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(boundary ? QSize(1800, 1700) : nested ? QSize(1800, 1250) : QSize(1440, 900));
    contexts->switchToContext("VC");
    vc->setSelectedPage(0);
    vc->page(0)->setPageScale(scale - vc->page(0)->pageScale());
    QCOMPARE(vc->page(0)->pageScale(), scale);
    QCOMPARE(slider->scaleFactor(), scale);
    vc->setEditMode(true);
    QQuickItem *viewport = nullptr;
    QTRY_VERIFY((viewport = vc->currentPageItem()));
    const qreal margin = viewport->property("leftMargin").toReal();
    QVERIFY(viewport->setProperty("contentX", -margin));
    QVERIFY(viewport->setProperty("contentY", -margin));
    QTRY_VERIFY(frame->renderItem() && slider->renderItem());
    QSignalSpy starts(m_app->doc()->masterTimer(), &MasterTimer::functionStarted);
    auto rectValue = [](const QRectF &rect) {
        return QJsonObject{{"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}};
    };
    QQuickItem *caption = nullptr, *corner = nullptr, *ancestorCaption = nullptr;
    QList<QQuickItem *> handles;
    std::function<void(QQuickItem *)> find = [&](QQuickItem *item) {
        if (item->property("text").toString() == "Intensity" && slider->renderItem()->isAncestorOf(item))
            caption = item;
        if (boundary == 5 && outer->renderItem()->isAncestorOf(item) && !frame->renderItem()->isAncestorOf(item) &&
            item->property("text").toString() == outer->caption())
            ancestorCaption = item;
        const bool named = item->objectName().startsWith(QString("vcResizeHandle-%1-").arg(frameID));
        if (named)
        {
            handles.append(item);
            if (item->property("index").toInt() == 3) corner = item;
        }
        for (QQuickItem *child : item->childItems()) find(child);
    };
    find(m_app->rootObject());
    QVERIFY(caption && corner);
    QCOMPARE(handles.size(), 4);
    QImage ancestorGlyphs, ancestorUnselected;
    for (bool selected : {false, true})
    {
        vc->setSelectedWidgetIDs(selected ? QVariantList{frameID} : QVariantList{});
        QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
        m_app->update();
        QTRY_VERIFY(!rendered.isEmpty());
        handles.clear();
        corner = nullptr;
        find(m_app->rootObject());
        QCOMPARE(handles.size(), 4);
        QVERIFY(corner);
        const QRectF textRect = caption->mapRectToScene(QRectF(QPointF(), QSizeF(caption->width(), caption->height())));
        const QRectF cornerRect = corner->mapRectToScene(QRectF(QPointF(), QSizeF(corner->width(), corner->height())));
        const qreal paintedWidth = caption->property("paintedWidth").toReal();
        const qreal paintedHeight = caption->property("paintedHeight").toReal();
        const QRectF glyphRect = caption->mapRectToScene(QRectF((caption->width() - paintedWidth) / 2, 0,
                                                               paintedWidth, paintedHeight));
        const QRectF intersection = textRect.intersected(cornerRect);
        const QPoint hit = (intersection.isEmpty() ? textRect.center() : intersection.center()).toPoint();
        QVERIFY(QRect(QPoint(), m_app->size()).contains(hit));
        const QRectF frameRect = frame->logicalGeometry(), sliderRect = slider->logicalGeometry();
        const bool cornerVisible = corner->isVisible();
        const QImage beforePress = m_app->grabWindow();
        if (boundary == 5)
        {
            QVERIFY(ancestorCaption);
            if (!selected)
            {
                auto grab = ancestorCaption->grabToImage();
                QVERIFY(grab);
                QTRY_VERIFY(!grab->image().isNull());
                ancestorGlyphs = grab->image();
                ancestorUnselected = beforePress;
            }
            else
            {
                int glyphPixels = 0, changedGlyphPixels = 0;
                for (int y = 0; y < ancestorGlyphs.height(); ++y)
                    for (int x = 0; x < ancestorGlyphs.width(); ++x)
                    {
                        if (ancestorGlyphs.pixelColor(x, y).alpha() < 200) continue;
                        const QPointF mapped = ancestorCaption->mapToScene(
                            QPointF((x + 0.5) * ancestorCaption->width() / ancestorGlyphs.width(),
                                    (y + 0.5) * ancestorCaption->height() / ancestorGlyphs.height()))
                            * beforePress.devicePixelRatio();
                        const QPoint point(qFloor(mapped.x()), qFloor(mapped.y()));
                        if (!beforePress.rect().contains(point)) continue;
                        ++glyphPixels;
                        if (beforePress.pixelColor(point) != ancestorUnselected.pixelColor(point)) ++changedGlyphPixels;
                    }
                qInfo() << "ANCESTOR_CAPTION_PIXELS" << scale << "glyphPixels" << glyphPixels
                        << "changedGlyphPixels" << changedGlyphPixels;
                const QString directory = qEnvironmentVariable("QLC_VC_VISUAL_DIR");
                if (!directory.isEmpty())
                {
                    const QString name = QString("ancestor-caption-%1").arg(scale);
                    QVERIFY(QDir().mkpath(directory));
                    QVERIFY(ancestorUnselected.save(QDir(directory).filePath(name + "-unselected.png")));
                    QVERIFY(beforePress.save(QDir(directory).filePath(name + "-selected.png")));
                    QVERIFY(ancestorGlyphs.save(QDir(directory).filePath(name + "-glyph-mask.png")));
                    QFile receipt(QDir(directory).filePath(name + ".json"));
                    QVERIFY(receipt.open(QIODevice::WriteOnly));
                    receipt.write(QJsonDocument(QJsonObject{{"scale", scale}, {"glyphPixels", glyphPixels},
                        {"changedGlyphPixels", changedGlyphPixels}, {"ancestorCaption", outer->caption()},
                        {"ancestorLogicalRect", rectValue(outer->logicalGeometry())},
                        {"childLogicalRect", rectValue(frame->logicalGeometry())},
                        {"ancestorCaptionSceneRect", rectValue(ancestorCaption->mapRectToScene(
                            QRectF(QPointF(), QSizeF(ancestorCaption->width(), ancestorCaption->height()))))}}).toJson());
                }
                QCOMPARE(changedGlyphPixels, 0);
                QVERIFY(glyphPixels > 100);
            }
        }
        QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, hit);
        QJsonArray selection;
        for (const QVariant &id : vc->selectedWidgetIDs()) selection.append(int(id.toUInt()));
        QJsonObject evidence{{"scale", scale}, {"frameSelectedBeforePress", selected},
            {"nested", nested},
            {"boundary", boundary},
            {"logicalFrameRect", rectValue(frame->logicalGeometry())},
            {"parentContentRect", rectValue(qobject_cast<VCFrame *>(frame->parent())->contentRect())},
            {"caption", caption->property("text").toString()}, {"captionRect", rectValue(textRect)},
            {"paintedWidth", paintedWidth}, {"paintedHeight", paintedHeight}, {"glyphRect", rectValue(glyphRect)},
            {"frameCornerRect", rectValue(cornerRect)}, {"cornerCaptionIntersection", rectValue(intersection)},
            {"cornerGlyphIntersection", rectValue(glyphRect.intersected(cornerRect))},
            {"cornerVisibleBeforePress", cornerVisible}, {"pressSelection", selection},
            {"frameResizingOnPress", frame->renderItem()->property("resizing").toBool()},
            {"sliderResizingOnPress", slider->renderItem()->property("resizing").toBool()},
            {"pixelDensity", vc->pixelDensity()}, {"devicePixelRatio", m_app->devicePixelRatio()}};
        QTest::keyClick(m_app.get(), Qt::Key_Escape);
        QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, hit);
        QCOMPARE(frame->logicalGeometry(), frameRect);
        QCOMPARE(slider->logicalGeometry(), sliderRect);
        QCOMPARE(starts.count(), 0);
        const QString directory = qEnvironmentVariable("QLC_VC_VISUAL_DIR");
        if (!directory.isEmpty())
        {
            QVERIFY(QDir().mkpath(directory));
            const QString name = QString("chrome-%1-%2%3").arg(scale).arg(selected ? "selected" : "unselected")
                .arg(boundary == 1 ? "-nested-flush-left" : boundary == 2 ? "-nested-near-bottom" : nested ? "-nested" : "");
            QFile output(QDir(directory).filePath(name + ".json"));
            QVERIFY(output.open(QIODevice::WriteOnly));
            output.write(QJsonDocument(evidence).toJson());
            QVERIFY(beforePress.save(QDir(directory).filePath(name + ".png")));
        }
        qInfo().noquote() << "SELECTED_CHROME" << QJsonDocument(evidence).toJson(QJsonDocument::Compact);
        if (selected)
        {
            QVERIFY2(glyphRect.intersected(cornerRect).isEmpty(), "Selected frame handle covers descendant caption glyphs");
            QCOMPARE(selection, QJsonArray{int(slider->id())});
            QVERIFY(!evidence.value("frameResizingOnPress").toBool());
        }
    }
    if (boundary == 6)
    {
        vc->setSelectedWidgetIDs({frameID});
        QQuickItem *headerControl = nullptr;
        std::function<void(QQuickItem *)> locate = [&](QQuickItem *item) {
            if (outer->renderItem()->isAncestorOf(item) && !frame->renderItem()->isAncestorOf(item) &&
                item->property("tooltip").toString() == "Enable/Disable this frame")
                headerControl = item;
            for (QQuickItem *child : item->childItems()) locate(child);
        };
        locate(m_app->rootObject());
        QVERIFY(headerControl);
        const QRectF controlRect = headerControl->mapRectToScene(
            QRectF(QPointF(), QSizeF(headerControl->width(), headerControl->height())));
        const QRectF original = frame->renderItem()->mapRectToScene(
            QRectF(frame->renderItem()->width(), -frame->renderItem()->property("handleSize").toReal(),
                   frame->renderItem()->property("handleSize").toReal(), frame->renderItem()->property("handleSize").toReal()));
        QVERIFY(!original.intersected(controlRect).isEmpty());
        const QPoint point = original.intersected(controlRect).center().toPoint();
        const bool disabled = outer->property("isDisabled").toBool();
        QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, point);
        qInfo() << "ANCESTOR_CONTROL_PRESS" << scale << "point" << point << "controlRect" << controlRect
                << "selection" << vc->selectedWidgetIDs()
                << "controlPressed" << headerControl->property("pressed")
                << "parentMoving" << outer->renderItem()->property("moving")
                << "childResizing" << frame->renderItem()->property("resizing");
        QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, point);
        QCOMPARE(outer->property("isDisabled").toBool(), !disabled);
        QVERIFY(!frame->renderItem()->property("resizing").toBool());
        QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, point);
        QCOMPARE(outer->property("isDisabled").toBool(), disabled);
        QCOMPARE(starts.count(), 0);
    }
    const QRectF sliderHit = slider->renderItem()->mapRectToScene(
        QRectF(QPointF(), QSizeF(slider->renderItem()->width(), slider->renderItem()->height())));
    vc->setSnapping(false);
    for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
    {
        vc->setSelectedWidgetIDs({frameID});
        QSignalSpy selectedPaint(m_app.get(), &QQuickWindow::frameSwapped);
        m_app->update();
        QTRY_VERIFY(!selectedPaint.isEmpty());
        handles.clear();
        find(m_app->rootObject());
        QCOMPARE(handles.size(), 4);
        QQuickItem *handle = nullptr;
        for (QQuickItem *candidate : handles)
            if (candidate->property("index").toInt() == cornerIndex) handle = candidate;
        QVERIFY(handle);
        QTRY_VERIFY(handle->isVisible());
        const QRectF hitRect = handle->mapRectToScene(QRectF(QPointF(), QSizeF(handle->width(), handle->height())));
        const QRectF nominal = frame->renderItem()->mapRectToScene(QRectF(
            cornerIndex == 0 || cornerIndex == 3 ? -handle->width() : frame->renderItem()->width(),
            cornerIndex < 2 ? -handle->height() : frame->renderItem()->height(), handle->width(), handle->height()));
        bool ancestorControlConflict = false;
        if (boundary == 3)
        {
            auto *grand = qobject_cast<VCWidget *>(outer->parent());
            QVERIFY(grand && grand->renderItem());
            auto *header = qobject_cast<QQuickItem *>(grand->renderItem()->property("frameHeaderItem").value<QObject *>());
            QVERIFY(header);
            std::function<void(QQuickItem *)> inspect = [&](QQuickItem *item) {
                if (item->property("tooltip").toString() == "Expand/Collapse this frame")
                {
                    const QRectF control = item->mapRectToScene(QRectF(QPointF(), QSizeF(item->width(), item->height())));
                    if (!nominal.intersected(control).isEmpty())
                    {
                        ancestorControlConflict = true;
                        QVERIFY(hitRect.intersected(control).isEmpty());
                    }
                }
                for (QQuickItem *child : item->childItems()) inspect(child);
            };
            inspect(header);
        }
        if (boundary != 5 && boundary != 6 && !ancestorControlConflict)
            QCOMPARE(hitRect, nominal);
        QVERIFY(!hitRect.intersects(sliderHit));
        QList<QQuickItem *> fragments;
        const QString prefix = QString("vcResizeFragment-%1-%2-").arg(frameID).arg(handle->property("index").toInt());
        std::function<void(QQuickItem *)> collect = [&](QQuickItem *item) {
            if (item->objectName().startsWith(prefix) && item->isVisible() && item->width() > 0 && item->height() > 0)
                fragments.append(item);
            for (QQuickItem *child : item->childItems()) collect(child);
        };
        collect(m_app->rootObject());
        QVERIFY(!fragments.isEmpty());
        QList<QRectF> paintedRegions;
        qreal area = 0;
        QObject *sharedController = nullptr;
        for (QQuickItem *fragment : fragments)
        {
            const QRectF region = fragment->mapRectToScene(QRectF(QPointF(), QSizeF(fragment->width(), fragment->height())));
            QVERIFY(hitRect.adjusted(-0.001, -0.001, 0.001, 0.001).contains(region));
            for (const QRectF &previous : paintedRegions)
                QVERIFY(region.intersected(previous).width() * region.intersected(previous).height() < 0.001);
            for (QQuickItem *ancestor = fragment->parentItem(); ancestor; ancestor = ancestor->parentItem())
                if (ancestor->clip())
                    QVERIFY(ancestor->mapRectToScene(QRectF(QPointF(), QSizeF(ancestor->width(), ancestor->height())))
                        .adjusted(-0.001, -0.001, 0.001, 0.001).contains(region));
            QVERIFY(fragment->parent());
            QVERIFY(fragment->parent() != fragment->parentItem());
            QQmlExpression controller(qmlContext(fragment), fragment, "cornerOwner");
            QObject *owner = controller.evaluate().value<QObject *>();
            QVERIFY2(!controller.hasError() && owner, qPrintable(controller.error().toString()));
            QCOMPARE(owner->objectName(), QString("vcResizeCorner-%1-%2").arg(frameID).arg(handle->property("index").toInt()));
            if (!sharedController) sharedController = owner;
            QCOMPARE(owner, sharedController);
            QQmlExpression declaringFrame(qmlContext(fragment), fragment, "wRoot");
            QCOMPARE(declaringFrame.evaluate().value<QObject *>(), frame->renderItem());
            area += region.width() * region.height();
            paintedRegions.append(region);
        }
        QVERIFY(std::abs(area - hitRect.width() * hitRect.height()) < 0.001);
        QSignalSpy paint(m_app.get(), &QQuickWindow::frameSwapped);
        m_app->update();
        QTRY_VERIFY(!paint.isEmpty());
        const QImage cornerWindow = m_app->grabWindow();
        QQmlComponent reference(m_app->rootContext()->engine());
        reference.setData("import QtQuick\nItem { width: 33; height: 33; Image { objectName: \"referenceArrow\"; "
            "width: 31; height: 31; sourceSize: Qt.size(width, height); source: \"qrc:/arrow-corner.svg\" } }", QUrl());
        QVERIFY2(reference.isReady(), qPrintable(reference.errorString()));
        std::unique_ptr<QQuickItem> referenceItem(qobject_cast<QQuickItem *>(reference.create()));
        QVERIFY(referenceItem);
        referenceItem->setParentItem(m_app->contentItem());
        referenceItem->setPosition(QPointF(-100, -100));
        QQuickItem *referenceArrow = referenceItem->findChild<QQuickItem *>("referenceArrow");
        QVERIFY(referenceArrow);
        referenceItem->setSize(QSizeF(handle->width() + 2, handle->height() + 2));
        referenceArrow->setSize(QSizeF(handle->width(), handle->height()));
        referenceArrow->setRotation(handle->rotation());
        referenceArrow->setPosition(QPointF(hitRect.x() - qFloor(hitRect.x()), hitRect.y() - qFloor(hitRect.y())));
        auto referenceGrab = referenceItem->grabToImage();
        QVERIFY(referenceGrab);
        QTRY_VERIFY(!referenceGrab->image().isNull());
        const QImage expectedArrow = referenceGrab->image();
        int opaquePixels = 0, mismatchedPixels = 0;
        for (int y = 1; y < expectedArrow.height() - 1; ++y)
            for (int x = 1; x < expectedArrow.width() - 1; ++x)
            {
                const QColor expected = expectedArrow.pixelColor(x, y);
                if (expected.alpha() < 250 || expectedArrow.pixelColor(x - 1, y).alpha() < 250 ||
                    expectedArrow.pixelColor(x + 1, y).alpha() < 250 ||
                    expectedArrow.pixelColor(x, y - 1).alpha() < 250 ||
                    expectedArrow.pixelColor(x, y + 1).alpha() < 250)
                    continue;
                const QPoint point(qFloor(hitRect.x()) + x, qFloor(hitRect.y()) + y);
                QVERIFY(cornerWindow.rect().contains(point));
                const QColor actual = cornerWindow.pixelColor(point);
                ++opaquePixels;
                if (std::max({std::abs(expected.red() - actual.red()), std::abs(expected.green() - actual.green()),
                              std::abs(expected.blue() - actual.blue())}) > 40)
                    ++mismatchedPixels;
            }
        qInfo() << "CHROME_FRAGMENT_PAINT" << "scale" << scale << "boundary" << boundary
                << "index" << handle->property("index") << "regions" << paintedRegions
                << "opaquePixels" << opaquePixels << "mismatchedPixels" << mismatchedPixels;
        QVERIFY(opaquePixels > 100);
        QCOMPARE(mismatchedPixels, 0);
        const QPoint press = hitRect.center().toPoint();
        QVERIFY(QRect(QPoint(), m_app->size()).contains(press));
        QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        qInfo() << "CHROME_HANDLE_PRESS" << "scale" << scale << "boundary" << boundary
                << "index" << handle->property("index") << "point" << press
                << "sceneRect" << hitRect << "visualParent"
                << (handle->parentItem() ? handle->parentItem()->objectName() : QString())
                << "childResizing" << frame->renderItem()->property("resizing")
                << "parentResizing" << (outer ? outer->renderItem()->property("resizing") : QVariant());
        if (!qEnvironmentVariable("QLC_VC_VISUAL_DIR").isEmpty())
        {
            const QString name = QString("handle-%1%2-boundary-%3-%4").arg(scale).arg(nested ? "-nested" : "").arg(boundary)
                .arg(handle->property("index").toInt());
            const QString directory = qEnvironmentVariable("QLC_VC_VISUAL_DIR");
            QVERIFY(cornerWindow.save(QDir(directory).filePath(name + ".png")));
            QFile output(QDir(directory).filePath(name + ".json"));
            QVERIFY(output.open(QIODevice::WriteOnly));
            output.write(QJsonDocument(QJsonObject{{"scale", scale}, {"nested", nested}, {"boundary", boundary},
                {"index", handle->property("index").toInt()}, {"sceneRect", rectValue(hitRect)},
                {"visualParent", handle->parentItem() ? handle->parentItem()->objectName() : QString()},
                {"ancestorSceneRect", outer ? rectValue(outer->renderItem()->mapRectToScene(
                    QRectF(QPointF(), QSizeF(outer->renderItem()->width(), outer->renderItem()->height())))) : QJsonObject()},
                {"childResizing", frame->renderItem()->property("resizing").toBool()},
                {"parentResizing", outer && outer->renderItem()->property("resizing").toBool()},
                {"selection", QJsonArray::fromVariantList(vc->selectedWidgetIDs())}}).toJson());
        }
        QVERIFY(frame->renderItem()->property("resizing").toBool());
        if (outer) QVERIFY(!outer->renderItem()->property("resizing").toBool());
        QVERIFY(!slider->renderItem()->property("resizing").toBool());
        QTest::keyClick(m_app.get(), Qt::Key_Escape);
        QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        QVERIFY(!frame->renderItem()->property("resizing").toBool());
        for (QQuickItem *fragment : fragments)
        {
            const QRectF region = fragment->mapRectToScene(QRectF(QPointF(), QSizeF(fragment->width(), fragment->height())));
            const QPointF point = region.center();
            const QPointF global = QPointF(m_app->mapToGlobal(point.toPoint())) + point - QPointF(point.toPoint());
            const QRectF before = frame->logicalGeometry();
            QSignalSpy geometry(frame, &VCWidget::geometryChanged);
            QMouseEvent down(QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QVERIFY(QCoreApplication::sendEvent(m_app.get(), &down));
            QVERIFY(frame->renderItem()->property("resizing").toBool());
            if (outer) QVERIFY(!outer->renderItem()->property("resizing").toBool());
            QTest::keyClick(m_app.get(), Qt::Key_Escape);
            QMouseEvent up(QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QVERIFY(QCoreApplication::sendEvent(m_app.get(), &up));
            QVERIFY(!frame->renderItem()->property("resizing").toBool());
            QCOMPARE(frame->logicalGeometry(), before);
            QCOMPARE(geometry.count(), 0);
        }
        const QRectF before = frame->logicalGeometry(), sliderBefore = slider->logicalGeometry();
        QSignalSpy changed(frame, &VCWidget::geometryChanged);
        const bool left = cornerIndex == 0 || cornerIndex == 3, top = cornerIndex < 2;
        const QPoint delta(left ? 4 : -4, top ? 4 : -4);
        QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        QVERIFY(frame->renderItem()->property("resizing").toBool());
        QTest::mouseMove(m_app.get(), press + delta, 20);
        QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, press + delta);
        QCOMPARE(frame->logicalGeometry(), QRectF(before.x() + (left ? 4 / scale : 0),
            before.y() + (top ? 4 / scale : 0), before.width() - 4 / scale, before.height() - 4 / scale));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(slider->logicalGeometry(), sliderBefore);
        QVERIFY(!frame->renderItem()->property("resizing").toBool());
        QVERIFY2(vc->editGeometry(frameID, before).value("accepted").toBool(), "Restore admitted fixture geometry after real corner resize");
    }
    handles.clear();
    find(m_app->rootObject());
    QCOMPARE(handles.size(), 4);
    QPointer<QQuickItem> growHandle;
    for (QQuickItem *handle : handles)
        if (handle->property("index").toInt() == 2) growHandle = handle;
    QVERIFY(growHandle);
    vc->setSnapping(false);
    vc->setSelectedWidgetIDs({frameID});
    const QRectF frameBefore = frame->logicalGeometry(), sliderBefore = slider->logicalGeometry();
    const QVariantList order = qobject_cast<VCFrame *>(frame)->orderedChildIDs(0);
    const QPoint press = growHandle->mapToScene(QPointF(growHandle->width() / 2, growHandle->height() / 2)).toPoint();
    QSignalSpy commits(frame, &VCWidget::geometryChanged);
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
    QVERIFY(frame->renderItem()->property("resizing").toBool());
    if (outer) QVERIFY(!outer->renderItem()->property("resizing").toBool());
    QTest::mouseMove(m_app.get(), press + QPoint(24, 24), 20);
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, press + QPoint(24, 24));
    const QRectF contentBounds = qobject_cast<VCFrame *>(frame->parent())->contentRect();
    const QSizeF expectedSize(qMin(frameBefore.width() + 24 / scale, contentBounds.right() - frameBefore.left()),
                              qMin(frameBefore.height() + 24 / scale, contentBounds.bottom() - frameBefore.top()));
    QCOMPARE(frame->logicalGeometry(), QRectF(frameBefore.topLeft(), expectedSize));
    QCOMPARE(commits.count(), 1);
    QCOMPARE(slider->logicalGeometry(), sliderBefore);
    QCOMPARE(slider->parent(), frame);
    QCOMPARE(qobject_cast<VCFrame *>(frame)->orderedChildIDs(0), order);
    QCOMPARE(starts.count(), 0);
    const QRectF grown = frame->logicalGeometry();
    QPointF neighbourPosition(grown.right(), grown.bottom());
    quint32 neighbourParent = parentID;
    if (boundary == 4)
    {
        neighbourPosition += outer->logicalGeometry().topLeft();
        neighbourParent = qobject_cast<VCWidget *>(outer->parent())->id();
    }
    else if (boundary == 6)
    {
        neighbourPosition += outer->logicalGeometry().topLeft();
        neighbourPosition.setX(qMax(neighbourPosition.x(), outer->logicalGeometry().right()));
        neighbourParent = vc->page(0)->id();
    }
    else if (boundary == 2)
    {
        for (QObject *owner = frame->parent(); owner && owner != vc->page(0); owner = owner->parent())
            if (auto *ancestor = qobject_cast<VCWidget *>(owner))
                neighbourPosition += ancestor->logicalGeometry().topLeft();
        neighbourParent = vc->page(0)->id();
    }
    const QRectF neighbourGeometry(neighbourPosition, QSizeF(100, boundary == 7 || boundary == 8 ? 100 : 60));
    const QVariantMap neighbourControl = vc->createWidgetGeometry(neighbourParent, "Button", neighbourGeometry);
    QVERIFY2(neighbourControl.value("accepted").toBool(), qPrintable(neighbourControl.value("error").toString()));
    if (neighbourParent == vc->page(0)->id())
        roots.append(neighbourControl.value("widgetIDs").toList().first());
    VCWidget *neighbour = vc->widget(neighbourControl.value("widgetIDs").toList().first().toUInt());
    neighbour->setCaption("Neighbour");
    QTRY_VERIFY(neighbour->renderItem());
    vc->setSelectedWidgetIDs({frameID});
    handles.clear();
    find(m_app->rootObject());
    QCOMPARE(handles.size(), 4);
    for (QQuickItem *handle : handles)
        if (handle->property("index").toInt() == 2) growHandle = handle;
    QVERIFY(growHandle);
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    const QRectF handleRect = growHandle->mapRectToScene(
        QRectF(QPointF(), QSizeF(growHandle->width(), growHandle->height())));
    const QRectF originalHandleRect = frame->renderItem()->mapRectToScene(
        QRectF(frame->renderItem()->width(), frame->renderItem()->height(), growHandle->width(), growHandle->height()));
    const QRectF neighbourRect = neighbour->renderItem()->mapRectToScene(
        QRectF(QPointF(), QSizeF(neighbour->renderItem()->width(), neighbour->renderItem()->height())));
    QVERIFY(handleRect.intersected(neighbourRect).isEmpty());
    const QRectF overlap = originalHandleRect.intersected(neighbourRect);
    QVERIFY(!overlap.isEmpty());
    if (boundary == 8)
    {
        qreal barHeight = 0;
        for (QQuickItem *control : viewport->childItems())
            if (control->property("orientation").toInt() == Qt::Horizontal && control->property("pressed").isValid() &&
                control->property("position").isValid())
                barHeight = control->height();
        QVERIFY(barHeight > 0);
        m_app->resize(m_app->width(), qCeil(originalHandleRect.bottom() + barHeight + 3));
        QSignalSpy resized(m_app.get(), &QQuickWindow::frameSwapped);
        m_app->update();
        QTRY_VERIFY(!resized.isEmpty());
        auto currentCorner = [&]() -> QQuickItem * {
            handles.clear();
            find(m_app->rootObject());
            for (QQuickItem *item : handles)
                if (item->property("index").toInt() == 2) return item;
            return nullptr;
        };
        QQuickItem *current = currentCorner();
        QVERIFY(current);
        auto sceneRect = [](QQuickItem *item) {
            return item->mapRectToScene(QRectF(QPointF(), QSizeF(item->width(), item->height())));
        };
        const QRectF original = frame->renderItem()->mapRectToScene(
            QRectF(frame->renderItem()->width(), frame->renderItem()->height(), current->width(), current->height()));
        const QRectF before = sceneRect(current);
        QVERIFY(std::abs(before.x() - original.x() - neighbourGeometry.width() * scale - 2) < 0.001);
        QVERIFY(std::abs(before.y() - original.y()) < 0.001);
        QByteArray beforeXML;
        QXmlStreamWriter beforeWriter(&beforeXML);
        QVERIFY(vc->saveXML(&beforeWriter));
        const QVariantList selection = vc->selectedWidgetIDs();
        const bool dirty = m_app->doc()->isModified();
        const qreal oldScroll = viewport->property("contentY").toReal();
        const qreal scroll = oldScroll + neighbourGeometry.height() * scale + current->height() + 4;
        QVERIFY(scroll <= viewport->property("contentHeight").toReal() - viewport->height() +
                          viewport->property("bottomMargin").toReal());
        QVERIFY(viewport->setProperty("contentY", scroll));
        QSignalSpy scrolled(m_app.get(), &QQuickWindow::frameSwapped);
        m_app->update();
        QTRY_VERIFY(!scrolled.isEmpty());
        QCOMPARE(viewport->property("contentY").toReal(), scroll);
        current = currentCorner();
        QVERIFY(current);
        const QRectF after = sceneRect(current);
        const QRectF movedOriginal = frame->renderItem()->mapRectToScene(
            QRectF(frame->renderItem()->width(), frame->renderItem()->height(), current->width(), current->height()));
        const QRectF expected(movedOriginal.topLeft() + QPointF(0, neighbourGeometry.height() * scale + 2),
                              movedOriginal.size());
        const QRectF view = sceneRect(viewport);
        QVERIFY(view.contains(expected));
        qInfo() << "SCROLLED_CORNER_PREFERENCE" << scale << "contentY" << oldScroll << scroll
                << "before" << before << "after" << after << "expected" << expected << "viewport" << view;
        QCOMPARE(after, expected);
        const QRectF localBefore = current->mapRectToItem(frame->renderItem(),
            QRectF(QPointF(), QSizeF(current->width(), current->height())));
        const QPoint held = after.center().toPoint();
        QSignalSpy changes(frame, &VCWidget::geometryChanged);
        QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, held);
        QVERIFY(frame->renderItem()->property("resizing").toBool());
        QVERIFY(viewport->setProperty("contentY", oldScroll));
        QSignalSpy heldFrame(m_app.get(), &QQuickWindow::frameSwapped);
        m_app->update();
        QTRY_VERIFY(!heldFrame.isEmpty());
        current = currentCorner();
        QVERIFY(current);
        QCOMPARE(current->mapRectToItem(frame->renderItem(),
            QRectF(QPointF(), QSizeF(current->width(), current->height()))), localBefore);
        QTest::keyClick(m_app.get(), Qt::Key_Escape);
        QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, held);
        QVERIFY(!frame->renderItem()->property("resizing").toBool());
        QCOMPARE(changes.count(), 0);
        QByteArray afterXML;
        QXmlStreamWriter afterWriter(&afterXML);
        QVERIFY(vc->saveXML(&afterWriter));
        QCOMPARE(afterXML, beforeXML);
        QCOMPARE(vc->selectedWidgetIDs(), selection);
        QCOMPARE(m_app->doc()->isModified(), dirty);
        QCOMPARE(starts.count(), 0);
        return;
    }
    if (boundary == 7)
    {
        QVERIFY(std::abs(handleRect.x() - originalHandleRect.x()) < 0.001);
        QVERIFY(std::abs(handleRect.y() - originalHandleRect.y() - neighbourGeometry.height() * scale - 2) < 0.001);
        for (int repeat = 0; repeat < 3; ++repeat)
        {
            vc->setSelectedWidgetIDs({});
            vc->setSelectedWidgetIDs({frameID});
            QSignalSpy repeated(m_app.get(), &QQuickWindow::frameSwapped);
            m_app->update();
            QTRY_VERIFY(!repeated.isEmpty());
            handles.clear();
            find(m_app->rootObject());
            for (QQuickItem *handle : handles)
                if (handle->property("index").toInt() == 2) growHandle = handle;
            QVERIFY(growHandle);
            QCOMPARE(growHandle->mapRectToScene(QRectF(QPointF(), QSizeF(growHandle->width(), growHandle->height()))), handleRect);
        }
    }
    const QPoint heldPress = handleRect.center().toPoint();
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, heldPress);
    QVERIFY(frame->renderItem()->property("resizing").toBool());
    const QRectF space = qobject_cast<VCFrame *>(neighbour->parent())->contentRect();
    const QRectF far(QPointF(space.right() - neighbourGeometry.width(), space.bottom() - neighbourGeometry.height() - 20),
                     neighbourGeometry.size());
    QVERIFY2(vc->editGeometry(neighbour->id(), far).value("accepted").toBool(), "Move fixture obstacle through public authoring while the real corner is grabbed");
    QSignalSpy heldPaint(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!heldPaint.isEmpty());
    QVERIFY(growHandle);
    QCOMPARE(growHandle->mapRectToScene(QRectF(QPointF(), QSizeF(growHandle->width(), growHandle->height()))), handleRect);
    QCOMPARE(frame->logicalGeometry(), grown);
    QTest::keyClick(m_app.get(), Qt::Key_Escape);
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, heldPress);
    QVERIFY(!frame->renderItem()->property("resizing").toBool());
    handles.clear();
    find(m_app->rootObject());
    for (QQuickItem *handle : handles)
        if (handle->property("index").toInt() == 2) growHandle = handle;
    QVERIFY(growHandle);
    QRectF clearedOriginal = originalHandleRect;
    for (QQuickItem *control : viewport->childItems())
        if (control->isVisible() && control->property("orientation").toInt() == Qt::Horizontal &&
            control->property("pressed").isValid() && control->property("position").isValid())
        {
            const QRectF bar = control->mapRectToScene(QRectF(QPointF(), control->size()));
            if (!clearedOriginal.intersected(bar).isEmpty())
                clearedOriginal.moveBottom(bar.top() - 2);
        }
    QCOMPARE(growHandle->mapRectToScene(QRectF(QPointF(), QSizeF(growHandle->width(), growHandle->height()))), clearedOriginal);
    QVERIFY(vc->editGeometry(neighbour->id(), neighbourGeometry).value("accepted").toBool());
    const QPoint neighbourPress = overlap.center().toPoint();
    QVERIFY(QRect(QPoint(), m_app->size()).contains(neighbourPress));
    QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, neighbourPress);
    QCOMPARE(vc->selectedWidgetIDs(), QVariantList{neighbour->id()});
    QVERIFY(!frame->renderItem()->property("resizing").toBool());
    QCOMPARE(frame->logicalGeometry(), grown);
    QCOMPARE(neighbour->logicalGeometry(), neighbourGeometry);
    QCOMPARE(starts.count(), 0);
    qInfo().noquote() << "CHROME_NEIGHBOUR" << QJsonDocument(QJsonObject{{"scale", scale},
        {"overlapRect", rectValue(overlap)}, {"childReceivesClick", true}, {"frameGeometryUnchanged", true}})
        .toJson(QJsonDocument::Compact);
}

void UpstreamIntegration_Test::vcDerivedWorkspacePersistence_data()
{
    QTest::addColumn<qreal>("scale");
    QTest::addColumn<bool>("integerGeometry");
    QTest::addColumn<QString>("inventoryCase");
    for (qreal scale : {qreal(0.8), qreal(1.1), qreal(2)})
        for (bool integers : {false, true})
            QTest::newRow(qPrintable(QString("%1-%2").arg(scale).arg(integers ? "integers" : "fractional")))
                << scale << integers << QString("current");
    for (const QString &inventoryCase : {QString("reused-projection"), QString("missing"),
                                         QString("duplicate"), QString("wrong-location"), QString("unexpected"),
                                         QString("stale-caption"), QString("wrong-type"),
                                         QString("unavailable-current"), QString("mismatched-current")})
        QTest::newRow(qPrintable(inventoryCase)) << qreal(0.8) << false << inventoryCase;
}

void UpstreamIntegration_Test::vcDerivedWorkspacePersistence()
{
    QFETCH(qreal, scale);
    QFETCH(bool, integerGeometry);
    QFETCH(QString, inventoryCase);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString original = files.filePath("original.qxw");
    const QString workspace = files.filePath("all-types.qxw");
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
    auto *model = vc->outlineModel();
    QSignalSpy treeChanges(model, &VCOutlineModel::treeChanged);
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(4);
    frame->setLogicalGeometry(integerGeometry ? QRectF(10, 20, 1400, 800)
                                              : QRectF(10.25, 20.5, 1400.75, 800.25));
    const quint32 frameID = frame->id();
    const QStringList types{"Button", "Slider", "XYPad", "Frame", "Solo frame", "Speed",
                            "Cue list", "Label", "Audio Triggers", "Animation", "Clock", "Record Panel"};
    QVariantList ids;
    for (int index = 0; index < types.size(); ++index)
    {
        const QRectF rect = integerGeometry ? QRectF(20 + (index % 4) * 330, 60, 300, 300)
                                            : QRectF(20.25 + (index % 4) * 330, 60.5, 300.75, 300.25);
        const QVariantMap result = vc->createWidgetGeometry(frameID, types[index], rect, index / 4);
        QVERIFY2(result.value("accepted").toBool(), qPrintable(result.value("error").toString()));
        const QVariant id = result.value("widgetIDs").toList().first();
        ids.append(id);
        vc->widget(id.toUInt())->setCaption(index == 7 ? QString() : QString("Duplicate"));
    }
    auto *scene = new Scene(m_app->doc());
    scene->setName("Persistence binding");
    QVERIFY(m_app->doc()->addFunction(scene));
    const quint32 functionID = scene->id();
    auto *button = qobject_cast<VCButton *>(vc->widget(ids.first().toUInt()));
    QVERIFY(button);
    button->setFunctionID(functionID);
    vc->page(0)->setPageScale(scale - vc->page(0)->pageScale());
    QCOMPARE(vc->page(0)->pageScale(), scale);
    QCOMPARE(button->scaleFactor(), scale);
    QVERIFY(vc->editStructure({ids[3]}, frameID, 0, 0).value("accepted").toBool());
    frame->setCurrentPage(1);
    frame->setCollapsed(true);
    auto snapshot = [&]() {
        QVariantList values;
        QVariantList allIDs{frameID};
        allIDs.append(ids);
        for (const QVariant &id : allIDs)
        {
            VCWidget *widget = vc->widget(id.toUInt());
            if (!widget) return QVariantList{};
            QVariantMap value{{"id", widget->id()}, {"type", widget->type()}, {"caption", widget->caption()},
                {"parentID", qobject_cast<VCWidget *>(widget->parent())->id()},
                {"subpage", widget->page()}, {"rect", widget->logicalGeometry()}};
            if (auto *container = qobject_cast<VCFrame *>(widget))
            {
                QVariantList orders;
                for (int subpage = 0; subpage < container->totalPagesNumber(); ++subpage)
                    orders.append(QVariant(container->orderedChildIDs(subpage)));
                value["orders"] = orders;
                value["pages"] = container->totalPagesNumber();
                value["currentPage"] = container->currentPage();
                value["collapsed"] = container->isCollapsed();
            }
            if (auto *control = qobject_cast<VCButton *>(widget))
                value["functionID"] = control->functionID();
            values.append(value);
        }
        return values;
    };
    const QVariantList before = snapshot();
    QCOMPARE(before.size(), types.size() + 1);
    const QString oldFilter = model->filterText();
    model->setFilterText({});
    const auto restoreFilter = qScopeGuard([&]() { model->setFilterText(oldFilter); });
    const QString lastKey = QString("widget:%1").arg(ids.last().toUInt());
    if (inventoryCase == "reused-projection")
    {
        QTRY_COMPARE(model->rowData(lastKey).value("label").toString(), QString("Duplicate"));
        QVERIFY(m_app->saveWorkspace(workspace));
        QPointer<VCWidget> oldFrame(frame);
        treeChanges.clear();
        QVERIFY(m_app->loadWorkspace(workspace));
        m_app->doc()->masterTimer()->stop();
        QVERIFY(oldFrame.isNull());
        frame = qobject_cast<VCFrame *>(vc->widget(frameID));
        QVERIFY(frame);
        QCOMPARE(snapshot(), before);
        QVERIFY(model->indexForKey(lastKey).isValid());
    }
    QMap<quint32, QVariantMap> expectedRows;
    QMap<quint32, QPointer<VCWidget>> expectedObjects;
    for (int page = 0; page < vc->pagesCount(); ++page)
        for (VCWidget *widget : vc->page(page)->children(true))
        {
            auto *owner = qobject_cast<VCFrame *>(widget->parent());
            QVERIFY(owner);
            expectedObjects.insert(widget->id(), widget);
            expectedRows.insert(widget->id(), {
                {"type", int(widget->type())},
                {"caption", widget->caption().isEmpty() ? QString("<Unnamed>") : widget->caption()},
                {"parentID", owner->id()}, {"page", page}, {"subpage", widget->page()},
                {"parentKey", owner->multiPageMode() ?
                    QString("subpage:%1:%2").arg(owner->id()).arg(widget->page()) :
                    QString("widget:%1").arg(owner->id())},
                {"parentKind", owner->multiPageMode() ? VCOutlineModel::SubpageRow :
                    qobject_cast<VCPage *>(owner) ? VCOutlineModel::PageRow : VCOutlineModel::WidgetRow}});
        }
    auto observeInventory = [&]() {
        QVariantList rows;
        std::function<void(const QModelIndex &)> visit = [&](const QModelIndex &parent) {
            for (int row = 0; row < model->rowCount(parent); ++row)
            {
                const QModelIndex index = model->index(row, 0, parent);
                if (index.data(VCOutlineModel::RowKindRole).toInt() == VCOutlineModel::WidgetRow)
                    rows.append(QVariantMap{
                        {"id", index.data(VCOutlineModel::WidgetIDRole)},
                        {"type", index.data(VCOutlineModel::WidgetTypeRole)},
                        {"caption", index.data(VCOutlineModel::LabelRole)},
                        {"parentID", index.data(VCOutlineModel::ParentIDRole)},
                        {"page", index.data(VCOutlineModel::PageIndexRole)},
                        {"subpage", index.data(VCOutlineModel::SubpageIndexRole)},
                        {"parentKey", index.data(VCOutlineModel::ParentKeyRole)},
                        {"parentKind", index.parent().data(VCOutlineModel::RowKindRole)}});
                visit(index);
            }
        };
        visit({});
        return rows;
    };
    auto inventoryError = [&](const QVariantList &rows) -> QString {
        QMap<quint32, QList<QVariantMap>> actual;
        for (const QVariant &row : rows)
            actual[row.toMap().value("id").toUInt()].append(row.toMap());
        for (auto it = expectedRows.cbegin(); it != expectedRows.cend(); ++it)
        {
            const quint32 id = it.key();
            const QVariantMap &expected = it.value();
            const QString identity = QString("widget:%1 (%2 '%3')").arg(id)
                .arg(VCWidget::typeToString(expected.value("type").toInt()), expected.value("caption").toString());
            const int count = actual.value(id).size();
            if (count != 1)
                return QString("%1 %2 at %3 page %4 subpage %5: expected count 1, actual %6")
                    .arg(count == 0 ? "missing" : "duplicate", identity, expected.value("parentKey").toString())
                    .arg(expected.value("page").toInt()).arg(expected.value("subpage").toInt()).arg(count);
            if (!expectedObjects.value(id) || vc->widget(id) != expectedObjects.value(id))
                return QString("unavailable current %1").arg(identity);
            const QVariantMap observed = actual.value(id).first();
            for (const QString &field : {QString("parentID"), QString("page"), QString("subpage"),
                                         QString("parentKey"), QString("parentKind"), QString("type"), QString("caption")})
                if (observed.value(field) != expected.value(field))
                    return QString("%1 %2 field %3: expected %4, actual %5")
                        .arg(field == "type" ? "wrong-type" : field == "caption" ? "stale-caption" : "wrong-location",
                             identity, field, expected.value(field).toString(), observed.value(field).toString());
        }
        for (auto it = actual.cbegin(); it != actual.cend(); ++it)
            if (!expectedRows.contains(it.key()))
                return QString("unexpected widget:%1: actual count %2").arg(it.key()).arg(it.value().size());
        return {};
    };
    if (inventoryCase == "reused-projection")
    {
        QVERIFY(treeChanges.isEmpty());
        const QString error = inventoryError(observeInventory());
        QVERIFY2(!error.isEmpty(), "The replaced projection must be refused before queued publication");
        qInfo().noquote() << "INVENTORY_STALE_PROJECTION_PROOF" << error;
    }
    QTRY_VERIFY2(!treeChanges.isEmpty() && inventoryError(observeInventory()).isEmpty(),
        qPrintable(treeChanges.isEmpty() ? QString("Current VC outline projection was not published") :
                                         inventoryError(observeInventory())));
    if (inventoryCase != "current" && inventoryCase != "reused-projection")
    {
        QVariantList broken = observeInventory();
        quint32 id = ids.first().toUInt();
        const auto liveObjects = expectedObjects;
        QString errorPrefix = inventoryCase;
        int row = 0;
        while (row < broken.size() && broken[row].toMap().value("id").toUInt() != id) ++row;
        QVERIFY(row < broken.size());
        if (inventoryCase == "missing")
            broken.removeAt(row);
        else if (inventoryCase == "duplicate")
            broken.append(broken[row]);
        else if (inventoryCase == "unexpected")
        {
            QVariantMap unexpected = broken[row].toMap();
            id = expectedRows.lastKey() + 1;
            QVERIFY(!expectedRows.contains(id));
            unexpected["id"] = id;
            broken.append(unexpected);
        }
        else if (inventoryCase == "unavailable-current" || inventoryCase == "mismatched-current")
        {
            expectedObjects[id] = inventoryCase == "unavailable-current" ? QPointer<VCWidget>() :
                expectedObjects.value(ids[1].toUInt());
            errorPrefix = "unavailable current";
        }
        else
        {
            QVariantMap misplaced = broken[row].toMap();
            if (inventoryCase == "stale-caption")
                misplaced["caption"] = "Stale Duplicate";
            else if (inventoryCase == "wrong-type")
                misplaced["type"] = misplaced.value("type").toInt() + 1;
            else
                misplaced["parentID"] = vc->page(0)->id();
            broken[row] = misplaced;
        }
        const QString error = inventoryError(broken);
        expectedObjects = liveObjects;
        QVERIFY2(error.startsWith(errorPrefix + " "), qPrintable(error));
        QVERIFY2(error.contains(QString("widget:%1%2").arg(id).arg(inventoryCase == "unexpected" ? ":" : " ")),
                 qPrintable(error));
        qInfo().noquote() << "INVENTORY_ERROR_PROOF" << error;
    }
    QHash<quint32, int> inventory;
    std::function<void(const QModelIndex &)> visit = [&](const QModelIndex &parent) {
        for (int row = 0; row < model->rowCount(parent); ++row)
        {
            const QModelIndex index = model->index(row, 0, parent);
            if (index.data(VCOutlineModel::RowKindRole).toInt() == VCOutlineModel::WidgetRow)
                ++inventory[index.data(VCOutlineModel::WidgetIDRole).toUInt()];
            visit(index);
        }
    };
    visit({});
    QSet<quint32> expectedInventory;
    for (int page = 0; page < vc->pagesCount(); ++page)
        for (VCWidget *widget : vc->page(page)->children(true))
        {
            expectedInventory.insert(widget->id());
            QCOMPARE(inventory.value(widget->id()), 1);
        }
    QCOMPARE(QSet<quint32>(inventory.keyBegin(), inventory.keyEnd()), expectedInventory);
    for (int index = 0; index < ids.size(); ++index)
    {
        const quint32 id = ids[index].toUInt();
        const QModelIndex row = model->indexForKey(QString("widget:%1").arg(id));
        QVERIFY(row.isValid());
        QCOMPARE(row.data(VCOutlineModel::ParentIDRole).toUInt(), frameID);
        QCOMPARE(row.data(VCOutlineModel::SubpageIndexRole).toInt(), index / 4);
        const QModelIndex subpage = row.parent();
        QCOMPARE(subpage.data(VCOutlineModel::RowKindRole).toInt(), int(VCOutlineModel::SubpageRow));
        QCOMPARE(subpage.parent().data(VCOutlineModel::WidgetIDRole).toUInt(), frameID);
        QCOMPARE(model->rowData(QString("widget:%1").arg(id)).value("widgetType").toInt(),
                 int(VCWidget::stringToType(types[index])));
    }
    QCOMPARE(model->filteredModel()->rowCount(model->indexForKey(QString("widget:%1").arg(frameID))), 4);
    QVERIFY(m_app->saveWorkspace(workspace));
    QVERIFY(!m_app->doc()->isModified());
    QPointer<VCWidget> replaced(frame);
    QVERIFY(m_app->loadWorkspace(workspace));
    m_app->doc()->masterTimer()->stop();
    QVERIFY(replaced.isNull());
    QCOMPARE(snapshot(), before);
    QVERIFY(!m_app->doc()->isModified());
    m_app->doc()->setModified();
    const QVariantList beforeFailure = snapshot();
    QVERIFY(!m_app->saveWorkspace(files.filePath("missing-parent/failure.qxw")));
    QCOMPARE(snapshot(), beforeFailure);
    QVERIFY(m_app->doc()->isModified());
    QCOMPARE(m_app->runningFunctionsCount(), 0);
}

void UpstreamIntegration_Test::vcPublicationLifetime_data()
{
    QTest::addColumn<QString>("publication");
    QTest::addColumn<QString>("operation");
    QTest::newRow("geometry-reentry") << QString("geometry") << QString("reentry");
    QTest::newRow("structure-reentry") << QString("structure") << QString("reentry");
    QTest::newRow("delete-load") << QString("delete") << QString("load");
    for (const QString &publication : {QString("geometry"), QString("structure"), QString("structure-before")})
        for (const QString &operation : {QString("load"), QString("missing-load"), QString("new"), QString("clear"),
             QString("memory-load"), QString("network-clear"), QString("vc-reset"), QString("file-xml"),
             QString("stream-xml"), QString("vc-xml")})
            QTest::newRow(qPrintable(publication + "-" + operation)) << publication << operation;
}

void UpstreamIntegration_Test::vcPublicationLifetime()
{
    QFETCH(QString, publication);
    QFETCH(QString, operation);
    auto *view2D = m_app->rootObject()->findChild<QQuickItem *>("twoDView");
    QVERIFY(view2D);
    QQmlExpression popupExpression(qmlContext(view2D), view2D, "monitorPOVPopup");
    QPointer<QObject> monitorPopup = popupExpression.evaluate().value<QObject *>();
    QVERIFY2(monitorPopup, qPrintable(popupExpression.error().toString()));
    const bool wasPopupVisible = monitorPopup->property("visible").toBool();
    const auto oldPointOfView = m_app->doc()->monitorProperties()->pointOfView();
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString original = files.filePath("original.qxw");
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
    VirtualConsole *vc = m_app->virtualConsole();
    QFile saved(original);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QByteArray originalXML = saved.readAll();
    auto *scene = new Scene(m_app->doc());
    scene->setName("Lifetime binding");
    QVERIFY(m_app->doc()->addFunction(scene));
    const quint32 functionID = scene->id();
    QPointer<Function> retainedFunction(scene);
    auto *frame = new VCFrame(m_app->doc(), vc, vc->page(0));
    vc->addWidgetToMap(frame);
    vc->page(0)->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 700, 400));
    QVariantList ids;
    for (int index = 0; index < 2; ++index)
    {
        const QVariantMap result = vc->createWidgetGeometry(frame->id(), "Button",
                                                           QRectF(20 + index * 200, 60, 100, 80));
        QVERIFY(result.value("accepted").toBool());
        ids.append(result.value("widgetIDs").toList().first());
    }
    const quint32 frameID = frame->id(), firstID = ids[0].toUInt(), secondID = ids[1].toUInt();
    QPointer<VCWidget> first(vc->widget(firstID)), second(vc->widget(secondID));
    qobject_cast<VCButton *>(first.data())->setFunctionID(functionID);
    vc->setEditMode(true);
    vc->setSelectedWidgetIDs(ids);
    m_app->doc()->resetModified();
    QSignalSpy errors(vc, &VirtualConsole::editRejected);
    QSignalSpy loadedChanges(m_app.get(), &App::docLoadedChanged);
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    QSignalSpy contextChanges(contexts, &ContextManager::currentContextChanged);
    QVariantMap reentry;
    bool observed = false, coherent = false, unchanged = false, loaded = true;
    const bool structure = publication.startsWith("structure");
    const bool deleting = publication == "delete";
    const bool replacement = operation != "reentry";
    auto observer = [&]() {
        if (observed) return;
        observed = true;
        coherent = first && second && (deleting ? !vc->widget(firstID) && vc->widget(secondID) == second
            : structure ? frame->orderedChildIDs(0) ==
            (publication.startsWith("structure-before") ? QVariantList{firstID, secondID} : QVariantList{secondID, firstID})
            : first->logicalGeometry() == QRectF(40, 100, 100, 80) &&
              second->logicalGeometry() == QRectF(240, 100, 100, 80));
        if (replacement)
        {
            QByteArray before;
            QXmlStreamWriter writer(&before);
            vc->saveXML(&writer);
            const QVariantList selection = vc->selectedWidgetIDs();
            const bool dirty = m_app->doc()->isModified();
            const QString fileName = m_app->fileName();
            const QString workspacePath = m_app->doc()->workspacePath();
            const QString context = contexts->currentContext();
            qInfo() << "PUBLICATION_REPLACEMENT" << publication << operation << "coherent" << coherent;
            if (operation == "load") loaded = m_app->loadWorkspace(original);
            else if (operation == "missing-load") loaded = m_app->loadWorkspace(files.filePath("missing.qxw"));
            else if (operation == "new") loaded = m_app->newWorkspace();
            else if (operation == "clear") m_app->clearDocument();
            else if (operation == "memory-load") m_app->slotLoadDocFromMemory(originalXML);
            else if (operation == "network-clear") m_app->slotClearDocFromNetwork();
            else if (operation == "vc-reset") vc->resetContents();
            else if (operation == "file-xml") loaded = m_app->loadXML(original) == QFile::NoError;
            else
            {
                QXmlStreamReader reader(originalXML);
                if (operation == "stream-xml")
                    loaded = m_app->loadXML(reader);
                else
                {
                    while (!reader.atEnd() &&
                           !(reader.isStartElement() && reader.name() == KXMLQLCVirtualConsole))
                        reader.readNext();
                    loaded = vc->loadXML(reader);
                }
            }
            QByteArray after;
            QXmlStreamWriter afterWriter(&after);
            vc->saveXML(&afterWriter);
            unchanged = before == after && selection == vc->selectedWidgetIDs() &&
                dirty == m_app->doc()->isModified() && fileName == m_app->fileName() &&
                workspacePath == m_app->doc()->workspacePath() && context == contexts->currentContext();
            qInfo() << "PUBLICATION_REPLACEMENT_RETURNED" << loaded << "unchanged" << unchanged;
        }
        else
            reentry = vc->editStructure(ids, frameID, 0, 0);
    };
    QMetaObject::Connection connection = deleting
        ? connect(vc, &VirtualConsole::widgetsChanged, this, observer, Qt::DirectConnection)
        : publication.startsWith("structure-before")
        ? connect(vc, &VirtualConsole::structureAboutToChange, this, observer, Qt::DirectConnection)
        : structure
        ? connect(vc, &VirtualConsole::structureChanged, this, observer, Qt::DirectConnection)
        : connect(first, &VCWidget::geometryChanged, this, observer, Qt::DirectConnection);
    const auto disconnectObserver = qScopeGuard([&]() { disconnect(connection); });
    const QVariantMap result = deleting ? vc->deleteAuthoringWidgets({firstID})
        : structure ? vc->editStructure({secondID}, frameID, 0, 0)
        : vc->applyGeometryPlan({{firstID, QRectF(40, 100, 100, 80)},
                                 {secondID, QRectF(240, 100, 100, 80)}});
    QVERIFY(observed);
    QVERIFY(coherent);
    QVERIFY(result.value("accepted").toBool());
    QVERIFY(result.value("applied").toBool());
    QVERIFY(second && retainedFunction);
    QCOMPARE(first.isNull(), deleting);
    QCOMPARE(vc->selectedWidgetIDs(), deleting ? QVariantList{secondID} : ids);
    if (!deleting) QCOMPARE(qobject_cast<VCButton *>(first.data())->functionID(), functionID);
    QCOMPARE(frame->orderedChildIDs(0), deleting ? QVariantList{secondID}
        : structure ? QVariantList({secondID, firstID}) : ids);
    if (!structure && !deleting)
    {
        QCOMPARE(first->logicalGeometry(), QRectF(40, 100, 100, 80));
        QCOMPARE(second->logicalGeometry(), QRectF(240, 100, 100, 80));
    }
    QVERIFY(m_app->doc()->isModified());
    if (replacement)
    {
        if (operation == "load" || operation == "missing-load" || operation == "new" || operation.endsWith("xml"))
            QVERIFY(!loaded);
        QVERIFY(unchanged);
        QCOMPARE(errors.count(), 1);
        QVERIFY(errors.first().first().toString().contains("busy", Qt::CaseInsensitive));
        QCOMPARE(loadedChanges.count(), 0);
        QCOMPARE(contextChanges.count(), 0);
        disconnect(connection);
        QSignalSpy ticks(m_app->doc()->masterTimer(), &MasterTimer::tickReady);
        if (operation == "load") QVERIFY(m_app->loadWorkspace(original));
        else if (operation == "missing-load")
        {
            QVERIFY(!m_app->loadWorkspace(files.filePath("missing.qxw")));
            QTRY_VERIFY(!ticks.isEmpty());
            m_app->doc()->masterTimer()->stop();
            ticks.clear();
            QVERIFY(m_app->loadWorkspace(original));
        }
        else if (operation == "new") QVERIFY(m_app->newWorkspace());
        else if (operation == "clear") m_app->clearDocument();
        else if (operation == "memory-load") m_app->slotLoadDocFromMemory(originalXML);
        else if (operation == "network-clear") m_app->slotClearDocFromNetwork();
        else if (operation == "vc-reset") vc->resetContents();
        else QVERIFY(m_app->loadWorkspace(original));
        if (operation != "vc-reset") QTRY_VERIFY(!ticks.isEmpty());
        m_app->doc()->masterTimer()->stop();
        QVERIFY(first.isNull() && second.isNull());
        if (operation != "vc-reset") QVERIFY(retainedFunction.isNull());
        QCOMPARE(errors.count(), 1);
    }
    else
    {
        QVERIFY(result.value("accepted").toBool());
        QVERIFY(!reentry.value("accepted").toBool());
        QVERIFY(!reentry.value("applied").toBool());
        QVERIFY(!reentry.value("error").toString().isEmpty());
        QVERIFY(m_app->doc()->isModified());
    }
}

void UpstreamIntegration_Test::vcDeferredReveal_data()
{
    QTest::addColumn<QString>("interleave");
    for (const QString &interleave : {QString("settled"), QString("page-switch"), QString("context-switch"),
                                     QString("view-unload"), QString("target-delete"), QString("subpage-switch"),
                                     QString("nested-subpage-switch")})
        QTest::newRow(qPrintable(interleave)) << interleave;
}

void UpstreamIntegration_Test::vcDeferredReveal()
{
    QFETCH(QString, interleave);
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const QSize oldSize = m_app->size();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode();
    const bool wasModified = m_app->doc()->isModified();
    const int oldPage = vc->selectedPage();
    const qreal oldScale = vc->page(0)->pageScale();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    QVariantList roots;
    quint32 functionID = Function::invalidId();
    QPointer<QQuickItem> loader;
    QPointer<QObject> errorPopup;
    bool wasErrorVisible = false;
    const auto cleanup = qScopeGuard([&]() {
        if (errorPopup)
        {
            QVERIFY(QMetaObject::invokeMethod(errorPopup.data(), wasErrorVisible ? "open" : "close"));
            QTRY_COMPARE(errorPopup->property("visible").toBool(), wasErrorVisible);
        }
        if (loader) loader->setProperty("active", true);
        vc->deleteVCWidgets(roots);
        if (functionID != Function::invalidId()) m_app->doc()->deleteFunction(functionID);
        vc->page(0)->setPageScale(oldScale - vc->page(0)->pageScale());
        vc->setSelectedPage(oldPage);
        vc->setEditMode(oldEdit);
        vc->setSelectedWidgetIDs(oldSelection);
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified) m_app->doc()->resetModified();
    });
    quint32 parentID = vc->page(0)->id();
    if (interleave == "nested-subpage-switch")
    {
        const QVariantMap outer = vc->createWidgetGeometry(parentID, "Frame", QRectF(1100, 350, 600, 400));
        QVERIFY(outer.value("accepted").toBool());
        roots = outer.value("widgetIDs").toList();
        parentID = roots.first().toUInt();
    }
    const QVariantMap creation = vc->createWidgetGeometry(parentID, "Frame",
        interleave == "nested-subpage-switch" ? QRectF(20, 30, 500, 350) : QRectF(1100, 350, 600, 400));
    QVERIFY2(creation.value("accepted").toBool(), qPrintable(creation.value("error").toString()));
    const quint32 frameID = creation.value("widgetIDs").toList().first().toUInt();
    if (roots.isEmpty()) roots.append(frameID);
    auto *frame = qobject_cast<VCFrame *>(vc->widget(frameID));
    QVERIFY(frame);
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(2);
    const QVariantMap control = vc->createWidgetGeometry(frame->id(), "Button", QRectF(50, 70, 120, 100), 1);
    QVERIFY(control.value("accepted").toBool());
    const quint32 id = control.value("widgetIDs").toList().first().toUInt();
    auto *button = qobject_cast<VCButton *>(vc->widget(id));
    QVERIFY(button);
    auto *scene = new Scene(m_app->doc());
    scene->setName("Reveal binding");
    QVERIFY(m_app->doc()->addFunction(scene));
    functionID = scene->id();
    button->setFunctionID(functionID);
    frame->setCollapsed(true);
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1200, 800);
    contexts->switchToContext("VC");
    vc->setSelectedPage(0);
    vc->page(0)->setPageScale(1 - vc->page(0)->pageScale());
    vc->setEditMode(true);
    vc->setSelectedWidgetIDs({id});
    auto *console = m_app->rootObject()->findChild<QQuickItem *>("virtualConsole");
    QVERIFY(console);
    errorPopup = console->findChild<QObject *>("vcLayoutError");
    QVERIFY(errorPopup);
    wasErrorVisible = errorPopup->property("visible").toBool();
    auto *pane = console->findChild<QQuickItem *>("vcFunctionsPane");
    QVERIFY(pane);
    QTRY_COMPARE(pane->width(), pane->property(pane->property("isOpen").toBool()
        ? "expandedWidth" : "collapseWidth").toReal());
    for (QQuickItem *candidate : console->findChildren<QQuickItem *>())
        if (candidate->property("source").toUrl() == QUrl("qrc:/VCPageArea.qml"))
            loader = candidate;
    QVERIFY(loader);
    QQuickItem *viewport = vc->currentPageItem();
    QVERIFY(viewport);
    QVERIFY(viewport->setProperty("contentX", 0));
    QVERIFY(viewport->setProperty("contentY", 0));
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    QSignalSpy starts(m_app->doc()->masterTimer(), &MasterTimer::functionStarted);
    QSignalSpy errors(vc, &VirtualConsole::editRejected);
    const QVariantMap result = vc->revealWidget(id);
    QVERIFY(result.value("accepted").toBool());
    QVERIFY(!frame->isCollapsed());
    QCOMPARE(frame->currentPage(), 1);
    QPointer<QQuickItem> observed(viewport);
    if (interleave == "page-switch")
    {
        vc->setSelectedPage(1);
        observed = vc->currentPageItem();
        QVERIFY(observed);
        QVERIFY(observed->setProperty("contentX", 13));
        QVERIFY(observed->setProperty("contentY", 17));
    }
    else if (interleave == "context-switch")
        contexts->switchToContext("FIXANDFUNC");
    else if (interleave == "view-unload")
    {
        QVERIFY(loader->setProperty("active", false));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(observed.isNull());
    }
    else if (interleave == "target-delete")
        QVERIFY(vc->deleteAuthoringWidgets({id}).value("accepted").toBool());
    else if (interleave.endsWith("subpage-switch"))
        frame->setCurrentPage(0);
    const qreal beforeX = observed ? observed->property("contentX").toReal() : 0;
    const qreal beforeY = observed ? observed->property("contentY").toReal() : 0;
    QByteArray before;
    QXmlStreamWriter writer(&before);
    vc->saveXML(&writer);
    const QVariantList selection = vc->selectedWidgetIDs();
    const int selectedPage = vc->selectedPage();
    const QString context = contexts->currentContext();
    const bool dirty = m_app->doc()->isModified();
    const int subpage = frame->currentPage();
    QTest::qWait(50);
    const qreal afterX = observed ? observed->property("contentX").toReal() : 0;
    const qreal afterY = observed ? observed->property("contentY").toReal() : 0;
    qInfo() << "DEFERRED_REVEAL" << interleave << "before" << QPointF(beforeX, beforeY)
            << "after" << QPointF(afterX, afterY) << "page" << vc->selectedPage()
            << "context" << contexts->currentContext() << "errors" << errors.count();
    QByteArray after;
    QXmlStreamWriter afterWriter(&after);
    vc->saveXML(&afterWriter);
    QCOMPARE(after, before);
    QCOMPARE(vc->selectedWidgetIDs(), selection);
    QCOMPARE(vc->selectedPage(), selectedPage);
    QCOMPARE(contexts->currentContext(), context);
    QCOMPARE(m_app->doc()->isModified(), dirty);
    QCOMPARE(frame->currentPage(), subpage);
    QCOMPARE(starts.count(), 0);
    QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(scene));
    QVERIFY(!scene->isRunning());
    if (interleave == "settled")
    {
        QVERIFY(observed);
        const QRectF displayed(1150, 420, 120, 100);
        QVERIFY(QRectF(afterX, afterY, observed->width(), observed->height()).contains(displayed));
        QCOMPARE(errors.count(), 0);
    }
    else
    {
        QCOMPARE(afterX, beforeX);
        QCOMPARE(afterY, beforeY);
        QCOMPARE(errors.count(), interleave == "view-unload" ? 1 : 0);
        if (interleave == "view-unload")
            QVERIFY(errors.first().first().toString().contains("Console page view is unavailable"));
    }
}

void UpstreamIntegration_Test::vcPageColorDeletion()
{
    auto *view2D = m_app->rootObject()->findChild<QQuickItem *>("twoDView");
    QVERIFY(view2D);
    QQmlExpression popupExpression(qmlContext(view2D), view2D, "monitorPOVPopup");
    QPointer<QObject> monitorPopup = popupExpression.evaluate().value<QObject *>();
    QVERIFY2(monitorPopup, qPrintable(popupExpression.error().toString()));
    const bool wasPopupVisible = monitorPopup->property("visible").toBool();
    const auto oldPointOfView = m_app->doc()->monitorProperties()->pointOfView();
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString original = files.filePath("original.qxw");
    const QString oldWorkingPath = m_app->workingPath();
    const bool wasModified = m_app->doc()->isModified();
    QCOMPARE(m_app->saveXML(original), QFile::NoError);
    const QSize oldSize = m_app->size();
    const bool oldVisible = m_app->isVisible();
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
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (wasModified) m_app->doc()->setModified();
        else m_app->doc()->resetModified();
    });
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    contexts->switchToContext("VC");
    const int count = vc->pagesCount();
    QVERIFY(count > 1);
    vc->setSelectedPage(count - 1);
    vc->setEditMode(true);
    QQuickItem *viewport = nullptr;
    QTRY_VERIFY((viewport = vc->currentPageItem()));
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    QTest::failOnWarning(QRegularExpression(".*VCPageArea.qml.*(TypeError|Cannot read).*"));
    vc->deletePage(count - 1);
    QTest::qWait(30);
    QCOMPARE(vc->pagesCount(), count - 1);
    QCOMPARE(vc->selectedPage(), count - 2);
    QTRY_VERIFY(vc->currentPageItem());
    qInfo() << "PAGE_COLOR_DELETE" << "count" << vc->pagesCount() << "selected" << vc->selectedPage();
}

void UpstreamIntegration_Test::vcFrameLegacyClip()
{
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const QSize oldSize = m_app->size();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode(), wasModified = m_app->doc()->isModified();
    const qreal oldScale = vc->page(0)->pageScale();
    const int oldPage = vc->selectedPage();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    const QVariantMap created = vc->createWidgetGeometry(vc->page(0)->id(), "Frame", QRectF(20, 20, 300, 300));
    QVERIFY(created.value("accepted").toBool());
    const quint32 frameID = created.value("widgetIDs").toList().first().toUInt();
    auto *scene = new Scene(m_app->doc());
    scene->setName("Clipped legacy binding");
    QVERIFY(m_app->doc()->addFunction(scene));
    const quint32 functionID = scene->id();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({frameID});
        m_app->doc()->deleteFunction(functionID);
        vc->page(0)->setPageScale(oldScale - vc->page(0)->pageScale());
        vc->setSelectedPage(oldPage);
        vc->setEditMode(oldEdit);
        vc->setSelectedWidgetIDs(oldSelection);
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified) m_app->doc()->resetModified();
    });
    const QVariantMap control = vc->createWidgetGeometry(frameID, "Button", QRectF(20, 60, 100, 70));
    QVERIFY(control.value("accepted").toBool());
    VCWidget *button = vc->widget(control.value("widgetIDs").toList().first().toUInt());
    qobject_cast<VCButton *>(button)->setFunctionID(functionID);
    button->setCaption("Legacy clipped");
    button->setLogicalGeometry(QRectF(20, 310, 100, 70));
    VCWidget *frame = vc->widget(frameID);
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1400, 900);
    contexts->switchToContext("VC");
    vc->setSelectedPage(0);
    vc->page(0)->setPageScale(1 - vc->page(0)->pageScale());
    vc->setEditMode(true);
    QTRY_VERIFY(frame->renderItem() && button->renderItem());
    QVERIFY(frame->renderItem()->clip());
    QSignalSpy starts(m_app->doc()->masterTimer(), &MasterTimer::functionStarted);
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    const QPoint press = button->renderItem()->mapToScene(
        QPointF(button->renderItem()->width() / 2, button->renderItem()->height() / 2)).toPoint();
    QVERIFY(QRect(QPoint(), m_app->size()).contains(press));
    vc->resetWidgetSelection();
    QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
    QVERIFY(vc->selectedWidgetIDs().isEmpty());
    const QString directory = qEnvironmentVariable("QLC_VC_VISUAL_DIR");
    if (!directory.isEmpty())
    {
        QVERIFY(QDir().mkpath(directory));
        QVERIFY(m_app->grabWindow().save(QDir(directory).filePath("clip-candidate-before.png")));
    }
    QByteArray before;
    QXmlStreamWriter writer(&before);
    vc->saveXML(&writer);
    for (bool editing : {false, true})
    {
        vc->setEditMode(editing);
        QVERIFY(frame->renderItem()->clip());
        QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        QVERIFY(vc->selectedWidgetIDs().isEmpty());
        QVERIFY(!scene->isRunning());
        QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(scene));
    }
    if (!directory.isEmpty())
        QVERIFY(m_app->grabWindow().save(QDir(directory).filePath("legacy-clip-after.png")));
    QByteArray after;
    QXmlStreamWriter afterWriter(&after);
    vc->saveXML(&afterWriter);
    QCOMPARE(after, before);
    QCOMPARE(starts.count(), 0);
    QVERIFY(!scene->isRunning());
    QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(scene));
    QCOMPARE(qobject_cast<VCButton *>(button)->functionID(), functionID);
    qInfo() << "FRAME_LEGACY_CLIP" << "legacyOutsideDescendantRemainsClippedInEditorAndRuntime" << true
            << "modelUnchanged" << true << "FunctionStarts" << starts.count();
}

void UpstreamIntegration_Test::vcFramePresentation_data()
{
    QTest::addColumn<QString>("mode");
    QTest::addColumn<qreal>("scale");
    for (const QString &mode : {QString("images-runtime"), QString("hierarchy-order"), QString("sibling-order"),
                               QString("collapse-inactive"), QString("lifetime"), QString("bounded-clearance")})
        QTest::newRow(qPrintable(mode)) << mode << qreal(1);
    for (qreal scale : {qreal(0.8), qreal(1.1), qreal(2)})
    {
        QTest::newRow(qPrintable(QString("sibling-cover-%1").arg(scale))) << QString("sibling-cover") << scale;
        QTest::newRow(qPrintable(QString("leaf-cover-%1").arg(scale))) << QString("leaf-cover") << scale;
    }
}

void UpstreamIntegration_Test::vcFramePresentation()
{
    QFETCH(QString, mode);
    QFETCH(qreal, scale);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    VirtualConsole *vc = m_app->virtualConsole();
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    const QString oldContext = contexts->currentContext();
    const QSize oldSize = m_app->size();
    const bool oldVisible = m_app->isVisible(), oldEdit = vc->editMode(), oldSnap = vc->snapping();
    const bool wasModified = m_app->doc()->isModified();
    const qreal oldScale = vc->page(0)->pageScale();
    const int oldPage = vc->selectedPage();
    const QVariantList oldSelection = vc->selectedWidgetIDs();
    QVariantList roots;
    quint32 functionID = Function::invalidId();
    const auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets(roots);
        if (functionID != Function::invalidId()) m_app->doc()->deleteFunction(functionID);
        vc->page(0)->setPageScale(oldScale - vc->page(0)->pageScale());
        vc->setSelectedPage(oldPage);
        vc->setEditMode(oldEdit);
        vc->setSnapping(oldSnap);
        vc->setSelectedWidgetIDs(oldSelection);
        contexts->switchToContext(oldContext);
        m_app->resize(oldSize);
        m_app->setVisible(oldVisible);
        if (!wasModified) m_app->doc()->resetModified();
    });
    auto create = [&](quint32 parent, const QRectF &rect) -> VCFrame * {
        const QVariantMap result = vc->createWidgetGeometry(parent, "Frame", rect);
        if (!result.value("accepted").toBool()) return nullptr;
        const quint32 id = result.value("widgetIDs").toList().first().toUInt();
        if (parent == vc->page(0)->id()) roots.append(id);
        return qobject_cast<VCFrame *>(vc->widget(id));
    };
    VCFrame *first = nullptr, *second = nullptr;
    if (mode == "hierarchy-order")
    {
        second = create(vc->page(0)->id(), QRectF(80, 80, 280, 260));
        first = create(vc->page(0)->id(), QRectF(500, 20, 600, 550));
        QVERIFY(first && second);
        QVERIFY(vc->editStructure({second->id()}, first->id(), 0).value("accepted").toBool());
    }
    else
    {
        first = create(vc->page(0)->id(), QRectF(20, 20, 600, 550));
        QVERIFY(first);
        second = create(mode.startsWith("sibling") ? vc->page(0)->id() : first->id(),
            mode.startsWith("sibling") ? QRectF(700, 20, 600, 550) : QRectF(80, 80, 280, 260));
        QVERIFY(second);
        if (mode.startsWith("sibling"))
            second->setLogicalGeometry(first->logicalGeometry());
    }
    const QColor firstColor(40, 70, 120), secondColor(110, 70, 40), imageColor(30, 180, 90);
    first->setBackgroundColor(firstColor);
    second->setBackgroundColor(secondColor);
    first->setZIndex(mode.startsWith("sibling") || mode == "leaf-cover" ? 10 : 0);
    second->setZIndex(mode.startsWith("sibling") ? 1 : 0);
    const QVariantMap createdControl = vc->createWidgetGeometry(mode == "leaf-cover" ? vc->page(0)->id() : second->id(), "Button",
        mode == "leaf-cover" ? QRectF(1000, 20, 100, 70) : QRectF(10, 45, 100, 70));
    QVERIFY(createdControl.value("accepted").toBool());
    auto *scene = new Scene(m_app->doc());
    scene->setName("Presentation binding");
    QVERIFY(m_app->doc()->addFunction(scene));
    functionID = scene->id();
    QPointer<VCButton> control = qobject_cast<VCButton *>(vc->widget(createdControl.value("widgetIDs").toList().first().toUInt()));
    QVERIFY(control);
    if (mode == "leaf-cover")
    {
        roots.append(control->id());
        control->setLogicalGeometry(QRectF(first->logicalGeometry().topLeft() + QPointF(10, 45), QSizeF(100, 70)));
        control->setZIndex(5);
    }
    control->setFunctionID(functionID);
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    m_app->resize(1800, 1250);
    contexts->switchToContext("VC");
    vc->setSelectedPage(0);
    vc->page(0)->setPageScale(scale - vc->page(0)->pageScale());
    QCOMPARE(vc->page(0)->pageScale(), scale);
    vc->setEditMode(true);
    vc->setSnapping(false);
    QTRY_VERIFY(first->renderItem() && second->renderItem());
    QTest::failOnWarning(QRegularExpression(".*(VCWidgetItem|VCFrameItem|VCPageArea)\\.qml.*(TypeError|ReferenceError|Binding loop).*"));
    auto findItem = [&](const QString &name) {
        QQuickItem *found = nullptr;
        std::function<void(QQuickItem *)> visit = [&](QQuickItem *item) {
            if (item->objectName() == name) found = item;
            for (QQuickItem *child : item->childItems()) visit(child);
        };
        visit(m_app->rootObject());
        return found;
    };
    QPointer<QQuickItem> backdrop = second->renderItem();
    QPointer<QQuickItem> chrome = findItem(QString("vcFrameChrome-%1").arg(second->id()));
    QVERIFY(backdrop && chrome);
    auto pixel = [&](QQuickItem *item, const QPointF &local) {
        const QImage image = m_app->grabWindow();
        const QPoint point = (item->mapToScene(local) * image.devicePixelRatio()).toPoint();
        return image.pixelColor(point);
    };
    QSignalSpy rendered(m_app.get(), &QQuickWindow::frameSwapped);
    m_app->update();
    QTRY_VERIFY(!rendered.isEmpty());
    const QRectF firstRect = first->logicalGeometry(), secondRect = second->logicalGeometry();
    QSignalSpy starts(m_app->doc()->masterTimer(), &MasterTimer::functionStarted);
    if (mode == "sibling-cover" || mode == "leaf-cover")
    {
        const QColor actual = pixel(first->renderItem(), QPointF(50 * scale, 80 * scale));
        const QPoint press = first->renderItem()->mapToScene(QPointF(50 * scale, 80 * scale)).toPoint();
        QVERIFY(QRect(QPoint(), m_app->size()).contains(press));
        QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        qInfo() << "MIXED_FRAME_STACK" << mode << scale << "pixel" << actual << "expected" << firstColor
                << "selection" << vc->selectedWidgetIDs() << "expectedFrame" << first->id()
                << "lowerControl" << control->id();
        QCOMPARE(actual, firstColor);
        QCOMPARE(vc->selectedWidgetIDs(), QVariantList{first->id()});
    }
    else if (mode == "images-runtime")
    {
        QImage image(16, 16, QImage::Format_ARGB32);
        image.fill(imageColor);
        const QString path = files.filePath("frame.png");
        QVERIFY(image.save(path));
        first->setBackgroundImage(path);
        QTest::qWait(30);
        QCOMPARE(pixel(first->renderItem(), QPointF(40, 420)), imageColor);
        QCOMPARE(pixel(second->renderItem(), QPointF(150, 180)), secondColor);
        vc->setSelectedWidgetIDs({second->id()});
        QVERIFY(chrome->isVisible());
        vc->setEditMode(false);
        QTest::qWait(30);
        QVERIFY(backdrop->isVisible() && !chrome->isVisible());
        QCOMPARE(pixel(first->renderItem(), QPointF(40, 420)), imageColor);
        QCOMPARE(pixel(second->renderItem(), QPointF(150, 180)), secondColor);
        first->setBackgroundImage({});
        vc->setEditMode(true);
        QVERIFY(first->renderItem()->setProperty("dropActive", true));
        QTest::qWait(30);
        QCOMPARE(pixel(first->renderItem(), QPointF(40, 420)),
                 first->renderItem()->property("frameBackgroundColor").value<QColor>());
    }
    else if (mode == "hierarchy-order")
    {
        QCOMPARE(pixel(first->renderItem(), QPointF(40, 420)), firstColor);
        QCOMPARE(pixel(second->renderItem(), QPointF(150, 180)), secondColor);
        const QPoint press = second->renderItem()->mapToScene(QPointF(150, 180)).toPoint();
        QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        QCOMPARE(vc->selectedWidgetIDs(), QVariantList{second->id()});
    }
    else if (mode == "sibling-order")
    {
        QCOMPARE(pixel(first->renderItem(), QPointF(150, 180)), firstColor);
        const QPoint press = first->renderItem()->mapToScene(QPointF(150, 180)).toPoint();
        QTest::mouseClick(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        QCOMPARE(vc->selectedWidgetIDs(), QVariantList{first->id()});
        second->setZIndex(20);
        QTest::qWait(30);
        QCOMPARE(pixel(first->renderItem(), QPointF(150, 180)), secondColor);
        QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::NoModifier, press);
        QTest::mouseMove(m_app.get(), press + QPoint(25, 0), 20);
        QVERIFY(second->renderItem()->property("moving").toBool());
        QVERIFY(!first->renderItem()->property("moving").toBool());
        QTest::keyClick(m_app.get(), Qt::Key_Escape);
        QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::NoModifier, press + QPoint(25, 0));
        QCOMPARE(vc->selectedWidgetIDs(), QVariantList{second->id()});
    }
    else if (mode == "collapse-inactive")
    {
        first->setMultiPageMode(true);
        first->setTotalPagesNumber(2);
        vc->setSelectedWidgetIDs({second->id()});
        QVERIFY(backdrop->isVisible() && chrome->isVisible());
        first->setCollapsed(true);
        QTRY_VERIFY(!chrome->isVisible());
        first->setCollapsed(false);
        QTRY_VERIFY(backdrop->isVisible() && chrome->isVisible());
        first->setCurrentPage(1);
        QTRY_VERIFY(!backdrop->isVisible() && !chrome->isVisible());
        first->setCurrentPage(0);
        QTRY_VERIFY(backdrop->isVisible() && chrome->isVisible());
    }
    else if (mode == "bounded-clearance")
    {
        const QVariantMap createdPeer = vc->createWidgetGeometry(vc->page(0)->id(), "Frame", QRectF(1200, 20, 600, 800));
        QVERIFY2(createdPeer.value("accepted").toBool(), qPrintable(createdPeer.value("error").toString()));
        roots.append(createdPeer.value("widgetIDs").toList().first());
        VCWidget *peer = vc->widget(roots.last().toUInt());
        peer->setLogicalGeometry(QRectF(0, 0, 1400, 900));
        peer->setZIndex(20);
        QTRY_VERIFY(peer->renderItem());
        QByteArray before;
        QXmlStreamWriter beforeWriter(&before);
        QVERIFY(vc->saveXML(&beforeWriter));
        vc->setSelectedWidgetIDs({second->id()});
        QObject *popup = m_app->rootObject()->findChild<QObject *>("vcLayoutError");
        QVERIFY(popup);
        QTRY_VERIFY(popup->property("visible").toBool());
        for (int corner = 0; corner < 4; ++corner)
        {
            QQuickItem *owner = findItem(QString("vcResizeCorner-%1-%2").arg(second->id()).arg(corner));
            QVERIFY(owner);
            QQmlExpression error(qmlContext(owner), owner, "placement.error");
            QVERIFY(error.evaluate().toString().contains("No clear resize handle position"));
            QCOMPARE(popup->property("message").toString(), error.evaluate().toString());
            QVERIFY(!findItem(QString("vcResizeHandle-%1-%2").arg(second->id()).arg(corner)));
        }
        QVERIFY(!second->renderItem()->property("resizing").toBool());
        QCOMPARE(vc->selectedWidgetIDs(), QVariantList{second->id()});
        QByteArray after;
        QXmlStreamWriter afterWriter(&after);
        QVERIFY(vc->saveXML(&afterWriter));
        QCOMPARE(after, before);
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        QTRY_VERIFY(!popup->property("visible").toBool());
        qInfo() << "BOUNDED_CLEARANCE" << "explicitDiagnostic" << popup->property("message")
                << "geometryIDsOrderBindingsUnchanged" << true << "FunctionStarts" << starts.count();
    }
    else
    {
        QVERIFY(vc->editGeometry(second->id(), QRectF(first->contentRect().topLeft(),
            second->logicalGeometry().size())).value("accepted").toBool());
        vc->setSelectedWidgetIDs({second->id()});
        QSignalSpy paint(m_app.get(), &QQuickWindow::frameSwapped);
        m_app->update();
        QTRY_VERIFY(!paint.isEmpty());
        QList<QPointer<QQuickItem>> projections;
        std::function<void(QQuickItem *)> collect = [&](QQuickItem *item) {
            if (item->objectName().startsWith(QString("vcResizeFragment-%1-").arg(second->id())) ||
                item->objectName().startsWith(QString("vcResizeCorner-%1-").arg(second->id())))
                projections.append(item);
            for (QQuickItem *child : item->childItems()) collect(child);
        };
        collect(m_app->rootObject());
        QVERIFY(projections.size() > 8);
        QVERIFY(vc->deleteAuthoringWidgets({first->id()}).value("accepted").toBool());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(backdrop.isNull() && chrome.isNull());
        for (const auto &projection : projections) QVERIFY(projection.isNull());
        QVERIFY(control.isNull());
        QCOMPARE(starts.count(), 0);
        QVERIFY(!scene->isRunning());
        QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(scene));
        return;
    }
    QCOMPARE(first->logicalGeometry(), firstRect);
    QCOMPARE(second->logicalGeometry(), secondRect);
    QCOMPARE(starts.count(), 0);
    QVERIFY(control && control->functionID() == functionID);
    QVERIFY(!scene->isRunning());
    QVERIFY(!m_app->doc()->masterTimer()->isStartQueued(scene));
    qInfo() << "FRAME_PRESENTATION" << mode << "geometryUnchanged" << true << "FunctionStarts" << starts.count();
}

QTEST_MAIN(UpstreamIntegration_Test)
#include "upstreamintegration_test.moc"
