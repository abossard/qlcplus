/*
  Q Light Controller Plus
  universe.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QElapsedTimer>
#include <QDebug>
#include <algorithm>
#include <math.h>

#include "channelmodifier.h"
#include "inputoutputmap.h"
#include "genericfader.h"
#include "qlcioplugin.h"
#include "outputpatch.h"
#include "grandmaster.h"
#include "mastertimer.h"
#include "inputpatch.h"
#include "qlcmacros.h"
#include "universe.h"
#include "function.h"
#include "qlcfile.h"
#include "utils.h"

#define RELATIVE_ZERO_8BIT   0x7F
#define RELATIVE_ZERO_16BIT  0x7F00

#define KXMLUniverseNormalBlend      QStringLiteral("Normal")
#define KXMLUniverseMaskBlend        QStringLiteral("Mask")
#define KXMLUniverseAdditiveBlend    QStringLiteral("Additive")
#define KXMLUniverseSubtractiveBlend QStringLiteral("Subtractive")

Universe::Universe(quint32 id, GrandMaster *gm, QObject *parent)
    : QThread(parent)
    , m_id(id)
    , m_name(QString("Universe %1").arg(id + 1))
    , m_grandMaster(gm)
    , m_passthrough(false)
    , m_monitor(false)
    , m_frozen(false)
    , m_forceDumpChanged(false)
    , m_inputPatch(NULL)
    , m_fbPatch(NULL)
    , m_channelsMask(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_modifiedZeroValues(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_modifierCount(0)
    , m_running(false)
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    , m_fadersMutex(QMutex::Recursive)
#endif
    , m_usedChannels(0)
    , m_totalChannels(0)
    , m_totalChannelsChanged(false)
    , m_intensityChannelsChanged(false)
    , m_preGMValues(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_postGMValues(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_lastPostGMValues(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_blackoutValues(new QByteArray(UNIVERSE_SIZE, char(0)))
    , m_passthroughValues()
{
    m_modifiers.fill(NULL, UNIVERSE_SIZE);

    connect(m_grandMaster, SIGNAL(valueChanged(uchar)),
            this, SLOT(slotGMValueChanged()));
}

Universe::~Universe()
{
    if (isRunning() == true)
    {
        // isRunning is inconsistent with m_running,
        // so double check if the thread is really in the run loop
        while (m_running == false)
            usleep(10000);

        m_running = false;
        if (!wait(2000))
        {
            qCritical() << Q_FUNC_INFO << "Universe" << m_id
                        << "thread did not stop within 2 seconds, forcing termination";
            terminate();
            wait();
        }
    }

    delete m_inputPatch;
    int opCount = m_outputPatchList.count();
    for (int i = 0; i < opCount; i++)
    {
        OutputPatch *patch = m_outputPatchList.takeLast();
        delete patch;
    }
    delete m_fbPatch;
}

void Universe::setName(QString name)
{
    if (name.isEmpty())
        m_name = QString("Universe %1").arg(m_id + 1);
    else
        m_name = name;
    emit nameChanged();
}

QString Universe::name() const
{
    return m_name;
}

void Universe::setID(quint32 id)
{
    m_id = id;
}

quint32 Universe::id() const
{
    return m_id;
}

ushort Universe::usedChannels() const
{
    return m_usedChannels;
}

ushort Universe::totalChannels() const
{
    return m_totalChannels;
}

bool Universe::hasChanged() const
{
    bool changed =
        memcmp(m_lastPostGMValues->constData(), m_postGMValues->constData(), m_usedChannels) != 0;
    if (changed)
        memcpy(m_lastPostGMValues->data(), m_postGMValues->constData(), m_usedChannels);
    return changed;
}

void Universe::setPassthrough(bool enable)
{
    if (enable == m_passthrough)
        return;

    qDebug() << "Set universe" << id() << "passthrough to" << enable;

    disconnectInputPatch();

    {
        // guards the short snapshot copy in currentSource() against this
        // buffer being replaced underneath it
        QMutexLocker outputLocker(&m_outputMutex);

        if (enable && m_passthroughValues.isNull())
        {
            // When passthrough is disabled, we don't release the array, since it's only ~512 B and
            // we would have to synchronize with other threads

            // When enabling passthrough, make sure the array is allocated BEFORE m_passthrough is set to
            // true. That way we only have to check for m_passthrough, and do not need to check
            // m_passthroughValues.isNull()
            m_passthroughValues.reset(new QByteArray(UNIVERSE_SIZE, char(0)));
        }

        m_passthrough = enable;
    }

    connectInputPatch();

    emit passthroughChanged();
}

bool Universe::passthrough() const
{
    return m_passthrough;
}

void Universe::setMonitor(bool enable)
{
    m_monitor = enable;
}

bool Universe::monitor() const
{
    return m_monitor;
}

void Universe::setFrozen(bool frozen)
{
    QMutexLocker outputLocker(&m_outputMutex);

    /* Repeated requests must not re-capture the held look */
    if (m_frozen == frozen)
        return;

    m_frozen = frozen;
    m_forceDumpChanged = true;

    if (frozen == false)
    {
        // The Grand Master channel mode may have changed while the look was
        // held. The live post-GM buffer is only refreshed for the channel
        // classes the mode covers, so recompose before publishing live again,
        // otherwise thaw would step back to a stale non-intensity value.
        for (int channel = 0; channel < int(m_usedChannels); channel++)
            updatePostGMValue(channel);
    }
}

bool Universe::frozen() const
{
    return m_frozen;
}

void Universe::slotGMValueChanged()
{
    {
        for (int i = 0; i < m_intensityChannels.size(); ++i)
        {
            int channel = m_intensityChannels.at(i);
            updatePostGMValue(channel);
        }
    }

    if (m_grandMaster->channelMode() == GrandMaster::AllChannels)
    {
        for (int i = 0; i < m_nonIntensityChannels.size(); ++i)
        {
            int channel = m_nonIntensityChannels.at(i);
            updatePostGMValue(channel);
        }
    }
}

/************************************************************************
 * Faders
 ************************************************************************/

