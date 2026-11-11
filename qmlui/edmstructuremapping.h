/*
  Q Light Controller Plus
  edmstructuremapping.h

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

#ifndef EDMSTRUCTUREMAPPING_H
#define EDMSTRUCTUREMAPPING_H

#include <QMap>
#include <QObject>
#include <QStringList>

class Doc;
class QXmlStreamReader;
class QXmlStreamWriter;

/** No Virtual Console widget (VCWidget::invalidId()) */
static const quint32 kNoLookWidget = 0xFFFFFFFF;

/**
 * Which Virtual Console button each section label presses, and whether the
 * director acts at all. Part of the workspace: <SongStructure Auto="1"><Look Label="drop" Widget="12"/>…
 * Labels the tool may add later and widgets that are gone are kept.
 */
class EdmStructureMapping : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool autoEnabled READ autoEnabled WRITE setAutoEnabled NOTIFY changed)

public:
    explicit EdmStructureMapping(Doc *doc = nullptr, QObject *parent = nullptr);

    /** The labels the editor offers */
    Q_INVOKABLE static QStringList labels();

    bool autoEnabled() const { return m_auto; }
    void setAutoEnabled(bool enabled);

    quint32 widget(const QString &label) const { return m_widgets.value(label, kNoLookWidget); }
    void setWidget(const QString &label, quint32 widgetId);

    /** Same for QML: -1 for none */
    Q_INVOKABLE int widgetId(const QString &label) const;
    Q_INVOKABLE void setWidgetId(const QString &label, int widgetId);

    /** Every mapped widget */
    QList<quint32> widgets() const { return m_widgets.values(); }

    void saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &doc);

signals:
    void changed();

private:
    Doc *m_doc;
    bool m_auto = false;
    QMap<QString, quint32> m_widgets;
};

#endif
