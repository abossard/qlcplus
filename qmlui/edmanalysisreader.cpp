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

static QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) == false)
        return QJsonObject();
    return QJsonDocument::fromJson(file.readAll()).object();
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
        m_idByPath.insert(it.value().toObject().value("path").toString(), it.key());
}

QVariantList EdmAnalysisReader::sections(const QString &audioPath)
{
    QVariantList list;
    if (audioPath.isEmpty())
        return list;

    refreshIndex();
    const QString id = m_idByPath.value(audioPath);
    if (id.isEmpty())
        return list;

    const QJsonArray sections = readJson(QDir(m_dir).filePath("final/" + id + ".json")).value("sections").toArray();
    for (const QJsonValue &value : sections)
    {
        const QJsonObject s = value.toObject();
        QVariantMap map;
        map.insert("label", s.value("label").toString());
        map.insert("startMs", s.value("start").toDouble() * 1000.0);
        map.insert("endMs", s.value("end").toDouble() * 1000.0);
        map.insert("reviewed", s.value("reviewed").toBool());
        list.append(map);
    }
    return list;
}
