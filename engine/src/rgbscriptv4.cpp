/*
  Q Light Controller Plus
  rgbscriptv4.cpp

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
#include <QTextStream>
#include <QJSEngine>
#include <QStringList>
#include <QThread>
#include <QDebug>
#include <QFile>
#include <QDir>

// cppcheck-suppress missingIncludeSystem
#include <QCoreApplication>
// cppcheck-suppress missingIncludeSystem
#include <QSemaphore>
#include <algorithm>

#include "rgbscriptv4.h"

#include "rgbscriptscache.h"
#include "audiochannel.h"
#include "audiocapture.h"
#include "audioprofile.h"
#include "audiosnapshot.h"
#include "mastertimer.h"
#include "qlcconfig.h"
#include "qlcfile.h"
#include "doc.h"
#include "rgbmatrix.h"

namespace
{
    RGBMatrix *owningMatrix(Doc *doc, const RGBScript *script)
    {
        if (doc == NULL || script == NULL)
            return NULL;

        foreach (Function *function, doc->functionsByType(Function::RGBMatrixType))
        {
            RGBMatrix *matrix = qobject_cast<RGBMatrix*> (function);
            if (matrix != NULL && matrix->algorithm() == script)
                return matrix;
        }

        return NULL;
    }

    QJSValue triggerObject(QJSEngine *engine, const TriggerState &trigger)
    {
        QJSValue obj = engine->newObject();
        obj.setProperty(QStringLiteral("value"), QJSValue(trigger.value));
        obj.setProperty(QStringLiteral("active"), QJSValue(trigger.active));
        obj.setProperty(QStringLiteral("firedThisFrame"), QJSValue(trigger.firedThisFrame));
        obj.setProperty(QStringLiteral("releasedThisFrame"), QJSValue(trigger.releasedThisFrame));
        obj.setProperty(QStringLiteral("heldMs"), QJSValue(trigger.heldMs));
        obj.setProperty(QStringLiteral("cooldownRemainingMs"), QJSValue(trigger.cooldownRemainingMs));
        return obj;
    }
}

/****************************************************************************
 * Initialization
 ****************************************************************************/

JSThread* RGBScript::s_jsThread = NULL;

class JSThread final : public QThread
{
public:
    QJSEngine *engine;
    QSemaphore ready;
    void run() override
    {
        engine = new QJSEngine();
        ready.release(1);
        exec();
        delete engine;
    }
};


RGBScript::RGBScript(Doc *doc)
    : RGBAlgorithm(doc)
    , m_apiVersion(0)
    , m_usesAudio(false)
    , m_audioInput(NULL)
    , m_loggedAudioProfileId(AudioProfile::invalidId())
    , m_audioRegistered(false)
{
}

RGBScript::RGBScript(const RGBScript& s)
    : RGBAlgorithm(s.doc())
    , m_fileName(s.m_fileName)
    , m_contents(s.m_contents)
    , m_apiVersion(0)
    , m_usesAudio(false)
    , m_audioInput(NULL)
    , m_loggedAudioProfileId(AudioProfile::invalidId())
    , m_audioRegistered(false)
{
    evaluate();
    foreach (RGBScriptProperty cap, s.m_properties)
    {
        setProperty(cap.m_name, s.property(cap.m_name));
    }
}

RGBScript::~RGBScript()
{
    teardownAudioCapture();
}

RGBScript &RGBScript::operator=(const RGBScript &s)
{
    if (this != &s)
    {
        m_fileName = s.m_fileName;
        m_contents = s.m_contents;
        m_apiVersion = s.m_apiVersion;
        evaluate();
        foreach (RGBScriptProperty cap, s.m_properties)
        {
            setProperty(cap.m_name, s.property(cap.m_name));
        }
    }

    return *this;
}

bool RGBScript::operator==(const RGBScript& s) const
{
    return this->fileName().isEmpty() == false && this->fileName() == s.fileName();
}

RGBAlgorithm* RGBScript::clone() const
{
    RGBScript *script = new RGBScript(*this);
    return static_cast<RGBAlgorithm*> (script);
}

/****************************************************************************
 * Load & Evaluation
 ****************************************************************************/

bool RGBScript::load(const QString& fileName)
{
    // Create the script engine when it's first needed
    initEngine();

    {
        m_contents.clear();
        m_script = QJSValue();
        m_rgbMap = QJSValue();
        m_rgbMapStepCount = QJSValue();
        m_rgbMapSetColors = QJSValue();
        m_apiVersion = 0;
    }

    m_fileName = fileName;
    QFile file(m_fileName);
    if (file.open(QIODevice::ReadOnly) == false)
    {
        qWarning() << "Unable to load RGB script" << m_fileName;
        return false;
    }

    QTextStream stream(&file);
    m_contents = stream.readAll();
    file.close();

    return evaluate();
}

QString RGBScript::fileName() const
{
    return m_fileName;
}

