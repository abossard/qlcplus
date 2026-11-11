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
            emit changed();
        });
    }
}

QStringList EdmStructureMapping::labels()
{
    return {"intro", "buildup", "drop", "breakdown", "groove", "outro"};
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
    if (m_auto == false && m_widgets.isEmpty())
        return;
    doc->writeStartElement(XML_ELEMENT);
    doc->writeAttribute("Auto", m_auto ? "1" : "0");
    for (auto it = m_widgets.constBegin(); it != m_widgets.constEnd(); ++it)
    {
        doc->writeStartElement("Look");
        doc->writeAttribute("Label", it.key());
        doc->writeAttribute("Widget", QString::number(it.value()));
        doc->writeEndElement();
    }
    doc->writeEndElement();
}

bool EdmStructureMapping::loadXML(QXmlStreamReader &doc)
{
    if (doc.name() != QLatin1String(XML_ELEMENT))
        return false;

    m_auto = doc.attributes().value("Auto") == QLatin1String("1");
    m_widgets.clear();
    while (doc.readNextStartElement())
    {
        const QXmlStreamAttributes attrs = doc.attributes();
        bool ok = false;
        const quint32 id = attrs.value("Widget").toUInt(&ok);
        if (ok && doc.name() == QLatin1String("Look") && attrs.value("Label").isEmpty() == false)
            m_widgets.insert(attrs.value("Label").toString(), id);
        doc.skipCurrentElement();
    }
    emit changed();
    return true;
}
