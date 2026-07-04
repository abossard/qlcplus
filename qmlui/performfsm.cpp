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
    }
    return QStringLiteral("Unknown");
}

void PerformFsm::setPerformEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    recomputeState();
}

void PerformFsm::setActiveShow(quint32 showId)
{
    if (m_activeShowId == showId)
        return;
    m_activeShowId = showId;
    // state first, so activeShowChanged consumers see a consistent state
    recomputeState();
    emit activeShowChanged(m_activeShowId);
}

void PerformFsm::setDeckPlaying(bool playing)
{
    if (m_deckPlaying == playing)
        return;
    m_deckPlaying = playing;
    recomputeState();
}

void PerformFsm::reset()
{
    // VDJ went away: the show and transport are gone, the user's Perform
    // toggle stays as-is (re-connect re-arms automatically)
    const quint32 oldShow = m_activeShowId;
    m_deckPlaying = false;
    m_activeShowId = InvalidShowId;
    recomputeState();
    if (oldShow != InvalidShowId)
        emit activeShowChanged(m_activeShowId);
}

void PerformFsm::recomputeState()
{
    PerformState next;
    if (m_enabled == false)
        next = PerformState::Idle;
    else if (m_activeShowId == InvalidShowId)
        next = PerformState::Armed;
    else if (m_deckPlaying)
        next = PerformState::Live;
    else
        next = PerformState::Suspended;

    if (next == m_state)
        return;

    qDebug() << "[PerformFsm]" << stateToString(m_state) << "->" << stateToString(next)
             << "(show" << m_activeShowId << ")";
    m_state = next;
    emit stateChanged(m_state);
}
