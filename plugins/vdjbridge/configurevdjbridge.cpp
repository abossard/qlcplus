/*
  Q Light Controller Plus
  configurevdjbridge.cpp

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

#include "configurevdjbridge.h"
#include "vdjbridgeplugin.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>

ConfigureVdjBridge::ConfigureVdjBridge(VdjBridgePlugin *plugin, QWidget *parent)
    : QDialog(parent)
    , m_plugin(plugin)
{
    setWindowTitle(tr("VDJ Bridge Configuration"));
    setMinimumSize(480, 400);

    auto *mainLayout = new QVBoxLayout(this);

    // --- Settings form ---
    auto *formLayout = new QFormLayout;

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(plugin->hostPort());
    formLayout->addRow(tr("Listen port:"), m_portSpin);

    m_bonjourCheck = new QCheckBox(tr("Enabled"), this);
    m_bonjourCheck->setChecked(plugin->bonjourEnabled());
    formLayout->addRow(tr("Bonjour:"), m_bonjourCheck);

    m_serviceNameEdit = new QLineEdit(this);
    m_serviceNameEdit->setText(plugin->serviceName());
    formLayout->addRow(tr("Service name:"), m_serviceNameEdit);

    m_statusLabel = new QLabel(this);
    formLayout->addRow(tr("Status:"), m_statusLabel);

    mainLayout->addLayout(formLayout);

    // --- Debug table ---
    auto *logLabel = new QLabel(tr("Debug values:"), this);
    mainLayout->addWidget(logLabel);

    m_debugTable = new QTableWidget(this);
    m_debugTable->setObjectName(QStringLiteral("debugTable"));
    m_debugTable->setColumnCount(5);
    m_debugTable->setHorizontalHeaderLabels({
        tr("Source"),
        tr("Key"),
        tr("Latest value"),
        tr("Receives"),
        tr("Latest value receives"),
    });
    m_debugTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_debugTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_debugTable->setAlternatingRowColors(true);
    m_debugTable->verticalHeader()->setVisible(false);
    m_debugTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_debugTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_debugTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_debugTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_debugTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    mainLayout->addWidget(m_debugTable, 1);

    // --- Buttons ---
    auto *buttonLayout = new QHBoxLayout;
    auto *clearBtn = new QPushButton(tr("Clear"), this);
    clearBtn->setObjectName(QStringLiteral("clearDebugTableButton"));
    buttonLayout->addWidget(clearBtn);
    buttonLayout->addStretch();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttonLayout->addWidget(buttonBox);
    mainLayout->addLayout(buttonLayout);

    // --- Connections: UI controls ---
    connect(m_portSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConfigureVdjBridge::slotPortChanged);
    connect(m_bonjourCheck, &QCheckBox::toggled,
            this, &ConfigureVdjBridge::slotBonjourToggled);
    connect(m_serviceNameEdit, &QLineEdit::editingFinished,
            this, [this]() { slotServiceNameChanged(m_serviceNameEdit->text()); });
    connect(clearBtn, &QPushButton::clicked,
            this, &ConfigureVdjBridge::slotClearLog);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::close);

    // --- Connections: plugin signals ---
    connect(m_plugin, &VdjBridgePlugin::deckTriggerReceived,
            this, &ConfigureVdjBridge::slotDeckTrigger);
    connect(m_plugin, &VdjBridgePlugin::globalTriggerReceived,
            this, &ConfigureVdjBridge::slotGlobalTrigger);
    connect(m_plugin, &VdjBridgePlugin::telemetryBeatReceived,
            this, &ConfigureVdjBridge::slotBeatReceived);
    connect(m_plugin, &VdjBridgePlugin::clientConnected,
            this, &ConfigureVdjBridge::slotClientConnected);
    connect(m_plugin, &VdjBridgePlugin::clientDisconnected,
            this, &ConfigureVdjBridge::slotClientDisconnected);

    // Initialize status
    updateStatusLabel();
}

// ──────────────────────────────────────────────────────────────────
// UI control slots
// ──────────────────────────────────────────────────────────────────

void ConfigureVdjBridge::slotPortChanged(int value)
{
    m_plugin->setParameter(0, 0, QLCIOPlugin::Input,
                           QStringLiteral(VDJ_HOST_PORT),
                           QVariant(static_cast<quint16>(value)));
    updateStatusLabel();
}

void ConfigureVdjBridge::slotBonjourToggled(bool checked)
{
    m_plugin->setParameter(0, 0, QLCIOPlugin::Input,
                           QStringLiteral(VDJ_BONJOUR_ENABLED),
                           QVariant(checked));
}

void ConfigureVdjBridge::slotServiceNameChanged(const QString &text)
{
    m_plugin->setParameter(0, 0, QLCIOPlugin::Input,
                           QStringLiteral(VDJ_SERVICE_NAME),
                           QVariant(text));
}

void ConfigureVdjBridge::slotClearLog()
{
    m_debugEntries.clear();
    m_debugTable->setRowCount(0);
    m_beatCounter = 0;
}

// ──────────────────────────────────────────────────────────────────
// Plugin signal slots
// ──────────────────────────────────────────────────────────────────

void ConfigureVdjBridge::slotDeckTrigger(int deckIndex, const QString &trigger, const QVariant &value)
{
    const QString source = tr("Deck %1").arg(deckIndex + 1);
    upsertDebugEntry(source, trigger, value.toString());
}

void ConfigureVdjBridge::slotGlobalTrigger(const QString &trigger, const QVariant &value)
{
    upsertDebugEntry(tr("Global"), trigger, value.toString());
}

void ConfigureVdjBridge::slotBeatReceived(int pos, qreal bpm, qreal strength, bool change)
{
    // Throttle: update every 4th beat to avoid flooding
    if (++m_beatCounter % 4 != 0)
        return;

    upsertDebugEntry(tr("Beat"), QStringLiteral("telemetry"),
                     QStringLiteral("pos=%1 bpm=%2 str=%3%4")
                     .arg(pos)
                     .arg(bpm, 0, 'f', 1)
                     .arg(strength, 0, 'f', 2)
                     .arg(change ? QStringLiteral(" [change]") : QString()));
}

void ConfigureVdjBridge::slotClientConnected()
{
    QString addr = m_plugin->clientAddress();
    if (addr.isEmpty())
        upsertDebugEntry(tr("Connection"), QStringLiteral("state"), tr("Connected"));
    else
        upsertDebugEntry(tr("Connection"), QStringLiteral("state"), tr("Connected from %1").arg(addr));
    updateStatusLabel();
}

void ConfigureVdjBridge::slotClientDisconnected()
{
    upsertDebugEntry(tr("Connection"), QStringLiteral("state"), tr("Disconnected"));
    updateStatusLabel();
}

// ──────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────

void ConfigureVdjBridge::upsertDebugEntry(const QString &source, const QString &key, const QString &valueText)
{
    const QString normalizedSource = normalizeText(source);
    const QString normalizedKey = normalizeText(key);
    const QString normalizedValue = normalizeText(valueText);
    const QString entryId = normalizedSource + QChar(0x1f) + normalizedKey;

    DebugEntry &entry = m_debugEntries[entryId];
    if (entry.row < 0)
    {
        entry.row = m_debugTable->rowCount();
        m_debugTable->insertRow(entry.row);
        m_debugTable->setItem(entry.row, 0, new QTableWidgetItem(normalizedSource));
        m_debugTable->setItem(entry.row, 1, new QTableWidgetItem(normalizedKey));
        m_debugTable->setItem(entry.row, 2, new QTableWidgetItem(normalizedValue));
        m_debugTable->setItem(entry.row, 3, new QTableWidgetItem(QStringLiteral("0")));
        m_debugTable->setItem(entry.row, 4, new QTableWidgetItem(QStringLiteral("0")));
    }

    entry.totalCount++;
    const quint64 valueCount = ++entry.valueCounts[normalizedValue];

    m_debugTable->item(entry.row, 2)->setText(normalizedValue);
    m_debugTable->item(entry.row, 3)->setText(QString::number(entry.totalCount));
    m_debugTable->item(entry.row, 4)->setText(QString::number(valueCount));
}

void ConfigureVdjBridge::updateStatusLabel()
{
    int status = m_plugin->connectionStatus(0);
    quint16 port = static_cast<quint16>(m_portSpin->value());

    switch (status)
    {
    case QLCIOPlugin::Connected:
    {
        QString addr = m_plugin->clientAddress();
        if (addr.isEmpty())
            m_statusLabel->setText(tr("Connected (port %1)").arg(port));
        else
            m_statusLabel->setText(tr("Connected from %1 (port %2)").arg(addr).arg(port));
        break;
    }
    case QLCIOPlugin::Advertising:
        m_statusLabel->setText(tr("Listening (port %1)").arg(port));
        break;
    default:
        m_statusLabel->setText(tr("Idle"));
        break;
    }
}

QString ConfigureVdjBridge::normalizeText(const QString &text)
{
    if (text.isEmpty())
        return QStringLiteral("<empty>");
    return text;
}
