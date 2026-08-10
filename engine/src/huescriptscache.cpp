/*
  Q Light Controller Plus
  huescriptscache.cpp

  Copyright (c) QLC+ contributors

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

#include <QTextStream>
#include <QDebug>
#include <QFile>
#include <QDir>

#include "huescriptscache.h"
#include "huescript.h"
#include "qlcconfig.h"
#include "qlcfile.h"

HUEScriptsCache::HUEScriptsCache(Doc *doc)
    : m_doc(doc)
{
}

QStringList HUEScriptsCache::names() const
{
    return m_scriptsMap.keys();
}

QStringList HUEScriptsCache::hsvNames() const
{
    return m_hsvNames;
}

HUEScript *HUEScriptsCache::script(QString name) const
{
    QString filename = m_scriptsMap.value(name);
    if (filename.isEmpty())
        return NULL;

    HUEScript *script = new HUEScript(m_doc);
    script->setHsvContract(m_hsvNames.contains(name));
    script->load(filename);
    return script;
}

QStringList HUEScriptsCache::s_hsvDirectories;

QStringList HUEScriptsCache::hsvScriptDirectories()
{
    QStringList dirs = s_hsvDirectories;
    if (dirs.contains(systemScriptsDirectory().absolutePath()) == false)
        dirs << systemScriptsDirectory().absolutePath();
    return dirs;
}

bool HUEScriptsCache::load(const QDir &dir, bool hsvContract)
{
    qDebug() << "Loading HUE scripts in" << dir.path() << "...";

    if (dir.exists() == false || dir.isReadable() == false)
        return false;

    if (hsvContract && s_hsvDirectories.contains(dir.absolutePath()) == false)
        s_hsvDirectories << dir.absolutePath();

    foreach (QString file, dir.entryList())
    {
        if (!file.endsWith(".js", Qt::CaseInsensitive))
            continue;

        QFile absFile(dir.absoluteFilePath(file));
        QString absFilename = absFile.fileName();

        if (!absFile.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QTextStream in(&absFile);
        QString line = in.readLine();
        while (!line.isNull())
        {
            QStringList tokens = line.split("=");
            if (tokens.length() == 2 && tokens[0].simplified() == "algo.name")
            {
                QString algoName = tokens[1].simplified().remove('"');
                algoName.remove(';');
                if (m_scriptsMap.value(algoName).isEmpty())
                {
                    m_scriptsMap.insert(algoName, absFilename);
                    if (hsvContract)
                        m_hsvNames.append(algoName);
                }
                break;
            }
            line = in.readLine();
        }
        absFile.close();
    }
    return true;
}

QDir HUEScriptsCache::systemScriptsDirectory()
{
    return QLCFile::systemDirectory(QString(HUESCRIPTDIR), QString(".js"));
}

QDir HUEScriptsCache::userScriptsDirectory()
{
    return QLCFile::userDirectory(QString(USERHUESCRIPTDIR), QString(HUESCRIPTDIR),
            QStringList() << QString("*%1").arg(".js"));
}
