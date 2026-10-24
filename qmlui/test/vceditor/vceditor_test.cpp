/*
  Q Light Controller Plus - Unit test
  vceditor_test.cpp

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

#include "vceditor_test.h"

#include <QtTest>
#include <QBuffer>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QSettings>
#include <QXmlStreamWriter>
#include <functional>

#define private public
#include "app.h"
#include "tardis.h"
#undef private
#include "contextmanager.h"
#include "doc.h"
#include "mastertimer.h"
#include "scene.h"
#include "uimanager.h"
#include "vcframe.h"
#include "vcpage.h"
#include "virtualconsole.h"

/* Tardis merges actions closer than this into one undo step */
static const int kUndoStepGapMs = 200;

using ConsoleEdit = std::function<QVariantMap(VirtualConsole *, Doc *, const ConsoleIds &)>;

static QString editError(const QVariantMap &result)
{
    if (result.isEmpty() || result.value("accepted").toBool())
        return QString();
    return result.value("error").toString();
}

/* The first line where two XML dumps differ, for readable failures */
static QString firstDifference(const QByteArray &expected, const QByteArray &actual)
{
    const QList<QByteArray> want = expected.split('\n'), got = actual.split('\n');
    for (int i = 0; i < qMax(want.size(), got.size()); i++)
    {
        const QByteArray w = want.value(i), g = got.value(i);
        if (w != g)
            return QString("line %1\n  expected: %2\n  actual:   %3")
                .arg(i + 1).arg(QString::fromUtf8(w.trimmed()), QString::fromUtf8(g.trimmed()));
    }
    return QString();
}

/* With QLC_VC_ARTIFACTS_DIR set, keep a file per test row for review */
static void saveArtifact(const QString &name, const QByteArray &data)
{
    const QString root = qEnvironmentVariable("QLC_VC_ARTIFACTS_DIR");
    if (root.isEmpty())
        return;
    QDir dir(QDir(root).filePath(QString::fromLatin1(QTest::currentTestFunction())));
    dir.mkpath(".");
    QFile file(dir.filePath(QString("%1-%2").arg(QString::fromLatin1(QTest::currentDataTag()), name)));
    if (file.open(QIODevice::WriteOnly))
        file.write(data);
}

