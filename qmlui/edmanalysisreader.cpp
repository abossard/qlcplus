/*
  Q Light Controller Plus
  edmanalysisreader.cpp

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

#include "edmanalysisreader.h"
#include "qlcconfig.h"
#include "qlcfile.h"
#include "audio.h"
#include "doc.h"
#include "show.h"
#include "showfunction.h"
#include "track.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

EdmAnalysisReader::EdmAnalysisReader(const QString &analysisDir)
    : m_dir(analysisDir)
{
}

QString EdmAnalysisReader::defaultDir()
{
    return QLCFile::userDirectory(QString(USERQLCPLUSDIR), QString(USERQLCPLUSDIR), QStringList())
        .absoluteFilePath("edm-analysis");
}

QString EdmAnalysisReader::firstAudioFile(Doc *doc, Show *show)
{
    if (doc == nullptr || show == nullptr)
        return QString();

    for (Track *track : show->tracks())
    {
        for (ShowFunction *sf : track->showFunctions())
        {
            Audio *audio = qobject_cast<Audio *>(doc->function(sf->functionID()));
            if (audio != nullptr)
                return audio->getSourceFileName();
        }
    }
    return QString();
}

/** Percent used when a section has no confidence value (older files) */
static const int kUnknownConfidence = 50;

static QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) == false)
        return QJsonObject();
    return QJsonDocument::fromJson(file.readAll()).object();
}

static QString pathKey(const QString &path)
{
    const QString canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? path : canonical;
}

static int percent(const QJsonObject &object, const char *key, int fallback)
{
    return object.contains(key) ? qRound(object.value(key).toDouble(fallback)) : fallback;
}

static double barAtMs(const EdmGrid &grid, double ms)
{
    return (ms - grid.firstDownbeatMs) / grid.barMs;
}

EdmAnalysis parseEdmAnalysis(const QByteArray &bytes)
{
    EdmAnalysis result;
    const QJsonObject root = QJsonDocument::fromJson(bytes).object();
    const QString schema = root.value("schema").toString();
    if (schema != "qlcplus-edm-final/2" && schema != "qlcplus-edm-final/3")
        return result;

    const QJsonObject grid = root.value("grid").toObject();
    const double period = grid.contains("period") ? grid.value("period").toDouble() : 60.0 / grid.value("bpm").toDouble();
    const QJsonArray sections = root.value("sections").toArray();
    if (period > 0 == false || qIsFinite(period) == false || sections.isEmpty())
        return result;

    result.grid.beatsPerBar = grid.value("beats_per_bar").toInt(4);
    result.grid.barMs = period * result.grid.beatsPerBar * 1000.0;
    result.grid.bpm = grid.contains("bpm") ? grid.value("bpm").toDouble() : 60.0 / period;
    result.grid.firstDownbeatMs = grid.value("first_downbeat").toDouble() * 1000.0;
    result.grid.confidence = percent(grid, "confidence", kUnknownConfidence);

    for (const QJsonValue &value : sections)
    {
        const QJsonObject s = value.toObject();
        EdmSection section;
        section.label = s.value("label").toString();
        section.startMs = s.value("start").toDouble() * 1000.0;
        section.endMs = s.value("end").toDouble() * 1000.0;
        section.startBar = s.contains("start_bar") ? s.value("start_bar").toDouble()
                         : result.sections.isEmpty() ? 0 : barAtMs(result.grid, section.startMs);
        section.endBar = s.contains("end_bar") ? s.value("end_bar").toDouble() : barAtMs(result.grid, section.endMs);
        section.confidence = percent(s, "confidence", kUnknownConfidence);
        section.labelConfidence = percent(s, "label_confidence", kUnknownConfidence);
        section.energy = s.value("energy").toDouble(-1);
        result.sections.append(section);
    }

    for (const QJsonValue &value : root.value("events").toArray())
    {
        const QJsonObject e = value.toObject();
        EdmEvent event;
        event.kind = e.value("kind").toString();
        if (event.kind.isEmpty())
            continue;
        event.timeMs = e.value("time").toDouble() * 1000.0;
        event.bar = e.contains("bar") ? e.value("bar").toDouble() : barAtMs(result.grid, event.timeMs);
        event.confidence = percent(e, "confidence", kEdmNoConfidence);
        result.events.append(event);
    }

    const QJsonObject track = root.value("track").toObject();
    result.valid = true;
    result.schema = schema.endsWith("/3") ? 3 : 2;
    result.id = track.value("sha256").toString().left(16);
    result.title = track.value("title").toString();
    return result;
}

void EdmAnalysisReader::refreshIndex()
{
    const QString indexPath = QDir(m_dir).filePath("index.json");
    const QDateTime modified = QFileInfo(indexPath).lastModified();
    if (modified == m_indexTime)
        return;

    m_indexTime = modified;
    m_idByPath.clear();
    const QJsonObject index = readJson(indexPath);
    for (auto it = index.constBegin(); it != index.constEnd(); ++it)
        m_idByPath.insert(pathKey(it.value().toObject().value("path").toString()), it.key());
}

EdmAnalysis EdmAnalysisReader::analysis(const QString &audioPath)
{
    if (audioPath.isEmpty())
        return EdmAnalysis();

    refreshIndex();
    return analysisById(m_idByPath.value(pathKey(audioPath)));
}

EdmAnalysis EdmAnalysisReader::analysisById(const QString &id)
{
    if (id.isEmpty())
        return EdmAnalysis();

    const QFileInfo info(QDir(m_dir).filePath("final/" + id + ".json"));
    const QDateTime modified = info.lastModified();
    Cached cached = m_cache.value(id);
    if (info.exists() == false)
    {
        m_cache.remove(id);
        return EdmAnalysis();
    }
    if (cached.modified != modified)
    {
        QFile file(info.filePath());
        cached.analysis = file.open(QIODevice::ReadOnly) ? parseEdmAnalysis(file.readAll()) : EdmAnalysis();
        cached.modified = modified;
        m_cache.insert(id, cached);
    }
    return cached.analysis;
}

QString EdmAnalysisReader::idByContent(const QString &audioPath) const
{
    QFile file(audioPath);
    if (file.open(QIODevice::ReadOnly) == false)
        return QString();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    const QString id = QString(hash.result().toHex()).left(16);
    return QFileInfo::exists(QDir(m_dir).filePath("final/" + id + ".json")) ? id : QString();
}

QVariantList EdmAnalysisReader::sections(const QString &audioPath)
{
    QVariantList list;
    for (const EdmSection &s : analysis(audioPath).sections)
    {
        QVariantMap map;
        map.insert("label", s.label);
        map.insert("startMs", s.startMs);
        map.insert("endMs", s.endMs);
        map.insert("confidence", s.confidence);
        map.insert("labelConfidence", s.labelConfidence);
        list.append(map);
    }
    return list;
}
