/*
  Q Light Controller Plus
  rgbmatrix.cpp

  Copyright (c) Heikki Junnila
                Massimo Callegari

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
#include <QDebug>
#include <QRandomGenerator>
#include <QThread>
#include <cmath>
#include <QDir>

#include "rgbscriptscache.h"
#include "qlcfixturehead.h"
#include "audiochannel.h"
#include "fixturegroup.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "rgbmatrix.h"
#include "rgbimage.h"
#include "doc.h"

#define KXMLQLCRGBMatrixStartColor      QStringLiteral("MonoColor")
#define KXMLQLCRGBMatrixEndColor        QStringLiteral("EndColor")
#define KXMLQLCRGBMatrixColor           QStringLiteral("Color")
#define KXMLQLCRGBMatrixColorIndex      QStringLiteral("Index")

#define KXMLQLCRGBMatrixFixtureGroup    QStringLiteral("FixtureGroup")
#define KXMLQLCRGBMatrixAudioProfileID  QStringLiteral("AudioProfileID")
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

#define KXMLQLCRGBMatrixBeatEffect          QStringLiteral("BeatEffect")
#define KXMLQLCRGBMatrixBeatSelection       QStringLiteral("BeatSelection")
#define KXMLQLCRGBMatrixBeatOrientation     QStringLiteral("BeatOrientation")

#define KXMLQLCRGBMatrixAudioRouting        QStringLiteral("AudioRouting")
#define KXMLQLCRGBMatrixAudioRoutingLow     QStringLiteral("low")
#define KXMLQLCRGBMatrixAudioRoutingMid     QStringLiteral("mid")
#define KXMLQLCRGBMatrixAudioRoutingHigh    QStringLiteral("high")
#define KXMLQLCRGBMatrixAudioRoutingBeat    QStringLiteral("beat")
#define KXMLQLCRGBMatrixAudioRoutingKick    QStringLiteral("kick")
#define KXMLQLCRGBMatrixAudioRoutingOnset   QStringLiteral("onset")

static const int RGBMatrixColorMask = 0x00FFFFFF;

/****************************************************************************
 * Initialization
 ****************************************************************************/

RGBMatrix::RGBMatrix(Doc *doc)
    : Function(doc, Function::RGBMatrixType)
    , m_dimmerControl(false)
    , m_fixtureGroupID(FixtureGroup::invalidId())
    , m_group(NULL)
    , m_requestEngineCreation(true)
    , m_runAlgorithm(NULL)
    , m_algorithm(NULL)
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
    , m_algorithmMutex(QMutex::Recursive)
#endif
    , m_stepHandler(new RGBMatrixStep())
    , m_stepsCount(0)
    , m_stepBeatDuration(0)
    , m_applyingStyleAttributes(false)
    , m_controlMode(RGBMatrix::ControlModeRgb)
    , m_rotation(0)
    , m_mirror(0)
    , m_mirrorBlend(MirrorFlip)
    , m_beatEffect(BeatEffectOff)
    , m_beatSelection(BeatSelAllBeat4)
    , m_beatOrientation(BeatOrientRows)
    , m_currentBeat(0)
    , m_lastBeat(-1)
    , m_randomSegment(0)
{
    // Phase 4: precomputed-map state starts empty. m_currentGeneration begins
    // at 0; consumers only trust the precomputed map when ready==1, so the
    // initial value of m_precomputedGeneration is harmless until first kick.
    m_precomputedReady.storeRelease(0);
    m_precomputedInFlight.storeRelease(0);
    m_currentGeneration.storeRelease(0);
    m_precomputedGeneration = 0;
    m_precomputedStep = -1;
    m_precomputedColor = 0;
    m_precomputedAlgorithm = nullptr;

    setName(tr("New RGB Matrix"));
    setDuration(500);

    m_rgbColors.fill(QColor(), RGBAlgorithmColorDisplayCount);
    setColor(0, Qt::red);

    setAlgorithm(RGBAlgorithm::algorithm(doc, "Stripes"));

    for (int i = 0; i < ColorAttributeCount; ++i)
    {
        registerAttribute(tr("Color %1").arg(i + 1), LastWins | Single, -1.0, 16777215.0,
                          getColor(i).isValid() ? int(getColor(i).rgb() & RGBMatrixColorMask) : -1);
    }

    int algoCount = RGBAlgorithm::algorithms(doc).count();
    registerAttribute(tr("Pattern"), LastWins | Single, 0.0, algoCount > 0 ? algoCount - 1 : 0, algorithmIndex());

    // Mark the PixelPlan dirty initially. It will be (re)built lazily on the
    // first updateMapChannels() call. Connect to Doc signals so that fixture
    // address/mode changes and group membership changes invalidate the cache.
    m_pixelPlanDirty.storeRelease(1);
    if (doc != NULL)
    {
        connect(doc, &Doc::fixtureChanged,
                this, &RGBMatrix::invalidatePixelPlan, Qt::DirectConnection);
        connect(doc, &Doc::fixtureRemoved,
                this, &RGBMatrix::invalidatePixelPlan, Qt::DirectConnection);
        connect(doc, &Doc::fixtureGroupChanged,
                this, &RGBMatrix::invalidatePixelPlan, Qt::DirectConnection);
        connect(doc, &Doc::fixtureGroupRemoved,
                this, &RGBMatrix::invalidatePixelPlan, Qt::DirectConnection);
    }
}