QSharedPointer<GenericFader> Universe::requestFader(Universe::FaderPriority priority)
{
    int insertPos = 0;
    QSharedPointer<GenericFader> fader = QSharedPointer<GenericFader>(new GenericFader());
    fader->setPriority(priority);

    {
        QMutexLocker fadersLocker(&m_fadersMutex);
        {
            for (int i = m_faders.count() - 1; i >= 0; i--)
            {
                const QSharedPointer<GenericFader>& f = m_faders.at(i);
                if (!f.isNull() && f->priority() <= fader->priority())
                {
                    insertPos = i + 1;
                    break;
                }
            }
        }

        m_faders.insert(insertPos, fader);

        qDebug() << "[Universe]" << id() << ": Generic fader with priority" << fader->priority()
                 << "registered at pos" << insertPos << ", count" << m_faders.count();
    }
    return fader;
}

void Universe::dismissFader(QSharedPointer<GenericFader> fader)
{
    QMutexLocker fadersLocker(&m_fadersMutex);
    int index = m_faders.indexOf(fader);
    if (index >= 0)
    {
        m_faders.takeAt(index);
        fader.clear();
    }
}

void Universe::requestFaderPriority(QSharedPointer<GenericFader> fader, Universe::FaderPriority priority)
{
    QMutexLocker fadersLocker(&m_fadersMutex);
    int pos = m_faders.indexOf(fader);
    int newPos = 0;

    if (pos == -1)
        return;

    for (int i = m_faders.count() - 1; i >= 0; i--)
    {
        QSharedPointer<GenericFader> f = m_faders.at(i);
        if (!f.isNull() && f->priority() <= priority)
        {
            newPos = i;
            fader->setPriority(priority);
            break;
        }
    }

    if (newPos != pos)
    {
        m_faders.move(pos, newPos);
        qDebug() << "[Universe]" << id() << ": Generic fader moved from" << pos
                 << "to" << m_faders.indexOf(fader) << ". Count:" << m_faders.count();
    }
}

QList<QSharedPointer<GenericFader> > Universe::faders()
{
    return m_faders;
}

void Universe::setFaderPause(quint32 functionID, bool enable)
{
    QMutexLocker fadersLocker(&m_fadersMutex);
    QMutableListIterator<QSharedPointer<GenericFader> > it(m_faders);
    while (it.hasNext())
    {
        QSharedPointer<GenericFader> fader = it.next();
        if (fader.isNull() || fader->parentFunctionID() != functionID)
            continue;

        fader->setPaused(enable);
    }
}

void Universe::setFaderFadeOut(int fadeTime)
{
    QMutexLocker fadersLocker(&m_fadersMutex);
    foreach (QSharedPointer<GenericFader> fader, m_faders)
    {
        if (!fader.isNull() && fader->parentFunctionID() != Function::invalidId())
            fader->setFadeOut(true, uint(fadeTime));
    }
}

void Universe::tick()
{
    // Keep at most one pending tick to avoid queueing stale work when running late.
    if (m_semaphore.available() == 0)
        m_semaphore.release(1);
}

void Universe::processFaders(uint elapsedMs)
{
    flushInput();
    zeroIntensityChannels();

    QList<QSharedPointer<GenericFader>> activeFaders;
    {
        QMutexLocker fadersLocker(&m_fadersMutex);
        activeFaders.reserve(m_faders.size());
        QMutableListIterator<QSharedPointer<GenericFader> > it(m_faders);
        while (it.hasNext())
        {
            QSharedPointer<GenericFader> fader = it.next(); //m_faders.at(i);
            if (fader.isNull())
                continue;

            // destroy a fader if it's been requested
            // and it's not fading out
            if (fader->deleteRequested() && !fader->isFadingOut())
            {
                fader->removeAll();
                it.remove();
                fader.clear();
                continue;
            }

            if (fader->isEnabled() == false)
                continue;

            activeFaders.append(fader);
        }
    }

    // Sort by priority so overlay blend modes (Mask/Subtractive) write after base faders
    std::stable_sort(activeFaders.begin(), activeFaders.end(),
        [](const QSharedPointer<GenericFader> &a, const QSharedPointer<GenericFader> &b) {
            return a->priority() < b->priority();
        });

    foreach (const QSharedPointer<GenericFader> &fader, activeFaders)
        fader->write(this, elapsedMs);

    /* One immutable publication object: the bytes that go on the wire are
       rendered from the very source that is retained as their provenance,
       so a producer advancing meanwhile cannot make the two disagree. */
    QByteArray frame;
    bool dataChanged = false;
    {
        QMutexLocker outputLocker(&m_outputMutex);

        const OutputSource source = currentSource();
        frame = renderHeldFrame(source);
        dataChanged = (frame != m_lastPublishedFrame);

        m_lastPublishedSource = source;
        m_lastPublishedFrame = frame;

        dumpOutputLocked(source, frame, dataChanged);
    }

    if (dataChanged)
        emit universeWritten(id(), frame);
}

void Universe::run()
{
    m_running = true;
    int timeout = int(MasterTimer::tick()) * 2;
    QElapsedTimer elapsedTimer;
    elapsedTimer.start();

    qDebug() << "Universe thread started" << id();

    while (m_running)
    {
        if (m_semaphore.tryAcquire(1, timeout) == false)
        {
            //qWarning() << "Semaphore not acquired on universe" << id();
            continue;
        }
        const qint64 elapsedNs = elapsedTimer.nsecsElapsed();
        elapsedTimer.restart();
        uint elapsedMs = uint((elapsedNs + 500000) / 1000000);
        if (elapsedMs == 0)
            elapsedMs = 1;

        processFaders(elapsedMs);
    }

    qDebug() << "Universe thread stopped" << id();
}

/************************************************************************
 * Values
 ************************************************************************/

