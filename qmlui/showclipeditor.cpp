/*
  Q Light Controller Plus
  showclipeditor.cpp

  Copyright (c) QLC+ contributors

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

#include <QQmlContext>
#include <QQuickView>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <climits>

#include "showclipautomation.h"
#include "showclipeditor.h"
#include "showfunction.h"
#include "showmanager.h"
#include "tardis.h"
#include "track.h"
#include "show.h"
#include "doc.h"

#define KXMLShowClip         QStringLiteral("Clip")
#define KXMLShowClipShow     QStringLiteral("Show")
#define KXMLShowClipStart    QStringLiteral("Start")
#define KXMLShowClipDuration QStringLiteral("Duration")

ShowClipEditor::ShowClipEditor(QQuickView *view, Doc *doc, ShowManager *manager)
    : QObject(manager)
    , m_doc(doc)
    , m_manager(manager)
{
    view->rootContext()->setContextProperty("showClipEditor", this);

    connect(manager, &ShowManager::currentShowIDChanged, this, [this]()
    {
        if (m_navigating || m_path.isEmpty())
            return;
        m_path.clear();
        emit pathChanged();
    });
    connect(doc, &Doc::functionRemoved, this, [this](quint32 id)
    {
        if (m_path.removeAll(id) > 0)
            emit pathChanged();
    });
    connect(doc, &Doc::functionNameChanged, this, [this](quint32 id)
    {
        if (m_path.isEmpty() == false && m_path.last() == id)
            emit pathChanged();
    });
}

bool ShowClipEditor::trimsInPoint(ShowFunction *sf) const
{
    if (sf == nullptr)
        return false;

    Function *function = m_doc->function(sf->functionID());
    return (function != nullptr && function->type() == Function::ShowType) || sf->clipAutomation().isEmpty() == false;
}

int ShowClipEditor::startExtensionLimit(ShowFunction *sf) const
{
    if (trimsInPoint(sf) == false)
        return -1;

    const quint32 limit = played(sf).startExtensionLimit();
    return limit > quint32(INT_MAX) ? -1 : int(limit);
}

bool ShowClipEditor::trimStart(ShowFunction *sf, int startTime)
{
    Show *show = m_manager->currentShow();
    if (sf == nullptr || show == nullptr || m_manager->readOnly())
        return false;

    const bool rebases = trimsInPoint(sf);
    if (rebases && show->isRunning())
        return false;

    const qint64 oldStart = sf->startTime();
    const qint64 end = oldStart + sf->duration();
    const ShowClipAutomation before = played(sf);
    qint64 start = qMax(qint64(0), qint64(startTime));
    if (rebases)
        start = qMax(start, oldStart - qint64(before.startExtensionLimit()));
    if (start >= end)
        return false;
    if (start == oldStart)
        return true;
    if (m_manager->checkOverlapping(show->getTrackFromShowFunctionID(sf->id()), sf,
                                    quint32(start), quint32(end - start)))
        return false;

    const QString undo = clipXml(show->id(), sf);
    sf->setStartTime(quint32(start));
    sf->setDuration(quint32(end - start));
    if (rebases)
        sf->setClipAutomation(before.withStartMoved(start - oldStart));
    Tardis::instance()->enqueueAction(Tardis::ShowManagerItemSetClip, sf->id(), undo, clipXml(show->id(), sf));

    return true;
}

QVariantMap ShowClipEditor::overlay(ShowFunction *sf) const
{
    QVariantMap drawn;
    if (sf == nullptr)
        return drawn;

    const ShowClipAutomation automation = played(sf);
    const quint32 duration = sf->duration();

    QVariantList boundaries;
    const QVector<ShowClipSegment> &segments = automation.segments();
    for (int i = 1; i < segments.count() && segments.at(i).clipMs < duration; i++)
        boundaries.append(int(segments.at(i).clipMs));

    QVariantList envelope;
    for (const ShowClipLevel &keyframe : automation.envelope())
        envelope.append(QVariantMap{{"clipMs", int(keyframe.clipMs)}, {"level", keyframe.level}});

    QVariantList shaded;
    const auto shade = [&shaded, duration](quint32 from, quint32 to)
    {
        to = qMin(to, duration);
        if (from < to)
            shaded.append(QVariantMap{{"fromMs", int(from)}, {"toMs", int(to)}});
    };
    bool dominant = true;
    quint32 since = 0;
    for (const ShowClipDominance &step : automation.dominance())
    {
        if (step.dominant == dominant)
            continue;
        if (dominant == false)
            shade(since, step.clipMs);
        dominant = step.dominant;
        since = step.clipMs;
    }
    if (dominant == false)
        shade(since, duration);

    drawn.insert("boundaries", boundaries);
    drawn.insert("envelope", envelope);
    drawn.insert("shaded", shaded);
    drawn.insert("inPoint", segments.isEmpty() ? -1 : int(automation.inPoint()));
    return drawn;
}

bool ShowClipEditor::openNested(ShowFunction *sf)
{
    Show *current = m_manager->currentShow();
    Function *nested = sf != nullptr ? m_doc->function(sf->functionID()) : nullptr;
    if (current == nullptr || nested == nullptr || nested->type() != Function::ShowType)
        return false;

    /* Switching rebuilds the timeline, the item that asked included,
     * so it waits for the event loop */
    const quint32 parentId = current->id();
    const quint32 nestedId = nested->id();
    QMetaObject::invokeMethod(this, [this, parentId, nestedId]()
    {
        if (m_manager->currentShowID() != int(parentId))
            return;
        m_path.append(parentId);
        if (select(nestedId))
            emit pathChanged();
        else
            m_path.removeLast();
    }, Qt::QueuedConnection);
    return true;
}

