/*
  Q Light Controller Plus
  mcp_authoring_settings_probe.cpp
*/

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>

#include <algorithm>

#include "qlcconfig.h"

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

QJsonObject fingerprintFromSettings(const QString &label, const QSettings &settings)
{
    QStringList keys = settings.allKeys();
    std::sort(keys.begin(), keys.end());
    const QByteArray digest = QCryptographicHash::hash(
        keys.join(QLatin1Char('\n')).toUtf8(),
        QCryptographicHash::Sha256).toHex();

    QJsonObject object;
    object.insert(QStringLiteral("label"), label);
    object.insert(QStringLiteral("backend"), settingsFormatName(settings.format()));
    object.insert(QStringLiteral("fileName"), settings.fileName());
    object.insert(QStringLiteral("keyCount"), int(keys.size()));
    object.insert(QStringLiteral("keyDigestSha256"), QString::fromLatin1(digest));
    return object;
}

QJsonObject captureDefaultFingerprint(const QString &label)
{
    QSettings settings;
    return fingerprintFromSettings(label, settings);
}

QJsonObject captureConstructorAudit()
{
    const QString org = QCoreApplication::organizationName();
    const QString appName = QCoreApplication::applicationName();

    QSettings productionDefault;
    QSettings explicitOrgApp(org, appName);
    QSettings scopeOrgApp(QSettings::UserScope, org, appName);
    QSettings explicitIni(QSettings::IniFormat, QSettings::UserScope, org, appName);
    QSettings explicitNative(QSettings::NativeFormat, QSettings::UserScope, org, appName);

    QJsonObject audit;
    audit.insert(QStringLiteral("productionDefaultCtor"),
                 fingerprintFromSettings(QStringLiteral("productionDefaultCtor"), productionDefault));
    audit.insert(QStringLiteral("explicitOrgAppCtor"),
                 fingerprintFromSettings(QStringLiteral("explicitOrgAppCtor"), explicitOrgApp));
    audit.insert(QStringLiteral("scopeOrgAppCtor"),
                 fingerprintFromSettings(QStringLiteral("scopeOrgAppCtor"), scopeOrgApp));
    audit.insert(QStringLiteral("explicitIniCtor"),
                 fingerprintFromSettings(QStringLiteral("explicitIniCtor"), explicitIni));
    audit.insert(QStringLiteral("explicitNativeCtor"),
                 fingerprintFromSettings(QStringLiteral("explicitNativeCtor"), explicitNative));
    return audit;
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
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qlcplus"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qlcplus.org"));
    QCoreApplication::setApplicationName(QStringLiteral(APPNAME));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("MCP authoring settings probe"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption outputOption(
        QStringList() << QStringLiteral("o") << QStringLiteral("output"),
        QStringLiteral("Write JSON output to file"),
        QStringLiteral("path"));
    parser.addOption(outputOption);
    parser.process(app);

    QJsonObject original = captureDefaultFingerprint(QStringLiteral("original"));
    QJsonObject originalAudit = captureConstructorAudit();

    QTemporaryDir controlledSettingsRoot;
    if (!controlledSettingsRoot.isValid())
    {
        qCritical() << "Failed to create controlled settings root";
        return 2;
    }

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, controlledSettingsRoot.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, controlledSettingsRoot.path());
    QJsonObject controlled = captureDefaultFingerprint(QStringLiteral("controlled"));
    QJsonObject controlledAudit = captureConstructorAudit();

    QJsonObject result;
    result.insert(QStringLiteral("timestampUtc"),
                  QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    result.insert(QStringLiteral("organizationName"), QCoreApplication::organizationName());
    result.insert(QStringLiteral("organizationDomain"), QCoreApplication::organizationDomain());
    result.insert(QStringLiteral("applicationName"), QCoreApplication::applicationName());
    result.insert(QStringLiteral("controlledSettingsRoot"), controlledSettingsRoot.path());
    result.insert(QStringLiteral("original"), original);
    result.insert(QStringLiteral("originalConstructorAudit"), originalAudit);
    result.insert(QStringLiteral("controlled"), controlled);
    result.insert(QStringLiteral("controlledConstructorAudit"), controlledAudit);
    result.insert(
        QStringLiteral("controlledFileInsideRoot"),
        controlled.value(QStringLiteral("fileName")).toString().startsWith(controlledSettingsRoot.path()));

    const QString outputPath = parser.value(outputOption).trimmed();
    if (!outputPath.isEmpty())
    {
        QString error;
        if (!writeJson(outputPath, result, &error))
        {
            qCritical() << "Failed to write probe output:" << outputPath << error;
            return 3;
        }
    }

    QFile stdoutFile;
    if (!stdoutFile.open(stdout, QIODevice::WriteOnly))
    {
        qCritical() << "Failed to open stdout for writing";
        return 4;
    }
    stdoutFile.write(QJsonDocument(result).toJson(QJsonDocument::Indented));
    stdoutFile.close();

    return 0;
}
