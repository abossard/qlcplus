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

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariantList>

class Doc;
class Show;

/** Confidence of an event the file gives none for (vocal_in, vocal_out) */
static const int kEdmNoConfidence = -1;

struct EdmGrid
{
    double bpm = 0;
    double barMs = 0;
    int beatsPerBar = 4;
    double firstDownbeatMs = 0;
    int confidence = 50;
};

struct EdmSection
{
    QString label;
    double startBar = 0;
    double endBar = 0;
    double startMs = 0;
    double endMs = 0;
    int confidence = 50;
    int labelConfidence = 50;
    double energy = -1; // -1 when the file has none
};

struct EdmEvent
{
    QString kind;
    double bar = 0;
    double timeMs = 0;
    int confidence = kEdmNoConfidence;
};

/** One song's analysis. Bar 0 starts at the first downbeat. */
struct EdmAnalysis
{
    bool valid = false;
    int schema = 0;
    QString id;
    QString title;
    EdmGrid grid;
    QList<EdmSection> sections;
    QList<EdmEvent> events;
};

/** Parse a final/<id>.json (schema qlcplus-edm-final/2 or /3); invalid for anything else */
EdmAnalysis parseEdmAnalysis(const QByteArray &bytes);

/**
 * Read-only access to the offline analysis (`edm-analysis` tool): the
 * final/<id>.json files hold each song's sections. Songs are found by
 * audio file path via index.json.
 */
class EdmAnalysisReader
{
public:
    explicit EdmAnalysisReader(const QString &analysisDir = defaultDir());

    /** The folder the tool and the Audio Analysis page write to */
    static QString defaultDir();

    /** Path of the first Audio function on the show's timeline, or empty */
    static QString firstAudioFile(Doc *doc, Show *show);

    /** Sections as {label, startMs, endMs, confidence, labelConfidence}, the
     *  last two in percent (50 when the file has no value); empty when the
     *  song was not analysed */
    QVariantList sections(const QString &audioPath);

    /** Cheap: the analysis of the song at this path (canonical paths compared,
     *  a missing file matches by its plain path); invalid when not found */
    EdmAnalysis analysis(const QString &audioPath);

    /** The analysis with this id (first 16 hex digits of the audio's sha256); invalid when missing */
    EdmAnalysis analysisById(const QString &id);

    /** Expensive, safe off the main thread: hashes the audio file and returns
     *  its id when final/<id>.json exists, else empty */
    QString idByContent(const QString &audioPath) const;

private:
    void refreshIndex();

    QString m_dir;
    QHash<QString, QString> m_idByPath;
    QDateTime m_indexTime;
    struct Cached
    {
        QDateTime modified;
        EdmAnalysis analysis;
    };
    QHash<QString, Cached> m_cache;
};

#endif
