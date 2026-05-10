/*
  Q Light Controller Plus
  ddpplugin.cpp

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

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "ddpplugin.h"
#include "configureddp.h"

static bool addressCompare(const DDPIO &v1, const DDPIO &v2)
{
    return v1.address.ip().toString() < v2.address.ip().toString();
}

DDPPlugin::~DDPPlugin() = default;

void DDPPlugin::init()
{
    QMutexLocker locker(&m_ioMutex);
    m_IOmapping.clear();
    foreach (QNetworkInterface iface, QNetworkInterface::allInterfaces())
    {
        foreach (QNetworkAddressEntry entry, iface.addressEntries())
        {
            QHostAddress addr = entry.ip();
            if (addr.protocol() != QAbstractSocket::IPv6Protocol)
            {
                DDPIO tmpIO;
                tmpIO.iface = iface;
                tmpIO.address = entry;
                tmpIO.controller.reset();

                bool alreadyInList = false;
                for (int j = 0; j < m_IOmapping.count(); j++)
                {
                    if (m_IOmapping.at(j).address == tmpIO.address)
                    {
                        alreadyInList = true;
                        break;
                    }
                }
                if (!alreadyInList)
                    m_IOmapping.append(tmpIO);
            }
        }
    }
    std::sort(m_IOmapping.begin(), m_IOmapping.end(), addressCompare);
}

QString DDPPlugin::name() const
{
    return QString("DDP");
}

int DDPPlugin::capabilities() const
{
    return QLCIOPlugin::Output | QLCIOPlugin::Infinite;
}

QString DDPPlugin::pluginInfo() const
{
    QString str;

    str += QString("<HTML>");
    str += QString("<HEAD>");
    str += QString("<TITLE>%1</TITLE>").arg(name());
    str += QString("</HEAD>");
    str += QString("<BODY>");

    str += QString("<P>");
    str += QString("<H3>%1</H3>").arg(name());
    str += tr("This plugin provides DMX output for devices supporting "
              "the DDP (Distributed Display Protocol), such as WLED controllers.");
    str += QString("</P>");

    return str;
}

QByteArray DDPPlugin::pluginDiagnostics() const
{
    if (!isDiagnosticsEnabled())
        return QByteArray();

    QList<DDPIO> mapping;
    {
        QMutexLocker locker(&m_ioMutex);
        mapping = m_IOmapping;
    }

    QJsonObject root;
    root["type"] = name();

    quint64 totalSent = 0;
    QJsonArray controllersArr;

    for (const DDPIO &io : mapping)
    {
        QJsonObject ctrlObj;
        ctrlObj["interface"] = io.iface.humanReadableName();
        ctrlObj["address"] = io.address.ip().toString();

        if (io.controller)
        {
            quint64 sent = io.controller->getPacketSentNumber();
            ctrlObj["packetsSent"] = (qint64)sent;
            totalSent += sent;
        }
        controllersArr.append(ctrlObj);
    }

    root["controllers"] = controllersArr;
    root["totalPacketsSent"] = (qint64)totalSent;
    root["lines"] = mapping.count();

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

/*********************************************************************
 * Outputs
 *********************************************************************/
QStringList DDPPlugin::outputs()
{
    QStringList list;

    QList<DDPIO> mapping;
    {
        QMutexLocker locker(&m_ioMutex);
        mapping = m_IOmapping;
    }

    for (const DDPIO &line : mapping)
        list << line.address.ip().toString();

    return list;
}

QString DDPPlugin::outputInfo(quint32 output)
{
    if (output >= (quint32)m_IOmapping.length())
        return QString();

    QString str;

    str += QString("<H3>%1 %2</H3>").arg(tr("Output")).arg(outputs()[output]);
    str += QString("<P>");
    QSharedPointer<DDPController> ctrl;
    {
        QMutexLocker locker(&m_ioMutex);
        ctrl = m_IOmapping.at(output).controller;
    }
    if (ctrl.isNull())
    {
        str += tr("Status: Not open");
    }
    else
    {
        str += tr("Status: Open");
        str += QString("<BR>");
        str += tr("Packets sent: ");
        str += QString("%1").arg(ctrl->getPacketSentNumber());
    }
    str += QString("</P>");
    str += QString("</BODY>");
    str += QString("</HTML>");

    return str;
}

