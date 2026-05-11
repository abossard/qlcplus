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

    /** Republish a previously-built HSV gradient JS array onto the audio object.
     *  When no gradient is set yet (script just loaded), return an array of
     *  `fallbackCount` neutral {h:0,s:0,v:0} stops so audio scripts can index
     *  blindly without isUndefined() checks. */
    QJSValue hsvColorArrayToJs(QJSEngine *engine, const QJSValue &source, int fallbackCount)
    {
        if (!source.isUndefined())
            return source;
        QJSValue arr = engine->newArray(quint32(fallbackCount));
        for (int i = 0; i < fallbackCount; i++)
        {
            QJSValue obj = engine->newObject();
            obj.setProperty(QStringLiteral("h"), QJSValue(0.0));
            obj.setProperty(QStringLiteral("s"), QJSValue(0.0));
            obj.setProperty(QStringLiteral("v"), QJSValue(0.0));
            arr.setProperty(quint32(i), obj);
        }
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

    /*********************************************************************
     * Audio Routing helpers
     *
     * Per-RGBMatrix audio slot remapping. The engine mutates the local
     * AudioSnapshot copy so scripts only ever see routed data.
     *********************************************************************/

    double getSourceScalar(const AudioSnapshot &orig,
                           RGBMatrix::AudioSource src,
                           double native,
                           int onsetMethodIndex)
    {
        switch (src)
        {
            case RGBMatrix::AudioSrcZero:    return 0.0;
            case RGBMatrix::AudioSrcLow:     return orig.lows;
            case RGBMatrix::AudioSrcMid:     return orig.mids;
            case RGBMatrix::AudioSrcHigh:    return orig.highs;
            case RGBMatrix::AudioSrcBeat:    return orig.beatTrigger.value;
            case RGBMatrix::AudioSrcKick:    return orig.kickTrigger.value;
            case RGBMatrix::AudioSrcOnset:   return orig.onsets.thresholdedDescriptors[onsetMethodIndex];
            case RGBMatrix::AudioSrcVolume:  return orig.volume.volumeNorm;
            case RGBMatrix::AudioSrcDefault:
            default:                         return native;
        }
    }

    TriggerState getSourceTrigger(const AudioSnapshot &orig,
                                  RGBMatrix::AudioSource src,
                                  const TriggerState &native,
                                  int onsetMethodIndex)
    {
        switch (src)
        {
            case RGBMatrix::AudioSrcZero:   return TriggerState{};
            case RGBMatrix::AudioSrcLow:    return orig.triggers[0];
            case RGBMatrix::AudioSrcMid:    return orig.triggers[1];
            case RGBMatrix::AudioSrcHigh:   return orig.triggers[2];
            case RGBMatrix::AudioSrcBeat:   return orig.beatTrigger;
            case RGBMatrix::AudioSrcKick:   return orig.kickTrigger;
            case RGBMatrix::AudioSrcVolume: return orig.volumeTrigger;
            case RGBMatrix::AudioSrcOnset:
            {
                TriggerState t;
                t.value = orig.onsets.thresholdedDescriptors[onsetMethodIndex];
                t.active = onsetFiredAt(orig, onsetMethodIndex);
                t.firedThisFrame = t.active;
                return t;
            }
            case RGBMatrix::AudioSrcDefault:
            default:                        return native;
        }
    }

    void applyAudioRouting(AudioSnapshot &snap,
                           const AudioSnapshot &orig,
                           const RGBMatrix::AudioRouting &routing,
                           int onsetMethodIndex)
    {
        // Low slot: snap.lows, snap.triggers[0], snap.beatPower, snap.bassPower
        if (routing.low != RGBMatrix::AudioSrcDefault)
        {
            snap.lows = getSourceScalar(orig, routing.low, orig.lows, onsetMethodIndex);
            snap.triggers[0] = getSourceTrigger(orig, routing.low, orig.triggers[0], onsetMethodIndex);
            if (routing.low == RGBMatrix::AudioSrcZero)
            {
                snap.beatPower = 0.0;
                snap.bassPower = 0.0;
            }
            else if (routing.low != RGBMatrix::AudioSrcLow)
            {
                // Non-native source: clear sub-band detail so detail.beat/bass
                // stay consistent with the remapped low.
                snap.beatPower = 0.0;
                snap.bassPower = 0.0;
            }
        }

        // Mid slot
        if (routing.mid != RGBMatrix::AudioSrcDefault)
        {
            snap.mids = getSourceScalar(orig, routing.mid, orig.mids, onsetMethodIndex);
            snap.triggers[1] = getSourceTrigger(orig, routing.mid, orig.triggers[1], onsetMethodIndex);
        }

        // High slot
        if (routing.high != RGBMatrix::AudioSrcDefault)
        {
            snap.highs = getSourceScalar(orig, routing.high, orig.highs, onsetMethodIndex);
            snap.triggers[2] = getSourceTrigger(orig, routing.high, orig.triggers[2], onsetMethodIndex);
        }

        // Beat slot: snap.beatTrigger + snap.music (phase / bpm gate cosPulse)
        if (routing.beat != RGBMatrix::AudioSrcDefault)
        {
            snap.beatTrigger = getSourceTrigger(orig, routing.beat, orig.beatTrigger, onsetMethodIndex);
            if (routing.beat == RGBMatrix::AudioSrcZero)
            {
                snap.music.beat = false;
                snap.music.beatPhase = 0.0;
                snap.music.barPhase = 0.0;
                snap.music.bpm = 0.0;
                snap.music.beatConfidence = 0.0;
                snap.music.tatum = false;
                snap.downbeatFired = false;
            }
            else if (routing.beat != RGBMatrix::AudioSrcBeat)
            {
                // Non-tempo source: clamp source value [0,1] into beatPhase,
                // disable tempo so cosPulse becomes 0.
                const double v = getSourceScalar(orig, routing.beat, orig.beatTrigger.value, onsetMethodIndex);
                snap.music.beatPhase = std::clamp(v, 0.0, 1.0);
                snap.music.bpm = 0.0;
                snap.music.beatConfidence = 0.0;
                snap.music.tatum = false;
            }
        }

        // Kick slot
        if (routing.kick != RGBMatrix::AudioSrcDefault)
        {
            snap.kickTrigger = getSourceTrigger(orig, routing.kick, orig.kickTrigger, onsetMethodIndex);
        }

        // Onset slot: rewrite all method bools + descriptors so any selected
        // method index reads the same routed value.
        if (routing.onset != RGBMatrix::AudioSrcDefault)
        {
            if (routing.onset == RGBMatrix::AudioSrcZero)
            {
                snap.onsets.energy = false;
                snap.onsets.hfc = false;
                snap.onsets.complex_ = false;
                snap.onsets.phase = false;
                snap.onsets.wphase = false;
                snap.onsets.specdiff = false;
                snap.onsets.kl = false;
                snap.onsets.mkl = false;
                snap.onsets.specflux = false;
                for (int i = 0; i < AUBIO_ONSET_METHODS; i++)
                {
                    snap.onsets.descriptors[i] = 0.0;
                    snap.onsets.thresholdedDescriptors[i] = 0.0;
                }
            }
            else if (routing.onset != RGBMatrix::AudioSrcOnset)
            {
                const TriggerState srcTrig = getSourceTrigger(orig, routing.onset,
                                                              TriggerState{}, onsetMethodIndex);
                const bool fired = srcTrig.firedThisFrame;
                const double v = srcTrig.value;
                snap.onsets.energy = fired;
                snap.onsets.hfc = fired;
                snap.onsets.complex_ = fired;
                snap.onsets.phase = fired;
                snap.onsets.wphase = fired;
                snap.onsets.specdiff = fired;
                snap.onsets.kl = fired;
                snap.onsets.mkl = fired;
                snap.onsets.specflux = fired;
                for (int i = 0; i < AUBIO_ONSET_METHODS; i++)
                {
                    snap.onsets.descriptors[i] = v;
                    snap.onsets.thresholdedDescriptors[i] = v;
                }
            }
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

    // Inject the matrix's HSV color stops as algo.gradientColors and a
    // pre-sampled 3-element algo.gradientBandColors LUT for low/mid/high banks,
    // plus algo.color (primary HSV color). Done on every frame so live UI /
    // MCP color edits show up next tick.
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

    // HSV-only contract: scripts MUST return a Float32Array of length
    // width*height*3 (interleaved H,S,V floats in [0,1]). The engine converts
    // to packed RGB here. Returning a Uint32Array (the legacy RGB path) is
    // no longer supported.
    const int width = size.width();
    const int height = size.height();
    if (width <= 0 || height <= 0)
        return;

    QJSValue bpeProp = yarray.property(QStringLiteral("BYTES_PER_ELEMENT"));
    if (!bpeProp.isNumber())
    {
        qWarning() << "RGBScript" << m_fileName
                   << "rgbMap() did not return a TypedArray. HSV contract"
                   << "requires a flat Float32Array of length width*height*3.";
        return;
    }

    // Reject Uint32Array (legacy RGB) — only Float32Array is accepted.
    // Both have BYTES_PER_ELEMENT==4, so check the constructor name.
    QJSValue ctorName = yarray.property(QStringLiteral("constructor"))
                              .property(QStringLiteral("name"));
    if (ctorName.toString() != QStringLiteral("Float32Array"))
    {
        qWarning() << "RGBScript" << m_fileName
                   << "returned" << ctorName.toString()
                   << "but only Float32Array (HSV) is supported.";
        return;
    }

    const int bpe = bpeProp.toInt();
    const int byteLength = yarray.property(QStringLiteral("byteLength")).toInt();
    const int expectedHsvBytes = width * height * 3 * 4;    // Float32Array: 3 floats/pixel

    if (bpe != 4 || byteLength != expectedHsvBytes)
    {
        qWarning() << "RGBScript" << m_fileName
                   << "returned a TypedArray with unexpected geometry: byteLength="
                   << byteLength << "BYTES_PER_ELEMENT=" << bpe
                   << "(expected" << expectedHsvBytes
                   << "for HSV Float32Array).";
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

namespace
{
    /** Shortest-arc HSV gradient interpolation.
     *  Mirrors RGBUtil.gradientAt() in rgbutil.js: stops are evenly spaced in
     *  [0,1]; hue is interpolated along the shorter arc; s,v are linear. */
    HsvColor interpolateHsv(const QVector<HsvColor> &stops, double t)
    {
        if (stops.isEmpty())
            return {0.0f, 0.0f, 0.0f};
        if (stops.size() == 1)
            return stops.at(0);
        if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
        double pos = t * (stops.size() - 1);
        int idx = int(pos);
        if (idx >= stops.size() - 1)
            return stops.at(stops.size() - 1);
        double frac = pos - idx;
        const HsvColor &a = stops.at(idx);
        const HsvColor &b = stops.at(idx + 1);
        double dh = double(b.h) - double(a.h);
        if (dh > 0.5) dh -= 1.0;
        else if (dh < -0.5) dh += 1.0;
        double h = double(a.h) + frac * dh;
        h = h - std::floor(h);
        return {
            float(h),
            float(double(a.s) + frac * (double(b.s) - double(a.s))),
            float(double(a.v) + frac * (double(b.v) - double(a.v)))
        };
    }

    QJSValue hsvToJs(QJSEngine *engine, const HsvColor &hsv)
    {
        QJSValue obj = engine->newObject();
        obj.setProperty(QStringLiteral("h"), QJSValue(double(hsv.h)));
        obj.setProperty(QStringLiteral("s"), QJSValue(double(hsv.s)));
        obj.setProperty(QStringLiteral("v"), QJSValue(double(hsv.v)));
        return obj;
    }
}

void RGBScript::injectGradientArrays(uint rgb)
{
    QJSEngine *engine = s_jsThread->engine;
    if (engine == NULL || m_script.isUndefined())
        return;

    // Compact valid stops from the owning matrix into HSV. The matrix still
    // stores QColor (RGB) under the hood; we convert here so scripts only
    // ever see HSV.
    QVector<HsvColor> hsvStops;
    RGBMatrix *matrix = owningMatrix(doc(), this);
    if (matrix != NULL)
    {
        QVector<QColor> cols = matrix->getColors();
        for (int i = 0; i < cols.size(); ++i)
        {
            const QColor &c = cols.at(i);
            if (!c.isValid())
                continue;
            hsvStops.append(rgbToHsv(c.rgb() & 0xFFFFFFu));
        }
    }

    // Always have at least one stop so scripts can index without isUndefined()
    // checks. Falls back to the primary `rgb` argument converted to HSV.
    if (hsvStops.isEmpty())
        hsvStops.append(rgbToHsv(rgb & 0xFFFFFFu));

    // algo.gradientColors: array of {h,s,v} stops (HSV-only contract).
    QJSValue gradArr = engine->newArray(quint32(hsvStops.size()));
    for (int i = 0; i < hsvStops.size(); ++i)
        gradArr.setProperty(quint32(i), hsvToJs(engine, hsvStops.at(i)));

    // algo.gradientBandColors: 3 evenly-sampled HSV stops for low/mid/high
    // mel banks. Sampling uses shortest-arc hue interp to match RGBUtil.gradientAt.
    QJSValue bandArr = engine->newArray(3);
    for (int i = 0; i < 3; ++i)
    {
        double t = (hsvStops.size() <= 1) ? 0.0 : double(i) / 2.0;
        HsvColor c = interpolateHsv(hsvStops, t);
        bandArr.setProperty(quint32(i), hsvToJs(engine, c));
    }

    // algo.color: the primary color as {h,s,v}. Replaces the packed `rgb`
    // argument scripts used to unpack manually.
    QJSValue colorObj = hsvToJs(engine, rgbToHsv(rgb & 0xFFFFFFu));

    m_script.setProperty(QStringLiteral("gradientColors"), gradArr);
    m_script.setProperty(QStringLiteral("gradientBandColors"), bandArr);
    m_script.setProperty(QStringLiteral("color"), colorObj);

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

    const int onsetMethodIndex = std::clamp(config.aubio.onsetMethodIndex,
                                            0, AUBIO_ONSET_METHODS - 1);

    // Per-RGBMatrix audio routing: mutate the snapshot copy BEFORE deriving
    // any JS audio object fields. Scripts cannot bypass this routing.
    if (matrix != NULL && !matrix->audioRouting().isAllDefault())
    {
        const AudioSnapshot original = snap;
        applyAudioRouting(snap, original, matrix->audioRouting(), onsetMethodIndex);
    }

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
                          hsvColorArrayToJs(engine, m_currentGradientColors, 0));
    colorsObj.setProperty(QStringLiteral("bands"),
                          hsvColorArrayToJs(engine, m_currentBandColors, kPowerBandCount));
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