void Universe::reset()
{
    QMutexLocker outputLocker(&m_outputMutex);

    m_preGMValues->fill(0);
    m_blackoutValues->fill(0);

    if (m_passthrough)
        (*m_postGMValues) = (*m_passthroughValues);
    else
        m_postGMValues->fill(0);

    m_modifiers.fill(NULL, UNIVERSE_SIZE);
    m_modifierCount = 0;
    m_passthrough = false; // not releasing m_passthroughValues, see comment in setPassthrough

    // the contents are gone, so no output patch may still hold anything
    // from them, including its legacy pause bytes
    foreach (OutputPatch *op, m_outputPatchList)
        op->releasePublicationHistory();
}

void Universe::reset(int address, int range)
{
    if (address >= UNIVERSE_SIZE)
        return;

    if (address + range > UNIVERSE_SIZE)
       range = UNIVERSE_SIZE - address;

    memset(m_preGMValues->data() + address, 0, range * sizeof(*m_preGMValues->data()));
    memset(m_blackoutValues->data() + address, 0, range * sizeof(*m_blackoutValues->data()));
    memcpy(m_postGMValues->data() + address, m_modifiedZeroValues->data() + address, range * sizeof(*m_postGMValues->data()));

    applyPassthroughValues(address, range);
}

void Universe::applyPassthroughValues(int address, int range)
{
    if (!m_passthrough)
        return;

    for (int i = address; i < address + range && i < UNIVERSE_SIZE; i++)
    {
        if (static_cast<uchar>(m_postGMValues->at(i)) < static_cast<uchar>(m_passthroughValues->at(i))) // HTP merge
        {
            (*m_postGMValues)[i] = (*m_passthroughValues)[i];
        }
    }
}

void Universe::zeroIntensityChannels()
{
    updateIntensityChannelsRanges();
    int const* channels = m_intensityChannelsRanges.constData();
    for (int i = 0; i < m_intensityChannelsRanges.size(); ++i)
    {
        short channel = channels[i] >> 16;
        short size = channels[i] & 0xffff;

        reset(channel, size);
    }
}

QHash<int, uchar> Universe::intensityChannels()
{
    QHash <int, uchar> intensityList;
    for (int i = 0; i < m_intensityChannels.size(); ++i)
    {
        int channel = m_intensityChannels.at(i);
        intensityList[channel] = m_preGMValues->at(channel);
    }
    return intensityList;
}

uchar Universe::postGMValue(int address) const
{
    if (address >= m_postGMValues->size())
        return 0;

    return uchar(m_postGMValues->at(address));
}

const QByteArray* Universe::postGMValues() const
{
    return m_postGMValues.data();
}

Universe::BlendMode Universe::stringToBlendMode(QString mode)
{
    if (mode == KXMLUniverseNormalBlend)
        return NormalBlend;
    else if (mode == KXMLUniverseMaskBlend)
        return MaskBlend;
    else if (mode == KXMLUniverseAdditiveBlend)
        return AdditiveBlend;
    else if (mode == KXMLUniverseSubtractiveBlend)
        return SubtractiveBlend;

    return NormalBlend;
}

QString Universe::blendModeToString(Universe::BlendMode mode)
{
    switch(mode)
    {
        default:
        case NormalBlend:
            return QString(KXMLUniverseNormalBlend);
        break;
        case MaskBlend:
            return QString(KXMLUniverseMaskBlend);
        break;
        case AdditiveBlend:
            return QString(KXMLUniverseAdditiveBlend);
        break;
        case SubtractiveBlend:
            return QString(KXMLUniverseSubtractiveBlend);
        break;
    }
}

Universe::FaderPriority Universe::blendModePriority(Universe::BlendMode mode)
{
    if (mode == MaskBlend || mode == SubtractiveBlend)
        return BlendOverlay;
    return Auto;
}

const QByteArray Universe::preGMValues() const
{
    return *m_preGMValues;
}

uchar Universe::preGMValue(int address) const
{
    if (address < 0 || address >= m_preGMValues->size())
        return 0U;

    return static_cast<uchar>(m_preGMValues->at(address));
}

uchar Universe::applyGM(int channel, uchar value)
{
    return applyGMValue(uchar(m_channelsMask->at(channel)), value);
}

uchar Universe::applyGMValue(uchar channelMask, uchar value) const
{
    if ((m_grandMaster->channelMode() == GrandMaster::Intensity && (channelMask & Intensity)) ||
        (m_grandMaster->channelMode() == GrandMaster::AllChannels))
    {
        if (m_grandMaster->valueMode() == GrandMaster::Limit)
            value = MIN(value, m_grandMaster->value());
        else
            value = char(floor((double(value) * m_grandMaster->fraction()) + 0.5));
    }

    return value;
}

OutputSource Universe::currentSource() const
{
    OutputSource source;

    /* Owning copies, not implicit sharing: writeMultiple() and friends keep a
       detached raw pointer across their loop, so a shared buffer could still
       be written through after the snapshot was taken. */
    source.usedChannels = m_usedChannels;
    source.preGM = QByteArray(m_preGMValues->constData(), m_usedChannels);
    source.blackout = QByteArray(m_blackoutValues->constData(), m_usedChannels);
    source.mask = QByteArray(m_channelsMask->constData(), m_usedChannels);

    // held by value, since a modifier object can be edited while the look is
    // held. The scan only runs when modifiers are actually in use.
    if (m_modifierCount > 0)
    {
        for (int channel = 0; channel < int(m_usedChannels); channel++)
        {
            ChannelModifier *modifier = m_modifiers.at(channel);
            if (modifier != NULL)
                source.modifiers.insert(channel, *modifier);
        }
    }

    if (m_passthrough && !m_passthroughValues.isNull())
        source.passthrough = QByteArray(m_passthroughValues->constData(), m_usedChannels);

    return source;
}

