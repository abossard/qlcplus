#include <QtTest>
#include <QQmlContext>
#include <QQuickItem>
#include <QSettings>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DLogic/QFrameAction>
#include <Qt3DRender/QLayer>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QSceneLoader>
#include <Qt3DRender/QRenderTarget>
#include <Qt3DRender/QPaintedTextureImage>

#include "upstream3d_test.h"
#include "app.h"
#include "contextmanager.h"
#include "doc.h"
#include "fixture.h"
#include "fixtureutils.h"
#include "mastertimer.h"
#include "monitorproperties.h"
#include "previewcontext.h"
#include "tardis.h"

#define private public
#include "mainview3d.h"
#undef private

class FrameGraphReceiver : public QQuickItem
{
    Q_OBJECT

public:
    QStringList events;
    QList<bool> rebuilds;

    Q_INVOKABLE void updateFrameGraph(QVariant rebuild)
    {
        rebuilds.append(rebuild.toBool());
        events.append("rebuild");
    }
};

void Upstream3D_Test::initTestCase()
{
    static QTemporaryDir settings;
    QVERIFY(settings.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settings.path());
    QCoreApplication::setOrganizationName("QLCPlusIntegrationTests");
    QCoreApplication::setApplicationName("Upstream3D");
    QVERIFY(QSettings().fileName().startsWith(settings.path() + "/"));
    m_app = new App;
    m_app->set3dSupported(false);
    m_app->startup();
    m_app->doc()->masterTimer()->stop();
    QVERIFY2(m_app->status() == QQuickView::Ready, qPrintable(m_app->errors().value(0).toString()));
    m_context = m_app->rootContext()->contextProperty("contextManager").value<ContextManager *>();
    m_view = m_app->rootContext()->contextProperty("View3D").value<MainView3D *>();
    QVERIFY(m_context);
    QVERIFY(m_view);
    // Exercise the production setters without starting the renderer or mesh loader.
    m_view->PreviewContext::enableContext(true);
}

void Upstream3D_Test::cleanupTestCase()
{
    delete m_app;
}

void Upstream3D_Test::positionDeltas_data()
{
    QTest::addColumn<int>("fixtures");
    QTest::addColumn<int>("generics");
    QTest::addColumn<bool>("lockFirst");
    QTest::newRow("single-fixture") << 1 << 0 << false;
    QTest::newRow("single-generic") << 0 << 1 << false;
    QTest::newRow("mixed-single") << 1 << 1 << false;
    QTest::newRow("mixed-unequal") << 2 << 1 << false;
    QTest::newRow("mixed-pairs") << 2 << 2 << false;
    QTest::newRow("locked-single") << 1 << 1 << true;
    QTest::newRow("locked-first-unlocked-later") << 2 << 2 << true;
}

void Upstream3D_Test::positionDeltas()
{
    QFETCH(int, fixtures);
    QFETCH(int, generics);
    QFETCH(bool, lockFirst);
    const QVector3D positions[] = {{1100, 2300, -700}, {-400, 5100, 900}};
    const QVector3D delta(125, -250, 375);
    Doc *doc = m_app->doc();
    MonitorProperties *props = doc->monitorProperties();
    QList<quint32> fixtureIDs;
    const auto cleanup = qScopeGuard([&]() {
        m_context->resetFixtureSelection();
        for (quint32 id : fixtureIDs)
            doc->deleteFixture(id);
        for (int i = 0; i < generics; ++i)
        {
            m_view->setItemSelection(100 + i, false, 0);
            props->removeItem(100 + i);
        }
        Tardis::instance()->resetHistory();
    });
    for (int i = 0; i < fixtures; ++i)
    {
        auto *fixture = new Fixture(doc);
        fixture->setChannels(1);
        fixture->setAddress(i);
        QVERIFY(doc->addFixture(fixture));
        const quint32 id = fixture->id();
        fixtureIDs.append(id);
        props->setFixturePosition(id, 0, 0, positions[i]);
        props->setFixtureFlags(id, 0, 0, lockFirst && i == 0 ? MonitorProperties::LockedFlag : 0);
        m_context->setFixtureSelection(FixtureUtils::fixtureItemID(id, 0, 0), -1, true);
    }
    for (int i = 0; i < generics; ++i)
    {
        props->setItemPosition(100 + i, positions[1 - i]);
        props->setItemFlags(100 + i, lockFirst && i == 0 ? MonitorProperties::LockedFlag : 0);
        m_view->setItemSelection(100 + i, true, Qt::ControlModifier);
    }
    QCOMPARE(m_context->selectedFixturesCount(), fixtures);
    QCOMPARE(m_view->genericSelectedCount(), generics);
    for (int step = 1; step <= 2; ++step)
    {
        m_context->setFixturesPosition(delta);
        m_view->setGenericItemsPosition(delta);
        for (int i = 0; i < fixtures; ++i)
            QCOMPARE(props->fixturePosition(fixtureIDs[i], 0, 0),
                     positions[i] + (lockFirst && i == 0 ? QVector3D() : delta * step));
        for (int i = 0; i < generics; ++i)
            QCOMPARE(props->itemPosition(100 + i),
                     positions[1 - i] + (lockFirst && i == 0 ? QVector3D() : delta * step));
    }
}

