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

// VDJ's title of an empty deck (English locale; other locales may differ)
const QLatin1String kPlaceholderTitle("Drag a song on this deck to load it");

// a displayed change: sub-integer bpm jitter is none
bool sameSong(const DjFsm::DeckSong &a, const DjFsm::DeckSong &b)
{
    return a.filepath == b.filepath && a.title == b.title && a.artist == b.artist && a.key == b.key
           && a.totalMs == b.totalMs && qRound(a.bpm) == qRound(b.bpm);
}

// announced: a song with a title, once per filepath it stays on the deck
bool announced(const DjFsm::DeckSong &song)
{
    return song.isValid() && !song.title.isEmpty();
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
        m_slots[d] = Slot();
        m_slots[d].deck.number = d + 1;
    }
    recomputeActiveDeck();
    for (int d = clamped; d < prev; ++d)
        emit deckChanged(d + 1);
    emit deckCountChanged(m_deckCount);
}

// ────────────────────────────────────────────────────────────────
//  Event inputs
// ────────────────────────────────────────────────────────────────

void DjFsm::onDeckTrigger(int deck, const QString &trigger, const QVariant &value)
{
    if (!indexValid(deck - 1))
        return;
    Slot &s = m_slots[deck - 1];
    if (trigger == QLatin1String("masterdeck"))
    {
        s.master = triggerIsOn(value);
        return;
    }
    if (deck > m_deckCount)
        return;

    Pending &p = s.pending;
    if (trigger == QLatin1String("loaded"))
    {
        s.loaded = triggerIsOn(value);
        s.loadedOnSeen = s.loadedOnSeen || s.loaded;
    }
    else if (trigger == QLatin1String("get_filepath"))
        p.filepath = value.toString();
    else if (trigger == QLatin1String("get_title"))
        p.title = value.toString();
    else if (trigger == QLatin1String("get_bpm"))
    {
        bool ok = false;
        const double bpm = value.toDouble(&ok);
        if (ok && bpm > 0.0)
            p.bpm = bpm;
    }
    else if (trigger == QLatin1String("get_artist"))
    {
        // an empty artist is a value: a file without tags
        p.artist = value.toString();
        s.perFileSeen = true;
    }
    else if (trigger == QLatin1String("get_key"))
    {
        p.key = value.toString();
        s.perFileSeen = true;
    }
    else if (trigger == QLatin1String("get_time total"))
    {
        bool ok = false;
        const double ms = value.toDouble(&ok);
        if (ok && ms > 0.0)
            p.totalMs = static_cast<int>(ms); // VDJ sends milliseconds (verified from live telemetry)
        s.perFileSeen = true;
    }
    else if (trigger == QLatin1String("get_title_artist") || trigger == QLatin1String("get_album")
             || trigger == QLatin1String("get_genre") || trigger == QLatin1String("get_firstbeat"))
        s.perFileSeen = true;
    else if (trigger == QLatin1String("play"))
        s.play = triggerIsOn(value);
    else if (trigger == QLatin1String("volume"))
    {
        bool ok = false;
        const double v = value.toDouble(&ok);
        if (!ok)
            return;
        const bool visible = std::abs(v - s.deck.volume) >= 0.01;
        s.deck.volume = v;
        if (visible)
            emit deckChanged(deck);
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
    // every deck's view, then the master, then the write's notifications, the
    // master's first: whoever hears one sees the whole write settled
    std::array<Deck, MaxDecks> before;
    for (int d = 0; d < m_deckCount; ++d)
    {
        before[d] = m_slots[d].deck;
        settle(m_slots[d]);
    }
    recomputeActiveDeck();
    for (int d = 0; d < m_deckCount; ++d)
    {
        const Deck &was = before[d], &now = m_slots[d].deck;
        const bool songMoved = !sameSong(was.song, now.song);
        if (announced(now.song) && (!announced(was.song) || was.song.filepath != now.song.filepath))
            emit songChanged(d + 1, now.song);
        else if (announced(now.song) && songMoved)
            emit songUpdated(d + 1, now.song);
        if (songMoved || was.state != now.state || was.playing != now.playing)
            emit deckChanged(d + 1);
    }
}

void DjFsm::settle(Slot &s)
{
    Pending &p = s.pending;
    const bool loaded = s.deck.song.isValid();

    // held song fields are the file's whose filepath the write carries, or a
    // position sample of the loaded song; `loaded` on brings the ejected file
    // back unless the write carries a per-file field, which VDJ resends for
    // another file only; an empty deck's values are nobody's and mirror at once
    if (p.filepath || s.positionSeen || !s.loaded
        || (s.loadedOnSeen && !loaded && !s.perFileSeen))
    {
        if (p.filepath && *p.filepath != s.last.filepath)
            s.deck.positionStale = !s.positionSeen;
        if (p.filepath) s.last.filepath = *p.filepath;
        if (p.title)    s.last.title    = *p.title;
        if (p.artist)   s.last.artist   = *p.artist;
        if (p.key)      s.last.key      = *p.key;
        if (p.bpm)      s.last.bpm      = *p.bpm;
        if (p.totalMs)  s.last.totalMs  = *p.totalMs;
        p = Pending();
    }

    s.deck.song = (s.loaded && !s.last.filepath.isEmpty() && s.last.title != kPlaceholderTitle) ? s.last : DeckSong();
    const bool valid = s.deck.song.isValid();
    s.deck.playing = s.play && valid;
    if (!valid)
    {
        s.deck.state = DeckState::Empty;
        s.deck.beatPos = 0.0;
        s.deck.elapsedMs = 0;
        s.deck.remainingMs = 0;
        s.deck.positionStale = false;
        s.posEmitSecond = -1;
        s.posEmitBeatQ = -1000;
        s.wasPlaying = false;
    }
    else
    {
        s.wasPlaying = s.wasPlaying || s.deck.playing;
        s.deck.state = s.deck.song.title.isEmpty() ? DeckState::Loading
                     : s.deck.playing              ? DeckState::Playing
                     : s.wasPlaying                ? DeckState::Paused
                                                   : DeckState::Loaded;
    }
    s.positionSeen = s.loadedOnSeen = s.perFileSeen = false;
}

void DjFsm::recomputeActiveDeck()
{
    int active = 0;
    for (int d = 1; d <= m_deckCount && active == 0; ++d)
        if (m_slots[d - 1].master)
            active = d;
    if (active != m_activeDeck)
    {
        m_activeDeck = active;
        emit activeDeckChanged(active);
    }
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
        m_slots[i] = Slot();
        m_slots[i].deck.number = i + 1;
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
