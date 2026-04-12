/*
  Q Light Controller Plus
  configureddp.cpp

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

#include <QTreeWidgetItem>
#include <QMessageBox>
#include <QSettings>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDebug>

#include "configureddp.h"
#include "ddpplugin.h"

#define KColumnInterface    0
#define KColumnUniverse     1
#define KColumnIPAddress    2
#define KColumnPort         3
#define KColumnOffset       4
#define KColumnDestId       5
#define KColumnTransmitMode 6
#define KColumnComponents   7

#define PROP_UNIVERSE (Qt::UserRole + 0)
#define PROP_LINE     (Qt::UserRole + 1)

#define SETTINGS_GEOMETRY "configureddp/geometry"

/*****************************************************************************
 * Initialization
 *****************************************************************************/

ConfigureDDP::ConfigureDDP(DDPPlugin* plugin, QWidget* parent)
    : QDialog(parent)
{
    Q_ASSERT(plugin != nullptr);
    m_plugin = plugin;

    setupUi(this);
    fillMappingTree();

    QSettings settings;
    QVariant geometrySettings = settings.value(SETTINGS_GEOMETRY);
    if (geometrySettings.isValid())
        restoreGeometry(geometrySettings.toByteArray());
}

ConfigureDDP::~ConfigureDDP()
{
    QSettings settings;
    settings.setValue(SETTINGS_GEOMETRY, saveGeometry());
}

void ConfigureDDP::fillMappingTree()
{
    QTreeWidgetItem* outputItem = nullptr;

    QList<DDPIO> IOmap = m_plugin->getIOMapping();
    foreach (DDPIO io, IOmap)
    {
        DDPController *controller = io.controller;
        if (controller == nullptr)
            continue;

        if (outputItem == nullptr)
        {
            outputItem = new QTreeWidgetItem(m_uniMapTree);
            outputItem->setText(KColumnInterface, tr("Outputs"));
            outputItem->setExpanded(true);
        }

        foreach (quint32 universe, controller->universesList())
        {
            DDPUniverseInfo *info = controller->getUniverseInfo(universe);
            if (info == nullptr)
                continue;

            QTreeWidgetItem *item = new QTreeWidgetItem(outputItem);
            item->setData(KColumnInterface, PROP_UNIVERSE, universe);
            item->setData(KColumnInterface, PROP_LINE, controller->line());

            item->setText(KColumnInterface, controller->getNetworkIP());
            item->setText(KColumnUniverse, QString::number(universe + 1));
            item->setTextAlignment(KColumnUniverse, Qt::AlignHCenter | Qt::AlignVCenter);

            // IP Address
            QLineEdit *ipEdit = new QLineEdit(info->destAddress.toString(), this);
            m_uniMapTree->setItemWidget(item, KColumnIPAddress, ipEdit);

            // Port
            QSpinBox *portSpin = new QSpinBox(this);
            portSpin->setRange(0, 0xFFFF);
            portSpin->setValue(info->destPort);
            m_uniMapTree->setItemWidget(item, KColumnPort, portSpin);

            // DDP Offset (byte offset into device pixel buffer)
            QSpinBox *offsetSpin = new QSpinBox(this);
            offsetSpin->setRange(0, 999999);
            offsetSpin->setValue(static_cast<int>(info->ddpOffset));
            offsetSpin->setToolTip(tr("Byte offset into the device's pixel buffer"));
            m_uniMapTree->setItemWidget(item, KColumnOffset, offsetSpin);

            // Destination ID
            QSpinBox *destIdSpin = new QSpinBox(this);
            destIdSpin->setRange(1, 255);
            destIdSpin->setValue(info->destId);
            destIdSpin->setToolTip(tr("DDP destination ID (1 = default for WLED)"));
            m_uniMapTree->setItemWidget(item, KColumnDestId, destIdSpin);

            // Transmission Mode
            QComboBox *transCombo = new QComboBox(this);
            transCombo->addItem(tr("Full"));
            transCombo->addItem(tr("Partial"));
            if (info->transmissionMode == DDPController::Partial)
                transCombo->setCurrentIndex(1);
            m_uniMapTree->setItemWidget(item, KColumnTransmitMode, transCombo);

            // Components (RGB / RGBW)
            QComboBox *compCombo = new QComboBox(this);
            compCombo->addItem(tr("RGB"));
            compCombo->addItem(tr("RGBW"));
            if (info->components == DDPController::RGBW)
                compCombo->setCurrentIndex(1);
            compCombo->setToolTip(tr("RGB = 3 bytes/pixel (480 max/packet), RGBW = 4 bytes/pixel (360 max/packet)"));
            m_uniMapTree->setItemWidget(item, KColumnComponents, compCombo);
        }
    }

    m_uniMapTree->header()->resizeSections(QHeaderView::ResizeToContents);
}

