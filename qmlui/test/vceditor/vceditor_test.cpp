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
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QRandomGenerator>
#include <QRegularExpression>
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
#include "monitorproperties.h"
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
    const QString tag = QTest::currentDataTag() ? QString::fromLatin1(QTest::currentDataTag()) : QString("default");
    QFile file(dir.filePath(QString("%1-%2").arg(tag, name)));
    if (file.open(QIODevice::WriteOnly))
        file.write(data);
}

/* An overview page of the screenshots and XML dumps, for review in a browser */
static void writeContactSheet()
{
    const QString root = qEnvironmentVariable("QLC_VC_ARTIFACTS_DIR");
    if (root.isEmpty())
        return;
    QString html = "<!doctype html><html><head><meta charset=\"utf-8\"><title>VC editor artifacts</title><style>"
                   "body{font-family:sans-serif;background:#1d1d1d;color:#ddd;margin:16px}"
                   "section{margin:0 0 24px}figure{display:inline-block;margin:0 8px 8px 0}"
                   "img{width:480px;border:1px solid #555}.bad{color:#ff6b6b}</style></head><body>";
    const QDir dir(root);
    for (const QString &function : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        const QDir rows(dir.filePath(function));
        QMap<QString, QStringList> shots;
        for (const QString &file : rows.entryList({"*.png"}, QDir::Files, QDir::Name))
            if (!file.endsWith("-diff.png"))
                shots[file.left(file.lastIndexOf('-'))].append(file);
        if (shots.isEmpty())
            continue;
        html += QString("<h2>%1</h2>").arg(function);
        for (auto it = shots.constBegin(); it != shots.constEnd(); ++it)
        {
            QFile before(rows.filePath(it.key() + "-before.xml")), undone(rows.filePath(it.key() + "-undone.xml"));
            const bool mismatch = before.open(QIODevice::ReadOnly) && undone.open(QIODevice::ReadOnly) &&
                                  before.readAll() != undone.readAll();
            html += QString("<section><h3>%1%2</h3>").arg(it.key(), mismatch ? " <span class=\"bad\">undo mismatch</span>" : "");
            for (const QString &file : it.value())
                html += QString("<figure><img src=\"%1/%2\"><figcaption>%3</figcaption></figure>")
                            .arg(function, file, file.mid(it.key().size() + 1).chopped(4));
            html += "</section>";
        }
    }
    QFile index(dir.filePath("index.html"));
    if (index.open(QIODevice::WriteOnly))
        index.write((html + "</body></html>").toUtf8());
}

