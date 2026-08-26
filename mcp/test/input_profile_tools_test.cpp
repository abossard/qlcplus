/*
  Q Light Controller Plus - Unit test
  input_profile_tools_test.cpp

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
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <nlohmann/json.hpp>

#include "input_profile_tools_test.h"
#include "tool_registry.h"
#include "doc.h"
#include "inputoutputmap.h"
#include "qlcinputprofile.h"
#include "qlcinputchannel.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {

Json parsed(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

Json invoke(Doc *doc, const char *tool, const Json &args)
{
    fastmcpp::tools::ToolManager tm;
    registerInputProfileTools(tm, doc);
    return parsed(tm.invoke(tool, args));
}

Json twoChannelProfile(const char *manufacturer, const char *model)
{
    return Json{
        {"manufacturer", manufacturer}, {"model", model}, {"type", "MIDI"},
        {"channels", Json::array({
            {{"number", 1}, {"name", "Fader 1"}, {"type", "Slider"}},
            {{"number", 2}, {"name", "Pad 1"}, {"type", "Button"}}
        })}
    };
}

}

void McpInputProfileTools_Test::initTestCase()
{
    /*
     * create_input_profiles writes into InputOutputMap::userProfileDirectory(),
     * which QLCFile::userDirectory() builds from getenv("HOME") directly —
     * QStandardPaths::setTestModeEnabled does NOT redirect it. Overriding HOME
     * for this process is the only thing that actually moves the target, and it
     * must happen before any profile path is resolved. Nothing in this test may
     * ever delete a directory it has not confirmed lives under m_home.
     */
    m_realHome = qgetenv("HOME");
    QVERIFY(m_home.isValid());
    qputenv("HOME", m_home.path().toUtf8());

    const QString redirected = InputOutputMap::userProfileDirectory().absolutePath();
    QVERIFY2(redirected.startsWith(m_home.path()),
             qPrintable("profile directory was not redirected: " + redirected));
}

void McpInputProfileTools_Test::cleanupTestCase()
{
    qputenv("HOME", m_realHome);
}

void McpInputProfileTools_Test::init()
{
    m_doc = new Doc(this);
}

void McpInputProfileTools_Test::cleanup()
{
    // The QTemporaryDir owns everything written here and removes itself; no
    // explicit recursive delete, so a failed redirect can never reach real data.
    delete m_doc;
    m_doc = nullptr;
}

// ─── create_input_profiles ─────────────────────────────────────────────────

void McpInputProfileTools_Test::createProfile_writesReloadableFile()
{
    Json result = invoke(m_doc, "create_input_profiles",
                         Json{{"items", Json::array({twoChannelProfile("Acme", "Deck 3000")})}});

    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QCOMPARE(result[0].value("status", std::string()), std::string("created"));

    const QString path = QString::fromStdString(result[0].value("path", std::string()));
    QVERIFY(QFile::exists(path));

    // The written file has to be something QLC+ can load back, not just bytes.
    QLCInputProfile *loaded = QLCInputProfile::loader(path);
    QVERIFY(loaded != NULL);
    QCOMPARE(loaded->manufacturer(), QString("Acme"));
    QCOMPARE(loaded->model(), QString("Deck 3000"));
    QCOMPARE(loaded->channels().count(), 2);
    delete loaded;
}

void McpInputProfileTools_Test::createProfile_channelNumbersAreOneBased()
{
    Json result = invoke(m_doc, "create_input_profiles",
                         Json{{"items", Json::array({twoChannelProfile("Acme", "Deck 3000")})}});

    QLCInputProfile *loaded =
        QLCInputProfile::loader(QString::fromStdString(result[0].value("path", std::string())));
    QVERIFY(loaded != NULL);

    // The UI shows channel 1; the profile stores it at index 0. Getting this
    // wrong shifts every mapping by one.
    QVERIFY(loaded->channel(0) != NULL);
    QCOMPARE(loaded->channel(0)->name(), QString("Fader 1"));
    QVERIFY(loaded->channel(1) != NULL);
    QCOMPARE(loaded->channel(1)->name(), QString("Pad 1"));
    delete loaded;
}

