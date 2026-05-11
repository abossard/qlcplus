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
#include <cmath>
#include <cstdint>

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

    static constexpr int kAudioApiVersion = 4;
    static constexpr int kPowerBandCount = 3;
    static constexpr int kBeatsPerBar = 4;
    static constexpr double kDownbeatWindow = 0.25;
    static constexpr double kPi = 3.14159265358979323846;
    static constexpr double kA4Hz = 440.0;
    static constexpr double kA4Midi = 69.0;
    static constexpr double kSemitonesPerOctave = 12.0;
    static const char* const kPowerBandNames[kPowerBandCount] = {
        "low", "mid", "high"
    };
    static const char* const kOnsetMethodNames[AUBIO_ONSET_METHODS] = {
        "energy", "hfc", "complex", "phase", "wphase",
        "specdiff", "kl", "mkl", "specflux"
    };

    QJSValue triggerToJs(QJSEngine *engine, const TriggerState &trigger)
    {
        QJSValue obj = engine->newObject();
        obj.setProperty(QStringLiteral("value"), QJSValue(trigger.value));
        obj.setProperty(QStringLiteral("active"), QJSValue(trigger.active));
        obj.setProperty(QStringLiteral("fired"), QJSValue(trigger.firedThisFrame));
        obj.setProperty(QStringLiteral("released"), QJSValue(trigger.releasedThisFrame));
        obj.setProperty(QStringLiteral("heldMs"), QJSValue(trigger.heldMs));
        obj.setProperty(QStringLiteral("cooldownMs"), QJSValue(trigger.cooldownRemainingMs));
        return obj;
    }

    QJSValue doubleArrayToJs(QJSEngine *engine, const double *values, int count)
    {
        QJSValue arr = engine->newArray(quint32(std::max(0, count)));
        for (int i = 0; i < count; i++)
            arr.setProperty(quint32(i), QJSValue(values[i]));
        return arr;
    }

    QJSValue packedColorArrayToJs(QJSEngine *engine, const QJSValue &source, int fallbackCount)
    {
        if (!source.isUndefined())
            return source;
        QJSValue arr = engine->newArray(quint32(fallbackCount));
        for (int i = 0; i < fallbackCount; i++)
            arr.setProperty(quint32(i), QJSValue(0));
        return arr;
    }

    double meanOf(const double *values, int count)
    {
        if (values == nullptr || count <= 0)
            return 0.0;
        double sum = 0.0;
        for (int i = 0; i < count; i++)
            sum += values[i];
        return sum / double(count);
    }

    double maxOf(const double *values, int count)
    {
        if (values == nullptr || count <= 0)
            return 0.0;
        double maxValue = values[0];
        for (int i = 1; i < count; i++)
            maxValue = std::max(maxValue, values[i]);
        return maxValue;
    }

    QJSValue spectrumBankToJs(QJSEngine *engine,
                              const AudioSnapshot::MelBankSnapshot &bank)
    {
        QJSValue obj = engine->newObject();
        obj.setProperty(QStringLiteral("values"),
                        doubleArrayToJs(engine, bank.processed, bank.count));
        obj.setProperty(QStringLiteral("novelty"),
                        doubleArrayToJs(engine, bank.novelty, bank.count));
        obj.setProperty(QStringLiteral("raw"),
                        doubleArrayToJs(engine, bank.raw, bank.count));
        obj.setProperty(QStringLiteral("mean"), QJSValue(meanOf(bank.processed, bank.count)));
        obj.setProperty(QStringLiteral("max"), QJSValue(maxOf(bank.processed, bank.count)));
        return obj;
    }

    QJSValue spectrumRangeToJs(QJSEngine *engine,
                               const AudioSnapshot::MelBankSnapshot &bank,
                               const MelBankConfig::Bank &fallback)
    {
        QJSValue obj = engine->newObject();
        obj.setProperty(QStringLiteral("minHz"),
                        QJSValue(bank.count > 0 ? bank.minHz : fallback.minHz));
        obj.setProperty(QStringLiteral("maxHz"),
                        QJSValue(bank.count > 0 ? bank.maxHz : fallback.maxHz));
        obj.setProperty(QStringLiteral("bands"),
                        QJSValue(bank.count > 0 ? bank.count : fallback.bands));
        return obj;
    }

    bool onsetFiredAt(const AudioSnapshot &snap, int methodIndex)
    {
        switch (methodIndex)
        {
        case 0: return snap.onsets.energy;
        case 1: return snap.onsets.hfc;
        case 2: return snap.onsets.complex_;
        case 3: return snap.onsets.phase;
        case 4: return snap.onsets.wphase;
        case 5: return snap.onsets.specdiff;
        case 6: return snap.onsets.kl;
        case 7: return snap.onsets.mkl;
        case 8: return snap.onsets.specflux;
        default: return false;
        }
    }

    QJSValue onsetMethodToJs(QJSEngine *engine, const AudioSnapshot &snap, int methodIndex)
    {
        QJSValue obj = engine->newObject();
        obj.setProperty(QStringLiteral("fired"), QJSValue(onsetFiredAt(snap, methodIndex)));
        obj.setProperty(QStringLiteral("descriptor"),
                        QJSValue(snap.onsets.descriptors[methodIndex]));
        obj.setProperty(QStringLiteral("thresholded"),
                        QJSValue(snap.onsets.thresholdedDescriptors[methodIndex]));
        return obj;
    }

    double hzToMidi(double hz)
    {
        if (hz <= 0.0)
            return 0.0;
        return kA4Midi + kSemitonesPerOctave * std::log2(hz / kA4Hz);
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
            QStringLiteral("audio_colors.js")
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

bool RGBScript::scheduleOnJSThread(std::function<void()> fn)
{
    if (s_jsThread == NULL || s_jsThread->engine == NULL)
        return false;

    QMetaObject::invokeMethod(s_jsThread->engine, std::move(fn), Qt::QueuedConnection);
    return true;
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
    {
        displayError(yarray, m_fileName);
        return;
    }

    // Phase 3 single path: scripts MUST return a flat Uint32Array of length
    // width*height (row-major). The C++ side reads its underlying ArrayBuffer
    // as a QByteArray and copies uint32_t values directly into RGBMap. This
    // avoids the per-element FastDtoa number→string conversions QV4 applies
    // when toVariant().toList() walks a nested Array<Array<number>>.
    //
    // TypedArrays don't satisfy QJSValue::isArray(); detect via the
    // BYTES_PER_ELEMENT property (only present on TypedArray instances).
    const int width = size.width();
    const int height = size.height();
    if (width <= 0 || height <= 0)
        return;

    QJSValue bpeProp = yarray.property(QStringLiteral("BYTES_PER_ELEMENT"));
    if (!bpeProp.isNumber())
    {
        qWarning() << "RGBScript" << m_fileName
                   << "rgbMap() did not return a TypedArray. Phase 3 requires"
                   << "a flat Uint32Array of length width*height.";
        return;
    }

    const int bpe = bpeProp.toInt();
    const int byteLength = yarray.property(QStringLiteral("byteLength")).toInt();
    const int expectedBytes = width * height * 4;
    if (bpe != 4 || byteLength != expectedBytes)
    {
        qWarning() << "RGBScript" << m_fileName
                   << "returned a TypedArray with unexpected geometry: byteLength="
                   << byteLength << "expected=" << expectedBytes
                   << "BYTES_PER_ELEMENT=" << bpe;
        return;
    }

    QJSValue bufProp = yarray.property(QStringLiteral("buffer"));
    if (!bufProp.isObject())
    {
        qWarning() << "RGBScript" << m_fileName
                   << "TypedArray has no underlying ArrayBuffer";
        return;
    }

    QByteArray bytes = bufProp.toVariant().toByteArray();
    if (bytes.size() != expectedBytes)
    {
        qWarning() << "RGBScript" << m_fileName
                   << "ArrayBuffer extraction size mismatch: got" << bytes.size()
                   << "expected" << expectedBytes;
        return;
    }

    const uint32_t *src = reinterpret_cast<const uint32_t*>(bytes.constData());
    map.resize(height);
    for (int y = 0; y < height; ++y)
    {
        map[y].resize(width);
        const uint32_t *row = src + (y * width);
        for (int x = 0; x < width; ++x)
            map[y][x] = row[x];
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

    // The audio object is sourced exclusively from the AudioProfile's
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
    AudioChannelConfig config = AudioChannelConfig::defaults();
    Doc *currentDoc = doc();
    RGBMatrix *matrix = owningMatrix(currentDoc, this);
    AudioProfile *profile = (currentDoc != NULL && matrix != NULL)
        ? currentDoc->audioProfileForFunction(matrix->id()) : NULL;
    if (profile != NULL)
    {
        channel = profile->channel();
        config = (channel != NULL) ? channel->config() : profile->channelConfig();
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
    // v4 shape is always present and scripts can run safely.
    AudioSnapshot snap;
    if (channel != NULL)
        snap = channel->snapshot();

    audioObj.setProperty(QStringLiteral("version"), QJSValue(kAudioApiVersion));

    const double powerBands[kPowerBandCount] = { snap.lows, snap.mids, snap.highs };
    int dominantIdx = 0;
    for (int i = 1; i < kPowerBandCount; i++)
    {
        if (powerBands[i] > powerBands[dominantIdx])
            dominantIdx = i;
    }

    QJSValue powerObj = engine->newObject();
    powerObj.setProperty(QStringLiteral("low"),   QJSValue(snap.lows));
    powerObj.setProperty(QStringLiteral("mid"),   QJSValue(snap.mids));
    powerObj.setProperty(QStringLiteral("high"),  QJSValue(snap.highs));
    powerObj.setProperty(QStringLiteral("total"), QJSValue(snap.lows + snap.mids + snap.highs));
    powerObj.setProperty(QStringLiteral("dominant"),
                         QJSValue(QString::fromLatin1(kPowerBandNames[dominantIdx])));
    powerObj.setProperty(QStringLiteral("dominantValue"), QJSValue(powerBands[dominantIdx]));
    QJSValue powerBandsArr = engine->newArray(kPowerBandCount);
    for (int i = 0; i < kPowerBandCount; i++)
        powerBandsArr.setProperty(quint32(i), QJSValue(powerBands[i]));
    powerObj.setProperty(QStringLiteral("bands"), powerBandsArr);
    QJSValue detailObj = engine->newObject();
    detailObj.setProperty(QStringLiteral("beat"), QJSValue(snap.beatPower));
    detailObj.setProperty(QStringLiteral("bass"), QJSValue(snap.bassPower));
    powerObj.setProperty(QStringLiteral("detail"), detailObj);
    audioObj.setProperty(QStringLiteral("power"), powerObj);

    const int onsetMethodIndex = std::clamp(config.aubio.onsetMethodIndex,
                                            0, AUBIO_ONSET_METHODS - 1);
    QJSValue onsetObj = engine->newObject();
    onsetObj.setProperty(QStringLiteral("fired"),
                         QJSValue(onsetFiredAt(snap, onsetMethodIndex)));
    onsetObj.setProperty(QStringLiteral("intensity"),
                         QJSValue(snap.onsets.thresholdedDescriptors[onsetMethodIndex]));
    onsetObj.setProperty(QStringLiteral("method"),
                         QJSValue(QString::fromLatin1(kOnsetMethodNames[onsetMethodIndex])));
    QJSValue onsetMethodsObj = engine->newObject();
    for (int i = 0; i < AUBIO_ONSET_METHODS; i++)
    {
        onsetMethodsObj.setProperty(QString::fromLatin1(kOnsetMethodNames[i]),
                                    onsetMethodToJs(engine, snap, i));
    }
    onsetObj.setProperty(QStringLiteral("methods"), onsetMethodsObj);
    audioObj.setProperty(QStringLiteral("onset"), onsetObj);

    QJSValue beatObj = engine->newObject();
    beatObj.setProperty(QStringLiteral("fired"), QJSValue(snap.beatTrigger.firedThisFrame));
    beatObj.setProperty(QStringLiteral("kick"), QJSValue(snap.kickTrigger.firedThisFrame));
    beatObj.setProperty(QStringLiteral("kickHeld"), QJSValue(snap.kickTrigger.active));
    beatObj.setProperty(QStringLiteral("kickIntensity"), QJSValue(snap.kickTrigger.value));
    beatObj.setProperty(QStringLiteral("phase"), QJSValue(snap.music.beatPhase));
    // Gate beat/bar pre-computations on active tempo tracking.
    // When no audio profile is bound, bpm==0 → cosPulse=0, downbeat=false.
    const bool tempoActive = snap.music.bpm > 0.0;
    beatObj.setProperty(QStringLiteral("cosPulse"),
                        QJSValue(tempoActive
                            ? std::max(0.0, std::cos(snap.music.beatPhase * kPi))
                            : 0.0));
    beatObj.setProperty(QStringLiteral("bpm"), QJSValue(snap.music.bpm));
    beatObj.setProperty(QStringLiteral("confidence"), QJSValue(snap.music.beatConfidence));
    beatObj.setProperty(QStringLiteral("tatum"), QJSValue(snap.music.tatum));
    audioObj.setProperty(QStringLiteral("beat"), beatObj);

    const int barPhaseInt = int(snap.music.barPhase);
    const double barFract = snap.music.barPhase - double(barPhaseInt);
    int barBeat = barPhaseInt % kBeatsPerBar;
    if (barBeat < 0)
        barBeat += kBeatsPerBar;
    const bool downbeat = tempoActive && barPhaseInt == 0 && barFract < kDownbeatWindow;
    QJSValue barObj = engine->newObject();
    barObj.setProperty(QStringLiteral("phase"), QJSValue(snap.music.barPhase));
    barObj.setProperty(QStringLiteral("phase01"), QJSValue(snap.music.barPhase / double(kBeatsPerBar)));
    barObj.setProperty(QStringLiteral("beat"), QJSValue(barBeat));
    barObj.setProperty(QStringLiteral("downbeat"), QJSValue(downbeat));
    barObj.setProperty(QStringLiteral("downbeatFired"), QJSValue(snap.downbeatFired));
    audioObj.setProperty(QStringLiteral("bar"), barObj);

    QJSValue spectrumObj = engine->newObject();
    spectrumObj.setProperty(QStringLiteral("low"), spectrumBankToJs(engine, snap.melLow));
    spectrumObj.setProperty(QStringLiteral("mid"), spectrumBankToJs(engine, snap.melMid));
    spectrumObj.setProperty(QStringLiteral("high"), spectrumBankToJs(engine, snap.melHigh));
    QJSValue fullSpectrumArr = engine->newArray(quint32(snap.melLow.count + snap.melMid.count + snap.melHigh.count));
    int fullIndex = 0;
    auto appendBank = [&](const AudioSnapshot::MelBankSnapshot &bank) {
        for (int i = 0; i < bank.count; i++)
            fullSpectrumArr.setProperty(quint32(fullIndex++), QJSValue(bank.processed[i]));
    };
    appendBank(snap.melLow);
    appendBank(snap.melMid);
    appendBank(snap.melHigh);
    spectrumObj.setProperty(QStringLiteral("full"), fullSpectrumArr);
    QJSValue rangesObj = engine->newObject();
    rangesObj.setProperty(QStringLiteral("low"),
                          spectrumRangeToJs(engine, snap.melLow, config.aubio.melBanks.low));
    rangesObj.setProperty(QStringLiteral("mid"),
                          spectrumRangeToJs(engine, snap.melMid, config.aubio.melBanks.mid));
    rangesObj.setProperty(QStringLiteral("high"),
                          spectrumRangeToJs(engine, snap.melHigh, config.aubio.melBanks.high));
    spectrumObj.setProperty(QStringLiteral("ranges"), rangesObj);
    double noveltySum = 0.0;
    double noveltyMax = 0.0;
    int noveltyCount = 0;
    auto accumulateNovelty = [&](const AudioSnapshot::MelBankSnapshot &bank) {
        for (int i = 0; i < bank.count; i++)
        {
            noveltySum += bank.novelty[i];
            noveltyMax = (noveltyCount == 0) ? bank.novelty[i] : std::max(noveltyMax, bank.novelty[i]);
            noveltyCount++;
        }
    };
    accumulateNovelty(snap.melLow);
    accumulateNovelty(snap.melMid);
    accumulateNovelty(snap.melHigh);
    QJSValue spectrumNoveltyObj = engine->newObject();
    spectrumNoveltyObj.setProperty(QStringLiteral("mean"),
                                   QJSValue(noveltyCount > 0 ? noveltySum / double(noveltyCount) : 0.0));
    spectrumNoveltyObj.setProperty(QStringLiteral("max"), QJSValue(noveltyMax));
    spectrumObj.setProperty(QStringLiteral("novelty"), spectrumNoveltyObj);
    audioObj.setProperty(QStringLiteral("spectrum"), spectrumObj);

    QJSValue volObj = engine->newObject();
    volObj.setProperty(QStringLiteral("raw"), QJSValue(snap.volume.raw));
    volObj.setProperty(QStringLiteral("smoothed"), QJSValue(snap.volume.smoothed));
    volObj.setProperty(QStringLiteral("normalized"), QJSValue(snap.volume.normalized));
    volObj.setProperty(QStringLiteral("ledfx"), QJSValue(snap.volume.volumeNorm));
    volObj.setProperty(QStringLiteral("rmsDb"), QJSValue(snap.features.rmsDb));
    volObj.setProperty(QStringLiteral("peakDb"), QJSValue(snap.features.peakDb));
    volObj.setProperty(QStringLiteral("crestFactor"), QJSValue(snap.features.crestFactor));
    volObj.setProperty(QStringLiteral("trigger"), triggerToJs(engine, snap.volumeTrigger));
    volObj.setProperty(QStringLiteral("fired"), QJSValue(snap.volumeTrigger.firedThisFrame));
    volObj.setProperty(QStringLiteral("held"), QJSValue(snap.volumeTrigger.active));
    audioObj.setProperty(QStringLiteral("volume"), volObj);

    QJSValue bandsObj = engine->newObject();
    bandsObj.setProperty(QStringLiteral("low"), triggerToJs(engine, snap.triggers[0]));
    bandsObj.setProperty(QStringLiteral("mid"), triggerToJs(engine, snap.triggers[1]));
    bandsObj.setProperty(QStringLiteral("high"), triggerToJs(engine, snap.triggers[2]));
    audioObj.setProperty(QStringLiteral("bands"), bandsObj);

    QJSValue featuresObj = engine->newObject();
    featuresObj.setProperty(QStringLiteral("centroidHz"), QJSValue(snap.features.centroidHz));
    featuresObj.setProperty(QStringLiteral("spread"), QJSValue(snap.features.spread));
    featuresObj.setProperty(QStringLiteral("rolloffHz"), QJSValue(snap.features.rolloffHz));
    featuresObj.setProperty(QStringLiteral("flux"), QJSValue(snap.features.flux));
    featuresObj.setProperty(QStringLiteral("hfc"), QJSValue(snap.features.hfc));
    featuresObj.setProperty(QStringLiteral("flatness"), QJSValue(snap.features.flatness));
    featuresObj.setProperty(QStringLiteral("mfcc"),
                            doubleArrayToJs(engine, snap.mfcc, AUBIO_MFCC_COEFFS));
    audioObj.setProperty(QStringLiteral("features"), featuresObj);

    QJSValue pitchObj = engine->newObject();
    pitchObj.setProperty(QStringLiteral("hz"), QJSValue(snap.pitch.hz));
    pitchObj.setProperty(QStringLiteral("midi"), QJSValue(hzToMidi(snap.pitch.hz)));
    pitchObj.setProperty(QStringLiteral("confidence"), QJSValue(snap.pitch.confidence));
    audioObj.setProperty(QStringLiteral("pitch"), pitchObj);

    QJSValue noteObj = engine->newObject();
    noteObj.setProperty(QStringLiteral("midi"), QJSValue(snap.note.midi));
    noteObj.setProperty(QStringLiteral("velocity"), QJSValue(snap.note.velocity));
    noteObj.setProperty(QStringLiteral("on"), QJSValue(snap.note.noteOn));
    noteObj.setProperty(QStringLiteral("off"), QJSValue(snap.note.noteOff));
    audioObj.setProperty(QStringLiteral("note"), noteObj);

    QJSValue gateObj = engine->newObject();
    gateObj.setProperty(QStringLiteral("closed"), QJSValue(snap.noiseGateClosed));
    gateObj.setProperty(QStringLiteral("brightnessFloor"), QJSValue(snap.brightnessFloor));
    audioObj.setProperty(QStringLiteral("gate"), gateObj);

    QJSValue timingObj = engine->newObject();
    timingObj.setProperty(QStringLiteral("audioDtMs"), QJSValue(snap.audioDtMs));
    timingObj.setProperty(QStringLiteral("consumerDtMs"), QJSValue(double(MasterTimer::tick())));
    audioObj.setProperty(QStringLiteral("timing"), timingObj);

    QJSValue colorsObj = engine->newObject();
    colorsObj.setProperty(QStringLiteral("gradient"),
                          packedColorArrayToJs(engine, m_currentGradientColors, 0));
    colorsObj.setProperty(QStringLiteral("bands"),
                          packedColorArrayToJs(engine, m_currentBandColors, kPowerBandCount));
    audioObj.setProperty(QStringLiteral("colors"), colorsObj);

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
