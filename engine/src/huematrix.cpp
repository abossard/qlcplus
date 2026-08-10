/*
  Q Light Controller Plus
  huematrix.cpp

  Copyright (c) QLC+ contributors

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
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QThread>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <cmath>

#include "huematrix.h"
#include "huescript.h"
#include "huescriptscache.h"
#include "rgbscriptscache.h"
#include "qlcfixturehead.h"
#include "audiochannel.h"
#include "audioprofile.h"
#include "fixturegroup.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "rgbimage.h"
#include "rgbplain.h"
#include "rgbaudio.h"
#include "rgbtext.h"
#include "doc.h"

#define KXMLQLCRGBMatrixStartColor      QStringLiteral("MonoColor")
#define KXMLQLCRGBMatrixEndColor        QStringLiteral("EndColor")
#define KXMLQLCRGBMatrixColor           QStringLiteral("Color")
#define KXMLQLCRGBMatrixColorIndex      QStringLiteral("Index")

#define KXMLQLCRGBMatrixFixtureGroup    QStringLiteral("FixtureGroup")
#define KXMLQLCRGBMatrixDimmerControl   QStringLiteral("DimmerControl")

#define KXMLQLCRGBMatrixProperty        QStringLiteral("Property")
#define KXMLQLCRGBMatrixPropertyName    QStringLiteral("Name")
#define KXMLQLCRGBMatrixPropertyValue   QStringLiteral("Value")

#define KXMLQLCRGBMatrixControlMode         QStringLiteral("ControlMode")
#define KXMLQLCRGBMatrixControlModeRgb      QStringLiteral("RGB")
#define KXMLQLCRGBMatrixControlModeAmber    QStringLiteral("Amber")
#define KXMLQLCRGBMatrixControlModeWhite    QStringLiteral("White")
#define KXMLQLCRGBMatrixControlModeUV       QStringLiteral("UV")
#define KXMLQLCRGBMatrixControlModeDimmer   QStringLiteral("Dimmer")
#define KXMLQLCRGBMatrixControlModeShutter  QStringLiteral("Shutter")
#define KXMLQLCRGBMatrixControlModeRgbw     QStringLiteral("RGBW")
#define KXMLQLCRGBMatrixControlModeRgbwBrighter QStringLiteral("RGBWBrighter")

#define KXMLQLCRGBMatrixRotation            QStringLiteral("Rotation")
#define KXMLQLCRGBMatrixMirror              QStringLiteral("Mirror")
#define KXMLQLCRGBMatrixMirrorBlend         QStringLiteral("MirrorBlend")
#define KXMLQLCRGBMatrixBrightness          QStringLiteral("Brightness")

#define KXMLQLCRGBMatrixBeatEffect          QStringLiteral("BeatEffect")
#define KXMLQLCRGBMatrixBeatSelection       QStringLiteral("BeatSelection")
#define KXMLQLCRGBMatrixBeatOrientation     QStringLiteral("BeatOrientation")

/****************************************************************************
 * Initialization
 ****************************************************************************/

HUEMatrix::HUEMatrix(Doc *doc)
    : RGBMatrix(doc)
    , m_rotation(0)
    , m_mirror(0)
    , m_mirrorBlend(MirrorFlip)
    , m_brightness(1.0)
    , m_beatEffect(BeatEffectOff)
    , m_beatSelection(BeatSelAllOnDownbeat)
    , m_beatOrientation(BeatOrientRows)
    , m_currentBeat(0)
    , m_lastBeat(-1)
    , m_randomSegment(0)
{
    m_type = Function::HUEMatrixType;

    // Precomputed-map state starts empty. m_currentGeneration begins at 0;
    // consumers only trust the precomputed map when ready==1, so the initial
    // value of m_precomputedGeneration is harmless until the first kick.
    m_precomputedReady.storeRelease(0);
    m_precomputedInFlight.storeRelease(0);
    m_currentGeneration.storeRelease(0);
    m_precomputedGeneration = 0;
    m_precomputedStep = -1;
    m_precomputedColor = 0;
    m_precomputedAlgorithm = nullptr;

    setName(tr("New HUE Matrix"));

    // Mark the PixelPlan dirty initially. It will be (re)built lazily on the
    // first updateMapChannels() call. Connect to Doc signals so that fixture
    // address/mode changes and group membership changes invalidate the cache.
    m_pixelPlanDirty.storeRelease(1);
    if (doc != NULL)
    {
        connect(doc, &Doc::fixtureChanged,
                this, &HUEMatrix::invalidatePixelPlan, Qt::DirectConnection);
        connect(doc, &Doc::fixtureRemoved,
                this, &HUEMatrix::invalidatePixelPlan, Qt::DirectConnection);
        connect(doc, &Doc::fixtureGroupChanged,
                this, &HUEMatrix::invalidatePixelPlan, Qt::DirectConnection);
        connect(doc, &Doc::fixtureGroupRemoved,
                this, &HUEMatrix::invalidatePixelPlan, Qt::DirectConnection);
    }
}

HUEMatrix::~HUEMatrix()
{
    // Any async precompute task captures `this`, so wait for it to drain before
    // we tear down. A task normally completes within one rgbMap duration
    // (~10ms), but the flag is raised *before* the job is queued: if the JS
    // thread is torn down in between, the queued lambda never runs and the flag
    // is never cleared. Bound the wait so shutdown cannot hang.
    QElapsedTimer timer;
    timer.start();
    while (m_precomputedInFlight.loadAcquire() != 0)
    {
        if (timer.elapsed() >= precomputeDrainTimeoutMs)
        {
            qWarning() << Q_FUNC_INFO << "async precompute did not drain within"
                       << precomputeDrainTimeoutMs << "ms for" << name()
                       << "- abandoning the wait";
            break;
        }
        QThread::yieldCurrentThread();
    }
}


QStringList HUEMatrix::availableAlgorithms(Doc *doc)
{
    QStringList list = RGBAlgorithm::algorithms(doc);
    foreach (QString name, doc->hueScriptsCache()->hsvNames())
    {
        if (list.contains(name) == false)
            list << name;
    }
    return list;
}

/** Names of the built-in (non-script) algorithms. A HUE script that declares
 *  one of these would shadow the built-in and make it unreachable by name. */
static QStringList builtInAlgorithmNames(Doc *doc)
{
    RGBPlain plain(doc);
    RGBText text(doc);
    RGBImage image(doc);
    RGBAudio audio(doc);
    return QStringList() << plain.name() << text.name() << image.name() << audio.name();
}

RGBAlgorithm *HUEMatrix::createAlgorithm(Doc *doc, const QString &name)
{
    // Built-ins win over scripts. Without this a HUE script declaring e.g.
    // "Audio Spectrum" would permanently hide the RGBAudio built-in.
    if (builtInAlgorithmNames(doc).contains(name))
        return RGBAlgorithm::algorithm(doc, name);

    // Scripts must be resolved through the HUE cache so that they get the
    // HSV contract and the audio machinery. Built-ins fall through to the
    // upstream factory unchanged.
    HUEScript *script = doc->hueScriptsCache()->script(name);
    if (script != NULL)
        return script;

    // The upstream factory returns an empty RGBScript for an unknown name.
    // Reject unknown names here so that callers can detect them.
    if (availableAlgorithms(doc).contains(name) == false)
        return NULL;

    return RGBAlgorithm::algorithm(doc, name);
}

