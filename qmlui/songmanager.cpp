/*
  Q Light Controller Plus
  songmanager.cpp

  Copyright (c) Massimo Callegari

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

#include "songmanager.h"

#include "doc.h"
#include "audio.h"
#include "function.h"
#include "show.h"
#include "track.h"
#include "showfunction.h"
#include "showfactory.h"
#include "vdjbridge.h"

#include <QFileInfo>
#include <QQmlContext>

// ---------------- SongListModel ----------------

SongListModel::SongListModel(Doc *doc, QObject *parent)
    : QAbstractListModel(parent)
    , m_doc(doc)
{
}

int SongListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QHash<int, QByteArray> SongListModel::roleNames() const
{
    return {
        { ShowIdRole,   "showId" },
        { AudioIdRole,  "audioId" },
        { TitleRole,    "title" },
        { ArtistRole,   "artist" },
        { BpmRole,      "bpm" },
        { KeyRole,      "key" },
        { DurationRole, "duration" },
        { FilepathRole, "filepath" },
    };
}

QVariant SongListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &row = m_rows.at(index.row());

    if (role == FilepathRole)
        return row.filepath;
    if (role == ShowIdRole)
        return row.showId;

    // Pull live metadata from Doc each query — Show/Audio may be edited.
    Function *show = m_doc ? m_doc->function(row.showId) : nullptr;
    if (!show || show->type() != Function::ShowType)
        return {};

    // Locate the Audio function referenced by the show's first track.
    Audio *audio = nullptr;
    const auto tracks = static_cast<Show *>(show)->tracks();
    for (Track *t : tracks)
    {
        const auto sfs = t->showFunctions();
        for (ShowFunction *sf : sfs)
        {
            Function *f = m_doc->function(sf->functionID());
            if (f && f->type() == Function::AudioType)
            {
                audio = static_cast<Audio *>(f);
                break;
            }
        }
        if (audio)
            break;
    }

    switch (role)
    {
    case AudioIdRole:
        return audio ? audio->id() : Function::invalidId();
    case TitleRole:
        return show->name();
    case ArtistRole:
        return QString(); // Embedded in the show name for now
    case BpmRole:
        return 0.0;
    case KeyRole:
        return QString();
    case DurationRole:
        return audio ? static_cast<qint64>(audio->totalDuration()) : 0;
    default:
        return {};
    }
}

void SongListModel::addSong(const QString &filepath, quint32 showId)
{
    for (const Row &r : std::as_const(m_rows))
    {
        if (r.filepath == filepath)
            return;
    }

    const int pos = m_rows.size();
    beginInsertRows(QModelIndex(), pos, pos);
    m_rows.append({ filepath, showId });
    endInsertRows();
}

void SongListModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

// ---------------- SongManager ----------------

SongManager::SongManager(QQuickView *view, Doc *doc, VdjBridge *bridge,
                         ShowFactory *factory, QObject *parent)
    : PreviewContext(view, doc, "SONGMGR", parent)
    , m_bridge(bridge)
    , m_factory(factory)
    , m_model(new SongListModel(doc, this))
{
    setContextResource("qrc:/SongManager.qml");
    setContextTitle(tr("Song Manager"));

    view->rootContext()->setContextProperty("songManager", this);

    qDebug() << "[SongManager] Created. factory=" << m_factory << "doc=" << m_doc << "bridge=" << m_bridge;

    if (m_factory)
    {
        connect(m_factory, &ShowFactory::showCreatedForSong,
                this, &SongManager::onShowCreatedForSong);
        qDebug() << "[SongManager] Connected to ShowFactory::showCreatedForSong";
    }
    else
    {
        qWarning() << "[SongManager] WARNING: ShowFactory is NULL — songs won't appear!";
    }

    if (m_doc)
    {
        connect(m_doc, &Doc::cleared, this, &SongManager::onDocCleared);
    }
}

QAbstractListModel *SongManager::songListModel() const
{
    return m_model;
}

void SongManager::onShowCreatedForSong(const QString &filepath, quint32 showId)
{
    qDebug() << "[SongManager] onShowCreatedForSong:" << filepath << "showId:" << showId;
    m_model->addSong(filepath, showId);
}

void SongManager::onDocCleared()
{
    m_model->clear();
}
