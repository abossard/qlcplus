/*
  Q Light Controller Plus
  huescript.cpp

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

#include <QRegularExpression>
#include <QTextStream>
#include <QJSEngine>
#include <QThread>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QSet>

#include <algorithm>
#include <cmath>

#include "jsthread_p.h"
#include "huescript.h"
#include "huescriptscache.h"
#include "huematrix.h"

#include "audiochannel.h"
#include "audiocapture.h"
#include "audioprofile.h"
#include "audiosnapshot.h"
#include "mastertimer.h"
#include "doc.h"

#include "huecolor.h"

namespace
{
    static constexpr int kAudioApiVersion = 5;
    static constexpr int kBeatsPerBar = 4;
    static constexpr double kPi = 3.14159265358979323846;

    RGBMatrix *owningMatrix(Doc *doc, const RGBScript *script)
    {
        if (doc == NULL || script == NULL)
            return NULL;

        foreach (Function *function, doc->functionsByType(Function::HUEMatrixType))
        {
            RGBMatrix *matrix = qobject_cast<RGBMatrix*> (function);
            if (matrix != NULL && matrix->algorithm() == script)
                return matrix;
        }

        return NULL;
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

HUEScript::HUEScript(Doc *doc)
    : RGBScript(doc)
    , m_hsvContract(true)
    , m_usesAudio(false)
    , m_audioInput(NULL)
    , m_audioRegistered(false)
    , m_hsvContractValidated(false)
{
}

HUEScript::HUEScript(const HUEScript &s)
    : RGBScript(s)
    , m_hsvContract(s.m_hsvContract)
    , m_usesAudio(false)
    , m_audioInput(NULL)
    , m_audioRegistered(false)
    , m_hsvContractValidated(false)
{
    // RGBScript's copy constructor already evaluated the contents through the
    // base implementation; re-read the audio metadata for this instance.
    parseAudioMetadata();
}

HUEScript::~HUEScript()
{
    teardownAudioCapture();
}

RGBAlgorithm *HUEScript::clone() const
{
    HUEScript *script = new HUEScript(*this);
    return static_cast<RGBAlgorithm*> (script);
}

void HUEScript::setHsvContract(bool hsv)
{
    m_hsvContract = hsv;
}

bool HUEScript::hsvContract() const
{
    return m_hsvContract;
}

/****************************************************************************
 * Load & Evaluation
 ****************************************************************************/

void HUEScript::loadHsvShims()
{
    static bool s_loaded = false;


    RGBScript::initEngine();

    if (s_loaded || s_jsThread == NULL || s_jsThread->engine == NULL)
        return;

    s_loaded = true;

    const QStringList shimNames = { QStringLiteral("hsvutil.js") };

    for (const QString &shimName : shimNames)
    {
        QString shimPath;
        foreach (QString dirPath, HUEScriptsCache::hsvScriptDirectories())
        {
            QString candidate = QDir(dirPath).filePath(shimName);
            if (QFile::exists(candidate))
            {
                shimPath = candidate;
                break;
            }
        }

        QFile shimFile(shimPath);
        if (shimPath.isEmpty() || shimFile.open(QIODevice::ReadOnly) == false)
        {
            qWarning() << "[HUEScript] shim not found:" << shimName;
            continue;
        }

        QString shimContents = QTextStream(&shimFile).readAll();
        shimFile.close();

        auto runShim = [shimContents, shimPath]{
            QJSValue result = s_jsThread->engine->evaluate(shimContents, shimPath);
            if (result.isError())
                RGBScript::displayError(result, shimPath);
        };

        // evaluate() may already have marshalled us onto the JS thread; a
        // blocking queued call to our own thread is a deadlock.
        if (QThread::currentThread() == s_jsThread)
            runShim();
        else
            QMetaObject::invokeMethod(s_jsThread->engine, runShim, Qt::BlockingQueuedConnection);
    }
}

bool HUEScript::evaluate()
{
    // Bring the JS thread up before the guard, so the very first evaluate()
    // is marshalled too rather than running inline on the caller's thread.
    RGBScript::initEngine();

    // Every QJSValue this method reaches belongs to the JS engine thread.
    // Same guard shape as RGBScript::evaluate().
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        bool retVal = false;
        QMetaObject::invokeMethod(s_jsThread->engine, [this]{ return evaluate(); },
                                  Qt::BlockingQueuedConnection, &retVal);
        return retVal;
    }

    loadHsvShims();

    m_usesAudio = false;
    m_audioInputCategories.clear();
    m_hsvContractValidated = false;

    if (RGBScript::evaluate() == false)
        return false;

    parseAudioMetadata();
    return true;
}

