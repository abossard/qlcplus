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
#include <functional>
#include <QObject>
#include <QStringList>
#include <QVariantList>

class Doc;
class QXmlStreamReader;
class QXmlStreamWriter;

/**
 * The workspace's default look per section label: a Function that new Song
 * Director blocks are pre-filled with. Part of the workspace:
 * <SongStructure><Look Label="drop" Function="12"/>…
 * An old file has only Widget (a Virtual Console button): that button's
 * Function becomes the default.
 * Labels the tool may add later and Functions that are gone are kept.
 */
class EdmStructureMapping : public QObject
{
    Q_OBJECT

public:
    explicit EdmStructureMapping(Doc *doc = nullptr, QObject *parent = nullptr);

    /** The labels the editor offers */
    Q_INVOKABLE static QStringList labels();

    /** The Functions that can be a look, as {id, text} by name; the folder is added when a name repeats */
    Q_INVOKABLE QVariantList lookFunctions() const;

    /** The default Function of a label, Function::invalidId() for none */
    quint32 function(const QString &label) const;
    void setFunction(const QString &label, quint32 functionId);

    /** Same for QML: -1 for none */
    Q_INVOKABLE int functionId(const QString &label) const;
    Q_INVOKABLE void setFunctionId(const QString &label, int functionId);

    void saveXML(QXmlStreamWriter *doc) const;

    /** Maps a legacy Widget attribute to its button's Function, invalidId() when it has none */
    using WidgetFunction = std::function<quint32(quint32)>;
    bool loadXML(QXmlStreamReader &doc, const WidgetFunction &widgetFunction = WidgetFunction());

signals:
    void changed();

private:
    Doc *m_doc;
    QMap<QString, quint32> m_functions;
};

#endif
