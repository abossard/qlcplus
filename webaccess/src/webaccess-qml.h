/*
  Q Light Controller Plus
  webaccess-qml.h

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

#ifndef WEBACCESS_QML_H
#define WEBACCESS_QML_H

#include <QSet>
#include <QHash>
#include <QBitArray>
#include <QVector>
#include <QPair>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "webaccessbase.h"

class QTimer;

class VirtualConsole;
class SimpleDesk;
class VCWidget;
class VCFrame;
class VCButton;
class VCSlider;
class VCLabel;
class VCCueList;
class VCAudioTriggers;
class VCClock;
class VCAnimation;
class VCSpeedDial;
class VCXYPad;
class Doc;

class QHttpRequest;
class QHttpResponse;
class QHttpConnection;

class QJsonObject;

class WebAccessQml final : public WebAccessBase
{
    Q_OBJECT
public:
    explicit WebAccessQml(Doc *doc, VirtualConsole *vcInstance, SimpleDesk *sdInstance,
                          int portNumber, bool enableAuth, QString passwdFile = QString(),
                          QObject *parent = nullptr);
    ~WebAccessQml();

signals:
    void loadProject(QByteArray xmlData);
    void storeAutostartProject(QString fileName);

protected slots:
    void slotHandleHTTPRequest(QHttpRequest *req, QHttpResponse *resp) override;
    void slotHandleWebSocketRequest(QHttpConnection *conn, QString data) override;
    void slotHandleWebSocketClose(QHttpConnection *conn) override;

    void slotUniverseWritten(quint32 universeIdx, QByteArray data);
    void slotFlushDmxDeltas(QHttpConnection *conn);

    void slotFunctionStarted(quint32 fid) override;
    void slotFunctionStopped(quint32 fid) override;

    void slotDocLoaded();
    void slotSelectedPageChanged(int page);

    void slotButtonStateChanged(int state);
    void slotButtonDisableStateChanged(bool disable);
    void slotLabelDisableStateChanged(bool disable);
    void slotWidgetVisibilityChanged(bool isVisible);
    void slotSliderValueChanged(int value);
    void slotSliderDisableStateChanged(bool disable);
    void slotSliderOverrideChanged();
    void slotSliderClickAndGoColorsChanged();
    void slotAudioTriggersToggled();
    void slotAudioTriggersVolumeChanged();
    void slotWidgetDisableStateChanged(bool disable);
    void slotCueIndexChanged(int idx);
    void slotCuePlaybackStateChanged();
    void slotCueSideFaderLevelChanged();
    void slotCueDisableStateChanged(bool disable);
    void slotFramePageChanged(int pageNum);
    void slotFrameDisableStateChanged(bool disable);
    void slotMatrixFaderChanged();
    void slotMatrixColorsChanged();
    void slotMatrixAlgorithmChanged();
    void slotXYPadPositionChanged();
    void slotXYPadPresetChanged();
    void slotSpeedDialTimeChanged();
    void slotSpeedDialFactorChanged();
    void slotClockTimeChanged(int time);
    void slotClockTimerRunningChanged(bool running);
    void slotGrandMasterValueChanged(uchar value);

protected:
    QString webFilePath(const QString &relativePath) const override;
    bool serveVCNextFile(QHttpResponse *resp, const QString &relativePath) const;
    void sendMatrixState(const VCAnimation *animation) const;
    void handleAutostartProject(const QString &path) override;
    void handleProjectLoad(const QByteArray &projectXml) override;

    QByteArray getVCJson();
    QByteArray buildFixturesJson();
    QByteArray buildChannelsJson(const QList<quint32> &fixtureIDs);
    QJsonObject baseWidgetToJson(const VCWidget *widget);
    QJsonObject widgetToJson(const VCWidget *widget);
    QJsonObject frameToJson(const VCFrame *frame);
    void collectWidgets(const VCFrame *frame, QList<VCWidget *> &list, bool recursive = true) const;

    void setupWidgetConnections(const VCWidget *widget);
    QString widgetBackgroundImagePath(const VCWidget *widget) const;

protected:
    QSet<quint32> m_connectedWidgets;

    // ---- DMX subscription / push -----------------------------------------
    struct DmxSubscription
    {
        QSet<quint32> fixtureIDs;
        QHash<quint32, QBitArray> subscribedAddrs;          // universe -> address mask
        QHash<quint32, QByteArray> lastSent;                // universe -> last sent values
        QHash<quint32, QVector<QPair<int, uchar>>> pendingDeltas; // universe -> [(addr,val)]
        QTimer *flushTimer = nullptr;
        qint64 lastActivity = 0;
    };

    void handleDmxJson(QHttpConnection *conn, const QJsonObject &msg);
    void rebuildSubscribedAddrs(QHttpConnection *conn);
    void sendDmxSnapshot(QHttpConnection *conn, quint32 fixtureID);
    void cleanupDmxSubscription(QHttpConnection *conn);

    QHash<QHttpConnection *, DmxSubscription> m_dmxSubs;
};

#endif // WEBACCESS_QML_H
