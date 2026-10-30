/*
  Q Light Controller Plus
  djsetrecorder.h

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

#ifndef DJSETRECORDER_H
#define DJSETRECORDER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

#include <array>
#include <functional>
#include <optional>

#include "djfsm.h"
#include "showclipautomation.h"

class Doc;
class Show;
class Track;
class ShowFactory;
class QDateTime;

/** Folder path of the recorded DJ sets */
inline const QString kSetFolderPath = QStringLiteral("Sets");

/**
 * Records a DJ set while Perform is on. The set is a Show (folder Sets,
 * named after its start, nested audio suppressed), created at the first
 * play, with one track per deck ("Deck N"). Every song stay on a deck is one
 * clip of the song's Show (ShowFactory): from its first play to a song
 * change, an eject, another Show for the song or Perform off; a pause stays
 * in the clip as a paused segment. The clip carries the deck's position map, its mix
 * weight as envelope and the active deck as dominance, compressed by the
 * ShowClipAutomation builders.
 *
 * The decks are read only at DjFsm::settled(), on a monotonic clock from
 * the set start. The open clips reach the set every kUpdateIntervalMs, at
 * once when one opens or closes, and mark the workspace modified; an opened
 * or closed one restarts its autosave. A set that leaves the Doc is not
 * written again: the decks that play start a new one.
 */
class DjSetRecorder final : public QObject
{
    Q_OBJECT

public:
    /** Monotonic milliseconds */
    using Clock = std::function<qint64()>;

    static constexpr int kUpdateIntervalMs = 250;
    /** A position farther than this from the prediction starts a segment */
    static constexpr quint32 kPositionToleranceMs = 40;
    static constexpr qreal kLevelTolerance = 0.02;

    DjSetRecorder(Doc *doc, DjFsm *fsm, ShowFactory *factory, QObject *parent = nullptr);

    /** The clock the set's times come from; a QElapsedTimer by default */
    void setClock(const Clock &clock);

    /** Perform on records from the next settled view; off closes every
     *  clip and finalizes the set */
    void setRecording(bool on);
    bool isRecording() const { return m_recording; }

    /** The set under way, Function::invalidId() for none */
    quint32 setShowId() const { return m_setId; }

    /** A song's clip waits for its playing set to stop: a save now misses it */
    bool hasWaitingClips() const;

    /** "Set yyyy-MM-dd HH:mm" */
    static QString setName(const QDateTime &start);

signals:
    /** The set under way is written up to ms on its timeline */
    void recordHeadChanged(quint32 setId, quint32 ms);

private:
    /** A song stay on a deck */
    struct DeckClip
    {
        QString filepath;                       //!< normalized
        quint32 showId = 0;                     //!< the song's Show
        std::optional<quint32> uid;             //!< the ShowFunction, once in the set
        qint64 startClock = 0;
        quint32 startMs = 0;                    //!< on the set's timeline
        quint32 endMs = 0;                      //!< clip time of the last view
        bool playing = true;
        QVector<ShowClipSegment> segments;      //!< closed play runs and pauses
        QVector<ShowClipPositionSample> run;    //!< the play run under way
        QVector<ShowClipLevelSample> levels;
        QVector<ShowClipInterval> active;       //!< the last one open while the deck is active
    };

    /** A closed clip still to be written to its set */
    struct ClosedClip
    {
        quint32 setId;
        int deck;                               //!< 0-based
        DeckClip clip;
    };

    /** At a settled view: opens, samples and closes the clips from the decks */
    void follow();
    /** The update timer: the open clips grow to the clock */
    void grow();
    void close(int deck, qint64 clock);
    /** A clip is open or a closed one still to be written */
    bool pending() const;
    void updateTimer();
    void sample(DeckClip &clip, const DjFsm::Deck &deck, bool active, quint32 clipMs) const;
    /** The play run under way becomes segments */
    void closeRun(DeckClip &clip) const;
    ShowClipAutomation automation(const DeckClip &clip) const;

    /** The set's Show, created starting at clock */
    Show *ensureSet(qint64 clock);
    Show *set() const;
    /** The deck's track, with the tracks of the decks before it */
    Track *deckTrack(Show *set, int deck) const;
    enum class Written
    {
        Created,    //!< a new clip in the set
        Updated,
        Waiting,    //!< a new clip waits for the running set to stop
        Gone,       //!< the set or the clip left the Doc
    };
    /** A clip into its set */
    Written writeClip(quint32 setId, int deck, DeckClip &clip);
    /** The closed clips, then the open ones */
    void write();
    /** The set left the Doc: its open clips go with it */
    void forgetSet();

    qint64 now() const;

    Doc *m_doc;
    DjFsm *m_fsm;
    ShowFactory *m_factory;
    Clock m_clock;
    QTimer m_updateTimer;

    bool m_recording = false;
    quint32 m_setId;
    qint64 m_setStartClock = 0;
    std::array<std::optional<DeckClip>, DjFsm::MaxDecks> m_clips;
    QVector<ClosedClip> m_closed;
};

#endif // DJSETRECORDER_H
