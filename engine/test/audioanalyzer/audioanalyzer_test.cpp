#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QtEndian>

#include "audioanalyzer_test.h"
#include "audioanalyzer.h"
#include "audiochannel.h"
#include "aubioprocessor.h"
#include "audioframe.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
AudioFrame pcmFrame(const float *samples, uint64_t sequence, uint64_t epoch = 3)
{
    AudioFrame frame;
    frame.samples = samples;
    frame.sampleRate = 30000;
    frame.sampleCount = 500;
    frame.frameIndex = sequence;
    frame.sourceEpoch = epoch;
    frame.sampleTime = (sequence - 1) * 500;
    double square = 0.0, peak = 0.0;
    for (int i = 0; i < 500; ++i)
    {
        square += double(samples[i]) * samples[i];
        peak = std::max(peak, std::abs(double(samples[i])));
    }
    frame.rms = std::sqrt(square / 500.0);
    frame.rmsDb = frame.rms > 0.0 ? 20.0 * std::log10(frame.rms) : -100.0;
    frame.peakDb = peak > 0.0 ? 20.0 * std::log10(peak) : -100.0;
    frame.volumeNorm = std::clamp(1.0 + frame.rmsDb / 100.0, 0.0, 1.0);
    frame.silent = frame.rms == 0.0;
    return frame;
}

std::array<float, 500> tone(double hz, double amplitude, uint64_t offset)
{
    std::array<float, 500> samples;
    for (size_t i = 0; i < samples.size(); ++i)
        samples[i] = float(amplitude * std::sin(2.0 * M_PI * hz * (offset + i) / 30000.0));
    return samples;
}

QJsonArray numbers(const double *data, int count)
{
    QJsonArray out;
    for (int i = 0; i < count; ++i)
        out.append(data[i]);
    return out;
}

QJsonObject bankJson(const AudioSnapshot::MelBankSnapshot &bank)
{
    return {{"centers_hz", numbers(bank.centersHz, bank.count)},
            {"raw_triangles", numbers(bank.raw, bank.count)},
            {"processed", numbers(bank.processed, bank.count)},
            {"novelty", numbers(bank.novelty, bank.count)},
            {"gain", bank.gain > 0.0 ? QJsonValue(bank.gain) : QJsonValue(QJsonValue::Null)}};
}

QByteArray nonPowerDigest(const AubioResults &a, const AudioSnapshot &snapshot)
{
    const QJsonObject fields{
        {"preEmphasis", numbers(a.preEmphasis.data(), int(a.preEmphasis.size()))},
        {"spectrum", numbers(a.spectrum.data(), int(a.spectrum.size()))},
        {"master", numbers(a.mel, AUBIO_MEL_BANDS)},
        {"banks", QJsonArray{bankJson(snapshot.melLow), bankJson(snapshot.melMid), bankJson(snapshot.melHigh)}},
        {"mfcc", numbers(a.mfcc, AUBIO_MFCC_COEFFS)},
        {"descriptors", QJsonArray{a.centroidHz, a.spread, a.rolloffHz, a.flux, a.hfc}},
        {"tssTransient", numbers(a.tssTransientNorm, a.tssBinCount)},
        {"tssSteady", numbers(a.tssSteadyNorm, a.tssBinCount)},
        {"pitch", QJsonArray{a.pitchHz, a.pitchValue, a.pitchConfidence}},
        {"tempo", QJsonArray{a.bpm, a.beatConfidence, a.beatPhase, a.barPhase, a.beatInBar,
                            a.beat, a.tatum, a.tempoValid, a.barWrap}},
        {"onsetDescriptors", numbers(a.onsetDescriptors, AUBIO_ONSET_METHODS)},
        {"onsetThresholded", numbers(a.onsetThresholdedDescriptors, AUBIO_ONSET_METHODS)},
        {"onsets", QJsonArray{a.onsets.energy, a.onsets.hfc, a.onsets.complex, a.onsets.phase,
                             a.onsets.wphase, a.onsets.specdiff, a.onsets.kl, a.onsets.mkl, a.onsets.specflux}},
        {"notes", QJsonArray{a.noteOn, a.noteOff, a.noteMidi, a.noteVelocity}}};
    return QCryptographicHash::hash(QJsonDocument(fields).toJson(QJsonDocument::Compact),
                                    QCryptographicHash::Sha256).toHex();
}
}

