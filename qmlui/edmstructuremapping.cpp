/*
  Q Light Controller Plus
  edmstructuremapping.cpp

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

#include "edmstructuremapping.h"
#include "doc.h"
#include "function.h"

#include <QHash>
#include <algorithm>
#include <QVariantMap>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#define XML_ELEMENT "SongStructure"

EdmStructureMapping::EdmStructureMapping(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    if (m_doc)
    {
        connect(m_doc, &Doc::cleared, this, [this]() {
            m_auto = false;
            m_widgets.clear();
            m_functions.clear();
            emit changed();
        });
    }
}

QStringList EdmStructureMapping::labels()
{
    return {"intro", "buildup", "drop", "breakdown", "groove", "outro"};
}

QVariantList EdmStructureMapping::lookFunctions() const
{
    QList<Function *> looks;
    if (m_doc)
    {
        for (Function::Type type : {Function::SceneType, Function::CollectionType, Function::ChaserType,
                                    Function::RGBMatrixType, Function::EFXType})
            looks += m_doc->functionsByType(type);
    }
    std::sort(looks.begin(), looks.end(), [](Function *a, Function *b) {
        return QString::localeAwareCompare(a->name(), b->name()) < 0;
    });
    QHash<QString, int> seen;
    for (Function *f : looks)
        seen[f->name()]++;

    QVariantList list;
    for (Function *f : looks)
    {
        const QString text = seen.value(f->name()) > 1 && f->path(true).isEmpty() == false
                                 ? QString("%1  (%2)").arg(f->name(), f->path(true))
                                 : f->name();
        list.append(QVariantMap {{"id", f->id()}, {"text", text}});
    }
    return list;
}

void EdmStructureMapping::setAutoEnabled(bool enabled)
{
    if (enabled == m_auto)
        return;
    m_auto = enabled;
    if (m_doc)
        m_doc->setModified();
    emit changed();
}

quint32 EdmStructureMapping::function(const QString &label) const
{
    return m_functions.value(label, Function::invalidId());
}

void EdmStructureMapping::setFunction(const QString &label, quint32 functionId)
{
    if (function(label) == functionId)
        return;
    if (functionId == Function::invalidId())
        m_functions.remove(label);
    else
        m_functions.insert(label, functionId);
    if (m_doc)
        m_doc->setModified();
    emit changed();
}

int EdmStructureMapping::functionId(const QString &label) const
{
    return function(label) == Function::invalidId() ? -1 : int(function(label));
}

void EdmStructureMapping::setFunctionId(const QString &label, int id)
{
    setFunction(label, id < 0 ? Function::invalidId() : quint32(id));
}

void EdmStructureMapping::setWidget(const QString &label, quint32 widgetId)
{
    if (widget(label) == widgetId)
        return;
    if (widgetId == kNoLookWidget)
        m_widgets.remove(label);
    else
        m_widgets.insert(label, widgetId);
    if (m_doc)
        m_doc->setModified();
    emit changed();
}

int EdmStructureMapping::widgetId(const QString &label) const
{
    return widget(label) == kNoLookWidget ? -1 : int(widget(label));
}

void EdmStructureMapping::setWidgetId(const QString &label, int id)
{
    setWidget(label, id < 0 ? kNoLookWidget : quint32(id));
}

void EdmStructureMapping::saveXML(QXmlStreamWriter *doc) const
{
    if (m_auto == false && m_widgets.isEmpty() && m_functions.isEmpty())
        return;
    doc->writeStartElement(XML_ELEMENT);
    doc->writeAttribute("Auto", m_auto ? "1" : "0");
    QStringList labels = m_widgets.keys() + m_functions.keys();
    labels.removeDuplicates();
    labels.sort();
    for (const QString &label : labels)
    {
        doc->writeStartElement("Look");
        doc->writeAttribute("Label", label);
        if (m_functions.contains(label))
            doc->writeAttribute("Function", QString::number(m_functions.value(label)));
        if (m_widgets.contains(label))
            doc->writeAttribute("Widget", QString::number(m_widgets.value(label)));
        doc->writeEndElement();
    }
    doc->writeEndElement();
}

bool EdmStructureMapping::loadXML(QXmlStreamReader &doc, const WidgetFunction &widgetFunction)
{
    if (doc.name() != QLatin1String(XML_ELEMENT))
        return false;

    m_auto = doc.attributes().value("Auto") == QLatin1String("1");
    m_widgets.clear();
    m_functions.clear();
    while (doc.readNextStartElement())
    {
        const QXmlStreamAttributes attrs = doc.attributes();
        const QString label = attrs.value("Label").toString();
        if (doc.name() == QLatin1String("Look") && label.isEmpty() == false)
        {
            bool widgetOk = false, functionOk = false;
            const quint32 widget = attrs.value("Widget").toUInt(&widgetOk);
            const quint32 function = attrs.value("Function").toUInt(&functionOk);
            if (widgetOk)
                m_widgets.insert(label, widget);
            if (functionOk)
                m_functions.insert(label, function);
            else if (widgetOk && widgetFunction)
            {
                const quint32 fromButton = widgetFunction(widget);
                if (fromButton != Function::invalidId())
                    m_functions.insert(label, fromButton);
            }
        }
        doc.skipCurrentElement();
    }
    emit changed();
    return true;
}
