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

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include "edmanalysisreader.h"

class Doc;
class Show;
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

/** What the director generated for one block of its track: a block whose
 *  current values still equal these is untouched */
struct SongDirectorBlock
{
    quint32 functionId = 0;   // the block's Collection
    SongBlock generated;
    QList<quint32> content;   // the Collection's functions when generated

    bool operator==(const SongDirectorBlock &other) const
    {
        return functionId == other.functionId && generated == other.generated && content == other.content;
    }
};

/**
 * The Song Director track of each song Show: one Collection block per
 * analysed section. Which track is the director's, and what each block was
 * generated as, is fork data kept beside the Show:
 * <SongDirector><Track Show="5" ID="2"><Block Function="12" Section="0" …/>…
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

    /** Add the Song Director track with one empty Collection block per
     *  section of the Show's song. False, changing nothing, when the Show
     *  has a director track already, no audio clip or no analysis */
    bool createBlocks(Show *show, EdmAnalysisReader &reader);

    void saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &doc);

private:
    struct Entry
    {
        QPointer<Track> track;
        quint32 showId = 0;
        QList<SongDirectorBlock> blocks;
    };
    const Entry *entry(Show *show) const;

    Doc *m_doc;
    QList<Entry> m_entries;
};

#endif