void AudioAnalyzer_Test::canonicalProfiles_data()
{
    QTest::addColumn<double>("hz");
    QTest::addColumn<int>("peakIndex");
    QTest::newRow("vocal-formant") << 900.0 << 8;
    QTest::newRow("high-tone") << 6000.0 << 19;
}

void AudioAnalyzer_Test::canonicalProfiles()
{
    QFETCH(double, hz);
    QFETCH(int, peakIndex);
    AudioAnalyzer analyzer;
    auto quiet = AudioChannelConfig::defaults();
    quiet.noiseGate.thresholdDb = -65.0;
    auto closed = quiet;
    closed.noiseGate.thresholdDb = -40.0;
    analyzer.createChannel(quiet, 7);
    analyzer.createChannel(closed, 29);
    for (uint64_t n = 1; n <= 20; ++n)
    {
        const auto samples = tone(hz, 0.001, (n - 1) * 500);
        auto frame = pcmFrame(samples.data(), n);
        analyzer.processFrame(frame);
        QVERIFY(frame.aubio);
        if (n > 1)
            QVERIFY(!analyzer.snapshot(7).noiseGateClosed);
        QVERIFY(analyzer.snapshot(29).noiseGateClosed);
    }
    const auto open = analyzer.snapshot(7);
    const auto gated = analyzer.snapshot(29);
    QVERIFY(open.available);
    QCOMPARE(open.profileId, uint32_t(7));
    QCOMPARE(open.sourceEpoch, uint64_t(3));
    QCOMPARE(open.sampleTime, uint64_t(9500));
    QCOMPARE(open.melHigh.count, 24);
    const int peak = int(std::max_element(open.melHigh.processed,
                                         open.melHigh.processed + 24) - open.melHigh.processed);
    QVERIFY2(std::abs(peak - peakIndex) <= 1, qPrintable(QString::number(peak)));
    QVERIFY(*std::max_element(open.melHigh.processed, open.melHigh.processed + 24) > 0.1);
    QCOMPARE(gated.mids, 0.0);
    QCOMPARE(gated.highs, 0.0);
    QCOMPARE(open.events.beat, uint64_t(0));
}

void AudioAnalyzer_Test::rawDetectorBranch()
{
    AubioProcessor plain, filtered;
    auto config = AudioChannelConfig::defaults().aubio;
    config.preEmphasisEnabled = false;
    plain.setPendingConfig(config);
    config.preEmphasisEnabled = true;
    filtered.setPendingConfig(config);
    plain.initialize(30000);
    filtered.initialize(30000);
    double spectralDifference = 0.0;
    for (int n = 0; n < 40; ++n)
    {
        const auto samples = tone(440.0, 0.2, uint64_t(n) * 500);
        plain.process(samples.data(), 500, true);
        filtered.process(samples.data(), 500, true);
        QCOMPARE(plain.results().pitchHz, filtered.results().pitchHz);
        QCOMPARE(plain.results().onsetDescriptors[1], filtered.results().onsetDescriptors[1]);
        spectralDifference += std::abs(plain.results().melHigh[4] - filtered.results().melHigh[4]);
    }
    QVERIFY(spectralDifference > 1e-6);
    QVERIFY(plain.results().pitchHz > 400.0 && plain.results().pitchHz < 480.0);
}

void AudioAnalyzer_Test::silenceFreezesSpectrum()
{
    AudioAnalyzer continuous, paused;
    const std::array<float, 500> zeros{};
    for (uint64_t n = 1; n <= 12; ++n)
    {
        const auto samples = tone(1100.0, 0.1, (n - 1) * 500);
        auto a = pcmFrame(samples.data(), n);
        auto b = a;
        continuous.processFrame(a);
        paused.processFrame(b);
    }
    for (uint64_t n = 13; n <= 1812; ++n)
    {
        auto frame = pcmFrame(zeros.data(), n);
        paused.processFrame(frame);
        QCOMPARE(paused.snapshot().melHigh.processed[8], 0.0);
    }
    const auto samples = tone(1100.0, 0.1, 6000);
    auto a = pcmFrame(samples.data(), 13);
    auto b = pcmFrame(samples.data(), 1813);
    continuous.processFrame(a);
    paused.processFrame(b);
    const auto expected = continuous.snapshot();
    const auto resumed = paused.snapshot();
    for (int i = 0; i < expected.melHigh.count; ++i)
    {
        QCOMPARE(resumed.melHigh.raw[i], expected.melHigh.raw[i]);
        QCOMPARE(resumed.melHigh.processed[i], expected.melHigh.processed[i]);
        QCOMPARE(resumed.melHigh.novelty[i], expected.melHigh.novelty[i]);
    }
}

