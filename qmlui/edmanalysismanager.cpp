/*
  Q Light Controller Plus
  edmanalysismanager.cpp

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

#include "edmanalysismanager.h"
#include "qlcconfig.h"
#include "qlcfile.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QQmlContext>
#include <QQuickView>
#include <QSettings>
#include <QTimer>

#ifndef EDM_ANALYSIS_TOOL_DIR
#define EDM_ANALYSIS_TOOL_DIR ""
#endif
#define SETTINGS_TOOL_DIR "edmanalysis/toolDir"
#define SETTINGS_OUTPUT_DIR "edmanalysis/outputDir"
#define REVIEW_PORT 8765

/*********************************************************************
 * EdmTrackModel
 *********************************************************************/

EdmTrackModel::EdmTrackModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int EdmTrackModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant EdmTrackModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid() == false || index.row() >= m_rows.size())
        return QVariant();

    const EdmTrackRow &row = m_rows.at(index.row());
    switch (role)
    {
        case TitleRole: return row.title.isEmpty() ? tr("waiting…") : row.title;
        case StatusRole: return row.status;
        case StageRole: return row.stage;
        case SummaryRole: return row.summary;
        case BpmRole: return row.bpm;
        case SecondsRole: return row.seconds;
        case ErrorRole: return row.error;
    }
    return QVariant();
}

QHash<int, QByteArray> EdmTrackModel::roleNames() const
{
    return {{TitleRole, "title"}, {StatusRole, "status"}, {StageRole, "stage"}, {SummaryRole, "summary"},
            {BpmRole, "bpm"}, {SecondsRole, "seconds"}, {ErrorRole, "error"}};
}

void EdmTrackModel::setRows(const QVector<EdmTrackRow> &rows)
{
    if (rows.size() != m_rows.size())
    {
        beginResetModel();
        m_rows = rows;
        endResetModel();
        return;
    }
    for (int i = 0; i < rows.size(); i++)
    {
        const EdmTrackRow &a = m_rows.at(i), &b = rows.at(i);
        if (a.status != b.status || a.stage != b.stage || a.title != b.title)
        {
            m_rows[i] = b;
            emit dataChanged(index(i), index(i));
        }
    }
}

/*********************************************************************
 * EdmAnalysisManager
 *********************************************************************/

EdmAnalysisManager::EdmAnalysisManager(QQuickView *view, Doc *doc, QObject *parent)
    : PreviewContext(view, doc, "AUDIOANALYSIS", parent)
    , m_process(new QProcess(this))
    , m_review(new QProcess(this))
    , m_model(new EdmTrackModel(this))
{
    setContextResource("qrc:/AudioAnalysis.qml");
    setContextTitle(tr("Audio Analysis"));
    view->rootContext()->setContextProperty("audioAnalysis", this);

    // the same folder the tool uses by default, so a run from the shell shows up here too
    m_outputDir = QSettings().value(SETTINGS_OUTPUT_DIR, QLCFile::userDirectory(QString(USERQLCPLUSDIR), QString(USERQLCPLUSDIR), QStringList())
                      .absoluteFilePath("edm-analysis")).toString();

    connect(m_process, &QProcess::readyReadStandardOutput, this, &EdmAnalysisManager::slotReadOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &EdmAnalysisManager::slotReadErrors);
    connect(m_process, &QProcess::finished, this, &EdmAnalysisManager::slotFinished);
    toolCommand();
}

EdmAnalysisManager::~EdmAnalysisManager()
{
    for (QProcess *p : {m_process, m_review})
    {
        if (p->state() != QProcess::NotRunning)
        {
            p->kill();
            p->waitForFinished(2000);
        }
    }
}

bool EdmAnalysisManager::running() const { return m_process->state() != QProcess::NotRunning; }
int EdmAnalysisManager::total() const { return m_progress.total; }
int EdmAnalysisManager::done() const { return m_progress.done; }
int EdmAnalysisManager::failed() const { return m_progress.failed; }
QString EdmAnalysisManager::folder() const { return m_folder; }
QString EdmAnalysisManager::outputDir() const { return m_outputDir; }
QString EdmAnalysisManager::toolProblem() const { return m_toolProblem; }
QString EdmAnalysisManager::lastMessage() const { return m_lastMessage; }
QAbstractListModel *EdmAnalysisManager::tracks() const { return m_model; }

QString EdmAnalysisManager::currentTitle() const
{
    return m_progress.current >= 0 ? m_progress.tracks.at(m_progress.current).title : QString();
}

QString EdmAnalysisManager::currentStage() const
{
    return m_progress.current >= 0 ? m_progress.tracks.at(m_progress.current).stage : QString();
}

