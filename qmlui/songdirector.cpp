/*
  Q Light Controller Plus
  songdirector.cpp

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

#include "songdirector.h"
#include "collection.h"
#include "doc.h"
#include "show.h"
#include "showfunction.h"
#include "track.h"

#include <QHash>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#define XML_ELEMENT "SongDirector"

QList<SongBlock> songBlocks(const QList<EdmSection> &sections, quint32 clipStartMs)
{
    QList<SongBlock> blocks;
    QHash<QString, int> numbers;
    for (int i = 0; i < sections.count(); i++)
    {
        const EdmSection &s = sections.at(i);
        // both edges rounded on their own: neighbouring blocks share their edge
        const qint64 start = clipStartMs + qRound64(s.startMs);
        const qint64 end = clipStartMs + qRound64(s.endMs);
        if (end <= start || start < 0)
            continue;

        SongBlock block;
        block.section = i;
        block.label = s.label;
        block.number = ++numbers[s.label];
        block.confidence = s.confidence;
        block.unsure = s.unsure;
        block.startMs = quint32(start);
        block.durationMs = quint32(end - start);
        blocks.append(block);
    }
    return blocks;
}

SongDirector::SongDirector(Doc *doc, QObject *parent)
    : QObject(parent)
    , m_doc(doc)
{
    if (doc != nullptr)
        connect(doc, &Doc::cleared, this, [this]() { m_entries.clear(); });
}

QString SongDirector::trackName()
{
    return tr("Song Director");
}

const SongDirector::Entry *SongDirector::entry(Show *show) const
{
    if (show == nullptr)
        return nullptr;
    for (const Entry &e : m_entries)
    {
        if (e.track != nullptr && e.showId == show->id() && show->tracks().contains(e.track.data()))
            return &e;
    }
    return nullptr;
}

Track *SongDirector::track(Show *show) const
{
    const Entry *e = entry(show);
    return e != nullptr ? e->track.data() : nullptr;
}

QList<SongDirectorBlock> SongDirector::blocks(Show *show) const
{
    const Entry *e = entry(show);
    return e != nullptr ? e->blocks : QList<SongDirectorBlock>();
}

bool SongDirector::createBlocks(Show *show, EdmAnalysisReader &reader)
{
    if (m_doc == nullptr || show == nullptr || track(show) != nullptr)
        return false;
    const ShowFunction *clip = EdmAnalysisReader::firstAudioClip(m_doc, show);
    if (clip == nullptr)
        return false;
    const QList<SongBlock> generated =
        songBlocks(reader.analysis(EdmAnalysisReader::firstAudioFile(m_doc, show)).sections, clip->startTime());
    if (generated.isEmpty())
        return false;

    Track *track = new Track(Function::invalidId(), show);
    track->setName(trackName());
    show->addTrack(track);

    Entry e;
    e.track = track;
    e.showId = show->id();
    for (const SongBlock &block : generated)
    {
        Collection *collection = new Collection(m_doc);
        collection->setName(QString("%1 / %2 %3").arg(show->name(), block.label).arg(block.number));
        collection->setPath(show->path(true));
        if (m_doc->addFunction(collection) == false)
        {
            delete collection;
            continue;
        }
        ShowFunction *sf = track->createShowFunction(collection->id());
        sf->setStartTime(block.startMs);
        sf->setDuration(block.durationMs);
        sf->setColor(ShowFunction::defaultColor(Function::CollectionType));

        SongDirectorBlock sidecar;
        sidecar.functionId = collection->id();
        sidecar.generated = block;
        sidecar.content = collection->functions();
        e.blocks.append(sidecar);
    }
    m_entries.append(e);
    return true;
}

void SongDirector::saveXML(QXmlStreamWriter *doc) const
{
    QList<const Entry *> live;
    for (const Entry &e : m_entries)
    {
        Show *show = m_doc != nullptr ? qobject_cast<Show *>(m_doc->function(e.showId)) : nullptr;
        if (entry(show) == &e)
            live.append(&e);
    }
    if (live.isEmpty())
        return;

    doc->writeStartElement(XML_ELEMENT);
    for (const Entry *e : live)
    {
        doc->writeStartElement("Track");
        doc->writeAttribute("Show", QString::number(e->showId));
        doc->writeAttribute("ID", QString::number(e->track->id()));
        for (const SongDirectorBlock &b : e->blocks)
        {
            doc->writeStartElement("Block");
            doc->writeAttribute("Function", QString::number(b.functionId));
            doc->writeAttribute("Section", QString::number(b.generated.section));
            doc->writeAttribute("Label", b.generated.label);
            doc->writeAttribute("Number", QString::number(b.generated.number));
            doc->writeAttribute("Confidence", QString::number(b.generated.confidence));
            doc->writeAttribute("Unsure", b.generated.unsure ? "1" : "0");
            doc->writeAttribute("Start", QString::number(b.generated.startMs));
            doc->writeAttribute("Duration", QString::number(b.generated.durationMs));
            if (b.content.isEmpty() == false)
            {
                QStringList ids;
                for (quint32 id : b.content)
                    ids << QString::number(id);
                doc->writeAttribute("Content", ids.join(","));
            }
            doc->writeEndElement();
        }
        doc->writeEndElement();
    }
    doc->writeEndElement();
}

/** One <Block>; false when a number is unreadable */
static bool readBlock(const QXmlStreamAttributes &attrs, SongDirectorBlock *block)
{
    bool ok[6];
    block->functionId = attrs.value("Function").toUInt(&ok[0]);
    block->generated.section = attrs.value("Section").toInt(&ok[1]);
    block->generated.label = attrs.value("Label").toString();
    block->generated.number = attrs.value("Number").toInt(&ok[2]);
    block->generated.confidence = attrs.value("Confidence").toInt(&ok[3]);
    block->generated.unsure = attrs.value("Unsure") == QLatin1String("1");
    block->generated.startMs = attrs.value("Start").toUInt(&ok[4]);
    block->generated.durationMs = attrs.value("Duration").toUInt(&ok[5]);
    for (bool good : ok)
    {
        if (good == false)
            return false;
    }
    const QString content = attrs.value("Content").toString();
    for (const QString &id : content.split(',', Qt::SkipEmptyParts))
    {
        bool good = false;
        block->content.append(id.toUInt(&good));
        if (good == false)
            return false;
    }
    return true;
}

bool SongDirector::loadXML(QXmlStreamReader &doc)
{
    if (doc.name() != QLatin1String(XML_ELEMENT))
        return false;

    m_entries.clear();
    while (doc.readNextStartElement())
    {
        if (doc.name() != QLatin1String("Track"))
        {
            doc.skipCurrentElement();
            continue;
        }
        Entry e;
        e.showId = doc.attributes().value("Show").toUInt();
        Show *show = m_doc != nullptr ? qobject_cast<Show *>(m_doc->function(e.showId)) : nullptr;
        bool ok = false;
        const quint32 trackId = doc.attributes().value("ID").toUInt(&ok);
        e.track = show != nullptr && ok ? show->track(trackId) : nullptr;
        while (doc.readNextStartElement())
        {
            SongDirectorBlock block;
            if (doc.name() == QLatin1String("Block") && readBlock(doc.attributes(), &block))
                e.blocks.append(block);
            doc.skipCurrentElement();
        }
        // a Show or track that is gone, or a second track for one Show, is dropped
        if (e.track != nullptr && track(show) == nullptr)
            m_entries.append(e);
    }
    return true;
}
