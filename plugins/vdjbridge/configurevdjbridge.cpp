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
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextCursor>
#include <QTime>

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

    // --- Debug log ---
    auto *logLabel = new QLabel(tr("Debug log:"), this);
    mainLayout->addWidget(logLabel);

    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(MaxLogLines);
    mainLayout->addWidget(m_logView, 1);

    // --- Buttons ---
    auto *buttonLayout = new QHBoxLayout;
    auto *clearBtn = new QPushButton(tr("Clear Log"), this);
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
    m_logView->clear();
}

// ──────────────────────────────────────────────────────────────────
// Plugin signal slots
// ──────────────────────────────────────────────────────────────────

void ConfigureVdjBridge::slotDeckTrigger(int deckIndex, const QString &trigger, const QVariant &value)
{
    appendLog(QStringLiteral("Deck %1: %2 = %3")
              .arg(deckIndex + 1).arg(trigger, value.toString()));
}

void ConfigureVdjBridge::slotGlobalTrigger(const QString &trigger, const QVariant &value)
{
    appendLog(QStringLiteral("Global: %1 = %2").arg(trigger, value.toString()));
}

void ConfigureVdjBridge::slotBeatReceived(int pos, qreal bpm, qreal strength, bool change)
{
    // Throttle: log every 4th beat to avoid flooding
    if (++m_beatCounter % 4 != 0)
        return;

    appendLog(QStringLiteral("Beat: pos=%1 bpm=%2 str=%3%4")
              .arg(pos)
              .arg(bpm, 0, 'f', 1)
              .arg(strength, 0, 'f', 2)
              .arg(change ? QStringLiteral(" [change]") : QString()));
}

void ConfigureVdjBridge::slotClientConnected()
{
    QString addr = m_plugin->clientAddress();
    if (addr.isEmpty())
        appendLog(QStringLiteral("Client connected"));
    else
        appendLog(QStringLiteral("Client connected from %1").arg(addr));
    updateStatusLabel();
}

void ConfigureVdjBridge::slotClientDisconnected()
{
    appendLog(QStringLiteral("Client disconnected"));
    updateStatusLabel();
}

// ──────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────

void ConfigureVdjBridge::appendLog(const QString &message)
{
    QString ts = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    m_logView->appendPlainText(QStringLiteral("[%1] %2").arg(ts, message));
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

void ConfigureVdjBridge::trimLog()
{
    // Using QPlainTextEdit::setMaximumBlockCount handles this automatically.
    // Kept as a no-op in case manual trimming is needed later.
}
