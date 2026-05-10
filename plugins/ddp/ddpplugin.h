/*
  Q Light Controller Plus
  ddpplugin.h

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

#ifndef DDPPLUGIN_H
#define DDPPLUGIN_H

#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QSharedPointer>
#include <QHostAddress>
#include <QMutex>
#include <QString>
#include <QList>

#include "qlcioplugin.h"
#include "ddpcontroller.h"

typedef struct
{
    QNetworkInterface iface;
    QNetworkAddressEntry address;
    QSharedPointer<DDPController> controller;
} DDPIO;

// Parameter name constants for setParameter() / getParameters()
#define DDP_IP          "ddpIP"
#define DDP_DESTPORT    "ddpPort"
#define DDP_OFFSET      "ddpOffset"
#define DDP_DESTID      "ddpDestId"
#define DDP_TRANSMITMODE "ddpTransmitMode"
#define DDP_COMPONENTS   "ddpComponents"
#define DDP_MAXFPS       "ddpMaxFps"
#define DDP_PIXELCOUNT   "ddpPixelCount"
#define DDP_SKIPUNCHANGED "ddpSkipUnchanged"

class DDPPlugin final : public QLCIOPlugin
{
    Q_OBJECT
    Q_INTERFACES(QLCIOPlugin)
    Q_PLUGIN_METADATA(IID QLCIOPlugin_iid)

    /*********************************************************************
     * Initialization
     *********************************************************************/
public:
    virtual ~DDPPlugin();

    /** @reimp */
    void init() override;

    /** @reimp */
    QString name() const override;

    /** @reimp */
    int capabilities() const override;

    /** @reimp */
    QString pluginInfo() const override;

    /** @reimp */
    QByteArray pluginDiagnostics() const override;

    /*********************************************************************
     * Outputs
     *********************************************************************/
public:
    /** @reimp */
    bool openOutput(quint32 output, quint32 universe) override;

    /** @reimp */
    void closeOutput(quint32 output, quint32 universe) override;

    /** @reimp */
    QStringList outputs() override;

    /** @reimp */
    QString outputInfo(quint32 output) override;

    /** @reimp */
    void writeUniverse(quint32 universe, quint32 output, const QByteArray& data, bool dataChanged) override;

    /*********************************************************************
     * Inputs — not supported
     *********************************************************************/
public:
    /** @reimp */
    bool openInput(quint32 input, quint32 universe) override;

    /** @reimp */
    void closeInput(quint32 input, quint32 universe) override;

    /** @reimp */
    QStringList inputs() override;

    /** @reimp */
    QString inputInfo(quint32 input) override;

    /*********************************************************************
     * Configuration
     *********************************************************************/
public:
    /** @reimp */
    void configure() override;

    /** @reimp */
    bool canConfigure() const override;

    /** @reimp */
    void setParameter(quint32 universe, quint32 line, Capability type, QString name, QVariant value) override;

    /** Get a list of the available I/O lines */
    QList<DDPIO> getIOMapping() const;

private:
    QList<DDPIO> m_IOmapping;
    /** Guards access to controller slots in m_IOmapping. The list itself is
     *  only resized on the engine main thread; the mutex protects the
     *  QSharedPointer slot read/reset against concurrent writeUniverse(). */
    mutable QMutex m_ioMutex;
};

#endif // DDPPLUGIN_H