void RGBScript::initEngine()
{
    if (s_jsThread == NULL)
    {
        s_jsThread = new JSThread();
        s_jsThread->start();
        // cppcheck-suppress unknownMacro
        qAddPostRoutine(RGBScript::cleanupEngine);
        s_jsThread->ready.acquire(1);

        // Load shared audio script helpers into the engine's global scope.
        QDir scriptsDir = RGBScriptsCache::systemScriptsDirectory();
        const QStringList shimNames = {
            QStringLiteral("rgbutil.js"),
            QStringLiteral("audiodsp.js"),
            QStringLiteral("audio_common.js")
        };
        for (const QString &shimName : shimNames)
        {
            QString shimPath = scriptsDir.filePath(shimName);
            QFile shimFile(shimPath);
            if (shimFile.open(QIODevice::ReadOnly))
            {
                QString shimContents = QTextStream(&shimFile).readAll();
                shimFile.close();
                QMetaObject::invokeMethod(s_jsThread->engine, [shimContents, shimPath]{
                    QJSValue result = s_jsThread->engine->evaluate(shimContents, shimPath);
                    if (result.isError())
                        displayError(result, shimPath);
                    else
                        qDebug() << "[RGBScript] Loaded RGB script shim" << shimPath;
                }, Qt::BlockingQueuedConnection);
            }
        }
    }
    Q_ASSERT(s_jsThread->engine != NULL);
}

void RGBScript::cleanupEngine()
{
    s_jsThread->exit();
    s_jsThread->wait();
    delete s_jsThread;
    s_jsThread = NULL;
}


bool RGBScript::evaluate()
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        bool retVal;
        QMetaObject::invokeMethod(s_jsThread->engine, [this]{ return evaluate();}, Qt::BlockingQueuedConnection, &retVal);
        return retVal;
    }

    m_rgbMap = QJSValue();
    m_rgbMapStepCount = QJSValue();
    m_rgbMapSetColors = QJSValue();
    m_apiVersion = 0;
    m_usesAudio = false;

    if (m_fileName.isEmpty() || m_contents.isEmpty())
    {
        qWarning() << m_fileName << ": Script filename or content is empty, cannot parse";
        return false;
    }

    initEngine();

    m_script = s_jsThread->engine->evaluate(m_contents, m_fileName);
    if (m_script.isError())
    {
        displayError(m_script, m_fileName);
        return false;
    }

    m_rgbMap = m_script.property(QStringLiteral("rgbMap"));
    if (m_rgbMap.isCallable() == false)
    {
        qWarning() << m_fileName << "is missing the rgbMap() function!";
        return false;
    }

    m_rgbMapStepCount = m_script.property(QStringLiteral("rgbMapStepCount"));
    if (m_rgbMapStepCount.isCallable() == false)
    {
        qWarning() << m_fileName << "is missing the rgbMapStepCount() function!";
        return false;
    }

    m_apiVersion = m_script.property("apiVersion").toInt();
    if (m_apiVersion > 0)
    {
        // Check if the script requests audio data
        QJSValue usesAudioVal = m_script.property("usesAudio");
        m_usesAudio = (!usesAudioVal.isUndefined() && usesAudioVal.toBool());

        if (m_apiVersion >= 3)
        {
            m_rgbMapSetColors = m_script.property(QStringLiteral("rgbMapSetColors"));
            if (m_rgbMapSetColors.isCallable() == false)
            {
                qWarning() << m_fileName << "is missing the rgbMapSetColors() function!";
                return false;
            }
        }
        if (m_apiVersion >= 2)
            return loadProperties();
        return true;
    }
    else
    {
        qWarning() << m_fileName << "has an invalid apiVersion:" << m_apiVersion;
        return false;
    }
}

void RGBScript::displayError(QJSValue e, const QString& fileName)
{
    if (e.isError())
    {
        QString msg("%1: Exception at line %2. Error: %3");
        qWarning() << msg.arg(fileName)
                         .arg(e.property("lineNumber").toInt())
                         .arg(e.toString());
        qDebug() << "Stack: " << e.property("stack").toString();
    }
}

/****************************************************************************
 * Script API
 ****************************************************************************/

int RGBScript::rgbMapStepCount(const QSize& size)
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        int retVal;
        QMetaObject::invokeMethod(s_jsThread->engine, [this, size]{ return rgbMapStepCount(size);}, Qt::BlockingQueuedConnection, &retVal);
        return retVal;
    }

    if (m_rgbMapStepCount.isCallable() == false)
        return -1;

    QJSValueList args;
    args << size.width() << size.height();
    QJSValue value = m_rgbMapStepCount.call(args);
    if (value.isError())
    {
        displayError(value, m_fileName);
        return -1;
    } 
    else 
    {
        int ret = value.isNumber() ? value.toInt() : -1;
        return ret;
    }
}

