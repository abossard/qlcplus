#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSEngine>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSettings>
#include <QSet>
#include <QtEndian>

#include "audioconsumers_test.h"
#include "audioanalyzer.h"
#include "audiochannel.h"
#include "audiocapture.h"
#include "audioframe.h"
#include "audioview.h"
#define private public
#include "doc.h"
#undef private
#include "huecolor.h"
#include "huematrix.h"
#include "huescript.h"
#include "huescriptscache.h"
#include "rgbscriptscache.h"
#include "rgbaudio.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstring>
#include <memory>
#include <limits>
#include <chrono>
#include <thread>
#include <vector>

#define private public
#include "scriptrunner.h"
#undef private

#ifdef Q_OS_MACOS
#include <mach/mach.h>
#include <malloc/malloc.h>
#endif

namespace
{
// Native scripts may subscribe while a test supplies snapshots directly.
// Keep those lifecycle calls real without opening shared physical input.
class ConsumerTestCapture final : public AudioCapture
{
public:
    ~ConsumerTestCapture() override { stop(); }
    void run() override {}
    void setVolume(qreal) override {}
protected:
    bool initialize() override { return false; }
    void uninitialize() override {}
    void suspend() override {}
    void resume() override {}
    qint64 latency() const override { return -1; }
    bool readAudio(int) override { return false; }
};

QDir scriptsDirectory()
{
    return QDir(QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("../../../resources/huescripts"));
}

QDir rgbScriptsDirectory()
{
    return QDir(QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("../../../resources/rgbscripts"));
}

void failOnUnexpectedHueScriptWarnings()
{
    QTest::failOnWarning(
        QRegularExpression(QStringLiteral(".*values cannot be applied before the 'type' property.*")));
    QTest::failOnWarning(
        QRegularExpression(QStringLiteral(".*Variable\\s+\"[^\"]+\"\\s+is\\s+used\\s+before\\s+its\\s+declaration.*")));
}

AudioSnapshot distinctSnapshot()
{
    AudioSnapshot s;
    s.sourceId = "test";
    s.profileId = 7;
    s.sourceEpoch = 2;
    s.configRevision = 9;
    s.available = true;
    s.status = "available";
    s.volume.raw = 0.23;
    s.volume.volumeNorm = 0.71;
    s.beatPower = 0.13;
    s.bassPower = 0.31;
    s.lows = 0.22;
    s.mids = 0.47;
    s.highs = 0.83;
    s.powersRaw[0] = 0.17;
    s.powersRaw[1] = 0.53;
    s.powersRaw[2] = 0.41;
    s.powersRaw[3] = 0.29;
    s.pitch.hz = 220.0;
    s.pitch.unit = "Midi";
    s.pitch.value = 57.0;
    s.pitch.confidence = 0.64;
    s.music.valid = true;
    s.music.bpm = 110;
    s.music.beatPhase = 0.2;
    s.music.beatInBar = 2;
    s.music.barPhase = 0.55;
    s.melLow.count = s.melMid.count = s.melHigh.count = 5;
    const double values[] = {-0.2, 0.1, 0.4, 0.8, 1.7};
    AudioSnapshot::MelBankSnapshot *banks[] = {&s.melLow, &s.melMid, &s.melHigh};
    for (int bank = 0; bank < 3; ++bank)
    {
        banks[bank]->minHz = 20;
        banks[bank]->maxHz = (bank + 1) * 1000;
        for (int i = 0; i < 5; ++i)
        {
            banks[bank]->centersHz[i] = 60 + 120 * (bank + 1) * i;
            banks[bank]->processed[i] = values[i] / (3 - bank);
            banks[bank]->novelty[i] = values[i] / (6 - bank);
        }
    }
    return s;
}

AudioFrame pcmFrame(const float *samples, uint64_t sequence)
{
    AudioFrame frame;
    frame.samples = samples;
    frame.sampleRate = 30000;
    frame.sampleCount = 500;
    frame.frameIndex = sequence;
    frame.sourceEpoch = 1;
    frame.sampleTime = (sequence - 1) * 500;
    double square = 0, peak = 0;
    for (int i = 0; i < 500; ++i)
    {
        square += double(samples[i]) * samples[i];
        peak = std::max(peak, std::abs(double(samples[i])));
    }
    frame.rms = std::sqrt(square / 500);
    frame.rmsDb = frame.rms > 0 ? 20 * std::log10(frame.rms) : -100;
    frame.peakDb = peak > 0 ? 20 * std::log10(peak) : -100;
    frame.volumeNorm = std::clamp(1 + frame.rmsDb / 100, 0.0, 1.0);
    frame.silent = frame.rms == 0;
    return frame;
}

QJsonArray pixels(const RGBMap &map, double brightness = 1)
{
    QJsonArray result;
    for (const auto &row : map)
        for (uint rgb : row)
            result.append(QJsonArray{qRed(rgb) / 255.0 * brightness,
                                     qGreen(rgb) / 255.0 * brightness,
                                     qBlue(rgb) / 255.0 * brightness});
    return result;
}

QByteArray packedPixels(const RGBMap &map)
{
    int pixelCount = 0;
    for (const auto &row : map)
        pixelCount += row.size();
    QByteArray packed;
    packed.reserve(pixelCount * 3);
    for (const auto &row : map)
    {
        for (uint pixel : row)
        {
            packed.append(char(qRed(pixel)));
            packed.append(char(qGreen(pixel)));
            packed.append(char(qBlue(pixel)));
        }
    }
    return packed;
}

const QStringList effectNames{"spectrum", "energy", "scroll", "wavelength", "strobe"};
const QStringList effectScripts{"audiospectrum.js", "audioenergy.js", "audiobarcode.js",
                                 "audioenergy2.js", "audiostrobe.js"};
const std::array<QSize, 4> layouts{{{24, 1}, {37, 1}, {1, 9}, {7, 11}}};

bool powerEchoMatches(const AudioSnapshot &snapshot, const QJsonArray &recorded)
{
    // Native aubio/vDSP reductions use Float32. Independent allocations can
    // change their rounding; this echo guard is still >1000x tighter than parity.
    constexpr double tolerance = 8 * std::numeric_limits<float>::epsilon();
    const double powers[] = {snapshot.beatPower, snapshot.bassPower, snapshot.mids, snapshot.highs};
    if (recorded.size() != 4)
        return false;
    for (int i = 0; i < 4; ++i)
        if (!recorded[i].isDouble() || !std::isfinite(powers[i])
            || !std::isfinite(recorded[i].toDouble())
            || std::abs(powers[i] - recorded[i].toDouble()) > tolerance)
            return false;
    return true;
}

int maxChannelDelta(const RGBMap &a, const RGBMap &b)
{
    if (a.size() != b.size())
        return INT_MAX;
    int maxDiff = 0;
    for (int y = 0; y < a.size(); ++y)
    {
        if (a[y].size() != b[y].size())
            return INT_MAX;
        for (int x = 0; x < a[y].size(); ++x)
        {
            const uint lhs = a[y][x];
            const uint rhs = b[y][x];
            maxDiff = std::max(maxDiff, std::abs(qRed(lhs) - qRed(rhs)));
            maxDiff = std::max(maxDiff, std::abs(qGreen(lhs) - qGreen(rhs)));
            maxDiff = std::max(maxDiff, std::abs(qBlue(lhs) - qBlue(rhs)));
        }
    }
    return maxDiff;
}

bool hasWhitePeak(const RGBMap &map)
{
    for (const auto &row : map)
    {
        for (uint pixel : row)
        {
            if (qRed(pixel) >= 250 && qGreen(pixel) >= 250 && qBlue(pixel) >= 250)
                return true;
        }
    }
    return false;
}

bool hasDarkPixel(const RGBMap &map)
{
    for (const auto &row : map)
    {
        for (uint pixel : row)
        {
            if (qRed(pixel) == 0 && qGreen(pixel) == 0 && qBlue(pixel) == 0)
                return true;
        }
    }
    return false;
}

int litPixels(const RGBMap &map)
{
    int lit = 0;
    for (const auto &row : map)
    {
        for (uint pixel : row)
        {
            if ((pixel & 0xffffff) != 0)
                ++lit;
        }
    }
    return lit;
}

int brightestRow(const RGBMap &map)
{
    int best = -1;
    int bestLevel = -1;
    for (int y = 0; y < map.size(); ++y)
    {
        for (uint pixel : map[y])
        {
            const int level = qRed(pixel) + qGreen(pixel) + qBlue(pixel);
            if (level > bestLevel)
            {
                bestLevel = level;
                best = y;
            }
        }
    }
    return bestLevel > 0 ? best : -1;
}

struct NativeSweepCase
{
    QString caseId;
    QString algorithmName;
    QString propertyName;
    QString propertyValue;
    QSize layout;
    bool usesAudio;
    bool branchCase;
};

QString slugToken(const QString &value)
{
    QString token = value.toLower();
    token.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    token.remove(QRegularExpression(QStringLiteral("(^-+|-+$)")));
    return token.isEmpty() ? QStringLiteral("default") : token;
}

void applyUnevenBank(AudioSnapshot::MelBankSnapshot &bank, double scale, int frameSeed)
{
    bank.count = 11;
    bank.minHz = 20;
    bank.maxHz = 12000;
    for (int i = 0; i < bank.count; ++i)
    {
        const double ramp = double(i + 1) / double(bank.count + 1);
        const double curve = std::fmod(0.11 * (i + 2 + frameSeed), 0.95);
        bank.centersHz[i] = 35 + i * 180;
        bank.processed[i] = std::clamp(scale * (0.18 + ramp + curve), 0.0, 1.75);
        bank.novelty[i] = std::clamp(scale * (0.09 + curve * 0.7 + ramp * 0.4), 0.0, 1.75);
    }
}

AudioSnapshot sweepSnapshotForFrame(int frame, int64_t baseNs)
{
    AudioSnapshot s = distinctSnapshot();
    s.sourceId = "native-execution-contract";
    s.profileId = 12;
    s.sourceEpoch = 4;
    s.configRevision = 19;
    s.available = true;
    s.status = "available";
    s.publishTimeNs = baseNs + frame * 20000000LL;
    s.volume.raw = 0.31 + 0.04 * frame;
    s.volume.volumeNorm = 0.57 + 0.07 * frame;
    s.beatPower = 0.19 + 0.06 * frame;
    s.bassPower = 0.28 + 0.05 * frame;
    s.lows = 0.23 + 0.03 * frame;
    s.mids = 0.51 + 0.05 * frame;
    s.highs = 0.77 - 0.04 * frame;
    s.powersRaw[0] = 0.12 + frame * 0.19;
    s.powersRaw[1] = 0.41 + frame * 0.11;
    s.powersRaw[2] = 0.33 + frame * 0.09;
    s.powersRaw[3] = 0.22 + frame * 0.07;
    s.pitch.hz = 220.0 + frame * 55.0;
    s.pitch.unit = "midi";
    s.pitch.value = 57.0 + frame * 3.0;
    s.pitch.confidence = 0.83 - frame * 0.08;
    s.music.valid = true;
    s.music.bpm = 126;
    s.music.beatPhase = std::fmod(0.19 * (frame + 1), 1.0);
    s.music.beatInBar = (frame + 1) % 4;
    s.music.barPhase = std::fmod(0.31 * (frame + 1), 1.0);
    s.events.onset = 8 + frame * 2;
    s.events.beat = 16 + frame;
    s.events.kick = 5 + frame;
    s.events.bar = 2 + frame / 2;
    applyUnevenBank(s.melLow, 0.77, frame);
    applyUnevenBank(s.melMid, 0.91, frame + 1);
    applyUnevenBank(s.melHigh, 1.0, frame + 2);
    return s;
}

bool ensureNativeSweepCachesLoaded(Doc *doc)
{
    static bool loaded = false;
    if (loaded)
        return true;
    if (!doc->rgbScriptsCache()->load(rgbScriptsDirectory()))
        return false;
    if (!doc->hueScriptsCache()->load(scriptsDirectory(), true))
        return false;
    if (!doc->hueScriptsCache()->load(rgbScriptsDirectory(), false))
        return false;
    loaded = true;
    return true;
}

Doc *nativeSweepDoc()
{
    static std::unique_ptr<Doc> doc;
    if (!doc)
    {
        doc = std::make_unique<Doc>(nullptr);
        doc->m_inputCapture.reset(new ConsumerTestCapture());
    }
    return doc.get();
}

const QVector<NativeSweepCase> &nativeSweepCases()
{
    static QVector<NativeSweepCase> rows;
    if (!rows.isEmpty())
        return rows;

    Doc *doc = nativeSweepDoc();
    if (!ensureNativeSweepCachesLoaded(doc))
        return rows;

    const QStringList names = HUEMatrix::availableAlgorithms(doc);
    QSet<QString> seenRows;
    int branchOrdinal = 0;
    auto addCase = [&](const QString &algorithmName, const QString &propertyName,
                       const QString &propertyValue, const QSize &layout,
                       bool usesAudio, bool branchCase) {
        const QString label = propertyName.isEmpty()
            ? QString("default-%1").arg(slugToken(algorithmName))
            : QString("%1-%2-%3").arg(slugToken(algorithmName),
                                      slugToken(propertyName),
                                      slugToken(propertyValue));
        if (seenRows.contains(label))
            return;
        seenRows.insert(label);
        rows.push_back({label, algorithmName, propertyName, propertyValue, layout, usesAudio, branchCase});
    };

    for (const QString &name : names)
    {
        std::unique_ptr<RGBAlgorithm> algorithm(HUEMatrix::createAlgorithm(doc, name));
        if (!algorithm)
            continue;
        addCase(name, QString(), QString(), QSize(7, 11), algorithm->usesAudio(), false);
        auto *script = dynamic_cast<RGBScript *>(algorithm.get());
        if (!script)
            continue;
        const QList<RGBScriptProperty> properties = script->properties();
        for (const RGBScriptProperty &property : properties)
        {
            if (property.m_type != RGBScriptProperty::List || property.m_listValues.isEmpty())
                continue;
            for (const QString &choice : property.m_listValues)
            {
                if (choice.isEmpty())
                    continue;
                const QSize layout = ((branchOrdinal++ % 2) == 0) ? QSize(1, 24) : QSize(7, 11);
                addCase(name, property.m_name, choice, layout, algorithm->usesAudio(), true);
            }
        }
    }
    return rows;
}

}

