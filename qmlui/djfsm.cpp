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
    if (trigger == QLatin1String("loaded"))
    {
        if (triggerIsOn(value))
            s.loadedOn = true;
        else
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
        }
    }
    else if (trigger == QLatin1String("get_artist"))
    {
        // an empty artist is a value: a file without tags
        p.artist = value.toString();
    }
    else if (trigger == QLatin1String("get_bpm"))
    {
        bool ok = false;
        const double bpm = value.toDouble(&ok);
        if (ok && bpm > 0.0)
        {
            p.bpm = bpm;
        }
    }
    else if (trigger == QLatin1String("get_key"))
    {
        if (!value.toString().isEmpty())
        {
            p.key = value.toString();
        }
    }
    else if (trigger == QLatin1String("get_time total"))
    {
        bool ok = false;
        const double ms = value.toDouble(&ok);
        if (ok && ms > 0.0)
        {
            p.totalMs = static_cast<int>(ms); // VDJ sends milliseconds (verified from live telemetry)
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
        if (s.ejectedThisWrite || (p.filepath && *p.filepath != s.last.filepath) || reloadPending(s))
        {
            // the eject write's values are VDJ's last ones and no song's; a load
            // brings this write's fields with the ones held from the previous
            // write; the ejected file back brings only what the eject changed
            commitPending(s, d);
        }
        else if (p.any() && s.positionSeen)
        {
            // song fields without a filepath are a manual load's first write
            // until the deck's position moves on without one: the song's own
            // change (a tempo ramp, a correction). VU, EQ and fader writes in
            // between say nothing about it.
            commitPending(s, d);
        }
        s.positionSeen = false;
        s.ejectedThisWrite = false;
    }
}

bool DjFsm::reloadPending(const Slot &s) const
{
    // loaded on, no filepath, the ejected file's title: the same file again.
    // Another title is a manual load's first write, its filepath follows.
    const Pending &p = s.pending;
    return s.loadedOn && s.deck.state == DeckState::Empty && !s.last.filepath.isEmpty()
           && !p.filepath && (!p.title || *p.title == s.last.title);
}

void DjFsm::commitPending(Slot &s, int deck1)
{
    Pending &p = s.pending;
    DeckSong &last = s.last;
    const bool newFile = p.filepath && *p.filepath != last.filepath;
    const bool reload = !newFile && reloadPending(s);
    bool changed = newFile;
    if (newFile)
    {
        last.filepath = *p.filepath;
        last.totalMs = 0;
    }
    // a field VDJ did not resend is the new song's too
    if (p.title && last.title != *p.title) { last.title = *p.title; changed = true; }
    if (p.artist && last.artist != *p.artist) { last.artist = *p.artist; changed = true; }
    if (p.bpm)
    {
        // a displayed change is an integer one, not sub-integer jitter
        if (qRound(*p.bpm) != qRound(last.bpm))
            changed = true;
        last.bpm = *p.bpm;
    }
    else if ((newFile || reload) && s.ejected && last.title == s.titleBeforeEject)
    {
        // no bpm after the eject: its 120 was real for another title only
        last.bpm = s.bpmBeforeEject;
    }
    if (p.key && last.key != *p.key) { last.key = *p.key; changed = true; }
    if (p.totalMs)
        last.totalMs = *p.totalMs;
    p = Pending();

    if (!newFile && !reload && s.deck.state == DeckState::Empty)
        return; // the empty deck's last values moved, no song did

    DeckSong &song = s.deck.song;
    if (newFile || reload)
    {
        s.deck.positionStale = !s.positionSeen;
        s.loadedOn = false;
        s.ejected = false;
        changed = true;
    }
    song = last;
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
    // a disconnect or a new subscription: VDJ's last values start over
    s.last         = DeckSong{};
    s.deck.volume  = 0.0;
    s.deck.number  = deck1;
    s.master       = false;
    s.pending      = Pending();
    s.positionSeen = false;
    s.ejected      = false;
    s.bpmBeforeEject = 0.0;
    s.titleBeforeEject.clear();
    emptyDeck(s);
}

void DjFsm::ejectDeck(Slot &s, int deck1)
{
    s.ejectedThisWrite = true;
    if (!s.deck.song.isValid() && s.deck.state == DeckState::Empty)
        return;
    s.ejected = true;
    s.bpmBeforeEject = s.deck.song.bpm;
    s.titleBeforeEject = s.deck.song.title;
    emptyDeck(s);
    emit deckChanged(deck1);
    if (deck1 == m_activeDeck)
        recomputeActiveSong();
}

void DjFsm::emptyDeck(Slot &s)
{
    // the song leaves the deck; VDJ's last values stay in `last` for the next
    // load, the fader and the master flag are no song fields
    s.deck.state   = DeckState::Empty;
    s.deck.song    = DeckSong{};
    s.deck.playing = false;
    s.loadedOn     = false;
    s.pending      = Pending(); // a held first write dies with the eject
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
