/*
  Q Light Controller Plus
  djfsm.cpp

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

#include "djfsm.h"

#include <algorithm>
#include <cmath>

namespace {
// VDJ booleans arrive as "on"/"off", true/false, or 1/0 (the telemetry client
// coerces JSON numbers→double and JSON bools→bool). Treat any truthy form as on.
bool triggerIsOn(const QVariant &value)
{
    if (value.typeId() == QMetaType::Bool)
        return value.toBool();
    bool ok = false;
    const double d = value.toDouble(&ok);
    if (ok)
        return d != 0.0;
    const QString s = value.toString().trimmed().toLower();
    return s == QLatin1String("on") || s == QLatin1String("true") || s == QLatin1String("1");
}
} // namespace


DjFsm::DjFsm(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<DjFsm::DeckSong>("DjFsm::DeckSong");

    for (int i = 0; i < MaxDecks; ++i)
        m_slots[i].deck.number = i + 1;
}

// ────────────────────────────────────────────────────────────────
//  Topology
// ────────────────────────────────────────────────────────────────

void DjFsm::setDeckCount(int count)
{
    const int clamped = std::clamp(count, 1, MaxDecks);
    if (clamped == m_deckCount)
        return;

    const int prev = m_deckCount;
    m_deckCount = clamped;

    for (int d = clamped; d < prev; ++d)
    {
        resetSlot(m_slots[d], d + 1);
        emit deckChanged(d + 1);
    }

    // the master flag may have reached a deck the old count hid (a real deck
    // 3 before get_decks 4) or dropped (a phantom copy before get_decks 2)
    followMasterFlags();

    emit deckCountChanged(m_deckCount);
}

void DjFsm::followMasterFlags()
{
    int master = 0;
    for (int d = 1; d <= m_deckCount && master == 0; ++d)
        if (m_slots[d - 1].master)
            master = d;
    if (master == m_activeDeck || (master == 0 && deckTracked(m_activeDeck)))
        return;
    m_activeDeck = master;
    emit activeDeckChanged(m_activeDeck);
    recomputeActiveSong();
}

// ────────────────────────────────────────────────────────────────
//  Event inputs
// ────────────────────────────────────────────────────────────────

void DjFsm::onDeckTrigger(int deck, const QString &trigger, const QVariant &value)
{
    if (trigger == QLatin1String("masterdeck"))
    {
        if (!indexValid(deck - 1))
            return;
        m_slots[deck - 1].master = triggerIsOn(value);
        if (m_slots[deck - 1].master && deckTracked(deck))
            onMasterDeck(deck);
        return;
    }
    if (!deckTracked(deck))
        return;

    Slot &s = slotForDeck(deck);
    Pending &p = s.pending;
    // any trigger of the deck: a playing deck's held fields flush at its next
    // tick, a paused deck's wait for its next event (its load's second write)
    s.touched = true;

    if (trigger == QLatin1String("loaded"))
    {
        if (!triggerIsOn(value))
            ejectDeck(s, deck);
    }
    else if (trigger == QLatin1String("get_filepath"))
    {
        const QString path = value.toString();
        if (path.isEmpty())
            ejectDeck(s, deck);
        else
        {
            p.filepath = path;
            p.fresh = true;
        }
    }
    else if (trigger == QLatin1String("get_title"))
    {
        const QString title = value.toString();
        // VDJ shows the placeholder on an empty deck only: an eject's first word
        if (isPlaceholderTitle(title))
            ejectDeck(s, deck);
        else if (!title.isEmpty())
        {
            p.title = title;
            p.fresh = true;
        }
    }
    else if (trigger == QLatin1String("get_artist"))
    {
        // an empty artist is a value: a file without tags
        p.artist = value.toString();
        p.fresh = true;
    }
    else if (trigger == QLatin1String("get_bpm"))
    {
        bool ok = false;
        const double bpm = value.toDouble(&ok);
        if (ok && bpm > 0.0)
        {
            p.bpm = bpm;
            p.fresh = true;
        }
    }
    else if (trigger == QLatin1String("get_key"))
    {
        if (!value.toString().isEmpty())
        {
            p.key = value.toString();
            p.fresh = true;
        }
    }
    else if (trigger == QLatin1String("get_time total"))
    {
        bool ok = false;
        const double ms = value.toDouble(&ok);
        if (ok && ms > 0.0)
        {
            p.totalMs = static_cast<int>(ms); // VDJ sends milliseconds (verified from live telemetry)
            p.fresh = true;
        }
    }
    else if (trigger == QLatin1String("play"))
    {
        // Store the raw play flag; the effective playing state is derived in
        // recomputeState() once the song is valid, so a play=on arriving before
        // its filepath commits is not lost.
        s.rawPlaying = triggerIsOn(value);
        const bool prevPlaying = s.deck.playing;
        recomputeState(s);
        if (s.deck.playing != prevPlaying)
            emit deckChanged(deck);
    }
    else if (trigger == QLatin1String("volume"))
    {
        bool ok = false;
        const double v = value.toDouble(&ok);
        if (ok && std::abs(v - s.deck.volume) >= 0.01)
        {
            s.deck.volume = v;
            emit deckChanged(deck);
        }
        else if (ok)
        {
            s.deck.volume = v; // store fine value without a visible refresh
        }
    }
    else if (trigger == QLatin1String("get_beatpos"))
    {
        bool ok = false;
        const double v = value.toDouble(&ok);
        if (ok)
        {
            s.deck.beatPos = v;          // full resolution stored for sync
            maybeEmitPosition(s, deck);  // throttled UI notification
        }
    }
    else if (trigger == QLatin1String("get_time elapsed absolute"))
    {
        bool ok = false;
        const double ms = value.toDouble(&ok);
        if (ok && ms >= 0.0)
        {
            s.deck.elapsedMs = static_cast<int>(ms); // VDJ sends milliseconds (verified from live telemetry)
            s.deck.positionStale = false;
            s.positionSeen = true;
            maybeEmitPosition(s, deck);
        }
    }
    else if (trigger == QLatin1String("get_time"))
    {
        bool ok = false;
        const double ms = value.toDouble(&ok);
        if (ok && ms >= 0.0)
            s.deck.remainingMs = static_cast<int>(ms); // VDJ sends milliseconds remaining (verified from live telemetry)
    }
}

void DjFsm::onTickEnded()
{
    for (int d = 1; d <= m_deckCount; ++d)
    {
        Slot &s = m_slots[d - 1];
        Pending &p = s.pending;
        if (p.filepath && *p.filepath != s.deck.song.filepath)
        {
            // a load: this write's fields with the ones held from the previous write
            commitPending(s, d);
        }
        else if (p.any())
        {
            // song fields without a filepath: a manual load's first write, or
            // the song's own change. The deck's next write tells which.
            if (p.fresh)
                p.fresh = false;
            else if (s.touched)
                commitPending(s, d);
        }
        s.touched = false;
        s.positionSeen = false;
    }
}

void DjFsm::commitPending(Slot &s, int deck1)
{
    Pending &p = s.pending;
    DeckSong &song = s.deck.song;
    const bool newSong = p.filepath && *p.filepath != song.filepath;
    bool changed = false;
    if (newSong)
    {
        song.filepath = *p.filepath;
        song.totalMs = 0;
        s.deck.positionStale = !s.positionSeen;
        changed = true;
    }
    // a title VDJ did not resend is the new song's title too
    if (p.title && song.title != *p.title) { song.title = *p.title; changed = true; }
    if (p.artist && song.artist != *p.artist) { song.artist = *p.artist; changed = true; }
    if (p.bpm)
    {
        // a displayed change is an integer one, not sub-integer jitter
        if (qRound(*p.bpm) != qRound(song.bpm))
            changed = true;
        song.bpm = *p.bpm;
    }
    if (p.key && song.key != *p.key) { song.key = *p.key; changed = true; }
    if (p.totalMs)
        song.totalMs = *p.totalMs;
    p = Pending();

    const bool prevPlaying = s.deck.playing;
    recomputeState(s);
    if (song.isValid() && !song.title.isEmpty() && song.filepath != s.committedFilepath)
    {
        s.committedFilepath = song.filepath;
        emit songChanged(deck1, song);
    }
    else if (changed && song.filepath == s.committedFilepath && song.isValid())
    {
        emit songUpdated(deck1, song);
    }
    if (changed || s.deck.playing != prevPlaying)
    {
        emit deckChanged(deck1);
        if (deck1 == m_activeDeck)
            recomputeActiveSong();
    }
}

void DjFsm::onMasterDeck(int deck)
{
    if (!deckTracked(deck) || deck == m_activeDeck)
        return;

    m_activeDeck = deck;
    emit activeDeckChanged(deck);
    recomputeActiveSong();
}

void DjFsm::onDisconnected()
{
    // no active deck first: nothing reads the emptied decks as the active one
    if (m_activeDeck != 0)
    {
        m_activeDeck = 0;
        emit activeDeckChanged(0);
        recomputeActiveSong();
    }
    for (int i = 0; i < MaxDecks; ++i)
    {
        resetSlot(m_slots[i], i + 1);
        emit deckChanged(i + 1);
    }
}

// ────────────────────────────────────────────────────────────────
//  State access
// ────────────────────────────────────────────────────────────────

const DjFsm::Deck &DjFsm::deckAt(int index0) const
{
    static const Deck invalid;
    if (!indexValid(index0))
        return invalid;
    return m_slots[index0].deck;
}

DjFsm::DeckSong DjFsm::activeSong() const
{
    if (m_activeDeck < 1 || m_activeDeck > m_deckCount)
        return DeckSong{};
    return m_slots[m_activeDeck - 1].deck.song;
}

QString DjFsm::stateName(DeckState s)
{
    switch (s)
    {
    case DeckState::Empty:   return QStringLiteral("Empty");
    case DeckState::Loading: return QStringLiteral("Loading");
    case DeckState::Loaded:  return QStringLiteral("Loaded");
    case DeckState::Playing: return QStringLiteral("Playing");
    case DeckState::Paused:  return QStringLiteral("Paused");
    }
    return QStringLiteral("Unknown");
}

// ────────────────────────────────────────────────────────────────
//  Private
// ────────────────────────────────────────────────────────────────

DjFsm::Slot &DjFsm::slotForDeck(int deck)
{
    const int idx = (deck >= 1 && deck <= MaxDecks) ? deck - 1 : 0;
    return m_slots[idx];
}

void DjFsm::resetSlot(Slot &s, int deck1)
{
    s.deck.song    = DeckSong{};
    s.deck.volume  = 0.0;
    s.deck.number  = deck1;
    s.master       = false;
    s.pending      = Pending();
    s.touched      = false;
    s.positionSeen = false;
    emptyDeck(s);
}

void DjFsm::ejectDeck(Slot &s, int deck1)
{
    if (!s.deck.song.isValid() && s.deck.state == DeckState::Empty)
        return;
    emptyDeck(s);
    emit deckChanged(deck1);
    if (deck1 == m_activeDeck)
        recomputeActiveSong();
}

void DjFsm::emptyDeck(Slot &s)
{
    // the song leaves; VDJ's last artist, bpm and key stay the next song's
    // until VDJ sends a change, the fader and the master flag are no song fields
    s.deck.state   = DeckState::Empty;
    s.deck.song.filepath.clear();
    s.deck.song.title.clear();
    s.deck.song.totalMs = 0;
    s.deck.playing = false;
    s.deck.beatPos = 0.0;
    s.deck.elapsedMs = 0;
    s.deck.remainingMs = 0;
    s.deck.positionStale = false;
    s.committedFilepath.clear();
    s.wasPlaying   = false;
    s.rawPlaying   = false;
    s.posEmitSecond = -1;
    s.posEmitBeatQ  = -1000;
}

void DjFsm::recomputeState(Slot &s)
{
    // The effective playing state is gated on song validity: a deck with no
    // song (no filepath) is always Empty and never Playing — this filters VDJ's
    // play broadcasts to phantom/mirror decks. Deriving from the raw flag here
    // (rather than mutating it in the play handler) means a play=on that arrived
    // before the filepath is correctly applied once the song becomes valid.
    const bool valid = s.deck.song.isValid();
    s.deck.playing = s.rawPlaying && valid;

    if (!valid)
    {
        s.deck.state = DeckState::Empty;
        s.wasPlaying = false;
        return;
    }

    if (s.deck.playing)
        s.wasPlaying = true;

    if (s.deck.song.title.isEmpty())
        s.deck.state = DeckState::Loading;
    else if (s.deck.playing)
        s.deck.state = DeckState::Playing;
    else if (s.wasPlaying)
        s.deck.state = DeckState::Paused;
    else
        s.deck.state = DeckState::Loaded;
}

bool DjFsm::isPlaceholderTitle(const QString &title)
{
    // VDJ shows this on empty decks (English locale; other locales may differ).
    return title == QLatin1String("Drag a song on this deck to load it");
}

void DjFsm::maybeEmitPosition(Slot &s, int deck1)
{
    // Position fields are stored at full resolution (for show sync); the UI
    // notification is throttled to once per displayed second or quarter-beat.
    const int second = s.deck.elapsedMs / 1000;
    const int beatQ  = static_cast<int>(s.deck.beatPos * 4.0);
    if (second == s.posEmitSecond && beatQ == s.posEmitBeatQ)
        return;
    s.posEmitSecond = second;
    s.posEmitBeatQ  = beatQ;
    emit positionChanged(deck1);
}

void DjFsm::recomputeActiveSong()
{
    const DeckSong song = activeSong();
    const QString key = song.filepath + QLatin1Char('\n') + song.title + QLatin1Char('\n') + song.artist;
    if (key == m_activeSongKey)
        return;
    m_activeSongKey = key;
    emit activeSongChanged();
}