void AudioConsumers_Test::initTestCase()
{
    const QString path = QDir::current().absoluteFilePath("build/low-latency-evidence/settings-consumers");
    QVERIFY(QDir().mkpath(path));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, path);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, path);
}

void AudioConsumers_Test::mapping()
{
    auto s = distinctSnapshot();
    auto v = AudioRenderView::fromSnapshot(s, 1000000000);
    const auto mapped = audioViewToVariant(v);
    QCOMPARE(mapped["version"].toInt(), 6);
    QCOMPARE(mapped["low"].toDouble(), 0.22);
    QCOMPARE(mapped["volume"].toMap()["rawRms"].toDouble(), 0.23);
    QCOMPARE(mapped["barPhase"].toDouble(), 0.55);
    const auto raw = mapped["powers"].toMap()["raw"].toMap();
    QCOMPARE(raw["beat"].toDouble(), 0.17);
    QCOMPARE(raw["bass"].toDouble(), 0.53);
    QCOMPARE(raw["low"].toDouble(), 0.35);
    QCOMPARE(raw["mid"].toDouble(), 0.41);
    QCOMPARE(raw["high"].toDouble(), 0.29);
    const auto pitch = mapped["pitch"].toMap();
    QVERIFY(pitch["valid"].toBool());
    QCOMPARE(pitch["hz"].toDouble(), 220.0);
    QVERIFY(std::abs(pitch["midi"].toDouble() - 57.0) < 1e-9);
    QCOMPARE(pitch["confidence"].toDouble(), 0.64);
    const double canonicalHz = 329.6275569128699;
    s.pitch.hz = canonicalHz;
    s.pitch.unit = "midi";
    s.pitch.value = 64.0;
    const auto midiDisplay = audioViewToVariant(AudioRenderView::fromSnapshot(s, 1000000002))["pitch"].toMap();
    s.pitch.unit = "cent";
    s.pitch.value = 6400.0;
    const auto centDisplay = audioViewToVariant(AudioRenderView::fromSnapshot(s, 1000000003))["pitch"].toMap();
    QVERIFY(midiDisplay["valid"].toBool());
    QVERIFY(centDisplay["valid"].toBool());
    QVERIFY(std::abs(midiDisplay["hz"].toDouble() - canonicalHz) < 1e-9);
    QVERIFY(std::abs(centDisplay["hz"].toDouble() - canonicalHz) < 1e-9);
    QVERIFY(std::abs(midiDisplay["midi"].toDouble() - 64.0) < 1e-12);
    QVERIFY(std::abs(centDisplay["midi"].toDouble() - 64.0) < 1e-12);
    QVERIFY(std::abs(midiDisplay["hz"].toDouble() - centDisplay["hz"].toDouble()) < 1e-12);
    QVERIFY(std::abs(midiDisplay["midi"].toDouble() - centDisplay["midi"].toDouble()) < 1e-12);
    s.pitch.hz = 0;
    const auto zeroPitch = audioViewToVariant(AudioRenderView::fromSnapshot(s, 1000000004))["pitch"].toMap();
    QVERIFY(!zeroPitch["valid"].toBool());
    QCOMPARE(zeroPitch["hz"].toDouble(), 0.0);
    QCOMPARE(zeroPitch["midi"].toDouble(), 0.0);
    QCOMPARE(zeroPitch["confidence"].toDouble(), 0.0);
    s.pitch.hz = std::numeric_limits<double>::infinity();
    const auto infPitch = audioViewToVariant(AudioRenderView::fromSnapshot(s, 1000000005))["pitch"].toMap();
    QVERIFY(!infPitch["valid"].toBool());
    QCOMPARE(infPitch["hz"].toDouble(), 0.0);
    s.pitch.hz = std::numeric_limits<double>::quiet_NaN();
    const auto nanPitch = audioViewToVariant(AudioRenderView::fromSnapshot(s, 1000000006))["pitch"].toMap();
    QVERIFY(!nanPitch["valid"].toBool());
    QCOMPARE(nanPitch["midi"].toDouble(), 0.0);
    const auto full = mapped["banks"].toMap()["full"].toMap();
    QCOMPARE(full["count"].toInt(), 5);
    QCOMPARE(full["processed"].toList()[4].toDouble(), 1.7);
    QCOMPARE(full["processed"].toList()[0].toDouble(), -0.2);
    QCOMPARE(audioFrequency(v, 0, 3), 0.22);
    QCOMPARE(audioFrequency(v, 1, 3), 0.47);
    QCOMPARE(audioFrequency(v, 2, 3), 0.83);
    QCOMPARE(audioFrequency(v, 1, 9), 0.0);
    QVERIFY(std::abs(audioFrequency(v, 5, 9) - 0.6) < 1e-12);
    QCOMPARE(audioFrequency(v, 8, 9), 1.0);
    QCOMPARE(audioFrequency(v, -1, 3), 0.0);
    QCOMPARE(audioFrequency(v, 3, 3), 0.0);
    QCOMPARE(audioFrequency(v, 0, 0), 0.0);
    s.publishTimeNs = 1000000000;
    auto stale = AudioRenderView::fromSnapshot(s, 1251000000);
    QVERIFY(!stale.available);
    QCOMPARE(stale.status, QString("stale"));
    QCOMPARE(stale.low, 0.0);
    QCOMPARE(stale.rawLow, 0.0);
    QVERIFY(!stale.pitchValid);
    QCOMPARE(audioFrequency(stale, 2, 3), 0.0);
    QCOMPARE(stale.banks[2].processed.size(), 5);
    QCOMPARE(stale.banks[2].processed[4], 0.0);
}

void AudioConsumers_Test::eventCadence_data()
{
    QTest::addColumn<int>("hz");
    QTest::addColumn<bool>("jitter");
    for (int hz : {25, 30, 50, 60})
        for (bool jitter : {false, true})
            QTest::newRow(qPrintable(QString("%1Hz-%2").arg(hz).arg(jitter ? "stall-jitter" : "regular")))
                << hz << jitter;
}

void AudioConsumers_Test::eventCadence()
{
    QFETCH(int, hz);
    QFETCH(bool, jitter);
    AudioEventCursor first, second;
    auto snap = distinctSnapshot();
    snap.music.valid = false;
    snap.music.bpm = 0;
    std::array<uint64_t, 4> sums{};
    auto initial = AudioRenderView::fromSnapshot(snap, 0);
    first.advance(initial);
    second.advance(initial);
    double time = 0, seconds = 0;
    bool stalled = false;
    for (int n = 0; time < 10; ++n)
    {
        time = std::min(10.0, time + (jitter ? (n % 2 ? 0.73 : 1.27) : 1.0) / hz);
        if (jitter && !stalled && time >= 4)
        {
            time += 0.12;
            stalled = true;
        }
        const auto frame = uint64_t(std::floor(time * 60 + 1e-7));
        snap.frameSequence = frame;
        snap.events.onset = frame / 2;
        snap.events.beat = frame / 7;
        snap.events.kick = frame / 11;
        snap.events.bar = frame / 28;
        auto a = AudioRenderView::fromSnapshot(snap, int64_t(time * 1e9));
        auto b = a;
        first.advance(a);
        second.advance(b);
        for (size_t i = 0; i < sums.size(); ++i)
        {
            QCOMPARE(a.deltas[i], b.deltas[i]);
            sums[i] += a.deltas[i];
        }
        seconds += a.deltaSeconds;
        const auto js = audioViewToVariant(a);
        QCOMPARE(js["bpm"].toDouble(), 120.0);
        QVERIFY(!js["tempo"].toMap()["valid"].toBool());
        QVERIFY(std::abs(js["dt"].toDouble() - a.deltaSeconds * 2) < 1e-12);
        first.advance(a);
        for (auto delta : a.deltas)
            QCOMPARE(delta, uint64_t(0));
        QCOMPARE(a.deltaSeconds, 0.0);
    }
    const std::array<uint64_t, 4> expected{300, 85, 54, 21};
    QCOMPARE(sums, expected);
    QVERIFY(std::abs(seconds - 10) < 1e-8);
    snap.sourceEpoch++;
    auto reset = AudioRenderView::fromSnapshot(snap, 11000000000);
    first.advance(reset);
    for (auto delta : reset.deltas)
        QCOMPARE(delta, uint64_t(0));
    QCOMPARE(reset.deltaSeconds, 0.0);
}

void AudioConsumers_Test::eventCadenceTransientLoss()
{
    AudioEventCursor cursor;
    auto base = distinctSnapshot();
    base.sourceId = "microphone";
    base.profileId = 7;
    base.sourceEpoch = 42;
    base.events = {};

    auto initial = AudioRenderView::fromSnapshot(base, 1000000000);
    cursor.advance(initial);

    auto next = base;
    next.events.beat = 1;
    auto available = AudioRenderView::fromSnapshot(next, 1100000000);
    cursor.advance(available);
    QVERIFY(std::abs(available.deltaSeconds - 0.1) < 1e-12);
    QCOMPARE(available.deltas[1], uint64_t(1));

    auto lost = AudioSnapshot{};
    lost.profileId = 7;
    lost.sourceEpoch = 0;
    lost.available = false;
    lost.status = QStringLiteral("reset");
    auto unavailable1 = AudioRenderView::fromSnapshot(lost, 1200000000);
    cursor.advance(unavailable1);
    QVERIFY(std::abs(unavailable1.deltaSeconds - 0.1) < 1e-12);
    for (auto delta : unavailable1.deltas)
        QCOMPARE(delta, uint64_t(0));

    auto unavailable2 = AudioRenderView::fromSnapshot(lost, 1300000000);
    cursor.advance(unavailable2);
    QVERIFY(std::abs(unavailable2.deltaSeconds - 0.1) < 1e-12);

    auto reboundSameIdentity = base;
    reboundSameIdentity.events.beat = 3;
    auto reboundSuppressed = AudioRenderView::fromSnapshot(reboundSameIdentity, 1400000000);
    cursor.advance(reboundSuppressed);
    // m_available gating policy keeps dt continuous but suppresses replay of
    // all missed event deltas on the first frame after loss.
    QVERIFY(std::abs(reboundSuppressed.deltaSeconds - 0.1) < 1e-12);
    for (auto delta : reboundSuppressed.deltas)
        QCOMPARE(delta, uint64_t(0));

    reboundSameIdentity.events.beat = 4;
    auto reboundDelivered = AudioRenderView::fromSnapshot(reboundSameIdentity, 1500000000);
    cursor.advance(reboundDelivered);
    QVERIFY(std::abs(reboundDelivered.deltaSeconds - 0.1) < 1e-12);
    QCOMPARE(reboundDelivered.deltas[1], uint64_t(1));

    auto reboundEpochChanged = base;
    reboundEpochChanged.sourceEpoch = 43;
    reboundEpochChanged.events.beat = 6;
    auto reboundFirst = AudioRenderView::fromSnapshot(reboundEpochChanged, 1600000000);
    cursor.advance(reboundFirst);
    QCOMPARE(reboundFirst.deltaSeconds, 0.0);
    for (auto delta : reboundFirst.deltas)
        QCOMPARE(delta, uint64_t(0));

    reboundEpochChanged.events.beat = 7;
    auto reboundSecond = AudioRenderView::fromSnapshot(reboundEpochChanged, 1700000000);
    cursor.advance(reboundSecond);
    QVERIFY(std::abs(reboundSecond.deltaSeconds - 0.1) < 1e-12);
    QCOMPARE(reboundSecond.deltas[1], uint64_t(1));

    auto resetDifferentProfile = AudioSnapshot{};
    resetDifferentProfile.profileId = 9;
    resetDifferentProfile.sourceEpoch = 0;
    resetDifferentProfile.available = false;
    resetDifferentProfile.status = QStringLiteral("reset");
    auto profileReset = AudioRenderView::fromSnapshot(resetDifferentProfile, 1800000000);
    cursor.advance(profileReset);
    QCOMPARE(profileReset.deltaSeconds, 0.0);
    for (auto delta : profileReset.deltas)
        QCOMPARE(delta, uint64_t(0));

    auto switched = base;
    switched.profileId = 9;
    switched.sourceEpoch = 5;
    switched.events.beat = 8;
    auto switchedView = AudioRenderView::fromSnapshot(switched, 1900000000);
    cursor.advance(switchedView);
    QCOMPARE(switchedView.deltaSeconds, 0.0);
    for (auto delta : switchedView.deltas)
        QCOMPARE(delta, uint64_t(0));

    switched.events.beat = 9;
    auto switchedSteady = AudioRenderView::fromSnapshot(switched, 2000000000);
    cursor.advance(switchedSteady);
    QVERIFY(std::abs(switchedSteady.deltaSeconds - 0.1) < 1e-12);
    QCOMPARE(switchedSteady.deltas[1], uint64_t(1));
}