QByteArray Universe::renderHeldFrame(const OutputSource &source) const
{
    QByteArray frame(source.usedChannels, char(0));

    const uchar *preGM = reinterpret_cast<const uchar *>(source.preGM.constData());
    const uchar *mask = reinterpret_cast<const uchar *>(source.mask.constData());
    const uchar *passthrough = source.passthrough.size() >= int(source.usedChannels)
            ? reinterpret_cast<const uchar *>(source.passthrough.constData()) : NULL;
    const bool hasModifiers = source.modifiers.isEmpty() == false;

    for (int channel = 0; channel < int(source.usedChannels); channel++)
    {
        uchar value = preGM[channel];

        if (value != 0)
            value = applyGMValue(mask[channel], value);

        if (hasModifiers)
        {
            QHash<int, ChannelModifier>::const_iterator modifier = source.modifiers.constFind(channel);
            if (modifier != source.modifiers.constEnd())
                value = modifier.value().getValue(value);
        }

        if (passthrough != NULL && value < passthrough[channel]) // HTP merge
            value = passthrough[channel];

        frame[channel] = char(value);
    }

    return frame;
}

uchar Universe::applyModifiers(int channel, uchar value)
{
    if (m_modifiers.at(channel) != NULL)
        return m_modifiers.at(channel)->getValue(value);

    return value;
}

uchar Universe::applyPassthrough(int channel, uchar value)
{
    if (m_passthrough)
    {
        const uchar passthroughValue = static_cast<uchar>(m_passthroughValues->at(channel));
        if (value < passthroughValue) // HTP merge
        {
            return passthroughValue;
        }
    }

    return value;
}

void Universe::updatePostGMValue(int channel)
{
    uchar value = preGMValue(channel);

    if (value != 0)
        value = applyGM(channel, value);

    value = applyModifiers(channel, value);
    value = applyPassthrough(channel, value);

    (*m_postGMValues)[channel] = static_cast<char>(value);
}

/************************************************************************
 * Patches
 ************************************************************************/

bool Universe::isPatched() const
{
    if (m_inputPatch != NULL || m_outputPatchList.count() || m_fbPatch != NULL)
        return true;

    return false;
}

bool Universe::setInputPatch(QLCIOPlugin *plugin,
                             quint32 input, QLCInputProfile *profile)
{
    qDebug() << "[Universe] setInputPatch - ID:" << m_id << ", plugin:" << ((plugin == NULL)?"None":plugin->name())
             << ", input:" << input << ", profile:" << ((profile == NULL)?"None":profile->name());
    if (m_inputPatch == NULL)
    {
        if (plugin == NULL || input == QLCIOPlugin::invalidLine())
            return true;

        m_inputPatch = new InputPatch(m_id, this);
        connectInputPatch();
    }
    else
    {
        if (input == QLCIOPlugin::invalidLine())
        {
            disconnectInputPatch();
            delete m_inputPatch;
            m_inputPatch = NULL;
            emit inputPatchChanged();
            return true;
        }
    }

    if (m_inputPatch != NULL)
    {
        bool result = m_inputPatch->set(plugin, input, profile);
        emit inputPatchChanged();
        return result;
    }

    return true;
}

bool Universe::setOutputPatch(QLCIOPlugin *plugin, quint32 output, int index)
{
    if (index < 0)
        return false;

    qDebug() << "[Universe] setOutputPatch - ID:" << m_id
             << ", plugin:" << ((plugin == NULL) ? "None" : plugin->name()) << ", output:" << output;

    // replace or delete an existing patch
    if (index < m_outputPatchList.count())
    {
        if (plugin == NULL || output == QLCIOPlugin::invalidLine())
        {
            // need to delete an existing patch
            OutputPatch *patch = NULL;
            {
                QMutexLocker outputLocker(&m_outputMutex);
                patch = m_outputPatchList.takeAt(index);
            }
            delete patch;
            emit outputPatchesCountChanged();
            return true;
        }

        OutputPatch *patch = m_outputPatchList.at(index);
        bool result = false;
        {
            QMutexLocker outputLocker(&m_outputMutex);
            // a re-patched line has no publication history of its own, and the
            // close/open must not race a publication through the old line
            patch->releasePublicationHistory();
            result = patch->set(plugin, output);
        }
        emit outputPatchChanged();
        return result;
    }
    else
    {
        if (plugin == NULL || output == QLCIOPlugin::invalidLine())
            return false;

        // add a new patch
        OutputPatch *patch = new OutputPatch(m_id, this);
        bool result = patch->set(plugin, output);
        {
            QMutexLocker outputLocker(&m_outputMutex);
            m_outputPatchList.append(patch);
        }
        emit outputPatchesCountChanged();
        return result;
    }

    return false;
}

bool Universe::setFeedbackPatch(QLCIOPlugin *plugin, quint32 output)
{
    qDebug() << Q_FUNC_INFO << "plugin:" << plugin << "output:" << output;
    if (m_fbPatch == NULL)
    {
        if (plugin == NULL || output == QLCIOPlugin::invalidLine())
            return false;

        m_fbPatch = new OutputPatch(m_id, this);
    }
    else
    {
        if (plugin == NULL || output == QLCIOPlugin::invalidLine())
        {
            delete m_fbPatch;
            m_fbPatch = NULL;
            emit hasFeedbackChanged();
            return true;
        }
    }
    if (m_fbPatch != NULL)
    {
        bool result = m_fbPatch->set(plugin, output);
        emit hasFeedbackChanged();
        return result;
    }

    return false;
}

bool Universe::hasFeedback() const
{
    return m_fbPatch != NULL ? true : false;
}

InputPatch *Universe::inputPatch() const
{
    return m_inputPatch;
}

OutputPatch *Universe::outputPatch(int index) const
{
    if (index < 0 || index >= m_outputPatchList.count())
        return NULL;

    return m_outputPatchList.at(index);
}

int Universe::outputPatchesCount() const
{
    return m_outputPatchList.count();
}

OutputPatch *Universe::feedbackPatch() const
{
    return m_fbPatch;
}

void Universe::dumpOutput(const QByteArray &data, bool dataChanged)
{
    QMutexLocker outputLocker(&m_outputMutex);

    const OutputSource source = currentSource();
    m_lastPublishedSource = source;
    m_lastPublishedFrame = data;

    dumpOutputLocked(source, data, dataChanged);
}

void Universe::dumpOutput()
{
    QMutexLocker outputLocker(&m_outputMutex);
    republishLastSubmitted();
}

