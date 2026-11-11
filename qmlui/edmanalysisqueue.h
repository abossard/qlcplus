/*
  Q Light Controller Plus
  edmanalysisqueue.h

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

#ifndef EDMANALYSISQUEUE_H
#define EDMANALYSISQUEUE_H

#include "edmanalysisprogress.h"

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>

#include <functional>

/** Where one song stands in the queue */
struct EdmQueueEntry
{
    QString state;   // queued, analysing, ready, failed; empty for a path never queued
    QString stage;   // while analysing
    QString error;   // when failed
    QString id;      // when ready: final/<id>.json
};

/**
 * Analyses single songs with the offline tool, one process at a time, first
 * in first out. A path is queued once; a failed one runs again only by retry().
 */
class EdmAnalysisQueue : public QObject
{
    Q_OBJECT

public:
    /** Program and leading arguments that run the tool; empty when it is missing */
    using ToolCommand = std::function<QStringList()>;

    explicit EdmAnalysisQueue(const QString &outputDir, QObject *parent = nullptr);
    ~EdmAnalysisQueue();

    void setTool(const ToolCommand &command, const QProcessEnvironment &environment);

    void enqueue(const QString &path);
    void retry(const QString &path);
    EdmQueueEntry entry(const QString &path) const;

    /** While blocked nothing starts; a run under way finishes */
    void setBlocked(bool blocked);
    bool running() const { return m_process->state() != QProcess::NotRunning; }

signals:
    void entryChanged(const QString &path);
    void runningChanged();

private slots:
    void slotReadOutput();
    void slotFinished(int exitCode, QProcess::ExitStatus status);
    void slotFailedToStart();

private:
    void startNext();
    void finishCurrent(const QString &error);

    QString m_outputDir;
    ToolCommand m_command;
    QProcessEnvironment m_environment;
    QProcess *m_process;
    QHash<QString, EdmQueueEntry> m_entries;
    QStringList m_waiting;
    QString m_current;
    EdmProgress m_progress;
    QString m_stderr;
    bool m_blocked = false;
};

#endif
