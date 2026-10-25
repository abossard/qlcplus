/*
  Q Light Controller Plus
  scriptrunner.cpp

  Copyright (C) Massimo Callegari

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

#include <QQmlEngine>
#include <QJSEngine>
#include <QJSValue>
#include <QRandomGenerator>
#include <utility>
#if !defined(Q_OS_IOS)
#include <QProcess>
#endif
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QDebug>
#include <memory>

#include "scriptrunner.h"
#include "genericfader.h"
#include "fadechannel.h"
#include "mastertimer.h"
#include "audiocapture.h"
#include "audiosnapshot.h"
#include "universe.h"
#include "scriptv4.h"

// How often (in milliseconds) waitTime() may run a JS garbage collection pass
static const int GC_INTERVAL_MS = 5000;

ScriptRunner::ScriptRunner(Doc *doc, Script *script, const QString &content, QObject *parent)
    : QThread(parent)
    , m_doc(doc)
    , m_script(script)
    , m_content(content)
    , m_running(false)
    , m_engine(NULL)
    , m_stopOnExit(true)
    , m_waitCount(0)
    , m_waitFunctionId(Function::invalidId())
{
}

ScriptRunner::~ScriptRunner()
{
    stop();
}

void ScriptRunner::execute()
{
    QMutexLocker engineLocker(&m_engineMutex);
    if (m_running || isRunning())
        return;

    m_running = true;
    m_audioCursor.reset();

    start();
}

void ScriptRunner::stop()
{
    stop(nullptr);
}

void ScriptRunner::stop(const QList<Universe *> *claimedUniverses)
{
    QSet<int> registeredBands;
    {
        QMutexLocker engineLocker(&m_engineMutex);
        m_running = false;
        if (m_engine != nullptr)
            m_engine->setInterrupted(true);
        registeredBands.swap(m_registeredBands);
    }
    m_waitCondition.wakeAll();

    // Wait for run() to return before tearing anything down. The script may
    // still be executing native code for a little while after the interrupt.
    wait();

    // Take ownership of what has to be released under the lock, then release
    // it without holding the lock
    QSet<quint32> startedFunctions;
    QMap<quint32, QSharedPointer<GenericFader> > faders;
    {
        QMutexLocker locker(&m_mutex);

        // Nothing queued by the stopped script must be dispatched anymore
        m_functionQueue.clear();
        m_fixtureValueQueue.clear();

        startedFunctions.swap(m_startedFunctions);
        faders.swap(m_fadersMap);
    }

    // Stop all functions started by this script
    foreach (quint32 fID, startedFunctions)
    {
        Function *function = m_doc->function(fID);
        if (function == NULL)
            continue;

        function->stop(FunctionParent::master());
    }

    QList<Universe*> universes;
    if (!faders.isEmpty())
        universes = claimedUniverses ? *claimedUniverses : m_doc->inputOutputMap()->claimUniverses();
    QMapIterator<quint32, QSharedPointer<GenericFader> > it(faders);
    while (it.hasNext())
    {
        it.next();

        if (it.key() >= (quint32)universes.size())
            continue;

        Universe *universe = universes.at(it.key());
        QSharedPointer<GenericFader> fader = it.value();
        if (fader.isNull())
            continue;

        foreach (const FadeChannel &fc, fader->channels())
        {
            quint32 address = fc.addressInUniverse();
            if (address == QLCChannel::invalid())
                continue;

            for (int i = 0; i < fc.channelCount() && address + i < UNIVERSE_SIZE; i++)
                universe->write(address + i, 0, true);
        }
    }
    if (!faders.isEmpty() && !claimedUniverses)
        m_doc->inputOutputMap()->releaseUniverses();

    // request to delete all the active faders
    foreach (QSharedPointer<GenericFader> fader, faders)
    {
        if (!fader.isNull())
            fader->requestDelete();
    }
    // Unregister any audio bands we registered without holding the engine lock.
    if (!registeredBands.isEmpty())
    {
        QSharedPointer<AudioCapture> capture = m_doc->audioInputCapture();
        if (!capture.isNull())
        {
            for (int bands : registeredBands)
                capture->unregisterBandsNumber(bands);
        }
    }
}

QStringList ScriptRunner::collectScriptData()
{
    QStringList syntaxErrorList;
    QJSEngine *engine = new QJSEngine();
    QJSValue objectValue = engine->newQObject(this);
    engine->globalObject().setProperty("Engine", objectValue);
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);

    QJSValue script = engine->evaluate("(function run() { " + m_content + " })");
    if (script.isError())
    {
        QString msg = QString("Uncaught exception at line %2. %3")
                        .arg(script.property("lineNumber").toInt())
                        .arg(script.toString());
        qWarning() << msg;
        //qDebug() << "Stack: " << script.property("stack").toString();
        syntaxErrorList << msg;
    }
    else
    {
        qDebug() << "All good.";
    }

    if (script.isCallable() == false)
    {
        qDebug() << "ERROR. No function method found.";
    }
    else
    {
        QJSValue ret = script.call(QJSValueList());
        if (ret.isError())
        {
            QString msg = QString("Uncaught exception at line %2. %3")
                            .arg(ret.property("lineNumber").toInt())
                            .arg(ret.toString());
            qWarning() << msg;
            syntaxErrorList << msg;
        }
    }

    delete engine;

    return syntaxErrorList;
}

int ScriptRunner::currentWaitTime() const
{
    QMutexLocker locker(&m_mutex);
    return m_waitCount * MasterTimer::tick();
}

ScriptRunner::FixtureValueKey ScriptRunner::fixtureValueKey(quint32 universe, quint32 fixtureID, quint32 channel)
{
    return qMakePair(universe, (quint64(fixtureID) << 32) | quint64(channel));
}

bool ScriptRunner::write(MasterTimer *timer, QList<Universe *> universes)
{
    // Take the queued fixture values out under the lock and work on a private
    // copy, so that the lock is not held while calling into the faders
    QHash<FixtureValueKey, FixtureValue> fixtureValues;
    {
        QMutexLocker locker(&m_mutex);
        if (m_waitCount > 0 && --m_waitCount == 0)
            // wake up a script sleeping in waitTime() as soon as its wait is over
            m_waitCondition.wakeAll();

        fixtureValues.swap(m_fixtureValueQueue);
    }

    for (const FixtureValue &val : std::as_const(fixtureValues))
    {

        // The universe index was computed when setFixture() was called and
        // has been sitting in the queue since then. Make sure it is still
        // valid before indexing into the list.
        if (val.m_universe >= quint32(universes.count()))
        {
            qWarning() << QString("Skipping fixture value: invalid universe %1").arg(val.m_universe);
            continue;
        }

        QSharedPointer<GenericFader> fader;
        {
            QMutexLocker locker(&m_mutex);
            fader = m_fadersMap.value(val.m_universe, QSharedPointer<GenericFader>());
        }

        if (fader.isNull())
        {
            fader = universes[val.m_universe]->requestFader();
            if (fader.isNull())
                continue;
            //fader->adjustIntensity(getAttributeValue(Intensity));
            //fader->setBlendMode(blendMode());

            QMutexLocker locker(&m_mutex);
            m_fadersMap[val.m_universe] = fader;
        }

        const uchar targetValue = val.m_value;
        const uint fadeTime = val.m_fadeTime;
        fader->updateChannel(m_doc, universes[val.m_universe], val.m_fixtureID, val.m_channel,
                             [targetValue, fadeTime](FadeChannel &fc)
        {
            fc.setStart(fc.current());
            fc.setTarget(targetValue);
            fc.setFadeTime(fadeTime);
            fc.setElapsed(0);
            fc.setReady(false);
        });
    }

    // Dispatch the queued start/stop/wait requests, unless the script is
    // waiting for a Function
    while (true)
    {
        QPair<quint32, FunctionOperation> pair;
        {
            QMutexLocker locker(&m_mutex);
            if (m_waitFunctionId != Function::invalidId() || m_functionQueue.isEmpty())
                break;
            pair = m_functionQueue.head();
        }

        const quint32 fID = pair.first;
        const FunctionOperation operation = pair.second;
        bool waiting = false;

        Function *function = m_doc->function(fID);
        if (function == NULL)
        {
            qWarning() << QString("No such function (ID %1)").arg(fID);
            // Drop the bad entry. A plain "continue" leaves it at the head of
            // the queue, so this loop would spin forever on the MasterTimer
            // thread and freeze DMX output for every universe.
        }
        else if (operation == FunctionOperation::START || operation == FunctionOperation::START_DONT_STOP)
        {
            function->start(timer, FunctionParent::master());
            if (operation == FunctionOperation::START)
            {
                QMutexLocker locker(&m_mutex);
                m_startedFunctions.insert(fID);
            }
        }
        else if (operation == FunctionOperation::STOP)
        {
            function->stop(FunctionParent::master());
            QMutexLocker locker(&m_mutex);
            m_startedFunctions.remove(fID);
        }
        else if (operation == FunctionOperation::WAIT_START)
        {
            if (!function->isRunning())
            {
                // the function is not running, so we we wait and we stop dequeuing
                QMutexLocker locker(&m_mutex);
                m_waitFunctionId = fID;
                connect(m_doc->masterTimer(), SIGNAL(functionStarted(quint32)), SLOT(slotWaitFunctionStarted(quint32)));
                waiting = true;
            }
        }
        else if (operation == FunctionOperation::WAIT_STOP)
        {
            if (!function->stopped())
            {
                // the function has to start or is still running, so we wait and we stop dequeuing
                QMutexLocker locker(&m_mutex);
                m_waitFunctionId = fID;
                connect(m_doc->masterTimer(), SIGNAL(functionStopped(quint32)), SLOT(slotWaitFunctionStopped(quint32)));
                waiting = true;
            }
        }

        if (waiting)
            break;

        // we can continue with the next function in the queue
        QMutexLocker locker(&m_mutex);
        if (!m_functionQueue.isEmpty())
            m_functionQueue.removeFirst();
    }

    // If the JS call method has ended on its own, the thread has finished.
    // Keep running until every queued operation has been dispatched, otherwise
    // commands issued right before the script fell off the end would be lost
    QMutexLocker locker(&m_mutex);
    if (m_running == false && m_functionQueue.isEmpty() && m_fixtureValueQueue.isEmpty() &&
        m_waitFunctionId == Function::invalidId())
        return false;

    return true;
}

void ScriptRunner::slotWaitFunctionStarted(quint32 fid)
{
    QMutexLocker locker(&m_mutex);
    if (m_waitFunctionId == fid)
    {
        disconnect(m_doc->masterTimer(), SIGNAL(functionStarted(quint32)), this, SLOT(slotWaitFunctionStarted(quint32)));
        m_waitFunctionId = Function::invalidId();
    }
}

void ScriptRunner::slotWaitFunctionStopped(quint32 fid)
{
    QMutexLocker locker(&m_mutex);
    if (m_waitFunctionId == fid)
    {
        disconnect(m_doc->masterTimer(), SIGNAL(functionStopped(quint32)), this, SLOT(slotWaitFunctionStopped(quint32)));
        m_startedFunctions.remove(fid);
        m_waitFunctionId = Function::invalidId();
    }
}

void ScriptRunner::run()
{
    {
        QMutexLocker locker(&m_mutex);
        m_waitCount = 0;
    }

    auto engine = std::make_unique<QJSEngine>();
    {
        QMutexLocker engineLocker(&m_engineMutex);
        if (!m_running)
            return;
        m_engine = engine.get();
    }
    m_gcTimer.start();
    QJSValue objectValue = engine->newQObject(this);
    engine->globalObject().setProperty("Engine", objectValue);
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);

    QJSValue script = engine->evaluate("(function run() { " + m_content + " })");

    if (script.isCallable() == false)
    {
        qDebug() << "ERROR. No function method found.";
    }
    else
    {
        QJSValue ret = script.call(QJSValueList());
        if (ret.isError())
        {
            QString msg("Uncaught exception at line %2. Error: %3");
            qWarning() << msg.arg(ret.property("lineNumber").toInt())
                             .arg(ret.toString());
        }

        qDebug() << "[ScriptRunner] Code executed";
    }

    // Signal write() that the JS code is done. Note that this must not stop
    // the Script right away: pending queued operations are dispatched by
    // write() on the MasterTimer thread and have to be flushed first
    {
        QMutexLocker engineLocker(&m_engineMutex);
        m_engine = nullptr;
    }
    // Unpublish before deleting on the runner thread, so stop() cannot
    // interrupt an engine that is being destroyed.
    engine.reset();
    m_running = false;
}

void ScriptRunner::maybeCollectGarbage()
{
    // Only run() modifies the engine pointer while this thread is running.
    if (m_engine == NULL || m_gcTimer.elapsed() < GC_INTERVAL_MS)
        return;

    m_engine->collectGarbage();
    m_gcTimer.restart();
}

/************************************************************************
 * JS exported methods
 ************************************************************************/