void RGBScript::rgbMapSetColors(const QVector<uint> &colors)
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QMetaObject::invokeMethod(s_jsThread->engine, [this, colors]{ return rgbMapSetColors(colors);}, Qt::QueuedConnection);
        return;
    }

    if (m_apiVersion <= 2)
        return;

    if (m_rgbMap.isUndefined() == true)
        return;

    if (m_rgbMapSetColors.isCallable() == false)
        return;

    int accColors = acceptColors();
    int rawColorCount = colors.count();

    QJSValue jsRawColors = s_jsThread->engine->newArray(accColors);
    for (int i = 0; i < rawColorCount && i < accColors; i++)
        jsRawColors.setProperty(i, QJSValue(colors.at(i)));

    QJSValueList args;
    args << jsRawColors;

    QJSValue value = m_rgbMapSetColors.call(args);
    if (value.isError())
        displayError(value, m_fileName);
}

QVector<uint> RGBScript::rgbMapGetColors()
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QVector<uint> retVal;
        QMetaObject::invokeMethod(s_jsThread->engine, [this]{ return rgbMapGetColors();}, Qt::BlockingQueuedConnection, &retVal);
        return retVal;
    }

    QVector<uint> colArray;

    if (m_rgbMap.isUndefined() == true)
        return colArray;

    QJSValue colors = m_rgbMapGetColors.call();
    if (!colors.isError() && colors.isArray())
    {
        QVariantList arr = colors.toVariant().toList();
        foreach (QVariant color, arr)
            colArray.append(color.toUInt());
    }

    return colArray;
}

void RGBScript::rgbMap(const QSize& size, uint rgb, int step, RGBMap &map)
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QMetaObject::invokeMethod(s_jsThread->engine, [this, size, rgb, step, &map]{ rgbMap(size, rgb, step, map);}, Qt::BlockingQueuedConnection);
        return;
    }

    if (m_rgbMap.isUndefined() == true)
        return;

    // Inject the matrix's color stops as algo.gradientColors and a
    // pre-sampled 3-element algo.gradientBandColors LUT for low/mid/high banks.
    // Done on every frame so live UI / MCP color edits show up next tick.
    injectGradientArrays(rgb);

    // If this is an audio-aware script, set up audio capture on first call
    // and inject audio data as a 5th argument
    if (m_usesAudio)
    {
        setupAudioCapture();
    }

    QJSValueList args;
    args << size.width() << size.height() << rgb << step;

    if (m_usesAudio)
        args << buildAudioDataObject();

    QJSValue yarray(m_rgbMap.call(args));
    if (yarray.isError())
        displayError(yarray, m_fileName);

    if (yarray.isArray())
    {
        QVariantList yvArray = yarray.toVariant().toList();
        int ylen = yvArray.length();
        map.resize(ylen);

        for (int y = 0; y < ylen && y < size.height(); y++)
        {
            QVariantList xvArray = yvArray.at(y).toList();
            int xlen = xvArray.length();
            map[y].resize(xlen);

            for (int x = 0; x < xlen && x < size.width(); x++)
                map[y][x] = xvArray.at(x).toUInt();
        }
    }
    else
    {
        qWarning() << "Returned value is not an array within an array!";
        return;
    }
}

QString RGBScript::name() const
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QString retVal;
        QMetaObject::invokeMethod(s_jsThread->engine, [this]{ return name();}, Qt::BlockingQueuedConnection, &retVal);
        return retVal;
    }

    QJSValue name = m_script.property(QStringLiteral("name"));
    QString ret = name.isUndefined() ? QString() : name.toString();
    return ret;
}

QString RGBScript::author() const
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QString retVal;
        QMetaObject::invokeMethod(s_jsThread->engine, [this]{ return author();}, Qt::BlockingQueuedConnection, &retVal);
        return retVal;
    }

    QJSValue author = m_script.property(QStringLiteral("author"));
    QString ret = author.isUndefined() ? QString() : author.toString();
    return ret;
}

int RGBScript::apiVersion() const
{
    return m_apiVersion;
}

RGBAlgorithm::Type RGBScript::type() const
{
    return RGBAlgorithm::Script;
}

int RGBScript::acceptColors() const
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        int retVal;
        QMetaObject::invokeMethod(s_jsThread->engine, [this]{ return acceptColors();}, Qt::BlockingQueuedConnection, &retVal);
        return retVal;
    }

    QJSValue accColors = m_script.property(QStringLiteral("acceptColors"));
    if (!accColors.isUndefined())
        return accColors.toInt();
    // if no property is provided, let's assume the script
    // will accept both start and end colors
    return 2;
}

bool RGBScript::loadXML(QXmlStreamReader &root)
{
    Q_UNUSED(root)

    return false;
}

bool RGBScript::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != NULL);

    if (apiVersion() > 0 && name().isEmpty() == false)
    {
        doc->writeStartElement(KXMLQLCRGBAlgorithm);
        doc->writeAttribute(KXMLQLCRGBAlgorithmType, KXMLQLCRGBScript);
        doc->writeCharacters(name());
        doc->writeEndElement();
        return true;
    }
    else
    {
        return false;
    }
}

