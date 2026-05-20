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
#include "vdjdeckmodel.h"

#include <QFileInfo>
#include <QQmlContext>

// ════════════════════════════════════════════════════════════════
//  SongListModel
// ════════════════════════════════════════════════════════════════

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
        { ShowIdRole,     "showId" },
        { AudioIdRole,    "audioId" },
        { TitleRole,      "title" },
        { ArtistRole,     "artist" },
        { BpmRole,        "bpm" },
        { KeyRole,        "key" },
        { DurationRole,   "duration" },
        { FilepathRole,   "filepath" },
        { IsPlayingRole,  "isPlaying" },
        { LastPlayedRole, "lastPlayed" },
        { LastEditedRole, "lastEdited" },
    };
}

QVariant SongListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &row = m_rows.at(index.row());

    if (role == FilepathRole)   return row.filepath;
    if (role == ShowIdRole)     return row.showId;
    if (role == IsPlayingRole)  return row.isPlaying;

    // Pull live metadata from Doc — Show/Audio may have been edited.
    Function *show = m_doc ? m_doc->function(row.showId) : nullptr;
    if (!show || show->type() != Function::ShowType)
        return {};

    // Timestamps are stored on the Function object and persisted to XML.
    if (role == LastPlayedRole) return show->lastPlayed();
    if (role == LastEditedRole) return show->lastEdited();

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
        return QString();
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

void SongListModel::rebuildFromDoc()
{
    if (!m_doc)
        return;

    // Preserve playing state from existing rows (not persisted on Function)
    QHash<quint32, bool> oldPlaying;
    for (const Row &r : std::as_const(m_rows))
        if (r.isPlaying)
            oldPlaying.insert(r.showId, true);

    beginResetModel();
    m_rows.clear();

    const auto shows = m_doc->functionsByType(Function::ShowType);
    for (Function *f : shows)
    {
        if (!f->path(true).startsWith(kSongFolderPath))
            continue;

        Row row;
        row.showId = f->id();
        row.isPlaying = oldPlaying.value(f->id(), false);

        // Recover filepath from the Audio child
        Show *show = static_cast<Show *>(f);
        const auto tracks = show->tracks();
        for (Track *t : tracks)
        {
            const auto sfs = t->showFunctions();
            for (ShowFunction *sf : sfs)
            {
                Function *child = m_doc->function(sf->functionID());
                if (child && child->type() == Function::AudioType)
                {
                    row.filepath = static_cast<Audio *>(child)->getSourceFileName();
                    break;
                }
            }
            if (!row.filepath.isEmpty())
                break;
        }

        m_rows.append(row);
    }
    endResetModel();
}

void SongListModel::addSong(const QString &filepath, quint32 showId)
{
    if (rowForShowId(showId) >= 0)
        return;

    const int pos = m_rows.size();
    beginInsertRows(QModelIndex(), pos, pos);
    Row row;
    row.filepath = filepath;
    row.showId = showId;
    m_rows.append(row);
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

void SongListModel::setPlaying(const QString &filepath, bool playing)
{
    int idx = rowForFilepath(filepath);
    if (idx < 0)
        return;

    Row &row = m_rows[idx];
    if (row.isPlaying == playing)
        return;

    row.isPlaying = playing;
    if (playing)
    {
        // Write lastPlayed to Function for persistence
        Function *f = m_doc ? m_doc->function(row.showId) : nullptr;
        if (f && !f->lastPlayed().isValid())
            f->setLastPlayed(QDateTime::currentDateTime());
    }

    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, { IsPlayingRole });
}

void SongListModel::markPlayed(const QString &filepath)
{
    int idx = rowForFilepath(filepath);
    if (idx < 0)
        return;

    QDateTime now = QDateTime::currentDateTime();
    Function *f = m_doc ? m_doc->function(m_rows[idx].showId) : nullptr;
    if (f)
        f->setLastPlayed(now);

    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, { LastPlayedRole });
}

void SongListModel::markEdited(quint32 showId)
{
    int idx = rowForShowId(showId);
    if (idx < 0)
        return;

    QDateTime now = QDateTime::currentDateTime();
    Function *f = m_doc ? m_doc->function(showId) : nullptr;
    if (f)
        f->setLastEdited(now);

    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, { LastEditedRole });
}

