/*
  Q Light Controller Plus
  vdjbridge.cpp

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

#include "vdjbridge.h"
#include "qlcioplugin.h"

#include <QDebug>

VdjBridge::VdjBridge(QObject *parent)
    : QObject(parent)
{
}

void VdjBridge::attachOS2LPlugin(QLCIOPlugin *plugin)
{
    if (m_plugin == plugin)
        return;

    if (!m_plugin.isNull())
        m_plugin->disconnect(this);

    m_plugin = plugin;
    if (m_plugin.isNull())
    {
        if (m_connected)
        {
            m_connected = false;
            emit connectedChanged();
        }
        return;
    }

    // String-based connections so qmlui does not need to link the plugin
    // shared library. Signatures must match exactly what the plugin emits.
    connect(m_plugin.data(), SIGNAL(songReceived(QVariantMap)),
            this, SLOT(onSongReceived(QVariantMap)));
    connect(m_plugin.data(), SIGNAL(beatInfoReceived(double,double,bool)),
            this, SLOT(onBeatInfo(double,double,bool)));
    connect(m_plugin.data(), SIGNAL(connectionStatusChanged(quint32,quint32)),
            this, SLOT(refreshConnectionStatus()));

    refreshConnectionStatus();
}

void VdjBridge::refreshConnectionStatus()
{
    bool now = false;
    if (!m_plugin.isNull())
        now = (m_plugin->connectionStatus(0) == QLCIOPlugin::Connected);

    if (now != m_connected)
    {
        m_connected = now;
        emit connectedChanged();
    }
}

void VdjBridge::onBeatInfo(double bpm, double pos, bool change)
{
    if (bpm > 0.0)
        m_bpm = bpm;
    m_beatPos = pos;
    // OS2L "change=true" marks the first beat of a new track/loop; reset the
    // running counter so the UI can display "beat N of 4" against the current
    // segment without drifting.
    if (change)
        m_beatCount = 0;
    ++m_beatCount;

    // The first beat we receive is the most reliable evidence VDJ is actually
    // streaming to us. connectionStatusChanged covers the TCP handshake but
    // not every transport (e.g. dropped + re-routed Bonjour entries).
    if (!m_connected)
    {
        m_connected = true;
        emit connectedChanged();
    }

    emit beatChanged();
}

void VdjBridge::onSongReceived(QVariantMap song)
{
    // Detect whether this is a real song change (vs. just an elapsed-time
    // update for the same track) so the Song Manager doesn't re-run its
    // create/lookup pipeline on every status-only event.
    const QString oldName = currentSongName();
    const QString oldPath = currentSongPath();
    const QString newName = song.value("name").toString();
    const QString newPath = song.value("path").toString();

    const bool sameTrack = (!oldName.isEmpty() && oldName == newName) ||
                           (!oldPath.isEmpty() && oldPath == newPath);

    m_currentSong = song;

    if (sameTrack)
    {
        emit songElapsedChanged();
    }
    else
    {
        if (song.value("bpm").toDouble() > 0.0)
            m_bpm = song.value("bpm").toDouble();
        emit songChanged();
        emit beatChanged();
    }

    if (!m_connected)
    {
        m_connected = true;
        emit connectedChanged();
    }
}