RGBAlgorithm *HUEMatrix::algorithmLoader(Doc *doc, QXmlStreamReader &root)
{
    if (root.attributes().value(KXMLQLCRGBAlgorithmType).toString() != KXMLQLCRGBScript)
        return RGBAlgorithm::loader(doc, root);

    QString name = root.readElementText();
    HUEScript *script = doc->hueScriptsCache()->script(name);
    if (script == NULL)
        qWarning() << Q_FUNC_INFO << "Script" << name << "not available";
    return script;
}

QIcon HUEMatrix::getIcon() const
{
    return QIcon(":/huematrix.svg");
}

void HUEMatrix::setTotalDuration(quint32 msec)
{
    QMutexLocker algorithmLocker(&m_algorithmMutex);

    if (m_algorithm == NULL)
        return;

    FixtureGroup *grp = doc()->fixtureGroup(fixtureGroup());
    if (grp == NULL)
        return;

    int steps = m_algorithm->rgbMapStepCount(effectiveAlgorithmSize(grp));
    setDuration(msec / steps);
}

quint32 HUEMatrix::totalDuration()
{
    QMutexLocker algorithmLocker(&m_algorithmMutex);

    if (m_algorithm == NULL)
        return 0;

    FixtureGroup *grp = doc()->fixtureGroup(fixtureGroup());
    if (grp == NULL)
        return 0;

    //qDebug () << "Algorithm steps:" << m_algorithm->rgbMapStepCount(grp->size());
    return m_algorithm->rgbMapStepCount(effectiveAlgorithmSize(grp)) * duration();
}

void HUEMatrix::setDimmerControl(bool dimmerControl)
{
    m_dimmerControl = dimmerControl;
    m_pixelPlanDirty.storeRelease(1);
}

Function* HUEMatrix::createCopy(Doc* doc, bool addToDoc)
{
    Q_ASSERT(doc != NULL);

    Function *copy = new RGBMatrix(doc);
    if (copy->copyFrom(this) == false)
    {
        delete copy;
        copy = NULL;
    }
    if (addToDoc == true && doc->addFunction(copy) == false)
    {
        delete copy;
        copy = NULL;
    }

    return copy;
}

bool HUEMatrix::copyFrom(const Function* function)
{
    const HUEMatrix *mtx = qobject_cast<const HUEMatrix*> (function);
    if (mtx == NULL)
        return false;

    setDimmerControl(mtx->dimmerControl());
    setFixtureGroup(mtx->fixtureGroup());

    m_rgbColors.clear();
    foreach (QColor col, mtx->getColors())
        m_rgbColors.append(col);

    if (mtx->algorithm() != NULL)
        setAlgorithm(mtx->algorithm()->clone());
    else
        setAlgorithm(NULL);

    setControlMode(mtx->controlMode());
    setRotation(mtx->rotation());
    setMirror(mtx->mirror());
    setMirrorBlend(mtx->mirrorBlend());
    setBrightness(mtx->brightness());
    setBeatEffect(mtx->beatEffect());
    setBeatSelection(mtx->beatSelection());
    setBeatOrientation(mtx->beatOrientation());

    return Function::copyFrom(function);
}

void HUEMatrix::setFixtureGroup(quint32 id)
{
    m_fixtureGroupID = id;
    {
        QMutexLocker algoLocker(&m_algorithmMutex);
        m_group = doc()->fixtureGroup(m_fixtureGroupID);
    }
    m_stepsCount = algorithmStepsCount();
    m_pixelPlanDirty.storeRelease(1);
    // Phase 4: any precomputed map is for the OLD group. Invalidate.
    m_currentGeneration.fetchAndAddRelaxed(1);
    m_precomputedReady.storeRelease(0);
}

void HUEMatrix::setAlgorithm(RGBAlgorithm *algo)
{
    RGBAlgorithm *oldAlgo = nullptr;
    {
        QMutexLocker algorithmLocker(&m_algorithmMutex);
        oldAlgo = m_algorithm;
        m_algorithm = algo;

        // Phase 4: invalidate any pending/stored precomputed frame BEFORE
        // releasing the algorithm pointer for deletion. Bumping the generation
        // ensures any in-flight JSThread task that captured the old generation
        // will discard its result rather than store it.
        m_currentGeneration.fetchAndAddRelaxed(1);
        m_precomputedReady.storeRelease(0);

        m_requestEngineCreation = true;

        /** If there's been a change of Script algorithm "on the fly",
         *  then re-apply the properties currently set in this RGBMatrix */
        if (m_algorithm != NULL && m_algorithm->type() == RGBAlgorithm::Script)
        {
            RGBScript *script = static_cast<RGBScript*> (m_algorithm);
            QMapIterator<QString, QString> it(m_properties);
            while (it.hasNext())
            {
                it.next();
                if (script->setProperty(it.key(), it.value()) == false)
                {
                    /** If the new algorithm doesn't expose a property,
                     *  then remove it from the cached list, otherwise
                     *  it would be carried around forever (and saved on XML) */
                    m_properties.take(it.key());
                }
            }

            // Colors are injected via injectColors() at render time;
            // no need to seed m_rgbColors from the script here.
        }
    }
    // Phase 4: deferred delete of the previous algorithm. For Script-typed
    // algos, this serializes after any pending precompute tasks on the
    // JSThread (FIFO), avoiding use-after-free in the async path.
    deferDeleteAlgorithm(oldAlgo);
    m_stepsCount = algorithmStepsCount();

    if (m_applyingStyleAttributes == false)
        Function::adjustAttribute(algorithmIndex(), PatternAttr);

    emit changed(id());
}

int HUEMatrix::algorithmStepsCount()
{
    QMutexLocker algorithmLocker(&m_algorithmMutex);

    if (m_algorithm == NULL)
        return 0;

    FixtureGroup *grp = doc()->fixtureGroup(fixtureGroup());
    if (grp != NULL)
        return m_algorithm->rgbMapStepCount(effectiveAlgorithmSize(grp));

    return 0;
}

void HUEMatrix::previewMap(int step, RGBMatrixStep *handler)
{
    QMutexLocker algorithmLocker(&m_algorithmMutex);
    if (m_algorithm == NULL || handler == NULL)
        return;

    if (m_group == NULL)
        m_group = doc()->fixtureGroup(fixtureGroup());

    if (m_group != NULL)
    {
        QSize algoSize = effectiveAlgorithmSize(m_group);
        if (m_algorithm->usesAudio())
            m_algorithm->setDisplaySize(m_group->size());
        setMapColors(m_algorithm);
        m_algorithm->rgbMap(algoSize, handler->stepColor().rgb(), step, handler->m_map);
        if (m_rotation || m_mirror)
            applyTransforms(handler->m_map, algoSize, m_group->size(),
                            m_rotation, m_mirror, m_mirrorBlend);
    }
}


