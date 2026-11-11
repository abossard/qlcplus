/*
  Q Light Controller Plus
  edmanalysisqueue.cpp

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

#include "edmanalysisqueue.h"

EdmAnalysisQueue::EdmAnalysisQueue(const QString &outputDir, QObject *parent)
    : QObject(parent)
    , m_outputDir(outputDir)
    , m_process(new QProcess(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &EdmAnalysisQueue::slotReadOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() { m_stderr += m_process->readAllStandardError(); });
    connect(m_process, &QProcess::finished, this, &EdmAnalysisQueue::slotFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            slotFailedToStart();
    });
}

EdmAnalysisQueue::~EdmAnalysisQueue()
{
    if (m_process->state() != QProcess::NotRunning)
    {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

void EdmAnalysisQueue::setTool(const ToolCommand &command, const QProcessEnvironment &environment)
{
    m_command = command;
    m_environment = environment;
}

EdmQueueEntry EdmAnalysisQueue::entry(const QString &path) const
{
    return m_entries.value(path);
}

void EdmAnalysisQueue::enqueue(const QString &path)
{
    if (path.isEmpty() || m_entries.contains(path))
        return;

    m_entries[path].state = "queued";
    m_waiting.append(path);
    emit entryChanged(path);
    startNext();
}

void EdmAnalysisQueue::retry(const QString &path)
{
    if (m_entries.value(path).state != "failed")
        return;

    m_entries[path] = EdmQueueEntry{"queued", QString(), QString(), QString()};
    m_waiting.append(path);
    emit entryChanged(path);
    startNext();
}

void EdmAnalysisQueue::setBlocked(bool blocked)
{
    m_blocked = blocked;
    startNext();
}

void EdmAnalysisQueue::startNext()
{
    if (running() || m_blocked || m_waiting.isEmpty())
        return;

    m_current = m_waiting.takeFirst();
    m_progress = EdmProgress();
    m_stderr.clear();
    m_entries[m_current].state = "analysing";
    emit entryChanged(m_current);

    QStringList command = m_command ? m_command() : QStringList();
    if (command.isEmpty())
    {
        finishCurrent(tr("The analysis tool was not found."));
        return;
    }

    const QString program = command.takeFirst();
    m_process->setProcessEnvironment(m_environment);
    m_process->start(program, command + edmAnalyzeArguments(m_current, m_outputDir));
    emit runningChanged();
}

void EdmAnalysisQueue::slotReadOutput()
{
    while (m_process->canReadLine())
        m_progress = applyEdmLine(m_progress, m_process->readLine().trimmed());

    const QString stage = m_progress.tracks.isEmpty() ? QString() : m_progress.tracks.first().stage;
    if (m_current.isEmpty() == false && stage != m_entries[m_current].stage)
    {
        m_entries[m_current].stage = stage;
        emit entryChanged(m_current);
    }
}

void EdmAnalysisQueue::slotFinished(int exitCode, QProcess::ExitStatus status)
{
    slotReadOutput();
    const EdmTrackRow row = m_progress.tracks.isEmpty() ? EdmTrackRow() : m_progress.tracks.first();
    QString error;
    if (status == QProcess::CrashExit)
        error = tr("The analysis tool stopped.");
    else if (row.status == "error")
        error = row.error;
    else if (exitCode != 0 || row.status != "done")
        error = m_stderr.trimmed().split('\n').last();
    if (error.isEmpty() && (exitCode != 0 || row.status != "done"))
        error = tr("The analysis tool ended with exit code %1.").arg(exitCode);

    if (error.isEmpty())
        m_entries[m_current].id = row.id;
    finishCurrent(error);
}

void EdmAnalysisQueue::slotFailedToStart()
{
    finishCurrent(tr("The analysis tool could not be started."));
}

void EdmAnalysisQueue::finishCurrent(const QString &error)
{
    const QString path = m_current;
    m_current.clear();
    EdmQueueEntry &e = m_entries[path];
    e.stage.clear();
    e.error = error;
    e.state = error.isEmpty() ? "ready" : "failed";
    emit entryChanged(path);
    emit runningChanged();
    startNext();
}