void ShowClipEditor::goBack()
{
    if (m_path.isEmpty())
        return;

    const quint32 parentId = m_path.takeLast();
    if (select(parentId) == false)
        m_path.append(parentId);
    emit pathChanged();
}

bool ShowClipEditor::canGoBack() const
{
    return m_path.isEmpty() == false;
}

QString ShowClipEditor::parentShowName() const
{
    Function *parent = m_path.isEmpty() ? nullptr : m_doc->function(m_path.last());
    return parent != nullptr ? parent->name() : QString();
}

QString ShowClipEditor::clipXml(quint32 showId, const ShowFunction *sf)
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement(KXMLShowClip);
    writer.writeAttribute(KXMLShowClipShow, QString::number(showId));
    writer.writeAttribute(KXMLShowClipStart, QString::number(sf->startTime()));
    writer.writeAttribute(KXMLShowClipDuration, QString::number(sf->duration()));
    sf->clipAutomation().saveXML(&writer);
    writer.writeEndElement();
    return xml;
}

bool ShowClipEditor::applyClip(Doc *doc, quint32 clipId, const QString &xml, bool history)
{
    QXmlStreamReader reader(xml);
    if (reader.readNextStartElement() == false || reader.name() != KXMLShowClip)
        return false;

    const QXmlStreamAttributes attrs = reader.attributes();
    const quint32 startTime = attrs.value(KXMLShowClipStart).toUInt();
    const quint32 duration = attrs.value(KXMLShowClipDuration).toUInt();
    Show *show = qobject_cast<Show *>(doc->function(attrs.value(KXMLShowClipShow).toUInt()));
    ShowFunction *sf = show != nullptr && (history == false || show->isRunning() == false)
            ? show->showFunction(clipId) : nullptr;
    ShowClipAutomation automation;
    if (sf == nullptr || reader.readNextStartElement() == false || automation.loadXML(reader) == false)
        return false;

    sf->setStartTime(startTime);
    sf->setDuration(duration);
    sf->setClipAutomation(automation);
    return true;
}

ShowClipAutomation ShowClipEditor::played(ShowFunction *sf) const
{
    ShowClipAutomation automation = sf->clipAutomation();
    Function *function = m_doc->function(sf->functionID());
    if ((function == nullptr || function->type() != Function::ShowType) && automation.segments().isEmpty() == false)
    {
        ShowClipSegment inPoint;
        inPoint.songMs = automation.inPoint();
        automation.setSegments({inPoint});
    }
    return automation;
}

bool ShowClipEditor::select(quint32 showId)
{
    m_navigating = true;
    m_manager->setCurrentShowID(int(showId));
    m_navigating = false;
    return m_manager->currentShowID() == int(showId);
}
