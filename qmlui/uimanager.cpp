/*
  Q Light Controller Plus
  uimanager.cpp

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

#include <QQmlComponent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>

#include "qlcfile.h"
#include "qlcconfig.h"
#include "uimanager.h"

#define UISTYLEFILE "qlcplusUiStyle.json"

UiManager::UiManager(QQuickView *view, Doc *doc, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_doc(doc)
    , m_currentPreset("Classic")
{
    initPresets();
}

UiManager::~UiManager()
{
}

void UiManager::initialize()
{
    /** Force the creation of the UISettings singleton and
     *  store a reference to it. In this way it is possible
     *  to change the UI settings at runtime */
    QQmlComponent component(m_view->engine());
    const char *source =
            "import QtQuick\n"
            "import \".\"\n"
            "QtObject {\n"
            "    property var style: UISettings\n"
            "}";
    component.setData(source, QUrl("qrc:/"));
    QObject *item = component.create();
    m_uiStyle = qvariant_cast<QObject*>(item->property("style"));

    /** Store default values first */
    setDefaultParameter("sizes", "scalingFactor", 1.0);

    setDefaultParameter("colors", "bgStronger", m_uiStyle->property("bgStronger"));
    setDefaultParameter("colors", "bgStrong", m_uiStyle->property("bgStrong"));
    setDefaultParameter("colors", "bgMedium", m_uiStyle->property("bgMedium"));
    setDefaultParameter("colors", "bgControl", m_uiStyle->property("bgControl"));
    setDefaultParameter("colors", "bgLight", m_uiStyle->property("bgLight"));
    setDefaultParameter("colors", "bgLighter", m_uiStyle->property("bgLighter"));
    setDefaultParameter("colors", "fgMain", m_uiStyle->property("fgMain"));
    setDefaultParameter("colors", "fgMedium", m_uiStyle->property("fgMedium"));
    setDefaultParameter("colors", "fgLight", m_uiStyle->property("fgLight"));

    setDefaultParameter("colors", "sectionHeader", m_uiStyle->property("sectionHeader"));
    setDefaultParameter("colors", "sectionHeaderDiv", m_uiStyle->property("sectionHeaderDiv"));
    setDefaultParameter("colors", "highlight", m_uiStyle->property("highlight"));
    setDefaultParameter("colors", "highlightPressed", m_uiStyle->property("highlightPressed"));
    setDefaultParameter("colors", "hover", m_uiStyle->property("hover"));
    setDefaultParameter("colors", "selection", m_uiStyle->property("selection"));
    setDefaultParameter("colors", "activeDropArea", m_uiStyle->property("activeDropArea"));
    setDefaultParameter("colors", "borderColorDark", m_uiStyle->property("borderColorDark"));

    setDefaultParameter("colors", "toolbarStartMain", m_uiStyle->property("toolbarStartMain"));
    setDefaultParameter("colors", "toolbarStartSub", m_uiStyle->property("toolbarStartSub"));
    setDefaultParameter("colors", "toolbarEnd", m_uiStyle->property("toolbarEnd"));
    setDefaultParameter("colors", "toolbarHoverStart", m_uiStyle->property("toolbarHoverStart"));
    setDefaultParameter("colors", "toolbarHoverEnd", m_uiStyle->property("toolbarHoverEnd"));
    setDefaultParameter("colors", "toolbarPressedStart", m_uiStyle->property("toolbarPressedStart"));
    setDefaultParameter("colors", "toolbarPressedEnd", m_uiStyle->property("toolbarPressedEnd"));

    setDefaultParameter("colors", "toolbarSelectionMain", m_uiStyle->property("toolbarSelectionMain"));
    setDefaultParameter("colors", "toolbarSelectionSub", m_uiStyle->property("toolbarSelectionSub"));

    setDefaultParameter("colors", "frameHeaderStart", m_uiStyle->property("frameHeaderStart"));
    setDefaultParameter("colors", "frameHeaderEnd", m_uiStyle->property("frameHeaderEnd"));
    setDefaultParameter("colors", "soloFrameHeaderStart", m_uiStyle->property("soloFrameHeaderStart"));
    setDefaultParameter("colors", "soloFrameHeaderEnd", m_uiStyle->property("soloFrameHeaderEnd"));

    /** Then load (if available) the user configuration */
    QFile jsonFile(userConfFilepath());
    if (jsonFile.exists())
    {
        QJsonParseError parseError;
        if (jsonFile.open(QIODevice::ReadOnly) != true)
            return;

        QByteArray ba = jsonFile.readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(ba, &parseError);

        if (parseError.error != QJsonParseError::NoError)
        {
            qWarning() << "UI Style parse error at" << parseError.offset << ":" << parseError.errorString();
        }
        else
        {
            QJsonObject jsonObject = jsonDoc.object();

            /** Apply saved preset first (if any) */
            if (jsonObject.contains("preset"))
            {
                QString presetName = jsonObject.value("preset").toString();
                if (presetName != "Classic" && m_presets.contains(presetName))
                    applyPreset(presetName);
                else
                    setCurrentPreset(presetName);
            }

            for (QString &category : jsonObject.keys())
            {
                if (category == "preset")
                    continue;

                QJsonObject categoryObj = jsonObject.value(category).toObject();
                for (QString &paramName : categoryObj.keys())
                {
                    QJsonValue paramVal = categoryObj.value(paramName);
                    setModified(paramName, paramVal.toVariant());
                }
            }
        }
        jsonFile.close();
    }
    else
    {
        /** No saved config — apply VS Code Dark as the default theme */
        applyPreset("VS Code Dark");
    }
}

