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
#include <QRegularExpression>
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

    static constexpr int kAudioApiVersion = 5;
    static constexpr int kBeatsPerBar = 4;
    static constexpr double kPi = 3.14159265358979323846;

    /** Convert HSV (h,s,v each in [0,1]) to packed 0xAARRGGBB via qRgb(). */
    static inline uint hsvToRgb(float h, float s, float v)
    {
        if (!std::isfinite(h)) h = 0.0f;
        if (!std::isfinite(s)) s = 0.0f;
        if (!std::isfinite(v)) v = 0.0f;
        h = h - floorf(h); if (h < 0) h += 1.0f;
        s = qBound(0.0f, s, 1.0f);
        v = qBound(0.0f, v, 1.0f);

        float c = v * s;
        float x = c * (1.0f - fabsf(fmodf(h * 6.0f, 2.0f) - 1.0f));
        float m = v - c;
        float r, g, b;
        int sector = (int)(h * 6.0f) % 6;
        switch (sector) {
            case 0: r=c; g=x; b=0; break;
            case 1: r=x; g=c; b=0; break;
            case 2: r=0; g=c; b=x; break;
            case 3: r=0; g=x; b=c; break;
            case 4: r=x; g=0; b=c; break;
            default: r=c; g=0; b=x; break;
        }
        int ri = qRound((r+m) * 255.0f);
        int gi = qRound((g+m) * 255.0f);
        int bi = qRound((b+m) * 255.0f);
        return qRgb(ri, gi, bi);
    }

    /** Convert packed 0xRRGGBB to HSV floats in [0,1]. */
    struct HsvColor { float h, s, v; };

    /** Marshal an HsvColor into a QJSValue {h,s,v} object. */
    static inline QJSValue hsvToJs(QJSEngine *engine, const HsvColor &hsv)
    {
        QJSValue obj = engine->newObject();
        obj.setProperty(QStringLiteral("h"), QJSValue(double(hsv.h)));
        obj.setProperty(QStringLiteral("s"), QJSValue(double(hsv.s)));
        obj.setProperty(QStringLiteral("v"), QJSValue(double(hsv.v)));
        return obj;
    }

    static inline HsvColor rgbToHsv(uint packed)
    {
        float r = float((packed >> 16) & 0xFF) / 255.0f;
        float g = float((packed >> 8) & 0xFF) / 255.0f;
        float b = float(packed & 0xFF) / 255.0f;
        float mx = std::max({r, g, b});
        float mn = std::min({r, g, b});
        float d = mx - mn;
        float h = 0.0f;
        float s = (mx == 0.0f) ? 0.0f : d / mx;
        if (d != 0.0f) {
            if (mx == r) h = fmodf((g - b) / d + 6.0f, 6.0f) / 6.0f;
            else if (mx == g) h = ((b - r) / d + 2.0f) / 6.0f;
            else h = ((r - g) / d + 4.0f) / 6.0f;
        }
        return {h, s, mx};
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
    , m_audioRegistered(false)
    , m_hsvContractValidated(false)
{
}

RGBScript::RGBScript(const RGBScript& s)
    : RGBAlgorithm(s.doc())
    , m_fileName(s.m_fileName)
    , m_contents(s.m_contents)
    , m_apiVersion(0)
    , m_usesAudio(false)
    , m_audioInput(NULL)
    , m_audioRegistered(false)
    , m_hsvContractValidated(false)
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
            QStringLiteral("hsvutil.js")
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
    m_hsvContractValidated = false;

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

        // Extract audio input categories: prefer explicit algo.audioInputs,
        // fall back to auto-detection from source.
        m_audioInputCategories.clear();
        if (m_usesAudio)
        {
            QJSValue inputsVal = m_script.property(QStringLiteral("audioInputs"));
            if (inputsVal.isArray())
            {
                const int len = inputsVal.property(QStringLiteral("length")).toInt();
                for (int i = 0; i < len; ++i)
                {
                    QString cat = inputsVal.property(quint32(i)).toString();
                    if (!cat.isEmpty() && !m_audioInputCategories.contains(cat))
                        m_audioInputCategories.append(cat);
                }
            }
            else
            {
                // Auto-detect from source: extract top-level audio.X references
                static const QRegularExpression rx(QStringLiteral("\\baudio\\.(\\w+)"));
                QRegularExpressionMatchIterator it = rx.globalMatch(m_contents);
                QSet<QString> seen;
                while (it.hasNext())
                {
                    QString cat = it.next().captured(1);
                    if (cat != QStringLiteral("timing") && !seen.contains(cat))
                    {
                        seen.insert(cat);
                        m_audioInputCategories.append(cat);
                    }
                }
                m_audioInputCategories.sort();
            }
        }

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

    // HSV-only contract: scripts receive an array of {h,s,v} objects, never
    // packed RGB integers. Convert each stop here.
    QJSEngine *engine = s_jsThread->engine;
    QJSValue jsRawColors = engine->newArray(accColors);
    for (int i = 0; i < rawColorCount && i < accColors; i++)
    {
        HsvColor hsv = rgbToHsv(colors.at(i) & 0xFFFFFFu);
        jsRawColors.setProperty(i, hsvToJs(engine, hsv));
    }

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

    // Resolve owning matrix once per frame — avoids repeated O(N) linear scan.
    RGBMatrix *matrix = owningMatrix(doc(), this);

    // Inject the user's color palette as algo.colors (array of {h,s,v}).
    injectColors(rgb, matrix);

    // If this is an audio-aware script, set up audio capture on first call
    // and inject audio data as a 5th argument
    if (m_usesAudio)
    {
        setupAudioCapture();
    }

    QJSValueList args;
    // 3rd argument is the primary color as {h,s,v} (HSV-only contract).
    // Same value as algo.color set by injectGradientArrays(); kept positional
    // so the rgbMap(width, height, color, step[, audio]) signature is stable.
    QJSEngine *engine = s_jsThread->engine;
    HsvColor primary = rgbToHsv(rgb & 0xFFFFFFu);
    args << size.width() << size.height() << hsvToJs(engine, primary) << step;

    if (m_usesAudio)
        args << buildAudioDataObject();

    QJSValue yarray(m_rgbMap.call(args));
    if (yarray.isError())
    {
        displayError(yarray, m_fileName);
        return;
    }

    // HSV-only contract: scripts MUST return a Float32Array of length
    // width*height*3 (interleaved H,S,V floats in [0,1]). The engine converts
    // to packed RGB here. Returning a Uint32Array (the legacy RGB path) is
    // no longer supported.
    const int width = size.width();
    const int height = size.height();
    if (width <= 0 || height <= 0)
        return;

    const int expectedHsvBytes = width * height * 3 * 4;    // Float32Array: 3 floats/pixel

    // Full validation on first frame; fast-path on subsequent frames.
    if (!m_hsvContractValidated)
    {
        QJSValue bpeProp = yarray.property(QStringLiteral("BYTES_PER_ELEMENT"));
        if (!bpeProp.isNumber())
        {
            qWarning() << "RGBScript" << m_fileName
                       << "rgbMap() did not return a TypedArray. HSV contract"
                       << "requires a flat Float32Array of length width*height*3.";
            return;
        }

        QJSValue ctorName = yarray.property(QStringLiteral("constructor"))
                                  .property(QStringLiteral("name"));
        if (ctorName.toString() != QStringLiteral("Float32Array"))
        {
            qWarning() << "RGBScript" << m_fileName
                       << "returned" << ctorName.toString()
                       << "but only Float32Array (HSV) is supported.";
            return;
        }

        if (bpeProp.toInt() != 4)
        {
            qWarning() << "RGBScript" << m_fileName
                       << "unexpected BYTES_PER_ELEMENT:" << bpeProp.toInt();
            return;
        }

        m_hsvContractValidated = true;
    }

    const int byteLength = yarray.property(QStringLiteral("byteLength")).toInt();
    if (byteLength != expectedHsvBytes)
    {
        qWarning() << "RGBScript" << m_fileName
                   << "TypedArray size mismatch: byteLength=" << byteLength
                   << "(expected" << expectedHsvBytes << ")";
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
    if (bytes.size() != expectedHsvBytes)
    {
        qWarning() << "RGBScript" << m_fileName
                   << "ArrayBuffer extraction size mismatch: got" << bytes.size()
                   << "expected" << expectedHsvBytes;
        return;
    }

    map.resize(height);
    // Convert interleaved float H,S,V triples to packed RGB.
    const float *src = reinterpret_cast<const float*>(bytes.constData());
    for (int y = 0; y < height; ++y)
    {
        map[y].resize(width);
        const float *row = src + (y * width * 3);
        for (int x = 0; x < width; ++x)
        {
            int i = x * 3;
            map[y][x] = hsvToRgb(row[i], row[i + 1], row[i + 2]);
        }
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

QStringList RGBScript::audioInputCategories() const
{
    return m_audioInputCategories;
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

void RGBScript::injectColors(uint rgb, RGBMatrix *matrix)
{
    QJSEngine *engine = s_jsThread->engine;
    if (engine == NULL || m_script.isUndefined())
        return;

    int numColors = acceptColors();
    if (numColors <= 0)
    {
        m_script.setProperty(QStringLiteral("colors"), engine->newArray(0));
        return;
    }

    // Convert user-picked color stops to HSV. Invalid/missing slots get
    // the step color fallback so algo.colors always has exactly numColors elements.
    HsvColor fallback = rgbToHsv(rgb & 0xFFFFFFu);
    QVector<QColor> cols;
    if (matrix != NULL)
        cols = matrix->getColors();

    // Check if any user-picked color is set (vs all being fallback/default).
    bool anyUserSet = false;
    QJSValue colorsArr = engine->newArray(quint32(numColors));
    for (int i = 0; i < numColors; ++i)
    {
        bool valid = (i < cols.size() && cols.at(i).isValid());
        if (valid) anyUserSet = true;
        HsvColor hsv = valid ? rgbToHsv(cols.at(i).rgb() & 0xFFFFFFu) : fallback;
        colorsArr.setProperty(quint32(i), hsvToJs(engine, hsv));
    }

    m_script.setProperty(QStringLiteral("colors"), colorsArr);
    m_script.setProperty(QStringLiteral("hasUserColors"), QJSValue(anyUserSet));
}

QJSValue RGBScript::buildAudioDataObject()
{
    QJSEngine *engine = s_jsThread->engine;
    QJSValue audioObj = engine->newObject();
    AudioChannel *channel = NULL;
    AudioChannelConfig config = AudioChannelConfig::defaults();
    Doc *currentDoc = doc();
    AudioProfile *profile = NULL;
    if (currentDoc != NULL)
    {
        profile = currentDoc->audioProfile(currentDoc->activeAudioProfileId());
        if (profile == NULL)
            profile = currentDoc->defaultAudioProfile();
    }
    if (profile != NULL)
    {
        channel = profile->channel();
        config = (channel != NULL) ? channel->config() : profile->channelConfig();
    }

    AudioSnapshot snap;
    if (channel != NULL)
        snap = channel->snapshot();

    const int onsetMethodIndex = std::clamp(config.aubio.onsetMethodIndex,
                                            0, AUBIO_ONSET_METHODS - 1);

    const bool tempoActive = snap.music.bpm > 0.0;
    const double bpm = tempoActive ? snap.music.bpm : 120.0;
    const double beatPhase = snap.music.beatPhase;
    const double dt = (double(MasterTimer::tick()) / 1000.0) * (bpm / 60.0);

    audioObj.setProperty(QStringLiteral("beat"),           QJSValue(snap.beatPower));
    audioObj.setProperty(QStringLiteral("bass"),           QJSValue(snap.bassPower));
    audioObj.setProperty(QStringLiteral("low"),            QJSValue(snap.lows));
    audioObj.setProperty(QStringLiteral("mid"),            QJSValue(snap.mids));
    audioObj.setProperty(QStringLiteral("high"),           QJSValue(snap.highs));
    audioObj.setProperty(QStringLiteral("onset"),          QJSValue(onsetFiredAt(snap, onsetMethodIndex)));
    audioObj.setProperty(QStringLiteral("onsetIntensity"), QJSValue(snap.onsets.thresholdedDescriptors[onsetMethodIndex]));
    audioObj.setProperty(QStringLiteral("beatFired"),      QJSValue(snap.beatTrigger.firedThisFrame));
    audioObj.setProperty(QStringLiteral("downbeat"),       QJSValue(snap.downbeatFired));
    audioObj.setProperty(QStringLiteral("bpm"),            QJSValue(bpm));
    audioObj.setProperty(QStringLiteral("phase"),          QJSValue(beatPhase));
    audioObj.setProperty(QStringLiteral("barPhase"),       QJSValue(snap.music.barPhase / double(kBeatsPerBar)));
    audioObj.setProperty(QStringLiteral("dt"),             QJSValue(tempoActive ? dt : 0.0));
    audioObj.setProperty(QStringLiteral("cosPulse"),       QJSValue(tempoActive
        ? std::max(0.0, std::cos(beatPhase * kPi)) : 0.0));
    audioObj.setProperty(QStringLiteral("version"),        QJSValue(kAudioApiVersion));

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