bool HUEMatrix::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCFunction)
    {
        qWarning() << Q_FUNC_INFO << "Function node not found";
        return false;
    }

    if (root.attributes().value(KXMLQLCFunctionType).toString() != typeToString(Function::HUEMatrixType))
    {
        qWarning() << Q_FUNC_INFO << "Function is not a HUE matrix";
        return false;
    }

    /* Load matrix contents */
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCFunctionSpeed)
        {
            loadXMLSpeed(root);
        }
        else if (root.name() == KXMLQLCFunctionTempoType)
        {
            loadXMLTempoType(root);
        }
        else if (root.name() == KXMLQLCRGBAlgorithm)
        {
            setAlgorithm(HUEMatrix::algorithmLoader(doc(), root));
        }
        else if (root.name() == KXMLQLCRGBMatrixFixtureGroup)
        {
            setFixtureGroup(root.readElementText().toUInt());
        }
        else if (root.name() == KXMLQLCFunctionDirection)
        {
            loadXMLDirection(root);
        }
        else if (root.name() == KXMLQLCFunctionRunOrder)
        {
            loadXMLRunOrder(root);
        }
        // Legacy support
        else if (root.name() == KXMLQLCRGBMatrixStartColor)
        {
            setColor(0, QColor::fromRgb(QRgb(root.readElementText().toUInt())));
        }
        else if (root.name() == KXMLQLCRGBMatrixEndColor)
        {
            setColor(1, QColor::fromRgb(QRgb(root.readElementText().toUInt())));
        }
        else if (root.name() == KXMLQLCRGBMatrixColor)
        {
            int colorIdx = root.attributes().value(KXMLQLCRGBMatrixColorIndex).toInt();
            setColor(colorIdx, QColor::fromRgb(QRgb(root.readElementText().toUInt())));
        }
        else if (root.name() == KXMLQLCRGBMatrixControlMode)
        {
            setControlMode(stringToControlMode(root.readElementText()));
        }
        else if (root.name() == KXMLQLCRGBMatrixProperty)
        {
            QString name = root.attributes().value(KXMLQLCRGBMatrixPropertyName).toString();
            QString value = root.attributes().value(KXMLQLCRGBMatrixPropertyValue).toString();
            setProperty(name, value);
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCRGBMatrixDimmerControl)
        {
            setDimmerControl(root.readElementText().toInt());
        }
        else if (root.name() == KXMLQLCRGBMatrixRotation)
        {
            setRotation(root.readElementText().toInt());
        }
        else if (root.name() == KXMLQLCRGBMatrixMirror)
        {
            setMirror(root.readElementText().toInt());
        }
        else if (root.name() == KXMLQLCRGBMatrixMirrorBlend)
        {
            setMirrorBlend(stringToMirrorBlend(root.readElementText()));
        }
        else if (root.name() == KXMLQLCRGBMatrixBrightness)
        {
            setBrightness(root.readElementText().toDouble());
        }
        else if (root.name() == KXMLQLCRGBMatrixBeatEffect)
        {
            setBeatEffect(stringToBeatEffect(root.readElementText()));
        }
        else if (root.name() == KXMLQLCRGBMatrixBeatSelection)
        {
            setBeatSelection(stringToBeatSelection(root.readElementText()));
        }
        else if (root.name() == KXMLQLCRGBMatrixBeatOrientation)
        {
            setBeatOrientation(stringToBeatOrientation(root.readElementText()));
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown RGB matrix tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    return true;
}

bool HUEMatrix::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    /* Function tag */
    doc->writeStartElement(KXMLQLCFunction);

    /* Common attributes */
    saveXMLCommon(doc);

    /* Tempo type */
    saveXMLTempoType(doc);

    /* Speeds */
    saveXMLSpeed(doc);

    /* Direction */
    saveXMLDirection(doc);

    /* Run order */
    saveXMLRunOrder(doc);

    /* Algorithm */
    if (m_algorithm != NULL)
        m_algorithm->saveXML(doc);

    /* LEGACY - Dimmer Control */
    if (dimmerControl())
        doc->writeTextElement(KXMLQLCRGBMatrixDimmerControl, QString::number(dimmerControl()));

    /* Colors */
    for (int i = 0; i < m_rgbColors.count(); i++)
    {
        if (m_rgbColors.at(i).isValid() == false)
            continue;

        doc->writeStartElement(KXMLQLCRGBMatrixColor);
        doc->writeAttribute(KXMLQLCRGBMatrixColorIndex, QString::number(i));
        doc->writeCharacters(QString::number(m_rgbColors.at(i).rgb()));
        doc->writeEndElement();
    }

    /* Control Mode */
    doc->writeTextElement(KXMLQLCRGBMatrixControlMode, HUEMatrix::controlModeToString(m_controlMode));

    /* Fixture Group */
    doc->writeTextElement(KXMLQLCRGBMatrixFixtureGroup, QString::number(fixtureGroup()));

    /* Properties */
    QMapIterator<QString, QString> it(m_properties);
    while (it.hasNext())
    {
        it.next();
        doc->writeStartElement(KXMLQLCRGBMatrixProperty);
        doc->writeAttribute(KXMLQLCRGBMatrixPropertyName, it.key());
        doc->writeAttribute(KXMLQLCRGBMatrixPropertyValue, it.value());
        doc->writeEndElement();
    }

    /* Rotation & Mirror */
    if (m_rotation != 0)
        doc->writeTextElement(KXMLQLCRGBMatrixRotation, QString::number(m_rotation));
    if (m_mirror != 0)
        doc->writeTextElement(KXMLQLCRGBMatrixMirror, QString::number(m_mirror));
    if (m_mirrorBlend != MirrorFlip)
        doc->writeTextElement(KXMLQLCRGBMatrixMirrorBlend, mirrorBlendToString(m_mirrorBlend));
    if (m_brightness != 1.0)
        doc->writeTextElement(KXMLQLCRGBMatrixBrightness, QString::number(m_brightness));

    /* Beat Transform */
    if (m_beatEffect != BeatEffectOff)
    {
        doc->writeTextElement(KXMLQLCRGBMatrixBeatEffect, beatEffectToString(m_beatEffect));
        doc->writeTextElement(KXMLQLCRGBMatrixBeatSelection, beatSelectionToString(m_beatSelection));
        doc->writeTextElement(KXMLQLCRGBMatrixBeatOrientation, beatOrientationToString(m_beatOrientation));
    }

    /* End the <Function> tag */
    doc->writeEndElement();

    return true;
}

void HUEMatrix::preRun(MasterTimer *timer)
{
    {
        QMutexLocker algorithmLocker(&m_algorithmMutex);

        m_group = doc()->fixtureGroup(m_fixtureGroupID);
        if (m_group == NULL)
        {
            // No fixture group to control
            stop(FunctionParent::master());
            return;
        }

        if (m_algorithm != NULL)
        {
            checkEngineCreation();

            // Copy direction from parent class direction
            m_stepHandler->initializeDirection(direction(), m_rgbColors[0], m_rgbColors[1], m_stepsCount, m_runAlgorithm);

            // Update continuous phase when starting playback
            if (m_stepsCount > 0)
                m_continuousPhase = double(m_stepHandler->currentStepIndex()) / double(m_stepsCount);

            if (m_runAlgorithm->type() == RGBAlgorithm::Script)
            {
                RGBScript *script = static_cast<RGBScript*> (m_runAlgorithm);
                QMapIterator<QString, QString> it(m_properties);
                while (it.hasNext())
                {
                    it.next();
                    script->setProperty(it.key(), it.value());
                }
            }
            else if (m_runAlgorithm->type() == RGBAlgorithm::Image)
            {
                RGBImage *image = static_cast<RGBImage*> (m_runAlgorithm);
                if (image->animatedSource())
                    image->rewindAnimation();
            }
        }
    }

    m_roundTime.restart();

    // The resolved fixture group may have changed since last preRun; force the
    // PixelPlan to be rebuilt on the first updateMapChannels() call.
    m_pixelPlanDirty.storeRelease(1);

    // Phase 4: drop any precomputed map carried over from a previous run.
    m_currentGeneration.fetchAndAddRelaxed(1);
    m_precomputedReady.storeRelease(0);

    Function::preRun(timer);
}

