/*
  Q Light Controller Plus
  songloadtracker.h

  Copyright (c) Massimo Callegari

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

#ifndef SONGLOADTRACKER_H
#define SONGLOADTRACKER_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVariant>

/**
 * Pure FSM that tracks when a brand-new song has finished loading on a VDJ deck.
 *
 * Emits songReady() exactly once per unique filepath per session (handles
 * linked decks, placeholder titles, out-of-order fields, and disconnects).
 *
 * No Doc, no Audio, no sockets — purely event-driven from trigger inputs.
 */
class SongLoadTracker : public QObject
{
    Q_OBJECT

public:
    explicit SongLoadTracker(QObject *parent = nullptr);

    struct SongInfo {
        int     deck      = 0;
        QString filepath;
        QString title;
        QString artist;
        QString album;
        QString genre;
        QString key;
        double  bpm       = 0.0;
        double  firstBeat = 0.0;
        int     totalMs   = 0;
    };

    /** Route a per-deck trigger into the FSM. deck is 1-based. */
    void onTrigger(int deck, const QString &trigger, const QVariant &value);

    /** Update master deck (1-based). Stored but not used for emission logic. */
    void onMasterDeck(int deck);

    /** VDJ socket dropped — reset all slots and clear emitted set. */
    void onDisconnected();

    // --- Introspection (for tests) ---
    int  masterDeck() const { return m_masterDeck; }
    bool hasEmitted(const QString &filepath) const;

signals:
    void songReady(const SongLoadTracker::SongInfo &info);

private:
    enum class DeckState { Empty, Collecting, Ready };

    enum FieldBit : quint16 {
        F_Filepath = 1 << 0,
        F_Title    = 1 << 1,
        F_Artist   = 1 << 2,
        F_Bpm      = 1 << 3,
        F_Loaded   = 1 << 4,
    };
    static constexpr quint16 REQUIRED = F_Filepath | F_Title | F_Artist | F_Bpm | F_Loaded;

    struct DeckSlot {
        DeckState state   = DeckState::Empty;
        SongInfo  info;
        bool      loaded  = false;
        quint16   gotMask = 0;
    };

    DeckSlot &slot(int deck);
    void resetSlot(DeckSlot &s);
    void tryEmit(int deck, DeckSlot &s);

    QHash<int, DeckSlot> m_slots;
    QSet<QString>        m_emittedPaths;
    int                  m_masterDeck = 1;
};

#endif // SONGLOADTRACKER_H
