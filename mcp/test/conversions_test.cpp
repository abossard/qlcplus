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

using namespace mcp;

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

    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../../qmlui/js/TimeUtils.js"),
        QDir::currentPath() + QStringLiteral("/../qmlui/js/TimeUtils.js"),
        QDir::currentPath() + QStringLiteral("/qmlui/js/TimeUtils.js")
    };

    QString path;
    for (const QString &candidate : candidates)
    {
        if (QFile::exists(candidate))
        {
            path = candidate;
            break;
        }
    }
    QVERIFY2(!path.isEmpty(), "Unable to locate qmlui/js/TimeUtils.js");

    QFile file(path);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(QStringLiteral("Unable to open %1").arg(path)));

    QJSEngine engine;
    QJSValue evaluated = engine.evaluate(QString::fromUtf8(file.readAll()), path);
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