static QString structureError(VCFrame *frame);

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
        {"deleteFilledPage", [](VirtualConsole *vc, Doc *, const ConsoleIds &) {
            vc->deletePage(1);
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
    // offscreen windows take their size only once shown
    m_app->resize(1600, 950);
    QTRY_COMPARE(m_app->size(), QSize(1600, 950));
    auto *contexts = qobject_cast<ContextManager *>(
        m_app->rootContext()->contextProperty("contextManager").value<QObject *>());
    QVERIFY(contexts);
    contexts->switchToContext("VC");
}

void VCEditor_Test::init()
{
    // New keeps the page count of the previous workspace
    while (vc()->pagesCount() > 4)
        vc()->deletePage(vc()->pagesCount() - 1);
    while (vc()->pagesCount() < 4)
        vc()->addPage(vc()->pagesCount());
    QVERIFY(m_app->newWorkspace());
    m_app->doc()->masterTimer()->stop();
    // a new workspace asks for the 2D point of view, and rejected edits of earlier
    // rows leave error dialogs: both would cover the console and block its keys
    m_app->doc()->monitorProperties()->setPointOfView(MonitorProperties::FrontView);
    for (QObject *object : m_app->rootObject()->findChildren<QObject *>())
        if (object->inherits("QQuickPopup") && object->property("visible").toBool())
            QMetaObject::invokeMethod(object, "close");
    QTRY_COMPARE(m_app->rootObject()->property("popupCount").toInt(), 0);
    vc()->setSelectedPage(0);
    vc()->setEditMode(true);
    vc()->setSnapping(false);
}

void VCEditor_Test::cleanupTestCase()
{
    writeContactSheet();
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
    ids.onPage2 = create(vc()->page(1)->id(), "Button", QRectF(300, 20, 100, 80));
    vc()->resetWidgetSelection();
    settleHistory();
    return ids;
}

void VCEditor_Test::checkScreenshot(const QString &name)
{
    const QString golden = qEnvironmentVariable("QLC_VC_GOLDEN_DIR");
    if (golden.isEmpty() && qEnvironmentVariable("QLC_VC_ARTIFACTS_DIR").isEmpty())
        return;
    const QImage image = m_app->grabWindow().convertToFormat(QImage::Format_RGB32);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    saveArtifact(name + ".png", png);
    if (golden.isEmpty())
        return;

    const QString file = QDir(golden).filePath(QString("%1-%2-%3.png").arg(QString::fromLatin1(QTest::currentTestFunction()),
                                                                           QString::fromLatin1(QTest::currentDataTag()), name));
    if (qEnvironmentVariableIntValue("QLC_VC_UPDATE_GOLDEN"))
    {
        QDir().mkpath(golden);
        QVERIFY(image.save(file));
        return;
    }
    const QImage reference = QImage(file).convertToFormat(QImage::Format_RGB32);
    QVERIFY2(!reference.isNull(), qPrintable("No reference screenshot " + file));
    QCOMPARE(image.size(), reference.size());
    // the channel tolerance absorbs antialiasing, the pixel budget tiny text shifts
    QImage diff(image.size(), QImage::Format_RGB32);
    diff.fill(Qt::black);
    int different = 0;
    for (int y = 0; y < image.height(); y++)
        for (int x = 0; x < image.width(); x++)
        {
            const QRgb a = image.pixel(x, y), b = reference.pixel(x, y);
            if (qAbs(qRed(a) - qRed(b)) > 16 || qAbs(qGreen(a) - qGreen(b)) > 16 || qAbs(qBlue(a) - qBlue(b)) > 16)
            {
                diff.setPixel(x, y, qRgb(255, 0, 0));
                different++;
            }
        }
    if (different > image.width() * image.height() / 10000)
    {
        QByteArray diffPng;
        QBuffer diffBuffer(&diffPng);
        diffBuffer.open(QIODevice::WriteOnly);
        diff.save(&diffBuffer, "PNG");
        saveArtifact(name + "-diff.png", diffPng);
        QFAIL(qPrintable(QString("Screenshot %1 differs from %2 in %3 pixels").arg(name, file).arg(different)));
    }
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
    checkScreenshot("before");

    QSignalSpy rejected(vc(), &VirtualConsole::editRejected);
    const QString error = editError(consoleEdits().value(edit)(vc(), m_app->doc(), ids));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY2(rejected.isEmpty(), qPrintable(rejected.value(0).value(0).toString()));
    vc()->resetWidgetSelection();
    settleHistory();
    const QByteArray after = consoleXML();
    saveArtifact("before.xml", before);
    saveArtifact("after.xml", after);
    checkScreenshot("after");
    QVERIFY2(after != before, "The edit changed nothing");

    Tardis::instance()->undoAction();
    settleHistory();
    QByteArray actual = consoleXML();
    saveArtifact("undone.xml", actual);
    checkScreenshot("undone");
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

void VCEditor_Test::clonesStayOnTheirPage()
{
    const ConsoleIds ids = buildConsole();
    auto *frame = qobject_cast<VCFrame *>(vc()->widget(ids.frame));
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(2);

    frame->cloneFirstPage();
    QCOMPARE(frame->orderedChildIDs(1).size(), frame->orderedChildIDs(0).size());
    for (VCWidget *child : frame->children(false))
        QCOMPARE(child->isVisible(), child->page() == frame->currentPage());
}

/*********************************************************************
 * Random edit sequences: rejected edits change nothing, accepted ones
 * keep the console well formed, and every step undoes and redoes
 *********************************************************************/

static QString rectText(const QRectF &rect)
{
    return QString("%1,%2 %3x%4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}

/* Overlapping siblings or children outside their frame content */
static QString structureError(VCFrame *frame)
{
    const QRectF content = frame->contentRect();
    const QList<VCWidget *> children = frame->children(false);
    for (int i = 0; i < children.size(); i++)
    {
        const QRectF rect = children[i]->logicalGeometry();
        // geometry carries the header height measured in QML, compare with a tolerance
        if (!qobject_cast<VCPage *>(frame) && !content.adjusted(-0.01, -0.01, 0.01, 0.01).contains(rect))
            return QString("Widget %1 (%2) lies outside frame %3 content (%4)").arg(children[i]->id())
                .arg(rectText(rect), QString::number(frame->id()), rectText(content));
        for (int j = i + 1; j < children.size(); j++)
        {
            const QRectF overlap = rect.intersected(children[j]->logicalGeometry());
            if (children[i]->page() == children[j]->page() && overlap.width() > 0.01 && overlap.height() > 0.01)
                return QString("Widgets %1 and %2 overlap").arg(children[i]->id()).arg(children[j]->id());
        }
        if (VCFrame *child = qobject_cast<VCFrame *>(children[i]))
        {
            const QString error = structureError(child);
            if (!error.isEmpty())
                return error;
        }
    }
    return QString();
}

void VCEditor_Test::randomEditSequence_data()
{
    QTest::addColumn<quint32>("seed");
    const int seeds = qEnvironmentVariableIsSet("QLC_VC_FUZZ_SEEDS") ? qEnvironmentVariableIntValue("QLC_VC_FUZZ_SEEDS") : 4;
    for (int seed = 1; seed <= seeds; seed++)
        QTest::newRow(qPrintable(QString("seed-%1").arg(seed))) << quint32(seed);
}

void VCEditor_Test::randomEditSequence()
{
    QFETCH(quint32, seed);
    buildConsole();
    QRandomGenerator rng(seed);
    VCFrame *page = vc()->page(0);
    auto pick = [&](const QList<VCWidget *> &list) { return list.at(rng.bounded(int(list.size()))); };
    auto frames = [&]() {
        QList<VCWidget *> list {page};
        for (VCWidget *widget : page->children(true))
            if (qobject_cast<VCFrame *>(widget))
                list.append(widget);
        return list;
    };
    auto delta = [&](int range) { return qreal(rng.bounded(2 * range + 1) - range); };

    /* Each edit describes itself and returns the edit result, if it has one */
    const QList<std::function<QVariantMap(QString *)>> edits {
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            *what = QString("move %1").arg(w->id());
            return vc()->editGeometry(w->id(), w->logicalGeometry().translated(delta(80), delta(80)), false);
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            QRectF rect = w->logicalGeometry();
            rect.setSize(rect.size() + QSizeF(delta(40), delta(40)));
            *what = QString("resize %1").arg(w->id());
            return vc()->editGeometry(w->id(), rect, false);
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            *what = QString("delete %1").arg(w->id());
            return vc()->deleteAuthoringWidgets({w->id()});
        },
        [&](QString *what) {
            auto *frame = qobject_cast<VCFrame *>(pick(frames()));
            const QStringList types {"Button", "Slider", "Label", "Frame"};
            const QString type = types.at(rng.bounded(int(types.size())));
            const QRectF content = frame->contentRect();
            const QPointF at(content.x() + rng.bounded(qMax(1, int(content.width()))),
                             content.y() + rng.bounded(qMax(1, int(content.height()))));
            *what = QString("create %1 in %2").arg(type).arg(frame->id());
            return vc()->createWidgetGeometry(frame->id(), type, QRectF(at, QSizeF(90, 70)));
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            auto *frame = qobject_cast<VCFrame *>(pick(frames()));
            const QRectF content = frame->contentRect();
            const QPointF at(content.x() + rng.bounded(qMax(1, int(content.width()))),
                             content.y() + rng.bounded(qMax(1, int(content.height()))));
            *what = QString("drop %1 into %2").arg(w->id()).arg(frame->id());
            return vc()->editDrop(w->id(), frame->id(), frame->currentPage(),
                                  QRectF(at, w->logicalGeometry().size()));
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            *what = QString("nudge %1").arg(w->id());
            vc()->setSelectedWidgetIDs({w->id()});
            vc()->nudgeWidgets(int(delta(3)), int(delta(3)));
            return QVariantMap();
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            VCWidget *frame = pick(frames());
            *what = QString("paste %1 into %2").arg(w->id()).arg(frame->id());
            vc()->setSelectedWidgetIDs({w->id()});
            vc()->copyToClipboard();
            vc()->setSelectedWidgetIDs({frame->id()});
            vc()->pasteFromClipboard();
            return QVariantMap();
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            *what = QString("caption %1").arg(w->id());
            vc()->setSelectedWidgetIDs({w->id()});
            vc()->setWidgetsCaption(QString("Caption %1").arg(rng.bounded(1000)));
            return QVariantMap();
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            auto *parent = qobject_cast<VCFrame *>(w->parent());
            const int index = rng.bounded(int(parent->orderedChildIDs(w->page()).size()));
            *what = QString("reorder %1 to %2").arg(w->id()).arg(index);
            return vc()->editStructure({w->id()}, parent->id(), w->page(), index);
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            auto *parent = qobject_cast<VCFrame *>(w->parent());
            VCWidget *other = pick(parent->children(false));
            const QList<int> alignments {Qt::AlignLeft, Qt::AlignRight, Qt::AlignTop, Qt::AlignBottom};
            *what = QString("align %1 to %2").arg(other->id()).arg(w->id());
            vc()->setSelectedWidgetIDs({other->id(), w->id()});
            vc()->setWidgetsAlignment(w, alignments.at(rng.bounded(int(alignments.size()))));
            return QVariantMap();
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            *what = QString("duplicate %1").arg(w->id());
            vc()->setSelectedWidgetIDs({w->id()});
            vc()->duplicateSelection();
            return QVariantMap();
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            VCWidget *frame = pick(frames());
            *what = QString("cut %1 into %2").arg(w->id()).arg(frame->id());
            vc()->setSelectedWidgetIDs({w->id()});
            vc()->cutToClipboard();
            vc()->setSelectedWidgetIDs({frame->id()});
            vc()->pasteFromClipboard();
            return QVariantMap();
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            auto *parent = qobject_cast<VCFrame *>(w->parent());
            VCWidget *other = pick(parent->children(false));
            auto *frame = qobject_cast<VCFrame *>(pick(frames()));
            const QRectF content = frame->contentRect();
            const QPointF at(content.x() + rng.bounded(qMax(1, int(content.width()))),
                             content.y() + rng.bounded(qMax(1, int(content.height()))));
            *what = QString("drop %1+%2 into %3").arg(w->id()).arg(other->id()).arg(frame->id());
            return vc()->editSelectionDrop({w->id(), other->id()}, w->id(), frame->id(), frame->currentPage(), at);
        },
        [&](QString *what) {
            QList<VCWidget *> list = frames();
            list.removeAll(page);
            if (list.isEmpty())
                return QVariantMap();
            auto *frame = qobject_cast<VCFrame *>(pick(list));
            *what = QString("collapse %1").arg(frame->id());
            frame->setCollapsed(!frame->isCollapsed());
            return QVariantMap();
        },
        [&](QString *what) {
            VCWidget *w = pick(page->children(true));
            QVariantList siblings;
            for (VCWidget *child : qobject_cast<VCFrame *>(w->parent())->children(false))
                if (child->page() == w->page())
                    siblings.append(child->id());
            *what = QString("distribute around %1").arg(w->id());
            vc()->setSelectedWidgetIDs(siblings);
            vc()->setWidgetsDistribution(rng.bounded(2) ? Qt::Horizontal : Qt::Vertical);
            return QVariantMap();
        },
    };

    QList<QByteArray> states {consoleXML()};
    QStringList steps;
    const int count = 12;
    for (int i = 0; i < count && !page->children(true).isEmpty(); i++)
    {
        QString what;
        QSignalSpy rejected(vc(), &VirtualConsole::editRejected);
        const QString error = editError(edits.at(rng.bounded(int(edits.size())))(&what));
        vc()->resetWidgetSelection();
        settleHistory();
        const QByteArray xml = consoleXML();
        const QString trail = QString("seed %1: %2").arg(seed).arg((steps + QStringList{what}).join(", "));
        if (!error.isEmpty() || !rejected.isEmpty())
        {
            QVERIFY2(xml == states.last(), qPrintable("Rejected edit changed the console, " + trail + "\n" +
                                                      firstDifference(states.last(), xml)));
            continue;
        }
        const QString structure = structureError(page);
        QVERIFY2(structure.isEmpty(), qPrintable(structure + ", " + trail));
        if (xml != states.last())
        {
            states.append(xml);
            steps.append(what);
        }
    }

    qInfo() << "seed" << seed << "accepted" << steps;
    for (int i = states.size() - 1; i > 0; i--)
    {
        Tardis::instance()->undoAction();
        settleHistory();
        const QByteArray xml = consoleXML();
        QVERIFY2(xml == states.at(i - 1), qPrintable(QString("Undo of '%1', seed %2: %3\n").arg(steps.at(i - 1)).arg(seed)
                                                      .arg(steps.join(", ")) + firstDifference(states.at(i - 1), xml)));
    }
    for (int i = 1; i < states.size(); i++)
    {
        Tardis::instance()->redoAction();
        settleHistory();
        const QByteArray xml = consoleXML();
        QVERIFY2(xml == states.at(i), qPrintable(QString("Redo of '%1', seed %2: %3\n").arg(steps.at(i - 1)).arg(seed)
                                                  .arg(steps.join(", ")) + firstDifference(states.at(i), xml)));
    }
}

void VCEditor_Test::framePageVisibility_data()
{
    QTest::addColumn<QString>("change");
    QTest::addColumn<int>("shownPage");
    QTest::newRow("fewer-pages") << QString("fewer-pages") << 1;
    QTest::newRow("multipage-off") << QString("multipage-off") << 0;
}

void VCEditor_Test::framePageVisibility()
{
    QFETCH(QString, change);
    QFETCH(int, shownPage);
    const ConsoleIds ids = buildConsole();
    auto *frame = qobject_cast<VCFrame *>(vc()->widget(ids.frame));
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(3);
    for (int subpage : {1, 2})
        QVERIFY(vc()->createWidgetGeometry(ids.frame, "Button", QRectF(10, 60, 100, 80), subpage)
                    .value("accepted").toBool());
    frame->setCurrentPage(2);

    if (change == "fewer-pages")
        frame->setTotalPagesNumber(2);
    else
        frame->setMultiPageMode(false);
    QCOMPARE(frame->currentPage(), shownPage);
    for (VCWidget *child : frame->children(false))
        QCOMPARE(child->isVisible(), child->page() == shownPage);
}

/*********************************************************************
 * Property panels
 *********************************************************************/

void VCEditor_Test::multiSelectionPanel()
{
    QTest::failOnWarning(QRegularExpression("VCWidgetProperties"));
    const ConsoleIds ids = buildConsole();
    auto *console = m_app->rootObject()->findChild<QQuickItem *>("virtualConsole");
    QVERIFY(console);
    QQmlExpression panel(qmlContext(console), console, "rightSidePanel");
    QObject *rightPanel = panel.evaluate().value<QObject *>();
    QVERIFY2(rightPanel, qPrintable(panel.error().toString()));
    QTRY_COMPARE(rightPanel->property("loaderSource").toString(), QString("qrc:/VCWidgetProperties.qml"));

    vc()->setSelectedWidgetIDs({ids.loose, ids.empty, ids.frame});
    QTest::qWait(100);
    checkScreenshot("panel");
}

void VCEditor_Test::frameCheckboxes_data()
{
    QTest::addColumn<QString>("property");
    QTest::addColumn<QString>("type");
    for (const QString &property : {"showHeader", "showEnable", "multiPageMode", "pagesLoop"})
        QTest::newRow(qPrintable(property)) << property << QString("Frame");
    QTest::newRow("excludeMonitoredFunctions") << QString("excludeMonitoredFunctions") << QString("Solo frame");
}

void VCEditor_Test::frameCheckboxes()
{
    QFETCH(QString, property);
    QFETCH(QString, type);
    const QVariantMap created = vc()->createWidgetGeometry(vc()->page(0)->id(), type, QRectF(20, 20, 300, 200));
    QVERIFY(created.value("accepted").toBool());
    VCWidget *frame = vc()->widget(created.value("widgetIDs").toList().first().toUInt());
    QQmlComponent component(m_app->engine(), QUrl("qrc:/VCFrameProperties.qml"));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties(
        {{"widgetRef", QVariant::fromValue(frame)}}, qmlContext(m_app->rootObject())));
    QVERIFY2(panel, qPrintable(component.errorString()));
    QObject *box = panel->findChild<QObject *>(property + "Check");
    QVERIFY(box);

    for (bool value : {true, false, true})
    {
        // a click toggles the box, the frame follows
        QVERIFY(box->setProperty("checked", value));
        QCOMPARE(frame->property(qPrintable(property)).toBool(), value);
    }
}

/*********************************************************************
 * Selection and keyboard editing
 *********************************************************************/

void VCEditor_Test::alignCenters_data()
{
    QTest::addColumn<int>("alignment");
    QTest::addColumn<QRectF>("expected");
    // the loose button follows the center of the empty frame (750,200 300x250)
    QTest::newRow("horizontal") << int(Qt::AlignHCenter) << QRectF(850, 20, 100, 80);
    QTest::newRow("vertical") << int(Qt::AlignVCenter) << QRectF(600, 285, 100, 80);
}

void VCEditor_Test::alignCenters()
{
    QFETCH(int, alignment);
    QFETCH(QRectF, expected);
    const ConsoleIds ids = buildConsole();
    vc()->setWidgetSelection(ids.loose, nullptr, true, true);
    vc()->setWidgetSelection(ids.empty, nullptr, true, true);

    vc()->setWidgetsAlignment(vc()->selectedWidget(), alignment);
    QCOMPARE(vc()->widget(ids.loose)->logicalGeometry(), expected);
}

void VCEditor_Test::matchSize_data()
{
    QTest::addColumn<int>("orientations");
    QTest::addColumn<QRectF>("expected");
    // the empty frame (750,200 300x250) takes the size of the loose button (100x80)
    QTest::newRow("width") << int(Qt::Horizontal) << QRectF(750, 200, 100, 250);
    QTest::newRow("height") << int(Qt::Vertical) << QRectF(750, 200, 300, 80);
    QTest::newRow("both") << int(Qt::Horizontal | Qt::Vertical) << QRectF(750, 200, 100, 80);
}

void VCEditor_Test::matchSize()
{
    QFETCH(int, orientations);
    QFETCH(QRectF, expected);
    const ConsoleIds ids = buildConsole();
    vc()->setWidgetSelection(ids.empty, nullptr, true, true);
    vc()->setWidgetSelection(ids.loose, nullptr, true, true);

    vc()->setWidgetsSize(vc()->selectedWidget(), orientations);
    QCOMPARE(vc()->widget(ids.empty)->logicalGeometry(), expected);
    QCOMPARE(vc()->widget(ids.loose)->logicalGeometry(), QRectF(600, 20, 100, 80));
}

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

void VCEditor_Test::widgetsInRect_data()
{
    QTest::addColumn<QString>("container");
    QTest::addColumn<QRectF>("band");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("page-band") << "page" << QRectF(500, 0, 150, 60) << QStringList{"frame", "loose"};
    QTest::newRow("page-empty") << "page" << QRectF(1300, 600, 100, 100) << QStringList();
    QTest::newRow("page-edge-touch") << "page" << QRectF(520, 300, 30, 30) << QStringList();
    QTest::newRow("frame-band") << "frame" << QRectF(0, 50, 250, 50) << QStringList{"a", "b"};
    QTest::newRow("frame-nested") << "frame" << QRectF(200, 300, 20, 20) << QStringList{"nested"};
}

void VCEditor_Test::widgetsInRect()
{
    QFETCH(QString, container);
    QFETCH(QRectF, band);
    QFETCH(QStringList, expected);
    const ConsoleIds ids = buildConsole();
    const QMap<QString, quint32> named {{"page", ids.page}, {"frame", ids.frame}, {"loose", ids.loose},
                                        {"a", ids.a}, {"b", ids.b}, {"nested", ids.nested}};
    QVariantList wanted;
    for (const QString &name : expected)
        wanted.append(named.value(name));

    QCOMPARE(vc()->widgetsInRect(named.value(container), band), wanted);
}

void VCEditor_Test::bandSelection_data()
{
    QTest::addColumn<int>("modifiers");
    QTest::addColumn<QStringList>("selected");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("shift") << int(Qt::ShiftModifier) << QStringList{"a"} << QStringList{"frame", "empty", "loose"};
    QTest::newRow("ctrl-shift-adds") << int(Qt::ShiftModifier | Qt::ControlModifier) << QStringList{"a"}
                                     << QStringList{"a", "frame", "empty", "loose"};
    QTest::newRow("plain-drag-pans") << int(Qt::NoModifier) << QStringList{"a"} << QStringList{"a"};
}

void VCEditor_Test::bandSelection()
{
    QFETCH(int, modifiers);
    QFETCH(QStringList, selected);
    QFETCH(QStringList, expected);
    const ConsoleIds ids = buildConsole();
    const QMap<QString, quint32> named {{"frame", ids.frame}, {"empty", ids.empty}, {"loose", ids.loose}, {"a", ids.a}};
    auto idsOf = [&](const QStringList &names) {
        QVariantList list;
        for (const QString &name : names)
            list.append(named.value(name));
        std::sort(list.begin(), list.end(), [](const QVariant &x, const QVariant &y) { return x.toUInt() < y.toUInt(); });
        return list;
    };
    QQuickItem *pageItem = vc()->page(0)->renderItem();
    QVERIFY(pageItem);
    const qreal scale = vc()->page(0)->pageScale();
    auto windowPoint = [&](const QPointF &logical) { return pageItem->mapToScene(logical * scale).toPoint(); };
    vc()->setSelectedWidgetIDs(idsOf(selected));

    // from empty canvas, up and left over the frame, the empty frame and the loose button
    const QPoint from = windowPoint(QPointF(1100, 600)), to = windowPoint(QPointF(500, 10));
    QTest::mousePress(m_app.get(), Qt::LeftButton, Qt::KeyboardModifiers(modifiers), from);
    for (int step = 1; step <= 10; step++)
        QTest::mouseMove(m_app.get(), from + (to - from) * step / 10);
    QTest::mouseRelease(m_app.get(), Qt::LeftButton, Qt::KeyboardModifiers(modifiers), to);
    QVariantList actual = vc()->selectedWidgetIDs();
    std::sort(actual.begin(), actual.end(), [](const QVariant &x, const QVariant &y) { return x.toUInt() < y.toUInt(); });
    QCOMPARE(actual, idsOf(expected));
}

void VCEditor_Test::keysIgnoredWhileTyping_data()
{
    QTest::addColumn<int>("key");
    QTest::newRow("up") << int(Qt::Key_Up);
    QTest::newRow("down") << int(Qt::Key_Down);
    QTest::newRow("left") << int(Qt::Key_Left);
    QTest::newRow("right") << int(Qt::Key_Right);
    QTest::newRow("delete") << int(Qt::Key_Delete);
    QTest::newRow("backspace") << int(Qt::Key_Backspace);
}

void VCEditor_Test::keysIgnoredWhileTyping()
{
    QFETCH(int, key);
    const ConsoleIds ids = buildConsole();
    auto *console = m_app->rootObject()->findChild<QQuickItem *>("virtualConsole");
    QVERIFY(console);
    // Stands in for the property fields, which live inside the console too
    QQmlComponent component(m_app->engine());
    component.setData("import QtQuick; TextInput { width: 100; height: 20 }", QUrl());
    std::unique_ptr<QQuickItem> field(qobject_cast<QQuickItem *>(component.create(qmlContext(console))));
    QVERIFY2(field, qPrintable(component.errorString()));
    field->setParentItem(console);
    m_app->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(m_app.get()));
    vc()->setSelectedWidgetIDs({ids.loose});
    field->forceActiveFocus();
    QTRY_VERIFY(field->hasActiveFocus());
    const QRectF before = vc()->widget(ids.loose)->logicalGeometry();

    QTest::keyClick(m_app.get(), Qt::Key(key));
    QVERIFY(vc()->widget(ids.loose));
    QCOMPARE(vc()->widget(ids.loose)->logicalGeometry(), before);
}

void VCEditor_Test::pasteRendersIntoFrame()
{
    const ConsoleIds ids = buildConsole();
    auto *frame = qobject_cast<VCFrame *>(vc()->widget(ids.empty));
    QTRY_VERIFY(frame->renderItem());
    vc()->setSelectedWidgetIDs({ids.loose});
    vc()->copyToClipboard();
    vc()->setSelectedWidgetIDs({ids.empty});

    vc()->pasteFromClipboard();
    QCOMPARE(frame->children(false).size(), 1);
    VCWidget *copy = frame->children(false).first();
    QTRY_VERIFY(copy->renderItem());
    // only the drop area keeps the copy above the frame chrome, so it can be clicked
    QCOMPARE(copy->renderItem()->parentItem()->objectName(), QString("frameDropArea%1").arg(ids.empty));
}

void VCEditor_Test::pastePlacement_data()
{
    QTest::addColumn<QString>("edit");
    QTest::addColumn<QString>("source");
    QTest::addColumn<QString>("target");
    QTest::addColumn<QRectF>("expected"); // empty: anywhere free
    QTest::newRow("paste-page") << "paste" << "loose" << "page" << QRectF();
    QTest::newRow("paste-frame") << "paste" << "a" << "frame" << QRectF();
    QTest::newRow("duplicate-page") << "duplicate" << "loose" << "page" << QRectF(710, 20, 100, 80);
    QTest::newRow("duplicate-frame-child") << "duplicate" << "a" << "frame" << QRectF();
    QTest::newRow("duplicate-frame") << "duplicate" << "empty" << "page" << QRectF();
}

void VCEditor_Test::pastePlacement()
{
    QFETCH(QString, edit);
    QFETCH(QString, source);
    QFETCH(QString, target);
    QFETCH(QRectF, expected);
    const ConsoleIds ids = buildConsole();
    const QMap<QString, quint32> named {{"loose", ids.loose}, {"a", ids.a}, {"empty", ids.empty},
                                        {"frame", ids.frame}, {"page", ids.page}};
    auto *parent = qobject_cast<VCFrame *>(vc()->widget(named.value(target)));
    const QList<VCWidget *> before = parent->children(false);
    QSignalSpy rejected(vc(), &VirtualConsole::editRejected);

    vc()->setSelectedWidgetIDs({named.value(source)});
    if (edit == "paste")
    {
        vc()->copyToClipboard();
        vc()->setSelectedWidgetIDs(target == "page" ? QVariantList() : QVariantList{named.value(target)});
        vc()->pasteFromClipboard();
    }
    else
    {
        vc()->duplicateSelection();
    }
    QVERIFY2(rejected.isEmpty(), qPrintable(rejected.value(0).value(0).toString()));
    QList<VCWidget *> added = parent->children(false);
    for (VCWidget *widget : before)
        added.removeAll(widget);
    QCOMPARE(added.size(), 1);
    QCOMPARE(added.first()->caption(), vc()->widget(named.value(source))->caption());
    if (!expected.isEmpty())
        QCOMPARE(added.first()->logicalGeometry(), expected);
    QCOMPARE(vc()->selectedWidgetIDs(), QVariantList{added.first()->id()});
    const QString structure = structureError(vc()->page(0));
    QVERIFY2(structure.isEmpty(), qPrintable(structure));
}

void VCEditor_Test::duplicateKeepsClipboard()
{
    const ConsoleIds ids = buildConsole();
    vc()->setSelectedWidgetIDs({ids.a});
    vc()->copyToClipboard();
    vc()->setSelectedWidgetIDs({ids.loose});

    vc()->duplicateSelection();
    QCOMPARE(vc()->clipboardItemsList().size(), 1);
    QCOMPARE(vc()->clipboardItemsList().first().toUInt(), ids.a);
}

void VCEditor_Test::newWorkspaceShowsFirstPage()
{
    vc()->setSelectedPage(1);
    QTRY_VERIFY(m_app->rootObject()->findChild<QQuickItem *>("vcPage1"));

    QVERIFY(m_app->newWorkspace());
    QCOMPARE(vc()->selectedPage(), 0);
    QTRY_VERIFY(m_app->rootObject()->findChild<QQuickItem *>("vcPage1") == nullptr);
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
