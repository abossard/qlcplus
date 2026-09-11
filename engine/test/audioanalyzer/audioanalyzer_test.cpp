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