int ScriptRunner::getChannelValue(int universe, int channel)
{
    if (m_running == false)
        return false;

    QList<Universe*> uniList;
    if (!m_doc->inputOutputMap()->claimUniversesWhileRunning(m_running, uniList))
        return 0;
    uchar dmxValue = 0;

    if (universe >= 0 && universe < uniList.count())
    {
        Universe *uni = uniList.at(universe);
        dmxValue = uni->preGMValue(channel);
    }
    m_doc->inputOutputMap()->releaseUniverses(false);

    return dmxValue;
}

bool ScriptRunner::setFixture(quint32 fxID, quint32 channel, uchar value, uint time)
{
    if (m_running == false)
        return false;

    // qDebug() << Q_FUNC_INFO;

    Fixture *fxi = m_doc->fixture(fxID);
    if (fxi == NULL)
    {
        qWarning() << QString("No such fixture (ID: %1)").arg(fxID);
        return false;
    }

    if (channel >= fxi->channels())
    {
        qWarning() << QString("Fixture (%1) has no channel number %2").arg(fxi->name()).arg(channel);
        return false;
    }

    int address = fxi->address() + channel;
    if (fxi->crossUniverse() == Fixture::CrossUniverseNone && address >= 512)
    {
        qWarning() << QString("Invalid address: %1").arg(address);
        return false;
    }

    // enqueue this fixture value to be processed at the next write call.
    // A cross-universe fixture channel may belong to a following universe
    FixtureValue val;
    val.m_universe = fxi->channelAddress(channel) >> 9;
    val.m_fixtureID = fxID;
    val.m_channel = channel;
    val.m_value = value;
    val.m_fadeTime = time;

    QMutexLocker locker(&m_mutex);
    // Overwrite a pending value for the same channel instead of queueing another one
    m_fixtureValueQueue.insert(fixtureValueKey(val.m_universe, fxID, channel), val);

    return true;
}