static QMap<QString, ConsoleEdit> consoleEdits()
{
    return {
        {"move", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->editGeometry(ids.loose, QRectF(600, 120, 100, 80), false);
        }},
        {"resize", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->editGeometry(ids.loose, QRectF(600, 20, 140, 100), false);
        }},
        {"resizeFrame", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->editGeometry(ids.frame, QRectF(20, 20, 540, 440), false);
        }},
        {"create", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->createWidgetGeometry(ids.page, "Button", QRectF(1200, 20, 100, 80));
        }},
        {"createInFrame", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->createWidgetGeometry(ids.empty, "Slider", QRectF(20, 60, 60, 150));
        }},
        {"deleteButton", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->deleteAuthoringWidgets({ids.loose});
        }},
        {"deleteFirstChild", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->deleteAuthoringWidgets({ids.a});
        }},
        {"deleteSelection", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->deleteAuthoringWidgets({ids.b, ids.a});
        }},
        {"deleteFrame", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->deleteAuthoringWidgets({ids.frame});
        }},
        {"deleteNestedFrame", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->deleteAuthoringWidgets({ids.nested});
        }},
        {"dropIntoFrame", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->editDrop(ids.loose, ids.empty, 0, QRectF(20, 60, 100, 80));
        }},
        {"dropOutOfFrame", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->editDrop(ids.a, ids.page, 0, QRectF(1200, 300, 100, 80));
        }},
        {"reorder", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->editStructure({ids.b}, ids.frame, 0, 0);
        }},
        {"selectionDrop", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            return vc->editSelectionDrop({ids.a, ids.b}, ids.a, ids.page, 0, QPointF(1200, 600));
        }},
        {"nudge", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            vc->setSelectedWidgetIDs({ids.loose});
            vc->nudgeWidgets(1, 0);
            return QVariantMap();
        }},
        {"paste", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            vc->setSelectedWidgetIDs({ids.loose});
            vc->copyToClipboard();
            vc->setSelectedWidgetIDs({ids.empty});
            vc->pasteFromClipboard();
            return QVariantMap();
        }},
        {"alignLeft", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            vc->setSelectedWidgetIDs({ids.loose, ids.empty});
            vc->setWidgetsAlignment(vc->widget(ids.loose), Qt::AlignLeft);
            return QVariantMap();
        }},
        {"caption", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            vc->setSelectedWidgetIDs({ids.loose});
            vc->setWidgetsCaption("Renamed");
            return QVariantMap();
        }},
        {"cutPaste", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            vc->setSelectedWidgetIDs({ids.loose});
            vc->cutToClipboard();
            vc->setSelectedWidgetIDs({ids.empty});
            vc->pasteFromClipboard();
            return QVariantMap();
        }},
        {"alignTop", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            vc->setSelectedWidgetIDs({ids.empty, ids.loose});
            vc->setWidgetsAlignment(vc->widget(ids.loose), Qt::AlignTop);
            return QVariantMap();
        }},
        {"distribute", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            vc->setSelectedWidgetIDs({ids.frame, ids.loose, ids.empty});
            vc->setWidgetsDistribution(Qt::Horizontal);
            return QVariantMap();
        }},
        {"background", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            vc->setSelectedWidgetIDs({ids.loose, ids.a});
            vc->setWidgetsBackgroundColor(Qt::red);
            return QVariantMap();
        }},
        {"font", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            vc->setSelectedWidgetIDs({ids.loose});
            vc->setWidgetsFont(QFont("Arial", 20));
            return QVariantMap();
        }},
        {"zIndex", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            vc->widget(ids.loose)->setZIndex(5);
            return QVariantMap();
        }},
        {"hideHeader", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            qobject_cast<VCFrame *>(vc->widget(ids.empty))->setShowHeader(false);
            return QVariantMap();
        }},
        {"hideEnableButton", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            qobject_cast<VCFrame *>(vc->widget(ids.frame))->setShowEnable(false);
            return QVariantMap();
        }},
        {"disableFrame", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            vc->widget(ids.frame)->setDisabled(true);
            return QVariantMap();
        }},
        {"multiPage", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            auto *frame = qobject_cast<VCFrame *>(vc->widget(ids.frame));
            frame->setMultiPageMode(true);
            frame->setTotalPagesNumber(3);
            return QVariantMap();
        }},
        {"pagesLoop", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            auto *frame = qobject_cast<VCFrame *>(vc->widget(ids.frame));
            frame->setMultiPageMode(true);
            frame->setPagesLoop(true);
            return QVariantMap();
        }},
        {"cloneFirstPage", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            auto *frame = qobject_cast<VCFrame *>(vc->widget(ids.frame));
            frame->setMultiPageMode(true);
            frame->setTotalPagesNumber(2);
            frame->cloneFirstPage();
            return QVariantMap();
        }},
        {"buttonMatrix", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            VCWidget *page = vc->widget(ids.page);
            qobject_cast<VCFrame *>(page)->addWidgetMatrix(page->renderItem(), "buttonmatrix",
                                                           QPoint(1200, 20), QSize(2, 2), QSize(60, 60));
            return QVariantMap();
        }},
        {"functionDrop", [](VirtualConsole *vc, Doc *doc, const ConsoleIds &ids) {
            auto *scene = new Scene(doc);
            scene->setName("Dropped scene");
            doc->addFunction(scene);
            return vc->createFunctions(ids.page, {scene->id()}, QPointF(1200, 20), 0);
        }},
        {"confirmLayout", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            const QVariantMap preview = vc->previewLayout(ids.frame, {{"allowGrowth", true}});
            return preview.value("accepted").toBool() ? vc->confirmLayout() : preview;
        }},
        {"addPage", [](VirtualConsole *vc, Doc *, const ConsoleIds &) {
            vc->addPage(1);
            return QVariantMap();
        }},
        {"deletePage", [](VirtualConsole *vc, Doc *, const ConsoleIds &) {
            vc->deletePage(3);
            return QVariantMap();
        }},
        {"collapse", [](VirtualConsole *vc, Doc *, const ConsoleIds &ids) {
            qobject_cast<VCFrame *>(vc->widget(ids.frame))->setCollapsed(true);
            return QVariantMap();
        }},
    };
}