RGBMatrix::~RGBMatrix()
{
    // Phase 4: any async precompute task captures `this`. Wait for it to
    // drain before we tear down. Bounded to ~one rgbMap duration (~10ms).
    while (m_precomputedInFlight.loadAcquire() != 0)
        QThread::yieldCurrentThread();

    //if (m_runAlgorithm != NULL)
    //    delete m_runAlgorithm;
    deferDeleteAlgorithm(m_algorithm);
    m_algorithm = NULL;
    delete m_stepHandler;
}

QIcon RGBMatrix::getIcon() const
{
    return QIcon(":/rgbmatrix.png");
}

int RGBMatrix::algorithmIndex() const
{
    if (m_algorithm == NULL || doc() == NULL)
        return 0;

    QStringList algoList = RGBAlgorithm::algorithms(doc());
    int idx = algoList.indexOf(m_algorithm->name());
    return idx >= 0 ? idx : 0;
}

void RGBMatrix::setTotalDuration(quint32 msec)
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

quint32 RGBMatrix::totalDuration()
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

void RGBMatrix::setDimmerControl(bool dimmerControl)
{
    m_dimmerControl = dimmerControl;
    m_pixelPlanDirty.storeRelease(1);
}

bool RGBMatrix::dimmerControl() const
{
    return m_dimmerControl;
}

/****************************************************************************
 * Copying
 ****************************************************************************/

Function* RGBMatrix::createCopy(Doc* doc, bool addToDoc)
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

bool RGBMatrix::copyFrom(const Function* function)
{
    const RGBMatrix *mtx = qobject_cast<const RGBMatrix*> (function);
    if (mtx == NULL)
        return false;

    setDimmerControl(mtx->dimmerControl());
    setFixtureGroup(mtx->fixtureGroup());
    setAudioProfileId(mtx->audioProfileId());

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
    setBeatEffect(mtx->beatEffect());
    setBeatSelection(mtx->beatSelection());
    setBeatOrientation(mtx->beatOrientation());
    setAudioRouting(mtx->audioRouting());

    return Function::copyFrom(function);
}

/****************************************************************************
 * Fixtures
 ****************************************************************************/

quint32 RGBMatrix::fixtureGroup() const
{
    return m_fixtureGroupID;
}

void RGBMatrix::setFixtureGroup(quint32 id)
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

quint32 RGBMatrix::audioProfileId() const
{
    return m_audioProfileId;
}

void RGBMatrix::setAudioProfileId(quint32 id)
{
    if (m_audioProfileId == id)
        return;

    m_audioProfileId = id;
    emit audioProfileIdChanged();
}

QList<quint32> RGBMatrix::components() const
{
    if (m_group != NULL)
        return m_group->fixtureList();

    return QList<quint32>();
}

/****************************************************************************
 * Algorithm
 ****************************************************************************/

void RGBMatrix::setAlgorithm(RGBAlgorithm *algo)
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

            QVector<uint> colors = script->rgbMapGetColors();
            for (int i = 0; i < colors.count(); i++)
                m_rgbColors.replace(i, QColor::fromRgb(colors.at(i)));
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

RGBAlgorithm *RGBMatrix::algorithm() const
{
    return m_algorithm;
}

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
QMutex& RGBMatrix::algorithmMutex()
{
    return m_algorithmMutex;
}
#else
QRecursiveMutex& RGBMatrix::algorithmMutex()
{
    return m_algorithmMutex;
}
#endif


int RGBMatrix::stepsCount() const
{
    return m_stepsCount;
}

int RGBMatrix::algorithmStepsCount()
{
    QMutexLocker algorithmLocker(&m_algorithmMutex);

    if (m_algorithm == NULL)
        return 0;

    FixtureGroup *grp = doc()->fixtureGroup(fixtureGroup());
    if (grp != NULL)
        return m_algorithm->rgbMapStepCount(effectiveAlgorithmSize(grp));

    return 0;
}

void RGBMatrix::previewMap(int step, RGBMatrixStep *handler)
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

/****************************************************************************
 * Color
 ****************************************************************************/

void RGBMatrix::setColor(int i, QColor c)
{
    if (i < 0)
        return;

    if (i >= m_rgbColors.count())
        m_rgbColors.resize(i + 1);

    m_rgbColors.replace(i, c);
    {
        QMutexLocker algorithmLocker(&m_algorithmMutex);
        if (m_algorithm != NULL)
        {
            m_algorithm->setColors(m_rgbColors);
            updateColorDelta();
        }
    }
    setMapColors(m_algorithm);

    if (m_applyingStyleAttributes == false && i >= 0 && i < ColorAttributeCount)
        Function::adjustAttribute(c.isValid() ? int(c.rgb() & RGBMatrixColorMask) : -1, Color1Attr + i);

    emit changed(id());
}

QColor RGBMatrix::getColor(int i) const
{
    if (i < 0 || i >= m_rgbColors.count())
        return QColor();

    return m_rgbColors.at(i);
}

QVector<QColor> RGBMatrix::getColors() const
{
    return m_rgbColors;
}

void RGBMatrix::updateColorDelta()
{
    if (m_rgbColors.count() > 1)
        m_stepHandler->calculateColorDelta(m_rgbColors[0], m_rgbColors[1], m_algorithm);
}

void RGBMatrix::setMapColors(RGBAlgorithm *algorithm)
{
    QMutexLocker algorithmLocker(&m_algorithmMutex);
    if (algorithm == NULL)
        return;

    if (algorithm->apiVersion() < 3)
        return;

    if (m_group == NULL)
        m_group = doc()->fixtureGroup(fixtureGroup());

    QVector<unsigned int> rawColors;
    const int acceptColors = algorithm->acceptColors();
    rawColors.reserve(acceptColors);
    for (int i = 0; i < acceptColors; i++)
    {
        if (m_rgbColors.count() > i)
        {
            QColor col = m_rgbColors.at(i);
            rawColors.append(col.isValid() ? col.rgb() : 0);
        }
        else
        {
            rawColors.append(0);
        }
    }

    algorithm->rgbMapSetColors(rawColors);
}