void AudioAnalyzer_Test::profileRevisionAndRetirement()
{
    AudioAnalyzer analyzer;
    auto config = AudioChannelConfig::defaults();
    auto *first = analyzer.createChannel(config, 7);
    auto *second = analyzer.createChannel(config, 29);
    const auto samples = tone(1500.0, 0.1, 0);
    auto frame = pcmFrame(samples.data(), 1);
    analyzer.processFrame(frame);
    const auto before = analyzer.snapshot(7);
    config.aubio.melBanks.high = {50.0, 8000.0, 13, {}};
    first->updateConfig(config);
    QCOMPARE(analyzer.snapshot(7).configRevision, before.configRevision);
    frame.frameIndex = 2;
    analyzer.processFrame(frame);
    const auto applied = analyzer.snapshot(7);
    QVERIFY(applied.configRevision > before.configRevision);
    QCOMPARE(applied.melHigh.count, 13);
    QCOMPARE(applied.melHigh.minHz, 50.0);
    QCOMPARE(applied.melHigh.maxHz, 8000.0);
    QCOMPARE(analyzer.snapshot(29).melHigh.count, 24);
    QVERIFY(applied.melHigh.centersHz[12] > applied.melHigh.centersHz[0]);
    analyzer.invalidate(4);
    QVERIFY(!analyzer.snapshot(7).available);
    QCOMPARE(analyzer.snapshot(7).events.beat, uint64_t(0));
    QCOMPARE(analyzer.snapshot(7).sourceEpoch, uint64_t(4));
    AudioSnapshot external;
    external.available = true;
    external.mids = 0.731;
    second->setExternalSource(true);
    second->injectSnapshot(external);
    frame.sourceEpoch = 4;
    analyzer.processFrame(frame);
    QCOMPARE(analyzer.snapshot(29).mids, 0.731);
    analyzer.destroyChannel(first);
    QVERIFY(!analyzer.snapshot(7).available);
    QVERIFY(!analyzer.snapshot(first).available);
}

void AudioAnalyzer_Test::powerWindow_data()
{
    QTest::addColumn<int>("window");
    QTest::addColumn<bool>("diagnostics");
    QTest::addColumn<double>("hz");
    for (int window : {4096, 2048})
        for (bool diagnostics : {false, true})
            for (double hz : {60.0, 170.0, 900.0, 6000.0})
                QTest::newRow(qPrintable(QString("%1-%2-%3").arg(window).arg(diagnostics).arg(hz)))
                    << window << diagnostics << hz;
}