bool ScriptRunner::stopOnExit(bool value)
{
    m_stopOnExit = value;

    return true;
}

Function* ScriptRunner::getFunctionIfRunning(quint32 fID) const
{
    if (m_running == false)
        return NULL;

    Function *function = m_doc->function(fID);
    if (function == NULL)
    {
        qWarning() << QString("No such function (ID %1)").arg(fID);
        return NULL;
    }

    return function;
}

bool ScriptRunner::enqueueFunction(quint32 fID, FunctionOperation operation)
{
    Function *function = getFunctionIfRunning(fID);
    if (function == NULL)
        return false;

    QPair<quint32, FunctionOperation> pair;
    pair.first = fID;
    pair.second = operation;

    QMutexLocker locker(&m_mutex);
    m_functionQueue.enqueue(pair);

    return true;
}

bool ScriptRunner::startFunction(quint32 fID)
{
    return enqueueFunction(fID, m_stopOnExit ? FunctionOperation::START : FunctionOperation::START_DONT_STOP);
}

bool ScriptRunner::stopFunction(quint32 fID)
{
    return enqueueFunction(fID, FunctionOperation::STOP);
}

bool ScriptRunner::isFunctionRunning(quint32 fID)
{
    Function *function = getFunctionIfRunning(fID);
    return function == NULL ? false : function->isRunning();
}

