/*
  Q Light Controller Plus
  edmanalysisprogress.cpp

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

#include "edmanalysisprogress.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

EdmProgress applyEdmLine(const EdmProgress &progress, const QByteArray &line)
{
    const QJsonObject ev = QJsonDocument::fromJson(line).object();
    const QString kind = ev.value("event").toString();
    const int index = ev.value("index").toInt(-1);
    const bool perTrack = kind == "track_start" || kind == "stage" || kind == "track_done" || kind == "track_error";
    if (perTrack && (index < 0 || index >= progress.tracks.size()))
        return progress;

    EdmProgress p = progress;
    if (kind == "start")
    {
        p = EdmProgress();
        p.total = ev.value("total").toInt();
        p.outDir = ev.value("out").toString();
        p.tracks.resize(p.total);
    }
    else if (kind == "track_start")
    {
        EdmTrackRow &row = p.tracks[index];
        row.path = ev.value("path").toString();
        row.title = QFileInfo(row.path).completeBaseName();
        row.status = "running";
        p.current = index;
    }
    else if (kind == "stage")
    {
        p.tracks[index].stage = ev.value("stage").toString();
    }
    else if (kind == "track_done")
    {
        EdmTrackRow &row = p.tracks[index];
        row.status = "done";
        row.stage.clear();
        row.bpm = ev.value("bpm").toDouble();
        row.seconds = ev.value("seconds").toDouble();
        row.summary = ev.value("summary").toString();
        row.id = ev.value("id").toString();
        p.done++;
        p.current = -1;
    }
    else if (kind == "track_error")
    {
        EdmTrackRow &row = p.tracks[index];
        row.status = "error";
        row.stage.clear();
        row.error = ev.value("error").toString();
        p.failed++;
        p.current = -1;
    }
    else if (kind == "finished")
    {
        p.finished = true;
        p.current = -1;
    }
    return p;
}

QStringList edmAnalyzeArguments(const QString &folder, const QString &outDir)
{
    return {"analyze", folder, "--out", outDir, "--jsonl"};
}
