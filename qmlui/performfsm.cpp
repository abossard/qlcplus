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
        case PerformState::Ended: return QStringLiteral("Ended");
    }
    return QStringLiteral("Unknown");
}

quint32 PerformFsm::activeShowId() const
{
    if (m_state == PerformState::Idle || m_state == PerformState::Armed)
        return InvalidShowId;
    return m_inputs.showId;
}

QList<PerformFsm::Step> PerformFsm::plan(PerformState prevState, quint32 prevShow,
                                         PerformState state, quint32 show, const ShowFacts &facts)
{
    using S = PerformState;
    using E = Effect;
    QList<Step> steps;
    const bool wasEngaged = prevState != S::Idle;
    const bool engaged = state != S::Idle;
    const bool showChanged = prevShow != show;

    // the previous Show goes first, while it is still on the external clock:
    // the recorder checkpoints its take there, then the lights go dark
    if (wasEngaged && prevShow != InvalidShowId && (!engaged || showChanged))
    {
        steps << Step{ engaged ? E::ResolveRecorder : E::ReleaseRecorder }
              << Step{ E::StopShow, prevShow } << Step{ E::ReleaseShow, prevShow };
    }
    else if (wasEngaged && !engaged)
    {
        steps << Step{ E::ReleaseRecorder };
    }
    if (!engaged)
        return steps;

    const bool adopting = show != InvalidShowId && (!wasEngaged || showChanged);
    if (adopting)
        steps << Step{ E::AdoptShow, show };
    if (show != InvalidShowId)
    {
        switch (state)
        {
            case S::Live:
                if (facts.running && facts.paused)
                    steps << Step{ E::ResumeShow, show };
                else if (!facts.running && !facts.startQueued)
                    steps << Step{ E::StartShow, show };
            break;
            case S::Suspended:
                if (facts.running && !facts.paused)
                    steps << Step{ E::PauseShow, show };
                else if (!facts.running && facts.startQueued)
                    steps << Step{ E::StopShow, show };
            break;
            case S::Ended:
                if (facts.running || facts.startQueued)
                    steps << Step{ E::StopShow, show };
            break;
            default:
            break;
        }
    }
    if (!wasEngaged || (showChanged && show != InvalidShowId))
        steps << Step{ E::ResolveRecorder, show };
    // the Show Manager re-reads the Show's transport on every transition
    if (show != InvalidShowId && (adopting || prevState != state))
        steps << Step{ E::SelectShow, show };
    return steps;
}

void PerformFsm::setPerformEnabled(bool on)
{
    if (m_enabled == on)
        return;
    const PerformState prevState = m_state;
    const quint32 prevShow = activeShowId();
    m_enabled = on;
    recomputeState(prevState, prevShow);
}

void PerformFsm::setInputs(const Inputs &inputs)
{
    if (m_inputs == inputs)
        return;
    const PerformState prevState = m_state;
    const quint32 prevShow = activeShowId();
    m_inputs = inputs;
    recomputeState(prevState, prevShow);
}

void PerformFsm::recomputeState(PerformState prevState, quint32 prevShow)
{
    if (m_enabled == false)
        m_state = PerformState::Idle;
    else if (m_inputs.showId == InvalidShowId)
        m_state = PerformState::Armed;
    else if (m_inputs.playing == false)
        m_state = PerformState::Suspended;
    else if (m_inputs.ended)
        m_state = PerformState::Ended;
    else
        m_state = PerformState::Live;

    if (m_state == prevState && activeShowId() == prevShow)
        return;

    qDebug() << "[PerformFsm]" << stateToString(prevState) << prevShow << "->"
             << stateToString(m_state) << activeShowId();
    emit stateChanged();
}