float ScriptRunner::getFunctionAttribute(quint32 fID, int attributeIndex) const
{
    Function *function = getFunctionIfRunning(fID);
    return function == NULL ? 0 : function->getAttributeValue(attributeIndex);
}

bool ScriptRunner::setFunctionAttribute(quint32 fID, int attributeIndex, float value)
{
    Function *function = getFunctionIfRunning(fID);
    if (function == NULL)
        return false;

    function->adjustAttribute(value, attributeIndex);

    return true;
}

bool ScriptRunner::setFunctionAttribute(quint32 fID, QString attributeName, float value)
{
    Function *function = getFunctionIfRunning(fID);
    if (function == NULL)
        return false;

    int attrIndex = function->getAttributeIndex(attributeName);
    function->adjustAttribute(value, attrIndex);

    return true;
}

bool ScriptRunner::systemCommand(QString command)
{
    if (m_running == false)
        return false;

    // qDebug() << Q_FUNC_INFO;

    // tokenize the command by splitting the base
    // program name and the arguments
    QStringList tokens = command.split(" ");
    if (tokens.count() == 0)
        return false;

    QString programName = tokens.first();
    QString multiPartArg;
    QStringList programArgs;
    for (int i = 1; i < tokens.size(); i++)
    {
        QString token = tokens.at(i);
        if (token.startsWith("'") && token.endsWith("'"))
        {
            programArgs << token.mid(1, token.length() - 2);
        }
        else if (token.startsWith("'"))
        {
            multiPartArg.clear();
            multiPartArg.append(token.mid(1));
        }
        else
        {
            if (multiPartArg.isEmpty())
                programArgs << token;
            else
            {
                multiPartArg.append(" ");
                if (token.endsWith("'"))
                {
                    multiPartArg.append(token.mid(0, token.length() - 1));
                    programArgs << multiPartArg;
                    multiPartArg.clear();
                }
                else
                {
                    multiPartArg.append(token);
                }
            }
        }
    }

#if !defined(Q_OS_IOS)
    qint64 pid;
    // Use the static overload: the QProcess allocated here was never deleted
    QProcess::startDetached(programName, programArgs, QString(), &pid);
#endif

    return true;
}

