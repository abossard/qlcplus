/*
  Q Light Controller Plus - Unit test
  conversions_test.cpp

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <cmath>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QJSValue>
#include "conversions_test.h"
#include "conversions.h"
#include "doc.h"
#include "scene.h"

using namespace mcp;

namespace {

QString timeUtilsPath()
{
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../../qmlui/js/TimeUtils.js"),
        QDir::currentPath() + QStringLiteral("/../qmlui/js/TimeUtils.js"),
        QDir::currentPath() + QStringLiteral("/qmlui/js/TimeUtils.js")
    };

    for (const QString &candidate : candidates)
    {
        if (QFile::exists(candidate))
            return candidate;
    }

    return QString();
}

QJSValue evaluateTimeUtils(QJSEngine &engine, const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QJSValue();

    return engine.evaluate(QString::fromUtf8(file.readAll()), path);
}

}

// --- beatStringToValue: fraction inputs ---

void Conversions_Test::beatStringToValue_fractions_data()
{
    QTest::addColumn<std::string>("input");
    QTest::addColumn<uint>("expected");

    QTest::newRow("1/16") << std::string("1/16") << 63u;
    QTest::newRow("1/8")  << std::string("1/8")  << 125u;
    QTest::newRow("1/4")  << std::string("1/4")  << 250u;
    QTest::newRow("3/8")  << std::string("3/8")  << 375u;
    QTest::newRow("1/2")  << std::string("1/2")  << 500u;
    QTest::newRow("1/1")  << std::string("1/1")  << 1000u;
    QTest::newRow("1")    << std::string("1")    << 1000u;
    QTest::newRow("2")    << std::string("2")    << 2000u;
    QTest::newRow("3/16") << std::string("3/16") << 188u;
    QTest::newRow("7/8")  << std::string("7/8")  << 875u;
}

void Conversions_Test::beatStringToValue_fractions()
{
    QFETCH(std::string, input);
    QFETCH(uint, expected);
    QCOMPARE(beatStringToValue(input), expected);
}

// --- beatStringToValue: decimal inputs ---

void Conversions_Test::beatStringToValue_decimal_data()
{
    QTest::addColumn<std::string>("input");
    QTest::addColumn<uint>("expected");

    QTest::newRow("0.0625 = 1/16") << std::string("0.0625") << 63u;
    QTest::newRow("0.5 = 1/2")     << std::string("0.5")    << 500u;
    QTest::newRow("1.5")           << std::string("1.5")    << 1500u;
    QTest::newRow("0.25 = 1/4")    << std::string("0.25")   << 250u;
    QTest::newRow("0.125 = 1/8")   << std::string("0.125")  << 125u;
}

void Conversions_Test::beatStringToValue_decimal()
{
    QFETCH(std::string, input);
    QFETCH(uint, expected);
    QCOMPARE(beatStringToValue(input), expected);
}

// --- beatStringToValue: invalid inputs ---

void Conversions_Test::beatStringToValue_invalid_data()
{
    QTest::addColumn<std::string>("input");

    QTest::newRow("empty")    << std::string("");
    QTest::newRow("abc")      << std::string("abc");
    QTest::newRow("1/0")      << std::string("1/0");
    QTest::newRow("-1/4")     << std::string("-1/4");
    QTest::newRow("0/4")      << std::string("0/4");
    QTest::newRow("-1")       << std::string("-1");
    QTest::newRow("0")        << std::string("0");
}

void Conversions_Test::beatStringToValue_invalid()
{
    QFETCH(std::string, input);
    QCOMPARE(beatStringToValue(input), 0u);
}

// --- beatStringToValue: overflow guard ---

void Conversions_Test::beatStringToValue_overflow()
{
    QCOMPARE(beatStringToValue("999999/1"), 0u);
    QCOMPARE(beatStringToValue("200000"), 0u);
    QCOMPARE(beatStringToValue("100001/1"), 0u);  // just over limit
    // At the limit should still work
    QVERIFY(beatStringToValue("100000/1") != 0u);
}

// --- qmlui/js/TimeUtils.js: Beat Editor text input parsing ---

void Conversions_Test::qlcStringToTime_beatTextInput_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<double>("expected");
    QTest::addColumn<bool>("expectedNaN");

    QTest::newRow("12/16")  << QStringLiteral("12/16") << 750.0  << false;
    QTest::newRow("13/16")  << QStringLiteral("13/16") << 813.0  << false;
    QTest::newRow("1/16")   << QStringLiteral("1/16")  << 63.0   << false;
    QTest::newRow("3/4")    << QStringLiteral("3/4")   << 750.0  << false;
    QTest::newRow("7/8")    << QStringLiteral("7/8")   << 875.0  << false;
    QTest::newRow("1/2")    << QStringLiteral("1/2")   << 500.0  << false;
    QTest::newRow("1/1")    << QStringLiteral("1/1")   << 1000.0 << false;
    QTest::newRow("16/16")  << QStringLiteral("16/16") << 1000.0 << false;
    QTest::newRow("32/16")  << QStringLiteral("32/16") << 2000.0 << false;
    QTest::newRow("0/16")   << QStringLiteral("0/16")  << 0.0    << false;
    QTest::newRow("5")      << QStringLiteral("5")     << 5000.0 << false;
    QTest::newRow("2 1/4")  << QStringLiteral("2 1/4") << 2250.0 << false;
    QTest::newRow("infinity") << QString::fromUtf8("∞") << -2.0  << false;
    QTest::newRow("3/5 invalid denominator") << QStringLiteral("3/5") << 0.0 << true;
    QTest::newRow("1/3 invalid denominator") << QStringLiteral("1/3") << 0.0 << true;
    QTest::newRow("garbage") << QStringLiteral("abc")   << 0.0   << true;
    QTest::newRow("negative fraction") << QStringLiteral("-1/16") << 0.0 << true;
}

void Conversions_Test::qlcStringToTime_beatTextInput()
{
    QFETCH(QString, input);
    QFETCH(double, expected);
    QFETCH(bool, expectedNaN);

    QString path = timeUtilsPath();
    QVERIFY2(!path.isEmpty(), "Unable to locate qmlui/js/TimeUtils.js");

    QJSEngine engine;
    QJSValue evaluated = evaluateTimeUtils(engine, path);
    QVERIFY2(!evaluated.isError(),
             qPrintable(QStringLiteral("TimeUtils.js evaluation failed: %1").arg(evaluated.toString())));

    QJSValue parser = engine.globalObject().property(QStringLiteral("qlcStringToTime"));
    QVERIFY2(parser.isCallable(), "qlcStringToTime is not callable");

    QJSValue result = parser.call(QJSValueList{QJSValue(input), QJSValue(1)});
    QVERIFY2(result.isNumber(), qPrintable(QStringLiteral("Expected numeric result for %1").arg(input)));

    const double actual = result.toNumber();
    if (expectedNaN)
        QVERIFY2(std::isnan(actual), qPrintable(QStringLiteral("Expected NaN for %1, got %2").arg(input).arg(actual)));
    else
        QCOMPARE(actual, expected);
}

void Conversions_Test::timeToQlcString_currentBeatDisplay_data()
{
    QTest::addColumn<int>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("1/16 canonical") << 63 << QStringLiteral(" 1/16");
    QTest::newRow("1/8 canonical") << 125 << QStringLiteral(" 1/8");
    QTest::newRow("off-grid 63+63") << 126 << QStringLiteral(" 126");
}

void Conversions_Test::timeToQlcString_currentBeatDisplay()
{
    QFETCH(int, input);
    QFETCH(QString, expected);

    QString path = timeUtilsPath();
    QVERIFY2(!path.isEmpty(), "Unable to locate qmlui/js/TimeUtils.js");

    QJSEngine engine;
    QJSValue evaluated = evaluateTimeUtils(engine, path);
    QVERIFY2(!evaluated.isError(),
             qPrintable(QStringLiteral("TimeUtils.js evaluation failed: %1").arg(evaluated.toString())));

    QJSValue formatter = engine.globalObject().property(QStringLiteral("timeToQlcString"));
    QVERIFY2(formatter.isCallable(), "timeToQlcString is not callable");

    QJSValue result = formatter.call(QJSValueList{QJSValue(input), QJSValue(1)});
    QVERIFY2(result.isString(), qPrintable(QStringLiteral("Expected string result for %1").arg(input)));
    QCOMPARE(result.toString(), expected);
}

void Conversions_Test::engineBeatTiming_characterization_data()
{
    QTest::addColumn<uint>("beatValue");
    QTest::addColumn<int>("beatDuration");
    QTest::addColumn<uint>("expectedMs");

    QTest::newRow("1/16 at 120 BPM") << 63u << 500 << 32u;
    QTest::newRow("1/8 canonical at 120 BPM") << 125u << 500 << 63u;
    QTest::newRow("63+63 off-grid at 120 BPM") << 126u << 500 << 63u;
    QTest::newRow("1/16 at 129 BPM") << 63u << 465 << 29u;
    QTest::newRow("1/16 at 163 BPM") << 63u << 366 << 23u;
}

void Conversions_Test::engineBeatTiming_characterization()
{
    QFETCH(uint, beatValue);
    QFETCH(int, beatDuration);
    QFETCH(uint, expectedMs);

    QCOMPARE(Function::beatsToTime(beatValue, beatDuration), expectedMs);
}

void Conversions_Test::engineBeatTiming_roundTripLoss()
{
    QCOMPARE(Function::speedAdd(63, 63), 126u);
    QCOMPARE(Function::timeToBeats(Function::beatsToTime(63, 500), 500), 63u);
    QCOMPARE(Function::beatsToTime(Function::timeToBeats(63, 500), 500), 63u);
    QCOMPARE(Function::timeToBeats(Function::beatsToTime(126, 500), 500), 125u);

    uint timeFadeIn = Function::beatsToTime(63, 500);
    uint timeDuration = Function::beatsToTime(Function::speedAdd(63, 63), 500);
    uint roundTrippedFadeIn = Function::timeToBeats(timeFadeIn, 500);
    uint roundTrippedDuration = Function::timeToBeats(timeDuration, 500);
    QCOMPARE(Function::speedSubtract(roundTrippedDuration, roundTrippedFadeIn), 62u);
}

void Conversions_Test::snapToBeatGrid_data()
{
    QTest::addColumn<uint>("input");
    QTest::addColumn<uint>("expected");

    // On-grid values (should pass through unchanged)
    QTest::newRow("0") << 0u << 0u;
    QTest::newRow("63") << 63u << 63u;
    QTest::newRow("125") << 125u << 125u;
    QTest::newRow("500") << 500u << 500u;
    QTest::newRow("938") << 938u << 938u;
    QTest::newRow("1000") << 1000u << 1000u;
    QTest::newRow("1063") << 1063u << 1063u;

    // Off-grid +1 drift from odd-sixteenth addition
    QTest::newRow("126->125") << 126u << 125u;
    QTest::newRow("251->250") << 251u << 250u;
    QTest::newRow("376->375") << 376u << 375u;
    QTest::newRow("501->500") << 501u << 500u;
    QTest::newRow("626->625") << 626u << 625u;
    QTest::newRow("751->750") << 751u << 750u;
    QTest::newRow("876->875") << 876u << 875u;
    QTest::newRow("1001->1000") << 1001u << 1000u;
    QTest::newRow("1126->1125") << 1126u << 1125u;
    QTest::newRow("1251->1250") << 1251u << 1250u;
    QTest::newRow("1376->1375") << 1376u << 1375u;
    QTest::newRow("1501->1500") << 1501u << 1500u;
    QTest::newRow("1626->1625") << 1626u << 1625u;
    QTest::newRow("1751->1750") << 1751u << 1750u;
    QTest::newRow("1876->1875") << 1876u << 1875u;

    // Off-grid -1 (subtraction case: 125-63=62)
    QTest::newRow("62->63") << 62u << 63u;
    QTest::newRow("187->188") << 187u << 188u;
    QTest::newRow("312->313") << 312u << 313u;
    QTest::newRow("437->438") << 437u << 438u;
    QTest::newRow("562->563") << 562u << 563u;
    QTest::newRow("687->688") << 687u << 688u;
    QTest::newRow("812->813") << 812u << 813u;
    QTest::newRow("937->938") << 937u << 938u;

    // Sentinel values
    QTest::newRow("infinite") << (uint)Function::infiniteSpeed() << (uint)Function::infiniteSpeed();
}

void Conversions_Test::snapToBeatGrid()
{
    QFETCH(uint, input);
    QFETCH(uint, expected);
    QCOMPARE(Function::snapToBeatGrid(input), expected);
}

void Conversions_Test::durationBeatSnap_data()
{
    QTest::addColumn<int>("tempoType");
    QTest::addColumn<uint>("inputDuration");
    QTest::addColumn<uint>("expectedDuration");

    // Beat mode: snaps
    QTest::newRow("beats 126->125") << (int)Function::Beats << 126u << 125u;
    QTest::newRow("beats 1001->1000") << (int)Function::Beats << 1001u << 1000u;
    QTest::newRow("beats 125 exact") << (int)Function::Beats << 125u << 125u;
    QTest::newRow("beats 62->63") << (int)Function::Beats << 62u << 63u;

    // Time mode: no snap
    QTest::newRow("time 126 stays") << (int)Function::Time << 126u << 126u;
    QTest::newRow("time 62 stays") << (int)Function::Time << 62u << 62u;
}

void Conversions_Test::durationBeatSnap()
{
    QFETCH(int, tempoType);
    QFETCH(uint, inputDuration);
    QFETCH(uint, expectedDuration);

    Doc doc(this);
    Scene scene(&doc);
    scene.setTempoType(Function::TempoType(tempoType));
    scene.setDuration(inputDuration);
    QCOMPARE(scene.duration(), expectedDuration);
}

void Conversions_Test::holdSpeedBeatSnap()
{
    Doc doc(this);
    Scene scene(&doc);
    scene.setTempoType(Function::Beats);
    scene.setFadeInSpeed(63);
    scene.setDuration(125);
    QCOMPARE(scene.holdSpeed(), 63u);
}

void Conversions_Test::beatsToTimeRounding_data()
{
    QTest::addColumn<uint>("beats");
    QTest::addColumn<uint>("beatDuration");
    QTest::addColumn<uint>("expected");

    QTest::newRow("1/16 at 120bpm") << 63u << 500u << 32u;
    QTest::newRow("1/8 at 120bpm") << 125u << 500u << 63u;
    QTest::newRow("1/4 at 120bpm") << 250u << 500u << 125u;
    QTest::newRow("1/2 at 120bpm") << 500u << 500u << 250u;
    QTest::newRow("15/16 at 120bpm") << 938u << 500u << 469u;
    QTest::newRow("1 beat at 120bpm") << 1000u << 500u << 500u;
    QTest::newRow("1/16 at 100bpm") << 63u << 600u << 38u;
    QTest::newRow("1/2 at 140bpm") << 500u << 429u << 215u;
}

void Conversions_Test::beatsToTimeRounding()
{
    QFETCH(uint, beats);
    QFETCH(uint, beatDuration);
    QFETCH(uint, expected);
    QCOMPARE(Function::beatsToTime(beats, beatDuration), expected);
}

// --- valueToBeatString: canonical values ---

void Conversions_Test::valueToBeatString_canonical_data()
{
    QTest::addColumn<uint>("input");
    QTest::addColumn<std::string>("expected");

    QTest::newRow("63 = 1/16")  << 63u   << std::string("1/16");
    QTest::newRow("125 = 1/8")  << 125u  << std::string("1/8");
    QTest::newRow("250 = 1/4")  << 250u  << std::string("1/4");
    QTest::newRow("500 = 1/2")  << 500u  << std::string("1/2");
    QTest::newRow("1000 = 1")   << 1000u << std::string("1");
    QTest::newRow("2000 = 2")   << 2000u << std::string("2");
    QTest::newRow("375 = 3/8")  << 375u  << std::string("3/8");
    QTest::newRow("938 = 15/16") << 938u << std::string("15/16");
}

void Conversions_Test::valueToBeatString_canonical()
{
    QFETCH(uint, input);
    QFETCH(std::string, expected);
    QCOMPARE(valueToBeatString(input), expected);
}

// --- valueToBeatString: composite (whole + fraction) ---

void Conversions_Test::valueToBeatString_composite_data()
{
    QTest::addColumn<uint>("input");
    QTest::addColumn<std::string>("expected");

    // 1000 + 125 = 1125 → "1+1/8" or "9/8" depending on decomposition
    // beatValueToMusical(1125) → count=9, subdiv=8 → gcd(9,8)=1 → "9/8"
    QTest::newRow("1125 = 9/8")  << 1125u << std::string("9/8");
    // 2000 + 500 = 2500 → count=5, subdiv=2 → "5/2"
    QTest::newRow("2500 = 5/2")  << 2500u << std::string("5/2");
    // 1000 + 63 = 1063 → count=17, subdiv=16 → gcd(17,16)=1 → "17/16"
    QTest::newRow("1063 = 17/16") << 1063u << std::string("17/16");
}

void Conversions_Test::valueToBeatString_composite()
{
    QFETCH(uint, input);
    QFETCH(std::string, expected);
    QCOMPARE(valueToBeatString(input), expected);
}

// --- valueToBeatString: off-grid snapping ---

void Conversions_Test::valueToBeatString_offgrid()
{
    // 100 is not on any musical grid → should snap to nearest 1/16 (125 = 1/8)
    std::string result = valueToBeatString(100);
    // Must NOT be "100" (ambiguous with 100 beats)
    QVERIFY2(result != "100",
             qPrintable(QString("Off-grid 100 produced ambiguous '%1'").arg(QString::fromStdString(result))));
    QCOMPARE(result, std::string("1/8"));  // 100 snaps to 125 → "1/8"

    // 495 snaps to nearest 1/16 (500 = 1/2)
    QCOMPARE(valueToBeatString(495), std::string("1/2"));

    // Tiny values (1-31) snap to 0
    QCOMPARE(valueToBeatString(1), std::string("0"));
    QCOMPARE(valueToBeatString(31), std::string("0"));
}

// --- valueToBeatString: zero ---

void Conversions_Test::valueToBeatString_zero()
{
    QCOMPARE(valueToBeatString(0), std::string("0"));
}

// --- valueToBeatString: GCD reduction ---

void Conversions_Test::valueToBeatString_gcdReduction_data()
{
    QTest::addColumn<uint>("input");
    QTest::addColumn<std::string>("expected");

    // These values' beat decomposition should be GCD-reduced
    QTest::newRow("125 (=2/16 → 1/8)")  << 125u  << std::string("1/8");
    QTest::newRow("250 (=4/16 → 1/4)")  << 250u  << std::string("1/4");
    QTest::newRow("500 (=8/16 → 1/2)")  << 500u  << std::string("1/2");
    QTest::newRow("750 (=3/4)")          << 750u  << std::string("3/4");
}

void Conversions_Test::valueToBeatString_gcdReduction()
{
    QFETCH(uint, input);
    QFETCH(std::string, expected);
    QCOMPARE(valueToBeatString(input), expected);
}

// --- Round-trip: all canonical 1/16 values ---

void Conversions_Test::roundTrip_allCanonical()
{
    // Every canonical 1/16 value from 63 to 938 must round-trip through string conversion
    static const uint kCanonical[] = {
        63, 125, 188, 250, 313, 375, 438,
        500, 563, 625, 688, 750, 813, 875, 938
    };

    for (uint val : kCanonical)
    {
        std::string str = valueToBeatString(val);
        QVERIFY2(!str.empty(),
                 qPrintable(QString("valueToBeatString(%1) returned empty").arg(val)));
        uint roundTripped = beatStringToValue(str);
        QCOMPARE(roundTripped, val);
    }

    // Also test whole beats 1-4
    for (uint b = 1; b <= 4; b++)
    {
        uint val = b * 1000;
        std::string str = valueToBeatString(val);
        uint roundTripped = beatStringToValue(str);
        QCOMPARE(roundTripped, val);
    }
}

QTEST_MAIN(Conversions_Test)