/************************************************************************
 * Properties
 ************************************************************************/

void RGBMatrix::setProperty(QString propName, QString value)
{
    QMutexLocker algoLocker(&m_algorithmMutex);
    m_properties[propName] = value;
    if (m_algorithm != NULL && m_algorithm->type() == RGBAlgorithm::Script)
    {
        RGBScript *script = static_cast<RGBScript*> (m_algorithm);
        script->setProperty(propName, value);

        QVector<uint> colors = script->rgbMapGetColors();
        for (int i = 0; i < colors.count(); i++)
            setColor(i, QColor::fromRgb(colors.at(i)));
    }
    m_stepsCount = algorithmStepsCount();
}

QString RGBMatrix::property(QString propName)
{
    QMutexLocker algoLocker(&m_algorithmMutex);

    /** If the property is cached, then return it right away */
    QMap<QString, QString>::iterator it = m_properties.find(propName);
    if (it != m_properties.end())
        return it.value();

    /** Otherwise, let's retrieve it from the Script */
    if (m_algorithm != NULL && m_algorithm->type() == RGBAlgorithm::Script)
    {
        RGBScript *script = static_cast<RGBScript*> (m_algorithm);
        return script->property(propName);
    }

    return QString();
}

/****************************************************************************
 * Load & Save
 ****************************************************************************/

bool RGBMatrix::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCFunction)
    {
        qWarning() << Q_FUNC_INFO << "Function node not found";
        return false;
    }

    if (root.attributes().value(KXMLQLCFunctionType).toString() != typeToString(Function::RGBMatrixType))
    {
        qWarning() << Q_FUNC_INFO << "Function is not an RGB matrix";
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
            setAlgorithm(RGBAlgorithm::loader(doc(), root));
        }
        else if (root.name() == KXMLQLCRGBMatrixFixtureGroup)
        {
            setFixtureGroup(root.readElementText().toUInt());
        }
        else if (root.name() == KXMLQLCRGBMatrixAudioProfileID)
        {
            setAudioProfileId(root.readElementText().toUInt());
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
        else if (root.name() == KXMLQLCRGBMatrixAudioRouting)
        {
            AudioRouting r;
            QXmlStreamAttributes attrs = root.attributes();
            if (attrs.hasAttribute(KXMLQLCRGBMatrixAudioRoutingLow))
                r.low = stringToAudioSource(attrs.value(KXMLQLCRGBMatrixAudioRoutingLow).toString());
            if (attrs.hasAttribute(KXMLQLCRGBMatrixAudioRoutingMid))
                r.mid = stringToAudioSource(attrs.value(KXMLQLCRGBMatrixAudioRoutingMid).toString());
            if (attrs.hasAttribute(KXMLQLCRGBMatrixAudioRoutingHigh))
                r.high = stringToAudioSource(attrs.value(KXMLQLCRGBMatrixAudioRoutingHigh).toString());
            if (attrs.hasAttribute(KXMLQLCRGBMatrixAudioRoutingBeat))
                r.beat = stringToAudioSource(attrs.value(KXMLQLCRGBMatrixAudioRoutingBeat).toString());
            if (attrs.hasAttribute(KXMLQLCRGBMatrixAudioRoutingKick))
                r.kick = stringToAudioSource(attrs.value(KXMLQLCRGBMatrixAudioRoutingKick).toString());
            if (attrs.hasAttribute(KXMLQLCRGBMatrixAudioRoutingOnset))
                r.onset = stringToAudioSource(attrs.value(KXMLQLCRGBMatrixAudioRoutingOnset).toString());
            setAudioRouting(r);
            root.skipCurrentElement();
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown RGB matrix tag:" << root.name();
            root.skipCurrentElement();
        }
    }

    return true;
}

bool RGBMatrix::saveXML(QXmlStreamWriter *doc) const
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
    doc->writeTextElement(KXMLQLCRGBMatrixControlMode, RGBMatrix::controlModeToString(m_controlMode));

    /* Fixture Group */
    doc->writeTextElement(KXMLQLCRGBMatrixFixtureGroup, QString::number(fixtureGroup()));

    /* Audio Profile */
    if (m_audioProfileId != AudioProfile::invalidId())
        doc->writeTextElement(KXMLQLCRGBMatrixAudioProfileID, QString::number(m_audioProfileId));

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

    /* Beat Transform */
    if (m_beatEffect != BeatEffectOff)
    {
        doc->writeTextElement(KXMLQLCRGBMatrixBeatEffect, beatEffectToString(m_beatEffect));
        doc->writeTextElement(KXMLQLCRGBMatrixBeatSelection, beatSelectionToString(m_beatSelection));
        doc->writeTextElement(KXMLQLCRGBMatrixBeatOrientation, beatOrientationToString(m_beatOrientation));
    }

    /* Audio Routing — only persist non-default slots */
    if (!m_audioRouting.isAllDefault())
    {
        doc->writeStartElement(KXMLQLCRGBMatrixAudioRouting);
        if (m_audioRouting.low != AudioSrcDefault)
            doc->writeAttribute(KXMLQLCRGBMatrixAudioRoutingLow, audioSourceToString(m_audioRouting.low));
        if (m_audioRouting.mid != AudioSrcDefault)
            doc->writeAttribute(KXMLQLCRGBMatrixAudioRoutingMid, audioSourceToString(m_audioRouting.mid));
        if (m_audioRouting.high != AudioSrcDefault)
            doc->writeAttribute(KXMLQLCRGBMatrixAudioRoutingHigh, audioSourceToString(m_audioRouting.high));
        if (m_audioRouting.beat != AudioSrcDefault)
            doc->writeAttribute(KXMLQLCRGBMatrixAudioRoutingBeat, audioSourceToString(m_audioRouting.beat));
        if (m_audioRouting.kick != AudioSrcDefault)
            doc->writeAttribute(KXMLQLCRGBMatrixAudioRoutingKick, audioSourceToString(m_audioRouting.kick));
        if (m_audioRouting.onset != AudioSrcDefault)
            doc->writeAttribute(KXMLQLCRGBMatrixAudioRoutingOnset, audioSourceToString(m_audioRouting.onset));
        doc->writeEndElement();
    }

    /* End the <Function> tag */
    doc->writeEndElement();

    return true;
}