void AudioAnalyzer_Test::powerWindow()
{
    QFETCH(int, window);
    QFETCH(bool, diagnostics);
    QFETCH(double, hz);
    auto config = AudioChannelConfig::defaults();
    config.aubio.diagnosticsEnabled = diagnostics;
    AudioAnalyzer analyzer;
    auto *ordinary = analyzer.createChannel(config, 7);
    config.aubio.powerWindowSize = window;
    auto *selected = analyzer.createChannel(config, 29);
    QVERIFY(ordinary);
    QVERIFY(selected);
    for (uint64_t n = 1; n <= 60; ++n)
    {
        if (n == 31)
        {
            auto reduced = ordinary->config();
            reduced.aubio.melBanks.high.bands = 13;
            ordinary->updateConfig(reduced);
            reduced.aubio.powerWindowSize = window;
            selected->updateConfig(reduced);
        }
        const auto samples = tone(hz, 0.2, (n - 1) * 500);
        auto frame = pcmFrame(samples.data(), n);
        analyzer.processFrame(frame);
        const auto a = ordinary->aubioResults();
        const auto b = selected->aubioResults();
        QCOMPARE(b.powerWindowSize, window);
        QCOMPARE(b.powerBinCount, window / 2 + 1);
        QCOMPARE(b.powerMelCount, window == 2048 ? b.melHighCount : 0);
        QCOMPARE(b.spectrum.size(), diagnostics ? size_t(2049) : size_t(0));
        QCOMPARE(b.tssBinCount, diagnostics && !selected->snapshot().noiseGateClosed ? 2049 : 0);
        QCOMPARE(b.preEmphasis, a.preEmphasis);
        QCOMPARE(b.spectrum, a.spectrum);
        QCOMPARE(b.centroidHz, a.centroidHz);
        QCOMPARE(b.spread, a.spread);
        QCOMPARE(b.rolloffHz, a.rolloffHz);
        QCOMPARE(b.flux, a.flux);
        QCOMPARE(b.hfc, a.hfc);
        for (int i = 0; i < AUBIO_MFCC_COEFFS; ++i)
            QCOMPARE(b.mfcc[i], a.mfcc[i]);
        for (int i = 0; i < 2049; ++i)
        {
            QCOMPARE(b.tssTransientNorm[i], a.tssTransientNorm[i]);
            QCOMPARE(b.tssSteadyNorm[i], a.tssSteadyNorm[i]);
        }
        const auto sa = ordinary->snapshot();
        const auto sb = selected->snapshot();
        const AudioSnapshot::MelBankSnapshot *banksA[] = {&sa.melLow, &sa.melMid, &sa.melHigh};
        const AudioSnapshot::MelBankSnapshot *banksB[] = {&sb.melLow, &sb.melMid, &sb.melHigh};
        for (int bank = 0; bank < 3; ++bank)
            for (int i = 0; i < kMaxMelBands; ++i)
            {
                QCOMPARE(banksB[bank]->raw[i], banksA[bank]->raw[i]);
                QCOMPARE(banksB[bank]->processed[i], banksA[bank]->processed[i]);
                QCOMPARE(banksB[bank]->novelty[i], banksA[bank]->novelty[i]);
                QCOMPARE(banksB[bank]->centersHz[i], banksA[bank]->centersHz[i]);
                QVERIFY(std::isfinite(b.powerMel[i]));
                if (i >= b.powerMelCount)
                    QCOMPARE(b.powerMel[i], 0.0);
            }
        for (double power : sb.powersRaw)
            QVERIFY(std::isfinite(power) && power >= 0.0 && power <= 1.0);
        QCOMPARE(sb.sampleTime, (n - 1) * 500);
        QCOMPARE(sb.sourceEpoch, uint64_t(3));
    }
    const auto snap = selected->snapshot();
    QVERIFY(*std::max_element(snap.melHigh.raw, snap.melHigh.raw + snap.melHigh.count) > 0.0);
}

void AudioAnalyzer_Test::powerResponse_data()
{
    QTest::addColumn<bool>("descending");
    QTest::addColumn<bool>("diagnostics");
    for (bool descending : {true, false})
        for (bool diagnostics : {false, true})
            QTest::newRow(qPrintable(QString("%1-diag%2")
                .arg(descending ? "descending-kick" : "simultaneous-tones").arg(diagnostics)))
                << descending << diagnostics;
}