void AudioConsumers_Test::packedScriptContract_data()
{
    QTest::addColumn<QString>("caseId");
    QTest::addColumn<int>("hz");
    for (int hz : {25, 30, 50, 60})
        QTest::newRow(qPrintable(QString("meltsparkle-cadence-%1hz").arg(hz)))
            << QString("meltsparkle-cadence") << hz;
    QTest::newRow("meltsparkle-zero-dt-and-rollback") << QString("meltsparkle-zero-dt") << 0;
    QTest::newRow("meltsparkle-stall") << QString("meltsparkle-stall") << 0;
    QTest::newRow("meltsparkle-artistic-threshold") << QString("meltsparkle-artistic-threshold") << 0;
    QTest::newRow("meltsparkle-ledfx-threshold") << QString("meltsparkle-ledfx-threshold") << 0;
    QTest::newRow("meltsparkle-cooldown-and-small-geometry") << QString("meltsparkle-cooldown") << 0;
    QTest::newRow("waterfall-frequency-time") << QString("waterfall-frequency-time") << 0;
    QTest::newRow("waterfall-center-fade") << QString("waterfall-center-fade") << 0;
    QTest::newRow("digitalrain-raw-phase-time") << QString("digitalrain-raw-phase-time") << 0;
    QTest::newRow("pitchspectrum-valid-pitch-and-stale") << QString("pitchspectrum-valid-pitch-and-stale") << 0;
}

