/*
  Q Light Controller Plus
  edmanalysisprogress.h

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

#ifndef EDMANALYSISPROGRESS_H
#define EDMANALYSISPROGRESS_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

/** One row of the offline analysis run, as reported by `edm-analysis analyze --jsonl` */
struct EdmTrackRow
{
    QString path;
    QString title;
    QString status = "queued";   // queued, running, done, error
    QString stage;
    QString summary;             // section labels, space separated
    QString error;
    QString id;
    double bpm = 0;
    double seconds = 0;
};

struct EdmProgress
{
    int total = 0;
    int done = 0;
    int failed = 0;
    int current = -1;
    bool finished = false;
    QString outDir;
    QVector<EdmTrackRow> tracks;
};

/** Fold one JSON line of tool output into the progress. Lines that are not
 *  progress events (model download chatter, broken JSON) leave it unchanged. */
EdmProgress applyEdmLine(const EdmProgress &progress, const QByteArray &line);

QStringList edmAnalyzeArguments(const QString &folder, const QString &outDir);

#endif