void AudioAnalyzer_Test::powerResponse()
{
    QFETCH(bool, descending);
    QFETCH(bool, diagnostics);
    AudioAnalyzer analyzer;
    auto config = AudioChannelConfig::defaults();
    config.aubio.diagnosticsEnabled = diagnostics;
    auto *ordinary = analyzer.createChannel(config, 7);
    config.aubio.powerWindowSize = 2048;
    auto *selected = analyzer.createChannel(config, 29);
    QVERIFY(ordinary);
    QVERIFY(selected);
    QCOMPARE(selected->config().freqPower, ordinary->config().freqPower);
    QCOMPARE(selected->config().aubio.melBanks, ordinary->config().aubio.melBanks);
    QJsonArray records;
    std::array<std::array<std::vector<double>, 4>, 2> traces;
    QCryptographicHash pcmHash(QCryptographicHash::Sha256);
    // Two seconds of silence, then a fixed 180 -> 50 Hz descending kick
    // (45-ms frequency decay), or simultaneous 60 + 170 Hz low tones.
    // Both use the same 120-ms amplitude decay, with no profile-specific PCM.
    for (uint64_t n = 0; n < 180; ++n)
    {
        std::array<float, 500> samples{};
        if (n >= 120)
            for (size_t i = 0; i < samples.size(); ++i)
            {
                const double t = double((n - 120) * 500 + i) / 30000.0;
                const double phase = 2.0 * M_PI *
                    (50.0 * t + 130.0 * 0.045 * (1.0 - std::exp(-t / 0.045)));
                samples[i] = float(std::exp(-t / 0.12) * (descending
                    ? 0.6 * std::sin(phase)
                    : 0.3 * (std::sin(2.0 * M_PI * 60.0 * t) + std::sin(2.0 * M_PI * 170.0 * t))));
            }
        pcmHash.addData(QByteArrayView(reinterpret_cast<const char *>(samples.data()),
                                      sizeof(samples)));
        auto frame = pcmFrame(samples.data(), n + 1);
        analyzer.processFrame(frame);
        if (n < 120)
            continue;
        QJsonArray profiles;
        for (int profile = 0; profile < 2; ++profile)
        {
            const auto snapshot = analyzer.snapshot(profile == 0 ? 7 : 29);
            const double values[] = {snapshot.powersRaw[0], snapshot.powersRaw[1],
                                     snapshot.beatPower, snapshot.bassPower};
            for (int k = 0; k < 4; ++k)
            {
                QVERIFY(std::isfinite(values[k]));
                traces[profile][k].push_back(values[k]);
            }
            profiles.append(numbers(values, 4));
        }
        const auto defaultDigest = nonPowerDigest(ordinary->aubioResults(), ordinary->snapshot());
        QCOMPARE(nonPowerDigest(selected->aubioResults(), selected->snapshot()), defaultDigest);
        records.append(QJsonObject{{"sample_end", double((n - 119) * 500)}, {"profiles", profiles},
            {"default_non_power_sha256", QString::fromLatin1(defaultDigest)}});
    }
    int crossings[2][4] = {};
    const char *labels[] = {"raw-kick", "raw-bass", "filtered-kick", "filtered-bass"};
    for (int profile = 0; profile < 2; ++profile)
    {
        for (int k = 0; k < 4; ++k)
        {
            const auto &trace = traces[profile][k];
            const auto peak = std::max_element(trace.begin(), trace.end());
            const int first = int(std::find_if(trace.begin(), trace.end(),
                [](double v) { return v > 1e-6; }) - trace.begin()) + 1;
            const int crossing = int(std::find_if(trace.begin(), trace.end(),
                [&](double v) { return v >= *peak * 0.5; }) - trace.begin()) + 1;
            crossings[profile][k] = crossing;
            qInfo().nospace() << (profile == 0 ? "Default " : "2048 power ") << labels[k]
                << " first_ms=" << first * 500.0 / 30.0
                << " half_ms=" << crossing * 500.0 / 30.0
                << " peak_ms=" << (peak - trace.begin() + 1) * 500.0 / 30.0
                << " peak=" << *peak;
            QVERIFY2(*peak > 0.01, "Power response must remain useful, not just a normalized vanished signal");
        }
        qInfo() << "Kick/Bass half-crossing lag_ms" << profile
                << (crossings[profile][2] - crossings[profile][3]) * 500.0 / 30.0;
    }
    qInfo() << "PCM SHA256" << pcmHash.result().toHex();
    const QString output = qEnvironmentVariable("QLC_AUDIO_RESPONSE_OUTPUT");
    if (!output.isEmpty())
    {
        QVERIFY(QDir().mkpath(output));
        QFile file(QDir(output).filePath(QString::fromLatin1(QTest::currentDataTag()) + ".json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(QJsonDocument(QJsonObject{
            {"pcm_sha256", QString::fromLatin1(pcmHash.result().toHex())},
            {"rate", 30000}, {"hop", 500}, {"records", records}}).toJson()) > 0);
    }
    if (descending)
        QVERIFY2(crossings[1][2] <= crossings[0][2] - 1,
                 "Descending kick must improve by at least one 500-sample hop");
    else
        QVERIFY2(crossings[1][2] <= crossings[0][2],
                 "Simultaneous low tones must not cross later");
}