void Universe::setBlackout(bool blackout)
{
    QMutexLocker outputLocker(&m_outputMutex);

    foreach (OutputPatch *op, m_outputPatchList)
    {
        if (op != NULL)
            op->setBlackout(blackout);
    }

    republishLastSubmitted();
}

void Universe::republishLastSubmitted()
{
    /* Reuse what was last submitted rather than sampling the live arrays: a
       worker may be part way through composing the next frame. */
    if (m_lastPublishedSource.isValid() && m_lastPublishedFrame.isEmpty() == false)
    {
        dumpOutputLocked(m_lastPublishedSource, m_lastPublishedFrame, true);
        return;
    }

    const OutputSource source = currentSource();
    dumpOutputLocked(source, renderHeldFrame(source), true);
}

void Universe::reconnectOutputPatches(QLCIOPlugin *plugin)
{
    QMutexLocker outputLocker(&m_outputMutex);

    foreach (OutputPatch *op, m_outputPatchList)
    {
        if (op != NULL && op->plugin() == plugin)
            op->reconnect();
    }
}

void Universe::dumpOutputLocked(const OutputSource &source, const QByteArray &data, bool dataChanged)
{
    if (m_outputPatchList.count() == 0)
        return;

    const bool frozen = m_frozen;
    const bool forceChanged = m_forceDumpChanged.exchange(false);

    foreach (OutputPatch *op, m_outputPatchList)
    {
        if (m_totalChannelsChanged == true)
            op->setPluginParameter(PLUGIN_UNIVERSECHANNELS, m_totalChannels);

        if (frozen)
        {
            const OutputSource &held = op->heldSource();
            QByteArray rendered;

            if (held.isValid() == false)
                // an output with no history of its own holds zero until thaw
                rendered = QByteArray(m_usedChannels, char(0));
            else
                rendered = renderHeldFrame(held);

            // the live extent can grow while the look is held: keep the held
            // bytes and extend with zeros so newly used channels stay dark
            if (rendered.size() < int(m_usedChannels))
                rendered.append(QByteArray(int(m_usedChannels) - rendered.size(), char(0)));

            // render the held look first, then suppress the blackout classes,
            // so the retained values are the rendered ones
            const QByteArray frame = op->blackout() ? held.blackoutSuppressed(rendered)
                                                    : rendered;

            op->dumpHeld(m_id, frame, rendered, forceChanged);
        }
        else if (op->blackout())
        {
            // owned bytes captured with the source, so they cannot be
            // rewritten while the plugin is reading them
            op->dump(m_id, source.blackout, dataChanged || forceChanged, source);
        }
        else
        {
            op->dump(m_id, data, dataChanged || forceChanged, source);
        }
    }
    m_totalChannelsChanged = false;
}

void Universe::flushInput()
{
    if (m_inputPatch == NULL)
        return;

    m_inputPatch->flush(m_id);
}

void Universe::slotInputValueChanged(quint32 universe, quint32 channel, uchar value, const QString &key)
{
    if (m_passthrough)
    {
        if (universe == m_id)
        {
            if (channel >= UNIVERSE_SIZE)
                return;

            // passthrough values feed the snapshot, so this off-worker write
            // must not land in the middle of the short copy
            QMutexLocker outputLocker(&m_outputMutex);

            if (channel >= m_usedChannels)
                m_usedChannels = channel + 1;

            (*m_passthroughValues)[channel] = value;

            updatePostGMValue(channel);
        }
    }
    else
        emit inputValueChanged(universe, channel, value, key);
}

void Universe::connectInputPatch()
{
    if (m_inputPatch == NULL)
        return;

    if (!m_passthrough)
        connect(m_inputPatch, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                this, SIGNAL(inputValueChanged(quint32,quint32,uchar,QString)));
    else
        connect(m_inputPatch, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                this, SLOT(slotInputValueChanged(quint32,quint32,uchar,const QString&)));
}

void Universe::disconnectInputPatch()
{
    if (m_inputPatch == NULL)
        return;

    if (!m_passthrough)
        disconnect(m_inputPatch, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                this, SIGNAL(inputValueChanged(quint32,quint32,uchar,QString)));
    else
        disconnect(m_inputPatch, SIGNAL(inputValueChanged(quint32,quint32,uchar,const QString&)),
                this, SLOT(slotInputValueChanged(quint32,quint32,uchar,const QString&)));
}

/************************************************************************
 * Channels capabilities
 ************************************************************************/

void Universe::setChannelCapability(ushort channel, QLCChannel::Group group, ChannelType forcedType)
{
    if (channel >= (ushort)m_channelsMask->length())
        return;

    // the capability mask feeds the snapshot, so this main-thread config
    // change must not land in the middle of the short copy
    QMutexLocker outputLocker(&m_outputMutex);

    if (Utils::vectorRemove(m_intensityChannels, channel))
        m_intensityChannelsChanged = true;
    Utils::vectorRemove(m_nonIntensityChannels, channel);

    if (forcedType != Undefined)
    {
        (*m_channelsMask)[channel] = char(forcedType);
        if ((forcedType & HTP) == HTP)
        {
            //qDebug() << "--- Channel" << channel << "forced type HTP";
            Utils::vectorSortedAddUnique(m_intensityChannels, channel);
            m_intensityChannelsChanged = true;
            if (group == QLCChannel::Intensity)
            {
                //qDebug() << "--- Channel" << channel << "Intensity + HTP";
                (*m_channelsMask)[channel] = char(HTP | Intensity);
            }
        }
        else if ((forcedType & LTP) == LTP)
        {
            //qDebug() << "--- Channel" << channel << "forced type LTP";
            Utils::vectorSortedAddUnique(m_nonIntensityChannels, channel);
        }
    }
    else
    {
        if (group == QLCChannel::Intensity)
        {
            //qDebug() << "--- Channel" << channel << "Intensity + HTP";
            (*m_channelsMask)[channel] = char(HTP | Intensity);
            Utils::vectorSortedAddUnique(m_intensityChannels, channel);
            m_intensityChannelsChanged = true;
        }
        else
        {
            //qDebug() << "--- Channel" << channel << "LTP";
            (*m_channelsMask)[channel] = char(LTP);
            Utils::vectorSortedAddUnique(m_nonIntensityChannels, channel);
        }
    }

    // qDebug() << Q_FUNC_INFO << "Channel:" << channel << "mask:" << QString::number(m_channelsMask->at(channel), 16);
    if (channel >= m_totalChannels)
    {
        m_totalChannels = channel + 1;
        m_totalChannelsChanged = true;
    }
}