void Upstream3D_Test::rotationDeltas_data()
{
    QTest::addColumn<int>("fixtures");
    QTest::addColumn<int>("generics");
    QTest::newRow("single-fixture") << 1 << 0;
    QTest::newRow("single-generic") << 0 << 1;
    QTest::newRow("mixed-single") << 1 << 1;
    QTest::newRow("mixed-pairs") << 2 << 2;
}

void Upstream3D_Test::rotationDeltas()
{
    QFETCH(int, fixtures);
    QFETCH(int, generics);
    const QVector3D rotations[] = {{350, 5, 355}, {100, 355, 10}};
    const QVector3D expected[2][2] = {{{10, 345, 10}, {120, 335, 25}},
                                     {{30, 325, 25}, {140, 315, 40}}};
    const QVector3D delta(20, -20, 15);
    Doc *doc = m_app->doc();
    MonitorProperties *props = doc->monitorProperties();
    QList<quint32> fixtureIDs;
    const auto cleanup = qScopeGuard([&]() {
        m_context->resetFixtureSelection();
        for (quint32 id : fixtureIDs)
            doc->deleteFixture(id);
        for (int i = 0; i < generics; ++i)
        {
            m_view->setItemSelection(100 + i, false, 0);
            props->removeItem(100 + i);
        }
        Tardis::instance()->resetHistory();
    });
    for (int i = 0; i < fixtures; ++i)
    {
        auto *fixture = new Fixture(doc);
        fixture->setChannels(1);
        fixture->setAddress(i);
        QVERIFY(doc->addFixture(fixture));
        const quint32 id = fixture->id();
        fixtureIDs.append(id);
        props->setFixtureRotation(id, 0, 0, rotations[i]);
        m_context->setFixtureSelection(FixtureUtils::fixtureItemID(id, 0, 0), -1, true);
    }
    for (int i = 0; i < generics; ++i)
    {
        props->setItemRotation(100 + i, rotations[i]);
        m_view->setItemSelection(100 + i, true, Qt::ControlModifier);
    }
    QCOMPARE(m_context->selectedFixturesCount(), fixtures);
    QCOMPARE(m_view->genericSelectedCount(), generics);
    for (int step = 0; step < 2; ++step)
    {
        m_context->setFixturesRotation(delta);
        m_view->setGenericItemsRotation(delta);
        for (int i = 0; i < fixtures; ++i)
            QCOMPARE(props->fixtureRotation(fixtureIDs[i], 0, 0), expected[step][i]);
        for (int i = 0; i < generics; ++i)
            QCOMPARE(props->itemRotation(100 + i), expected[step][i]);
    }
}

void Upstream3D_Test::renderQuality_data()
{
    QTest::addColumn<bool>("attached");
    QTest::addColumn<bool>("raising");
    QTest::newRow("low-to-high") << true << true;
    QTest::newRow("high-to-low") << true << false;
    QTest::newRow("no-scene-low-to-high") << false << true;
    QTest::newRow("no-scene-high-to-low") << false << false;
}

void Upstream3D_Test::renderQuality()
{
    QFETCH(bool, attached);
    QFETCH(bool, raising);
    FrameGraphReceiver scene;
    const auto initial = raising ? MainView3D::LowQuality : MainView3D::HighQuality;
    const auto target = raising ? MainView3D::HighQuality : MainView3D::LowQuality;
    m_view->setRenderQuality(initial);
    m_view->m_scene3D = attached ? &scene : nullptr;
    const auto cleanup = qScopeGuard([&]() { m_view->m_scene3D = nullptr; });
    connect(m_view, &MainView3D::renderQualityChanged, &scene,
            [&](MainView3D::RenderQuality) { scene.events.append("quality"); });
    QSignalSpy changed(m_view, &MainView3D::renderQualityChanged);

    m_view->setRenderQuality(target);
    QCOMPARE(m_view->renderQuality(), target);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.at(0).at(0).value<MainView3D::RenderQuality>(), target);
    QCOMPARE(scene.events, attached ? QStringList({"quality", "rebuild"}) : QStringList({"quality"}));
    QCOMPARE(scene.rebuilds, attached ? QList<bool>({true}) : QList<bool>());

    m_view->setRenderQuality(target);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(scene.events.size(), attached ? 2 : 1);
    QCOMPARE(scene.rebuilds.size(), attached ? 1 : 0);
}

QTEST_MAIN(Upstream3D_Test)
#include "upstream3d_test.moc"
