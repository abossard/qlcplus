#include <QtTest>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "audioprofile_test.h"
#include "audioprofile.h"
#include "audioanalyzer.h"
#include "audiochannel.h"
#include "audioframe.h"

#include <array>
#include <cmath>

void AudioProfile_Test::versionTwoRoundTrip_data()
{
    QTest::addColumn<quint32>("id");
    QTest::addColumn<int>("source");
    QTest::addColumn<QString>("revision");
    QTest::newRow("legacy-microphone-7") << quint32(7) << 0 << QString();
    QTest::newRow("legacy-osc-29") << quint32(29) << 1 << QString();
    QTest::newRow("canonical-osc-29") << quint32(29) << 1 << QString(" AnalysisContractRevision=\"3\"");
}

void AudioProfile_Test::versionTwoRoundTrip()
{
    QFETCH(quint32, id);
    QFETCH(int, source);
    QFETCH(QString, revision);
    const QString xml = QString(R"(<AudioProfile ID="%1" Name="Unequal tuning" IsDefault="True"
        Version="2" AudioSource="%2" OscPort="12029"%3>
      <Envelope Attack="23.5" Release="173.75"/>
      <NoiseGate Threshold="%4" Hold="27"/>
      <Triggers><Band Name="low" High="0.39" Low="0.21" Hold="117" Cooldown="213"/>
        <Band Name="mid" High="0.58" Low="0.34" Hold="73" Cooldown="111"/>
        <Band Name="high" High="0.79" Low="0.57" Hold="43" Cooldown="87"/></Triggers>
      <Kick Enabled="1" BeatMaxHz="117" BeatMinPercentDiff="0.37" BeatMinAmplitude="0.64"
        BeatRefractorySec="0.17" BeatHistoryLen="19"/>
      <MelPost Enabled="0" PowerFactor="1.375" GaussianSigma="2.25" SmoothDecay="0.61"
        SmoothRise="0.91" CommonDecay="0.82" CommonRise="0.13" DiffDecay="0.21"
        DiffRise="0.72" AgcDecay="0.07" AgcRise="0.84"/>
      <Volume Smoothing="84.5" BrightnessFloor="0.19"/>
      <FreqPower><Band Name="beat" MaxHz="90" Decay="0.23" Rise="0.73"/>
        <Band Name="bass" MaxHz="310" Decay="0.17" Rise="0.87"/>
        <Band Name="mids" MaxHz="2800" Decay="0.41" Rise="0.81"/>
        <Band Name="high" MaxHz="9200" Decay="0.34" Rise="0.94"/></FreqPower>
      <Aubio PitchMethod="yinfft" PitchUnit="Hz" PitchSilenceDb="-58" PitchTolerance="0.83"
        FilterbankNorm="0" FilterbankPower="1.4" TempoMethod="default" TempoSilenceDb="-72"
        TempoThreshold="0.47" TempoDelayMs="11" TatumSubdivision="2" BeatsPerBar="3"
        PreEmphasisEnabled="0" WindowType="hanning" MelScale="htk"
        OnsetMethodsEnabled="010000001" OnsetMethodIndex="8"
        NoteSilenceDb="-59" NoteMinIntervalMs="63" NoteReleaseDropDb="8"
        MfccPower="1.3" MfccScale="0.7" TssAlpha="2.2" TssBeta="2.7" TssThreshold="0.31"
        CoastBeats="5" TempoDecayHalfLifeBeats="1.7" TempoDecayTargetBpm="48">
        <OnsetOverride Method="hfc" Threshold="0.29123456789" Silence="-64" Minioi="47"
          Delay="23" Compression="3.5" Awhitening="False"/>
        <OnsetOverride Method="specflux" Threshold="0.43" Silence="-51" Minioi="81"
          Delay="37" Compression="7.5" Awhitening="True"/>
        <MelBank Role="low" MinHz="27" MaxHz="370" Bands="13" Preset="Custom"
          PowerFactor="1.35" GaussianSigma="1.2" SmoothDecay="0.51" SmoothRise="0.87"
          CommonDecay="0.77" CommonRise="0.04" DiffDecay="0.26" DiffRise="0.76"
          AgcDecay="0.03" AgcRise="0.92" Enabled="1"/>
        <MelBank Role="mid" MinHz="31" MaxHz="2300" Bands="21" PowerFactor="1.65"
          GaussianSigma="1.7" AgcDecay="0.08" AgcRise="0.83" Enabled="0"/>
        <MelBank Role="high" MinHz="45" MaxHz="13000" Bands="%5" PowerFactor="2.15"
          GaussianSigma="2.3" AgcDecay="0.12" AgcRise="0.71" Enabled="1"/>
      </Aubio>
    </AudioProfile>)").arg(id).arg(source).arg(revision).arg(id == 7 ? -65 : -72).arg(id == 7 ? 29 : 17);
    AudioProfile original(0);
    QXmlStreamReader reader(xml);
    QVERIFY(reader.readNextStartElement());
    QVERIFY(original.loadXML(reader));
    QCOMPARE(original.id(), id);
    QCOMPARE(original.audioSource(), source);
    QCOMPARE(original.oscPort(), quint16(12029));
    QCOMPARE(original.channelConfig().aubio.melBanks.low.bands, 13);
    QCOMPARE(original.channelConfig().aubio.melBanks.mid.bands, 21);
    QCOMPARE(original.channelConfig().aubio.melBanks.high.bands, id == 7 ? 29 : 17);
    const auto a = original.channelConfig();
    QString written;
    QXmlStreamWriter writer(&written);
    QVERIFY(original.saveXML(&writer));
    QVERIFY(written.contains("AnalysisContractRevision=\"3\""));
    AudioProfile restored(1);
    QXmlStreamReader rereader(written);
    QVERIFY(rereader.readNextStartElement());
    QVERIFY(restored.loadXML(rereader));
    QString rewritten;
    QXmlStreamWriter rewriter(&rewritten);
    QVERIFY(restored.saveXML(&rewriter));
    QCOMPARE(rewritten, written);
    QCOMPARE(restored.id(), id);
    QCOMPARE(restored.audioSource(), source);
    QCOMPARE(restored.oscPort(), original.oscPort());
    const auto b = restored.channelConfig();
    QVERIFY(a.aubio.melBanks == b.aubio.melBanks);
    QVERIFY(a.melPost == b.melPost);
    QVERIFY(a.freqPower == b.freqPower);
    QCOMPARE(b.aubio.onsetOverrides[1].compression, 3.5);
    QCOMPARE(b.aubio.onsetOverrides[1].threshold, 0.29123456789);
    QCOMPARE(b.aubio.onsetOverrides[8].compression, 7.5);
    QCOMPARE(b.aubio.onsetOverrides[1].awhitening, 0);
    QCOMPARE(b.aubio.onsetOverrides[8].awhitening, 1);
    QCOMPARE(b.aubio.coastBeats, 5.0);
    QCOMPARE(b.aubio.tempoDecayHalfLifeBeats, 1.7);
    QCOMPARE(b.aubio.tempoDecayTargetBpm, 48.0);
    QCOMPARE(b.noiseGate.thresholdDb, id == 7 ? -65.0 : -72.0);
    QCOMPARE(b.triggers.low.highThreshold, 0.39);
    QCOMPARE(b.triggers.mid.lowThreshold, 0.34);
    QCOMPARE(b.triggers.high.cooldownMs, 87.0);
    QCOMPARE(b.envelope.attackMs, 23.5);
    QCOMPARE(b.envelope.releaseMs, 173.75);
    QCOMPARE(b.volumeSmoothingMs, 84.5);
    QCOMPARE(b.brightnessFloor, 0.19);
    QCOMPARE(b.aubio.pitchTolerance, 0.83);
    QCOMPARE(b.aubio.filterbankNorm, 0.0);
    QCOMPARE(b.aubio.filterbankPower, 1.4);
    QCOMPARE(b.aubio.tempoThreshold, 0.47);
    QCOMPARE(b.aubio.tempoDelayMs, 11.0);
    QCOMPARE(b.aubio.beatsPerBar, 3);
    QCOMPARE(b.aubio.tatumSubdivision, 2);
    QCOMPARE(b.aubio.preEmphasisEnabled, false);
    QCOMPARE(b.aubio.mfccPower, 1.3);
    QCOMPARE(b.aubio.mfccScale, 0.7);
    QCOMPARE(b.aubio.tssThreshold, 0.31);
    QCOMPARE(b.aubio.noteMinIntervalMs, 63.0);
    QCOMPARE(b.aubio.noteReleaseDropDb, 8.0);
    QVERIFY(!restored.migrationWarning().isEmpty());
}

void AudioProfile_Test::profileBinding()
{
    AudioAnalyzer analyzer;
    AudioProfile microphone(7), external(29);
    external.setAudioSource(AudioProfile::OscSynesthesia);
    microphone.bindAnalyzer(&analyzer);
    external.bindAnalyzer(&analyzer);
    std::array<float, 500> samples;
    for (int i = 0; i < 500; ++i)
        samples[i] = float(0.1 * std::sin(2.0 * M_PI * i / 30.0));
    AudioFrame frame;
    frame.samples = samples.data();
    frame.rms = 0.07;
    frame.rmsDb = -23.0;
    frame.volumeNorm = 0.77;
    frame.sourceEpoch = 5;
    analyzer.processFrame(frame);
    QCOMPARE(analyzer.snapshot(7).profileId, uint32_t(7));
    QVERIFY(!analyzer.snapshot(29).available);
    AudioSnapshot osc;
    osc.mids = 0.83;
    external.channel()->injectSnapshot(osc);
    analyzer.processFrame(frame);
    QCOMPARE(analyzer.snapshot(29).mids, 0.83);
    microphone.releaseAnalyzer();
    QVERIFY(!analyzer.snapshot(7).available);
}

QTEST_MAIN(AudioProfile_Test)