void HUEScript::parseAudioMetadata()
{
    // Reads m_script properties, so it must run on the JS engine thread.
    // Reachable from the copy constructor, which has no guard of its own.
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QMetaObject::invokeMethod(s_jsThread->engine, [this]{ parseAudioMetadata(); },
                                  Qt::BlockingQueuedConnection);
        return;
    }

    m_usesAudio = false;
    m_audioInputCategories.clear();

    if (m_script.isObject() == false)
        return;

    QJSValue usesAudioVal = m_script.property(QStringLiteral("usesAudio"));
    m_usesAudio = (!usesAudioVal.isUndefined() && usesAudioVal.toBool());
    if (m_usesAudio == false)
        return;

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
        return;
    }

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

bool HUEScript::scheduleOnJSThread(std::function<void()> fn)
{
    if (s_jsThread == NULL || s_jsThread->engine == NULL)
        return false;

    QMetaObject::invokeMethod(s_jsThread->engine, std::move(fn), Qt::QueuedConnection);
    return true;
}

/****************************************************************************
 * RGBAlgorithm API
 ****************************************************************************/

void HUEScript::rgbMapSetColors(const QVector<uint> &colors)
{
    // Same guard shape as RGBScript::rgbMapSetColors().
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QMetaObject::invokeMethod(s_jsThread->engine, [this, colors]{ rgbMapSetColors(colors); },
                                  Qt::QueuedConnection);
        return;
    }

    if (m_hsvContract == false)
    {
        RGBScript::rgbMapSetColors(colors);
        return;
    }

    if (m_apiVersion <= 2 || m_rgbMapSetColors.isCallable() == false)
        return;

    int accColors = acceptColors();
    int rawColorCount = colors.count();

    // HSV contract: scripts receive an array of {h,s,v} objects, never
    // packed RGB integers.
    QJSEngine *engine = s_jsThread->engine;
    QJSValue jsRawColors = engine->newArray(accColors);
    for (int i = 0; i < rawColorCount && i < accColors; i++)
        jsRawColors.setProperty(i, HUEColor::hsvToJs(engine, HUEColor::rgbToHsv(colors.at(i) & 0xFFFFFFu)));

    QJSValueList args;
    args << jsRawColors;

    QJSValue value = m_rgbMapSetColors.call(args);
    if (value.isError())
        displayError(value, m_fileName);
}

