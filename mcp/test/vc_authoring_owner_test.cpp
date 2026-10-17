/*
  Q Light Controller Plus
  vc_authoring_owner_test.cpp
*/

#include <QtTest>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QSettings>
#include <QVariantMap>
#include <QScopeGuard>
#include <QUuid>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <algorithm>
#include <limits>

#include <nlohmann/json.hpp>
#include <fastmcpp/tools/manager.hpp>

#include "vc_authoring_owner_test.h"

#include "app.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "universe.h"
#include "ioplugincache.h"
#include "mastertimer.h"
#include "outputpatch.h"
#include "qlcchannel.h"
#include "qlcconfig.h"
#include "qlcinputchannel.h"
#include "qlcinputprofile.h"
#include "qlcinputsource.h"
#include "qlcioplugin.h"
#include "function.h"
#include "show.h"
#include "functionparent.h"
#include "vcbridgev5.h"
#include "virtualconsole.h"
#include "vcbutton.h"
#include "vcanimation.h"
#include "vcframe.h"
#include "vcpage.h"
#include "vcrecordpanel.h"
#include "vcslider.h"
#include "scene.h"
#include "show.h"
#include "showcommandrecorder.h"
#include "tool_registry.h"
#include "vc_tools_common.h"

using Json = nlohmann::json;

namespace {

Json parseInvoke(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

bool containsWidgetId(const Json &pages, int id)
{
    if (!pages.is_array())
        return false;
    for (const Json &page : pages)
    {
        if (!page.is_object() || !page.contains("widgets") || !page["widgets"].is_array())
            continue;
        for (const Json &widget : page["widgets"])
        {
            if (widget.is_object() && widget.value("id", -1) == id)
                return true;
        }
    }
    return false;
}

QString mutateCursorField(const QString &cursor, const QString &field, const QJsonValue &value)
{
    const QByteArray decoded = QByteArray::fromBase64(
        cursor.toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    const QJsonDocument doc = QJsonDocument::fromJson(decoded);
    if (!doc.isObject())
        return cursor;

    QJsonObject object = doc.object();
    object.insert(field, value);
    const QByteArray encoded = QJsonDocument(object).toJson(QJsonDocument::Compact).toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QString::fromLatin1(encoded);
}

bool isGeometryOrBindingUnsupportedError(const QString &error)
{
    const QString lower = error.toLower();
    return lower.contains(QStringLiteral("geometry preparation only")) ||
           lower.contains(QStringLiteral("controller and feedback binding edits are not supported"));
}

QString settingsFormatName(QSettings::Format format)
{
    switch (format)
    {
    case QSettings::NativeFormat:
        return QStringLiteral("NativeFormat");
    case QSettings::IniFormat:
        return QStringLiteral("IniFormat");
    default:
        return QStringLiteral("CustomFormat");
    }
}

QJsonObject settingsFingerprint()
{
    QSettings settings;
    QStringList keys = settings.allKeys();
    std::sort(keys.begin(), keys.end());
    const QByteArray digest = QCryptographicHash::hash(
        keys.join(QLatin1Char('\n')).toUtf8(),
        QCryptographicHash::Sha256).toHex();

    QJsonObject result;
    result.insert(QStringLiteral("backend"), settingsFormatName(settings.format()));
    result.insert(QStringLiteral("fileName"), settings.fileName());
    result.insert(QStringLiteral("keyCount"), int(keys.size()));
    result.insert(QStringLiteral("keyDigestSha256"), QString::fromLatin1(digest));
    return result;
}

QJsonObject nativeOrgAppFingerprint()
{
    QSettings settings(
        QSettings::NativeFormat,
        QSettings::UserScope,
        QCoreApplication::organizationName(),
        QCoreApplication::applicationName());

    QStringList keys = settings.allKeys();
    std::sort(keys.begin(), keys.end());
    const QByteArray digest = QCryptographicHash::hash(
        keys.join(QLatin1Char('\n')).toUtf8(),
        QCryptographicHash::Sha256).toHex();

    QJsonObject result;
    result.insert(QStringLiteral("backend"), settingsFormatName(settings.format()));
    result.insert(QStringLiteral("fileName"), settings.fileName());
    result.insert(QStringLiteral("keyCount"), int(keys.size()));
    result.insert(QStringLiteral("keyDigestSha256"), QString::fromLatin1(digest));
    return result;
}

QStringList pluginCandidates(QDir dir)
{
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QStringLiteral("*%1").arg(KExtPlugin));
    QStringList plugins = dir.exists() ? dir.entryList(QDir::Files, QDir::Name) : QStringList();
    std::sort(plugins.begin(), plugins.end());
    return plugins;
}

QStringList ownerInputProfileCandidates(const QString &fileName)
{
    QStringList candidates;
    candidates << QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../../../resources/inputprofiles/%1").arg(fileName));
    candidates << QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../../../../resources/inputprofiles/%1").arg(fileName));
#ifdef MCP_AUTHORING_SOURCE_DIR
    candidates << QDir(QStringLiteral(MCP_AUTHORING_SOURCE_DIR))
        .absoluteFilePath(QStringLiteral("resources/inputprofiles/%1").arg(fileName));
#endif
    return candidates;
}

QString resolveExistingPath(const QStringList &candidates)
{
    for (const QString &candidate : candidates)
    {
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QString();
}

QJsonArray toJsonArray(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values)
        array.append(value);
    return array;
}

bool removePluginsFromDir(const QDir &dir, QString *error)
{
    const QStringList plugins = pluginCandidates(dir);
    for (const QString &plugin : plugins)
    {
        const QString absolute = dir.absoluteFilePath(plugin);
        if (!QFile::remove(absolute))
        {
            if (error)
                *error = QStringLiteral("Failed to remove plugin %1").arg(absolute);
            return false;
        }
    }
    return true;
}

bool copyPlugins(const QDir &sourceDir, const QDir &targetDir, QString *error)
{
    const QStringList sourceFiles = pluginCandidates(sourceDir);
    if (sourceFiles.isEmpty())
    {
        if (error)
            *error = QStringLiteral("No probe plugins found in %1").arg(sourceDir.absolutePath());
        return false;
    }

    for (const QString &plugin : sourceFiles)
    {
        const QString sourcePath = sourceDir.absoluteFilePath(plugin);
        const QString targetPath = targetDir.absoluteFilePath(plugin);
        QFile::remove(targetPath);
        if (!QFile::copy(sourcePath, targetPath))
        {
            if (error)
                *error = QStringLiteral("Failed to copy %1 to %2").arg(sourcePath, targetPath);
            return false;
        }
    }
    return true;
}

bool writeJson(const QString &path, const QJsonObject &payload, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(payload).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

} // namespace

VCAuthoringOwner_Test::~VCAuthoringOwner_Test() = default;

void VCAuthoringOwner_Test::initTestCase()
{
    QVERIFY2(m_settingsDir.isValid(), "Failed to create temporary settings directory");
    QVERIFY2(m_workspaceDir.isValid(), "Failed to create temporary workspace directory");
    QVERIFY2(m_runtimeCwdDir.isValid(), "Failed to create temporary runtime CWD directory");

    QCoreApplication::setOrganizationName(QStringLiteral("qlcplus"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qlcplus.org"));
    QCoreApplication::setApplicationName(QStringLiteral(APPNAME));
    const QJsonObject originalSettings = settingsFingerprint();
    const QJsonObject originalNativeOrgApp = nativeOrgAppFingerprint();

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, m_settingsDir.path());
    const QJsonObject controlledSettingsBeforeStartup = settingsFingerprint();
    const QJsonObject controlledNativeOrgAppBeforeStartup = nativeOrgAppFingerprint();
    QSettings settings;
    QVERIFY2(settings.fileName().startsWith(m_settingsDir.path()),
             qPrintable(QStringLiteral("Unexpected settings file: %1").arg(settings.fileName())));
    QVERIFY2(
        controlledNativeOrgAppBeforeStartup == originalNativeOrgApp,
        qPrintable(QStringLiteral("Native org/app fingerprint changed before App startup. before=%1 after=%2")
                       .arg(QString::fromUtf8(QJsonDocument(originalNativeOrgApp).toJson(QJsonDocument::Compact)),
                            QString::fromUtf8(QJsonDocument(controlledNativeOrgAppBeforeStartup).toJson(QJsonDocument::Compact)))));

    m_previousCwd = QDir::currentPath();
    m_cwdChanged = QDir::setCurrent(m_runtimeCwdDir.path());
    QVERIFY2(m_cwdChanged, qPrintable(QStringLiteral("Failed to switch runtime CWD to %1").arg(m_runtimeCwdDir.path())));

    QDir probeDir(QStringLiteral(MCP_AUTHORING_PROBE_PLUGIN_DIR));
    QVERIFY2(probeDir.exists(), qPrintable(QStringLiteral("Probe plugin dir missing: %1").arg(probeDir.path())));
    const QStringList probeCandidates = pluginCandidates(probeDir);
    QVERIFY2(!probeCandidates.isEmpty(),
             qPrintable(QStringLiteral("No probe plugins found in %1").arg(probeDir.path())));

    QDir expectedSearchDir(IOPluginCache::systemPluginDirectory());
    const QString expectedSearchPath = QDir::cleanPath(expectedSearchDir.absolutePath());
    QVERIFY2(expectedSearchDir.exists() || expectedSearchDir.mkpath(QStringLiteral(".")),
             qPrintable(QStringLiteral("Failed to create plugin search dir: %1").arg(expectedSearchPath)));
    const QStringList preloadedCandidates = pluginCandidates(expectedSearchDir);
    QString pluginCurationError;
    QVERIFY2(removePluginsFromDir(expectedSearchDir, &pluginCurationError),
             qPrintable(pluginCurationError));
    QVERIFY2(copyPlugins(probeDir, expectedSearchDir, &pluginCurationError),
             qPrintable(pluginCurationError));
    const QStringList curatedCandidates = pluginCandidates(expectedSearchDir);
    qInfo() << "Pre-start plugin search path:" << expectedSearchPath << "inventory:" << preloadedCandidates;
    qInfo() << "Probe plugin inventory:" << probeCandidates;
    qInfo() << "Curated startup plugin inventory:" << curatedCandidates;
    QCOMPARE(curatedCandidates, probeCandidates);

    QJsonArray startupOrder;
    startupOrder.append(QStringLiteral("set-controlled-settings-before-app-construction"));
    startupOrder.append(QStringLiteral("verify-probe-only-plugin-inventory-before-startup"));
    startupOrder.append(QStringLiteral("construct-app"));
    m_app = std::make_unique<App>();
    m_app->set3dSupported(false);
    startupOrder.append(QStringLiteral("startup-app"));
    m_app->startup();
    m_app->resize(1200, 800);
    m_app->doc()->masterTimer()->stop();
    QVERIFY2(m_app->status() == QQuickView::Ready, qPrintable(m_app->errors().value(0).toString()));

    m_initialAccessMask = m_app->accessMask();
    m_app->doc()->setWorkspacePath(m_workspaceDir.path());
    QVERIFY2(m_app->doc()->workspacePath().startsWith(m_workspaceDir.path()),
             qPrintable(QStringLiteral("Unexpected workspace path: %1").arg(m_app->doc()->workspacePath())));

    m_bridge = std::make_unique<VCBridgeV5>(m_app->doc(), m_app->virtualConsole(), m_app.get());

    m_probeName = QStringLiteral("MCP Authoring Probe Plugin");
    startupOrder.append(QStringLiteral("inspect-loaded-plugins"));
    QStringList loadedNames;
    const QList<QLCIOPlugin *> plugins = m_app->doc()->ioPluginCache()->plugins();
    for (QLCIOPlugin *plugin : plugins)
    {
        loadedNames << plugin->name();
        if (plugin && plugin->name() == m_probeName)
            m_probePlugin = plugin;
    }
    qInfo() << "MCP owner probe directory:" << probeDir.absolutePath();
    qInfo() << "Loaded IO plugins:" << loadedNames;

    QVERIFY2(m_probePlugin, "Probe plugin was not loaded");
    QCOMPARE(loadedNames, QStringList{m_probeName});
    QVERIFY(m_probePlugin->property("initCount").toInt() >= 1);
    const QJsonObject effectiveSettingsAfterStartup = settingsFingerprint();
    const QJsonObject nativeOrgAppAfterStartup = nativeOrgAppFingerprint();
    QVERIFY2(effectiveSettingsAfterStartup.value(QStringLiteral("fileName")).toString().startsWith(m_settingsDir.path()),
             qPrintable(QStringLiteral("Effective App settings escaped controlled root: %1")
                            .arg(effectiveSettingsAfterStartup.value(QStringLiteral("fileName")).toString())));
    QVERIFY2(
        nativeOrgAppAfterStartup == originalNativeOrgApp,
        qPrintable(QStringLiteral("Native org/app fingerprint changed after App startup. before=%1 after=%2")
                       .arg(QString::fromUtf8(QJsonDocument(originalNativeOrgApp).toJson(QJsonDocument::Compact)),
                            QString::fromUtf8(QJsonDocument(nativeOrgAppAfterStartup).toJson(QJsonDocument::Compact)))));

    InputOutputMap *ioMap = m_app->doc()->inputOutputMap();
    for (Universe *universe : ioMap->universes())
    {
        QVERIFY(universe->inputPatch() == nullptr);
        QVERIFY(universe->outputPatchesCount() == 0);
        QVERIFY(universe->feedbackPatch() == nullptr);
    }

    const QString startupProofPath =
        m_runtimeCwdDir.path() + QDir::separator() + QStringLiteral("mcp_authoring_owner_startup_proof.json");
    QJsonObject startupProof;
    startupProof.insert(QStringLiteral("startupOrder"), startupOrder);
    startupProof.insert(QStringLiteral("settingsDirectory"), m_settingsDir.path());
    startupProof.insert(QStringLiteral("settings"), QJsonObject{
        {QStringLiteral("original"), originalSettings},
        {QStringLiteral("originalNativeOrgApp"), originalNativeOrgApp},
        {QStringLiteral("controlledBeforeStartup"), controlledSettingsBeforeStartup},
        {QStringLiteral("controlledNativeOrgAppBeforeStartup"), controlledNativeOrgAppBeforeStartup},
        {QStringLiteral("effectiveAfterStartup"), effectiveSettingsAfterStartup},
        {QStringLiteral("nativeOrgAppAfterStartup"), nativeOrgAppAfterStartup}
    });
    startupProof.insert(QStringLiteral("pluginResources"), QJsonObject{
        {QStringLiteral("systemPluginSearchPath"), expectedSearchPath},
        {QStringLiteral("probePluginDirectory"), probeDir.absolutePath()},
        {QStringLiteral("preStartCandidates"), toJsonArray(preloadedCandidates)},
        {QStringLiteral("probeCandidates"), toJsonArray(probeCandidates)},
        {QStringLiteral("curatedCandidates"), toJsonArray(curatedCandidates)},
        {QStringLiteral("loadedPluginNamesAfterStartup"), toJsonArray(loadedNames)}
    });
    QString startupProofError;
    QVERIFY2(writeJson(startupProofPath, startupProof, &startupProofError),
             qPrintable(QStringLiteral("Failed to write startup proof: %1 (%2)")
                            .arg(startupProofPath, startupProofError)));
    qInfo() << "Owner startup proof artifact:" << startupProofPath;
}

void VCAuthoringOwner_Test::cleanupTestCase()
{
    if (m_app)
    {
        InputOutputMap *ioMap = m_app->doc()->inputOutputMap();
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), false, 0);
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        m_app->setAccessMask(m_initialAccessMask);
    }
    m_bridge.reset();
    m_probePlugin.clear();
    m_app.reset();
    if (m_cwdChanged)
    {
        const bool restored = QDir::setCurrent(m_previousCwd);
        QVERIFY2(restored, qPrintable(QStringLiteral("Failed to restore original CWD: %1").arg(m_previousCwd)));
        m_cwdChanged = false;
    }
}

QByteArray VCAuthoringOwner_Test::serializeConsole() const
{
    if (!m_app)
        return {};
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    if (!m_app->virtualConsole()->saveXML(&writer))
        return {};
    return xml;
}

int VCAuthoringOwner_Test::counter(const char *name) const
{
    if (!m_probePlugin)
        return -1;
    return m_probePlugin->property(name).toInt();
}

void VCAuthoringOwner_Test::resetProbeCounters() const
{
    QVERIFY2(m_probePlugin, "Probe plugin pointer is null");
    const bool invoked = QMetaObject::invokeMethod(m_probePlugin, "resetCounters");
    QVERIFY(invoked);
}

void VCAuthoringOwner_Test::verifyOnlyProbePatchesActive() const
{
    QVERIFY2(m_app, "App not initialized");
    InputOutputMap *ioMap = m_app->doc()->inputOutputMap();
    for (Universe *universe : ioMap->universes())
    {
        if (universe->inputPatch() != nullptr)
            QCOMPARE(universe->inputPatch()->plugin()->name(), m_probeName);

        const int patches = universe->outputPatchesCount();
        for (int i = 0; i < patches; ++i)
        {
            OutputPatch *patch = universe->outputPatch(i);
            QVERIFY(patch != nullptr);
            QCOMPARE(patch->plugin()->name(), m_probeName);
        }

        if (OutputPatch *feedbackPatch = universe->feedbackPatch())
            QCOMPARE(feedbackPatch->plugin()->name(), m_probeName);
    }
}

void VCAuthoringOwner_Test::queryPreviewApply_keepRuntimeEffectsAtZero()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");
    QVERIFY2(m_probePlugin, "Probe plugin not loaded");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    const bool outputPatched = ioMap->setOutputPatch(0, m_probeName, "", "", 0, false, 0);
    QVERIFY2(outputPatched, "Failed to patch probe output");
    const bool feedbackPatched = ioMap->setOutputPatch(0, m_probeName, "", "", 0, true);
    QVERIFY2(feedbackPatched, "Failed to patch probe feedback");
    verifyOnlyProbePatchesActive();
    QVERIFY2(counter("openCount") > 0, "Probe open counter did not move after patching");

    const bool outputUnpatched = ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), false, 0);
    QVERIFY2(outputUnpatched, "Failed to unpatch probe output for close oracle");
    const bool feedbackUnpatched = ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
    QVERIFY2(feedbackUnpatched, "Failed to unpatch probe feedback for close oracle");
    QVERIFY2(counter("closeCount") > 0, "Probe close counter did not move after unpatching");

    QVERIFY2(ioMap->setOutputPatch(0, m_probeName, "", "", 0, false, 0),
             "Failed to repatch probe output after close oracle");
    QVERIFY2(ioMap->setOutputPatch(0, m_probeName, "", "", 0, true),
             "Failed to repatch probe feedback after close oracle");
    verifyOnlyProbePatchesActive();
    qInfo() << "Patch/unpatch oracle counters"
            << "open" << counter("openCount")
            << "close" << counter("closeCount");

    auto restore = qScopeGuard([&]() {
        vc->deleteVCWidgets(QVariantList{});
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), false, 0);
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        m_app->setAccessMask(m_initialAccessMask);
    });

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 400));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 60, 100, 80));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanupWidgets = qScopeGuard([&]() {
        vc->deleteVCWidgets({buttonId, frameId});
    });

    // Positive oracle outside measured MCP callbacks: prove counters react.
    resetProbeCounters();
    Universe *u0 = ioMap->universe(0);
    QVERIFY(u0 != nullptr);
    u0->setChannelCapability(0, QLCChannel::Intensity);
    u0->zeroIntensityChannels();
    QVERIFY(u0->write(0, 123));
    const QByteArray frameData = u0->postGMValues()->mid(0, u0->usedChannels());
    u0->dumpOutput(frameData, true);
    QVERIFY(ioMap->sendFeedBack(0, 1, 200, QVariantMap{{QStringLiteral("source"), QStringLiteral("probe")}}));
    QVERIFY(counter("writeCount") > 0);
    QVERIFY(counter("feedbackCount") > 0);
    qInfo() << "Positive oracle counters"
            << "open" << counter("openCount")
            << "close" << counter("closeCount")
            << "write" << counter("writeCount")
            << "feedback" << counter("feedbackCount");

    // Fence and measure only query/preview/apply and access denial.
    resetProbeCounters();
    QCOMPARE(counter("openCount"), 0);
    QCOMPARE(counter("closeCount"), 0);
    QCOMPARE(counter("writeCount"), 0);
    QCOMPARE(counter("feedbackCount"), 0);

    fastmcpp::tools::ToolManager tm;
    registerQueryTools(tm, doc, m_bridge.get());
    const Json queryResult = parseInvoke(tm.invoke("vc_query_pages", Json{
        {"pageIndex", 0},
        {"properties", Json::array({"type", "geometry"})}
    }));
    QVERIFY2(queryResult.is_array(), queryResult.dump().c_str());
    QVERIFY2(containsWidgetId(queryResult, int(buttonId)), queryResult.dump().c_str());

    const QRectF originalButtonRect = button->logicalGeometry();
    const QRectF originalFrameRect = frame->logicalGeometry();
    const bool dirtyBeforePreview = doc->isModified();
    const QByteArray xmlBeforePreview = serializeConsole();

    const QRectF movedRect(80, 120, 100, 80);
    const QVariantMap preview = vc->previewGeometry(buttonId, movedRect, false);
    QVERIFY(preview.value("accepted").toBool());
    QCOMPARE(button->logicalGeometry(), originalButtonRect);
    QCOMPARE(frame->logicalGeometry(), originalFrameRect);
    QCOMPARE(doc->isModified(), dirtyBeforePreview);
    const QByteArray xmlAfterPreview = serializeConsole();
    QCOMPARE(xmlAfterPreview, xmlBeforePreview);

    const QVariantMap applied = vc->editGeometry(buttonId, movedRect, false);
    QVERIFY(applied.value("accepted").toBool());
    QCOMPARE(button->logicalGeometry(), movedRect);
    QCOMPARE(frame->logicalGeometry(), originalFrameRect);

    const int oldMask = m_app->accessMask();
    const auto restoreMask = qScopeGuard([&]() { m_app->setAccessMask(oldMask); });
    m_app->setAccessMask(oldMask & ~App::AC_VCEditing);
    const QRectF deniedRect(120, 140, 100, 80);
    const QVariantMap denied = vc->editGeometry(buttonId, deniedRect, false);
    QVERIFY(!denied.value("accepted").toBool());
    QVERIFY(denied.value("error").toString().contains(QStringLiteral("editing"), Qt::CaseInsensitive));
    QCOMPARE(button->logicalGeometry(), movedRect);
    QCOMPARE(frame->logicalGeometry(), originalFrameRect);

    QCOMPARE(counter("openCount"), 0);
    QCOMPARE(counter("closeCount"), 0);
    QCOMPARE(counter("writeCount"), 0);
    QCOMPARE(counter("feedbackCount"), 0);
    qInfo() << "Measured operation counters"
            << "open" << counter("openCount")
            << "close" << counter("closeCount")
            << "write" << counter("writeCount")
            << "feedback" << counter("feedbackCount")
            << "denialError" << denied.value("error").toString();
}

void VCAuthoringOwner_Test::authoringMethods_requireProductionContext()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    VCBridge::AuthoringSetupQuery query;
    const VCBridge::AuthoringReadResult readWithApp = m_bridge->queryAuthoring(VCBridge::AuthoringQuery(query));
    QVERIFY2(readWithApp.code != QStringLiteral("unsupportedBridge"),
             qPrintable(readWithApp.error));

    VCBridgeV5 legacyBridge(m_app->doc(), m_app->virtualConsole());
    const VCBridge::AuthoringReadResult readNoApp = legacyBridge.queryAuthoring(VCBridge::AuthoringQuery(query));
    QCOMPARE(readNoApp.code, QStringLiteral("unsupportedBridge"));
    VCBridge::AuthoringBindingsRequest unsupportedReq;
    VCBridge::AuthoringBindingItem unsupportedItem;
    unsupportedItem.widgetID = 1;
    unsupportedItem.kind = VCBridge::AuthoringBindingItem::Kind::Function;
    unsupportedItem.action = VCBridge::AuthoringBindingItem::Action::Set;
    unsupportedItem.functionID = Function::invalidId();
    unsupportedReq.items.append(unsupportedItem);
    const VCBridge::AuthoringPreparationResult unsupportedPrep =
        legacyBridge.prepareAuthoring(VCBridge::AuthoringRequest(unsupportedReq));
    QCOMPARE(unsupportedPrep.code, QStringLiteral("unsupportedBridge"));
    QCOMPARE(unsupportedPrep.atomicScope, VCBridge::AuthoringPreparationResult::AtomicScope::IndependentItems);

    Doc mismatchedDoc(this);
    VCBridgeV5 mismatchedDocBridge(&mismatchedDoc, m_app->virtualConsole(), m_app.get());
    const VCBridge::AuthoringReadResult readMismatchDoc = mismatchedDocBridge.queryAuthoring(VCBridge::AuthoringQuery(query));
    QCOMPARE(readMismatchDoc.code, QStringLiteral("unsupportedBridge"));

    VCBridgeV5 mismatchedVcBridge(m_app->doc(), nullptr, m_app.get());
    const VCBridge::AuthoringReadResult readMismatchVc = mismatchedVcBridge.queryAuthoring(VCBridge::AuthoringQuery(query));
    QCOMPARE(readMismatchVc.code, QStringLiteral("unsupportedBridge"));

    VCBridgeV5 missingContext(nullptr, nullptr, m_app.get());
    const VCBridge::AuthoringReadResult readMissing = missingContext.queryAuthoring(VCBridge::AuthoringQuery(query));
    QCOMPARE(readMissing.code, QStringLiteral("unsupportedBridge"));
}

void VCAuthoringOwner_Test::authoringQueries_unimplementedReturnUnsupported_data()
{
    QTest::addColumn<QString>("kind");
    QTest::newRow("targets") << QStringLiteral("targets");
    QTest::newRow("sources") << QStringLiteral("sources");
    QTest::newRow("profiles") << QStringLiteral("profiles");
}

void VCAuthoringOwner_Test::authoringQueries_unimplementedReturnUnsupported()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, kind);

    VCBridge::AuthoringTargetControlQuery query;
    if (kind == QStringLiteral("targets"))
    {
        query.kind = VCBridge::AuthoringTargetControlQuery::Kind::Targets;
        query.widgetIDs = QList<quint32>{quint32(m_app->virtualConsole()->page(0)->id())};
    }
    else if (kind == QStringLiteral("sources"))
    {
        query.kind = VCBridge::AuthoringTargetControlQuery::Kind::Sources;
    }
    else
    {
        query.kind = VCBridge::AuthoringTargetControlQuery::Kind::Profiles;
    }

    const VCBridge::AuthoringReadResult result =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(query));

    QVERIFY2(result.code != QStringLiteral("unsupportedPreparation"),
             qPrintable(result.error));
    QVERIFY2(result.code.isEmpty() || result.code == QStringLiteral("invalidArgument"),
             qPrintable(QStringLiteral("Unexpected code: %1 (%2)").arg(result.code, result.error)));
}

void VCAuthoringOwner_Test::authoringCore_registeredToolNativeRoundtrip_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(frame);
    page0->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setCurrentPage(0);
    frame->setLogicalGeometry(QRectF(0, 0, 700, 420));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setPage(0);
    button->setCaption(QStringLiteral("roundtrip-button"));
    button->setLogicalGeometry(QRectF(20, 20, 160, 80));
    button->setActionType(VCButton::Toggle);
    const QUuid buttonControlId = button->ensureRecordingId();
    QVERIFY(!buttonControlId.isNull());
    QSharedPointer<QLCInputSource> buttonSource(new QLCInputSource(0, 10));
    buttonSource->setID(0);
    buttonSource->setPage(0);
    buttonSource->setWorkingMode(QLCInputSource::Absolute);
    button->addInputSource(buttonSource);

    VCSlider *slider = new VCSlider(doc, frame);
    vc->addWidgetToMap(slider);
    frame->addWidgetToPageMap(slider);
    slider->setPage(0);
    slider->setCaption(QStringLiteral("roundtrip-slider"));
    slider->setLogicalGeometry(QRectF(220, 20, 100, 220));
    const QUuid sliderControlId = slider->ensureRecordingId();
    QVERIFY(!sliderControlId.isNull());
    QSharedPointer<QLCInputSource> sliderSource(new QLCInputSource(1, 12));
    sliderSource->setID(0);
    sliderSource->setPage(0);
    sliderSource->setWorkingMode(QLCInputSource::Absolute);
    sliderSource->setFeedbackValue(QLCInputFeedback::LowerValue, 7);
    sliderSource->setFeedbackValue(QLCInputFeedback::UpperValue, 201);
    sliderSource->setFeedbackValue(QLCInputFeedback::MonitorValue, 99);
    sliderSource->setFeedbackExtraParams(QLCInputFeedback::LowerValue, 11);
    sliderSource->setFeedbackExtraParams(QLCInputFeedback::UpperValue, 12);
    sliderSource->setFeedbackExtraParams(QLCInputFeedback::MonitorValue, 13);
    slider->addInputSource(sliderSource);
    QSharedPointer<QLCInputSource> sliderSourceDup(new QLCInputSource(1, 12));
    sliderSourceDup->setID(0);
    sliderSourceDup->setPage(0);
    sliderSourceDup->setWorkingMode(QLCInputSource::Absolute);
    slider->addInputSource(sliderSourceDup);

    VCAnimation *animation = new VCAnimation(doc, frame);
    vc->addWidgetToMap(animation);
    frame->addWidgetToPageMap(animation);
    animation->setPage(0);
    animation->setCaption(QStringLiteral("roundtrip-animation"));
    animation->setLogicalGeometry(QRectF(360, 20, 220, 220));
    const int firstPresetId = animation->addTextPreset(QStringLiteral("alpha"));
    QVERIFY(firstPresetId >= 30);
    const int secondPresetId = animation->addTextPreset(QStringLiteral("beta"));
    QVERIFY(secondPresetId >= 30);

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    const quint32 sliderId = slider->id();
    const quint32 animationId = animation->id();
    auto cleanupWidgets = qScopeGuard([&]() {
        vc->deleteVCWidgets({buttonId, sliderId, animationId, frameId});
    });

    Show *show = new Show(doc);
    show->setName(QStringLiteral("roundtrip-show"));
    QVERIFY(doc->addFunction(show));
    const quint32 showId = show->id();
    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("roundtrip-scene"));
    QVERIFY(doc->addFunction(scene));
    const quint32 sceneId = scene->id();
    auto cleanupShow = qScopeGuard([&]() {
        doc->deleteFunction(showId);
        doc->deleteFunction(sceneId);
    });

    ShowCommandTrack track;
    QString trackError;
    ShowCommand buttonOn = ShowCommand::setButtonState(6101u, 1000u, buttonControlId, true);
    buttonOn.pairId = 9701u;
    QVERIFY2(track.insert(buttonOn, &trackError), qPrintable(trackError));
    ShowCommand buttonOff = ShowCommand::setButtonState(6102u, 1200u, buttonControlId, false);
    buttonOff.pairId = 9701u;
    QVERIFY2(track.insert(buttonOff, &trackError), qPrintable(trackError));
    ShowCommand sliderPosition = ShowCommand::setSliderPosition(
        6103u, 1300u, sliderControlId, ShowControlRole::LevelSlider, QString(), 0.42);
    QVERIFY2(track.insert(sliderPosition, &trackError), qPrintable(trackError));
    ShowCommand sceneStart = ShowCommand::start(6104u, 1500u, sceneId);
    QVERIFY2(track.insert(sceneStart, &trackError), qPrintable(trackError));
    QVERIFY2(show->setStoppedCommandTrack(track, &trackError), qPrintable(trackError));

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    int baselineOpen = counter("openCount");
    int baselineClose = counter("closeCount");
    int baselineWrite = counter("writeCount");
    int baselineFeedback = counter("feedbackCount");
    const QUuid buttonControlIdBefore = button->recordingId();
    const QUuid sliderControlIdBefore = slider->recordingId();
    const bool recorderRunningBefore = show->isRunning();

    fastmcpp::tools::ToolManager tm;
    registerQueryTools(tm, doc, m_bridge.get());

    const qint64 fixedNow = 990000;
    m_bridge->setAuthoringNowForTest(fixedNow);
    auto clearClock = qScopeGuard([&]() { m_bridge->clearAuthoringNowForTest(); });

    Json setupArgs = {
        {"kind", "widgets"},
        {"frameWidgetID", frameId},
        {"childSubpage", 0},
        {"recursive", true},
        {"widgetIDs", Json::array({buttonId, sliderId})},
        {"fields", Json::array({"identity", "placement", "logicalGeometry", "inputBindings"})},
        {"limit", 1}
    };

    const Json setupToolFirst = parseInvoke(tm.invoke("vc_query_setup", setupArgs));
    QVERIFY2(setupToolFirst.is_object(), setupToolFirst.dump().c_str());
    QCOMPARE(QString::fromStdString(setupToolFirst.value("code", std::string())), QString());
    QVERIFY(setupToolFirst.contains("rows"));
    QVERIFY(setupToolFirst["rows"].is_array());
    QCOMPARE(int(setupToolFirst["rows"].size()), 1);
    QVERIFY(setupToolFirst.contains("nextCursor"));
    const QString setupCursor = QString::fromStdString(setupToolFirst.value("nextCursor", std::string()));
    QVERIFY2(!setupCursor.isEmpty(), "Expected continuation cursor for limit=1");
    const Json setupRow0 = setupToolFirst["rows"].at(0);
    QVERIFY(setupRow0.contains("pageWidgetID"));
    QVERIFY(setupRow0.contains("parentWidgetID"));
    QVERIFY(setupRow0.contains("childSubpage"));
    QVERIFY(setupRow0.contains("siblingIndex"));
    QVERIFY(setupRow0.contains("logicalGeometry"));
    QVERIFY(setupRow0.contains("inputBindings"));
    QVERIFY(setupRow0.contains("configurationDigest"));

    VCBridge::AuthoringSetupQuery setupTyped;
    setupTyped.kind = VCBridge::AuthoringSetupQuery::Kind::Widgets;
    setupTyped.frameWidgetID = frameId;
    setupTyped.childSubpage = 0;
    setupTyped.recursive = true;
    setupTyped.widgetIDs = QList<quint32>{buttonId, sliderId};
    setupTyped.fields = QStringList{
        QStringLiteral("identity"),
        QStringLiteral("placement"),
        QStringLiteral("logicalGeometry"),
        QStringLiteral("inputBindings")
    };
    setupTyped.limit = 1;
    const VCBridge::AuthoringReadResult setupDirectFirst =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(setupTyped));
    const Json setupDirectFirstJson = VCAuthoring::toJson(setupDirectFirst);
    QVERIFY2(setupToolFirst == setupDirectFirstJson, "Tool setup JSON must match direct typed setup JSON");

    Json setupSecondArgs = setupArgs;
    setupSecondArgs["cursor"] = setupCursor.toStdString();
    const Json setupToolSecond = parseInvoke(tm.invoke("vc_query_setup", setupSecondArgs));
    QCOMPARE(QString::fromStdString(setupToolSecond.value("code", std::string())), QString());
    QVERIFY(setupToolSecond.value("complete", false));
    QVERIFY(setupToolSecond.contains("rows"));
    QCOMPARE(int(setupToolSecond["rows"].size()), 1);

    Json setupTamperedFields = setupSecondArgs;
    setupTamperedFields["fields"] = Json::array({"identity"});
    const Json tamperedFieldsResult = parseInvoke(tm.invoke("vc_query_setup", setupTamperedFields));
    QCOMPARE(QString::fromStdString(tamperedFieldsResult.value("code", std::string())),
             QStringLiteral("invalidCursor"));

    Json recCmdArgs = {
        {"showFunctionID", showId},
        {"kind", "commands"},
        {"fields", Json::array({"identity", "timing", "semanticPayload"})},
        {"limit", 50}
    };
    const Json recCmdTool = parseInvoke(tm.invoke("vc_query_recordings", recCmdArgs));
    QCOMPARE(QString::fromStdString(recCmdTool.value("code", std::string())), QString());
    QVERIFY(recCmdTool.contains("rows") && recCmdTool["rows"].is_array());

    VCBridge::AuthoringRecordingQuery recCmdTyped;
    recCmdTyped.showFunctionID = showId;
    recCmdTyped.kind = VCBridge::AuthoringRecordingQuery::Kind::Commands;
    recCmdTyped.fields = QStringList{
        QStringLiteral("identity"),
        QStringLiteral("timing"),
        QStringLiteral("semanticPayload")
    };
    recCmdTyped.limit = 50;
    const VCBridge::AuthoringReadResult recCmdDirect =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(recCmdTyped));
    const Json recCmdDirectJson = VCAuthoring::toJson(recCmdDirect);
    QVERIFY2(recCmdTool == recCmdDirectJson, "Tool recording-commands JSON must match direct typed JSON");

    bool sawPair = false;
    bool sawOnPayload = false;
    bool sawOffPayload = false;
    bool sawNullUuidStart = false;
    for (const Json &row : recCmdTool["rows"])
    {
        const std::string action = row.value("action", std::string());
        if (action == "SetButtonState")
        {
            if (row.value("pairID", 0u) == 9701u)
                sawPair = true;
            if (row.contains("payload") && row["payload"].is_object())
            {
                const Json payload = row["payload"];
                if (payload.contains("on") && payload["on"].is_boolean())
                {
                    if (payload["on"].get<bool>())
                        sawOnPayload = true;
                    else
                        sawOffPayload = true;
                }
            }
        }
        if (action == "Start")
        {
            sawNullUuidStart = true;
            QVERIFY(row.value("controlRecordingUUID", std::string()).empty());
        }
    }
    QVERIFY2(sawPair, "Expected paired SetButtonState rows");
    QVERIFY2(sawOnPayload && sawOffPayload, "Expected on/off semantic payload rows");
    QVERIFY2(sawNullUuidStart, "Expected Start command row with null control UUID serialization");

    Json recRefArgs = {
        {"showFunctionID", showId},
        {"kind", "references"},
        {"fields", Json::array({"identity", "readiness"})},
        {"limit", 50}
    };
    const Json recRefTool = parseInvoke(tm.invoke("vc_query_recordings", recRefArgs));
    QCOMPARE(QString::fromStdString(recRefTool.value("code", std::string())), QString());
    QVERIFY(recRefTool.contains("rows") && recRefTool["rows"].is_array());

    bool sawSliderReadiness = false;
    const std::string sliderUuid = sliderControlId.toString(QUuid::WithoutBraces).toLower().toStdString();
    for (const Json &row : recRefTool["rows"])
    {
        if (row.value("controlRecordingUUID", std::string()) != sliderUuid)
            continue;
        sawSliderReadiness = true;
        QVERIFY(row.contains("status") && row["status"].is_string());
        QVERIFY(!row["status"].get<std::string>().empty());
    }
    QVERIFY2(sawSliderReadiness, "Expected slider reference readiness row from registered tool path");

    InputOutputMap *ioMap = doc->inputOutputMap();
    QVERIFY2(ioMap != nullptr, "Missing InputOutputMap");
    auto restoreControls = qScopeGuard([&]() {
        ioMap->setInputPatch(0, KInputNone, "", "", QLCIOPlugin::invalidLine(), KInputNone);
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), false, 0);
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
    });
    QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, KInputNone),
             "Failed to patch probe input for controls query");
    QVERIFY2(ioMap->setOutputPatch(0, m_probeName, "", "", 0, false, 0),
             "Failed to patch probe output for controls query");
    QVERIFY2(ioMap->setOutputPatch(0, m_probeName, "", "", 0, true),
             "Failed to patch probe feedback for controls query");

    const QString profileFile = QStringLiteral("Akai-APCMini.qxi");
    const QStringList profileCandidates = ownerInputProfileCandidates(profileFile);
    const QString profilePath = resolveExistingPath(profileCandidates);
    QVERIFY2(!profilePath.isEmpty(),
             qPrintable(QStringLiteral("Failed to resolve %1 in candidates: %2")
                            .arg(profileFile, profileCandidates.join(QStringLiteral(", ")))));
    std::unique_ptr<QLCInputProfile> loadedProfile(QLCInputProfile::loader(profilePath));
    QVERIFY2(loadedProfile != nullptr, qPrintable(QStringLiteral("Failed to load profile: %1").arg(profilePath)));
    const QString profileName = loadedProfile->name();
    if (!ioMap->profileNames().contains(profileName))
        QVERIFY2(ioMap->addProfile(loadedProfile.release()),
                 qPrintable(QStringLiteral("Failed to add profile: %1").arg(profileName)));
    QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, profileName),
             "Failed to patch probe input with loaded profile");

    fastmcpp::tools::ToolManager tmInput;
    registerVCInputTools(tmInput, doc, m_bridge.get());
    QVERIFY2(tmInput.has("vc_query_controls"), "Missing vc_query_controls registration");

    resetProbeCounters();
    baselineOpen = counter("openCount");
    baselineClose = counter("closeCount");
    baselineWrite = counter("writeCount");
    baselineFeedback = counter("feedbackCount");

    Json targetArgs = {
        {"kind", "targets"},
        {"widgetIDs", Json::array({buttonId, sliderId, animationId})},
        {"fields", Json::array({"identity", "domains", "bindings", "feedback"})},
        {"limit", 1}
    };

    const Json targetToolFirst = parseInvoke(tmInput.invoke("vc_query_controls", targetArgs));
    QCOMPARE(QString::fromStdString(targetToolFirst.value("code", std::string())), QString());
    QVERIFY(targetToolFirst.contains("rows") && targetToolFirst["rows"].is_array());
    QCOMPARE(int(targetToolFirst["rows"].size()), 1);
    const QString targetCursor = QString::fromStdString(targetToolFirst.value("nextCursor", std::string()));
    QVERIFY2(!targetCursor.isEmpty(), "Expected controls targets cursor");

    VCBridge::AuthoringTargetControlQuery targetTyped;
    targetTyped.kind = VCBridge::AuthoringTargetControlQuery::Kind::Targets;
    targetTyped.widgetIDs = QList<quint32>{buttonId, sliderId, animationId};
    targetTyped.fields = QStringList{
        QStringLiteral("identity"),
        QStringLiteral("domains"),
        QStringLiteral("bindings"),
        QStringLiteral("feedback")
    };
    targetTyped.limit = 1;
    const VCBridge::AuthoringReadResult targetDirectFirst =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(targetTyped));
    QVERIFY2(targetToolFirst == VCAuthoring::toJson(targetDirectFirst),
             "Tool controls-targets JSON must match direct typed controls JSON");

    Json targetSecondArgs = targetArgs;
    targetSecondArgs["cursor"] = targetCursor.toStdString();
    const Json targetToolSecond = parseInvoke(tmInput.invoke("vc_query_controls", targetSecondArgs));
    QCOMPARE(QString::fromStdString(targetToolSecond.value("code", std::string())), QString());
    QVERIFY(targetToolSecond.contains("rows") && targetToolSecond["rows"].is_array());
    QVERIFY(!targetToolSecond["rows"].empty());
    Json targetPaged = targetToolSecond;
    int guard = 0;
    while (!targetPaged.value("complete", false) && guard++ < 50)
    {
        QVERIFY(targetPaged.contains("nextCursor"));
        const std::string next = targetPaged.value("nextCursor", std::string());
        QVERIFY2(!next.empty(), "Expected continuation cursor while complete=false");
        Json nextArgs = targetArgs;
        nextArgs["cursor"] = next;
        targetPaged = parseInvoke(tmInput.invoke("vc_query_controls", nextArgs));
        QCOMPARE(QString::fromStdString(targetPaged.value("code", std::string())), QString());
        QVERIFY(targetPaged.contains("rows") && targetPaged["rows"].is_array());
    }
    QVERIFY2(targetPaged.value("complete", false), "Target controls pagination did not complete");

    bool sawKnownSliderSlot = false;
    bool sawButtonSlot0 = false;
    bool sawSliderResetSlot1 = false;
    bool sawUnknownAnimationPreset = false;
    Json targetInspect = targetToolFirst;
    guard = 0;
    while (true)
    {
        for (const Json &row : targetInspect["rows"])
        {
            if (!row.is_object())
                continue;
            const quint32 wid = row.value("widgetID", quint32(-1));
            const int slot = row.value("controlSlotID", -1);
            if (wid == buttonId && slot == 0)
            {
                sawButtonSlot0 = true;
                QCOMPARE(QString::fromStdString(row.value("valueDomain", std::string())),
                         QStringLiteral("triggerNonZero"));
                QVERIFY(row.contains("limitations"));
                QVERIFY(row["limitations"].is_array());
                bool sawConditional = false;
                for (const Json &lim : row["limitations"])
                {
                    if (lim.value("code", std::string()) == "behaviorConditional")
                        sawConditional = true;
                }
                QVERIFY2(sawConditional, "Expected behaviorConditional limitation for button slot 0");
            }
            if (wid == sliderId && slot == 0)
            {
                sawKnownSliderSlot = true;
                QVERIFY(row.contains("validationLevel"));
                QCOMPARE(QString::fromStdString(row.value("validationLevel", std::string())),
                         QStringLiteral("nativeKnown"));
                QVERIFY(row.contains("assignmentSupported"));
                QVERIFY(row["assignmentSupported"].is_boolean());
                QVERIFY(row["assignmentSupported"].get<bool>());
                QVERIFY(row.contains("inputBindings") && row["inputBindings"].is_array());
                const Json entries = row["inputBindings"];
                QVERIFY(entries.is_array());
                QCOMPARE(int(entries.size()), 2);
                const Json entry0 = entries.at(0);
                const Json entry1 = entries.at(1);
                QCOMPARE(entry0.value("sourceOrdinal", -1), 0);
                QCOMPARE(entry1.value("sourceOrdinal", -1), 1);
                QVERIFY(entry0.contains("sourceKey"));
                QVERIFY(entry1.contains("sourceKey"));
                QCOMPARE(entry0["sourceKey"].value("controlSlotID", -1), 0);
                QCOMPARE(entry1["sourceKey"].value("controlSlotID", -1), 0);
                QCOMPARE(entry0["sourceKey"].value("inputUniverseID", -1), 1);
                QCOMPARE(entry1["sourceKey"].value("inputUniverseID", -1), 1);
                QCOMPARE(entry0["sourceKey"].value("inputChannel", -1), 12);
                QCOMPARE(entry1["sourceKey"].value("inputChannel", -1), 12);
                QVERIFY(entry0.value("duplicate", false));
                QVERIFY(entry1.value("duplicate", false));
                QVERIFY(entry0.value("ambiguous", false));
                QVERIFY(entry1.value("ambiguous", false));
                QVERIFY(row.contains("feedback") && row["feedback"].is_array());
                const Json feedbackEntries = row["feedback"];
                QVERIFY(feedbackEntries.is_array());
                QCOMPARE(int(feedbackEntries.size()), 2);
                const Json feedbackEntry = feedbackEntries.at(0);
                QCOMPARE(feedbackEntry.value("idleValue", -1), 7);
                QCOMPARE(feedbackEntry.value("activeValue", -1), 201);
                QCOMPARE(feedbackEntry.value("monitorValue", -1), 99);
                QCOMPARE(feedbackEntry.value("idleChannel", -1), 11);
                QCOMPARE(feedbackEntry.value("activeChannel", -1), 12);
                QCOMPARE(feedbackEntry.value("monitorChannel", -1), 13);
            }
            if (wid == sliderId && slot == 1)
            {
                sawSliderResetSlot1 = true;
                QCOMPARE(QString::fromStdString(row.value("valueDomain", std::string())),
                         QStringLiteral("triggerNonZero"));
            }
            if (wid == animationId && slot >= 30)
            {
                sawUnknownAnimationPreset = true;
                QVERIFY(row.contains("validationLevel"));
                QCOMPARE(QString::fromStdString(row.value("validationLevel", std::string())),
                         QStringLiteral("nativeUnknown"));
                QVERIFY(row.contains("assignmentSupported"));
                QVERIFY(!row["assignmentSupported"].get<bool>());
                QVERIFY(row.contains("limitations"));
            }
        }
        if (targetInspect.value("complete", false))
            break;
        QVERIFY(targetInspect.contains("nextCursor"));
        const std::string next = targetInspect.value("nextCursor", std::string());
        QVERIFY2(!next.empty(), "Expected continuation cursor while complete=false");
        Json nextArgs = targetArgs;
        nextArgs["cursor"] = next;
        targetInspect = parseInvoke(tmInput.invoke("vc_query_controls", nextArgs));
        QCOMPARE(QString::fromStdString(targetInspect.value("code", std::string())), QString());
        QVERIFY(++guard < 50);
    }
    QVERIFY2(sawButtonSlot0, "Expected button slot 0 domain row");
    QVERIFY2(sawKnownSliderSlot, "Expected known slider slot row with native groups");
    QVERIFY2(sawSliderResetSlot1, "Expected slider reset slot 1 domain row");
    QVERIFY2(sawUnknownAnimationPreset, "Expected unknown animation preset slot row");

    Json targetIdentityOnlyArgs = {
        {"kind", "targets"},
        {"widgetIDs", Json::array({buttonId})},
        {"fields", Json::array({"identity"})},
        {"limit", 50}
    };
    const Json targetIdentityOnly = parseInvoke(tmInput.invoke("vc_query_controls", targetIdentityOnlyArgs));
    QCOMPARE(QString::fromStdString(targetIdentityOnly.value("code", std::string())), QString());
    QVERIFY(targetIdentityOnly.contains("rows") && targetIdentityOnly["rows"].is_array());
    QVERIFY(!targetIdentityOnly["rows"].empty());
    const Json identityRow = targetIdentityOnly["rows"].at(0);
    QVERIFY(!identityRow.contains("validationLevel"));
    QVERIFY(!identityRow.contains("assignmentSupported"));
    QVERIFY(!identityRow.contains("valueDomain"));
    QVERIFY(!identityRow.contains("keyboardAllowed"));
    QVERIFY(!identityRow.contains("inputBindings"));
    QVERIFY(!identityRow.contains("feedback"));

    Json targetDefaultArgs = {
        {"kind", "targets"},
        {"widgetIDs", Json::array({buttonId})},
        {"limit", 50}
    };
    const Json targetDefault = parseInvoke(tmInput.invoke("vc_query_controls", targetDefaultArgs));
    QCOMPARE(QString::fromStdString(targetDefault.value("code", std::string())), QString());
    QVERIFY(targetDefault.contains("rows") && targetDefault["rows"].is_array());
    QVERIFY(!targetDefault["rows"].empty());
    const Json defaultRow = targetDefault["rows"].at(0);
    QVERIFY(defaultRow.contains("validationLevel"));
    QVERIFY(defaultRow.contains("assignmentSupported"));
    QVERIFY(!defaultRow.contains("inputBindings"));
    QVERIFY(!defaultRow.contains("feedback"));

    VCAnimation *cursorAnimation = new VCAnimation(doc, frame);
    vc->addWidgetToMap(cursorAnimation);
    frame->addWidgetToPageMap(cursorAnimation);
    cursorAnimation->setPage(0);
    cursorAnimation->setCaption(QStringLiteral("cursor-animation"));
    cursorAnimation->setLogicalGeometry(QRectF(620, 20, 220, 220));
    const int cursorPresetA = cursorAnimation->addTextPreset(QStringLiteral("cursor-alpha"));
    QVERIFY(cursorPresetA >= 30);
    const quint32 cursorAnimationId = cursorAnimation->id();
    Json targetInventoryArgs = {
        {"kind", "targets"},
        {"widgetIDs", Json::array({cursorAnimationId})},
        {"fields", Json::array({"identity", "domains"})},
        {"limit", 1}
    };
    const Json targetInventoryFirst = parseInvoke(tmInput.invoke("vc_query_controls", targetInventoryArgs));
    QCOMPARE(QString::fromStdString(targetInventoryFirst.value("code", std::string())), QString());
    QVERIFY(!targetInventoryFirst.value("complete", true));
    const QString targetInventoryCursor = QString::fromStdString(targetInventoryFirst.value("nextCursor", std::string()));
    QVERIFY2(!targetInventoryCursor.isEmpty(), "Expected controls-targets inventory cursor");
    const int cursorPresetB = cursorAnimation->addTextPreset(QStringLiteral("cursor-beta"));
    QVERIFY(cursorPresetB >= 30);
    Json targetInventorySecondArgs = targetInventoryArgs;
    targetInventorySecondArgs["cursor"] = targetInventoryCursor.toStdString();
    const Json targetInventorySecond = parseInvoke(tmInput.invoke("vc_query_controls", targetInventorySecondArgs));
    const QString inventoryCode = QString::fromStdString(targetInventorySecond.value("code", std::string()));
    QVERIFY2(inventoryCode == QStringLiteral("invalidCursor") || inventoryCode == QStringLiteral("staleBasis"),
             qPrintable(QStringLiteral("Expected invalidCursor/staleBasis after inventory change, got %1").arg(inventoryCode)));
    vc->deleteVCWidgets({cursorAnimationId});

    Json targetTampered = targetSecondArgs;
    targetTampered["fields"] = Json::array({"identity"});
    const Json targetTamperedResult = parseInvoke(tmInput.invoke("vc_query_controls", targetTampered));
    QCOMPARE(QString::fromStdString(targetTamperedResult.value("code", std::string())),
             QStringLiteral("invalidCursor"));

    Json targetMalformed = targetArgs;
    targetMalformed["cursor"] = "not-base64";
    const Json targetMalformedResult = parseInvoke(tmInput.invoke("vc_query_controls", targetMalformed));
    QCOMPARE(QString::fromStdString(targetMalformedResult.value("code", std::string())),
             QStringLiteral("invalidCursor"));

    Json sourceArgs = {
        {"kind", "sources"},
        {"pluginName", m_probeName.toStdString()},
        {"inputUniverseID", 0},
        {"fields", Json::array({"identity", "patches", "status"})},
        {"limit", 50}
    };
    const Json sourcesTool = parseInvoke(tmInput.invoke("vc_query_controls", sourceArgs));
    QCOMPARE(QString::fromStdString(sourcesTool.value("code", std::string())), QString());
    QVERIFY(sourcesTool.contains("rows") && sourcesTool["rows"].is_array());
    QVERIFY(int(sourcesTool["rows"].size()) >= 2);
    bool sawMissingUid = false;
    int missingStatusRows = 0;
    int statusUnavailableLimitRows = 0;
    int statusReasonRows = 0;
    int uidUnavailableLimitRows = 0;
    bool sawInputConnectedOrAdvertising = false;
    for (const Json &row : sourcesTool["rows"])
    {
        if (!row.is_object())
            continue;
        const std::string direction = row.value("direction", std::string());
        if (direction == "input")
        {
            if (row.contains("status"))
            {
                const std::string status = row.value("status", std::string());
                if (status == "connected" || status == "advertising")
                    sawInputConnectedOrAdvertising = true;
            }
            else
            {
                ++missingStatusRows;
            }
            if (!row.contains("inputUID"))
                sawMissingUid = true;
        }
        if (direction == "output" || direction == "feedback")
        {
            QVERIFY(!row.contains("status"));
            ++missingStatusRows;
        }
        if (row.contains("limitations") && row["limitations"].is_array())
        {
            for (const Json &lim : row["limitations"])
            {
                if (lim.value("code", std::string()) == "statusUnavailable")
                    ++statusUnavailableLimitRows;
                if (lim.value("code", std::string()) == "uidUnavailable")
                {
                    sawMissingUid = true;
                    ++uidUnavailableLimitRows;
                    QVERIFY(lim.value("scope", std::string()).find("direction:") != std::string::npos);
                }
            }
        }
        if (row.contains("reason") && row["reason"].is_string() && !row["reason"].get<std::string>().empty())
            ++statusReasonRows;
    }
    QVERIFY2(sawMissingUid, "Expected uidUnavailable or missing UID projection");
    QVERIFY2(statusUnavailableLimitRows + statusReasonRows >= missingStatusRows,
             qPrintable(QStringLiteral("Missing status rows=%1, statusUnavailable limits=%2, reason rows=%3")
                            .arg(missingStatusRows)
                            .arg(statusUnavailableLimitRows)
                            .arg(statusReasonRows)));
    QVERIFY2(uidUnavailableLimitRows >= 1, "Expected uidUnavailable limitation with scoped direction");
    Q_UNUSED(sawInputConnectedOrAdvertising);

    Json sourceIdentityOnlyArgs = {
        {"kind", "sources"},
        {"pluginName", m_probeName.toStdString()},
        {"inputUniverseID", 0},
        {"limit", 50}
    };
    const Json sourceIdentityOnly = parseInvoke(tmInput.invoke("vc_query_controls", sourceIdentityOnlyArgs));
    QCOMPARE(QString::fromStdString(sourceIdentityOnly.value("code", std::string())), QString());
    QVERIFY(sourceIdentityOnly.contains("rows") && sourceIdentityOnly["rows"].is_array());
    QVERIFY(!sourceIdentityOnly["rows"].empty());
    const Json sourceIdentityRow = sourceIdentityOnly["rows"].at(0);
    QVERIFY(!sourceIdentityRow.contains("inputLine"));
    QVERIFY(!sourceIdentityRow.contains("outputLine"));
    QVERIFY(!sourceIdentityRow.contains("status"));
    QVERIFY(!sourceIdentityRow.contains("configurationDigest"));

    VCBridge::AuthoringTargetControlQuery sourceTyped;
    sourceTyped.kind = VCBridge::AuthoringTargetControlQuery::Kind::Sources;
    sourceTyped.pluginName = m_probeName;
    sourceTyped.inputUniverseID = 0;
    sourceTyped.fields = QStringList{
        QStringLiteral("identity"),
        QStringLiteral("patches"),
        QStringLiteral("status")
    };
    sourceTyped.limit = 50;
    const VCBridge::AuthoringReadResult sourceDirect =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(sourceTyped));
    QVERIFY2(sourcesTool == VCAuthoring::toJson(sourceDirect),
             "Tool controls-sources JSON must match direct typed controls JSON");

    Json profileArgs = {
        {"kind", "profiles"},
        {"profileName", profileName.toStdString()},
        {"fields", Json::array({"identity", "channelMap", "feedbackTables"})},
        {"limit", 250}
    };
    const Json profilesTool = parseInvoke(tmInput.invoke("vc_query_controls", profileArgs));
    QCOMPARE(QString::fromStdString(profilesTool.value("code", std::string())), QString());
    QVERIFY(profilesTool.contains("rows") && profilesTool["rows"].is_array());
    QVERIFY(!profilesTool["rows"].empty());
    bool sawProfileChannel49 = false;
    bool sawColorTableZero = false;
    for (const Json &row : profilesTool["rows"])
    {
        if (!row.is_object())
            continue;
        if (row.value("kind", std::string()) == "profileChannel" &&
            row.value("inputChannel", -1) == 48 &&
            row.value("profileChannelNumber", -1) == 49)
        {
            sawProfileChannel49 = true;
        }
        if (row.value("kind", std::string()) == "profileFeedbackTable" &&
            row.value("table", std::string()) == "color" &&
            row.value("key", -1) == 0)
        {
            sawColorTableZero = row.value("label", std::string()) == "Off" &&
                row.value("color", std::string()) == "#000000";
        }
    }
    QVERIFY2(sawProfileChannel49, "Expected profile channel mapping 48 -> 49");
    QVERIFY2(sawColorTableZero, "Expected color feedback table key 0 Off/#000000");

    Json profileIdentityOnlyArgs = {
        {"kind", "profiles"},
        {"profileName", profileName.toStdString()},
        {"limit", 250}
    };
    const Json profileIdentityOnly = parseInvoke(tmInput.invoke("vc_query_controls", profileIdentityOnlyArgs));
    QCOMPARE(QString::fromStdString(profileIdentityOnly.value("code", std::string())), QString());
    QVERIFY(profileIdentityOnly.contains("rows") && profileIdentityOnly["rows"].is_array());
    QVERIFY(!profileIdentityOnly["rows"].empty());
    for (const Json &row : profileIdentityOnly["rows"])
    {
        QVERIFY(row.value("kind", std::string()) == "profile");
    }

    VCBridge::AuthoringTargetControlQuery profileTyped;
    profileTyped.kind = VCBridge::AuthoringTargetControlQuery::Kind::Profiles;
    profileTyped.profileName = profileName;
    profileTyped.fields = QStringList{
        QStringLiteral("identity"),
        QStringLiteral("channelMap"),
        QStringLiteral("feedbackTables")
    };
    profileTyped.limit = 250;
    const VCBridge::AuthoringReadResult profileDirect =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(profileTyped));
    QVERIFY2(profilesTool == VCAuthoring::toJson(profileDirect),
             "Tool controls-profiles JSON must match direct typed controls JSON");

    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
    QCOMPARE(button->recordingId(), buttonControlIdBefore);
    QCOMPARE(slider->recordingId(), sliderControlIdBefore);
    QCOMPARE(show->isRunning(), recorderRunningBefore);
}

void VCAuthoringOwner_Test::authoringCore_registeredBindingApply_roundtrip_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    QVERIFY(ioMap != nullptr);
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(frame);
    page0->addWidgetToPageMap(frame);
    frame->setLogicalGeometry(QRectF(0, 0, 420, 320));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(30, 40, 100, 80));
    button->setCaption(QStringLiteral("S2B1 target"));
    button->setFunctionID(Function::invalidId());

    VCSlider *slider = new VCSlider(doc, frame);
    vc->addWidgetToMap(slider);
    frame->addWidgetToPageMap(slider);
    slider->setLogicalGeometry(QRectF(180, 40, 120, 120));
    slider->setSliderMode(VCSlider::Level);

    Scene *seedScene = new Scene(doc);
    seedScene->setName(QStringLiteral("S2B1 Seed"));
    QVERIFY(doc->addFunction(seedScene));
    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("S2B1 Scene"));
    QVERIFY(doc->addFunction(scene));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    const quint32 sliderId = slider->id();
    const quint32 seedSceneId = seedScene->id();
    const quint32 sceneId = scene->id();
    auto cleanup = qScopeGuard([&]() {
        doc->deleteFunction(seedSceneId);
        doc->deleteFunction(sceneId);
        vc->deleteVCWidgets({sliderId, buttonId, frameId});
    });

    fastmcpp::tools::ToolManager tmInput;
    fastmcpp::tools::ToolManager tmUpdate;
    fastmcpp::tools::ToolManager tmQuery;
    registerVCInputTools(tmInput, doc, m_bridge.get());
    registerVCUpdateTools(tmUpdate, doc, m_bridge.get());
    registerQueryTools(tmQuery, doc, m_bridge.get());
    QVERIFY2(tmInput.has("vc_prepare_bindings"), "Missing vc_prepare_bindings");
    QVERIFY2(tmUpdate.has("vc_apply_preparation"), "Missing vc_apply_preparation");
    QVERIFY2(tmQuery.has("vc_query_widgets"), "Missing vc_query_widgets");
    QVERIFY2(tmInput.has("vc_query_controls"), "Missing vc_query_controls");

    auto findItemByWidget = [](const Json &result, quint32 widgetID) -> Json {
        if (!result.contains("items") || !result["items"].is_array())
            return Json::object();
        for (const Json &item : result["items"])
        {
            if (!item.is_object())
                continue;
            if (item.value("targetWidgetID", -1) == int(widgetID))
                return item;
        }
        return Json::object();
    };
    auto findChange = [](const Json &item, const std::string &field) -> Json {
        if (!item.is_object() || !item.contains("changes") || !item["changes"].is_array())
            return Json::object();
        for (const Json &change : item["changes"])
        {
            if (!change.is_object())
                continue;
            if (change.value("field", std::string()) == field)
                return change;
        }
        return Json::object();
    };

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    const quint32 functionBefore = button->functionID();
    const QMap<QKeySequence, quint32> keysBefore = button->keySequenceMap();

    Json prepSet = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "set"},
            {"functionID", sceneId}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepSet.value("code", std::string())), QString());
    QVERIFY(prepSet.contains("preparationToken"));
    QCOMPARE(QString::fromStdString(prepSet.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    const QString setToken = QString::fromStdString(prepSet.value("preparationToken", std::string()));
    QVERIFY(!setToken.isEmpty());
    QCOMPARE(button->functionID(), functionBefore);
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(doc->isModified(), dirtyBefore);
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);

    Json applySet = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", setToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applySet.value("code", std::string())), QString());
    QCOMPARE(QString::fromStdString(applySet.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QVERIFY2(applySet.value("applied", false), applySet.dump().c_str());
    QCOMPARE(applySet.value("tokenConsumed", false), true);
    QCOMPARE(button->functionID(), sceneId);
    QCOMPARE(doc->isModified(), true);
    const Json applySetItem = findItemByWidget(applySet, buttonId);
    QVERIFY(applySetItem.is_object());
    QCOMPARE(applySetItem.value("targetWidgetID", -1), int(buttonId));
    QCOMPARE(applySetItem.value("outcome", std::string()), std::string("applied"));
    QCOMPARE(applySetItem.value("selectedFunctionBeforeID", 0ull), qulonglong(functionBefore));
    QCOMPARE(applySetItem.value("selectedFunctionAfterID", 0ull), qulonglong(sceneId));
    QCOMPARE(applySetItem.value("selectedFunctionID", 0ull), qulonglong(sceneId));
    QCOMPARE(applySetItem.value("outcome", std::string()), std::string("applied"));
    const Json applySetFunctionChange = findChange(applySetItem, "functionID");
    QVERIFY(applySetFunctionChange.is_object());
    QCOMPARE(applySetFunctionChange.value("before", 0ull), qulonglong(functionBefore));
    QCOMPARE(applySetFunctionChange.value("after", 0ull), qulonglong(sceneId));

    Json functionReadback = parseInvoke(tmQuery.invoke("vc_query_widgets", Json{
        {"widgetIDs", Json::array({buttonId})}
    }));
    QVERIFY(functionReadback.is_array());
    QVERIFY(!functionReadback.empty());
    QCOMPARE(functionReadback.at(0).value("id", -1), int(buttonId));
    QCOMPARE(functionReadback.at(0).value("functionID", -1), int(sceneId));

    Json controlsReadback = parseInvoke(tmInput.invoke("vc_query_controls", Json{
        {"kind", "targets"},
        {"widgetIDs", Json::array({buttonId})},
        {"fields", Json::array({"identity", "domains"})},
        {"limit", 50}
    }));
    QCOMPARE(QString::fromStdString(controlsReadback.value("code", std::string())), QString());
    QVERIFY(controlsReadback.contains("rows"));
    QVERIFY(controlsReadback["rows"].is_array());
    bool sawButtonSlot0 = false;
    for (const Json &row : controlsReadback["rows"])
    {
        if (row.value("widgetID", -1) == int(buttonId) && row.value("controlSlotID", -1) == 0)
        {
            sawButtonSlot0 = true;
            QCOMPARE(QString::fromStdString(row.value("valueDomain", std::string())),
                     QStringLiteral("triggerNonZero"));
            QCOMPARE(row.value("keyboardAllowed", false), true);
        }
    }
    QVERIFY2(sawButtonSlot0, "Expected controls readback for button slot 0");

    Json prepFunctionNoop = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "set"},
            {"functionID", sceneId}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepFunctionNoop.value("code", std::string())), QString());
    const QString functionNoopToken = QString::fromStdString(prepFunctionNoop.value("preparationToken", std::string()));
    QVERIFY(!functionNoopToken.isEmpty());
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);
    Json applyFunctionNoop = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", functionNoopToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyFunctionNoop.value("code", std::string())), QString());
    QCOMPARE(QString::fromStdString(applyFunctionNoop.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QCOMPARE(applyFunctionNoop.value("tokenConsumed", false), true);
    QCOMPARE(applyFunctionNoop.value("applied", false), false);
    const Json applyFunctionNoopItem = findItemByWidget(applyFunctionNoop, buttonId);
    QVERIFY(applyFunctionNoopItem.is_object());
    QCOMPARE(applyFunctionNoopItem.value("outcome", std::string()), std::string("unchanged"));
    QCOMPARE(applyFunctionNoopItem.value("changedCount", -1), 0);
    QCOMPARE(doc->isModified(), false);

    Json prepClear = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "clear"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepClear.value("code", std::string())), QString());
    QCOMPARE(QString::fromStdString(prepClear.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    const QString clearToken = QString::fromStdString(prepClear.value("preparationToken", std::string()));
    QVERIFY(!clearToken.isEmpty());
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);

    Json applyClear = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", clearToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyClear.value("code", std::string())), QString());
    QCOMPARE(QString::fromStdString(applyClear.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QCOMPARE(applyClear.value("tokenConsumed", false), true);
    QCOMPARE(applyClear.value("applied", false), true);
    const Json applyClearItem = findItemByWidget(applyClear, buttonId);
    QVERIFY(applyClearItem.is_object());
    QCOMPARE(applyClearItem.value("outcome", std::string()), std::string("applied"));
    QCOMPARE(applyClearItem.value("selectedFunctionBeforeID", 0ull), qulonglong(sceneId));
    QCOMPARE(applyClearItem.value("selectedFunctionAfterID", 0ull), qulonglong(quint32(Function::invalidId())));
    const Json applyClearFunctionChange = findChange(applyClearItem, "functionID");
    QVERIFY(applyClearFunctionChange.is_object());
    QCOMPARE(applyClearFunctionChange.value("before", 0ull), qulonglong(sceneId));
    QCOMPARE(applyClearFunctionChange.value("after", 0ull), qulonglong(quint32(Function::invalidId())));
    QCOMPARE(button->functionID(), quint32(Function::invalidId()));
    QCOMPARE(doc->isModified(), true);

    functionReadback = parseInvoke(tmQuery.invoke("vc_query_widgets", Json{
        {"widgetIDs", Json::array({buttonId})}
    }));
    QVERIFY(functionReadback.is_array());
    QVERIFY(!functionReadback.empty());
    QCOMPARE(functionReadback.at(0).value("id", -1), int(buttonId));
    QVERIFY(!functionReadback.at(0).contains("functionID"));

    Json prepKeySet = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "key"},
            {"action", "set"},
            {"controlSlotID", 0},
            {"keySequence", "Ctrl+Alt+K"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepKeySet.value("code", std::string())), QString());
    QCOMPARE(QString::fromStdString(prepKeySet.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    const QString keySetToken = QString::fromStdString(prepKeySet.value("preparationToken", std::string()));
    QVERIFY(!keySetToken.isEmpty());
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);

    Json applyKeySet = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", keySetToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyKeySet.value("code", std::string())), QString());
    QCOMPARE(QString::fromStdString(applyKeySet.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QCOMPARE(applyKeySet.value("tokenConsumed", false), true);
    QCOMPARE(applyKeySet.value("applied", false), true);
    const Json applyKeySetItem = findItemByWidget(applyKeySet, buttonId);
    QVERIFY(applyKeySetItem.is_object());
    QCOMPARE(applyKeySetItem.value("outcome", std::string()), std::string("applied"));
    QCOMPARE(applyKeySetItem.value("selectedControlSlotID", -1), 0);
    const Json applyKeySetChange = findChange(applyKeySetItem, "keySequence");
    QVERIFY(applyKeySetChange.is_object());
    QCOMPARE(QString::fromStdString(applyKeySetChange.value("before", std::string())), QString());
    QCOMPARE(QString::fromStdString(applyKeySetChange.value("after", std::string())), QStringLiteral("Ctrl+Alt+K"));
    const QMap<QKeySequence, quint32> keysAfterSet = button->keySequenceMap();
    QVERIFY(keysAfterSet.contains(QKeySequence(QStringLiteral("Ctrl+Alt+K"))));
    QCOMPARE(keysAfterSet.value(QKeySequence(QStringLiteral("Ctrl+Alt+K"))), quint32(0));
    QCOMPARE(doc->isModified(), true);

    Json prepKeyRemove = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "key"},
            {"action", "remove"},
            {"controlSlotID", 0},
            {"keySequence", "Ctrl+Alt+K"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepKeyRemove.value("code", std::string())), QString());
    QCOMPARE(QString::fromStdString(prepKeyRemove.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    const Json prepKeyRemoveItem = findItemByWidget(prepKeyRemove, buttonId);
    QVERIFY(prepKeyRemoveItem.is_object());
    const Json prepKeyRemoveChange = findChange(prepKeyRemoveItem, "keySequence");
    QVERIFY(prepKeyRemoveChange.is_object());
    QCOMPARE(QString::fromStdString(prepKeyRemoveChange.value("before", std::string())),
             QStringLiteral("Ctrl+Alt+K"));
    QCOMPARE(QString::fromStdString(prepKeyRemoveChange.value("after", std::string())),
             QString());
    const QString keyRemoveToken = QString::fromStdString(prepKeyRemove.value("preparationToken", std::string()));
    QVERIFY(!keyRemoveToken.isEmpty());
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);

    Json applyKeyRemove = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", keyRemoveToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyKeyRemove.value("code", std::string())), QString());
    QCOMPARE(QString::fromStdString(applyKeyRemove.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QCOMPARE(applyKeyRemove.value("tokenConsumed", false), true);
    QCOMPARE(applyKeyRemove.value("applied", false), true);
    const Json applyKeyRemoveItem = findItemByWidget(applyKeyRemove, buttonId);
    QVERIFY(applyKeyRemoveItem.is_object());
    QCOMPARE(applyKeyRemoveItem.value("outcome", std::string()), std::string("applied"));
    QCOMPARE(applyKeyRemoveItem.value("selectedControlSlotID", -1), 0);
    const Json applyKeyRemoveChange = findChange(applyKeyRemoveItem, "keySequence");
    QVERIFY(applyKeyRemoveChange.is_object());
    QCOMPARE(QString::fromStdString(applyKeyRemoveChange.value("before", std::string())),
             QStringLiteral("Ctrl+Alt+K"));
    QCOMPARE(QString::fromStdString(applyKeyRemoveChange.value("after", std::string())),
             QString());
    const QMap<QKeySequence, quint32> keysAfterRemove = button->keySequenceMap();
    QVERIFY(!keysAfterRemove.contains(QKeySequence(QStringLiteral("Ctrl+Alt+K"))));
    QCOMPARE(keysAfterRemove, keysBefore);
    QCOMPARE(doc->isModified(), true);

    Json prepNoop = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "key"},
            {"action", "set"},
            {"controlSlotID", 0},
            {"keySequence", "Ctrl+Alt+K"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepNoop.value("code", std::string())), QString());
    const QString noopToken = QString::fromStdString(prepNoop.value("preparationToken", std::string()));
    QVERIFY(!noopToken.isEmpty());
    Json applyNoopSet = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", noopToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyNoopSet.value("code", std::string())), QString());
    QCOMPARE(applyNoopSet.value("applied", false), true);
    QVERIFY(button->keySequenceMap().contains(QKeySequence(QStringLiteral("Ctrl+Alt+K"))));

    Json prepNoopRemove = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "key"},
            {"action", "remove"},
            {"controlSlotID", 0},
            {"keySequence", "Ctrl+Alt+K"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepNoopRemove.value("code", std::string())), QString());
    const QString noopRemoveToken = QString::fromStdString(prepNoopRemove.value("preparationToken", std::string()));
    QVERIFY(!noopRemoveToken.isEmpty());
    Json applyNoopRemovePrep = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", noopRemoveToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyNoopRemovePrep.value("code", std::string())), QString());
    QCOMPARE(applyNoopRemovePrep.value("applied", false), true);
    QVERIFY(!button->keySequenceMap().contains(QKeySequence(QStringLiteral("Ctrl+Alt+K"))));

    Json prepNoopRemove2 = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "key"},
            {"action", "remove"},
            {"controlSlotID", 0},
            {"keySequence", "Ctrl+Alt+K"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepNoopRemove2.value("code", std::string())), QString());
    const QString noopToken2 = QString::fromStdString(prepNoopRemove2.value("preparationToken", std::string()));
    QVERIFY(!noopToken2.isEmpty());
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);
    Json applyNoop = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", noopToken2.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyNoop.value("code", std::string())), QString());
    QCOMPARE(QString::fromStdString(applyNoop.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QCOMPARE(applyNoop.value("tokenConsumed", false), true);
    QCOMPARE(applyNoop.value("applied", false), false);
    const Json applyNoopItem = findItemByWidget(applyNoop, buttonId);
    QVERIFY(applyNoopItem.is_object());
    QCOMPARE(applyNoopItem.value("outcome", std::string()), std::string("unchanged"));
    QCOMPARE(doc->isModified(), false);

    Json prepSetFunctionForPersistence = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "set"},
            {"functionID", sceneId}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepSetFunctionForPersistence.value("code", std::string())), QString());
    const QString setFunctionPersistenceToken =
        QString::fromStdString(prepSetFunctionForPersistence.value("preparationToken", std::string()));
    QVERIFY(!setFunctionPersistenceToken.isEmpty());
    Json applySetFunctionForPersistence = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", setFunctionPersistenceToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applySetFunctionForPersistence.value("code", std::string())), QString());
    QCOMPARE(applySetFunctionForPersistence.value("applied", false), true);

    Json prepSetKeyForPersistence = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "key"},
            {"action", "set"},
            {"controlSlotID", 0},
            {"keySequence", "Ctrl+Alt+K"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepSetKeyForPersistence.value("code", std::string())), QString());
    const QString setKeyPersistenceToken =
        QString::fromStdString(prepSetKeyForPersistence.value("preparationToken", std::string()));
    QVERIFY(!setKeyPersistenceToken.isEmpty());
    Json applySetKeyForPersistence = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", setKeyPersistenceToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applySetKeyForPersistence.value("code", std::string())), QString());
    QCOMPARE(applySetKeyForPersistence.value("applied", false), true);

    QCOMPARE(button->functionID(), sceneId);
    QCOMPARE(button->keySequenceMap().value(QKeySequence(QStringLiteral("Ctrl+Alt+K"))), quint32(0));

    QByteArray appliedXml;
    {
        QBuffer appliedBuffer(&appliedXml);
        QVERIFY(appliedBuffer.open(QIODevice::WriteOnly));
        QXmlStreamWriter appliedWriter(&appliedBuffer);
        appliedWriter.writeStartDocument();
        QVERIFY(button->saveXML(&appliedWriter));
        appliedWriter.writeEndDocument();
        appliedBuffer.close();
    }
    std::unique_ptr<VCButton> loadedApplied = std::make_unique<VCButton>(doc, nullptr);
    QBuffer appliedReadBuffer(&appliedXml);
    QVERIFY(appliedReadBuffer.open(QIODevice::ReadOnly));
    QXmlStreamReader appliedReader(&appliedReadBuffer);
    appliedReader.readNextStartElement();
    QVERIFY2(!appliedReader.atEnd(), "Invalid button XML payload");
    QVERIFY2(loadedApplied->loadXML(appliedReader), "VCButton::loadXML failed");
    QVERIFY2(!appliedReader.hasError(), qPrintable(appliedReader.errorString()));
    QCOMPARE(loadedApplied->id(), buttonId);
    appliedReadBuffer.close();
    QCOMPARE(loadedApplied->functionID(), sceneId);
    QCOMPARE(loadedApplied->actionType(), button->actionType());
    QCOMPARE(loadedApplied->keySequenceMap().value(QKeySequence(QStringLiteral("Ctrl+Alt+K"))), quint32(0));

    Json prepClearFunctionForPersistence = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "clear"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepClearFunctionForPersistence.value("code", std::string())), QString());
    const QString clearFunctionPersistenceToken =
        QString::fromStdString(prepClearFunctionForPersistence.value("preparationToken", std::string()));
    QVERIFY(!clearFunctionPersistenceToken.isEmpty());
    Json applyClearFunctionForPersistence = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", clearFunctionPersistenceToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyClearFunctionForPersistence.value("code", std::string())), QString());
    QCOMPARE(applyClearFunctionForPersistence.value("applied", false), true);

    Json prepRemoveKeyForPersistence = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "key"},
            {"action", "remove"},
            {"controlSlotID", 0},
            {"keySequence", "Ctrl+Alt+K"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepRemoveKeyForPersistence.value("code", std::string())), QString());
    const QString removeKeyPersistenceToken =
        QString::fromStdString(prepRemoveKeyForPersistence.value("preparationToken", std::string()));
    QVERIFY(!removeKeyPersistenceToken.isEmpty());
    Json applyRemoveKeyForPersistence = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", removeKeyPersistenceToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyRemoveKeyForPersistence.value("code", std::string())), QString());
    QCOMPARE(applyRemoveKeyForPersistence.value("applied", false), true);

    QCOMPARE(button->functionID(), quint32(Function::invalidId()));
    QVERIFY(!button->keySequenceMap().contains(QKeySequence(QStringLiteral("Ctrl+Alt+K"))));

    QByteArray clearedXml;
    {
        QBuffer clearedBuffer(&clearedXml);
        QVERIFY(clearedBuffer.open(QIODevice::WriteOnly));
        QXmlStreamWriter clearedWriter(&clearedBuffer);
        clearedWriter.writeStartDocument();
        QVERIFY(button->saveXML(&clearedWriter));
        clearedWriter.writeEndDocument();
        clearedBuffer.close();
    }
    std::unique_ptr<VCButton> loadedCleared = std::make_unique<VCButton>(doc, nullptr);
    QBuffer clearedReadBuffer(&clearedXml);
    QVERIFY(clearedReadBuffer.open(QIODevice::ReadOnly));
    QXmlStreamReader clearedReader(&clearedReadBuffer);
    clearedReader.readNextStartElement();
    QVERIFY2(!clearedReader.atEnd(), "Invalid button XML payload");
    QVERIFY2(loadedCleared->loadXML(clearedReader), "VCButton::loadXML failed");
    QVERIFY2(!clearedReader.hasError(), qPrintable(clearedReader.errorString()));
    QCOMPARE(loadedCleared->id(), buttonId);
    clearedReadBuffer.close();
    QCOMPARE(loadedCleared->functionID(), quint32(Function::invalidId()));
    QCOMPARE(loadedCleared->actionType(), button->actionType());
    QVERIFY(!loadedCleared->keySequenceMap().contains(QKeySequence(QStringLiteral("Ctrl+Alt+K"))));

    const QString controllerProfilePath = m_workspaceDir.filePath(QStringLiteral("s2b2-controller-persist%1")
                                                                   .arg(KExtInputProfile));
    QFile controllerProfileFile(controllerProfilePath);
    QVERIFY2(controllerProfileFile.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(controllerProfilePath)));
    const QByteArray controllerProfileXml = QByteArrayLiteral(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE InputProfile>\n"
        "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
        " <Creator><Name>S2B2</Name><Version>1</Version><Author>test</Author></Creator>\n"
        " <Manufacturer>S2B2</Manufacturer>\n"
        " <Model>ControllerPersistence</Model>\n"
        " <Type>MIDI</Type>\n"
        " <Channel Number=\"7\"><Name>Ch7</Name><Type>Slider</Type></Channel>\n"
        " <Channel Number=\"8\"><Name>Ch8</Name><Type>Slider</Type></Channel>\n"
        "</InputProfile>\n");
    QCOMPARE(controllerProfileFile.write(controllerProfileXml), controllerProfileXml.size());
    controllerProfileFile.close();
    std::unique_ptr<QLCInputProfile> controllerProfileLoaded(QLCInputProfile::loader(controllerProfilePath));
    QVERIFY2(controllerProfileLoaded != nullptr, "Controller persistence profile loader failed");
    const QString controllerProfileName = controllerProfileLoaded->name();
    ioMap->loadProfiles(QDir(m_workspaceDir.path()));
    QVERIFY(ioMap->profileNames().contains(controllerProfileName));
    QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, controllerProfileName),
             "Failed to patch input profile for controller persistence");

    auto querySlotBindings = [&](quint32 widgetId, quint32 slotId) -> Json {
        Json controls = parseInvoke(tmInput.invoke("vc_query_controls", Json{
            {"kind", "targets"},
            {"widgetIDs", Json::array({widgetId})},
            {"fields", Json::array({"identity", "bindings"})},
            {"limit", 100}
        }));
        if (!controls.contains("rows") || !controls["rows"].is_array())
            return Json::array();
        for (const Json &row : controls["rows"])
        {
            if (!row.is_object())
                continue;
            if (row.value("widgetID", -1) != int(widgetId))
                continue;
            if (row.value("controlSlotID", -1) != int(slotId))
                continue;
            if (row.contains("inputBindings") && row["inputBindings"].is_array())
                return row["inputBindings"];
            return Json::array();
        }
        return Json::array();
    };

    Json prepControllerAppend = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "controller"},
            {"action", "append"},
            {"controlSlotID", 0},
            {"destination", {{"inputUniverseID", 0}, {"inputChannel", 7}, {"profileName", controllerProfileName.toStdString()}}}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepControllerAppend.value("code", std::string())), QString());
    const QString controllerAppendToken =
        QString::fromStdString(prepControllerAppend.value("preparationToken", std::string()));
    QVERIFY(!controllerAppendToken.isEmpty());
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);
    Json applyControllerAppend = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", controllerAppendToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyControllerAppend.value("code", std::string())), QString());
    QCOMPARE(applyControllerAppend.value("applied", false), true);
    QCOMPARE(applyControllerAppend.value("tokenConsumed", false), true);
    const Json appendItem = findItemByWidget(applyControllerAppend, buttonId);
    QVERIFY(appendItem.is_object());
    QCOMPARE(appendItem.value("outcome", std::string()), std::string("applied"));
    const Json appendChange = findChange(appendItem, "sourceKey");
    QVERIFY(appendChange.is_object());
    QCOMPARE(button->inputSources().size(), 1);
    QCOMPARE(button->inputSources().at(0)->id(), quint32(0));
    QCOMPARE(button->inputSources().at(0)->universe(), quint32(0));
    QCOMPARE(button->inputSources().at(0)->channel() & 0x0000FFFF, quint32(7));
    QCOMPARE(doc->isModified(), true);
    Json appendedBindings = querySlotBindings(buttonId, 0);
    QCOMPARE(int(appendedBindings.size()), 1);
    QCOMPARE(appendedBindings.at(0).value("sourceKey", Json::object()).value("inputUniverseID", -1), 0);
    QCOMPARE(appendedBindings.at(0).value("sourceKey", Json::object()).value("inputChannel", -1), 7);

    Json prepControllerMove = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "controller"},
            {"action", "move"},
            {"controlSlotID", 0},
            {"sourceKey", {{"controlSlotID", 0}, {"inputUniverseID", 0}, {"inputChannel", 7}, {"childSubpage", 0}}},
            {"destination", {{"inputUniverseID", 0}, {"inputChannel", 8}, {"profileName", controllerProfileName.toStdString()}}}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepControllerMove.value("code", std::string())), QString());
    const QString controllerMoveToken =
        QString::fromStdString(prepControllerMove.value("preparationToken", std::string()));
    QVERIFY(!controllerMoveToken.isEmpty());
    Json applyControllerMove = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", controllerMoveToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyControllerMove.value("code", std::string())), QString());
    QCOMPARE(applyControllerMove.value("applied", false), true);
    QCOMPARE(button->inputSources().size(), 1);
    QCOMPARE(button->inputSources().at(0)->channel() & 0x0000FFFF, quint32(8));
    Json movedBindings = querySlotBindings(buttonId, 0);
    QCOMPARE(int(movedBindings.size()), 1);
    QCOMPARE(movedBindings.at(0).value("sourceKey", Json::object()).value("inputChannel", -1), 8);

    QByteArray controllerMovedXml;
    {
        QBuffer controllerBuffer(&controllerMovedXml);
        QVERIFY(controllerBuffer.open(QIODevice::WriteOnly));
        QXmlStreamWriter controllerWriter(&controllerBuffer);
        controllerWriter.writeStartDocument();
        QVERIFY(button->saveXML(&controllerWriter));
        controllerWriter.writeEndDocument();
        controllerBuffer.close();
    }
    std::unique_ptr<VCButton> loadedControllerMoved = std::make_unique<VCButton>(doc, nullptr);
    QBuffer controllerMovedReadBuffer(&controllerMovedXml);
    QVERIFY(controllerMovedReadBuffer.open(QIODevice::ReadOnly));
    QXmlStreamReader controllerMovedReader(&controllerMovedReadBuffer);
    controllerMovedReader.readNextStartElement();
    QVERIFY2(!controllerMovedReader.atEnd(), "Invalid controller-moved XML payload");
    QVERIFY2(loadedControllerMoved->loadXML(controllerMovedReader), "VCButton::loadXML failed for controller-moved state");
    QVERIFY2(!controllerMovedReader.hasError(), qPrintable(controllerMovedReader.errorString()));
    QCOMPARE(loadedControllerMoved->inputSources().size(), 1);
    QCOMPARE(loadedControllerMoved->inputSources().at(0)->universe(), quint32(0));
    QCOMPARE(loadedControllerMoved->inputSources().at(0)->channel() & 0x0000FFFF, quint32(8));
    controllerMovedReadBuffer.close();

    const QString staleProfilePath = m_workspaceDir.filePath(QStringLiteral("s2b2-controller-stale%1")
                                                              .arg(KExtInputProfile));
    QFile staleProfileFile(staleProfilePath);
    QVERIFY2(staleProfileFile.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(QStringLiteral("Failed to create stale profile fixture at %1").arg(staleProfilePath)));
    const QByteArray staleProfileXml = QByteArrayLiteral(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE InputProfile>\n"
        "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
        " <Creator><Name>S2B2</Name><Version>1</Version><Author>test</Author></Creator>\n"
        " <Manufacturer>S2B2</Manufacturer>\n"
        " <Model>ControllerStale</Model>\n"
        " <Type>MIDI</Type>\n"
        " <Channel Number=\"8\"><Name>Ch8Btn</Name><Type>Button</Type></Channel>\n"
        "</InputProfile>\n");
    QCOMPARE(staleProfileFile.write(staleProfileXml), staleProfileXml.size());
    staleProfileFile.close();
    std::unique_ptr<QLCInputProfile> staleProfileLoaded(QLCInputProfile::loader(staleProfilePath));
    QVERIFY2(staleProfileLoaded != nullptr, "Controller stale profile loader failed");
    const QString staleProfileName = staleProfileLoaded->name();
    ioMap->loadProfiles(QDir(m_workspaceDir.path()));
    QVERIFY(ioMap->profileNames().contains(staleProfileName));

    Json prepStaleController = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "controller"},
            {"action", "append"},
            {"controlSlotID", 0},
            {"destination", {{"inputUniverseID", 0}, {"inputChannel", 8}, {"profileName", controllerProfileName.toStdString()}}}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepStaleController.value("code", std::string())), QString());
    const QString staleControllerToken =
        QString::fromStdString(prepStaleController.value("preparationToken", std::string()));
    QVERIFY(!staleControllerToken.isEmpty());
    QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, staleProfileName),
             "Failed to repatch stale profile");
    Json applyStaleController = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", staleControllerToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyStaleController.value("code", std::string())),
             QStringLiteral("staleSelection"));
    QCOMPARE(applyStaleController.value("tokenConsumed", false), true);
    QCOMPARE(button->inputSources().size(), 1);
    QCOMPARE(button->inputSources().at(0)->channel() & 0x0000FFFF, quint32(8));
    QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, controllerProfileName),
             "Failed to restore controller profile");

    Json prepControllerRemove = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "controller"},
            {"action", "remove"},
            {"controlSlotID", 0},
            {"sourceKey", {{"controlSlotID", 0}, {"inputUniverseID", 0}, {"inputChannel", 8}, {"childSubpage", 0}}}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepControllerRemove.value("code", std::string())), QString());
    const QString controllerRemoveToken =
        QString::fromStdString(prepControllerRemove.value("preparationToken", std::string()));
    QVERIFY(!controllerRemoveToken.isEmpty());
    Json applyControllerRemove = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", controllerRemoveToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyControllerRemove.value("code", std::string())), QString());
    QCOMPARE(applyControllerRemove.value("applied", false), true);
    QCOMPARE(button->inputSources().size(), 0);
    Json removedBindings = querySlotBindings(buttonId, 0);
    QCOMPARE(int(removedBindings.size()), 0);

    QByteArray controllerClearedXml;
    {
        QBuffer controllerBuffer(&controllerClearedXml);
        QVERIFY(controllerBuffer.open(QIODevice::WriteOnly));
        QXmlStreamWriter controllerWriter(&controllerBuffer);
        controllerWriter.writeStartDocument();
        QVERIFY(button->saveXML(&controllerWriter));
        controllerWriter.writeEndDocument();
        controllerBuffer.close();
    }
    std::unique_ptr<VCButton> loadedControllerCleared = std::make_unique<VCButton>(doc, nullptr);
    QBuffer controllerReadBuffer(&controllerClearedXml);
    QVERIFY(controllerReadBuffer.open(QIODevice::ReadOnly));
    QXmlStreamReader controllerReader(&controllerReadBuffer);
    controllerReader.readNextStartElement();
    QVERIFY2(!controllerReader.atEnd(), "Invalid controller-cleared XML payload");
    QVERIFY2(loadedControllerCleared->loadXML(controllerReader), "VCButton::loadXML failed for controller-cleared state");
    QVERIFY2(!controllerReader.hasError(), qPrintable(controllerReader.errorString()));
    QCOMPARE(loadedControllerCleared->inputSources().size(), 0);
    controllerReadBuffer.close();

    Json prepStaleBasis = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "set"},
            {"functionID", sceneId}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepStaleBasis.value("code", std::string())), QString());
    const QString staleBasisToken = QString::fromStdString(prepStaleBasis.value("preparationToken", std::string()));
    QVERIFY(!staleBasisToken.isEmpty());
    const int pageCountBeforeTemp = vc->pagesCount();
    vc->addPage(pageCountBeforeTemp);
    QCOMPARE(vc->pagesCount(), pageCountBeforeTemp + 1);
    Json applyStaleBasis = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", staleBasisToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyStaleBasis.value("code", std::string())), QStringLiteral("staleBasis"));
    QCOMPARE(QString::fromStdString(applyStaleBasis.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QCOMPARE(applyStaleBasis.value("tokenConsumed", false), true);
    vc->deletePage(pageCountBeforeTemp);
    QCOMPARE(vc->pagesCount(), pageCountBeforeTemp);

    Json prepStaleSelection = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "set"},
            {"functionID", sceneId}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepStaleSelection.value("code", std::string())), QString());
    const QString staleSelectionToken = QString::fromStdString(prepStaleSelection.value("preparationToken", std::string()));
    QVERIFY(!staleSelectionToken.isEmpty());
    scene->setName(QStringLiteral("S2B1 Scene mutated"));
    Json applyStaleSelection = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", staleSelectionToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyStaleSelection.value("code", std::string())), QStringLiteral("staleSelection"));
    QCOMPARE(QString::fromStdString(applyStaleSelection.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QCOMPARE(applyStaleSelection.value("tokenConsumed", false), true);
    Json applyStaleSelectionAgain = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", staleSelectionToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyStaleSelectionAgain.value("code", std::string())), QStringLiteral("invalidPreparation"));
    scene->setName(QStringLiteral("S2B1 Scene"));

    m_bridge->setAuthoringNowForTest(1000);
    Json prepExpiry = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "set"},
            {"functionID", sceneId}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepExpiry.value("code", std::string())), QString());
    const QString expiryToken = QString::fromStdString(prepExpiry.value("preparationToken", std::string()));
    QVERIFY(!expiryToken.isEmpty());
    m_bridge->setAuthoringNowForTest(1000 + VCBridge::kAuthoringRecordTtlMs + 2);
    Json applyExpired = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", expiryToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyExpired.value("code", std::string())), QStringLiteral("preparationExpired"));
    QCOMPARE(applyExpired.value("tokenConsumed", false), true);
    m_bridge->clearAuthoringNowForTest();

    Json prepKeyConflict = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", sliderId},
            {"kind", "key"},
            {"action", "set"},
            {"controlSlotID", 2},
            {"keySequence", "Ctrl+Shift+L"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepKeyConflict.value("code", std::string())), QString());
    const QString keyConflictToken = QString::fromStdString(prepKeyConflict.value("preparationToken", std::string()));
    QVERIFY(!keyConflictToken.isEmpty());
    slider->addKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+L")), 1);
    Json applyKeyConflict = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", keyConflictToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyKeyConflict.value("code", std::string())), QStringLiteral("staleSelection"));
    QCOMPARE(QString::fromStdString(applyKeyConflict.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QCOMPARE(applyKeyConflict.value("tokenConsumed", false), true);
    QCOMPARE(applyKeyConflict.value("applied", false), false);
    QVERIFY(slider->keySequenceMap().contains(QKeySequence(QStringLiteral("Ctrl+Shift+L"))));
    QCOMPARE(slider->keySequenceMap().value(QKeySequence(QStringLiteral("Ctrl+Shift+L"))), quint32(1));
    slider->deleteKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+L")));

    Json prepWidgetStale = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "key"},
            {"action", "set"},
            {"controlSlotID", 0},
            {"keySequence", "Ctrl+Shift+M"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prepWidgetStale.value("code", std::string())), QString());
    const QString widgetStaleToken = QString::fromStdString(prepWidgetStale.value("preparationToken", std::string()));
    QVERIFY(!widgetStaleToken.isEmpty());
    frame->removeWidgetFromPageMap(button);
    vc->removeWidgetFromMap(button);
    VCButton *replacement = new VCButton(doc, frame);
    replacement->setID(buttonId);
    replacement->setLogicalGeometry(button->logicalGeometry());
    replacement->setCaption(button->caption());
    frame->addWidgetToPageMap(replacement);
    vc->addWidgetToMap(replacement);
    Json applyWidgetStale = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", widgetStaleToken.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(applyWidgetStale.value("code", std::string())), QStringLiteral("staleBasis"));
    QCOMPARE(QString::fromStdString(applyWidgetStale.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QCOMPARE(applyWidgetStale.value("tokenConsumed", false), true);
    frame->removeWidgetFromPageMap(replacement);
    vc->removeWidgetFromMap(replacement);
    replacement->deleteLater();
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);

    Json preconditionSet = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "set"},
            {"functionID", sceneId}
        }})}
    }));
    QCOMPARE(QString::fromStdString(preconditionSet.value("code", std::string())), QString());
    const QString preconditionToken =
        QString::fromStdString(preconditionSet.value("preparationToken", std::string()));
    QVERIFY(!preconditionToken.isEmpty());

    doc->setMode(Doc::Operate);
    Json applyPrecondition = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", preconditionToken.toStdString()}
    }));
    doc->setMode(Doc::Design);
    QCOMPARE(QString::fromStdString(applyPrecondition.value("code", std::string())),
             QStringLiteral("authoringPrecondition"));
    QCOMPARE(QString::fromStdString(applyPrecondition.value("atomicScope", std::string())),
             QStringLiteral("independentItems"));
    QCOMPARE(applyPrecondition.value("tokenConsumed", false), true);

    // N1: token consumed refusal when selected function transitions to playback after prepare.
    {
        button->setFunctionID(Function::invalidId());
        const QMap<QKeySequence, quint32> n1KeysBefore = button->keySequenceMap();
        Json n1Prep = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
            {"items", Json::array({{
                {"widgetID", buttonId},
                {"kind", "function"},
                {"action", "set"},
                {"functionID", sceneId}
            }})}
        }));
        QCOMPARE(QString::fromStdString(n1Prep.value("code", std::string())), QString());
        const QString n1Token = QString::fromStdString(n1Prep.value("preparationToken", std::string()));
        QVERIFY(!n1Token.isEmpty());
        MasterTimer *n1Timer = doc->masterTimer();
        QVERIFY(n1Timer != nullptr);
        auto n1Cleanup = qScopeGuard([&]() {
            scene->stop(FunctionParent::master());
            n1Timer->stop();
            for (int i = 0; i < 20 && !n1Timer->runningFunctionIds().isEmpty(); ++i)
            {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                QTest::qWait(5);
            }
        });
        n1Timer->start();
        scene->start(n1Timer, FunctionParent::master());
        for (int i = 0;
             i < 40 && !n1Timer->isStartQueued(scene) && !n1Timer->runningFunctionIds().contains(sceneId);
             ++i)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QTest::qWait(5);
        }
        QVERIFY2(n1Timer->isStartQueued(scene) || n1Timer->runningFunctionIds().contains(sceneId),
                 "Scene did not queue or start before apply precondition check");
        doc->resetModified();
        QCOMPARE(doc->isModified(), false);
        Json n1Apply = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
            {"preparationToken", n1Token.toStdString()}
        }));
        QCOMPARE(QString::fromStdString(n1Apply.value("code", std::string())), QStringLiteral("authoringPrecondition"));
        QCOMPARE(QString::fromStdString(n1Apply.value("atomicScope", std::string())), QStringLiteral("independentItems"));
        QCOMPARE(n1Apply.value("tokenConsumed", false), true);
        QCOMPARE(n1Apply.value("applied", false), false);
        QCOMPARE(button->functionID(), quint32(Function::invalidId()));
        QCOMPARE(button->keySequenceMap(), n1KeysBefore);
        QCOMPARE(doc->isModified(), false);
    }

    // N2: protected target page refusal both before prepare and after valid prepare.
    if (page0->PIN() != 0)
    {
        vc->validatePagePIN(0, QStringLiteral("1234"), true);
        QVERIFY(vc->setPagePIN(0, QStringLiteral("1234"), QString()));
    }
    button->setFunctionID(Function::invalidId());
    const QMap<QKeySequence, quint32> n2KeysBefore = button->keySequenceMap();
    QVERIFY(vc->setPagePIN(0, QString(), QStringLiteral("1234")));
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);
    Json n2PrepareDenied = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "set"},
            {"functionID", sceneId}
        }})}
    }));
    QCOMPARE(QString::fromStdString(n2PrepareDenied.value("code", std::string())), QStringLiteral("authoringPrecondition"));
    QCOMPARE(QString::fromStdString(n2PrepareDenied.value("preparationToken", std::string())), QString());
    QCOMPARE(button->functionID(), quint32(Function::invalidId()));
    QCOMPARE(button->keySequenceMap(), n2KeysBefore);
    QCOMPARE(doc->isModified(), false);
    QVERIFY(vc->setPagePIN(0, QStringLiteral("1234"), QString()));

    Json n2Prep = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "set"},
            {"functionID", sceneId}
        }})}
    }));
    QCOMPARE(QString::fromStdString(n2Prep.value("code", std::string())), QString());
    const QString n2Token = QString::fromStdString(n2Prep.value("preparationToken", std::string()));
    QVERIFY(!n2Token.isEmpty());
    QVERIFY(vc->setPagePIN(0, QString(), QStringLiteral("1234")));
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);
    Json n2ApplyDenied = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", n2Token.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(n2ApplyDenied.value("code", std::string())), QStringLiteral("authoringPrecondition"));
    QCOMPARE(QString::fromStdString(n2ApplyDenied.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QCOMPARE(n2ApplyDenied.value("tokenConsumed", false), true);
    QCOMPARE(n2ApplyDenied.value("applied", false), false);
    QCOMPARE(button->functionID(), quint32(Function::invalidId()));
    QCOMPARE(button->keySequenceMap(), n2KeysBefore);
    QCOMPARE(doc->isModified(), false);
    QVERIFY(vc->setPagePIN(0, QStringLiteral("1234"), QString()));

    // N3: recorder/capture transition after prepare must refuse apply, consume token, and keep state unchanged.
    {
        ShowCommandRecorder *n3Recorder = ShowCommandRecorder::instance();
        QVERIFY(n3Recorder != nullptr);
        const bool n3RecordingBaseline = n3Recorder->isRecording();
        const quint32 n3BoundShowBaseline = n3Recorder->boundShowId();
        Show *n3Show = new Show(doc);
        n3Show->setName(QStringLiteral("S2B1 recorder guard show"));
        QVERIFY(doc->addFunction(n3Show));
        const quint32 n3ShowId = n3Show->id();
        auto n3Cleanup = qScopeGuard([&]() {
            if (n3Recorder->isRecording() != n3RecordingBaseline)
                n3Recorder->setRecording(n3RecordingBaseline);
            if (n3Recorder->boundShowId() != n3BoundShowBaseline)
                n3Recorder->setResolvedShow(n3BoundShowBaseline);
            doc->deleteFunction(n3ShowId);
        });

        button->setFunctionID(Function::invalidId());
        const QMap<QKeySequence, quint32> n3KeysBefore = button->keySequenceMap();
        Json n3Prep = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
            {"items", Json::array({{
                {"widgetID", buttonId},
                {"kind", "function"},
                {"action", "set"},
                {"functionID", sceneId}
            }})}
        }));
        QCOMPARE(QString::fromStdString(n3Prep.value("code", std::string())), QString());
        const QString n3Token = QString::fromStdString(n3Prep.value("preparationToken", std::string()));
        QVERIFY(!n3Token.isEmpty());
        QVERIFY(n3Recorder->setResolvedShow(n3ShowId));
        QVERIFY(n3Recorder->setRecording(true));
        QCOMPARE(n3Recorder->isRecording(), true);
        doc->resetModified();
        QCOMPARE(doc->isModified(), false);
        Json n3ApplyDenied = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
            {"preparationToken", n3Token.toStdString()}
        }));
        QCOMPARE(QString::fromStdString(n3ApplyDenied.value("code", std::string())), QStringLiteral("authoringPrecondition"));
        QCOMPARE(QString::fromStdString(n3ApplyDenied.value("atomicScope", std::string())), QStringLiteral("independentItems"));
        QCOMPARE(n3ApplyDenied.value("tokenConsumed", false), true);
        QCOMPARE(n3ApplyDenied.value("applied", false), false);
        QCOMPARE(button->functionID(), quint32(Function::invalidId()));
        QCOMPARE(button->keySequenceMap(), n3KeysBefore);
        QCOMPARE(doc->isModified(), false);
        QVERIFY(n3Recorder->setRecording(false));
        QVERIFY(n3Recorder->setResolvedShow(n3BoundShowBaseline));
    }

    // N4: real bridge requireAtomic binding request must refuse unsupportedPreparation with request-scope unsupportedConfiguration.
    button->setFunctionID(Function::invalidId());
    slider->deleteKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+Y")));
    const QMap<QKeySequence, quint32> n4SliderKeysBefore = slider->keySequenceMap();
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);
    Json n4RequireAtomic = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"requireAtomic", true},
        {"items", Json::array({
            {
                {"widgetID", buttonId},
                {"kind", "function"},
                {"action", "set"},
                {"functionID", sceneId}
            },
            {
                {"widgetID", sliderId},
                {"kind", "key"},
                {"action", "set"},
                {"controlSlotID", 2},
                {"keySequence", "Ctrl+Shift+Y"}
            }
        })}
    }));
    QCOMPARE(QString::fromStdString(n4RequireAtomic.value("code", std::string())), QStringLiteral("unsupportedPreparation"));
    QCOMPARE(QString::fromStdString(n4RequireAtomic.value("preparationToken", std::string())), QString());
    bool n4RequestUnsupported = false;
    if (n4RequireAtomic.contains("limitations") && n4RequireAtomic["limitations"].is_array())
    {
        for (const Json &lim : n4RequireAtomic["limitations"])
        {
            if (!lim.is_object())
                continue;
            if (lim.value("scope", std::string()) == std::string("request") &&
                lim.value("code", std::string()) == std::string("unsupportedConfiguration"))
            {
                n4RequestUnsupported = true;
                break;
            }
        }
    }
    QVERIFY2(n4RequestUnsupported, "Missing request-scope unsupportedConfiguration");
    QCOMPARE(button->functionID(), quint32(Function::invalidId()));
    QCOMPARE(slider->keySequenceMap(), n4SliderKeysBefore);
    QCOMPARE(doc->isModified(), false);

    // N5: native key prepare negatives.
    slider->deleteKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+L")));
    slider->addKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+L")), 1);
    const QMap<QKeySequence, quint32> n5KeysBefore = slider->keySequenceMap();
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);
    Json n5Ambiguous = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", sliderId},
            {"kind", "key"},
            {"action", "set"},
            {"controlSlotID", 2},
            {"keySequence", "Ctrl+Shift+L"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(n5Ambiguous.value("code", std::string())), QStringLiteral("ambiguousSource"));
    QCOMPARE(QString::fromStdString(n5Ambiguous.value("preparationToken", std::string())), QString());
    QCOMPARE(slider->keySequenceMap(), n5KeysBefore);
    QCOMPARE(doc->isModified(), false);

    Json n5Ineligible = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", sliderId},
            {"kind", "key"},
            {"action", "set"},
            {"controlSlotID", 0},
            {"keySequence", "Ctrl+Shift+J"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(n5Ineligible.value("code", std::string())), QStringLiteral("invalidArgument"));
    QCOMPARE(QString::fromStdString(n5Ineligible.value("preparationToken", std::string())), QString());
    QCOMPARE(slider->keySequenceMap(), n5KeysBefore);
    QCOMPARE(doc->isModified(), false);

    Json n5Unregistered = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", sliderId},
            {"kind", "key"},
            {"action", "set"},
            {"controlSlotID", 99},
            {"keySequence", "Ctrl+Shift+K"}
        }})}
    }));
    QCOMPARE(QString::fromStdString(n5Unregistered.value("code", std::string())), QStringLiteral("invalidArgument"));
    QCOMPARE(QString::fromStdString(n5Unregistered.value("preparationToken", std::string())), QString());
    QCOMPARE(slider->keySequenceMap(), n5KeysBefore);
    QCOMPARE(doc->isModified(), false);
    slider->deleteKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+L")));

    // N6: one real two-item token (function + key) must apply in request order with changedCount=2.
    button->setCaption(QStringLiteral("S2B1 target custom"));
    button->setFunctionID(Function::invalidId());
    slider->deleteKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+Y")));
    doc->resetModified();
    QCOMPARE(doc->isModified(), false);
    Json n6Prep = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({
            {
                {"widgetID", buttonId},
                {"kind", "function"},
                {"action", "set"},
                {"functionID", sceneId}
            },
            {
                {"widgetID", sliderId},
                {"kind", "key"},
                {"action", "set"},
                {"controlSlotID", 2},
                {"keySequence", "Ctrl+Shift+Y"}
            }
        })}
    }));
    QCOMPARE(QString::fromStdString(n6Prep.value("code", std::string())), QString());
    const QString n6Token = QString::fromStdString(n6Prep.value("preparationToken", std::string()));
    QVERIFY(!n6Token.isEmpty());
    QCOMPARE(button->functionID(), quint32(Function::invalidId()));
    QVERIFY(!slider->keySequenceMap().contains(QKeySequence(QStringLiteral("Ctrl+Shift+Y"))));
    Json n6Apply = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", n6Token.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(n6Apply.value("code", std::string())), QString());
    QCOMPARE(QString::fromStdString(n6Apply.value("atomicScope", std::string())), QStringLiteral("independentItems"));
    QCOMPARE(n6Apply.value("tokenConsumed", false), true);
    QCOMPARE(n6Apply.value("applied", false), true);
    QCOMPARE(n6Apply.value("changedCount", 2), 2);
    QVERIFY(n6Apply.contains("items"));
    QVERIFY(n6Apply["items"].is_array());
    QCOMPARE(n6Apply["items"].size(), 2u);
    const Json n6Item0 = n6Apply["items"].at(0);
    const Json n6Item1 = n6Apply["items"].at(1);
    QCOMPARE(n6Item0.value("targetWidgetID", -1), int(buttonId));
    QCOMPARE(n6Item0.value("outcome", std::string()), std::string("applied"));
    const Json n6Change0 = findChange(n6Item0, "functionID");
    QVERIFY(n6Change0.is_object());
    QCOMPARE(n6Change0.value("before", 0ull), qulonglong(quint32(Function::invalidId())));
    QCOMPARE(n6Change0.value("after", 0ull), qulonglong(sceneId));
    QCOMPARE(n6Item1.value("targetWidgetID", -1), int(sliderId));
    QCOMPARE(n6Item1.value("outcome", std::string()), std::string("applied"));
    const Json n6Change1 = findChange(n6Item1, "keySequence");
    QVERIFY(n6Change1.is_object());
    QCOMPARE(QString::fromStdString(n6Change1.value("before", std::string())), QString());
    QCOMPARE(QString::fromStdString(n6Change1.value("after", std::string())), QStringLiteral("Ctrl+Shift+Y"));
    QCOMPARE(button->functionID(), sceneId);
    QVERIFY(slider->keySequenceMap().contains(QKeySequence(QStringLiteral("Ctrl+Shift+Y"))));
    QCOMPARE(slider->keySequenceMap().value(QKeySequence(QStringLiteral("Ctrl+Shift+Y"))), quint32(2));
    QCOMPARE(doc->isModified(), true);
    slider->deleteKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+Y")));
    button->setFunctionID(Function::invalidId());
}

void VCAuthoringOwner_Test::authoringCore_functionBinding_captionSideEffect_data()
{
    QTest::addColumn<QString>("captionMode");
    QTest::addColumn<QString>("initialCaption");
    QTest::addColumn<QString>("expectedCaption");
    QTest::addColumn<bool>("expectCaptionChanged");

    QTest::newRow("default-caption")
        << QStringLiteral("default-caption")
        << QStringLiteral("__USE_DEFAULT__")
        << QStringLiteral("Caption Scene")
        << true;
    QTest::newRow("empty-caption")
        << QStringLiteral("empty-caption")
        << QString()
        << QString()
        << false;
    QTest::newRow("custom-caption")
        << QStringLiteral("custom-caption")
        << QStringLiteral("Custom Caption")
        << QStringLiteral("Custom Caption")
        << false;
}

void VCAuthoringOwner_Test::authoringCore_functionBinding_captionSideEffect()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, captionMode);
    QFETCH(QString, initialCaption);
    QFETCH(QString, expectedCaption);
    QFETCH(bool, expectCaptionChanged);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(frame);
    page0->addWidgetToPageMap(frame);
    frame->setLogicalGeometry(QRectF(0, 0, 420, 320));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(30, 40, 100, 80));
    if (initialCaption == QStringLiteral("__USE_DEFAULT__"))
        button->setCaption(button->defaultCaption());
    else
        button->setCaption(initialCaption);
    button->setFunctionID(Function::invalidId());

    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("Caption Scene"));
    QVERIFY(doc->addFunction(scene));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    const quint32 sceneId = scene->id();
    auto cleanup = qScopeGuard([&]() {
        doc->deleteFunction(sceneId);
        vc->deleteVCWidgets({buttonId, frameId});
    });

    fastmcpp::tools::ToolManager tmInput;
    fastmcpp::tools::ToolManager tmUpdate;
    registerVCInputTools(tmInput, doc, m_bridge.get());
    registerVCUpdateTools(tmUpdate, doc, m_bridge.get());

    const QString beforeCaption = button->caption();
    Json prep = parseInvoke(tmInput.invoke("vc_prepare_bindings", Json{
        {"items", Json::array({{
            {"widgetID", buttonId},
            {"kind", "function"},
            {"action", "set"},
            {"functionID", sceneId}
        }})}
    }));
    QCOMPARE(QString::fromStdString(prep.value("code", std::string())), QString());
    QVERIFY(prep.contains("items"));
    QVERIFY(prep["items"].is_array());
    QVERIFY(!prep["items"].empty());
    const Json prepItem = prep["items"][0];
    QVERIFY(prepItem.is_object());
    QVERIFY(prepItem.contains("changes"));
    QVERIFY(prepItem["changes"].is_array());

    bool sawCaptionChange = false;
    for (const Json &change : prepItem["changes"])
    {
        if (!change.is_object())
            continue;
        if (change.value("field", std::string()) != "caption")
            continue;
        sawCaptionChange = true;
        QCOMPARE(QString::fromStdString(change.value("before", std::string())), beforeCaption);
        QCOMPARE(QString::fromStdString(change.value("after", std::string())), expectedCaption);
    }
    QCOMPARE(sawCaptionChange, expectCaptionChanged);

    const QString token = QString::fromStdString(prep.value("preparationToken", std::string()));
    QVERIFY(!token.isEmpty());
    Json apply = parseInvoke(tmUpdate.invoke("vc_apply_preparation", Json{
        {"preparationToken", token.toStdString()}
    }));
    QCOMPARE(QString::fromStdString(apply.value("code", std::string())), QString());
    QCOMPARE(button->caption(), expectedCaption);
    QVERIFY(apply.contains("items"));
    QVERIFY(apply["items"].is_array());
    QVERIFY(!apply["items"].empty());

    bool sawAppliedCaptionChange = false;
    for (const Json &change : apply["items"][0]["changes"])
    {
        if (!change.is_object())
            continue;
        if (change.value("field", std::string()) != "caption")
            continue;
        sawAppliedCaptionChange = true;
        QCOMPARE(QString::fromStdString(change.value("after", std::string())), expectedCaption);
    }
    QCOMPARE(sawAppliedCaptionChange, expectCaptionChanged);

    Q_UNUSED(captionMode);
}

void VCAuthoringOwner_Test::authoringPassS1_querySetup_widgetsAndHydration_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(frame);
    page0->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setTotalPagesNumber(2);
    frame->setCurrentPage(1);
    frame->setLogicalGeometry(QRectF(0, 0, 420, 320));

    VCButton *buttonA = new VCButton(doc, frame);
    vc->addWidgetToMap(buttonA);
    frame->addWidgetToPageMap(buttonA);
    buttonA->setCaption(QStringLiteral("S1 A"));
    buttonA->setPage(1);
    buttonA->setLogicalGeometry(QRectF(20, 30, 90, 70));
    QSharedPointer<QLCInputSource> sourceA(new QLCInputSource(0, 7));
    sourceA->setID(0);
    sourceA->setPage(1);
    sourceA->setWorkingMode(QLCInputSource::Absolute);
    buttonA->addInputSource(sourceA);

    VCButton *buttonB = new VCButton(doc, frame);
    vc->addWidgetToMap(buttonB);
    frame->addWidgetToPageMap(buttonB);
    buttonB->setCaption(QStringLiteral("S1 B"));
    buttonB->setPage(1);
    buttonB->setLogicalGeometry(QRectF(130, 30, 90, 70));
    QSharedPointer<QLCInputSource> sourceB(new QLCInputSource(0, 8));
    sourceB->setID(0);
    sourceB->setPage(1);
    sourceB->setWorkingMode(QLCInputSource::Absolute);
    buttonB->addInputSource(sourceB);

    const quint32 frameId = frame->id();
    const quint32 buttonAId = buttonA->id();
    const quint32 buttonBId = buttonB->id();
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({buttonAId, buttonBId, frameId}); });

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    VCBridge::AuthoringSetupQuery identityQuery;
    identityQuery.kind = VCBridge::AuthoringSetupQuery::Kind::Widgets;
    identityQuery.limit = 100;
    identityQuery.cursor = QString();
    QCOMPARE(identityQuery.fields, QStringList());
    identityQuery.pageWidgetID = page0->id();
    identityQuery.frameWidgetID = frameId;
    identityQuery.childSubpage = 1;
    identityQuery.widgetIDs = QList<quint32>{buttonAId, buttonBId};
    identityQuery.recursive = false;

    const VCBridge::AuthoringReadResult identityResult =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(identityQuery));

    // Expected S1 behavior (intentionally red until queryAuthoring setup path is implemented).
    QCOMPARE(identityResult.code, QString());
    QCOMPARE(identityResult.error, QString());
    QVERIFY(identityResult.complete);
    QVERIFY(identityResult.nextCursor.isEmpty());
    QCOMPARE(identityResult.rows.size(), 2);
    QSet<quint32> seenIdentityWidgets;
    for (const VCBridge::AuthoringReadRow &typedRow : identityResult.rows)
    {
        QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row));
        const auto row = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row);
        seenIdentityWidgets.insert(row.widgetID);
        QCOMPARE(row.pageWidgetID, page0->id());
        QCOMPARE(row.parentWidgetID, qint32(frameId));
        QCOMPARE(row.childSubpage, 1);
        QCOMPARE(row.machineType, QStringLiteral("button"));
        // Default identity/placement path must not hydrate expensive groups.
        QVERIFY(!row.inputBindings.has_value());
        QVERIFY(!row.frameConfiguration.has_value());
        QVERIFY(!row.appearance.has_value());
        QVERIFY(!row.feedback.has_value());
    }
    QCOMPARE(seenIdentityWidgets, QSet<quint32>({buttonAId, buttonBId}));

    VCBridge::AuthoringSetupQuery pagedQuery;
    const QStringList explicitFields{
        QStringLiteral("inputBindings"),
        QStringLiteral("frameConfiguration"),
        QStringLiteral("appearance")
    };
    pagedQuery.kind = VCBridge::AuthoringSetupQuery::Kind::Widgets;
    pagedQuery.limit = 1;
    pagedQuery.cursor = QString();
    pagedQuery.fields = explicitFields;
    QCOMPARE(pagedQuery.fields, explicitFields);
    pagedQuery.pageWidgetID = page0->id();
    pagedQuery.frameWidgetID = frameId;
    pagedQuery.childSubpage = 1;
    pagedQuery.widgetIDs = QList<quint32>{buttonAId, buttonBId};
    pagedQuery.recursive = false;

    const VCBridge::AuthoringReadResult firstPage =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(pagedQuery));
    QCOMPARE(firstPage.code, QString());
    QCOMPARE(firstPage.error, QString());
    QCOMPARE(firstPage.rows.size(), 1);
    QCOMPARE(firstPage.complete, false);
    QVERIFY2(!firstPage.nextCursor.isEmpty(), "First page should expose continuation cursor");
    QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(firstPage.rows.first().row));
    const auto firstRow = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(firstPage.rows.first().row);
    QVERIFY(firstRow.widgetID == buttonAId || firstRow.widgetID == buttonBId);
    QVERIFY(firstRow.inputBindings.has_value());
    QVERIFY(!firstRow.frameConfiguration.has_value());
    QVERIFY(firstRow.appearance.has_value());

    VCBridge::AuthoringSetupQuery secondPageQuery = pagedQuery;
    secondPageQuery.cursor = firstPage.nextCursor;
    QCOMPARE(secondPageQuery.fields, explicitFields);
    const VCBridge::AuthoringReadResult secondPage =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(secondPageQuery));
    QCOMPARE(secondPage.code, QString());
    QCOMPARE(secondPage.error, QString());
    QCOMPARE(secondPage.rows.size(), 1);
    QCOMPARE(secondPage.complete, true);
    QVERIFY(secondPage.nextCursor.isEmpty());
    QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(secondPage.rows.first().row));
    const auto secondRow = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(secondPage.rows.first().row);
    QVERIFY(secondRow.widgetID == buttonAId || secondRow.widgetID == buttonBId);
    QVERIFY(secondRow.widgetID != firstRow.widgetID);
    QVERIFY(secondRow.inputBindings.has_value());
    QVERIFY(!secondRow.frameConfiguration.has_value());
    QVERIFY(secondRow.appearance.has_value());

    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
}

void VCAuthoringOwner_Test::authoringPassS1_queryRecording_validationRows_redDraft_data()
{
    QTest::addColumn<QString>("cursor");
    QTest::addColumn<qint64>("fromTimeMs");
    QTest::addColumn<qint64>("toTimeMs");
    QTest::addColumn<QString>("expectedCode");

    QTest::newRow("recording-window-inverted")
        << QString()
        << qint64(2000)
        << qint64(1000)
        << QStringLiteral("invalidArgument");

    QTest::newRow("recording-cursor-utf8-too-long")
        << QString(1025, QLatin1Char('a'))
        << qint64(0)
        << qint64(1000)
        << QStringLiteral("invalidArgument");
}

void VCAuthoringOwner_Test::authoringPassS1_queryRecording_validationRows_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, cursor);
    QFETCH(qint64, fromTimeMs);
    QFETCH(qint64, toTimeMs);
    QFETCH(QString, expectedCode);

    VCBridge::AuthoringRecordingQuery query;
    query.kind = VCBridge::AuthoringRecordingQuery::Kind::Commands;
    query.showFunctionID = 1;
    query.cursor = cursor;
    query.limit = 10;
    query.fromTimeMs = fromTimeMs;
    query.toTimeMs = toTimeMs;

    const VCBridge::AuthoringReadResult result =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(query));

    // Expected S1 behavior (intentionally red until recording query validation is implemented).
    QCOMPARE(result.code, expectedCode);
    QVERIFY(!result.error.isEmpty());
}

void VCAuthoringOwner_Test::authoringPassS1_queryRecording_semanticRows_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    Show *show = new Show(doc);
    show->setName(QStringLiteral("S1SemanticShow"));
    QVERIFY(doc->addFunction(show));
    const quint32 showId = show->id();
    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("S1SemanticSceneTarget"));
    QVERIFY(doc->addFunction(scene));
    const quint32 sceneId = scene->id();
    auto cleanupShow = qScopeGuard([&]() {
        doc->deleteFunction(showId);
        doc->deleteFunction(sceneId);
    });

    // Seed authored semantic command track through native public Show API.
    const QUuid recordingControl = QUuid(QStringLiteral("{f14ec083-4f75-43f3-a0bb-b8fdb4476d89}"));
    ShowCommandTrack track;
    QString trackError;
    ShowCommand start = ShowCommand::start(1001u, 1000u, sceneId);
    QVERIFY2(track.insert(start, &trackError), qPrintable(trackError));
    ShowCommand controlOn = ShowCommand::setButtonState(1002u, 1200u, recordingControl, true);
    controlOn.pairId = 9001u;
    QVERIFY2(track.insert(controlOn, &trackError), qPrintable(trackError));
    ShowCommand controlOff = ShowCommand::setButtonState(1003u, 1400u, recordingControl, false);
    controlOff.pairId = 9001u;
    QVERIFY2(track.insert(controlOff, &trackError), qPrintable(trackError));
    ShowCommand stop = ShowCommand::stop(1004u, 1800u, sceneId);
    QVERIFY2(track.insert(stop, &trackError), qPrintable(trackError));
    QVERIFY2(show->setStoppedCommandTrack(track, &trackError), qPrintable(trackError));

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    VCBridge::AuthoringRecordingQuery cmdQuery;
    cmdQuery.kind = VCBridge::AuthoringRecordingQuery::Kind::Commands;
    cmdQuery.limit = 10;
    cmdQuery.fields = QStringList{
        QStringLiteral("identity"),
        QStringLiteral("timing"),
        QStringLiteral("semanticPayload")
    };
    cmdQuery.showFunctionID = showId;
    cmdQuery.fromTimeMs = 0;
    cmdQuery.toTimeMs = std::numeric_limits<qint64>::max();

    const VCBridge::AuthoringReadResult commandResult =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(cmdQuery));

    VCBridge::AuthoringRecordingQuery refQuery;
    refQuery.kind = VCBridge::AuthoringRecordingQuery::Kind::References;
    refQuery.limit = 10;
    refQuery.fields = QStringList{
        QStringLiteral("identity"),
        QStringLiteral("readiness")
    };
    refQuery.showFunctionID = showId;

    const VCBridge::AuthoringReadResult referenceResult =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(refQuery));

    // Expected S1 behavior (intentionally red until queryAuthoring recording path is implemented).
    QCOMPARE(commandResult.code, QString());
    QCOMPARE(commandResult.error, QString());
    QVERIFY(commandResult.complete);
    QVERIFY(commandResult.nextCursor.isEmpty());
    QCOMPARE(commandResult.rows.size(), 4);
    qint64 previousTimeMs = std::numeric_limits<qint64>::min();
    int previousOrder = -1;
    bool sawPair = false;
    for (const VCBridge::AuthoringReadRow &typedRow : commandResult.rows)
    {
        QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::RecordingCommandRow>(typedRow.row));
        const auto cmd = std::get<VCBridge::AuthoringReadRow::RecordingCommandRow>(typedRow.row);
        QCOMPARE(cmd.showFunctionID, showId);
        QVERIFY(cmd.eventID >= 1001u && cmd.eventID <= 1004u);
        if (cmd.eventID == 1002u || cmd.eventID == 1003u)
        {
            QCOMPARE(cmd.controlRecordingUUID.toLower(), recordingControl.toString(QUuid::WithoutBraces).toLower());
            QVERIFY(cmd.pairID.has_value());
            QCOMPARE(*cmd.pairID, 9001u);
            sawPair = true;
        }
        QVERIFY(cmd.timeMs.has_value());
        QVERIFY(cmd.order.has_value());
        QVERIFY(*cmd.timeMs >= previousTimeMs);
        if (*cmd.timeMs == previousTimeMs)
            QVERIFY(*cmd.order > previousOrder);
        previousTimeMs = *cmd.timeMs;
        previousOrder = *cmd.order;
    }
    QVERIFY2(sawPair, "Expected at least one paired control command row");

    QCOMPARE(referenceResult.code, QString());
    QCOMPARE(referenceResult.error, QString());
    QVERIFY(referenceResult.complete);
    QVERIFY(referenceResult.nextCursor.isEmpty());
    QVERIFY2(!referenceResult.rows.isEmpty(), "Expected at least one recording reference row");
    bool sawSeededReference = false;
    for (const VCBridge::AuthoringReadRow &typedRow : referenceResult.rows)
    {
        QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::RecordingReferenceRow>(typedRow.row));
        const auto ref = std::get<VCBridge::AuthoringReadRow::RecordingReferenceRow>(typedRow.row);
        if (ref.showFunctionID != showId)
            continue;
        if (ref.controlRecordingUUID.toLower() ==
            recordingControl.toString(QUuid::WithoutBraces).toLower())
        {
            sawSeededReference = true;
            QVERIFY(ref.count >= 1);
        }
    }
    QVERIFY2(sawSeededReference, "Seeded recording UUID must be visible in semantic references");

    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
}

void VCAuthoringOwner_Test::authoringCore_querySetup_payloadEnvelopeRows_redDraft_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::newRow("two-rows-fit-individually-overflow-combined")
        << QStringLiteral("two-rows-fit-individually-overflow-combined");
    QTest::newRow("single-row-oversized")
        << QStringLiteral("single-row-oversized");
}

void VCAuthoringOwner_Test::authoringCore_querySetup_payloadEnvelopeRows_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(frame);
    page0->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setCurrentPage(0);
    frame->setLogicalGeometry(QRectF(0, 0, 420, 320));

    VCButton *a = new VCButton(doc, frame);
    vc->addWidgetToMap(a);
    frame->addWidgetToPageMap(a);
    a->setPage(0);
    a->setLogicalGeometry(QRectF(20, 40, 120, 80));

    VCButton *b = new VCButton(doc, frame);
    vc->addWidgetToMap(b);
    frame->addWidgetToPageMap(b);
    b->setPage(0);
    b->setLogicalGeometry(QRectF(180, 40, 120, 80));

    const quint32 frameId = frame->id();
    const quint32 aId = a->id();
    const quint32 bId = b->id();
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({aId, bId, frameId}); });

    VCBridge::AuthoringSetupQuery q;
    q.kind = VCBridge::AuthoringSetupQuery::Kind::Widgets;
    q.pageWidgetID = page0->id();
    q.frameWidgetID = frameId;
    q.childSubpage = 0;
    q.recursive = false;
    q.fields = QStringList{QStringLiteral("identity")};

    if (scenario == QStringLiteral("two-rows-fit-individually-overflow-combined"))
    {
        // Each row should stay under cap, but two rows together should exceed it.
        a->setCaption(QString(150000, QLatin1Char('A')));
        b->setCaption(QString(150000, QLatin1Char('B')));
        q.limit = 2;
        q.widgetIDs = QList<quint32>{aId, bId};

        const VCBridge::AuthoringReadResult first = m_bridge->queryAuthoring(VCBridge::AuthoringQuery(q));
        QCOMPARE(first.code, QString());
        QCOMPARE(first.error, QString());
        QCOMPARE(first.rows.size(), 1);
        QCOMPARE(first.complete, false);
        QVERIFY2(!first.nextCursor.isEmpty(), "Expected continuation cursor for oversized combined page");
    }
    else
    {
        // A single row over cap must return compact itemTooLarge response.
        a->setCaption(QString(320000, QLatin1Char('X')));
        b->setCaption(QStringLiteral("small"));
        q.limit = 1;
        q.widgetIDs = QList<quint32>{aId};

        const VCBridge::AuthoringReadResult row = m_bridge->queryAuthoring(VCBridge::AuthoringQuery(q));
        QCOMPARE(row.code, QStringLiteral("itemTooLarge"));
        QVERIFY(!row.error.isEmpty());
        QCOMPARE(row.rows.size(), 0);
        QVERIFY(row.nextCursor.isEmpty());
        QCOMPARE(row.complete, false);
    }
}

void VCAuthoringOwner_Test::authoringCore_queryRecording_semanticsAndReadiness_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCSlider *slider = new VCSlider(doc, page0);
    vc->addWidgetToMap(slider);
    page0->addWidgetToPageMap(slider);
    slider->setCaption(QStringLiteral("semantic-slider"));
    slider->setPage(0);
    slider->setLogicalGeometry(QRectF(20, 20, 120, 200));
    const QUuid sliderControlId = slider->ensureRecordingId();
    QVERIFY(!sliderControlId.isNull());

    VCButton *toggle = new VCButton(doc, page0);
    vc->addWidgetToMap(toggle);
    page0->addWidgetToPageMap(toggle);
    toggle->setCaption(QStringLiteral("semantic-toggle"));
    toggle->setPage(0);
    toggle->setLogicalGeometry(QRectF(180, 20, 120, 80));
    toggle->setActionType(VCButton::Toggle);
    const QUuid toggleControlId = toggle->ensureRecordingId();
    QVERIFY(!toggleControlId.isNull());

    const quint32 sliderId = slider->id();
    const quint32 toggleId = toggle->id();
    auto cleanupWidgets = qScopeGuard([&]() { vc->deleteVCWidgets({sliderId, toggleId}); });

    Show *show = new Show(doc);
    show->setName(QStringLiteral("semantic-ready-show"));
    QVERIFY(doc->addFunction(show));
    const quint32 showId = show->id();
    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("semantic-ready-scene"));
    QVERIFY(doc->addFunction(scene));
    const quint32 sceneId = scene->id();
    auto cleanupShow = qScopeGuard([&]() {
        doc->deleteFunction(showId);
        doc->deleteFunction(sceneId);
    });

    ShowCommandTrack track;
    QString trackError;
    ShowCommand sliderPosition = ShowCommand::setSliderPosition(
        3001u, 1000u, sliderControlId, ShowControlRole::LevelSlider, QString(), 0.75);
    QVERIFY2(track.insert(sliderPosition, &trackError), qPrintable(trackError));
    ShowCommand toggleOn = ShowCommand::setButtonState(3002u, 1200u, toggleControlId, true);
    toggleOn.pairId = 7123u;
    QVERIFY2(track.insert(toggleOn, &trackError), qPrintable(trackError));
    ShowCommand toggleOff = ShowCommand::setButtonState(3003u, 1400u, toggleControlId, false);
    toggleOff.pairId = 7123u;
    QVERIFY2(track.insert(toggleOff, &trackError), qPrintable(trackError));
    ShowCommand setIntensity = ShowCommand::setIntensity(3004u, 1500u, sceneId, 0.33);
    QVERIFY2(track.insert(setIntensity, &trackError), qPrintable(trackError));
    ShowCommand sceneStart = ShowCommand::start(3005u, 1600u, sceneId);
    QVERIFY2(track.insert(sceneStart, &trackError), qPrintable(trackError));
    QVERIFY2(show->setStoppedCommandTrack(track, &trackError), qPrintable(trackError));

    VCBridge::AuthoringRecordingQuery commands;
    commands.kind = VCBridge::AuthoringRecordingQuery::Kind::Commands;
    commands.showFunctionID = showId;
    commands.limit = 50;
    commands.fields = QStringList{
        QStringLiteral("identity"),
        QStringLiteral("timing"),
        QStringLiteral("semanticPayload"),
        QStringLiteral("readiness")
    };

    const VCBridge::AuthoringReadResult commandResult =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(commands));
    QCOMPARE(commandResult.code, QString());
    QVERIFY2(!commandResult.rows.isEmpty(), "Expected command rows");

    bool sawSliderPayload = false;
    bool sawIntensityPayload = false;
    bool sawNullUuidFunctionCommand = false;
    bool sawOnOffPair = false;
    for (const VCBridge::AuthoringReadRow &typedRow : commandResult.rows)
    {
        QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::RecordingCommandRow>(typedRow.row));
        const auto row = std::get<VCBridge::AuthoringReadRow::RecordingCommandRow>(typedRow.row);
        QVERIFY(row.action.has_value());
        const QString action = *row.action;
        if (action == QStringLiteral("SetSliderPosition"))
        {
            sawSliderPayload = true;
            QVERIFY(row.payload.has_value());
            QVERIFY(row.payload->userType() == QMetaType::QVariantMap);
            const QVariantMap map = row.payload->toMap();
            QVERIFY2(map.contains(QStringLiteral("position")), "SetSliderPosition payload missing position");
        }
        if (action == QStringLiteral("SetIntensity"))
        {
            sawIntensityPayload = true;
            QVERIFY2(row.payload.has_value() && row.payload->isValid(), "SetIntensity payload must be present");
            const QVariantMap map = row.payload->toMap();
            QVERIFY2(map.contains(QStringLiteral("intensity")),
                     "SetIntensity payload missing intensity");
        }
        if (action == QStringLiteral("Start"))
        {
            sawNullUuidFunctionCommand = true;
            QVERIFY2(row.controlRecordingUUID.isEmpty(),
                     "Function command must not serialize fabricated controlRecordingUUID");
        }
        if (action == QStringLiteral("SetButtonState") && row.pairID.has_value() && *row.pairID == 7123u)
            sawOnOffPair = true;
    }
    QVERIFY2(sawSliderPayload, "Missing slider position semantic payload row");
    QVERIFY2(sawIntensityPayload, "Missing intensity semantic payload row");
    QVERIFY2(sawNullUuidFunctionCommand, "Missing function command null-UUID row");
    QVERIFY2(sawOnOffPair, "Missing paired on/off rows");

    VCBridge::AuthoringRecordingQuery references;
    references.kind = VCBridge::AuthoringRecordingQuery::Kind::References;
    references.showFunctionID = showId;
    references.limit = 50;
    references.fields = QStringList{
        QStringLiteral("identity"),
        QStringLiteral("readiness")
    };

    const VCBridge::AuthoringReadResult referenceResult =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(references));
    QCOMPARE(referenceResult.code, QString());
    QVERIFY2(!referenceResult.rows.isEmpty(), "Expected reference rows");

    bool sawSliderReadyReference = false;
    const QString sliderKey = sliderControlId.toString(QUuid::WithoutBraces).toLower();
    for (const VCBridge::AuthoringReadRow &typedRow : referenceResult.rows)
    {
        QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::RecordingReferenceRow>(typedRow.row));
        const auto row = std::get<VCBridge::AuthoringReadRow::RecordingReferenceRow>(typedRow.row);
        if (row.controlRecordingUUID.toLower() == sliderKey)
        {
            sawSliderReadyReference = true;
            QVERIFY(row.status.has_value());
            QVERIFY(*row.status == QStringLiteral("ready") || *row.status == QStringLiteral("incompatible"));
            if (*row.status == QStringLiteral("ready"))
                QVERIFY(!row.reason.has_value() || row.reason->isEmpty());
            else
                QVERIFY(row.reason.has_value() && !row.reason->isEmpty());
        }
    }
    QVERIFY2(sawSliderReadyReference, "Expected seeded slider control reference row");
}

void VCAuthoringOwner_Test::authoringCore_queryRecording_continuationUnfiltered_500x10_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");
    m_bridge->clearAuthoringNowForTest();
    m_bridge->clearAuthoringRecords();
    auto clearAuthoringRecords = qScopeGuard([&]() { m_bridge->clearAuthoringRecords(); });

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCSlider *slider = new VCSlider(doc, page0);
    vc->addWidgetToMap(slider);
    page0->addWidgetToPageMap(slider);
    slider->setCaption(QStringLiteral("cont-slider"));
    slider->setPage(0);
    slider->setLogicalGeometry(QRectF(20, 20, 120, 220));
    const QUuid sliderControlId = slider->ensureRecordingId();
    QVERIFY(!sliderControlId.isNull());

    VCButton *toggle = new VCButton(doc, page0);
    vc->addWidgetToMap(toggle);
    page0->addWidgetToPageMap(toggle);
    toggle->setCaption(QStringLiteral("cont-toggle"));
    toggle->setPage(0);
    toggle->setLogicalGeometry(QRectF(180, 20, 120, 80));
    toggle->setActionType(VCButton::Toggle);
    const QUuid toggleControlId = toggle->ensureRecordingId();
    QVERIFY(!toggleControlId.isNull());

    const quint32 sliderId = slider->id();
    const quint32 toggleId = toggle->id();
    auto cleanupWidgets = qScopeGuard([&]() { vc->deleteVCWidgets({sliderId, toggleId}); });

    Show *show = new Show(doc);
    show->setName(QStringLiteral("recording-continuation-show"));
    QVERIFY(doc->addFunction(show));
    const quint32 showId = show->id();
    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("recording-continuation-scene"));
    QVERIFY(doc->addFunction(scene));
    const quint32 sceneId = scene->id();
    auto cleanupShow = qScopeGuard([&]() {
        doc->deleteFunction(showId);
        doc->deleteFunction(sceneId);
    });

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    ShowCommandTrack track;
    QString trackError;
    QSet<quint32> expectedEventIds;
    expectedEventIds.reserve(500);

    for (int i = 0; i < 500; ++i)
    {
        const quint32 eventId = 6000u + quint32(i);
        const qint64 timeMs = 1000 + (i / 2) * 10;
        ShowCommand cmd;
        switch (i % 5)
        {
        case 0:
            cmd = ShowCommand::start(eventId, timeMs, sceneId);
            break;
        case 1:
            cmd = ShowCommand::stop(eventId, timeMs, sceneId);
            break;
        case 2:
            cmd = ShowCommand::setButtonState(eventId, timeMs, toggleControlId, (i % 2) == 0);
            break;
        case 3:
            cmd = ShowCommand::setSliderPosition(
                eventId, timeMs, sliderControlId, ShowControlRole::LevelSlider, QString(), (i % 100) / 100.0);
            break;
        default:
            cmd = ShowCommand::setIntensity(eventId, timeMs, sceneId, (i % 100) / 100.0);
            break;
        }
        QVERIFY2(track.insert(cmd, &trackError), qPrintable(trackError));
        expectedEventIds.insert(eventId);
    }
    QCOMPARE(expectedEventIds.size(), 500);
    QVERIFY2(show->setStoppedCommandTrack(track, &trackError), qPrintable(trackError));

    auto collectCommands = [&](int limit, QList<quint32> &orderedEventIds, bool &sawContinuation) {
        VCBridge::AuthoringRecordingQuery q;
        q.kind = VCBridge::AuthoringRecordingQuery::Kind::Commands;
        q.showFunctionID = showId;
        q.limit = limit;
        q.fields = QStringList{QStringLiteral("identity"), QStringLiteral("timing")};

        QString basis;
        QSet<quint32> seen;
        for (;;)
        {
            const VCBridge::AuthoringReadResult result =
                m_bridge->queryAuthoring(VCBridge::AuthoringQuery(q));
            QCOMPARE(result.code, QString());
            QCOMPARE(result.error, QString());

            if (basis.isEmpty())
            {
                QVERIFY(!result.basis.isEmpty());
                basis = result.basis;
            }
            else
            {
                QCOMPARE(result.basis, basis);
            }

            qint64 previousTimeMs = std::numeric_limits<qint64>::min();
            int previousOrder = -1;
            for (const VCBridge::AuthoringReadRow &typedRow : result.rows)
            {
                QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::RecordingCommandRow>(typedRow.row));
                const auto row = std::get<VCBridge::AuthoringReadRow::RecordingCommandRow>(typedRow.row);
                QVERIFY(expectedEventIds.contains(row.eventID));
                QVERIFY2(!seen.contains(row.eventID), "Recording continuation returned duplicate eventID");
                QVERIFY(row.timeMs.has_value());
                QVERIFY(row.order.has_value());
                QVERIFY(*row.timeMs >= previousTimeMs);
                if (*row.timeMs == previousTimeMs)
                    QVERIFY(*row.order > previousOrder);
                previousTimeMs = *row.timeMs;
                previousOrder = *row.order;

                seen.insert(row.eventID);
                orderedEventIds << row.eventID;
            }

            if (result.complete)
            {
                QVERIFY(result.nextCursor.isEmpty());
                break;
            }

            sawContinuation = true;
            QVERIFY2(!result.nextCursor.isEmpty(), "Expected continuation cursor for bounded recording census");
            q.cursor = result.nextCursor;
        }

        QCOMPARE(seen, expectedEventIds);
    };

    QList<quint32> wideOrder;
    bool wideSawContinuation = false;
    collectCommands(250, wideOrder, wideSawContinuation);
    QVERIFY(wideSawContinuation);
    QCOMPARE(wideOrder.size(), 500);

    QList<quint32> pagedOrder;
    bool pagedSawContinuation = false;
    collectCommands(10, pagedOrder, pagedSawContinuation);
    QVERIFY(pagedSawContinuation);
    QCOMPARE(pagedOrder.size(), 500);
    QCOMPARE(pagedOrder, wideOrder);

    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
}

void VCAuthoringOwner_Test::authoringCore_queryRecording_continuationWindowed_500x10_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");
    m_bridge->clearAuthoringNowForTest();
    m_bridge->clearAuthoringRecords();
    auto clearAuthoringRecords = qScopeGuard([&]() { m_bridge->clearAuthoringRecords(); });

    constexpr qint64 kWindowFromMs = 1300;
    constexpr qint64 kWindowToMs = 1990;

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCSlider *slider = new VCSlider(doc, page0);
    vc->addWidgetToMap(slider);
    page0->addWidgetToPageMap(slider);
    slider->setCaption(QStringLiteral("windowed-cont-slider"));
    slider->setPage(0);
    slider->setLogicalGeometry(QRectF(20, 20, 120, 220));
    const QUuid sliderControlId = slider->ensureRecordingId();
    QVERIFY(!sliderControlId.isNull());

    VCButton *toggle = new VCButton(doc, page0);
    vc->addWidgetToMap(toggle);
    page0->addWidgetToPageMap(toggle);
    toggle->setCaption(QStringLiteral("windowed-cont-toggle"));
    toggle->setPage(0);
    toggle->setLogicalGeometry(QRectF(180, 20, 120, 80));
    toggle->setActionType(VCButton::Toggle);
    const QUuid toggleControlId = toggle->ensureRecordingId();
    QVERIFY(!toggleControlId.isNull());

    const quint32 sliderId = slider->id();
    const quint32 toggleId = toggle->id();
    auto cleanupWidgets = qScopeGuard([&]() { vc->deleteVCWidgets({sliderId, toggleId}); });

    Show *show = new Show(doc);
    show->setName(QStringLiteral("recording-windowed-continuation-show"));
    QVERIFY(doc->addFunction(show));
    const quint32 showId = show->id();
    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("recording-windowed-continuation-scene"));
    QVERIFY(doc->addFunction(scene));
    const quint32 sceneId = scene->id();
    auto cleanupShow = qScopeGuard([&]() {
        doc->deleteFunction(showId);
        doc->deleteFunction(sceneId);
    });

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    ShowCommandTrack track;
    QString trackError;
    QSet<quint32> expectedWindowEventIds;
    expectedWindowEventIds.reserve(500);

    for (int i = 0; i < 500; ++i)
    {
        const quint32 eventId = 7000u + quint32(i);
        const qint64 timeMs = 1000 + (i / 2) * 10;
        ShowCommand cmd;
        switch (i % 5)
        {
        case 0:
            cmd = ShowCommand::start(eventId, timeMs, sceneId);
            break;
        case 1:
            cmd = ShowCommand::stop(eventId, timeMs, sceneId);
            break;
        case 2:
            cmd = ShowCommand::setButtonState(eventId, timeMs, toggleControlId, (i % 2) == 0);
            break;
        case 3:
            cmd = ShowCommand::setSliderPosition(
                eventId, timeMs, sliderControlId, ShowControlRole::LevelSlider, QString(), (i % 100) / 100.0);
            break;
        default:
            cmd = ShowCommand::setIntensity(eventId, timeMs, sceneId, (i % 100) / 100.0);
            break;
        }
        QVERIFY2(track.insert(cmd, &trackError), qPrintable(trackError));
        if (timeMs >= kWindowFromMs && timeMs <= kWindowToMs)
            expectedWindowEventIds.insert(eventId);
    }

    QVERIFY(!expectedWindowEventIds.isEmpty());
    QVERIFY(expectedWindowEventIds.size() > 10);
    QVERIFY2(show->setStoppedCommandTrack(track, &trackError), qPrintable(trackError));

    auto collectWindowedCommands = [&](int limit, QList<quint32> &orderedEventIds, bool &sawContinuation) {
        VCBridge::AuthoringRecordingQuery q;
        q.kind = VCBridge::AuthoringRecordingQuery::Kind::Commands;
        q.showFunctionID = showId;
        q.fromTimeMs = kWindowFromMs;
        q.toTimeMs = kWindowToMs;
        q.limit = limit;
        q.fields = QStringList{QStringLiteral("identity"), QStringLiteral("timing")};

        QString basis;
        QSet<quint32> seen;
        for (;;)
        {
            const VCBridge::AuthoringReadResult result =
                m_bridge->queryAuthoring(VCBridge::AuthoringQuery(q));
            QCOMPARE(result.code, QString());
            QCOMPARE(result.error, QString());

            if (basis.isEmpty())
            {
                QVERIFY(!result.basis.isEmpty());
                basis = result.basis;
            }
            else
            {
                QCOMPARE(result.basis, basis);
            }

            qint64 previousTimeMs = std::numeric_limits<qint64>::min();
            int previousOrder = -1;
            for (const VCBridge::AuthoringReadRow &typedRow : result.rows)
            {
                QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::RecordingCommandRow>(typedRow.row));
                const auto row = std::get<VCBridge::AuthoringReadRow::RecordingCommandRow>(typedRow.row);
                QVERIFY(expectedWindowEventIds.contains(row.eventID));
                QVERIFY2(!seen.contains(row.eventID), "Windowed continuation returned duplicate eventID");
                QVERIFY(row.timeMs.has_value());
                QVERIFY(*row.timeMs >= kWindowFromMs);
                QVERIFY(*row.timeMs <= kWindowToMs);
                QVERIFY(row.order.has_value());
                QVERIFY(*row.timeMs >= previousTimeMs);
                if (*row.timeMs == previousTimeMs)
                    QVERIFY(*row.order > previousOrder);
                previousTimeMs = *row.timeMs;
                previousOrder = *row.order;

                seen.insert(row.eventID);
                orderedEventIds << row.eventID;
            }

            if (result.complete)
            {
                QVERIFY(result.nextCursor.isEmpty());
                break;
            }

            sawContinuation = true;
            QVERIFY2(!result.nextCursor.isEmpty(), "Expected continuation cursor for bounded windowed recording query");
            q.cursor = result.nextCursor;
        }

        QCOMPARE(seen, expectedWindowEventIds);
    };

    QList<quint32> wideOrder;
    bool wideSawContinuation = false;
    collectWindowedCommands(64, wideOrder, wideSawContinuation);
    QVERIFY(wideSawContinuation);
    QCOMPARE(wideOrder.size(), expectedWindowEventIds.size());

    QList<quint32> pagedOrder;
    bool pagedSawContinuation = false;
    collectWindowedCommands(10, pagedOrder, pagedSawContinuation);
    QVERIFY(pagedSawContinuation);
    QCOMPARE(pagedOrder.size(), expectedWindowEventIds.size());
    QCOMPARE(pagedOrder, wideOrder);

    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
}

void VCAuthoringOwner_Test::authoringCore_querySetup_cursorCodeRows_redDraft_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::newRow("malformed-cursor") << QStringLiteral("malformed-cursor");
    QTest::newRow("selector-mismatch") << QStringLiteral("selector-mismatch");
    QTest::newRow("instance-mismatch") << QStringLiteral("instance-mismatch");
    QTest::newRow("cursor-expired") << QStringLiteral("cursor-expired");
    QTest::newRow("basis-stale-after-frame-label-change") << QStringLiteral("basis-stale-after-frame-label-change");
    QTest::newRow("basis-stale-after-frame-page-count-change") << QStringLiteral("basis-stale-after-frame-page-count-change");
    QTest::newRow("basis-not-stale-after-frame-current-page-change") << QStringLiteral("basis-not-stale-after-frame-current-page-change");
}

void VCAuthoringOwner_Test::authoringCore_querySetup_cursorCodeRows_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(frame);
    page0->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setTotalPagesNumber(2);
    frame->setMultiPageMode(true);
    frame->setCurrentPage(0);
    frame->setLogicalGeometry(QRectF(0, 0, 420, 320));

    VCButton *a = new VCButton(doc, frame);
    vc->addWidgetToMap(a);
    frame->addWidgetToPageMap(a);
    a->setPage(0);
    a->setCaption(QStringLiteral("cursor-A"));
    a->setLogicalGeometry(QRectF(20, 40, 120, 80));

    VCButton *b = new VCButton(doc, frame);
    vc->addWidgetToMap(b);
    frame->addWidgetToPageMap(b);
    b->setPage(0);
    b->setCaption(QStringLiteral("cursor-B"));
    b->setLogicalGeometry(QRectF(180, 40, 120, 80));

    const quint32 frameId = frame->id();
    const quint32 aId = a->id();
    const quint32 bId = b->id();
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({aId, bId, frameId}); });

    VCBridge::AuthoringSetupQuery firstQuery;
    firstQuery.kind = VCBridge::AuthoringSetupQuery::Kind::Widgets;
    firstQuery.limit = 1;
    firstQuery.pageWidgetID = page0->id();
    firstQuery.frameWidgetID = frameId;
    firstQuery.childSubpage = 0;
    firstQuery.widgetIDs = QList<quint32>{aId, bId};
    firstQuery.recursive = false;
    firstQuery.fields = QStringList{QStringLiteral("identity"), QStringLiteral("placement")};

    const VCBridge::AuthoringReadResult first = m_bridge->queryAuthoring(VCBridge::AuthoringQuery(firstQuery));
    QCOMPARE(first.code, QString());
    QVERIFY(!first.nextCursor.isEmpty());

    VCBridge::AuthoringSetupQuery nextQuery = firstQuery;
    if (scenario == QStringLiteral("malformed-cursor"))
    {
        nextQuery.cursor = QStringLiteral("not-base64");
    }
    else if (scenario == QStringLiteral("selector-mismatch"))
    {
        nextQuery.cursor = mutateCursorField(first.nextCursor, QStringLiteral("selector"), QJsonValue(QStringLiteral("deadbeef")));
    }
    else if (scenario == QStringLiteral("instance-mismatch"))
    {
        nextQuery.cursor = mutateCursorField(first.nextCursor, QStringLiteral("instance"), QJsonValue(QStringLiteral("different-instance")));
    }
    else if (scenario == QStringLiteral("cursor-expired"))
    {
        m_bridge->setAuthoringNowForTest(5000);
        VCBridge::AuthoringReadResult fixedNow = m_bridge->queryAuthoring(VCBridge::AuthoringQuery(firstQuery));
        QCOMPARE(fixedNow.code, QString());
        QVERIFY(!fixedNow.nextCursor.isEmpty());
        m_bridge->setAuthoringNowForTest(5000 + VCBridge::kAuthoringRecordTtlMs + 1);
        nextQuery.cursor = fixedNow.nextCursor;
    }
    else if (scenario == QStringLiteral("basis-stale-after-frame-label-change"))
    {
        frame->setShortcutName(1, QStringLiteral("Two"));
        nextQuery.cursor = first.nextCursor;
    }
    else if (scenario == QStringLiteral("basis-not-stale-after-frame-current-page-change"))
    {
        const int before = frame->currentPage();
        frame->setCurrentPage(1);
        QVERIFY(frame->multiPageMode());
        QVERIFY(frame->currentPage() != before);
        nextQuery.cursor = first.nextCursor;
    }
    else
    {
        frame->setTotalPagesNumber(3);
        nextQuery.cursor = first.nextCursor;
    }

    const VCBridge::AuthoringReadResult continued =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(nextQuery));

    if (scenario == QStringLiteral("cursor-expired"))
    {
        QCOMPARE(continued.code, QStringLiteral("cursorExpired"));
        m_bridge->clearAuthoringNowForTest();
    }
    else if (scenario == QStringLiteral("basis-not-stale-after-frame-current-page-change"))
    {
        QCOMPARE(continued.code, QString());
        QCOMPARE(continued.rows.size(), 1);
        QCOMPARE(continued.error, QString());
    }
    else if (scenario.startsWith(QStringLiteral("basis-stale")))
    {
        QCOMPARE(continued.code, QStringLiteral("staleBasis"));
    }
    else
    {
        QCOMPARE(continued.code, QStringLiteral("invalidCursor"));
    }
    if (scenario != QStringLiteral("basis-not-stale-after-frame-current-page-change"))
        QVERIFY(!continued.error.isEmpty());
}

void VCAuthoringOwner_Test::authoringCore_querySetup_censusContinuation_1000x20_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");
    auto clearAuthoringRecords = qScopeGuard([&]() { m_bridge->clearAuthoringRecords(); });

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(frame);
    page0->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(20);
    frame->setCurrentPage(0);
    frame->setLogicalGeometry(QRectF(0, 0, 920, 720));

    QList<quint32> widgetIds;
    widgetIds.reserve(1000);
    for (int subpage = 0; subpage < 20; ++subpage)
    {
        frame->setShortcutName(subpage, QStringLiteral("Page %1").arg(subpage));
        for (int slot = 0; slot < 50; ++slot)
        {
            VCButton *button = new VCButton(doc, frame);
            vc->addWidgetToMap(button);
            button->setPage(subpage);
            frame->addWidgetToPageMap(button);
            button->setCaption(QStringLiteral("census-%1-%2").arg(subpage).arg(slot));
            button->setLogicalGeometry(QRectF(10 + ((slot % 10) * 88), 10 + ((slot / 10) * 36), 80, 30));
            widgetIds << button->id();
        }
    }
    QCOMPARE(widgetIds.size(), 1000);

    const quint32 frameId = frame->id();
    QVariantList cleanupIds;
    cleanupIds.reserve(widgetIds.size() + 1);
    for (quint32 id : widgetIds)
        cleanupIds << QVariant::fromValue(id);
    cleanupIds << QVariant::fromValue(frameId);
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets(cleanupIds); });

    VCBridge::AuthoringSetupQuery subpagesQuery;
    subpagesQuery.kind = VCBridge::AuthoringSetupQuery::Kind::Subpages;
    subpagesQuery.frameWidgetID = frameId;
    subpagesQuery.limit = 7;
    subpagesQuery.fields = QStringList{QStringLiteral("identity"), QStringLiteral("placement")};

    QSet<int> seenSubpages;
    QString subpageBasis;
    int subpagePageCount = 0;
    for (;;)
    {
        const VCBridge::AuthoringReadResult result =
            m_bridge->queryAuthoring(VCBridge::AuthoringQuery(subpagesQuery));
        ++subpagePageCount;
        QCOMPARE(result.code, QString());
        if (subpageBasis.isEmpty())
        {
            QVERIFY(!result.basis.isEmpty());
            subpageBasis = result.basis;
        }
        else
        {
            QCOMPARE(result.basis, subpageBasis);
        }

        for (const VCBridge::AuthoringReadRow &typedRow : result.rows)
        {
            QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupSubpageRow>(typedRow.row));
            const auto row = std::get<VCBridge::AuthoringReadRow::SetupSubpageRow>(typedRow.row);
            QCOMPARE(row.frameWidgetID, frameId);
            QVERIFY(row.childSubpage >= 0 && row.childSubpage < 20);
            seenSubpages.insert(row.childSubpage);
        }

        if (result.complete)
        {
            QVERIFY(result.nextCursor.isEmpty());
            break;
        }

        QVERIFY2(!result.nextCursor.isEmpty(), "Expected continuation cursor for bounded subpage census");
        subpagesQuery.cursor = result.nextCursor;
        QVERIFY(subpagePageCount < 20);
    }
    QVERIFY(subpagePageCount > 1);
    QCOMPARE(seenSubpages.size(), 20);
    for (int subpage = 0; subpage < 20; ++subpage)
        QVERIFY(seenSubpages.contains(subpage));

    QSet<quint32> expectedWidgetIds;
    for (quint32 id : widgetIds)
        expectedWidgetIds.insert(id);

    auto collectWidgets = [&](int limit, QList<quint32> &orderedIds, bool &sawContinuation) {
        VCBridge::AuthoringSetupQuery query;
        query.kind = VCBridge::AuthoringSetupQuery::Kind::Widgets;
        query.frameWidgetID = frameId;
        query.widgetIDs = widgetIds;
        query.recursive = false;
        query.limit = limit;
        query.fields = QStringList{QStringLiteral("identity"), QStringLiteral("placement")};

        QString basis;
        QSet<quint32> seen;
        int pageCount = 0;
        for (;;)
        {
            const VCBridge::AuthoringReadResult result =
                m_bridge->queryAuthoring(VCBridge::AuthoringQuery(query));
            ++pageCount;
            QCOMPARE(result.code, QString());
            if (basis.isEmpty())
            {
                QVERIFY(!result.basis.isEmpty());
                basis = result.basis;
            }
            else
            {
                QCOMPARE(result.basis, basis);
            }

            for (const VCBridge::AuthoringReadRow &typedRow : result.rows)
            {
                QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row));
                const auto row = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row);
                QVERIFY(expectedWidgetIds.contains(row.widgetID));
                QVERIFY(row.childSubpage >= 0 && row.childSubpage < 20);
                QVERIFY2(!seen.contains(row.widgetID), "Widget census emitted duplicate ID");
                seen.insert(row.widgetID);
                orderedIds << row.widgetID;
            }

            if (result.complete)
            {
                QVERIFY(result.nextCursor.isEmpty());
                break;
            }

            sawContinuation = true;
            QVERIFY2(!result.nextCursor.isEmpty(), "Expected continuation cursor for bounded widget census");
            query.cursor = result.nextCursor;
            QVERIFY(pageCount < 200);
        }

        QCOMPARE(seen.size(), 1000);
    };

    QList<quint32> baselineOrder;
    bool baselineSawContinuation = false;
    collectWidgets(250, baselineOrder, baselineSawContinuation);
    QVERIFY(baselineSawContinuation);
    QCOMPARE(baselineOrder.size(), 1000);

    QList<quint32> pagedOrder;
    bool pagedSawContinuation = false;
    collectWidgets(20, pagedOrder, pagedSawContinuation);
    QVERIFY(pagedSawContinuation);
    QCOMPARE(pagedOrder.size(), 1000);

    QSet<quint32> baselineSet;
    for (quint32 id : baselineOrder)
        baselineSet.insert(id);
    QCOMPARE(baselineSet, expectedWidgetIds);

    QSet<quint32> pagedSet;
    for (quint32 id : pagedOrder)
        pagedSet.insert(id);
    QCOMPARE(pagedSet, expectedWidgetIds);

    QCOMPARE(pagedOrder, baselineOrder);
}

void VCAuthoringOwner_Test::authoringCore_querySetup_censusMeasurements_1000x20_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");
    auto clearAuthoringRecords = qScopeGuard([&]() { m_bridge->clearAuthoringRecords(); });

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(frame);
    page0->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setMultiPageMode(true);
    frame->setTotalPagesNumber(20);
    frame->setCurrentPage(0);
    frame->setLogicalGeometry(QRectF(0, 0, 920, 720));

    QList<quint32> widgetIds;
    widgetIds.reserve(1000);
    for (int subpage = 0; subpage < 20; ++subpage)
    {
        frame->setShortcutName(subpage, QStringLiteral("Measure %1").arg(subpage));
        for (int slot = 0; slot < 50; ++slot)
        {
            VCButton *button = new VCButton(doc, frame);
            vc->addWidgetToMap(button);
            button->setPage(subpage);
            frame->addWidgetToPageMap(button);
            button->setCaption(QStringLiteral("measure-%1-%2").arg(subpage).arg(slot));
            button->setLogicalGeometry(QRectF(10 + ((slot % 10) * 88), 10 + ((slot / 10) * 36), 80, 30));
            widgetIds << button->id();
        }
    }
    QCOMPARE(widgetIds.size(), 1000);

    const quint32 frameId = frame->id();
    QVariantList cleanupIds;
    cleanupIds.reserve(widgetIds.size() + 1);
    for (quint32 id : widgetIds)
        cleanupIds << QVariant::fromValue(id);
    cleanupIds << QVariant::fromValue(frameId);
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets(cleanupIds); });

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    QSet<quint32> expectedWidgetIds;
    expectedWidgetIds.reserve(widgetIds.size());
    for (quint32 id : widgetIds)
        expectedWidgetIds.insert(id);

    struct CensusMetrics
    {
        int pageCount = 0;
        int rowCount = 0;
        bool sawContinuation = false;
        qint64 elapsedMs = 0;
        qsizetype totalPayloadBytes = 0;
        qsizetype maxPayloadBytes = 0;
        QString basis;
        QList<quint32> orderedIds;
    };

    auto collectMetrics = [&](int limit, CensusMetrics &metrics) {
        VCBridge::AuthoringSetupQuery query;
        query.kind = VCBridge::AuthoringSetupQuery::Kind::Widgets;
        query.frameWidgetID = frameId;
        query.widgetIDs = widgetIds;
        query.recursive = false;
        query.limit = limit;
        query.fields =
            QStringList{QStringLiteral("identity"), QStringLiteral("placement"), QStringLiteral("logicalGeometry")};

        QSet<quint32> seen;
        QElapsedTimer timer;
        timer.start();
        for (;;)
        {
            const VCBridge::AuthoringReadResult result =
                m_bridge->queryAuthoring(VCBridge::AuthoringQuery(query));
            ++metrics.pageCount;
            QCOMPARE(result.code, QString());
            QCOMPARE(result.error, QString());

            if (metrics.basis.isEmpty())
            {
                QVERIFY(!result.basis.isEmpty());
                metrics.basis = result.basis;
            }
            else
            {
                QCOMPARE(result.basis, metrics.basis);
            }

            const qsizetype payloadBytes = qsizetype(VCAuthoring::toJson(result).dump().size());
            metrics.totalPayloadBytes += payloadBytes;
            metrics.maxPayloadBytes = std::max(metrics.maxPayloadBytes, payloadBytes);

            QVERIFY(int(result.rows.size()) <= limit);
            for (const VCBridge::AuthoringReadRow &typedRow : result.rows)
            {
                QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row));
                const auto row = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row);
                QVERIFY(expectedWidgetIds.contains(row.widgetID));
                QCOMPARE(row.pageWidgetID, page0->id());
                QCOMPARE(row.parentWidgetID, qint32(frameId));
                QVERIFY(row.childSubpage >= 0 && row.childSubpage < 20);
                QVERIFY(row.siblingIndex >= 0 && row.siblingIndex < 50);
                QVERIFY(row.logicalGeometry.has_value());
                QVERIFY(row.logicalGeometry->width() > 0.0);
                QVERIFY(row.logicalGeometry->height() > 0.0);
                QVERIFY2(!seen.contains(row.widgetID), "Measurement census emitted duplicate widget ID");
                seen.insert(row.widgetID);
                metrics.orderedIds << row.widgetID;
            }
            metrics.rowCount += int(result.rows.size());

            if (result.complete)
            {
                QVERIFY(result.nextCursor.isEmpty());
                break;
            }

            metrics.sawContinuation = true;
            QVERIFY2(!result.nextCursor.isEmpty(), "Expected continuation cursor for bounded measurement census");
            query.cursor = result.nextCursor;
            QVERIFY(metrics.pageCount < 300);
        }

        metrics.elapsedMs = timer.elapsed();
        QCOMPARE(seen, expectedWidgetIds);
        QCOMPARE(metrics.rowCount, 1000);
        QCOMPARE(metrics.orderedIds.size(), 1000);
    };

    CensusMetrics wide;
    collectMetrics(250, wide);
    CensusMetrics paged;
    collectMetrics(20, paged);

    QVERIFY(wide.sawContinuation);
    QVERIFY(paged.sawContinuation);
    QVERIFY(wide.pageCount >= 4);
    QVERIFY(paged.pageCount >= 50);
    QVERIFY(paged.pageCount >= wide.pageCount);
    QVERIFY(wide.elapsedMs >= 0);
    QVERIFY(paged.elapsedMs >= 0);
    QVERIFY(wide.totalPayloadBytes > 0);
    QVERIFY(paged.totalPayloadBytes > 0);
    QVERIFY(wide.maxPayloadBytes > 0);
    QVERIFY(paged.maxPayloadBytes > 0);
    QVERIFY(paged.totalPayloadBytes >= wide.totalPayloadBytes);
    QVERIFY(wide.maxPayloadBytes >= paged.maxPayloadBytes);
    QCOMPARE(paged.orderedIds, wide.orderedIds);

    qInfo() << "s2c13 1000x20 setup census metrics"
            << "widePages" << wide.pageCount
            << "wideMs" << wide.elapsedMs
            << "wideTotalPayloadBytes" << wide.totalPayloadBytes
            << "wideMaxPayloadBytes" << wide.maxPayloadBytes
            << "pagedPages" << paged.pageCount
            << "pagedMs" << paged.elapsedMs
            << "pagedTotalPayloadBytes" << paged.totalPayloadBytes
            << "pagedMaxPayloadBytes" << paged.maxPayloadBytes;

    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
}

void VCAuthoringOwner_Test::authoringCore_querySetup_recursiveTraversalMeasurements_1000x20_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");
    auto clearAuthoringRecords = qScopeGuard([&]() { m_bridge->clearAuthoringRecords(); });

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *outer = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(outer);
    page0->addWidgetToPageMap(outer);
    outer->setShowHeader(false);
    outer->setTotalPagesNumber(20);
    outer->setMultiPageMode(true);
    outer->setCurrentPage(0);
    outer->setLogicalGeometry(QRectF(0, 0, 960, 760));

    QList<quint32> nestedFrameIds;
    nestedFrameIds.reserve(20);
    QList<quint32> leafButtonIds;
    leafButtonIds.reserve(980);
    QSet<quint32> expectedDescendantIds;
    expectedDescendantIds.reserve(1000);

    for (int subpage = 0; subpage < 20; ++subpage)
    {
        outer->setShortcutName(subpage, QStringLiteral("Traversal %1").arg(subpage));

        VCFrame *nested = new VCFrame(doc, vc, outer);
        vc->addWidgetToMap(nested);
        nested->setPage(subpage);
        outer->addWidgetToPageMap(nested);
        nested->setShowHeader(false);
        nested->setMultiPageMode(false);
        nested->setTotalPagesNumber(1);
        nested->setCurrentPage(0);
        nested->setLogicalGeometry(QRectF(12 + ((subpage % 5) * 184), 18 + ((subpage / 5) * 176), 172, 162));

        nestedFrameIds << nested->id();
        expectedDescendantIds.insert(nested->id());

        for (int slot = 0; slot < 49; ++slot)
        {
            VCButton *button = new VCButton(doc, nested);
            vc->addWidgetToMap(button);
            button->setPage(0);
            nested->addWidgetToPageMap(button);
            button->setCaption(QStringLiteral("recursive-%1-%2").arg(subpage).arg(slot));
            button->setLogicalGeometry(QRectF(6 + ((slot % 7) * 24), 8 + ((slot / 7) * 22), 20, 18));

            leafButtonIds << button->id();
            expectedDescendantIds.insert(button->id());
        }
    }

    QCOMPARE(nestedFrameIds.size(), 20);
    QCOMPARE(leafButtonIds.size(), 980);
    QCOMPARE(expectedDescendantIds.size(), 1000);

    const quint32 outerId = outer->id();
    QVariantList cleanupIds;
    cleanupIds.reserve(1 + nestedFrameIds.size() + leafButtonIds.size());
    for (quint32 id : leafButtonIds)
        cleanupIds << QVariant::fromValue(id);
    for (quint32 id : nestedFrameIds)
        cleanupIds << QVariant::fromValue(id);
    cleanupIds << QVariant::fromValue(outerId);
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets(cleanupIds); });

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    QSet<quint32> nestedFrameSet;
    nestedFrameSet.reserve(nestedFrameIds.size());
    for (quint32 id : nestedFrameIds)
        nestedFrameSet.insert(id);

    struct TraversalMetrics
    {
        int pageCount = 0;
        int rowCount = 0;
        bool sawContinuation = false;
        bool sawTerminalEnvelope = false;
        qint64 elapsedMs = 0;
        qsizetype totalPayloadBytes = 0;
        qsizetype maxPayloadBytes = 0;
        QString basis;
        QList<quint32> orderedIds;
    };

    auto collectTraversal = [&](int limit, TraversalMetrics &metrics) {
        VCBridge::AuthoringSetupQuery query;
        query.kind = VCBridge::AuthoringSetupQuery::Kind::Widgets;
        query.pageWidgetID = page0->id();
        query.frameWidgetID = outerId;
        query.recursive = true;
        query.limit = limit;
        query.fields =
            QStringList{QStringLiteral("identity"), QStringLiteral("placement"), QStringLiteral("logicalGeometry")};

        QSet<quint32> seen;
        QElapsedTimer timer;
        timer.start();
        for (;;)
        {
            const VCBridge::AuthoringReadResult result =
                m_bridge->queryAuthoring(VCBridge::AuthoringQuery(query));
            ++metrics.pageCount;
            QCOMPARE(result.code, QString());
            QCOMPARE(result.error, QString());
            QVERIFY(int(result.rows.size()) <= limit);

            if (metrics.basis.isEmpty())
            {
                QVERIFY(!result.basis.isEmpty());
                metrics.basis = result.basis;
            }
            else
            {
                QCOMPARE(result.basis, metrics.basis);
            }

            const qsizetype payloadBytes = qsizetype(VCAuthoring::toJson(result).dump().size());
            metrics.totalPayloadBytes += payloadBytes;
            metrics.maxPayloadBytes = std::max(metrics.maxPayloadBytes, payloadBytes);

            for (const VCBridge::AuthoringReadRow &typedRow : result.rows)
            {
                QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row));
                const auto row = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row);

                QVERIFY(expectedDescendantIds.contains(row.widgetID));
                QVERIFY2(!seen.contains(row.widgetID), "Recursive traversal emitted duplicate widget ID");
                seen.insert(row.widgetID);
                metrics.orderedIds << row.widgetID;

                QCOMPARE(row.pageWidgetID, page0->id());
                QVERIFY(row.parentWidgetID >= 0);
                const quint32 parentId = quint32(row.parentWidgetID);
                QVERIFY(parentId == outerId || nestedFrameSet.contains(parentId));
                QVERIFY(row.childSubpage >= 0);
                QVERIFY(row.siblingIndex >= 0);
                QVERIFY(row.logicalGeometry.has_value());
                QVERIFY(row.logicalGeometry->width() > 0.0);
                QVERIFY(row.logicalGeometry->height() > 0.0);

                if (parentId == outerId)
                    QVERIFY(nestedFrameSet.contains(row.widgetID));
                else
                    QVERIFY(nestedFrameSet.contains(parentId));
            }
            metrics.rowCount += int(result.rows.size());

            if (result.complete)
            {
                metrics.sawTerminalEnvelope = true;
                QVERIFY(result.nextCursor.isEmpty());
                break;
            }

            metrics.sawContinuation = true;
            QVERIFY2(!result.nextCursor.isEmpty(),
                     "Expected continuation cursor for bounded recursive traversal census");
            query.cursor = result.nextCursor;
            QVERIFY(metrics.pageCount < 400);
        }

        metrics.elapsedMs = timer.elapsed();
        QCOMPARE(seen, expectedDescendantIds);
        QCOMPARE(metrics.rowCount, expectedDescendantIds.size());
        QCOMPARE(metrics.orderedIds.size(), expectedDescendantIds.size());
    };

    TraversalMetrics wide;
    collectTraversal(250, wide);
    TraversalMetrics paged;
    collectTraversal(20, paged);

    QVERIFY(wide.sawContinuation);
    QVERIFY(paged.sawContinuation);
    QVERIFY(wide.sawTerminalEnvelope);
    QVERIFY(paged.sawTerminalEnvelope);
    QCOMPARE(wide.rowCount, 1000);
    QCOMPARE(paged.rowCount, 1000);
    QVERIFY(wide.pageCount >= 4);
    QVERIFY(paged.pageCount >= 50);
    QVERIFY(paged.pageCount >= wide.pageCount);
    QVERIFY(wide.elapsedMs >= 0);
    QVERIFY(paged.elapsedMs >= 0);
    QVERIFY(wide.totalPayloadBytes > 0);
    QVERIFY(paged.totalPayloadBytes > 0);
    QVERIFY(wide.maxPayloadBytes > 0);
    QVERIFY(paged.maxPayloadBytes > 0);
    QVERIFY(paged.totalPayloadBytes >= wide.totalPayloadBytes);
    QVERIFY(wide.maxPayloadBytes >= paged.maxPayloadBytes);
    QCOMPARE(paged.basis, wide.basis);
    QCOMPARE(paged.orderedIds, wide.orderedIds);

    qInfo() << "s2c14 1000x20 recursive traversal metrics"
            << "widePages" << wide.pageCount
            << "wideMs" << wide.elapsedMs
            << "wideTotalPayloadBytes" << wide.totalPayloadBytes
            << "wideMaxPayloadBytes" << wide.maxPayloadBytes
            << "pagedPages" << paged.pageCount
            << "pagedMs" << paged.elapsedMs
            << "pagedTotalPayloadBytes" << paged.totalPayloadBytes
            << "pagedMaxPayloadBytes" << paged.maxPayloadBytes;

    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
}

void VCAuthoringOwner_Test::authoringCore_querySetup_recursiveUnfilteredCensus_continuation_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");
    auto clearAuthoringRecords = qScopeGuard([&]() { m_bridge->clearAuthoringRecords(); });

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *outer = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(outer);
    page0->addWidgetToPageMap(outer);
    outer->setShowHeader(false);
    outer->setTotalPagesNumber(2);
    outer->setMultiPageMode(true);
    outer->setCurrentPage(1);
    outer->setLogicalGeometry(QRectF(0, 0, 700, 460));

    VCFrame *nestedA = new VCFrame(doc, vc, outer);
    vc->addWidgetToMap(nestedA);
    nestedA->setPage(1);
    outer->addWidgetToPageMap(nestedA);
    nestedA->setShowHeader(false);
    nestedA->setTotalPagesNumber(2);
    nestedA->setMultiPageMode(true);
    nestedA->setCurrentPage(0);
    nestedA->setLogicalGeometry(QRectF(20, 20, 280, 200));

    VCFrame *nestedB = new VCFrame(doc, vc, outer);
    vc->addWidgetToMap(nestedB);
    nestedB->setPage(1);
    outer->addWidgetToPageMap(nestedB);
    nestedB->setShowHeader(false);
    nestedB->setTotalPagesNumber(2);
    nestedB->setMultiPageMode(true);
    nestedB->setCurrentPage(1);
    nestedB->setLogicalGeometry(QRectF(340, 20, 280, 200));

    VCButton *a0 = new VCButton(doc, nestedA);
    vc->addWidgetToMap(a0);
    a0->setPage(0);
    nestedA->addWidgetToPageMap(a0);
    a0->setCaption(QStringLiteral("nestedA-0"));
    a0->setLogicalGeometry(QRectF(10, 10, 100, 60));

    VCButton *a1 = new VCButton(doc, nestedA);
    vc->addWidgetToMap(a1);
    a1->setPage(1);
    nestedA->addWidgetToPageMap(a1);
    a1->setCaption(QStringLiteral("nestedA-1"));
    a1->setLogicalGeometry(QRectF(130, 10, 100, 60));

    VCButton *b0 = new VCButton(doc, nestedB);
    vc->addWidgetToMap(b0);
    b0->setPage(0);
    nestedB->addWidgetToPageMap(b0);
    b0->setCaption(QStringLiteral("nestedB-0"));
    b0->setLogicalGeometry(QRectF(10, 10, 100, 60));

    VCButton *b1 = new VCButton(doc, nestedB);
    vc->addWidgetToMap(b1);
    b1->setPage(1);
    nestedB->addWidgetToPageMap(b1);
    b1->setCaption(QStringLiteral("nestedB-1"));
    b1->setLogicalGeometry(QRectF(130, 10, 100, 60));

    VCButton *outerPeerPage0 = new VCButton(doc, outer);
    vc->addWidgetToMap(outerPeerPage0);
    outerPeerPage0->setPage(0);
    outer->addWidgetToPageMap(outerPeerPage0);
    outerPeerPage0->setCaption(QStringLiteral("outer-page0-peer"));
    outerPeerPage0->setLogicalGeometry(QRectF(20, 280, 120, 70));

    const quint32 outerId = outer->id();
    const quint32 nestedAId = nestedA->id();
    const quint32 nestedBId = nestedB->id();
    const quint32 a0Id = a0->id();
    const quint32 a1Id = a1->id();
    const quint32 b0Id = b0->id();
    const quint32 b1Id = b1->id();
    const quint32 outerPeerId = outerPeerPage0->id();
    auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({a0Id, a1Id, b0Id, b1Id, outerPeerId, nestedAId, nestedBId, outerId});
    });

    auto collectRecursiveUnfiltered = [&](int limit, QList<quint32> &ordered, bool &sawContinuation,
                                          QString &basisOut) {
        VCBridge::AuthoringSetupQuery q;
        q.kind = VCBridge::AuthoringSetupQuery::Kind::Widgets;
        q.pageWidgetID = page0->id();
        q.frameWidgetID = outerId;
        q.childSubpage = 1;
        q.recursive = true;
        q.limit = limit;
        q.fields = QStringList{QStringLiteral("identity"), QStringLiteral("placement")};

        QSet<quint32> seen;
        for (;;)
        {
            const VCBridge::AuthoringReadResult result =
                m_bridge->queryAuthoring(VCBridge::AuthoringQuery(q));
            QCOMPARE(result.code, QString());
            if (basisOut.isEmpty())
            {
                QVERIFY(!result.basis.isEmpty());
                basisOut = result.basis;
            }
            else
            {
                QCOMPARE(result.basis, basisOut);
            }

            for (const VCBridge::AuthoringReadRow &typedRow : result.rows)
            {
                QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row));
                const auto row = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row);
                QVERIFY2(!seen.contains(row.widgetID), "Recursive unfiltered query emitted duplicate widgetID");
                seen.insert(row.widgetID);
                ordered << row.widgetID;
                QVERIFY(row.widgetID != outerPeerId);
            }

            if (result.complete)
            {
                QVERIFY(result.nextCursor.isEmpty());
                break;
            }

            sawContinuation = true;
            QVERIFY2(!result.nextCursor.isEmpty(),
                     "Recursive unfiltered query should expose continuation cursor when incomplete");
            q.cursor = result.nextCursor;
        }
    };

    QList<quint32> baselineOrder;
    bool baselineSawContinuation = false;
    QString baselineBasis;
    collectRecursiveUnfiltered(50, baselineOrder, baselineSawContinuation, baselineBasis);
    QVERIFY(!baselineBasis.isEmpty());

    QList<quint32> pagedOrder;
    bool pagedSawContinuation = false;
    QString pagedBasis;
    collectRecursiveUnfiltered(2, pagedOrder, pagedSawContinuation, pagedBasis);
    QVERIFY(pagedSawContinuation);
    QCOMPARE(pagedBasis, baselineBasis);

    QSet<quint32> expected{
        nestedAId, nestedBId, a0Id, a1Id, b0Id, b1Id
    };
    QSet<quint32> baselineSet;
    for (quint32 id : baselineOrder)
        baselineSet.insert(id);
    QSet<quint32> pagedSet;
    for (quint32 id : pagedOrder)
        pagedSet.insert(id);

    QCOMPARE(baselineSet, expected);
    QCOMPARE(pagedSet, expected);
    QCOMPARE(pagedOrder, baselineOrder);
}

void VCAuthoringOwner_Test::authoringCore_querySetup_scopeOrderDigest_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *inside = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(inside);
    page0->addWidgetToPageMap(inside);
    inside->setShowHeader(false);
    inside->setCurrentPage(0);
    inside->setLogicalGeometry(QRectF(0, 0, 600, 360));

    VCButton *insideA = new VCButton(doc, inside);
    vc->addWidgetToMap(insideA);
    inside->addWidgetToPageMap(insideA);
    insideA->setPage(0);
    insideA->setCaption(QStringLiteral("inside-A"));
    insideA->setLogicalGeometry(QRectF(10, 10, 100, 70));

    VCButton *insideB = new VCButton(doc, inside);
    vc->addWidgetToMap(insideB);
    inside->addWidgetToPageMap(insideB);
    insideB->setPage(0);
    insideB->setCaption(QStringLiteral("inside-B"));
    insideB->setLogicalGeometry(QRectF(140, 10, 100, 70));

    VCFrame *nested = new VCFrame(doc, vc, inside);
    vc->addWidgetToMap(nested);
    inside->addWidgetToPageMap(nested);
    nested->setShowHeader(false);
    nested->setCurrentPage(0);
    nested->setPage(0);
    nested->setLogicalGeometry(QRectF(270, 10, 140, 110));

    VCButton *nestedButton = new VCButton(doc, nested);
    vc->addWidgetToMap(nestedButton);
    nested->addWidgetToPageMap(nestedButton);
    nestedButton->setPage(0);
    nestedButton->setCaption(QStringLiteral("nested-D"));
    nestedButton->setLogicalGeometry(QRectF(10, 10, 80, 60));

    VCFrame *outside = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(outside);
    page0->addWidgetToPageMap(outside);
    outside->setShowHeader(false);
    outside->setCurrentPage(0);
    outside->setLogicalGeometry(QRectF(0, 380, 600, 360));

    VCButton *outsideButton = new VCButton(doc, outside);
    vc->addWidgetToMap(outsideButton);
    outside->addWidgetToPageMap(outsideButton);
    outsideButton->setPage(0);
    outsideButton->setCaption(QStringLiteral("outside-C"));
    outsideButton->setLogicalGeometry(QRectF(10, 10, 100, 70));

    const quint32 insideId = inside->id();
    const quint32 insideAId = insideA->id();
    const quint32 insideBId = insideB->id();
    const quint32 nestedId = nested->id();
    const quint32 nestedButtonId = nestedButton->id();
    const quint32 outsideId = outside->id();
    const quint32 outsideButtonId = outsideButton->id();
    auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({insideAId, insideBId, nestedButtonId, nestedId, outsideButtonId, insideId, outsideId});
    });

    VCBridge::AuthoringSetupQuery query;
    query.kind = VCBridge::AuthoringSetupQuery::Kind::Widgets;
    query.limit = 10;
    query.pageWidgetID = page0->id();
    query.frameWidgetID = insideId;
    query.childSubpage = 0;
    query.recursive = false;
    query.fields = QStringList{
        QStringLiteral("identity"),
        QStringLiteral("placement"),
        QStringLiteral("appearance")
    };
    query.widgetIDs = QList<quint32>{insideAId, outsideButtonId, insideBId, 0x7ffffff0u};

    const VCBridge::AuthoringReadResult result =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(query));

    QCOMPARE(result.code, QStringLiteral("invalidArgument"));
    QVERIFY(result.error.contains(QStringLiteral("widgetIDs"), Qt::CaseInsensitive));

    VCBridge::AuthoringSetupQuery inScopeOnly = query;
    inScopeOnly.widgetIDs = QList<quint32>{insideAId, insideBId};
    const VCBridge::AuthoringReadResult ok =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(inScopeOnly));
    QCOMPARE(ok.code, QString());
    QCOMPARE(ok.rows.size(), 2);
    const Json okJson = VCAuthoring::toJson(ok);
    QVERIFY2(okJson.contains("rows") && okJson["rows"].is_array() && !okJson["rows"].empty(),
             okJson.dump().c_str());
    QVERIFY2(!okJson["rows"].at(0).contains("logicalGeometry"),
             "logicalGeometry must be omitted when logicalGeometry group is not requested");

    QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(ok.rows.at(0).row));
    QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(ok.rows.at(1).row));
    const auto first = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(ok.rows.at(0).row);
    const auto second = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(ok.rows.at(1).row);
    QCOMPARE(first.widgetID, insideAId);
    QCOMPARE(second.widgetID, insideBId);
    QCOMPARE(first.siblingIndex, 0);
    QCOMPARE(second.siblingIndex, 1);
    QCOMPARE(first.parentWidgetID, qint32(insideId));
    QCOMPARE(first.childSubpage, 0);
    QVERIFY(first.configurationDigest.has_value());
    QVERIFY(second.configurationDigest.has_value());

    VCBridge::AuthoringSetupQuery identityOnly = inScopeOnly;
    identityOnly.fields = QStringList{QStringLiteral("identity")};
    const VCBridge::AuthoringReadResult identityOnlyResult =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(identityOnly));
    QCOMPARE(identityOnlyResult.code, QString());
    QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(identityOnlyResult.rows.at(0).row));
    const auto identityOnlyFirst =
        std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(identityOnlyResult.rows.at(0).row);
    QCOMPARE(identityOnlyFirst.parentWidgetID, qint32(insideId));
    QCOMPARE(identityOnlyFirst.childSubpage, 0);
    QCOMPARE(identityOnlyFirst.siblingIndex, 0);

    VCBridge::AuthoringSetupQuery nestedRecursive = query;
    nestedRecursive.recursive = true;
    nestedRecursive.fields = QStringList{QStringLiteral("identity"), QStringLiteral("placement")};
    nestedRecursive.widgetIDs = QList<quint32>{nestedButtonId};
    const VCBridge::AuthoringReadResult nestedOk =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(nestedRecursive));
    QCOMPARE(nestedOk.code, QString());
    QCOMPARE(nestedOk.rows.size(), 1);
    QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(nestedOk.rows.at(0).row));
    const auto nestedRow = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(nestedOk.rows.at(0).row);
    QCOMPARE(nestedRow.widgetID, nestedButtonId);
    QCOMPARE(nestedRow.parentWidgetID, qint32(nestedId));

    VCBridge::AuthoringSetupQuery nestedOutScope = nestedRecursive;
    nestedOutScope.widgetIDs = QList<quint32>{outsideButtonId};
    const VCBridge::AuthoringReadResult nestedOutScopeResult =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(nestedOutScope));
    QCOMPARE(nestedOutScopeResult.code, QStringLiteral("invalidArgument"));
    QVERIFY(nestedOutScopeResult.error.contains(QStringLiteral("out-of-scope"), Qt::CaseInsensitive));

    const QString digestBefore = *first.configurationDigest;
    outsideButton->setDisabled(!outsideButton->isDisabled());
    const VCBridge::AuthoringReadResult afterOutsideMutation =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(inScopeOnly));
    QCOMPARE(afterOutsideMutation.code, QString());
    QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(afterOutsideMutation.rows.at(0).row));
    const auto afterFirst = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(afterOutsideMutation.rows.at(0).row);
    QVERIFY(afterFirst.configurationDigest.has_value());
    QCOMPARE(*afterFirst.configurationDigest, digestBefore);

    VCBridge::AuthoringSetupQuery withGeometry = inScopeOnly;
    withGeometry.fields = QStringList{
        QStringLiteral("identity"),
        QStringLiteral("placement"),
        QStringLiteral("logicalGeometry")
    };
    const VCBridge::AuthoringReadResult geometryRead =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(withGeometry));
    QCOMPARE(geometryRead.code, QString());
    const Json geometryJson = VCAuthoring::toJson(geometryRead);
    QVERIFY2(geometryJson.contains("rows") && geometryJson["rows"].is_array() && !geometryJson["rows"].empty(),
             geometryJson.dump().c_str());
    QVERIFY2(geometryJson["rows"].at(0).contains("logicalGeometry"),
             "logicalGeometry must be present when logicalGeometry group is requested");
}

void VCAuthoringOwner_Test::authoringCore_querySetup_recursiveFrameSubpageJoin_redDraft()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *outer = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(outer);
    page0->addWidgetToPageMap(outer);
    outer->setShowHeader(false);
    outer->setTotalPagesNumber(2);
    outer->setMultiPageMode(true);
    outer->setCurrentPage(1);
    QCOMPARE(outer->currentPage(), 1);
    outer->setLogicalGeometry(QRectF(0, 0, 640, 420));

    VCFrame *nested = new VCFrame(doc, vc, outer);
    vc->addWidgetToMap(nested);
    nested->setPage(1);
    outer->addWidgetToPageMap(nested);
    nested->setShowHeader(false);
    nested->setTotalPagesNumber(2);
    nested->setMultiPageMode(true);
    nested->setCurrentPage(0);
    QCOMPARE(nested->page(), 1);
    QCOMPARE(nested->currentPage(), 0);
    nested->setLogicalGeometry(QRectF(20, 20, 260, 220));

    VCButton *grandchildLocal0 = new VCButton(doc, nested);
    vc->addWidgetToMap(grandchildLocal0);
    grandchildLocal0->setPage(0);
    nested->addWidgetToPageMap(grandchildLocal0);
    grandchildLocal0->setCaption(QStringLiteral("grandchild-local-0"));
    grandchildLocal0->setLogicalGeometry(QRectF(10, 10, 120, 80));

    VCButton *outerPage0Peer = new VCButton(doc, outer);
    vc->addWidgetToMap(outerPage0Peer);
    outer->addWidgetToPageMap(outerPage0Peer);
    outerPage0Peer->setCaption(QStringLiteral("outer-page-0-peer"));
    outerPage0Peer->setPage(0);
    outerPage0Peer->setLogicalGeometry(QRectF(300, 20, 120, 80));

    const quint32 outerId = outer->id();
    const quint32 nestedId = nested->id();
    const quint32 grandchildId = grandchildLocal0->id();
    const quint32 outerPeerId = outerPage0Peer->id();
    auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({grandchildId, outerPeerId, nestedId, outerId});
    });

    VCBridge::AuthoringSetupQuery census;
    census.kind = VCBridge::AuthoringSetupQuery::Kind::Widgets;
    census.frameWidgetID = outerId;
    census.childSubpage = 1;
    census.recursive = true;
    census.limit = 50;
    census.fields = QStringList{QStringLiteral("identity"), QStringLiteral("placement")};

    const VCBridge::AuthoringReadResult censusResult =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(census));
    QCOMPARE(censusResult.code, QString());
    QVERIFY2(!censusResult.rows.isEmpty(), "Expected at least nested frame and child rows");

    bool sawGrandchild = false;
    for (const VCBridge::AuthoringReadRow &typedRow : censusResult.rows)
    {
        QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row));
        const auto row = std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(typedRow.row);
        QVERIFY(row.widgetID != outerPeerId);
        if (row.widgetID == grandchildId)
        {
            sawGrandchild = true;
            QCOMPARE(row.parentWidgetID, qint32(nestedId));
            QCOMPARE(row.childSubpage, 0);
        }
    }
    QVERIFY2(sawGrandchild, "Recursive frame scope on outer subpage must include nested grandchild on local page 0");

    VCBridge::AuthoringSetupQuery explicitInScope = census;
    explicitInScope.widgetIDs = QList<quint32>{grandchildId};
    const VCBridge::AuthoringReadResult explicitInScopeResult =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(explicitInScope));
    QCOMPARE(explicitInScopeResult.code, QString());
    QCOMPARE(explicitInScopeResult.rows.size(), 1);
    QVERIFY(std::holds_alternative<VCBridge::AuthoringReadRow::SetupWidgetRow>(explicitInScopeResult.rows.at(0).row));
    const auto explicitRow =
        std::get<VCBridge::AuthoringReadRow::SetupWidgetRow>(explicitInScopeResult.rows.at(0).row);
    QCOMPARE(explicitRow.widgetID, grandchildId);
    QCOMPARE(explicitRow.childSubpage, 0);

    VCBridge::AuthoringSetupQuery explicitOutScope = census;
    explicitOutScope.widgetIDs = QList<quint32>{outerPeerId};
    const VCBridge::AuthoringReadResult explicitOutScopeResult =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(explicitOutScope));
    QCOMPARE(explicitOutScopeResult.code, QStringLiteral("invalidArgument"));
    QVERIFY(explicitOutScopeResult.error.contains(QStringLiteral("out-of-scope"), Qt::CaseInsensitive));

    VCBridge::AuthoringSetupQuery nonRecursive = census;
    nonRecursive.recursive = false;
    nonRecursive.widgetIDs = QList<quint32>{grandchildId};
    const VCBridge::AuthoringReadResult nonRecursiveResult =
        m_bridge->queryAuthoring(VCBridge::AuthoringQuery(nonRecursive));
    QCOMPARE(nonRecursiveResult.code, QStringLiteral("invalidArgument"));
    QVERIFY(nonRecursiveResult.error.contains(QStringLiteral("out-of-scope"), Qt::CaseInsensitive));
}

void VCAuthoringOwner_Test::authoringPreparation_boundsAcrossFamilies_data()
{
    QTest::addColumn<QString>("family");
    QTest::addColumn<QString>("boundKind");

    const QStringList families = {QStringLiteral("layout"), QStringLiteral("bindings"), QStringLiteral("recordPanel")};
    const QStringList bounds = {
        QStringLiteral("items-128"),
        QStringLiteral("items-129"),
        QStringLiteral("roots-128"),
        QStringLiteral("roots-129"),
        QStringLiteral("bytes-65536"),
        QStringLiteral("bytes-65537")
    };
    for (const QString &family : families)
        for (const QString &bound : bounds)
            QTest::newRow(qPrintable(family + "-" + bound)) << family << bound;
}

void VCAuthoringOwner_Test::authoringPreparation_boundsAcrossFamilies()
{
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, family);
    QFETCH(QString, boundKind);

    auto canonicalSizedBasis = [&](auto makeRequest, int targetBytes) -> QString {
        for (int i = 0; i <= targetBytes + 2048; ++i)
        {
            auto request = makeRequest(QString(i, QChar('x')));
            const QByteArray canonical = m_bridge->canonicalizeRequest(request);
            if (canonical.size() == targetBytes)
                return QString(i, QChar('x'));
            if (canonical.size() > targetBytes)
                break;
        }
        return QString();
    };

    VCBridge::AuthoringPreparationResult result;
    if (family == QStringLiteral("layout"))
    {
        VCBridge::AuthoringLayoutRequest request;
        request.operation = VCBridge::AuthoringLayoutRequest::Operation::PageCreate;
        request.name = QStringLiteral("page");
        if (boundKind == QStringLiteral("items-128"))
        {
            for (int i = 0; i < 128; ++i)
                request.widgetIDs.append(quint32(1000 + i));
        }
        else if (boundKind == QStringLiteral("items-129"))
        {
            for (int i = 0; i < 129; ++i)
                request.widgetIDs.append(quint32(1000 + i));
        }
        else if (boundKind == QStringLiteral("roots-128"))
        {
            for (int i = 0; i < 128; ++i)
                request.widgetIDs.append(quint32(2000 + i));
        }
        else if (boundKind == QStringLiteral("roots-129"))
        {
            for (int i = 0; i < 129; ++i)
                request.widgetIDs.append(quint32(1000 + i));
        }
        else if (boundKind == QStringLiteral("bytes-65536") || boundKind == QStringLiteral("bytes-65537"))
        {
            const int target = (boundKind == QStringLiteral("bytes-65536")) ? 65536 : 65537;
            const QString basis = canonicalSizedBasis(
                [&](const QString &b) -> VCBridge::AuthoringRequest {
                    VCBridge::AuthoringLayoutRequest r;
                    r.operation = VCBridge::AuthoringLayoutRequest::Operation::PageCreate;
                    r.name = QStringLiteral("page");
                    r.expectedBasis = b;
                    return VCBridge::AuthoringRequest(r);
                },
                target);
            QVERIFY2(!basis.isNull(), "Failed to construct exact canonical-size layout request");
            request.expectedBasis = basis;
        }
        else
            QFAIL("Unexpected bound kind");
        result = m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    }
    else if (family == QStringLiteral("bindings"))
    {
        VCBridge::AuthoringBindingsRequest request;
        if (boundKind == QStringLiteral("items-128"))
        {
            for (int i = 0; i < 128; ++i)
            {
                VCBridge::AuthoringBindingItem item;
                item.widgetID = 1234;
                item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
                item.action = VCBridge::AuthoringBindingItem::Action::Set;
                request.items.append(item);
            }
        }
        else if (boundKind == QStringLiteral("items-129"))
        {
            for (int i = 0; i < 129; ++i)
            {
                VCBridge::AuthoringBindingItem item;
                item.widgetID = 1234;
                item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
                item.action = VCBridge::AuthoringBindingItem::Action::Set;
                request.items.append(item);
            }
        }
        else if (boundKind == QStringLiteral("roots-128"))
        {
            for (int i = 0; i < 128; ++i)
            {
                VCBridge::AuthoringBindingItem item;
                item.widgetID = quint32(1000 + i);
                item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
                item.action = VCBridge::AuthoringBindingItem::Action::Set;
                request.items.append(item);
            }
        }
        else if (boundKind == QStringLiteral("roots-129"))
        {
            for (int i = 0; i < 129; ++i)
            {
                VCBridge::AuthoringBindingItem item;
                item.widgetID = quint32(1000 + i);
                item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
                item.action = VCBridge::AuthoringBindingItem::Action::Set;
                request.items.append(item);
            }
        }
        else if (boundKind == QStringLiteral("bytes-65536") || boundKind == QStringLiteral("bytes-65537"))
        {
            VCBridge::AuthoringBindingItem item;
            item.widgetID = 1234;
            item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            request.items.append(item);
            const int target = (boundKind == QStringLiteral("bytes-65536")) ? 65536 : 65537;
            const QString basis = canonicalSizedBasis(
                [&](const QString &b) -> VCBridge::AuthoringRequest {
                    VCBridge::AuthoringBindingsRequest r = request;
                    r.expectedBasis = b;
                    return VCBridge::AuthoringRequest(r);
                },
                target);
            QVERIFY2(!basis.isNull(), "Failed to construct exact canonical-size bindings request");
            request.expectedBasis = basis;
        }
        else
            QFAIL("Unexpected bound kind");
        result = m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    }
    else
    {
        VCBridge::AuthoringRecordPanelRequest request;
        if (boundKind == QStringLiteral("items-128"))
        {
            for (int i = 0; i < 128; ++i)
            {
                VCBridge::AuthoringRecordPanelItem item;
                item.widgetID = 1234;
                request.items.append(item);
            }
        }
        else if (boundKind == QStringLiteral("items-129"))
        {
            for (int i = 0; i < 129; ++i)
            {
                VCBridge::AuthoringRecordPanelItem item;
                item.widgetID = 1234;
                request.items.append(item);
            }
        }
        else if (boundKind == QStringLiteral("roots-128"))
        {
            for (int i = 0; i < 128; ++i)
            {
                VCBridge::AuthoringRecordPanelItem item;
                item.widgetID = quint32(1000 + i);
                request.items.append(item);
            }
        }
        else if (boundKind == QStringLiteral("roots-129"))
        {
            for (int i = 0; i < 129; ++i)
            {
                VCBridge::AuthoringRecordPanelItem item;
                item.widgetID = quint32(1000 + i);
                request.items.append(item);
            }
        }
        else if (boundKind == QStringLiteral("bytes-65536") || boundKind == QStringLiteral("bytes-65537"))
        {
            VCBridge::AuthoringRecordPanelItem item;
            item.widgetID = 1234;
            request.items.append(item);
            const int target = (boundKind == QStringLiteral("bytes-65536")) ? 65536 : 65537;
            const QString basis = canonicalSizedBasis(
                [&](const QString &b) -> VCBridge::AuthoringRequest {
                    VCBridge::AuthoringRecordPanelRequest r = request;
                    r.expectedBasis = b;
                    return VCBridge::AuthoringRequest(r);
                },
                target);
            QVERIFY2(!basis.isNull(), "Failed to construct exact canonical-size record-panel request");
            request.expectedBasis = basis;
        }
        else
            QFAIL("Unexpected bound kind");
        result = m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    }

    if (boundKind == QStringLiteral("items-129") ||
        boundKind == QStringLiteral("roots-129") ||
        boundKind == QStringLiteral("bytes-65537"))
    {
        QCOMPARE(result.code, QStringLiteral("preparationLimit"));
        if (family == QStringLiteral("bindings"))
            QCOMPARE(result.atomicScope, VCBridge::AuthoringPreparationResult::AtomicScope::IndependentItems);
        QVERIFY(result.preparationToken.isEmpty());
        QCOMPARE(result.expiresInMs, 0);
    }
    else
    {
        QVERIFY2(result.code != QStringLiteral("preparationLimit"),
                 qPrintable(QStringLiteral("Unexpected preparationLimit for %1/%2")
                                .arg(family, boundKind)));
    }
}

void VCAuthoringOwner_Test::authoringLayout_singleOperationRows_data()
{
    QTest::addColumn<QString>("operation");

    QTest::newRow("page-create") << QStringLiteral("page-create");
    QTest::newRow("align") << QStringLiteral("align");
    QTest::newRow("frame-pages") << QStringLiteral("frame-pages");
    QTest::newRow("page-rename") << QStringLiteral("page-rename");
    QTest::newRow("page-delete") << QStringLiteral("page-delete");
}

void VCAuthoringOwner_Test::authoringLayout_singleOperationRows()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, operation);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 300));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    int createdPageIndex = -1;
    auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({buttonId, frameId});
        if (createdPageIndex >= 0 && createdPageIndex < vc->pagesCount())
            vc->deletePage(createdPageIndex);
    });

    VCBridge::AuthoringLayoutRequest request;
    if (operation == QStringLiteral("page-create"))
    {
        request.operation = VCBridge::AuthoringLayoutRequest::Operation::PageCreate;
        request.name = QStringLiteral("new-page");
    }
    else if (operation == QStringLiteral("align"))
    {
        request.operation = VCBridge::AuthoringLayoutRequest::Operation::Align;
        request.frameWidgetID = frameId;
        request.columns = 1;
        request.padding = 0;
    }
    else if (operation == QStringLiteral("frame-pages"))
    {
        request.operation = VCBridge::AuthoringLayoutRequest::Operation::FramePages;
        request.frameWidgetID = frameId;
        request.totalPages = 2;
        request.pageLabels = QStringList{QStringLiteral("A"), QStringLiteral("B")};
        request.pagesLoop = false;
    }
    else if (operation == QStringLiteral("page-rename"))
    {
        request.operation = VCBridge::AuthoringLayoutRequest::Operation::PageRename;
        request.pageWidgetID = quint32(page->id());
        request.name = QStringLiteral("renamed-page");
    }
    else
    {
        createdPageIndex = vc->pagesCount();
        vc->addPage(createdPageIndex);
        VCPage *extraPage = vc->page(createdPageIndex);
        QVERIFY(extraPage != nullptr);
        request.operation = VCBridge::AuthoringLayoutRequest::Operation::PageDelete;
        request.pageWidgetID = quint32(extraPage->id());
    }

    const VCBridge::AuthoringPreparationResult result =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    const bool supportedOperation =
        (operation == QStringLiteral("page-create") ||
         operation == QStringLiteral("frame-pages") ||
         operation == QStringLiteral("page-rename") ||
         operation == QStringLiteral("page-delete"));
    if (supportedOperation)
    {
        QCOMPARE(result.code, QString());
        QVERIFY(!result.preparationToken.isEmpty());
        const VCBridge::AuthoringApplyResult applied =
            m_bridge->applyAuthoring(result.preparationToken);
        QCOMPARE(applied.code, QString());
        QCOMPARE(applied.tokenConsumed, true);
    }
    else
    {
        QCOMPARE(result.code, QStringLiteral("unsupportedPreparation"));
        QVERIFY(result.preparationToken.isEmpty());
    }
}

void VCAuthoringOwner_Test::authoringCanonical_optionalAndDelimiterRows_data()
{
    QTest::addColumn<QString>("scenario");

    QTest::newRow("recordpanel-nullopt-vs-empty") << QStringLiteral("recordpanel-nullopt-vs-empty");
    QTest::newRow("bindings-profile-U-vs-nullopt") << QStringLiteral("bindings-profile-U-vs-nullopt");
    QTest::newRow("layout-labels-pipe-collision") << QStringLiteral("layout-labels-pipe-collision");
    QTest::newRow("layout-anchor-nullopt-vs-zero") << QStringLiteral("layout-anchor-nullopt-vs-zero");
}

void VCAuthoringOwner_Test::authoringCanonical_optionalAndDelimiterRows()
{
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);

    QByteArray first;
    QByteArray second;
    if (scenario == QStringLiteral("recordpanel-nullopt-vs-empty"))
    {
        VCBridge::AuthoringRecordPanelRequest a;
        VCBridge::AuthoringRecordPanelItem ai;
        ai.widgetID = 100;
        a.items.append(ai);

        VCBridge::AuthoringRecordPanelRequest b;
        VCBridge::AuthoringRecordPanelItem bi;
        bi.widgetID = 100;
        bi.targetFolder = QString();
        b.items.append(bi);

        first = m_bridge->canonicalizeRequest(VCBridge::AuthoringRequest(a));
        second = m_bridge->canonicalizeRequest(VCBridge::AuthoringRequest(b));
    }
    else if (scenario == QStringLiteral("bindings-profile-U-vs-nullopt"))
    {
        VCBridge::AuthoringBindingsRequest a;
        VCBridge::AuthoringBindingItem ai;
        ai.widgetID = 100;
        ai.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        ai.action = VCBridge::AuthoringBindingItem::Action::Set;
        VCBridge::AuthoringDestination ad;
        ad.inputUniverseID = 0;
        ad.inputChannel = 1;
        ai.destination = ad;
        a.items.append(ai);

        VCBridge::AuthoringBindingsRequest b;
        VCBridge::AuthoringBindingItem bi = ai;
        VCBridge::AuthoringDestination bd = ad;
        bd.profileName = QStringLiteral("U");
        bi.destination = bd;
        b.items.append(bi);

        first = m_bridge->canonicalizeRequest(VCBridge::AuthoringRequest(a));
        second = m_bridge->canonicalizeRequest(VCBridge::AuthoringRequest(b));
    }
    else if (scenario == QStringLiteral("layout-labels-pipe-collision"))
    {
        VCBridge::AuthoringLayoutRequest a;
        a.operation = VCBridge::AuthoringLayoutRequest::Operation::FramePages;
        a.pageLabels = QStringList{QStringLiteral("a|b")};

        VCBridge::AuthoringLayoutRequest b;
        b.operation = VCBridge::AuthoringLayoutRequest::Operation::FramePages;
        b.pageLabels = QStringList{QStringLiteral("a"), QStringLiteral("b")};

        first = m_bridge->canonicalizeRequest(VCBridge::AuthoringRequest(a));
        second = m_bridge->canonicalizeRequest(VCBridge::AuthoringRequest(b));
    }
    else
    {
        VCBridge::AuthoringLayoutRequest a;
        a.operation = VCBridge::AuthoringLayoutRequest::Operation::MoveGroup;
        a.logicalAnchor = std::nullopt;

        VCBridge::AuthoringLayoutRequest b = a;
        b.logicalAnchor = QPointF(0.0, 0.0);

        first = m_bridge->canonicalizeRequest(VCBridge::AuthoringRequest(a));
        second = m_bridge->canonicalizeRequest(VCBridge::AuthoringRequest(b));
    }

    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());
    QVERIFY2(first != second, "Semantically distinct requests must canonicalize differently");
}

void VCAuthoringOwner_Test::authoringGeometry_diffDetailRows_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::newRow("invalid-diff-detail") << QStringLiteral("invalid-diff-detail");
    QTest::newRow("summary-changed") << QStringLiteral("summary-changed");
    QTest::newRow("full-changed") << QStringLiteral("full-changed");
    QTest::newRow("full-unchanged") << QStringLiteral("full-unchanged");
}

void VCAuthoringOwner_Test::authoringGeometry_diffDetailRows()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 300));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({buttonId, frameId}); });

    VCBridge::AuthoringLayoutRequest request;
    VCBridge::AuthoringGeometryChange change;
    change.widgetID = buttonId;
    const QRectF beforeRect = button->logicalGeometry();
    change.logicalRect = (scenario == QStringLiteral("full-unchanged"))
        ? beforeRect
        : QRectF(120, 80, 80, 60);
    request.geometryChanges.append(change);

    if (scenario == QStringLiteral("invalid-diff-detail"))
        request.diffDetail = QStringLiteral("not-valid");
    else if (scenario == QStringLiteral("summary-changed"))
        request.diffDetail = QStringLiteral("summary");
    else
        request.diffDetail = QStringLiteral("full");

    const VCBridge::AuthoringPreparationResult prep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));

    if (scenario == QStringLiteral("invalid-diff-detail"))
    {
        QCOMPARE(prep.code, QStringLiteral("invalidArgument"));
        QVERIFY(prep.preparationToken.isEmpty());
        QCOMPARE(prep.expiresInMs, 0);
        QVERIFY(prep.preconditions.isEmpty());
        return;
    }

    QCOMPARE(prep.code, QString());
    QVERIFY(!prep.preparationToken.isEmpty());
    QCOMPARE(prep.items.size(), 1);
    const VCBridge::AuthoringPreparationItem &item = prep.items.first();
    QVERIFY(item.selectedPageWidgetID.has_value());
    QVERIFY(item.selectedParentWidgetID.has_value());
    QVERIFY(item.selectedChildSubpage.has_value());
    QVERIFY(item.configurationDigest.has_value());

    if (scenario == QStringLiteral("summary-changed"))
    {
        QCOMPARE(prep.diffDetail, VCBridge::AuthoringPreparationResult::DiffDetail::Summary);
        QCOMPARE(prep.diffComplete, false);
        QCOMPARE(item.outcome, VCBridge::AuthoringPreparationItem::Outcome::Prepared);
        QCOMPARE(item.changedCount, 1);
    }
    else if (scenario == QStringLiteral("full-changed"))
    {
        QCOMPARE(prep.diffDetail, VCBridge::AuthoringPreparationResult::DiffDetail::Full);
        QCOMPARE(prep.diffComplete, true);
        QCOMPARE(item.outcome, VCBridge::AuthoringPreparationItem::Outcome::Prepared);
        QCOMPARE(item.changedCount, 1);
        QVERIFY2(!item.changes.isEmpty(), "Full diff for changed geometry must include field-level changes");
        bool hasLogicalGeometry = false;
        for (const VCBridge::AuthoringFieldChange &field : item.changes)
        {
            if (field.field == QStringLiteral("logicalGeometry"))
            {
                hasLogicalGeometry = true;
                QCOMPARE(field.before.toRectF(), beforeRect);
                QCOMPARE(field.after.toRectF(), change.logicalRect);
                QCOMPARE(field.changed, true);
            }
        }
        QVERIFY2(hasLogicalGeometry, "Missing logicalGeometry field diff in full mode");
    }
    else
    {
        QCOMPARE(prep.diffDetail, VCBridge::AuthoringPreparationResult::DiffDetail::Full);
        QCOMPARE(prep.diffComplete, true);
        QCOMPARE(item.outcome, VCBridge::AuthoringPreparationItem::Outcome::Unchanged);
        QCOMPARE(item.changedCount, 0);
    }

    const VCBridge::AuthoringApplyResult apply = m_bridge->applyAuthoring(prep.preparationToken);
    QCOMPARE(apply.code, QString());
    QCOMPARE(apply.applied, true);
    QCOMPARE(apply.tokenConsumed, true);
    QCOMPARE(apply.items.size(), 1);
    const VCBridge::AuthoringApplyItem &applyItem = apply.items.first();
    QVERIFY(applyItem.selectedPageWidgetID.has_value());
    QVERIFY(applyItem.selectedParentWidgetID.has_value());
    QVERIFY(applyItem.selectedChildSubpage.has_value());
    QVERIFY(applyItem.configurationDigest.has_value());
    if (scenario == QStringLiteral("full-changed"))
        QVERIFY2(!applyItem.changes.isEmpty(), "Full apply diff for changed geometry must include field-level changes");
}

void VCAuthoringOwner_Test::authoringGeometry_selectedIdentity_exactValues()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCButton *idBump = new VCButton(doc, vc->page(0));
    vc->addWidgetToMap(idBump);
    vc->page(0)->addWidgetToPageMap(idBump);
    const quint32 idBumpId = idBump->id();
    auto idBumpCleanup = qScopeGuard([&]() { vc->deleteVCWidgets({idBumpId}); });

    const int page1Index = vc->pagesCount();
    vc->addPage(page1Index);
    VCPage *page = vc->page(page1Index);
    QVERIFY(page != nullptr);

    VCFrame *outer = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(outer);
    page->addWidgetToPageMap(outer);
    outer->setShowHeader(false);
    outer->setLogicalGeometry(QRectF(0, 0, 700, 500));

    VCFrame *nested = new VCFrame(doc, vc, outer);
    vc->addWidgetToMap(nested);
    nested->setPage(1);
    outer->addWidgetToPageMap(nested);
    nested->setShowHeader(false);
    nested->setLogicalGeometry(QRectF(50, 40, 300, 220));

    VCButton *button = new VCButton(doc, nested);
    vc->addWidgetToMap(button);
    button->setPage(1);
    nested->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(15, 20, 80, 50));

    const quint32 outerId = outer->id();
    const quint32 nestedId = nested->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({buttonId, nestedId, outerId}); });

    QVERIFY(page->id() != nestedId);
    QVERIFY(page->id() != quint32(page1Index));
    QVERIFY(page->id() != 1u);
    QVERIFY(nestedId != quint32(page1Index));
    QVERIFY(nestedId != 1u);

    VCBridge::AuthoringLayoutRequest request;
    request.operation = VCBridge::AuthoringLayoutRequest::Operation::Geometry;
    request.diffDetail = QStringLiteral("full");
    VCBridge::AuthoringGeometryChange change;
    change.widgetID = buttonId;
    change.logicalRect = QRectF(120, 90, 80, 50);
    request.geometryChanges.append(change);

    const VCBridge::AuthoringPreparationResult prep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(prep.code, QString());
    QCOMPARE(prep.items.size(), 1);
    QVERIFY(prep.items.first().selectedPageWidgetID.has_value());
    QVERIFY(prep.items.first().selectedParentWidgetID.has_value());
    QVERIFY(prep.items.first().selectedChildSubpage.has_value());
    QCOMPARE(prep.items.first().selectedPageWidgetID.value(), quint32(page->id()));
    QCOMPARE(prep.items.first().selectedParentWidgetID.value(), qint32(nestedId));
    QCOMPARE(prep.items.first().selectedChildSubpage.value(), 1);

    const VCBridge::AuthoringApplyResult apply = m_bridge->applyAuthoring(prep.preparationToken);
    QCOMPARE(apply.code, QString());
    QCOMPARE(apply.items.size(), 1);
    QVERIFY(apply.items.first().selectedPageWidgetID.has_value());
    QVERIFY(apply.items.first().selectedParentWidgetID.has_value());
    QVERIFY(apply.items.first().selectedChildSubpage.has_value());
    QCOMPARE(apply.items.first().selectedPageWidgetID.value(), quint32(page->id()));
    QCOMPARE(apply.items.first().selectedParentWidgetID.value(), qint32(nestedId));
    QCOMPARE(apply.items.first().selectedChildSubpage.value(), 1);
}

void VCAuthoringOwner_Test::authoringGeometry_fullDetailSnapToOriginal_noop()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 300));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({buttonId, frameId}); });

    const bool oldSnap = vc->snapping();
    const qreal oldSnapSize = vc->snappingSize();
    auto restoreSnap = qScopeGuard([&]() {
        vc->setSnapping(oldSnap);
        vc->setSnappingSize(oldSnapSize);
    });
    vc->setSnapping(true);
    vc->setSnappingSize(10.0);
    QVERIFY(vc->snapping());
    QVERIFY(vc->snappingSize() > 0.0);

    const QRectF original = button->logicalGeometry();
    const QVariantMap guide = vc->snapGuide(frameId);
    QVERIFY(guide.value(QStringLiteral("enabled")).toBool());
    QVERIFY(guide.contains(QStringLiteral("step")));
    QVERIFY(guide.value(QStringLiteral("step")).canConvert<QSizeF>());

    QRectF requested;
    bool foundSnapBack = false;
    for (int delta = 1; delta <= 25 && !foundSnapBack; ++delta)
    {
        const QRectF candidate = QRectF(original.x() + delta, original.y(), original.width(), original.height());
        if (candidate == original)
            continue;
        const QVariantMap preview = vc->previewGeometry(buttonId, candidate, true);
        if (!preview.value(QStringLiteral("accepted")).toBool())
            continue;
        QVERIFY(preview.contains(QStringLiteral("geometries")));
        const QVariantList geometries = preview.value(QStringLiteral("geometries")).toList();
        QVERIFY(!geometries.isEmpty());
        const QVariantMap firstGeometry = geometries.first().toMap();
        QVERIFY(firstGeometry.contains(QStringLiteral("rect")));
        const QRectF snapped = firstGeometry.value(QStringLiteral("rect")).toRectF();
        if (snapped == original)
        {
            requested = candidate;
            foundSnapBack = true;
        }
    }
    QVERIFY2(foundSnapBack, "Could not find a request rect that snaps back to original via native preview");

    VCBridge::AuthoringLayoutRequest request;
    request.operation = VCBridge::AuthoringLayoutRequest::Operation::Geometry;
    request.snapToGrid = true;
    request.diffDetail = QStringLiteral("full");
    VCBridge::AuthoringGeometryChange change;
    change.widgetID = buttonId;
    change.logicalRect = requested;
    request.geometryChanges.append(change);

    const VCBridge::AuthoringPreparationResult prep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(prep.code, QString());
    QCOMPARE(prep.items.size(), 1);
    QCOMPARE(prep.diffDetail, VCBridge::AuthoringPreparationResult::DiffDetail::Full);
    QCOMPARE(prep.items.first().outcome, VCBridge::AuthoringPreparationItem::Outcome::Unchanged);
    QCOMPARE(prep.items.first().changedCount, 0);
    QVERIFY(prep.items.first().changes.isEmpty());

    const VCBridge::AuthoringApplyResult apply = m_bridge->applyAuthoring(prep.preparationToken);
    QCOMPARE(apply.code, QString());
    QCOMPARE(apply.items.size(), 1);
    QCOMPARE(apply.items.first().outcome, VCBridge::AuthoringApplyItem::Outcome::Unchanged);
    QCOMPARE(apply.items.first().changedCount, 0);
    QVERIFY(apply.items.first().changes.isEmpty());
}

void VCAuthoringOwner_Test::authoringGeometry_applyOrder_matchesRequest()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 400));

    VCButton *first = new VCButton(doc, frame);
    vc->addWidgetToMap(first);
    frame->addWidgetToPageMap(first);
    first->setLogicalGeometry(QRectF(20, 40, 80, 60));

    VCButton *second = new VCButton(doc, frame);
    vc->addWidgetToMap(second);
    frame->addWidgetToPageMap(second);
    second->setLogicalGeometry(QRectF(120, 40, 80, 60));

    const quint32 frameId = frame->id();
    const quint32 firstId = first->id();
    const quint32 secondId = second->id();
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({firstId, secondId, frameId}); });

    quint32 highId = firstId;
    quint32 lowId = secondId;
    QRectF highTarget(260, 120, 80, 60);
    QRectF lowTarget(360, 120, 80, 60);
    if (firstId < secondId)
    {
        highId = secondId;
        lowId = firstId;
        highTarget = QRectF(360, 120, 80, 60);
        lowTarget = QRectF(260, 120, 80, 60);
    }

    VCBridge::AuthoringLayoutRequest request;
    request.operation = VCBridge::AuthoringLayoutRequest::Operation::Geometry;
    request.diffDetail = QStringLiteral("full");
    VCBridge::AuthoringGeometryChange high;
    high.widgetID = highId;
    high.logicalRect = highTarget;
    request.geometryChanges.append(high);
    VCBridge::AuthoringGeometryChange low;
    low.widgetID = lowId;
    low.logicalRect = lowTarget;
    request.geometryChanges.append(low);

    const VCBridge::AuthoringPreparationResult prep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(prep.code, QString());
    QCOMPARE(prep.items.size(), 2);
    QCOMPARE(prep.items.at(0).index, 0);
    QCOMPARE(prep.items.at(0).targetWidgetID, highId);
    QCOMPARE(prep.items.at(1).index, 1);
    QCOMPARE(prep.items.at(1).targetWidgetID, lowId);

    const VCBridge::AuthoringApplyResult apply = m_bridge->applyAuthoring(prep.preparationToken);
    QCOMPARE(apply.code, QString());
    QCOMPARE(apply.items.size(), 2);
    QCOMPARE(apply.items.at(0).index, 0);
    QCOMPARE(apply.items.at(0).targetWidgetID, highId);
    QCOMPARE(apply.items.at(1).index, 1);
    QCOMPARE(apply.items.at(1).targetWidgetID, lowId);
}

void VCAuthoringOwner_Test::authoringGeometry_internalRecordShape_andScopedStale()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 300));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setCaption(QStringLiteral("before-caption"));
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({buttonId, frameId}); });

    VCBridge::AuthoringLayoutRequest request;
    request.operation = VCBridge::AuthoringLayoutRequest::Operation::Geometry;
    request.diffDetail = QStringLiteral("full");
    VCBridge::AuthoringGeometryChange change;
    change.widgetID = buttonId;
    change.logicalRect = QRectF(120, 80, 80, 60);
    request.geometryChanges.append(change);

    const VCBridge::AuthoringPreparationResult prep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(prep.code, QString());
    QVERIFY(!prep.preparationToken.isEmpty());
    bool tokenConsumed = false;
    auto consumePreparedToken = qScopeGuard([&]() {
        if (!tokenConsumed && !prep.preparationToken.isEmpty())
            m_bridge->applyAuthoring(prep.preparationToken);
    });

    auto it = m_bridge->m_authoringRecords.find(prep.preparationToken);
    QVERIFY(it != m_bridge->m_authoringRecords.end());
    const VCBridgeV5::PreparationRecord &record = it.value();
    QVERIFY(std::holds_alternative<VCBridgeV5::PreparedGeometryPayload>(record.operationPayload));
    const VCBridgeV5::PreparedGeometryPayload payload =
        std::get<VCBridgeV5::PreparedGeometryPayload>(record.operationPayload);
    QVERIFY(payload.candidateRectangles.contains(buttonId));
    QCOMPARE(record.predicates.requireFeedbackPatchIdle, true);
    QCOMPARE(record.predicates.requireNoRelativeSources, true);
    QVERIFY(!record.geometryGuards.isEmpty());
    bool hasTargetGuard = false;
    for (const VCBridgeV5::GeometryGuard &guard : record.geometryGuards)
    {
        if (guard.key.kind == VCBridgeV5::GuardKey::Kind::Widget &&
            guard.key.widgetID.has_value() && guard.key.widgetID.value() == buttonId)
        {
            hasTargetGuard = true;
            QVERIFY(guard.identity.data() == button);
            QVERIFY(!guard.scopedConfigurationDigest.isEmpty());
            break;
        }
    }
    QVERIFY2(hasTargetGuard, "Expected target guard for prepared widget");

    Function *sameIdFunction = doc->function(buttonId);
    bool createdSameIdFunction = false;
    if (!sameIdFunction)
    {
        Scene *created = new Scene(doc);
        created->setName(QStringLiteral("same-id-function-guard"));
        QVERIFY(doc->addFunction(created, buttonId));
        sameIdFunction = created;
        createdSameIdFunction = true;
    }
    auto cleanupFunction = qScopeGuard([&]() {
        if (createdSameIdFunction && sameIdFunction)
            doc->deleteFunction(sameIdFunction->id());
    });
    QCOMPARE(sameIdFunction->id(), buttonId);
    QCOMPARE(doc->function(buttonId), sameIdFunction);

    VCBridgeV5::GuardKey widgetKey;
    widgetKey.kind = VCBridgeV5::GuardKey::Kind::Widget;
    widgetKey.widgetID = buttonId;
    QObject *resolvedWidget = m_bridge->resolveGuardObject(widgetKey);
    QVERIFY(resolvedWidget == button);

    VCBridgeV5::GuardKey functionKey;
    functionKey.kind = VCBridgeV5::GuardKey::Kind::Function;
    functionKey.functionID = buttonId;
    QObject *resolvedFunction = m_bridge->resolveGuardObject(functionKey);
    QVERIFY(resolvedFunction == sameIdFunction);
    QVERIFY(resolvedFunction != resolvedWidget);

    QSharedPointer<QLCInputSource> sourceA(new QLCInputSource(7, 9));
    sourceA->setID(1);
    sourceA->setPage(0);
    QSharedPointer<QLCInputSource> sourceB(new QLCInputSource(7, 9));
    sourceB->setID(2);
    sourceB->setPage(0);
    button->addInputSource(sourceA);
    button->addInputSource(sourceB);

    VCBridgeV5::GuardKey ambiguousSourceKey;
    ambiguousSourceKey.kind = VCBridgeV5::GuardKey::Kind::InputSource;
    ambiguousSourceKey.widgetID = buttonId;
    VCBridge::AuthoringSourceKey ambiguousSource;
    ambiguousSource.inputUniverseID = 7;
    ambiguousSource.inputChannel = 9;
    ambiguousSourceKey.sourceKey = ambiguousSource;
    QVERIFY(m_bridge->resolveGuardObject(ambiguousSourceKey) == nullptr);

    VCBridgeV5::GuardKey exactSourceKey = ambiguousSourceKey;
    exactSourceKey.sourceKey->controlSlotID = 1;
    QObject *resolvedSource = m_bridge->resolveGuardObject(exactSourceKey);
    QVERIFY(resolvedSource == sourceA.data());
    QVERIFY(!m_bridge->scopedConfigurationDigest(exactSourceKey).isEmpty());

    // Scoped-configuration stale case (disable-state changes while identity/geometry and basis stay).
    button->setDisabled(true);
    const VCBridge::AuthoringApplyResult staleApply =
        m_bridge->applyAuthoring(prep.preparationToken);
    tokenConsumed = true;
    QCOMPARE(staleApply.code, QStringLiteral("staleSelection"));
    QCOMPARE(staleApply.applied, false);
    QCOMPARE(staleApply.tokenConsumed, true);
}

void VCAuthoringOwner_Test::authoringGeometry_profileDigest_doesNotLoadWorkspaceProfile()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    Universe *universe0 = ioMap->universe(0);
    QVERIFY(universe0 != nullptr);
    QVERIFY2(m_probePlugin, "Probe plugin not loaded");
    QVERIFY2(universe0->setInputPatch(m_probePlugin.data(), 0, static_cast<QLCInputProfile *>(nullptr)),
             "Failed to create input patch via Universe::setInputPatch");
    InputPatch *patch = ioMap->inputPatch(0);
    QVERIFY(patch != nullptr);
    QVERIFY(patch->isPatched());
    std::unique_ptr<QLCInputProfile> unloadedProfile;
    QVector<QString> ownedTokens;
    auto cleanupTokens = qScopeGuard([&]() {
        for (const QString &token : ownedTokens)
        {
            if (m_bridge->m_authoringRecords.contains(token))
                m_bridge->applyAuthoring(token);
        }
    });

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 300));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));
    QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
    source->setID(17);
    source->setWorkingMode(QLCInputSource::Absolute);
    button->addInputSource(source);

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() {
        patch->set(static_cast<QLCInputProfile *>(nullptr));
        unloadedProfile.reset();
        vc->deleteVCWidgets({buttonId, frameId});
        universe0->setInputPatch(nullptr, QLCIOPlugin::invalidLine(), static_cast<QLCInputProfile *>(nullptr));
    });

    const QString marker = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString manufacturer = QStringLiteral("MCP_OWNER_%1").arg(marker.left(8));
    const QString model = QStringLiteral("Profile_%1").arg(marker.mid(8, 8));
    const QString profilePath = m_workspaceDir.filePath(QStringLiteral("authoring-owner-%1%2")
                                                         .arg(marker, KExtInputProfile));

    QFile profileFile(profilePath);
    QVERIFY2(profileFile.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(profilePath)));
    const QByteArray profileXml = QStringLiteral(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE InputProfile>\n"
        "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
        " <Creator>\n"
        "  <Name>MCP Owner Test</Name>\n"
        "  <Version>1.0</Version>\n"
        "  <Author>VCAuthoringOwner_Test</Author>\n"
        " </Creator>\n"
        " <Manufacturer>%1</Manufacturer>\n"
        " <Model>%2</Model>\n"
        " <Type>MIDI</Type>\n"
        " <Channel Number=\"7\">\n"
        "  <Name>Test Channel</Name>\n"
        "  <Type>Slider</Type>\n"
        " </Channel>\n"
        "</InputProfile>\n")
        .arg(manufacturer, model)
        .toUtf8();
    QCOMPARE(profileFile.write(profileXml), profileXml.size());
    profileFile.close();

    unloadedProfile.reset(QLCInputProfile::loader(profilePath));
    QVERIFY2(unloadedProfile != nullptr, "Profile fixture did not load from workspace file");
    const QString profileName = unloadedProfile->name();
    QVERIFY(QFile::exists(profilePath));
    QVERIFY2(!ioMap->profileNames().contains(profileName),
             qPrintable(QStringLiteral("Profile was unexpectedly preloaded: %1").arg(profileName)));
    QVERIFY(patch->set(unloadedProfile.get()));
    QCOMPARE(patch->profileName(), profileName);
    QVERIFY2(!ioMap->profileNames().contains(profileName),
             qPrintable(QStringLiteral("Profile unexpectedly entered IOMap cache before callback path: %1")
                            .arg(profileName)));

    VCBridge::AuthoringLayoutRequest request;
    request.operation = VCBridge::AuthoringLayoutRequest::Operation::Geometry;
    VCBridge::AuthoringGeometryChange change;
    change.widgetID = buttonId;
    change.logicalRect = QRectF(80, 60, 80, 60);
    request.geometryChanges.append(change);

    const VCBridge::AuthoringPreparationResult prep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(prep.code, QString());
    QVERIFY(!prep.preparationToken.isEmpty());
    ownedTokens.append(prep.preparationToken);
    QVERIFY2(!ioMap->profileNames().contains(profileName),
             qPrintable(QStringLiteral("Guard observation loaded workspace profile unexpectedly: %1")
                            .arg(profileName)));

    const VCBridge::AuthoringApplyResult apply = m_bridge->applyAuthoring(prep.preparationToken);
    QCOMPARE(apply.code, QString());
    QCOMPARE(apply.applied, true);
    ownedTokens.removeAll(prep.preparationToken);
    QVERIFY2(!ioMap->profileNames().contains(profileName),
             qPrintable(QStringLiteral("Apply guard check loaded workspace profile unexpectedly: %1")
                            .arg(profileName)));

    // Same-context positive oracle: prove this exact IOMap can resolve the workspace profile.
    QLCInputProfile *loadedByLookup = ioMap->profile(profileName);
    QVERIFY2(loadedByLookup != nullptr,
             qPrintable(QStringLiteral("Workspace profile was not discoverable via public loader path: %1")
                            .arg(profileName)));
    QVERIFY(ioMap->profileNames().contains(profileName));
}

void VCAuthoringOwner_Test::authoringGeometry_frameVolatileState_doesNotStaleSelection()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 400));
    frame->setTotalPagesNumber(2);
    frame->setMultiPageMode(true);
    frame->setCurrentPage(0);
    frame->setCollapsed(false);
    frame->setPagesLoop(false);
    frame->setShortcutName(0, QStringLiteral("base-page-0"));
    QVector<QString> ownedTokens;
    auto cleanupTokens = qScopeGuard([&]() {
        for (const QString &token : ownedTokens)
        {
            if (m_bridge->m_authoringRecords.contains(token))
                m_bridge->applyAuthoring(token);
        }
    });

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({buttonId, frameId}); });

    VCBridge::AuthoringLayoutRequest request;
    request.operation = VCBridge::AuthoringLayoutRequest::Operation::Geometry;
    request.diffDetail = QStringLiteral("full");
    VCBridge::AuthoringGeometryChange moveA;
    moveA.widgetID = buttonId;
    moveA.logicalRect = QRectF(120, 80, 80, 60);
    request.geometryChanges = {moveA};

    const VCBridge::AuthoringPreparationResult prepVolatile =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(prepVolatile.code, QString());
    QVERIFY(!prepVolatile.preparationToken.isEmpty());
    ownedTokens.append(prepVolatile.preparationToken);

    const int beforePage = frame->currentPage();
    frame->setCurrentPage(1);
    QVERIFY(frame->currentPage() != beforePage);
    frame->setCollapsed(true);
    const VCBridge::AuthoringApplyResult applyVolatile =
        m_bridge->applyAuthoring(prepVolatile.preparationToken);
    QCOMPARE(applyVolatile.code, QString());
    QCOMPARE(applyVolatile.applied, true);
    ownedTokens.removeAll(prepVolatile.preparationToken);
    QCOMPARE(button->logicalGeometry(), moveA.logicalRect);

    VCBridge::AuthoringGeometryChange moveB;
    moveB.widgetID = buttonId;
    moveB.logicalRect = QRectF(220, 100, 80, 60);
    request.geometryChanges = {moveB};
    const VCBridge::AuthoringPreparationResult prepPagesLoop =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(prepPagesLoop.code, QString());
    QVERIFY(!prepPagesLoop.preparationToken.isEmpty());
    ownedTokens.append(prepPagesLoop.preparationToken);

    frame->setPagesLoop(true);
    const VCBridge::AuthoringApplyResult stalePagesLoop =
        m_bridge->applyAuthoring(prepPagesLoop.preparationToken);
    QCOMPARE(stalePagesLoop.code, QStringLiteral("staleBasis"));
    QCOMPARE(stalePagesLoop.applied, false);
    QCOMPARE(stalePagesLoop.tokenConsumed, true);
    ownedTokens.removeAll(prepPagesLoop.preparationToken);

    VCBridge::AuthoringGeometryChange moveC;
    moveC.widgetID = buttonId;
    moveC.logicalRect = QRectF(260, 120, 80, 60);
    request.geometryChanges = {moveC};
    const VCBridge::AuthoringPreparationResult prepPageLabels =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(prepPageLabels.code, QString());
    QVERIFY(!prepPageLabels.preparationToken.isEmpty());
    ownedTokens.append(prepPageLabels.preparationToken);

    frame->setShortcutName(0, QStringLiteral("mutated-page-label"));
    const VCBridge::AuthoringApplyResult stalePageLabel =
        m_bridge->applyAuthoring(prepPageLabels.preparationToken);
    QCOMPARE(stalePageLabel.code, QStringLiteral("staleBasis"));
    QCOMPARE(stalePageLabel.applied, false);
    QCOMPARE(stalePageLabel.tokenConsumed, true);
    ownedTokens.removeAll(prepPageLabels.preparationToken);
}

void VCAuthoringOwner_Test::authoringGeometry_mixedGuardKinds_enforceGuardCap()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 700, 450));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));
    QVector<QString> ownedTokens;
    auto cleanupTokens = qScopeGuard([&]() {
        for (const QString &token : ownedTokens)
        {
            if (m_bridge->m_authoringRecords.contains(token))
                m_bridge->applyAuthoring(token);
        }
    });

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({buttonId, frameId}); });

    VCBridge::AuthoringLayoutRequest request;
    request.operation = VCBridge::AuthoringLayoutRequest::Operation::Geometry;
    request.diffDetail = QStringLiteral("summary");
    VCBridge::AuthoringGeometryChange change;
    change.widgetID = buttonId;
    change.logicalRect = QRectF(120, 90, 80, 60);
    request.geometryChanges.append(change);

    const VCBridge::AuthoringPreparationResult baselinePrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(baselinePrep.code, QString());
    QVERIFY(!baselinePrep.preparationToken.isEmpty());
    ownedTokens.append(baselinePrep.preparationToken);
    auto baselineRecord = m_bridge->m_authoringRecords.find(baselinePrep.preparationToken);
    QVERIFY(baselineRecord != m_bridge->m_authoringRecords.end());
    const int baselineGuards = baselineRecord->geometryGuards.size();
    QVERIFY(baselineGuards > 0);
    const VCBridge::AuthoringApplyResult baselineApply =
        m_bridge->applyAuthoring(baselinePrep.preparationToken);
    QCOMPARE(baselineApply.code, QString());
    QCOMPARE(baselineApply.applied, true);
    ownedTokens.removeAll(baselinePrep.preparationToken);

    const int targetWithinCap = 4096;
    const int targetOverflow = 4097;
    const int withinCapSources = targetWithinCap - baselineGuards;
    QVERIFY(withinCapSources > 0);
    QCOMPARE(baselineGuards + withinCapSources, targetWithinCap);

    for (int i = 0; i < withinCapSources; ++i)
    {
        QSharedPointer<QLCInputSource> source(new QLCInputSource(0, quint32(i)));
        source->setID(0); // VCButton slot 0
        source->setPage(0);
        source->setWorkingMode(QLCInputSource::Absolute);
        button->addInputSource(source);
    }

    const VCBridge::AuthoringPreparationResult withinCapPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(withinCapPrep.code, QString());
    QVERIFY(!withinCapPrep.preparationToken.isEmpty());
    ownedTokens.append(withinCapPrep.preparationToken);
    auto withinCapRecord = m_bridge->m_authoringRecords.find(withinCapPrep.preparationToken);
    QVERIFY(withinCapRecord != m_bridge->m_authoringRecords.end());
    const int estimatedGuards = m_bridge->estimateGuardCount(
        std::get<VCBridgeV5::PreparedGeometryPayload>(withinCapRecord->operationPayload).candidateRectangles);
    QCOMPARE(withinCapRecord->geometryGuards.size(), targetWithinCap);
    QVERIFY(withinCapRecord->geometryGuards.size() > estimatedGuards);
    const VCBridge::AuthoringApplyResult withinCapApply =
        m_bridge->applyAuthoring(withinCapPrep.preparationToken);
    QCOMPARE(withinCapApply.code, QString());
    QCOMPARE(withinCapApply.applied, true);
    ownedTokens.removeAll(withinCapPrep.preparationToken);

    const int overflowIndex = withinCapSources;
    QSharedPointer<QLCInputSource> overflowSource(
        new QLCInputSource(0, quint32(overflowIndex)));
    overflowSource->setID(0); // VCButton slot 0
    overflowSource->setPage(0);
    overflowSource->setWorkingMode(QLCInputSource::Absolute);
    button->addInputSource(overflowSource);

    const VCBridge::AuthoringPreparationResult overflowPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(targetWithinCap + 1, targetOverflow);
    QCOMPARE(overflowPrep.code, QStringLiteral("preparationLimit"));
    QVERIFY(overflowPrep.preparationToken.isEmpty());
    QCOMPARE(overflowPrep.expiresInMs, 0);
}

void VCAuthoringOwner_Test::authoringGeometry_applyRefusals_functionAndSourceGuardStale()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 300));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("function-guard-stale"));
    QVERIFY(doc->addFunction(scene));
    button->setFunctionID(scene->id());

    QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 1));
    source->setID(31);
    source->setWorkingMode(QLCInputSource::Absolute);
    source->setSensitivity(2);
    button->addInputSource(source);
    QVector<QString> ownedTokens;
    auto cleanupTokens = qScopeGuard([&]() {
        for (const QString &token : ownedTokens)
        {
            if (m_bridge->m_authoringRecords.contains(token))
                m_bridge->applyAuthoring(token);
        }
    });

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({buttonId, frameId});
        doc->deleteFunction(scene->id());
    });

    VCBridge::AuthoringLayoutRequest request;
    request.operation = VCBridge::AuthoringLayoutRequest::Operation::Geometry;
    VCBridge::AuthoringGeometryChange change;
    change.widgetID = buttonId;
    change.logicalRect = QRectF(120, 80, 80, 60);
    request.geometryChanges = {change};
    const QRectF originalGeometry = button->logicalGeometry();

    const VCBridge::AuthoringPreparationResult functionPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(functionPrep.code, QString());
    QVERIFY(!functionPrep.preparationToken.isEmpty());
    ownedTokens.append(functionPrep.preparationToken);
    const QString basisBeforeFunctionMutation = m_bridge->authoringBasis();
    scene->setName(QStringLiteral("function-guard-mutated"));
    QCOMPARE(m_bridge->authoringBasis(), basisBeforeFunctionMutation);
    const VCBridge::AuthoringApplyResult functionApply =
        m_bridge->applyAuthoring(functionPrep.preparationToken);
    QCOMPARE(functionApply.code, QStringLiteral("staleSelection"));
    QCOMPARE(functionApply.applied, false);
    QCOMPARE(functionApply.tokenConsumed, true);
    ownedTokens.removeAll(functionPrep.preparationToken);
    QCOMPARE(button->logicalGeometry(), originalGeometry);

    const VCBridge::AuthoringPreparationResult sourcePrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(request));
    QCOMPARE(sourcePrep.code, QString());
    QVERIFY(!sourcePrep.preparationToken.isEmpty());
    ownedTokens.append(sourcePrep.preparationToken);
    // Structural isolation for this row: keep only InputSource guards so staleSelection
    // cannot be attributed to Widget/Function guard paths.
    auto sourceRecord = m_bridge->m_authoringRecords.find(sourcePrep.preparationToken);
    QVERIFY(sourceRecord != m_bridge->m_authoringRecords.end());
    QVector<VCBridgeV5::GeometryGuard> sourceOnlyGuards;
    for (const VCBridgeV5::GeometryGuard &guard : sourceRecord->geometryGuards)
    {
        if (guard.key.kind == VCBridgeV5::GuardKey::Kind::InputSource)
            sourceOnlyGuards.append(guard);
    }
    QVERIFY(!sourceOnlyGuards.isEmpty());
    sourceRecord->geometryGuards = sourceOnlyGuards;
    source->setSensitivity(source->sensitivity() + 1);
    const VCBridge::AuthoringApplyResult sourceApply =
        m_bridge->applyAuthoring(sourcePrep.preparationToken);
    QCOMPARE(sourceApply.code, QStringLiteral("staleSelection"));
    QCOMPARE(sourceApply.applied, false);
    QCOMPARE(sourceApply.tokenConsumed, true);
    ownedTokens.removeAll(sourcePrep.preparationToken);
    QCOMPARE(button->logicalGeometry(), originalGeometry);
}

void VCAuthoringOwner_Test::authoringGeometry_prepareFailures_noTokenAndAllFindings()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    MasterTimer *timer = doc->masterTimer();
    QVERIFY(timer != nullptr);
    timer->stopAllFunctions();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 400));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanupWidgets = qScopeGuard([&]() { vc->deleteVCWidgets({buttonId, frameId}); });

    VCBridge::AuthoringLayoutRequest invalid;
    VCBridge::AuthoringGeometryChange badGeometry;
    badGeometry.widgetID = buttonId;
    badGeometry.logicalRect = QRectF(std::numeric_limits<qreal>::quiet_NaN(), 10, 40, 40);
    invalid.geometryChanges.append(badGeometry);
    VCBridge::AuthoringGeometryChange unknown;
    unknown.widgetID = 0x7ffffff0;
    unknown.logicalRect = QRectF(10, 20, 40, 40);
    invalid.geometryChanges.append(unknown);

    const VCBridge::AuthoringPreparationResult invalidResult =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(invalid));
    QCOMPARE(invalidResult.code, QStringLiteral("invalidArgument"));
    QVERIFY(invalidResult.preparationToken.isEmpty());
    QCOMPARE(invalidResult.expiresInMs, 0);
    QVERIFY(invalidResult.items.size() == 2);
    QCOMPARE(invalidResult.items.at(0).code, QStringLiteral("invalidArgument"));
    QCOMPARE(invalidResult.items.at(1).code, QStringLiteral("invalidArgument"));

    VCBridge::AuthoringLayoutRequest duplicate;
    VCBridge::AuthoringGeometryChange first;
    first.widgetID = buttonId;
    first.logicalRect = QRectF(30, 50, 80, 60);
    duplicate.geometryChanges.append(first);
    VCBridge::AuthoringGeometryChange second = first;
    second.logicalRect = QRectF(40, 60, 80, 60);
    duplicate.geometryChanges.append(second);

    const VCBridge::AuthoringPreparationResult duplicateResult =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(duplicate));
    QCOMPARE(duplicateResult.code, QStringLiteral("invalidArgument"));
    QVERIFY(duplicateResult.preparationToken.isEmpty());
    QCOMPARE(duplicateResult.expiresInMs, 0);
    QVERIFY(duplicateResult.items.size() == 2);
    QCOMPARE(duplicateResult.items.at(1).code, QStringLiteral("invalidArgument"));
}

void VCAuthoringOwner_Test::authoringGeometry_preconditionRows_sourcesProtectionAndPlayback()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    MasterTimer *timer = doc->masterTimer();
    QVERIFY(timer != nullptr);
    timer->stopAllFunctions();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    InputOutputMap *ioMap = doc->inputOutputMap();
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 400));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanupWidgets = qScopeGuard([&]() {
        vc->deleteVCWidgets({buttonId, frameId});
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), false, 0);
        if (page->PIN() != 0)
        {
            vc->validatePagePIN(0, QStringLiteral("1234"), true);
            QVERIFY(vc->setPagePIN(0, QStringLiteral("1234"), QString()));
        }
    });

    VCBridge::AuthoringLayoutRequest req;
    VCBridge::AuthoringGeometryChange change;
    change.widgetID = buttonId;
    change.logicalRect = QRectF(60, 70, 80, 60);
    req.geometryChanges.append(change);

    // Source with no feedback patch succeeds.
    QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 1));
    source->setID(0);
    button->addInputSource(source);
    const VCBridge::AuthoringPreparationResult noFeedbackPatch =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(noFeedbackPatch.code, QString());
    QVERIFY(!noFeedbackPatch.preparationToken.isEmpty());
    const VCBridge::AuthoringApplyResult noFeedbackApply =
        m_bridge->applyAuthoring(noFeedbackPatch.preparationToken);
    QCOMPARE(noFeedbackApply.code, QString());
    QCOMPARE(noFeedbackApply.applied, true);

    // Same source with feedback patch refuses.
    QVERIFY2(ioMap->setOutputPatch(0, m_probeName, "", "", 0, true),
             "Failed to patch feedback line for F-check row");
    const VCBridge::AuthoringPreparationResult feedbackPatched =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(feedbackPatched.code, QStringLiteral("authoringPrecondition"));
    QVERIFY(feedbackPatched.error.contains(QStringLiteral("Feedback patch"), Qt::CaseInsensitive));
    QVERIFY(feedbackPatched.preparationToken.isEmpty());
    ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);

    // Relative source refuses with unsupportedPreparation.
    source->setWorkingMode(QLCInputSource::Relative);
    const VCBridge::AuthoringPreparationResult relativeRefused =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(relativeRefused.code, QStringLiteral("unsupportedPreparation"));
    QVERIFY(relativeRefused.error.contains(QStringLiteral("Relative"), Qt::CaseInsensitive));
    source->setWorkingMode(QLCInputSource::Absolute);

    // Protected page rows: locked and cached-unlocked both refuse.
    QVERIFY(vc->setPagePIN(0, QString(), QStringLiteral("1234")));
    const VCBridge::AuthoringPreparationResult protectedLocked =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(protectedLocked.code, QStringLiteral("authoringPrecondition"));
    QVERIFY(protectedLocked.preparationToken.isEmpty());
    QVERIFY(vc->validatePagePIN(0, QStringLiteral("1234"), true));
    const VCBridge::AuthoringPreparationResult protectedUnlockedCached =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(protectedUnlockedCached.code, QStringLiteral("authoringPrecondition"));
    QVERIFY(vc->setPagePIN(0, QStringLiteral("1234"), QString()));

    // Genuine playback row: prepare refuses while a function is running.
    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("Authoring queued guard scene"));
    QVERIFY(doc->addFunction(scene));
    MasterTimer *queuedTimer = doc->masterTimer();
    QVERIFY(queuedTimer != nullptr);
    auto cleanupFunction = qScopeGuard([&]() {
        scene->stop(FunctionParent::master());
        queuedTimer->stop();
        doc->deleteFunction(scene->id());
    });
    queuedTimer->start();
    scene->start(queuedTimer, FunctionParent::master());
    VCBridge::AuthoringPreparationResult queuedRefused;
    bool checkedDuringPlayback = false;
    for (int i = 0; i < 40 && !checkedDuringPlayback; ++i)
    {
        const bool queued = queuedTimer->isStartQueued(scene);
        const bool running = queuedTimer->runningFunctionIds().contains(scene->id());
        if (queued || running)
        {
            queuedRefused = m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
            checkedDuringPlayback = true;
            break;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QTest::qWait(5);
    }
    QVERIFY2(checkedDuringPlayback, "Scene did not reach queued or running state before precondition check");
    QCOMPARE(queuedRefused.code, QStringLiteral("authoringPrecondition"));
}

void VCAuthoringOwner_Test::authoringGeometry_preconditionRows_recorderAndPanel_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("expectedObs");
    QTest::addColumn<QString>("expectedErrorPart");

    QTest::newRow("recording-active")
        << QStringLiteral("recording-active")
        << QStringLiteral("recordingIdle")
        << QStringLiteral("Recorder is active");
    QTest::newRow("bound-show-authoring")
        << QStringLiteral("bound-show-authoring")
        << QStringLiteral("recordingIdle")
        << QStringLiteral("Recorder is active");
    QTest::newRow("record-panel-capture")
        << QStringLiteral("record-panel-capture")
        << QStringLiteral("recordPanelCapture")
        << QStringLiteral("Record panel capture is active");
}

void VCAuthoringOwner_Test::authoringGeometry_preconditionRows_recorderAndPanel()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);
    QFETCH(QString, expectedObs);
    QFETCH(QString, expectedErrorPart);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    if (page->PIN() != 0)
    {
        vc->validatePagePIN(0, QStringLiteral("1234"), true);
        QVERIFY(vc->setPagePIN(0, QStringLiteral("1234"), QString()));
    }
    if (ShowCommandRecorder *recorder = ShowCommandRecorder::instance())
    {
        if (recorder->isRecording())
            QVERIFY(recorder->setRecording(false));
        QVERIFY(recorder->setResolvedShow(Function::invalidId()));
    }

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 400));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    VCRecordPanel *panel = nullptr;
    Show *show = nullptr;
    ShowCommandRecorder *recorder = ShowCommandRecorder::instance();
    QVERIFY(recorder != nullptr);
    const QRectF panelBaselineRect(120, 40, 120, 80);
    const bool recorderWasRecording = recorder->isRecording();
    const quint32 recorderBoundShow = recorder->boundShowId();

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanupWidgets = qScopeGuard([&]() {
        if (panel != nullptr && panel->isRecordingChaser())
            panel->stopChaser();
        if (panel != nullptr)
        {
            frame->removeWidgetFromPageMap(panel);
            vc->removeWidgetFromMap(panel);
            panel->deleteLater();
        }
        vc->deleteVCWidgets({buttonId, frameId});
        if (show != nullptr)
            doc->deleteFunction(show->id());
        if (recorder->boundShowId() != recorderBoundShow)
            recorder->setResolvedShow(recorderBoundShow);
        if (recorder->isRecording() != recorderWasRecording)
            recorder->setRecording(recorderWasRecording);
    });

    if (scenario == QStringLiteral("record-panel-capture"))
    {
        panel = new VCRecordPanel(doc, frame);
        vc->addWidgetToMap(panel);
        frame->addWidgetToPageMap(panel);
        panel->setLogicalGeometry(panelBaselineRect);
        QVERIFY(!panel->isRecordingChaser());
    }

    VCBridge::AuthoringLayoutRequest req;
    VCBridge::AuthoringGeometryChange change;
    if (scenario == QStringLiteral("record-panel-capture"))
    {
        change.widgetID = panel->id();
        change.logicalRect = QRectF(260, 40, 120, 80);
    }
    else
    {
        change.widgetID = buttonId;
        change.logicalRect = QRectF(60, 70, 80, 60);
    }
    req.geometryChanges.append(change);

    const VCBridge::AuthoringPreparationResult baseline =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(baseline.code, QString());
    QVERIFY(!baseline.preparationToken.isEmpty());
    const VCBridge::AuthoringApplyResult baselineApply =
        m_bridge->applyAuthoring(baseline.preparationToken);
    QCOMPARE(baselineApply.code, QString());
    QCOMPARE(baselineApply.applied, true);
    if (scenario == QStringLiteral("record-panel-capture"))
        panel->setLogicalGeometry(panelBaselineRect);
    else
        button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    if (scenario == QStringLiteral("record-panel-capture"))
    {
        panel->startChaser();
        QVERIFY(panel->isRecordingChaser());
    }
    else
    {
        show = new Show(doc);
        show->setName(QStringLiteral("Authoring recorder guard show"));
        QVERIFY(doc->addFunction(show));
        QVERIFY(recorder->setResolvedShow(show->id()));
        QVERIFY(recorder->setRecording(true));
        QVERIFY(recorder->isRecording());
        if (scenario == QStringLiteral("bound-show-authoring"))
            QVERIFY(recorder->isAuthoring());
    }

    const VCBridge::AuthoringPreparationResult refused =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(refused.code, QStringLiteral("authoringPrecondition"));
    QVERIFY(refused.preparationToken.isEmpty());
    QVERIFY(refused.error.contains(expectedErrorPart, Qt::CaseInsensitive));
    bool foundObs = false;
    for (const VCBridge::AuthoringPreconditionObservation &obs : refused.preconditions)
    {
        if (obs.name != expectedObs)
            continue;
        foundObs = true;
        QCOMPARE(obs.met, false);
        break;
    }
    QVERIFY2(foundObs, qPrintable(QStringLiteral("Missing precondition observation: %1").arg(expectedObs)));

    if (scenario == QStringLiteral("record-panel-capture"))
    {
        panel->stopChaser();
        QVERIFY(!panel->isRecordingChaser());
    }
    else
    {
        QVERIFY(recorder->setRecording(false));
        QVERIFY(recorder->setResolvedShow(Function::invalidId()));
    }

    const VCBridge::AuthoringPreparationResult afterClear =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(afterClear.code, QString());
    QVERIFY(!afterClear.preparationToken.isEmpty());
    const VCBridge::AuthoringApplyResult afterClearApply =
        m_bridge->applyAuthoring(afterClear.preparationToken);
    QCOMPARE(afterClearApply.code, QString());
    QCOMPARE(afterClearApply.applied, true);
}

void VCAuthoringOwner_Test::authoringCore_layoutFramePages_andRecordPanelConfig_persistAcrossLoad()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *layoutFrame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(layoutFrame);
    page->addWidgetToPageMap(layoutFrame);
    layoutFrame->setShowHeader(false);
    layoutFrame->setMultiPageMode(true);
    layoutFrame->setLogicalGeometry(QRectF(20, 20, 300, 220));

    VCFrame *panelFrame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(panelFrame);
    page->addWidgetToPageMap(panelFrame);
    panelFrame->setShowHeader(false);
    panelFrame->setLogicalGeometry(QRectF(360, 20, 300, 220));

    VCRecordPanel *panel = new VCRecordPanel(doc, panelFrame);
    vc->addWidgetToMap(panel);
    panelFrame->addWidgetToPageMap(panel);
    panel->setLogicalGeometry(QRectF(20, 20, 180, 120));

    const quint32 layoutFrameId = layoutFrame->id();
    const quint32 panelFrameId = panelFrame->id();
    const quint32 panelId = panel->id();
    auto cleanupWidgets = qScopeGuard([&]() {
        vc->deleteVCWidgets({panelId, panelFrameId, layoutFrameId});
    });

    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, doc, m_bridge.get());
    QVERIFY(tm.has("vc_prepare_layout"));
    QVERIFY(tm.has("vc_prepare_record_panel"));
    QVERIFY(tm.has("vc_apply_preparation"));

    const Json layoutPrep = parseInvoke(tm.invoke(
        "vc_prepare_layout",
        Json{
            {"operation", "framePages"},
            {"frameWidgetID", layoutFrameId},
            {"totalPages", 3},
            {"pageLabels", Json::array({"Intro", "Main", "Outro"})},
            {"pagesLoop", true}
        }));
    QVERIFY2(!layoutPrep.contains("error"), layoutPrep.dump().c_str());
    const std::string layoutToken = layoutPrep.value("preparationToken", std::string());
    QVERIFY2(!layoutToken.empty(), layoutPrep.dump().c_str());
    const Json layoutApply = parseInvoke(tm.invoke(
        "vc_apply_preparation",
        Json{{"preparationToken", layoutToken}}));
    QVERIFY2(!layoutApply.contains("error"), layoutApply.dump().c_str());
    QCOMPARE(layoutApply.value("applied", false), true);

    QCOMPARE(layoutFrame->totalPagesNumber(), 3);
    QCOMPARE(layoutFrame->pageLabels(),
             QStringList({QStringLiteral("Intro"), QStringLiteral("Main"), QStringLiteral("Outro")}));
    QCOMPARE(layoutFrame->pagesLoop(), true);
    QCOMPARE(layoutFrame->multiPageMode(), true);

    const Json panelPrep = parseInvoke(tm.invoke(
        "vc_prepare_record_panel",
        Json{
            {"items", Json::array({
                {
                    {"widgetID", panelId},
                    {"targetFolder", "C:/authoring/s2c3"},
                    {"scenePrefix", "SC2_"},
                    {"chaserPrefix", "CH2_"},
                    {"defaultFadeIn", 111},
                    {"defaultHold", 222},
                    {"defaultFadeOut", 333}
                }
            })}
        }));
    QVERIFY2(!panelPrep.contains("error"), panelPrep.dump().c_str());
    const std::string panelToken = panelPrep.value("preparationToken", std::string());
    QVERIFY2(!panelToken.empty(), panelPrep.dump().c_str());

    const Json panelApply = parseInvoke(tm.invoke(
        "vc_apply_preparation",
        Json{{"preparationToken", panelToken}}));
    QVERIFY2(!panelApply.contains("error"), panelApply.dump().c_str());
    QCOMPARE(panelApply.value("applied", false), true);

    QCOMPARE(panel->targetFolder(), QStringLiteral("C:/authoring/s2c3"));
    QCOMPARE(panel->scenePrefix(), QStringLiteral("SC2_"));
    QCOMPARE(panel->chaserPrefix(), QStringLiteral("CH2_"));
    QCOMPARE(panel->defaultFadeIn(), uint(111));
    QCOMPARE(panel->defaultHold(), uint(222));
    QCOMPARE(panel->defaultFadeOut(), uint(333));

    QByteArray frameXml;
    {
        QBuffer frameBuffer(&frameXml);
        QVERIFY(frameBuffer.open(QIODevice::WriteOnly));
        QXmlStreamWriter writer(&frameBuffer);
        writer.writeStartDocument();
        QVERIFY(layoutFrame->saveXML(&writer));
        writer.writeEndDocument();
        frameBuffer.close();
    }
    std::unique_ptr<VCFrame> loadedFrame = std::make_unique<VCFrame>(doc, nullptr, nullptr);
    QBuffer frameReadBuffer(&frameXml);
    QVERIFY(frameReadBuffer.open(QIODevice::ReadOnly));
    QXmlStreamReader frameReader(&frameReadBuffer);
    frameReader.readNextStartElement();
    QVERIFY2(!frameReader.atEnd(), "Invalid frame XML");
    QVERIFY2(loadedFrame->loadXML(frameReader), "VCFrame::loadXML failed");
    QCOMPARE(loadedFrame->id(), layoutFrameId);
    QCOMPARE(loadedFrame->totalPagesNumber(), 3);
    QCOMPARE(loadedFrame->pageLabels(),
             QStringList({QStringLiteral("Intro"), QStringLiteral("Main"), QStringLiteral("Outro")}));
    QCOMPARE(loadedFrame->pagesLoop(), true);
    QCOMPARE(loadedFrame->multiPageMode(), true);

    QByteArray panelXml;
    {
        QBuffer panelBuffer(&panelXml);
        QVERIFY(panelBuffer.open(QIODevice::WriteOnly));
        QXmlStreamWriter writer(&panelBuffer);
        writer.writeStartDocument();
        QVERIFY(panel->saveXML(&writer));
        writer.writeEndDocument();
        panelBuffer.close();
    }
    std::unique_ptr<VCRecordPanel> loadedPanel = std::make_unique<VCRecordPanel>(doc, nullptr);
    QBuffer panelReadBuffer(&panelXml);
    QVERIFY(panelReadBuffer.open(QIODevice::ReadOnly));
    QXmlStreamReader panelReader(&panelReadBuffer);
    panelReader.readNextStartElement();
    QVERIFY2(!panelReader.atEnd(), "Invalid record panel XML");
    QVERIFY2(loadedPanel->loadXML(panelReader), "VCRecordPanel::loadXML failed");
    QCOMPARE(loadedPanel->id(), panelId);
    QCOMPARE(loadedPanel->targetFolder(), QStringLiteral("C:/authoring/s2c3"));
    QCOMPARE(loadedPanel->scenePrefix(), QStringLiteral("SC2_"));
    QCOMPARE(loadedPanel->chaserPrefix(), QStringLiteral("CH2_"));
    QCOMPARE(loadedPanel->defaultFadeIn(), uint(111));
    QCOMPARE(loadedPanel->defaultHold(), uint(222));
    QCOMPARE(loadedPanel->defaultFadeOut(), uint(333));
}

void VCAuthoringOwner_Test::authoringCore_layoutPageOps_persistAcrossLoad()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    QVERIFY(vc->pagesCount() > 0);
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    const quint32 page0Id = page0->id();
    const QString page0CaptionBefore = page0->caption();
    const QString createdCaption = QStringLiteral("S2C7 Created Page");
    const QString renamedCaption = QStringLiteral("S2C7 Renamed Page");
    quint32 createdPageId = std::numeric_limits<quint32>::max();

    auto pageIndexById = [&](quint32 pageId) -> int {
        for (int i = 0; i < vc->pagesCount(); ++i)
        {
            VCPage *candidate = vc->page(i);
            if (candidate && candidate->id() == pageId)
                return i;
        }
        return -1;
    };
    auto pageIdsInOrder = [&]() -> QList<quint32> {
        QList<quint32> ids;
        for (int i = 0; i < vc->pagesCount(); ++i)
        {
            VCPage *candidate = vc->page(i);
            if (!candidate)
                return {};
            ids << candidate->id();
        }
        return ids;
    };
    auto pageCaptionsInOrder = [&]() -> QStringList {
        QStringList captions;
        for (int i = 0; i < vc->pagesCount(); ++i)
        {
            VCPage *candidate = vc->page(i);
            if (!candidate)
                return {};
            captions << candidate->caption();
        }
        return captions;
    };

    const QList<quint32> pageIdsBefore = pageIdsInOrder();
    const QStringList pageCaptionsBefore = pageCaptionsInOrder();
    QCOMPARE(pageIdsBefore.size(), vc->pagesCount());
    QCOMPARE(pageCaptionsBefore.size(), vc->pagesCount());

    auto cleanup = qScopeGuard([&]() {
        const int createdIndex = pageIndexById(createdPageId);
        if (createdIndex >= 0)
            vc->deletePage(createdIndex);
        const int page0Index = pageIndexById(page0Id);
        if (page0Index >= 0)
        {
            VCPage *restorePage0 = vc->page(page0Index);
            if (restorePage0)
                restorePage0->setCaption(page0CaptionBefore);
        }
    });

    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, doc, m_bridge.get());
    QVERIFY(tm.has("vc_prepare_layout"));
    QVERIFY(tm.has("vc_apply_preparation"));

    auto prepareAndApply = [&](const Json &args) {
        const Json prep = parseInvoke(tm.invoke("vc_prepare_layout", args));
        QVERIFY2(!prep.contains("error"), prep.dump().c_str());
        const std::string token = prep.value("preparationToken", std::string());
        QVERIFY2(!token.empty(), prep.dump().c_str());
        const Json applied = parseInvoke(tm.invoke(
            "vc_apply_preparation",
            Json{{"preparationToken", token}}));
        QVERIFY2(!applied.contains("error"), applied.dump().c_str());
        QCOMPARE(applied.value("applied", false), true);
    };

    prepareAndApply(Json{
        {"operation", "pageCreate"},
        {"name", createdCaption.toStdString()}
    });

    QCOMPARE(vc->pagesCount(), pageIdsBefore.size() + 1);
    const QList<quint32> pageIdsAfterCreate = pageIdsInOrder();
    QCOMPARE(pageIdsAfterCreate.size(), vc->pagesCount());
    for (int i = 0; i < pageIdsBefore.size(); ++i)
        QCOMPARE(pageIdsAfterCreate.at(i), pageIdsBefore.at(i));

    for (int i = 0; i < vc->pagesCount(); ++i)
    {
        VCPage *candidate = vc->page(i);
        if (candidate && candidate->caption() == createdCaption)
        {
            createdPageId = candidate->id();
            break;
        }
    }
    QVERIFY(createdPageId != std::numeric_limits<quint32>::max());
    QCOMPARE(pageIndexById(createdPageId), vc->pagesCount() - 1);

    prepareAndApply(Json{
        {"operation", "pageRename"},
        {"pageWidgetID", page0Id},
        {"name", renamedCaption.toStdString()}
    });

    QCOMPARE(pageIndexById(page0Id), 0);
    VCPage *renamedPage0 = vc->page(0);
    QVERIFY(renamedPage0 != nullptr);
    QCOMPARE(renamedPage0->id(), page0Id);
    QCOMPARE(renamedPage0->caption(), renamedCaption);

    prepareAndApply(Json{
        {"operation", "pageDelete"},
        {"pageWidgetID", createdPageId}
    });

    QCOMPARE(pageIndexById(createdPageId), -1);
    QCOMPARE(vc->pagesCount(), pageIdsBefore.size());
    const QList<quint32> pageIdsAfterDelete = pageIdsInOrder();
    const QStringList pageCaptionsAfterDelete = pageCaptionsInOrder();
    QCOMPARE(pageIdsAfterDelete.size(), vc->pagesCount());
    QCOMPARE(pageCaptionsAfterDelete.size(), vc->pagesCount());

    QCOMPARE(pageIdsAfterDelete, pageIdsBefore);
    QStringList expectedCaptionsAfterDelete = pageCaptionsBefore;
    expectedCaptionsAfterDelete[0] = renamedCaption;
    QCOMPARE(pageCaptionsAfterDelete, expectedCaptionsAfterDelete);

    QList<quint32> loadedIdsInOrder;
    QStringList loadedCaptionsInOrder;
    for (int i = 0; i < vc->pagesCount(); ++i)
    {
        VCPage *livePage = vc->page(i);
        QVERIFY(livePage != nullptr);

        QByteArray pageXml;
        {
            QBuffer pageBuffer(&pageXml);
            QVERIFY(pageBuffer.open(QIODevice::WriteOnly));
            QXmlStreamWriter writer(&pageBuffer);
            writer.writeStartDocument();
            QVERIFY(livePage->saveXML(&writer));
            writer.writeEndDocument();
            pageBuffer.close();
        }

        std::unique_ptr<VCPage> loadedPage = std::make_unique<VCPage>(nullptr, doc, nullptr, i, nullptr);
        QBuffer pageReadBuffer(&pageXml);
        QVERIFY(pageReadBuffer.open(QIODevice::ReadOnly));
        QXmlStreamReader pageReader(&pageReadBuffer);
        pageReader.readNextStartElement();
        QVERIFY2(!pageReader.atEnd(), "Invalid page XML");
        QVERIFY2(loadedPage->loadXML(pageReader), "VCPage::loadXML failed");

        loadedIdsInOrder << loadedPage->id();
        loadedCaptionsInOrder << loadedPage->caption();
        QCOMPARE(loadedPage->id(), livePage->id());
        QCOMPARE(loadedPage->caption(), livePage->caption());
    }

    QCOMPARE(loadedIdsInOrder, pageIdsAfterDelete);
    QCOMPARE(loadedCaptionsInOrder, pageCaptionsAfterDelete);
}

void VCAuthoringOwner_Test::authoringGeometry_applyRefusals_afterStateChange_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("expectedCode");
    QTest::addColumn<QString>("expectedErrorPart");

    QTest::newRow("queued-start")
        << QStringLiteral("queued-start")
        << QStringLiteral("authoringPrecondition")
        << QStringLiteral("queued starts");
    QTest::newRow("feedback-patch")
        << QStringLiteral("feedback-patch")
        << QStringLiteral("authoringPrecondition")
        << QStringLiteral("Feedback patch");
    QTest::newRow("relative-source")
        << QStringLiteral("relative-source")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("Relative input sources");
    QTest::newRow("protected-page-pin")
        << QStringLiteral("protected-page-pin")
        << QStringLiteral("authoringPrecondition")
        << QStringLiteral("Protected page editing");
    QTest::newRow("armed-recorder")
        << QStringLiteral("armed-recorder")
        << QStringLiteral("authoringPrecondition")
        << QStringLiteral("Recorder is active");
}

void VCAuthoringOwner_Test::authoringGeometry_applyRefusals_afterStateChange()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);
    QFETCH(QString, expectedCode);
    QFETCH(QString, expectedErrorPart);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    InputOutputMap *ioMap = doc->inputOutputMap();
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    if (page->PIN() != 0)
    {
        vc->validatePagePIN(0, QStringLiteral("1234"), true);
        QVERIFY(vc->setPagePIN(0, QStringLiteral("1234"), QString()));
    }
    if (ShowCommandRecorder *recorder = ShowCommandRecorder::instance())
    {
        if (recorder->isRecording())
            QVERIFY(recorder->setRecording(false));
        QVERIFY(recorder->setResolvedShow(Function::invalidId()));
    }
    MasterTimer *timer = doc->masterTimer();
    QVERIFY(timer != nullptr);
    auto noQueuedStarts = [&]() {
        const QList<Function *> functions = doc->functions();
        for (Function *function : functions)
            if (function && timer->isStartQueued(function))
                return false;
        return true;
    };
    if (page->PIN() != 0)
    {
        vc->validatePagePIN(0, QStringLiteral("1234"), true);
        QVERIFY(vc->setPagePIN(0, QStringLiteral("1234"), QString()));
    }
    ShowCommandRecorder *recorder = ShowCommandRecorder::instance();
    QVERIFY(recorder != nullptr);
    if (recorder->isRecording())
        QVERIFY(recorder->setRecording(false));
    QVERIFY(recorder->setResolvedShow(Function::invalidId()));

    QVERIFY2(ioMap->setOutputPatch(0, m_probeName, "", "", 0, false, 0),
             "Failed to patch probe output");

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 400));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    QSharedPointer<QLCInputSource> source;
    if (scenario == QStringLiteral("feedback-patch") || scenario == QStringLiteral("relative-source"))
    {
        source.reset(new QLCInputSource(0, 1));
        source->setID(0);
        button->addInputSource(source);
    }

    Scene *scene = nullptr;
    Show *show = nullptr;
    const bool recorderWasRecording = recorder->isRecording();
    const quint32 recorderBoundShow = recorder->boundShowId();
    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();

    auto cleanup = qScopeGuard([&]() {
        timer->stopAllFunctions();
        timer->stop();
        for (int i = 0; i < 20 && (!timer->runningFunctionIds().isEmpty() || !noQueuedStarts()); ++i)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        if (scene != nullptr)
        {
            doc->deleteFunction(scene->id());
            scene = nullptr;
        }
        if (show != nullptr)
        {
            doc->deleteFunction(show->id());
            show = nullptr;
        }
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), false, 0);
        if (page->PIN() != 0)
            vc->setPagePIN(0, QStringLiteral("1234"), QString());
        if (recorder->boundShowId() != recorderBoundShow)
            recorder->setResolvedShow(recorderBoundShow);
        if (recorder->isRecording() != recorderWasRecording)
            recorder->setRecording(recorderWasRecording);
        vc->deleteVCWidgets({buttonId, frameId});
    });

    timer->stopAllFunctions();
    timer->stop();
    for (int i = 0; i < 20 && (!timer->runningFunctionIds().isEmpty() || !noQueuedStarts()); ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    QVERIFY2(timer->runningFunctionIds().isEmpty(), "Running functions were not quiesced before prepare");
    QVERIFY2(noQueuedStarts(), "Queued starts were not quiesced before prepare");

    VCBridge::AuthoringLayoutRequest req;
    VCBridge::AuthoringGeometryChange change;
    change.widgetID = buttonId;
    change.logicalRect = QRectF(80, 120, 80, 60);
    req.geometryChanges.append(change);

    const VCBridge::AuthoringPreparationResult prep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QVERIFY2(prep.code.isEmpty(), qPrintable(QStringLiteral("prepare failed unexpectedly: %1").arg(prep.error)));
    QCOMPARE(prep.code, QString());
    QVERIFY(!prep.preparationToken.isEmpty());

    if (scenario == QStringLiteral("queued-start"))
    {
        scene = new Scene(doc);
        scene->setName(QStringLiteral("Apply refusal queued"));
        QVERIFY(doc->addFunction(scene));
        timer->stop();
        scene->start(timer, FunctionParent::master());
        QVERIFY(timer->isStartQueued(scene));
    }
    else if (scenario == QStringLiteral("feedback-patch"))
    {
        QVERIFY2(ioMap->setOutputPatch(0, m_probeName, "", "", 0, true),
                 "Failed to patch feedback line");
    }
    else if (scenario == QStringLiteral("relative-source"))
    {
        QVERIFY(!source.isNull());
        source->setWorkingMode(QLCInputSource::Relative);
    }
    else if (scenario == QStringLiteral("protected-page-pin"))
    {
        QVERIFY(vc->setPagePIN(0, QString(), QStringLiteral("1234")));
    }
    else if (scenario == QStringLiteral("armed-recorder"))
    {
        show = new Show(doc);
        show->setName(QStringLiteral("Apply refusal recorder"));
        QVERIFY(doc->addFunction(show));
        QVERIFY(recorder->setResolvedShow(show->id()));
        QVERIFY(recorder->setRecording(true));
        QVERIFY(recorder->isAuthoring());
    }

    const QRectF beforeRect = button->logicalGeometry();
    const bool beforeDirty = doc->isModified();
    const QByteArray beforeXml = serializeConsole();
    const int beforePendingRuns = recorder->pendingControlRuns();
    const int beforePendingRequests = recorder->pendingUserRequests().count();
    resetProbeCounters();
    const VCBridge::AuthoringApplyResult apply =
        m_bridge->applyAuthoring(prep.preparationToken);
    QCOMPARE(apply.code, expectedCode);
    QVERIFY(apply.error.contains(expectedErrorPart, Qt::CaseInsensitive));
    QCOMPARE(apply.applied, false);
    QCOMPARE(apply.tokenConsumed, true);
    QCOMPARE(button->logicalGeometry(), beforeRect);
    QCOMPARE(doc->isModified(), beforeDirty);
    if (scenario != QStringLiteral("protected-page-pin"))
        QCOMPARE(serializeConsole(), beforeXml);
    QCOMPARE(counter("openCount"), 0);
    QCOMPARE(counter("closeCount"), 0);
    QCOMPARE(counter("writeCount"), 0);
    QCOMPARE(counter("feedbackCount"), 0);
    QCOMPARE(recorder->pendingControlRuns(), beforePendingRuns);
    QCOMPARE(recorder->pendingUserRequests().count(), beforePendingRequests);
}

void VCAuthoringOwner_Test::authoringPreparation_predicateObservationRows_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("expectedCode");
    QTest::addColumn<QString>("expectedObservation");
    QTest::addColumn<QString>("expectedErrorPart");

    QTest::newRow("P-no-profile")
        << QStringLiteral("P-no-profile")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("profileRoutingReady")
        << QStringLiteral("Destination input patch/profile is not ready");
    QTest::newRow("P-relative-destination")
        << QStringLiteral("P-relative-destination")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("profileRoutingReady")
        << QStringLiteral("Relative destination profile channels are not supported");
    QTest::newRow("F-feedback-patched")
        << QStringLiteral("F-feedback-patched")
        << QStringLiteral("authoringPrecondition")
        << QStringLiteral("feedbackPatchIdle")
        << QStringLiteral("Feedback patch must be disabled");
    QTest::newRow("R-relative")
        << QStringLiteral("R-relative")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("relativeSources")
        << QStringLiteral("Relative input sources");
    QTest::newRow("U-needs-update")
        << QStringLiteral("U-needs-update")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("feedbackNeedsUpdateIdle")
        << QStringLiteral("Feedback source update is pending");
    QTest::newRow("W-writer-safe")
        << QStringLiteral("W-writer-safe")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("writerSafe")
        << QStringLiteral("writer effects are not certifiable");
}

void VCAuthoringOwner_Test::authoringPreparation_predicateObservationRows()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);
    QFETCH(QString, expectedCode);
    QFETCH(QString, expectedObservation);
    QFETCH(QString, expectedErrorPart);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    if (page->PIN() != 0)
    {
        vc->validatePagePIN(0, QStringLiteral("1234"), true);
        QVERIFY(vc->setPagePIN(0, QStringLiteral("1234"), QString()));
    }

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 300));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    VCSlider *slider = new VCSlider(doc, frame);
    vc->addWidgetToMap(slider);
    frame->addWidgetToPageMap(slider);
    slider->setLogicalGeometry(QRectF(130, 40, 120, 120));
    slider->setSliderMode(VCSlider::Level);
    VCFrame *otherFrame = nullptr;

    QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 1));
    source->setID(0);
    source->setWorkingMode(QLCInputSource::Absolute);
    button->addInputSource(source);

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    const quint32 sliderId = slider->id();
    auto cleanup = qScopeGuard([&]() {
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), false, 0);
        QVariantList widgetIds {buttonId, sliderId, frameId};
        if (otherFrame != nullptr)
            widgetIds.append(otherFrame->id());
        vc->deleteVCWidgets(widgetIds);
    });

    VCBridge::AuthoringPreparationResult result;
    if (scenario == QStringLiteral("W-writer-safe"))
    {
        otherFrame = new VCFrame(doc, vc, page);
        vc->addWidgetToMap(otherFrame);
        page->addWidgetToPageMap(otherFrame);
        otherFrame->setShowHeader(false);
        otherFrame->setLogicalGeometry(QRectF(0, 320, 500, 300));

        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::Reorder;
        req.widgetIDs.append(sliderId);
        req.destinationFrameWidgetID = otherFrame->id();
        req.destinationSubpage = slider->page();
        req.beforeWidgetID = buttonId;
        result = m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    }
    else
    {
        VCBridge::AuthoringBindingsRequest req;
        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.controlSlotID = 0;
        if (scenario == QStringLiteral("P-no-profile"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
            item.action = VCBridge::AuthoringBindingItem::Action::Append;
            VCBridge::AuthoringDestination destination;
            destination.inputUniverseID = 0;
            destination.inputChannel = 1;
            destination.profileName = QStringLiteral("MissingProfile");
            item.destination = destination;
        }
        else if (scenario == QStringLiteral("P-relative-destination"))
        {
            const QString profilePath = m_workspaceDir.filePath(QStringLiteral("pass-b-relative-destination%1")
                                                                 .arg(KExtInputProfile));
            QFile f(profilePath);
            QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text),
                     qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(profilePath)));
            const QByteArray xml = QByteArrayLiteral(
                "<?xml version='1.0' encoding='UTF-8'?>\n"
                "<!DOCTYPE InputProfile>\n"
                "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
                " <Creator><Name>PassB</Name><Version>1</Version><Author>test</Author></Creator>\n"
                " <Manufacturer>PBMCP</Manufacturer>\n"
                " <Model>RelativeDestination</Model>\n"
                " <Type>MIDI</Type>\n"
                " <Channel Number=\"7\"><Name>Rel7</Name><Type>Slider</Type><Movement Sensitivity=\"10\">Relative</Movement></Channel>\n"
                "</InputProfile>\n");
            QCOMPARE(f.write(xml), xml.size());
            f.close();
            std::unique_ptr<QLCInputProfile> loaded(QLCInputProfile::loader(profilePath));
            QVERIFY2(loaded != nullptr, "Profile fixture loader failed");
            const QString profileName = loaded->name();
            ioMap->loadProfiles(QDir(m_workspaceDir.path()));
            QVERIFY(ioMap->profileNames().contains(profileName));
            QLCInputProfile *profile = ioMap->profile(profileName);
            QVERIFY(profile != nullptr);
            QLCInputChannel *profileChannel = profile->channel(7);
            QVERIFY(profileChannel != nullptr);
            QCOMPARE(profileChannel->movementType(), QLCInputChannel::Relative);
            QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, profileName),
                     "Failed to patch input with relative profile");
            item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
            item.action = VCBridge::AuthoringBindingItem::Action::Append;
            VCBridge::AuthoringDestination destination;
            destination.inputUniverseID = 0;
            destination.inputChannel = 7;
            destination.profileName = profileName;
            item.destination = destination;
        }
        else if (scenario == QStringLiteral("F-feedback-patched"))
        {
            QVERIFY2(ioMap->setOutputPatch(0, m_probeName, "", "", 0, true),
                     "Failed to patch feedback line");
            item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.sourceKey = VCBridge::AuthoringSourceKey{
                0,
                0,
                1,
                0
            };
            item.idleValue = 10;
            item.activeValue = 20;
            item.monitorValue = 30;
        }
        else if (scenario == QStringLiteral("R-relative"))
        {
            const QString profilePath = m_workspaceDir.filePath(QStringLiteral("pass-b-r-relative%1")
                                                                 .arg(KExtInputProfile));
            QFile f(profilePath);
            QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text),
                     qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(profilePath)));
            const QByteArray xml = QByteArrayLiteral(
                "<?xml version='1.0' encoding='UTF-8'?>\n"
                "<!DOCTYPE InputProfile>\n"
                "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
                " <Creator><Name>PassB</Name><Version>1</Version><Author>test</Author></Creator>\n"
                " <Manufacturer>PBMCP</Manufacturer>\n"
                " <Model>RRelative</Model>\n"
                " <Type>MIDI</Type>\n"
                " <Channel Number=\"1\"><Name>Abs1</Name><Type>Slider</Type></Channel>\n"
                "</InputProfile>\n");
            QCOMPARE(f.write(xml), xml.size());
            f.close();
            std::unique_ptr<QLCInputProfile> loaded(QLCInputProfile::loader(profilePath));
            QVERIFY2(loaded != nullptr, "Profile fixture loader failed");
            const QString profileName = loaded->name();
            ioMap->loadProfiles(QDir(m_workspaceDir.path()));
            QVERIFY(ioMap->profileNames().contains(profileName));
            QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, profileName),
                     "Failed to patch input with absolute profile");
            source->setWorkingMode(QLCInputSource::Relative);
            item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
            item.action = VCBridge::AuthoringBindingItem::Action::Move;
            item.sourceKey = VCBridge::AuthoringSourceKey{
                0,
                0,
                1,
                0
            };
            VCBridge::AuthoringDestination destination;
            destination.inputUniverseID = 0;
            destination.inputChannel = 1;
            destination.profileName = profileName;
            item.destination = destination;
        }
        else // U-needs-update
        {
            source->setWorkingMode(QLCInputSource::Encoder);
            item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.sourceKey = VCBridge::AuthoringSourceKey{
                0,
                0,
                1,
                0
            };
            item.idleValue = 10;
            item.activeValue = 20;
            item.monitorValue = 30;
        }
        req.items.append(item);
        result = m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    }

    QCOMPARE(result.code, expectedCode);
    QVERIFY(result.preparationToken.isEmpty());
    QCOMPARE(result.expiresInMs, 0);
    QVERIFY(result.error.contains(expectedErrorPart, Qt::CaseInsensitive));

    bool foundObs = false;
    for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
    {
        if (obs.name != expectedObservation)
            continue;
        foundObs = true;
        QCOMPARE(obs.met, false);
        break;
    }
    QVERIFY2(foundObs, qPrintable(QStringLiteral("Missing observation %1").arg(expectedObservation)));
}

void VCAuthoringOwner_Test::authoringPassB_gateMetUnsupported_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("expectedObservation");

    QTest::newRow("controller-append-loaded-destination")
        << QStringLiteral("controller-append-loaded-destination")
        << QStringLiteral("profileRoutingReady");
    QTest::newRow("feedback-set-absolute-no-feedbackpatch")
        << QStringLiteral("feedback-set-absolute-no-feedbackpatch")
        << QStringLiteral("feedbackNeedsUpdateIdle");
    QTest::newRow("layout-align-safe-closure")
        << QStringLiteral("layout-align-safe-closure")
        << QStringLiteral("relativeSources");
    QTest::newRow("controller-remove-absolute-source-unpatched")
        << QStringLiteral("controller-remove-absolute-source-unpatched")
        << QStringLiteral("relativeSources");
}

void VCAuthoringOwner_Test::authoringPassB_gateMetUnsupported()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);
    QFETCH(QString, expectedObservation);
    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 400));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    VCSlider *slider = new VCSlider(doc, frame);
    vc->addWidgetToMap(slider);
    frame->addWidgetToPageMap(slider);
    slider->setSliderMode(VCSlider::Level);
    slider->setLogicalGeometry(QRectF(130, 40, 120, 140));

    VCRecordPanel *panel = new VCRecordPanel(doc, frame);
    vc->addWidgetToMap(panel);
    frame->addWidgetToPageMap(panel);
    panel->setLogicalGeometry(QRectF(280, 40, 160, 120));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    const quint32 sliderId = slider->id();
    const quint32 panelId = panel->id();
    auto cleanup = qScopeGuard([&]() {
        ioMap->setInputPatch(0, KInputNone, "", "", QLCIOPlugin::invalidLine(), KInputNone);
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        vc->deleteVCWidgets({buttonId, sliderId, panelId, frameId});
    });
    bool dirtyBeforePrepare = false;
    QByteArray xmlBeforePrepare;
    int baselineOpen = 0;
    int baselineClose = 0;
    int baselineWrite = 0;
    int baselineFeedback = 0;
    auto beginMeasuredPrepareWindow = [&]() {
        dirtyBeforePrepare = doc->isModified();
        xmlBeforePrepare = serializeConsole();
        resetProbeCounters();
        baselineOpen = counter("openCount");
        baselineClose = counter("closeCount");
        baselineWrite = counter("writeCount");
        baselineFeedback = counter("feedbackCount");
    };

    VCBridge::AuthoringPreparationResult result;
    if (scenario == QStringLiteral("controller-append-loaded-destination"))
    {
        const QString path = m_workspaceDir.filePath(QStringLiteral("pass-b-append%1").arg(KExtInputProfile));
        QFile f(path);
        QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(path)));
        const QByteArray xml = QByteArrayLiteral(
            "<?xml version='1.0' encoding='UTF-8'?>\n"
            "<!DOCTYPE InputProfile>\n"
            "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
            " <Creator><Name>PassB</Name><Version>1</Version><Author>test</Author></Creator>\n"
            " <Manufacturer>PBMCP</Manufacturer>\n"
            " <Model>Profile_append</Model>\n"
            " <Type>MIDI</Type>\n"
            " <Channel Number=\"7\"><Name>Ch</Name><Type>Slider</Type></Channel>\n"
            "</InputProfile>\n");
        QCOMPARE(f.write(xml), xml.size());
        f.close();
        std::unique_ptr<QLCInputProfile> loaded(QLCInputProfile::loader(path));
        QVERIFY2(loaded != nullptr, "Profile fixture loader failed");
        const QString profileName = loaded->name();
        QDir workspaceDir(m_workspaceDir.path());
        ioMap->loadProfiles(workspaceDir);
        QVERIFY(ioMap->profileNames().contains(profileName));
        QLCInputProfile *profile = ioMap->profile(profileName);
        QVERIFY(profile != nullptr);
        QLCInputChannel *profileChannel = profile->channel(7);
        QVERIFY(profileChannel != nullptr);
        QCOMPARE(profileChannel->type(), QLCInputChannel::Slider);
        QCOMPARE(profileChannel->movementType(), QLCInputChannel::Absolute);
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, profileName),
                 "Failed to patch input with loaded profile");

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        item.controlSlotID = 0;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 7;
        dst.profileName = profileName;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);
        beginMeasuredPrepareWindow();
        result = m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    }
    else if (scenario == QStringLiteral("feedback-set-absolute-no-feedbackpatch"))
    {
        QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
        source->setID(0);
        source->setWorkingMode(QLCInputSource::Absolute);
        button->addInputSource(source);

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
        item.action = VCBridge::AuthoringBindingItem::Action::Set;
        item.controlSlotID = 0;
        VCBridge::AuthoringSourceKey sourceKey;
        sourceKey.controlSlotID = 0;
        sourceKey.inputUniverseID = 0;
        sourceKey.inputChannel = 7;
        sourceKey.childSubpage = 0;
        item.sourceKey = sourceKey;
        item.idleValue = 10;
        item.activeValue = 20;
        item.monitorValue = 30;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);
        beginMeasuredPrepareWindow();
        result = m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    }
    else if (scenario == QStringLiteral("controller-remove-absolute-source-unpatched"))
    {
        QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
        source->setID(0);
        source->setPage(0);
        source->setWorkingMode(QLCInputSource::Absolute);
        button->addInputSource(source);
        VCBridge::AuthoringBindingItem item;
        item.widgetID = button->id();
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Remove;
        item.controlSlotID = 0;
        item.sourceKey = VCBridge::AuthoringSourceKey{
            0,
            0,
            7,
            0
        };
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);
        beginMeasuredPrepareWindow();
        result = m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    }
    else if (scenario == QStringLiteral("recordpanel-set-supported-fields"))
    {
        VCBridge::AuthoringRecordPanelItem item;
        item.widgetID = panelId;
        item.targetFolder = QStringLiteral("C:/tmp");
        item.scenePrefix = QStringLiteral("SC_");
        item.chaserPrefix = QStringLiteral("CH_");
        item.defaultFadeIn = 120;
        item.defaultHold = 220;
        item.defaultFadeOut = 320;
        VCBridge::AuthoringRecordPanelRequest req;
        req.items.append(item);
        beginMeasuredPrepareWindow();
        result = m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    }
    else // layout-align-safe-closure
    {
        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::Align;
        req.frameWidgetID = frameId;
        req.columns = 2;
        req.padding = 8;
        beginMeasuredPrepareWindow();
        result = m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    }

    const bool supportedScenario =
        (scenario == QStringLiteral("controller-append-loaded-destination") ||
         scenario == QStringLiteral("controller-remove-absolute-source-unpatched") ||
         scenario == QStringLiteral("feedback-set-absolute-no-feedbackpatch"));
    if (supportedScenario)
    {
        QCOMPARE(result.code, QString());
        QVERIFY(!result.preparationToken.isEmpty());
        QVERIFY(result.expiresInMs > 0);
        const VCBridge::AuthoringApplyResult applied = m_bridge->applyAuthoring(result.preparationToken);
        QCOMPARE(applied.code, QString());
        QCOMPARE(applied.tokenConsumed, true);
    }
    else
    {
        QCOMPARE(result.code, QStringLiteral("unsupportedPreparation"));
        QVERIFY(result.preparationToken.isEmpty());
        QCOMPARE(result.expiresInMs, 0);
    }
    if (supportedScenario)
    {
        QCOMPARE(doc->isModified(), dirtyBeforePrepare);
        QVERIFY(serializeConsole() != xmlBeforePrepare);
    }
    else
    {
        QCOMPARE(doc->isModified(), dirtyBeforePrepare);
        QCOMPARE(serializeConsole(), xmlBeforePrepare);
    }

    bool foundObs = false;
    for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
    {
        if (obs.name != expectedObservation)
            continue;
        foundObs = true;
        QCOMPARE(obs.met, true);
        break;
    }
    QVERIFY2(foundObs, qPrintable(QStringLiteral("Missing met precondition observation: %1").arg(expectedObservation)));

    // B3: prepare is read-only and must leave runtime counters untouched.
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
}

void VCAuthoringOwner_Test::authoringPassB_destinationPackedChannelLookup()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setTotalPagesNumber(2);
    frame->setCurrentPage(1);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 400));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));
    button->setPage(1);
    QCOMPARE(button->page(), 1);

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    std::unique_ptr<QLCInputProfile> unloadedPatchedProfile;
    auto cleanup = qScopeGuard([&]() {
        ioMap->setInputPatch(0, KInputNone, "", "", QLCIOPlugin::invalidLine(), KInputNone);
        vc->deleteVCWidgets({buttonId, frameId});
    });

    const QString profilePath = m_workspaceDir.filePath(QStringLiteral("pass-b-packed%1").arg(KExtInputProfile));
    QFile f(profilePath);
    QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(profilePath)));
    const QByteArray xml = QByteArrayLiteral(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE InputProfile>\n"
        "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
        " <Creator><Name>PassB</Name><Version>1</Version><Author>test</Author></Creator>\n"
        " <Manufacturer>PBMCP</Manufacturer>\n"
        " <Model>Packed</Model>\n"
        " <Type>MIDI</Type>\n"
        " <Channel Number=\"5\"><Name>Ch5</Name><Type>Slider</Type></Channel>\n"
        "</InputProfile>\n");
    QCOMPARE(f.write(xml), xml.size());
    f.close();
    std::unique_ptr<QLCInputProfile> loaded(QLCInputProfile::loader(profilePath));
    QVERIFY2(loaded != nullptr, "Profile fixture loader failed");
    const QString profileName = loaded->name();
    QDir workspaceDir(m_workspaceDir.path());
    ioMap->loadProfiles(workspaceDir);
    QVERIFY(ioMap->profileNames().contains(profileName));
    QLCInputProfile *profile = ioMap->profile(profileName);
    QVERIFY(profile != nullptr);
    QLCInputChannel *profileChannel = profile->channel(5);
    QVERIFY(profileChannel != nullptr);
    QCOMPARE(profileChannel->type(), QLCInputChannel::Slider);
    QCOMPARE(profileChannel->movementType(), QLCInputChannel::Absolute);
    QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, profileName),
             "Failed to patch input with loaded profile");

    QSharedPointer<QLCInputSource> existing(new QLCInputSource(0, 5));
    existing->setID(0);
    existing->setPage(1);
    existing->setWorkingMode(QLCInputSource::Absolute);
    button->addInputSource(existing);
    const quint32 packedChannel = (1u << 16) | 5u;
    QCOMPARE(existing->channel(), packedChannel);

    VCBridge::AuthoringBindingItem item;
    item.widgetID = buttonId;
    item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
    item.action = VCBridge::AuthoringBindingItem::Action::Move;
    item.controlSlotID = 0;
    VCBridge::AuthoringSourceKey srcKey;
    srcKey.controlSlotID = 0;
    srcKey.inputUniverseID = 0;
    srcKey.inputChannel = 5;
    srcKey.childSubpage = 1;
    item.sourceKey = srcKey;
    VCBridge::AuthoringDestination dst;
    dst.inputUniverseID = 0;
    dst.inputChannel = 5; // unpacked request channel
    dst.profileName = profileName;
    item.destination = dst;
    VCBridge::AuthoringBindingsRequest req;
    req.items.append(item);
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    const VCBridge::AuthoringPreparationResult result =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(result.code, QString());
    QVERIFY(!result.preparationToken.isEmpty());
    QVERIFY(result.expiresInMs > 0);

    bool sawProfileRoutingReady = false;
    bool sawRelativeSources = false;
    bool anyMetFalse = false;
    for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
    {
        if (!obs.met)
            anyMetFalse = true;
        if (obs.name == QStringLiteral("profileRoutingReady"))
            sawProfileRoutingReady = obs.met;
        if (obs.name == QStringLiteral("relativeSources"))
            sawRelativeSources = obs.met;
    }
    QVERIFY(!anyMetFalse);
    QVERIFY2(sawProfileRoutingReady, "Missing met profileRoutingReady observation");
    QVERIFY2(sawRelativeSources, "Missing met relativeSources observation");

    const VCBridge::AuthoringApplyResult applied = m_bridge->applyAuthoring(result.preparationToken);
    QCOMPARE(applied.code, QString());
    QCOMPARE(applied.tokenConsumed, true);

    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
}

void VCAuthoringOwner_Test::authoringPassB_destinationPredicateMatrix_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("expectedCode");
    QTest::addColumn<QString>("expectedObservation");
    QTest::addColumn<bool>("expectedMet");
    QTest::addColumn<QString>("expectedErrorPart");

    QTest::newRow("destination-unpatched")
        << QStringLiteral("destination-unpatched")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("profileRoutingReady")
        << false
        << QStringLiteral("Destination input patch/profile is not ready");
    QTest::newRow("destination-profile-mismatch")
        << QStringLiteral("destination-profile-mismatch")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("profileRoutingReady")
        << false
        << QStringLiteral("Destination input patch/profile is not ready");
    QTest::newRow("destination-unloaded-requested-profile")
        << QStringLiteral("destination-unloaded-requested-profile")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("profileRoutingReady")
        << false
        << QStringLiteral("Destination profile is not loaded");
    QTest::newRow("destination-missing-channel")
        << QStringLiteral("destination-missing-channel")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("profileRoutingReady")
        << false
        << QStringLiteral("Destination profile channel is unavailable");
    QTest::newRow("destination-relative-channel")
        << QStringLiteral("destination-relative-channel")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("profileRoutingReady")
        << false
        << QStringLiteral("Relative destination profile channels are not supported");
    QTest::newRow("destination-absolute-encoder")
        << QStringLiteral("destination-absolute-encoder")
        << QString()
        << QStringLiteral("profileRoutingReady")
        << true
        << QStringLiteral("");
}

void VCAuthoringOwner_Test::authoringPassB_destinationPredicateMatrix()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);
    QFETCH(QString, expectedCode);
    QFETCH(QString, expectedObservation);
    QFETCH(bool, expectedMet);
    QFETCH(QString, expectedErrorPart);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 300));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    std::unique_ptr<QLCInputProfile> unloadedPatchedProfile;
    auto cleanup = qScopeGuard([&]() {
        ioMap->setInputPatch(0, KInputNone, "", "", QLCIOPlugin::invalidLine(), KInputNone);
        vc->deleteVCWidgets({buttonId, frameId});
    });

    const QString profileTag = QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
    const QString absModel = QStringLiteral("PBMCPAbs_%1").arg(profileTag);
    const QString relModel = QStringLiteral("PBMCPRelative_%1").arg(profileTag);
    const QString encModel = QStringLiteral("PBMCPEncoder_%1").arg(profileTag);

    const QString absPath = m_workspaceDir.filePath(absModel + KExtInputProfile);
    QFile absFile(absPath);
    QVERIFY2(absFile.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(absPath)));
    const QByteArray absXml = QStringLiteral(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE InputProfile>\n"
        "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
        " <Creator><Name>PassB</Name><Version>1</Version><Author>test</Author></Creator>\n"
        " <Manufacturer>PBMCP</Manufacturer>\n"
        " <Model>%1</Model>\n"
        " <Type>MIDI</Type>\n"
        " <Channel Number=\"7\"><Name>Ch7</Name><Type>Slider</Type></Channel>\n"
        "</InputProfile>\n")
                             .arg(absModel)
                             .toUtf8();
    QCOMPARE(absFile.write(absXml), absXml.size());
    absFile.close();
    std::unique_ptr<QLCInputProfile> absLoaded(QLCInputProfile::loader(absPath));
    QVERIFY2(absLoaded != nullptr, "Absolute profile fixture loader failed");
    const QString absProfile = absLoaded->name();

    const QString relativePath = m_workspaceDir.filePath(relModel + KExtInputProfile);
    QFile relFile(relativePath);
    QVERIFY2(relFile.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(relativePath)));
    const QByteArray relXml = QStringLiteral(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE InputProfile>\n"
        "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
        " <Creator><Name>PassB</Name><Version>1</Version><Author>test</Author></Creator>\n"
        " <Manufacturer>PBMCP</Manufacturer>\n"
        " <Model>%1</Model>\n"
        " <Type>MIDI</Type>\n"
        " <Channel Number=\"7\"><Name>Rel7</Name><Type>Slider</Type><Movement Sensitivity=\"8\">Relative</Movement></Channel>\n"
        "</InputProfile>\n")
                             .arg(relModel)
                             .toUtf8();
    QCOMPARE(relFile.write(relXml), relXml.size());
    relFile.close();
    std::unique_ptr<QLCInputProfile> relLoaded(QLCInputProfile::loader(relativePath));
    QVERIFY2(relLoaded != nullptr, "Relative profile fixture loader failed");
    const QString relativeProfile = relLoaded->name();

    const QString encoderPath = m_workspaceDir.filePath(encModel + KExtInputProfile);
    QFile encFile(encoderPath);
    QVERIFY2(encFile.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(encoderPath)));
    const QByteArray encXml = QStringLiteral(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE InputProfile>\n"
        "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
        " <Creator><Name>PassB</Name><Version>1</Version><Author>test</Author></Creator>\n"
        " <Manufacturer>PBMCP</Manufacturer>\n"
        " <Model>%1</Model>\n"
        " <Type>MIDI</Type>\n"
        " <Channel Number=\"7\"><Name>Enc7</Name><Type>Encoder</Type></Channel>\n"
        "</InputProfile>\n")
                             .arg(encModel)
                             .toUtf8();
    QCOMPARE(encFile.write(encXml), encXml.size());
    encFile.close();
    std::unique_ptr<QLCInputProfile> encLoaded(QLCInputProfile::loader(encoderPath));
    QVERIFY2(encLoaded != nullptr, "Encoder profile fixture loader failed");
    const QString encoderProfile = encLoaded->name();

    if (scenario != QStringLiteral("destination-unloaded-requested-profile"))
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));

    if (scenario == QStringLiteral("destination-profile-mismatch"))
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch input");
    else if (scenario == QStringLiteral("destination-missing-channel"))
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch input");
    else if (scenario == QStringLiteral("destination-relative-channel"))
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, relativeProfile), "Failed to patch input");
    else if (scenario == QStringLiteral("destination-unloaded-requested-profile"))
    {
        unloadedPatchedProfile.reset(QLCInputProfile::loader(absPath));
        QVERIFY2(unloadedPatchedProfile != nullptr, "Unloaded profile fixture loader failed");
        Universe *universe0 = ioMap->universe(0);
        QVERIFY(universe0 != nullptr);
        QVERIFY(universe0->setInputPatch(m_probePlugin, 0, unloadedPatchedProfile.get()));
        QVERIFY2(!ioMap->profileNames().contains(absProfile), "Requested unloaded profile unexpectedly entered IOMap cache");
    }
    else if (scenario == QStringLiteral("destination-absolute-encoder"))
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, encoderProfile), "Failed to patch input");

    VCBridge::AuthoringBindingItem item;
    item.widgetID = buttonId;
    item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
    item.action = VCBridge::AuthoringBindingItem::Action::Append;
    item.controlSlotID = 0;
    VCBridge::AuthoringDestination destination;
    destination.inputUniverseID = 0;
    if (scenario == QStringLiteral("destination-missing-channel"))
        destination.inputChannel = 9;
    else
        destination.inputChannel = 7;
    if (scenario == QStringLiteral("destination-profile-mismatch"))
        destination.profileName = encoderProfile;
    else if (scenario == QStringLiteral("destination-unloaded-requested-profile"))
        destination.profileName = absProfile;
    else if (scenario == QStringLiteral("destination-relative-channel"))
        destination.profileName = relativeProfile;
    else if (scenario == QStringLiteral("destination-absolute-encoder"))
        destination.profileName = encoderProfile;
    else
        destination.profileName = absProfile;
    item.destination = destination;
    VCBridge::AuthoringBindingsRequest req;
    req.items.append(item);

    const VCBridge::AuthoringPreparationResult result =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(result.atomicScope, VCBridge::AuthoringPreparationResult::AtomicScope::IndependentItems);
    QCOMPARE(result.code, expectedCode);
    if (expectedCode.isEmpty())
    {
        QVERIFY(!result.preparationToken.isEmpty());
        QVERIFY(result.expiresInMs > 0);
        const VCBridge::AuthoringApplyResult applied = m_bridge->applyAuthoring(result.preparationToken);
        QCOMPARE(applied.code, QString());
        QCOMPARE(applied.tokenConsumed, true);
    }
    else
    {
        QVERIFY(result.error.contains(expectedErrorPart, Qt::CaseInsensitive));
        QVERIFY(result.preparationToken.isEmpty());
        QCOMPARE(result.expiresInMs, 0);
    }

    bool foundObs = false;
    for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
    {
        if (obs.name != expectedObservation)
            continue;
        foundObs = true;
        QCOMPARE(obs.met, expectedMet);
        break;
    }
    QVERIFY2(foundObs, qPrintable(QStringLiteral("Missing observation %1").arg(expectedObservation)));
}

void VCAuthoringOwner_Test::authoringPassB_functionBinding_feedbackPredicate()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 300));
    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));
    QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
    source->setID(0);
    source->setWorkingMode(QLCInputSource::Absolute);
    button->addInputSource(source);
    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("pass-b-function-feedback"));
    QVERIFY(doc->addFunction(scene));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() {
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        vc->deleteVCWidgets({buttonId, frameId});
        doc->deleteFunction(scene->id());
    });

    QVERIFY2(ioMap->setOutputPatch(0, m_probeName, "", "", 0, true),
             "Failed to patch feedback line");

    VCBridge::AuthoringBindingItem item;
    item.widgetID = buttonId;
    item.kind = VCBridge::AuthoringBindingItem::Kind::Function;
    item.action = VCBridge::AuthoringBindingItem::Action::Set;
    item.functionID = scene->id();
    VCBridge::AuthoringBindingsRequest req;
    req.items.append(item);

    const VCBridge::AuthoringPreparationResult result =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(result.code, QStringLiteral("authoringPrecondition"));
    QVERIFY(result.error.contains(QStringLiteral("Feedback patch"), Qt::CaseInsensitive));
    QVERIFY(result.preparationToken.isEmpty());
}

void VCAuthoringOwner_Test::authoringPassB_feedbackPredicates_useAffectedSourcesAndDescendants()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 700, 400));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 80, 60));

    QSharedPointer<QLCInputSource> source0(new QLCInputSource(0, 7));
    source0->setID(0);
    source0->setWorkingMode(QLCInputSource::Absolute);
    button->addInputSource(source0);
    QSharedPointer<QLCInputSource> source1(new QLCInputSource(1, 9));
    source1->setID(1);
    source1->setWorkingMode(QLCInputSource::Absolute);
    button->addInputSource(source1);

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() {
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        ioMap->setOutputPatch(1, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        vc->deleteVCWidgets({buttonId, frameId});
    });

    QVERIFY2(ioMap->setOutputPatch(1, m_probeName, "", "", 0, true),
             "Failed to patch feedback line for second source universe");

    VCBridge::AuthoringBindingItem item;
    item.widgetID = buttonId;
    item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
    item.action = VCBridge::AuthoringBindingItem::Action::Set;
    item.controlSlotID = 0;
    item.sourceKey = VCBridge::AuthoringSourceKey{
        0,
        0,
        7,
        0
    };
    item.idleValue = 10;
    item.activeValue = 20;
    item.monitorValue = 30;
    VCBridge::AuthoringBindingsRequest req;
    req.items.append(item);

    const VCBridge::AuthoringPreparationResult result =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(result.code, QStringLiteral("authoringPrecondition"));
    QVERIFY(result.error.contains(QStringLiteral("Feedback patch"), Qt::CaseInsensitive));
    bool foundObs = false;
    for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
    {
        if (obs.name != QStringLiteral("feedbackPatchIdle"))
            continue;
        foundObs = true;
        QCOMPARE(obs.met, false);
        QVERIFY(obs.scope.contains(QStringLiteral("universe:1")));
    }
    QVERIFY2(foundObs, "Missing feedbackPatchIdle failure for non-selected affected source");
}

void VCAuthoringOwner_Test::authoringPassB_reorderSameParent_levelSlider_noWriterSafeRefusal()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 300));

    VCSlider *slider = new VCSlider(doc, frame);
    vc->addWidgetToMap(slider);
    frame->addWidgetToPageMap(slider);
    slider->setSliderMode(VCSlider::Level);
    slider->setLogicalGeometry(QRectF(20, 40, 120, 140));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(180, 40, 80, 60));

    const quint32 frameId = frame->id();
    const quint32 sliderId = slider->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({sliderId, buttonId, frameId});
    });

    VCBridge::AuthoringLayoutRequest req;
    req.operation = VCBridge::AuthoringLayoutRequest::Operation::Reorder;
    req.widgetIDs.append(sliderId);
    req.destinationFrameWidgetID = frameId;
    req.destinationSubpage = slider->page();
    req.beforeWidgetID = buttonId;

    const VCBridge::AuthoringPreparationResult result =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(result.code, QStringLiteral("unsupportedPreparation"));
    QCOMPARE(result.error,
             QStringLiteral("This bridge supports geometry preparation only at this checkpoint"));
    QVERIFY(result.preparationToken.isEmpty());
    QCOMPARE(result.expiresInMs, 0);
    bool sawRequestLimitation = false;
    bool sawRelativeSources = false;
    bool sawFeedbackPatchIdle = false;
    bool sawWriterSafe = false;
    bool anyMetFalse = false;
    for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
    {
        if (!obs.met)
            anyMetFalse = true;
        if (obs.name == QStringLiteral("relativeSources"))
            sawRelativeSources = obs.met;
        if (obs.name == QStringLiteral("feedbackPatchIdle"))
            sawFeedbackPatchIdle = obs.met;
        if (obs.name == QStringLiteral("writerSafe"))
            sawWriterSafe = true;
    }
    QVERIFY(!anyMetFalse);
    QVERIFY2(sawRelativeSources, "Missing met relativeSources observation");
    QVERIFY2(sawFeedbackPatchIdle, "Missing met feedbackPatchIdle observation");
    QVERIFY2(!sawWriterSafe, "writerSafe must be absent for same-parent reorder");
    for (const VCBridge::AuthoringLimitation &lim : result.limitations)
    {
        if (lim.code == QStringLiteral("unsupportedPreparation") &&
            lim.scope == QStringLiteral("request") &&
            lim.message == QStringLiteral("Recognized authoring family is not implemented at this checkpoint"))
            sawRequestLimitation = true;
    }
    QVERIFY2(sawRequestLimitation, "Missing request-scope unsupportedPreparation limitation");
}

void VCAuthoringOwner_Test::authoringPassB_mixedBindings_doNotLeakObservationsAcrossItems()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 700, 400));

    VCButton *controllerButton = new VCButton(doc, frame);
    vc->addWidgetToMap(controllerButton);
    frame->addWidgetToPageMap(controllerButton);
    controllerButton->setLogicalGeometry(QRectF(20, 40, 100, 80));

    VCButton *feedbackButton = new VCButton(doc, frame);
    vc->addWidgetToMap(feedbackButton);
    frame->addWidgetToPageMap(feedbackButton);
    feedbackButton->setLogicalGeometry(QRectF(180, 40, 100, 80));
    QSharedPointer<QLCInputSource> feedbackSource(new QLCInputSource(0, 7));
    feedbackSource->setID(0);
    feedbackSource->setWorkingMode(QLCInputSource::Absolute);
    feedbackButton->addInputSource(feedbackSource);

    const quint32 frameId = frame->id();
    const quint32 controllerButtonId = controllerButton->id();
    const quint32 feedbackButtonId = feedbackButton->id();
    auto cleanup = qScopeGuard([&]() {
        ioMap->setInputPatch(0, KInputNone, "", "", QLCIOPlugin::invalidLine(), KInputNone);
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        vc->deleteVCWidgets({controllerButtonId, feedbackButtonId, frameId});
    });

    const QString path = m_workspaceDir.filePath(QStringLiteral("pass-b-mixed-bindings%1").arg(KExtInputProfile));
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(path)));
    const QByteArray xml = QByteArrayLiteral(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE InputProfile>\n"
        "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
        " <Creator><Name>PassB</Name><Version>1</Version><Author>test</Author></Creator>\n"
        " <Manufacturer>PBMCP</Manufacturer>\n"
        " <Model>MixedBindings</Model>\n"
        " <Type>MIDI</Type>\n"
        " <Channel Number=\"7\"><Name>Ch7</Name><Type>Slider</Type></Channel>\n"
        " <Channel Number=\"8\"><Name>Ch8</Name><Type>Slider</Type></Channel>\n"
        "</InputProfile>\n");
    QCOMPARE(f.write(xml), xml.size());
    f.close();
    std::unique_ptr<QLCInputProfile> loaded(QLCInputProfile::loader(path));
    QVERIFY2(loaded != nullptr, "Profile fixture loader failed");
    const QString profileName = loaded->name();
    ioMap->loadProfiles(QDir(m_workspaceDir.path()));
    QVERIFY(ioMap->profileNames().contains(profileName));
    QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, profileName),
             "Failed to patch input with loaded profile");

    VCBridge::AuthoringBindingItem controllerItem;
    controllerItem.widgetID = controllerButtonId;
    controllerItem.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
    controllerItem.action = VCBridge::AuthoringBindingItem::Action::Append;
    controllerItem.controlSlotID = 0;
    VCBridge::AuthoringDestination destination;
    destination.inputUniverseID = 0;
    destination.inputChannel = 8;
    destination.profileName = profileName;
    controllerItem.destination = destination;

    VCBridge::AuthoringBindingItem feedbackItem;
    feedbackItem.widgetID = feedbackButtonId;
    feedbackItem.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
    feedbackItem.action = VCBridge::AuthoringBindingItem::Action::Set;
    feedbackItem.controlSlotID = 0;
    feedbackItem.sourceKey = VCBridge::AuthoringSourceKey{
        0,
        0,
        7,
        0
    };
    feedbackItem.idleValue = 10;
    feedbackItem.activeValue = 20;
    feedbackItem.monitorValue = 30;

    VCBridge::AuthoringBindingsRequest req;
    req.items.append(controllerItem);
    req.items.append(feedbackItem);

    const VCBridge::AuthoringPreparationResult result =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(result.code, QString());
    QCOMPARE(result.error, QString());
    QVERIFY(!result.preparationToken.isEmpty());
    QVERIFY(result.expiresInMs > 0);

    bool sawControllerProfileObs = false;
    bool sawFeedbackPatchObs = false;
    bool sawFeedbackNeedsUpdateObs = false;
    bool anyMetFalse = false;
    for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
    {
        if (!obs.met)
            anyMetFalse = true;
        if (obs.name == QStringLiteral("profileRoutingReady") &&
            obs.scope.contains(QStringLiteral("widget:%1").arg(controllerButtonId)))
        {
            sawControllerProfileObs = true;
            QCOMPARE(obs.met, true);
        }
        if (obs.name == QStringLiteral("feedbackPatchIdle") &&
            obs.scope.contains(QStringLiteral("widget:%1").arg(feedbackButtonId)))
        {
            sawFeedbackPatchObs = true;
            QCOMPARE(obs.met, true);
        }
        if (obs.name == QStringLiteral("feedbackNeedsUpdateIdle") &&
            obs.scope.contains(QStringLiteral("widget:%1").arg(feedbackButtonId)))
        {
            sawFeedbackNeedsUpdateObs = true;
            QCOMPARE(obs.met, true);
        }
        QVERIFY2(!(obs.scope.contains(QStringLiteral("widget:%1").arg(controllerButtonId)) &&
                   (obs.name == QStringLiteral("relativeSources") ||
                    obs.name == QStringLiteral("feedbackPatchIdle") ||
                    obs.name == QStringLiteral("feedbackNeedsUpdateIdle"))),
                 "Controller append item leaked source/feedback observations");
        QVERIFY2(!(obs.scope.contains(QStringLiteral("widget:%1").arg(feedbackButtonId)) &&
                   (obs.name == QStringLiteral("profileRoutingReady") ||
                    obs.name == QStringLiteral("relativeSources"))),
                 "Feedback item leaked controller profile/relative observations");
    }
    QVERIFY(!anyMetFalse);
    QVERIFY2(sawControllerProfileObs, "Missing controller profileRoutingReady observation for controller widget");
    QVERIFY2(sawFeedbackPatchObs, "Missing feedbackPatchIdle observation for feedback widget");
    QVERIFY2(sawFeedbackNeedsUpdateObs, "Missing feedbackNeedsUpdateIdle observation for feedback widget");
    const VCBridge::AuthoringApplyResult mixedApply =
        m_bridge->applyAuthoring(result.preparationToken);
    QCOMPARE(mixedApply.code, QString());
    QCOMPARE(mixedApply.tokenConsumed, true);
    QCOMPARE(feedbackSource->feedbackValue(QLCInputFeedback::LowerValue), uchar(10));
    QCOMPARE(feedbackSource->feedbackValue(QLCInputFeedback::UpperValue), uchar(20));
    QCOMPARE(feedbackSource->feedbackValue(QLCInputFeedback::MonitorValue), uchar(30));

    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("MixedFunctionControllerScene"));
    QVERIFY(doc->addFunction(scene));
    const quint32 sceneId = scene->id();
    auto cleanupScene = qScopeGuard([&]() { doc->deleteFunction(sceneId); });

    VCBridge::AuthoringBindingItem functionItem;
    functionItem.widgetID = controllerButtonId;
    functionItem.kind = VCBridge::AuthoringBindingItem::Kind::Function;
    functionItem.action = VCBridge::AuthoringBindingItem::Action::Set;
    functionItem.functionID = sceneId;

    VCBridge::AuthoringBindingItem controllerAllowedItem;
    controllerAllowedItem.widgetID = feedbackButtonId;
    controllerAllowedItem.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
    controllerAllowedItem.action = VCBridge::AuthoringBindingItem::Action::Append;
    controllerAllowedItem.controlSlotID = 0;
    VCBridge::AuthoringDestination controllerAllowedDestination;
    controllerAllowedDestination.inputUniverseID = 0;
    controllerAllowedDestination.inputChannel = 7;
    controllerAllowedDestination.profileName = profileName;
    controllerAllowedItem.destination = controllerAllowedDestination;

    VCBridge::AuthoringBindingsRequest mixedAllowedReq;
    mixedAllowedReq.items.append(functionItem);
    mixedAllowedReq.items.append(controllerAllowedItem);
    const VCBridge::AuthoringPreparationResult mixedAllowedPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(mixedAllowedReq));
    QCOMPARE(mixedAllowedPrep.code, QString());
    QCOMPARE(mixedAllowedPrep.atomicScope, VCBridge::AuthoringPreparationResult::AtomicScope::IndependentItems);
    QVERIFY(!mixedAllowedPrep.preparationToken.isEmpty());
    const VCBridge::AuthoringApplyResult mixedAllowedApply =
        m_bridge->applyAuthoring(mixedAllowedPrep.preparationToken);
    QCOMPARE(mixedAllowedApply.code, QString());
    QCOMPARE(mixedAllowedApply.tokenConsumed, true);
    QCOMPARE(controllerButton->functionID(), sceneId);

    QSharedPointer<QLCInputSource> functionSource(new QLCInputSource(0, 9));
    functionSource->setID(0);
    functionSource->setWorkingMode(QLCInputSource::Absolute);
    controllerButton->addInputSource(functionSource);

    QVERIFY2(ioMap->setOutputPatch(0, m_probeName, "", "", 0, true),
             "Failed to patch feedback output for mixed function/controller test");

    VCBridge::AuthoringBindingsRequest mixedBlockedReq;
    mixedBlockedReq.items.append(functionItem);
    mixedBlockedReq.items.append(controllerAllowedItem);
    const VCBridge::AuthoringPreparationResult mixedBlockedPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(mixedBlockedReq));
    QCOMPARE(mixedBlockedPrep.atomicScope, VCBridge::AuthoringPreparationResult::AtomicScope::IndependentItems);
    QCOMPARE(mixedBlockedPrep.code, QStringLiteral("authoringPrecondition"));
    QVERIFY(mixedBlockedPrep.preparationToken.isEmpty());
    QVERIFY(mixedBlockedPrep.error.contains(QStringLiteral("Feedback patch"), Qt::CaseInsensitive));
}

void VCAuthoringOwner_Test::authoringPassB_assignmentA_inputValidationAndCodes_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("expectedCode");
    QTest::addColumn<QString>("expectedErrorPart");

    QTest::newRow("controller-set-rejected")
        << QStringLiteral("controller-set-rejected")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Controller action is not supported");
    QTest::newRow("controller-clear-rejected")
        << QStringLiteral("controller-clear-rejected")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Controller action is not supported");
    QTest::newRow("controller-move-missing-sourcekey")
        << QStringLiteral("controller-move-missing-sourcekey")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Source key is required");
    QTest::newRow("controller-move-missing-destination")
        << QStringLiteral("controller-move-missing-destination")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Controller destination requires");
    QTest::newRow("controller-remove-unresolved-source")
        << QStringLiteral("controller-remove-unresolved-source")
        << QStringLiteral("ambiguousSource")
        << QStringLiteral("ambiguous or unresolved");
    QTest::newRow("controller-remove-duplicate-native-pair")
        << QStringLiteral("controller-remove-duplicate-native-pair")
        << QStringLiteral("ambiguousSource")
        << QStringLiteral("duplicate native universe/channel pair");
    QTest::newRow("binding-missing-target")
        << QStringLiteral("binding-missing-target")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Binding target widget ID");
    QTest::newRow("function-set-missing-id")
        << QStringLiteral("function-set-missing-id")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Function set requires");
    QTest::newRow("function-set-invalid-id")
        << QStringLiteral("function-set-invalid-id")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Function set requires");
    QTest::newRow("function-set-nonexistent-id")
        << QStringLiteral("function-set-nonexistent-id")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("unknown function");
    QTest::newRow("binding-invalid-diffdetail")
        << QStringLiteral("binding-invalid-diffdetail")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("diffDetail");
    QTest::newRow("layout-reorder-destination-wrong-class")
        << QStringLiteral("layout-reorder-destination-wrong-class")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("destinationFrameWidgetID must reference a frame or page");
    QTest::newRow("layout-movegroup-missing-destination")
        << QStringLiteral("layout-movegroup-missing-destination")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("destinationFrameWidgetID is required");
    QTest::newRow("layout-align-page-wrong-class")
        << QStringLiteral("layout-align-page-wrong-class")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("pageWidgetID must reference a page");
    QTest::newRow("layout-page-rename-wrong-class")
        << QStringLiteral("layout-page-rename-wrong-class")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("pageWidgetID must reference a page");
    QTest::newRow("layout-page-delete-missing")
        << QStringLiteral("layout-page-delete-missing")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("pageWidgetID is required");
    QTest::newRow("layout-frame-pages-wrong-class")
        << QStringLiteral("layout-frame-pages-wrong-class")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("frameWidgetID must reference a frame");
    QTest::newRow("recordpanel-wrong-class")
        << QStringLiteral("recordpanel-wrong-class")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("invalid role");
    QTest::newRow("layout-reorder-mixed-valid-invalid-root")
        << QStringLiteral("layout-reorder-mixed-valid-invalid-root")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("unknown widget ID");
}

void VCAuthoringOwner_Test::authoringPassB_assignmentA_inputValidationAndCodes()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);
    QFETCH(QString, expectedCode);
    QFETCH(QString, expectedErrorPart);
    bool bindingScenario = false;

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 700, 400));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 100, 80));

    VCButton *otherButton = new VCButton(doc, frame);
    vc->addWidgetToMap(otherButton);
    frame->addWidgetToPageMap(otherButton);
    otherButton->setLogicalGeometry(QRectF(180, 40, 100, 80));

    VCRecordPanel *panel = new VCRecordPanel(doc, frame);
    vc->addWidgetToMap(panel);
    frame->addWidgetToPageMap(panel);
    panel->setLogicalGeometry(QRectF(340, 40, 150, 120));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    const quint32 otherButtonId = otherButton->id();
    const quint32 panelId = panel->id();
    auto cleanup = qScopeGuard([&]() {
        vc->deleteVCWidgets({buttonId, otherButtonId, panelId, frameId});
    });

    VCBridge::AuthoringRequest request = VCBridge::AuthoringRequest(VCBridge::AuthoringLayoutRequest{});
    if (scenario == QStringLiteral("controller-set-rejected") ||
        scenario == QStringLiteral("controller-clear-rejected") ||
        scenario == QStringLiteral("controller-move-missing-sourcekey") ||
        scenario == QStringLiteral("controller-move-missing-destination") ||
        scenario == QStringLiteral("controller-remove-unresolved-source") ||
        scenario == QStringLiteral("controller-remove-duplicate-native-pair") ||
        scenario == QStringLiteral("binding-missing-target") ||
        scenario == QStringLiteral("function-set-missing-id") ||
        scenario == QStringLiteral("function-set-invalid-id") ||
        scenario == QStringLiteral("function-set-nonexistent-id") ||
        scenario == QStringLiteral("binding-invalid-diffdetail"))
    {
        VCBridge::AuthoringBindingsRequest req;
        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Set;
        item.controlSlotID = 0;

        if (scenario == QStringLiteral("controller-clear-rejected"))
        {
            item.action = VCBridge::AuthoringBindingItem::Action::Clear;
        }
        else if (scenario == QStringLiteral("controller-move-missing-sourcekey"))
        {
            item.action = VCBridge::AuthoringBindingItem::Action::Move;
            VCBridge::AuthoringDestination destination;
            destination.inputUniverseID = 0;
            destination.inputChannel = 7;
            destination.profileName = QStringLiteral("x");
            item.destination = destination;
        }
        else if (scenario == QStringLiteral("controller-move-missing-destination"))
        {
            item.action = VCBridge::AuthoringBindingItem::Action::Move;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
            QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
            source->setID(0);
            source->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(source);
        }
        else if (scenario == QStringLiteral("controller-remove-unresolved-source"))
        {
            item.action = VCBridge::AuthoringBindingItem::Action::Remove;
            QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
            source->setID(0);
            source->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(source);
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 9, 0};
        }
        else if (scenario == QStringLiteral("controller-remove-duplicate-native-pair"))
        {
            item.action = VCBridge::AuthoringBindingItem::Action::Remove;
            QSharedPointer<QLCInputSource> sourceA(new QLCInputSource(0, 7));
            sourceA->setID(0);
            sourceA->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(sourceA);
            QSharedPointer<QLCInputSource> sourceB(new QLCInputSource(0, 7));
            sourceB->setID(1);
            sourceB->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(sourceB);
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
        }
        else if (scenario == QStringLiteral("binding-missing-target"))
        {
            item.widgetID = 0x7ffffff1u;
            item.action = VCBridge::AuthoringBindingItem::Action::Append;
            VCBridge::AuthoringDestination destination;
            destination.inputUniverseID = 0;
            destination.inputChannel = 7;
            destination.profileName = QStringLiteral("x");
            item.destination = destination;
        }
        else if (scenario == QStringLiteral("function-set-missing-id"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Function;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.functionID = std::nullopt;
        }
        else if (scenario == QStringLiteral("function-set-invalid-id"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Function;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.functionID = Function::invalidId();
        }
        else if (scenario == QStringLiteral("function-set-nonexistent-id"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Function;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.functionID = 0x7ffffff0u;
        }

        req.items.append(item);
        if (scenario == QStringLiteral("binding-invalid-diffdetail"))
            req.diffDetail = QStringLiteral("not-valid");
        request = VCBridge::AuthoringRequest(req);
        bindingScenario = true;
    }
    else if (scenario == QStringLiteral("recordpanel-wrong-class"))
    {
        VCBridge::AuthoringRecordPanelRequest req;
        VCBridge::AuthoringRecordPanelItem item;
        item.widgetID = buttonId;
        req.items.append(item);
        request = VCBridge::AuthoringRequest(req);
    }
    else
    {
        VCBridge::AuthoringLayoutRequest req;
        if (scenario == QStringLiteral("layout-reorder-destination-wrong-class"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::Reorder;
            req.widgetIDs.append(buttonId);
            req.destinationFrameWidgetID = buttonId;
            req.destinationSubpage = 0;
        }
        else if (scenario == QStringLiteral("layout-movegroup-missing-destination"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::MoveGroup;
            req.widgetIDs.append(buttonId);
        }
        else if (scenario == QStringLiteral("layout-align-page-wrong-class"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::Align;
            req.pageWidgetID = frameId;
            req.columns = 2;
        }
        else if (scenario == QStringLiteral("layout-page-rename-wrong-class"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::PageRename;
            req.pageWidgetID = frameId;
            req.name = QStringLiteral("renamed");
        }
        else if (scenario == QStringLiteral("layout-page-delete-missing"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::PageDelete;
            req.pageWidgetID = std::nullopt;
        }
        else if (scenario == QStringLiteral("layout-frame-pages-wrong-class"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::FramePages;
            req.frameWidgetID = buttonId;
            req.totalPages = 2;
        }
        else if (scenario == QStringLiteral("layout-reorder-mixed-valid-invalid-root"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::Reorder;
            req.widgetIDs.append(buttonId);
            req.widgetIDs.append(0x7ffffff0u);
            req.destinationFrameWidgetID = frameId;
            req.destinationSubpage = 0;
        }
        else
        {
            QFAIL("Unexpected scenario");
        }
        request = VCBridge::AuthoringRequest(req);
    }

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    const QRectF buttonGeometryBefore = button->logicalGeometry();
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    const VCBridge::AuthoringPreparationResult result = m_bridge->prepareAuthoring(request);
    QCOMPARE(result.code, expectedCode);
    if (bindingScenario)
        QCOMPARE(result.atomicScope, VCBridge::AuthoringPreparationResult::AtomicScope::IndependentItems);
    QVERIFY(result.error.contains(expectedErrorPart, Qt::CaseInsensitive));
    QVERIFY(result.preparationToken.isEmpty());
    QCOMPARE(result.expiresInMs, 0);
    QVERIFY2(result.preconditions.isEmpty(), "Input validation must fail before precondition observations");
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(button->logicalGeometry(), buttonGeometryBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
}

void VCAuthoringOwner_Test::authoringPassB_assignmentA2_inputValidation_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("expectedCode");
    QTest::addColumn<QString>("expectedErrorPart");

    QTest::newRow("partial-sourcekey-remove")
        << QStringLiteral("partial-sourcekey-remove")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Source key requires");
    QTest::newRow("partial-sourcekey-move")
        << QStringLiteral("partial-sourcekey-move")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Source key requires");
    QTest::newRow("partial-sourcekey-feedback")
        << QStringLiteral("partial-sourcekey-feedback")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Source key requires");
    QTest::newRow("invalid-pair-function-append")
        << QStringLiteral("invalid-pair-function-append")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Function action is not supported");
    QTest::newRow("invalid-pair-key-clear")
        << QStringLiteral("invalid-pair-key-clear")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Key action is not supported");
    QTest::newRow("invalid-pair-feedback-remove")
        << QStringLiteral("invalid-pair-feedback-remove")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Feedback action is not supported");
    QTest::newRow("function-set-nonbutton")
        << QStringLiteral("function-set-nonbutton")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Function binding requires a VCButton target");
    QTest::newRow("function-clear-nonbutton")
        << QStringLiteral("function-clear-nonbutton")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Function binding requires a VCButton target");
    QTest::newRow("controller-append-missing-controlslot")
        << QStringLiteral("controller-append-missing-controlslot")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("controlSlotID is required");
    QTest::newRow("key-set-missing-controlslot")
        << QStringLiteral("key-set-missing-controlslot")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("controlSlotID is required");
    QTest::newRow("key-remove-missing-keysequence")
        << QStringLiteral("key-remove-missing-keysequence")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Key remove requires keySequence");
    QTest::newRow("feedback-missing-groups")
        << QStringLiteral("feedback-missing-groups")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Feedback requires at least one complete values or channels group");
    QTest::newRow("feedback-partial-values-group")
        << QStringLiteral("feedback-partial-values-group")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Feedback values group must include idleValue, activeValue and monitorValue");
    QTest::newRow("feedback-partial-channels-group")
        << QStringLiteral("feedback-partial-channels-group")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Feedback channels group must include idleChannel, activeChannel and monitorChannel");
    QTest::newRow("feedback-values-out-of-range")
        << QStringLiteral("feedback-values-out-of-range")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Feedback values must be in range 0..255");
    QTest::newRow("feedback-channels-out-of-range")
        << QStringLiteral("feedback-channels-out-of-range")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Feedback channels must be in range -1..15");
    QTest::newRow("layout-reorder-missing-destination-subpage")
        << QStringLiteral("layout-reorder-missing-destination-subpage")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("destinationSubpage is required");
    QTest::newRow("layout-movegroup-missing-anchor-widget")
        << QStringLiteral("layout-movegroup-missing-anchor-widget")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("anchorWidgetID is required");
    QTest::newRow("layout-movegroup-missing-logical-anchor")
        << QStringLiteral("layout-movegroup-missing-logical-anchor")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("logicalAnchor is required");
    QTest::newRow("layout-page-rename-missing-name")
        << QStringLiteral("layout-page-rename-missing-name")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("name is required");
    QTest::newRow("layout-page-create-missing-name")
        << QStringLiteral("layout-page-create-missing-name")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("name is required");
    QTest::newRow("layout-frame-pages-missing-total-pages")
        << QStringLiteral("layout-frame-pages-missing-total-pages")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("totalPages is required");
    QTest::newRow("layout-frame-pages-missing-page-labels")
        << QStringLiteral("layout-frame-pages-missing-page-labels")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("pageLabels is required");
    QTest::newRow("layout-frame-pages-missing-pages-loop")
        << QStringLiteral("layout-frame-pages-missing-pages-loop")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("pagesLoop is required");
    QTest::newRow("layout-align-missing-columns")
        << QStringLiteral("layout-align-missing-columns")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("columns is required");
    QTest::newRow("layout-align-missing-padding")
        << QStringLiteral("layout-align-missing-padding")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("padding is required");
    QTest::newRow("layout-align-page-as-frame")
        << QStringLiteral("layout-align-page-as-frame")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("frameWidgetID must reference a frame");
    QTest::newRow("layout-framepages-page-as-frame")
        << QStringLiteral("layout-framepages-page-as-frame")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("frameWidgetID must reference a frame");
    QTest::newRow("binding-late-invalid-second-item")
        << QStringLiteral("binding-late-invalid-second-item")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Binding target widget ID");
    QTest::newRow("recordpanel-late-invalid-second-item")
        << QStringLiteral("recordpanel-late-invalid-second-item")
        << QStringLiteral("invalidArgument")
        << QStringLiteral("Record panel target widget ID");
}

void VCAuthoringOwner_Test::authoringPassB_assignmentA2_inputValidation()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);
    QFETCH(QString, expectedCode);
    QFETCH(QString, expectedErrorPart);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(frame);
    page0->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 700, 400));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 100, 80));

    VCSlider *slider = new VCSlider(doc, frame);
    vc->addWidgetToMap(slider);
    frame->addWidgetToPageMap(slider);
    slider->setSliderMode(VCSlider::Level);
    slider->setLogicalGeometry(QRectF(160, 40, 120, 150));

    VCRecordPanel *panel = new VCRecordPanel(doc, frame);
    vc->addWidgetToMap(panel);
    frame->addWidgetToPageMap(panel);
    panel->setLogicalGeometry(QRectF(320, 40, 160, 120));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    const quint32 sliderId = slider->id();
    const quint32 panelId = panel->id();
    const quint32 page0Id = page0->id();
    std::optional<quint32> createdFunctionId;
    auto cleanup = qScopeGuard([&]() {
        ioMap->setInputPatch(0, KInputNone, "", "", QLCIOPlugin::invalidLine(), KInputNone);
        if (createdFunctionId.has_value())
            doc->deleteFunction(*createdFunctionId);
        vc->deleteVCWidgets({buttonId, sliderId, panelId, frameId});
    });

    auto writeAbsoluteProfile = [&](const QString &model, QString &profileName) -> bool {
        const QString path = m_workspaceDir.filePath(QStringLiteral("assignment-a2-%1%2")
            .arg(model, KExtInputProfile));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qWarning() << "Failed to create profile fixture at" << path;
            return false;
        }
        const QByteArray xml = QByteArrayLiteral(
            "<?xml version='1.0' encoding='UTF-8'?>\n"
            "<!DOCTYPE InputProfile>\n"
            "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
            " <Creator><Name>PassB</Name><Version>1</Version><Author>test</Author></Creator>\n"
            " <Manufacturer>PBMCP</Manufacturer>\n"
            " <Model>MODEL_TOKEN</Model>\n"
            " <Type>MIDI</Type>\n"
            " <Channel Number=\"7\"><Name>Ch7</Name><Type>Slider</Type></Channel>\n"
            "</InputProfile>\n");
        QByteArray updated = xml;
        updated.replace("MODEL_TOKEN", model.toUtf8());
        if (f.write(updated) != updated.size())
        {
            qWarning() << "Failed to write profile fixture";
            return false;
        }
        f.close();
        std::unique_ptr<QLCInputProfile> loaded(QLCInputProfile::loader(path));
        if (!loaded)
        {
            qWarning() << "Profile fixture loader failed";
            return false;
        }
        profileName = loaded->name();
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        if (!ioMap->profileNames().contains(profileName))
        {
            qWarning() << "Profile did not load into ioMap";
            return false;
        }
        QLCInputProfile *profile = ioMap->profile(profileName);
        if (!profile || !profile->channel(7))
        {
            qWarning() << "Loaded profile missing required absolute channel";
            return false;
        }
        if (profile->channel(7)->movementType() != QLCInputChannel::Absolute)
        {
            qWarning() << "Loaded profile channel movement type mismatch";
            return false;
        }
        if (!ioMap->setInputPatch(0, m_probeName, "", "", 0, profileName))
        {
            qWarning() << "Failed to patch input with loaded profile";
            return false;
        }
        return true;
    };

    VCBridge::AuthoringRequest request = VCBridge::AuthoringRequest(VCBridge::AuthoringLayoutRequest{});
    Scene *scene = nullptr;

    if (scenario.startsWith(QStringLiteral("layout-")))
    {
        VCBridge::AuthoringLayoutRequest req;
        if (scenario == QStringLiteral("layout-reorder-missing-destination-subpage"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::Reorder;
            req.widgetIDs.append(buttonId);
            req.destinationFrameWidgetID = frameId;
        }
        else if (scenario == QStringLiteral("layout-movegroup-missing-anchor-widget"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::MoveGroup;
            req.widgetIDs.append(buttonId);
            req.destinationFrameWidgetID = frameId;
            req.destinationSubpage = 0;
            req.logicalAnchor = QPointF(20, 40);
        }
        else if (scenario == QStringLiteral("layout-movegroup-missing-logical-anchor"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::MoveGroup;
            req.widgetIDs.append(buttonId);
            req.destinationFrameWidgetID = frameId;
            req.destinationSubpage = 0;
            req.anchorWidgetID = buttonId;
        }
        else if (scenario == QStringLiteral("layout-page-rename-missing-name"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::PageRename;
            req.pageWidgetID = page0Id;
        }
        else if (scenario == QStringLiteral("layout-page-create-missing-name"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::PageCreate;
        }
        else if (scenario == QStringLiteral("layout-frame-pages-missing-total-pages"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::FramePages;
            req.frameWidgetID = frameId;
            req.pageLabels = QStringList() << QStringLiteral("P0");
            req.pagesLoop = false;
        }
        else if (scenario == QStringLiteral("layout-frame-pages-missing-page-labels"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::FramePages;
            req.frameWidgetID = frameId;
            req.totalPages = 1;
            req.pagesLoop = false;
        }
        else if (scenario == QStringLiteral("layout-frame-pages-missing-pages-loop"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::FramePages;
            req.frameWidgetID = frameId;
            req.totalPages = 1;
            req.pageLabels = QStringList() << QStringLiteral("P0");
        }
        else if (scenario == QStringLiteral("layout-align-missing-columns"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::Align;
            req.frameWidgetID = frameId;
            req.padding = 6;
        }
        else if (scenario == QStringLiteral("layout-align-missing-padding"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::Align;
            req.frameWidgetID = frameId;
            req.columns = 2;
        }
        else if (scenario == QStringLiteral("layout-align-page-as-frame"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::Align;
            req.frameWidgetID = page0Id;
            req.columns = 2;
            req.padding = 6;
        }
        else if (scenario == QStringLiteral("layout-framepages-page-as-frame"))
        {
            req.operation = VCBridge::AuthoringLayoutRequest::Operation::FramePages;
            req.frameWidgetID = page0Id;
            req.totalPages = 2;
            req.pageLabels = QStringList() << QStringLiteral("P0") << QStringLiteral("P1");
            req.pagesLoop = false;
        }
        else
        {
            QFAIL("Unexpected layout scenario");
        }
        request = VCBridge::AuthoringRequest(req);
    }
    else if (scenario.startsWith(QStringLiteral("recordpanel-")))
    {
        VCBridge::AuthoringRecordPanelRequest req;
        VCBridge::AuthoringRecordPanelItem valid;
        valid.widgetID = panelId;
        valid.targetFolder = QStringLiteral("D:/captures");
        req.items.append(valid);
        VCBridge::AuthoringRecordPanelItem invalid;
        invalid.widgetID = buttonId;
        req.items.append(invalid);
        request = VCBridge::AuthoringRequest(req);
    }
    else
    {
        VCBridge::AuthoringBindingsRequest req;
        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.controlSlotID = 0;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 7;
        dst.profileName = QStringLiteral("unused");
        item.destination = dst;

        if (scenario == QStringLiteral("partial-sourcekey-remove"))
        {
            QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
            source->setID(0);
            source->setPage(0);
            source->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(source);
            item.action = VCBridge::AuthoringBindingItem::Action::Remove;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, std::nullopt};
        }
        else if (scenario == QStringLiteral("partial-sourcekey-move"))
        {
            QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
            source->setID(0);
            source->setPage(0);
            source->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(source);
            QString profileName;
            QVERIFY(writeAbsoluteProfile(QStringLiteral("partial-move"), profileName));
            item.action = VCBridge::AuthoringBindingItem::Action::Move;
            item.destination->profileName = profileName;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, std::nullopt, 0};
        }
        else if (scenario == QStringLiteral("partial-sourcekey-feedback"))
        {
            QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
            source->setID(0);
            source->setPage(0);
            source->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(source);
            item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.sourceKey = VCBridge::AuthoringSourceKey{std::nullopt, 0, 7, 0};
            item.idleValue = 10;
            item.activeValue = 20;
            item.monitorValue = 30;
        }
        else if (scenario == QStringLiteral("invalid-pair-function-append"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Function;
            item.action = VCBridge::AuthoringBindingItem::Action::Append;
        }
        else if (scenario == QStringLiteral("invalid-pair-key-clear"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Key;
            item.action = VCBridge::AuthoringBindingItem::Action::Clear;
        }
        else if (scenario == QStringLiteral("invalid-pair-feedback-remove"))
        {
            QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
            source->setID(0);
            source->setPage(0);
            source->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(source);
            item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
            item.action = VCBridge::AuthoringBindingItem::Action::Remove;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
            item.idleValue = 10;
            item.activeValue = 20;
            item.monitorValue = 30;
        }
        else if (scenario == QStringLiteral("function-set-nonbutton") ||
                 scenario == QStringLiteral("function-clear-nonbutton"))
        {
            scene = new Scene(doc);
            scene->setName(QStringLiteral("A2Scene"));
            QVERIFY(doc->addFunction(scene));
            createdFunctionId = scene->id();
            item.widgetID = sliderId;
            item.kind = VCBridge::AuthoringBindingItem::Kind::Function;
            item.action = (scenario == QStringLiteral("function-set-nonbutton"))
                ? VCBridge::AuthoringBindingItem::Action::Set
                : VCBridge::AuthoringBindingItem::Action::Clear;
            item.functionID = scene->id();
        }
        else if (scenario == QStringLiteral("controller-append-missing-controlslot"))
        {
            QString profileName;
            QVERIFY(writeAbsoluteProfile(QStringLiteral("missing-control"), profileName));
            item.controlSlotID = std::nullopt;
            item.destination->profileName = profileName;
        }
        else if (scenario == QStringLiteral("key-set-missing-controlslot"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Key;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.controlSlotID = std::nullopt;
            item.keySequence = QStringLiteral("Ctrl+K");
        }
        else if (scenario == QStringLiteral("key-remove-missing-keysequence"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Key;
            item.action = VCBridge::AuthoringBindingItem::Action::Remove;
        }
        else if (scenario == QStringLiteral("feedback-missing-groups"))
        {
            QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
            source->setID(0);
            source->setPage(0);
            source->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(source);
            item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
        }
        else if (scenario == QStringLiteral("feedback-partial-values-group"))
        {
            QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
            source->setID(0);
            source->setPage(0);
            source->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(source);
            item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
            item.idleValue = 10;
            item.activeValue = 20;
        }
        else if (scenario == QStringLiteral("feedback-partial-channels-group"))
        {
            QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
            source->setID(0);
            source->setPage(0);
            source->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(source);
            item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
            item.idleChannel = 0;
            item.activeChannel = 1;
        }
        else if (scenario == QStringLiteral("feedback-values-out-of-range"))
        {
            QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
            source->setID(0);
            source->setPage(0);
            source->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(source);
            item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
            item.idleValue = 10;
            item.activeValue = 20;
            item.monitorValue = 300;
        }
        else if (scenario == QStringLiteral("feedback-channels-out-of-range"))
        {
            QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
            source->setID(0);
            source->setPage(0);
            source->setWorkingMode(QLCInputSource::Absolute);
            button->addInputSource(source);
            item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
            item.idleChannel = -1;
            item.activeChannel = 0;
            item.monitorChannel = 16;
        }
        else if (scenario == QStringLiteral("binding-late-invalid-second-item"))
        {
            scene = new Scene(doc);
            scene->setName(QStringLiteral("A2SceneLate"));
            QVERIFY(doc->addFunction(scene));
            createdFunctionId = scene->id();
            item.kind = VCBridge::AuthoringBindingItem::Kind::Function;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.functionID = scene->id();
            VCBridge::AuthoringBindingItem invalid = item;
            invalid.widgetID = 0x7ffffff0u;
            req.items.append(item);
            req.items.append(invalid);
            request = VCBridge::AuthoringRequest(req);
            const QByteArray xmlBefore = serializeConsole();
            const bool dirtyBefore = doc->isModified();
            resetProbeCounters();
            const int baselineOpen = counter("openCount");
            const int baselineClose = counter("closeCount");
            const int baselineWrite = counter("writeCount");
            const int baselineFeedback = counter("feedbackCount");

            const VCBridge::AuthoringPreparationResult result = m_bridge->prepareAuthoring(request);
            QCOMPARE(result.code, expectedCode);
            QVERIFY(result.error.contains(expectedErrorPart, Qt::CaseInsensitive));
            QVERIFY(result.preparationToken.isEmpty());
            QCOMPARE(result.expiresInMs, 0);
            QVERIFY2(result.preconditions.isEmpty(), "Input validation must fail before precondition observations");
            QCOMPARE(serializeConsole(), xmlBefore);
            QCOMPARE(doc->isModified(), dirtyBefore);
            QCOMPARE(counter("openCount"), baselineOpen);
            QCOMPARE(counter("closeCount"), baselineClose);
            QCOMPARE(counter("writeCount"), baselineWrite);
            QCOMPARE(counter("feedbackCount"), baselineFeedback);
            return;
        }
        else
        {
            QFAIL("Unexpected binding scenario");
        }

        req.items.append(item);
        request = VCBridge::AuthoringRequest(req);
    }

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    const QRectF buttonGeometryBefore = button->logicalGeometry();
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    const VCBridge::AuthoringPreparationResult result = m_bridge->prepareAuthoring(request);
    QCOMPARE(result.code, expectedCode);
    QVERIFY(result.error.contains(expectedErrorPart, Qt::CaseInsensitive));
    QVERIFY(result.preparationToken.isEmpty());
    QCOMPARE(result.expiresInMs, 0);
    QVERIFY2(result.preconditions.isEmpty(), "Input validation must fail before precondition observations");
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(button->logicalGeometry(), buttonGeometryBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
}

void VCAuthoringOwner_Test::authoringPassB_assignmentA2_gateMetUnsupported_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("expectedObservation");

    QTest::newRow("function-set-button-valid")
        << QStringLiteral("function-set-button-valid")
        << QStringLiteral("feedbackPatchIdle");
    QTest::newRow("function-clear-button-valid")
        << QStringLiteral("function-clear-button-valid")
        << QStringLiteral("feedbackPatchIdle");
    QTest::newRow("key-set-valid")
        << QStringLiteral("key-set-valid")
        << QString();
    QTest::newRow("key-remove-valid")
        << QStringLiteral("key-remove-valid")
        << QString();
    QTest::newRow("layout-reorder-page-destination-valid")
        << QStringLiteral("layout-reorder-page-destination-valid")
        << QStringLiteral("relativeSources");
}

void VCAuthoringOwner_Test::authoringPassB_assignmentA2_gateMetUnsupported()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);
    QFETCH(QString, expectedObservation);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(frame);
    page0->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 700, 400));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 100, 80));

    VCSlider *slider = new VCSlider(doc, frame);
    vc->addWidgetToMap(slider);
    frame->addWidgetToPageMap(slider);
    slider->setSliderMode(VCSlider::Level);
    slider->setLogicalGeometry(QRectF(160, 40, 120, 150));

    VCButton *pageButton = new VCButton(doc, page0);
    vc->addWidgetToMap(pageButton);
    page0->addWidgetToPageMap(pageButton);
    pageButton->setLogicalGeometry(QRectF(20, 260, 100, 80));

    QSharedPointer<QLCInputSource> source(new QLCInputSource(0, 7));
    source->setID(0);
    source->setPage(0);
    source->setWorkingMode(QLCInputSource::Absolute);
    button->addInputSource(source);

    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("A2ValidScene"));
    QVERIFY(doc->addFunction(scene));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    const quint32 sliderId = slider->id();
    const quint32 pageButtonId = pageButton->id();
    const quint32 sceneId = scene->id();
    const quint32 page0Id = page0->id();
    auto cleanup = qScopeGuard([&]() {
        doc->deleteFunction(sceneId);
        vc->deleteVCWidgets({buttonId, sliderId, pageButtonId, frameId});
    });

    VCBridge::AuthoringRequest request = VCBridge::AuthoringRequest(VCBridge::AuthoringLayoutRequest{});
    if (scenario == QStringLiteral("function-set-button-valid") ||
        scenario == QStringLiteral("function-clear-button-valid") ||
        scenario == QStringLiteral("key-set-valid") ||
        scenario == QStringLiteral("key-remove-valid"))
    {
        VCBridge::AuthoringBindingsRequest req;
        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.controlSlotID = 0;
        if (scenario == QStringLiteral("function-set-button-valid"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Function;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.functionID = sceneId;
        }
        else if (scenario == QStringLiteral("function-clear-button-valid"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Function;
            item.action = VCBridge::AuthoringBindingItem::Action::Clear;
            item.functionID = sceneId;
        }
        else if (scenario == QStringLiteral("key-set-valid"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Key;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.keySequence = QStringLiteral("Ctrl+Shift+K");
        }
        else
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Key;
            item.action = VCBridge::AuthoringBindingItem::Action::Remove;
            item.keySequence = QStringLiteral("Ctrl+Shift+K");
        }
        req.items.append(item);
        request = VCBridge::AuthoringRequest(req);
    }
    else if (scenario == QStringLiteral("layout-reorder-page-destination-valid"))
    {
        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::Reorder;
        req.widgetIDs.append(pageButtonId);
        req.destinationFrameWidgetID = page0Id;
        req.destinationSubpage = 0;
        request = VCBridge::AuthoringRequest(req);
    }
    else
    {
        QFAIL("Unexpected scenario");
    }

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    const QRectF buttonGeometryBefore = button->logicalGeometry();
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    const VCBridge::AuthoringPreparationResult result = m_bridge->prepareAuthoring(request);
    QString pendingToken;
    const bool bindingNowSupported =
        (scenario == QStringLiteral("function-set-button-valid") ||
         scenario == QStringLiteral("function-clear-button-valid") ||
         scenario == QStringLiteral("key-set-valid") ||
         scenario == QStringLiteral("key-remove-valid"));
    if (bindingNowSupported)
    {
        QCOMPARE(result.code, QString());
        QCOMPARE(result.error, QString());
        QVERIFY(!result.preparationToken.isEmpty());
        QVERIFY(result.expiresInMs > 0);
        pendingToken = result.preparationToken;
    }
    else
    {
        QCOMPARE(result.code, QStringLiteral("unsupportedPreparation"));
        QVERIFY2(isGeometryOrBindingUnsupportedError(result.error),
                 qPrintable(QStringLiteral("Unexpected unsupported error: %1").arg(result.error)));
        QVERIFY(result.preparationToken.isEmpty());
        QCOMPARE(result.expiresInMs, 0);
    }
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(button->logicalGeometry(), buttonGeometryBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);

    if (!expectedObservation.isEmpty())
    {
        bool found = false;
        for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
        {
            if (obs.name == expectedObservation)
            {
                found = true;
                QCOMPARE(obs.met, true);
            }
            QVERIFY2(obs.met, qPrintable(QStringLiteral("Unexpected unmet observation %1").arg(obs.name)));
        }
        QVERIFY2(found, qPrintable(QStringLiteral("Missing expected observation %1").arg(expectedObservation)));
    }
    else
    {
        for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
            QVERIFY2(obs.met, qPrintable(QStringLiteral("Unexpected unmet observation %1").arg(obs.name)));
    }

    if (!pendingToken.isEmpty())
    {
        const VCBridge::AuthoringApplyResult applied = m_bridge->applyAuthoring(pendingToken);
        QCOMPARE(applied.code, QString());
        QCOMPARE(applied.tokenConsumed, true);
    }
}

void VCAuthoringOwner_Test::authoringPassB_assignmentB_feedbackClosure_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("expectedCode");
    QTest::addColumn<QString>("expectedObservation");
    QTest::addColumn<bool>("expectedMet");
    QTest::addColumn<QString>("expectedScopePart");

    QTest::newRow("own-second-source-feedback-patched")
        << QStringLiteral("own-second-source-feedback-patched")
        << QStringLiteral("authoringPrecondition")
        << QStringLiteral("feedbackPatchIdle")
        << false
        << QStringLiteral("universe:1");
    QTest::newRow("own-second-source-encoder-needs-update")
        << QStringLiteral("own-second-source-encoder-needs-update")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("feedbackNeedsUpdateIdle")
        << false
        << QStringLiteral("source:1/");
    QTest::newRow("active-child-feedback-patched")
        << QStringLiteral("active-child-feedback-patched")
        << QStringLiteral("authoringPrecondition")
        << QStringLiteral("feedbackPatchIdle")
        << false
        << QStringLiteral("universe:2");
    QTest::newRow("active-child-encoder-needs-update")
        << QStringLiteral("active-child-encoder-needs-update")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("feedbackNeedsUpdateIdle")
        << false
        << QStringLiteral("source:2/");
    QTest::newRow("inactive-child-hazard-ignored")
        << QStringLiteral("inactive-child-hazard-ignored")
        << QString()
        << QStringLiteral("feedbackNeedsUpdateIdle")
        << true
        << QStringLiteral("widget:");
    QTest::newRow("nested-active-grandchild-different-page-feedback")
        << QStringLiteral("nested-active-grandchild-different-page-feedback")
        << QStringLiteral("authoringPrecondition")
        << QStringLiteral("feedbackPatchIdle")
        << false
        << QStringLiteral("universe:3");
    QTest::newRow("nested-active-grandchild-different-page-needs-update")
        << QStringLiteral("nested-active-grandchild-different-page-needs-update")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("feedbackNeedsUpdateIdle")
        << false
        << QStringLiteral("source:3/");
    QTest::newRow("nested-inactive-grandchild-outer-page-ignored")
        << QStringLiteral("nested-inactive-grandchild-outer-page-ignored")
        << QString()
        << QStringLiteral("feedbackPatchIdle")
        << true
        << QStringLiteral("widget:");
    QTest::newRow("unrelated-ancestor-hazard-ignored")
        << QStringLiteral("unrelated-ancestor-hazard-ignored")
        << QString()
        << QStringLiteral("feedbackPatchIdle")
        << true
        << QStringLiteral("widget:");
}

void VCAuthoringOwner_Test::authoringPassB_assignmentB_feedbackClosure()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);
    QFETCH(QString, expectedCode);
    QFETCH(QString, expectedObservation);
    QFETCH(bool, expectedMet);
    QFETCH(QString, expectedScopePart);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    VCFrame *outer = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(outer);
    page->addWidgetToPageMap(outer);
    outer->setShowHeader(false);
    outer->setTotalPagesNumber(2);
    outer->setMultiPageMode(true);
    outer->setCurrentPage(0);
    outer->setLogicalGeometry(QRectF(0, 0, 800, 500));

    QSharedPointer<QLCInputSource> targetSource(new QLCInputSource(0, 7));
    targetSource->setID(0);
    targetSource->setPage(0);
    targetSource->setWorkingMode(QLCInputSource::Absolute);
    outer->addInputSource(targetSource);

    const quint32 outerId = outer->id();
    QVariantList createdWidgetIds{QVariant::fromValue(outerId)};
    auto cleanup = qScopeGuard([&]() {
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        ioMap->setOutputPatch(1, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        ioMap->setOutputPatch(2, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        ioMap->setOutputPatch(3, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        vc->deleteVCWidgets(createdWidgetIds);
    });

    auto addButtonWithSource = [&](VCFrame *parent, int pageIndex, quint32 universe, quint32 channel,
                                   QLCInputSource::WorkingMode mode, quint8 slotId) -> VCButton * {
        VCButton *button = new VCButton(doc, parent);
        vc->addWidgetToMap(button);
        parent->addWidgetToPageMap(button);
        button->setLogicalGeometry(QRectF(20 + 40 * createdWidgetIds.size(), 40, 80, 60));
        button->setPage(pageIndex);
        createdWidgetIds.append(QVariant::fromValue(button->id()));
        QSharedPointer<QLCInputSource> source(new QLCInputSource(universe, channel));
        source->setID(slotId);
        source->setPage(pageIndex);
        source->setWorkingMode(mode);
        button->addInputSource(source);
        return button;
    };

    auto addFrame = [&](VCFrame *parent, int pageIndex) -> VCFrame * {
        VCFrame *frame = new VCFrame(doc, vc, parent);
        vc->addWidgetToMap(frame);
        parent->addWidgetToPageMap(frame);
        frame->setShowHeader(false);
        frame->setTotalPagesNumber(2);
        frame->setMultiPageMode(true);
        frame->setLogicalGeometry(QRectF(220 + 50 * createdWidgetIds.size(), 40, 220, 180));
        frame->setPage(pageIndex);
        createdWidgetIds.append(QVariant::fromValue(frame->id()));
        return frame;
    };

    if (scenario == QStringLiteral("own-second-source-feedback-patched") ||
        scenario == QStringLiteral("own-second-source-encoder-needs-update"))
    {
        QSharedPointer<QLCInputSource> second(new QLCInputSource(1, 9));
        second->setID(1);
        second->setPage(0);
        if (scenario == QStringLiteral("own-second-source-encoder-needs-update"))
        {
            second->setWorkingMode(QLCInputSource::Encoder);
            QVERIFY(second->needsUpdate());
        }
        else
        {
            second->setWorkingMode(QLCInputSource::Absolute);
            QVERIFY2(ioMap->setOutputPatch(1, m_probeName, "", "", 0, true),
                     "Failed to patch feedback on universe 1");
        }
        outer->addInputSource(second);
    }
    else if (scenario == QStringLiteral("active-child-feedback-patched") ||
             scenario == QStringLiteral("active-child-encoder-needs-update"))
    {
        VCButton *activeChild = addButtonWithSource(
            outer, outer->currentPage(), 2, 11,
            scenario == QStringLiteral("active-child-encoder-needs-update")
                ? QLCInputSource::Encoder
                : QLCInputSource::Absolute,
            0);
        QVERIFY(activeChild->page() == outer->currentPage());
        if (scenario == QStringLiteral("active-child-feedback-patched"))
        {
            QVERIFY2(ioMap->setOutputPatch(2, m_probeName, "", "", 0, true),
                     "Failed to patch feedback on active child universe");
        }
        else
        {
            const QList<QSharedPointer<QLCInputSource>> childSources = activeChild->inputSources();
            QVERIFY(!childSources.isEmpty());
            QCOMPARE(childSources.first()->workingMode(), QLCInputSource::Encoder);
            QVERIFY(childSources.first()->needsUpdate());
        }
    }
    else if (scenario == QStringLiteral("inactive-child-hazard-ignored"))
    {
        VCButton *inactiveChild = addButtonWithSource(outer, 1, 2, 12, QLCInputSource::Encoder, 0);
        QVERIFY(inactiveChild->page() != outer->currentPage());
        QVERIFY2(ioMap->setOutputPatch(2, m_probeName, "", "", 0, true),
                 "Failed to patch feedback on inactive child universe");
        const QList<QSharedPointer<QLCInputSource>> childSources = inactiveChild->inputSources();
        QVERIFY(!childSources.isEmpty());
        QVERIFY(childSources.first()->needsUpdate());
    }
    else if (scenario == QStringLiteral("nested-active-grandchild-different-page-feedback") ||
             scenario == QStringLiteral("nested-active-grandchild-different-page-needs-update") ||
             scenario == QStringLiteral("nested-inactive-grandchild-outer-page-ignored"))
    {
        VCFrame *childFrame = addFrame(outer, outer->currentPage());
        childFrame->setCurrentPage(1);
        VCButton *grandchild = addButtonWithSource(
            childFrame,
            scenario == QStringLiteral("nested-inactive-grandchild-outer-page-ignored") ? 0 : 1,
            3, 13,
            scenario == QStringLiteral("nested-active-grandchild-different-page-needs-update") ||
                scenario == QStringLiteral("nested-inactive-grandchild-outer-page-ignored")
                ? QLCInputSource::Encoder
                : QLCInputSource::Absolute,
            0);

        if (scenario == QStringLiteral("nested-active-grandchild-different-page-feedback"))
        {
            childFrame->setCurrentPage(1);
            grandchild->setPage(childFrame->currentPage());
            QVERIFY(grandchild->page() == childFrame->currentPage());
            QVERIFY2(ioMap->setOutputPatch(3, m_probeName, "", "", 0, true),
                     "Failed to patch feedback on nested active grandchild universe");
        }
        else if (scenario == QStringLiteral("nested-active-grandchild-different-page-needs-update"))
        {
            childFrame->setCurrentPage(1);
            grandchild->setPage(childFrame->currentPage());
            QVERIFY(grandchild->page() == childFrame->currentPage());
            const QList<QSharedPointer<QLCInputSource>> sources = grandchild->inputSources();
            QVERIFY(!sources.isEmpty());
            QCOMPARE(sources.first()->workingMode(), QLCInputSource::Encoder);
            QVERIFY(sources.first()->needsUpdate());
        }
        else
        {
            childFrame->setCurrentPage(1);
            grandchild->setPage(0);
            QCOMPARE(childFrame->currentPage(), 1);
            QCOMPARE(grandchild->page(), 0);
            QVERIFY(grandchild->parent() == childFrame);
            QVERIFY(grandchild->page() != childFrame->currentPage());
            QVERIFY2(ioMap->setOutputPatch(3, m_probeName, "", "", 0, true),
                     "Failed to patch feedback on nested inactive grandchild universe");
            const QList<QSharedPointer<QLCInputSource>> sources = grandchild->inputSources();
            QVERIFY(!sources.isEmpty());
            QVERIFY(sources.first()->needsUpdate());
        }
    }
    else if (scenario == QStringLiteral("unrelated-ancestor-hazard-ignored"))
    {
        VCFrame *ancestor = new VCFrame(doc, vc, page);
        vc->addWidgetToMap(ancestor);
        page->addWidgetToPageMap(ancestor);
        ancestor->setShowHeader(false);
        ancestor->setTotalPagesNumber(1);
        ancestor->setCurrentPage(0);
        ancestor->setLogicalGeometry(QRectF(0, 260, 780, 220));
        createdWidgetIds.prepend(QVariant::fromValue(ancestor->id()));
        page->removeWidgetFromPageMap(outer);
        outer->setParent(ancestor);
        ancestor->addWidgetToPageMap(outer);
        outer->setPage(0);
        VCButton *sibling = addButtonWithSource(ancestor, 0, 3, 14, QLCInputSource::Absolute, 0);
        Q_UNUSED(sibling);
        QVERIFY2(ioMap->setOutputPatch(3, m_probeName, "", "", 0, true),
                 "Failed to patch feedback on ancestor sibling universe");
    }
    else
    {
        QFAIL("Unexpected scenario");
    }

    VCBridge::AuthoringBindingItem item;
    item.widgetID = outerId;
    item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
    item.action = VCBridge::AuthoringBindingItem::Action::Set;
    item.controlSlotID = 0;
    item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
    item.idleValue = 10;
    item.activeValue = 20;
    item.monitorValue = 30;
    VCBridge::AuthoringBindingsRequest req;
    req.items.append(item);

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    const QRectF outerBefore = outer->logicalGeometry();
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    const VCBridge::AuthoringPreparationResult result =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(result.code, expectedCode);
    if (expectedCode.isEmpty())
    {
        QCOMPARE(result.error, QString());
        QVERIFY(!result.preparationToken.isEmpty());
        QVERIFY(result.expiresInMs > 0);
    }
    else
    {
        QVERIFY(result.preparationToken.isEmpty());
        QCOMPARE(result.expiresInMs, 0);
    }
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(outer->logicalGeometry(), outerBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);

    bool foundExpected = false;
    for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
    {
        if (obs.name == expectedObservation)
        {
            foundExpected = true;
            QCOMPARE(obs.met, expectedMet);
            QVERIFY(obs.scope.contains(expectedScopePart));
        }
        QVERIFY2(obs.name != QStringLiteral("writerSafe"),
                 "Feedback request must not emit writerSafe observation");
    }
    QVERIFY2(foundExpected, qPrintable(QStringLiteral("Missing observation %1").arg(expectedObservation)));

    if (expectedCode.isEmpty())
    {
        const VCBridge::AuthoringApplyResult applied = m_bridge->applyAuthoring(result.preparationToken);
        QCOMPARE(applied.code, QString());
        QCOMPARE(applied.tokenConsumed, true);
        QCOMPARE(targetSource->feedbackValue(QLCInputFeedback::LowerValue), uchar(10));
        QCOMPARE(targetSource->feedbackValue(QLCInputFeedback::UpperValue), uchar(20));
        QCOMPARE(targetSource->feedbackValue(QLCInputFeedback::MonitorValue), uchar(30));
    }
}

void VCAuthoringOwner_Test::authoringPassB_assignmentB_writerSafeClosure_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QString>("expectedCode");
    QTest::addColumn<QString>("expectedObservation");
    QTest::addColumn<bool>("expectedMet");

    QTest::newRow("safe-cross-parent-unrelated-destination-level")
        << QStringLiteral("safe-cross-parent-unrelated-destination-level")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("writerSafe")
        << true;
    QTest::newRow("safe-no-driver-unrelated-destination-sibling-frame-level")
        << QStringLiteral("safe-no-driver-unrelated-destination-sibling-frame-level")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("writerSafe")
        << true;
    QTest::newRow("departing-submaster-affects-source-sibling-isolated")
        << QStringLiteral("departing-submaster-affects-source-sibling-isolated")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("writerSafe")
        << false;
    QTest::newRow("arrival-submaster-affects-destination-sibling-isolated")
        << QStringLiteral("arrival-submaster-affects-destination-sibling-isolated")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("writerSafe")
        << false;
    QTest::newRow("departing-submaster-affects-source-nested-sibling-frame")
        << QStringLiteral("departing-submaster-affects-source-nested-sibling-frame")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("writerSafe")
        << false;
    QTest::newRow("arrival-submaster-affects-destination-nested-sibling-frame")
        << QStringLiteral("arrival-submaster-affects-destination-nested-sibling-frame")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("writerSafe")
        << false;
    QTest::newRow("moved-subtree-active-button")
        << QStringLiteral("moved-subtree-active-button")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("writerSafe")
        << false;
    QTest::newRow("moved-subtree-adjust-slider")
        << QStringLiteral("moved-subtree-adjust-slider")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("writerSafe")
        << false;
    QTest::newRow("common-census-protected-destination-page")
        << QStringLiteral("common-census-protected-destination-page")
        << QStringLiteral("authoringPrecondition")
        << QStringLiteral("protectedPages")
        << false;
    QTest::newRow("common-census-destination-relative-source")
        << QStringLiteral("common-census-destination-relative-source")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("relativeSources")
        << false;
    QTest::newRow("common-census-destination-feedback-patched-source")
        << QStringLiteral("common-census-destination-feedback-patched-source")
        << QStringLiteral("authoringPrecondition")
        << QStringLiteral("feedbackPatchIdle")
        << false;
    QTest::newRow("common-census-destination-record-panel-capture")
        << QStringLiteral("common-census-destination-record-panel-capture")
        << QStringLiteral("authoringPrecondition")
        << QStringLiteral("recordPanelCapture")
        << false;
    QTest::newRow("safe-cross-parent-group-no-writers")
        << QStringLiteral("safe-cross-parent-group-no-writers")
        << QStringLiteral("unsupportedPreparation")
        << QStringLiteral("writerSafe")
        << true;
}

void VCAuthoringOwner_Test::authoringPassB_assignmentB_writerSafeClosure()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);
    QFETCH(QString, expectedCode);
    QFETCH(QString, expectedObservation);
    QFETCH(bool, expectedMet);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *sourcePage = vc->page(0);
    QVERIFY(sourcePage != nullptr);

    const bool destinationOnProtectedPage = (scenario == QStringLiteral("common-census-protected-destination-page"));
    int destinationPageIndex = 0;
    if (destinationOnProtectedPage)
    {
        destinationPageIndex = vc->pagesCount();
        vc->addPage(destinationPageIndex);
    }
    VCPage *destinationPage = vc->page(destinationPageIndex);
    QVERIFY(destinationPage != nullptr);
    if (destinationOnProtectedPage)
        destinationPage->setPIN(1234);

    VCFrame *sourceFrame = new VCFrame(doc, vc, sourcePage);
    vc->addWidgetToMap(sourceFrame);
    sourcePage->addWidgetToPageMap(sourceFrame);
    sourceFrame->setShowHeader(false);
    sourceFrame->setTotalPagesNumber(1);
    sourceFrame->setCurrentPage(0);
    sourceFrame->setLogicalGeometry(QRectF(0, 0, 360, 320));

    VCFrame *destinationFrame = new VCFrame(doc, vc, destinationPage);
    vc->addWidgetToMap(destinationFrame);
    destinationPage->addWidgetToPageMap(destinationFrame);
    destinationFrame->setShowHeader(false);
    destinationFrame->setTotalPagesNumber(1);
    destinationFrame->setCurrentPage(0);
    destinationFrame->setLogicalGeometry(QRectF(380, 0, 360, 320));

    VCButton *movedButton = new VCButton(doc, sourceFrame);
    vc->addWidgetToMap(movedButton);
    sourceFrame->addWidgetToPageMap(movedButton);
    movedButton->setLogicalGeometry(QRectF(20, 40, 90, 70));

    VCButton *movedButton2 = new VCButton(doc, sourceFrame);
    vc->addWidgetToMap(movedButton2);
    sourceFrame->addWidgetToPageMap(movedButton2);
    movedButton2->setLogicalGeometry(QRectF(120, 40, 90, 70));

    VCSlider *sourceSubmaster = new VCSlider(doc, sourceFrame);
    vc->addWidgetToMap(sourceSubmaster);
    sourceFrame->addWidgetToPageMap(sourceSubmaster);
    sourceSubmaster->setSliderMode(VCSlider::Submaster);
    sourceSubmaster->setValue(64);
    sourceSubmaster->setLogicalGeometry(QRectF(20, 150, 120, 120));

    VCSlider *sourceLevel = new VCSlider(doc, sourceFrame);
    vc->addWidgetToMap(sourceLevel);
    sourceFrame->addWidgetToPageMap(sourceLevel);
    sourceLevel->setSliderMode(VCSlider::Level);
    sourceLevel->setLogicalGeometry(QRectF(160, 150, 120, 120));

    VCSlider *destLevel = new VCSlider(doc, destinationFrame);
    vc->addWidgetToMap(destLevel);
    destinationFrame->addWidgetToPageMap(destLevel);
    destLevel->setSliderMode(VCSlider::Level);
    destLevel->setLogicalGeometry(QRectF(20, 150, 120, 120));

    VCFrame *movedFrame = nullptr;
    VCButton *movedActiveButton = nullptr;
    VCSlider *movedAdjustSlider = nullptr;
    VCRecordPanel *destinationPanel = nullptr;
    VCFrame *sourceSiblingFrame = nullptr;
    VCSlider *sourceNestedLevel = nullptr;
    VCFrame *destinationSiblingFrame = nullptr;
    VCSlider *destinationNestedLevel = nullptr;

    const quint32 sourceFrameId = sourceFrame->id();
    const quint32 destinationFrameId = destinationFrame->id();
    const quint32 movedButtonId = movedButton->id();
    const quint32 movedButton2Id = movedButton2->id();
    const quint32 sourceSubmasterId = sourceSubmaster->id();
    const quint32 sourceLevelId = sourceLevel->id();
    const quint32 destLevelId = destLevel->id();
    quint32 movedFrameId = VCWidget::invalidId();
    quint32 movedActiveButtonId = VCWidget::invalidId();
    quint32 movedAdjustSliderId = VCWidget::invalidId();
    quint32 destinationPanelId = VCWidget::invalidId();
    quint32 sourceSiblingFrameId = VCWidget::invalidId();
    quint32 sourceNestedLevelId = VCWidget::invalidId();
    quint32 destinationSiblingFrameId = VCWidget::invalidId();
    quint32 destinationNestedLevelId = VCWidget::invalidId();
    auto cleanup = qScopeGuard([&]() {
        ioMap->setOutputPatch(3, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
        if (destinationPanel != nullptr && destinationPanel->isRecordingChaser())
            destinationPanel->stopChaser();
        vc->deleteVCWidgets({movedButtonId, movedButton2Id, sourceSubmasterId, sourceLevelId,
                             destLevelId, movedFrameId, movedActiveButtonId, movedAdjustSliderId,
                             destinationPanelId, sourceSiblingFrameId, sourceNestedLevelId,
                             destinationSiblingFrameId, destinationNestedLevelId,
                             sourceFrameId, destinationFrameId});
        if (destinationOnProtectedPage)
        {
            destinationPage->setPIN(0);
            vc->deletePage(destinationPageIndex);
        }
    });

    VCBridge::AuthoringLayoutRequest req;
    req.operation = VCBridge::AuthoringLayoutRequest::Operation::MoveGroup;
    req.destinationFrameWidgetID = destinationFrameId;
    req.destinationSubpage = 0;
    req.anchorWidgetID = movedButtonId;
    req.logicalAnchor = QPointF(420, 80);

    if (scenario == QStringLiteral("safe-cross-parent-unrelated-destination-level"))
    {
        req.widgetIDs.append(movedButtonId);
        QCOMPARE(destLevel->sliderMode(), VCSlider::Level);
    }
    else if (scenario == QStringLiteral("departing-submaster-affects-source-sibling-isolated"))
    {
        req.widgetIDs.append(sourceSubmasterId);
        QCOMPARE(sourceSubmaster->sliderMode(), VCSlider::Submaster);
        QCOMPARE(sourceLevel->sliderMode(), VCSlider::Level);
        destLevel->setSliderMode(VCSlider::Submaster);
    }
    else if (scenario == QStringLiteral("safe-no-driver-unrelated-destination-sibling-frame-level"))
    {
        req.widgetIDs.append(movedButtonId);
        destinationSiblingFrame = new VCFrame(doc, vc, destinationFrame);
        vc->addWidgetToMap(destinationSiblingFrame);
        destinationFrame->addWidgetToPageMap(destinationSiblingFrame);
        destinationSiblingFrame->setShowHeader(false);
        destinationSiblingFrame->setTotalPagesNumber(1);
        destinationSiblingFrame->setCurrentPage(0);
        destinationSiblingFrame->setLogicalGeometry(QRectF(240, 20, 110, 190));
        destinationSiblingFrameId = destinationSiblingFrame->id();
        destinationNestedLevel = new VCSlider(doc, destinationSiblingFrame);
        vc->addWidgetToMap(destinationNestedLevel);
        destinationSiblingFrame->addWidgetToPageMap(destinationNestedLevel);
        destinationNestedLevel->setSliderMode(VCSlider::Level);
        destinationNestedLevel->setLogicalGeometry(QRectF(15, 20, 80, 140));
        destinationNestedLevelId = destinationNestedLevel->id();
        QCOMPARE(destinationNestedLevel->sliderMode(), VCSlider::Level);
    }
    else if (scenario == QStringLiteral("arrival-submaster-affects-destination-sibling-isolated"))
    {
        req.widgetIDs.append(sourceSubmasterId);
        QCOMPARE(sourceSubmaster->sliderMode(), VCSlider::Submaster);
        QCOMPARE(destLevel->sliderMode(), VCSlider::Level);
        sourceLevel->setSliderMode(VCSlider::Submaster);
    }
    else if (scenario == QStringLiteral("departing-submaster-affects-source-nested-sibling-frame"))
    {
        req.widgetIDs.append(sourceSubmasterId);
        sourceLevel->setSliderMode(VCSlider::Submaster);
        destLevel->setSliderMode(VCSlider::Submaster);
        sourceSiblingFrame = new VCFrame(doc, vc, sourceFrame);
        vc->addWidgetToMap(sourceSiblingFrame);
        sourceFrame->addWidgetToPageMap(sourceSiblingFrame);
        sourceSiblingFrame->setShowHeader(false);
        sourceSiblingFrame->setTotalPagesNumber(1);
        sourceSiblingFrame->setCurrentPage(0);
        sourceSiblingFrame->setLogicalGeometry(QRectF(260, 30, 90, 170));
        sourceSiblingFrameId = sourceSiblingFrame->id();
        sourceNestedLevel = new VCSlider(doc, sourceSiblingFrame);
        vc->addWidgetToMap(sourceNestedLevel);
        sourceSiblingFrame->addWidgetToPageMap(sourceNestedLevel);
        sourceNestedLevel->setSliderMode(VCSlider::Level);
        sourceNestedLevel->setLogicalGeometry(QRectF(10, 20, 70, 120));
        sourceNestedLevelId = sourceNestedLevel->id();
        QCOMPARE(sourceNestedLevel->sliderMode(), VCSlider::Level);
    }
    else if (scenario == QStringLiteral("arrival-submaster-affects-destination-nested-sibling-frame"))
    {
        req.widgetIDs.append(sourceSubmasterId);
        sourceLevel->setSliderMode(VCSlider::Submaster);
        destLevel->setSliderMode(VCSlider::Submaster);
        destinationSiblingFrame = new VCFrame(doc, vc, destinationFrame);
        vc->addWidgetToMap(destinationSiblingFrame);
        destinationFrame->addWidgetToPageMap(destinationSiblingFrame);
        destinationSiblingFrame->setShowHeader(false);
        destinationSiblingFrame->setTotalPagesNumber(1);
        destinationSiblingFrame->setCurrentPage(0);
        destinationSiblingFrame->setLogicalGeometry(QRectF(250, 30, 90, 170));
        destinationSiblingFrameId = destinationSiblingFrame->id();
        destinationNestedLevel = new VCSlider(doc, destinationSiblingFrame);
        vc->addWidgetToMap(destinationNestedLevel);
        destinationSiblingFrame->addWidgetToPageMap(destinationNestedLevel);
        destinationNestedLevel->setSliderMode(VCSlider::Level);
        destinationNestedLevel->setLogicalGeometry(QRectF(10, 20, 70, 120));
        destinationNestedLevelId = destinationNestedLevel->id();
        QCOMPARE(destinationNestedLevel->sliderMode(), VCSlider::Level);
    }
    else if (scenario == QStringLiteral("moved-subtree-active-button") ||
             scenario == QStringLiteral("moved-subtree-adjust-slider"))
    {
        sourceLevel->setSliderMode(VCSlider::Submaster);
        destLevel->setSliderMode(VCSlider::Submaster);
        movedFrame = new VCFrame(doc, vc, sourceFrame);
        vc->addWidgetToMap(movedFrame);
        sourceFrame->addWidgetToPageMap(movedFrame);
        movedFrame->setShowHeader(false);
        movedFrame->setTotalPagesNumber(1);
        movedFrame->setCurrentPage(0);
        movedFrame->setLogicalGeometry(QRectF(20, 20, 240, 200));
        movedFrameId = movedFrame->id();
        if (scenario == QStringLiteral("moved-subtree-active-button"))
        {
            movedActiveButton = new VCButton(doc, movedFrame);
            vc->addWidgetToMap(movedActiveButton);
            movedFrame->addWidgetToPageMap(movedActiveButton);
            movedActiveButton->setState(VCButton::Active);
            movedActiveButton->setLogicalGeometry(QRectF(20, 20, 90, 70));
            movedActiveButtonId = movedActiveButton->id();
            QCOMPARE(movedActiveButton->state(), VCButton::Active);
        }
        else
        {
            movedAdjustSlider = new VCSlider(doc, movedFrame);
            vc->addWidgetToMap(movedAdjustSlider);
            movedFrame->addWidgetToPageMap(movedAdjustSlider);
            movedAdjustSlider->setSliderMode(VCSlider::Adjust);
            movedAdjustSlider->setLogicalGeometry(QRectF(20, 20, 120, 120));
            movedAdjustSliderId = movedAdjustSlider->id();
            QCOMPARE(movedAdjustSlider->sliderMode(), VCSlider::Adjust);
        }
        req.anchorWidgetID = movedFrame->id();
        req.widgetIDs.append(movedFrame->id());
    }
    else if (scenario == QStringLiteral("common-census-protected-destination-page"))
    {
        req.widgetIDs.append(movedButtonId);
        QVERIFY(destinationPage->PIN() != 0);
    }
    else if (scenario == QStringLiteral("common-census-destination-relative-source"))
    {
        req.widgetIDs.append(movedButtonId);
        QSharedPointer<QLCInputSource> relativeSource(new QLCInputSource(2, 1));
        relativeSource->setID(0);
        relativeSource->setPage(0);
        relativeSource->setWorkingMode(QLCInputSource::Relative);
        destinationFrame->addInputSource(relativeSource);
        QCOMPARE(relativeSource->workingMode(), QLCInputSource::Relative);
    }
    else if (scenario == QStringLiteral("common-census-destination-feedback-patched-source"))
    {
        req.widgetIDs.append(movedButtonId);
        QSharedPointer<QLCInputSource> absoluteSource(new QLCInputSource(3, 2));
        absoluteSource->setID(0);
        absoluteSource->setPage(0);
        absoluteSource->setWorkingMode(QLCInputSource::Absolute);
        destinationFrame->addInputSource(absoluteSource);
        QVERIFY2(ioMap->setOutputPatch(3, m_probeName, "", "", 0, true),
                 "Failed to patch destination frame feedback source");
    }
    else if (scenario == QStringLiteral("common-census-destination-record-panel-capture"))
    {
        req.widgetIDs.append(movedButtonId);
        destinationPanel = new VCRecordPanel(doc, destinationFrame);
        vc->addWidgetToMap(destinationPanel);
        destinationFrame->addWidgetToPageMap(destinationPanel);
        destinationPanel->setLogicalGeometry(QRectF(180, 40, 120, 90));
        destinationPanelId = destinationPanel->id();
        destinationPanel->startChaser();
        QVERIFY(destinationPanel->isRecordingChaser());
    }
    else // safe-cross-parent-group-no-writers
    {
        req.widgetIDs.append(movedButtonId);
        req.widgetIDs.append(movedButton2Id);
        sourceLevel->setSliderMode(VCSlider::Submaster);
        destLevel->setSliderMode(VCSlider::Submaster);
    }

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    auto normalizeProtectedPinXml = [&](const QByteArray &xml,
                                        QByteArray *normalizedOut,
                                        int *targetFrameCountOut,
                                        int *targetPinCountOut,
                                        int *totalPinCountOut) -> bool {
        QXmlStreamReader reader(xml);
        QByteArray normalized;
        QXmlStreamWriter writer(&normalized);
        writer.setAutoFormatting(false);

        struct FrameState
        {
            bool isTarget = false;
            int depth = 0;
        };
        QVector<FrameState> frameStack;
        int targetFrameCount = 0;
        int targetPinCount = 0;
        int totalPinCount = 0;
        int depth = 0;

        while (!reader.atEnd())
        {
            reader.readNext();
            switch (reader.tokenType())
            {
            case QXmlStreamReader::StartDocument:
                writer.writeStartDocument();
                break;
            case QXmlStreamReader::EndDocument:
                writer.writeEndDocument();
                break;
            case QXmlStreamReader::StartElement:
            {
                ++depth;
                const QString elementName = reader.name().toString();
                writer.writeStartElement(elementName);
                const QXmlStreamAttributes attrs = reader.attributes();
                for (const QXmlStreamAttribute &attr : attrs)
                    writer.writeAttribute(attr.name().toString(), attr.value().toString());

                if (elementName == QStringLiteral("Frame"))
                {
                    const bool isTargetFrame = attrs.value(QLatin1String("ID")).toUInt() == destinationPage->id();
                    if (isTargetFrame)
                        ++targetFrameCount;
                    frameStack.append(FrameState{isTargetFrame, depth});
                }

                const bool inTargetDirectPin =
                    elementName == QStringLiteral("PIN") &&
                    !frameStack.isEmpty() &&
                    frameStack.last().isTarget &&
                    depth == frameStack.last().depth + 1;
                if (elementName == QStringLiteral("PIN"))
                    ++totalPinCount;

                if (inTargetDirectPin)
                {
                    ++targetPinCount;
                    writer.writeCharacters(QStringLiteral("__PROTECTED_PIN__"));
                    reader.skipCurrentElement();
                    writer.writeEndElement();
                    --depth;
                    break;
                }

                break;
            }
            case QXmlStreamReader::EndElement:
            {
                const QString elementName = reader.name().toString();
                writer.writeEndElement();
                if (elementName == QStringLiteral("Frame") &&
                    !frameStack.isEmpty() &&
                    frameStack.last().depth == depth)
                {
                    frameStack.removeLast();
                }
                --depth;
                break;
            }
            case QXmlStreamReader::Characters:
                writer.writeCharacters(reader.text().toString());
                break;
            case QXmlStreamReader::Comment:
                writer.writeComment(reader.text().toString());
                break;
            case QXmlStreamReader::DTD:
                writer.writeDTD(reader.text().toString());
                break;
            case QXmlStreamReader::ProcessingInstruction:
                writer.writeProcessingInstruction(reader.processingInstructionTarget().toString(),
                                                  reader.processingInstructionData().toString());
                break;
            default:
                break;
            }
        }

        if (reader.hasError())
            return false;
        if (targetFrameCount != 1 || targetPinCount != 1)
            return false;

        if (normalizedOut)
            *normalizedOut = normalized;
        if (targetFrameCountOut)
            *targetFrameCountOut = targetFrameCount;
        if (targetPinCountOut)
            *targetPinCountOut = targetPinCount;
        if (totalPinCountOut)
            *totalPinCountOut = totalPinCount;
        return true;
    };

    const QByteArray xmlBefore = serializeConsole();
    QByteArray xmlBeforeSecond;
    QByteArray normalizedBefore;
    if (destinationOnProtectedPage)
    {
        QCOMPARE(destinationPage->PIN(), 1234);
        xmlBeforeSecond = serializeConsole();
        int frameCountA = 0;
        int targetPinsA = 0;
        int totalPinsA = 0;
        int frameCountB = 0;
        int targetPinsB = 0;
        int totalPinsB = 0;
        QByteArray normalizedBeforeSecond;
        QVERIFY(normalizeProtectedPinXml(xmlBefore, &normalizedBefore, &frameCountA, &targetPinsA, &totalPinsA));
        QVERIFY(normalizeProtectedPinXml(xmlBeforeSecond, &normalizedBeforeSecond, &frameCountB, &targetPinsB, &totalPinsB));
        QCOMPARE(frameCountA, 1);
        QCOMPARE(frameCountB, 1);
        QCOMPARE(targetPinsA, 1);
        QCOMPARE(targetPinsB, 1);
        QCOMPARE(totalPinsA, 1);
        QCOMPARE(totalPinsB, 1);
        QCOMPARE(normalizedBefore, normalizedBeforeSecond);

        QByteArray altered = xmlBefore;
        altered.replace("<WindowState", "<WindowStateX");
        int alteredFrameCount = 0;
        int alteredTargetPins = 0;
        int alteredTotalPins = 0;
        QByteArray normalizedAltered;
        QVERIFY(normalizeProtectedPinXml(altered, &normalizedAltered, &alteredFrameCount, &alteredTargetPins, &alteredTotalPins));
        QVERIFY(normalizedAltered != normalizedBefore);
    }
    const bool dirtyBefore = doc->isModified();
    const QRectF movedBefore = movedButton->logicalGeometry();
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    const VCBridge::AuthoringPreparationResult result =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(result.code, expectedCode);
    QVERIFY(result.preparationToken.isEmpty());
    QCOMPARE(result.expiresInMs, 0);
    if (destinationOnProtectedPage)
    {
        QCOMPARE(destinationPage->PIN(), 1234);
        int frameCountAfter = 0;
        int targetPinsAfter = 0;
        int totalPinsAfter = 0;
        QByteArray normalizedAfter;
        QVERIFY(normalizeProtectedPinXml(serializeConsole(), &normalizedAfter,
                                         &frameCountAfter, &targetPinsAfter, &totalPinsAfter));
        QCOMPARE(frameCountAfter, 1);
        QCOMPARE(targetPinsAfter, 1);
        QCOMPARE(totalPinsAfter, 1);
        QCOMPARE(normalizedAfter, normalizedBefore);
    }
    else
    {
        QCOMPARE(serializeConsole(), xmlBefore);
    }
    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(movedButton->logicalGeometry(), movedBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);

    bool foundExpected = false;
    bool anyMetFalse = false;
    QString expectedScopePart;
    if (scenario == QStringLiteral("departing-submaster-affects-source-sibling-isolated"))
        expectedScopePart = QStringLiteral("widget:%1").arg(sourceLevelId);
    else if (scenario == QStringLiteral("arrival-submaster-affects-destination-sibling-isolated"))
        expectedScopePart = QStringLiteral("widget:%1").arg(destLevelId);
    else if (scenario == QStringLiteral("departing-submaster-affects-source-nested-sibling-frame"))
        expectedScopePart = QStringLiteral("widget:%1").arg(sourceNestedLevelId);
    else if (scenario == QStringLiteral("arrival-submaster-affects-destination-nested-sibling-frame"))
        expectedScopePart = QStringLiteral("widget:%1").arg(destinationNestedLevelId);
    else if (scenario == QStringLiteral("moved-subtree-active-button"))
        expectedScopePart = QStringLiteral("widget:%1").arg(movedActiveButtonId);
    else if (scenario == QStringLiteral("moved-subtree-adjust-slider"))
        expectedScopePart = QStringLiteral("widget:%1").arg(movedAdjustSliderId);
    else if (scenario == QStringLiteral("common-census-protected-destination-page"))
        expectedScopePart = QStringLiteral("page:%1").arg(destinationPage->id());
    else if (scenario == QStringLiteral("common-census-destination-relative-source"))
        expectedScopePart = QStringLiteral("widget:%1").arg(destinationFrameId);
    else if (scenario == QStringLiteral("common-census-destination-feedback-patched-source"))
        expectedScopePart = QStringLiteral("widget:%1 universe:3").arg(destinationFrameId);
    else if (scenario == QStringLiteral("common-census-destination-record-panel-capture"))
        expectedScopePart = QStringLiteral("widget:%1").arg(destinationPanelId);
    else
        expectedScopePart = QStringLiteral("closure");

    for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
    {
        if (!obs.met)
            anyMetFalse = true;
        if (obs.name == expectedObservation)
        {
            foundExpected = true;
            QCOMPARE(obs.met, expectedMet);
            QVERIFY(obs.scope.contains(expectedScopePart));
        }
    }
    QVERIFY2(foundExpected, qPrintable(QStringLiteral("Missing observation: %1").arg(expectedObservation)));

    if (expectedCode == QStringLiteral("unsupportedPreparation") && expectedMet)
    {
        QVERIFY(!anyMetFalse);
        QVERIFY2(isGeometryOrBindingUnsupportedError(result.error),
                 qPrintable(QStringLiteral("Unexpected unsupported error: %1").arg(result.error)));
        bool sawRequestLimitation = false;
        for (const VCBridge::AuthoringLimitation &lim : result.limitations)
        {
            if (lim.code == QStringLiteral("unsupportedPreparation") && lim.scope == QStringLiteral("request"))
                sawRequestLimitation = true;
        }
        QVERIFY2(sawRequestLimitation, "Missing request-scope unsupportedPreparation limitation");
    }
}

void VCAuthoringOwner_Test::authoringPassB_assignmentC_gateMetMatrix_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QStringList>("requiredObservations");
    QTest::addColumn<QStringList>("forbiddenObservations");

    const QStringList common = {QStringLiteral("protectedPages"), QStringLiteral("recordPanelCapture"), QStringLiteral("recordingIdle")};
    const QStringList noPRUW = {QStringLiteral("profileRoutingReady"), QStringLiteral("relativeSources"),
                                QStringLiteral("feedbackNeedsUpdateIdle"), QStringLiteral("writerSafe")};

    QTest::newRow("layout-reorder-same-parent-level")
        << QStringLiteral("layout-reorder-same-parent-level")
        << (common + QStringList{QStringLiteral("relativeSources"), QStringLiteral("feedbackPatchIdle")})
        << (QStringList{QStringLiteral("writerSafe"), QStringLiteral("profileRoutingReady"),
                        QStringLiteral("feedbackNeedsUpdateIdle")});
    QTest::newRow("layout-reorder-safe-cross-parent")
        << QStringLiteral("layout-reorder-safe-cross-parent")
        << (common + QStringList{QStringLiteral("relativeSources"), QStringLiteral("feedbackPatchIdle"), QStringLiteral("writerSafe")})
        << (QStringList{QStringLiteral("profileRoutingReady"), QStringLiteral("feedbackNeedsUpdateIdle")});
    QTest::newRow("layout-movegroup-safe-cross-parent")
        << QStringLiteral("layout-movegroup-safe-cross-parent")
        << (common + QStringList{QStringLiteral("relativeSources"), QStringLiteral("feedbackPatchIdle"), QStringLiteral("writerSafe")})
        << (QStringList{QStringLiteral("profileRoutingReady"), QStringLiteral("feedbackNeedsUpdateIdle")});
    QTest::newRow("layout-align-valid")
        << QStringLiteral("layout-align-valid")
        << (common + QStringList{QStringLiteral("relativeSources"), QStringLiteral("feedbackPatchIdle")})
        << (QStringList{QStringLiteral("writerSafe"), QStringLiteral("profileRoutingReady"),
                        QStringLiteral("feedbackNeedsUpdateIdle")});
    QTest::newRow("layout-page-create-valid")
        << QStringLiteral("layout-page-create-valid")
        << common
        << (noPRUW + QStringList{QStringLiteral("feedbackPatchIdle")});
    QTest::newRow("layout-page-rename-valid")
        << QStringLiteral("layout-page-rename-valid")
        << common
        << (noPRUW + QStringList{QStringLiteral("feedbackPatchIdle")});
    QTest::newRow("layout-page-delete-valid")
        << QStringLiteral("layout-page-delete-valid")
        << (common + QStringList{QStringLiteral("feedbackPatchIdle")})
        << (QStringList{QStringLiteral("relativeSources"), QStringLiteral("writerSafe"),
                        QStringLiteral("profileRoutingReady"), QStringLiteral("feedbackNeedsUpdateIdle")});
    QTest::newRow("layout-frame-pages-valid")
        << QStringLiteral("layout-frame-pages-valid")
        << (common + QStringList{QStringLiteral("feedbackPatchIdle")})
        << (QStringList{QStringLiteral("relativeSources"), QStringLiteral("writerSafe"),
                        QStringLiteral("profileRoutingReady"), QStringLiteral("feedbackNeedsUpdateIdle")});
    QTest::newRow("function-set-valid")
        << QStringLiteral("function-set-valid")
        << (common + QStringList{QStringLiteral("feedbackPatchIdle")})
        << (QStringList{QStringLiteral("relativeSources"), QStringLiteral("writerSafe"),
                        QStringLiteral("profileRoutingReady"), QStringLiteral("feedbackNeedsUpdateIdle")});
    QTest::newRow("function-clear-valid")
        << QStringLiteral("function-clear-valid")
        << (common + QStringList{QStringLiteral("feedbackPatchIdle")})
        << (QStringList{QStringLiteral("relativeSources"), QStringLiteral("writerSafe"),
                        QStringLiteral("profileRoutingReady"), QStringLiteral("feedbackNeedsUpdateIdle")});
    QTest::newRow("controller-append-valid")
        << QStringLiteral("controller-append-valid")
        << (common + QStringList{QStringLiteral("profileRoutingReady")})
        << (QStringList{QStringLiteral("relativeSources"), QStringLiteral("feedbackPatchIdle"),
                        QStringLiteral("feedbackNeedsUpdateIdle"), QStringLiteral("writerSafe")});
    QTest::newRow("controller-move-valid")
        << QStringLiteral("controller-move-valid")
        << (common + QStringList{QStringLiteral("relativeSources"), QStringLiteral("profileRoutingReady")})
        << (QStringList{QStringLiteral("feedbackPatchIdle"), QStringLiteral("feedbackNeedsUpdateIdle"),
                        QStringLiteral("writerSafe")});
    QTest::newRow("controller-remove-valid")
        << QStringLiteral("controller-remove-valid")
        << (common + QStringList{QStringLiteral("relativeSources")})
        << (QStringList{QStringLiteral("feedbackPatchIdle"), QStringLiteral("feedbackNeedsUpdateIdle"),
                        QStringLiteral("writerSafe"), QStringLiteral("profileRoutingReady")});
    QTest::newRow("key-set-valid")
        << QStringLiteral("key-set-valid")
        << common
        << (noPRUW + QStringList{QStringLiteral("feedbackPatchIdle")});
    QTest::newRow("key-remove-valid")
        << QStringLiteral("key-remove-valid")
        << common
        << (noPRUW + QStringList{QStringLiteral("feedbackPatchIdle")});
    QTest::newRow("feedback-values-valid")
        << QStringLiteral("feedback-values-valid")
        << (common + QStringList{QStringLiteral("feedbackPatchIdle"), QStringLiteral("feedbackNeedsUpdateIdle")})
        << (QStringList{QStringLiteral("relativeSources"), QStringLiteral("profileRoutingReady"),
                        QStringLiteral("writerSafe")});
    QTest::newRow("feedback-channels-valid")
        << QStringLiteral("feedback-channels-valid")
        << (common + QStringList{QStringLiteral("feedbackPatchIdle"), QStringLiteral("feedbackNeedsUpdateIdle")})
        << (QStringList{QStringLiteral("relativeSources"), QStringLiteral("profileRoutingReady"),
                        QStringLiteral("writerSafe")});
    QTest::newRow("feedback-values-and-channels-valid")
        << QStringLiteral("feedback-values-and-channels-valid")
        << (common + QStringList{QStringLiteral("feedbackPatchIdle"), QStringLiteral("feedbackNeedsUpdateIdle")})
        << (QStringList{QStringLiteral("relativeSources"), QStringLiteral("profileRoutingReady"),
                        QStringLiteral("writerSafe")});
    QTest::newRow("recordpanel-set-supported-fields")
        << QStringLiteral("recordpanel-set-supported-fields")
        << (common + QStringList{QStringLiteral("recordingIdle")})
        << (QStringList{QStringLiteral("relativeSources"), QStringLiteral("profileRoutingReady"),
                        QStringLiteral("writerSafe"), QStringLiteral("feedbackPatchIdle"), QStringLiteral("feedbackNeedsUpdateIdle")});
}

void VCAuthoringOwner_Test::authoringPassB_assignmentC_gateMetMatrix()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    QFETCH(QString, scenario);
    QFETCH(QStringList, requiredObservations);
    QFETCH(QStringList, forbiddenObservations);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    VCPage *page0 = vc->page(0);
    QVERIFY(page0 != nullptr);

    VCFrame *sourceFrame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(sourceFrame);
    page0->addWidgetToPageMap(sourceFrame);
    sourceFrame->setShowHeader(false);
    sourceFrame->setTotalPagesNumber(2);
    sourceFrame->setCurrentPage(0);
    sourceFrame->setLogicalGeometry(QRectF(0, 0, 340, 320));

    VCFrame *destinationFrame = new VCFrame(doc, vc, page0);
    vc->addWidgetToMap(destinationFrame);
    page0->addWidgetToPageMap(destinationFrame);
    destinationFrame->setShowHeader(false);
    destinationFrame->setTotalPagesNumber(2);
    destinationFrame->setCurrentPage(0);
    destinationFrame->setLogicalGeometry(QRectF(360, 0, 340, 320));

    VCButton *sourceButton = new VCButton(doc, sourceFrame);
    vc->addWidgetToMap(sourceButton);
    sourceFrame->addWidgetToPageMap(sourceButton);
    sourceButton->setLogicalGeometry(QRectF(20, 40, 90, 60));

    VCButton *sourceButton2 = new VCButton(doc, sourceFrame);
    vc->addWidgetToMap(sourceButton2);
    sourceFrame->addWidgetToPageMap(sourceButton2);
    sourceButton2->setLogicalGeometry(QRectF(120, 40, 90, 60));

    VCSlider *sourceLevel = new VCSlider(doc, sourceFrame);
    vc->addWidgetToMap(sourceLevel);
    sourceFrame->addWidgetToPageMap(sourceLevel);
    sourceLevel->setSliderMode(VCSlider::Level);
    sourceLevel->setLogicalGeometry(QRectF(20, 130, 120, 160));

    VCRecordPanel *panel = new VCRecordPanel(doc, sourceFrame);
    vc->addWidgetToMap(panel);
    sourceFrame->addWidgetToPageMap(panel);
    panel->setLogicalGeometry(QRectF(170, 130, 140, 100));

    QSharedPointer<QLCInputSource> buttonSource(new QLCInputSource(0, 7));
    buttonSource->setID(0);
    buttonSource->setPage(0);
    buttonSource->setWorkingMode(QLCInputSource::Absolute);
    sourceButton->addInputSource(buttonSource);

    Scene *scene = new Scene(doc);
    scene->setName(QStringLiteral("AssignmentCMatrixScene"));
    QVERIFY(doc->addFunction(scene));

    const QString profilePath = m_workspaceDir.filePath(QStringLiteral("assignment-c-matrix%1").arg(KExtInputProfile));
    QFile profileFile(profilePath);
    QVERIFY2(profileFile.open(QIODevice::WriteOnly | QIODevice::Text),
             qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(profilePath)));
    const QByteArray profileXml = QByteArrayLiteral(
        "<?xml version='1.0' encoding='UTF-8'?>\n"
        "<!DOCTYPE InputProfile>\n"
        "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
        " <Creator><Name>PassC</Name><Version>1</Version><Author>test</Author></Creator>\n"
        " <Manufacturer>PCMCP</Manufacturer>\n"
        " <Model>Matrix</Model>\n"
        " <Type>MIDI</Type>\n"
        " <Channel Number=\"7\"><Name>Ch7</Name><Type>Slider</Type></Channel>\n"
        "</InputProfile>\n");
    QCOMPARE(profileFile.write(profileXml), profileXml.size());
    profileFile.close();
    std::unique_ptr<QLCInputProfile> loaded(QLCInputProfile::loader(profilePath));
    QVERIFY2(loaded != nullptr, "Profile fixture loader failed");
    const QString profileName = loaded->name();
    ioMap->loadProfiles(QDir(m_workspaceDir.path()));
    QVERIFY(ioMap->profileNames().contains(profileName));
    QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, profileName),
             "Failed to patch loaded profile for matrix scenarios");

    int extraPageIndex = -1;
    const quint32 sourceFrameId = sourceFrame->id();
    const quint32 destinationFrameId = destinationFrame->id();
    const quint32 sourceButtonId = sourceButton->id();
    const quint32 sourceButton2Id = sourceButton2->id();
    const quint32 sourceLevelId = sourceLevel->id();
    const quint32 panelId = panel->id();
    const quint32 sceneId = scene->id();
    auto cleanup = qScopeGuard([&]() {
        ioMap->setInputPatch(0, KInputNone, "", "", QLCIOPlugin::invalidLine(), KInputNone);
        vc->deleteVCWidgets({sourceButtonId, sourceButton2Id, sourceLevelId, panelId, sourceFrameId, destinationFrameId});
        doc->deleteFunction(sceneId);
        if (extraPageIndex >= 0)
            vc->deletePage(extraPageIndex);
    });

    VCBridge::AuthoringRequest request = VCBridge::AuthoringRequest(VCBridge::AuthoringLayoutRequest{});
    if (scenario == QStringLiteral("layout-reorder-same-parent-level"))
    {
        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::Reorder;
        req.widgetIDs.append(sourceLevelId);
        req.destinationFrameWidgetID = sourceFrameId;
        req.destinationSubpage = sourceLevel->page();
        req.beforeWidgetID = sourceButtonId;
        request = VCBridge::AuthoringRequest(req);
    }
    else if (scenario == QStringLiteral("layout-reorder-safe-cross-parent"))
    {
        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::Reorder;
        req.widgetIDs.append(sourceButtonId);
        req.destinationFrameWidgetID = destinationFrameId;
        req.destinationSubpage = 0;
        request = VCBridge::AuthoringRequest(req);
    }
    else if (scenario == QStringLiteral("layout-movegroup-safe-cross-parent"))
    {
        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::MoveGroup;
        req.widgetIDs.append(sourceButtonId);
        req.widgetIDs.append(sourceButton2Id);
        req.destinationFrameWidgetID = destinationFrameId;
        req.destinationSubpage = 0;
        req.anchorWidgetID = sourceButtonId;
        req.logicalAnchor = QPointF(420, 80);
        request = VCBridge::AuthoringRequest(req);
    }
    else if (scenario == QStringLiteral("layout-align-valid"))
    {
        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::Align;
        req.frameWidgetID = sourceFrameId;
        req.columns = 2;
        req.padding = 8;
        request = VCBridge::AuthoringRequest(req);
    }
    else if (scenario == QStringLiteral("layout-page-create-valid"))
    {
        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::PageCreate;
        req.name = QStringLiteral("Assignment C page");
        request = VCBridge::AuthoringRequest(req);
    }
    else if (scenario == QStringLiteral("layout-page-rename-valid"))
    {
        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::PageRename;
        req.pageWidgetID = page0->id();
        req.name = QStringLiteral("Renamed page");
        request = VCBridge::AuthoringRequest(req);
    }
    else if (scenario == QStringLiteral("layout-page-delete-valid"))
    {
        extraPageIndex = vc->pagesCount();
        vc->addPage(extraPageIndex);
        VCPage *extraPage = vc->page(extraPageIndex);
        QVERIFY(extraPage != nullptr);
        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::PageDelete;
        req.pageWidgetID = extraPage->id();
        request = VCBridge::AuthoringRequest(req);
    }
    else if (scenario == QStringLiteral("layout-frame-pages-valid"))
    {
        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::FramePages;
        req.frameWidgetID = sourceFrameId;
        req.totalPages = 2;
        req.pageLabels = QStringList{QStringLiteral("One"), QStringLiteral("Two")};
        req.pagesLoop = true;
        request = VCBridge::AuthoringRequest(req);
    }
    else if (scenario == QStringLiteral("function-set-valid") || scenario == QStringLiteral("function-clear-valid") ||
             scenario == QStringLiteral("controller-append-valid") || scenario == QStringLiteral("controller-move-valid") ||
             scenario == QStringLiteral("controller-remove-valid") || scenario == QStringLiteral("key-set-valid") ||
             scenario == QStringLiteral("key-remove-valid") || scenario == QStringLiteral("feedback-values-valid") ||
             scenario == QStringLiteral("feedback-channels-valid") || scenario == QStringLiteral("feedback-values-and-channels-valid"))
    {
        VCBridge::AuthoringBindingsRequest req;
        VCBridge::AuthoringBindingItem item;
        item.widgetID = sourceButtonId;
        item.controlSlotID = 0;
        if (scenario == QStringLiteral("function-set-valid"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Function;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.functionID = sceneId;
        }
        else if (scenario == QStringLiteral("function-clear-valid"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Function;
            item.action = VCBridge::AuthoringBindingItem::Action::Clear;
            item.functionID = sceneId;
        }
        else if (scenario == QStringLiteral("controller-append-valid"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
            item.action = VCBridge::AuthoringBindingItem::Action::Append;
            VCBridge::AuthoringDestination dst;
            dst.inputUniverseID = 0;
            dst.inputChannel = 7;
            dst.profileName = profileName;
            item.destination = dst;
        }
        else if (scenario == QStringLiteral("controller-move-valid"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
            item.action = VCBridge::AuthoringBindingItem::Action::Move;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
            VCBridge::AuthoringDestination dst;
            dst.inputUniverseID = 0;
            dst.inputChannel = 7;
            dst.profileName = profileName;
            item.destination = dst;
        }
        else if (scenario == QStringLiteral("controller-remove-valid"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
            item.action = VCBridge::AuthoringBindingItem::Action::Remove;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
        }
        else if (scenario == QStringLiteral("key-set-valid"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Key;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.keySequence = QStringLiteral("Ctrl+Shift+L");
        }
        else if (scenario == QStringLiteral("key-remove-valid"))
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Key;
            item.action = VCBridge::AuthoringBindingItem::Action::Remove;
            item.keySequence = QStringLiteral("Ctrl+Shift+L");
        }
        else
        {
            item.kind = VCBridge::AuthoringBindingItem::Kind::Feedback;
            item.action = VCBridge::AuthoringBindingItem::Action::Set;
            item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
            if (scenario == QStringLiteral("feedback-values-valid") || scenario == QStringLiteral("feedback-values-and-channels-valid"))
            {
                item.idleValue = 10;
                item.activeValue = 20;
                item.monitorValue = 30;
            }
            if (scenario == QStringLiteral("feedback-channels-valid") || scenario == QStringLiteral("feedback-values-and-channels-valid"))
            {
                item.idleChannel = 1;
                item.activeChannel = 2;
                item.monitorChannel = 3;
            }
        }
        req.items.append(item);
        request = VCBridge::AuthoringRequest(req);
    }
    else if (scenario == QStringLiteral("recordpanel-set-supported-fields"))
    {
        VCBridge::AuthoringRecordPanelRequest req;
        VCBridge::AuthoringRecordPanelItem item;
        item.widgetID = panelId;
        item.targetFolder = QStringLiteral("C:/tmp");
        item.scenePrefix = QStringLiteral("SC_");
        item.chaserPrefix = QStringLiteral("CH_");
        item.defaultFadeIn = 110;
        item.defaultHold = 210;
        item.defaultFadeOut = 310;
        req.items.append(item);
        request = VCBridge::AuthoringRequest(req);
    }
    else
    {
        QFAIL("Unexpected scenario");
    }

    const QByteArray xmlBefore = serializeConsole();
    const bool dirtyBefore = doc->isModified();
    const QRectF sourceButtonBefore = sourceButton->logicalGeometry();
    const QRectF sourceLevelBefore = sourceLevel->logicalGeometry();
    const QRectF destinationFrameBefore = destinationFrame->logicalGeometry();
    const int pageCountBefore = vc->pagesCount();
    const QString page0CaptionBefore = page0->caption();
    const int sourceFrameTotalPagesBefore = sourceFrame->totalPagesNumber();
    const QStringList sourceFramePageLabelsBefore = sourceFrame->pageLabels();
    const bool sourceFramePagesLoopBefore = sourceFrame->pagesLoop();
    const QString panelTargetFolderBefore = panel->targetFolder();
    const QString panelScenePrefixBefore = panel->scenePrefix();
    const QString panelChaserPrefixBefore = panel->chaserPrefix();
    const quint32 buttonFunctionBefore = sourceButton->functionID();
    QList<quint32> sourceOrderBefore;
    for (const QSharedPointer<QLCInputSource> &src : sourceButton->inputSources())
        sourceOrderBefore.append(src ? src->channel() : 0u);
    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    const VCBridge::AuthoringPreparationResult result = m_bridge->prepareAuthoring(request);
    QString pendingToken;
    const bool bindingNowSupported =
        (scenario == QStringLiteral("layout-page-create-valid") ||
         scenario == QStringLiteral("layout-page-rename-valid") ||
         scenario == QStringLiteral("layout-page-delete-valid") ||
         scenario == QStringLiteral("layout-frame-pages-valid") ||
         scenario == QStringLiteral("function-set-valid") ||
         scenario == QStringLiteral("function-clear-valid") ||
         scenario == QStringLiteral("controller-append-valid") ||
         scenario == QStringLiteral("controller-move-valid") ||
         scenario == QStringLiteral("controller-remove-valid") ||
         scenario == QStringLiteral("key-set-valid") ||
         scenario == QStringLiteral("key-remove-valid") ||
         scenario == QStringLiteral("feedback-values-valid") ||
         scenario == QStringLiteral("feedback-channels-valid") ||
         scenario == QStringLiteral("feedback-values-and-channels-valid") ||
         scenario == QStringLiteral("recordpanel-set-supported-fields"));
    if (bindingNowSupported)
    {
        QCOMPARE(result.code, QString());
        QCOMPARE(result.error, QString());
        QVERIFY(!result.preparationToken.isEmpty());
        QVERIFY(result.expiresInMs > 0);
        pendingToken = result.preparationToken;
    }
    else
    {
        QCOMPARE(result.code, QStringLiteral("unsupportedPreparation"));
        QVERIFY2(isGeometryOrBindingUnsupportedError(result.error),
                 qPrintable(QStringLiteral("Unexpected unsupported error: %1").arg(result.error)));
        QVERIFY(result.preparationToken.isEmpty());
        QCOMPARE(result.expiresInMs, 0);
    }
    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(serializeConsole(), xmlBefore);
    QCOMPARE(sourceButton->logicalGeometry(), sourceButtonBefore);
    QCOMPARE(sourceLevel->logicalGeometry(), sourceLevelBefore);
    QCOMPARE(destinationFrame->logicalGeometry(), destinationFrameBefore);
    QCOMPARE(panel->targetFolder(), panelTargetFolderBefore);
    QCOMPARE(panel->scenePrefix(), panelScenePrefixBefore);
    QCOMPARE(panel->chaserPrefix(), panelChaserPrefixBefore);
    QCOMPARE(sourceButton->functionID(), buttonFunctionBefore);
    QList<quint32> sourceOrderAfter;
    for (const QSharedPointer<QLCInputSource> &src : sourceButton->inputSources())
        sourceOrderAfter.append(src ? src->channel() : 0u);
    QCOMPARE(sourceOrderAfter, sourceOrderBefore);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("writeCount"), baselineWrite);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);

    QMap<QString, bool> obsState;
    for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
    {
        obsState.insert(obs.name, obs.met);
        QVERIFY2(obs.met, qPrintable(QStringLiteral("Unexpected unmet observation %1").arg(obs.name)));
    }
    for (const QString &name : requiredObservations)
    {
        QVERIFY2(obsState.contains(name), qPrintable(QStringLiteral("Missing required observation %1").arg(name)));
        QVERIFY2(obsState.value(name), qPrintable(QStringLiteral("Required observation not met: %1").arg(name)));
    }
    for (const QString &name : forbiddenObservations)
        QVERIFY2(!obsState.contains(name), qPrintable(QStringLiteral("Forbidden observation present: %1").arg(name)));

    if (!pendingToken.isEmpty())
    {
        const VCBridge::AuthoringApplyResult applied = m_bridge->applyAuthoring(pendingToken);
        QCOMPARE(applied.code, QString());
        QCOMPARE(applied.tokenConsumed, true);

        if (scenario == QStringLiteral("recordpanel-set-supported-fields"))
        {
            QCOMPARE(panel->targetFolder(), QStringLiteral("C:/tmp"));
            QCOMPARE(panel->scenePrefix(), QStringLiteral("SC_"));
            QCOMPARE(panel->chaserPrefix(), QStringLiteral("CH_"));
            QCOMPARE(panel->defaultFadeIn(), uint(110));
            QCOMPARE(panel->defaultHold(), uint(210));
            QCOMPARE(panel->defaultFadeOut(), uint(310));

            QByteArray persistedXml;
            {
                QBuffer buffer(&persistedXml);
                QVERIFY(buffer.open(QIODevice::WriteOnly));
                QXmlStreamWriter writer(&buffer);
                writer.writeStartDocument();
                QVERIFY(panel->saveXML(&writer));
                writer.writeEndDocument();
                buffer.close();
            }
            std::unique_ptr<VCRecordPanel> loadedPanel = std::make_unique<VCRecordPanel>(doc, nullptr);
            QBuffer readBuffer(&persistedXml);
            QVERIFY(readBuffer.open(QIODevice::ReadOnly));
            QXmlStreamReader reader(&readBuffer);
            reader.readNextStartElement();
            QVERIFY2(!reader.atEnd(), "Invalid record panel XML");
            QVERIFY2(loadedPanel->loadXML(reader), "VCRecordPanel::loadXML failed");
            QCOMPARE(loadedPanel->targetFolder(), panel->targetFolder());
            QCOMPARE(loadedPanel->scenePrefix(), panel->scenePrefix());
            QCOMPARE(loadedPanel->chaserPrefix(), panel->chaserPrefix());
            QCOMPARE(loadedPanel->defaultFadeIn(), panel->defaultFadeIn());
            QCOMPARE(loadedPanel->defaultHold(), panel->defaultHold());
            QCOMPARE(loadedPanel->defaultFadeOut(), panel->defaultFadeOut());
            readBuffer.close();
        }
        else if (scenario == QStringLiteral("feedback-values-valid") ||
                 scenario == QStringLiteral("feedback-channels-valid") ||
                 scenario == QStringLiteral("feedback-values-and-channels-valid"))
        {
            if (scenario == QStringLiteral("feedback-values-valid") ||
                scenario == QStringLiteral("feedback-values-and-channels-valid"))
            {
                QCOMPARE(buttonSource->feedbackValue(QLCInputFeedback::LowerValue), uchar(10));
                QCOMPARE(buttonSource->feedbackValue(QLCInputFeedback::UpperValue), uchar(20));
                QCOMPARE(buttonSource->feedbackValue(QLCInputFeedback::MonitorValue), uchar(30));
            }
            if (scenario == QStringLiteral("feedback-channels-valid") ||
                scenario == QStringLiteral("feedback-values-and-channels-valid"))
            {
                QCOMPARE(buttonSource->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt(), 1);
                QCOMPARE(buttonSource->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt(), 2);
                QCOMPARE(buttonSource->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt(), 3);
            }
        }
        else if (scenario == QStringLiteral("layout-page-create-valid"))
        {
            QCOMPARE(vc->pagesCount(), pageCountBefore + 1);
            bool foundCaption = false;
            for (int i = 0; i < vc->pagesCount(); ++i)
            {
                VCPage *candidate = vc->page(i);
                if (candidate && candidate->caption() == QStringLiteral("Assignment C page"))
                {
                    foundCaption = true;
                    break;
                }
            }
            QVERIFY(foundCaption);
        }
        else if (scenario == QStringLiteral("layout-page-rename-valid"))
        {
            QVERIFY(page0CaptionBefore != QStringLiteral("Renamed page"));
            QCOMPARE(page0->caption(), QStringLiteral("Renamed page"));
        }
        else if (scenario == QStringLiteral("layout-page-delete-valid"))
        {
            QCOMPARE(vc->pagesCount(), pageCountBefore - 1);
        }
        else if (scenario == QStringLiteral("layout-frame-pages-valid"))
        {
            QCOMPARE(sourceFrame->totalPagesNumber(), 2);
            QCOMPARE(sourceFrame->pageLabels(), QStringList({QStringLiteral("One"), QStringLiteral("Two")}));
            QCOMPARE(sourceFrame->pagesLoop(), true);
            QVERIFY(sourceFrameTotalPagesBefore != sourceFrame->totalPagesNumber() ||
                    sourceFramePageLabelsBefore != sourceFrame->pageLabels() ||
                    sourceFramePagesLoopBefore != sourceFrame->pagesLoop());
        }
    }

    Q_UNUSED(bindingNowSupported);
}

void VCAuthoringOwner_Test::authoringPassB_assignmentC_residencyAndRouting_data()
{
    QTest::addColumn<QString>("scenario");

    QTest::newRow("controller-move-old-unpatched-to-patched-destination")
        << QStringLiteral("controller-move-old-unpatched-to-patched-destination");
    QTest::newRow("controller-append-absolute-extra-press")
        << QStringLiteral("controller-append-absolute-extra-press");
    QTest::newRow("controller-append-absolute-encoder")
        << QStringLiteral("controller-append-absolute-encoder");
    QTest::newRow("controller-remove-relative-existing-source")
        << QStringLiteral("controller-remove-relative-existing-source");
    QTest::newRow("controller-append-fanout-default-refused")
        << QStringLiteral("controller-append-fanout-default-refused");
    QTest::newRow("controller-append-fanout-allowed")
        << QStringLiteral("controller-append-fanout-allowed");
    QTest::newRow("controller-append-fanout-stale-after-prepare")
        << QStringLiteral("controller-append-fanout-stale-after-prepare");
    QTest::newRow("controller-append-fanout-noop-does-not-block")
        << QStringLiteral("controller-append-fanout-noop-does-not-block");
    QTest::newRow("controller-move-fanout-noop-does-not-block")
        << QStringLiteral("controller-move-fanout-noop-does-not-block");
    QTest::newRow("controller-append-same-profile-channel-mutated")
        << QStringLiteral("controller-append-same-profile-channel-mutated");
    QTest::newRow("controller-append-same-profile-movement-mutated")
        << QStringLiteral("controller-append-same-profile-movement-mutated");
    QTest::newRow("controller-append-same-profile-limits-mutated")
        << QStringLiteral("controller-append-same-profile-limits-mutated");
    QTest::newRow("controller-append-same-profile-lower-channel-mutated")
        << QStringLiteral("controller-append-same-profile-lower-channel-mutated");
    QTest::newRow("controller-append-attached-profile-registry-mismatch")
        << QStringLiteral("controller-append-attached-profile-registry-mismatch");
    QTest::newRow("controller-page-slot2-routing")
        << QStringLiteral("controller-page-slot2-routing");
    QTest::newRow("controller-nonzero-slot-subpage-roundtrip")
        << QStringLiteral("controller-nonzero-slot-subpage-roundtrip");
    QTest::newRow("controller-source-lifetime-same-key-move-refused")
        << QStringLiteral("controller-source-lifetime-same-key-move-refused");
    QTest::newRow("controller-source-lifetime-same-key-remove-refused")
        << QStringLiteral("controller-source-lifetime-same-key-remove-refused");
    QTest::newRow("controller-mixed-outcomes-valid-first")
        << QStringLiteral("controller-mixed-outcomes-valid-first");
    QTest::newRow("controller-mixed-outcomes-refused-first")
        << QStringLiteral("controller-mixed-outcomes-refused-first");
    QTest::newRow("controller-bad-second-refusal-clean")
        << QStringLiteral("controller-bad-second-refusal-clean");
    QTest::newRow("controller-bad-second-refusal-dirty")
        << QStringLiteral("controller-bad-second-refusal-dirty");
    QTest::newRow("geometry-profile-load-between-prepare-and-apply")
        << QStringLiteral("geometry-profile-load-between-prepare-and-apply");
}

void VCAuthoringOwner_Test::authoringPassB_assignmentC_residencyAndRouting()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");
    QFETCH(QString, scenario);

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    InputOutputMap *ioMap = doc->inputOutputMap();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);
    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 600, 320));
    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 40, 100, 70));
    button->setPage(0);

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanup = qScopeGuard([&]() {
        ioMap->setInputPatch(0, KInputNone, "", "", QLCIOPlugin::invalidLine(), KInputNone);
        ioMap->setInputPatch(1, KInputNone, "", "", QLCIOPlugin::invalidLine(), KInputNone);
        vc->deleteVCWidgets({buttonId, frameId});
    });

    const QString marker = QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
    const QString absModel = QStringLiteral("AssignmentCAbs_%1").arg(marker.left(8));
    const QString extraModel = QStringLiteral("AssignmentCExtra_%1").arg(marker.mid(8, 8));
    const QString absPath = m_workspaceDir.filePath(absModel + KExtInputProfile);
    const QString extraPath = m_workspaceDir.filePath(extraModel + KExtInputProfile);

    auto writeProfile = [&](const QString &path, const QString &model, const QString &channelXml) {
        QFile file(path);
        QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("Failed to create profile fixture at %1").arg(path)));
        const QByteArray xml = QStringLiteral(
            "<?xml version='1.0' encoding='UTF-8'?>\n"
            "<!DOCTYPE InputProfile>\n"
            "<InputProfile xmlns=\"http://www.qlcplus.org/InputProfile\">\n"
            " <Creator><Name>PassC</Name><Version>1</Version><Author>test</Author></Creator>\n"
            " <Manufacturer>PCMCP</Manufacturer>\n"
            " <Model>%1</Model>\n"
            " <Type>MIDI</Type>\n"
            " %2\n"
            "</InputProfile>\n").arg(model, channelXml).toUtf8();
        QCOMPARE(file.write(xml), xml.size());
        file.close();
    };

    writeProfile(absPath, absModel,
                 QStringLiteral("<Channel Number=\"7\"><Name>Abs7</Name><Type>Slider</Type></Channel>\n"
                                " <Channel Number=\"8\"><Name>Abs8</Name><Type>Slider</Type></Channel>\n"
                                " <Channel Number=\"9\"><Name>Enc9</Name><Type>Encoder</Type></Channel>"));
    writeProfile(extraPath, extraModel,
                 QStringLiteral("<Channel Number=\"8\"><Name>Btn8</Name><Type>Button</Type><ExtraPress>True</ExtraPress></Channel>"));
    std::unique_ptr<QLCInputProfile> absLoaded(QLCInputProfile::loader(absPath));
    std::unique_ptr<QLCInputProfile> extraLoaded(QLCInputProfile::loader(extraPath));
    QVERIFY(absLoaded != nullptr);
    QVERIFY(extraLoaded != nullptr);
    const QString absProfile = absLoaded->name();
    const QString extraProfile = extraLoaded->name();
    QLCInputChannel *extraChannel = extraLoaded->channel(8);
    QVERIFY(extraChannel != nullptr);
    QCOMPARE(extraChannel->type(), QLCInputChannel::Button);
    QCOMPARE(extraChannel->sendExtraPress(), true);

    if (scenario == QStringLiteral("controller-move-old-unpatched-to-patched-destination"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));

        QSharedPointer<QLCInputSource> existing(new QLCInputSource(1, 7));
        existing->setID(0);
        existing->setPage(0);
        existing->setWorkingMode(QLCInputSource::Absolute);
        button->addInputSource(existing);

        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch destination profile");
        ioMap->setInputPatch(1, KInputNone, "", "", QLCIOPlugin::invalidLine(), KInputNone);

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Move;
        item.controlSlotID = 0;
        item.sourceKey = VCBridge::AuthoringSourceKey{0, 1, 7, 0};
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 7;
        dst.profileName = absProfile;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);

        const QByteArray xmlBefore = serializeConsole();
        const bool dirtyBefore = doc->isModified();
        const QRectF buttonBefore = button->logicalGeometry();
        resetProbeCounters();
        const int baselineOpen = counter("openCount");
        const int baselineClose = counter("closeCount");
        const int baselineWrite = counter("writeCount");
        const int baselineFeedback = counter("feedbackCount");

        const VCBridge::AuthoringPreparationResult result =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(result.code, QString());
        QVERIFY(!result.preparationToken.isEmpty());
        bool sawPR = false;
        for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
            if (obs.name == QStringLiteral("profileRoutingReady"))
                sawPR = obs.met;
        QVERIFY(sawPR);
        const VCBridge::AuthoringApplyResult applied = m_bridge->applyAuthoring(result.preparationToken);
        QCOMPARE(applied.code, QString());
        QCOMPARE(applied.tokenConsumed, true);

        QCOMPARE(doc->isModified(), dirtyBefore);
        QVERIFY(serializeConsole() != xmlBefore);
        QCOMPARE(button->logicalGeometry(), buttonBefore);
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
    }
    else if (scenario == QStringLiteral("controller-append-absolute-extra-press"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(extraProfile));

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        item.controlSlotID = 0;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 8;
        dst.profileName = extraProfile;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, extraProfile), "Failed to patch extra-press profile");

        const QByteArray xmlBefore = serializeConsole();
        const QRectF buttonBefore = button->logicalGeometry();
        resetProbeCounters();
        const int baselineOpen = counter("openCount");
        const int baselineClose = counter("closeCount");
        const int baselineWrite = counter("writeCount");
        const int baselineFeedback = counter("feedbackCount");

        const VCBridge::AuthoringPreparationResult result =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(result.code, QString());
        QVERIFY(!result.preparationToken.isEmpty());
        bool sawPR = false;
        for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
            if (obs.name == QStringLiteral("profileRoutingReady"))
                sawPR = obs.met;
        QVERIFY(sawPR);
        doc->resetModified();
        QCOMPARE(doc->isModified(), false);
        const VCBridge::AuthoringApplyResult applied = m_bridge->applyAuthoring(result.preparationToken);
        QCOMPARE(applied.code, QString());
        QCOMPARE(applied.tokenConsumed, true);
        QCOMPARE(doc->isModified(), true);
        QCOMPARE(button->inputSources().size(), 1);
        QSharedPointer<QLCInputSource> source = button->inputSources().at(0);
        QVERIFY(!source.isNull());
        QCOMPARE(source->workingMode(), QLCInputSource::Absolute);
        QCOMPARE(source->sendExtraPressRelease(), true);

        QByteArray persistedXml;
        {
            QBuffer buffer(&persistedXml);
            QVERIFY(buffer.open(QIODevice::WriteOnly));
            QXmlStreamWriter writer(&buffer);
            writer.writeStartDocument();
            QVERIFY(button->saveXML(&writer));
            writer.writeEndDocument();
            buffer.close();
        }
        std::unique_ptr<VCButton> loadedButton = std::make_unique<VCButton>(doc, nullptr);
        QBuffer readBuffer(&persistedXml);
        QVERIFY(readBuffer.open(QIODevice::ReadOnly));
        QXmlStreamReader reader(&readBuffer);
        reader.readNextStartElement();
        QVERIFY2(!reader.atEnd(), "Invalid controller extra-press XML");
        QVERIFY2(loadedButton->loadXML(reader), "VCButton::loadXML failed for extra-press source");
        QCOMPARE(loadedButton->inputSources().size(), 1);
        QCOMPARE(loadedButton->inputSources().at(0)->workingMode(), QLCInputSource::Absolute);
        QCOMPARE(loadedButton->inputSources().at(0)->sendExtraPressRelease(), true);
        readBuffer.close();

        QVERIFY(serializeConsole() != xmlBefore);
        QCOMPARE(button->logicalGeometry(), buttonBefore);
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
    }
    else if (scenario == QStringLiteral("controller-append-absolute-encoder"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        item.controlSlotID = 0;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 9;
        dst.profileName = absProfile;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        const VCBridge::AuthoringPreparationResult prep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(prep.code, QString());
        QVERIFY(!prep.preparationToken.isEmpty());
        const VCBridge::AuthoringApplyResult apply =
            m_bridge->applyAuthoring(prep.preparationToken);
        QCOMPARE(apply.code, QString());
        QCOMPARE(apply.tokenConsumed, true);
        QCOMPARE(button->inputSources().size(), 1);
        QSharedPointer<QLCInputSource> source = button->inputSources().at(0);
        QVERIFY(!source.isNull());
        QCOMPARE(source->workingMode(), QLCInputSource::Encoder);

        QByteArray persistedXml;
        {
            QBuffer buffer(&persistedXml);
            QVERIFY(buffer.open(QIODevice::WriteOnly));
            QXmlStreamWriter writer(&buffer);
            writer.writeStartDocument();
            QVERIFY(button->saveXML(&writer));
            writer.writeEndDocument();
            buffer.close();
        }
        std::unique_ptr<VCButton> loadedButton = std::make_unique<VCButton>(doc, nullptr);
        QBuffer readBuffer(&persistedXml);
        QVERIFY(readBuffer.open(QIODevice::ReadOnly));
        QXmlStreamReader reader(&readBuffer);
        reader.readNextStartElement();
        QVERIFY2(!reader.atEnd(), "Invalid controller encoder XML");
        QVERIFY2(loadedButton->loadXML(reader), "VCButton::loadXML failed for encoder source");
        QCOMPARE(loadedButton->inputSources().size(), 1);
        QCOMPARE(loadedButton->inputSources().at(0)->workingMode(), QLCInputSource::Encoder);
        readBuffer.close();
    }
    else if (scenario == QStringLiteral("controller-remove-relative-existing-source"))
    {
        QSharedPointer<QLCInputSource> existing(new QLCInputSource(0, 7));
        existing->setID(0);
        existing->setPage(0);
        existing->setWorkingMode(QLCInputSource::Relative);
        button->addInputSource(existing);
        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Remove;
        item.controlSlotID = 0;
        item.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);

        const QByteArray xmlBefore = serializeConsole();
        const bool dirtyBefore = doc->isModified();
        const QRectF buttonBefore = button->logicalGeometry();
        resetProbeCounters();
        const int baselineOpen = counter("openCount");
        const int baselineClose = counter("closeCount");
        const int baselineWrite = counter("writeCount");
        const int baselineFeedback = counter("feedbackCount");

        const VCBridge::AuthoringPreparationResult result =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(result.code, QStringLiteral("unsupportedPreparation"));
        bool sawRelative = false;
        bool sawProfileRouting = false;
        for (const VCBridge::AuthoringPreconditionObservation &obs : result.preconditions)
        {
            if (obs.name == QStringLiteral("relativeSources"))
            {
                sawRelative = true;
                QCOMPARE(obs.met, false);
                QVERIFY(obs.scope.contains(QStringLiteral("widget:%1").arg(buttonId)));
            }
            if (obs.name == QStringLiteral("profileRoutingReady"))
                sawProfileRouting = true;
        }
        QVERIFY(sawRelative);
        QVERIFY(!sawProfileRouting);

        QCOMPARE(doc->isModified(), dirtyBefore);
        QCOMPARE(serializeConsole(), xmlBefore);
        QCOMPARE(button->logicalGeometry(), buttonBefore);
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
    }
    else if (scenario == QStringLiteral("controller-append-fanout-default-refused"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        VCButton *other = new VCButton(doc, frame);
        vc->addWidgetToMap(other);
        frame->addWidgetToPageMap(other);
        other->setLogicalGeometry(QRectF(160, 40, 100, 70));
        other->setPage(0);
        const quint32 otherId = other->id();
        auto cleanupOther = qScopeGuard([&]() { vc->deleteVCWidgets({otherId}); });

        QSharedPointer<QLCInputSource> existing(new QLCInputSource(0, 7));
        existing->setID(0);
        existing->setPage(0);
        existing->setWorkingMode(QLCInputSource::Absolute);
        other->addInputSource(existing);

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        item.controlSlotID = 0;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 7;
        dst.profileName = absProfile;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);

        const VCBridge::AuthoringPreparationResult result =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(result.atomicScope, VCBridge::AuthoringPreparationResult::AtomicScope::IndependentItems);
        QCOMPARE(result.code, QStringLiteral("ambiguousSource"));
        QVERIFY(result.preparationToken.isEmpty());
    }
    else if (scenario == QStringLiteral("controller-append-fanout-allowed"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        VCButton *other = new VCButton(doc, frame);
        vc->addWidgetToMap(other);
        frame->addWidgetToPageMap(other);
        other->setLogicalGeometry(QRectF(160, 40, 100, 70));
        other->setPage(0);
        const quint32 otherId = other->id();
        auto cleanupOther = qScopeGuard([&]() { vc->deleteVCWidgets({otherId}); });

        QSharedPointer<QLCInputSource> existing(new QLCInputSource(0, 7));
        existing->setID(0);
        existing->setPage(0);
        existing->setWorkingMode(QLCInputSource::Absolute);
        other->addInputSource(existing);

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        item.controlSlotID = 0;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 7;
        dst.profileName = absProfile;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.allowFanout = true;
        req.items.append(item);

        const VCBridge::AuthoringPreparationResult prep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(prep.code, QString());
        QVERIFY(!prep.preparationToken.isEmpty());
        doc->resetModified();
        QCOMPARE(doc->isModified(), false);
        const VCBridge::AuthoringApplyResult apply =
            m_bridge->applyAuthoring(prep.preparationToken);
        QCOMPARE(apply.code, QString());
        QCOMPARE(apply.tokenConsumed, true);
        QCOMPARE(doc->isModified(), true);
    }
    else if (scenario == QStringLiteral("controller-append-fanout-stale-after-prepare"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        VCButton *other = new VCButton(doc, frame);
        vc->addWidgetToMap(other);
        frame->addWidgetToPageMap(other);
        other->setLogicalGeometry(QRectF(160, 40, 100, 70));
        other->setPage(0);
        const quint32 otherId = other->id();
        auto cleanupOther = qScopeGuard([&]() { vc->deleteVCWidgets({otherId}); });

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        item.controlSlotID = 0;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 7;
        dst.profileName = absProfile;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);

        const VCBridge::AuthoringPreparationResult prep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(prep.code, QString());
        QVERIFY(!prep.preparationToken.isEmpty());

        QSharedPointer<QLCInputSource> existing(new QLCInputSource(0, 7));
        existing->setID(0);
        existing->setPage(0);
        existing->setWorkingMode(QLCInputSource::Absolute);
        other->addInputSource(existing);

        const VCBridge::AuthoringApplyResult apply =
            m_bridge->applyAuthoring(prep.preparationToken);
        QCOMPARE(apply.code, QStringLiteral("staleSelection"));
        QCOMPARE(apply.tokenConsumed, true);
        QCOMPARE(button->inputSources().size(), 0);
    }
    else if (scenario == QStringLiteral("controller-append-fanout-noop-does-not-block"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        VCButton *fanoutPeer = new VCButton(doc, frame);
        vc->addWidgetToMap(fanoutPeer);
        frame->addWidgetToPageMap(fanoutPeer);
        fanoutPeer->setLogicalGeometry(QRectF(160, 40, 100, 70));
        fanoutPeer->setPage(0);
        const quint32 fanoutPeerId = fanoutPeer->id();

        VCButton *realTarget = new VCButton(doc, frame);
        vc->addWidgetToMap(realTarget);
        frame->addWidgetToPageMap(realTarget);
        realTarget->setLogicalGeometry(QRectF(300, 40, 100, 70));
        realTarget->setPage(0);
        const quint32 realTargetId = realTarget->id();

        auto cleanupWidgets = qScopeGuard([&]() { vc->deleteVCWidgets({fanoutPeerId, realTargetId}); });

        QSharedPointer<QLCInputSource> fanoutSrc(new QLCInputSource(0, 7));
        fanoutSrc->setID(0);
        fanoutSrc->setPage(0);
        fanoutSrc->setWorkingMode(QLCInputSource::Absolute);
        fanoutPeer->addInputSource(fanoutSrc);
        QSharedPointer<QLCInputSource> noopSrc(new QLCInputSource(0, 7));
        noopSrc->setID(0);
        noopSrc->setPage(0);
        noopSrc->setWorkingMode(QLCInputSource::Absolute);
        button->addInputSource(noopSrc);

        VCBridge::AuthoringBindingItem noopItem;
        noopItem.widgetID = buttonId;
        noopItem.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        noopItem.action = VCBridge::AuthoringBindingItem::Action::Append;
        noopItem.controlSlotID = 0;
        VCBridge::AuthoringDestination noopDst;
        noopDst.inputUniverseID = 0;
        noopDst.inputChannel = 7;
        noopDst.profileName = absProfile;
        noopItem.destination = noopDst;

        VCBridge::AuthoringBindingItem realItem;
        realItem.widgetID = realTargetId;
        realItem.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        realItem.action = VCBridge::AuthoringBindingItem::Action::Append;
        realItem.controlSlotID = 0;
        VCBridge::AuthoringDestination realDst;
        realDst.inputUniverseID = 0;
        realDst.inputChannel = 8;
        realDst.profileName = absProfile;
        realItem.destination = realDst;

        VCBridge::AuthoringBindingsRequest req;
        req.items.append(noopItem);
        req.items.append(realItem);

        const VCBridge::AuthoringPreparationResult prep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(prep.code, QString());
        QVERIFY(!prep.preparationToken.isEmpty());
        const VCBridge::AuthoringApplyResult apply =
            m_bridge->applyAuthoring(prep.preparationToken);
        QCOMPARE(apply.code, QString());
        QCOMPARE(apply.tokenConsumed, true);
        QCOMPARE(apply.items.size(), 2);
        QCOMPARE(apply.items.at(0).outcome, VCBridge::AuthoringApplyItem::Outcome::Unchanged);
        QCOMPARE(apply.items.at(1).outcome, VCBridge::AuthoringApplyItem::Outcome::Applied);
        QCOMPARE(button->inputSources().size(), 1);
        QCOMPARE(realTarget->inputSources().size(), 1);
        QCOMPARE(realTarget->inputSources().at(0)->channel() & 0x0000FFFF, quint32(8));
    }
    else if (scenario == QStringLiteral("controller-move-fanout-noop-does-not-block"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        VCButton *fanoutPeer = new VCButton(doc, frame);
        vc->addWidgetToMap(fanoutPeer);
        frame->addWidgetToPageMap(fanoutPeer);
        fanoutPeer->setLogicalGeometry(QRectF(160, 40, 100, 70));
        fanoutPeer->setPage(0);
        const quint32 fanoutPeerId = fanoutPeer->id();

        VCButton *realTarget = new VCButton(doc, frame);
        vc->addWidgetToMap(realTarget);
        frame->addWidgetToPageMap(realTarget);
        realTarget->setLogicalGeometry(QRectF(300, 40, 100, 70));
        realTarget->setPage(0);
        const quint32 realTargetId = realTarget->id();
        auto cleanupWidgets = qScopeGuard([&]() { vc->deleteVCWidgets({fanoutPeerId, realTargetId}); });

        QSharedPointer<QLCInputSource> fanoutSrc(new QLCInputSource(0, 7));
        fanoutSrc->setID(0);
        fanoutSrc->setPage(0);
        fanoutSrc->setWorkingMode(QLCInputSource::Absolute);
        fanoutPeer->addInputSource(fanoutSrc);
        QSharedPointer<QLCInputSource> noopSrc(new QLCInputSource(0, 7));
        noopSrc->setID(0);
        noopSrc->setPage(0);
        noopSrc->setWorkingMode(QLCInputSource::Absolute);
        button->addInputSource(noopSrc);

        VCBridge::AuthoringBindingItem noopMove;
        noopMove.widgetID = buttonId;
        noopMove.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        noopMove.action = VCBridge::AuthoringBindingItem::Action::Move;
        noopMove.controlSlotID = 0;
        noopMove.sourceKey = VCBridge::AuthoringSourceKey{0, 0, 7, 0};
        VCBridge::AuthoringDestination noopDst;
        noopDst.inputUniverseID = 0;
        noopDst.inputChannel = 7;
        noopDst.profileName = absProfile;
        noopMove.destination = noopDst;

        VCBridge::AuthoringBindingItem realItem;
        realItem.widgetID = realTargetId;
        realItem.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        realItem.action = VCBridge::AuthoringBindingItem::Action::Append;
        realItem.controlSlotID = 0;
        VCBridge::AuthoringDestination realDst;
        realDst.inputUniverseID = 0;
        realDst.inputChannel = 8;
        realDst.profileName = absProfile;
        realItem.destination = realDst;

        VCBridge::AuthoringBindingsRequest req;
        req.items.append(noopMove);
        req.items.append(realItem);

        const VCBridge::AuthoringPreparationResult prep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(prep.code, QString());
        QVERIFY(!prep.preparationToken.isEmpty());
        const VCBridge::AuthoringApplyResult apply =
            m_bridge->applyAuthoring(prep.preparationToken);
        QCOMPARE(apply.code, QString());
        QCOMPARE(apply.tokenConsumed, true);
        QCOMPARE(apply.items.size(), 2);
        QCOMPARE(apply.items.at(0).outcome, VCBridge::AuthoringApplyItem::Outcome::Unchanged);
        QCOMPARE(apply.items.at(1).outcome, VCBridge::AuthoringApplyItem::Outcome::Applied);
        QCOMPARE(button->inputSources().size(), 1);
        QCOMPARE(realTarget->inputSources().size(), 1);
        QCOMPARE(realTarget->inputSources().at(0)->channel() & 0x0000FFFF, quint32(8));
    }
    else if (scenario == QStringLiteral("controller-append-same-profile-channel-mutated"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        item.controlSlotID = 0;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 7;
        dst.profileName = absProfile;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);

        const VCBridge::AuthoringPreparationResult prep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(prep.code, QString());
        QVERIFY(!prep.preparationToken.isEmpty());

        QLCInputProfile *loadedProfile = ioMap->profile(absProfile);
        QVERIFY(loadedProfile != nullptr);
        QLCInputChannel *channel = loadedProfile->channel(7);
        QVERIFY(channel != nullptr);
        channel->setType(QLCInputChannel::Button);
        channel->setSendExtraPress(true);

        const VCBridge::AuthoringApplyResult apply =
            m_bridge->applyAuthoring(prep.preparationToken);
        QCOMPARE(apply.code, QStringLiteral("staleSelection"));
        QCOMPARE(apply.tokenConsumed, true);
        QCOMPARE(button->inputSources().size(), 0);
    }
    else if (scenario == QStringLiteral("controller-append-same-profile-movement-mutated"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        item.controlSlotID = 0;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 7;
        dst.profileName = absProfile;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);

        const VCBridge::AuthoringPreparationResult prep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(prep.code, QString());
        QVERIFY(!prep.preparationToken.isEmpty());

        QLCInputProfile *loadedProfile = ioMap->profile(absProfile);
        QVERIFY(loadedProfile != nullptr);
        QLCInputChannel *channel = loadedProfile->channel(7);
        QVERIFY(channel != nullptr);
        channel->setMovementType(QLCInputChannel::Relative);

        const VCBridge::AuthoringApplyResult apply =
            m_bridge->applyAuthoring(prep.preparationToken);
        QCOMPARE(apply.code, QStringLiteral("staleSelection"));
        QCOMPARE(apply.tokenConsumed, true);
        QCOMPARE(button->inputSources().size(), 0);
    }
    else if (scenario == QStringLiteral("controller-append-same-profile-limits-mutated"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        item.controlSlotID = 0;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 7;
        dst.profileName = absProfile;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);

        const VCBridge::AuthoringPreparationResult prep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(prep.code, QString());
        QVERIFY(!prep.preparationToken.isEmpty());

        QLCInputProfile *loadedProfile = ioMap->profile(absProfile);
        QVERIFY(loadedProfile != nullptr);
        QLCInputChannel *channel = loadedProfile->channel(7);
        QVERIFY(channel != nullptr);
        channel->setLowerValue(11);
        channel->setUpperValue(201);

        const VCBridge::AuthoringApplyResult apply =
            m_bridge->applyAuthoring(prep.preparationToken);
        QCOMPARE(apply.code, QStringLiteral("staleSelection"));
        QCOMPARE(apply.tokenConsumed, true);
        QCOMPARE(button->inputSources().size(), 0);
    }
    else if (scenario == QStringLiteral("controller-append-same-profile-lower-channel-mutated"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        item.controlSlotID = 0;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 7;
        dst.profileName = absProfile;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);

        const VCBridge::AuthoringPreparationResult prep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(prep.code, QString());
        QVERIFY(!prep.preparationToken.isEmpty());

        QLCInputProfile *loadedProfile = ioMap->profile(absProfile);
        QVERIFY(loadedProfile != nullptr);
        QLCInputChannel *channel = loadedProfile->channel(7);
        QVERIFY(channel != nullptr);
        channel->setLowerChannel(channel->lowerChannel() + 1);

        const VCBridge::AuthoringApplyResult apply =
            m_bridge->applyAuthoring(prep.preparationToken);
        QCOMPARE(apply.code, QStringLiteral("staleSelection"));
        QCOMPARE(apply.tokenConsumed, true);
        QCOMPARE(button->inputSources().size(), 0);
    }
    else if (scenario == QStringLiteral("controller-append-attached-profile-registry-mismatch"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        std::unique_ptr<QLCInputProfile> foreignProfile(QLCInputProfile::loader(absPath));
        QVERIFY(foreignProfile != nullptr);
        Universe *universe0 = ioMap->universe(0);
        QVERIFY(universe0 != nullptr);
        QVERIFY(universe0->setInputPatch(m_probePlugin.data(), 0, foreignProfile.get()));
        InputPatch *patched = ioMap->inputPatch(0);
        QVERIFY(patched != nullptr);
        QCOMPARE(patched->profileName(), absProfile);

        VCBridge::AuthoringBindingItem item;
        item.widgetID = buttonId;
        item.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        item.action = VCBridge::AuthoringBindingItem::Action::Append;
        item.controlSlotID = 0;
        VCBridge::AuthoringDestination dst;
        dst.inputUniverseID = 0;
        dst.inputChannel = 7;
        dst.profileName = absProfile;
        item.destination = dst;
        VCBridge::AuthoringBindingsRequest req;
        req.items.append(item);

        const VCBridge::AuthoringPreparationResult prep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(prep.code, QStringLiteral("unsupportedPreparation"));
        QVERIFY(prep.preparationToken.isEmpty());
        QVERIFY(prep.error.contains(QStringLiteral("profile"), Qt::CaseInsensitive));
        QVERIFY(prep.error.contains(QStringLiteral("mismatch"), Qt::CaseInsensitive));
    }
    else if (scenario == QStringLiteral("controller-page-slot2-routing"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        VCBridge::AuthoringBindingItem appendItem;
        appendItem.widgetID = page->id();
        appendItem.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        appendItem.action = VCBridge::AuthoringBindingItem::Action::Append;
        appendItem.controlSlotID = 2;
        VCBridge::AuthoringDestination appendDst;
        appendDst.inputUniverseID = 0;
        appendDst.inputChannel = 8;
        appendDst.profileName = absProfile;
        appendItem.destination = appendDst;
        VCBridge::AuthoringBindingsRequest appendReq;
        appendReq.items.append(appendItem);

        VCBridge::AuthoringPreparationResult appendPrep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(appendReq));
        QCOMPARE(appendPrep.code, QString());
        QVERIFY(!appendPrep.preparationToken.isEmpty());
        VCBridge::AuthoringApplyResult appendApply =
            m_bridge->applyAuthoring(appendPrep.preparationToken);
        QCOMPARE(appendApply.code, QString());
        QCOMPARE(appendApply.tokenConsumed, true);

        const quint32 key8 = quint32((0u << 16) | 8u);
        QVERIFY(page->hasInputSourceForKey(key8));

        VCBridge::AuthoringBindingItem moveItem = appendItem;
        moveItem.action = VCBridge::AuthoringBindingItem::Action::Move;
        moveItem.sourceKey = VCBridge::AuthoringSourceKey{2, 0, 8, 0};
        VCBridge::AuthoringDestination moveDst;
        moveDst.inputUniverseID = 0;
        moveDst.inputChannel = 7;
        moveDst.profileName = absProfile;
        moveItem.destination = moveDst;
        VCBridge::AuthoringBindingsRequest moveReq;
        moveReq.items.append(moveItem);
        VCBridge::AuthoringPreparationResult movePrep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(moveReq));
        QCOMPARE(movePrep.code, QString());
        QVERIFY(!movePrep.preparationToken.isEmpty());
        VCBridge::AuthoringApplyResult moveApply =
            m_bridge->applyAuthoring(movePrep.preparationToken);
        QCOMPARE(moveApply.code, QString());
        QCOMPARE(moveApply.tokenConsumed, true);

        const quint32 key7 = quint32((0u << 16) | 7u);
        QVERIFY(!page->hasInputSourceForKey(key8));
        QVERIFY(page->hasInputSourceForKey(key7));

        VCBridge::AuthoringBindingItem removeItem;
        removeItem.widgetID = page->id();
        removeItem.kind = VCBridge::AuthoringBindingItem::Kind::Controller;
        removeItem.action = VCBridge::AuthoringBindingItem::Action::Remove;
        removeItem.controlSlotID = 2;
        removeItem.sourceKey = VCBridge::AuthoringSourceKey{2, 0, 7, 0};
        VCBridge::AuthoringBindingsRequest removeReq;
        removeReq.items.append(removeItem);
        VCBridge::AuthoringPreparationResult removePrep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(removeReq));
        QCOMPARE(removePrep.code, QString());
        QVERIFY(!removePrep.preparationToken.isEmpty());
        VCBridge::AuthoringApplyResult removeApply =
            m_bridge->applyAuthoring(removePrep.preparationToken);
        QCOMPARE(removeApply.code, QString());
        QCOMPARE(removeApply.tokenConsumed, true);
        QVERIFY(!page->hasInputSourceForKey(key7));
    }
    else if (scenario == QStringLiteral("controller-nonzero-slot-subpage-roundtrip"))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile), "Failed to patch abs profile");

        frame->setTotalPagesNumber(2);
        VCSlider *pagedSlider = new VCSlider(doc, frame);
        vc->addWidgetToMap(pagedSlider);
        frame->addWidgetToPageMap(pagedSlider);
        pagedSlider->setSliderMode(VCSlider::Level);
        pagedSlider->setMonitorEnabled(true);
        pagedSlider->setLogicalGeometry(QRectF(320, 140, 110, 120));
        pagedSlider->setPage(1);
        const quint32 pagedSliderId = pagedSlider->id();
        auto cleanupSlider = qScopeGuard([&]() { vc->deleteVCWidgets({pagedSliderId}); });

        fastmcpp::tools::ToolManager tmInput;
        fastmcpp::tools::ToolManager tmUpdate;
        registerVCInputTools(tmInput, doc, m_bridge.get());
        registerVCUpdateTools(tmUpdate, doc, m_bridge.get());
        QVERIFY(tmInput.has("vc_prepare_bindings"));
        QVERIFY(tmUpdate.has("vc_apply_preparation"));

        const int baselineOpen = counter("openCount");
        const int baselineClose = counter("closeCount");
        const int baselineWrite = counter("writeCount");
        const int baselineFeedback = counter("feedbackCount");

        const Json appendItem = {
            {"widgetID", pagedSliderId},
            {"kind", "controller"},
            {"action", "append"},
            {"controlSlotID", 1},
            {"destination", {{"inputUniverseID", 0}, {"inputChannel", 8},
                             {"profileName", absProfile.toStdString()}}}
        };
        const Json appendPrep = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", Json::array({appendItem})}}));
        QVERIFY2(!appendPrep.contains("error"), appendPrep.dump().c_str());
        QVERIFY(appendPrep.contains("preparationToken"));
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
        const Json appendApply = parseInvoke(tmUpdate.invoke("vc_apply_preparation",
            Json{{"preparationToken", appendPrep.at("preparationToken")}}));
        QVERIFY2(appendApply.value("applied", false), appendApply.dump().c_str());
        QCOMPARE(appendApply.value("tokenConsumed", false), true);
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
        QCOMPARE(pagedSlider->inputSources().size(), 1);
        QSharedPointer<QLCInputSource> appendedSource = pagedSlider->inputSources().at(0);
        QVERIFY(!appendedSource.isNull());
        QCOMPARE(appendedSource->id(), quint32(1));
        QCOMPARE(appendedSource->universe(), quint32(0));
        QCOMPARE(appendedSource->channel() & 0x0000FFFF, quint32(8));
        QCOMPARE(appendedSource->channel(), quint32((1u << 16) | 8u));
        QCOMPARE(int(appendedSource->page()), 1);

        Json moveItem = appendItem;
        moveItem["action"] = "move";
        moveItem["sourceKey"] = {{"controlSlotID", 1}, {"inputUniverseID", 0},
                                 {"inputChannel", 8}, {"childSubpage", 1}};
        moveItem["destination"]["inputChannel"] = 7;
        Json wrongSubpageMove = moveItem;
        wrongSubpageMove["sourceKey"]["childSubpage"] = 0;
        const QByteArray beforeWrongSubpageMove = serializeConsole();
        const Json refusedMove = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", Json::array({wrongSubpageMove})}}));
        QCOMPARE(refusedMove.value("code", std::string()), std::string("ambiguousSource"));
        QCOMPARE(refusedMove.value("preparationToken", std::string()), std::string());
        QCOMPARE(serializeConsole(), beforeWrongSubpageMove);
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
        const Json movePrep = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", Json::array({moveItem})}}));
        QVERIFY2(!movePrep.contains("error"), movePrep.dump().c_str());
        QVERIFY(movePrep.contains("preparationToken"));
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
        const Json moveApply = parseInvoke(tmUpdate.invoke("vc_apply_preparation",
            Json{{"preparationToken", movePrep.at("preparationToken")}}));
        QVERIFY2(moveApply.value("applied", false), moveApply.dump().c_str());
        QCOMPARE(moveApply.value("tokenConsumed", false), true);
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
        QCOMPARE(pagedSlider->inputSources().size(), 1);
        QCOMPARE(pagedSlider->inputSources().at(0)->id(), quint32(1));
        QCOMPARE(pagedSlider->inputSources().at(0)->channel() & 0x0000FFFF, quint32(7));
        QCOMPARE(pagedSlider->inputSources().at(0)->channel(), quint32((1u << 16) | 7u));
        QCOMPARE(int(pagedSlider->inputSources().at(0)->page()), 1);

        QByteArray persistedXml;
        QXmlStreamWriter writer(&persistedXml);
        writer.writeStartDocument();
        QVERIFY(pagedSlider->saveXML(&writer));
        writer.writeEndDocument();
        QVERIFY(!writer.hasError());
        std::unique_ptr<VCSlider> loadedSlider = std::make_unique<VCSlider>(doc, nullptr);
        QBuffer readBuffer(&persistedXml);
        QVERIFY(readBuffer.open(QIODevice::ReadOnly));
        QXmlStreamReader reader(&readBuffer);
        QVERIFY(reader.readNextStartElement());
        QVERIFY2(loadedSlider->loadXML(reader), "VCSlider::loadXML failed for nonzero slot/subpage");
        QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
        QCOMPARE(loadedSlider->id(), pagedSliderId);
        QCOMPARE(loadedSlider->page(), 1);
        QCOMPARE(loadedSlider->inputSources().size(), 1);
        const QSharedPointer<QLCInputSource> loadedSource = loadedSlider->inputSources().at(0);
        QVERIFY(!loadedSource.isNull());
        QCOMPARE(loadedSource->id(), quint32(1));
        QCOMPARE(loadedSource->universe(), quint32(0));
        QCOMPARE(loadedSource->channel(), pagedSlider->inputSources().at(0)->channel());
        QCOMPARE(loadedSource->channel(), quint32((1u << 16) | 7u));
        QCOMPARE(int(loadedSource->page()), 1);
        QCOMPARE(loadedSource->workingMode(), pagedSlider->inputSources().at(0)->workingMode());
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);

        const Json removeItem = {
            {"widgetID", pagedSliderId},
            {"kind", "controller"},
            {"action", "remove"},
            {"controlSlotID", 1},
            {"sourceKey", {{"controlSlotID", 1}, {"inputUniverseID", 0},
                           {"inputChannel", 7}, {"childSubpage", 1}}}
        };
        Json wrongSubpageRemove = removeItem;
        wrongSubpageRemove["sourceKey"]["childSubpage"] = 0;
        const QByteArray beforeWrongSubpageRemove = serializeConsole();
        const Json refusedRemove = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", Json::array({wrongSubpageRemove})}}));
        QCOMPARE(refusedRemove.value("code", std::string()), std::string("ambiguousSource"));
        QCOMPARE(refusedRemove.value("preparationToken", std::string()), std::string());
        QCOMPARE(serializeConsole(), beforeWrongSubpageRemove);
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
        const Json removePrep = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", Json::array({removeItem})}}));
        QVERIFY2(!removePrep.contains("error"), removePrep.dump().c_str());
        QVERIFY(removePrep.contains("preparationToken"));
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
        const Json removeApply = parseInvoke(tmUpdate.invoke("vc_apply_preparation",
            Json{{"preparationToken", removePrep.at("preparationToken")}}));
        QVERIFY2(removeApply.value("applied", false), removeApply.dump().c_str());
        QCOMPARE(removeApply.value("tokenConsumed", false), true);
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
        QCOMPARE(pagedSlider->inputSources().size(), 0);
    }
    else if (scenario.startsWith(QStringLiteral("controller-source-lifetime-same-key-")))
    {
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile),
                 "Failed to patch source-lifetime profile");

        fastmcpp::tools::ToolManager tmInput;
        fastmcpp::tools::ToolManager tmUpdate;
        registerVCInputTools(tmInput, doc, m_bridge.get());
        registerVCUpdateTools(tmUpdate, doc, m_bridge.get());
        QVERIFY(tmInput.has("vc_prepare_bindings"));
        QVERIFY(tmInput.has("vc_query_controls"));
        QVERIFY(tmUpdate.has("vc_apply_preparation"));

        resetProbeCounters();
        auto verifyCounters = [&]() {
            QCOMPARE(counter("openCount"), 0);
            QCOMPARE(counter("closeCount"), 0);
            QCOMPARE(counter("writeCount"), 0);
            QCOMPARE(counter("feedbackCount"), 0);
        };
        auto readControls = [&]() {
            return parseInvoke(tmInput.invoke("vc_query_controls", Json{
                {"kind", "targets"}, {"widgetIDs", Json::array({buttonId})},
                {"fields", Json::array({"identity", "bindings"})}, {"limit", 100}
            }));
        };
        const Json append = {
            {"widgetID", buttonId}, {"kind", "controller"}, {"action", "append"},
            {"controlSlotID", 0},
            {"destination", {{"inputUniverseID", 0}, {"inputChannel", 7},
                             {"profileName", absProfile.toStdString()}}}
        };
        const Json seedPrep = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", Json::array({append})}}));
        QVERIFY2(!seedPrep.contains("error"), seedPrep.dump().c_str());
        QVERIFY(!seedPrep.at("preparationToken").get<std::string>().empty());
        verifyCounters();
        const Json seedApply = parseInvoke(tmUpdate.invoke("vc_apply_preparation",
            Json{{"preparationToken", seedPrep.at("preparationToken")}}));
        QVERIFY2(seedApply.value("applied", false), seedApply.dump().c_str());
        QCOMPARE(seedApply.value("tokenConsumed", false), true);
        QCOMPARE(button->inputSources().size(), 1);
        QSharedPointer<QLCInputSource> original = button->inputSources().at(0);
        QVERIFY(!original.isNull());
        QPointer<QLCInputSource> originalLifetime(original.data());
        const Json originalControls = readControls();
        QVERIFY2(!originalControls.contains("error"), originalControls.dump().c_str());
        int matchedSlots = 0;
        for (const Json &row : originalControls.at("rows"))
        {
            if (row.value("widgetID", -1) != int(buttonId) ||
                row.value("controlSlotID", -1) != 0)
                continue;
            ++matchedSlots;
            QCOMPARE(row.at("inputBindings").size(), size_t(1));
            const Json &key = row.at("inputBindings").at(0).at("sourceKey");
            QCOMPARE(key.at("controlSlotID").get<int>(), 0);
            QCOMPARE(key.at("inputUniverseID").get<int>(), 0);
            QCOMPARE(key.at("inputChannel").get<int>(), 7);
            QCOMPARE(key.at("childSubpage").get<int>(), 0);
        }
        QCOMPARE(matchedSlots, 1);
        const QByteArray originalXml = serializeConsole();
        doc->resetModified();
        verifyCounters();

        const bool moving = scenario.endsWith(QStringLiteral("-move-refused"));
        Json operation = append;
        operation["action"] = moving ? "move" : "remove";
        operation["sourceKey"] = {{"controlSlotID", 0}, {"inputUniverseID", 0},
                                  {"inputChannel", 7}, {"childSubpage", 0}};
        if (moving)
            operation["destination"]["inputChannel"] = 8;
        else
            operation.erase("destination");
        const Json prepared = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", Json::array({operation})}}));
        QVERIFY2(!prepared.contains("error"), prepared.dump().c_str());
        QVERIFY(!prepared.at("preparationToken").get<std::string>().empty());
        QCOMPARE(prepared.at("items").size(), size_t(1));
        QCOMPARE(prepared.at("items").at(0).at("outcome").get<std::string>(), std::string("prepared"));
        QCOMPARE(readControls(), originalControls);
        QCOMPARE(serializeConsole(), originalXml);
        QCOMPARE(button->inputSources().at(0).data(), original.data());
        QCOMPARE(doc->isModified(), false);
        verifyCounters();

        // Keep the old allocation alive until the identical replacement exists.
        QSharedPointer<QLCInputSource> replacement(new QLCInputSource(0, 7));
        replacement->setID(0);
        replacement->setPage(0);
        QVERIFY(replacement.data() != original.data());
        vc->deleteInputSource(button, original->id(), original->universe(), original->channel());
        QCOMPARE(button->inputSources().size(), 0);
        button->addInputSource(replacement);
        page->mapInputSource(replacement, button, true);
        vc->updatePageInputs();
        original.clear();
        QVERIFY(originalLifetime.isNull());
        QCOMPARE(button->inputSources().size(), 1);
        QCOMPARE(button->inputSources().at(0).data(), replacement.data());
        QCOMPARE(readControls(), originalControls);
        QCOMPARE(serializeConsole(), originalXml);
        doc->resetModified();
        verifyCounters();

        auto verifyReplacement = [&]() {
            QCOMPARE(button->inputSources().size(), 1);
            QCOMPARE(button->inputSources().at(0).data(), replacement.data());
            QCOMPARE(replacement->id(), quint32(0));
            QCOMPARE(replacement->universe(), quint32(0));
            QCOMPARE(replacement->channel(), quint32(7));
            QCOMPARE(int(replacement->page()), 0);
            QCOMPARE(replacement->workingMode(), QLCInputSource::Absolute);
            QVERIFY(originalLifetime.isNull());
            QCOMPARE(readControls(), originalControls);
            QCOMPARE(serializeConsole(), originalXml);
            QCOMPARE(doc->isModified(), false);
            verifyCounters();
        };
        verifyReplacement();
        const Json refused = parseInvoke(tmUpdate.invoke("vc_apply_preparation",
            Json{{"preparationToken", prepared.at("preparationToken")}}));
        QCOMPARE(refused.value("code", std::string()), std::string("staleSelection"));
        QCOMPARE(refused.value("error", std::string()), std::string("Guard identity or configuration changed"));
        QCOMPARE(refused.value("applied", true), false);
        QCOMPARE(refused.value("tokenConsumed", false), true);
        QCOMPARE(refused.at("atomicScope").get<std::string>(), std::string("independentItems"));
        QVERIFY(refused.at("items").empty());
        verifyReplacement();

        const Json replay = parseInvoke(tmUpdate.invoke("vc_apply_preparation",
            Json{{"preparationToken", prepared.at("preparationToken")}}));
        QCOMPARE(replay.value("code", std::string()), std::string("invalidPreparation"));
        QCOMPARE(replay.value("applied", true), false);
        QCOMPARE(replay.value("tokenConsumed", true), false);
        QVERIFY(replay.at("items").empty());
        verifyReplacement();
    }
    else if (scenario.startsWith(QStringLiteral("controller-bad-second-refusal-")))
    {
        VCButton *other = new VCButton(doc, frame);
        vc->addWidgetToMap(other);
        frame->addWidgetToPageMap(other);
        other->setLogicalGeometry(QRectF(160, 40, 100, 70));
        other->setPage(0);
        const quint32 otherId = other->id();
        auto cleanupOther = qScopeGuard([&]() { vc->deleteVCWidgets({otherId}); });

        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile),
                 "Failed to patch bad-second-refusal profile");

        fastmcpp::tools::ToolManager tmInput;
        fastmcpp::tools::ToolManager tmUpdate;
        registerVCInputTools(tmInput, doc, m_bridge.get());
        registerVCUpdateTools(tmUpdate, doc, m_bridge.get());
        QVERIFY(tmInput.has("vc_prepare_bindings"));
        QVERIFY(tmInput.has("vc_query_controls"));
        QVERIFY(tmUpdate.has("vc_apply_preparation"));

        resetProbeCounters();
        auto verifyCounters = [&]() {
            QCOMPARE(counter("openCount"), 0);
            QCOMPARE(counter("closeCount"), 0);
            QCOMPARE(counter("writeCount"), 0);
            QCOMPARE(counter("feedbackCount"), 0);
        };
        const Json append = {
            {"widgetID", buttonId}, {"kind", "controller"}, {"action", "append"},
            {"controlSlotID", 0},
            {"destination", {{"inputUniverseID", 0}, {"inputChannel", 7},
                             {"profileName", absProfile.toStdString()}}}
        };
        doc->resetModified();
        const Json seedPrep = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", Json::array({append})}}));
        QVERIFY2(!seedPrep.contains("error"), seedPrep.dump().c_str());
        QVERIFY(!seedPrep.at("preparationToken").get<std::string>().empty());
        QCOMPARE(button->inputSources().size(), 0);
        QCOMPARE(doc->isModified(), false);
        verifyCounters();
        const Json seedApply = parseInvoke(tmUpdate.invoke("vc_apply_preparation",
            Json{{"preparationToken", seedPrep.at("preparationToken")}}));
        QVERIFY2(seedApply.value("applied", false), seedApply.dump().c_str());
        QCOMPARE(seedApply.value("tokenConsumed", false), true);
        QCOMPARE(doc->isModified(), true);
        QCOMPARE(button->inputSources().size(), 1);
        const QSharedPointer<QLCInputSource> committedSource = button->inputSources().at(0);
        QVERIFY(!committedSource.isNull());

        auto readControls = [&]() {
            return parseInvoke(tmInput.invoke("vc_query_controls", Json{
                {"kind", "targets"}, {"widgetIDs", Json::array({buttonId})},
                {"fields", Json::array({"identity", "bindings"})}, {"limit", 100}
            }));
        };
        const Json committedControls = readControls();
        QVERIFY2(!committedControls.contains("error"), committedControls.dump().c_str());
        int matchedSlots = 0;
        for (const Json &row : committedControls.at("rows"))
        {
            if (row.value("widgetID", -1) != int(buttonId) ||
                row.value("controlSlotID", -1) != 0)
                continue;
            ++matchedSlots;
            QCOMPARE(row.at("inputBindings").size(), size_t(1));
            const Json &key = row.at("inputBindings").at(0).at("sourceKey");
            QCOMPARE(key.at("controlSlotID").get<int>(), 0);
            QCOMPARE(key.at("inputUniverseID").get<int>(), 0);
            QCOMPARE(key.at("inputChannel").get<int>(), 7);
            QCOMPARE(key.at("childSubpage").get<int>(), 0);
        }
        QCOMPARE(matchedSlots, 1);
        const QByteArray committedXml = serializeConsole();
        const bool dirtyBefore = scenario.endsWith(QStringLiteral("-dirty"));
        if (!dirtyBefore)
            doc->resetModified();
        QCOMPARE(doc->isModified(), dirtyBefore);
        auto verifyCommittedState = [&]() {
            QCOMPARE(serializeConsole(), committedXml);
            QCOMPARE(readControls(), committedControls);
            QCOMPARE(button->inputSources().size(), 1);
            QCOMPARE(button->inputSources().at(0).data(), committedSource.data());
            QCOMPARE(committedSource->id(), quint32(0));
            QCOMPARE(committedSource->universe(), quint32(0));
            QCOMPARE(committedSource->channel(), quint32(7));
            QCOMPARE(int(committedSource->page()), 0);
            QCOMPARE(committedSource->workingMode(), QLCInputSource::Absolute);
            QCOMPARE(other->inputSources().size(), 0);
            QCOMPARE(doc->isModified(), dirtyBefore);
            verifyCounters();
        };
        verifyCommittedState();

        Json move = append;
        move["action"] = "move";
        move["sourceKey"] = {{"controlSlotID", 0}, {"inputUniverseID", 0},
                             {"inputChannel", 7}, {"childSubpage", 0}};
        move["destination"]["inputChannel"] = 8;
        Json invalid = append;
        invalid["widgetID"] = otherId;
        invalid["controlSlotID"] = 255;
        invalid["destination"]["inputChannel"] = 9;
        const Json refusedPrep = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", Json::array({move, invalid})}}));
        QVERIFY2(refusedPrep.contains("items"), refusedPrep.dump().c_str());
        QCOMPARE(refusedPrep.value("code", std::string()), std::string("invalidArgument"));
        QCOMPARE(refusedPrep.at("preparationToken").get<std::string>(), std::string());
        QCOMPARE(refusedPrep.at("atomicScope").get<std::string>(), std::string("independentItems"));
        QCOMPARE(refusedPrep.at("items").size(), size_t(2));
        for (int index = 0; index < 2; ++index)
        {
            const Json &item = refusedPrep.at("items").at(index);
            QCOMPARE(item.at("index").get<int>(), index);
            QCOMPARE(item.at("targetWidgetID").get<int>(), int(index == 0 ? buttonId : otherId));
            QCOMPARE(item.at("selectedControlSlotID").get<int>(), index == 0 ? 0 : 255);
            QCOMPARE(item.at("outcome").get<std::string>(),
                     index == 0 ? std::string("prepared") : std::string("refused"));
        }
        QCOMPARE(refusedPrep.at("items").at(1).at("code").get<std::string>(),
                 std::string("invalidArgument"));
        verifyCommittedState();

        const Json replay = parseInvoke(tmUpdate.invoke("vc_apply_preparation",
            Json{{"preparationToken", seedPrep.at("preparationToken")}}));
        QCOMPARE(replay.value("code", std::string()), std::string("invalidPreparation"));
        QCOMPARE(replay.value("applied", true), false);
        verifyCommittedState();
    }
    else if (scenario.startsWith(QStringLiteral("controller-mixed-outcomes-")))
    {
        VCButton *other = new VCButton(doc, frame);
        vc->addWidgetToMap(other);
        frame->addWidgetToPageMap(other);
        other->setLogicalGeometry(QRectF(160, 40, 100, 70));
        other->setPage(0);
        other->setFunctionID(Function::invalidId());
        const quint32 otherId = other->id();
        auto cleanupOther = qScopeGuard([&]() { vc->deleteVCWidgets({otherId}); });

        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(absProfile));
        QVERIFY2(ioMap->setInputPatch(0, m_probeName, "", "", 0, absProfile),
                 "Failed to patch mixed-outcomes profile");

        fastmcpp::tools::ToolManager tmInput;
        fastmcpp::tools::ToolManager tmUpdate;
        registerVCInputTools(tmInput, doc, m_bridge.get());
        registerVCUpdateTools(tmUpdate, doc, m_bridge.get());
        QVERIFY(tmInput.has("vc_prepare_bindings"));
        QVERIFY(tmInput.has("vc_query_controls"));
        QVERIFY(tmUpdate.has("vc_apply_preparation"));

        resetProbeCounters();
        auto verifyCounters = [&]() {
            QCOMPARE(counter("openCount"), 0);
            QCOMPARE(counter("closeCount"), 0);
            QCOMPARE(counter("writeCount"), 0);
            QCOMPARE(counter("feedbackCount"), 0);
        };
        auto readControls = [&]() {
            return parseInvoke(tmInput.invoke("vc_query_controls", Json{
                {"kind", "targets"},
                {"widgetIDs", Json::array({buttonId})},
                {"fields", Json::array({"identity", "bindings"})},
                {"limit", 100}
            }));
        };
        auto verifyReadback = [&](int channel) {
            const Json controls = readControls();
            QVERIFY2(!controls.contains("error"), controls.dump().c_str());
            QVERIFY(controls.contains("rows"));
            QVERIFY(controls.at("rows").is_array());
            int matchedSlots = 0;
            for (const Json &row : controls.at("rows"))
            {
                if (row.value("widgetID", -1) != int(buttonId) ||
                    row.value("controlSlotID", -1) != 0)
                    continue;
                ++matchedSlots;
                const Json &bindings = row.at("inputBindings");
                QCOMPARE(bindings.size(), size_t(1));
                const Json &key = bindings.at(0).at("sourceKey");
                QCOMPARE(key.at("controlSlotID").get<int>(), 0);
                QCOMPARE(key.at("inputUniverseID").get<int>(), 0);
                QCOMPARE(key.at("inputChannel").get<int>(), channel);
                QCOMPARE(key.at("childSubpage").get<int>(), 0);
            }
            QCOMPARE(matchedSlots, 1);
            QCOMPARE(button->inputSources().size(), 1);
            const QSharedPointer<QLCInputSource> source = button->inputSources().at(0);
            QVERIFY(!source.isNull());
            QCOMPARE(source->id(), quint32(0));
            QCOMPARE(source->universe(), quint32(0));
            QCOMPARE(source->channel(), quint32(channel));
            QCOMPARE(int(source->page()), 0);
            verifyCounters();
        };

        const Json append = {
            {"widgetID", buttonId}, {"kind", "controller"}, {"action", "append"},
            {"controlSlotID", 0},
            {"destination", {{"inputUniverseID", 0}, {"inputChannel", 7},
                             {"profileName", absProfile.toStdString()}}}
        };
        const Json seedPrep = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", Json::array({append})}}));
        QVERIFY2(!seedPrep.contains("error"), seedPrep.dump().c_str());
        QVERIFY(!seedPrep.at("preparationToken").get<std::string>().empty());
        QCOMPARE(button->inputSources().size(), 0);
        verifyCounters();
        const Json seedApply = parseInvoke(tmUpdate.invoke("vc_apply_preparation",
            Json{{"preparationToken", seedPrep.at("preparationToken")}}));
        QVERIFY2(seedApply.value("applied", false), seedApply.dump().c_str());
        QCOMPARE(seedApply.value("tokenConsumed", false), true);
        verifyReadback(7);

        Json move = append;
        move["action"] = "move";
        move["sourceKey"] = {{"controlSlotID", 0}, {"inputUniverseID", 0},
                             {"inputChannel", 7}, {"childSubpage", 0}};
        move["destination"]["inputChannel"] = 8;
        Json invalid = append;
        invalid["widgetID"] = otherId;
        invalid["controlSlotID"] = 255;
        invalid["destination"]["inputChannel"] = 9;
        const bool refusedFirst = scenario.endsWith(QStringLiteral("refused-first"));
        const Json mixedItems = refusedFirst ? Json::array({invalid, move})
                                             : Json::array({move, invalid});
        const QByteArray committedXml = serializeConsole();
        doc->resetModified();
        const Json mixedPrep = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", mixedItems}}));
        QVERIFY2(mixedPrep.contains("items"), mixedPrep.dump().c_str());
        QCOMPARE(mixedPrep.value("code", std::string()), std::string("invalidArgument"));
        QCOMPARE(mixedPrep.value("preparationToken", std::string()), std::string());
        QCOMPARE(mixedPrep.at("atomicScope").get<std::string>(), std::string("independentItems"));
        QCOMPARE(mixedPrep.at("items").size(), size_t(2));
        for (int index = 0; index < 2; ++index)
        {
            const Json &item = mixedPrep.at("items").at(index);
            const bool refused = (index == (refusedFirst ? 0 : 1));
            QCOMPARE(item.at("index").get<int>(), index);
            QCOMPARE(item.at("targetWidgetID").get<int>(), int(refused ? otherId : buttonId));
            QCOMPARE(item.at("selectedControlSlotID").get<int>(), refused ? 255 : 0);
            QCOMPARE(item.at("outcome").get<std::string>(),
                     refused ? std::string("refused") : std::string("prepared"));
            if (refused)
                QCOMPARE(item.at("code").get<std::string>(), std::string("invalidArgument"));
        }
        QCOMPARE(serializeConsole(), committedXml);
        QCOMPARE(doc->isModified(), false);
        verifyReadback(7);
        QCOMPARE(other->inputSources().size(), 0);

        const Json unchanged = {
            {"widgetID", otherId}, {"kind", "function"}, {"action", "clear"}
        };
        const Json acceptedPrep = parseInvoke(tmInput.invoke("vc_prepare_bindings",
            Json{{"items", Json::array({unchanged, move})}}));
        QVERIFY2(!acceptedPrep.contains("error"), acceptedPrep.dump().c_str());
        QVERIFY(!acceptedPrep.at("preparationToken").get<std::string>().empty());
        QCOMPARE(acceptedPrep.at("items").size(), size_t(2));
        QCOMPARE(acceptedPrep.at("items").at(0).at("outcome").get<std::string>(), std::string("unchanged"));
        QCOMPARE(acceptedPrep.at("items").at(1).at("outcome").get<std::string>(), std::string("prepared"));
        QCOMPARE(serializeConsole(), committedXml);
        QCOMPARE(doc->isModified(), false);
        verifyReadback(7);
        const Json acceptedApply = parseInvoke(tmUpdate.invoke("vc_apply_preparation",
            Json{{"preparationToken", acceptedPrep.at("preparationToken")}}));
        QVERIFY2(!acceptedApply.contains("error"), acceptedApply.dump().c_str());
        QCOMPARE(acceptedApply.at("atomicScope").get<std::string>(), std::string("independentItems"));
        QCOMPARE(acceptedApply.value("applied", false), true);
        QCOMPARE(acceptedApply.value("tokenConsumed", false), true);
        QCOMPARE(acceptedApply.at("items").size(), size_t(2));
        for (int index = 0; index < 2; ++index)
        {
            const Json &item = acceptedApply.at("items").at(index);
            QCOMPARE(item.at("index").get<int>(), index);
            QCOMPARE(item.at("targetWidgetID").get<int>(), int(index == 0 ? otherId : buttonId));
            QCOMPARE(item.at("outcome").get<std::string>(),
                     index == 0 ? std::string("unchanged") : std::string("applied"));
        }
        QCOMPARE(doc->isModified(), true);
        verifyReadback(8);
        QCOMPARE(other->inputSources().size(), 0);
        QCOMPARE(other->functionID(), Function::invalidId());
    }
    else // geometry-profile-load-between-prepare-and-apply
    {
        Universe *universe0 = ioMap->universe(0);
        QVERIFY(universe0 != nullptr);
        std::unique_ptr<QLCInputProfile> unloadedProfile(QLCInputProfile::loader(absPath));
        QVERIFY(unloadedProfile != nullptr);
        QVERIFY(universe0->setInputPatch(m_probePlugin.data(), 0, unloadedProfile.get()));
        InputPatch *patch = ioMap->inputPatch(0);
        QVERIFY(patch != nullptr);
        QCOMPARE(patch->profileName(), unloadedProfile->name());
        QVERIFY(!ioMap->profileNames().contains(unloadedProfile->name()));

        const QRectF buttonBefore = button->logicalGeometry();
        QSharedPointer<QLCInputSource> existing(new QLCInputSource(0, 7));
        existing->setID(0);
        existing->setPage(0);
        existing->setWorkingMode(QLCInputSource::Absolute);
        button->addInputSource(existing);

        const QByteArray xmlBefore = serializeConsole();
        const bool dirtyBefore = doc->isModified();
        resetProbeCounters();
        const int baselineOpen = counter("openCount");
        const int baselineClose = counter("closeCount");
        const int baselineWrite = counter("writeCount");
        const int baselineFeedback = counter("feedbackCount");

        VCBridge::AuthoringLayoutRequest req;
        req.operation = VCBridge::AuthoringLayoutRequest::Operation::Geometry;
        VCBridge::AuthoringGeometryChange change;
        change.widgetID = buttonId;
        change.logicalRect = QRectF(220, 120, 100, 70);
        req.geometryChanges.append(change);

        const VCBridge::AuthoringPreparationResult prep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
        QCOMPARE(prep.code, QString());
        QVERIFY(!prep.preparationToken.isEmpty());
        QVERIFY(!ioMap->profileNames().contains(unloadedProfile->name()));
        ioMap->loadProfiles(QDir(m_workspaceDir.path()));
        QVERIFY(ioMap->profileNames().contains(unloadedProfile->name()));
        const VCBridge::AuthoringApplyResult apply =
            m_bridge->applyAuthoring(prep.preparationToken);
        QCOMPARE(apply.code, QString());
        QCOMPARE(apply.applied, true);
        QCOMPARE(button->logicalGeometry(), QRectF(220, 120, 100, 70));
        QCOMPARE(counter("openCount"), baselineOpen);
        QCOMPARE(counter("closeCount"), baselineClose);
        QCOMPARE(counter("writeCount"), baselineWrite);
        QCOMPARE(counter("feedbackCount"), baselineFeedback);
        QCOMPARE(doc->isModified(), dirtyBefore);
        QVERIFY(serializeConsole() != xmlBefore);
        QVERIFY(button->logicalGeometry() != buttonBefore);
    }
}

void VCAuthoringOwner_Test::authoringGeometry_interdependentGroup_swap()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);

    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 500, 300));

    VCButton *left = new VCButton(doc, frame);
    vc->addWidgetToMap(left);
    frame->addWidgetToPageMap(left);
    left->setLogicalGeometry(QRectF(20, 60, 100, 80));

    VCButton *right = new VCButton(doc, frame);
    vc->addWidgetToMap(right);
    frame->addWidgetToPageMap(right);
    right->setLogicalGeometry(QRectF(140, 60, 100, 80));

    const quint32 frameId = frame->id();
    const quint32 leftId = left->id();
    const quint32 rightId = right->id();
    auto cleanup = qScopeGuard([&]() { vc->deleteVCWidgets({leftId, rightId, frameId}); });

    const QRectF leftBefore = left->logicalGeometry();
    const QRectF rightBefore = right->logicalGeometry();

    VCBridge::AuthoringLayoutRequest req;
    VCBridge::AuthoringGeometryChange a;
    a.widgetID = leftId;
    a.logicalRect = rightBefore;
    req.geometryChanges.append(a);
    VCBridge::AuthoringGeometryChange b;
    b.widgetID = rightId;
    b.logicalRect = leftBefore;
    req.geometryChanges.append(b);

    const VCBridge::AuthoringPreparationResult prep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(prep.code, QString());
    QVERIFY(!prep.preparationToken.isEmpty());
    QVERIFY(!prep.candidateDigest.isEmpty());
    QCOMPARE(prep.atomicScope, VCBridge::AuthoringPreparationResult::AtomicScope::NativeGroup);

    const VCBridge::AuthoringApplyResult apply =
        m_bridge->applyAuthoring(prep.preparationToken);
    QCOMPARE(apply.code, QString());
    QCOMPARE(apply.applied, true);
    QCOMPARE(apply.atomicScope, VCBridge::AuthoringApplyResult::AtomicScope::NativeGroup);
    QCOMPARE(left->logicalGeometry(), rightBefore);
    QCOMPARE(right->logicalGeometry(), leftBefore);
}

void VCAuthoringOwner_Test::authoringGeometry_prepareApply_guardsAndLimits()
{
    QVERIFY2(m_app, "App not initialized");
    QVERIFY2(m_bridge, "Bridge not initialized");
    m_bridge->clearAuthoringNowForTest();
    m_bridge->clearAuthoringRecords();

    Doc *doc = m_app->doc();
    VirtualConsole *vc = m_app->virtualConsole();
    MasterTimer *timer = doc->masterTimer();
    QVERIFY(timer != nullptr);
    timer->stopAllFunctions();
    doc->setMode(Doc::Design);
    vc->setEditMode(true);
    m_app->setAccessMask(m_initialAccessMask);
    InputOutputMap *ioMap = doc->inputOutputMap();
    VCPage *page = vc->page(0);
    QVERIFY(page != nullptr);

    QVERIFY2(ioMap->setOutputPatch(0, m_probeName, "", "", 0, false, 0),
             "Failed to patch probe output");
    verifyOnlyProbePatchesActive();
    QVERIFY2(counter("openCount") > 0, "Probe open counter did not move after patching");

    VCFrame *frame = new VCFrame(doc, vc, page);
    vc->addWidgetToMap(frame);
    page->addWidgetToPageMap(frame);
    frame->setShowHeader(false);
    frame->setLogicalGeometry(QRectF(0, 0, 800, 600));

    VCButton *button = new VCButton(doc, frame);
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);
    button->setLogicalGeometry(QRectF(20, 60, 100, 80));

    const quint32 frameId = frame->id();
    const quint32 buttonId = button->id();
    auto cleanupWidgets = qScopeGuard([&]() {
        vc->deleteVCWidgets({buttonId, frameId});
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), false, 0);
        ioMap->setOutputPatch(0, KOutputNone, "", "", QLCIOPlugin::invalidLine(), true);
    });

    const QRectF originalRect = button->logicalGeometry();
    bool dirtyBefore = doc->isModified();
    QByteArray xmlBefore = serializeConsole();
    resetProbeCounters();

    VCBridge::AuthoringLayoutRequest req;
    VCBridge::AuthoringGeometryChange change;
    change.widgetID = buttonId;
    change.logicalRect = QRectF(110, 140, 100, 80);
    req.geometryChanges.append(change);

    // Precondition negative rows: access, mode, editMode and timer edit admission.
    const int oldMask = m_app->accessMask();
    m_app->setAccessMask(oldMask & ~App::AC_VCEditing);
    const VCBridge::AuthoringPreparationResult deniedAccess =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(deniedAccess.code, QStringLiteral("authoringPrecondition"));
    QVERIFY(deniedAccess.preparationToken.isEmpty());
    QCOMPARE(button->logicalGeometry(), originalRect);
    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(serializeConsole(), xmlBefore);
    m_app->setAccessMask(oldMask);

    doc->setMode(Doc::Operate);
    const VCBridge::AuthoringPreparationResult deniedMode =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(deniedMode.code, QStringLiteral("authoringPrecondition"));
    QCOMPARE(button->logicalGeometry(), originalRect);
    doc->setMode(Doc::Design);

    vc->setEditMode(false);
    const VCBridge::AuthoringPreparationResult deniedEditMode =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(deniedEditMode.code, QStringLiteral("authoringPrecondition"));
    QCOMPARE(button->logicalGeometry(), originalRect);
    vc->setEditMode(true);

    dirtyBefore = doc->isModified();
    xmlBefore = serializeConsole();

    const VCBridge::AuthoringPreparationResult prep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(prep.code, QString());
    QVERIFY2(!prep.preparationToken.isEmpty(), qPrintable(prep.error));
    QCOMPARE(prep.atomicScope, VCBridge::AuthoringPreparationResult::AtomicScope::NativeGroup);
    QVERIFY(!prep.candidateDigest.isEmpty());

    QCOMPARE(button->logicalGeometry(), originalRect);
    QCOMPARE(doc->isModified(), dirtyBefore);
    QCOMPARE(serializeConsole(), xmlBefore);

    QVERIFY(timer->beginRegistryEdit());
    const VCBridge::AuthoringApplyResult deniedTimer =
        m_bridge->applyAuthoring(prep.preparationToken);
    QCOMPARE(deniedTimer.code, QStringLiteral("authoringPrecondition"));
    QCOMPARE(deniedTimer.applied, false);
    QCOMPARE(deniedTimer.tokenConsumed, true);
    timer->endFunctionEdit();
    QCOMPARE(button->logicalGeometry(), originalRect);

    const VCBridge::AuthoringPreparationResult prep2 =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(prep2.code, QString());
    QVERIFY(!prep2.preparationToken.isEmpty());

    const VCBridge::AuthoringApplyResult apply =
        m_bridge->applyAuthoring(prep2.preparationToken);
    QCOMPARE(apply.code, QString());
    QCOMPARE(apply.applied, true);
    QCOMPARE(apply.tokenConsumed, true);
    QCOMPARE(button->logicalGeometry(), QRectF(110, 140, 100, 80));
    QCOMPARE(apply.atomicScope, VCBridge::AuthoringApplyResult::AtomicScope::NativeGroup);

    const VCBridge::AuthoringApplyResult consumed =
        m_bridge->applyAuthoring(prep2.preparationToken);
    QCOMPARE(consumed.code, QStringLiteral("invalidPreparation"));
    QCOMPARE(consumed.applied, false);

    const VCBridge::AuthoringApplyResult unknown =
        m_bridge->applyAuthoring(QStringLiteral("missing-token"));
    QCOMPARE(unknown.code, QStringLiteral("invalidPreparation"));
    QCOMPARE(unknown.applied, false);

    // Same-ID replacement with old object still alive must stale-guard.
    VCBridge::AuthoringLayoutRequest staleReq;
    VCBridge::AuthoringGeometryChange staleChange;
    staleChange.widgetID = buttonId;
    staleChange.logicalRect = QRectF(180, 200, 100, 80);
    staleReq.geometryChanges.append(staleChange);
    const VCBridge::AuthoringPreparationResult stalePrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(staleReq));
    QCOMPARE(stalePrep.code, QString());
    QVERIFY(!stalePrep.preparationToken.isEmpty());

    frame->removeWidgetFromPageMap(button);
    vc->removeWidgetFromMap(button);
    VCButton *replacement = new VCButton(doc, frame);
    replacement->setID(buttonId);
    replacement->setLogicalGeometry(button->logicalGeometry());
    frame->addWidgetToPageMap(replacement);
    vc->addWidgetToMap(replacement);
    QVERIFY(replacement != button);
    QVERIFY(vc->widget(buttonId) == replacement);

    const VCBridge::AuthoringApplyResult staleApply =
        m_bridge->applyAuthoring(stalePrep.preparationToken);
    QCOMPARE(staleApply.code, QStringLiteral("staleSelection"));
    QCOMPARE(staleApply.tokenConsumed, true);
    QVERIFY(button != nullptr);
    QVERIFY(button->parent() != nullptr);

    frame->removeWidgetFromPageMap(replacement);
    vc->removeWidgetFromMap(replacement);
    replacement->deleteLater();
    vc->addWidgetToMap(button);
    frame->addWidgetToPageMap(button);

    // Expiry semantics: requested token returns preparationExpired, then invalidPreparation.
    m_bridge->setAuthoringNowForTest(1000);
    VCBridge::AuthoringPreparationResult expPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(req));
    QCOMPARE(expPrep.code, QString());
    QVERIFY(!expPrep.preparationToken.isEmpty());
    m_bridge->setAuthoringNowForTest(1000 + VCBridge::kAuthoringRecordTtlMs + 1);
    const VCBridge::AuthoringApplyResult expired =
        m_bridge->applyAuthoring(expPrep.preparationToken);
    QCOMPARE(expired.code, QStringLiteral("preparationExpired"));
    QCOMPARE(expired.tokenConsumed, true);
    const VCBridge::AuthoringApplyResult expiredAgain =
        m_bridge->applyAuthoring(expPrep.preparationToken);
    QCOMPARE(expiredAgain.code, QStringLiteral("invalidPreparation"));
    m_bridge->clearAuthoringNowForTest();

    QList<QString> capacityTokens;
    for (int i = 0; i < 32; ++i)
    {
        VCBridge::AuthoringLayoutRequest capReq;
        VCBridge::AuthoringGeometryChange capChange;
        capChange.widgetID = buttonId;
        capChange.logicalRect = QRectF(110 + i, 140, 100, 80);
        capReq.geometryChanges.append(capChange);
        const VCBridge::AuthoringPreparationResult capPrep =
            m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(capReq));
        QCOMPARE(capPrep.code, QString());
        QVERIFY(!capPrep.preparationToken.isEmpty());
        capacityTokens.append(capPrep.preparationToken);
    }
    VCBridge::AuthoringLayoutRequest overflowReq;
    VCBridge::AuthoringGeometryChange overflowChange;
    overflowChange.widgetID = buttonId;
    overflowChange.logicalRect = QRectF(999, 140, 100, 80);
    overflowReq.geometryChanges.append(overflowChange);
    const VCBridge::AuthoringPreparationResult overflowPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(overflowReq));
    QCOMPARE(overflowPrep.code, QStringLiteral("preparationCapacity"));

    for (const QString &token : capacityTokens)
    {
        const VCBridge::AuthoringApplyResult release = m_bridge->applyAuthoring(token);
        QCOMPARE(release.tokenConsumed, true);
        if (release.code.isEmpty())
            QCOMPARE(release.applied, true);
        else
            QCOMPARE(release.code, QStringLiteral("staleBasis"));
    }

    VCBridge::AuthoringLayoutRequest tooManyItemsReq;
    for (int i = 0; i < 129; ++i)
    {
        VCBridge::AuthoringGeometryChange itemChange;
        itemChange.widgetID = buttonId;
        itemChange.logicalRect = QRectF(200 + i, 200, 100, 80);
        tooManyItemsReq.geometryChanges.append(itemChange);
    }
    const VCBridge::AuthoringPreparationResult tooManyItemsPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(tooManyItemsReq));
    QCOMPARE(tooManyItemsPrep.code, QStringLiteral("preparationLimit"));

    VCBridge::AuthoringLayoutRequest oversizedReq;
    oversizedReq.expectedBasis = QString(70000, QChar('x'));
    oversizedReq.geometryChanges.append(change);
    const VCBridge::AuthoringPreparationResult oversizedPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(oversizedReq));
    QCOMPARE(oversizedPrep.code, QStringLiteral("preparationLimit"));

    // Group atomicity: stale second member must preserve first member.
    VCButton *b2 = new VCButton(doc, frame);
    vc->addWidgetToMap(b2);
    frame->addWidgetToPageMap(b2);
    b2->setLogicalGeometry(QRectF(240, 60, 100, 80));
    const quint32 b2id = b2->id();
    const QRectF b1Original = button->logicalGeometry();
    const QRectF b2Original = b2->logicalGeometry();

    VCBridge::AuthoringLayoutRequest grouped;
    VCBridge::AuthoringGeometryChange c1;
    c1.widgetID = buttonId;
    c1.logicalRect = QRectF(300, 140, 100, 80);
    grouped.geometryChanges.append(c1);
    VCBridge::AuthoringGeometryChange c2;
    c2.widgetID = b2id;
    c2.logicalRect = QRectF(430, 140, 100, 80);
    grouped.geometryChanges.append(c2);
    const VCBridge::AuthoringPreparationResult groupPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(grouped));
    QCOMPARE(groupPrep.code, QString());
    QVERIFY(!groupPrep.preparationToken.isEmpty());

    frame->removeWidgetFromPageMap(b2);
    vc->removeWidgetFromMap(b2);
    VCButton *b2Replacement = new VCButton(doc, frame);
    b2Replacement->setID(b2id);
    b2Replacement->setLogicalGeometry(b2Original);
    frame->addWidgetToPageMap(b2Replacement);
    vc->addWidgetToMap(b2Replacement);
    QVERIFY(b2Replacement != b2);

    const VCBridge::AuthoringApplyResult staleGroupApply =
        m_bridge->applyAuthoring(groupPrep.preparationToken);
    QCOMPARE(staleGroupApply.code, QStringLiteral("staleSelection"));
    QCOMPARE(button->logicalGeometry(), b1Original);
    QCOMPARE(b2Replacement->logicalGeometry(), b2Original);

    frame->removeWidgetFromPageMap(b2Replacement);
    vc->removeWidgetFromMap(b2Replacement);
    b2Replacement->deleteLater();
    vc->addWidgetToMap(b2);
    frame->addWidgetToPageMap(b2);

    // Bridge-routed measurement window: baseline fixture traffic vs bridge-callback window.
    resetProbeCounters();
    Universe *u0 = doc->inputOutputMap()->universe(0);
    QVERIFY(u0 != nullptr);
    u0->setChannelCapability(0, QLCChannel::Intensity);
    u0->zeroIntensityChannels();

    auto runObservationCycles = [&](uchar value, int cycles) {
        int startWrite = counter("writeCount");
        for (int i = 0; i < cycles; ++i)
        {
            if (!u0->write(0, value))
                return -1;
            const QByteArray frameData = u0->postGMValues()->mid(0, u0->usedChannels());
            u0->dumpOutput(frameData, true);
        }
        return counter("writeCount") - startWrite;
    };

    const int controlCycleDelta = runObservationCycles(10, 3);
    QVERIFY(controlCycleDelta >= 0);
    QVERIFY(controlCycleDelta > 0);

    resetProbeCounters();
    const int baselineOpen = counter("openCount");
    const int baselineClose = counter("closeCount");
    const int baselineWrite = counter("writeCount");
    const int baselineFeedback = counter("feedbackCount");

    VCBridge::AuthoringLayoutRequest measuredReq;
    VCBridge::AuthoringGeometryChange measuredChange;
    measuredChange.widgetID = buttonId;
    measuredChange.logicalRect = QRectF(360, 220, 100, 80);
    measuredReq.geometryChanges.append(measuredChange);
    const VCBridge::AuthoringPreparationResult measuredPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(measuredReq));
    QCOMPARE(measuredPrep.code, QString());
    QVERIFY(!measuredPrep.preparationToken.isEmpty());
    const VCBridge::AuthoringApplyResult measuredApply =
        m_bridge->applyAuthoring(measuredPrep.preparationToken);
    QCOMPARE(measuredApply.code, QString());
    QCOMPARE(measuredApply.applied, true);

    const int measuredCycleDelta = runObservationCycles(10, 3);
    QVERIFY(measuredCycleDelta >= 0);
    QCOMPARE(counter("openCount"), baselineOpen);
    QCOMPARE(counter("closeCount"), baselineClose);
    QCOMPARE(counter("feedbackCount"), baselineFeedback);
    QCOMPARE(measuredCycleDelta, controlCycleDelta);
    QCOMPARE(counter("writeCount") - baselineWrite, controlCycleDelta);

    ShowCommandRecorder *recorder = ShowCommandRecorder::instance();
    QVERIFY(recorder != nullptr);
    const int recorderPendingRunsBaseline = recorder->pendingControlRuns();
    const int recorderPendingReqBaseline = recorder->pendingUserRequests().count();
    const bool recorderRecordingBaseline = recorder->isRecording();
    const bool recorderAuthoringBaseline = recorder->isAuthoring();
    MasterTimer *stateTimer = doc->masterTimer();
    QVERIFY(stateTimer != nullptr);
    const QList<quint32> runningBefore = stateTimer->runningFunctionIds();

    // Refused prepare in measured bridge window (access disabled).
    resetProbeCounters();
    const int accessMaskBefore = m_app->accessMask();
    m_app->setAccessMask(accessMaskBefore & ~App::AC_VCEditing);
    const VCBridge::AuthoringPreparationResult refusedPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(measuredReq));
    m_app->setAccessMask(accessMaskBefore);
    QCOMPARE(refusedPrep.code, QStringLiteral("authoringPrecondition"));
    QVERIFY(refusedPrep.error.contains(QStringLiteral("access profile"), Qt::CaseInsensitive));
    QVERIFY(refusedPrep.preparationToken.isEmpty());
    QCOMPARE(counter("openCount"), 0);
    QCOMPARE(counter("closeCount"), 0);
    QCOMPARE(counter("writeCount"), 0);
    QCOMPARE(counter("feedbackCount"), 0);
    QCOMPARE(recorder->pendingControlRuns(), recorderPendingRunsBaseline);
    QCOMPARE(recorder->pendingUserRequests().count(), recorderPendingReqBaseline);
    QCOMPARE(recorder->isRecording(), recorderRecordingBaseline);
    QCOMPARE(recorder->isAuthoring(), recorderAuthoringBaseline);
    QCOMPARE(stateTimer->runningFunctionIds(), runningBefore);

    // Refused apply in measured bridge window (protected page PIN after prepare).
    const VCBridge::AuthoringPreparationResult pinPrep =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(measuredReq));
    QCOMPARE(pinPrep.code, QString());
    QVERIFY(!pinPrep.preparationToken.isEmpty());
    QVERIFY(vc->setPagePIN(0, QString(), QStringLiteral("1234")));
    resetProbeCounters();
    const QRectF beforePinRefusal = button->logicalGeometry();
    const bool dirtyBeforePinRefusal = doc->isModified();
    const QByteArray xmlBeforePinRefusal = serializeConsole();
    const VCBridge::AuthoringApplyResult pinRefused =
        m_bridge->applyAuthoring(pinPrep.preparationToken);
    QCOMPARE(pinRefused.code, QStringLiteral("authoringPrecondition"));
    QCOMPARE(pinRefused.applied, false);
    QCOMPARE(pinRefused.tokenConsumed, true);
    QVERIFY(pinRefused.error.contains(QStringLiteral("Protected page editing"), Qt::CaseInsensitive));
    QCOMPARE(button->logicalGeometry(), beforePinRefusal);
    QCOMPARE(doc->isModified(), dirtyBeforePinRefusal);
    QCOMPARE(counter("openCount"), 0);
    QCOMPARE(counter("closeCount"), 0);
    QCOMPARE(counter("writeCount"), 0);
    QCOMPARE(counter("feedbackCount"), 0);
    QCOMPARE(recorder->pendingControlRuns(), recorderPendingRunsBaseline);
    QCOMPARE(recorder->pendingUserRequests().count(), recorderPendingReqBaseline);
    QCOMPARE(recorder->isRecording(), recorderRecordingBaseline);
    QCOMPARE(recorder->isAuthoring(), recorderAuthoringBaseline);
    QCOMPARE(stateTimer->runningFunctionIds(), runningBefore);
    QVERIFY(vc->setPagePIN(0, QStringLiteral("1234"), QString()));

    // Positive sensitivity of the same observation path.
    int beforePositive = counter("writeCount");
    for (int i = 0; i < 3; ++i)
    {
        QVERIFY(u0->write(0, 45));
        const QByteArray frameData = u0->postGMValues()->mid(0, u0->usedChannels());
        u0->dumpOutput(frameData, true);
    }
    QVERIFY(counter("writeCount") > beforePositive);

    // Guard closure cap: frame + descendants must refuse above 4096 guards.
    for (int i = 0; i < 4100; ++i)
    {
        VCButton *bulk = new VCButton(doc, frame);
        vc->addWidgetToMap(bulk);
        frame->addWidgetToPageMap(bulk);
        bulk->setLogicalGeometry(QRectF(5 + (i % 100), 5 + (i / 100), 8, 8));
    }
    VCBridge::AuthoringLayoutRequest guardReq;
    VCBridge::AuthoringGeometryChange guardChange;
    guardChange.widgetID = frameId;
    guardChange.logicalRect = QRectF(0, 0, 801, 601);
    guardReq.geometryChanges.append(guardChange);
    const VCBridge::AuthoringPreparationResult guardLimit =
        m_bridge->prepareAuthoring(VCBridge::AuthoringRequest(guardReq));
    QCOMPARE(guardLimit.code, QStringLiteral("preparationLimit"));
}

QTEST_MAIN(VCAuthoringOwner_Test)