uchar Universe::channelCapabilities(ushort channel) const
{
    if (channel >= (ushort)m_channelsMask->length())
        return Undefined;

    return m_channelsMask->at(channel);
}

void Universe::setChannelDefaultValue(ushort channel, uchar value)
{
    if (channel >= m_totalChannels)
    {
        m_totalChannels = channel + 1;
        m_totalChannelsChanged = true;
    }

    if (channel >= m_usedChannels)
        m_usedChannels = channel + 1;

    (*m_preGMValues)[channel] = value;
    updatePostGMValue(channel);
}

void Universe::setChannelModifier(ushort channel, ChannelModifier *modifier)
{
    if (channel >= (ushort)m_modifiers.count())
        return;

    // modifiers feed the snapshot, so this main-thread config change must
    // not land in the middle of the short copy
    QMutexLocker outputLocker(&m_outputMutex);

    if (m_modifiers.at(channel) == NULL && modifier != NULL)
        m_modifierCount++;
    else if (m_modifiers.at(channel) != NULL && modifier == NULL)
        m_modifierCount--;

    m_modifiers[channel] = modifier;

    if (modifier != NULL)
    {
        (*m_modifiedZeroValues)[channel] = modifier->getValue(0);

        if (channel >= m_totalChannels)
        {
            m_totalChannels = channel + 1;
            m_totalChannelsChanged = true;
        }

        if (channel >= m_usedChannels)
            m_usedChannels = channel + 1;
    }

    updatePostGMValue(channel);
}

ChannelModifier *Universe::channelModifier(ushort channel)
{
    if (channel >= (ushort)m_modifiers.count())
        return NULL;

    return m_modifiers.at(channel);
}

void Universe::updateIntensityChannelsRanges()
{
    if (!m_intensityChannelsChanged)
        return;

    m_intensityChannelsChanged = false;

    m_intensityChannelsRanges.clear();
    short currentPos = -1;
    short currentSize = 0;

    for (int i = 0; i < m_intensityChannels.size(); ++i)
    {
        int channel = m_intensityChannels.at(i);
        if (currentPos + currentSize == channel)
            ++currentSize;
        else
        {
            if (currentPos != -1)
                m_intensityChannelsRanges.append((currentPos << 16) | currentSize);
            currentPos = channel;
            currentSize = 1;
        }
    }
    if (currentPos != -1)
        m_intensityChannelsRanges.append((currentPos << 16) | currentSize);

    qDebug() << Q_FUNC_INFO << ":" << m_intensityChannelsRanges.size() << "ranges";
}

/****************************************************************************
 * Writing
 ****************************************************************************/

bool Universe::write(int address, uchar value, bool forceLTP)
{
    Q_ASSERT(address < UNIVERSE_SIZE);

    //qDebug() << "[Universe]" << id() << ": write channel" << address << ", value:" << value;

    if (address >= m_usedChannels)
        m_usedChannels = address + 1;

    uchar *preGM = reinterpret_cast<uchar *>(m_preGMValues->data());
    const uchar *mask = reinterpret_cast<const uchar *>(m_channelsMask->constData());

    if (mask[address] & HTP)
    {
        if (forceLTP == false && value < preGM[address])
        {
            return false;
        }
    }
    else
    {
        // preserve non HTP channels for blackout
        reinterpret_cast<uchar *>(m_blackoutValues->data())[address] = value;
    }

    preGM[address] = value;

    updatePostGMValue(address);

    return true;
}

bool Universe::writeMultiple(int address, quint32 value, int channelCount)
{
    uchar *preGM = reinterpret_cast<uchar *>(m_preGMValues->data());
    uchar *blackout = reinterpret_cast<uchar *>(m_blackoutValues->data());
    const uchar *mask = reinterpret_cast<const uchar *>(m_channelsMask->constData());
    const uchar *valueBytes = reinterpret_cast<const uchar *>(&value);

    for (int i = 0; i < channelCount; i++)
    {
        const uchar v = valueBytes[channelCount - 1 - i];
        const int addr = address + i;

        // preserve non HTP channels for blackout
        if ((mask[addr] & HTP) == 0)
            blackout[addr] = v;

        preGM[addr] = v;

        updatePostGMValue(addr);
    }

    return true;
}

bool Universe::writeRelative(int address, quint32 value, int channelCount)
{
    Q_ASSERT(address < UNIVERSE_SIZE);

    //qDebug() << "Write relative channel" << address << "value" << value;

    if (address + channelCount >= m_usedChannels)
        m_usedChannels = address + channelCount;

    uchar *preGM = reinterpret_cast<uchar *>(m_preGMValues->data());
    uchar *blackout = reinterpret_cast<uchar *>(m_blackoutValues->data());

    if (channelCount == 1)
    {
        short newVal = preGM[address];
        newVal += short(value) - RELATIVE_ZERO_8BIT;
        const uchar clamped = uchar(CLAMP(newVal, 0, UCHAR_MAX));
        preGM[address] = clamped;
        blackout[address] = clamped;
        updatePostGMValue(address);
    }
    else
    {
        quint32 currentValue = 0;
        for (int i = 0; i < channelCount; i++)
            currentValue = (currentValue << 8) + preGM[address + i];

        currentValue = qint32(CLAMP((qint32)currentValue + (qint32)value - RELATIVE_ZERO_16BIT, 0, 0xFFFF));

        const uchar *currentBytes = reinterpret_cast<const uchar *>(&currentValue);
        for (int i = 0; i < channelCount; i++)
        {
            const uchar v = currentBytes[channelCount - 1 - i];
            preGM[address + i] = v;
            blackout[address + i] = v;
            updatePostGMValue(address + i);
        }
    }

    return true;
}