bool RGBScript::usesAudio() const
{
    return m_usesAudio;
}

void RGBScript::setDisplaySize(const QSize &size)
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QMetaObject::invokeMethod(s_jsThread->engine, [this, size]{ setDisplaySize(size); }, Qt::BlockingQueuedConnection);
        return;
    }

    if (!m_script.isObject())
        return;

    m_script.setProperty(QStringLiteral("displayWidth"), size.width());
    m_script.setProperty(QStringLiteral("displayHeight"), size.height());
}

void RGBScript::postRun()
{
    teardownAudioCapture();
}

/****************************************************************************
 * Audio support
 ****************************************************************************/

void RGBScript::setupAudioCapture()
{
    if (doc() == NULL)
        return;

    QSharedPointer<AudioCapture> capture = doc()->audioInputCapture();
    if (capture.isNull())
        return;

    if (m_audioInput != NULL && capture.data() == m_audioInput)
        return; // Already connected to this capture

    teardownAudioCapture();
    m_audioInput = capture.data();

    // The v3 audio object is sourced exclusively from the AudioProfile's
    // AudioChannel snapshot inside buildAudioDataObject(). We don't need
    // to subscribe to aubioDataReady — we just need the capture thread
    // to be running.
    m_audioInput->registerBandsNumber(1);
    m_audioRegistered = true;
}

void RGBScript::teardownAudioCapture()
{
    if (m_audioInput != NULL)
    {
        if (m_audioRegistered)
        {
            m_audioInput->unregisterBandsNumber(1);
            m_audioRegistered = false;
        }
        m_audioInput = NULL;
    }
}

uint RGBScript::interpolateGradientColor(const QVector<uint> &colors, double t)
{
    if (colors.isEmpty())
        return 0;
    if (colors.size() == 1)
        return colors.at(0) & 0xFFFFFFu;
    if (t < 0.0) t = 0.0;
    else if (t > 1.0) t = 1.0;

    double pos = t * (colors.size() - 1);
    int idx = int(pos);
    if (idx >= colors.size() - 1)
        return colors.at(colors.size() - 1) & 0xFFFFFFu;

    double frac = pos - idx;
    uint c1 = colors.at(idx), c2 = colors.at(idx + 1);
    int r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    int r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;
    int r = int(qRound(r1 + frac * (r2 - r1)));
    int g = int(qRound(g1 + frac * (g2 - g1)));
    int b = int(qRound(b1 + frac * (b2 - b1)));
    return (uint(r) << 16) | (uint(g) << 8) | uint(b);
}

void RGBScript::injectGradientArrays(uint rgb)
{
    QJSEngine *engine = s_jsThread->engine;
    if (engine == NULL || m_script.isUndefined())
        return;

    // Compact valid stops from the owning matrix into 0xRRGGBB.
    QVector<uint> stops;
    RGBMatrix *matrix = owningMatrix(doc(), this);
    if (matrix != NULL)
    {
        QVector<QColor> cols = matrix->getColors();
        for (int i = 0; i < cols.size(); ++i)
        {
            const QColor &c = cols.at(i);
            if (!c.isValid())
                continue;
            stops.append(c.rgb() & 0xFFFFFFu);
        }
    }

    // gradientColors: empty if no valid stops -> fall back to [rgb] so scripts
    // that index into it don't crash. Mask rgb to 0xRRGGBB to match stop format.
    QJSValue gradArr;
    if (stops.isEmpty())
    {
        gradArr = engine->newArray(1);
        gradArr.setProperty(0, QJSValue(double(rgb & 0xFFFFFFu)));
    }
    else
    {
        gradArr = engine->newArray(quint32(stops.size()));
        for (int i = 0; i < stops.size(); ++i)
            gradArr.setProperty(quint32(i), QJSValue(double(stops.at(i))));
    }

    // gradientBandColors: 3 evenly sampled colors (one per mel bank: low/mid/high).
    // If only one valid color, all 3 entries are that color.
    QVector<uint> sampleStops = stops;
    if (sampleStops.isEmpty())
        sampleStops.append(rgb & 0xFFFFFFu);

    QJSValue bandArr = engine->newArray(3);
    for (int i = 0; i < 3; ++i)
    {
        double t = (sampleStops.size() <= 1) ? 0.0 : double(i) / 2.0;
        uint c = interpolateGradientColor(sampleStops, t);
        bandArr.setProperty(quint32(i), QJSValue(double(c)));
    }

    m_script.setProperty(QStringLiteral("gradientColors"), gradArr);
    m_script.setProperty(QStringLiteral("gradientBandColors"), bandArr);

    m_currentGradientColors = gradArr;
    m_currentBandColors = bandArr;
}