void AudioConsumers_Test::packedScriptContract()
{
    QFETCH(QString, caseId);
    QFETCH(int, hz);

    failOnUnexpectedHueScriptWarnings();

    Doc doc(nullptr);
    doc.m_inputCapture.reset(new ConsumerTestCapture());
    QVERIFY(doc.hueScriptsCache()->load(scriptsDirectory(), true));
    const auto scriptPath = [&](const QString &name) { return scriptsDirectory().filePath(name); };
    const auto makeSnapshot = []() {
        AudioSnapshot s = distinctSnapshot();
        s.sourceId = "native-contract";
        s.profileId = 3;
        s.sourceEpoch = 9;
        s.configRevision = 12;
        s.available = true;
        s.status = "available";
        s.music.valid = true;
        s.music.bpm = 120;
        s.music.beatPhase = 0.25;
        s.publishTimeNs = 0;
        return s;
    };
    const auto setFullBank = [](AudioSnapshot &s, const std::array<double, 4> &processed,
                                const std::array<double, 4> &novelty) {
        s.melHigh.count = 4;
        s.melHigh.minHz = 20;
        s.melHigh.maxHz = 12000;
        for (int i = 0; i < 4; ++i)
        {
            s.melHigh.centersHz[i] = 200 + i * 300;
            s.melHigh.processed[i] = processed[size_t(i)];
            s.melHigh.novelty[i] = novelty[size_t(i)];
        }
    };
    const auto rowEnergy = [](const QVector<uint> &row) {
        int level = 0;
        for (uint pixel : row)
            level += qRed(pixel) + qGreen(pixel) + qBlue(pixel);
        return level;
    };

    if (caseId == "meltsparkle-cadence" || caseId == "meltsparkle-stall")
    {
        const auto runDecay = [&](int cadenceHz, bool withStall) -> RGBMap {
            auto script = std::make_unique<HUEScript>(&doc);
            if (!script->load(scriptPath("audiomeltsparkle.js"))
                || !script->setProperty("mode", "Artistic")
                || !script->setProperty("presetBgBright", "0")
                || !script->setProperty("presetStrobeBlur", "0")
                || !script->setProperty("presetStrobeWidth", "1"))
            {
                return RGBMap();
            }
            AudioSnapshot s = makeSnapshot();
            s.lows = 0;
            s.mids = 0;
            s.highs = 1;
            s.events.onset = 0;
            RGBMap map;
            const int64_t startNs = 1000000000LL;
            s.publishTimeNs = startNs;
            script->rgbMapWithAudio({1, 1}, 0xffffff, 0, map, AudioRenderView::fromSnapshot(s, startNs));
            s.events.onset = 1;
            const int64_t eventNs = startNs + int64_t(std::llround((1.0 / cadenceHz) * 1e9));
            s.publishTimeNs = eventNs;
            script->rgbMapWithAudio({1, 1}, 0xffffff, 0, map, AudioRenderView::fromSnapshot(s, eventNs));
            if (withStall)
            {
                for (const int64_t offset : {eventNs + 40000000LL, eventNs + 80000000LL, eventNs + 200000000LL})
                {
                    s.publishTimeNs = offset;
                    script->rgbMapWithAudio({1, 1}, 0xffffff, 0, map,
                        AudioRenderView::fromSnapshot(s, offset));
                }
            }
            else
            {
                const int steps = int(std::round(0.2 * cadenceHz));
                for (int i = 1; i <= steps; ++i)
                {
                    const int64_t nowNs = eventNs + int64_t(std::llround((double(i) / cadenceHz) * 1e9));
                    s.publishTimeNs = nowNs;
                    script->rgbMapWithAudio({1, 1}, 0xffffff, 0, map, AudioRenderView::fromSnapshot(s, nowNs));
                }
            }
            return map;
        };
        const RGBMap observed = caseId == "meltsparkle-stall" ? runDecay(50, true) : runDecay(hz, false);
        QVERIFY(!observed.isEmpty());
        QCOMPARE(observed.size(), 1);
        QCOMPARE(observed[0].size(), 1);
        const uint pixel = observed[0][0];
        const int observedPeak = std::max({qRed(pixel), qGreen(pixel), qBlue(pixel)});
        const double decayMul = 1.0 - 0.25;
        const double elapsedSeconds = 0.2;
        const int expectedPeak = int(std::lround(255.0 * std::pow(decayMul, elapsedSeconds * 50.0)));
        QVERIFY2(std::abs(observedPeak - expectedPeak) <= 1,
            qPrintable(QString("peak=%1 expected=%2 cadence=%3")
                .arg(observedPeak).arg(expectedPeak).arg(caseId == "meltsparkle-stall" ? 50 : hz)));
        return;
    }

    if (caseId == "meltsparkle-zero-dt")
    {
        HUEScript script(&doc);
        QVERIFY(script.load(scriptPath("audiomeltsparkle.js")));
        QVERIFY(script.setProperty("mode", "Artistic"));
        QVERIFY(script.setProperty("presetBgBright", "0"));
        QVERIFY(script.setProperty("presetStrobeBlur", "0"));
        QVERIFY(script.setProperty("presetStrobeWidth", "1"));
        AudioSnapshot s = makeSnapshot();
        RGBMap warmup;
        const int64_t warmupNs = 1990000000LL;
        s.events.onset = 0;
        s.publishTimeNs = warmupNs;
        script.rgbMapWithAudio({1, 1}, 0xffffff, 0, warmup, AudioRenderView::fromSnapshot(s, warmupNs));
        s.highs = 1;
        s.events.onset = 1;
        RGBMap first, repeat, rollback;
        const int64_t t0 = 2000000000LL;
        s.publishTimeNs = t0;
        script.rgbMapWithAudio({1, 1}, 0xffffff, 0, first, AudioRenderView::fromSnapshot(s, t0));
        script.rgbMapWithAudio({1, 1}, 0xffffff, 0, repeat, AudioRenderView::fromSnapshot(s, t0));
        script.rgbMapWithAudio({1, 1}, 0xffffff, 0, rollback, AudioRenderView::fromSnapshot(s, t0 - 1000000));
        QCOMPARE(repeat, first);
        QCOMPARE(rollback, first);
        QVERIFY(hasWhitePeak(first));
        return;
    }

    if (caseId == "meltsparkle-artistic-threshold")
    {
        HUEScript script(&doc);
        QVERIFY(script.load(scriptPath("audiomeltsparkle.js")));
        QVERIFY(script.setProperty("mode", "Artistic"));
        QVERIFY(script.setProperty("presetBgBright", "0"));
        QVERIFY(script.setProperty("presetStrobeBlur", "0"));
        QVERIFY(script.setProperty("presetStrobeWidth", "1"));
        AudioSnapshot s = makeSnapshot();
        RGBMap warmup;
        RGBMap below, above;
        const int64_t t0 = 3000000000LL;
        s.events.onset = 0;
        s.publishTimeNs = t0 - 10000000;
        script.rgbMapWithAudio({37, 1}, 0xffffff, 0, warmup, AudioRenderView::fromSnapshot(s, t0 - 10000000));
        s.highs = 0.70;
        s.events.onset = 1;
        s.publishTimeNs = t0;
        script.rgbMapWithAudio({37, 1}, 0xffffff, 0, below, AudioRenderView::fromSnapshot(s, t0));
        QCOMPARE(litPixels(below), 0);
        s.highs = 0.80;
        s.events.onset = 2;
        s.publishTimeNs = t0 + 200000000;
        script.rgbMapWithAudio({37, 1}, 0xffffff, 0, above, AudioRenderView::fromSnapshot(s, t0 + 200000000));
        QVERIFY(hasWhitePeak(above));
        QVERIFY(hasDarkPixel(above));
        return;
    }

    if (caseId == "meltsparkle-ledfx-threshold")
    {
        HUEScript script(&doc);
        QVERIFY(script.load(scriptPath("audiomeltsparkle.js")));
        QVERIFY(script.setProperty("mode", "Artistic"));
        QVERIFY(script.setProperty("presetBgBright", "0.4"));
        QVERIFY(script.setProperty("presetStrobeBlur", "0"));
        QVERIFY(script.setProperty("presetStrobeWidth", "1"));
        QVERIFY(script.setProperty("presetStrobeThreshold", "1"));
        AudioSnapshot s = makeSnapshot();
        RGBMap warmup;
        RGBMap artisticMap, sourceModeMap;
        const int64_t t0 = 4000000000LL;
        s.lows = 0;
        s.powersRaw[0] = 1;
        s.powersRaw[1] = 1;
        s.highs = 0;
        s.events.onset = 0;
        s.publishTimeNs = t0 - 10000000;
        script.rgbMapWithAudio({37, 1}, 0xffffff, 0, warmup, AudioRenderView::fromSnapshot(s, t0 - 10000000));
        s.publishTimeNs = t0 + 200000000;
        script.rgbMapWithAudio({37, 1}, 0xffffff, 0, artisticMap, AudioRenderView::fromSnapshot(s, t0 + 200000000));
        QVERIFY(script.setProperty("mode", "LedFx Melt and Sparkle"));
        s.sourceEpoch += 1;
        s.publishTimeNs = t0 + 400000000;
        script.rgbMapWithAudio({37, 1}, 0xffffff, 0, warmup, AudioRenderView::fromSnapshot(s, t0 + 400000000));
        s.publishTimeNs = t0 + 600000000;
        script.rgbMapWithAudio({37, 1}, 0xffffff, 0, sourceModeMap,
            AudioRenderView::fromSnapshot(s, t0 + 600000000));
        QVERIFY(litPixels(artisticMap) > 0);
        QVERIFY(litPixels(sourceModeMap) > 0);
        QVERIFY(maxChannelDelta(artisticMap, sourceModeMap) > 0);
        return;
    }

    if (caseId == "meltsparkle-cooldown")
    {
        HUEScript script(&doc);
        QVERIFY(script.load(scriptPath("audiomeltsparkle.js")));
        QVERIFY(script.setProperty("mode", "Artistic"));
        QVERIFY(script.setProperty("presetBgBright", "0"));
        QVERIFY(script.setProperty("presetStrobeBlur", "0"));
        QVERIFY(script.setProperty("presetStrobeWidth", "1"));
        QVERIFY(script.setProperty("presetStrobeDecay", "1"));
        QVERIFY(script.setProperty("presetStrobeThreshold", "0.2"));
        AudioSnapshot s = makeSnapshot();
        RGBMap warmup;
        s.highs = 1;
        RGBMap first, cooldownBlocked, cooldownReleased, tiny;
        const int64_t t0 = 5000000000LL;
        s.events.onset = 0;
        s.publishTimeNs = t0 - 10000000;
        script.rgbMapWithAudio({37, 1}, 0xffffff, 0, warmup, AudioRenderView::fromSnapshot(s, t0 - 10000000));
        s.events.onset = 1;
        s.publishTimeNs = t0;
        script.rgbMapWithAudio({37, 1}, 0xffffff, 0, first, AudioRenderView::fromSnapshot(s, t0));
        QVERIFY(hasWhitePeak(first));
        QVERIFY(hasDarkPixel(first));
        s.events.onset = 2;
        s.publishTimeNs = t0 + 50000000;
        script.rgbMapWithAudio({37, 1}, 0xffffff, 0, cooldownBlocked,
            AudioRenderView::fromSnapshot(s, t0 + 50000000));
        QCOMPARE(litPixels(cooldownBlocked), 0);
        s.events.onset = 3;
        s.publishTimeNs = t0 + 500000000;
        script.rgbMapWithAudio({37, 1}, 0xffffff, 0, cooldownReleased,
            AudioRenderView::fromSnapshot(s, t0 + 500000000));
        QVERIFY(hasWhitePeak(cooldownReleased));
        s.events.onset = 4;
        s.publishTimeNs = t0 + 900000000;
        script.rgbMapWithAudio({1, 1}, 0xffffff, 0, tiny, AudioRenderView::fromSnapshot(s, t0 + 900000000));
        QCOMPARE(tiny.size(), 1);
        QCOMPARE(tiny[0].size(), 1);
        QVERIFY(hasWhitePeak(tiny));
        return;
    }

    if (caseId == "waterfall-frequency-time")
    {
        const auto setFullBankValues = [](AudioSnapshot &s, const std::vector<double> &values) {
            const int count = std::min(int(values.size()), AudioSnapshot::kMelBankBandsMax);
            s.melHigh.count = count;
            s.melHigh.minHz = 20;
            s.melHigh.maxHz = 12000;
            for (int i = 0; i < count; ++i)
            {
                s.melHigh.centersHz[i] = 150 + i * 150;
                s.melHigh.processed[i] = values[size_t(i)];
                s.melHigh.novelty[i] = values[size_t(i)];
            }
        };
        QVERIFY(doc.rgbScriptsCache()->load(rgbScriptsDirectory()));
        QVERIFY(doc.hueScriptsCache()->load(scriptsDirectory(), true));
        QVERIFY(doc.hueScriptsCache()->load(rgbScriptsDirectory(), false));

        HUEMatrix *brightHolder = new HUEMatrix(&doc);
        brightHolder->setName("waterfall-bright");
        brightHolder->setAlgorithm(HUEMatrix::createAlgorithm(&doc, "Audio Waterfall"));
        QVERIFY(brightHolder->algorithm() != nullptr);
        brightHolder->setColor(0, QColor("#ffffff"));
        brightHolder->setColor(1, QColor("#000000"));
        QVERIFY(doc.addFunction(brightHolder));
        HUEScript *brightAtZero = dynamic_cast<HUEScript *>(brightHolder->algorithm());
        QVERIFY(brightAtZero != nullptr);
        QVERIFY(brightAtZero->setProperty("bands", "4"));
        QVERIFY(brightAtZero->setProperty("aggregation", "Mean"));
        QVERIFY(brightAtZero->setProperty("centerMode", "Off"));
        QVERIFY(brightAtZero->setProperty("dropSeconds", "2"));
        QVERIFY(brightAtZero->setProperty("fadeOut", "0"));
        AudioSnapshot s = makeSnapshot();
        setFullBankValues(s, {0, 0, 0, 0});
        RGBMap zeroBright;
        const int64_t t0 = 6000000000LL;
        s.publishTimeNs = t0;
        brightAtZero->rgbMapWithAudio({5, 3}, 0xffffff, 0, zeroBright, AudioRenderView::fromSnapshot(s, t0));
        QCOMPARE(zeroBright.size(), 3);
        QCOMPARE(zeroBright[0].size(), 5);
        for (int x = 0; x < 5; ++x)
            QVERIFY2(qRed(zeroBright[0][x]) >= 240, "G(0) should keep the first bright palette stop");

        HUEMatrix *holder = new HUEMatrix(&doc);
        holder->setName("waterfall-shift");
        holder->setAlgorithm(HUEMatrix::createAlgorithm(&doc, "Audio Waterfall"));
        QVERIFY(holder->algorithm() != nullptr);
        holder->setColor(0, QColor("#000000"));
        holder->setColor(1, QColor("#ff0000"));
        holder->setColor(2, QColor("#00ff00"));
        holder->setColor(3, QColor("#0000ff"));
        holder->setColor(4, QColor("#ffffff"));
        QVERIFY(doc.addFunction(holder));
        HUEScript *script = dynamic_cast<HUEScript *>(holder->algorithm());
        QVERIFY(script != nullptr);
        QVERIFY(script->setProperty("bands", "4"));
        QVERIFY(script->setProperty("aggregation", "Max"));
        QVERIFY(script->setProperty("centerMode", "Off"));
        QVERIFY(script->setProperty("dropSeconds", "3"));
        QVERIFY(script->setProperty("fadeOut", "0"));
        setFullBankValues(s, {1, 0.8, 0, 0, 0, 0, 0, 0});
        RGBMap first, duplicate, second;
        s.publishTimeNs = t0;
        {
            const QVariantMap bankMap = audioViewToVariant(AudioRenderView::fromSnapshot(s, t0))
                .value("banks").toMap().value("full").toMap();
            const QVariantList novelty = bankMap.value("novelty").toList();
            QCOMPARE(bankMap.value("count").toInt(), 8);
            QCOMPARE(novelty.size(), 8);
            QCOMPARE(novelty[0].toDouble(), 1.0);
            QCOMPARE(novelty[7].toDouble(), 0.0);
        }
        script->rgbMapWithAudio({12, 5}, 0xffffff, 0, first, AudioRenderView::fromSnapshot(s, t0));
        script->rgbMapWithAudio({12, 5}, 0xffffff, 0, duplicate, AudioRenderView::fromSnapshot(s, t0));
        setFullBankValues(s, {0, 0, 0, 0, 0, 0, 0.8, 1});
        s.publishTimeNs = t0 + 700000000;
        {
            const QVariantMap bankMap = audioViewToVariant(AudioRenderView::fromSnapshot(s, t0 + 700000000))
                .value("banks").toMap().value("full").toMap();
            const QVariantList novelty = bankMap.value("novelty").toList();
            QCOMPARE(bankMap.value("count").toInt(), 8);
            QCOMPARE(novelty.size(), 8);
            QCOMPARE(novelty[0].toDouble(), 0.0);
            QCOMPARE(novelty[7].toDouble(), 1.0);
        }
        script->rgbMapWithAudio({12, 5}, 0xffffff, 0, second, AudioRenderView::fromSnapshot(s, t0 + 700000000));
        QCOMPARE(duplicate, first);
        QCOMPARE(first.size(), 5);
        QCOMPARE(first[0].size(), 12);
        const auto brightestX = [](const QVector<uint> &row) {
            int best = -1;
            int bestLuma = -1;
            for (int i = 0; i < row.size(); ++i)
            {
                const uint px = row[i];
                const int luma = qRed(px) + qGreen(px) + qBlue(px);
                if (luma > bestLuma)
                {
                    bestLuma = luma;
                    best = i;
                }
            }
            return best;
        };
        const auto rowToString = [](const QVector<uint> &row) {
            QStringList channels;
            channels.reserve(row.size());
            for (uint pixel : row)
                channels << QString::asprintf("%02x%02x%02x", qRed(pixel), qGreen(pixel), qBlue(pixel));
            return channels.join(",");
        };
        QVERIFY2(first[0] != second[0],
            qPrintable(QString("firstRow=%1 secondRow=%2")
                .arg(rowToString(first[0]), rowToString(second[0]))));
        const int leftPeak = brightestX(first[0]);
        const int rightPeak = brightestX(second[0]);
        QVERIFY2(leftPeak >= 0 && leftPeak < 6,
            qPrintable(QString("leftPeak=%1 firstRow=%2").arg(leftPeak).arg(rowToString(first[0]))));
        QVERIFY2(rightPeak >= 6 && rightPeak < 12,
            qPrintable(QString("rightPeak=%1 secondRow=%2").arg(rightPeak).arg(rowToString(second[0]))));
        QCOMPARE(second[1], first[0]);
        QCOMPARE(rowEnergy(second[2]), 0);
        return;
    }

    if (caseId == "waterfall-center-fade")
    {
        HUEScript script(&doc);
        QVERIFY(script.load(scriptPath("audiowaterfall.js")));
        QVERIFY(script.setProperty("bands", "4"));
        QVERIFY(script.setProperty("aggregation", "Max"));
        QVERIFY(script.setProperty("centerMode", "On"));
        QVERIFY(script.setProperty("dropSeconds", "3"));
        QVERIFY(script.setProperty("fadeOut", "1"));
        script.rgbMapSetColors({0xffffff, 0x00ff00});
        AudioSnapshot s = makeSnapshot();
        setFullBank(s, {0.9, 0.1, 0.0, 0.0}, {0.9, 0.1, 0.0, 0.0});
        RGBMap map;
        const int64_t t0 = 7000000000LL;
        s.publishTimeNs = t0;
        script.rgbMapWithAudio({4, 5}, 0xffffff, 0, map, AudioRenderView::fromSnapshot(s, t0));
        QVERIFY(rowEnergy(map[2]) > 0);
        QCOMPARE(rowEnergy(map[0]), 0);
        QCOMPARE(rowEnergy(map[4]), 0);
        return;
    }

    if (caseId == "digitalrain-raw-phase-time")
    {
        HUEScript script(&doc);
        QVERIFY(script.load(scriptPath("audiodigitalrain.js")));
        QVERIFY(script.setProperty("addSpeed", "30"));
        QVERIFY(script.setProperty("lineWidth", "30"));
        QVERIFY(script.setProperty("runSeconds", "2"));
        QVERIFY(script.setProperty("tail", "100"));
        QVERIFY(script.setProperty("tailSegments", "4"));
        QVERIFY(script.setProperty("multiplier", "3"));
        AudioSnapshot s = makeSnapshot();
        s.powersRaw[0] = 0;
        s.powersRaw[1] = 0;
        s.powersRaw[2] = 0;
        s.powersRaw[3] = 0;
        RGBMap warmup, baseline, phase0, phase99, moved, boosted, tail;
        const int64_t t0 = 8000000000LL;
        s.music.beatPhase = 0.1;
        s.publishTimeNs = t0;
        script.rgbMapWithAudio({8, 24}, 0xffffff, 0, warmup, AudioRenderView::fromSnapshot(s, t0));
        s.publishTimeNs = t0 + 400000000;
        script.rgbMapWithAudio({8, 24}, 0xffffff, 0, baseline, AudioRenderView::fromSnapshot(s, t0 + 400000000));
        s.music.beatPhase = 0.0;
        s.publishTimeNs = t0 + 400000000;
        script.rgbMapWithAudio({8, 24}, 0xffffff, 0, phase0, AudioRenderView::fromSnapshot(s, t0 + 400000000));
        s.music.beatPhase = 0.99;
        s.publishTimeNs = t0 + 400000000;
        script.rgbMapWithAudio({8, 24}, 0xffffff, 0, phase99, AudioRenderView::fromSnapshot(s, t0 + 400000000));
        s.music.beatPhase = 0.1;
        s.publishTimeNs = t0 + 800000000;
        script.rgbMapWithAudio({8, 24}, 0xffffff, 0, moved, AudioRenderView::fromSnapshot(s, t0 + 800000000));
        s.powersRaw[0] = 1;
        s.powersRaw[1] = 1;
        s.powersRaw[2] = 1;
        s.powersRaw[3] = 1;
        s.publishTimeNs = t0 + 1200000000;
        script.rgbMapWithAudio({8, 24}, 0xffffff, 0, boosted, AudioRenderView::fromSnapshot(s, t0 + 1200000000));
        s.powersRaw[0] = 0;
        s.powersRaw[1] = 0;
        s.powersRaw[2] = 0;
        s.powersRaw[3] = 0;
        s.publishTimeNs = t0 + 1700000000;
        script.rgbMapWithAudio({8, 24}, 0xffffff, 0, tail, AudioRenderView::fromSnapshot(s, t0 + 1700000000));
        QVERIFY(brightestRow(baseline) >= 0);
        QVERIFY(maxChannelDelta(phase0, phase99) > 0);
        QVERIFY(maxChannelDelta(baseline, moved) > 0);
        QVERIFY(maxChannelDelta(moved, boosted) > 0);
        QVERIFY(litPixels(tail) > 0);
        return;
    }

    if (caseId == "pitchspectrum-valid-pitch-and-stale")
    {
        QVERIFY(doc.rgbScriptsCache()->load(rgbScriptsDirectory()));
        QVERIFY(doc.hueScriptsCache()->load(scriptsDirectory(), true));
        QVERIFY(doc.hueScriptsCache()->load(rgbScriptsDirectory(), false));

        HUEMatrix *holder = new HUEMatrix(&doc);
        holder->setName("pitch-spectrum-proof");
        holder->setAlgorithm(HUEMatrix::createAlgorithm(&doc, "Audio Pitch Spectrum"));
        QVERIFY(holder->algorithm() != nullptr);
        holder->setColor(0, QColor("#ff0000"));
        holder->setColor(1, QColor("#ffff00"));
        holder->setColor(2, QColor("#00ff00"));
        holder->setColor(3, QColor("#00ffff"));
        holder->setColor(4, QColor("#0000ff"));
        QVERIFY(doc.addFunction(holder));
        HUEScript *script = dynamic_cast<HUEScript *>(holder->algorithm());
        QVERIFY(script != nullptr);
        QVERIFY(script->setProperty("blur", "0"));
        QVERIFY(script->setProperty("mirror", "No"));
        QVERIFY(script->setProperty("fadeRate", "0"));
        QVERIFY(script->setProperty("responsiveness", "1"));
        AudioSnapshot s = makeSnapshot();
        setFullBank(s, {1.0, 1.0, 1.0, 1.0}, {0.0, 0.0, 0.0, 0.0});
        s.pitch.hz = 27.5;  // MIDI 21 -> first palette stop
        s.pitch.value = 21.0;
        s.pitch.unit = "midi";
        s.pitch.confidence = 0.9;
        RGBMap lowPitch, highPitch, fresh, stale;
        const int64_t t0 = 9000000000LL;
        s.publishTimeNs = t0;
        script->rgbMapWithAudio({5, 1}, 0xffffff, 0, lowPitch, AudioRenderView::fromSnapshot(s, t0));
        s.pitch.hz = 4186.009044809578;  // MIDI 108 -> last palette stop
        s.pitch.value = 108.0;
        s.publishTimeNs = t0 + 100000000;
        script->rgbMapWithAudio({5, 1}, 0xffffff, 0, highPitch, AudioRenderView::fromSnapshot(s, t0 + 100000000));
        QCOMPARE(lowPitch.size(), 1);
        QCOMPARE(highPitch.size(), 1);
        QCOMPARE(lowPitch[0].size(), 5);
        QCOMPARE(highPitch[0].size(), 5);
        for (int i = 0; i < 5; ++i)
        {
            QCOMPARE(qRed(lowPitch[0][i]), 255);
            QCOMPARE(qGreen(lowPitch[0][i]), 0);
            QCOMPARE(qBlue(lowPitch[0][i]), 0);
            QCOMPARE(qRed(highPitch[0][i]), 0);
            QCOMPARE(qGreen(highPitch[0][i]), 0);
            QCOMPARE(qBlue(highPitch[0][i]), 255);
        }
        QVERIFY(lowPitch != highPitch);
        QVERIFY(script->setProperty("fadeRate", "0.5"));
        s.publishTimeNs = t0 + 200000000;
        script->rgbMapWithAudio({5, 1}, 0xffffff, 0, fresh,
            AudioRenderView::fromSnapshot(s, t0 + 200000000));
        QVERIFY(litPixels(fresh) > 0);
        script->rgbMapWithAudio({5, 1}, 0xffffff, 0, stale,
            AudioRenderView::fromSnapshot(s, t0 + 500000000));
        QCOMPARE(litPixels(stale), 0);
        return;
    }

    QFAIL(qPrintable(QString("Unhandled packed-script case %1").arg(caseId)));
}

