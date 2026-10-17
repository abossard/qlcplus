/*
  Q Light Controller Plus - Unit test
*/

#include <QtTest>

#include <algorithm>
#include <fastmcpp/mcp/handler.hpp>
#include <fastmcpp/prompts/manager.hpp>
#include <fastmcpp/resources/manager.hpp>
#include <fastmcpp/server/server.hpp>
#include <fastmcpp/tools/manager.hpp>
#include <nlohmann/json.hpp>

#include "vc_authoring_query_tools_test.h"
#include "doc.h"
#include "tool_registry.h"
#include "vcbridge.h"

using Json = nlohmann::json;

namespace {

class CaptureAuthoringBridge final : public VCBridge
{
public:
    mutable int queryCalls = 0;
    mutable std::optional<AuthoringQuery> lastQuery;
    AuthoringReadResult nextReadResult;

    CaptureAuthoringBridge()
    {
        nextReadResult.complete = true;
        nextReadResult.basis = QStringLiteral("test-basis");
        nextReadResult.basisScope = QStringLiteral("vc");
    }

    AuthoringReadResult queryAuthoring(const AuthoringQuery &query) const override
    {
        ++queryCalls;
        lastQuery = query;
        return nextReadResult;
    }

    int addPage(const QString &) override { return -1; }
    QList<PageInfo> pages() const override { return {}; }
    int pagesCount() const override { return 0; }
    int addFrame(int, const QRect &, const QString &, bool) override { return -1; }
    int addButton(int, const QRect &, quint32, const QString &, const QString &, int) override { return -1; }
    int addSlider(int, const QRect &, const QString &, const QString &, quint32,
                  const QList<QPair<quint32, quint32>> &) override { return -1; }
    int addXYPad(int, const QRect &, const QList<quint32> &) override { return -1; }
    int addCueList(int, const QRect &, quint32, const QString &) override { return -1; }
    int addLabel(int, const QRect &, const QString &) override { return -1; }
    bool mapWidgetInput(int, quint32, quint32) override { return false; }
    bool setWidgetFeedback(int, int, int, int, int, int, int) override { return false; }
    bool setWidgetColors(int, const QColor &, const QColor &) override { return false; }
    int addSpeedDial(int, const QRect &, const QList<quint32> &) override { return -1; }
    int addAudioTriggers(int, const QRect &) override { return -1; }
    int addClock(int, const QRect &, const QString &) override { return -1; }
    int addRecordPanel(int, const QRect &) override { return -1; }
};

Json parseInvoke(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

fastmcpp::tools::ToolManager makeToolManager(Doc *doc, VCBridge *bridge)
{
    fastmcpp::tools::ToolManager tm;
    registerQueryTools(tm, doc, bridge);
    return tm;
}

bool hasTool(const fastmcpp::tools::ToolManager &tm, const char *name)
{
    const auto names = tm.list_names();
    return std::find(names.begin(), names.end(), std::string(name)) != names.end();
}

QString repeatedString(const QString &s, int count)
{
    QString out;
    out.reserve(s.size() * count);
    for (int i = 0; i < count; ++i)
        out += s;
    return out;
}

}

void VCAuthoringQueryTools_Test::init()
{
    m_doc = new Doc(this);
}

void VCAuthoringQueryTools_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void VCAuthoringQueryTools_Test::toolSurface_discoveryAndMetadata_data()
{
    QTest::addColumn<QString>("toolName");

    QTest::newRow("setup") << QStringLiteral("vc_query_setup");
    QTest::newRow("recordings") << QStringLiteral("vc_query_recordings");
}

void VCAuthoringQueryTools_Test::toolSurface_discoveryAndMetadata()
{
    QFETCH(QString, toolName);

    CaptureAuthoringBridge bridge;
    auto tm = makeToolManager(m_doc, &bridge);
    const std::string name = toolName.toStdString();

    QVERIFY2(hasTool(tm, name.c_str()), qPrintable(QStringLiteral("tool not registered: %1").arg(toolName)));
    const auto &tool = tm.get(name);
    QCOMPARE(tool.name(), name);
    QVERIFY(tool.description().has_value());
    QVERIFY(!tool.description()->empty());
    QVERIFY(tool.annotations().has_value());
    QCOMPARE(*tool.annotations(), mcp::kAnnotReadOnly);

    const Json schema = tool.input_schema();
    QVERIFY2(schema.is_object(), schema.dump().c_str());
    QVERIFY2(schema.dump().find("\"oneOf\"") == std::string::npos, schema.dump().c_str());
    QVERIFY2(schema.dump().find("\"anyOf\"") == std::string::npos, schema.dump().c_str());
    QVERIFY2(schema.dump().find("\"allOf\"") == std::string::npos, schema.dump().c_str());
}

void VCAuthoringQueryTools_Test::toolSurface_setupSchemaContract()
{
    CaptureAuthoringBridge bridge;
    auto tm = makeToolManager(m_doc, &bridge);
    QVERIFY2(hasTool(tm, "vc_query_setup"), "tool not registered: vc_query_setup");
    const Json schema = tm.input_schema_for("vc_query_setup");
    QVERIFY2(schema.is_object(), schema.dump().c_str());

    const Json &props = schema.at("properties");
    QVERIFY(props.contains("kind"));
    QVERIFY(props.contains("pageWidgetID"));
    QVERIFY(props.contains("frameWidgetID"));
    QVERIFY(props.contains("childSubpage"));
    QVERIFY(props.contains("widgetIDs"));
    QVERIFY(props.contains("recursive"));
    QVERIFY(props.contains("nameFilter"));
    QVERIFY(props.contains("typeFilter"));
    QVERIFY(props.contains("fields"));
    QVERIFY(props.contains("limit"));
    QVERIFY(props.contains("cursor"));

    QCOMPARE(props.at("kind").at("type").get<std::string>(), std::string("string"));
    QCOMPARE(props.at("kind").at("enum"),
             Json::array({"widgets", "pages", "subpages"}));
    QCOMPARE(props.at("limit").at("type").get<std::string>(), std::string("integer"));
    QCOMPARE(props.at("limit").at("minimum").get<int>(), 1);
    QCOMPARE(props.at("limit").at("maximum").get<int>(), 250);
    QCOMPARE(props.at("cursor").at("type").get<std::string>(), std::string("string"));
    QCOMPARE(props.at("recursive").at("type").get<std::string>(), std::string("boolean"));
    QCOMPARE(props.at("widgetIDs").at("type").get<std::string>(), std::string("array"));
    QCOMPARE(props.at("widgetIDs").at("items").at("type").get<std::string>(), std::string("integer"));
    QCOMPARE(props.at("fields").at("type").get<std::string>(), std::string("array"));
    QCOMPARE(props.at("fields").at("items").at("type").get<std::string>(), std::string("string"));
    QVERIFY(props.at("fields").at("items").contains("enum"));
    QCOMPARE(props.at("fields").at("items").at("enum"),
             Json::array({"identity", "placement", "logicalGeometry", "functionBinding",
                          "inputBindings", "feedback", "recordPanel", "appearance",
                          "frameConfiguration"}));
    QVERIFY(!schema.contains("required") || schema.at("required").empty());
}

void VCAuthoringQueryTools_Test::toolSurface_recordingsSchemaContract()
{
    CaptureAuthoringBridge bridge;
    auto tm = makeToolManager(m_doc, &bridge);
    QVERIFY2(hasTool(tm, "vc_query_recordings"), "tool not registered: vc_query_recordings");
    const Json schema = tm.input_schema_for("vc_query_recordings");
    QVERIFY2(schema.is_object(), schema.dump().c_str());

    const Json &props = schema.at("properties");
    QVERIFY(props.contains("showFunctionID"));
    QVERIFY(props.contains("kind"));
    QVERIFY(props.contains("controlRecordingUUID"));
    QVERIFY(props.contains("fromTimeMs"));
    QVERIFY(props.contains("toTimeMs"));
    QVERIFY(props.contains("fields"));
    QVERIFY(props.contains("limit"));
    QVERIFY(props.contains("cursor"));

    QCOMPARE(props.at("showFunctionID").at("type").get<std::string>(), std::string("integer"));
    QCOMPARE(props.at("kind").at("type").get<std::string>(), std::string("string"));
    QCOMPARE(props.at("kind").at("enum"), Json::array({"references", "commands"}));
    QCOMPARE(props.at("limit").at("minimum").get<int>(), 1);
    QCOMPARE(props.at("limit").at("maximum").get<int>(), 250);
    QCOMPARE(props.at("cursor").at("type").get<std::string>(), std::string("string"));
    QCOMPARE(props.at("fields").at("type").get<std::string>(), std::string("array"));
    QCOMPARE(props.at("fields").at("items").at("type").get<std::string>(), std::string("string"));
    QVERIFY(props.at("fields").at("items").contains("enum"));
    QCOMPARE(props.at("fields").at("items").at("enum"),
             Json::array({"identity", "timing", "semanticPayload", "readiness"}));

    QVERIFY(schema.contains("required"));
    QCOMPARE(schema.at("required"), Json::array({"showFunctionID"}));
}

void VCAuthoringQueryTools_Test::vcQuerySetup_parsesTypedQuery_data()
{
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<QString>("expectedKind");
    QTest::addColumn<int>("expectedLimit");
    QTest::addColumn<bool>("expectedRecursive");
    QTest::addColumn<int>("expectedPageWidgetID");
    QTest::addColumn<int>("expectedFrameWidgetID");
    QTest::addColumn<int>("expectedChildSubpage");
    QTest::addColumn<int>("expectedWidgetIDCount");
    QTest::addColumn<QStringList>("expectedFields");

    QTest::newRow("defaults")
        << QByteArray("{}")
        << QStringLiteral("widgets") << 100 << true
        << -1 << -1 << -1 << 0 << QStringList();

    QTest::newRow("pages kind")
        << QByteArray(R"({"kind":"pages","limit":250,"cursor":"c1","fields":["identity","placement"],"pageWidgetID":12})")
        << QStringLiteral("pages") << 250 << true
        << 12 << -1 << -1 << 0 << QStringList{QStringLiteral("identity"), QStringLiteral("placement")};

    QTest::newRow("subpages with frame selector")
        << QByteArray(R"({"kind":"subpages","frameWidgetID":77,"childSubpage":3,"fields":["identity","placement"]})")
        << QStringLiteral("subpages") << 100 << true
        << -1 << 77 << 3 << 0 << QStringList{QStringLiteral("identity"), QStringLiteral("placement")};

    QTest::newRow("widgets explicit scope")
        << QByteArray(R"({"kind":"widgets","pageWidgetID":11,"widgetIDs":[2,3,4],"recursive":false,"fields":["identity","logicalGeometry","inputBindings"],"limit":25,"cursor":"after"})")
        << QStringLiteral("widgets") << 25 << false
        << 11 << -1 << -1 << 3 << QStringList{QStringLiteral("identity"), QStringLiteral("logicalGeometry"), QStringLiteral("inputBindings")};

    QTest::newRow("cursor 1024 utf8 bytes accepted")
        << QString("{\"kind\":\"widgets\",\"cursor\":\"%1\"}")
              .arg(repeatedString(QStringLiteral("é"), 512)).toUtf8()
        << QStringLiteral("widgets") << 100 << true
        << -1 << -1 << -1 << 0 << QStringList();

    QTest::newRow("limit lower bound accepted")
        << QByteArray(R"({"kind":"widgets","limit":1})")
        << QStringLiteral("widgets") << 1 << true
        << -1 << -1 << -1 << 0 << QStringList();
}

void VCAuthoringQueryTools_Test::vcQuerySetup_parsesTypedQuery()
{
    QFETCH(QByteArray, argsJson);
    QFETCH(QString, expectedKind);
    QFETCH(int, expectedLimit);
    QFETCH(bool, expectedRecursive);
    QFETCH(int, expectedPageWidgetID);
    QFETCH(int, expectedFrameWidgetID);
    QFETCH(int, expectedChildSubpage);
    QFETCH(int, expectedWidgetIDCount);
    QFETCH(QStringList, expectedFields);

    CaptureAuthoringBridge bridge;
    auto tm = makeToolManager(m_doc, &bridge);
    QVERIFY2(hasTool(tm, "vc_query_setup"), "tool not registered: vc_query_setup");

    const Json result = parseInvoke(tm.invoke("vc_query_setup", Json::parse(argsJson.constData())));
    QVERIFY2(result.is_object(), result.dump().c_str());
    QVERIFY2(!result.contains("error"), result.dump().c_str());
    QCOMPARE(bridge.queryCalls, 1);
    QVERIFY(bridge.lastQuery.has_value());
    QVERIFY(std::holds_alternative<VCBridge::AuthoringSetupQuery>(*bridge.lastQuery));

    const auto &query = std::get<VCBridge::AuthoringSetupQuery>(*bridge.lastQuery);
    if (expectedKind == QStringLiteral("pages"))
        QCOMPARE(query.kind, VCBridge::AuthoringSetupQuery::Kind::Pages);
    else if (expectedKind == QStringLiteral("subpages"))
        QCOMPARE(query.kind, VCBridge::AuthoringSetupQuery::Kind::Subpages);
    else
        QCOMPARE(query.kind, VCBridge::AuthoringSetupQuery::Kind::Widgets);

    QCOMPARE(query.limit, expectedLimit);
    QCOMPARE(query.recursive, expectedRecursive);
    QCOMPARE(int(query.pageWidgetID), expectedPageWidgetID);
    QCOMPARE(int(query.frameWidgetID), expectedFrameWidgetID);
    QCOMPARE(query.childSubpage, expectedChildSubpage);
    QCOMPARE(query.widgetIDs.size(), expectedWidgetIDCount);
    QCOMPARE(query.fields, expectedFields);
}

void VCAuthoringQueryTools_Test::vcQuerySetup_envelopeAndCursorBehavior()
{
    CaptureAuthoringBridge bridge;

    VCBridge::AuthoringReadRow pageRow;
    VCBridge::AuthoringReadRow::SetupPageRow pagePayload;
    pagePayload.pageWidgetID = 17u;
    pagePayload.pageIndex = 3;
    pagePayload.caption = QStringLiteral("Frontline");
    pagePayload.externalInputMode = QStringLiteral("Inherit");
    pagePayload.configurationDigest = QStringLiteral("page-digest-17");
    pageRow.row = pagePayload;

    VCBridge::AuthoringReadRow subpageRow;
    VCBridge::AuthoringReadRow::SetupSubpageRow subpagePayload;
    subpagePayload.frameWidgetID = 44u;
    subpagePayload.childSubpage = 2;
    subpagePayload.label = QStringLiteral("Verse");
    subpagePayload.configurationDigest = QStringLiteral("subpage-digest-44-2");
    subpageRow.row = subpagePayload;

    bridge.nextReadResult.version = 1;
    bridge.nextReadResult.basis = QStringLiteral("basis-setup-1");
    bridge.nextReadResult.basisScope = QStringLiteral("setup");
    bridge.nextReadResult.complete = false;
    bridge.nextReadResult.nextCursor = QStringLiteral("cursor-next-1");
    bridge.nextReadResult.rows = {pageRow, subpageRow};

    auto tm = makeToolManager(m_doc, &bridge);
    QVERIFY2(hasTool(tm, "vc_query_setup"), "tool not registered: vc_query_setup");

    const Json first = parseInvoke(tm.invoke(
        "vc_query_setup", Json::parse(R"({"kind":"pages","limit":1})")));
    QVERIFY2(first.is_object(), first.dump().c_str());
    QVERIFY2(!first.contains("error"), first.dump().c_str());
    QCOMPARE(first.value("version", -1), 1);
    QCOMPARE(first.value("basis", std::string()), std::string("basis-setup-1"));
    QCOMPARE(first.value("basisScope", std::string()), std::string("setup"));
    QCOMPARE(first.value("complete", true), false);
    QCOMPARE(first.value("nextCursor", std::string()), std::string("cursor-next-1"));
    QVERIFY2(!first.contains("code"), first.dump().c_str());
    QVERIFY2(first.contains("rows") && first["rows"].is_array(), first.dump().c_str());
    QCOMPARE(first["rows"].size(), size_t(2));
    QCOMPARE(first["rows"].at(0).value("kind", std::string()), std::string("setupPage"));
    QCOMPARE(first["rows"].at(0).value("pageWidgetID", 0), 17);
    QCOMPARE(first["rows"].at(1).value("kind", std::string()), std::string("setupSubpage"));
    QCOMPARE(first["rows"].at(1).value("childSubpage", -1), 2);
    QCOMPARE(bridge.queryCalls, 1);
    QVERIFY(bridge.lastQuery.has_value());
    QVERIFY(std::holds_alternative<VCBridge::AuthoringSetupQuery>(*bridge.lastQuery));
    const auto firstQuery = std::get<VCBridge::AuthoringSetupQuery>(*bridge.lastQuery);
    QCOMPARE(firstQuery.kind, VCBridge::AuthoringSetupQuery::Kind::Pages);
    QCOMPARE(firstQuery.limit, 1);

    VCBridge::AuthoringLimitation limitation;
    limitation.code = QStringLiteral("cursorConflict");
    limitation.scope = QStringLiteral("cursor");
    limitation.message = QStringLiteral("selector mismatch");

    bridge.nextReadResult.complete = false;
    bridge.nextReadResult.nextCursor.clear();
    bridge.nextReadResult.rows.clear();
    bridge.nextReadResult.code = QStringLiteral("invalidCursor");
    bridge.nextReadResult.error = QStringLiteral("Malformed continuation cursor");
    bridge.nextReadResult.limitations = {limitation};

    const Json second = parseInvoke(tm.invoke(
        "vc_query_setup", Json::parse(R"({"kind":"pages","cursor":"bogus"})")));
    QVERIFY2(second.is_object(), second.dump().c_str());
    QCOMPARE(second.value("code", std::string()), std::string("invalidCursor"));
    QCOMPARE(second.value("error", std::string()), std::string("Malformed continuation cursor"));
    QVERIFY2(second.contains("limitations") && second["limitations"].is_array(), second.dump().c_str());
    QCOMPARE(second["limitations"].size(), size_t(1));
    QCOMPARE(second["limitations"].at(0).value("scope", std::string()), std::string("cursor"));
    QCOMPARE(second["rows"].size(), size_t(0));
    QCOMPARE(second.value("nextCursor", std::string()), std::string());
    QCOMPARE(second.value("complete", true), false);
}

void VCAuthoringQueryTools_Test::vcQuerySetup_rejectsInvalidArgs_data()
{
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<QString>("errorPart");

    const QString longCursor = repeatedString(QStringLiteral("x"), 1025);

    QTest::newRow("unknown field")
        << QByteArray(R"({"bogus":1})")
        << QStringLiteral("unknown");
    QTest::newRow("invalid kind")
        << QByteArray(R"({"kind":"bad"})")
        << QStringLiteral("kind");
    QTest::newRow("limit too small")
        << QByteArray(R"({"limit":0})")
        << QStringLiteral("limit");
    QTest::newRow("limit too large")
        << QByteArray(R"({"limit":251})")
        << QStringLiteral("limit");
    QTest::newRow("limit wrap above uint32")
        << QByteArray(R"({"limit":4294967396})")
        << QStringLiteral("limit");
    QTest::newRow("cursor not string")
        << QByteArray(R"({"cursor":5})")
        << QStringLiteral("cursor");
    QTest::newRow("cursor too long")
        << QString("{\"cursor\":\"%1\"}").arg(longCursor).toUtf8()
        << QStringLiteral("cursor");
    QTest::newRow("cursor too long multibyte")
        << QString("{\"cursor\":\"%1\"}")
              .arg(repeatedString(QStringLiteral("é"), 513)).toUtf8()
        << QStringLiteral("cursor");
    QTest::newRow("pageWidgetID max sentinel rejected")
        << QByteArray(R"({"pageWidgetID":4294967295})")
        << QStringLiteral("pageWidgetID");
    QTest::newRow("pageWidgetID overflow rejected")
        << QByteArray(R"({"pageWidgetID":4294967296})")
        << QStringLiteral("pageWidgetID");
    QTest::newRow("frameWidgetID max sentinel rejected")
        << QByteArray(R"({"frameWidgetID":4294967295})")
        << QStringLiteral("frameWidgetID");
    QTest::newRow("frameWidgetID overflow rejected")
        << QByteArray(R"({"frameWidgetID":4294967296})")
        << QStringLiteral("frameWidgetID");
    QTest::newRow("childSubpage max sentinel rejected")
        << QByteArray(R"({"childSubpage":4294967295,"frameWidgetID":2})")
        << QStringLiteral("childSubpage");
    QTest::newRow("page and frame scopes conflict")
        << QByteArray(R"({"pageWidgetID":1,"frameWidgetID":2})")
        << QStringLiteral("exclusive");
    QTest::newRow("pages rejects frameWidgetID")
        << QByteArray(R"({"kind":"pages","frameWidgetID":2})")
        << QStringLiteral("pages");
    QTest::newRow("pages rejects childSubpage")
        << QByteArray(R"({"kind":"pages","childSubpage":0})")
        << QStringLiteral("childSubpage");
    QTest::newRow("pages rejects widgetIDs")
        << QByteArray(R"({"kind":"pages","widgetIDs":[2]})")
        << QStringLiteral("pages");
    QTest::newRow("pages rejects recursive")
        << QByteArray(R"({"kind":"pages","recursive":false})")
        << QStringLiteral("pages");
    QTest::newRow("pages rejects nameFilter")
        << QByteArray(R"({"kind":"pages","nameFilter":"FX*"})")
        << QStringLiteral("pages");
    QTest::newRow("pages rejects typeFilter")
        << QByteArray(R"({"kind":"pages","typeFilter":"button"})")
        << QStringLiteral("pages");
    QTest::newRow("pages rejects widget-only field group")
        << QByteArray(R"({"kind":"pages","fields":["logicalGeometry"]})")
        << QStringLiteral("fields");
    QTest::newRow("subpages needs frame")
        << QByteArray(R"({"kind":"subpages"})")
        << QStringLiteral("frameWidgetID");
    QTest::newRow("subpages rejects pageWidgetID")
        << QByteArray(R"({"kind":"subpages","frameWidgetID":2,"pageWidgetID":1})")
        << QStringLiteral("exclusive");
    QTest::newRow("subpages rejects widgetIDs")
        << QByteArray(R"({"kind":"subpages","frameWidgetID":2,"widgetIDs":[3]})")
        << QStringLiteral("subpages");
    QTest::newRow("subpages rejects recursive")
        << QByteArray(R"({"kind":"subpages","frameWidgetID":2,"recursive":false})")
        << QStringLiteral("subpages");
    QTest::newRow("subpages rejects nameFilter")
        << QByteArray(R"({"kind":"subpages","frameWidgetID":2,"nameFilter":"FX*"})")
        << QStringLiteral("subpages");
    QTest::newRow("subpages rejects typeFilter")
        << QByteArray(R"({"kind":"subpages","frameWidgetID":2,"typeFilter":"button"})")
        << QStringLiteral("subpages");
    QTest::newRow("subpages rejects widget-only field group")
        << QByteArray(R"({"kind":"subpages","frameWidgetID":2,"fields":["inputBindings"]})")
        << QStringLiteral("fields");
    QTest::newRow("childSubpage without frame")
        << QByteArray(R"({"childSubpage":0})")
        << QStringLiteral("frameWidgetID");
    QTest::newRow("widget id negative")
        << QByteArray(R"({"widgetIDs":[-1]})")
        << QStringLiteral("widgetIDs");
    QTest::newRow("widget ids empty array")
        << QByteArray(R"({"widgetIDs":[]})")
        << QStringLiteral("widgetIDs");
    QTest::newRow("pageWidgetID float")
        << QByteArray(R"({"pageWidgetID":1.5})")
        << QStringLiteral("pageWidgetID");
    QTest::newRow("frameWidgetID bool")
        << QByteArray(R"({"frameWidgetID":true})")
        << QStringLiteral("frameWidgetID");
    QTest::newRow("childSubpage string")
        << QByteArray(R"({"childSubpage":"0","frameWidgetID":2})")
        << QStringLiteral("childSubpage");
    QTest::newRow("fields wrong type")
        << QByteArray(R"({"fields":"caption"})")
        << QStringLiteral("fields");
    QTest::newRow("fields unknown")
        << QByteArray(R"({"fields":["notAField"]})")
        << QStringLiteral("fields");
    QTest::newRow("fields legacy pageWidgetID rejected")
        << QByteArray(R"({"fields":["pageWidgetID"]})")
        << QStringLiteral("fields");
    QTest::newRow("fields legacy caption rejected")
        << QByteArray(R"({"fields":["caption"]})")
        << QStringLiteral("fields");
    QTest::newRow("fields legacy widgetID rejected")
        << QByteArray(R"({"fields":["widgetID"]})")
        << QStringLiteral("fields");
}

void VCAuthoringQueryTools_Test::vcQuerySetup_rejectsInvalidArgs()
{
    QFETCH(QByteArray, argsJson);
    QFETCH(QString, errorPart);

    CaptureAuthoringBridge bridge;
    auto tm = makeToolManager(m_doc, &bridge);
    QVERIFY2(hasTool(tm, "vc_query_setup"), "tool not registered: vc_query_setup");
    const Json result = parseInvoke(tm.invoke("vc_query_setup", Json::parse(argsJson.constData())));

    QVERIFY2(result.is_object(), result.dump().c_str());
    QVERIFY2(result.contains("error"), result.dump().c_str());
    const QString error = QString::fromStdString(result["error"].get<std::string>());
    QVERIFY2(error.contains(errorPart, Qt::CaseInsensitive), qPrintable(error));
    QCOMPARE(result.value("version", -1), 1);
    QCOMPARE(result.value("code", std::string()), std::string("invalidArgument"));
    QVERIFY(result.contains("limitations"));
    QVERIFY(result["limitations"].is_array());
    QCOMPARE(result["limitations"].size(), size_t(0));
    QCOMPARE(bridge.queryCalls, 0);
}

void VCAuthoringQueryTools_Test::vcQueryRecordings_parsesTypedQuery_data()
{
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<QString>("expectedKind");
    QTest::addColumn<int>("expectedLimit");
    QTest::addColumn<QString>("expectedUUID");
    QTest::addColumn<qint64>("expectedFrom");
    QTest::addColumn<qint64>("expectedTo");
    QTest::addColumn<QStringList>("expectedFields");

    QTest::newRow("default kind")
        << QByteArray(R"({"showFunctionID":17})")
        << QStringLiteral("references") << 100 << QString() << qint64(-1) << qint64(-1) << QStringList();

    QTest::newRow("commands with window")
        << QByteArray(R"({"showFunctionID":17,"kind":"commands","controlRecordingUUID":"550e8400-e29b-41d4-a716-446655440000","fromTimeMs":0,"toTimeMs":9000,"fields":["identity","semanticPayload"],"limit":250,"cursor":"next"})")
        << QStringLiteral("commands") << 250 << QStringLiteral("550e8400-e29b-41d4-a716-446655440000")
        << qint64(0) << qint64(9000) << QStringList{QStringLiteral("identity"), QStringLiteral("semanticPayload")};

    QTest::newRow("equal time bounds allowed")
        << QByteArray(R"({"showFunctionID":17,"kind":"commands","fromTimeMs":1200,"toTimeMs":1200,"fields":["timing"]})")
        << QStringLiteral("commands") << 100 << QString()
        << qint64(1200) << qint64(1200) << QStringList{QStringLiteral("timing")};

    QTest::newRow("limit lower bound accepted")
        << QByteArray(R"({"showFunctionID":17,"limit":1})")
        << QStringLiteral("references") << 1 << QString()
        << qint64(-1) << qint64(-1) << QStringList();

    QTest::newRow("cursor 1024 utf8 bytes accepted")
        << QString("{\"showFunctionID\":17,\"cursor\":\"%1\"}")
              .arg(repeatedString(QStringLiteral("é"), 512)).toUtf8()
        << QStringLiteral("references") << 100 << QString()
        << qint64(-1) << qint64(-1) << QStringList();
}

void VCAuthoringQueryTools_Test::vcQueryRecordings_parsesTypedQuery()
{
    QFETCH(QByteArray, argsJson);
    QFETCH(QString, expectedKind);
    QFETCH(int, expectedLimit);
    QFETCH(QString, expectedUUID);
    QFETCH(qint64, expectedFrom);
    QFETCH(qint64, expectedTo);
    QFETCH(QStringList, expectedFields);

    CaptureAuthoringBridge bridge;
    auto tm = makeToolManager(m_doc, &bridge);
    QVERIFY2(hasTool(tm, "vc_query_recordings"), "tool not registered: vc_query_recordings");
    const Json result = parseInvoke(tm.invoke("vc_query_recordings", Json::parse(argsJson.constData())));

    QVERIFY2(result.is_object(), result.dump().c_str());
    QVERIFY2(!result.contains("error"), result.dump().c_str());
    QCOMPARE(bridge.queryCalls, 1);
    QVERIFY(bridge.lastQuery.has_value());
    QVERIFY(std::holds_alternative<VCBridge::AuthoringRecordingQuery>(*bridge.lastQuery));

    const auto &query = std::get<VCBridge::AuthoringRecordingQuery>(*bridge.lastQuery);
    if (expectedKind == QStringLiteral("commands"))
        QCOMPARE(query.kind, VCBridge::AuthoringRecordingQuery::Kind::Commands);
    else
        QCOMPARE(query.kind, VCBridge::AuthoringRecordingQuery::Kind::References);
    QCOMPARE(query.limit, expectedLimit);
    QCOMPARE(query.controlRecordingUUID, expectedUUID);
    QCOMPARE(query.fromTimeMs, expectedFrom);
    QCOMPARE(query.toTimeMs, expectedTo);
    QCOMPARE(query.fields, expectedFields);
}

void VCAuthoringQueryTools_Test::vcQueryRecordings_rejectsInvalidArgs_data()
{
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<QString>("errorPart");

    const QString longCursor = repeatedString(QStringLiteral("r"), 1025);

    QTest::newRow("missing showFunctionID")
        << QByteArray(R"({"kind":"references"})")
        << QStringLiteral("showFunctionID");
    QTest::newRow("showFunctionID negative")
        << QByteArray(R"({"showFunctionID":-1})")
        << QStringLiteral("showFunctionID");
    QTest::newRow("invalid kind")
        << QByteArray(R"({"showFunctionID":1,"kind":"bad"})")
        << QStringLiteral("kind");
    QTest::newRow("limit too large")
        << QByteArray(R"({"showFunctionID":1,"limit":251})")
        << QStringLiteral("limit");
    QTest::newRow("limit wrap above uint32")
        << QByteArray(R"({"showFunctionID":1,"limit":4294967396})")
        << QStringLiteral("limit");
    QTest::newRow("cursor too long")
        << QString("{\"showFunctionID\":1,\"cursor\":\"%1\"}").arg(longCursor).toUtf8()
        << QStringLiteral("cursor");
    QTest::newRow("cursor too long multibyte")
        << QString("{\"showFunctionID\":1,\"cursor\":\"%1\"}")
              .arg(repeatedString(QStringLiteral("é"), 513)).toUtf8()
        << QStringLiteral("cursor");
    QTest::newRow("showFunctionID max sentinel rejected")
        << QByteArray(R"({"showFunctionID":4294967295})")
        << QStringLiteral("showFunctionID");
    QTest::newRow("showFunctionID overflow rejected")
        << QByteArray(R"({"showFunctionID":4294967296})")
        << QStringLiteral("showFunctionID");
    QTest::newRow("unknown field")
        << QByteArray(R"({"showFunctionID":1,"widgetIDs":[2]})")
        << QStringLiteral("unknown");
    QTest::newRow("showFunctionID float rejected")
        << QByteArray(R"({"showFunctionID":1.2})")
        << QStringLiteral("showFunctionID");
    QTest::newRow("showFunctionID bool rejected")
        << QByteArray(R"({"showFunctionID":true})")
        << QStringLiteral("showFunctionID");
    QTest::newRow("showFunctionID string rejected")
        << QByteArray(R"({"showFunctionID":"1"})")
        << QStringLiteral("showFunctionID");
    QTest::newRow("fromTimeMs wrong type")
        << QByteArray(R"({"showFunctionID":1,"fromTimeMs":"0"})")
        << QStringLiteral("fromTimeMs");
    QTest::newRow("toTimeMs less than from")
        << QByteArray(R"({"showFunctionID":1,"fromTimeMs":50,"toTimeMs":49})")
        << QStringLiteral("time");
    QTest::newRow("fromTimeMs over int64 rejected")
        << QByteArray(R"({"showFunctionID":1,"fromTimeMs":9223372036854775808.0})")
        << QStringLiteral("fromTimeMs");
    QTest::newRow("toTimeMs over int64 rejected")
        << QByteArray(R"({"showFunctionID":1,"toTimeMs":9223372036854775808.0})")
        << QStringLiteral("toTimeMs");
    QTest::newRow("recordings fields legacy eventID rejected")
        << QByteArray(R"({"showFunctionID":1,"fields":["eventID"]})")
        << QStringLiteral("fields");
    QTest::newRow("recordings fields legacy action rejected")
        << QByteArray(R"({"showFunctionID":1,"fields":["action"]})")
        << QStringLiteral("fields");
}

void VCAuthoringQueryTools_Test::vcQueryRecordings_rejectsInvalidArgs()
{
    QFETCH(QByteArray, argsJson);
    QFETCH(QString, errorPart);

    CaptureAuthoringBridge bridge;
    auto tm = makeToolManager(m_doc, &bridge);
    QVERIFY2(hasTool(tm, "vc_query_recordings"), "tool not registered: vc_query_recordings");
    const Json result = parseInvoke(tm.invoke("vc_query_recordings", Json::parse(argsJson.constData())));

    QVERIFY2(result.is_object(), result.dump().c_str());
    QVERIFY2(result.contains("error"), result.dump().c_str());
    const QString error = QString::fromStdString(result["error"].get<std::string>());
    QVERIFY2(error.contains(errorPart, Qt::CaseInsensitive), qPrintable(error));
    QCOMPARE(result.value("version", -1), 1);
    QCOMPARE(result.value("code", std::string()), std::string("invalidArgument"));
    QVERIFY(result.contains("limitations"));
    QVERIFY(result["limitations"].is_array());
    QCOMPARE(result["limitations"].size(), size_t(0));
    QCOMPARE(bridge.queryCalls, 0);
}

void VCAuthoringQueryTools_Test::transport_toolResultEncoding_data()
{
    QTest::addColumn<QString>("tool");
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<bool>("isError");
    QTest::addColumn<bool>("structured");

    QTest::newRow("setup request error")
        << QStringLiteral("vc_query_setup")
        << QByteArray(R"({"limit":0})")
        << true << true;
    QTest::newRow("recordings request error")
        << QStringLiteral("vc_query_recordings")
        << QByteArray(R"({"showFunctionID":"bad"})")
        << true << true;
    QTest::newRow("recordings success object")
        << QStringLiteral("vc_query_recordings")
        << QByteArray(R"({"showFunctionID":7})")
        << false << true;
}

void VCAuthoringQueryTools_Test::transport_toolResultEncoding()
{
    QFETCH(QString, tool);
    QFETCH(QByteArray, argsJson);
    QFETCH(bool, isError);
    QFETCH(bool, structured);

    CaptureAuthoringBridge bridge;
    auto tm = makeToolManager(m_doc, &bridge);
    QVERIFY2(hasTool(tm, tool.toStdString().c_str()),
             qPrintable(QStringLiteral("tool not registered: %1").arg(tool)));
    fastmcpp::server::Server server("qlcplus", "5.0.0");
    fastmcpp::resources::ResourceManager rm;
    fastmcpp::prompts::PromptManager pm;
    auto handler = mcp::withToolResultEncoding(
        fastmcpp::mcp::make_mcp_handler("qlcplus", "5.0.0", server, tm, rm, pm));

    const Json response = handler({{"jsonrpc", "2.0"}, {"id", 99}, {"method", "tools/call"},
        {"params", {{"name", tool.toStdString()}, {"arguments", Json::parse(argsJson.constData())}}}});
    QVERIFY2(response.contains("result"), response.dump().c_str());
    const Json &result = response["result"];
    QVERIFY2(result["content"].is_array() && result["content"].size() == 1, response.dump().c_str());
    const Json payload = Json::parse(result["content"][0]["text"].get<std::string>());
    QVERIFY2(result.contains("isError") && result["isError"] == isError, response.dump().c_str());
    QCOMPARE(result.contains("structuredContent"), structured);
    if (structured)
        QCOMPARE(result["structuredContent"], payload);
    if (isError)
    {
        QCOMPARE(payload.value("version", -1), 1);
        QCOMPARE(payload.value("code", std::string()), std::string("invalidArgument"));
        QVERIFY(payload.contains("error"));
        QVERIFY(payload.contains("limitations"));
        QVERIFY(payload["limitations"].is_array());
    }
}

QTEST_MAIN(VCAuthoringQueryTools_Test)
