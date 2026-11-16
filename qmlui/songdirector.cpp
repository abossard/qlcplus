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

QList<SongBlock> songBlocks(const QList<SongSection> &sections, quint32 clipStartMs)
{
    return songBlocks(QList<EdmSection>(sections.begin(), sections.end()), clipStartMs);
}

/** "drop 2" */
static QString blockName(const SongBlock &block)
{
    return QString("%1 %2").arg(block.label).arg(block.number);
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
            m_edits.clear();
        });
        // a removed Show's id goes to the next Function
        connect(doc, &Doc::functionRemoved, this, [this](quint32 id) {
            m_done.remove(id);
            m_waiting.remove(id);
            m_edits.remove(id);
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
            if (EdmStructureMapping::isLook(f) == false || f->contains(collection->id()))
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
    const EdmAnalysis analysis = reader.analysis(EdmAnalysisReader::firstAudioFile(m_doc, show));
    if (analysis.valid == false)
        return false;
    const QList<SongBlock> generated = songBlocks(effectiveSections(analysis, m_edits.value(show->id())), clip->startTime());
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
 *  stopped while it is the item of a kept block on the director track. The
 *  deletion of its last item there, or of the track, releases it */
static void holdBlock(QObject *context, Track *track, Collection *collection)
{
    auto release = [owner = QPointer<Track>(track), held = QPointer<Collection>(collection)]() {
        if (held == nullptr)
            return;
        if (owner != nullptr)
            for (const ShowFunction *sf : owner->showFunctions())
                if (sf->functionID() == held->id())
                    return;
        held->setHoldUntilStopped(false);
    };
    bool hasItem = false;
    for (ShowFunction *sf : track->showFunctions())
    {
        if (sf->functionID() != collection->id())
            continue;
        hasItem = true;
        QObject::connect(sf, &QObject::destroyed, context, release);
    }
    collection->setHoldUntilStopped(hasItem);
    QObject::connect(track, &QObject::destroyed, context, release);
}

bool SongDirector::addBlock(Show *show, Track *track, const SongBlock &block, QList<SongDirectorBlock> &blocks)
{
    Collection *collection = new Collection(m_doc);
    collection->setName(blockName(block));
    collection->setPath(show->path(true));
    if (m_doc->addFunction(collection) == false)
    {
        delete collection;
        return false;
    }
    if (m_mapping)
    {
        const quint32 look = m_mapping->function(block.label);
        if (look != collection->id() && EdmStructureMapping::isLook(m_doc->function(look)))
            collection->addFunction(look);
    }
    ShowFunction *sf = track->createShowFunction(collection->id());
    sf->setStartTime(block.startMs);
    sf->setDuration(block.durationMs);
    sf->setColor(ShowFunction::defaultColor(Function::CollectionType));
    holdBlock(this, track, collection);

    SongDirectorBlock sidecar;
    sidecar.functionId = collection->id();
    sidecar.generated = block;
    sidecar.content = collection->functions();
    blocks.append(sidecar);
    return true;
}

bool SongDirector::untouchedContent(const SongDirectorBlock &block) const
{
    const auto *collection = qobject_cast<Collection *>(m_doc->function(block.functionId));
    return collection == nullptr || collection->functions() == block.content;
}

ShowFunction *SongDirector::item(Track *track, quint32 functionId) const
{
    for (ShowFunction *sf : track->showFunctions())
        if (sf->functionID() == functionId)
            return sf;
    return nullptr;
}

static qint64 overlap(const SongBlock &a, const SongBlock &b)
{
    return qMin<qint64>(a.startMs + a.durationMs, b.startMs + b.durationMs) - qMax<qint64>(a.startMs, b.startMs);
}

/** Each target section takes the old block it shares time with: one of its
 *  label first, then the one it shares the most time with. -1: a new block */
static QList<int> matchBlocks(const QList<SongBlock> &target, const QList<SongDirectorBlock> &old)
{
    struct Pair { bool sameLabel; qint64 shared; int t; int o; };
    QList<Pair> pairs;
    for (int t = 0; t < target.count(); t++)
        for (int o = 0; o < old.count(); o++)
            if (old.at(o).orphan == false && overlap(target.at(t), old.at(o).generated) > 0)
                pairs.append({ old.at(o).generated.label == target.at(t).label, overlap(target.at(t), old.at(o).generated), t, o });
    std::stable_sort(pairs.begin(), pairs.end(), [](const Pair &a, const Pair &b) {
        return a.sameLabel != b.sameLabel ? a.sameLabel : a.shared > b.shared;
    });
    QList<int> owner(target.count(), -1);
    QList<bool> taken(old.count(), false);
    for (const Pair &p : pairs)
    {
        if (owner.at(p.t) >= 0 || taken.at(p.o))
            continue;
        owner[p.t] = p.o;
        taken[p.o] = true;
    }
    return owner;
}

/** The index of the block sharing the most time with \a block, or -1 */
static int mostShared(const SongBlock &block, const QList<SongDirectorBlock> &blocks)
{
    int best = -1;
    qint64 shared = 0;
    for (int i = 0; i < blocks.count(); i++)
    {
        if (blocks.at(i).orphan || overlap(block, blocks.at(i).generated) <= shared)
            continue;
        shared = overlap(block, blocks.at(i).generated);
        best = i;
    }
    return best;
}

/** Replace the Collection's content */
static void setContent(Collection *collection, const QList<quint32> &content)
{
    for (quint32 id : collection->functions())
        collection->removeFunction(id);
    for (quint32 id : content)
        collection->addFunction(id);
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

    const QList<SongDirectorBlock> old = e->blocks;
    const QList<SongBlock> target = songBlocks(effectiveSections(analysis, m_edits.value(show->id())), clip->startTime());
    const QList<int> owner = matchBlocks(target, old);
    QList<bool> kept(old.count(), false);
    for (int o : owner)
        if (o >= 0)
            kept[o] = true;

    QList<SongDirectorBlock> blocks;
    bool changed = false;
    for (int t = 0; t < target.count(); t++)
    {
        const SongBlock &section = target.at(t);
        if (owner.at(t) < 0)
        {
            // a new section: the content of the user's block of its label whose time it took (a split), else the default
            const int from = mostShared(section, old);
            const bool copy = from >= 0 && old.at(from).generated.label == section.label && untouchedContent(old.at(from)) == false;
            changed |= addBlock(show, track, section, blocks);
            if (copy && blocks.isEmpty() == false && blocks.last().generated == section)
            {
                if (auto *collection = qobject_cast<Collection *>(m_doc->function(blocks.last().functionId)))
                    if (auto *source = qobject_cast<Collection *>(m_doc->function(old.at(from).functionId)))
                    {
                        setContent(collection, source->functions());
                        blocks.last().content = collection->functions();
                    }
            }
            continue;
        }
        SongDirectorBlock b = old.at(owner.at(t));
        auto *collection = qobject_cast<Collection *>(m_doc->function(b.functionId));
        // content still as generated takes the default of a changed label
        if (collection != nullptr && b.generated.label != section.label && untouchedContent(b))
        {
            QList<quint32> content;
            const quint32 look = m_mapping ? m_mapping->function(section.label) : Function::invalidId();
            if (look != collection->id() && EdmStructureMapping::isLook(m_doc->function(look)))
                content << look;
            setContent(collection, content);
            b.content = collection->functions();
            changed = true;
        }
        if (collection != nullptr && collection->name() != blockName(section))
        {
            collection->setName(blockName(section));
            changed = true;
        }
        if (ShowFunction *sf = item(track, b.functionId))
        {
            if (sf->startTime() != section.startMs || sf->duration() != section.durationMs)
            {
                sf->setStartTime(section.startMs);
                sf->setDuration(section.durationMs);
                changed = true;
            }
        }
        if ((b.generated == section) == false)
        {
            b.generated = section;
            changed = true;
        }
        blocks.append(b);
    }

    // a block without a section goes; content the user changed joins the block now covering its time
    for (int o = 0; o < old.count(); o++)
    {
        if (kept.at(o))
            continue;
        const SongDirectorBlock &b = old.at(o);
        if (b.orphan)
        {
            blocks.append(b);
            continue;
        }
        changed = true;
        auto *source = qobject_cast<Collection *>(m_doc->function(b.functionId));
        if (source != nullptr && untouchedContent(b) == false)
        {
            const int into = mostShared(b.generated, blocks);
            auto *collection = into >= 0 ? qobject_cast<Collection *>(m_doc->function(blocks.at(into).functionId)) : nullptr;
            if (collection == nullptr)
            {
                // no block covers its time any more: kept as it is, without a section
                SongDirectorBlock orphan = b;
                orphan.orphan = true;
                source->setHoldUntilStopped(false);
                blocks.append(orphan);
                continue;
            }
            for (quint32 id : source->functions())
                if (collection->functions().contains(id) == false && id != collection->id())
                    collection->addFunction(id);
        }
        removeBlock(show, track, b.functionId);
    }
    if (changed)
        e->blocks = blocks;
    return changed;
}

void SongDirector::removeBlock(Show *show, Track *track, quint32 functionId)
{
    while (ShowFunction *sf = item(track, functionId))
    {
        emit blockRemoving(show->id(), sf);
        track->removeShowFunction(sf);
    }
    if (m_doc->function(functionId) == nullptr)
        return;
    if (m_removeFunction)
    {
        m_removeFunction(functionId);
        return;
    }
    // the runner of a block started by hand would run a freed Collection
    if (Function *f = m_doc->function(functionId); f->isRunning())
        f->stopAndWait();
    m_doc->deleteFunction(functionId);
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
        update(show, reader);
    }
}

void SongDirector::update(Show *show, EdmAnalysisReader &reader)
{
    // the runner holds the Show's items: a running Show is changed once it stops
    if (show->isRunning())
    {
        if (m_waiting.contains(show->id()) == false)
        {
            m_waiting.insert(show->id());
            connect(show, qOverload<quint32>(&Function::stopped), this, [this]() { sync(); }, Qt::QueuedConnection);
        }
        return;
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

SectionEdits SongDirector::sectionEdits(Show *show) const
{
    return show != nullptr ? m_edits.value(show->id()) : SectionEdits();
}

QList<SongSection> SongDirector::sections(Show *show, EdmAnalysisReader &reader) const
{
    const ShowFunction *clip = m_doc != nullptr ? EdmAnalysisReader::firstAudioClip(m_doc, show) : nullptr;
    if (clip == nullptr)
        return QList<SongSection>();
    const EdmAnalysis analysis = reader.analysis(EdmAnalysisReader::firstAudioFile(m_doc, show));
    if (analysis.valid == false)
        return QList<SongSection>();
    QList<SongSection> list = effectiveSections(analysis, m_edits.value(show->id()));
    for (SongSection &s : list)
    {
        s.startMs += clip->startTime();
        s.endMs += clip->startTime();
    }
    return list;
}

QList<SongSection> SongDirector::sections(Show *show) const
{
    return m_reader ? sections(show, *m_reader) : QList<SongSection>();
}

bool SongDirector::editSections(Show *show, const SectionAction &action, EdmAnalysisReader &reader, QString *error)
{
    const ShowFunction *clip = m_doc != nullptr && show != nullptr ? EdmAnalysisReader::firstAudioClip(m_doc, show) : nullptr;
    const EdmAnalysis analysis = clip ? reader.analysis(EdmAnalysisReader::firstAudioFile(m_doc, show)) : EdmAnalysis();
    if (analysis.valid == false)
    {
        if (error)
            *error = QString("the Show has no analysed audio clip");
        return false;
    }
    SectionAction songTime = action;
    songTime.timeMs -= clip->startTime();
    QString why;
    const SectionEdits before = m_edits.value(show->id());
    const SectionEdits after = applySectionAction(analysis, before, songTime, &why);
    if (error)
        *error = why;
    if (why.isEmpty() == false)
        return false;
    if (after == before)
        return true;
    if (after.isEmpty())
        m_edits.remove(show->id());
    else
        m_edits.insert(show->id(), after);
    m_doc->setModified();
    emit sectionsChanged(show->id());
    update(show, reader);
    return true;
}

bool SongDirector::editSections(Show *show, const SectionAction &action, QString *error)
{
    if (m_reader == nullptr)
    {
        if (error)
            *error = QString("no analysis folder");
        return false;
    }
    return editSections(show, action, *m_reader, error);
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
    QList<quint32> edited;
    for (auto it = m_edits.cbegin(); it != m_edits.cend(); ++it)
        if (m_doc != nullptr && qobject_cast<Show *>(m_doc->function(it.key())) != nullptr && it.value().isEmpty() == false)
            edited.append(it.key());
    std::sort(edited.begin(), edited.end());
    if (live.isEmpty() && done.isEmpty() && edited.isEmpty())
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
            if (b.orphan)
                doc->writeAttribute("Orphan", "1");
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
    for (quint32 id : edited)
    {
        doc->writeStartElement("Sections");
        doc->writeAttribute("Show", QString::number(id));
        for (const SectionPin &pin : m_edits.value(id).pins)
        {
            doc->writeStartElement("Pin");
            doc->writeAttribute("Label", pin.label);
            doc->writeAttribute("Start", QString::number(pin.startMs, 'g', 17));
            doc->writeAttribute("End", QString::number(pin.endMs, 'g', 17));
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
    block->orphan = attrs.value("Orphan") == QLatin1String("1");
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
    m_edits.clear();
    while (doc.readNextStartElement())
    {
        if (doc.name() == QLatin1String("Sections"))
        {
            bool showOk = false;
            const quint32 showId = doc.attributes().value("Show").toUInt(&showOk);
            SectionEdits edits;
            while (doc.readNextStartElement())
            {
                bool startOk = false, endOk = false;
                SectionPin pin;
                pin.label = doc.attributes().value("Label").toString();
                pin.startMs = doc.attributes().value("Start").toDouble(&startOk);
                pin.endMs = doc.attributes().value("End").toDouble(&endOk);
                if (doc.name() == QLatin1String("Pin") && startOk && endOk && pin.endMs > pin.startMs
                    && (edits.pins.isEmpty() || edits.pins.last().endMs <= pin.startMs))
                    edits.pins.append(pin);
                doc.skipCurrentElement();
            }
            if (showOk && edits.isEmpty() == false && m_doc != nullptr && qobject_cast<Show *>(m_doc->function(showId)) != nullptr)
                m_edits.insert(showId, edits);
            continue;
        }
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
                if (auto *collection = qobject_cast<Collection *>(m_doc->function(block.functionId)); collection && block.orphan == false)
                    holdBlock(this, e.track.data(), collection);
        }
    }
    return true;
}