bool HUEMatrix::advanceStep(MasterTimer *timer, quint32 &prevElapsed)
{
    // Refresh beat duration before any beat checks
    if (tempoType() == Beats)
        m_stepBeatDuration = beatsToTime(duration(), timer->beatTimeDuration());

    prevElapsed = elapsed();
    incrementElapsed();

    if (tempoType() == Time)
        return elapsed() >= duration() ? roundCheckLocked() : false;

    if (tempoType() != Beats)
        return false;

    if (timer->isBeat())
    {
        incrementElapsedBeats();
        if (elapsedBeats() % duration() != 0)
            return false;

        bool stepChanged = roundCheckLocked();
        resetElapsed();
        return stepChanged;
    }

    if (elapsed() >= m_stepBeatDuration && (uint)timer->timeToNextBeat() > m_stepBeatDuration / 16)
        return roundCheckLocked();

    return false;
}

int HUEMatrix::updateBeatPhase(MasterTimer *timer)
{
    int beatsPerBar = 4;

    if (m_beatEffect == BeatEffectOff)
        return beatsPerBar;

    AudioProfile *profile = doc()->audioProfile(doc()->activeAudioProfileId());
    if (profile == NULL)
        profile = doc()->defaultAudioProfile();
    AudioChannel *channel = (profile != NULL) ? profile->channel() : NULL;
    if (profile != NULL)
        beatsPerBar = std::clamp(profile->channelConfig().aubio.beatsPerBar, 1, 8);

    // Prefer the analysed bar phase, fall back to counting timer beats
    AudioSnapshot snap;
    if (channel != NULL)
        snap = channel->snapshot();

    if (channel != NULL && snap.music.barPhase > 0)
        m_currentBeat = int(snap.music.barPhase) % beatsPerBar;
    else if (timer->isBeat())
        m_currentBeat = (m_currentBeat + 1) % beatsPerBar;

    // Latch random segment on beat change
    if (m_beatSelection == BeatSelRandom && m_currentBeat != m_lastBeat)
    {
        m_randomSegment = QRandomGenerator::global()->bounded(beatsPerBar);
        m_lastBeat = m_currentBeat;
    }

    return beatsPerBar;
}

bool HUEMatrix::consumePrecomputedMap(const QSize &algoSize, uint stepColor,
                                      int stepIndex, quint32 generation)
{
    if (m_precomputedReady.loadAcquire() != 1)
        return false;

    QMutexLocker pre(&m_precomputedMutex);

    bool matches = m_precomputedReady.loadAcquire() == 1
                   && m_precomputedGeneration == generation
                   && m_precomputedAlgorithm == m_runAlgorithm
                   && m_precomputedStep == stepIndex
                   && m_precomputedColor == stepColor
                   && m_precomputedAlgoSize == algoSize;

    // Either consumed or stale: the slot is free again in both cases.
    if (matches)
    {
        m_stepHandler->m_map = std::move(m_precomputedMap);
        m_precomputedMap = RGBMap();
    }
    m_precomputedReady.storeRelease(0);

    return matches;
}

void HUEMatrix::write(MasterTimer *timer, QList<Universe *> universes)
{
    QMutexLocker algorithmLocker(&m_algorithmMutex);

    if (m_group == NULL)
    {
        // No fixture group to control
        stop(FunctionParent::master());
        return;
    }

    // No time to do anything.
    if (duration() == 0)
        return;

    if (m_algorithm != NULL && m_requestEngineCreation)
        checkEngineCreation();

    // Invalid/nonexistent script
    if (m_runAlgorithm == NULL || m_runAlgorithm->apiVersion() == 0)
        return;

    if (isPaused())
        return;

    quint32 prevElapsed = 0;
    bool stepChanged = advanceStep(timer, prevElapsed);
    int beatsPerBar = updateBeatPhase(timer);

    // Recompute when: step just changed, first tick of a step, audio-reactive,
    // or beat transform active. Otherwise the previous map still holds.
    bool needsRecompute = stepChanged
                          || prevElapsed < MasterTimer::tick()
                          || m_runAlgorithm->usesAudio()
                          || m_beatEffect != BeatEffectOff;
    if (!needsRecompute)
        return;

    QSize algoSize = effectiveAlgorithmSize(m_group);
    uint stepColor = m_stepHandler->stepColor().rgb();
    int stepIndex = m_stepHandler->currentStepIndex();
    quint32 generation = m_currentGeneration.loadAcquire();

    // A pre-computed frame already has rotation/mirror applied (those are
    // stable across ticks). The beat transform is never pre-computed because
    // m_currentBeat is only known on this thread, at this tick.
    if (consumePrecomputedMap(algoSize, stepColor, stepIndex, generation) == false)
    {
        if (m_runAlgorithm->usesAudio())
            m_runAlgorithm->setDisplaySize(m_group->size());
        m_runAlgorithm->rgbMap(algoSize, stepColor, stepIndex, m_stepHandler->m_map);
        if (m_rotation || m_mirror)
            applyTransforms(m_stepHandler->m_map, algoSize, m_group->size(),
                            m_rotation, m_mirror, m_mirrorBlend);
    }

    if (m_beatEffect != BeatEffectOff)
        applyBeatTransform(m_stepHandler->m_map, m_currentBeat, beatsPerBar);

    updateMapChannels(m_stepHandler->m_map, m_group, universes, timer->beatTimeDuration());

    // Kick off pre-computation for the NEXT tick, assuming it will use the
    // same step and colour (true for most ticks). If that assumption is wrong,
    // consumePrecomputedMap() detects the mismatch and falls back to the
    // synchronous path above.
    kickAsyncRgbMap(m_runAlgorithm, algoSize, stepColor, stepIndex, generation);
}

void HUEMatrix::postRun(MasterTimer *timer, QList<Universe *> universes)
{
    uint fadeout = overrideFadeOutSpeed() == defaultSpeed() ? fadeOutSpeed() : overrideFadeOutSpeed();

    /* If no fade out is needed, dismiss all the requested faders.
     * Otherwise, set all the faders to fade out and let Universe dismiss them
     * when done */
    if (fadeout == 0)
    {
        dismissAllFaders();
    }
    else
    {
        if (tempoType() == Beats)
            fadeout = beatsToTime(fadeout, timer->beatTimeDuration());

        foreach (QSharedPointer<GenericFader> fader, m_fadersMap)
        {
            if (!fader.isNull())
                fader->setFadeOut(true, fadeout);
        }
    }

    m_fadersMap.clear();

    {
        QMutexLocker algorithmLocker(&m_algorithmMutex);
        checkEngineCreation();
        if (m_runAlgorithm != NULL)
            m_runAlgorithm->postRun();
    }

    Function::postRun(timer, universes);
}

bool HUEMatrix::roundCheckLocked()
{
    if (m_algorithm == NULL)
        return false;

    bool advanced = m_stepHandler->checkNextStep(runOrder(), m_rgbColors[0], m_rgbColors[1], m_stepsCount);
    if (advanced == false)
    {
        stop(FunctionParent::master());
        return false;
    }

    // Update continuous phase based on current step index (prevents cumulative rounding errors)
    // This is analogous to how EFX uses m_currentAngle for phase scaling
    if (m_stepsCount > 0)
        m_continuousPhase = double(m_stepHandler->currentStepIndex()) / double(m_stepsCount);

    m_roundTime.restart();

    if (tempoType() == Beats)
        roundElapsed(m_stepBeatDuration);
    else
        roundElapsed(duration());

    return true;
}

void HUEMatrix::roundCheck()
{
    QMutexLocker algorithmLocker(&m_algorithmMutex);
    roundCheckLocked();
}