void ConfigureDDP::showIPAlert(const QString &ip)
{
    QMessageBox::critical(this, tr("Invalid IP"),
        tr("%1 is not a valid IP.\nPlease fix it before confirming.").arg(ip));
}

/*****************************************************************************
 * Dialog actions
 *****************************************************************************/

void ConfigureDDP::accept()
{
    for (int i = 0; i < m_uniMapTree->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *topItem = m_uniMapTree->topLevelItem(i);
        for (int c = 0; c < topItem->childCount(); c++)
        {
            QTreeWidgetItem *item = topItem->child(c);
            if (!item->data(KColumnInterface, PROP_UNIVERSE).isValid())
                continue;

            quint32 universe = item->data(KColumnInterface, PROP_UNIVERSE).toUInt();
            quint32 line = item->data(KColumnInterface, PROP_LINE).toUInt();

            // IP Address
            QLineEdit *ipEdit = qobject_cast<QLineEdit*>(
                m_uniMapTree->itemWidget(item, KColumnIPAddress));
            QHostAddress addr(ipEdit->text());
            if (addr.isNull() && ipEdit->text() != "255.255.255.255")
            {
                showIPAlert(ipEdit->text());
                return;
            }
            m_plugin->setParameter(universe, line, QLCIOPlugin::Output,
                DDP_IP, ipEdit->text());

            // Port
            QSpinBox *portSpin = qobject_cast<QSpinBox*>(
                m_uniMapTree->itemWidget(item, KColumnPort));
            m_plugin->setParameter(universe, line, QLCIOPlugin::Output,
                DDP_DESTPORT, portSpin->value());

            // DDP Offset
            QSpinBox *offsetSpin = qobject_cast<QSpinBox*>(
                m_uniMapTree->itemWidget(item, KColumnOffset));
            m_plugin->setParameter(universe, line, QLCIOPlugin::Output,
                DDP_OFFSET, offsetSpin->value());

            // Destination ID
            QSpinBox *destIdSpin = qobject_cast<QSpinBox*>(
                m_uniMapTree->itemWidget(item, KColumnDestId));
            m_plugin->setParameter(universe, line, QLCIOPlugin::Output,
                DDP_DESTID, destIdSpin->value());

            // Transmission Mode
            QComboBox *transCombo = qobject_cast<QComboBox*>(
                m_uniMapTree->itemWidget(item, KColumnTransmitMode));
            DDPController::TransmissionMode mode =
                (transCombo->currentIndex() == 1) ? DDPController::Partial : DDPController::Full;
            m_plugin->setParameter(universe, line, QLCIOPlugin::Output,
                DDP_TRANSMITMODE, DDPController::transmissionModeToString(mode));

            // Components (RGB / RGBW)
            QComboBox *compCombo = qobject_cast<QComboBox*>(
                m_uniMapTree->itemWidget(item, KColumnComponents));
            DDPController::Components comp =
                (compCombo->currentIndex() == 1) ? DDPController::RGBW : DDPController::RGB;
            m_plugin->setParameter(universe, line, QLCIOPlugin::Output,
                DDP_COMPONENTS, DDPController::componentsToString(comp));
        }
    }

    QDialog::accept();
}

int ConfigureDDP::exec()
{
    return QDialog::exec();
}