QJSValue RGBScript::buildAudioDataObject()
{
    QJSEngine *engine = s_jsThread->engine;
    QJSValue audioObj = engine->newObject();
    AudioChannel *channel = NULL;
    Doc *currentDoc = doc();
    RGBMatrix *matrix = owningMatrix(currentDoc, this);
    AudioProfile *profile = (currentDoc != NULL && matrix != NULL)
        ? currentDoc->audioProfileForFunction(matrix->id()) : NULL;
    if (profile != NULL)
    {
        channel = profile->channel();
        if (m_loggedAudioProfileId != profile->id())
        {
            qDebug().noquote() << QStringLiteral("RGBScript %1: using audio profile %2 (ID: %3)")
                .arg(name(), profile->name())
                .arg(profile->id());
            m_loggedAudioProfileId = profile->id();
        }
    }
    else
    {
        m_loggedAudioProfileId = AudioProfile::invalidId();
    }

    // Use a default-constructed snapshot when no profile is assigned, so the
    // v3 shape is always present and scripts can run safely.
    AudioSnapshot snap;
    if (channel != NULL)
        snap = channel->snapshot();

    // Mel filterbank: `audio.mel` returns post-processed values (LedFx-style
    // power scaling + AGC + smoothing) when MelPostProcessor is enabled, or
    // raw aubio values when disabled (bypass). Scripts that need the raw
    // aubio output unconditionally can use `audio.melRaw`. `audio.melNovelty`
    // exposes the diff signal (zero when post-processing is disabled).
    QJSValue melArr = engine->newArray(AUBIO_MEL_BANDS);
    QJSValue melRawArr = engine->newArray(AUBIO_MEL_BANDS);
    QJSValue melNoveltyArr = engine->newArray(AUBIO_MEL_BANDS);
    for (int i = 0; i < AUBIO_MEL_BANDS; i++)
    {
        melArr.setProperty(i, QJSValue(snap.melProcessed[i]));
        melRawArr.setProperty(i, QJSValue(snap.mel[i]));
        melNoveltyArr.setProperty(i, QJSValue(snap.melNovelty[i]));
    }
    audioObj.setProperty(QStringLiteral("mel"), melArr);
    audioObj.setProperty(QStringLiteral("melRaw"), melRawArr);
    audioObj.setProperty(QStringLiteral("melNovelty"), melNoveltyArr);

    // Multi-resolution mel banks (Phase 3): three nested matt-mel banks
    // sharing the same FFT. Bass effects read `audio.melLow` (high resolution
    // under 350 Hz), vocals/snare effects read `audio.melMid`, hats/air
    // effects read `audio.melHigh`. `audio.mel` (the legacy 40-band) is kept
    // unchanged for backward compatibility.
    if (snap.melLow.count > 0 || snap.melMid.count > 0 || snap.melHigh.count > 0)
    {
        QJSValue melLowArr = engine->newArray(snap.melLow.count);
        QJSValue melMidArr = engine->newArray(snap.melMid.count);
        QJSValue melHighArr = engine->newArray(snap.melHigh.count);
        QJSValue melLowRawArr = engine->newArray(snap.melLow.count);
        QJSValue melMidRawArr = engine->newArray(snap.melMid.count);
        QJSValue melHighRawArr = engine->newArray(snap.melHigh.count);
        QJSValue melLowNovArr = engine->newArray(snap.melLow.count);
        QJSValue melMidNovArr = engine->newArray(snap.melMid.count);
        QJSValue melHighNovArr = engine->newArray(snap.melHigh.count);
        for (int i = 0; i < snap.melLow.count; i++)
        {
            melLowArr.setProperty(i, QJSValue(snap.melLow.processed[i]));
            melLowRawArr.setProperty(i, QJSValue(snap.melLow.raw[i]));
            melLowNovArr.setProperty(i, QJSValue(snap.melLow.novelty[i]));
        }
        for (int i = 0; i < snap.melMid.count; i++)
        {
            melMidArr.setProperty(i, QJSValue(snap.melMid.processed[i]));
            melMidRawArr.setProperty(i, QJSValue(snap.melMid.raw[i]));
            melMidNovArr.setProperty(i, QJSValue(snap.melMid.novelty[i]));
        }
        for (int i = 0; i < snap.melHigh.count; i++)
        {
            melHighArr.setProperty(i, QJSValue(snap.melHigh.processed[i]));
            melHighRawArr.setProperty(i, QJSValue(snap.melHigh.raw[i]));
            melHighNovArr.setProperty(i, QJSValue(snap.melHigh.novelty[i]));
        }
        audioObj.setProperty(QStringLiteral("melLow"), melLowArr);
        audioObj.setProperty(QStringLiteral("melMid"), melMidArr);
        audioObj.setProperty(QStringLiteral("melHigh"), melHighArr);
        audioObj.setProperty(QStringLiteral("melLowRaw"), melLowRawArr);
        audioObj.setProperty(QStringLiteral("melMidRaw"), melMidRawArr);
        audioObj.setProperty(QStringLiteral("melHighRaw"), melHighRawArr);
        audioObj.setProperty(QStringLiteral("melLowNovelty"), melLowNovArr);
        audioObj.setProperty(QStringLiteral("melMidNovelty"), melMidNovArr);
        audioObj.setProperty(QStringLiteral("melHighNovelty"), melHighNovArr);
    }

    // Per-bank Hz metadata so scripts can pick the right bank for an effect's
    // frequency range without hardcoding constants. Always emitted (even when
    // the banks aren't populated yet) so feature detection via
    // `typeof audio.melRanges !== 'undefined'` works deterministically.
    QJSValue melRangesObj = engine->newObject();
    auto buildBankMeta = [&](const AudioSnapshot::MelBankSnapshot &bank,
                             double defaultMin, double defaultMax,
                             int defaultBands) {
        QJSValue obj = engine->newObject();
        obj.setProperty(QStringLiteral("minHz"),
                        QJSValue(bank.count > 0 ? bank.minHz : defaultMin));
        obj.setProperty(QStringLiteral("maxHz"),
                        QJSValue(bank.count > 0 ? bank.maxHz : defaultMax));
        obj.setProperty(QStringLiteral("bands"),
                        QJSValue(bank.count > 0 ? bank.count : defaultBands));
        return obj;
    };
    melRangesObj.setProperty(QStringLiteral("low"),
                             buildBankMeta(snap.melLow,  20.0,   350.0, 24));
    melRangesObj.setProperty(QStringLiteral("mid"),
                             buildBankMeta(snap.melMid,  20.0,  2000.0, 24));
    melRangesObj.setProperty(QStringLiteral("high"),
                             buildBankMeta(snap.melHigh, 20.0, 15000.0, 24));
    audioObj.setProperty(QStringLiteral("melRanges"), melRangesObj);

    // MFCC (13 coefficients)
    QJSValue mfccArr = engine->newArray(AUBIO_MFCC_COEFFS);
    for (int i = 0; i < AUBIO_MFCC_COEFFS; i++)
        mfccArr.setProperty(i, QJSValue(snap.mfcc[i]));
    audioObj.setProperty(QStringLiteral("mfcc"), mfccArr);

    // LedFx-parity scalar bank power: mean of each mel bank's processed[].
    // Use these for simple frequency-balanced effects without iterating mel arrays.
    audioObj.setProperty(QStringLiteral("lows"),  QJSValue(snap.lows));
    audioObj.setProperty(QStringLiteral("mids"),  QJSValue(snap.mids));
    audioObj.setProperty(QStringLiteral("highs"), QJSValue(snap.highs));

    QJSValue triggersObj = engine->newObject();
    triggersObj.setProperty(QStringLiteral("low"),    triggerObject(engine, snap.triggers[0]));
    triggersObj.setProperty(QStringLiteral("mid"),    triggerObject(engine, snap.triggers[1]));
    triggersObj.setProperty(QStringLiteral("high"),   triggerObject(engine, snap.triggers[2]));
    triggersObj.setProperty(QStringLiteral("volume"), triggerObject(engine, snap.volumeTrigger));
    triggersObj.setProperty(QStringLiteral("beat"),   triggerObject(engine, snap.beatTrigger));
    triggersObj.setProperty(QStringLiteral("kick"),   triggerObject(engine, snap.kickTrigger));
    audioObj.setProperty(QStringLiteral("triggers"), triggersObj);

    QJSValue volObj = engine->newObject();
    volObj.setProperty(QStringLiteral("raw"), QJSValue(snap.volume.raw));
    volObj.setProperty(QStringLiteral("smoothed"), QJSValue(snap.volume.smoothed));
    volObj.setProperty(QStringLiteral("normalized"), QJSValue(snap.volume.normalized));
    audioObj.setProperty(QStringLiteral("volume"), volObj);

    QJSValue musicObj = engine->newObject();
    musicObj.setProperty(QStringLiteral("beat"), QJSValue(snap.music.beat));
    musicObj.setProperty(QStringLiteral("bpm"), QJSValue(snap.music.bpm));
    musicObj.setProperty(QStringLiteral("beatPhase"), QJSValue(snap.music.beatPhase));
    musicObj.setProperty(QStringLiteral("barPhase"), QJSValue(snap.music.barPhase));
    musicObj.setProperty(QStringLiteral("beatConfidence"), QJSValue(snap.music.beatConfidence));
    musicObj.setProperty(QStringLiteral("tatum"), QJSValue(snap.music.tatum));
    audioObj.setProperty(QStringLiteral("music"), musicObj);

    QJSValue featuresObj = engine->newObject();
    featuresObj.setProperty(QStringLiteral("rmsDb"), QJSValue(snap.features.rmsDb));
    featuresObj.setProperty(QStringLiteral("peakDb"), QJSValue(snap.features.peakDb));
    featuresObj.setProperty(QStringLiteral("crestFactor"), QJSValue(snap.features.crestFactor));
    featuresObj.setProperty(QStringLiteral("centroidHz"), QJSValue(snap.features.centroidHz));
    featuresObj.setProperty(QStringLiteral("spread"), QJSValue(snap.features.spread));
    featuresObj.setProperty(QStringLiteral("rolloffHz"), QJSValue(snap.features.rolloffHz));
    featuresObj.setProperty(QStringLiteral("flux"), QJSValue(snap.features.flux));
    featuresObj.setProperty(QStringLiteral("hfc"), QJSValue(snap.features.hfc));
    featuresObj.setProperty(QStringLiteral("flatness"), QJSValue(snap.features.flatness));
    audioObj.setProperty(QStringLiteral("features"), featuresObj);

    QJSValue onsetsObj = engine->newObject();
    onsetsObj.setProperty(QStringLiteral("energy"), QJSValue(snap.onsets.energy));
    onsetsObj.setProperty(QStringLiteral("hfc"), QJSValue(snap.onsets.hfc));
    onsetsObj.setProperty(QStringLiteral("complex"), QJSValue(snap.onsets.complex_));
    onsetsObj.setProperty(QStringLiteral("phase"), QJSValue(snap.onsets.phase));
    onsetsObj.setProperty(QStringLiteral("wphase"), QJSValue(snap.onsets.wphase));
    onsetsObj.setProperty(QStringLiteral("specdiff"), QJSValue(snap.onsets.specdiff));
    onsetsObj.setProperty(QStringLiteral("kl"), QJSValue(snap.onsets.kl));
    onsetsObj.setProperty(QStringLiteral("mkl"), QJSValue(snap.onsets.mkl));
    onsetsObj.setProperty(QStringLiteral("specflux"), QJSValue(snap.onsets.specflux));

    // Continuous onset magnitudes (raw + after adaptive whitening). Mirrors
    // LedFx's get_descriptor() / get_thresholded_descriptor() pattern: lets
    // scripts react to "how hard" an onset fires, not just on/off.
    // Method order matches the booleans above:
    //   0=energy 1=hfc 2=complex 3=phase 4=wphase
    //   5=specdiff 6=kl 7=mkl 8=specflux
    QJSValue descArr = engine->newArray(AUBIO_ONSET_METHODS);
    QJSValue threshArr = engine->newArray(AUBIO_ONSET_METHODS);
    for (int i = 0; i < AUBIO_ONSET_METHODS; i++)
    {
        descArr.setProperty(i, QJSValue(snap.onsets.descriptors[i]));
        threshArr.setProperty(i, QJSValue(snap.onsets.thresholdedDescriptors[i]));
    }
    onsetsObj.setProperty(QStringLiteral("descriptors"), descArr);
    onsetsObj.setProperty(QStringLiteral("thresholdedDescriptors"), threshArr);
    audioObj.setProperty(QStringLiteral("onsets"), onsetsObj);

    QJSValue pitchObj = engine->newObject();
    pitchObj.setProperty(QStringLiteral("hz"), QJSValue(snap.pitch.hz));
    pitchObj.setProperty(QStringLiteral("confidence"), QJSValue(snap.pitch.confidence));
    audioObj.setProperty(QStringLiteral("pitch"), pitchObj);

    QJSValue noteObj = engine->newObject();
    noteObj.setProperty(QStringLiteral("midi"), QJSValue(snap.note.midi));
    noteObj.setProperty(QStringLiteral("velocity"), QJSValue(snap.note.velocity));
    noteObj.setProperty(QStringLiteral("noteOn"), QJSValue(snap.note.noteOn));
    noteObj.setProperty(QStringLiteral("noteOff"), QJSValue(snap.note.noteOff));
    audioObj.setProperty(QStringLiteral("note"), noteObj);

    audioObj.setProperty(QStringLiteral("audioDtMs"), QJSValue(snap.audioDtMs));
    audioObj.setProperty(QStringLiteral("brightnessFloor"), QJSValue(snap.brightnessFloor));
    audioObj.setProperty(QStringLiteral("noiseGateClosed"), QJSValue(snap.noiseGateClosed));
    audioObj.setProperty(QStringLiteral("consumerDtMs"), QJSValue(double(MasterTimer::tick())));

    // Mirror the auto-injected gradient arrays on the audio object so
    // audio-aware scripts can grab them via either algo.* or audio.*.
    if (!m_currentGradientColors.isUndefined())
        audioObj.setProperty(QStringLiteral("gradientColors"), m_currentGradientColors);
    if (!m_currentBandColors.isUndefined())
        audioObj.setProperty(QStringLiteral("bandColors"), m_currentBandColors);

    return audioObj;
}