void UiManager::setDefaultParameter(QString category, QString name, QVariant value)
{
    UiProperty prop;
    prop.m_category = category;
    prop.m_default = value;
    prop.m_modified = value;
    m_parameterMap.insert(name, prop);
}

QVariant UiManager::getDefault(QString name) const
{
    UiProperty prop = m_parameterMap.value(name);
    return prop.m_default;
}

QVariant UiManager::getModified(QString name) const
{
    UiProperty prop = m_parameterMap.value(name);
    return prop.m_modified;
}

void UiManager::setModified(QString name, QVariant value)
{
    UiProperty prop = m_parameterMap.value(name);
    prop.m_modified = value;
    m_parameterMap.insert(name, prop);
    std::string str = name.toStdString();
    m_uiStyle->setProperty(str.c_str(), value);
}

QString UiManager::userConfFilepath() const
{
    QDir userConfDir = QLCFile::userDirectory(QString(USERQLCPLUSDIR), QString(USERQLCPLUSDIR), QStringList());
    return userConfDir.absolutePath() + QDir::separator() + UISTYLEFILE;
}

bool UiManager::saveSettings() const
{
    bool ret = true;
    QFile jsonFile(userConfFilepath());
    QMap<QString, QJsonObject*> objMap;
    QJsonObject objRoot;

    /** Add parameters to JSON objects representing categories */
    QMapIterator<QString, UiProperty> it(m_parameterMap);
    while (it.hasNext())
    {
        it.next();
        QString paramName = it.key();
        UiProperty prop = it.value();

        if (objMap.contains(prop.m_category) == false)
            objMap.insert(prop.m_category, new QJsonObject());

        QJsonObject *categoryObj = objMap.value(prop.m_category);
        categoryObj->insert(paramName, QJsonValue::fromVariant(prop.m_modified));
    }

    /** Add each JSON object to the root object */
    QMapIterator<QString, QJsonObject*> cIt(objMap);
    while (cIt.hasNext())
    {
        cIt.next();
        objRoot[cIt.key()] = *cIt.value();
    }

    /** Store the active preset name */
    objRoot["preset"] = m_currentPreset;

    /** Finally, store on file */
    QByteArray ba = QJsonDocument(objRoot).toJson();
    //QTextStream ts(stdout);
    //ts << "rendered JSON" << endl;
    //ts << ba;

    if (jsonFile.open(QIODevice::WriteOnly) == true)
    {
        if (jsonFile.write(ba) <= 0)
            ret = false;

        jsonFile.close();
    }
    else
    {
        ret = false;
    }

    return ret;
}