void VCEditor_Test::initTestCase()
{
    QVERIFY(m_settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, m_settings.path());
    QCoreApplication::setOrganizationName("QLCPlusVCEditorTests");
    QCoreApplication::setApplicationName("VCEditor");
    m_app = std::make_unique<App>();
    m_app->set3dSupported(false);
    m_app->startup();
    // Geometry expectations use a fixed scale, independent of the offscreen screen size
    m_app->m_uiManager->setModified("scalingFactor", 1.0);
    m_app->resize(1200, 800);
    QVERIFY2(m_app->status() == QQuickView::Ready, qPrintable(m_app->errors().value(0).toString()));
    QQmlExpression startupNotice(qmlContext(m_app->rootObject()), m_app->rootObject(), "scalingInfoPopup");
    QObject *notice = startupNotice.evaluate().value<QObject *>();
    QVERIFY2(notice, qPrintable(startupNotice.error().toString()));
    // Fresh offscreen settings can open the modal first-use scaling notice
    if (notice->property("visible").toBool())
    {
        QVERIFY(QMetaObject::invokeMethod(notice, "accept"));
        QTRY_VERIFY(!notice->property("visible").toBool());
    }
    QTRY_COMPARE(m_app->rootObject()->property("popupCount").toInt(), 0);
    m_app->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_app.get()));
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    contexts->switchToContext("VC");
}

void VCEditor_Test::init()
{
    QVERIFY(m_app->newWorkspace());
    m_app->doc()->masterTimer()->stop();
    vc()->setSelectedPage(0);
    vc()->setEditMode(true);
    vc()->setSnapping(false);
}

void VCEditor_Test::cleanupTestCase()
{
    m_app.reset();
}

VirtualConsole *VCEditor_Test::vc() const
{
    return m_app->virtualConsole();
}

ConsoleIds VCEditor_Test::buildConsole()
{
    ConsoleIds ids;
    ids.page = vc()->page(0)->id();
    auto create = [&](quint32 parent, const QString &type, const QRectF &rect) {
        const QVariantMap result = vc()->createWidgetGeometry(parent, type, rect);
        if (!result.value("accepted").toBool())
            qFatal("Cannot build the test console: %s", qPrintable(result.value("error").toString()));
        return result.value("widgetIDs").toList().first().toUInt();
    };
    ids.frame = create(ids.page, "Frame", QRectF(20, 20, 500, 420));
    ids.a = create(ids.frame, "Button", QRectF(10, 60, 100, 80));
    ids.b = create(ids.frame, "Button", QRectF(130, 60, 100, 80));
    ids.nested = create(ids.frame, "Frame", QRectF(10, 180, 300, 200));
    ids.inner = create(ids.nested, "Button", QRectF(10, 60, 100, 80));
    ids.empty = create(ids.page, "Frame", QRectF(750, 200, 300, 250));
    ids.loose = create(ids.page, "Button", QRectF(600, 20, 100, 80));
    vc()->resetWidgetSelection();
    settleHistory();
    return ids;
}

QByteArray VCEditor_Test::consoleXML() const
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.setAutoFormatting(true);
    vc()->saveXML(&writer);
    return buffer.data();
}

void VCEditor_Test::settleHistory()
{
    Tardis *tardis = Tardis::instance();
    QTRY_VERIFY(([tardis]() {
        QMutexLocker locker(&tardis->m_queueMutex);
        return tardis->m_actionsQueue.isEmpty();
    })());
    QTest::qWait(kUndoStepGapMs);
}

/*********************************************************************
 * Every console edit is one undo step: undo restores the saved
 * console byte for byte and redo restores the edited one
 *********************************************************************/

void VCEditor_Test::editUndoRedoRoundTrip_data()
{
    QTest::addColumn<QString>("edit");
    for (const QString &edit : consoleEdits().keys())
        QTest::newRow(qPrintable(edit)) << edit;
}

void VCEditor_Test::editUndoRedoRoundTrip()
{
    QFETCH(QString, edit);
    const ConsoleIds ids = buildConsole();
    const QByteArray before = consoleXML();

    QSignalSpy rejected(vc(), &VirtualConsole::editRejected);
    const QString error = editError(consoleEdits().value(edit)(vc(), m_app->doc(), ids));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY2(rejected.isEmpty(), qPrintable(rejected.value(0).value(0).toString()));
    vc()->resetWidgetSelection();
    settleHistory();
    const QByteArray after = consoleXML();
    saveArtifact("before.xml", before);
    saveArtifact("after.xml", after);
    QVERIFY2(after != before, "The edit changed nothing");

    Tardis::instance()->undoAction();
    settleHistory();
    QByteArray actual = consoleXML();
    saveArtifact("undone.xml", actual);
    QEXPECT_FAIL("cloneFirstPage", "Not recorded", Abort);
    QEXPECT_FAIL("addPage", "Not recorded", Abort);
    QEXPECT_FAIL("deletePage", "Not recorded", Abort);
    QEXPECT_FAIL("font", "Undo stores the default font as an explicit one", Abort);
    QVERIFY2(actual == before, qPrintable("Undo: " + firstDifference(before, actual)));

    Tardis::instance()->redoAction();
    settleHistory();
    actual = consoleXML();
    saveArtifact("redone.xml", actual);
    QVERIFY2(actual == after, qPrintable("Redo: " + firstDifference(after, actual)));
}