void AudioAnalyzer_Test::rawDetectorsPreserved_data()
{
    QTest::addColumn<QString>("unit");
    QTest::addColumn<bool>("diagnostics");
    for (const QString &unit : {QString("midi"), QString("bin")})
        for (bool diagnostics : {false, true})
            QTest::newRow(qPrintable(unit + (diagnostics ? "-diagnostics" : "-ordinary")))
                << unit << diagnostics;
}

void AudioAnalyzer_Test::rawDetectorsPreserved()
{
    QFETCH(QString, unit);
    QFETCH(bool, diagnostics);
    auto config = AudioChannelConfig::defaults().aubio;
    config.pitchUnit = unit;
    config.diagnosticsEnabled = diagnostics;
    AubioProcessor ordinary, shortPower;
    ordinary.setPendingConfig(config);
    config.powerWindowSize = 2048;
    shortPower.setPendingConfig(config);
    ordinary.initialize(30000);
    shortPower.initialize(30000);
    int beats = 0, onsets = 0, notes = 0, pitches = 0;
    for (int n = 0; n < 1500; ++n)
    {
        if (n == 180)
        {
            std::fill_n(config.onsetMethodEnabled, 9, true);
            shortPower.setPendingConfig(config);
            config.powerWindowSize = 4096;
            ordinary.setPendingConfig(config);
        }
        std::array<float, 500> samples{};
        for (size_t i = 0; i < samples.size(); ++i)
        {
            const double t = double(n * 500 + i) / 30000.0;
            const double local = std::fmod(t, 0.5);
            samples[i] = float(n < 300 ? 0.2 * std::sin(2 * M_PI * 440 * t)
                : (local < 0.35 ? 0.25 * std::sin(2 * M_PI * 440 * t) * std::exp(-local / 0.2) : 0.0)
                + 0.5 * std::exp(-local / 0.045) * std::sin(2 * M_PI * 70 * t));
        }
        ordinary.process(samples.data(), 500);
        shortPower.process(samples.data(), 500);
        const auto &a = ordinary.results();
        const auto &b = shortPower.results();
        QCOMPARE(b.pitchValue, a.pitchValue);
        QCOMPARE(b.pitchHz, a.pitchHz);
        QCOMPARE(b.pitchConfidence, a.pitchConfidence);
        QCOMPARE(b.bpm, a.bpm);
        QCOMPARE(b.beatConfidence, a.beatConfidence);
        QCOMPARE(b.beat, a.beat);
        QCOMPARE(b.tatum, a.tatum);
        QCOMPARE(b.beatPhase, a.beatPhase);
        QCOMPARE(b.barPhase, a.barPhase);
        QCOMPARE(b.noteMidi, a.noteMidi);
        QCOMPARE(b.noteVelocity, a.noteVelocity);
        QCOMPARE(b.noteOn, a.noteOn);
        QCOMPARE(b.noteOff, a.noteOff);
        const bool eventsA[] = {a.onsets.energy, a.onsets.hfc, a.onsets.complex, a.onsets.phase,
            a.onsets.wphase, a.onsets.specdiff, a.onsets.kl, a.onsets.mkl, a.onsets.specflux};
        const bool eventsB[] = {b.onsets.energy, b.onsets.hfc, b.onsets.complex, b.onsets.phase,
            b.onsets.wphase, b.onsets.specdiff, b.onsets.kl, b.onsets.mkl, b.onsets.specflux};
        for (int i = 0; i < 9; ++i)
        {
            QCOMPARE(eventsB[i], eventsA[i]);
            QCOMPARE(b.onsetDescriptors[i], a.onsetDescriptors[i]);
            QCOMPARE(b.onsetThresholdedDescriptors[i], a.onsetThresholdedDescriptors[i]);
            onsets += eventsA[i];
        }
        pitches += a.pitchHz > 0.0;
        notes += a.noteOn;
        beats += a.beat;
    }
    qInfo() << "identical per-hop raw detectors: pitches" << pitches << "beats" << beats
            << "onsets" << onsets << "notes" << notes;
    QVERIFY(pitches > 0);
    QVERIFY(beats > 0);
    QVERIFY(onsets > 0);
    QVERIFY(!diagnostics || notes > 0);
    QCOMPARE(AubioProcessor::windowSize(), uint32_t(4096));
    QCOMPARE(AubioProcessor::hopSize(), uint32_t(500));
}

