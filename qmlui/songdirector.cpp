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
#include "edmstructuremapping.h"
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
        // the label's confidence: the analysis flags a section unsure with a constant section confidence of 50
        block.confidence = s.labelConfidence;
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
    {
        connect(doc, &Doc::cleared, this, [this]() {
            m_entries.clear();
            m_done.clear();
            m_waiting.clear();
        });
        // a removed Show's id goes to the next Function
        connect(doc, &Doc::functionRemoved, this, [this](quint32 id) {
            m_done.remove(id);
            m_waiting.remove(id);
        });
    }
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

bool SongDirector::dropOnBlock(Show *show, Track *track, quint32 timeMs, const QList<quint32> &functionIds)
{
    if (m_doc == nullptr || show == nullptr || track == nullptr || track != this->track(show))
        return false;

    // a block owns its start and not its end: the shared edge belongs to the next block
    for (ShowFunction *sf : track->showFunctions())
    {
        if (timeMs < sf->startTime() || timeMs >= sf->startTime() + sf->duration())
            continue;
        auto *collection = qobject_cast<Collection *>(m_doc->function(sf->functionID()));
        if (collection == nullptr)
            return false;
        for (quint32 id : functionIds)
        {
            Function *f = m_doc->function(id);
            if (f == nullptr || f->type() == Function::ShowType || f->contains(collection->id()))
                continue;
            collection->addFunction(id);
        }
        return true;
    }
    return false;
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
        addBlock(show, track, block, e.blocks);
    m_entries.append(e);
    m_done.insert(show->id());
    return true;
}

/** A block runs for the length of its Show item: its Collection holds until
 *  stopped while it is a block of the director track, the track's deletion releases it */
static void holdBlock(QObject *context, Track *track, Collection *collection)
{
    collection->setHoldUntilStopped(true);
    QObject::connect(track, &QObject::destroyed, context, [held = QPointer<Collection>(collection)]() {
        if (held)
            held->setHoldUntilStopped(false);
    });
}

bool SongDirector::addBlock(Show *show, Track *track, const SongBlock &block, QList<SongDirectorBlock> &blocks)
{
    Collection *collection = new Collection(m_doc);
    collection->setName(QString("%1 / %2 %3").arg(show->name(), block.label).arg(block.number));
    collection->setPath(show->path(true));
    if (m_doc->addFunction(collection) == false)
    {
        delete collection;
        return false;
    }
    holdBlock(this, track, collection);
    if (m_mapping)
    {
        const quint32 look = m_mapping->function(block.label);
        if (look != Function::invalidId() && look != collection->id() && m_doc->function(look) != nullptr)
            collection->addFunction(look);
    }
    ShowFunction *sf = track->createShowFunction(collection->id());
    sf->setStartTime(block.startMs);
    sf->setDuration(block.durationMs);
    sf->setColor(ShowFunction::defaultColor(Function::CollectionType));

    SongDirectorBlock sidecar;
    sidecar.functionId = collection->id();
    sidecar.generated = block;
    sidecar.content = collection->functions();
    blocks.append(sidecar);
    return true;
}

ShowFunction *SongDirector::untouchedItem(Track *track, const SongDirectorBlock &block) const
{
    ShowFunction *item = nullptr;
    for (ShowFunction *sf : track->showFunctions())
    {
        if (sf->functionID() != block.functionId)
            continue;
        if (item != nullptr)
            return nullptr;   // a block copied is a block edited
        item = sf;
    }
    const auto *collection = qobject_cast<Collection *>(m_doc->function(block.functionId));
    if (item == nullptr || collection == nullptr || item->startTime() != block.generated.startMs
        || item->duration() != block.generated.durationMs || collection->functions() != block.content)
        return nullptr;
    return item;
}

