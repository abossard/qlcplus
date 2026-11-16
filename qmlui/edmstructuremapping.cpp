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
            m_functions.clear();
            emit changed();
        });
        // a removed Function's id goes to the next Function: its defaults go with it
        connect(m_doc, &Doc::functionRemoved, this, [this](quint32 id) {
            if (m_functions.removeIf([id](const auto &it) { return it.value() == id; }) > 0)
                emit changed();
        });
    }
}

QStringList EdmStructureMapping::labels()
{
    return {"intro", "buildup", "drop", "breakdown", "groove", "outro"};
}

bool EdmStructureMapping::isLook(const Function *function)
{
    if (function == nullptr)
        return false;
    switch (function->type())
    {
        case Function::SceneType:
        case Function::CollectionType:
        case Function::ChaserType:
        case Function::RGBMatrixType:
        case Function::EFXType:
            return true;
        default:
            return false;
    }
}

QVariantList EdmStructureMapping::lookFunctions() const
{
    QList<Function *> looks;
    if (m_doc)
    {
        for (Function *f : m_doc->functions())
            if (isLook(f))
                looks.append(f);
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

void EdmStructureMapping::saveXML(QXmlStreamWriter *doc) const
{
    QMap<QString, quint32> looks = m_functions;
    if (m_doc)
        looks.removeIf([this](const auto &it) { return isLook(m_doc->function(it.value())) == false; });
    if (looks.isEmpty())
        return;
    doc->writeStartElement(XML_ELEMENT);
    for (auto it = looks.constBegin(); it != looks.constEnd(); ++it)
    {
        doc->writeStartElement("Look");
        doc->writeAttribute("Label", it.key());
        doc->writeAttribute("Function", QString::number(it.value()));
        doc->writeEndElement();
    }
    doc->writeEndElement();
}

bool EdmStructureMapping::loadXML(QXmlStreamReader &doc, const WidgetFunction &widgetFunction)
{
    if (doc.name() != QLatin1String(XML_ELEMENT))
        return false;

    m_functions.clear();
    const auto accept = [this](quint32 id) {
        return id != Function::invalidId() && (m_doc == nullptr || isLook(m_doc->function(id)));
    };
    while (doc.readNextStartElement())
    {
        const QXmlStreamAttributes attrs = doc.attributes();
        const QString label = attrs.value("Label").toString();
        if (doc.name() == QLatin1String("Look") && label.isEmpty() == false)
        {
            bool widgetOk = false, functionOk = false;
            const quint32 widget = attrs.value("Widget").toUInt(&widgetOk);
            const quint32 function = attrs.value("Function").toUInt(&functionOk);
            const quint32 look = functionOk ? function
                                 : widgetOk && widgetFunction ? widgetFunction(widget) : Function::invalidId();
            if (accept(look))
                m_functions.insert(label, look);
        }
        doc.skipCurrentElement();
    }
    emit changed();
    return true;
}