/************************************************************************
 * Capabilities
 ************************************************************************/

QList<RGBScriptProperty> RGBScript::properties()
{
    return m_properties;
}

QHash<QString, QString> RGBScript::propertiesAsStrings()
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QHash<QString, QString> retVal;
        QMetaObject::invokeMethod(s_jsThread->engine, [this]{ return propertiesAsStrings();}, Qt::BlockingQueuedConnection, &retVal);
        return retVal;
    }

    QHash<QString, QString> properties;
    foreach (RGBScriptProperty cap, m_properties)
    {
        QJSValue readMethod = m_script.property(cap.m_readMethod);
        if (readMethod.isCallable())
        {
            QJSValueList args;
            QJSValue value = readMethod.call(args);
            if (value.isError())
                displayError(value, m_fileName);
            else if (!value.isUndefined())
                properties.insert(cap.m_name, value.toString());
        }
    }
    return properties;
}

bool RGBScript::setProperty(QString propertyName, QString value)
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        bool retVal;
        QMetaObject::invokeMethod(s_jsThread->engine, [this, propertyName, value]{ return setProperty(propertyName, value);}, Qt::BlockingQueuedConnection, &retVal);
        return retVal;
    }

    foreach (RGBScriptProperty cap, m_properties)
    {
        if (cap.m_name == propertyName)
        {
            QJSValue writeMethod = m_script.property(cap.m_writeMethod);
            if (writeMethod.isCallable() == false)
            {
                qWarning() << name() << "doesn't have a write function for" << propertyName;
                return false;
            }
            QJSValueList args;
            args << value;
            QJSValue written = writeMethod.call(args);
            if (written.isError())
            {
                displayError(written, m_fileName);
                return false;
            } 
            else 
            {
                return true;
            }
        }
    }
    return false;
}

