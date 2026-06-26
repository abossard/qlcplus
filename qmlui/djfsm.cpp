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

    if (m_activeDeck > m_deckCount)
    {
        m_activeDeck = 0;
        emit activeDeckChanged(0);
        recomputeActiveSong();
    }

    emit deckCountChanged(m_deckCount);
}

// ────────────────────────────────────────────────────────────────
//  Event inputs
// ────────────────────────────────────────────────────────────────

void DjFsm::onDeckTrigger(int deck, const QString &trigger, const QVariant &value)
{
    if (!deckTracked(deck))
        return;

    Slot &s = slotForDeck(deck);
    DeckSong &song = s.deck.song;
    bool changed = false;
    const bool prevPlaying = s.deck.playing;

    if (trigger == QLatin1String("get_filepath"))
    {
        const QString path = value.toString();
        if (path.isEmpty())
        {
            if (song.isValid() || s.deck.state != DeckState::Empty)
            {
                resetSlot(s, deck);
                emit deckChanged(deck);
                if (deck == m_activeDeck)
                    recomputeActiveSong();
            }
            return;
        }
        if (path != song.filepath)
        {
            // A new song's filepath. Fields that have NOT been freshly set
            // since the last commit are stale leftovers from the previous song
            // (filepath-first order) and are cleared; freshly-set fields belong
            // to this song (metadata-first order) and are kept. This makes song
            // identification correct under any field arrival order.
            clearStaleMetadata(s);
            song.filepath = path;
            s.gotMask |= F_Filepath;
            s.freshMask |= F_Filepath;
            changed = true;
        }
    }
    else if (trigger == QLatin1String("get_title"))
    {
        const QString title = value.toString();
        if (!title.isEmpty() && !isPlaceholderTitle(title) && song.title != title)
        {
            song.title = title;
            s.gotMask |= F_Title;
            s.freshMask |= F_Title;
            changed = true;
        }
    }
    else if (trigger == QLatin1String("get_artist"))
    {
        const QString artist = value.toString();
        if (!artist.isEmpty() && song.artist != artist)
        {
            song.artist = artist;
            s.gotMask |= F_Artist;
            s.freshMask |= F_Artist;
            changed = true;
        }
    }
    else if (trigger == QLatin1String("get_bpm"))
    {
        bool ok = false;
        const double bpm = value.toDouble(&ok);
        if (ok && bpm > 0.0)
        {
            // Notify on a real (integer) tempo change — not on sub-integer
            // jitter (e.g. 127.999 vs 127.999168, both round to 128).
            const int prevRounded = qRound(song.bpm);
            song.bpm = bpm;
            s.gotMask |= F_Bpm;
            s.freshMask |= F_Bpm;
            if (qRound(bpm) != prevRounded)
                changed = true;
        }
    }
    else if (trigger == QLatin1String("get_key"))
    {
        if (!value.toString().isEmpty() && song.key != value.toString())
        {
            song.key = value.toString();
            changed = true;
        }
    }
    else if (trigger == QLatin1String("get_album"))
    {
        song.album = value.toString();
    }
    else if (trigger == QLatin1String("get_genre"))
    {
        song.genre = value.toString();
    }
    else if (trigger == QLatin1String("get_firstbeat"))
    {
        bool ok = false;
        const double fb = value.toDouble(&ok);
        if (ok)
            song.firstBeat = fb;
    }
    else if (trigger == QLatin1String("get_time total"))
    {
        bool ok = false;
        const double ms = value.toDouble(&ok);
        if (ok && ms > 0.0)
            song.totalMs = static_cast<int>(ms); // VDJ sends milliseconds (verified from live telemetry)
    }
    else if (trigger == QLatin1String("loaded"))
    {
        const bool loaded = triggerIsOn(value);
        if (s.deck.loaded != loaded) { s.deck.loaded = loaded; changed = true; }
    }
    else if (trigger == QLatin1String("play"))
    {
        // Store the raw play flag; the effective playing state is derived in
        // recomputeState() once the song is valid. This recovers a play=on that
        // VDJ sent BEFORE the track's filepath (unreliable field ordering),
        // which would otherwise be lost.
        s.rawPlaying = triggerIsOn(value);
    }
    else if (trigger == QLatin1String("volume"))
    {
        bool ok = false;
        const double v = value.toDouble(&ok);
        if (ok && std::abs(v - s.deck.volume) >= 0.01)
        {
            s.deck.volume = v;
            changed = true;
        }
        else if (ok)
        {
            s.deck.volume = v; // store fine value without a visible refresh
        }
    }
    else if (trigger == QLatin1String("level"))
    {
        bool ok = false;
        const double v = value.toDouble(&ok);
        if (ok)
            s.deck.level = v; // continuous metering, no visible refresh
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
        return;
    }
    else if (trigger == QLatin1String("get_time elapsed absolute"))
    {
        bool ok = false;
        const double ms = value.toDouble(&ok);
        if (ok && ms >= 0.0)
        {
            s.deck.elapsedMs = static_cast<int>(ms); // VDJ sends milliseconds (verified from live telemetry)
            maybeEmitPosition(s, deck);
        }
        return;
    }
    else if (trigger == QLatin1String("get_time"))
    {
        bool ok = false;
        const double ms = value.toDouble(&ok);
        if (ok && ms >= 0.0)
            s.deck.remainingMs = static_cast<int>(ms); // VDJ sends milliseconds remaining (verified from live telemetry)
        return;
    }
    else
    {
        return; // continuous transport fields don't affect deck state
    }

    recomputeState(s);
    maybeAnnounceSong(s);

    // A play state that only became effective after recomputeState (e.g. a
    // play=on that arrived before the filepath) must still notify consumers.
    if (s.deck.playing != prevPlaying)
        changed = true;

    if (changed)
    {
        emit deckChanged(deck);
        if (deck == m_activeDeck)
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
    for (int i = 0; i < MaxDecks; ++i)
    {
        resetSlot(m_slots[i], i + 1);
        emit deckChanged(i + 1);
    }

    if (m_activeDeck != 0)
    {
        m_activeDeck = 0;
        emit activeDeckChanged(0);
    }
    recomputeActiveSong();
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
    s.deck.state   = DeckState::Empty;
    s.deck.song    = DeckSong{};
    s.deck.loaded  = false;
    s.deck.playing = false;
    s.deck.volume  = 0.0;
    s.deck.level   = 0.0;
    s.deck.beatPos = 0.0;
    s.deck.elapsedMs = 0;
    s.deck.remainingMs = 0;
    s.deck.number  = deck1;
    s.gotMask      = 0;
    s.freshMask    = 0;
    s.committedFilepath.clear();
    s.wasPlaying   = false;
    s.rawPlaying   = false;
    s.posEmitSecond = -1;
    s.posEmitBeatQ  = -1000;
}

void DjFsm::clearStaleMetadata(Slot &s)
{
    DeckSong &song = s.deck.song;

    // Required fields: clear only those NOT freshly set since the last commit.
    if (!(s.freshMask & F_Title))  { song.title.clear();  s.gotMask &= ~F_Title; }
    if (!(s.freshMask & F_Artist)) { song.artist.clear(); s.gotMask &= ~F_Artist; }
    if (!(s.freshMask & F_Bpm))    { song.bpm = 0.0;      s.gotMask &= ~F_Bpm; }

    // Optional display-only fields carry no freshness bit; clear them so a new
    // song never shows the previous song's key/album/etc. They re-arrive within
    // the same burst if VirtualDJ sends them.
    song.album.clear();
    song.genre.clear();
    song.key.clear();
    song.firstBeat = 0.0;
    song.totalMs   = 0;
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

    if ((s.gotMask & RequiredMeta) != RequiredMeta)
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

void DjFsm::maybeAnnounceSong(Slot &s)
{
    if ((s.gotMask & RequiredMeta) != RequiredMeta)
        return;
    if (s.deck.song.filepath == s.committedFilepath)
        return;

    s.committedFilepath = s.deck.song.filepath;
    s.freshMask = 0;
    emit songChanged(s.deck.number, s.deck.song);
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
    const QString fp = activeSong().filepath;
    if (fp == m_activeFilepath)
        return;
    m_activeFilepath = fp;
    emit activeSongChanged();
}
