/*
  Q Light Controller Plus
  edmanalysismanager.h

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

#ifndef EDMANALYSISMANAGER_H
#define EDMANALYSISMANAGER_H

#include "previewcontext.h"
#include "edmanalysisprogress.h"

#include <QAbstractListModel>
#include <QProcess>
#include <QUrl>

/** Rows of the current analysis run */
class EdmTrackModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles { TitleRole = Qt::UserRole + 1, StatusRole, StageRole, SummaryRole, BpmRole, SecondsRole, ErrorRole };

    explicit EdmTrackModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** Replace the rows, signalling only what changed */
    void setRows(const QVector<EdmTrackRow> &rows);

private:
    QVector<EdmTrackRow> m_rows;
};

/**
 * The Audio Analysis page: runs the offline `edm-analysis` tool on a folder
 * of MP3 files and reports its progress live, then opens the rating site.
 */
class EdmAnalysisManager : public PreviewContext
{
    Q_OBJECT

    Q_PROPERTY(bool running READ running NOTIFY progressChanged)
    Q_PROPERTY(int total READ total NOTIFY progressChanged)
    Q_PROPERTY(int done READ done NOTIFY progressChanged)
    Q_PROPERTY(int failed READ failed NOTIFY progressChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY progressChanged)
    Q_PROPERTY(QString currentStage READ currentStage NOTIFY progressChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY folderChanged)
    Q_PROPERTY(QString outputDir READ outputDir CONSTANT)
    Q_PROPERTY(QString toolProblem READ toolProblem NOTIFY toolProblemChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(QAbstractListModel *tracks READ tracks CONSTANT)

public:
    explicit EdmAnalysisManager(QQuickView *view, Doc *doc, QObject *parent = nullptr);
    ~EdmAnalysisManager();

    bool running() const;
    int total() const;
    int done() const;
    int failed() const;
    QString currentTitle() const;
    QString currentStage() const;
    QString folder() const;
    QString outputDir() const;
    QString toolProblem() const;
    QString lastMessage() const;
    QAbstractListModel *tracks() const;

    Q_INVOKABLE void start(const QUrl &folder);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void openReview();
    Q_INVOKABLE void revealOutput();

signals:
    void progressChanged();
    void folderChanged();
    void toolProblemChanged();
    void lastMessageChanged();

private slots:
    void slotReadOutput();
    void slotReadErrors();
    void slotFinished(int exitCode, QProcess::ExitStatus status);

private:
    /** Program and leading arguments that run the tool, empty with toolProblem set when missing */
    QStringList toolCommand();
    QProcessEnvironment toolEnvironment() const;
    void setLastMessage(const QString &message);

    QProcess *m_process;
    QProcess *m_review;
    EdmTrackModel *m_model;
    EdmProgress m_progress;
    QString m_folder;
    QString m_outputDir;
    QString m_toolProblem;
    QString m_lastMessage;
};

#endif
