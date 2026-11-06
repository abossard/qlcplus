/*
  Q Light Controller Plus
  edmanalysisreader.h

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

#ifndef EDMANALYSISREADER_H
#define EDMANALYSISREADER_H

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QVariantList>

class Doc;
class Show;

/**
 * Read-only access to the offline analysis (`edm-analysis` tool): the
 * final/<id>.json files hold each song's sections with the reviewer's
 * corrections applied. Songs are found by audio file path via index.json.
 */
class EdmAnalysisReader
{
public:
    explicit EdmAnalysisReader(const QString &analysisDir = defaultDir());

    /** The folder the tool and the Audio Analysis page write to */
    static QString defaultDir();

    /** Path of the first Audio function on the show's timeline, or empty */
    static QString firstAudioFile(Doc *doc, Show *show);

    /** Sections as {label, startMs, endMs, reviewed}; empty when the song was not analysed */
    QVariantList sections(const QString &audioPath);

private:
    void refreshIndex();

    QString m_dir;
    QHash<QString, QString> m_idByPath;
    QDateTime m_indexTime;
};

#endif
