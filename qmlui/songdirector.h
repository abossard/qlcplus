/*
  Q Light Controller Plus
  songdirector.h

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

#ifndef SONGDIRECTOR_H
#define SONGDIRECTOR_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <functional>
#include <memory>
#include <QPointer>
#include <QString>

#include "edmanalysisreader.h"
#include "songsections.h"

class Doc;
class EdmStructureMapping;
class Show;
class ShowFunction;
class Track;
class QXmlStreamReader;
class QXmlStreamWriter;

/** One analysed section as a Show block, in Show time */
struct SongBlock
{
    int section = 0;      // index in the analysis
    QString label;
    int number = 1;       // 1 for the first section with this label, 2 for the next…
    int confidence = 0;   // percent
    bool unsure = false;
    quint32 startMs = 0;
    quint32 durationMs = 0;

    bool operator==(const SongBlock &other) const
    {
        return section == other.section && label == other.label && number == other.number
            && confidence == other.confidence && unsure == other.unsure
            && startMs == other.startMs && durationMs == other.durationMs;
    }
};

/** One block per section with a length, moved by the song's audio clip start */
QList<SongBlock> songBlocks(const QList<EdmSection> &sections, quint32 clipStartMs);
QList<SongBlock> songBlocks(const QList<SongSection> &sections, quint32 clipStartMs);

/** What the director generated for one block of its track: a block whose
 *  current values still equal these is untouched */
struct SongDirectorBlock
{
    quint32 functionId = 0;   // the block's Collection
    SongBlock generated;
    QList<quint32> content;   // the Collection's functions when generated or last refilled
    bool orphan = false;      // a block of an older workspace kept without a section: it takes no section again

    bool operator==(const SongDirectorBlock &other) const
    {
        return functionId == other.functionId && generated == other.generated && content == other.content
            && orphan == other.orphan;
    }
};

/**
 * The Song Director track of each song Show: one Collection block per
 * section, the analysis' with the user's section edits on top. Blocks always
 * take their section's start and length; their content is the user's.
 * Which track is the director's, what each block was generated as and the
 * section edits are fork data kept beside the Show:
 * <SongDirector><Track Show="5" ID="2"><Block Function="12" Section="0" …/>…
 * <Sections Show="5"><Pin Label="drop" Start="16000" End="32000"/>…
 */
class SongDirector : public QObject
{
    Q_OBJECT

public:
    explicit SongDirector(Doc *doc, QObject *parent = nullptr);

    /** The name of the tracks it creates */
    static QString trackName();

    /** The Show's Song Director track, or null */
    Track *track(Show *show) const;

    /** The generated values of the director track's blocks */
    QList<SongDirectorBlock> blocks(Show *show) const;

    /** Which Shows sync() gives blocks: Perform's Show of the song. Unset: every Show */
    void setSongShowFilter(std::function<bool(Show *)> isSongShow) { m_isSongShow = std::move(isSongShow); }

    /** The analysis folder sync() without a reader reads */
    void setAnalysisDir(const QString &dir);

    /** Every song Show with an analysis: blocks are created once, when it has
     *  no director track and never had one; afterwards blocks follow the
     *  effective sections (moved, resized, renamed, added, removed). A block
     *  keeps its content; one whose content is still as generated takes the
     *  default of a changed label. A block without a section joins its
     *  changed content into the block now covering its time. A running Show
     *  waits until it stops */
    void sync(EdmAnalysisReader &reader);

    /** The user's section edits of the Show's song */
    SectionEdits sectionEdits(Show *show) const;

    /** The Show's sections, analysis and edits, in Show time (moved by the
     *  audio clip start); empty without an analysed clip */
    QList<SongSection> sections(Show *show, EdmAnalysisReader &reader) const;

    /** One section edit on the Show (action.timeMs in Show time), then its
     *  blocks follow. False, changing nothing, when refused; error says why */
    bool editSections(Show *show, const SectionAction &action, EdmAnalysisReader &reader, QString *error = nullptr);

    /** The workspace defaults that pre-fill new blocks; none leaves them empty */
    void setMapping(EdmStructureMapping *mapping) { m_mapping = mapping; }

    /** Deletes the Collection of a block that sync() removes, as the Function
     *  Manager's delete does (editor closed, stopped). Unset: stopped and
     *  deleted from the Doc */
    void setFunctionRemover(std::function<void(quint32)> remover) { m_removeFunction = std::move(remover); }

    /** Add the Song Director track with one Collection block per section of
     *  the Show's song, holding the label's default Function when it has one. False, changing nothing, when the Show
     *  has a director track already, no audio clip or no analysis */
    bool createBlocks(Show *show, EdmAnalysisReader &reader);

    /** A Function dropped on the Song Director track at \a timeMs goes into the
     *  Collection of the block under that time, the block itself stays. Items
     *  that are no looks (EdmStructureMapping::isLook), Functions that would
     *  play the block and unknown ids are skipped. True when the drop hit a block, so
     *  the caller adds no Show item */
    bool dropOnBlock(Show *show, Track *track, quint32 timeMs, const QList<quint32> &functionIds);

    void saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &doc);

public slots:
    /** sync() with the folder of setAnalysisDir() */
    void sync();

public:
    /** editSections() with the folder of setAnalysisDir() */
    bool editSections(Show *show, const SectionAction &action, QString *error = nullptr);
    /** sections() with the folder of setAnalysisDir() */
    QList<SongSection> sections(Show *show) const;

signals:
    /** sync() created or changed the Show's blocks */
    void blocksChanged(quint32 showId);

    /** sync() is about to remove and delete this block item of the Show */
    void blockRemoving(quint32 showId, ShowFunction *item);

    /** The Show's section edits changed */
    void sectionsChanged(quint32 showId);

private:
    struct Entry
    {
        QPointer<Track> track;
        quint32 showId = 0;
        QList<SongDirectorBlock> blocks;
    };
    const Entry *entry(Show *show) const;
    /** One Collection block on the track, appended to \a blocks; false when the Doc is full */
    bool addBlock(Show *show, Track *track, const SongBlock &block, QList<SongDirectorBlock> &blocks);
    /** The Collection's content is still as generated (or last refilled) */
    bool untouchedContent(const SongDirectorBlock &block) const;
    /** The block's first item on the track, or null */
    ShowFunction *item(Track *track, quint32 functionId) const;
    /** Blocks take the Show's effective sections; true when anything changed */
    bool follow(Show *show, EdmAnalysisReader &reader);
    /** Create or follow the Show's blocks now, or once it stops running */
    void update(Show *show, EdmAnalysisReader &reader);
    /** The block's items off the track, then its Collection stopped and deleted */
    void removeBlock(Show *show, Track *track, quint32 functionId);

    Doc *m_doc;
    EdmStructureMapping *m_mapping = nullptr;
    QList<Entry> m_entries;
    std::function<bool(Show *)> m_isSongShow;
    std::function<void(quint32)> m_removeFunction;
    std::unique_ptr<EdmAnalysisReader> m_reader;
    QSet<quint32> m_done;      // Shows that got their blocks once
    QSet<quint32> m_waiting;   // running Shows a sync waits for
    QHash<quint32, SectionEdits> m_edits;   // by Show
};

#endif