/****************************************************************************
 * Running
 ****************************************************************************/

void RGBMatrix::tap()
{
    if (stopped() == false)
    {
        FixtureGroup *grp = doc()->fixtureGroup(fixtureGroup());
        // Filter out taps that are too close to each other
        if (grp != NULL && uint(m_roundTime.elapsed()) >= (duration() / 4))
        {
            roundCheck();
            resetElapsed();
        }
    }
}

void RGBMatrix::checkEngineCreation()
{
    m_runAlgorithm = m_algorithm;
    m_requestEngineCreation = false;
}

void RGBMatrix::preRun(MasterTimer *timer)
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

void RGBMatrix::write(MasterTimer *timer, QList<Universe *> universes)
{
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

        // Refresh beat duration before any beat checks
        if (tempoType() == Beats)
            m_stepBeatDuration = beatsToTime(duration(), timer->beatTimeDuration());

        // Advance step if due
        // Save pre-increment elapsed for the map-compute guard below
        quint32 prevElapsed = elapsed();
        incrementElapsed();
        bool stepChanged = false;

        if (tempoType() == Time && elapsed() >= duration())
        {
            stepChanged = roundCheckLocked();
        }
        else if (tempoType() == Beats)
        {
            if (timer->isBeat())
            {
                incrementElapsedBeats();
                if (elapsedBeats() % duration() == 0)
                {
                    stepChanged = roundCheckLocked();
                    resetElapsed();
                }
            }
            else if (elapsed() >= m_stepBeatDuration && (uint)timer->timeToNextBeat() > m_stepBeatDuration / 16)
            {
                stepChanged = roundCheckLocked();
            }
        }

        // --- Beat Transform: update current beat ---
        if (m_beatEffect != BeatEffectOff)
        {
            AudioProfile *profile = doc()->audioProfileForFunction(id());
            AudioChannel *channel = (profile != NULL) ? profile->channel() : NULL;
            if (channel != NULL)
            {
                AudioSnapshot snap = channel->snapshot();
                if (snap.music.barPhase > 0)
                    m_currentBeat = int(snap.music.barPhase) % 4;
                else if (timer->isBeat())
                    m_currentBeat = (m_currentBeat + 1) % 4;
            }
            else if (timer->isBeat())
            {
                m_currentBeat = (m_currentBeat + 1) % 4;
            }

            // Latch random segment on beat change
            if (m_beatSelection == BeatSelRandom && m_currentBeat != m_lastBeat)
            {
                m_randomSegment = QRandomGenerator::global()->bounded(4);
                m_lastBeat = m_currentBeat;
            }
        }

        // Compute and output map
        // Recompute when: step just changed, first tick of a step, audio-reactive, or beat transform active
        if (stepChanged || prevElapsed < MasterTimer::tick() || m_runAlgorithm->usesAudio()
            || m_beatEffect != BeatEffectOff)
        {
            QSize algoSize = effectiveAlgorithmSize(m_group);
            uint stepColor = m_stepHandler->stepColor().rgb();
            int stepIndex = m_stepHandler->currentStepIndex();
            quint32 generation = m_currentGeneration.loadAcquire();

            // --- Phase 4: try to consume a precomputed frame ---
            //
            // The precomputed map already has rotation/mirror applied (those
            // are stable across ticks). Beat transform is NOT precomputed
            // because m_currentBeat is determined per-tick on this thread.
            bool mapReady = false;
            if (m_precomputedReady.loadAcquire() == 1)
            {
                QMutexLocker pre(&m_precomputedMutex);
                if (m_precomputedReady.loadAcquire() == 1
                    && m_precomputedGeneration == generation
                    && m_precomputedAlgorithm == m_runAlgorithm
                    && m_precomputedStep == stepIndex
                    && m_precomputedColor == stepColor
                    && m_precomputedAlgoSize == algoSize)
                {
                    m_stepHandler->m_map = std::move(m_precomputedMap);
                    m_precomputedMap = RGBMap();
                    m_precomputedReady.storeRelease(0);
                    mapReady = true;
                }
                else
                {
                    // Stale or mismatched: discard.
                    m_precomputedReady.storeRelease(0);
                }
            }

            if (!mapReady)
            {
                if (m_runAlgorithm->usesAudio())
                    m_runAlgorithm->setDisplaySize(m_group->size());
                m_runAlgorithm->rgbMap(algoSize, stepColor, stepIndex, m_stepHandler->m_map);
                if (m_rotation || m_mirror)
                    applyTransforms(m_stepHandler->m_map, algoSize, m_group->size(),
                                    m_rotation, m_mirror, m_mirrorBlend);
            }

            if (m_beatEffect != BeatEffectOff)
                applyBeatTransform(m_stepHandler->m_map, m_currentBeat);
            updateMapChannels(m_stepHandler->m_map, m_group, universes, timer->beatTimeDuration());

            // --- Phase 4: kick off async pre-computation for the NEXT tick ---
            //
            // We assume the next tick will use the same step/color (true for
            // most ticks: step changes only when elapsed >= duration). If the
            // assumption is wrong, the consumer will detect the mismatch and
            // fall back to the synchronous path.
            kickAsyncRgbMap(m_runAlgorithm, algoSize, stepColor, stepIndex, generation);
        }
    }
}

