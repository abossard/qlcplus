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
#include <QList>
#include <QString>

class Doc;
class VdjBridge;
class ShowFactory;

/**
 * Per-session model of auto-created song Shows.
 *
 * One row per Show created by ShowFactory in response to a VDJ
 * song-load event. Rows accumulate during the session and are
 * cleared when the workspace is reset.
 *
 * QAbstractListModel (not QVariantList) so that ListView can apply
 * incremental updates as songs are added.
 */
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
    };

    explicit SongListModel(Doc *doc, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** Add a row for a Show created from filepath. No-op if filepath already known. */
    void addSong(const QString &filepath, quint32 showId);

    /** Remove all rows. */
    void clear();

private:
    struct Row
    {
        QString filepath;
        quint32 showId = 0;
    };

    Doc *m_doc;
    QList<Row> m_rows;
};

/**
 * Song Manager context.
 *
 * Standalone view (not embedded in ShowManager) showing:
 *  - VDJ telemetry connection status (derived from VdjBridge)
 *  - Per-deck states + master deck info
 *  - Scrollable list of auto-created song Shows
 *
 * Status bar properties are derived in QML from vdjBridge to avoid
 * duplicating state in C++.
 */
class SongManager final : public PreviewContext
{
    Q_OBJECT
    Q_PROPERTY(QAbstractListModel *songListModel READ songListModel CONSTANT)

public:
    SongManager(QQuickView *view, Doc *doc, VdjBridge *bridge,
                ShowFactory *factory, QObject *parent = nullptr);

    QAbstractListModel *songListModel() const;

private slots:
    void onShowCreatedForSong(const QString &filepath, quint32 showId);
    void onDocCleared();

private:
    VdjBridge *m_bridge;
    ShowFactory *m_factory;
    SongListModel *m_model;
};

#endif // SONGMANAGER_H
