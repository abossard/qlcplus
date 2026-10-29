/*
  Q Light Controller Plus
  djmanager.cpp

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

#include "djmanager.h"

#include "doc.h"
#include "audio.h"
#include "function.h"
#include "show.h"
#include "track.h"
#include "showfunction.h"
#include "showfactory.h"
#include "vdjbridge.h"
#include "vdjdatabasereader.h"

#include <QFileInfo>
#include <QQmlContext>
#include <QSettings>


// ════════════════════════════════════════════════════════════════
//  DjSongModel
// ════════════════════════════════════════════════════════════════

DjSongModel::DjSongModel(Doc *doc, ShowFactory *index, QObject *parent)
    : QAbstractListModel(parent)
    , m_doc(doc)
    , m_index(index)
{
}

quint32 DjSongModel::showFor(const QString &filepath) const
{
    return m_index ? m_index->showIdForFilepath(filepath) : Function::invalidId();
}

int DjSongModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QHash<int, QByteArray> DjSongModel::roleNames() const
{
    return {
        { FilepathRole,   "filepath" },
        { TitleRole,      "title" },
        { ArtistRole,     "artist" },
        { BpmRole,        "bpm" },
        { DurationRole,   "duration" },
        { ShowIdRole,     "showId" },
        { HasShowRole,    "hasShow" },
        { IsPlayingRole,  "isPlaying" },
        { IsActiveRole,   "isActive" },
        { LastPlayedRole, "lastPlayed" },
        { LastEditedRole, "lastEdited" },
    };
}

QVariant DjSongModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &row = m_rows.at(index.row());
    const quint32 showId = showFor(row.filepath);
    Function *show = m_doc && showId != Function::invalidId() ? m_doc->function(showId) : nullptr;

    switch (role)
    {
    case FilepathRole:  return row.filepath;
    case TitleRole:     return row.title;
    case ArtistRole:    return row.artist;
    case BpmRole:       return row.bpm;
    case ShowIdRole:    return showId;
    case HasShowRole:   return showId != Function::invalidId();
    case IsPlayingRole: return row.isPlaying;
    case IsActiveRole:  return row.isActive;
    case DurationRole:
    {
        if (row.durationMs > 0)
            return static_cast<qint64>(row.durationMs);
        // Fall back to the show's audio duration.
        Audio *audio = ShowFactory::songAudio(m_doc, qobject_cast<Show *>(show));
        return static_cast<qint64>(audio ? audio->totalDuration() : 0);
    }
    case LastPlayedRole: return show ? show->lastPlayed() : QDateTime();
    case LastEditedRole: return show ? show->lastEdited() : QDateTime();
    default:
        return {};
    }
}

void DjSongModel::emitRow(int idx, const QList<int> &roles)
{
    if (idx < 0 || idx >= m_rows.size())
        return;
    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, roles.toVector());
}

void DjSongModel::rebuildFromDoc()
{
    if (!m_doc)
        return;

    // Preserve transient flags (playing/active) across the rebuild.
    QHash<QString, bool> oldPlaying;
    for (const Row &r : std::as_const(m_rows))
        if (r.isPlaying)
            oldPlaying.insert(r.filepath, true);

    beginResetModel();
    m_rows.clear();

    const auto shows = m_doc->functionsByType(Function::ShowType);
    for (Function *f : shows)
    {
        if (!f->path(true).startsWith(kSongFolderPath))
            continue;

        // Skip Shows with no recoverable song filepath (not a DJ song row).
        Audio *audio = ShowFactory::songAudio(m_doc, static_cast<Show *>(f));
        if (!audio || rowForFilepath(audio->getSourceFileName()) >= 0)
            continue;

        Row row;
        row.filepath = ShowFactory::normalizePath(audio->getSourceFileName());
        row.title = f->name();
        row.durationMs = static_cast<int>(audio->totalDuration());
        row.isPlaying = oldPlaying.value(row.filepath, false);
        row.isActive = (!m_activeFilepath.isEmpty() && row.filepath == m_activeFilepath);
        m_rows.append(row);
    }
    endResetModel();
}

bool DjSongModel::upsertSong(const DjFsm::DeckSong &song)
{
    if (song.filepath.isEmpty())
        return false;

    int idx = rowForFilepath(song.filepath);
    if (idx >= 0)
    {
        // Refresh cached metadata as the FSM reports it, an empty artist included
        Row &row = m_rows[idx];
        bool changed = false;
        if (!song.title.isEmpty() && row.title != song.title)   { row.title = song.title; changed = true; }
        if (row.artist != song.artist) { row.artist = song.artist; changed = true; }
        if (song.bpm > 0.0 && !qFuzzyCompare(row.bpm, song.bpm)) { row.bpm = song.bpm; changed = true; }
        if (song.totalMs > 0 && row.durationMs != song.totalMs) { row.durationMs = song.totalMs; changed = true; }
        if (changed)
            emitRow(idx, { TitleRole, ArtistRole, BpmRole, DurationRole });
        return false;
    }

    const int pos = m_rows.size();
    beginInsertRows(QModelIndex(), pos, pos);
    Row row;
    row.filepath = ShowFactory::normalizePath(song.filepath);
    row.title = song.title.isEmpty() ? QFileInfo(song.filepath).completeBaseName() : song.title;
    row.artist = song.artist;
    row.bpm = song.bpm;
    row.durationMs = song.totalMs;
    row.isActive = (!m_activeFilepath.isEmpty() && song.filepath == m_activeFilepath);
    m_rows.append(row);
    endInsertRows();
    return true;
}

bool DjSongModel::hasFilepath(const QString &filepath) const
{
    return rowForFilepath(filepath) >= 0;
}

void DjSongModel::refreshShow(const QString &filepath)
{
    const QList<int> roles{ ShowIdRole, HasShowRole, DurationRole, LastPlayedRole, LastEditedRole };
    if (!filepath.isEmpty())
        emitRow(rowForFilepath(filepath), roles);
    else if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(m_rows.size() - 1), roles.toVector());
}

DjFsm::DeckSong DjSongModel::songForFilepath(const QString &filepath) const
{
    DjFsm::DeckSong song;
    int idx = rowForFilepath(filepath);
    if (idx < 0)
        return song;
    const Row &row = m_rows[idx];
    song.filepath = row.filepath;
    song.title = row.title;
    song.artist = row.artist;
    song.bpm = row.bpm;
    song.totalMs = row.durationMs;
    return song;
}

void DjSongModel::setPlayingFilepaths(const QSet<QString> &playing)
{
    for (int i = 0; i < m_rows.size(); ++i)
    {
        bool nowPlaying = playing.contains(m_rows[i].filepath);
        if (m_rows[i].isPlaying != nowPlaying)
        {
            m_rows[i].isPlaying = nowPlaying;
            emitRow(i, { IsPlayingRole });
        }
    }
}

void DjSongModel::setActiveFilepath(const QString &filepath)
{
    if (m_activeFilepath == filepath)
        return;
    m_activeFilepath = filepath;
    const QString key = ShowFactory::normalizePath(filepath);
    for (int i = 0; i < m_rows.size(); ++i)
    {
        bool active = (!key.isEmpty() && m_rows[i].filepath == key);
        if (m_rows[i].isActive != active)
        {
            m_rows[i].isActive = active;
            emitRow(i, { IsActiveRole });
        }
    }
}

void DjSongModel::markPlayed(const QString &filepath)
{
    int idx = rowForFilepath(filepath);
    if (idx < 0)
        return;
    Function *f = m_doc ? m_doc->function(showFor(filepath)) : nullptr;
    if (f)
    {
        // saved with the workspace, so the workspace is modified
        f->setLastPlayed(QDateTime::currentDateTime());
        m_doc->setModified();
    }
    emitRow(idx, { LastPlayedRole });
}

void DjSongModel::markEdited(quint32 showId)
{
    Function *f = m_doc && showId != Function::invalidId() ? m_doc->function(showId) : nullptr;
    if (!f)
        return;
    // the change that reports here already modified the workspace
    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (showFor(m_rows[i].filepath) != showId)
            continue;
        f->setLastEdited(QDateTime::currentDateTime());
        emitRow(i, { LastEditedRole });
    }
}

void DjSongModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

int DjSongModel::rowForFilepath(const QString &fp) const
{
    const QString key = ShowFactory::normalizePath(fp);
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows[i].filepath == key)
            return i;
    return -1;
}

// ════════════════════════════════════════════════════════════════
//  DjSongFilterModel
// ════════════════════════════════════════════════════════════════

DjSongFilterModel::DjSongFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(DjSongModel::TitleRole);
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
}

void DjSongFilterModel::setSortMode(SortMode mode)
{
    if (m_sortMode == mode)
        return;
    m_sortMode = mode;
    invalidate();
}

void DjSongFilterModel::setSortAscending(bool asc)
{
    if (m_ascending == asc)
        return;
    m_ascending = asc;
    sort(0, m_ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
}

void DjSongFilterModel::setSearchFilter(const QString &text)
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

bool DjSongFilterModel::filterAcceptsRow(int sourceRow,
                                         const QModelIndex &sourceParent) const
{
    if (m_searchText.isEmpty())
        return true;

    QAbstractItemModel *src = sourceModel();
    QModelIndex idx = src->index(sourceRow, 0, sourceParent);

    if (src->data(idx, DjSongModel::TitleRole).toString().toLower().contains(m_searchText))
        return true;
    if (src->data(idx, DjSongModel::ArtistRole).toString().toLower().contains(m_searchText))
        return true;
    if (src->data(idx, DjSongModel::FilepathRole).toString().toLower().contains(m_searchText))
        return true;
    return false;
}

bool DjSongFilterModel::lessThan(const QModelIndex &left,
                                 const QModelIndex &right) const
{
    QAbstractItemModel *src = sourceModel();

    switch (m_sortMode)
    {
    case Alphabetical:
    {
        QString l = src->data(left, DjSongModel::TitleRole).toString().toLower();
        QString r = src->data(right, DjSongModel::TitleRole).toString().toLower();
        return l < r;
    }
    case RecentlyPlayed:
    {
        QDateTime l = src->data(left, DjSongModel::LastPlayedRole).toDateTime();
        QDateTime r = src->data(right, DjSongModel::LastPlayedRole).toDateTime();
        if (!l.isValid() && !r.isValid()) return false;
        if (!l.isValid()) return false;
        if (!r.isValid()) return true;
        return l > r;
    }
    case RecentlyEdited:
    {
        QDateTime l = src->data(left, DjSongModel::LastEditedRole).toDateTime();
        QDateTime r = src->data(right, DjSongModel::LastEditedRole).toDateTime();
        if (!l.isValid() && !r.isValid()) return false;
        if (!l.isValid()) return false;
        if (!r.isValid()) return true;
        return l > r;
    }
    }
    return false;
}

// ════════════════════════════════════════════════════════════════
//  DjDeckModel
// ════════════════════════════════════════════════════════════════

DjDeckModel::DjDeckModel(DjFsm *fsm, QObject *parent)
    : QAbstractListModel(parent)
    , m_fsm(fsm)
{
}

int DjDeckModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_fsm)
        return 0;
    return m_fsm->deckCount();
}

QHash<int, QByteArray> DjDeckModel::roleNames() const
{
    return {
        { DeckNumberRole, "deckNumber" },
        { StateRole,      "state" },
        { TitleRole,      "title" },
        { ArtistRole,     "artist" },
        { FilepathRole,   "filepath" },
        { BpmRole,        "bpm" },
        { VolumeRole,     "volume" },
        { BeatPosRole,    "beatPos" },
        { ElapsedRole,    "elapsedMs" },
        { RemainingRole,  "remainingMs" },
        { IsActiveRole,   "isActive" },
        { IsPlayingRole,  "isPlaying" },
        { WeightRole,     "weight" },
    };
}

QVariant DjDeckModel::data(const QModelIndex &index, int role) const
{
    if (!m_fsm || !index.isValid() || index.row() < 0 || index.row() >= m_fsm->deckCount())
        return {};

    const DjFsm::Deck &deck = m_fsm->deckAt(index.row());

    switch (role)
    {
    case DeckNumberRole: return deck.number;
    case StateRole:      return DjFsm::stateName(deck.state);
    case TitleRole:      return deck.song.title;
    // an emptied deck keeps VDJ's last artist and bpm for the next song, not for display
    case ArtistRole:     return deck.song.isValid() ? deck.song.artist : QString();
    case FilepathRole:   return deck.song.filepath;
    case BpmRole:        return deck.song.isValid() ? deck.song.bpm : 0.0;
    case VolumeRole:     return deck.volume;
    case BeatPosRole:    return deck.beatPos;
    case ElapsedRole:    return deck.elapsedMs;
    case RemainingRole:  return deck.remainingMs;
    case IsActiveRole:   return deck.number == m_activeDeck;
    case IsPlayingRole:  return deck.playing;
    case WeightRole:     return deck.weight;
    default:             return {};
    }
}

void DjDeckModel::refreshDeck(int deck)
{
    if (!m_fsm || deck < 1 || deck > m_fsm->deckCount())
        return;
    QModelIndex mi = index(deck - 1);
    emit dataChanged(mi, mi);
}

void DjDeckModel::refreshDeckPosition(int deck)
{
    if (!m_fsm || deck < 1 || deck > m_fsm->deckCount())
        return;
    QModelIndex mi = index(deck - 1);
    emit dataChanged(mi, mi, { BeatPosRole, ElapsedRole, RemainingRole });
}

void DjDeckModel::refreshDeckWeight(int deck)
{
    if (!m_fsm || deck < 1 || deck > m_fsm->deckCount())
        return;
    QModelIndex mi = index(deck - 1);
    emit dataChanged(mi, mi, { WeightRole });
}

void DjDeckModel::refreshAll()
{
    beginResetModel();
    endResetModel();
}

void DjDeckModel::setActiveDeck(int deck)
{
    if (m_activeDeck == deck || !m_fsm)
        return;
    int prev = m_activeDeck;
    m_activeDeck = deck;
    if (prev >= 1 && prev <= m_fsm->deckCount())
    {
        QModelIndex mi = index(prev - 1);
        emit dataChanged(mi, mi, { IsActiveRole });
    }
    if (deck >= 1 && deck <= m_fsm->deckCount())
    {
        QModelIndex mi = index(deck - 1);
        emit dataChanged(mi, mi, { IsActiveRole });
    }
}

// ════════════════════════════════════════════════════════════════
//  DjManager
// ════════════════════════════════════════════════════════════════

DjManager::DjManager(QQuickView *view, Doc *doc, VdjBridge *bridge,
                     ShowFactory *factory, QObject *parent)
    : PreviewContext(view, doc, "DJMGR", parent)
    , m_bridge(bridge)
    , m_factory(factory)
    , m_fsm(bridge ? bridge->djFsm() : nullptr)
    , m_model(new DjSongModel(doc, factory, this))
    , m_proxy(new DjSongFilterModel(this))
    , m_deckModel(new DjDeckModel(m_fsm, this))
{
    m_proxy->setSourceModel(m_model);

    setContextResource("qrc:/DjManager.qml");
    setContextTitle(tr("DJ Manager"));

    view->rootContext()->setContextProperty("djManager", this);

    // resolve the VirtualDJ database once at startup; the QML view offers
    // a refresh and re-checks whenever it is (re)loaded
    refreshVdjDatabase();

    if (m_factory)
    {
        connect(m_factory, &ShowFactory::mappingChanged, this,
                [this](const QString &filepath) { m_model->refreshShow(filepath); });
    }

    if (m_fsm)
    {
        connect(m_fsm, &DjFsm::songChanged, this, &DjManager::onSongChanged);
        connect(m_fsm, &DjFsm::songUpdated, this,
                [this](int, const DjFsm::DeckSong &song) { m_model->upsertSong(song); });
        connect(m_fsm, &DjFsm::deckChanged, this, &DjManager::onDeckChanged);
        connect(m_fsm, &DjFsm::positionChanged, this, &DjManager::onDeckPositionChanged);
        connect(m_fsm, &DjFsm::weightChanged, m_deckModel, &DjDeckModel::refreshDeckWeight);
        connect(m_fsm, &DjFsm::activeDeckChanged, this, &DjManager::onActiveDeckChanged);
        connect(m_fsm, &DjFsm::activeSongChanged, this, &DjManager::onActiveSongChanged);
        connect(m_fsm, &DjFsm::deckCountChanged, this, &DjManager::onDeckCountChanged);

        // Apply the persisted deck count (default 2).
        QSettings settings;
        int persisted = settings.value("vdj/deckCount", DjFsm::DefaultDecks).toInt();
        m_fsm->setDeckCount(persisted);
    }

    if (m_bridge)
    {
        connect(m_bridge, &VdjBridge::performModeChanged,
                this, &DjManager::performModeChanged);
    }

    if (m_doc)
    {
        connect(m_doc, &Doc::cleared, this, &DjManager::onDocCleared);
        // queued: App reads the song choices after the Engine, before the event loop runs
        connect(m_doc, &Doc::loaded, this, &DjManager::onDocLoaded, Qt::QueuedConnection);
        connect(m_doc, &Doc::functionAdded, this, &DjManager::onFunctionAdded);
        connect(m_doc, &Doc::functionChanged, this, &DjManager::onFunctionChanged);

        m_model->rebuildFromDoc();
    }
}

QString DjManager::vdjDatabasePath() const
{
    return m_vdjDatabasePath;
}

bool DjManager::vdjDatabaseFound() const
{
    return m_vdjDatabasePath.isEmpty() == false;
}

void DjManager::refreshVdjDatabase()
{
    const QString path = VdjDatabaseReader::locateDatabase();
    if (path == m_vdjDatabasePath)
        return;

    m_vdjDatabasePath = path;
    if (path.isEmpty())
        qWarning() << "[DjManager] no VirtualDJ database.xml found; candidates:"
                   << VdjDatabaseReader::databaseCandidates(QString());
    else
        qDebug() << "[DjManager] VirtualDJ database:" << path;
    emit vdjDatabasePathChanged();
}

QAbstractItemModel *DjManager::songListModel() const
{
    return m_proxy;
}

QAbstractItemModel *DjManager::deckModel() const
{
    return m_deckModel;
}

int DjManager::sortMode() const
{
    return static_cast<int>(m_proxy->sortMode());
}

void DjManager::setSortMode(int mode)
{
    auto m = static_cast<DjSongFilterModel::SortMode>(mode);
    if (m_proxy->sortMode() == m)
        return;
    m_proxy->setSortMode(m);
    emit sortModeChanged();
}

bool DjManager::sortAscending() const
{
    return m_proxy->sortAscending();
}

void DjManager::setSortAscending(bool asc)
{
    if (m_proxy->sortAscending() == asc)
        return;
    m_proxy->setSortAscending(asc);
    emit sortAscendingChanged();
}

QString DjManager::searchFilter() const
{
    return m_proxy->searchFilter();
}

void DjManager::setSearchFilter(const QString &text)
{
    QString prev = m_proxy->searchFilter();
    m_proxy->setSearchFilter(text);
    if (m_proxy->searchFilter() == prev)
        return;
    emit searchFilterChanged();
}

bool DjManager::performMode() const
{
    return m_bridge ? m_bridge->performMode() : false;
}

void DjManager::setPerformMode(bool on)
{
    if (!m_bridge)
        return;
    m_bridge->setPerformMode(on);
    // performModeChanged is forwarded from the bridge signal.
}

int DjManager::activeDeck() const
{
    return m_fsm ? m_fsm->activeDeck() : 0;
}

QString DjManager::activeTitle() const
{
    return m_fsm ? m_fsm->activeSong().title : QString();
}

QString DjManager::activeArtist() const
{
    // an emptied deck keeps VDJ's last artist for the next song, not for the header
    const DjFsm::DeckSong song = m_fsm ? m_fsm->activeSong() : DjFsm::DeckSong();
    return song.isValid() ? song.artist : QString();
}

int DjManager::activeElapsedMs() const
{
    if (!m_fsm || m_fsm->activeDeck() < 1)
        return 0;
    return m_fsm->deckAt(m_fsm->activeDeck() - 1).elapsedMs;
}

double DjManager::activeBeatPos() const
{
    if (!m_fsm || m_fsm->activeDeck() < 1)
        return 0.0;
    return m_fsm->deckAt(m_fsm->activeDeck() - 1).beatPos;
}

int DjManager::deckCount() const
{
    return m_fsm ? m_fsm->deckCount() : DjFsm::DefaultDecks;
}

void DjManager::setDeckCount(int count)
{
    if (!m_fsm || count == m_fsm->deckCount())
        return;
    m_fsm->setDeckCount(count);
    QSettings settings;
    settings.setValue("vdj/deckCount", m_fsm->deckCount());
    // deckCountChanged is forwarded via onDeckCountChanged.
}

// --- Per-song show actions ---

void DjManager::loadShow(const QString &filepath)
{
    quint32 showId = m_factory ? m_factory->showIdForFilepath(filepath) : Function::invalidId();
    if (showId == Function::invalidId())
        return;
    emit showLoadRequested(static_cast<int>(showId));
}

void DjManager::assignShow(const QString &filepath, int showId)
{
    if (!m_doc || !m_factory || filepath.isEmpty())
        return;

    quint32 id = static_cast<quint32>(showId);
    Function *f = m_doc->function(id);
    if (!f || f->type() != Function::ShowType)
        return;

    m_factory->registerMapping(filepath, id);
}

void DjManager::createShow(const QString &filepath)
{
    if (!m_factory || filepath.isEmpty())
        return;
    DjFsm::DeckSong song = m_model->songForFilepath(filepath);
    if (!song.isValid())
        return;
    // the Show New builds is the user's choice for the song
    const quint32 id = m_factory->buildShow(song);
    if (id != Function::invalidId())
        m_factory->registerMapping(filepath, id);
}

QVariantList DjManager::availableShows() const
{
    QVariantList list;
    if (!m_doc)
        return list;
    for (Function *f : m_doc->functionsByType(Function::ShowType))
    {
        QVariantMap entry;
        entry["id"] = f->id();
        entry["name"] = f->name();
        list.append(entry);
    }
    return list;
}

// --- FSM-driven slots ---

void DjManager::onSongChanged(int deck, const DjFsm::DeckSong &song)
{
    Q_UNUSED(deck)

    m_model->upsertSong(song);
    // an announced song without a Show gets one now
    if (m_factory && m_factory->showIdForFilepath(song.filepath) == Function::invalidId())
        m_factory->buildShow(song);

    // A song row may have just been added for a deck that is already playing:
    // reflect the playing state immediately.
    recomputePlayingSongs();
}

void DjManager::onDeckChanged(int deck)
{
    m_deckModel->refreshDeck(deck);
    // NOTE: the song list is updated ONLY from the committed songChanged (a
    // consistent snapshot). The FSM's live deck.song is intentionally
    // inconsistent mid-burst (e.g. filepath still the previous song while the
    // new title is already set), so it must NOT be written into the
    // filepath-keyed song list here.
    recomputePlayingSongs();
}

void DjManager::onDeckPositionChanged(int deck)
{
    m_deckModel->refreshDeckPosition(deck);
    if (m_fsm && deck == m_fsm->activeDeck())
        emit activePositionChanged();
}

void DjManager::onActiveDeckChanged(int deck)
{
    m_deckModel->setActiveDeck(deck);
    emit activeChanged();
    emit activePositionChanged();
}

void DjManager::onActiveSongChanged()
{
    QString fp = m_fsm ? m_fsm->activeSong().filepath : QString();
    m_model->setActiveFilepath(fp);
    emit activeChanged();
}

void DjManager::onDeckCountChanged()
{
    m_deckModel->refreshAll();
    recomputePlayingSongs();
    emit deckCountChanged();
    emit activeChanged();
}

void DjManager::recomputePlayingSongs()
{
    if (!m_fsm)
        return;

    QSet<QString> nowPlaying;
    for (int i = 0; i < m_fsm->deckCount(); ++i)
    {
        const DjFsm::Deck &deck = m_fsm->deckAt(i);
        if (deck.playing && !deck.song.filepath.isEmpty())
            nowPlaying.insert(deck.song.filepath);
    }

    // Stamp lastPlayed once when a song newly starts playing.
    for (const QString &fp : nowPlaying)
        if (!m_lastPlayingPaths.contains(fp))
            m_model->markPlayed(fp);

    // Always re-apply to the model (not gated on a set diff): a song row may
    // have been added AFTER its deck started playing, and must still be marked.
    m_model->setPlayingFilepaths(nowPlaying);
    m_lastPlayingPaths = nowPlaying;
}

void DjManager::onDocCleared()
{
    m_model->clear();
    m_lastPlayingPaths.clear();
    // the decks play on through the workspace change: their songs are back as
    // rows now, announced again once the workspace is in (never built here: a
    // Show built now would take an id the loading workspace assigns)
    for (int i = 0; m_fsm && i < m_fsm->deckCount(); ++i)
    {
        const DjFsm::DeckSong &song = m_fsm->deckAt(i).song;
        if (song.isValid() && !song.title.isEmpty())
            m_model->upsertSong(song);
    }
    recomputePlayingSongs();
}

void DjManager::onDocLoaded()
{
    // the decks play on through the workspace change: their songs are announced to it
    for (int i = 0; m_fsm && i < m_fsm->deckCount(); ++i)
    {
        const DjFsm::DeckSong &song = m_fsm->deckAt(i).song;
        if (song.isValid() && !song.title.isEmpty())
            onSongChanged(i + 1, song);
    }
}

void DjManager::onFunctionAdded(quint32 fid)
{
    // a song Show of the Doc gets a row of its own, never a full rebuild: the
    // FSM-sourced rows stay, and a Show under construction (no clip yet) adds none
    Function *f = m_doc ? m_doc->function(fid) : nullptr;
    if (!f || f->type() != Function::ShowType || !f->path(true).startsWith(kSongFolderPath))
        return;
    Audio *audio = ShowFactory::songAudio(m_doc, static_cast<Show *>(f));
    if (!audio || m_model->hasFilepath(audio->getSourceFileName()))
        return;

    DjFsm::DeckSong song;
    song.filepath = audio->getSourceFileName();
    song.title = f->name();
    song.totalMs = static_cast<int>(audio->totalDuration());
    m_model->upsertSong(song);
}

void DjManager::onFunctionChanged(quint32 fid)
{
    m_model->markEdited(fid);
}