QSharedPointer<GenericFader> HUEMatrix::getFader(Universe *universe)
{
    // get the universe Fader first. If doesn't exist, create it
    if (universe == NULL)
        return QSharedPointer<GenericFader>();

    QSharedPointer<GenericFader> fader = m_fadersMap.value(universe->id(), QSharedPointer<GenericFader>());
    if (fader.isNull())
    {
        fader = universe->requestFader(Universe::blendModePriority(blendMode()));
        fader->adjustIntensity(getAttributeValue(Intensity));
        fader->setBlendMode(blendMode());
        fader->setName(name());
        fader->setParentFunctionID(id());
        m_fadersMap[universe->id()] = fader;
    }

    return fader;
}

void HUEMatrix::updateFaderValues(FadeChannel &fc, uchar value, uint fadeTime, uint fadeOutTime)
{
    fc.setStart(fc.current());
    fc.setTarget(value);
    fc.setElapsed(0);
    fc.setReady(false);
    // fade in/out depends on target value
    if (value == 0)
        fc.setFadeTime(fadeOutTime);
    else
        fc.setFadeTime(fadeTime);
}

void HUEMatrix::invalidatePixelPlan(quint32 id)
{
    Q_UNUSED(id)
    // Conservative: any fixture / fixture-group change may affect the resolved
    // channels. Marking dirty is cheap; the rebuild only happens on next tick.
    m_pixelPlanDirty.storeRelease(1);
}

void HUEMatrix::deferDeleteAlgorithm(RGBAlgorithm *algo)
{
    if (algo == nullptr)
        return;

#ifdef QT_QML_LIB
    // Script-typed algorithms own QJSValue handles tied to the JSThread's
    // QJSEngine. Even ignoring our async tasks, deleting them off-thread is
    // unsafe. Queue the delete on the JSThread so it serializes after any
    // pending precompute tasks (FIFO).
    if (algo->type() == RGBAlgorithm::Script
        && HUEScript::scheduleOnJSThread([algo]() { delete algo; }))
    {
        return;
    }
#endif
    delete algo;
}

void HUEMatrix::kickAsyncRgbMap(RGBAlgorithm *algo, const QSize &algoSize,
                                uint stepColor, int step, quint32 generation)
{
#ifdef QT_QML_LIB
    if (algo == nullptr)
        return;

    // Only Script algorithms benefit — others (Image, PlainColor, Text, Audio)
    // run inline on this thread already and are fast.
    if (algo->type() != RGBAlgorithm::Script)
        return;

    // Throttle: at most one async task in flight per matrix. If a previous
    // task hasn't been consumed yet, skip — the current frame's sync path is
    // already producing usable output.
    int expected = 0;
    if (!m_precomputedInFlight.testAndSetAcquire(expected, 1))
        return;

    // Capture by value: algo pointer + immutable params + generation snapshot.
    // We do NOT take m_algorithmMutex inside the JSThread task — that would
    // deadlock with the synchronous rgbMap path (MasterTimer holds the mutex
    // while BlockingQueuedConnection-waiting on JSThread).
    //
    // Lifetime safety:
    //   - algo pointer remains valid as long as any in-flight task could be
    //     using it: setAlgorithm() / ~RGBMatrix() use deferDeleteAlgorithm()
    //     which queues the delete on the JSThread *after* this task (FIFO).
    //   - `this` remains valid because ~RGBMatrix() spins on
    //     m_precomputedInFlight before destroying members.
    QSize captureAlgoSize = algoSize;
    int captureRotation = m_rotation;
    int captureMirror = m_mirror;
    MirrorBlend captureBlend = m_mirrorBlend;
    QSize captureGroupSize;
    if (m_group != nullptr)
        captureGroupSize = m_group->size();
    bool captureUsesAudio = algo->usesAudio();

    bool ok = HUEScript::scheduleOnJSThread(
        [this, algo, captureAlgoSize, stepColor, step, generation,
         captureRotation, captureMirror, captureBlend, captureGroupSize,
         captureUsesAudio]()
        {
            // Re-check generation BEFORE we even call rgbMap. Cheap and lets
            // us bail out if the matrix was reconfigured while we were queued.
            if (m_currentGeneration.loadAcquire() != generation)
            {
                m_precomputedInFlight.storeRelease(0);
                return;
            }

            if (captureUsesAudio && !captureGroupSize.isEmpty())
                algo->setDisplaySize(captureGroupSize);

            RGBMap localMap;
            algo->rgbMap(captureAlgoSize, stepColor, step, localMap);

            if ((captureRotation || captureMirror) && !captureGroupSize.isEmpty())
                applyTransforms(localMap, captureAlgoSize, captureGroupSize,
                                captureRotation, captureMirror, captureBlend);

            // Re-check generation AFTER computation. If a parameter changed
            // mid-flight, drop the result rather than poison the cache.
            if (m_currentGeneration.loadAcquire() != generation)
            {
                m_precomputedInFlight.storeRelease(0);
                return;
            }

            {
                QMutexLocker pre(&m_precomputedMutex);
                m_precomputedMap = std::move(localMap);
                m_precomputedAlgoSize = captureAlgoSize;
                m_precomputedStep = step;
                m_precomputedColor = stepColor;
                m_precomputedGeneration = generation;
                m_precomputedAlgorithm = algo;
            }
            m_precomputedReady.storeRelease(1);
            m_precomputedInFlight.storeRelease(0);
        });

    if (!ok)
        m_precomputedInFlight.storeRelease(0);
#else
    Q_UNUSED(algo)
    Q_UNUSED(algoSize)
    Q_UNUSED(stepColor)
    Q_UNUSED(step)
    Q_UNUSED(generation)
#endif
}