void McpInputProfileTools_Test::createProfile_channelTypes_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<int>("expected");

    QTest::newRow("slider")   << QStringLiteral("Slider")  << true  << (int)QLCInputChannel::Slider;
    QTest::newRow("knob")     << QStringLiteral("Knob")    << true  << (int)QLCInputChannel::Knob;
    QTest::newRow("encoder")  << QStringLiteral("Encoder") << true  << (int)QLCInputChannel::Encoder;
    QTest::newRow("button")   << QStringLiteral("Button")  << true  << (int)QLCInputChannel::Button;
    QTest::newRow("nextPage") << QStringLiteral("NextPage")<< true  << (int)QLCInputChannel::NextPage;
    QTest::newRow("lowercase")<< QStringLiteral("button")  << true  << (int)QLCInputChannel::Button;
    QTest::newRow("bogus")    << QStringLiteral("Wheel")   << false << 0;
}

void McpInputProfileTools_Test::createProfile_channelTypes()
{
    QFETCH(QString, input);
    QFETCH(bool, valid);
    QFETCH(int, expected);

    Json result = invoke(m_doc, "create_input_profiles", Json{{"items", Json::array({
        {{"manufacturer", "Acme"}, {"model", "Types"}, {"channels", Json::array({
            {{"number", 1}, {"name", "Control"}, {"type", input.toStdString()}}
        })}}
    })}});

    if (!valid)
    {
        QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
        return;
    }

    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QLCInputProfile *loaded =
        QLCInputProfile::loader(QString::fromStdString(result[0].value("path", std::string())));
    QVERIFY(loaded != NULL);
    QCOMPARE((int)loaded->channel(0)->type(), expected);
    delete loaded;
}

void McpInputProfileTools_Test::createProfile_duplicateChannelNumber_rejectedWholesale()
{
    Json result = invoke(m_doc, "create_input_profiles", Json{{"items", Json::array({
        {{"manufacturer", "Acme"}, {"model", "Dupes"}, {"channels", Json::array({
            {{"number", 1}, {"name", "First"}, {"type", "Slider"}},
            {{"number", 1}, {"name", "Second"}, {"type", "Button"}}
        })}}
    })}});

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
    // Nothing may be written or registered for a rejected profile.
    QVERIFY(!result[0].contains("path"));
    QVERIFY(m_doc->inputOutputMap()->profile("Acme Dupes") == NULL);
}

void McpInputProfileTools_Test::createProfile_emptyChannels_rejected()
{
    Json result = invoke(m_doc, "create_input_profiles", Json{{"items", Json::array({
        {{"manufacturer", "Acme"}, {"model", "Empty"}, {"channels", Json::array()}}
    })}});

    QVERIFY2(result[0].contains("error"), result[0].dump().c_str());
}

void McpInputProfileTools_Test::createProfile_upsertsByManufacturerAndModel()
{
    invoke(m_doc, "create_input_profiles",
           Json{{"items", Json::array({twoChannelProfile("Acme", "Deck 3000")})}});

    Json second = invoke(m_doc, "create_input_profiles", Json{{"items", Json::array({
        {{"manufacturer", "Acme"}, {"model", "Deck 3000"}, {"channels", Json::array({
            {{"number", 1}, {"name", "Renamed"}, {"type", "Knob"}},
            {{"number", 2}, {"name", "Pad 1"}, {"type", "Button"}},
            {{"number", 3}, {"name", "Extra"}, {"type", "Button"}}
        })}}
    })}});

    QCOMPARE(second[0].value("status", std::string()), std::string("updated"));

    QLCInputProfile *loaded =
        QLCInputProfile::loader(QString::fromStdString(second[0].value("path", std::string())));
    QVERIFY(loaded != NULL);
    QCOMPARE(loaded->channels().count(), 3);
    QCOMPARE(loaded->channel(0)->name(), QString("Renamed"));
    delete loaded;
}

