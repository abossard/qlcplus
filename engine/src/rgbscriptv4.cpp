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
#include <QThread>
#include <QDebug>
#include <QFile>
#include <QDir>

// cppcheck-suppress missingIncludeSystem
#include <QCoreApplication>
// cppcheck-suppress missingIncludeSystem
#include <QSemaphore>

#include "rgbscriptv4.h"

#include "rgbscriptscache.h"
#include "audiocapture.h"
#include "mastertimer.h"
#include "qlcconfig.h"
#include "qlcfile.h"
#include "doc.h"

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
    , m_audioBandsNumber(-1)
    , m_audioMaxMagnitude(0)
    , m_audioPower(0)
    , m_audioBeat(false)
{
}

RGBScript::RGBScript(const RGBScript& s)
    : RGBAlgorithm(s.doc())
    , m_fileName(s.m_fileName)
    , m_contents(s.m_contents)
    , m_apiVersion(0)
    , m_usesAudio(false)
    , m_audioInput(NULL)
    , m_audioBandsNumber(-1)
    , m_audioMaxMagnitude(0)
    , m_audioPower(0)
    , m_audioBeat(false)
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

        // Load the LedFX compatibility shim into the engine's global scope
        // so all audio-reactive scripts can use LedFx.* helpers.
        QDir scriptsDir = RGBScriptsCache::systemScriptsDirectory();
        QString shimPath = scriptsDir.filePath("ledfx_compat.js");
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
                    qDebug() << "[RGBScript] Loaded LedFX compatibility shim";
            }, Qt::BlockingQueuedConnection);
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

        // Auto-inject rotation and mirror properties for audio scripts
        if (m_usesAudio)
        {
            // Set defaults if not already defined by the script
            if (m_script.property("presetRotation").isUndefined())
                m_script.setProperty("presetRotation", 0);
            if (m_script.property("presetMirror").isUndefined())
                m_script.setProperty("presetMirror", 0);

            // Add setter/getter functions for the properties
            s_jsThread->engine->evaluate(
                "(function(a) {"
                "  if (!a._rotSet) {"
                "    a.setRotation = function(v) { a.presetRotation = parseInt(v); };"
                "    a.getRotation = function() { return a.presetRotation; };"
                "    a.setMirror = function(v) {"
                "      if (v === 'Horizontal') a.presetMirror = 1;"
                "      else if (v === 'Vertical') a.presetMirror = 2;"
                "      else if (v === 'Both') a.presetMirror = 3;"
                "      else a.presetMirror = 0;"
                "    };"
                "    a.getMirror = function() {"
                "      if (a.presetMirror === 1) return 'Horizontal';"
                "      if (a.presetMirror === 2) return 'Vertical';"
                "      if (a.presetMirror === 3) return 'Both';"
                "      return 'Off';"
                "    };"
                "    a.properties.push('name:presetRotation|type:list|display:Rotation|"
                "values:0,90,180,270|write:setRotation|read:getRotation');"
                "    a.properties.push('name:presetMirror|type:list|display:Mirror|"
                "values:Off,Horizontal,Vertical,Both|write:setMirror|read:getMirror');"
                "    a._rotSet = true;"
                "  }"
                "})"
            ).call(QJSValueList() << m_script);
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

    // Read rotation/mirror for audio scripts (0=0°, 1=90°, 2=180°, 3=270°)
    int rotation = 0;
    int mirror = 0; // 0=off, 1=horizontal, 2=vertical, 3=both
    if (m_usesAudio)
    {
        QJSValue rotVal = m_script.property("presetRotation");
        if (!rotVal.isUndefined()) rotation = (rotVal.toInt() / 90) & 3;
        QJSValue mirVal = m_script.property("presetMirror");
        if (!mirVal.isUndefined()) mirror = mirVal.toInt() & 3;
    }

    // For 90°/270° rotation, swap width↔height so JS renders in rotated space
    QSize jsSize = size;
    if (rotation == 1 || rotation == 3)
        jsSize = QSize(size.height(), size.width());

    // If this is an audio-aware script, set up audio capture on first call
    // and inject audio data as a 5th argument
    if (m_usesAudio)
    {
        setupAudioCapture();

        // Register fixed band count (max 32). JS shim interpolates to grid width.
        if (m_audioBandsNumber != AUDIO_FIXED_BANDS && m_audioInput != NULL)
        {
            if (m_audioBandsNumber > 0)
                m_audioInput->unregisterBandsNumber(m_audioBandsNumber);
            m_audioBandsNumber = AUDIO_FIXED_BANDS;
            m_audioInput->registerBandsNumber(AUDIO_FIXED_BANDS);
        }
    }

    // Call the rgbMap function with (possibly swapped) dimensions
    QJSValueList args;
    args << jsSize.width() << jsSize.height() << rgb << step;

    if (m_usesAudio)
        args << buildAudioDataObject();

    QJSValue yarray(m_rgbMap.call(args));
    if (yarray.isError())
        displayError(yarray, m_fileName);

    // Parse the returned 2D array into a temporary map
    RGBMap jsMap;
    if (yarray.isArray())
    {
        QVariantList yvArray = yarray.toVariant().toList();
        int ylen = yvArray.length();
        jsMap.resize(ylen);

        for (int y = 0; y < ylen && y < jsSize.height(); y++)
        {
            QVariantList xvArray = yvArray.at(y).toList();
            int xlen = xvArray.length();
            jsMap[y].resize(xlen);

            for (int x = 0; x < xlen && x < jsSize.width(); x++)
                jsMap[y][x] = xvArray.at(x).toUInt();
        }
    }
    else
    {
        qWarning() << "Returned value is not an array within an array!";
        return;
    }

    // Apply rotation and mirror transforms
    if (m_usesAudio && (rotation || mirror))
        applyTransforms(jsMap, jsSize, size, rotation, mirror, map);
    else
        map = jsMap;
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

    // Use DirectConnection so lambdas fire on AudioCapture's thread immediately.
    // Thread safety is handled by m_audioMutex in the lambdas.
    m_audioDataConn = QObject::connect(m_audioInput, &AudioCapture::dataProcessed,
        [this](double *spectrumBands, int size, double maxMagnitude, quint32 power)
        {
            int expected = m_audioBandsNumber.load();
            if (size != expected || expected <= 0)
                return;
            QMutexLocker locker(&m_audioMutex);
            m_audioSpectrum.resize(expected);
            for (int i = 0; i < expected; i++)
                m_audioSpectrum[i] = spectrumBands[i];
            m_audioMaxMagnitude = maxMagnitude;
            m_audioPower = power;
        });
    m_audioBeatConn = QObject::connect(m_audioInput, &AudioCapture::beatDetected,
        [this]()
        {
            QMutexLocker locker(&m_audioMutex);
            m_audioBeat = true;
        });
}