void VCEditor_Test::liveToggleMakesNoHistory_data()
{
    QTest::addColumn<QString>("toggle");
    QTest::newRow("collapse") << QString("collapse");
    QTest::newRow("disable") << QString("disable");
}

void VCEditor_Test::liveToggleMakesNoHistory()
{
    QFETCH(QString, toggle);
    const ConsoleIds ids = buildConsole();
    Tardis *tardis = Tardis::instance();
    const int steps = tardis->m_history.count();
    vc()->setEditMode(false);

    auto *frame = qobject_cast<VCFrame *>(vc()->widget(ids.frame));
    if (toggle == "collapse")
        frame->setCollapsed(true);
    else
        frame->setDisabled(true);
    settleHistory();
    QCOMPARE(tardis->m_history.count(), steps);
}

/*********************************************************************
 * Selection and keyboard editing
 *********************************************************************/

void VCEditor_Test::pageSwitchPrunesSelection_data()
{
    QTest::addColumn<bool>("selectOnTarget");
    QTest::newRow("other-page") << false;
    QTest::newRow("target-page") << true;
}

void VCEditor_Test::pageSwitchPrunesSelection()
{
    QFETCH(bool, selectOnTarget);
    const ConsoleIds ids = buildConsole();
    const QVariantMap created = vc()->createWidgetGeometry(vc()->page(1)->id(), "Button", QRectF(20, 20, 100, 80));
    QVERIFY(created.value("accepted").toBool());
    const quint32 onTarget = created.value("widgetIDs").toList().first().toUInt();
    QVariantList selection {ids.loose, ids.frame};
    if (selectOnTarget)
        selection.append(onTarget);
    vc()->setSelectedWidgetIDs(selection);

    vc()->setSelectedPage(1);
    QCOMPARE(vc()->selectedWidgetIDs(), selectOnTarget ? QVariantList{onTarget} : QVariantList());
}

void VCEditor_Test::nudgeStep_data()
{
    QTest::addColumn<bool>("snapping");
    QTest::addColumn<int>("steps");
    QTest::addColumn<qreal>("moved");
    QTest::newRow("free") << false << 1 << qreal(1);
    QTest::newRow("free-shift") << false << 5 << qreal(5);
    QTest::newRow("snapped") << true << 1 << qreal(10);
    QTest::newRow("snapped-shift") << true << 5 << qreal(50);
}

void VCEditor_Test::nudgeStep()
{
    QFETCH(bool, snapping);
    QFETCH(int, steps);
    QFETCH(qreal, moved);
    const ConsoleIds ids = buildConsole();
    vc()->setSnappingSize(10);
    vc()->setSnapping(snapping);
    const QRectF before = vc()->widget(ids.loose)->logicalGeometry();

    vc()->setSelectedWidgetIDs({ids.loose});
    vc()->nudgeWidgets(steps, 0);
    QCOMPARE(vc()->widget(ids.loose)->logicalGeometry(), before.translated(moved, 0));
}

void VCEditor_Test::alignToLastSelected_data()
{
    QTest::addColumn<bool>("frameLast");
    QTest::newRow("button-last") << false;
    QTest::newRow("frame-last") << true;
}

void VCEditor_Test::alignToLastSelected()
{
    QFETCH(bool, frameLast);
    const ConsoleIds ids = buildConsole();
    const quint32 first = frameLast ? ids.loose : ids.empty;
    const quint32 last = frameLast ? ids.empty : ids.loose;
    const qreal lastX = vc()->widget(last)->logicalGeometry().x();

    vc()->setWidgetSelection(first, nullptr, true, true);
    vc()->setWidgetSelection(last, nullptr, true, true);
    QCOMPARE(vc()->selectedWidget(), vc()->widget(last));
    vc()->setWidgetsAlignment(vc()->selectedWidget(), Qt::AlignLeft);
    QCOMPARE(vc()->widget(first)->logicalGeometry().x(), lastX);
    QCOMPARE(vc()->widget(last)->logicalGeometry().x(), lastX);
}

QTEST_MAIN(VCEditor_Test)
