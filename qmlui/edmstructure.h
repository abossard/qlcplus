/*
  Q Light Controller Plus
  edmstructure.h

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

#ifndef EDMSTRUCTURE_H
#define EDMSTRUCTURE_H

#include "djfsm.h"
#include "edmanalysisqueue.h"
#include "edmanalysisreader.h"
#include "edmsongposition.h"

#include <QObject>
#include <QVariantList>

/**
 * One deck's song structure for QML. The static part (state, sections,
 * events) and the live part (the playhead's values) notify separately: the
 * live part at most every 1/16 bar, and at once on a section change or a jump.
 * Bars are the analysis' bars, bar 0 at the song's first downbeat.
 */
class EdmDeckStructure : public QObject
{
    Q_OBJECT

    // static: none (no song, or no analysis and none coming), lookup, queued, analysing, ready, failed
    Q_PROPERTY(QString path MEMBER m_path NOTIFY staticChanged)               // the song on the deck, empty when none
    Q_PROPERTY(QString state MEMBER m_state NOTIFY staticChanged)
    Q_PROPERTY(QString stage MEMBER m_stage NOTIFY staticChanged)
    Q_PROPERTY(QString error MEMBER m_error NOTIFY staticChanged)
    Q_PROPERTY(QVariantList sections MEMBER m_sections NOTIFY staticChanged)   // {label, startBar, endBar, confidence}
    Q_PROPERTY(QVariantList events MEMBER m_events NOTIFY staticChanged)       // {kind, bar, confidence}, -1 without one
    Q_PROPERTY(double totalBars MEMBER m_totalBars NOTIFY staticChanged)

    // live
    Q_PROPERTY(double bar MEMBER m_bar NOTIFY liveChanged)
    Q_PROPERTY(int section MEMBER m_section NOTIFY liveChanged)                // -1: none
    Q_PROPERTY(double barInSection MEMBER m_barInSection NOTIFY liveChanged)
    Q_PROPERTY(double barsToNextSection MEMBER m_barsToNextSection NOTIFY liveChanged)   // -1: none
    Q_PROPERTY(QString nextSectionLabel MEMBER m_nextSectionLabel NOTIFY liveChanged)
    Q_PROPERTY(int nextSectionConfidence MEMBER m_nextSectionConfidence NOTIFY liveChanged)
    Q_PROPERTY(double barsToDrop MEMBER m_barsToDrop NOTIFY liveChanged)       // -1: none
    Q_PROPERTY(int dropConfidence MEMBER m_dropConfidence NOTIFY liveChanged)  // -1: none or not in the file

public:
    explicit EdmDeckStructure(QObject *parent = nullptr) : QObject(parent) {}

signals:
    void staticChanged();
    void liveChanged();

private:
    friend class EdmStructure;

    QString m_path;
    QString m_state = "none";
    QString m_stage;
    QString m_error;
    QVariantList m_sections;
    QVariantList m_events;
    double m_totalBars = 0;
    double m_bar = 0;
    int m_section = -1;
    double m_barInSection = -1;
    double m_barsToNextSection = -1;
    QString m_nextSectionLabel;
    int m_nextSectionConfidence = -1;
    double m_barsToDrop = -1;
    int m_dropConfidence = -1;
};

/**
 * Follows the songs on VirtualDJ's decks: finds each song's analysis (by path,
 * then by content off the main thread, then by queueing the offline tool),
 * and turns the decks' positions into song positions and crossings.
 * Decks are 1-based, as in DjFsm.
 */
class EdmStructure : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int activeDeck READ activeDeck NOTIFY activeDeckChanged)

public:
    EdmStructure(DjFsm *fsm, EdmAnalysisQueue *queue, const QString &analysisDir = EdmAnalysisReader::defaultDir(),
                 QObject *parent = nullptr);

    int activeDeck() const { return m_active; }

    /** The deck's structure, 1..DjFsm::MaxDecks; null otherwise */
    Q_INVOKABLE QObject *deck(int deck);

    /** none, queued, analysing, ready or failed, for a song on a deck or not */
    Q_INVOKABLE QString analysisStateForPath(const QString &path);

    /** Analyse this song now, whatever the auto setting; a song that is ready, queued or running is left alone, a failed one runs again */
    Q_INVOKABLE void retry(const QString &path);
    Q_INVOKABLE void analyse(const QString &path);

signals:
    /** A followed deck (it has an analysis and a position) moved: crossed
     *  something, jumped back, or became or stopped being the active deck */
    void deckAdvanced(int deck, const EdmAnalysis &analysis, const EdmSongPosition &position,
                      const EdmCrossings &crossings, bool activeDeck);
    /** A deck that was followed has no analysis any more (ejected, another song), or the
     *  active deck became one without an analysis (then deck is the new active deck) */
    void deckLost(int deck, bool activeDeck);
    void activeDeckChanged();
    void analysisStateChanged(const QString &path);

private slots:
    void slotSongChanged(int deck);
    void slotSettled();
    void slotEntryChanged(const QString &path);

private:
    struct Deck
    {
        EdmDeckStructure *view = nullptr;
        QString path;
        EdmAnalysis analysis;
        int token = 0;           //!< a lookup result for an older song is dropped
        bool missed = false;     //!< both lookups found nothing: the queue decides
        bool followed = false;
        bool lastActive = false;
        bool hasLast = false;
        double lastBar = 0;
        double notifiedBar = 0;
    };

    void syncDeck(int deck);
    void lookUp(int deck);
    void contentLookupDone(int deck, int token, const QString &id);
    void takeQueueState(int deck);
    void setState(int deck, const QString &state, const QString &stage = QString(), const QString &error = QString());
    void setAnalysis(int deck, const EdmAnalysis &analysis);
    void advance(int deck, int active);
    void setLive(Deck &d, const EdmSongPosition &position);

    DjFsm *m_fsm;
    EdmAnalysisQueue *m_queue;
    QString m_dir;
    EdmAnalysisReader m_reader;
    Deck m_decks[DjFsm::MaxDecks];
    int m_active = 0;
};

#endif