bool Universe::writeBlended(int address, quint32 value, int channelCount, Universe::BlendMode blend)
{
    if (address + channelCount >= m_usedChannels)
        m_usedChannels = address + channelCount;

    const uchar *preGM = reinterpret_cast<const uchar *>(m_preGMValues->constData());
    const uchar *mask = reinterpret_cast<const uchar *>(m_channelsMask->constData());

    quint32 currentValue = 0;
    for (int i = 0; i < channelCount; i++)
        currentValue = (currentValue << 8) + preGM[address + i];

    switch (blend)
    {
        case NormalBlend:
        {
            if ((mask[address] & HTP) && value < currentValue)
            {
                return false;
            }
        }
        break;
        case MaskBlend:
        {
            const float maxValue = (channelCount == 1)
                                   ? 255.0f
                                   : (channelCount == 2 ? 65535.0f : float(pow(255.0f, channelCount)));
            if (value)
            {
                if (currentValue)
                    value = float(currentValue) * (float(value) / maxValue);
                else
                    value = 0;
            }
        }
        break;
        case AdditiveBlend:
        {
            const float maxValue = (channelCount == 1)
                                   ? 255.0f
                                   : (channelCount == 2 ? 65535.0f : float(pow(255.0f, channelCount)));
            //qDebug() << "Universe write additive channel" << channel << ", value:" << currVal << "+" << value;
            value = fmin(float(currentValue + value), maxValue);
        }
        break;
        case SubtractiveBlend:
        {
            if (value >= currentValue)
                value = 0;
            else
                value = currentValue - value;
        }
        break;
        default:
            qDebug() << "[Universe] Blend mode not handled. Implement me!" << blend;
            return false;
        break;
    }

    writeMultiple(address, value, channelCount);

    return true;
}

/*********************************************************************
 * Load & Save
 *********************************************************************/