QStringList EdmAnalysisManager::toolCommand()
{
    const QString dir = QSettings().value(SETTINGS_TOOL_DIR, QString(EDM_ANALYSIS_TOOL_DIR)).toString();
    QString problem;
    QStringList cmd;

    const QString venvTool = QDir(dir).absoluteFilePath(".venv/bin/edm-analysis");
    if (QFileInfo(venvTool).isExecutable())
    {
        cmd << venvTool;
    }
    else
    {
        // a GUI app does not inherit the shell PATH, so look where uv installs itself
        const QStringList uvCandidates = {"/opt/homebrew/bin/uv", QDir::homePath() + "/.local/bin/uv", "/usr/local/bin/uv"};
        for (const QString &uv : uvCandidates)
        {
            if (QFileInfo(uv).isExecutable() && QFileInfo(QDir(dir).absoluteFilePath("pyproject.toml")).exists())
            {
                cmd << uv << "run" << "--project" << dir << "edm-analysis";
                break;
            }
        }
        if (cmd.isEmpty())
            problem = tr("The analysis tool was not found in %1. Run `uv sync` there, or set %2 in the settings.")
                          .arg(dir, SETTINGS_TOOL_DIR);
    }

    if (problem != m_toolProblem)
    {
        m_toolProblem = problem;
        emit toolProblemChanged();
    }
    return cmd;
}

QProcessEnvironment EdmAnalysisManager::toolEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", "/opt/homebrew/bin:/usr/local/bin:" + env.value("PATH"));  // ffmpeg
    env.insert("PYTHONUNBUFFERED", "1");
    return env;
}

void EdmAnalysisManager::setLastMessage(const QString &message)
{
    m_lastMessage = message;
    emit lastMessageChanged();
}

void EdmAnalysisManager::setQueueRunning(bool running)
{
    m_queueRunning = running;
}

void EdmAnalysisManager::start(const QUrl &folder)
{
    if (running())
        return;
    if (m_queueRunning)
    {
        setLastMessage(tr("Waiting for the song analysis to finish."));
        return;
    }

    QStringList cmd = toolCommand();
    if (cmd.isEmpty())
        return;

    m_folder = folder.isLocalFile() ? folder.toLocalFile() : folder.toString();
    emit folderChanged();

    m_progress = EdmProgress();
    m_model->setRows(m_progress.tracks);
    setLastMessage(tr("Starting…"));

    const QString program = cmd.takeFirst();
    m_process->setProcessEnvironment(toolEnvironment());
    m_process->start(program, cmd + edmAnalyzeArguments(m_folder, m_outputDir));
    emit progressChanged();
}

void EdmAnalysisManager::cancel()
{
    if (running())
    {
        m_process->terminate();
        if (m_process->waitForFinished(3000) == false)
            m_process->kill();
    }
}

void EdmAnalysisManager::openReview()
{
    QStringList cmd = toolCommand();
    if (cmd.isEmpty())
        return;

    const QUrl url(QString("http://127.0.0.1:%1/").arg(REVIEW_PORT));
    if (m_review->state() == QProcess::NotRunning)
    {
        const QString program = cmd.takeFirst();
        m_review->setProcessEnvironment(toolEnvironment());
        m_review->start(program, cmd << "review" << "--out" << m_outputDir << "--port" << QString::number(REVIEW_PORT));
        QTimer::singleShot(1500, this, [url]() { QDesktopServices::openUrl(url); });
        return;
    }
    QDesktopServices::openUrl(url);
}

void EdmAnalysisManager::revealOutput()
{
    QDir().mkpath(m_outputDir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_outputDir));
}

void EdmAnalysisManager::slotReadOutput()
{
    while (m_process->canReadLine())
        m_progress = applyEdmLine(m_progress, m_process->readLine().trimmed());

    m_model->setRows(m_progress.tracks);
    if (m_progress.current >= 0)
        setLastMessage(tr("Analyzing %1 of %2").arg(m_progress.current + 1).arg(m_progress.total));
    emit progressChanged();
}

void EdmAnalysisManager::slotReadErrors()
{
    // warnings and model download chatter; keep the last line as a hint
    const QStringList lines = QString::fromUtf8(m_process->readAllStandardError()).split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty() == false && lines.last().contains("Warning") == false)
        setLastMessage(lines.last().trimmed());
}

void EdmAnalysisManager::slotFinished(int exitCode, QProcess::ExitStatus status)
{
    slotReadOutput();
    if (status == QProcess::CrashExit)
        setLastMessage(tr("Stopped."));
    else if (m_progress.total == 0)
        setLastMessage(tr("No MP3 files found in this folder (exit code %1).").arg(exitCode));
    else
        setLastMessage(tr("Finished: %1 analyzed, %2 failed.").arg(m_progress.done).arg(m_progress.failed));
    emit progressChanged();
}
