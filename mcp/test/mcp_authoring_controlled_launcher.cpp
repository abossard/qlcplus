/*
  Q Light Controller Plus
  mcp_authoring_controlled_launcher.cpp
*/

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickView>
#include <QSettings>

#include <algorithm>

#include "app.h"
#include "doc.h"
#include "ioplugincache.h"
#include "mcpinit.h"
#include "qlcconfig.h"
#include "qlcfile.h"
#include "qlcioplugin.h"
#include "virtualconsole.h"

namespace
{

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

QStringList pluginFiles(QDir dir)
{
    dir.setFilter(QDir::Files);
    dir.setNameFilters(QStringList() << QStringLiteral("*%1").arg(KExtPlugin));
    QStringList files = dir.entryList(QDir::Files, QDir::Name);
    std::sort(files.begin(), files.end());
    return files;
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
    const QStringList files = pluginFiles(dir);
    for (const QString &fileName : files)
    {
        const QString absolute = dir.absoluteFilePath(fileName);
        if (!QFile::remove(absolute))
        {
            if (error)
                *error = QStringLiteral("Failed to remove %1").arg(absolute);
            return false;
        }
    }
    return true;
}

bool copyPlugins(const QDir &sourceDir, const QDir &targetDir, QString *error)
{
    const QStringList sourceFiles = pluginFiles(sourceDir);
    if (sourceFiles.isEmpty())
    {
        if (error)
        {
            *error = QStringLiteral("No probe plugins found in %1").arg(sourceDir.absolutePath());
        }
        return false;
    }

    for (const QString &fileName : sourceFiles)
    {
        const QString sourcePath = sourceDir.absoluteFilePath(fileName);
        const QString targetPath = targetDir.absoluteFilePath(fileName);
        QFile::remove(targetPath);
        if (!QFile::copy(sourcePath, targetPath))
        {
            if (error)
            {
                *error = QStringLiteral("Failed to copy plugin %1 -> %2")
                             .arg(sourcePath, targetPath);
            }
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

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("qlcplus"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qlcplus.org"));
    QCoreApplication::setApplicationName(QStringLiteral(APPNAME));
    QCoreApplication::setApplicationVersion(QStringLiteral(APPVERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("MCP controlled owner launcher"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption settingsRootOption(
        QStringList() << QStringLiteral("settings-root"),
        QStringLiteral("Controlled QSettings root path"),
        QStringLiteral("path"));
    QCommandLineOption pluginModeOption(
        QStringList() << QStringLiteral("plugin-mode"),
        QStringLiteral("Plugin resource mode: probe or empty"),
        QStringLiteral("mode"),
        QStringLiteral("probe"));
    QCommandLineOption proofFileOption(
        QStringList() << QStringLiteral("proof-file"),
        QStringLiteral("Write startup proof JSON to this file"),
        QStringLiteral("path"));
    QCommandLineOption startupWorkspaceOption(
        QStringList() << QStringLiteral("startup-workspace"),
        QStringLiteral("Workspace file to load before MCP server startup"),
        QStringLiteral("path"));

    parser.addOption(settingsRootOption);
    parser.addOption(pluginModeOption);
    parser.addOption(proofFileOption);
    parser.addOption(startupWorkspaceOption);
    mcpAddOptions(parser);
    parser.process(app);

    const QString settingsRoot = parser.value(settingsRootOption).trimmed();
    if (settingsRoot.isEmpty())
    {
        qCritical() << "--settings-root is required";
        return 2;
    }

    QJsonArray startupOrder;
    auto mark = [&](const QString &event) { startupOrder.append(event); };
    mark(QStringLiteral("set-production-application-identity"));

    const QJsonObject originalSettings = settingsFingerprint();
    mark(QStringLiteral("capture-original-settings-fingerprint"));

    QDir settingsDir(settingsRoot);
    if (!settingsDir.exists() && !settingsDir.mkpath(QStringLiteral(".")))
    {
        qCritical() << "Failed to create settings root:" << settingsRoot;
        return 3;
    }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDir.path());
    const QJsonObject controlledSettingsBeforeApp = settingsFingerprint();
    mark(QStringLiteral("set-controlled-settings-before-app-construction"));

    const QString pluginMode = parser.value(pluginModeOption).trimmed().toLower();
    if (pluginMode != QStringLiteral("probe") && pluginMode != QStringLiteral("empty"))
    {
        qCritical() << "Invalid --plugin-mode:" << pluginMode
                    << "(expected probe or empty)";
        return 4;
    }

    const QDir probeSourceDir(QStringLiteral(MCP_AUTHORING_PROBE_PLUGIN_DIR));
    QDir systemPluginDir(IOPluginCache::systemPluginDirectory());
    if (!systemPluginDir.exists() && !systemPluginDir.mkpath(QStringLiteral(".")))
    {
        qCritical() << "Failed to create system plugin directory:"
                    << systemPluginDir.absolutePath();
        return 5;
    }
    const QStringList preCurationCandidates = pluginFiles(systemPluginDir);
    QString error;
    if (!removePluginsFromDir(systemPluginDir, &error))
    {
        qCritical() << error;
        return 6;
    }
    if (pluginMode == QStringLiteral("probe"))
    {
        if (!copyPlugins(probeSourceDir, systemPluginDir, &error))
        {
            qCritical() << error;
            return 7;
        }
    }
    const QStringList postCurationCandidates = pluginFiles(systemPluginDir);
    mark(QStringLiteral("curate-plugin-resources-before-app-construction"));

    App qlcplusApp;
    qlcplusApp.set3dSupported(false);
    mark(QStringLiteral("construct-app-instance"));

    qlcplusApp.startup();
    qlcplusApp.resize(1200, 800);
    qlcplusApp.show();
    qlcplusApp.requestActivate();
    app.processEvents();
    mark(QStringLiteral("startup-app-and-show-native-window"));

    if (qlcplusApp.status() != QQuickView::Ready)
    {
        qCritical() << "App status is not Ready:" << qlcplusApp.errors();
        return 8;
    }

    const QString startupWorkspace = parser.value(startupWorkspaceOption).trimmed();
    if (!startupWorkspace.isEmpty())
    {
        qlcplusApp.loadWorkspace(startupWorkspace);
        mark(QStringLiteral("load-startup-workspace-before-mcp"));
    }

    const int accessMaskBefore = qlcplusApp.accessMask();
    const int accessMaskRequired = accessMaskBefore | App::AC_VCControl | App::AC_VCEditing;
    qlcplusApp.setAccessMask(accessMaskRequired);
    bool vcEditModeAfterSetup = false;
    if (qlcplusApp.virtualConsole())
    {
        qlcplusApp.virtualConsole()->setEditMode(true);
        vcEditModeAfterSetup = qlcplusApp.virtualConsole()->editMode();
    }
    mark(QStringLiteral("establish-vc-edit-access-preconditions"));

    mcpInit(qlcplusApp.doc(), qlcplusApp.virtualConsole(), qlcplusApp.functionManager(),
            qlcplusApp.flowConsole(), &qlcplusApp, parser);
    mark(QStringLiteral("start-production-mcp-http-server"));

    const QJsonObject effectiveSettings = settingsFingerprint();
    QStringList loadedPlugins;
    for (QLCIOPlugin *plugin : qlcplusApp.doc()->ioPluginCache()->plugins())
    {
        if (plugin)
            loadedPlugins.append(plugin->name());
    }
    std::sort(loadedPlugins.begin(), loadedPlugins.end());

    QJsonObject windowProof;
    windowProof.insert(QStringLiteral("visible"), qlcplusApp.isVisible());
    windowProof.insert(QStringLiteral("isExposed"), qlcplusApp.isExposed());
    windowProof.insert(QStringLiteral("visibility"), int(qlcplusApp.visibility()));
    windowProof.insert(QStringLiteral("winId"),
                       QString::number(static_cast<qulonglong>(qlcplusApp.winId())));
    windowProof.insert(QStringLiteral("statusReady"),
                       qlcplusApp.status() == QQuickView::Ready);

    QJsonObject pluginProof;
    pluginProof.insert(QStringLiteral("mode"), pluginMode);
    pluginProof.insert(QStringLiteral("probeSourceDir"), probeSourceDir.absolutePath());
    pluginProof.insert(QStringLiteral("systemPluginDir"), systemPluginDir.absolutePath());
    pluginProof.insert(QStringLiteral("preCurationCandidates"), toJsonArray(preCurationCandidates));
    pluginProof.insert(QStringLiteral("postCurationCandidates"), toJsonArray(postCurationCandidates));
    pluginProof.insert(QStringLiteral("loadedPluginNamesAfterStartup"), toJsonArray(loadedPlugins));

    QJsonObject settingsProof;
    settingsProof.insert(QStringLiteral("original"), originalSettings);
    settingsProof.insert(QStringLiteral("controlledBeforeApp"), controlledSettingsBeforeApp);
    settingsProof.insert(QStringLiteral("effectiveAfterStartup"), effectiveSettings);
    settingsProof.insert(
        QStringLiteral("effectiveFileInsideControlledRoot"),
        effectiveSettings.value(QStringLiteral("fileName")).toString().startsWith(settingsDir.path()));

    QJsonObject proof;
    proof.insert(QStringLiteral("timestampUtc"),
                 QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    proof.insert(QStringLiteral("organizationName"), QCoreApplication::organizationName());
    proof.insert(QStringLiteral("organizationDomain"), QCoreApplication::organizationDomain());
    proof.insert(QStringLiteral("applicationName"), QCoreApplication::applicationName());
    proof.insert(QStringLiteral("startupOrder"), startupOrder);
    proof.insert(QStringLiteral("settings"), settingsProof);
    proof.insert(QStringLiteral("pluginResources"), pluginProof);
    proof.insert(QStringLiteral("preconditions"), QJsonObject{
        {QStringLiteral("accessMaskBefore"), accessMaskBefore},
        {QStringLiteral("accessMaskAfter"), qlcplusApp.accessMask()},
        {QStringLiteral("requiredAccessMask"), accessMaskRequired},
        {QStringLiteral("vcEditModeAfterSetup"), vcEditModeAfterSetup}
    });
    proof.insert(QStringLiteral("window"), windowProof);
    proof.insert(QStringLiteral("mcpPort"), parser.value(QStringLiteral("mcp-port")).toInt());

    QString proofPath = parser.value(proofFileOption).trimmed();
    if (proofPath.isEmpty())
        proofPath = QDir::current().absoluteFilePath(QStringLiteral("mcp_authoring_controlled_launcher_proof.json"));
    if (!writeJson(proofPath, proof, &error))
    {
        qCritical() << "Failed to write proof file:" << proofPath << error;
        return 9;
    }
    qInfo().noquote() << QStringLiteral("Controlled launcher proof file: %1").arg(proofPath);

    return app.exec();
}
