/*
  Q Light Controller
  outputpatch.cpp

  Copyright (c) Heikki Junnila

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

#if defined(WIN32) || defined(Q_OS_WIN)
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#else
#   include <unistd.h>
#endif

#include "qlcioplugin.h"
#include "outputpatch.h"
#include "universe.h"

#define GRACE_MS 1

QByteArray OutputSource::blackoutSuppressed(const QByteArray &frame) const
{
    QByteArray suppressed = frame;
    const uchar *capabilities = reinterpret_cast<const uchar *>(mask.constData());

    for (int channel = 0; channel < suppressed.size(); channel++)
    {
        if (channel >= mask.size() || (capabilities[channel] & Universe::HTP))
            suppressed[channel] = char(0);
    }

    return suppressed;
}

/*****************************************************************************
 * Initialization
 *****************************************************************************/

OutputPatch::OutputPatch(QObject* parent)
    : QObject(parent)
    , m_plugin(NULL)
    , m_pluginLine(QLCIOPlugin::invalidLine())
    , m_universe(UINT_MAX)
    , m_paused(false)
    , m_blackout(false)
{
}

OutputPatch::OutputPatch(quint32 universe, QObject* parent)
    : QObject(parent)
    , m_plugin(NULL)
    , m_pluginLine(QLCIOPlugin::invalidLine())
    , m_universe(universe)
    , m_paused(false)
    , m_blackout(false)
{
}

OutputPatch::~OutputPatch()
{
    if (m_plugin != NULL)
        m_plugin->closeOutput(m_pluginLine, m_universe);
}

/****************************************************************************
 * Plugin & Output
 ****************************************************************************/

bool OutputPatch::set(QLCIOPlugin* plugin, quint32 output)
{
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
        m_plugin->closeOutput(m_pluginLine, m_universe);

    m_plugin = plugin;
    m_pluginLine = output;

    if (m_plugin != NULL)
    {
        emit pluginNameChanged();
        if (m_pluginLine != QLCIOPlugin::invalidLine())
            emit outputNameChanged();
    }

    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
        return m_plugin->openOutput(m_pluginLine, m_universe);

    return false;
}

bool OutputPatch::reconnect()
{
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
    {
        m_plugin->closeOutput(m_pluginLine, m_universe);
#if defined(WIN32) || defined(Q_OS_WIN)
        Sleep(GRACE_MS);
#else
        usleep(GRACE_MS * 1000);
#endif
        bool ret = m_plugin->openOutput(m_pluginLine, m_universe);
        if (ret == true)
        {
            QMap<QString, QVariant>::iterator it = m_parametersCache.begin();
            for (; it != m_parametersCache.end(); it++)
                m_plugin->setParameter(m_universe, m_pluginLine, QLCIOPlugin::Output, it.key(), it.value());

            // the reopened line faces a receiver with no cache, so the next
            // published frame has to be reported as a full refresh
            m_heldFrame.clear();
        }
        return ret;
    }
    return false;
}

QString OutputPatch::pluginName() const
{
    if (m_plugin != NULL)
        return m_plugin->name();
    else
        return KOutputNone;
}

QLCIOPlugin* OutputPatch::plugin() const
{
    return m_plugin;
}

QString OutputPatch::outputName() const
{
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine() &&
        m_pluginLine < quint32(m_plugin->outputs().size()))
    {
        return m_plugin->outputs()[m_pluginLine];
    }
    else
    {
        return KOutputNone;
    }
}

QString OutputPatch::outputUID() const
{
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine() &&
        m_pluginLine < quint32(m_plugin->outputs().size()))
    {
        return m_plugin->outputsUID()[m_pluginLine];
    }
    else
    {
        return KOutputNone;
    }
}

quint32 OutputPatch::output() const
{
    return m_pluginLine;
}

bool OutputPatch::isPatched() const
{
    return output() != QLCIOPlugin::invalidLine() && m_plugin != NULL;
}

void OutputPatch::setPluginParameter(QString prop, QVariant value)
{
    m_parametersCache[prop] = value;
    if (m_plugin != NULL)
        m_plugin->setParameter(m_universe, m_pluginLine, QLCIOPlugin::Output, prop, value);
}

QMap<QString, QVariant> OutputPatch::getPluginParameters()
{
    if (m_plugin != NULL)
        return m_plugin->getParameters(m_universe, m_pluginLine, QLCIOPlugin::Output);

    return QMap<QString, QVariant>();
}

/*****************************************************************************
 * Value dump
 *****************************************************************************/
bool OutputPatch::paused() const
{
    return m_paused;
}

void OutputPatch::setPaused(bool paused)
{
    if (m_paused == paused)
        return;

    m_paused = paused;

    if (m_pauseBuffer.length())
        m_pauseBuffer.clear();

    emit pausedChanged(m_paused);
}

bool OutputPatch::blackout() const
{
    return m_blackout;
}

void OutputPatch::setBlackout(bool blackout)
{
    if (m_blackout == blackout)
        return;

    m_blackout = blackout;
    emit blackoutChanged(m_blackout);
}

void OutputPatch::dump(quint32 universe, const QByteArray& data, bool dataChanged,
                       const OutputSource &source)
{
    /* Don't do anything if there is no plugin and/or output line. */
    if (m_plugin != NULL && m_pluginLine != QLCIOPlugin::invalidLine())
    {
        if (m_paused)
        {
            if (m_pauseBuffer.isNull())
            {
                m_pauseBuffer.append(data);
                // remember where the buffered frame came from, so a later
                // freeze holds the paused look and not the live one
                m_source = source;
            }

            // The pause buffer is kept unblackened so it can be restored
            // intact, but a paused output must not defeat Blackout.
            const QByteArray frame = m_blackout ? m_source.blackoutSuppressed(m_pauseBuffer)
                                                : m_pauseBuffer;

            m_plugin->writeUniverse(universe, m_pluginLine, frame, dataChanged);
        }
        else
        {
            m_source = source;
            m_plugin->writeUniverse(universe, m_pluginLine, data, dataChanged);
        }
    }
}

void OutputPatch::dumpHeld(quint32 universe, const QByteArray &data,
                           const QByteArray &unblackened, bool forceChanged)
{
    if (m_plugin == NULL || m_pluginLine == QLCIOPlugin::invalidLine())
        return;

    // A pause requested while the look is held pins the frame on stage, so
    // thaw resumes that instead of whatever the producers reached meanwhile.
    // The buffer is stored unblackened, like every other pause capture.
    if (m_paused && m_pauseBuffer.isNull())
        m_pauseBuffer.append(unblackened);

    const bool dataChanged = forceChanged || data != m_heldFrame;
    m_heldFrame = data;

    m_plugin->writeUniverse(universe, m_pluginLine, data, dataChanged);
}

const OutputSource &OutputPatch::heldSource() const
{
    return m_source;
}

void OutputPatch::releasePublicationHistory()
{
    m_source = OutputSource();
    m_heldFrame.clear();
    m_pauseBuffer.clear();
}
