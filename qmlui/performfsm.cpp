/*
  Q Light Controller Plus
  performfsm.cpp

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

#include "performfsm.h"

#include <QDebug>

#include <algorithm>

namespace {

using Drive = PerformFsm::Drive;

const Drive *driveOf(const PerformFsm::Snapshot &snapshot, quint32 showId)
{
    for (const Drive &drive : snapshot.drives)
        if (drive.showId == showId)
            return &drive;
    return nullptr;
}

/** The active deck's drive, if its song has a Show */
const Drive *dominantOf(const PerformFsm::Snapshot &snapshot)
{
    for (const Drive &drive : snapshot.drives)
        if (drive.dominant)
            return &drive;
    return nullptr;
}

/** A deck holding a Show */
PerformFsm::PerformState stateOf(const PerformFsm::DeckInputs &deck)
{
    if (deck.playing == false)
        return PerformFsm::PerformState::Suspended;
    return deck.ended ? PerformFsm::PerformState::Ended : PerformFsm::PerformState::Live;
}

QString describe(const PerformFsm::Snapshot &snapshot)
{
    if (snapshot.engaged == false)
        return QStringLiteral("Idle");
    QStringList drives;
    for (const Drive &drive : snapshot.drives)
        drives << QStringLiteral("%1@%2 %3%4").arg(drive.showId).arg(drive.deck)
                      .arg(PerformFsm::stateToString(drive.state), drive.dominant ? "*" : "");
    return drives.isEmpty() ? QStringLiteral("Armed") : drives.join(QStringLiteral(", "));
}

}

PerformFsm::PerformFsm(QObject *parent)
    : QObject(parent)
{
}

QString PerformFsm::stateToString(PerformState state)
{
    switch (state)
    {
        case PerformState::Idle: return QStringLiteral("Idle");
        case PerformState::Armed: return QStringLiteral("Armed");
        case PerformState::Live: return QStringLiteral("Live");
        case PerformState::Suspended: return QStringLiteral("Suspended");
        case PerformState::Ended: return QStringLiteral("Ended");
    }
    return QStringLiteral("Unknown");
}

PerformFsm::PerformState PerformFsm::state() const
{
    if (m_snapshot.engaged == false)
        return PerformState::Idle;
    const Drive *active = dominantOf(m_snapshot);
    return active ? active->state : PerformState::Armed;
}

quint32 PerformFsm::activeShowId() const
{
    const Drive *active = dominantOf(m_snapshot);
    return active ? active->showId : InvalidShowId;
}

QList<PerformFsm::Step> PerformFsm::plan(const Snapshot &prev, const Snapshot &next,
                                         const QHash<quint32, ShowFacts> &facts)
{
    using S = PerformState;
    using E = Effect;
    QList<Step> steps;
    const Drive *wasActive = dominantOf(prev);
    const Drive *active = dominantOf(next);
    const quint32 prevShow = wasActive ? wasActive->showId : InvalidShowId;
    const quint32 show = active ? active->showId : InvalidShowId;
    const bool showChanged = prevShow != show;

    // the recorder leaves the active deck's Show first, while it is still
    // dominant and on its clock: it checkpoints its take there
    if (prev.engaged && !next.engaged)
        steps << Step{ E::ReleaseRecorder };
    else if (prev.engaged && prevShow != InvalidShowId && showChanged)
        steps << Step{ E::ResolveRecorder };

    // a Show no deck drives any more goes dark
    for (const Drive &was : prev.drives)
        if (!driveOf(next, was.showId))
            steps << Step{ E::StopShow, was.showId } << Step{ E::ReleaseShow, was.showId };

    // dominance moves: the old active deck's Show lets go first, so two never
    // replay at once; a Show changing decks is promoted on its new deck's clock
    for (const Drive &drive : next.drives)
        if (const Drive *was = driveOf(prev, drive.showId); was && was->dominant && !drive.dominant)
            steps << Step{ E::DemoteShow, drive.showId };
    for (const Drive &drive : next.drives)
        if (const Drive *was = driveOf(prev, drive.showId); was && was->deck != drive.deck)
            steps << Step{ E::FollowDeck, drive.showId };
    for (const Drive &drive : next.drives)
        if (const Drive *was = driveOf(prev, drive.showId); was && !was->dominant && drive.dominant)
            steps << Step{ E::PromoteShow, drive.showId };

    // every driven Show takes its deck's clock and transport
    for (const Drive &drive : next.drives)
    {
        if (!driveOf(prev, drive.showId))
            steps << Step{ E::AdoptShow, drive.showId };

        const ShowFacts f = facts.value(drive.showId);
        switch (drive.state)
        {
            case S::Live:
                if (f.running && f.paused)
                    steps << Step{ E::ResumeShow, drive.showId };
                else if (!f.running && !f.startQueued)
                    steps << Step{ E::StartShow, drive.showId };
            break;
            case S::Suspended:
                if (f.running && !f.paused)
                    steps << Step{ E::PauseShow, drive.showId };
                else if (!f.running && f.startQueued)
                    steps << Step{ E::StopShow, drive.showId };
            break;
            case S::Ended:
                if (f.running || f.startQueued)
                    steps << Step{ E::StopShow, drive.showId };
            break;
            default:
            break;
        }
    }

    // the recorder binds the active deck's Show once it runs there
    if (next.engaged && (!prev.engaged || (showChanged && show != InvalidShowId)))
        steps << Step{ E::ResolveRecorder, show };
    // the Show Manager re-reads the active deck's Show on every transition of it
    if (active && (showChanged || wasActive->state != active->state))
        steps << Step{ E::SelectShow, show };
    return steps;
}

void PerformFsm::setPerformEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    recompute();
}

void PerformFsm::setInputs(const Inputs &inputs)
{
    if (m_inputs == inputs)
        return;
    m_inputs = inputs;
    recompute();
}

void PerformFsm::recompute()
{
    Snapshot next;
    next.engaged = m_enabled;
    if (m_enabled)
    {
        // the active deck first: a Show two decks hold runs from it, else from the lower deck
        const int active = m_inputs.activeDeck;
        QList<int> order;
        if (active >= 1 && active <= MaxDecks)
            order << active;
        for (int deck = 1; deck <= MaxDecks; deck++)
            if (deck != active)
                order << deck;
        for (const int deck : order)
        {
            const DeckInputs &inputs = m_inputs.decks[deck - 1];
            if (inputs.showId != InvalidShowId && !driveOf(next, inputs.showId))
                next.drives << Drive{ inputs.showId, deck, stateOf(inputs), deck == active };
        }
        std::sort(next.drives.begin(), next.drives.end(),
                  [](const Drive &a, const Drive &b) { return a.deck < b.deck; });
    }
    if (next == m_snapshot)
        return;

    qDebug().noquote() << "[PerformFsm]" << describe(m_snapshot) << "->" << describe(next);
    m_snapshot = next;
    emit stateChanged();
}