void HUEMatrix::rebuildPixelPlan(const FixtureGroup *grp)
{
    m_pixelPlan.clear();
    m_pixelPlanGroup = grp;
    m_pixelPlanDirty.storeRelease(0);

    if (grp == NULL)
        return;

    Doc *d = doc();

    QMapIterator<QLCPoint, GroupHead> it(grp->headsMap());
    while (it.hasNext())
    {
        it.next();
        const QLCPoint &pt = it.key();
        const GroupHead &grpHead = it.value();

        Fixture *fxi = d->fixture(grpHead.fxi);
        if (fxi == NULL)
            continue;

        QLCFixtureHead head = fxi->head(grpHead.head);
        const quint32 absAddress = fxi->universeAddress();
        const quint32 fxID = grpHead.fxi;
        const quint16 px = quint16(pt.x());
        const quint16 py = quint16(pt.y());

        auto addEntry = [&](quint32 ch, PixelValueSource src) {
            if (ch == QLCChannel::invalid())
                return;
            PixelPlanEntry e;
            e.x = px;
            e.y = py;
            e.universeIndex = quint32((absAddress + ch) / 512);
            e.fixtureID = fxID;
            e.channel = ch;
            e.source = src;
            m_pixelPlan.append(e);
        };

        if (m_controlMode == ControlModeRgb)
        {
            QVector<quint32> chList = head.rgbChannels();
            if (chList.size() == 3)
            {
                addEntry(chList.at(0), VS_Red);
                addEntry(chList.at(1), VS_Green);
                addEntry(chList.at(2), VS_Blue);
            }
            else
            {
                chList = head.cmyChannels();
                if (chList.size() == 3)
                {
                    addEntry(chList.at(0), VS_Cyan);
                    addEntry(chList.at(1), VS_Magenta);
                    addEntry(chList.at(2), VS_Yellow);
                }
            }
        }
        else if (m_controlMode == ControlModeRgbw || m_controlMode == ControlModeRgbwBrighter)
        {
            const bool subtractWhite = (m_controlMode == ControlModeRgbw);
            quint32 rCh = head.channelNumber(QLCChannel::Red, QLCChannel::MSB);
            quint32 gCh = head.channelNumber(QLCChannel::Green, QLCChannel::MSB);
            quint32 bCh = head.channelNumber(QLCChannel::Blue, QLCChannel::MSB);
            quint32 wCh = head.channelNumber(QLCChannel::White, QLCChannel::MSB);

            if (rCh != QLCChannel::invalid() && gCh != QLCChannel::invalid() && bCh != QLCChannel::invalid())
            {
                addEntry(rCh, subtractWhite ? VS_RedSubW : VS_Red);
                addEntry(gCh, subtractWhite ? VS_GreenSubW : VS_Green);
                addEntry(bCh, subtractWhite ? VS_BlueSubW : VS_Blue);

                if (wCh != QLCChannel::invalid())
                    addEntry(wCh, VS_White);

                if (m_dimmerControl)
                {
                    quint32 masterDim = fxi->masterIntensityChannel();
                    quint32 headDim = head.channelNumber(QLCChannel::Intensity, QLCChannel::MSB);

                    if (masterDim != QLCChannel::invalid())
                        addEntry(masterDim, VS_Grey);

                    if (headDim != QLCChannel::invalid() && headDim != masterDim)
                        addEntry(headDim, VS_GreyOrFull);
                }
            }
        }
        else if (m_controlMode == ControlModeShutter)
        {
            QVector<quint32> chList = head.shutterChannels();
            if (chList.size())
                addEntry(chList.first(), VS_Grey);
        }
        else if (m_controlMode == ControlModeDimmer || m_dimmerControl)
        {
            quint32 masterDim = fxi->masterIntensityChannel();
            quint32 headDim = head.channelNumber(QLCChannel::Intensity, QLCChannel::MSB);

            if (masterDim != QLCChannel::invalid())
                addEntry(masterDim, VS_Grey);

            if (headDim != QLCChannel::invalid() && headDim != masterDim)
                addEntry(headDim, masterDim != QLCChannel::invalid() ? VS_GreyOrFull : VS_Grey);
        }
        else
        {
            quint32 ch = QLCChannel::invalid();
            if (m_controlMode == ControlModeWhite)
                ch = head.channelNumber(QLCChannel::White, QLCChannel::MSB);
            else if (m_controlMode == ControlModeAmber)
                ch = head.channelNumber(QLCChannel::Amber, QLCChannel::MSB);
            else if (m_controlMode == ControlModeUV)
                ch = head.channelNumber(QLCChannel::UV, QLCChannel::MSB);

            addEntry(ch, VS_Grey);
        }
    }

    // Sort by (universeIndex, y, x, channel) so the per-tick loop can amortize
    // the getFader() lookup across consecutive entries from the same universe,
    // and benefit from per-pixel scratch reuse within a universe.
    std::sort(m_pixelPlan.begin(), m_pixelPlan.end(),
              [](const PixelPlanEntry &a, const PixelPlanEntry &b) {
                  if (a.universeIndex != b.universeIndex) return a.universeIndex < b.universeIndex;
                  if (a.y != b.y) return a.y < b.y;
                  if (a.x != b.x) return a.x < b.x;
                  return a.channel < b.channel;
              });
}

void HUEMatrix::updateMapChannels(const RGBMap& map, const FixtureGroup *grp, QList<Universe *> universes, int beatDuration)
{
    uint fadeTime = (overrideFadeInSpeed() == defaultSpeed()) ? fadeInSpeed() : overrideFadeInSpeed();
    uint fadeOutTime = (overrideFadeOutSpeed() == defaultSpeed()) ? fadeOutSpeed() : overrideFadeOutSpeed();

    if (tempoType() == Beats)
    {
        fadeTime = beatsToTime(fadeTime, beatDuration);
        fadeOutTime = beatsToTime(fadeOutTime, beatDuration);
    }

    // Rebuild the pre-resolved channel plan whenever it has been invalidated
    // (fixture group / control mode / dimmer control / fixture address change).
    if (m_pixelPlanDirty.loadAcquire() || m_pixelPlanGroup != grp)
        rebuildPixelPlan(grp);

    if (m_pixelPlan.isEmpty())
        return;

    Doc *d = doc();
    const int mapH = map.count();
    const int universeCount = universes.size();

    quint32 lastUni = UINT_MAX;
    Universe *cachedUniverse = nullptr;
    QSharedPointer<GenericFader> cachedFader;

    quint16 lastX = 0xFFFF;
    quint16 lastY = 0xFFFF;
    bool pixelValid = false;
    uchar r = 0, g = 0, b = 0, w = 0, grey = 0;
    QColor cmyCol;

    for (int i = 0, n = m_pixelPlan.size(); i < n; ++i)
    {
        const PixelPlanEntry &e = m_pixelPlan.at(i);

        // Per-pixel scratch values. Recomputed only when (x, y) changes,
        // which (after sorting) happens once per pixel within a universe.
        if (e.x != lastX || e.y != lastY)
        {
            lastX = e.x;
            lastY = e.y;
            pixelValid = (int(e.y) < mapH && int(e.x) < map.at(e.y).count());
            if (pixelValid)
            {
                uint col = map.at(e.y).at(e.x);
                r = uchar(qRed(col));
                g = uchar(qGreen(col));
                b = uchar(qBlue(col));
                w = qMin(r, qMin(g, b));
                grey = rgbToGrey(col);
                cmyCol = QColor::fromRgb(col);

                if (m_brightness != 1.0)
                {
                    r = uchar(qBound(0, int(r * m_brightness), 255));
                    g = uchar(qBound(0, int(g * m_brightness), 255));
                    b = uchar(qBound(0, int(b * m_brightness), 255));
                    w = uchar(qBound(0, int(w * m_brightness), 255));
                    grey = uchar(qBound(0, int(grey * m_brightness), 255));
                    cmyCol = QColor::fromRgb(qBound(0, int(qRed(cmyCol.rgb()) * m_brightness), 255),
                                             qBound(0, int(qGreen(cmyCol.rgb()) * m_brightness), 255),
                                             qBound(0, int(qBlue(cmyCol.rgb()) * m_brightness), 255));
                }
            }
        }

        if (!pixelValid)
            continue;

        // Universe / fader lookup is amortized across consecutive entries from
        // the same universe thanks to the sort in rebuildPixelPlan().
        if (e.universeIndex != lastUni)
        {
            lastUni = e.universeIndex;
            if (int(e.universeIndex) >= universeCount)
            {
                cachedUniverse = nullptr;
                cachedFader.clear();
            }
            else
            {
                cachedUniverse = universes.at(e.universeIndex);
                cachedFader = getFader(cachedUniverse);
            }
        }

        if (cachedFader.isNull())
            continue;

        uchar value = 0;
        switch (e.source)
        {
            case VS_Red:        value = r; break;
            case VS_Green:      value = g; break;
            case VS_Blue:       value = b; break;
            case VS_Cyan:       value = uchar(cmyCol.cyan()); break;
            case VS_Magenta:    value = uchar(cmyCol.magenta()); break;
            case VS_Yellow:     value = uchar(cmyCol.yellow()); break;
            case VS_White:      value = w; break;
            case VS_RedSubW:    value = uchar(r - w); break;
            case VS_GreenSubW:  value = uchar(g - w); break;
            case VS_BlueSubW:   value = uchar(b - w); break;
            case VS_Grey:       value = grey; break;
            case VS_GreyOrFull: value = grey == 0 ? 0 : 255; break;
        }

        cachedFader->updateChannel(d, cachedUniverse, e.fixtureID, e.channel,
            [this, value, fadeTime, fadeOutTime](FadeChannel &fc)
        {
            updateFaderValues(fc, value, fadeTime, fadeOutTime);
        });
    }
}