void McpInputProfileTools_Test::createProfile_updateReplacesInMemoryProfile()
{
    invoke(m_doc, "create_input_profiles",
           Json{{"items", Json::array({twoChannelProfile("Acme", "Deck 3000")})}});

    invoke(m_doc, "create_input_profiles", Json{{"items", Json::array({
        {{"manufacturer", "Acme"}, {"model", "Deck 3000"}, {"channels", Json::array({
            {{"number", 1}, {"name", "Renamed"}, {"type", "Knob"}},
            {{"number", 2}, {"name", "Pad 1"}, {"type", "Button"}},
            {{"number", 3}, {"name", "Extra"}, {"type", "Button"}}
        })}}
    })}});

    // Writing only the file would leave set_input_profile handing out the stale
    // channel map until the next restart.
    QLCInputProfile *registered = m_doc->inputOutputMap()->profile("Acme Deck 3000");
    QVERIFY(registered != NULL);
    QCOMPARE(registered->channels().count(), 3);
    QCOMPARE(registered->channel(0)->name(), QString("Renamed"));

    Json read = invoke(m_doc, "query_input_profile_channels",
                       Json{{"profileName", "Acme Deck 3000"}});
    QCOMPARE(read["channels"].size(), (size_t)3);
}

void McpInputProfileTools_Test::createProfile_distinctPairsDoNotCollide()
{
    Json first = invoke(m_doc, "create_input_profiles",
                        Json{{"items", Json::array({twoChannelProfile("Acme-X", "Y")})}});
    Json second = invoke(m_doc, "create_input_profiles",
                         Json{{"items", Json::array({twoChannelProfile("Acme", "X-Y")})}});

    // Naive sanitising maps both to Acme-X-Y.qxi, so the second would clobber
    // the first and one profile would vanish on the next start.
    const std::string a = first[0].value("path", std::string());
    const std::string b = second[0].value("path", std::string());
    QVERIFY2(a != b, a.c_str());
    QVERIFY(QFile::exists(QString::fromStdString(a)));
    QVERIFY(QFile::exists(QString::fromStdString(b)));
}

void McpInputProfileTools_Test::createProfile_badMovement_isPerItemError()
{
    Json result = invoke(m_doc, "create_input_profiles", Json{{"items", Json::array({
        twoChannelProfile("Acme", "Good"),
        {{"manufacturer", "Acme"}, {"model", "Bad"}, {"channels", Json::array({
            {{"number", 1}, {"name", "x"}, {"type", "Slider"}, {"movement", 5}}
        })}}
    })}});

    // A mistyped field must not abort the batch after the first profile was
    // already written and registered.
    QCOMPARE(result.size(), (size_t)2);
    QVERIFY2(!result[0].contains("error"), result[0].dump().c_str());
    QVERIFY2(result[1].contains("error"), result[1].dump().c_str());
    QVERIFY(m_doc->inputOutputMap()->profile("Acme Good") != NULL);
    QVERIFY(m_doc->inputOutputMap()->profile("Acme Bad") == NULL);
}

void McpInputProfileTools_Test::createProfile_registeredForSetInputProfile()
{
    invoke(m_doc, "create_input_profiles",
           Json{{"items", Json::array({twoChannelProfile("Acme", "Deck 3000")})}});

    // The point of authoring a profile is being able to assign it afterwards.
    QLCInputProfile *registered = m_doc->inputOutputMap()->profile("Acme Deck 3000");
    QVERIFY2(registered != NULL,
             qPrintable(m_doc->inputOutputMap()->profileNames().join(", ")));
    QCOMPARE(registered->channels().count(), 2);
}

// ─── query_input_profile_channels ──────────────────────────────────────────

void McpInputProfileTools_Test::queryProfileChannels_reportsMap()
{
    invoke(m_doc, "create_input_profiles",
           Json{{"items", Json::array({twoChannelProfile("Acme", "Deck 3000")})}});

    Json result = invoke(m_doc, "query_input_profile_channels",
                         Json{{"profileName", "Acme Deck 3000"}});

    QVERIFY2(!result.contains("error"), result.dump().c_str());
    QCOMPARE(result["channels"].size(), (size_t)2);
    QCOMPARE(result["channels"][0].value("number", -1), 1);
    QCOMPARE(result["channels"][0].value("name", std::string()), std::string("Fader 1"));
    QCOMPARE(result["channels"][1].value("number", -1), 2);
    QCOMPARE(result["channels"][1].value("name", std::string()), std::string("Pad 1"));
}

void McpInputProfileTools_Test::queryProfileChannels_unknownProfile_error()
{
    Json result = invoke(m_doc, "query_input_profile_channels",
                         Json{{"profileName", "Nothing Here"}});

    QVERIFY2(result.contains("error"), result.dump().c_str());
}

QTEST_MAIN(McpInputProfileTools_Test)
