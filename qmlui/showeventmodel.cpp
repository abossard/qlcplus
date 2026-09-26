/*
  Q Light Controller Plus
  showeventmodel.cpp

  Copyright (c) QLC+ contributors

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

#include "showeventmodel.h"

#include "doc.h"
#include "showcommandrecorder.h"

ShowEventModel::ShowEventModel(Doc *doc, ShowCommandRecorder *recorder, QObject *parent)
    : QAbstractListModel(parent)
    , m_doc(doc)
    , m_recorder(recorder)
{
    ShowEventLog::setWakeReceiver(this);
    m_summary = ShowEventLog::summary();
    if (m_doc != nullptr)
    {
        // a new workspace: the panel closes and the summary starts over
        connect(m_doc, &Doc::clearing, this, [this]()
        {
            setOpen(false);
            ShowEventLog::resetSummary();
            refreshSummary();
        });
    }
}

ShowEventModel::~ShowEventModel()
{
    if (m_observing)
        ShowEventLog::close();
    ShowEventLog::setWakeReceiver(nullptr);
}

int ShowEventModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.count();
}

QString ShowEventModel::originName(int origin)
{
    switch (origin)
    {
        case int(ShowCommandOrigin::Pointer): return tr("Pointer");
        case int(ShowCommandOrigin::Midi): return tr("MIDI");
        case int(ShowCommandOrigin::Audio): return tr("Audio");
        case int(ShowCommandOrigin::Programmatic): return tr("Programmatic");
        case int(ShowCommandOrigin::Replay): return tr("Replay");
        case int(ShowCommandOrigin::Keyboard): return tr("Keyboard");
        case int(ShowCommandOrigin::Osc): return tr("OSC");
        default: break;
    }
    return QString();
}

QString ShowEventModel::phaseName(ShowEventLog::Phase phase)
{
    switch (phase)
    {
        case ShowEventLog::Phase::Input: return tr("Input");
        case ShowEventLog::Phase::Decision: return tr("Decision");
        case ShowEventLog::Phase::Commit: return tr("Commit");
        case ShowEventLog::Phase::Transport: return tr("Transport");
        case ShowEventLog::Phase::Execute: return tr("Execute");
    }
    return QString();
}

QString ShowEventModel::outcomeName(ShowEventLog::Outcome outcome)
{
    switch (outcome)
    {
        case ShowEventLog::Outcome::Received: return tr("Received");
        case ShowEventLog::Outcome::Recorded: return tr("Recorded");
        case ShowEventLog::Outcome::Ignored: return tr("Ignored");
        case ShowEventLog::Outcome::Unsupported: return tr("Unsupported");
        case ShowEventLog::Outcome::Rejected: return tr("Rejected");
        case ShowEventLog::Outcome::Published: return tr("Published");
        case ShowEventLog::Outcome::Failed: return tr("Failed");
        case ShowEventLog::Outcome::Queued: return tr("Queued");
        case ShowEventLog::Outcome::Requested: return tr("Requested");
        case ShowEventLog::Outcome::Applied: return tr("Applied");
        case ShowEventLog::Outcome::Skipped: return tr("Skipped");
        case ShowEventLog::Outcome::Cancelled: return tr("Cancelled");
    }
    return QString();
}

namespace
{
QString timeText(qint64 ms)
{
    return ms < 0 ? ShowEventModel::tr("unknown") : QString::number(double(ms) / 1000.0, 'f', 3);
}

bool isFailure(ShowEventLog::Outcome outcome)
{
    return outcome == ShowEventLog::Outcome::Rejected || outcome == ShowEventLog::Outcome::Failed ||
           outcome == ShowEventLog::Outcome::Unsupported || outcome == ShowEventLog::Outcome::Cancelled;
}
} // namespace

QVariant ShowEventModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.count())
        return QVariant();

    const ShowEventLog::Entry &e = m_rows.at(index.row());
    switch (role)
    {
        case SeqRole: return qreal(e.seq);
        case ArrivalRole: return timeText(e.arrivalMs - m_openedMs);
        case ShowTimeRole: return timeText(e.showTimeMs);
        case PhaseRole: return phaseName(e.phase);
        case OriginRole: return originName(e.origin);
        case ControlRole: return e.control;
        case ActionRole: return e.action;
        case ValueRole: return e.value;
        case OutcomeRole: return outcomeName(e.outcome);
        case ReasonRole: return e.reason;
        case FailedRole: return isFailure(e.outcome);
        default: break;
    }
    return QVariant();
}

QHash<int, QByteArray> ShowEventModel::roleNames() const
{
    return {
        { SeqRole, "seq" }, { ArrivalRole, "arrival" }, { ShowTimeRole, "showTime" },
        { PhaseRole, "phase" }, { OriginRole, "origin" }, { ControlRole, "control" },
        { ActionRole, "action" }, { ValueRole, "value" }, { OutcomeRole, "outcome" },
        { ReasonRole, "reason" }, { FailedRole, "failed" }
    };
}

void ShowEventModel::setOpen(bool open)
{
    if (open == m_open)
        return;
    m_open = open;
    emit openChanged();
    updateActive();
}

void ShowEventModel::setTab(int tab)
{
    if (tab != EventsTab && tab != ReferencesTab)
        return;
    if (tab == m_tab)
        return;
    m_tab = tab;
    emit tabChanged();
}

void ShowEventModel::setWindowVisible(bool visible)
{
    if (visible == m_windowVisible)
        return;
    m_windowVisible = visible;
    updateActive();
}

void ShowEventModel::showTab(int tab)
{
    setTab(tab);
    setOpen(true);
    emit focusRequested();
}

void ShowEventModel::showProblems()
{
    showTab(problemTab());
}

void ShowEventModel::updateActive()
{
    const bool active = isActive();
    if (active == m_observing)
    {
        emit activeChanged();
        return;
    }
    m_observing = active;

    // every observation starts empty and ends empty
    beginResetModel();
    m_rows.clear();
    m_lastSeq = 0;
    m_evicted = 0;
    m_dropped = 0;
    if (active)
    {
        m_generation = ShowEventLog::open();
        m_openedMs = ShowEventLog::nowMs();
    }
    else
    {
        ShowEventLog::close();
        m_generation = 0;
    }
    endResetModel();
    if (m_recorder != nullptr)
        m_recorder->setReferencesObserved(active);
    emit countsChanged();
    emit activeChanged();
}

void ShowEventModel::refreshSummary()
{
    const ShowEventLog::Summary summary = ShowEventLog::summary();
    if (summary.count != m_summary.count || summary.reason != m_summary.reason || summary.view != m_summary.view)
    {
        m_summary = summary;
        emit problemsChanged();
    }
}

void ShowEventModel::drainShowEvents()
{
    // only the queued call consumes the wake, so at most one is ever posted
    ShowEventLog::wakeHandled();
    refreshSummary();

    if (!m_observing)
        return;
    const ShowEventLog::Snapshot snapshot = ShowEventLog::snapshot(m_lastSeq);
    // a wake posted for an earlier observation reads nothing into this one
    if (snapshot.generation != m_generation)
        return;

    const bool counts = snapshot.evicted != m_evicted || snapshot.dropped != m_dropped || !snapshot.entries.isEmpty();
    m_evicted = snapshot.evicted;
    m_dropped = snapshot.dropped;
    if (!snapshot.entries.isEmpty())
    {
        const int incoming = snapshot.entries.count();
        const int excess = m_rows.count() + incoming - ShowEventLog::Capacity;
        if (excess > 0)
        {
            const int removed = qMin(excess, m_rows.count());
            beginRemoveRows(QModelIndex(), 0, removed - 1);
            m_rows.remove(0, removed);
            endRemoveRows();
        }
        const int keep = qMin(incoming, ShowEventLog::Capacity);
        beginInsertRows(QModelIndex(), m_rows.count(), m_rows.count() + keep - 1);
        m_rows += snapshot.entries.mid(incoming - keep);
        endInsertRows();
        m_lastSeq = snapshot.entries.last().seq;
    }
    if (counts)
        emit countsChanged();
}

int ShowEventModel::rowOfSeq(qreal seq) const
{
    const quint64 wanted = quint64(seq);
    // rows are in arrival order
    const auto it = std::lower_bound(m_rows.cbegin(), m_rows.cend(), wanted,
                                     [](const ShowEventLog::Entry &e, quint64 s) { return e.seq < s; });
    return it != m_rows.cend() && it->seq == wanted ? int(it - m_rows.cbegin()) : -1;
}

QVariantMap ShowEventModel::details(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_rows.count())
        return map;

    const ShowEventLog::Entry &e = m_rows.at(row);
    const auto id = [](quint32 value) { return value == ShowEventLog::NoId ? QVariant() : QVariant(qreal(value)); };
    const auto token = [](quint64 value) { return value == 0 ? QVariant() : QVariant(qreal(value)); };
    map["seq"] = qreal(e.seq);
    map["arrival"] = timeText(e.arrivalMs - m_openedMs);
    map["showTime"] = timeText(e.showTimeMs);
    map["phase"] = phaseName(e.phase);
    map["origin"] = originName(e.origin);
    map["outcome"] = outcomeName(e.outcome);
    map["control"] = e.control;
    map["action"] = e.action;
    map["value"] = e.value;
    map["before"] = e.before;
    map["after"] = e.after;
    map["reason"] = e.reason;
    map["showId"] = id(e.showId);
    map["widgetId"] = id(e.widgetId);
    map["functionId"] = id(e.functionId);
    map["commandId"] = id(e.commandId);
    map["controlId"] = e.controlId.isNull() ? QString() : e.controlId.toString(QUuid::WithoutBraces);
    map["take"] = token(e.take);
    map["traversal"] = token(e.traversal);
    map["trace"] = token(e.trace);
    return map;
}