void HUEMatrix::setControlMode(RGBMatrix::ControlMode mode)
{
    m_controlMode = mode;
    m_pixelPlanDirty.storeRelease(1);
    // Control mode affects how rgbMap output is interpreted, but not the
    // rgbMap output itself. Bump the generation defensively to avoid any
    // edge case where the script could observe controlMode.
    m_currentGeneration.fetchAndAddRelaxed(1);
    m_precomputedReady.storeRelease(0);
    emit changed(id());
}

QString HUEMatrix::controlModeToString(RGBMatrix::ControlMode mode)
{
    switch (mode)
    {
        case RGBMatrix::ControlModeRgbw:
            return QString(KXMLQLCRGBMatrixControlModeRgbw);
        case RGBMatrix::ControlModeRgbwBrighter:
            return QString(KXMLQLCRGBMatrixControlModeRgbwBrighter);
        default:
            return RGBMatrix::controlModeToString(mode);
    }
}

RGBMatrix::ControlMode HUEMatrix::stringToControlMode(QString mode)
{
    if (mode.compare(KXMLQLCRGBMatrixControlModeRgb, Qt::CaseInsensitive) == 0)
        return ControlModeRgb;
    else if (mode.compare(KXMLQLCRGBMatrixControlModeAmber, Qt::CaseInsensitive) == 0)
        return ControlModeAmber;
    else if (mode.compare(KXMLQLCRGBMatrixControlModeWhite, Qt::CaseInsensitive) == 0)
        return ControlModeWhite;
    else if (mode.compare(KXMLQLCRGBMatrixControlModeUV, Qt::CaseInsensitive) == 0)
        return ControlModeUV;
    else if (mode.compare(KXMLQLCRGBMatrixControlModeDimmer, Qt::CaseInsensitive) == 0)
        return ControlModeDimmer;
    else if (mode.compare(KXMLQLCRGBMatrixControlModeShutter, Qt::CaseInsensitive) == 0)
        return ControlModeShutter;
    else if (mode.compare(KXMLQLCRGBMatrixControlModeRgbw, Qt::CaseInsensitive) == 0)
        return ControlModeRgbw;
    else if (mode.compare(KXMLQLCRGBMatrixControlModeRgbwBrighter, Qt::CaseInsensitive) == 0)
        return ControlModeRgbwBrighter;

    return ControlModeRgb;
}

int HUEMatrix::rotation() const
{
    return m_rotation;
}

void HUEMatrix::setRotation(int r)
{
    m_rotation = r & 3;
    m_currentGeneration.fetchAndAddRelaxed(1);
    m_precomputedReady.storeRelease(0);
    emit changed(id());
}

int HUEMatrix::mirror() const
{
    return m_mirror;
}

void HUEMatrix::setMirror(int m)
{
    m_mirror = m & 3;
    m_currentGeneration.fetchAndAddRelaxed(1);
    m_precomputedReady.storeRelease(0);
    emit changed(id());
}

HUEMatrix::MirrorBlend HUEMatrix::mirrorBlend() const
{
    return m_mirrorBlend;
}

void HUEMatrix::setMirrorBlend(MirrorBlend b)
{
    if (b < MirrorFlip || b > MirrorAdditive)
        b = MirrorFlip;
    m_mirrorBlend = b;
    m_currentGeneration.fetchAndAddRelaxed(1);
    m_precomputedReady.storeRelease(0);
    emit changed(id());
}

qreal HUEMatrix::brightness() const
{
    return m_brightness;
}

void HUEMatrix::setBrightness(qreal b)
{
    m_brightness = qMax(0.0, b);
    emit changed(id());
}

QSize HUEMatrix::effectiveAlgorithmSize() const
{
    if (m_group == NULL)
        return QSize();
    return effectiveAlgorithmSize(m_group);
}

QSize HUEMatrix::effectiveAlgorithmSize(const FixtureGroup *grp) const
{
    QSize s = grp->size();
    if (m_rotation == 1 || m_rotation == 3)
        s = QSize(s.height(), s.width());
    return s;
}

QString HUEMatrix::mirrorBlendToString(MirrorBlend b)
{
    switch (b)
    {
        case MirrorMax: return QStringLiteral("Max");
        case MirrorAverage: return QStringLiteral("Average");
        case MirrorAdditive: return QStringLiteral("Additive");
        default: return QStringLiteral("Flip");
    }
}

HUEMatrix::MirrorBlend HUEMatrix::stringToMirrorBlend(const QString &s)
{
    if (s.compare(QStringLiteral("Max"), Qt::CaseInsensitive) == 0) return MirrorMax;
    if (s.compare(QStringLiteral("Average"), Qt::CaseInsensitive) == 0) return MirrorAverage;
    if (s.compare(QStringLiteral("Additive"), Qt::CaseInsensitive) == 0) return MirrorAdditive;
    return MirrorFlip;
}

static inline uint blendPixels(uint a, uint b, HUEMatrix::MirrorBlend blend)
{
    uint ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    uint br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    uint r, g, bl;

    switch (blend)
    {
        case HUEMatrix::MirrorMax:
            r = qMax(ar, br); g = qMax(ag, bg); bl = qMax(ab, bb);
            break;
        case HUEMatrix::MirrorAverage:
            r = (ar + br) / 2; g = (ag + bg) / 2; bl = (ab + bb) / 2;
            break;
        case HUEMatrix::MirrorAdditive:
            r = qMin(255u, ar + br); g = qMin(255u, ag + bg); bl = qMin(255u, ab + bb);
            break;
        default: // MirrorFlip — caller handles this case directly
            return a;
    }
    return (r << 16) | (g << 8) | bl;
}