void SongListModel::clearPlayingState()
{
    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i].isPlaying)
        {
            m_rows[i].isPlaying = false;
            QModelIndex mi = index(i);
            emit dataChanged(mi, mi, { IsPlayingRole });
        }
    }
}

int SongListModel::rowForFilepath(const QString &fp) const
{
    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i].filepath == fp)
            return i;
    }
    return -1;
}

int SongListModel::rowForShowId(quint32 id) const
{
    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (m_rows[i].showId == id)
            return i;
    }
    return -1;
}

// ════════════════════════════════════════════════════════════════
//  SongSortFilterProxyModel
// ════════════════════════════════════════════════════════════════

SongSortFilterProxyModel::SongSortFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(SongListModel::TitleRole);
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
}

void SongSortFilterProxyModel::setSortMode(SortMode mode)
{
    if (m_sortMode == mode)
        return;
    m_sortMode = mode;
    invalidate();
}

void SongSortFilterProxyModel::setSortAscending(bool asc)
{
    if (m_ascending == asc)
        return;
    m_ascending = asc;
    sort(0, m_ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
}

void SongSortFilterProxyModel::setSearchFilter(const QString &text)
{
    QString lower = text.toLower();
    if (m_searchText == lower)
        return;
    m_searchText = lower;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    beginFilterChange();
    endFilterChange();
#else
    invalidateFilter();
#endif
}

bool SongSortFilterProxyModel::filterAcceptsRow(int sourceRow,
                                                 const QModelIndex &sourceParent) const
{
    if (m_searchText.isEmpty())
        return true;

    QAbstractItemModel *src = sourceModel();
    QModelIndex idx = src->index(sourceRow, 0, sourceParent);

    QString title = src->data(idx, SongListModel::TitleRole).toString().toLower();
    if (title.contains(m_searchText))
        return true;

    QString artist = src->data(idx, SongListModel::ArtistRole).toString().toLower();
    if (artist.contains(m_searchText))
        return true;

    QString filepath = src->data(idx, SongListModel::FilepathRole).toString().toLower();
    if (filepath.contains(m_searchText))
        return true;

    return false;
}

bool SongSortFilterProxyModel::lessThan(const QModelIndex &left,
                                         const QModelIndex &right) const
{
    QAbstractItemModel *src = sourceModel();

    switch (m_sortMode)
    {
    case Alphabetical:
    {
        QString l = src->data(left, SongListModel::TitleRole).toString().toLower();
        QString r = src->data(right, SongListModel::TitleRole).toString().toLower();
        return l < r;
    }
    case RecentlyPlayed:
    {
        QDateTime l = src->data(left, SongListModel::LastPlayedRole).toDateTime();
        QDateTime r = src->data(right, SongListModel::LastPlayedRole).toDateTime();
        // Null dates sort to the end (less recent)
        if (!l.isValid() && !r.isValid()) return false;
        if (!l.isValid()) return false;
        if (!r.isValid()) return true;
        // More recent = "less than" so it appears first in ascending order
        return l > r;
    }
    case RecentlyEdited:
    {
        QDateTime l = src->data(left, SongListModel::LastEditedRole).toDateTime();
        QDateTime r = src->data(right, SongListModel::LastEditedRole).toDateTime();
        if (!l.isValid() && !r.isValid()) return false;
        if (!l.isValid()) return false;
        if (!r.isValid()) return true;
        return l > r;
    }
    }
    return false;
}

// ════════════════════════════════════════════════════════════════
//  SongManager
// ════════════════════════════════════════════════════════════════

SongManager::SongManager(QQuickView *view, Doc *doc, VdjBridge *bridge,
                         ShowFactory *factory, QObject *parent)
    : PreviewContext(view, doc, "SONGMGR", parent)
    , m_bridge(bridge)
    , m_factory(factory)
    , m_model(new SongListModel(doc, this))
    , m_proxy(new SongSortFilterProxyModel(this))
{
    m_proxy->setSourceModel(m_model);

    setContextResource("qrc:/SongManager.qml");
    setContextTitle(tr("Song Manager"));

    view->rootContext()->setContextProperty("songManager", this);

    if (m_factory)
    {
        connect(m_factory, &ShowFactory::showCreatedForSong,
                this, &SongManager::onShowCreatedForSong);
    }

    if (m_doc)
    {
        connect(m_doc, &Doc::cleared, this, &SongManager::onDocCleared);
        connect(m_doc, &Doc::functionAdded, this, &SongManager::onFunctionAdded);
        connect(m_doc, &Doc::functionRemoved, this, &SongManager::onFunctionRemoved);
        connect(m_doc, &Doc::functionChanged, this, &SongManager::onFunctionChanged);

        // Initial population from workspace already loaded
        m_model->rebuildFromDoc();
    }

    connectDeckSignals();
}

QAbstractItemModel *SongManager::songListModel() const
{
    return m_proxy;
}

int SongManager::sortMode() const
{
    return static_cast<int>(m_proxy->sortMode());
}

void SongManager::setSortMode(int mode)
{
    auto m = static_cast<SongSortFilterProxyModel::SortMode>(mode);
    if (m_proxy->sortMode() == m)
        return;
    m_proxy->setSortMode(m);
    m_proxy->sort(0, m_proxy->sortAscending() ? Qt::AscendingOrder : Qt::DescendingOrder);
    emit sortModeChanged();
}

bool SongManager::sortAscending() const
{
    return m_proxy->sortAscending();
}

void SongManager::setSortAscending(bool asc)
{
    if (m_proxy->sortAscending() == asc)
        return;
    m_proxy->setSortAscending(asc);
    emit sortAscendingChanged();
}

QString SongManager::searchFilter() const
{
    return m_proxy->searchFilter();
}

void SongManager::setSearchFilter(const QString &text)
{
    QString prev = m_proxy->searchFilter();
    m_proxy->setSearchFilter(text);
    if (m_proxy->searchFilter() == prev)
        return;
    emit searchFilterChanged();
}

int SongManager::songCount() const
{
    return m_proxy->rowCount();
}

void SongManager::onShowCreatedForSong(const QString &filepath, quint32 showId)
{
    m_model->addSong(filepath, showId);
    emit songCountChanged();
}

void SongManager::onDocCleared()
{
    m_model->clear();
    emit songCountChanged();
}

void SongManager::onFunctionAdded(quint32 fid)
{
    Q_UNUSED(fid)
    m_model->rebuildFromDoc();
    emit songCountChanged();
}

void SongManager::onFunctionRemoved(quint32 fid)
{
    Q_UNUSED(fid)
    m_model->rebuildFromDoc();
    emit songCountChanged();
}

void SongManager::onFunctionChanged(quint32 fid)
{
    m_model->markEdited(fid);
}

void SongManager::refreshDeckPlayingState()
{
    if (!m_bridge)
        return;

    // Build a set of currently-playing filepaths
    QSet<QString> nowPlaying;
    QList<QObject *> deckList = m_bridge->decks();
    for (QObject *obj : deckList)
    {
        VdjDeckModel *deck = qobject_cast<VdjDeckModel *>(obj);
        if (!deck || !deck->playing())
            continue;

        const QString &fp = deck->filepath();
        if (!fp.isEmpty())
            nowPlaying.insert(fp);
    }

    // Early exit if nothing changed since last refresh
    if (nowPlaying == m_lastPlayingPaths)
        return;

    // Mark newly-playing songs
    for (const QString &fp : nowPlaying)
    {
        if (!m_lastPlayingPaths.contains(fp))
        {
            m_model->setPlaying(fp, true);
            m_model->markPlayed(fp);
        }
    }

    // Clear songs that stopped playing
    for (const QString &fp : std::as_const(m_lastPlayingPaths))
    {
        if (!nowPlaying.contains(fp))
            m_model->setPlaying(fp, false);
    }

    m_lastPlayingPaths = nowPlaying;
}

void SongManager::connectDeckSignals()
{
    if (!m_bridge)
        return;

    QList<QObject *> deckList = m_bridge->decks();
    for (QObject *obj : deckList)
    {
        VdjDeckModel *deck = qobject_cast<VdjDeckModel *>(obj);
        if (!deck)
            continue;

        connect(deck, &VdjDeckModel::transportChanged,
                this, &SongManager::refreshDeckPlayingState);
        connect(deck, &VdjDeckModel::metadataChanged,
                this, &SongManager::refreshDeckPlayingState);
    }
}
