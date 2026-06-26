/*
  Q Light Controller Plus
  configurevdjbridge.h

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

#ifndef CONFIGUREVDJBRIDGE_H
#define CONFIGUREVDJBRIDGE_H

#include <QDialog>
#include <QHash>

class QSpinBox;
class QCheckBox;
class QLineEdit;
class QLabel;
class QTableWidget;
class VdjBridgePlugin;

class ConfigureVdjBridge : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigureVdjBridge(VdjBridgePlugin *plugin, QWidget *parent = nullptr);

private slots:
    void slotPortChanged(int value);
    void slotBonjourToggled(bool checked);
    void slotServiceNameChanged(const QString &text);
    void slotClearLog();

    void slotDeckTrigger(int deckIndex, const QString &trigger, const QVariant &value);
    void slotGlobalTrigger(const QString &trigger, const QVariant &value);
    void slotBeatReceived(int pos, qreal bpm, qreal strength, bool change);
    void slotClientConnected();
    void slotClientDisconnected();

private:
    void upsertDebugEntry(const QString &source, const QString &key, const QString &valueText);
    void updateStatusLabel();
    static QString normalizeText(const QString &text);

    struct DebugEntry
    {
        int row = -1;
        quint64 totalCount = 0;
        QHash<QString, quint64> valueCounts;
    };

    VdjBridgePlugin *m_plugin;

    QSpinBox       *m_portSpin;
    QCheckBox      *m_bonjourCheck;
    QLineEdit      *m_serviceNameEdit;
    QLabel         *m_statusLabel;
    QTableWidget   *m_debugTable;
    QHash<QString, DebugEntry> m_debugEntries;

    int m_beatCounter = 0;
};

#endif // CONFIGUREVDJBRIDGE_H
