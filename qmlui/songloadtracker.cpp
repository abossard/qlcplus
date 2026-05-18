/*
  Q Light Controller Plus
  songloadtracker.cpp

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

#include "songloadtracker.h"

SongLoadTracker::SongLoadTracker(QObject *parent)
    : QObject(parent)
{
}

void SongLoadTracker::onTrigger(int deck, const QString &trigger, const QVariant &value)
{
    DeckSlot &s = slot(deck);

    // --- get_filepath: the anchor event ---
    if (trigger == QLatin1String("get_filepath"))
    {
        QString path = value.toString();

        if (path.isEmpty())
        {
            resetSlot(s);
            return;
        }

        // New filepath (different from current) — reset and start fresh
        if (path != s.info.filepath)
        {
            resetSlot(s);
            s.info.filepath = path;
            s.info.deck = deck;
            s.gotMask |= F_Filepath;
        }
        // Transition to Collecting if not already there/Ready
        if (s.state == DeckState::Empty)
            s.state = DeckState::Collecting;
        tryEmit(deck, s);
        return;
    }

    // --- Accept ALL metadata unconditionally regardless of state ---
    // Fields accumulate even in Empty state (before filepath arrives).
    // tryEmit only fires when all REQUIRED bits are set.

    if (trigger == QLatin1String("get_title"))
    {
        QString title = value.toString();
        if (!title.isEmpty() && !isPlaceholderTitle(title))
        {
            s.info.title = title;
            s.gotMask |= F_Title;
            if (s.state == DeckState::Collecting)
                tryEmit(deck, s);
        }
    }
    else if (trigger == QLatin1String("get_artist"))
    {
        QString artist = value.toString();
        if (!artist.isEmpty())
        {
            s.info.artist = artist;
            s.gotMask |= F_Artist;
            if (s.state == DeckState::Collecting)
                tryEmit(deck, s);
        }
    }
    else if (trigger == QLatin1String("get_bpm"))
    {
        bool ok = false;
        double bpm = value.toDouble(&ok);
        if (ok && bpm > 0.0)
        {
            s.info.bpm = bpm;
            s.gotMask |= F_Bpm;
            if (s.state == DeckState::Collecting)
                tryEmit(deck, s);
        }
    }
    else if (trigger == QLatin1String("loaded"))
    {
        bool loaded = (value.toString() == QLatin1String("on"));
        s.loaded = loaded;
        if (loaded)
        {
            s.gotMask |= F_Loaded;
            // If we were in Empty with accumulated metadata and filepath
            // arrived earlier, we may now be complete
            if (s.state == DeckState::Empty && (s.gotMask & F_Filepath))
                s.state = DeckState::Collecting;
            if (s.state == DeckState::Collecting)
                tryEmit(deck, s);
        }
        else
        {
            s.gotMask &= ~F_Loaded;
        }
    }
    // --- Optional fields (no mask bit, don't affect emission) ---
    else if (trigger == QLatin1String("get_key"))
    {
        QString key = value.toString();
        if (!key.isEmpty())
            s.info.key = key;
    }
    else if (trigger == QLatin1String("get_album"))
        s.info.album = value.toString();
    else if (trigger == QLatin1String("get_genre"))
        s.info.genre = value.toString();
    else if (trigger == QLatin1String("get_firstbeat"))
    {
        bool ok = false;
        double fb = value.toDouble(&ok);
        if (ok)
            s.info.firstBeat = fb;
    }
    else if (trigger == QLatin1String("get_time total"))
    {
        bool ok = false;
        double t = value.toDouble(&ok);
        if (ok && t > 0.0)
            s.info.totalMs = static_cast<int>(t * 1000.0);
    }
}

void SongLoadTracker::onMasterDeck(int deck)
{
    if (deck >= 1 && deck <= 4)
        m_masterDeck = deck;
}

void SongLoadTracker::onDisconnected()
{
    m_slots.clear();
    m_emittedPaths.clear();
}

bool SongLoadTracker::hasEmitted(const QString &filepath) const
{
    return m_emittedPaths.contains(filepath);
}

// --- Private ---

SongLoadTracker::DeckSlot &SongLoadTracker::slot(int deck)
{
    return m_slots[deck];
}

void SongLoadTracker::resetSlot(DeckSlot &s)
{
    s.state = DeckState::Empty;
    s.info = SongInfo{};
    s.loaded = false;
    s.gotMask = 0;
}

bool SongLoadTracker::isPlaceholderTitle(const QString &title) const
{
    // VDJ shows this on empty decks (English locale; other locales may differ)
    return title == QLatin1String("Drag a song on this deck to load it");
}

void SongLoadTracker::tryEmit(int deck, DeckSlot &s)
{
    if ((s.gotMask & REQUIRED) != REQUIRED)
        return;

    // Linked-deck dedup: if already emitted for this filepath, just mark Ready
    if (m_emittedPaths.contains(s.info.filepath))
    {
        s.state = DeckState::Ready;
        return;
    }

    s.info.deck = deck;
    m_emittedPaths.insert(s.info.filepath);
    s.state = DeckState::Ready;
    emit songReady(s.info);
}