bool ScriptRunner::waitTime(uint ms)
{
    QMutexLocker locker(&m_mutex);

    // This has to happen before the m_running check: Script::totalDuration()
    // runs the script through collectScriptData() with m_running == false
    // and sums up the waits via currentWaitTime().
    m_waitCount += ms / MasterTimer::tick();

    if (m_running == false)
        return false;

    // qDebug() << Q_FUNC_INFO;

    // Sleep until write() has counted m_waitCount down to zero. The timeout
    // makes sure a stop request is noticed even if no wake-up arrives.
    while (m_running && m_waitCount > 0)
    {
        m_waitCondition.wait(&m_mutex, 10);

        // A GC pass can take a while. Never run it with the mutex held: write()
        // needs it on the MasterTimer thread every tick and would stall DMX output.
        locker.unlock();
        maybeCollectGarbage();
        locker.relock();
    }

    return true;
}

bool ScriptRunner::waitTime(QString time)
{
    return waitTime(Function::stringToSpeed(time));
}

bool ScriptRunner::waitFunctionStart(quint32 fID)
{
    return enqueueFunction(fID, FunctionOperation::WAIT_START);
}

bool ScriptRunner::waitFunctionStop(quint32 fID)
{
    return enqueueFunction(fID, FunctionOperation::WAIT_STOP);
}

