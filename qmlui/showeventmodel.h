/*
  Q Light Controller Plus
  showeventmodel.h

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

#ifndef SHOWEVENTMODEL_H
#define SHOWEVENTMODEL_H

#include <QAbstractListModel>
#include <QPointer>
#include <QVector>

#include "showeventlog.h"

class Doc;
class ShowCommandRecorder;

/**
 * The shared debug panel's state and its Events rows.
 *
 * The panel is open when the operator opened it and its window is shown.
 * Only then does ShowEventLog observe and the recorder follow the controls
 * the Show references; tab changes keep both. Closing, hiding the window or a
 * workspace reset ends the observation and drops every row. The rows are a
 * bounded presentation of the log's current observation, read on the single
 * wake the log posts. The failed-operation summary is read on the same wake.
 */
class ShowEventModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)
    Q_PROPERTY(int tab READ tab WRITE setTab NOTIFY tabChanged)
    Q_PROPERTY(bool windowVisible READ windowVisible WRITE setWindowVisible NOTIFY activeChanged)
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
    Q_PROPERTY(int count READ count NOTIFY countsChanged)
    Q_PROPERTY(qreal evicted READ evicted NOTIFY countsChanged)
    Q_PROPERTY(qreal dropped READ dropped NOTIFY countsChanged)
    Q_PROPERTY(qreal problemCount READ problemCount NOTIFY problemsChanged)
    Q_PROPERTY(QString problemReason READ problemReason NOTIFY problemsChanged)
    Q_PROPERTY(int problemTab READ problemTab NOTIFY problemsChanged)

public:
    enum Tab { EventsTab = 0, ReferencesTab = 1 };
    Q_ENUM(Tab)

    enum Roles
    {
        SeqRole = Qt::UserRole + 1,
        ArrivalRole,
        ShowTimeRole,
        PhaseRole,
        OriginRole,
        ControlRole,
        ActionRole,
        ValueRole,
        OutcomeRole,
        ReasonRole,
        FailedRole
    };

    ShowEventModel(Doc *doc, ShowCommandRecorder *recorder, QObject *parent = nullptr);
    ~ShowEventModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isOpen() const { return m_open; }
    void setOpen(bool open);
    int tab() const { return m_tab; }
    void setTab(int tab);
    bool windowVisible() const { return m_windowVisible; }
    void setWindowVisible(bool visible);
    /** Observing: open and its window shown */
    bool isActive() const { return m_open && m_windowVisible; }

    int count() const { return m_rows.count(); }
    qreal evicted() const { return qreal(m_evicted); }
    qreal dropped() const { return qreal(m_dropped); }
    qreal problemCount() const { return qreal(m_summary.count); }
    QString problemReason() const { return m_summary.reason; }
    int problemTab() const { return m_summary.view == ShowEventLog::View::References ? ReferencesTab : EventsTab; }

    /** Open, or focus when open, on that tab */
    Q_INVOKABLE void showTab(int tab);
    /** Open on the tab the latest failure concerns */
    Q_INVOKABLE void showProblems();
    /** The row of an event, -1 once it left the presentation */
    Q_INVOKABLE int rowOfSeq(qreal seq) const;
    /** Every field of the row's event, for the details view */
    Q_INVOKABLE QVariantMap details(int row) const;

    /** The log's wake */
    Q_INVOKABLE void drainShowEvents();

    const QVector<ShowEventLog::Entry> &rows() const { return m_rows; }

    static QString originName(int origin);
    static QString phaseName(ShowEventLog::Phase phase);
    static QString outcomeName(ShowEventLog::Outcome outcome);

signals:
    void openChanged();
    void tabChanged();
    void activeChanged();
    void countsChanged();
    void problemsChanged();
    /** A launcher asked for the panel, open already or not */
    void focusRequested();

private:
    void updateActive();
    /** Read the failure summary, without consuming the log's wake */
    void refreshSummary();

    Doc *m_doc;
    QPointer<ShowCommandRecorder> m_recorder;
    bool m_open = false;
    bool m_windowVisible = true;
    bool m_observing = false;
    int m_tab = EventsTab;
    quint64 m_generation = 0;
    qint64 m_openedMs = 0;
    quint64 m_lastSeq = 0;
    quint64 m_evicted = 0;
    quint64 m_dropped = 0;
    QVector<ShowEventLog::Entry> m_rows;
    ShowEventLog::Summary m_summary;
};

#endif // SHOWEVENTMODEL_H
