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
#include "performfsm.h"
#include "qlccapability.h"
#include "qlcfixturedef.h"
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

void UpstreamIntegration_Test::videoEditorUrlDialog_data()
{
    QTest::addColumn<bool>("escape");
    QTest::newRow("cancel") << false;
    QTest::newRow("escape") << true;
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

QTEST_MAIN(UpstreamIntegration_Test)
#include "upstreamintegration_test.moc"