bool SongDirector::follow(Show *show, EdmAnalysisReader &reader)
{
    Entry *e = const_cast<Entry *>(entry(show));
    const ShowFunction *clip = EdmAnalysisReader::firstAudioClip(m_doc, show);
    if (e == nullptr || clip == nullptr)
        return false;
    const EdmAnalysis analysis = reader.analysis(EdmAnalysisReader::firstAudioFile(m_doc, show));
    if (analysis.valid == false)
        return false;   // an analysis that is gone leaves the blocks as they are
    Track *track = e->track.data();

    // a section takes the block of its label and number (drop 2 stays drop 2)
    const QList<SongDirectorBlock> old = e->blocks;
    QList<bool> matched(old.count(), false);
    QList<SongDirectorBlock> blocks;
    bool changed = false;
    for (const SongBlock &block : songBlocks(analysis.sections, clip->startTime()))
    {
        int i = 0;
        while (i < old.count() && (matched.at(i) || old.at(i).generated.label != block.label
                                   || old.at(i).generated.number != block.number))
            i++;
        if (i == old.count())
        {
            changed |= addBlock(show, track, block, blocks);
            continue;
        }
        matched[i] = true;
        SongDirectorBlock b = old.at(i);
        ShowFunction *sf = untouchedItem(track, b);
        if (sf != nullptr && (b.generated == block) == false)
        {
            sf->setStartTime(block.startMs);
            sf->setDuration(block.durationMs);
            b.generated = block;
            changed = true;
        }
        blocks.append(b);
    }
    // a block without its section goes when untouched; one the user changed stays
    for (int i = 0; i < old.count(); i++)
    {
        if (matched.at(i))
            continue;
        if (ShowFunction *sf = untouchedItem(track, old.at(i)))
        {
            track->removeShowFunction(sf);
            m_doc->deleteFunction(old.at(i).functionId);
            changed = true;
        }
        else
            blocks.append(old.at(i));
    }
    if (changed)
        e->blocks = blocks;
    return changed;
}

void SongDirector::setAnalysisDir(const QString &dir)
{
    m_reader = std::make_unique<EdmAnalysisReader>(dir);
}

void SongDirector::sync()
{
    if (m_reader)
        sync(*m_reader);
}

void SongDirector::sync(EdmAnalysisReader &reader)
{
    if (m_doc == nullptr)
        return;
    for (Function *f : m_doc->functionsByType(Function::ShowType))
    {
        Show *show = qobject_cast<Show *>(f);
        if (show == nullptr || (m_isSongShow && m_isSongShow(show) == false))
            continue;
        // the runner holds the Show's items: a running Show is changed once it stops
        if (show->isRunning())
        {
            if (m_waiting.contains(show->id()) == false)
            {
                m_waiting.insert(show->id());
                connect(show, qOverload<quint32>(&Function::stopped), this, [this]() { sync(); }, Qt::QueuedConnection);
            }
            continue;
        }
        const bool changed = track(show) == nullptr
            ? m_done.contains(show->id()) == false && createBlocks(show, reader)
            : follow(show, reader);
        if (changed)
        {
            m_doc->setModified();
            emit blocksChanged(show->id());
        }
    }
}

void SongDirector::saveXML(QXmlStreamWriter *doc) const
{
    QList<const Entry *> live;
    QList<quint32> done;   // Shows that had their blocks and lost the track: not created again
    for (const Entry &e : m_entries)
    {
        Show *show = m_doc != nullptr ? qobject_cast<Show *>(m_doc->function(e.showId)) : nullptr;
        if (entry(show) == &e)
            live.append(&e);
    }
    for (quint32 id : m_done)
    {
        Show *show = m_doc != nullptr ? qobject_cast<Show *>(m_doc->function(id)) : nullptr;
        if (show != nullptr && entry(show) == nullptr)
            done.append(id);
    }
    std::sort(done.begin(), done.end());
    if (live.isEmpty() && done.isEmpty())
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
    for (quint32 id : done)
    {
        doc->writeStartElement("Done");
        doc->writeAttribute("Show", QString::number(id));
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
    m_done.clear();
    while (doc.readNextStartElement())
    {
        if (doc.name() == QLatin1String("Done"))
        {
            m_done.insert(doc.attributes().value("Show").toUInt());
            doc.skipCurrentElement();
            continue;
        }
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
        {
            m_entries.append(e);
            m_done.insert(e.showId);
            for (const SongDirectorBlock &block : e.blocks)
                if (auto *collection = qobject_cast<Collection *>(m_doc->function(block.functionId)))
                    holdBlock(this, e.track.data(), collection);
        }
    }
    return true;
}