void HUEScript::rgbMap(const QSize &size, uint rgb, int step, RGBMap &map)
{
    // Same guard shape as RGBScript::rgbMap(). This is also what protects the
    // live DMX path, where HUEMatrix::write() calls us on the MasterTimer
    // thread whenever the async precompute misses.
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QMetaObject::invokeMethod(s_jsThread->engine,
                                  [this, size, rgb, step, &map]{ rgbMap(size, rgb, step, map); },
                                  Qt::BlockingQueuedConnection);
        return;
    }

    if (m_hsvContract == false)
    {
        // Upstream contract: nested packed-uint arrays, packed uint color arg.
        RGBScript::rgbMap(size, rgb, step, map);
        return;
    }

    if (m_rgbMap.isUndefined() == true)
        return;

    RGBMatrix *matrix = owningMatrix(doc(), this);

    injectColors(rgb, matrix);

    if (m_usesAudio)
        setupAudioCapture();

    QJSEngine *engine = s_jsThread->engine;
    QJSValueList args;
    // 3rd argument is the primary color as {h,s,v}; the signature
    // rgbMap(width, height, color, step[, audio]) stays positional.
    args << size.width() << size.height()
         << HUEColor::hsvToJs(engine, HUEColor::rgbToHsv(rgb & 0xFFFFFFu)) << step;

    if (m_usesAudio)
        args << buildAudioDataObject();

    QJSValue yarray(m_rgbMap.call(args));
    if (yarray.isError())
    {
        displayError(yarray, m_fileName);
        return;
    }

    const int width = size.width();
    const int height = size.height();
    if (width <= 0 || height <= 0)
        return;

    const int expectedHsvBytes = width * height * 3 * 4;    // 3 floats/pixel

    // Full validation on first frame; fast-path on subsequent frames.
    if (!m_hsvContractValidated)
    {
        QJSValue bpeProp = yarray.property(QStringLiteral("BYTES_PER_ELEMENT"));
        if (!bpeProp.isNumber())
        {
            qWarning() << "HUEScript" << m_fileName
                       << "rgbMap() did not return a TypedArray. HSV contract"
                       << "requires a flat Float32Array of length width*height*3.";
            return;
        }

        QJSValue ctorName = yarray.property(QStringLiteral("constructor"))
                                  .property(QStringLiteral("name"));
        if (ctorName.toString() != QStringLiteral("Float32Array"))
        {
            qWarning() << "HUEScript" << m_fileName
                       << "returned" << ctorName.toString()
                       << "but only Float32Array (HSV) is supported.";
            return;
        }

        if (bpeProp.toInt() != 4)
        {
            qWarning() << "HUEScript" << m_fileName
                       << "unexpected BYTES_PER_ELEMENT:" << bpeProp.toInt();
            return;
        }

        m_hsvContractValidated = true;
    }

    const int byteLength = yarray.property(QStringLiteral("byteLength")).toInt();
    if (byteLength != expectedHsvBytes)
    {
        qWarning() << "HUEScript" << m_fileName
                   << "TypedArray size mismatch: byteLength=" << byteLength
                   << "(expected" << expectedHsvBytes << ")";
        return;
    }

    QJSValue bufProp = yarray.property(QStringLiteral("buffer"));
    if (!bufProp.isObject())
    {
        qWarning() << "HUEScript" << m_fileName
                   << "TypedArray has no underlying ArrayBuffer";
        return;
    }

    QByteArray bytes = bufProp.toVariant().toByteArray();
    if (bytes.size() != expectedHsvBytes)
    {
        qWarning() << "HUEScript" << m_fileName
                   << "ArrayBuffer extraction size mismatch: got" << bytes.size()
                   << "expected" << expectedHsvBytes;
        return;
    }

    map.resize(height);
    const float *src = reinterpret_cast<const float*> (bytes.constData());
    for (int y = 0; y < height; ++y)
    {
        map[y].resize(width);
        const float *row = src + (y * width * 3);
        for (int x = 0; x < width; ++x)
        {
            int i = x * 3;
            map[y][x] = HUEColor::hsvToRgb(row[i], row[i + 1], row[i + 2]);
        }
    }
}

bool HUEScript::usesAudio() const
{
    return m_usesAudio;
}

QStringList HUEScript::audioInputCategories() const
{
    return m_audioInputCategories;
}

void HUEScript::setDisplaySize(const QSize &size)
{
    if (s_jsThread != NULL && QThread::currentThread() != s_jsThread)
    {
        QMetaObject::invokeMethod(s_jsThread->engine, [this, size]{ setDisplaySize(size); },
                                  Qt::BlockingQueuedConnection);
        return;
    }

    if (!m_script.isObject())
        return;

    m_script.setProperty(QStringLiteral("displayWidth"), size.width());
    m_script.setProperty(QStringLiteral("displayHeight"), size.height());
}

void HUEScript::postRun()
{
    teardownAudioCapture();
}

/****************************************************************************
 * Audio support
 ****************************************************************************/

void HUEScript::setupAudioCapture()
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
    // AudioChannel snapshot inside buildAudioDataObject(); we only need the
    // capture thread to be running.
    m_audioInput->registerBandsNumber(1);
    m_audioRegistered = true;
}

void HUEScript::teardownAudioCapture()
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

void HUEScript::injectColors(uint rgb, RGBMatrix *matrix)
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

    // Convert user-picked color stops to HSV. Invalid/missing slots get the
    // step color fallback so algo.colors always has exactly numColors entries.
    HUEColor::Hsv fallback = HUEColor::rgbToHsv(rgb & 0xFFFFFFu);
    QVector<QColor> cols;
    if (matrix != NULL)
        cols = matrix->getColors();

    bool anyUserSet = false;
    QJSValue colorsArr = engine->newArray(quint32(numColors));
    for (int i = 0; i < numColors; ++i)
    {
        bool valid = (i < cols.size() && cols.at(i).isValid());
        if (valid)
            anyUserSet = true;
        HUEColor::Hsv hsv = valid ? HUEColor::rgbToHsv(cols.at(i).rgb() & 0xFFFFFFu) : fallback;
        colorsArr.setProperty(quint32(i), HUEColor::hsvToJs(engine, hsv));
    }

    m_script.setProperty(QStringLiteral("colors"), colorsArr);
    m_script.setProperty(QStringLiteral("hasUserColors"), QJSValue(anyUserSet));
}

QJSValue HUEScript::buildAudioDataObject()
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
