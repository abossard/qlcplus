/*
  Q Light Controller Plus - Unit test
  audioprofile_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <cmath>

#include "audioprofile_test.h"
#include "audioprofile.h"
#include "doc.h"

namespace
{
constexpr double kTolerance = 0.000001;

bool fuzzyEqual(double actual, double expected)
{
    return std::abs(actual - expected) <= kTolerance;
}

void compareConfig(const AudioChannelConfig &actual, const AudioChannelConfig &expected)
{
    QVERIFY(fuzzyEqual(actual.envelope.attackMs, expected.envelope.attackMs));
    QVERIFY(fuzzyEqual(actual.envelope.releaseMs, expected.envelope.releaseMs));

    QVERIFY(fuzzyEqual(actual.agc.maxGainDb, expected.agc.maxGainDb));
    QVERIFY(fuzzyEqual(actual.agc.releaseMs, expected.agc.releaseMs));
    QVERIFY(fuzzyEqual(actual.agc.noiseFloorDb, expected.agc.noiseFloorDb));
    QVERIFY(fuzzyEqual(actual.agc.inputGainLinear, expected.agc.inputGainLinear));
    QCOMPARE(actual.agc.enabled, expected.agc.enabled);

    QVERIFY(fuzzyEqual(actual.triggers.highThreshold, expected.triggers.highThreshold));
    QVERIFY(fuzzyEqual(actual.triggers.lowThreshold, expected.triggers.lowThreshold));
    QVERIFY(fuzzyEqual(actual.triggers.holdMs, expected.triggers.holdMs));
    QVERIFY(fuzzyEqual(actual.triggers.cooldownMs, expected.triggers.cooldownMs));

    QVERIFY(fuzzyEqual(actual.bandLayout.subMaxHz, expected.bandLayout.subMaxHz));
    QVERIFY(fuzzyEqual(actual.bandLayout.bassMaxHz, expected.bandLayout.bassMaxHz));
    QVERIFY(fuzzyEqual(actual.bandLayout.lowMidMaxHz, expected.bandLayout.lowMidMaxHz));
    QVERIFY(fuzzyEqual(actual.bandLayout.midMaxHz, expected.bandLayout.midMaxHz));
    QVERIFY(fuzzyEqual(actual.bandLayout.highMaxHz, expected.bandLayout.highMaxHz));

    QVERIFY(fuzzyEqual(actual.noiseGate.thresholdDb, expected.noiseGate.thresholdDb));
    QVERIFY(fuzzyEqual(actual.noiseGate.holdMs, expected.noiseGate.holdMs));

    QVERIFY(fuzzyEqual(actual.brightnessFloor, expected.brightnessFloor));
    QVERIFY(fuzzyEqual(actual.volumeSmoothingMs, expected.volumeSmoothingMs));
}

AudioChannelConfig nonDefaultConfig()
{
    AudioChannelConfig config = AudioChannelConfig::defaults();
    config.envelope.attackMs = 11.0;
    config.envelope.releaseMs = 222.0;
    config.agc.maxGainDb = 12.0;
    config.agc.releaseMs = 900.0;
    config.agc.noiseFloorDb = -48.0;
    config.agc.inputGainLinear = 2.4;
    config.agc.enabled = false;
    config.triggers.highThreshold = 0.73;
    config.triggers.lowThreshold = 0.31;
    config.triggers.holdMs = 95.0;
    config.triggers.cooldownMs = 175.0;
    config.bandLayout.subMaxHz = 70.0;
    config.bandLayout.bassMaxHz = 280.0;
    config.bandLayout.lowMidMaxHz = 640.0;
    config.bandLayout.midMaxHz = 2400.0;
    config.bandLayout.highMaxHz = 6100.0;
    config.noiseGate.thresholdDb = -62.0;
    config.noiseGate.holdMs = 180.0;
    config.brightnessFloor = 0.22;
    config.volumeSmoothingMs = 75.0;
    return config;
}
}

void AudioProfile_Test::testCreateProfile()
{
    AudioProfile profile(42);
    profile.setName(QStringLiteral("Main Audio"));
    profile.setIsDefault(true);

    QCOMPARE(profile.id(), quint32(42));
    QCOMPARE(profile.name(), QStringLiteral("Main Audio"));
    QCOMPARE(profile.isDefault(), true);
    compareConfig(profile.channelConfig(), AudioChannelConfig::defaults());
}

void AudioProfile_Test::testXmlRoundTrip()
{
    AudioProfile profile(7);
    profile.setName(QStringLiteral("Round Trip"));
    profile.setIsDefault(true);
    const AudioChannelConfig expectedConfig = nonDefaultConfig();
    profile.setChannelConfig(expectedConfig);

    QString xml;
    QXmlStreamWriter writer(&xml);
    QVERIFY(profile.saveXML(&writer));

    QXmlStreamReader reader(xml);
    QVERIFY(reader.readNextStartElement());

    AudioProfile loaded(AudioProfile::invalidId());
    QVERIFY(loaded.loadXML(reader));

    QCOMPARE(loaded.id(), quint32(7));
    QCOMPARE(loaded.name(), QStringLiteral("Round Trip"));
    QCOMPARE(loaded.isDefault(), true);
    compareConfig(loaded.channelConfig(), expectedConfig);
}

void AudioProfile_Test::testLegacyMigration()
{
    const AudioChannelConfig config = AudioProfile::configFromLegacySliders(5, 5, 50, 5);

    QVERIFY(fuzzyEqual(config.agc.inputGainLinear, 1.6));
    const double expectedAttackMs = -40.0 / std::log(1.0 - 0.55);
    QVERIFY(fuzzyEqual(config.envelope.attackMs, expectedAttackMs));
    QVERIFY(fuzzyEqual(config.envelope.releaseMs, 4.0 * expectedAttackMs));
    QVERIFY(fuzzyEqual(config.brightnessFloor, 0.5));
    QVERIFY(fuzzyEqual(config.triggers.highThreshold, 0.25));
    QVERIFY(fuzzyEqual(config.triggers.lowThreshold, 0.05));
}

void AudioProfile_Test::testDocRegistration()
{
    Doc doc(nullptr);

    AudioProfile *first = new AudioProfile(1);
    first->setName(QStringLiteral("First"));
    AudioProfile *second = new AudioProfile(2);
    second->setName(QStringLiteral("Second"));
    second->setIsDefault(true);

    QVERIFY(doc.addAudioProfile(first));
    QVERIFY(doc.addAudioProfile(second));

    QCOMPARE(doc.audioProfile(1), first);
    QCOMPARE(doc.audioProfile(2), second);
    QCOMPARE(doc.defaultAudioProfile(), second);
    QCOMPARE(doc.audioProfiles().size(), 2);
    QVERIFY(doc.audioProfiles().contains(first));
    QVERIFY(doc.audioProfiles().contains(second));
}

void AudioProfile_Test::testEnsureDefault()
{
    Doc doc(nullptr);

    AudioProfile *created = doc.ensureDefaultAudioProfile();
    QVERIFY(created != nullptr);
    QCOMPARE(created->name(), QStringLiteral("Default Audio"));
    QCOMPARE(created->isDefault(), true);
    compareConfig(created->channelConfig(), AudioChannelConfig::defaults());
    QCOMPARE(doc.audioProfiles().size(), 1);
    QCOMPARE(doc.defaultAudioProfile(), created);

    AudioProfile *existing = doc.ensureDefaultAudioProfile();
    QCOMPARE(existing, created);
    QCOMPARE(doc.audioProfiles().size(), 1);
}

void AudioProfile_Test::testVersionValidation_data()
{
    QTest::addColumn<QString>("versionAttr"); // empty string => omit attribute
    QTest::addColumn<bool>("expectLoaded");

    QTest::newRow("missing")  << QString()              << true;
    QTest::newRow("zero")     << QStringLiteral("0")    << true;
    QTest::newRow("one")      << QStringLiteral("1")    << true;
    QTest::newRow("two")      << QStringLiteral("2")    << true;
    QTest::newRow("negative") << QStringLiteral("-1")   << true;
    QTest::newRow("garbage")  << QStringLiteral("abc")  << true;
}

void AudioProfile_Test::testVersionValidation()
{
    QFETCH(QString, versionAttr);
    QFETCH(bool, expectLoaded);

    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement(QStringLiteral("AudioProfile"));
    writer.writeAttribute(QStringLiteral("ID"), QStringLiteral("11"));
    writer.writeAttribute(QStringLiteral("Name"), QStringLiteral("VTest"));
    writer.writeAttribute(QStringLiteral("IsDefault"), QStringLiteral("False"));
    if (!versionAttr.isNull())
        writer.writeAttribute(QStringLiteral("Version"), versionAttr);
    // Write a single Envelope element with a non-default value so we can
    // verify partial parsing still applies in legacy/unknown-version modes.
    writer.writeStartElement(QStringLiteral("Envelope"));
    writer.writeAttribute(QStringLiteral("Attack"), QStringLiteral("33.0"));
    writer.writeEndElement();
    writer.writeEndElement();

    QXmlStreamReader reader(xml);
    QVERIFY(reader.readNextStartElement());

    AudioProfile loaded(AudioProfile::invalidId());
    const bool ok = loaded.loadXML(reader);
    QCOMPARE(ok, expectLoaded);

    if (!ok)
        return;

    QCOMPARE(loaded.id(), quint32(11));
    QCOMPARE(loaded.name(), QStringLiteral("VTest"));

    // Envelope.Attack from XML applies; remaining values fall back to defaults.
    QVERIFY(fuzzyEqual(loaded.channelConfig().envelope.attackMs, 33.0));
    const AudioChannelConfig defaults = AudioChannelConfig::defaults();
    QVERIFY(fuzzyEqual(loaded.channelConfig().envelope.releaseMs, defaults.envelope.releaseMs));
    QVERIFY(fuzzyEqual(loaded.channelConfig().agc.maxGainDb, defaults.agc.maxGainDb));
    QVERIFY(fuzzyEqual(loaded.channelConfig().brightnessFloor, defaults.brightnessFloor));
}

QTEST_APPLESS_MAIN(AudioProfile_Test)