bool Universe::loadXML(QXmlStreamReader &root, int index, InputOutputMap *ioMap)
{
    if (root.name() != KXMLQLCUniverse)
    {
        qWarning() << Q_FUNC_INFO << "Universe node not found";
        return false;
    }

    int outputIndex = 0;

    QXmlStreamAttributes attrs = root.attributes();

    if (attrs.hasAttribute(KXMLQLCUniverseName))
        setName(attrs.value(KXMLQLCUniverseName).toString());

    if (attrs.hasAttribute(KXMLQLCUniversePassthrough))
    {
        if (attrs.value(KXMLQLCUniversePassthrough).toString() == KXMLQLCTrue ||
            attrs.value(KXMLQLCUniversePassthrough).toString() == "1")
            setPassthrough(true);
        else
            setPassthrough(false);
    }
    else
    {
        setPassthrough(false);
    }

    while (root.readNextStartElement())
    {
        QXmlStreamAttributes pAttrs = root.attributes();

        if (root.name() == KXMLQLCUniverseInputPatch)
        {
            QString plugin = KInputNone;
            quint32 inputLine = QLCIOPlugin::invalidLine();
            QString inputUID;
            QString inputName;
            QString profile = KInputNone;

            if (pAttrs.hasAttribute(KXMLQLCUniversePlugin))
                plugin = pAttrs.value(KXMLQLCUniversePlugin).toString();
            if (pAttrs.hasAttribute(KXMLQLCUniverseLineUID))
                inputUID = pAttrs.value(KXMLQLCUniverseLineUID).toString();
            if (pAttrs.hasAttribute(KXMLQLCUniverseLineName))
                inputName = pAttrs.value(KXMLQLCUniverseLineName).toString();
            else if (pAttrs.hasAttribute(KXMLQLCUniverseLineUID))
                inputName = inputUID; // backward compat: old files stored name in UID attribute
            if (pAttrs.hasAttribute(KXMLQLCUniverseLine))
                inputLine = pAttrs.value(KXMLQLCUniverseLine).toString().toUInt();
            if (pAttrs.hasAttribute(KXMLQLCUniverseProfileName))
                profile = pAttrs.value(KXMLQLCUniverseProfileName).toString();

            // apply the parameters just loaded
            ioMap->setInputPatch(index, plugin, inputUID, inputName, inputLine, profile);

            QXmlStreamReader::TokenType tType = root.readNext();
            if (tType == QXmlStreamReader::Characters)
                tType = root.readNext();

            // check if there is a PluginParameters tag defined
            if (tType == QXmlStreamReader::StartElement)
            {
                if (root.name() == KXMLQLCUniversePluginParameters)
                    loadXMLPluginParameters(root, InputPatchTag, 0);
                root.skipCurrentElement();
            }
        }
        else if (root.name() == KXMLQLCUniverseOutputPatch)
        {
            QString plugin = KOutputNone;
            QString outputUID;
            QString outputName;
            quint32 outputLine = QLCIOPlugin::invalidLine();

            if (pAttrs.hasAttribute(KXMLQLCUniversePlugin))
                plugin = pAttrs.value(KXMLQLCUniversePlugin).toString();
            if (pAttrs.hasAttribute(KXMLQLCUniverseLineUID))
                outputUID = pAttrs.value(KXMLQLCUniverseLineUID).toString();
            if (pAttrs.hasAttribute(KXMLQLCUniverseLineName))
                outputName = pAttrs.value(KXMLQLCUniverseLineName).toString();
            else if (pAttrs.hasAttribute(KXMLQLCUniverseLineUID))
                outputName = outputUID; // backward compat: old files stored name in UID attribute
            if (pAttrs.hasAttribute(KXMLQLCUniverseLine))
                outputLine = pAttrs.value(KXMLQLCUniverseLine).toString().toUInt();

            // apply the parameters just loaded
            ioMap->setOutputPatch(index, plugin, outputUID, outputName, outputLine, false, outputIndex);

            QXmlStreamReader::TokenType tType = root.readNext();
            if (tType == QXmlStreamReader::Characters)
                tType = root.readNext();

            // check if there is a PluginParameters tag defined
            if (tType == QXmlStreamReader::StartElement)
            {
                if (root.name() == KXMLQLCUniversePluginParameters)
                    loadXMLPluginParameters(root, OutputPatchTag, outputIndex);
                root.skipCurrentElement();
            }

            outputIndex++;
        }
        else if (root.name() == KXMLQLCUniverseFeedbackPatch)
        {
            QString plugin = KOutputNone;
            QString outputUID;
            QString outputName;
            quint32 output = QLCIOPlugin::invalidLine();

            if (pAttrs.hasAttribute(KXMLQLCUniversePlugin))
                plugin = pAttrs.value(KXMLQLCUniversePlugin).toString();
            if (pAttrs.hasAttribute(KXMLQLCUniverseLineUID))
                outputUID = pAttrs.value(KXMLQLCUniverseLineUID).toString();
            if (pAttrs.hasAttribute(KXMLQLCUniverseLineName))
                outputName = pAttrs.value(KXMLQLCUniverseLineName).toString();
            else if (pAttrs.hasAttribute(KXMLQLCUniverseLineUID))
                outputName = outputUID; // backward compat: old files stored name in UID attribute
            if (pAttrs.hasAttribute(KXMLQLCUniverseLine))
                output = pAttrs.value(KXMLQLCUniverseLine).toString().toUInt();

            // apply the parameters just loaded
            ioMap->setOutputPatch(index, plugin, outputUID, outputName, output, true);

            QXmlStreamReader::TokenType tType = root.readNext();
            if (tType == QXmlStreamReader::Characters)
                tType = root.readNext();

            // check if there is a PluginParameters tag defined
            if (tType == QXmlStreamReader::StartElement)
            {
                if (root.name() == KXMLQLCUniversePluginParameters)
                    loadXMLPluginParameters(root, FeedbackPatchTag, 0);
                root.skipCurrentElement();
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Universe tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    return true;
}

bool Universe::loadXMLPluginParameters(QXmlStreamReader &root, PatchTagType currentTag, int patchIndex)
{
    if (root.name() != KXMLQLCUniversePluginParameters)
    {
        qWarning() << Q_FUNC_INFO << "PluginParameters node not found";
        return false;
    }

    QXmlStreamAttributes pluginAttrs = root.attributes();
    for (int i = 0; i < pluginAttrs.count(); i++)
    {
        QXmlStreamAttribute attr = pluginAttrs.at(i);
        if (currentTag == InputPatchTag)
        {
            InputPatch *ip = inputPatch();
            if (ip != NULL)
                ip->setPluginParameter(attr.name().toString(), attr.value().toString());
        }
        else if (currentTag == OutputPatchTag)
        {
            OutputPatch *op = outputPatch(patchIndex);
            if (op != NULL)
                op->setPluginParameter(attr.name().toString(), attr.value().toString());
        }
        else if (currentTag == FeedbackPatchTag)
        {
            OutputPatch *fbp = feedbackPatch();
            if (fbp != NULL)
                fbp->setPluginParameter(attr.name().toString(), attr.value().toString());
        }
    }
    root.skipCurrentElement();

    return true;
}

bool Universe::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    doc->writeStartElement(KXMLQLCUniverse);
    doc->writeAttribute(KXMLQLCUniverseName, name());
    doc->writeAttribute(KXMLQLCUniverseID, QString::number(id()));

    if (passthrough() == true)
        doc->writeAttribute(KXMLQLCUniversePassthrough, KXMLQLCTrue);

    if (inputPatch() != NULL)
    {
        savePatchXML(doc, KXMLQLCUniverseInputPatch, inputPatch()->pluginName(),
            inputPatch()->inputName(), inputPatch()->inputUID(),
            inputPatch()->input(), inputPatch()->profileName(), inputPatch()->getPluginParameters());
    }
    foreach (OutputPatch *op, m_outputPatchList)
    {
        savePatchXML(doc, KXMLQLCUniverseOutputPatch, op->pluginName(),
            op->outputName(), op->outputUID(),
            op->output(), "", op->getPluginParameters());
    }
    if (feedbackPatch() != NULL)
    {
        savePatchXML(doc, KXMLQLCUniverseFeedbackPatch, feedbackPatch()->pluginName(),
            feedbackPatch()->outputName(), feedbackPatch()->outputUID(),
            feedbackPatch()->output(), "", feedbackPatch()->getPluginParameters());
    }

    /* End the <Universe> tag */
    doc->writeEndElement();

    return true;
}

void Universe::savePatchXML(
    QXmlStreamWriter *doc,
    const QString &tag,
    const QString &pluginName,
    const QString &lineName,
    const QString &lineUID,
    quint32 line,
    QString profileName,
    QMap<QString, QVariant> parameters) const
{
    // sanity check: don't save invalid data
    if (pluginName.isEmpty() || pluginName == KInputNone || line == QLCIOPlugin::invalidLine())
        return;

    doc->writeStartElement(tag);
    doc->writeAttribute(KXMLQLCUniversePlugin, pluginName);
    doc->writeAttribute(KXMLQLCUniverseLineName, lineName);
    doc->writeAttribute(KXMLQLCUniverseLineUID, lineUID);
    doc->writeAttribute(KXMLQLCUniverseLine, QString::number(line));
    if (!profileName.isEmpty() && profileName != KInputNone)
        doc->writeAttribute(KXMLQLCUniverseProfileName, profileName);

    savePluginParametersXML(doc, parameters);
    doc->writeEndElement();
}

bool Universe::savePluginParametersXML(QXmlStreamWriter *doc,
                                       QMap<QString, QVariant> parameters) const
{
    Q_ASSERT(doc != NULL);

    if (parameters.isEmpty())
        return false;

    doc->writeStartElement(KXMLQLCUniversePluginParameters);
    QMapIterator<QString, QVariant> it(parameters);
    while (it.hasNext())
    {
        it.next();
        QString pName = it.key();
        QVariant pValue = it.value();
        doc->writeAttribute(pName, pValue.toString());
    }
    doc->writeEndElement();

    return true;
}