void RGBMatrix::postRun(MasterTimer *timer, QList<Universe *> universes)
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

bool RGBMatrix::roundCheckLocked()
{
    if (m_algorithm == NULL)
        return false;

    bool advanced = m_stepHandler->checkNextStep(runOrder(), m_rgbColors[0], m_rgbColors[1], m_stepsCount);
    if (advanced == false)
    {
        stop(FunctionParent::master());
        return false;
    }

    m_roundTime.restart();

    if (tempoType() == Beats)
        roundElapsed(m_stepBeatDuration);
    else
        roundElapsed(duration());

    return true;
}

void RGBMatrix::roundCheck()
{
    QMutexLocker algorithmLocker(&m_algorithmMutex);
    roundCheckLocked();
}

QSharedPointer<GenericFader> RGBMatrix::getFader(Universe *universe)
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

void RGBMatrix::updateFaderValues(FadeChannel &fc, uchar value, uint fadeTime, uint fadeOutTime)
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

void RGBMatrix::invalidatePixelPlan(quint32 id)
{
    Q_UNUSED(id)
    // Conservative: any fixture / fixture-group change may affect the resolved
    // channels. Marking dirty is cheap; the rebuild only happens on next tick.
    m_pixelPlanDirty.storeRelease(1);
}

/*****************************************************************************
 * Phase 4: Async rgbMap pre-computation helpers
 *****************************************************************************/

void RGBMatrix::deferDeleteAlgorithm(RGBAlgorithm *algo)
{
    if (algo == nullptr)
        return;

#ifdef QT_QML_LIB
    // Script-typed algorithms own QJSValue handles tied to the JSThread's
    // QJSEngine. Even ignoring our async tasks, deleting them off-thread is
    // unsafe. Queue the delete on the JSThread so it serializes after any
    // pending precompute tasks (FIFO).
    if (algo->type() == RGBAlgorithm::Script
        && RGBScript::scheduleOnJSThread([algo]() { delete algo; }))
    {
        return;
    }
#endif
    delete algo;
}

