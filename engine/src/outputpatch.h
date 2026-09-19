/*
  Q Light Controller
  outputpatch.h

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

#ifndef OUTPUTPATCH_H
#define OUTPUTPATCH_H

#include <QByteArray>
#include <QVector>
#include <QObject>
#include <QHash>
#include <QMap>

#include "channelmodifier.h"

class QLCIOPlugin;

/** @addtogroup engine Engine
 * @{
 */

/**
 * Everything needed to render a submitted DMX frame again at a later time.
 *
 * The published bytes alone are not enough: a zero or clamped Grand Master
 * destroys information that a later Grand Master change has to bring back.
 * All arrays are implicitly shared copies of the Universe buffers, so
 * retaining one only costs a reference until the Universe writes again.
 */
struct OutputSource
{
    /** Number of channels the frame covered */
    ushort usedChannels = 0;
    /** Channel values before Grand Master, modifiers and passthrough */
    QByteArray preGM;
    /** Preserved non-HTP values, published instead of the look during blackout */
    QByteArray blackout;
    /** Channel capability mask, needed by the Grand Master Intensity mode */
    QByteArray mask;
    /** Passthrough input values. Empty when the universe was not in passthrough */
    QByteArray passthrough;
    /** The channel modifiers that were in effect, held by value: a modifier
     *  object can be edited in place, so a pointer would not be a snapshot */
    QHash<int, ChannelModifier> modifiers;

    /**
     * Zero the channel classes Blackout suppresses, keeping the rest.
     *
     * Applied to an already composed frame, so the retained values are the
     * rendered ones rather than the raw pre-GM blackout buffer. A channel
     * with no captured capability is suppressed: an unclassified channel
     * must not keep emitting light.
     */
    QByteArray blackoutSuppressed(const QByteArray &frame) const;

    bool isValid() const
    {
        return usedChannels > 0 && preGM.size() >= int(usedChannels)
               && mask.size() >= int(usedChannels);
    }
};

#define KOutputNone QObject::tr("None")

#define KXMLQLCOutputPatch          QStringLiteral("Patch")
#define KXMLQLCOutputPatchUniverse  QStringLiteral("Universe")
#define KXMLQLCOutputPatchPlugin    QStringLiteral("Plugin")
#define KXMLQLCOutputPatchOutput    QStringLiteral("Output")

class OutputPatch : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(OutputPatch)

    Q_PROPERTY(QString outputName READ outputName NOTIFY outputNameChanged)
    Q_PROPERTY(QString pluginName READ pluginName NOTIFY pluginNameChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool blackout READ blackout WRITE setBlackout NOTIFY blackoutChanged)

    /********************************************************************
     * Initialization
     ********************************************************************/
public:
    OutputPatch(QObject* parent = 0);
    OutputPatch(quint32 universe, QObject* parent = 0);
    virtual ~OutputPatch();

    /********************************************************************
     * Plugin & output
     ********************************************************************/
public:
    /**
     * Set the plugin to use and the plugin line number to output data on
     */
    bool set(QLCIOPlugin* plugin, quint32 output);

    /**
     * If a valid plugin and line have been set, close
     * the output line and re-open it again
     */
    bool reconnect();

    /** The plugin instance that has been assigned to a patch */
    QLCIOPlugin* plugin() const;

    /** Friendly name of the plugin assigned to a patch ("None" if none) */
    QString pluginName() const;

    /** An output line provided by the assigned plugin */
    quint32 output() const;

    /** Friendly name of the assigned output line */
    QString outputName() const;

    /** Stable UID of the assigned output line, used for project XML persistence */
    QString outputUID() const;

    /** Returns true if a valid plugin line has been set */
    bool isPatched() const;

    /** Set a parameter specific to the patched plugin */
    void setPluginParameter(QString prop, QVariant value);

    /** Retrieve the map of custom parameters set to the patched plugin */
    QMap<QString, QVariant> getPluginParameters();

signals:
    void outputNameChanged();
    void pluginNameChanged();

private:
    /** The reference of the plugin associated by this Output patch */
    QLCIOPlugin* m_plugin;
    /** The plugin line open by this Output patch */
    quint32 m_pluginLine;
    /** The universe that this Output patch is attached to */
    quint32 m_universe;
    /** The patch parameters cache */
    QMap<QString, QVariant>m_parametersCache;

    /********************************************************************
     * Value dump
     ********************************************************************/
public:
    /** Get/Set the output patch pause state */
    bool paused() const;
    void setPaused(bool paused);

    /** Get/Set the output patch blackout state */
    bool blackout() const;
    void setBlackout(bool blackout);

    /** Write the contents of a 512 channel value buffer to the plugin.
      * Called periodically by OutputMap. No need to call manually.
      * $source describes how $data was composed, so a later global freeze
      * can render the same look again with the live Grand Master. */
    void dump(quint32 universe, const QByteArray &data, bool dataChanged,
              const OutputSource &source = OutputSource());

    /** Write a frame composed outside the normal pipeline, while the
      * workspace-global freeze holds the look. The held frame already
      * reflects this patch's pause state, so the pause buffer is bypassed.
      * $unblackened is the same frame before blackout suppression: a pause
      * requested while frozen pins that, so thaw resumes the frame the
      * operator could see rather than a later live one.
      * The change flag is derived from the effective bytes, plus $forceChanged
      * for the transitions that a byte comparison cannot see. */
    void dumpHeld(quint32 universe, const QByteArray &data,
                  const QByteArray &unblackened, bool forceChanged);

    /** The source of the frame this patch last published */
    const OutputSource &heldSource() const;

    /** Forget every trace of what this patch published, including the legacy
      * pause bytes. A re-patched line or a replaced workspace has no history
      * of its own and must not republish a look that belonged to the previous
      * output. The pause flag itself is a user setting and is left alone. */
    void releasePublicationHistory();

signals:
    void pausedChanged(bool paused);
    void blackoutChanged(bool blackout);

private:
    /** A buffer used when this output patch is paused */
    QByteArray m_pauseBuffer;
    bool m_paused;
    bool m_blackout;
    /** The source of the frame this patch last published */
    OutputSource m_source;
    /** The last frame published while the global freeze was active */
    QByteArray m_heldFrame;
};

/** @} */

#endif