bool DDPPlugin::openOutput(quint32 output, quint32 universe)
{
    if (output >= (quint32)m_IOmapping.length())
    {
        qDebug() << "[DDP] cannot open line" << output
                 << "(available:" << m_IOmapping.length() << ")";
        return false;
    }

    qDebug() << "[DDP] Open output on" << m_IOmapping.at(output).address.ip().toString();

    QSharedPointer<DDPController> controller;
    {
        QMutexLocker locker(&m_ioMutex);
        if (m_IOmapping[output].controller.isNull())
        {
            // No QObject parent — lifetime is managed by QSharedPointer
            // with deleteLater() to ensure destruction on the correct thread.
            m_IOmapping[output].controller = QSharedPointer<DDPController>(
                new DDPController(
                    m_IOmapping.at(output).iface,
                    m_IOmapping.at(output).address,
                    output, nullptr),
                [](DDPController *c) { c->deleteLater(); });
        }
        controller = m_IOmapping[output].controller;
    }

    controller->addUniverse(universe);
    addToMap(universe, output, Output);

    return true;
}

void DDPPlugin::closeOutput(quint32 output, quint32 universe)
{
    if (output >= (quint32)m_IOmapping.length())
        return;

    removeFromMap(universe, output, Output);

    QSharedPointer<DDPController> controller;
    {
        QMutexLocker locker(&m_ioMutex);
        controller = m_IOmapping.at(output).controller;
    }

    if (!controller.isNull())
    {
        controller->removeUniverse(universe);
        if (controller->universesList().isEmpty())
        {
            // Drop the slot's strong ref. Any in-flight writeUniverse() still
            // holds its own copy and will keep the controller alive until it
            // returns; the controller is then destroyed safely.
            QMutexLocker locker(&m_ioMutex);
            m_IOmapping[output].controller.reset();
        }
    }
}

void DDPPlugin::writeUniverse(quint32 universe, quint32 output,
                               const QByteArray &data, bool dataChanged)
{
    if (output >= (quint32)m_IOmapping.count())
        return;

    // Copy the QSharedPointer under m_ioMutex, release the mutex, then call
    // sendDmx on the local copy. This keeps the controller alive for the
    // duration of the call even if closeOutput() resets the slot mid-way,
    // and avoids holding m_ioMutex across a (potentially blocking) socket
    // write.
    QSharedPointer<DDPController> controller;
    {
        QMutexLocker locker(&m_ioMutex);
        controller = m_IOmapping[output].controller;
    }
    if (!controller.isNull())
        controller->sendDmx(universe, data, dataChanged);
}

/*********************************************************************
 * Inputs — not supported
 *********************************************************************/
QStringList DDPPlugin::inputs()
{
    return QStringList();
}

bool DDPPlugin::openInput(quint32 input, quint32 universe)
{
    Q_UNUSED(input)
    Q_UNUSED(universe)
    return false;
}

void DDPPlugin::closeInput(quint32 input, quint32 universe)
{
    Q_UNUSED(input)
    Q_UNUSED(universe)
}

QString DDPPlugin::inputInfo(quint32 input)
{
    Q_UNUSED(input)
    return QString();
}

/*********************************************************************
 * Configuration
 *********************************************************************/
void DDPPlugin::configure()
{
    ConfigureDDP conf(this);
    conf.exec();
}

bool DDPPlugin::canConfigure() const
{
    return true;
}

void DDPPlugin::setParameter(quint32 universe, quint32 line, Capability type,
                              QString name, QVariant value)
{
    // Persist into the base class first, so workspace XML stays in sync even
    // when the controller for this line hasn't been opened yet.
    QLCIOPlugin::setParameter(universe, line, type, name, value);

    if (line >= (quint32)m_IOmapping.length())
        return;

    QSharedPointer<DDPController> controller;
    {
        QMutexLocker locker(&m_ioMutex);
        controller = m_IOmapping.at(line).controller;
    }
    if (controller.isNull())
        return;

    if (type == Output)
    {
        if (name == DDP_IP)
            controller->setDestAddress(universe, value.toString());
        else if (name == DDP_DESTPORT)
            controller->setDestPort(universe, value.toUInt());
        else if (name == DDP_OFFSET)
            controller->setDDPOffset(universe, value.toUInt());
        else if (name == DDP_DESTID)
            controller->setDestId(universe, value.toUInt());
        else if (name == DDP_TRANSMITMODE)
            controller->setTransmissionMode(universe,
                DDPController::stringToTransmissionMode(value.toString()));
        else if (name == DDP_COMPONENTS)
        {
            DDPController::Components comp =
                DDPController::stringToComponents(value.toString());
            controller->setComponents(universe, comp);
        }
        else if (name == DDP_MAXFPS)
            controller->setMaxFps(value.toInt());
        else if (name == DDP_PIXELCOUNT)
            controller->setPixelCount(value.toInt());
        else if (name == DDP_SKIPUNCHANGED)
            controller->setSkipUnchanged(value.toBool());
        else
            qWarning() << Q_FUNC_INFO << name << "is not a valid DDP output parameter";
    }
}

QList<DDPIO> DDPPlugin::getIOMapping() const
{
    QMutexLocker locker(&m_ioMutex);
    return m_IOmapping;
}