void HUEMatrix::applyTransforms(RGBMap &map, const QSize & /* srcSize */, const QSize &dstSize,
                                int rotation, int mirror, MirrorBlend blend)
{
    int dw = dstSize.width();
    int dh = dstSize.height();

    // Source dimensions from the actual map data
    int sh = map.size();
    int sw = (sh > 0) ? map[0].size() : 0;

    // Step 1: Rotation — build a new map with destination dimensions
    RGBMap rotated;
    rotated.resize(dh);
    for (int y = 0; y < dh; y++)
    {
        rotated[y].resize(dw);
        rotated[y].fill(0);
    }

    for (int dy = 0; dy < dh; dy++)
    {
        for (int dx = 0; dx < dw; dx++)
        {
            int sx, sy;
            switch (rotation)
            {
                case 0: // 0°: identity
                    sx = dx; sy = dy;
                    break;
                case 1: // 90° CW
                    sx = dy; sy = sh - 1 - dx;
                    break;
                case 2: // 180°
                    sx = dw - 1 - dx; sy = dh - 1 - dy;
                    break;
                case 3: // 270° CW
                    sx = sw - 1 - dy; sy = dx;
                    break;
                default:
                    sx = dx; sy = dy;
                    break;
            }

            if (sy >= 0 && sy < sh && sx >= 0 && sx < (int)map[sy].size())
                rotated[dy][dx] = map[sy][sx];
        }
    }

    // Step 2: Mirror
    if (mirror & 1) // Horizontal — mirror placed on vertical center line
    {
        if (blend == MirrorFlip)
        {
            // Pure flip: left half is source, copied to right half
            for (int y = 0; y < dh; y++)
                for (int x = 0; x < dw / 2; x++)
                    rotated[y][dw - 1 - x] = rotated[y][x];
        }
        else
        {
            for (int y = 0; y < dh; y++)
            {
                for (int x = 0; x < dw / 2; x++)
                {
                    int mx = dw - 1 - x;
                    uint merged = blendPixels(rotated[y][x], rotated[y][mx], blend);
                    rotated[y][x] = merged;
                    rotated[y][mx] = merged;
                }
            }
        }
    }

    if (mirror & 2) // Vertical — mirror placed on horizontal center line
    {
        if (blend == MirrorFlip)
        {
            // Pure flip: top half is source, copied to bottom half
            for (int y = 0; y < dh / 2; y++)
                for (int x = 0; x < dw; x++)
                    rotated[dh - 1 - y][x] = rotated[y][x];
        }
        else
        {
            for (int y = 0; y < dh / 2; y++)
            {
                int my = dh - 1 - y;
                for (int x = 0; x < dw; x++)
                {
                    uint merged = blendPixels(rotated[y][x], rotated[my][x], blend);
                    rotated[y][x] = merged;
                    rotated[my][x] = merged;
                }
            }
        }
    }

    map = rotated;
}

HUEMatrix::BeatEffect HUEMatrix::beatEffect() const
{
    return m_beatEffect;
}

void HUEMatrix::setBeatEffect(BeatEffect e)
{
    m_beatEffect = e;
}

HUEMatrix::BeatSelection HUEMatrix::beatSelection() const
{
    return m_beatSelection;
}

void HUEMatrix::setBeatSelection(BeatSelection s)
{
    m_beatSelection = s;
}

HUEMatrix::BeatOrientation HUEMatrix::beatOrientation() const
{
    return m_beatOrientation;
}

void HUEMatrix::setBeatOrientation(BeatOrientation o)
{
    m_beatOrientation = o;
}

QString HUEMatrix::beatEffectToString(BeatEffect e)
{
    switch (e)
    {
        case BeatEffectMirror:      return QStringLiteral("Mirror");
        case BeatEffectColorInvert: return QStringLiteral("ColorInvert");
        case BeatEffectBlackout:    return QStringLiteral("Blackout");
        case BeatEffectWhiteout:    return QStringLiteral("Whiteout");
        default:                    return QStringLiteral("Off");
    }
}

HUEMatrix::BeatEffect HUEMatrix::stringToBeatEffect(const QString &s)
{
    if (s.compare(QStringLiteral("Mirror"), Qt::CaseInsensitive) == 0)      return BeatEffectMirror;
    if (s.compare(QStringLiteral("ColorInvert"), Qt::CaseInsensitive) == 0) return BeatEffectColorInvert;
    if (s.compare(QStringLiteral("Blackout"), Qt::CaseInsensitive) == 0)    return BeatEffectBlackout;
    if (s.compare(QStringLiteral("Whiteout"), Qt::CaseInsensitive) == 0)    return BeatEffectWhiteout;
    return BeatEffectOff;
}

QString HUEMatrix::beatSelectionToString(BeatSelection s)
{
    switch (s)
    {
        case BeatSelWalk:    return QStringLiteral("Walk");
        case BeatSelRandom:  return QStringLiteral("Random");
        default:             return QStringLiteral("AllOnDownbeat");
    }
}

HUEMatrix::BeatSelection HUEMatrix::stringToBeatSelection(const QString &s)
{
    if (s.compare(QStringLiteral("Walk"), Qt::CaseInsensitive) == 0)   return BeatSelWalk;
    if (s.compare(QStringLiteral("Random"), Qt::CaseInsensitive) == 0) return BeatSelRandom;
    return BeatSelAllOnDownbeat;
}

QString HUEMatrix::beatOrientationToString(BeatOrientation o)
{
    switch (o)
    {
        case BeatOrientColumns: return QStringLiteral("Columns");
        default:                return QStringLiteral("Rows");
    }
}

HUEMatrix::BeatOrientation HUEMatrix::stringToBeatOrientation(const QString &s)
{
    if (s.compare(QStringLiteral("Columns"), Qt::CaseInsensitive) == 0) return BeatOrientColumns;
    return BeatOrientRows;
}

void HUEMatrix::segmentRange(int segment, int total, int segmentsCount, int &start, int &end)
{
    if (segmentsCount < 1) segmentsCount = 1;
    int base = total / segmentsCount;
    int remainder = total % segmentsCount;
    start = segment * base + qMin(segment, remainder);
    end = start + base + (segment < remainder ? 1 : 0);
}

void HUEMatrix::applyBeatTransform(RGBMap &map, int currentBeat, int beatsPerBar)
{
    if (m_beatEffect == BeatEffectOff)
        return;

    int rows = map.size();
    if (rows == 0) return;
    int cols = map[0].size();
    if (cols == 0) return;

    int total = (m_beatOrientation == BeatOrientRows) ? rows : cols;
    if (beatsPerBar < 1) beatsPerBar = 1;

    // Determine affected segments
    QVector<int> affected;
    if (m_beatSelection == BeatSelAllOnDownbeat)
    {
        if (currentBeat != beatsPerBar - 1) return;
        affected.reserve(beatsPerBar);
        for (int i = 0; i < beatsPerBar; i++) affected.append(i);
    }
    else if (m_beatSelection == BeatSelWalk)
    {
        affected = {currentBeat % beatsPerBar};
    }
    else // BeatSelRandom
    {
        affected = {m_randomSegment % beatsPerBar};
    }

    for (int seg : affected)
    {
        int start, end;
        segmentRange(seg, total, beatsPerBar, start, end);

        switch (m_beatEffect)
        {
            case BeatEffectMirror:
                if (m_beatOrientation == BeatOrientRows)
                {
                    for (int y = start; y < end; y++)
                        std::reverse(map[y].begin(), map[y].end());
                }
                else
                {
                    for (int y = 0; y < rows; y++)
                        std::reverse(map[y].begin() + start, map[y].begin() + end);
                }
                break;

            case BeatEffectColorInvert:
                if (m_beatOrientation == BeatOrientRows)
                {
                    for (int y = start; y < end; y++)
                        for (int x = 0; x < cols; x++)
                            map[y][x] ^= 0x00FFFFFF;
                }
                else
                {
                    for (int y = 0; y < rows; y++)
                        for (int x = start; x < end; x++)
                            map[y][x] ^= 0x00FFFFFF;
                }
                break;

            case BeatEffectBlackout:
                if (m_beatOrientation == BeatOrientRows)
                {
                    for (int y = start; y < end; y++)
                        map[y].fill(0);
                }
                else
                {
                    for (int y = 0; y < rows; y++)
                        for (int x = start; x < end; x++)
                            map[y][x] = 0;
                }
                break;

            case BeatEffectWhiteout:
                if (m_beatOrientation == BeatOrientRows)
                {
                    for (int y = start; y < end; y++)
                        map[y].fill(0x00FFFFFF);
                }
                else
                {
                    for (int y = 0; y < rows; y++)
                        for (int x = start; x < end; x++)
                            map[y][x] = 0x00FFFFFF;
                }
                break;

            default:
                break;
        }
    }
}