void AudioAnalyzer_Test::powerReconfiguration()
{
    AudioAnalyzer analyzer;
    auto *channel = analyzer.defaultChannel();
    auto config = AudioChannelConfig::defaults();
    config.aubio.diagnosticsEnabled = true;
    uint64_t n = 0;
    for (int window : {4096, 2048, 4096, 2048})
    {
        config.aubio.powerWindowSize = window;
        config.aubio.melBanks.high.bands = n ? 13 : 24;
        channel->updateConfig(config);
        for (int i = 0; i < 20; ++i)
        {
            auto samples = tone(170, 0.2, n * 500);
            auto frame = pcmFrame(samples.data(), ++n);
            analyzer.processFrame(frame);
        }
        const auto result = channel->aubioResults();
        QCOMPARE(result.powerWindowSize, window);
        QCOMPARE(result.powerBinCount, window / 2 + 1);
        QCOMPARE(result.spectrum.size(), size_t(2049));
        QCOMPARE(result.tssBinCount, 2049);
        for (int i = result.powerMelCount; i < kMaxMelBands; ++i)
            QCOMPARE(result.powerMel[i], 0.0);
    }
    auto invalid = config;
    invalid.aubio.powerWindowSize = 1024;
    QVERIFY(!invalid.validationError().isEmpty());
    channel->updateConfig(invalid);
    QCOMPARE(channel->config().aubio.powerWindowSize, 2048);
    QVERIFY(!analyzer.createChannel(invalid, 7));
}