void RGBMatrix::kickAsyncRgbMap(RGBAlgorithm *algo, const QSize &algoSize,
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

    bool ok = RGBScript::scheduleOnJSThread(
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

void RGBMatrix::rebuildPixelPlan(const FixtureGroup *grp)
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
                addEntry(headDim, VS_GreyOrFull);
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

void RGBMatrix::updateMapChannels(const RGBMap& map, const FixtureGroup *grp, QList<Universe *> universes, int beatDuration)
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

uchar RGBMatrix::rgbToGrey(uint col)
{
    // the weights are taken from
    // https://en.wikipedia.org/wiki/YUV#SDTV_with_BT.601
    return (0.299 * qRed(col) + 0.587 * qGreen(col) + 0.114 * qBlue(col));
}

/*********************************************************************
 * Attributes
 *********************************************************************/

int RGBMatrix::adjustAttribute(qreal fraction, int attributeId)
{
    int attrIndex = Function::adjustAttribute(fraction, attributeId);

    if (attrIndex == Intensity)
    {
        foreach (QSharedPointer<GenericFader> fader, m_fadersMap)
        {
            if (!fader.isNull())
                fader->adjustIntensity(getAttributeValue(Function::Intensity));
        }
    }
    else if (attrIndex >= Color1Attr && attrIndex <= ColorLastAttr)
    {
        applyColorAttribute(attrIndex - Color1Attr, getAttributeValue(attrIndex));
    }
    else if (attrIndex == PatternAttr)
    {
        applyPatternAttribute(getAttributeValue(PatternAttr));
    }

    return attrIndex;
}

void RGBMatrix::applyStyleAttributes()
{
    for (int i = 0; i < ColorAttributeCount; ++i)
        applyColorAttribute(i, getAttributeValue(Color1Attr + i));

    applyPatternAttribute(getAttributeValue(PatternAttr));
}

void RGBMatrix::applyColorAttribute(int colorIndex, qreal packedColor)
{
    if (colorIndex < 0 || colorIndex >= ColorAttributeCount)
        return;

    int packed = qRound(packedColor);
    QColor targetColor = packed < 0 ? QColor() :
                                      QColor::fromRgb(static_cast<QRgb>((packed & RGBMatrixColorMask) | 0xFF000000));
    if (getColor(colorIndex) == targetColor)
        return;

    bool previous = m_applyingStyleAttributes;
    m_applyingStyleAttributes = true;
    setColor(colorIndex, targetColor);
    m_applyingStyleAttributes = previous;
}

void RGBMatrix::applyPatternAttribute(qreal patternIndex)
{
    if (doc() == NULL)
        return;

    QStringList algoList = RGBAlgorithm::algorithms(doc());
    if (algoList.isEmpty())
        return;

    int idx = qRound(patternIndex);
    if (idx < 0)
        idx = 0;
    else if (idx >= algoList.count())
        idx = algoList.count() - 1;

    RGBAlgorithm *algo = RGBAlgorithm::algorithm(doc(), algoList.at(idx));
    if (algo == NULL)
        return;

    if (m_algorithm != NULL && m_algorithm->name() == algo->name())
    {
        delete algo;
        return;
    }

    algo->setColors(getColors());

    bool previous = m_applyingStyleAttributes;
    m_applyingStyleAttributes = true;
    setAlgorithm(algo);
    m_applyingStyleAttributes = previous;
}

/*************************************************************************
 * Blend mode
 *************************************************************************/

void RGBMatrix::setBlendMode(Universe::BlendMode mode)
{
    if (mode == blendMode())
        return;

    foreach (QSharedPointer<GenericFader> fader, m_fadersMap)
    {
        if (!fader.isNull())
            fader->setBlendMode(mode);
    }

    Function::setBlendMode(mode);
    emit changed(id());
}

/*************************************************************************
 * Control Mode
 *************************************************************************/

RGBMatrix::ControlMode RGBMatrix::controlMode() const
{
    return m_controlMode;
}

void RGBMatrix::setControlMode(RGBMatrix::ControlMode mode)
{
    m_controlMode = mode;
    m_pixelPlanDirty.storeRelease(1);
    // Phase 4: control mode affects how rgbMap output is interpreted, but
    // not the rgbMap output itself. Bump generation defensively to avoid any
    // edge case where the script could observe controlMode (it currently
    // doesn't, but better safe than sorry).
    m_currentGeneration.fetchAndAddRelaxed(1);
    m_precomputedReady.storeRelease(0);
    emit changed(id());
}

RGBMatrix::ControlMode RGBMatrix::stringToControlMode(QString mode)
{
    if (mode == KXMLQLCRGBMatrixControlModeRgb)
        return ControlModeRgb;
    else if (mode == KXMLQLCRGBMatrixControlModeAmber)
        return ControlModeAmber;
    else if (mode == KXMLQLCRGBMatrixControlModeWhite)
        return ControlModeWhite;
    else if (mode == KXMLQLCRGBMatrixControlModeUV)
        return ControlModeUV;
    else if (mode == KXMLQLCRGBMatrixControlModeDimmer)
        return ControlModeDimmer;
    else if (mode == KXMLQLCRGBMatrixControlModeShutter)
        return ControlModeShutter;
    else if (mode == KXMLQLCRGBMatrixControlModeRgbw)
        return ControlModeRgbw;
    else if (mode == KXMLQLCRGBMatrixControlModeRgbwBrighter)
        return ControlModeRgbwBrighter;

    return ControlModeRgb;
}

QString RGBMatrix::controlModeToString(RGBMatrix::ControlMode mode)
{
    switch(mode)
    {
        default:
        case ControlModeRgb:
            return QString(KXMLQLCRGBMatrixControlModeRgb);
        break;
        case ControlModeAmber:
            return QString(KXMLQLCRGBMatrixControlModeAmber);
        break;
        case ControlModeWhite:
            return QString(KXMLQLCRGBMatrixControlModeWhite);
        break;
        case ControlModeUV:
            return QString(KXMLQLCRGBMatrixControlModeUV);
        break;
        case ControlModeDimmer:
            return QString(KXMLQLCRGBMatrixControlModeDimmer);
        break;
        case ControlModeShutter:
            return QString(KXMLQLCRGBMatrixControlModeShutter);
        break;
        case ControlModeRgbw:
            return QString(KXMLQLCRGBMatrixControlModeRgbw);
        break;
        case ControlModeRgbwBrighter:
            return QString(KXMLQLCRGBMatrixControlModeRgbwBrighter);
        break;
    }
}

/*************************************************************************
 * Rotation & Mirror
 *************************************************************************/

int RGBMatrix::rotation() const
{
    return m_rotation;
}

void RGBMatrix::setRotation(int r)
{
    m_rotation = r & 3;
    m_currentGeneration.fetchAndAddRelaxed(1);
    m_precomputedReady.storeRelease(0);
    emit changed(id());
}

int RGBMatrix::mirror() const
{
    return m_mirror;
}

void RGBMatrix::setMirror(int m)
{
    m_mirror = m & 3;
    m_currentGeneration.fetchAndAddRelaxed(1);
    m_precomputedReady.storeRelease(0);
    emit changed(id());
}

RGBMatrix::MirrorBlend RGBMatrix::mirrorBlend() const
{
    return m_mirrorBlend;
}

void RGBMatrix::setMirrorBlend(MirrorBlend b)
{
    if (b < MirrorFlip || b > MirrorAdditive)
        b = MirrorFlip;
    m_mirrorBlend = b;
    m_currentGeneration.fetchAndAddRelaxed(1);
    m_precomputedReady.storeRelease(0);
    emit changed(id());
}

QSize RGBMatrix::effectiveAlgorithmSize() const
{
    if (m_group == NULL)
        return QSize();
    return effectiveAlgorithmSize(m_group);
}

QSize RGBMatrix::effectiveAlgorithmSize(const FixtureGroup *grp) const
{
    QSize s = grp->size();
    if (m_rotation == 1 || m_rotation == 3)
        s = QSize(s.height(), s.width());
    return s;
}

QString RGBMatrix::mirrorBlendToString(MirrorBlend b)
{
    switch (b)
    {
        case MirrorMax: return QStringLiteral("Max");
        case MirrorAverage: return QStringLiteral("Average");
        case MirrorAdditive: return QStringLiteral("Additive");
        default: return QStringLiteral("Flip");
    }
}

RGBMatrix::MirrorBlend RGBMatrix::stringToMirrorBlend(const QString &s)
{
    if (s == QStringLiteral("Max")) return MirrorMax;
    if (s == QStringLiteral("Average")) return MirrorAverage;
    if (s == QStringLiteral("Additive")) return MirrorAdditive;
    return MirrorFlip;
}

static inline uint blendPixels(uint a, uint b, RGBMatrix::MirrorBlend blend)
{
    uint ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    uint br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    uint r, g, bl;

    switch (blend)
    {
        case RGBMatrix::MirrorMax:
            r = qMax(ar, br); g = qMax(ag, bg); bl = qMax(ab, bb);
            break;
        case RGBMatrix::MirrorAverage:
            r = (ar + br) / 2; g = (ag + bg) / 2; bl = (ab + bb) / 2;
            break;
        case RGBMatrix::MirrorAdditive:
            r = qMin(255u, ar + br); g = qMin(255u, ag + bg); bl = qMin(255u, ab + bb);
            break;
        default: // MirrorFlip — caller handles this case directly
            return a;
    }
    return (r << 16) | (g << 8) | bl;
}

void RGBMatrix::applyTransforms(RGBMap &map, const QSize & /* srcSize */, const QSize &dstSize,
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

/*************************************************************************
 * Beat Transform
 *************************************************************************/

RGBMatrix::BeatEffect RGBMatrix::beatEffect() const
{
    return m_beatEffect;
}

void RGBMatrix::setBeatEffect(BeatEffect e)
{
    m_beatEffect = e;
}

RGBMatrix::BeatSelection RGBMatrix::beatSelection() const
{
    return m_beatSelection;
}

void RGBMatrix::setBeatSelection(BeatSelection s)
{
    m_beatSelection = s;
}

RGBMatrix::BeatOrientation RGBMatrix::beatOrientation() const
{
    return m_beatOrientation;
}

void RGBMatrix::setBeatOrientation(BeatOrientation o)
{
    m_beatOrientation = o;
}

QString RGBMatrix::beatEffectToString(BeatEffect e)
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

RGBMatrix::BeatEffect RGBMatrix::stringToBeatEffect(const QString &s)
{
    if (s == QStringLiteral("Mirror"))      return BeatEffectMirror;
    if (s == QStringLiteral("ColorInvert")) return BeatEffectColorInvert;
    if (s == QStringLiteral("Blackout"))    return BeatEffectBlackout;
    if (s == QStringLiteral("Whiteout"))    return BeatEffectWhiteout;
    return BeatEffectOff;
}

QString RGBMatrix::beatSelectionToString(BeatSelection s)
{
    switch (s)
    {
        case BeatSelWalk:    return QStringLiteral("Walk");
        case BeatSelRandom:  return QStringLiteral("Random");
        default:             return QStringLiteral("AllBeat4");
    }
}

RGBMatrix::BeatSelection RGBMatrix::stringToBeatSelection(const QString &s)
{
    if (s == QStringLiteral("Walk"))   return BeatSelWalk;
    if (s == QStringLiteral("Random")) return BeatSelRandom;
    return BeatSelAllBeat4;
}

QString RGBMatrix::beatOrientationToString(BeatOrientation o)
{
    switch (o)
    {
        case BeatOrientColumns: return QStringLiteral("Columns");
        default:                return QStringLiteral("Rows");
    }
}

RGBMatrix::BeatOrientation RGBMatrix::stringToBeatOrientation(const QString &s)
{
    if (s == QStringLiteral("Columns")) return BeatOrientColumns;
    return BeatOrientRows;
}

void RGBMatrix::segmentRange(int segment, int total, int &start, int &end)
{
    int base = total / 4;
    int remainder = total % 4;
    start = segment * base + qMin(segment, remainder);
    end = start + base + (segment < remainder ? 1 : 0);
}

void RGBMatrix::applyBeatTransform(RGBMap &map, int currentBeat)
{
    if (m_beatEffect == BeatEffectOff)
        return;

    int rows = map.size();
    if (rows == 0) return;
    int cols = map[0].size();
    if (cols == 0) return;

    int total = (m_beatOrientation == BeatOrientRows) ? rows : cols;

    // Determine affected segments
    QVector<int> affected;
    if (m_beatSelection == BeatSelAllBeat4)
    {
        if (currentBeat != 3) return;
        affected = {0, 1, 2, 3};
    }
    else if (m_beatSelection == BeatSelWalk)
    {
        affected = {currentBeat & 3};
    }
    else // BeatSelRandom
    {
        affected = {m_randomSegment};
    }

    for (int seg : affected)
    {
        int start, end;
        segmentRange(seg, total, start, end);

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


/*************************************************************************
 *************************************************************************
 *                          RGBMatrixStep class
 *************************************************************************
 *************************************************************************/

RGBMatrixStep::RGBMatrixStep()
    : m_direction(Function::Forward)
    , m_currentStepIndex(0)
    , m_stepColor(QColor())
    , m_crDelta(0)
    , m_cgDelta(0)
    , m_cbDelta(0)
{

}

void RGBMatrixStep::setCurrentStepIndex(int index)
{
    m_currentStepIndex = index;
}

int RGBMatrixStep::currentStepIndex() const
{
    return m_currentStepIndex;
}

void RGBMatrixStep::calculateColorDelta(const QColor& startColor, const QColor& endColor, const RGBAlgorithm *algorithm)
{
    m_crDelta = 0;
    m_cgDelta = 0;
    m_cbDelta = 0;

    if (endColor.isValid() && algorithm != NULL && algorithm->acceptColors() > 1)
    {
        m_crDelta = endColor.red() - startColor.red();
        m_cgDelta = endColor.green() - startColor.green();
        m_cbDelta = endColor.blue() - startColor.blue();

        //qDebug() << "Color deltas:" << m_crDelta << m_cgDelta << m_cbDelta;
    }
}

void RGBMatrixStep::setStepColor(QColor color)
{
    m_stepColor = color;
}

QColor RGBMatrixStep::stepColor() const
{
    return m_stepColor;
}

void RGBMatrixStep::updateStepColor(int stepIndex, QColor startColor, int stepsCount)
{
    if (stepsCount <= 0)
        return;

    if (stepsCount == 1)
    {
        m_stepColor = startColor;
    }
    else
    {
        m_stepColor.setRed(startColor.red() + (m_crDelta * stepIndex / (stepsCount - 1)));
        m_stepColor.setGreen(startColor.green() + (m_cgDelta * stepIndex / (stepsCount - 1)));
        m_stepColor.setBlue(startColor.blue() + (m_cbDelta * stepIndex / (stepsCount - 1)));
    }

    //qDebug() << "RGBMatrix step" << stepIndex << ", color:" << QString::number(m_stepColor.rgb(), 16);
}

void RGBMatrixStep::initializeDirection(Function::Direction direction, const QColor& startColor, const QColor& endColor, int stepsCount, const RGBAlgorithm *algorithm)
{
    m_direction = direction;

    if (m_direction == Function::Forward)
    {
        setCurrentStepIndex(0);
        setStepColor(startColor);
    }
    else
    {
        setCurrentStepIndex(stepsCount - 1);

        if (endColor.isValid())
            setStepColor(endColor);
        else
            setStepColor(startColor);
    }

    calculateColorDelta(startColor, endColor, algorithm);
}

bool RGBMatrixStep::checkNextStep(Function::RunOrder order,
                                  QColor startColor, QColor endColor, int stepsNumber)
{
    if (order == Function::PingPong)
    {
        if (m_direction == Function::Forward && (m_currentStepIndex + 1) == stepsNumber)
        {
            m_direction = Function::Backward;
            m_currentStepIndex = stepsNumber - 2;
            if (endColor.isValid())
                m_stepColor = endColor;

            updateStepColor(m_currentStepIndex, startColor, stepsNumber);
        }
        else if (m_direction == Function::Backward && (m_currentStepIndex - 1) < 0)
        {
            m_direction = Function::Forward;
            m_currentStepIndex = 1;
            m_stepColor = startColor;
            updateStepColor(m_currentStepIndex, startColor, stepsNumber);
        }
        else
        {
            if (m_direction == Function::Forward)
                m_currentStepIndex++;
            else
                m_currentStepIndex--;
            updateStepColor(m_currentStepIndex, startColor, stepsNumber);
        }
    }
    else if (order == Function::SingleShot)
    {
        if (m_direction == Function::Forward)
        {
            if (m_currentStepIndex >= stepsNumber - 1)
                return false;
            else
            {
                m_currentStepIndex++;
                updateStepColor(m_currentStepIndex, startColor, stepsNumber);
            }
        }
        else
        {
            if (m_currentStepIndex <= 0)
                return false;
            else
            {
                m_currentStepIndex--;
                updateStepColor(m_currentStepIndex, startColor, stepsNumber);
            }
        }
    }
    else
    {
        if (m_direction == Function::Forward)
        {
            if (m_currentStepIndex >= stepsNumber - 1)
            {
                m_currentStepIndex = 0;
                m_stepColor = startColor;
            }
            else
            {
                m_currentStepIndex++;
                updateStepColor(m_currentStepIndex, startColor, stepsNumber);
            }
        }
        else
        {
            if (m_currentStepIndex <= 0)
            {
                m_currentStepIndex = stepsNumber - 1;
                if (endColor.isValid())
                    m_stepColor = endColor;
            }
            else
            {
                m_currentStepIndex--;
                updateStepColor(m_currentStepIndex, startColor, stepsNumber);
            }
        }
    }

    return true;
}

/*************************************************************************
 * Audio Routing
 *************************************************************************/

RGBMatrix::AudioRouting RGBMatrix::audioRouting() const
{
    return m_audioRouting;
}

void RGBMatrix::setAudioRouting(const AudioRouting &r)
{
    m_audioRouting = r;
}

QString RGBMatrix::audioSourceToString(AudioSource s)
{
    switch (s)
    {
        case AudioSrcZero:   return QStringLiteral("zero");
        case AudioSrcLow:    return QStringLiteral("low");
        case AudioSrcMid:    return QStringLiteral("mid");
        case AudioSrcHigh:   return QStringLiteral("high");
        case AudioSrcBeat:   return QStringLiteral("beat");
        case AudioSrcKick:   return QStringLiteral("kick");
        case AudioSrcOnset:  return QStringLiteral("onset");
        case AudioSrcVolume: return QStringLiteral("volume");
        case AudioSrcDefault:
        default:             return QStringLiteral("default");
    }
}

RGBMatrix::AudioSource RGBMatrix::stringToAudioSource(const QString &s)
{
    if (s == QStringLiteral("zero"))   return AudioSrcZero;
    if (s == QStringLiteral("low"))    return AudioSrcLow;
    if (s == QStringLiteral("mid"))    return AudioSrcMid;
    if (s == QStringLiteral("high"))   return AudioSrcHigh;
    if (s == QStringLiteral("beat"))   return AudioSrcBeat;
    if (s == QStringLiteral("kick"))   return AudioSrcKick;
    if (s == QStringLiteral("onset"))  return AudioSrcOnset;
    if (s == QStringLiteral("volume")) return AudioSrcVolume;
    return AudioSrcDefault;
}