void UiManager::initPresets()
{
    /* VS Code Dark preset — flat, no gradients, muted foreground */
    QMap<QString, QVariant> vscodeDark;
    vscodeDark["bgStronger"]          = QColor("#111111");
    vscodeDark["bgStrong"]            = QColor("#1E1E1E");
    vscodeDark["bgMedium"]            = QColor("#252526");
    vscodeDark["bgControl"]           = QColor("#3C3C3C");
    vscodeDark["bgLight"]             = QColor("#4D4D4D");
    vscodeDark["bgLighter"]           = QColor("#585858");
    vscodeDark["fgMain"]              = QColor("#CCCCCC");
    vscodeDark["fgMedium"]            = QColor("#858585");
    vscodeDark["fgLight"]             = QColor("#C5C5C5");
    vscodeDark["sectionHeader"]       = QColor("#04395E");
    vscodeDark["sectionHeaderDiv"]    = QColor("#37373D");
    vscodeDark["highlight"]           = QColor("#007ACC");
    vscodeDark["highlightPressed"]    = QColor("#0E639C");
    vscodeDark["hover"]               = QColor("#2A2D2E");
    vscodeDark["selection"]           = QColor("#264F78");
    vscodeDark["activeDropArea"]      = QColor("#062F4A");
    vscodeDark["borderColorDark"]     = QColor("#1E1E1E");
    vscodeDark["toolbarStartMain"]    = QColor("#252526");
    vscodeDark["toolbarStartSub"]     = QColor("#252526");
    vscodeDark["toolbarEnd"]          = QColor("#252526");
    vscodeDark["toolbarHoverStart"]   = QColor("#2A2D2E");
    vscodeDark["toolbarHoverEnd"]     = QColor("#2A2D2E");
    vscodeDark["toolbarPressedStart"] = QColor("#1B1B1F");
    vscodeDark["toolbarPressedEnd"]   = QColor("#1B1B1F");
    vscodeDark["toolbarSelectionMain"] = QColor("#007ACC");
    vscodeDark["toolbarSelectionSub"] = QColor("#007ACC");
    vscodeDark["frameHeaderStart"]     = QColor("#333333");
    vscodeDark["frameHeaderEnd"]       = QColor("#333333");
    vscodeDark["soloFrameHeaderStart"] = QColor("#6C1A1A");
    vscodeDark["soloFrameHeaderEnd"]   = QColor("#6C1A1A");
    m_presets.insert("VS Code Dark", vscodeDark);
}

QStringList UiManager::presetNames() const
{
    QStringList names;
    names << "Classic";
    names << m_presets.keys();
    return names;
}

QString UiManager::currentPreset() const
{
    return m_currentPreset;
}

void UiManager::setCurrentPreset(const QString &name)
{
    if (m_currentPreset == name)
        return;
    m_currentPreset = name;
    emit currentPresetChanged();
}

void UiManager::applyPreset(const QString &name)
{
    if (name == "Classic")
    {
        /* Restore all colors to their original defaults */
        QMapIterator<QString, UiProperty> it(m_parameterMap);
        while (it.hasNext())
        {
            it.next();
            if (it.value().m_category == "colors")
                setModified(it.key(), it.value().m_default);
        }
    }
    else if (m_presets.contains(name))
    {
        const QMap<QString, QVariant> &preset = m_presets.value(name);
        QMapIterator<QString, QVariant> it(preset);
        while (it.hasNext())
        {
            it.next();
            setModified(it.key(), it.value());
        }
    }
    setCurrentPreset(name);
    saveSettings();
}
