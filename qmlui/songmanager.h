/*
  Q Light Controller Plus
  songmanager.h

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

#ifndef SONGMANAGER_H
#define SONGMANAGER_H

#include "previewcontext.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>

class Doc;
class VdjBridge;
class ShowFactory;
class VdjDeckModel;

// ────────────────────────────────────────────────────────────────
//  SongListModel — flat model of all song Shows in the VDJ Songs folder
// ────────────────────────────────────────────────────────────────

class SongListModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        ShowIdRole = Qt::UserRole + 1,
        AudioIdRole,
        TitleRole,
        ArtistRole,
        BpmRole,
        KeyRole,
        DurationRole,
        FilepathRole,
        IsPlayingRole,
        LastPlayedRole,
        LastEditedRole,
    };

    explicit SongListModel(Doc *doc, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** Rebuild the model from all Shows in the VDJ Songs folder. */
    void rebuildFromDoc();

    /** Add a row for a Show. No-op if showId already present. */
    void addSong(const QString &filepath, quint32 showId);

    /** Remove all rows. */
    void clear();

    /** Mark a song as currently playing (matched by filepath). */
    void setPlaying(const QString &filepath, bool playing);

    /** Record a "last played" timestamp for a filepath. */
    void markPlayed(const QString &filepath);

    /** Record a "last edited" timestamp for a show ID. */
    void markEdited(quint32 showId);

    /** Clear all playing flags. */
    void clearPlayingState();

    struct Row
    {
        QString filepath;
        quint32 showId = 0;
        bool isPlaying = false;
    };

private:
    int rowForFilepath(const QString &fp) const;
    int rowForShowId(quint32 id) const;

    Doc *m_doc;
    QList<Row> m_rows;
};

// ────────────────────────────────────────────────────────────────
//  SongSortFilterProxyModel — sort/filter proxy for SongListModel
// ────────────────────────────────────────────────────────────────

class SongSortFilterProxyModel final : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum SortMode { Alphabetical = 0, RecentlyPlayed, RecentlyEdited };
    Q_ENUM(SortMode)

    explicit SongSortFilterProxyModel(QObject *parent = nullptr);

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
//  SongManager — controller connecting VDJ, Doc, and the models
// ────────────────────────────────────────────────────────────────

class SongManager final : public PreviewContext
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *songListModel READ songListModel CONSTANT)
    Q_PROPERTY(int sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY sortAscendingChanged)
    Q_PROPERTY(QString searchFilter READ searchFilter WRITE setSearchFilter NOTIFY searchFilterChanged)
    Q_PROPERTY(int songCount READ songCount NOTIFY songCountChanged)

public:
    SongManager(QQuickView *view, Doc *doc, VdjBridge *bridge,
                ShowFactory *factory, QObject *parent = nullptr);

    QAbstractItemModel *songListModel() const;

    int sortMode() const;
    void setSortMode(int mode);

    bool sortAscending() const;
    void setSortAscending(bool asc);

    QString searchFilter() const;
    void setSearchFilter(const QString &text);

    int songCount() const;

signals:
    void sortModeChanged();
    void sortAscendingChanged();
    void searchFilterChanged();
    void songCountChanged();

private slots:
    void onShowCreatedForSong(const QString &filepath, quint32 showId);
    void onDocCleared();
    void onFunctionAdded(quint32 fid);
    void onFunctionRemoved(quint32 fid);
    void onFunctionChanged(quint32 fid);
    void refreshDeckPlayingState();

private:
    void connectDeckSignals();

    VdjBridge *m_bridge;
    ShowFactory *m_factory;
    SongListModel *m_model;
    SongSortFilterProxyModel *m_proxy;
    QSet<QString> m_lastPlayingPaths;
};

#endif // SONGMANAGER_H