void AudioAnalyzer_Test::dumpCorpus()
{
    const QString input = qEnvironmentVariable("QLC_AUDIO_CORPUS_INPUT");
    if (input.isEmpty())
        QSKIP("Set QLC_AUDIO_CORPUS_INPUT and QLC_AUDIO_CORPUS_OUTPUT for reference replay");
    QFile source(input);
    QVERIFY(source.open(QIODevice::ReadOnly));
    const auto fixtures = QJsonDocument::fromJson(source.readAll()).object().value("fixtures").toArray();
    QVERIFY(!fixtures.isEmpty());
    const QDir sourceDirectory(QFileInfo(source).absolutePath());
    const QDir outputDirectory(qEnvironmentVariable("QLC_AUDIO_CORPUS_OUTPUT"));
    QVERIFY(!qEnvironmentVariable("QLC_AUDIO_CORPUS_OUTPUT").isEmpty());
    QVERIFY(QDir().mkpath(outputDirectory.absolutePath()));
    const QStringList selected = qEnvironmentVariable("QLC_AUDIO_CORPUS_FIXTURE").split(',', Qt::SkipEmptyParts);
    int exported = 0;
    for (const auto &value : fixtures)
    {
        const auto fixture = value.toObject();
        const QString name = fixture.value("id").toString();
        if (!selected.isEmpty() && !selected.contains(name))
            continue;
        QFile pcmFile(sourceDirectory.filePath(fixture.value("pcm").toString()));
        QVERIFY(pcmFile.open(QIODevice::ReadOnly));
        const QByteArray pcm = pcmFile.readAll();
        QVERIFY(!pcm.isEmpty() && pcm.size() % 2000 == 0);
        QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(pcm, QCryptographicHash::Sha256).toHex()),
                 fixture.value("pcm_sha256").toString());
        AudioAnalyzer analyzer;
        auto config = AudioChannelConfig::defaults();
        config.aubio.diagnosticsEnabled = true;
        if (fixture.value("config").toObject().contains("gateDb"))
            config.noiseGate.thresholdDb = fixture.value("config").toObject().value("gateDb").toDouble();
        analyzer.defaultChannel()->updateConfig(config);
        AubioProcessor coefficients;
        coefficients.setPendingConfig(config.aubio);
        coefficients.initialize(30000);
        const std::array<float, 500> zeros{};
        coefficients.process(zeros.data(), 500, false);
        QJsonArray matrices;
        for (int bank = 0; bank < 3; ++bank)
        {
            QJsonArray rows;
            for (const auto &row : coefficients.bankCoefficients(bank))
                rows.append(numbers(row.data(), int(row.size())));
            matrices.append(rows);
        }
        QJsonArray indices;
        const double cutoffs[] = {config.freqPower.beat.maxHz, config.freqPower.bass.maxHz,
                                 config.freqPower.mids.maxHz, config.freqPower.high.maxHz};
        for (double cutoff : cutoffs)
        {
            const auto &a = coefficients.results();
            indices.append(int(std::upper_bound(a.melHighCenters,
                a.melHighCenters + a.melHighCount, cutoff) - a.melHighCenters));
        }
        const auto &a = coefficients.results();
        const int kickStop = std::max(1, int(std::upper_bound(a.melLowCenters,
            a.melLowCenters + a.melLowCount, config.kick.beatMaxHz) - a.melLowCenters) - 1);
        QFile sink(outputDirectory.filePath(name + ".jsonl"));
        QVERIFY(sink.open(QIODevice::WriteOnly));
        const QJsonObject header{
            {"kind", "header"}, {"schema", "qlc-audio-reference-v1"},
            {"producer", "production-qlc-cpp"}, {"fixture", name},
            {"pcm_sha256", fixture.value("pcm_sha256")},
            {"frame_count", double(pcm.size() / 2000)}, {"segments", fixture.value("segments")},
            {"coefficients", matrices}, {"power_indices", indices}, {"kick_stop_index", kickStop},
            {"event_alignment_frames", 0}, {"phase_alignment_frames", 0}, {"effects", QJsonArray{}}};
        QVERIFY(sink.write(QJsonDocument(header).toJson(QJsonDocument::Compact) + '\n') > 0);
        for (qsizetype offset = 0; offset < pcm.size(); offset += 2000)
        {
            std::array<float, 500> samples;
            for (int i = 0; i < 500; ++i)
            {
                const uint32_t bits = qFromLittleEndian<uint32_t>(
                    reinterpret_cast<const uchar *>(pcm.constData() + offset + i * 4));
                std::memcpy(&samples[i], &bits, sizeof(float));
            }
            auto frame = pcmFrame(samples.data(), uint64_t(offset / 2000 + 1));
            analyzer.processFrame(frame);
            const auto snap = analyzer.snapshot();
            QJsonArray powers{snap.beatPower, snap.bassPower, snap.mids, snap.highs};
            const QJsonObject record{
                {"kind", "frame"}, {"fixture", name}, {"frame", double(offset / 2000)},
                {"sample_end", double(snap.sampleTime + 500)},
                {"time_s", double(snap.sampleTime + 500) / 30000.0},
                {"gate", !snap.noiseGateClosed},
                {"volume_raw", frame.volumeNorm}, {"volume_filtered", snap.volume.volumeNorm},
                {"raw_rms", frame.rms},
                {"preemphasis", snap.noiseGateClosed ? QJsonValue(QJsonValue::Null)
                    : QJsonValue(numbers(frame.aubio->preEmphasis.data(), int(frame.aubio->preEmphasis.size())))},
                {"fft", numbers(frame.aubio->spectrum.data(), int(frame.aubio->spectrum.size()))},
                {"banks", QJsonArray{bankJson(snap.melLow), bankJson(snap.melMid), bankJson(snap.melHigh)}},
                {"powers_raw", numbers(snap.powersRaw, 4)}, {"powers", powers},
                {"pitch_midi", frame.aubio->pitchValue}, {"tempo_bpm", snap.music.bpm},
                {"tempo_confidence", snap.music.beatConfidence},
                {"beat_phase", snap.music.beatPhase}, {"bar_phase", snap.music.barPhase},
                {"events", QJsonObject{{"onset", double(snap.events.onset)}, {"tempo", double(snap.events.beat)},
                                      {"kick", double(snap.events.kick)}, {"bar_wrap", double(snap.events.bar)}}},
                {"effects", QJsonArray{}}};
            QVERIFY(sink.write(QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n') > 0);
        }
        ++exported;
    }
    QVERIFY(exported > 0);
}

QTEST_MAIN(AudioAnalyzer_Test)