QString RGBScript::property(QString propertyName) const
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QString retVal;
        QMetaObject::invokeMethod(s_jsThread->engine, [this, propertyName]{ return property(propertyName);}, Qt::BlockingQueuedConnection, &retVal);
        return retVal;
    }

    foreach (RGBScriptProperty cap, m_properties)
    {
        if (cap.m_name == propertyName)
        {
            QJSValue readMethod = m_script.property(cap.m_readMethod);
            if (readMethod.isCallable() == false)
            {
                qWarning() << name() << "doesn't have a read function for" << propertyName;
                return QString();
            }
            QJSValueList args;
            QJSValue value = readMethod.call(args);
            if (value.isError())
            {
                displayError(value, m_fileName);
                return QString();
            } 
            else if (!value.isUndefined())
            {
                return value.toString();
            }
            else
            {
                return QString();
            }
        }
    }
    return QString();
}

bool RGBScript::loadProperties()
{
    QJSValue svCaps = m_script.property(QStringLiteral("properties"));
    if (svCaps.isArray() == false)
    {
        qWarning() << m_fileName << "properties is not an array!";
        return false;
    }
    QVariant varCaps = svCaps.toVariant();
    if (varCaps.isValid() == false)
    {
        qWarning() << m_fileName << "has invalid properties!";
        return false;
    }

    m_properties.clear();

    QStringList slCaps = varCaps.toStringList();
    foreach (QString cap, slCaps)
    {
        RGBScriptProperty newCap;

        QStringList propsList = cap.split('|');
        foreach (QString prop, propsList)
        {
            QStringList keyValue = prop.split(':');
            if (keyValue.length() < 2)
            {
                qWarning() << prop << ": malformed property. Please fix it.";
                continue;
            }
            QString key = keyValue.at(0).simplified();
            QString value = keyValue.at(1);
            if (key == QStringLiteral("name"))
            {
                newCap.m_name = value;
            }
            else if (key == QStringLiteral("type"))
            {
                if (value == "list") newCap.m_type = RGBScriptProperty::List;
                else if (value == "float") newCap.m_type = RGBScriptProperty::Float;
                else if (value == "range") newCap.m_type = RGBScriptProperty::Range;
                else if (value == "string") newCap.m_type = RGBScriptProperty::String;
            }
            else if (key == QStringLiteral("display"))
            {
                newCap.m_displayName = value.simplified();
            }
            else if (key == QStringLiteral("values"))
            {
                QStringList values = value.split(",");
                switch(newCap.m_type)
                {
                    case RGBScriptProperty::List:
                        newCap.m_listValues = values;
                    break;
                    case RGBScriptProperty::Range:
                    {
                        if (values.length() < 2)
                        {
                            qWarning() << value << ": malformed property. A range should be defined as 'min,max'. Please fix it.";
                        }
                        else
                        {
                            newCap.m_rangeMinValue = values.at(0).toInt();
                            newCap.m_rangeMaxValue = values.at(1).toInt();
                        }
                    }
                    break;
                    default:
                        qWarning() << value << ": values cannot be applied before the 'type' property or on type:integer and type:string";
                    break;
                }
            }
            else if (key == QStringLiteral("write"))
            {
                newCap.m_writeMethod = value.simplified();
            }
            else if (key == QStringLiteral("read"))
            {
                newCap.m_readMethod = value.simplified();
            }
            else
            {
                qWarning() << value << ": unknown property!";
            }
        }

        if (newCap.m_name.isEmpty() == false &&
            newCap.m_type != RGBScriptProperty::None)
                m_properties.append(newCap);
    }

    return true;
}
