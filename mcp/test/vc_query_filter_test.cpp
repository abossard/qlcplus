/*
  Q Light Controller Plus - Unit test
  vc_query_filter_test.cpp

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

#include <QtTest>

#include "vc_query_filter_test.h"
#include "vc_query_helpers.h"

using Json = nlohmann::json;

// ========== Argument Validation (parameterized) ==========

void VCQueryFilter_Test::validateArgs_data()
{
    QTest::addColumn<QByteArray>("jsonArgs");
    QTest::addColumn<bool>("expectValid");
    QTest::addColumn<QString>("errorSubstring");

    // --- Valid cases ---
    QTest::newRow("empty args")
        << QByteArray("{}") << true << QString();
    QTest::newRow("nameFilter string")
        << QByteArray(R"({"nameFilter": "Wash*"})") << true << QString();
    QTest::newRow("typeFilter string")
        << QByteArray(R"({"typeFilter": "button"})") << true << QString();
    QTest::newRow("typeFilter array of strings")
        << QByteArray(R"({"typeFilter": ["button", "slider"]})") << true << QString();
    QTest::newRow("typeFilter all valid types")
        << QByteArray(R"({"typeFilter": ["button","slider","xypad","frame","soloframe","speedDial","cuelist","label","audioTrigger","matrix","clock"]})")
        << true << QString();
    QTest::newRow("functionID valid")
        << QByteArray(R"({"functionID": 42})") << true << QString();
    QTest::newRow("fixtureID valid")
        << QByteArray(R"({"fixtureID": 0})") << true << QString();
    QTest::newRow("channel valid")
        << QByteArray(R"({"channel": 5})") << true << QString();
    QTest::newRow("pageIndex valid")
        << QByteArray(R"({"pageIndex": 0})") << true << QString();
    QTest::newRow("parentID valid")
        << QByteArray(R"({"parentID": 100})") << true << QString();
    QTest::newRow("properties valid")
        << QByteArray(R"({"properties": ["type", "caption"]})") << true << QString();
    QTest::newRow("properties valid set")
        << QByteArray(R"({"properties": ["type", "caption", "channels"]})") << true << QString();
    QTest::newRow("multiple filters valid")
        << QByteArray(R"({"nameFilter": "Par*", "typeFilter": "slider", "pageIndex": 0})")
        << true << QString();

    // --- Invalid cases ---
    QTest::newRow("nameFilter integer")
        << QByteArray(R"({"nameFilter": 123})") << false << QStringLiteral("must be a string");
    QTest::newRow("nameFilter null")
        << QByteArray(R"({"nameFilter": null})") << false << QStringLiteral("must be a string");
    QTest::newRow("typeFilter integer")
        << QByteArray(R"({"typeFilter": 42})") << false << QStringLiteral("must be a string or array");
    QTest::newRow("typeFilter array with integer")
        << QByteArray(R"({"typeFilter": ["button", 42]})") << false << QStringLiteral("must be a string or array of strings");
    QTest::newRow("typeFilter unknown type")
        << QByteArray(R"({"typeFilter": "foobar"})") << false << QStringLiteral("unknown widget type");
    QTest::newRow("typeFilter array with unknown type")
        << QByteArray(R"({"typeFilter": ["button", "nonexistent"]})") << false << QStringLiteral("unknown widget type");
    QTest::newRow("functionID negative")
        << QByteArray(R"({"functionID": -1})") << false << QStringLiteral("non-negative integer");
    QTest::newRow("functionID string")
        << QByteArray(R"({"functionID": "abc"})") << false << QStringLiteral("non-negative integer");
    QTest::newRow("fixtureID float")
        << QByteArray(R"({"fixtureID": 3.5})") << false << QStringLiteral("non-negative integer");
    QTest::newRow("channel negative")
        << QByteArray(R"({"channel": -10})") << false << QStringLiteral("non-negative integer");
    QTest::newRow("pageIndex negative")
        << QByteArray(R"({"pageIndex": -1})") << false << QStringLiteral("non-negative integer");
    QTest::newRow("properties not array")
        << QByteArray(R"({"properties": "type"})") << false << QStringLiteral("must be an array");
    QTest::newRow("properties array with int")
        << QByteArray(R"({"properties": ["type", 42]})") << false << QStringLiteral("must be an array of strings");
    QTest::newRow("properties unknown prop")
        << QByteArray(R"({"properties": ["type", "nonexistent"]})") << false << QStringLiteral("unknown property");
    QTest::newRow("unknown top-level field")
        << QByteArray(R"({"unknownField": true})") << false << QStringLiteral("unknown field");
}

void VCQueryFilter_Test::validateArgs()
{
    QFETCH(QByteArray, jsonArgs);
    QFETCH(bool, expectValid);
    QFETCH(QString, errorSubstring);

    Json args = Json::parse(jsonArgs.toStdString());
    auto result = VCQueryPages::validateArgs(args);

    if (expectValid)
    {
        QVERIFY2(result.empty(), qPrintable(QString("Expected valid, got error: %1")
                 .arg(QString::fromStdString(result))));
    }
    else
    {
        QVERIFY2(!result.empty(), "Expected validation error, got success");
        auto errJson = Json::parse(result);
        QVERIFY2(errJson.contains("error"), "Error response must contain 'error' key");
        QString errMsg = QString::fromStdString(errJson["error"].get<std::string>());
        QVERIFY2(errMsg.contains(errorSubstring),
                 qPrintable(QStringLiteral("Expected '%1' in error: %2")
                            .arg(errorSubstring, errMsg)));
    }
}

// ========== Glob Matching (parameterized) ==========

void VCQueryFilter_Test::globMatch_data()
{
    QTest::addColumn<QString>("pattern");
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("expectedMatch");

    QTest::newRow("exact match")         << "Wash"             << "Wash"              << true;
    QTest::newRow("trailing wildcard")   << "Wash*"            << "Wash Left"         << true;
    QTest::newRow("leading wildcard")    << "*Left"            << "Wash Left"         << true;
    QTest::newRow("middle wildcard")     << "Wash*Left"        << "Wash Center Left"  << true;
    QTest::newRow("no match")            << "Wash*"            << "Par Can"           << false;
    QTest::newRow("case insensitive")    << "wash*"            << "Wash Left"         << true;
    QTest::newRow("question mark")       << "Wash ?"           << "Wash A"            << true;
    QTest::newRow("question mark no match") << "Wash ?"        << "Wash AB"           << false;
    QTest::newRow("empty pattern empty text") << ""            << ""                  << true;
    QTest::newRow("star matches empty")  << "*"                << ""                  << true;
    QTest::newRow("star matches anything") << "*"              << "anything"          << true;
    QTest::newRow("mixed case pattern")  << "WASH*"            << "wash left"         << true;
    QTest::newRow("special regex chars") << "Par (Left)*"      << "Par (Left) #1"     << true;
}

void VCQueryFilter_Test::globMatch()
{
    QFETCH(QString, pattern);
    QFETCH(QString, text);
    QFETCH(bool, expectedMatch);

    bool result = VCQueryPages::globMatch(pattern, text);
    QCOMPARE(result, expectedMatch);
}

// ========== Widget Filtering (parameterized) ==========

static VCBridge::WidgetDetails makeTestWidget(
    const QString &type, const QString &caption,
    quint32 functionID = 0, int parentID = -1,
    const QList<QPair<quint32, quint32>> &channels = {})
{
    VCBridge::WidgetDetails d;
    d.id = 1;
    d.type = type;
    d.caption = caption;
    d.functionID = functionID;
    d.parentID = parentID;
    d.channels = channels;
    d.geometry = QRect(0, 0, 100, 60);
    return d;
}

void VCQueryFilter_Test::filterWidget_data()
{
    QTest::addColumn<QString>("widgetType");
    QTest::addColumn<QString>("widgetCaption");
    QTest::addColumn<int>("widgetFunctionID");
    QTest::addColumn<int>("widgetParentID");
    QTest::addColumn<QByteArray>("filterArgs");
    QTest::addColumn<bool>("expectedMatch");

    // Channels for some tests: fixture 3 channel 5, fixture 7 channel 0
    // (encoded as QByteArray filter args, decoded in test body)

    QTest::newRow("no filter matches all")
        << "button" << "Go" << 10 << 1
        << QByteArray("{}") << true;

    QTest::newRow("typeFilter button matches button")
        << "button" << "Go" << 10 << 1
        << QByteArray(R"({"typeFilter": "button"})") << true;

    QTest::newRow("typeFilter button rejects slider")
        << "slider" << "Dimmer" << 0 << 1
        << QByteArray(R"({"typeFilter": "button"})") << false;

    QTest::newRow("typeFilter array matches any")
        << "slider" << "Dimmer" << 0 << 1
        << QByteArray(R"({"typeFilter": ["button", "slider"]})") << true;

    QTest::newRow("nameFilter glob match")
        << "button" << "Wash Left" << 10 << 1
        << QByteArray(R"({"nameFilter": "Wash*"})") << true;

    QTest::newRow("nameFilter glob no match")
        << "button" << "Par Can" << 10 << 1
        << QByteArray(R"({"nameFilter": "Wash*"})") << false;

    QTest::newRow("functionID match")
        << "button" << "Go" << 42 << 1
        << QByteArray(R"({"functionID": 42})") << true;

    QTest::newRow("functionID no match")
        << "button" << "Go" << 42 << 1
        << QByteArray(R"({"functionID": 99})") << false;

    QTest::newRow("parentID match")
        << "button" << "Go" << 10 << 50
        << QByteArray(R"({"parentID": 50})") << true;

    QTest::newRow("parentID no match")
        << "button" << "Go" << 10 << 50
        << QByteArray(R"({"parentID": 99})") << false;

    QTest::newRow("multiple filters AND logic - all match")
        << "button" << "Wash Left" << 42 << 1
        << QByteArray(R"({"typeFilter": "button", "nameFilter": "Wash*", "functionID": 42})")
        << true;

    QTest::newRow("multiple filters AND logic - one fails")
        << "button" << "Wash Left" << 42 << 1
        << QByteArray(R"({"typeFilter": "slider", "nameFilter": "Wash*", "functionID": 42})")
        << false;
}

void VCQueryFilter_Test::filterWidget()
{
    QFETCH(QString, widgetType);
    QFETCH(QString, widgetCaption);
    QFETCH(int, widgetFunctionID);
    QFETCH(int, widgetParentID);
    QFETCH(QByteArray, filterArgs);
    QFETCH(bool, expectedMatch);

    auto d = makeTestWidget(widgetType, widgetCaption,
                            (quint32)widgetFunctionID, widgetParentID);

    Json args = Json::parse(filterArgs.toStdString());
    bool result = VCQueryPages::filterWidget(d, args);
    QCOMPARE(result, expectedMatch);
}

// ========== Field Selection / Serialization (parameterized) ==========

void VCQueryFilter_Test::serializeWidget_data()
{
    QTest::addColumn<QStringList>("properties");
    QTest::addColumn<QStringList>("expectedKeys");
    QTest::addColumn<QStringList>("notExpectedKeys");

    QTest::newRow("empty properties returns all core fields")
        << QStringList()
        << QStringList({"id", "type", "caption", "parentID", "x", "y"})
        << QStringList();

    QTest::newRow("only type")
        << QStringList({"type"})
        << QStringList({"id", "type"})
        << QStringList({"caption", "x", "y"});

    QTest::newRow("type+caption")
        << QStringList({"type", "caption"})
        << QStringList({"id", "type", "caption"})
        << QStringList({"x", "y", "parentID"});

    QTest::newRow("geometry")
        << QStringList({"geometry"})
        << QStringList({"id", "x", "y", "width", "height"})
        << QStringList({"type", "caption"});

    QTest::newRow("channels requested")
        << QStringList({"channels"})
        << QStringList({"id", "channels"})
        << QStringList({"type", "caption"});

    QTest::newRow("id always included even if not requested")
        << QStringList({"caption"})
        << QStringList({"id", "caption"})
        << QStringList({"type"});
}

void VCQueryFilter_Test::serializeWidget()
{
    QFETCH(QStringList, properties);
    QFETCH(QStringList, expectedKeys);
    QFETCH(QStringList, notExpectedKeys);

    // Build a widget with known values
    VCBridge::WidgetDetails d;
    d.id = 42;
    d.type = "button";
    d.caption = "Test Button";
    d.geometry = QRect(10, 20, 100, 60);
    d.parentID = 5;
    d.functionID = 10;
    d.action = "toggle";
    d.channels.append({3, 5});
    d.channels.append({7, 0});

    std::set<std::string> propSet;
    for (const auto &p : properties)
        propSet.insert(p.toStdString());
    propSet = VCQueryPages::expandCompoundGroups(propSet);

    Json result = VCQueryPages::serializeWidget(d, propSet);

    // Verify expected keys
    for (const auto &key : expectedKeys)
    {
        QVERIFY2(result.contains(key.toStdString()),
                 qPrintable(QStringLiteral("Expected key '%1' in result: %2")
                            .arg(key, QString::fromStdString(result.dump()))));
    }

    // Verify not-expected keys
    for (const auto &key : notExpectedKeys)
    {
        QVERIFY2(!result.contains(key.toStdString()),
                 qPrintable(QStringLiteral("Did not expect key '%1' in result: %2")
                            .arg(key, QString::fromStdString(result.dump()))));
    }
}

// ========== Known Properties Completeness ==========

void VCQueryFilter_Test::knownPropertiesCompleteness()
{
    // Ensure all compound groups are in the valid set
    QVERIFY(VCQueryPages::kValidProperties.count("buttonConfig"));
    QVERIFY(VCQueryPages::kValidProperties.count("cueListConfig"));
    QVERIFY(VCQueryPages::kValidProperties.count("clockConfig"));
    QVERIFY(VCQueryPages::kValidProperties.count("speedDialConfig"));
    QVERIFY(VCQueryPages::kValidProperties.count("matrixConfig"));
    QVERIFY(VCQueryPages::kValidProperties.count("xyPadConfig"));

    // Ensure core properties are present
    QVERIFY(VCQueryPages::kValidProperties.count("type"));
    QVERIFY(VCQueryPages::kValidProperties.count("caption"));
    QVERIFY(VCQueryPages::kValidProperties.count("geometry"));
    QVERIFY(VCQueryPages::kValidProperties.count("parentID"));
    QVERIFY(VCQueryPages::kValidProperties.count("functionID"));
    QVERIFY(VCQueryPages::kValidProperties.count("channels"));
    QVERIFY(VCQueryPages::kValidProperties.count("inputMappings"));
    QVERIFY(VCQueryPages::kValidProperties.count("bgColor"));
    QVERIFY(VCQueryPages::kValidProperties.count("fgColor"));

    // Ensure all widget types are in the valid set
    QVERIFY(VCQueryPages::kValidWidgetTypes.count("button"));
    QVERIFY(VCQueryPages::kValidWidgetTypes.count("slider"));
    QVERIFY(VCQueryPages::kValidWidgetTypes.count("xypad"));
    QVERIFY(VCQueryPages::kValidWidgetTypes.count("frame"));
    QVERIFY(VCQueryPages::kValidWidgetTypes.count("soloframe"));
    QVERIFY(VCQueryPages::kValidWidgetTypes.count("speedDial"));
    QVERIFY(VCQueryPages::kValidWidgetTypes.count("cuelist"));
    QVERIFY(VCQueryPages::kValidWidgetTypes.count("label"));
    QVERIFY(VCQueryPages::kValidWidgetTypes.count("audioTrigger"));
    QVERIFY(VCQueryPages::kValidWidgetTypes.count("matrix"));
    QVERIFY(VCQueryPages::kValidWidgetTypes.count("clock"));
    QCOMPARE((int)VCQueryPages::kValidWidgetTypes.size(), 11);
}

// ========== Compound Group Expansion ==========

void VCQueryFilter_Test::compoundGroupExpansion()
{
    // buttonConfig should expand to its child properties
    std::set<std::string> input = {"buttonConfig", "type"};
    auto result = VCQueryPages::expandCompoundGroups(input);

    QVERIFY(result.count("iconPath"));
    QVERIFY(result.count("startupIntensityEnabled"));
    QVERIFY(result.count("startupIntensity"));
    QVERIFY(result.count("flashOverride"));
    QVERIFY(result.count("flashForceLTP"));
    QVERIFY(result.count("stopAllFadeTime"));
    QVERIFY(result.count("type"));          // preserved
    QVERIFY(!result.count("buttonConfig")); // group name removed

    // matrixConfig expansion
    std::set<std::string> input2 = {"matrixConfig"};
    auto result2 = VCQueryPages::expandCompoundGroups(input2);
    QVERIFY(result2.count("color1"));
    QVERIFY(result2.count("color5"));
    QVERIFY(result2.count("animation"));
    QVERIFY(result2.count("instantApply"));
    QVERIFY(result2.count("visibilityMask"));
    QVERIFY(!result2.count("matrixConfig"));

    // xyPadConfig expansion
    std::set<std::string> input3 = {"xyPadConfig"};
    auto result3 = VCQueryPages::expandCompoundGroups(input3);
    QVERIFY(result3.count("displayMode"));
    QVERIFY(result3.count("fixtures"));
    QVERIFY(result3.count("position"));
    QVERIFY(result3.count("presets"));
    QVERIFY(!result3.count("xyPadConfig"));
}

QTEST_MAIN(VCQueryFilter_Test)