void AudioConsumers_Test::paletteRoutingRegression_data()
{
    QTest::addColumn<QString>("algorithmName");
    QTest::addColumn<QString>("sourceMode");
    QTest::addColumn<QString>("scriptFile");

    QTest::newRow("melt") << QString("Audio Melt") << QString("LedFx Melt") << QString("audiomelt.js");
    QTest::newRow("melt-and-sparkle")
        << QString("Audio Melt and Sparkle") << QString("LedFx Melt and Sparkle")
        << QString("audiomeltsparkle.js");
    QTest::newRow("blocks")
        << QString("Audio Blocks") << QString("LedFx Block Reflections") << QString("audioblocks.js");
    QTest::newRow("crawler")
        << QString("Audio Crawler") << QString("LedFx Crawler") << QString("audiocrawler.js");
    QTest::newRow("lava")
        << QString("Audio Lava Lamp") << QString("LedFx Lava Lamp") << QString("audiolava.js");
    QTest::newRow("water")
        << QString("Audio Water") << QString("LedFx Water") << QString("audiowater.js");
    QTest::newRow("fire")
        << QString("Audio Fire") << QString("LedFx Fire") << QString("audiofire.js");
}

void AudioConsumers_Test::paletteRoutingRegression()
{
    QFETCH(QString, algorithmName);
    QFETCH(QString, sourceMode);
    QFETCH(QString, scriptFile);

    failOnUnexpectedHueScriptWarnings();

    Doc doc(nullptr);
    doc.m_inputCapture.reset(new ConsumerTestCapture());
    QVERIFY(doc.rgbScriptsCache()->load(rgbScriptsDirectory()));
    QVERIFY(doc.hueScriptsCache()->load(scriptsDirectory(), true));
    QVERIFY(doc.hueScriptsCache()->load(rgbScriptsDirectory(), false));

    const QSize layout(13, 5);
    const std::array<QColor, 3> paletteA{
        QColor("#ff2200"),
        QColor("#00d060"),
        QColor("#2040ff")
    };
    const std::array<QColor, 3> paletteB{
        QColor("#2040ff"),
        QColor("#00d060"),
        QColor("#ff2200")
    };

    const auto makeSnapshot = [](int frame, int64_t baseNs) {
        AudioSnapshot snapshot = sweepSnapshotForFrame(frame, baseNs);
        snapshot.sourceId = "palette-routing-regression";
        snapshot.profileId = 27;
        snapshot.sourceEpoch = 5;
        snapshot.configRevision = 2;
        snapshot.events = {};
        snapshot.lows = 0.0;
        snapshot.mids = 0.73;
        snapshot.highs = 0.86;
        snapshot.powersRaw[0] = 0.0;
        snapshot.powersRaw[1] = 0.67;
        snapshot.powersRaw[2] = 0.92;
        snapshot.powersRaw[3] = 0.44;
        snapshot.publishTimeNs = baseNs + int64_t(frame) * 20000000LL;
        return snapshot;
    };

    struct RenderResult
    {
        RGBMap map;
        QString error;
    };

    const auto renderWithPalette = [&](const QString &mode,
                                       const std::array<QColor, 3> &palette,
                                       const QString &scriptPath,
                                       int64_t baseNs,
                                       const QString &suffix) -> RenderResult {
        RenderResult result;
        std::unique_ptr<HUEMatrix> holder(new HUEMatrix(&doc));
        holder->setName(QString("palette-routing-%1-%2-%3")
            .arg(slugToken(algorithmName), slugToken(mode), suffix));

        std::unique_ptr<HUEScript> loadedScript(new HUEScript(&doc));
        loadedScript->setHsvContract(true);
        if (!loadedScript->load(scriptPath))
        {
            result.error = QString("Failed to load script copy %1").arg(scriptPath);
            return result;
        }
        holder->setAlgorithm(loadedScript.release());

        if (holder->algorithm() == nullptr)
        {
            result.error = QString("Missing algorithm %1").arg(algorithmName);
            return result;
        }

        for (int colorIndex = 0; colorIndex < 3; ++colorIndex)
            holder->setColor(colorIndex, palette[size_t(colorIndex)]);

        if (!doc.addFunction(holder.get()))
        {
            result.error = QString("Failed to register HUEMatrix owner for %1").arg(algorithmName);
            return result;
        }
        HUEMatrix *ownedHolder = holder.release();
        const quint32 holderId = ownedHolder->id();
        auto holderCleanup = qScopeGuard([&]() {
            if (doc.function(holderId) != nullptr)
                doc.deleteFunction(holderId);
        });

        auto *script = dynamic_cast<HUEScript *>(ownedHolder->algorithm());
        if (script == nullptr)
        {
            result.error = QString("Algorithm %1 is not a HUEScript instance").arg(algorithmName);
            return result;
        }
        if (!script->setProperty("mode", mode))
        {
            result.error = QString("Failed to set mode %1 on %2").arg(mode, algorithmName);
            return result;
        }
        if (algorithmName == "Audio Melt and Sparkle"
            && !script->setProperty("presetStrobeThreshold", "10"))
        {
            result.error = QString("Failed to force deterministic threshold for %1").arg(algorithmName);
            return result;
        }

        for (int frame = 0; frame < 4; ++frame)
        {
            AudioSnapshot snapshot = makeSnapshot(frame, baseNs);
            script->rgbMapWithAudio(layout, 0xffffff, frame, result.map,
                AudioRenderView::fromSnapshot(snapshot, snapshot.publishTimeNs), layout);
            if (result.map.size() != layout.height())
            {
                result.error = QString("Unexpected map height for %1 (%2): got %3 expected %4")
                    .arg(algorithmName, mode)
                    .arg(result.map.size())
                    .arg(layout.height());
                return result;
            }
            for (const auto &row : result.map)
            {
                if (row.size() != layout.width())
                {
                    result.error = QString("Unexpected map width for %1 (%2): got %3 expected %4")
                        .arg(algorithmName, mode)
                        .arg(row.size())
                        .arg(layout.width());
                    return result;
                }
            }
        }

        return result;
    };

    QFile sourceScript(scriptsDirectory().filePath(scriptFile));
    QVERIFY2(sourceScript.open(QIODevice::ReadOnly),
        qPrintable(QString("Failed to read %1").arg(sourceScript.fileName())));
    const QString sourceScriptBody = QString::fromUtf8(sourceScript.readAll());
    QString downgradedScriptSource = sourceScriptBody;
    const QRegularExpression acceptColorsPattern(QStringLiteral("algo\\.acceptColors\\s*=\\s*3\\s*;"));
    QVERIFY2(acceptColorsPattern.match(downgradedScriptSource).hasMatch(),
        qPrintable(QString("Missing acceptColors=3 in %1").arg(scriptFile)));
    downgradedScriptSource.replace(acceptColorsPattern, QStringLiteral("algo.acceptColors = 0;"));
    const QDir scratchDir(QDir::current().filePath(QStringLiteral("hue-palette-regression")));
    QVERIFY(QDir().mkpath(scratchDir.absolutePath()));
    const auto writeSeededCopy = [&](const QString &tag, const QString &body) {
        constexpr quint32 kSeed = 0x4d3c2b1aU;
        const QString path = scratchDir.filePath(QString("%1-%2.js").arg(slugToken(scriptFile), tag));
        QFile copy(path);
        if (!copy.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return QString();
        QString seededBody = body;
        seededBody.replace(QRegularExpression(QStringLiteral("\\bMath\\.random\\(\\)")),
                           QStringLiteral("__qlcSeededRandom()"));
        const QString seededPreamble = QStringLiteral(
            "var __qlcSeededRandom = (function(seed){\n"
            "var state = seed >>> 0;\n"
            "return function() {\n"
            "state = (1664525 * state + 1013904223) >>> 0;\n"
            "return state / 4294967296;\n"
            "};\n"
            "})(%1);\n").arg(kSeed);
        if (copy.write((seededPreamble + seededBody).toUtf8()) <= 0)
            return QString();
        copy.close();
        return path;
    };

    const QString seededPath = writeSeededCopy("seeded", sourceScriptBody);
    QVERIFY2(!seededPath.isEmpty(), qPrintable(QString("Failed to write seeded copy for %1").arg(scriptFile)));
    const QString downgradedPath = writeSeededCopy("seeded-acceptcolors0", downgradedScriptSource);
    QVERIFY2(!downgradedPath.isEmpty(), qPrintable(QString("Failed to write downgraded copy for %1").arg(scriptFile)));
    auto copyCleanup = qScopeGuard([&]() {
        QFile::remove(seededPath);
        QFile::remove(downgradedPath);
        QDir().rmdir(scratchDir.absolutePath());
    });

    const RenderResult artisticA = renderWithPalette("Artistic", paletteA, seededPath, 41000000000LL, "artistic-a");
    QVERIFY2(artisticA.error.isEmpty(), qPrintable(artisticA.error));
    const RenderResult artisticB = renderWithPalette("Artistic", paletteB, seededPath, 41000000000LL, "artistic-b");
    QVERIFY2(artisticB.error.isEmpty(), qPrintable(artisticB.error));
    const QByteArray artisticPackedA = packedPixels(artisticA.map);
    const QByteArray artisticPackedB = packedPixels(artisticB.map);
    QCOMPARE(artisticPackedA, artisticPackedB);

    const RenderResult sourceA = renderWithPalette(sourceMode, paletteA, seededPath, 42000000000LL, "source-a");
    QVERIFY2(sourceA.error.isEmpty(), qPrintable(sourceA.error));
    const RenderResult sourceB = renderWithPalette(sourceMode, paletteB, seededPath, 42000000000LL, "source-b");
    QVERIFY2(sourceB.error.isEmpty(), qPrintable(sourceB.error));
    QVERIFY2(litPixels(sourceA.map) > 0 && litPixels(sourceB.map) > 0,
        qPrintable(QString("%1 produced black source-mode output").arg(algorithmName)));
    const QByteArray sourcePackedA = packedPixels(sourceA.map);
    const QByteArray sourcePackedB = packedPixels(sourceB.map);
    QVERIFY2(sourcePackedA != sourcePackedB,
        qPrintable(QString("%1 did not react to source-mode palette reversal").arg(algorithmName)));

    const RenderResult degradedA =
        renderWithPalette(sourceMode, paletteA, downgradedPath, 43000000000LL, "degraded-a");
    QVERIFY2(degradedA.error.isEmpty(), qPrintable(degradedA.error));
    const RenderResult degradedB =
        renderWithPalette(sourceMode, paletteB, downgradedPath, 43000000000LL, "degraded-b");
    QVERIFY2(degradedB.error.isEmpty(), qPrintable(degradedB.error));
    QCOMPARE(packedPixels(degradedA.map), packedPixels(degradedB.map));
}

void AudioConsumers_Test::nativeExecutionContractSweep_data()
{
    QTest::addColumn<QString>("caseId");
    QTest::addColumn<QString>("algorithmName");
    QTest::addColumn<QString>("propertyName");
    QTest::addColumn<QString>("propertyValue");
    QTest::addColumn<QSize>("layout");
    QTest::addColumn<bool>("usesAudio");
    QTest::addColumn<bool>("branchCase");

    const QVector<NativeSweepCase> &rows = nativeSweepCases();
    QVERIFY2(!rows.isEmpty(), "Failed to enumerate native execution sweep cases from descriptors");

    QSet<QString> algorithmNames;
    int branchCases = 0;
    for (const NativeSweepCase &row : rows)
    {
        algorithmNames.insert(row.algorithmName);
        branchCases += row.branchCase ? 1 : 0;
        QTest::newRow(row.caseId.toUtf8().constData())
            << row.caseId << row.algorithmName << row.propertyName << row.propertyValue
            << row.layout << row.usesAudio << row.branchCase;
    }

    QCOMPARE(algorithmNames.size(), 70);
    qInfo() << "NATIVE_EXECUTION_CONTRACT_SWEEP rows" << rows.size()
            << "algorithmRows" << algorithmNames.size()
            << "listBranchRows" << branchCases;
}

void AudioConsumers_Test::nativeExecutionContractSweep()
{
    QFETCH(QString, caseId);
    QFETCH(QString, algorithmName);
    QFETCH(QString, propertyName);
    QFETCH(QString, propertyValue);
    QFETCH(QSize, layout);
    QFETCH(bool, usesAudio);

    failOnUnexpectedHueScriptWarnings();
    QTest::failOnWarning(
        QRegularExpression(QStringLiteral(".*Exception at line.*Error:.*")));
    QTest::failOnWarning(
        QRegularExpression(QStringLiteral(".*requires a flat Float32Array of length width\\*height\\*3.*")));
    QTest::failOnWarning(
        QRegularExpression(QStringLiteral(".*Float32Array \\(HSV\\) is supported.*")));
    QTest::failOnWarning(
        QRegularExpression(QStringLiteral(".*TypedArray size mismatch.*")));
    QTest::failOnWarning(
        QRegularExpression(QStringLiteral(".*ArrayBuffer extraction size mismatch.*")));

    Doc *doc = nativeSweepDoc();
    QVERIFY(ensureNativeSweepCachesLoaded(doc));

    static bool paletteControlChecked = false;
    if (!paletteControlChecked)
    {
        std::unique_ptr<HUEMatrix> controlHolder(new HUEMatrix(doc));
        controlHolder->setName("native-execution-palette-control");
        controlHolder->setAlgorithm(HUEMatrix::createAlgorithm(doc, "Audio Magnitude"));
        QVERIFY(controlHolder->algorithm() != nullptr);
        controlHolder->setColor(0, QColor("#ff0000"));
        controlHolder->setColor(1, QColor("#ffff00"));
        controlHolder->setColor(2, QColor("#00ff00"));
        controlHolder->setColor(3, QColor("#00ffff"));
        controlHolder->setColor(4, QColor("#0000ff"));
        QVERIFY2(doc->addFunction(controlHolder.get()), "Failed to register palette control HUEMatrix");
        HUEMatrix *ownedControl = controlHolder.release();
        const quint32 controlId = ownedControl->id();
        auto controlCleanup = qScopeGuard([&]() {
            if (doc->function(controlId) != nullptr)
                doc->deleteFunction(controlId);
        });
        auto *controlScript = dynamic_cast<HUEScript *>(ownedControl->algorithm());
        QVERIFY(controlScript != nullptr);
        QVERIFY(controlScript->setProperty("blur", "0"));
        QVERIFY(controlScript->setProperty("mirror", "Off"));
        AudioSnapshot s = sweepSnapshotForFrame(0, 26000000000LL);
        RGBMap firstPaletteMap;
        controlScript->rgbMapWithAudio({5, 1}, 0xffffff, 0, firstPaletteMap,
            AudioRenderView::fromSnapshot(s, s.publishTimeNs), {5, 1});
        QCOMPARE(firstPaletteMap.size(), 1);
        QCOMPARE(firstPaletteMap[0].size(), 5);
        ownedControl->setColor(0, QColor("#f0f0f0"));
        ownedControl->setColor(1, QColor("#c0c0ff"));
        ownedControl->setColor(2, QColor("#7070ff"));
        ownedControl->setColor(3, QColor("#2020ff"));
        ownedControl->setColor(4, QColor("#000040"));
        RGBMap secondPaletteMap;
        controlScript->rgbMapWithAudio({5, 1}, 0xffffff, 0, secondPaletteMap,
            AudioRenderView::fromSnapshot(s, s.publishTimeNs), {5, 1});
        QCOMPARE(secondPaletteMap.size(), 1);
        QCOMPARE(secondPaletteMap[0].size(), 5);
        QVERIFY2(maxChannelDelta(firstPaletteMap, secondPaletteMap) > 0,
            "Palette-sensitive control did not react to owner palette change");
        paletteControlChecked = true;
    }

    std::unique_ptr<HUEMatrix> holder(new HUEMatrix(doc));
    holder->setName(QString("native-execution-sweep-%1").arg(caseId));
    holder->setAlgorithm(HUEMatrix::createAlgorithm(doc, algorithmName));
    QVERIFY2(holder->algorithm() != nullptr, qPrintable(QString("Missing algorithm %1").arg(algorithmName)));
    holder->setColor(0, QColor("#0a1430"));
    holder->setColor(1, QColor("#2070ff"));
    holder->setColor(2, QColor("#25dba5"));
    holder->setColor(3, QColor("#f0a030"));
    holder->setColor(4, QColor("#ffe8e0"));
    QVERIFY2(doc->addFunction(holder.get()),
        qPrintable(QString("Failed to register HUEMatrix for %1").arg(caseId)));
    HUEMatrix *ownedHolder = holder.release();
    const quint32 holderId = ownedHolder->id();
    auto holderCleanup = qScopeGuard([&]() {
        if (doc->function(holderId) != nullptr)
            doc->deleteFunction(holderId);
    });

    RGBAlgorithm *algorithm = ownedHolder->algorithm();
    auto *script = dynamic_cast<RGBScript *>(algorithm);
    if (!propertyName.isEmpty())
    {
        QVERIFY2(script != nullptr, qPrintable(QString("Property row for non-script algorithm %1").arg(algorithmName)));
        QVERIFY2(script->setProperty(propertyName, propertyValue),
            qPrintable(QString("setProperty failed for %1.%2=%3")
                .arg(algorithmName, propertyName, propertyValue)));
        QCOMPARE(script->property(propertyName), propertyValue);
    }

    QVERIFY(layout.width() > 0);
    QVERIFY(layout.height() > 0);

    QVector<RGBMap> maps;
    maps.reserve(4);
    auto *hueScript = dynamic_cast<HUEScript *>(algorithm);
    const int64_t baseNs = 24000000000LL;
    for (int frame = 0; frame < 4; ++frame)
    {
        RGBMap map;
        if (hueScript != nullptr)
        {
            if (usesAudio)
            {
                const int snapshotFrame = frame > 1 ? 1 : frame;
                AudioSnapshot snapshot = sweepSnapshotForFrame(snapshotFrame, baseNs);
                const int64_t nowNs = (frame == 3) ? snapshot.publishTimeNs + 350000000LL
                                                   : snapshot.publishTimeNs;
                hueScript->rgbMapWithAudio(layout, 0xffffff, frame, map,
                    AudioRenderView::fromSnapshot(snapshot, nowNs), layout);
            }
            else
            {
                hueScript->rgbMap(layout, 0xffffff, frame, map);
                if (frame < 3)
                    QTest::qSleep(25);
            }
        }
        else
            algorithm->rgbMap(layout, 0xffffff, frame, map);

        QCOMPARE(map.size(), layout.height());
        for (const auto &row : map)
            QCOMPARE(row.size(), layout.width());
        maps.push_back(map);
    }

    QVERIFY(!maps.isEmpty());
}

void AudioConsumers_Test::scriptRunnerStop_data()
{
    QTest::addColumn<QString>("content");
    QTest::addColumn<bool>("cancel");
    QTest::addColumn<bool>("beforePublication");
    QTest::addColumn<bool>("invalidSyntax");
    QTest::newRow("natural-finish") << "Engine.objectName = 'running';" << false << false << false;
    QTest::newRow("cancel-javascript")
        << "while (true) { Engine.objectName = 'running'; }" << true << false << false;
    QTest::newRow("cancel-wait")
        << "Engine.objectName = 'running'; Engine.waitTime(60000);" << true << false << false;
    QTest::newRow("cancel-before-publication")
        << "Engine.objectName = 'unexpected';" << true << true << false;
    QTest::newRow("malformed-javascript") << "var value = ;" << false << false << true;
}

void AudioConsumers_Test::scriptRunnerStop()
{
    QFETCH(QString, content);
    QFETCH(bool, cancel);
    QFETCH(bool, beforePublication);
    QFETCH(bool, invalidSyntax);
    Doc doc(nullptr);
    doc.m_inputCapture.reset(new ConsumerTestCapture());
    ScriptRunner runner(&doc, nullptr, content);
    QSemaphore entered, continueScript, destroying, continueDestruction;
    bool destructionOnRunner = false;
    bool engineUnpublished = false;
    const auto cleanup = qScopeGuard([&]() {
        continueScript.release();
        continueDestruction.release();
        runner.stop();
        runner.wait();
    });
    const auto gate = [&]() {
        entered.release();
        continueScript.acquire();
    };
    if (invalidSyntax)
    {
        connect(&runner, &QThread::finished, &runner, [&, gate]() {
            {
                QMutexLocker engineLocker(&runner.m_engineMutex);
                engineUnpublished = runner.m_engine == nullptr;
            }
            gate();
        }, Qt::DirectConnection);
    }
    else if (beforePublication)
        connect(&runner, &QThread::started, &runner, gate, Qt::DirectConnection);
    else
        connect(&runner, &QObject::objectNameChanged, &runner, gate, Qt::DirectConnection);

    runner.execute();
    QVERIFY2(entered.tryAcquire(1, 5000), "runner did not reach the execution gate");
    if (!beforePublication && !invalidSyntax)
    {
        QMutexLocker engineLocker(&runner.m_engineMutex);
        QVERIFY(runner.m_engine != nullptr);
        connect(runner.m_engine, &QObject::destroyed, &runner, [&]() {
            destructionOnRunner = QThread::currentThread() == &runner;
            {
                QMutexLocker engineLocker(&runner.m_engineMutex);
                engineUnpublished = runner.m_engine == nullptr;
            }
            destroying.release();
            continueDestruction.acquire();
        }, Qt::DirectConnection);
    }

    if (cancel || invalidSyntax)
        runner.stop();
    continueScript.release();
    if (!beforePublication && !invalidSyntax)
    {
        QVERIFY2(destroying.tryAcquire(1, 5000), "runner did not begin engine destruction");
        // The worker is inside engine destruction while the caller stops it.
        runner.stop();
        continueDestruction.release();
    }
    QVERIFY2(runner.wait(5000), "runner did not finish after stop");
    QVERIFY(!runner.m_running);
    QVERIFY(runner.m_engine == nullptr);
    if (beforePublication)
        QVERIFY(runner.objectName().isEmpty());
    else
    {
        if (!invalidSyntax)
            QVERIFY(destructionOnRunner);
        QVERIFY(engineUnpublished);
    }
}

void AudioConsumers_Test::pcmToPixels_data()
{
    QTest::addColumn<double>("frequency");
    QTest::newRow("quiet-formant") << 900.0;
    QTest::newRow("high-content") << 6000.0;
}

void AudioConsumers_Test::pcmToPixels()
{
    QFETCH(double, frequency);
    Doc doc(nullptr);
    doc.m_inputCapture.reset(new ConsumerTestCapture());
    QVERIFY(doc.hueScriptsCache()->load(scriptsDirectory(), true));
    HUEScript spectrum(&doc);
    QVERIFY(spectrum.load(scriptsDirectory().filePath("audiospectrum.js")));
    AudioAnalyzer analyzer;
    auto config = AudioChannelConfig::defaults();
    config.noiseGate.thresholdDb = -65;
    analyzer.defaultChannel()->updateConfig(config);
    RGBMap map, firstLit;
    bool changed = false;
    uint64_t beats = 0, kicks = 0;
    for (int n = 0; n < 30; ++n)
    {
        std::array<float, 500> samples{};
        for (int i = 0; i < 500; ++i)
        {
            const double t = (n * 500 + i) / 30000.0;
            samples[i] = float(0.001 * std::sin(2 * M_PI * frequency * t)
                + 0.0003 * std::sin(2 * M_PI * (frequency * 1.31) * t));
        }
        auto frame = pcmFrame(samples.data(), n + 1);
        analyzer.processFrame(frame);
        auto snap = analyzer.snapshot();
        beats = snap.events.beat;
        kicks = snap.events.kick;
        snap.publishTimeNs = int64_t(n + 1) * 1000000000 / 60;
        spectrum.rgbMapWithAudio({37, 1}, 0xffffff, 0, map,
            AudioRenderView::fromSnapshot(snap, snap.publishTimeNs));
        QVERIFY(!map.isEmpty());
        const bool lit = std::any_of(map[0].begin(), map[0].end(),
            [](uint pixel) { return (pixel & 0xffffff) != 0; });
        if (lit && firstLit.isEmpty())
            firstLit = map;
        if (!firstLit.isEmpty() && map != firstLit)
            changed = true;
    }
    QVERIFY(!firstLit.isEmpty());
    QVERIFY(changed);
    QCOMPARE(beats, uint64_t(0));
    QCOMPARE(kicks, uint64_t(0));
}

void AudioConsumers_Test::powerEcho_data()
{
    QTest::addColumn<int>("offset");
    for (int offset : {0, 1, 3, 7})
        QTest::newRow(qPrintable(QString("float-offset-%1").arg(offset))) << offset;
}

void AudioConsumers_Test::powerEcho()
{
    QFETCH(int, offset);
    AudioAnalyzer first, second, wrongInput;
    std::array<float, 507> samples{};
    std::array<float, 500> other{};
    bool rejectedWrongInput = false;
    double maximumError = 0;
    for (int hop = 0; hop < 90; ++hop)
    {
        for (int i = 0; i < 500; ++i)
        {
            const double time = double(hop * 500 + i) / 30000;
            samples[i + offset] = float(0.09 * std::sin(2 * M_PI * 937 * time)
                + 0.03 * std::sin(2 * M_PI * 7313 * time));
            other[i] = float(0.08 * std::sin(2 * M_PI * 179 * time));
        }
        auto frame = pcmFrame(samples.data() + offset, hop + 1);
        first.processFrame(frame);
        second.processFrame(frame);
        auto wrong = pcmFrame(other.data(), hop + 1);
        wrongInput.processFrame(wrong);
        const auto a = first.snapshot(), b = second.snapshot();
        const QJsonArray recorded{a.beatPower, a.bassPower, a.mids, a.highs};
        QVERIFY(powerEchoMatches(b, recorded));
        maximumError = std::max(maximumError, std::abs(a.mids - b.mids));
        rejectedWrongInput |= !powerEchoMatches(wrongInput.snapshot(), recorded);
        if (hop > 0)
        {
            auto corrupted = recorded;
            corrupted[2] = a.mids + 1e-4;
            QVERIFY(!powerEchoMatches(b, corrupted));
        }
    }
    QVERIFY(rejectedWrongInput);
    auto observed = first.snapshot();
    observed.mids = 0.54521181188317935;
    QVERIFY(powerEchoMatches(observed,
        {observed.beatPower, observed.bassPower, 0.54521177534713339, observed.highs}));
    qInfo() << "PCM_POWER_ECHO maximum_mids_error" << maximumError << "wrong_input_rejected" << rejectedWrongInput;
}

void AudioConsumers_Test::exportEffects()
{
    const QDir input(qEnvironmentVariable("QLC_AUDIO_EFFECT_INPUT"));
    const QDir output(qEnvironmentVariable("QLC_AUDIO_EFFECT_OUTPUT"));
    const QDir reference(qEnvironmentVariable("QLC_AUDIO_EFFECT_REFERENCE"));
    if (qEnvironmentVariable("QLC_AUDIO_EFFECT_INPUT").isEmpty())
        QSKIP("Set QLC_AUDIO_EFFECT_INPUT, QLC_AUDIO_EFFECT_OUTPUT and QLC_AUDIO_EFFECT_CORPUS");
    QVERIFY(!qEnvironmentVariable("QLC_AUDIO_EFFECT_OUTPUT").isEmpty());
    QVERIFY(!qEnvironmentVariable("QLC_AUDIO_EFFECT_REFERENCE").isEmpty());
    QVERIFY(input.absolutePath() != output.absolutePath());
    QVERIFY(QDir().mkpath(output.absolutePath()));
    QFile corpusFile(qEnvironmentVariable("QLC_AUDIO_EFFECT_CORPUS"));
    QVERIFY(corpusFile.open(QIODevice::ReadOnly));
    const auto fixtures = QJsonDocument::fromJson(corpusFile.readAll()).object()["fixtures"].toArray();
    QVERIFY(!fixtures.isEmpty());
    const QDir corpusDirectory(QFileInfo(corpusFile).absolutePath());
    Doc doc(nullptr);
    doc.m_inputCapture.reset(new ConsumerTestCapture());
    QVERIFY(doc.hueScriptsCache()->load(scriptsDirectory(), true));
    const QStringList selected = qEnvironmentVariable("QLC_AUDIO_CORPUS_FIXTURE").split(',', Qt::SkipEmptyParts);
    int exported = 0;
    for (const auto &file : input.entryList({"*.jsonl"}, QDir::Files))
    {
        if (!selected.isEmpty() && !selected.contains(QFileInfo(file).baseName()))
            continue;
        QFile source(input.filePath(file)), sink(output.filePath(file));
        QVERIFY(source.open(QIODevice::ReadOnly));
        QVERIFY2(!sink.exists(), qPrintable("Refusing to overwrite " + sink.fileName()));
        QVERIFY(sink.open(QIODevice::WriteOnly | QIODevice::NewOnly));
        auto header = QJsonDocument::fromJson(source.readLine()).object();
        QCOMPARE(header["producer"].toString(), QString("production-qlc-cpp"));
        QFile referenceHeader(reference.filePath(QFileInfo(file).baseName() + ".header.json"));
        QVERIFY2(referenceHeader.open(QIODevice::ReadOnly), qPrintable(referenceHeader.fileName()));
        const auto referenceDefinition = QJsonDocument::fromJson(referenceHeader.readAll()).object();
        QCOMPARE(referenceDefinition["pcm_sha256"], header["pcm_sha256"]);
        const auto definitions = referenceDefinition["effects"].toArray();
        QCOMPARE(definitions.size(), effectScripts.size() * int(layouts.size()));
        QJsonObject fixture;
        for (const auto &entry : fixtures)
            if (entry.toObject()["id"] == header["fixture"])
                fixture = entry.toObject();
        QVERIFY(!fixture.isEmpty());
        QFile pcmFile(corpusDirectory.filePath(fixture["pcm"].toString()));
        QVERIFY(pcmFile.open(QIODevice::ReadOnly));
        const auto pcm = pcmFile.readAll();
        QVERIFY(!pcm.isEmpty() && pcm.size() % 2000 == 0);
        QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(pcm, QCryptographicHash::Sha256).toHex()),
                 header["pcm_sha256"].toString());
        AudioAnalyzer analyzer;
        auto config = AudioChannelConfig::defaults();
        config.aubio.diagnosticsEnabled = true;
        if (fixture["config"].toObject().contains("gateDb"))
            config.noiseGate.thresholdDb = fixture["config"].toObject()["gateDb"].toDouble();
        analyzer.defaultChannel()->updateConfig(config);
        std::vector<std::unique_ptr<HUEScript>> scripts;
        for (int effect = 0; effect < effectScripts.size(); ++effect)
            for (const auto &layout : layouts)
            {
                const auto definition = definitions[int(scripts.size())].toObject();
                QCOMPARE(definition["name"].toString(), effectNames[effect]);
                QCOMPARE(definition["width"].toInt(), layout.width());
                QCOMPARE(definition["height"].toInt(), layout.height());
                QFile original(scriptsDirectory().filePath(effectScripts[effect]));
                QVERIFY(original.open(QIODevice::ReadOnly));
                const QByteArray contents = original.readAll();
                const QString instrumentedPath = output.filePath(QString("%1-%2-%3x%4.js")
                    .arg(QFileInfo(file).baseName(), effectNames[effect]).arg(layout.width()).arg(layout.height()));
                QFile instrumented(instrumentedPath);
                QVERIFY(instrumented.open(QIODevice::WriteOnly | QIODevice::NewOnly));
                // Observe the actual script's linear intermediates without replacing its renderer.
                QVERIFY(instrumented.write(contents + R"JS(
;(function(algo) {
    algo.referenceDiagnostics = true;
    var render = algo.rgbMap, events = {onset: 0, beat: 0, kick: 0, bar: 0};
    algo.rgbMap = function(w, h, color, step, audio) {
        var map = render(w, h, color, step, audio);
        Object.keys(events).forEach(function(key) { events[key] += audio.events.delta[key]; });
        algo.referenceFrame.contract = {version: audio.version, fullCount: audio.banks.full.count,
            events: events, deltaSeconds: audio.timing.deltaSeconds};
        return map;
    };
    algo.properties.push("name:referenceTrace|type:string|display:Trace|write:setTrace|read:getTrace");
    algo.setTrace = function() {};
    algo.getTrace = function() { return JSON.stringify(algo.referenceFrame); };
    return algo;
})(testAlgo);
)JS") > 0);
                instrumented.close();
                auto script = std::make_unique<HUEScript>(&doc);
                QVERIFY(script->load(instrumentedPath));
                QVERIFY(script->setProperty("mode", "Reference"));
                QCOMPARE(script->property("mode"), QString("Reference"));
                if (effectNames[effect] == "strobe")
                    QVERIFY(script->setProperty("referenceClock", "Extrapolated"));
                const auto parameters = definition["config"].toObject();
                QVERIFY(script->setProperty("referenceBlur", QString::number(parameters["blur"].toDouble())));
                QVERIFY(script->setProperty("referenceMirror", parameters["mirror"].toBool() ? "On" : "Off"));
                QVERIFY(script->setProperty("referenceBrightness", QString::number(parameters["brightness"].toDouble())));
                if (effect > 0)
                {
                    QStringList palette;
                    for (const auto &color : definition["palette"].toArray())
                        palette.append(color.toString());
                    QCOMPARE(palette.size(), 3);
                    QVERIFY(script->setProperty("referencePalette", palette.join(',')));
                }
                scripts.push_back(std::move(script));
            }
        header["effects"] = definitions;
        header["effect_comparison"] = "faithful-reference-v1";
        header["effect_oracle"] = referenceDefinition["oracle"];
        header["pre_boundary"] = "Actual HUE script linear RGB, before mirror/brightness/blur/clipping";
        header["final_boundary"] = "Actual HUEScript HSV conversion and packed 8-bit RGB, row-major";
        header["effect_clock"] = "Reference extrapolated phase, including before tempo acquisition. "
            "Strobe comparison explicitly selects Extrapolated; the shipped Reference mode defaults to Detected.";
        QVERIFY(sink.write(QJsonDocument(header).toJson(QJsonDocument::Compact) + '\n') > 0);
        int frames = 0;
        QJsonObject initialEvents;
        while (!source.atEnd())
        {
            auto record = QJsonDocument::fromJson(source.readLine()).object();
            QVERIFY(record["banks"].toArray().size() == 3);
            QCOMPARE(record["frame"].toInt(), frames);
            if (frames == 0)
                initialEvents = record["events"].toObject();
            QVERIFY((frames + 1) * 2000 <= pcm.size());
            std::array<float, 500> samples;
            for (int i = 0; i < 500; ++i)
            {
                const uint32_t bits = qFromLittleEndian<uint32_t>(
                    reinterpret_cast<const uchar *>(pcm.constData() + frames * 2000 + i * 4));
                std::memcpy(&samples[i], &bits, sizeof(float));
            }
            auto frame = pcmFrame(samples.data(), frames + 1);
            analyzer.processFrame(frame);
            auto snapshot = analyzer.snapshot();
            QVERIFY2(powerEchoMatches(snapshot, record["powers"].toArray()),
                qPrintable(QString("PCM replay frame %1 powers differ; mids %2 versus analyzer trace %3")
                    .arg(frames).arg(snapshot.mids, 0, 'g', 17)
                    .arg(record["powers"].toArray()[2].toDouble(), 0, 'g', 17)));
            snapshot.publishTimeNs = int64_t(record["time_s"].toDouble() * 1e9);
            auto view = AudioRenderView::fromSnapshot(snapshot, snapshot.publishTimeNs);
            QJsonArray effects;
            for (size_t i = 0; i < scripts.size(); ++i)
            {
                const auto size = layouts[i % layouts.size()];
                RGBMap map;
                scripts[i]->rgbMapWithAudio(size, 0xffffff, 0, map, view);
                QCOMPARE(map.size(), size.height());
                const auto observed = QJsonDocument::fromJson(scripts[i]->property("referenceTrace").toUtf8()).object();
                const auto pre = observed["pre"].toArray();
                const auto transformed = observed["transformed"].toArray();
                QCOMPARE(pre.size(), size.width() * size.height());
                QCOMPARE(transformed.size(), pre.size());
                const auto contract = observed["contract"].toObject();
                QCOMPARE(contract["version"].toInt(), 6);
                QCOMPARE(contract["fullCount"].toInt(), 24);
                QVERIFY(std::abs(contract["deltaSeconds"].toDouble() - (frames ? 1.0 / 60 : 0)) < 1e-8);
                const auto received = contract["events"].toObject();
                const auto produced = record["events"].toObject();
                QCOMPARE(received["onset"].toDouble(),
                         produced["onset"].toDouble() - initialEvents["onset"].toDouble());
                QCOMPARE(received["beat"].toDouble(),
                         produced["tempo"].toDouble() - initialEvents["tempo"].toDouble());
                QCOMPARE(received["kick"].toDouble(),
                         produced["kick"].toDouble() - initialEvents["kick"].toDouble());
                QCOMPARE(received["bar"].toDouble(),
                         produced["bar_wrap"].toDouble() - initialEvents["bar_wrap"].toDouble());
                effects.append(QJsonObject{{"name", effectNames[int(i / layouts.size())]},
                    {"width", size.width()}, {"height", size.height()}, {"pre", pre},
                    {"transformed", transformed}, {"final", pixels(map)}, {"audio_contract", contract}});
            }
            record["effects"] = effects;
            QVERIFY(sink.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n') > 0);
            ++frames;
        }
        QCOMPARE(frames, header["frame_count"].toInt());
        QVERIFY(frames > 0);
        ++exported;
    }
    QVERIFY(exported > 0);
}

void AudioConsumers_Test::runtimeReplay()
{
    const QString requestPath = qEnvironmentVariable("QLC_AUDIO_RUNTIME_REQUEST");
    if (requestPath.isEmpty())
        QSKIP("Set QLC_AUDIO_RUNTIME_REQUEST and QLC_AUDIO_RUNTIME_TRACE for ten-minute replay");
#ifndef Q_OS_MACOS
    QSKIP("Native RSS/allocation measurements currently target the baseline macOS machine");
#else
    QFile requestFile(requestPath);
    QVERIFY(requestFile.open(QIODevice::ReadOnly));
    const auto request = QJsonDocument::fromJson(requestFile.readAll()).object();
    QCOMPARE(request["duration_s"].toInt(), 600);
    QCOMPARE(request["analysis_hz"].toInt(), 60);
    QCOMPARE(request["render_hz"].toInt(), 50);
    QFile pcmFile(request["pcm"].toString());
    QVERIFY(pcmFile.open(QIODevice::ReadOnly));
    const auto pcm = pcmFile.readAll();
    QVERIFY(!pcm.isEmpty() && pcm.size() % 2000 == 0);
    QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(pcm, QCryptographicHash::Sha256).toHex()),
             request["pcm_sha256"].toString());
    std::vector<float> samples(size_t(pcm.size() / 4));
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const uint32_t bits = qFromLittleEndian<uint32_t>(
            reinterpret_cast<const uchar *>(pcm.constData() + i * 4));
        std::memcpy(&samples[i], &bits, sizeof(float));
    }
    AudioAnalyzer analyzer;
    auto config = AudioChannelConfig::defaults();
    config.aubio.diagnosticsEnabled = request["diagnostics"].toBool();
    QVERIFY(analyzer.createChannel(config, 7));
    QVERIFY(analyzer.createChannel(config, 29));
    QVERIFY(analyzer.defaultChannel());
    Doc doc(nullptr);
    doc.m_inputCapture.reset(new ConsumerTestCapture());
    QVERIFY(doc.hueScriptsCache()->load(scriptsDirectory(), true));
    std::vector<std::unique_ptr<HUEScript>> scripts;
    const QString mode = qEnvironmentVariable("QLC_AUDIO_RUNTIME_MODE", "Artistic");
    for (int i = 0; i < 4; ++i)
    {
        auto script = std::make_unique<HUEScript>(&doc);
        QVERIFY(script->load(scriptsDirectory().filePath(effectScripts[i])));
        if (mode == "Reference")
            QVERIFY(script->setProperty("mode", mode));
        scripts.push_back(std::move(script));
    }
    QFile trace(qEnvironmentVariable("QLC_AUDIO_RUNTIME_TRACE"));
    QVERIFY(!trace.fileName().isEmpty());
    QVERIFY(trace.open(QIODevice::WriteOnly));
    const QJsonObject header{{"kind", "runtime-header"}, {"producer", "production-qlc-cpp"},
        {"diagnostics", config.aubio.diagnosticsEnabled}, {"mode", mode}, {"profiles", QJsonArray{7, 29}},
        {"effects", QJsonArray{"spectrum", "energy", "scroll", "wavelength"}},
        {"duration_s", 600}, {"analysis_hz", 60}, {"render_hz", 50}, {"pending_capacity", 1},
        {"default_channel_retained", true}, {"producer_channels", 3}, {"render_phase_ms", 3},
        {"scheduling", "independent PCM producer and render caller; live GUI event loop; MasterTimerPrivate overrun reanchor; shared production JS thread"},
        {"pending_sampling", "independent 1ms observer during synchronous work"},
        {"pending_scope", "in-flight audio dispatches from the single render caller, not the entire Qt event queue"},
        {"freshness_scope", "publication age when all four native maps have returned, not physical device latency"},
        {"allocation_metric", "live blocks in default malloc zone"},
        {"blocking_metric", "wall time for two snapshots and four display-size plus synchronous HUE dispatches"},
        {"cpu_metric", "separate Mach JS thread user+system time; never substituted for wall budgets"}};
    trace.write(QJsonDocument(header).toJson(QJsonDocument::Compact) + '\n');
    thread_t jsThread = MACH_PORT_NULL;
    QSemaphore ready;
    QVERIFY(HUEScript::scheduleOnJSThread([&] { jsThread = mach_thread_self(); ready.release(); }));
    QVERIFY(ready.tryAcquire(1, 5000));
    const auto jsCpuMs = [&] {
        thread_basic_info_data_t info{};
        mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
        if (thread_info(jsThread, THREAD_BASIC_INFO, reinterpret_cast<thread_info_t>(&info), &count) != KERN_SUCCESS)
            return -1.0;
        return 1000.0 * (info.user_time.seconds + info.system_time.seconds)
            + (info.user_time.microseconds + info.system_time.microseconds) / 1000.0;
    };
    std::vector<double> analysisTimes(36000), analysisStarts(36000), analysisEnds(36000), renderTimes;
    renderTimes.reserve(30000);
    std::atomic<int> completedAnalysis{0}, pendingPeak{0}, pendingSamples{0};
    std::atomic<bool> finished{false};
    const auto start = std::chrono::steady_clock::now();
    const auto nowSeconds = [&] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    };
    std::thread producer([&] {
        for (int analysis = 0; analysis < 36000; ++analysis)
        {
            std::this_thread::sleep_until(start + std::chrono::nanoseconds(int64_t((analysis + 1) * 1e9 / 60)));
            const size_t offset = (size_t(analysis) * 500) % samples.size();
            auto frame = pcmFrame(samples.data() + offset, analysis + 1);
            analysisStarts[analysis] = nowSeconds();
            QElapsedTimer measured;
            measured.start();
            analyzer.processFrame(frame);
            analysisTimes[analysis] = measured.nsecsElapsed() / 1e6;
            analysisEnds[analysis] = nowSeconds();
            completedAnalysis = analysis + 1;
        }
    });
    std::thread observer([&] {
        while (!finished)
        {
            const int pending = HUEScript::pendingAudioRenders();
            int previous = pendingPeak.load();
            while (pending > previous && !pendingPeak.compare_exchange_weak(previous, pending)) {}
            if (pending > 0)
                ++pendingSamples;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    bool valid = true;
    std::atomic<bool> renderFinished{false};
    std::thread renderer([&] {
    auto deadline = start + std::chrono::milliseconds(3);
    const auto finish = start + std::chrono::milliseconds(600003);
    while ((deadline += std::chrono::milliseconds(20)) <= finish)
    {
        const double time = std::chrono::duration<double>(deadline - start).count() - .003;
        // Match MasterTimerPrivate: one immediate late tick, then reanchor.
        // Do not fabricate catch-up renders for ticks the production timer skips.
        const bool late = std::chrono::steady_clock::now() >= deadline;
        if (!late)
            std::this_thread::sleep_until(deadline);
        const double actualStart = nowSeconds();
        const int analysisBacklog = std::max(0, std::min(36000, int(actualStart * 60)) - completedAnalysis.load());
        const double cpuStart = jsCpuMs();
        QElapsedTimer measured;
        measured.start();
        const double workStart = nowSeconds();
        const auto first = AudioRenderView::fromSnapshot(analyzer.snapshot(7), AudioRenderView::nowNs());
        const auto second = AudioRenderView::fromSnapshot(analyzer.snapshot(29), AudioRenderView::nowNs());
        const double snapshotMs = measured.nsecsElapsed() / 1e6;
        QJsonArray dispatches;
        for (size_t i = 0; i < scripts.size(); ++i)
        {
            QElapsedTimer dispatch;
            dispatch.start();
            RGBMap map;
            scripts[i]->rgbMapWithAudio({37, 1}, 0xffffff, 0, map, i % 2 ? second : first, {37, 1});
            dispatches.append(dispatch.nsecsElapsed() / 1e6);
            valid &= map.size() == 1 && map[0].size() == 37;
        }
        const double ms = measured.nsecsElapsed() / 1e6;
        const double workEnd = nowSeconds();
        const double renderEndAgeMs = std::max(0.0, double(AudioRenderView::nowNs()
            - std::min(first.publishTimeNs, second.publishTimeNs)) / 1e6);
        const double cpuEnd = jsCpuMs();
        renderTimes.push_back(ms);
        mach_task_basic_info_data_t info{};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        valid &= task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
            reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS;
        valid &= cpuStart >= 0 && cpuEnd >= cpuStart;
        malloc_statistics_t memory{};
        malloc_zone_statistics(malloc_default_zone(), &memory);
        const QJsonObject row{{"kind", "render"}, {"time_s", time}, {"render_ms", ms}, {"blocking_ms", ms},
            {"work_start_s", workStart}, {"work_end_s", workEnd},
            {"actual_start_s", actualStart}, {"deadline_lateness_ms", std::max(0.0, actualStart - time - .003) * 1000},
            {"js_cpu_ms", cpuEnd - cpuStart}, {"snapshot_ms", snapshotMs}, {"dispatch_ms", dispatches},
            {"pending_work", pendingPeak.exchange(0)}, {"pending_observations", pendingSamples.load()},
            {"analysis_backlog", analysisBacklog},
            {"rss_bytes", double(info.resident_size)}, {"allocations", double(memory.blocks_in_use)},
            {"stale_age_ms", std::max(first.staleAgeMs, second.staleAgeMs)},
            {"stale_age_at_render_end_ms", renderEndAgeMs},
            {"fresh_at_render_end", first.available && second.available && renderEndAgeMs <= 250},
            {"audio_frame", double(first.frameSequence)}, {"second_audio_frame", double(second.frameSequence)},
            {"available", first.available && second.available}};
        valid &= trace.write(QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n') > 0;
        if (late)
            deadline = std::chrono::steady_clock::now();
    }
    renderFinished = true;
    });
    while (!renderFinished)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    renderer.join();
    producer.join();
    finished = true;
    observer.join();
    mach_port_deallocate(mach_task_self(), jsThread);
    for (int analysis = 0; analysis < 36000; ++analysis)
    {
        const QJsonObject row{{"kind", "analysis"}, {"frame", analysis},
            {"time_s", (analysis + 1) / 60.0}, {"analysis_ms", analysisTimes[analysis]},
            {"actual_start_s", analysisStarts[analysis]}, {"work_end_s", analysisEnds[analysis]}};
        valid &= trace.write(QJsonDocument(row).toJson(QJsonDocument::Compact) + '\n') > 0;
    }
    for (auto *values : {&analysisTimes, &renderTimes})
    {
        std::sort(values->begin(), values->end());
        qInfo() << (values == &analysisTimes ? "analysis_ms" : "four_render_and_blocking_ms")
                << "p50" << values->at(size_t(values->size() * 0.50))
                << "p95" << values->at(size_t(values->size() * 0.95))
                << "p99" << values->at(size_t(values->size() * 0.99));
    }
    qInfo() << "RUNTIME actual_duration_s" << nowSeconds() << "pending_observations" << pendingSamples.load()
            << "render_rounds" << renderTimes.size() << "missed_rounds" << 30000 - renderTimes.size();
    QVERIFY(valid);
    QCOMPARE(completedAnalysis.load(), 36000);
    QVERIFY(!renderTimes.empty());
    QVERIFY(pendingSamples > 0);
#endif
}

QTEST_MAIN(AudioConsumers_Test)
