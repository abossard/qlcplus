/*
  Q Light Controller Plus
  djmanager.h

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

#ifndef DJMANAGER_H
#define DJMANAGER_H

#include "previewcontext.h"
#include "djfsm.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>

class Doc;
class VdjBridge;
class ShowFactory;

// ────────────────────────────────────────────────────────────────
//  DjSongModel — songs seen on the decks, keyed by filepath identity
// ────────────────────────────────────────────────────────────────

class DjSongModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        FilepathRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        BpmRole,
        KeyRole,
        DurationRole,
        ShowIdRole,
        HasShowRole,
        IsPlayingRole,
        IsActiveRole,
        LastPlayedRole,
        LastEditedRole,
    };

    struct Row
    {
        QString filepath;       //!< identity
        QString title;
        QString artist;
        QString key;
        double  bpm = 0.0;
        int     durationMs = 0;
        quint32 showId = 0;     //!< Function::invalidId() when unassigned
        bool    isPlaying = false;
        bool    isActive = false;
    };

    explicit DjSongModel(Doc *doc, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** Rebuild from all Shows in the VDJ Songs folder (startup). */
    void rebuildFromDoc();

    /** Add or refresh a song row from FSM metadata. Returns true if newly added. */
    bool upsertSong(const DjFsm::DeckSong &song);

    /** True if a row with this filepath already exists. */
    bool hasFilepath(const QString &filepath) const;

    /** Assign (or clear with Function::invalidId()) the show for a filepath. */
    void setShow(const QString &filepath, quint32 showId);

    /** Unassign the show from whichever row currently references it. */
    void clearShowById(quint32 showId);

    /** Show ID currently assigned to a filepath, or Function::invalidId(). */
    quint32 showIdForFilepath(const QString &filepath) const;

    /** Build a DeckSong snapshot from a row (used to create a fresh show). */
    DjFsm::DeckSong songForFilepath(const QString &filepath) const;

    /** Mark the currently-playing filepaths (diff against the last set). */
    void setPlayingFilepaths(const QSet<QString> &playing);

    /** Mark which filepath is on the active deck (empty = none). */
    void setActiveFilepath(const QString &filepath);

    /** Record a "last played" timestamp for a filepath. */
    void markPlayed(const QString &filepath);

    /** Record a "last edited" timestamp for a show ID. */
    void markEdited(quint32 showId);

    void clear();

private:
    int rowForFilepath(const QString &fp) const;
    int rowForShowId(quint32 id) const;
    void emitRow(int idx, const QList<int> &roles);

    Doc *m_doc;
    QList<Row> m_rows;
    QString m_activeFilepath;
};

// ────────────────────────────────────────────────────────────────
//  DjSongFilterModel — sort/filter proxy for DjSongModel
// ────────────────────────────────────────────────────────────────

class DjSongFilterModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum SortMode { Alphabetical = 0, RecentlyPlayed, RecentlyEdited };
    Q_ENUM(SortMode)

    explicit DjSongFilterModel(QObject *parent = nullptr);

    void setSortMode(SortMode mode);
    SortMode sortMode() const { return m_sortMode; }

    void setSortAscending(bool asc);
    bool sortAscending() const { return m_ascending; }

    void setSearchFilter(const QString &text);
    QString searchFilter() const { return m_searchText; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    SortMode m_sortMode = Alphabetical;
    bool m_ascending = true;
    QString m_searchText;
};

// ────────────────────────────────────────────────────────────────
//  DjDeckModel — flat 4-row table of the FSM deck states (top area)
// ────────────────────────────────────────────────────────────────

class DjDeckModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        DeckNumberRole = Qt::UserRole + 1,
        StateRole,
        TitleRole,
        ArtistRole,
        FilepathRole,
        BpmRole,
        VolumeRole,
        BeatPosRole,
        ElapsedRole,
        RemainingRole,
        IsActiveRole,
        IsPlayingRole,
    };

    explicit DjDeckModel(DjFsm *fsm, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** Refresh a single deck row (1-based) from the FSM. */
    void refreshDeck(int deck);

    /** Refresh only the position columns of a deck row (1-based). */
    void refreshDeckPosition(int deck);

    /** Reset the whole table (e.g. after the deck count changes). */
    void refreshAll();

    /** Set the active deck (1-based; 0 = none) and refresh affected rows. */
    void setActiveDeck(int deck);

private:
    DjFsm *m_fsm;
    int m_activeDeck = 0;
};

