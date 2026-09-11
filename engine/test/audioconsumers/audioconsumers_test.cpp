#include <QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSemaphore>
#include <QtEndian>

#include "audioconsumers_test.h"
#include "audioanalyzer.h"
#include "audiochannel.h"
#include "audioframe.h"
#include "audioview.h"
#include "doc.h"
#include "huecolor.h"
#include "huematrix.h"
#include "huescript.h"
#include "huescriptscache.h"
#include "rgbaudio.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <limits>
#include <chrono>
#include <thread>
#include <vector>

#ifdef Q_OS_MACOS
#include <mach/mach.h>
#include <malloc/malloc.h>
#endif

namespace
{
QDir scriptsDirectory()
{
    return QDir(QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("../../../resources/huescripts"));
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
    QCOMPARE(audioFrequency(stale, 2, 3), 0.0);
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

void AudioConsumers_Test::packedScriptContract()
{
    Doc doc(nullptr);
    QVERIFY(doc.hueScriptsCache()->load(scriptsDirectory(), true));
    HUEScript script(&doc);
    QVERIFY(script.load(QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().filePath("snapshot.js")));
    auto snapshot = distinctSnapshot();
    RGBMap map;
    script.rgbMapWithAudio({4, 1}, 0xffffff, 0, map,
        AudioRenderView::fromSnapshot(snapshot, 1000000000), {11, 9});
    QCOMPARE(map.size(), 1);
    QCOMPARE(map[0][0], HUEColor::hsvToRgb(0, 1, float(0.22)));
    QCOMPARE(map[0][1], HUEColor::hsvToRgb(float(1.0 / 3), 1, float(0.4)));
    snapshot.events.beat = 3;
    script.rgbMapWithAudio({4, 1}, 0xffffff, 0, map,
        AudioRenderView::fromSnapshot(snapshot, 1120000000));
    QCOMPARE(qBlue(map[0][2]), qBlue(HUEColor::hsvToRgb(float(2.0 / 3), 1, float(0.3))));
    QCOMPARE(qRed(map[0][3]), qRed(HUEColor::hsvToRgb(0, 0, float(0.12))));
    script.rgbMapWithAudio({4, 1}, 0xffffff, 0, map,
        AudioRenderView::fromSnapshot(snapshot, 1120000000));
    QCOMPARE(qBlue(map[0][2]), 0);
    QCOMPARE(qRed(map[0][3]), 0);
    RGBAudio builtin(&doc);
    builtin.rgbMapWithAudio({5, 8}, 0xff0000, map,
        AudioRenderView::fromSnapshot(snapshot, 1120000000));
    int lit[] = {0, 0, 0, 0, 0};
    for (const auto &row : map)
        for (int x = 0; x < 5; ++x)
            lit[x] += (row[x] & 0xffffff) != 0;
    QCOMPARE(lit[0], 0);
    QCOMPARE(lit[1], 1);
    QCOMPARE(lit[2], 4);
    QCOMPARE(lit[3], 7);
    QCOMPARE(lit[4], 8);
    QCOMPARE(builtin.name(), QString("Audio Spectrum"));
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