bool ScriptRunner::setBlackout(bool enable)
{
    if (m_running == false)
        return false;

    // qDebug() << Q_FUNC_INFO;

    m_doc->inputOutputMap()->requestBlackout(enable ? InputOutputMap::BlackoutRequestOn :
                                                      InputOutputMap::BlackoutRequestOff);

    return true;
}

bool ScriptRunner::setBPM(int bpm)
{
    if (m_running == false)
        return false;

    // qDebug() << Q_FUNC_INFO;

    m_doc->inputOutputMap()->setBpmNumber(bpm);

    return true;
}

int ScriptRunner::random(QString minTime, QString maxTime)
{
    if (m_running == false)
        return 0;

    int min = Function::stringToSpeed(minTime);
    int max = Function::stringToSpeed(maxTime);

    return random(min, max);
}

int ScriptRunner::random(int minTime, int maxTime)
{
    if (m_running == false)
        return 0;

    // "(maxTime + 1) - minTime" overflows a 32 bit int when maxTime is INT_MAX,
    // and mixing it with the unsigned result of generate() gives a garbage
    // modulus for inverted (max < min) ranges. Do the math in 64 bit instead.
    if (maxTime < minTime)
        std::swap(minTime, maxTime);

    const qint64 range = qint64(maxTime) - qint64(minTime) + 1;
    const quint64 offset = QRandomGenerator::global()->generate64() % quint64(range);

    return int(qint64(minTime) + qint64(offset));
}

int ScriptRunner::getBPM()
{
    if (m_running == false)
        return 0;

    return m_doc->inputOutputMap()->bpmNumber();
}

int ScriptRunner::getBeatDuration()
{
    if (m_running == false)
        return 0;

    return m_doc->masterTimer()->beatTimeDuration();
}

bool ScriptRunner::isBeat()
{
    if (m_running == false)
        return false;

    return m_doc->masterTimer()->isBeat();
}

int ScriptRunner::getAudioLevel()
{
    if (m_running == false)
        return 0;

    return qBound(0, int(resolveAudio().rawRms * 255), 255);
}

int ScriptRunner::getAudioFrequency(int bandIndex, int numBands)
{
    if (m_running == false)
        return 0;

    if (numBands <= 0 || bandIndex < 0 || bandIndex >= numBands)
        return 0;

    return int(audioFrequency(resolveAudio(), bandIndex, numBands) * 255);
}

AudioRenderView ScriptRunner::resolveAudio()
{
    QMutexLocker engineLocker(&m_engineMutex);
    if (m_running && m_registeredBands.isEmpty())
    {
        const auto capture = m_doc->audioInputCapture();
        if (!capture.isNull())
        {
            capture->registerBandsNumber(1);
            m_registeredBands.insert(1);
        }
    }
    return AudioRenderView::fromSnapshot(
        m_running ? m_doc->audioSnapshot() : AudioSnapshot(), AudioRenderView::nowNs());
}

QVariantMap ScriptRunner::getAudioSnapshot()
{
    auto view = resolveAudio();
    m_audioCursor.advance(view);
    return audioViewToVariant(view);
}

int ScriptRunner::getOwnID()
{
    if (m_script == nullptr)
        return -1;

    return (int)m_script->id();
}

int ScriptRunner::getElapsed()
{
    if (m_script == nullptr)
        return 0;

    return (int)m_script->elapsed();
}

int ScriptRunner::getEnvelopeDuration()
{
    if (m_script == nullptr)
        return 0;

    uint dur = m_script->overrideDuration();
    return (dur == Function::defaultSpeed()) ? 0 : (int)dur;
}

int ScriptRunner::getEnvelopeFadeIn()
{
    if (m_script == nullptr)
        return 0;

    uint fi = m_script->overrideFadeInSpeed();
    return (fi == Function::defaultSpeed()) ? 0 : (int)fi;
}

int ScriptRunner::getEnvelopeFadeOut()
{
    if (m_script == nullptr)
        return 0;

    uint fo = m_script->overrideFadeOutSpeed();
    return (fo == Function::defaultSpeed()) ? 0 : (int)fo;
}
