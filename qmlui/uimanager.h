/*
  Q Light Controller Plus
  uimanager.h

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

#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <QQuickView>
#include <QObject>

typedef struct
{
    QVariant m_default;
    QVariant m_modified;
    QString m_category;
} UiProperty;

class Doc;

class UiManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList presetNames READ presetNames CONSTANT)
    Q_PROPERTY(QString currentPreset READ currentPreset WRITE setCurrentPreset NOTIFY currentPresetChanged)

public:
    UiManager(QQuickView *view, Doc *doc, QObject *parent = nullptr);
    ~UiManager();

    void initialize();
    void setDefaultParameter(QString category, QString name, QVariant value);

    Q_INVOKABLE QVariant getDefault(QString name) const;

    Q_INVOKABLE QVariant getModified(QString name) const;
    Q_INVOKABLE void setModified(QString name, QVariant value);

    Q_INVOKABLE QString userConfFilepath() const;
    Q_INVOKABLE bool saveSettings() const;

    /** Theme preset support */
    QStringList presetNames() const;
    QString currentPreset() const;
    void setCurrentPreset(const QString &name);
    Q_INVOKABLE void applyPreset(const QString &name);

signals:
    void currentPresetChanged();

private:
    void initPresets();

    /** Reference to the QML view root */
    QQuickView *m_view;

    /** Reference to the project workspace */
    Doc *m_doc;

    /** Reference to the UI QML settings */
    QObject *m_uiStyle;

    /** A map ok key,value representing every UI parameter
     *  that can be changed at runtime */
    QMap<QString, UiProperty> m_parameterMap;

    /** Built-in theme presets: name -> { colorName -> value } */
    QMap<QString, QMap<QString, QVariant>> m_presets;

    /** Currently active preset name */
    QString m_currentPreset;
};

#endif // UIMANAGER_H