// ────────────────────────────────────────────────────────────────
//  DjManager — controller wiring the FSM, Doc, and the models
// ────────────────────────────────────────────────────────────────

class DjManager final : public PreviewContext
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *songListModel READ songListModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *deckModel READ deckModel CONSTANT)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY sortAscendingChanged)
    Q_PROPERTY(QString searchFilter READ searchFilter WRITE setSearchFilter NOTIFY searchFilterChanged)
    Q_PROPERTY(int songCount READ songCount NOTIFY songCountChanged)
    Q_PROPERTY(bool performMode READ performMode WRITE setPerformMode NOTIFY performModeChanged)
    Q_PROPERTY(int activeDeck READ activeDeck NOTIFY activeChanged)
    Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeChanged)
    Q_PROPERTY(QString activeArtist READ activeArtist NOTIFY activeChanged)
    Q_PROPERTY(int activeElapsedMs READ activeElapsedMs NOTIFY activePositionChanged)
    Q_PROPERTY(int activeRemainingMs READ activeRemainingMs NOTIFY activePositionChanged)
    Q_PROPERTY(double activeBeatPos READ activeBeatPos NOTIFY activePositionChanged)
    Q_PROPERTY(int deckCount READ deckCount WRITE setDeckCount NOTIFY deckCountChanged)

public:
    DjManager(QQuickView *view, Doc *doc, VdjBridge *bridge,
              ShowFactory *factory, QObject *parent = nullptr);

    QAbstractItemModel *songListModel() const;
    QAbstractItemModel *deckModel() const;

    int sortMode() const;
    void setSortMode(int mode);

    bool sortAscending() const;
    void setSortAscending(bool asc);

    QString searchFilter() const;
    void setSearchFilter(const QString &text);

    int songCount() const;

    bool performMode() const;
    void setPerformMode(bool on);

    int activeDeck() const;
    QString activeTitle() const;
    QString activeArtist() const;
    int activeElapsedMs() const;
    int activeRemainingMs() const;
    double activeBeatPos() const;

    int deckCount() const;
    void setDeckCount(int count);

    // --- Per-song show actions (invoked from QML) ---

    /** Open the show assigned to a song in the Show Manager. */
    Q_INVOKABLE void loadShow(const QString &filepath);

    /** Assign an existing show to a song. */
    Q_INVOKABLE void assignShow(const QString &filepath, int showId);

    /** Remove the show assignment from a song. */
    Q_INVOKABLE void clearShow(const QString &filepath);

    /** Create a brand-new show for a song and assign it. */
    Q_INVOKABLE void createShow(const QString &filepath);

    /** List of existing Shows as [{ id, name }] for the assign picker. */
    Q_INVOKABLE QVariantList availableShows() const;

signals:
    void sortModeChanged();
    void sortAscendingChanged();
    void searchFilterChanged();
    void songCountChanged();
    void performModeChanged();
    void activeChanged();
    void activePositionChanged();
    void deckCountChanged();
    void showLoadRequested(int showId);

private slots:
    void onSongChanged(int deck, const DjFsm::DeckSong &song);
    void onDeckChanged(int deck);
    void onDeckPositionChanged(int deck);
    void onActiveDeckChanged(int deck);
    void onActiveSongChanged();
    void onDeckCountChanged();
    void onShowCreatedForSong(const QString &filepath, quint32 showId);
    void onDocCleared();
    void onFunctionAdded(quint32 fid);
    void onFunctionRemoved(quint32 fid);
    void onFunctionChanged(quint32 fid);

private:
    void ensureShowForSong(const DjFsm::DeckSong &song);
    void recomputePlayingSongs();
    void mergeShowFromDoc(quint32 showId);

    VdjBridge *m_bridge;
    ShowFactory *m_factory;
    DjFsm *m_fsm;
    DjSongModel *m_model;
    DjSongFilterModel *m_proxy;
    DjDeckModel *m_deckModel;
    QSet<QString> m_lastPlayingPaths;
};

#endif // DJMANAGER_H