void RGBScript::teardownAudioCapture()
{
    if (m_audioInput != NULL)
    {
        // Unregister first — this blocks on AudioCapture::m_mutex,
        // ensuring any in-flight processData() emission completes
        // before we disconnect.
        if (m_audioBandsNumber > 0)
            m_audioInput->unregisterBandsNumber(m_audioBandsNumber);

        // Now safe to disconnect
        QObject::disconnect(m_audioDataConn);
        QObject::disconnect(m_audioBeatConn);

        m_audioInput = NULL;
        m_audioBandsNumber = -1;
    }

    // Clear stale audio state
    QMutexLocker locker(&m_audioMutex);
    m_audioSpectrum.clear();
    m_audioMaxMagnitude = 0;
    m_audioPower = 0;
    m_audioBeat = false;
}

QJSValue RGBScript::buildAudioDataObject()
{
    QMutexLocker locker(&m_audioMutex);

    QJSValue audioObj = s_jsThread->engine->newObject();

    // Build normalized spectrum array (0.0 - 1.0)
    int specSize = m_audioSpectrum.size();
    QJSValue spectrumArr = s_jsThread->engine->newArray(specSize);
    for (int i = 0; i < specSize; i++)
    {
        double normalized = (m_audioMaxMagnitude > 0)
            ? qMin(1.0, m_audioSpectrum[i] / m_audioMaxMagnitude) : 0.0;
        spectrumArr.setProperty(i, QJSValue(normalized));
    }
    audioObj.setProperty("spectrum", spectrumArr);

    // Volume: normalize power to 0.0-1.0
    audioObj.setProperty("volume", QJSValue(double(m_audioPower) / 0x7FFF));

    // Beat: consumed on read (reset after building object)
    audioObj.setProperty("beat", QJSValue(m_audioBeat));
    m_audioBeat = false;

    // BPM from MasterTimer
    int bpm = 120;
    if (doc() && doc()->masterTimer())
        bpm = doc()->masterTimer()->bpmNumber();
    audioObj.setProperty("bpm", QJSValue(bpm));

    // Raw maxMagnitude for scripts that want absolute values
    audioObj.setProperty("maxMagnitude", QJSValue(m_audioMaxMagnitude));

    return audioObj;
}

void RGBScript::applyTransforms(const RGBMap &src, const QSize &srcSize,
                                const QSize &dstSize, int rotation, int mirror,
                                RGBMap &dst)
{
    Q_UNUSED(dstSize);
    int sh = srcSize.height();
    int sw = srcSize.width();

    // Step 1: Rotation
    // 0° and 90°: output = src as-is (90° already got swapped dimensions)
    // 180° and 270°: flip both axes (270° already got swapped dimensions)
    if (rotation == 0 || rotation == 1)
    {
        dst = src;
    }
    else // 180° or 270°: reverse rows and reverse each row
    {
        dst.resize(sh);
        for (int y = 0; y < sh; y++)
        {
            int sy = sh - 1 - y;
            if (sy >= src.size()) continue;
            dst[y].resize(sw);
            for (int x = 0; x < sw && x < src[sy].size(); x++)
                dst[y][x] = src[sy][sw - 1 - x];
        }
    }

    // Step 2: Mirror using max() blending (LedFX style)
    int dw = dst.isEmpty() ? 0 : dst[0].size();
    int dh = dst.size();

    if (mirror & 1) // Horizontal mirror
    {
        for (int y = 0; y < dh; y++)
        {
            for (int x = 0; x < dw / 2; x++)
            {
                int mx = dw - 1 - x;
                uint left = dst[y][x];
                uint right = dst[y][mx];
                uint r = qMax((left >> 16) & 0xFF, (right >> 16) & 0xFF);
                uint g = qMax((left >> 8) & 0xFF, (right >> 8) & 0xFF);
                uint b = qMax(left & 0xFF, right & 0xFF);
                uint merged = (r << 16) | (g << 8) | b;
                dst[y][x] = merged;
                dst[y][mx] = merged;
            }
        }
    }

    if (mirror & 2) // Vertical mirror
    {
        for (int y = 0; y < dh / 2; y++)
        {
            int my = dh - 1 - y;
            for (int x = 0; x < dw; x++)
            {
                uint top = dst[y][x];
                uint bot = dst[my][x];
                uint r = qMax((top >> 16) & 0xFF, (bot >> 16) & 0xFF);
                uint g = qMax((top >> 8) & 0xFF, (bot >> 8) & 0xFF);
                uint b = qMax(top & 0xFF, bot & 0xFF);
                uint merged = (r << 16) | (g << 8) | b;
                dst[y][x] = merged;
                dst[my][x] = merged;
            }
        }
    }
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
