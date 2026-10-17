/*
  Q Light Controller Plus - Unit test
  vc_authoring_input_tools_test.cpp
*/

#include <QtTest>
#include <nlohmann/json.hpp>

#include <exception>
#include <optional>

#include "vc_authoring_input_tools_test.h"
#include "tool_registry.h"
#include "vcbridge.h"
#include "doc.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {

class TestVCAuthoringBridge final : public VCBridge
{
public:
    // Baseline VCBridge API
    int addPage(const QString &) override { return 0; }
    QList<PageInfo> pages() const override { return {}; }
    int pagesCount() const override { return 0; }
    int addFrame(int, const QRect &, const QString &, bool) override { return 1; }
    int addButton(int, const QRect &, quint32, const QString &, const QString &, int) override { return 2; }
    int addSlider(int, const QRect &, const QString &, const QString &, quint32,
                  const QList<QPair<quint32, quint32>> &) override { return 3; }
    int addXYPad(int, const QRect &, const QList<quint32> &) override { return 4; }
    int addCueList(int, const QRect &, quint32, const QString &) override { return 5; }
    int addLabel(int, const QRect &, const QString &) override { return 6; }
    bool mapWidgetInput(int, quint32, quint32) override { return true; }
    bool setWidgetFeedback(int, int, int, int, int, int, int) override { return true; }
    bool setWidgetColors(int, const QColor &, const QColor &) override { return true; }
    int addSpeedDial(int, const QRect &, const QList<quint32> &) override { return 7; }
    int addAudioTriggers(int, const QRect &) override { return 8; }
    int addClock(int, const QRect &, const QString &) override { return 9; }
    int addRecordPanel(int, const QRect &) override { return 10; }
    WidgetDetails getWidgetDetails(int) const override { return WidgetDetails(); }
    bool setXYPadPosition(int, qreal, qreal) override { return true; }
    bool setAudioTriggerCapture(int, bool) override { return true; }
    bool setAudioTriggerVolume(int, int) override { return true; }
    bool configureFrame(int, const FrameConfig &) override { return true; }
    int findWidgetByCaption(int, const QString &, const QString &) const override { return -1; }
    bool setWidgetPage(int, int) override { return true; }
    bool configureSlider(int, const SliderConfig &) override { return true; }
    bool setWidgetCaption(int, const QString &) override { return true; }
    bool setButtonAction(int, const QString &) override { return true; }
    bool configureButton(int, const ButtonConfig &) override { return true; }
    int createWidgetOnPage(int, const QString &, const QRect &, const QString &, int) override { return -1; }
    WidgetSnapshot snapshotFrame(int) const override { return WidgetSnapshot(); }
    LayoutEditResult alignLayout(int, int, int, int, bool) override { return LayoutEditResult(); }

    // Authoring capture surface
    AuthoringReadResult queryAuthoring(const AuthoringQuery &query) const override
    {
        ++queryAuthoringCalls;
        lastTargetQuery.reset();
        if (const AuthoringTargetControlQuery *target = std::get_if<AuthoringTargetControlQuery>(&query))
            lastTargetQuery = *target;
        return nextReadResult;
    }

    AuthoringPreparationResult prepareAuthoring(const AuthoringRequest &request) override
    {
        ++prepareAuthoringCalls;
        lastBindingsRequest.reset();
        if (const AuthoringBindingsRequest *bindings = std::get_if<AuthoringBindingsRequest>(&request))
            lastBindingsRequest = *bindings;
        return nextPreparationResult;
    }

    mutable int queryAuthoringCalls = 0;
    int prepareAuthoringCalls = 0;
    mutable std::optional<AuthoringTargetControlQuery> lastTargetQuery;
    std::optional<AuthoringBindingsRequest> lastBindingsRequest;
    AuthoringReadResult nextReadResult;
    AuthoringPreparationResult nextPreparationResult;
};

Json parsedToolResult(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
}

fastmcpp::tools::ToolManager makeInputToolManager(Doc *doc, VCBridge *bridge)
{
    fastmcpp::tools::ToolManager tm;
    registerVCInputTools(tm, doc, bridge);
    return tm;
}

Json invokeNoThrow(fastmcpp::tools::ToolManager &tm, const char *tool, const Json &args, bool &threw)
{
    threw = false;
    try
    {
        return parsedToolResult(tm.invoke(tool, args));
    }
    catch (const std::exception &e)
    {
        threw = true;
        return Json{{"exception", std::string(e.what())}};
    }
    catch (...)
    {
        threw = true;
        return Json{{"exception", "unknown"}};
    }
}

void assertInvalidArgumentEnvelope(const Json &result, const QString &errorPart)
{
    QVERIFY2(result.is_object(), result.dump().c_str());
    QVERIFY2(result.contains("version"), result.dump().c_str());
    QCOMPARE(result.value("version", -1), 1);
    QCOMPARE(QString::fromStdString(result.value("code", std::string())), QStringLiteral("invalidArgument"));
    QVERIFY2(result.contains("error"), result.dump().c_str());
    const QString error = QString::fromStdString(result.value("error", std::string()));
    QVERIFY2(error.contains(errorPart, Qt::CaseInsensitive), qPrintable(error));
    QVERIFY2(result.contains("limitations"), result.dump().c_str());
    QVERIFY2(result.at("limitations").is_array(), result.dump().c_str());
}

bool schemaContainsCombinators(const Json &value)
{
    if (value.is_object())
    {
        if (value.contains("oneOf") || value.contains("anyOf") || value.contains("allOf"))
            return true;
        for (auto it = value.begin(); it != value.end(); ++it)
            if (schemaContainsCombinators(it.value()))
                return true;
        return false;
    }
    if (value.is_array())
    {
        for (const Json &entry : value)
            if (schemaContainsCombinators(entry))
                return true;
    }
    return false;
}

} // namespace

void VCAuthoringInputTools_Test::init()
{
    m_doc = new Doc(this);
}

void VCAuthoringInputTools_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void VCAuthoringInputTools_Test::tools_registeredWithExpectedSchemasAndAnnotations_data()
{
    QTest::addColumn<QString>("toolName");
    QTest::addColumn<bool>("expectIdempotent");

    QTest::newRow("query-controls") << QStringLiteral("vc_query_controls") << true;
    QTest::newRow("prepare-bindings") << QStringLiteral("vc_prepare_bindings") << false;
}

void VCAuthoringInputTools_Test::tools_registeredWithExpectedSchemasAndAnnotations()
{
    QFETCH(QString, toolName);
    QFETCH(bool, expectIdempotent);

    TestVCAuthoringBridge bridge;
    auto tm = makeInputToolManager(m_doc, &bridge);

    const std::string name = toolName.toStdString();
    QVERIFY2(tm.has(name), qPrintable(QStringLiteral("Expected registered tool: %1").arg(toolName)));
    if (!tm.has(name))
        return;

    const auto &tool = tm.get(name);
    QVERIFY(tool.annotations().has_value());
    const Json annotations = *tool.annotations();
    QCOMPARE(annotations.value("readOnlyHint", false), true);
    QCOMPARE(annotations.value("destructiveHint", true), false);
    QCOMPARE(annotations.value("idempotentHint", !expectIdempotent), expectIdempotent);
    QCOMPARE(annotations.value("openWorldHint", true), false);

    const Json schema = tool.input_schema();
    QCOMPARE(schema.value("type", std::string()), std::string("object"));
    QVERIFY(!schemaContainsCombinators(schema));

    if (toolName == QStringLiteral("vc_query_controls"))
    {
        const Json props = schema.at("properties");
        for (const char *field : {"kind", "widgetIDs", "pluginName", "profileName",
                                  "inputUniverseID", "fields", "limit", "cursor"})
            QVERIFY2(props.contains(field), field);
    }
    else
    {
        const Json props = schema.at("properties");
        for (const char *field : {"items", "expectedBasis", "allowFanout", "requireAtomic", "diffDetail"})
            QVERIFY2(props.contains(field), field);
        QVERIFY2(schema.contains("required"), "vc_prepare_bindings must require items");
        QVERIFY(schema.at("required") == Json::array({"items"}));
        const Json itemProps = props.at("items").at("items").at("properties");
        for (const char *field : {"widgetID", "kind", "action", "controlSlotID", "functionID",
                                  "sourceKey", "destination", "keySequence", "idleValue",
                                  "activeValue", "monitorValue", "idleChannel", "activeChannel", "monitorChannel"})
            QVERIFY2(itemProps.contains(field), field);
    }
}

void VCAuthoringInputTools_Test::tools_schemaUsesKMaxIdForIdBounds_data()
{
    QTest::addColumn<QString>("toolName");
    QTest::addColumn<QString>("jsonPath");

    QTest::newRow("query-widgetIDs-items-maximum")
        << QStringLiteral("vc_query_controls")
        << QStringLiteral("/properties/widgetIDs/items/maximum");
    QTest::newRow("query-inputUniverseID-maximum")
        << QStringLiteral("vc_query_controls")
        << QStringLiteral("/properties/inputUniverseID/maximum");
    QTest::newRow("prepare-widgetID-maximum")
        << QStringLiteral("vc_prepare_bindings")
        << QStringLiteral("/properties/items/items/properties/widgetID/maximum");
    QTest::newRow("prepare-functionID-maximum")
        << QStringLiteral("vc_prepare_bindings")
        << QStringLiteral("/properties/items/items/properties/functionID/maximum");
    QTest::newRow("prepare-sourceKey-inputUniverseID-maximum")
        << QStringLiteral("vc_prepare_bindings")
        << QStringLiteral("/properties/items/items/properties/sourceKey/properties/inputUniverseID/maximum");
    QTest::newRow("prepare-destination-inputUniverseID-maximum")
        << QStringLiteral("vc_prepare_bindings")
        << QStringLiteral("/properties/items/items/properties/destination/properties/inputUniverseID/maximum");
}

void VCAuthoringInputTools_Test::tools_schemaUsesKMaxIdForIdBounds()
{
    QFETCH(QString, toolName);
    QFETCH(QString, jsonPath);

    TestVCAuthoringBridge bridge;
    auto tm = makeInputToolManager(m_doc, &bridge);

    const std::string name = toolName.toStdString();
    QVERIFY2(tm.has(name), qPrintable(QStringLiteral("Expected registered tool: %1").arg(toolName)));
    if (!tm.has(name))
        return;

    const Json schema = tm.get(name).input_schema();
    const std::string pointerPath = jsonPath.toStdString();
    const Json *target = nullptr;
    try
    {
        target = &schema.at(Json::json_pointer(pointerPath));
    }
    catch (...)
    {
        target = nullptr;
    }
    QVERIFY2(target != nullptr, qPrintable(QStringLiteral("Schema path not found: %1").arg(jsonPath)));
    QVERIFY2(target->is_number_integer() || target->is_number_unsigned(), target->dump().c_str());
    const uint64_t actual = target->get<uint64_t>();
    QCOMPARE(actual, uint64_t(mcp::kMaxId));
}

void VCAuthoringInputTools_Test::queryControls_capturesTypedQuery_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QByteArray>("argsJson");

    QTest::newRow("targets-explicit")
        << QStringLiteral("targets-explicit")
        << QByteArray(R"({"kind":"targets","widgetIDs":[11,12],"limit":25,"cursor":"c1","fields":["identity","domains","bindings","feedback"]})");
    QTest::newRow("sources-with-selectors")
        << QStringLiteral("sources-with-selectors")
        << QByteArray(R"({"kind":"sources","pluginName":"MIDI","inputUniverseID":3,"limit":250,"fields":["identity","patches","status"]})");
    QTest::newRow("profiles-with-name")
        << QStringLiteral("profiles-with-name")
        << QByteArray(R"({"kind":"profiles","profileName":"APC Mini","fields":["identity","channelMap","feedbackTables"]})");
    QTest::newRow("default-kind-targets")
        << QStringLiteral("default-kind-targets")
        << QByteArray(R"({"widgetIDs":[42]})");
    QTest::newRow("cursor-1024-multibyte")
        << QStringLiteral("cursor-1024-multibyte")
        << QByteArray(QStringLiteral("{\"kind\":\"targets\",\"widgetIDs\":[42],\"cursor\":\"%1\"}")
                          .arg(QString(341, QChar(0x20AC)) + QStringLiteral("a")).toUtf8());
}

void VCAuthoringInputTools_Test::queryControls_capturesTypedQuery()
{
    QFETCH(QString, scenario);
    QFETCH(QByteArray, argsJson);

    TestVCAuthoringBridge bridge;
    bridge.nextReadResult.complete = true;
    bridge.nextReadResult.code = QStringLiteral("ok");
    bridge.nextReadResult.basis = QStringLiteral("basis-query");
    auto tm = makeInputToolManager(m_doc, &bridge);

    QVERIFY2(tm.has("vc_query_controls"), "Missing vc_query_controls registration");
    if (!tm.has("vc_query_controls"))
        return;

    bool threw = false;
    const Json result = invokeNoThrow(tm, "vc_query_controls", Json::parse(argsJson.constData()), threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.queryAuthoringCalls, 1);
    QVERIFY2(bridge.lastTargetQuery.has_value(), "Expected AuthoringTargetControlQuery dispatch");
    const VCBridge::AuthoringTargetControlQuery &captured = *bridge.lastTargetQuery;

    if (scenario == QStringLiteral("targets-explicit"))
    {
        QCOMPARE(captured.kind, VCBridge::AuthoringTargetControlQuery::Kind::Targets);
        QCOMPARE(captured.limit, 25);
        QCOMPARE(captured.cursor, QStringLiteral("c1"));
        QCOMPARE(captured.fields, QStringList({"identity", "domains", "bindings", "feedback"}));
        QCOMPARE(captured.widgetIDs, QList<quint32>({11u, 12u}));
        QVERIFY(captured.pluginName.isEmpty());
    }
    else if (scenario == QStringLiteral("sources-with-selectors"))
    {
        QCOMPARE(captured.kind, VCBridge::AuthoringTargetControlQuery::Kind::Sources);
        QCOMPARE(captured.limit, 250);
        QCOMPARE(captured.pluginName, QStringLiteral("MIDI"));
        QCOMPARE(captured.inputUniverseID, quint32(3));
        QCOMPARE(captured.fields, QStringList({"identity", "patches", "status"}));
        QVERIFY(captured.widgetIDs.isEmpty());
    }
    else if (scenario == QStringLiteral("profiles-with-name"))
    {
        QCOMPARE(captured.kind, VCBridge::AuthoringTargetControlQuery::Kind::Profiles);
        QCOMPARE(captured.profileName, QStringLiteral("APC Mini"));
        QCOMPARE(captured.limit, 100);
        QCOMPARE(captured.fields, QStringList({"identity", "channelMap", "feedbackTables"}));
        QCOMPARE(captured.inputUniverseID, quint32((quint32)-1));
    }
    else
    {
        QCOMPARE(captured.kind, VCBridge::AuthoringTargetControlQuery::Kind::Targets);
        QCOMPARE(captured.widgetIDs, QList<quint32>({42u}));
        QCOMPARE(captured.limit, 100);
        if (scenario == QStringLiteral("cursor-1024-multibyte"))
            QCOMPARE(captured.cursor.toUtf8().size(), 1024);
    }

    QVERIFY2(result.is_object(), result.dump().c_str());
}

void VCAuthoringInputTools_Test::queryControls_rejectsInvalidRequestsBeforeBridge_data()
{
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<QString>("errorPart");

    QTest::newRow("unknown-top-field")
        << QByteArray(R"({"kind":"targets","widgetIDs":[1],"bogus":1})")
        << QStringLiteral("Unknown fields");
    QTest::newRow("bad-kind")
        << QByteArray(R"({"kind":"nope","widgetIDs":[1]})")
        << QStringLiteral("kind");
    QTest::newRow("bad-kind-case-variant")
        << QByteArray(R"({"kind":"Targets","widgetIDs":[1]})")
        << QStringLiteral("kind");
    QTest::newRow("targets-missing-widgetids")
        << QByteArray(R"({"kind":"targets"})")
        << QStringLiteral("widgetIDs");
    QTest::newRow("sources-with-widgetids")
        << QByteArray(R"({"kind":"sources","widgetIDs":[1]})")
        << QStringLiteral("widgetIDs");
    QTest::newRow("profiles-with-plugin")
        << QByteArray(R"({"kind":"profiles","pluginName":"MIDI"})")
        << QStringLiteral("pluginName");
    QTest::newRow("unknown-group-token")
        << QByteArray(R"({"kind":"targets","widgetIDs":[1],"fields":["identity","mystery"]})")
        << QStringLiteral("fields");
    QTest::newRow("targets-inapplicable-group")
        << QByteArray(R"({"kind":"targets","widgetIDs":[1],"fields":["channelMap"]})")
        << QStringLiteral("fields");
    QTest::newRow("sources-inapplicable-group")
        << QByteArray(R"({"kind":"sources","pluginName":"MIDI","fields":["channelMap"]})")
        << QStringLiteral("fields");
    QTest::newRow("profiles-inapplicable-group")
        << QByteArray(R"({"kind":"profiles","profileName":"APC","fields":["domains"]})")
        << QStringLiteral("fields");
    QTest::newRow("limit-too-large")
        << QByteArray(R"({"kind":"targets","widgetIDs":[1],"limit":251})")
        << QStringLiteral("limit");
    QTest::newRow("cursor-too-long")
        << QByteArray(QStringLiteral("{\"kind\":\"targets\",\"widgetIDs\":[1],\"cursor\":\"%1\"}")
                          .arg(QString(1025, QLatin1Char('x'))).toUtf8())
        << QStringLiteral("cursor");
    QTest::newRow("cursor-too-long-multibyte")
        << QByteArray(QStringLiteral("{\"kind\":\"targets\",\"widgetIDs\":[1],\"cursor\":\"%1\"}")
                          .arg(QString(342, QChar(0x20AC)) + QStringLiteral("a")).toUtf8())
        << QStringLiteral("cursor");
    QTest::newRow("input-universe-negative")
        << QByteArray(R"({"kind":"sources","inputUniverseID":-1})")
        << QStringLiteral("inputUniverseID");
}

void VCAuthoringInputTools_Test::queryControls_rejectsInvalidRequestsBeforeBridge()
{
    QFETCH(QByteArray, argsJson);
    QFETCH(QString, errorPart);

    TestVCAuthoringBridge bridge;
    auto tm = makeInputToolManager(m_doc, &bridge);

    QVERIFY2(tm.has("vc_query_controls"), "Missing vc_query_controls registration");
    if (!tm.has("vc_query_controls"))
        return;

    bool threw = false;
    const Json result = invokeNoThrow(tm, "vc_query_controls", Json::parse(argsJson.constData()), threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.queryAuthoringCalls, 0);
    assertInvalidArgumentEnvelope(result, errorPart);
}

void VCAuthoringInputTools_Test::prepareBindings_capturesTypedRequest_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::addColumn<QByteArray>("argsJson");

    QTest::newRow("function-set")
        << QStringLiteral("function-set")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"function","action":"set","functionID":99}]})");
    QTest::newRow("function-clear")
        << QStringLiteral("function-clear")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"function","action":"clear"}]})");
    QTest::newRow("function-default-set")
        << QStringLiteral("function-default-set")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"function","functionID":99}]})");
    QTest::newRow("function-default-set-explicit-outer-flags")
        << QStringLiteral("function-default-set-explicit-outer-flags")
        << QByteArray(R"({"allowFanout":true,"requireAtomic":true,"diffDetail":"full","expectedBasis":"b42","items":[{"widgetID":7,"kind":"function","functionID":99}]})");
    QTest::newRow("controller-append")
        << QStringLiteral("controller-append")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"append","controlSlotID":2,"destination":{"inputUniverseID":1,"inputChannel":65535,"profileName":"APC"}}]})");
    QTest::newRow("controller-move")
        << QStringLiteral("controller-move")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"move","controlSlotID":2,"sourceKey":{"controlSlotID":2,"inputUniverseID":1,"inputChannel":7,"childSubpage":0},"destination":{"inputUniverseID":1,"inputChannel":8,"profileName":"APC"}}]})");
    QTest::newRow("controller-remove")
        << QStringLiteral("controller-remove")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"remove","controlSlotID":2,"sourceKey":{"controlSlotID":2,"inputUniverseID":1,"inputChannel":7,"childSubpage":0}}]})");
    QTest::newRow("key-set")
        << QStringLiteral("key-set")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"key","action":"set","controlSlotID":1,"keySequence":"Ctrl+K"}]})");
    QTest::newRow("key-remove")
        << QStringLiteral("key-remove")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"key","action":"remove","controlSlotID":1,"keySequence":"Ctrl+K"}]})");
    QTest::newRow("feedback-values")
        << QStringLiteral("feedback-values")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"feedback","sourceKey":{"controlSlotID":1,"inputUniverseID":0,"inputChannel":7,"childSubpage":0},"idleValue":10,"activeValue":20,"monitorValue":30}]})");
    QTest::newRow("feedback-channels")
        << QStringLiteral("feedback-channels")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"feedback","sourceKey":{"controlSlotID":1,"inputUniverseID":0,"inputChannel":7,"childSubpage":0},"idleChannel":-1,"activeChannel":0,"monitorChannel":15}]})");
    QTest::newRow("feedback-public-json-values-no-outer-action-slot")
        << QStringLiteral("feedback-public-json-values-no-outer-action-slot")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"feedback","sourceKey":{"controlSlotID":1,"inputUniverseID":0,"inputChannel":7,"childSubpage":0},"idleValue":10,"activeValue":20,"monitorValue":30}]})");
}

void VCAuthoringInputTools_Test::prepareBindings_capturesTypedRequest()
{
    QFETCH(QString, scenario);
    QFETCH(QByteArray, argsJson);

    TestVCAuthoringBridge bridge;
    bridge.nextPreparationResult.code = QStringLiteral("ok");
    bridge.nextPreparationResult.preparationToken = QStringLiteral("tok");
    auto tm = makeInputToolManager(m_doc, &bridge);

    QVERIFY2(tm.has("vc_prepare_bindings"), "Missing vc_prepare_bindings registration");
    if (!tm.has("vc_prepare_bindings"))
        return;

    bool threw = false;
    const Json result = invokeNoThrow(tm, "vc_prepare_bindings", Json::parse(argsJson.constData()), threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.prepareAuthoringCalls, 1);
    QVERIFY2(bridge.lastBindingsRequest.has_value(), "Expected AuthoringBindingsRequest dispatch");

    const VCBridge::AuthoringBindingsRequest &captured = *bridge.lastBindingsRequest;
    QCOMPARE(captured.items.size(), 1);
    const VCBridge::AuthoringBindingItem &item = captured.items.first();
    QCOMPARE(item.widgetID, quint32(7));
    if (scenario != QStringLiteral("function-default-set-explicit-outer-flags"))
    {
        QCOMPARE(captured.allowFanout, false);
        QCOMPARE(captured.requireAtomic, false);
        QCOMPARE(captured.diffDetail, QStringLiteral("summary"));
        QVERIFY(captured.expectedBasis.isEmpty());
    }

    if (scenario == QStringLiteral("function-set") || scenario == QStringLiteral("function-default-set") ||
        scenario == QStringLiteral("function-default-set-explicit-outer-flags"))
    {
        QCOMPARE(item.kind, VCBridge::AuthoringBindingItem::Kind::Function);
        QCOMPARE(item.action, VCBridge::AuthoringBindingItem::Action::Set);
        QVERIFY(item.functionID.has_value());
        QCOMPARE(*item.functionID, quint32(99));
        if (scenario == QStringLiteral("function-default-set-explicit-outer-flags"))
        {
            QCOMPARE(captured.allowFanout, true);
            QCOMPARE(captured.requireAtomic, true);
            QCOMPARE(captured.diffDetail, QStringLiteral("full"));
            QCOMPARE(captured.expectedBasis, QStringLiteral("b42"));
        }
    }
    else if (scenario == QStringLiteral("function-clear"))
    {
        QCOMPARE(item.kind, VCBridge::AuthoringBindingItem::Kind::Function);
        QCOMPARE(item.action, VCBridge::AuthoringBindingItem::Action::Clear);
        QVERIFY(!item.functionID.has_value());
    }
    else if (scenario == QStringLiteral("controller-append"))
    {
        QCOMPARE(item.kind, VCBridge::AuthoringBindingItem::Kind::Controller);
        QCOMPARE(item.action, VCBridge::AuthoringBindingItem::Action::Append);
        QVERIFY(item.destination.has_value());
        QCOMPARE(item.destination->inputUniverseID.value_or(0), quint32(1));
        QCOMPARE(item.destination->inputChannel.value_or(0), quint32(65535));
        QCOMPARE(item.destination->profileName.value_or(QString()), QStringLiteral("APC"));
        QVERIFY(!item.sourceKey.has_value());
    }
    else if (scenario == QStringLiteral("controller-move"))
    {
        QCOMPARE(item.action, VCBridge::AuthoringBindingItem::Action::Move);
        QVERIFY(item.sourceKey.has_value());
        QCOMPARE(item.sourceKey->controlSlotID.value_or(0), quint8(2));
        QCOMPARE(item.sourceKey->inputUniverseID.value_or(0), quint32(1));
        QCOMPARE(item.sourceKey->inputChannel.value_or(0), quint32(7));
        QCOMPARE(item.sourceKey->childSubpage.value_or(-1), 0);
        QVERIFY(item.destination.has_value());
    }
    else if (scenario == QStringLiteral("controller-remove"))
    {
        QCOMPARE(item.action, VCBridge::AuthoringBindingItem::Action::Remove);
        QVERIFY(item.sourceKey.has_value());
        QVERIFY(!item.destination.has_value());
    }
    else if (scenario == QStringLiteral("key-set") || scenario == QStringLiteral("key-remove"))
    {
        QCOMPARE(item.kind, VCBridge::AuthoringBindingItem::Kind::Key);
        QVERIFY(item.controlSlotID.has_value());
        QCOMPARE(item.keySequence.value_or(QString()), QStringLiteral("Ctrl+K"));
    }
    else if (scenario == QStringLiteral("feedback-values"))
    {
        QCOMPARE(item.kind, VCBridge::AuthoringBindingItem::Kind::Feedback);
        QCOMPARE(item.action, VCBridge::AuthoringBindingItem::Action::Set);
        QVERIFY(item.sourceKey.has_value());
        QVERIFY(!item.controlSlotID.has_value());
        QVERIFY(item.idleValue.has_value());
        QVERIFY(item.monitorValue.has_value());
        QVERIFY(!item.idleChannel.has_value());
    }
    else if (scenario == QStringLiteral("feedback-channels"))
    {
        QCOMPARE(item.kind, VCBridge::AuthoringBindingItem::Kind::Feedback);
        QCOMPARE(item.action, VCBridge::AuthoringBindingItem::Action::Set);
        QVERIFY(item.sourceKey.has_value());
        QVERIFY(!item.controlSlotID.has_value());
        QVERIFY(item.idleChannel.has_value());
        QCOMPARE(item.idleChannel.value_or(0), -1);
        QCOMPARE(item.monitorChannel.value_or(0), 15);
        QVERIFY(!item.idleValue.has_value());
    }
    else
    {
        QCOMPARE(item.kind, VCBridge::AuthoringBindingItem::Kind::Feedback);
        QCOMPARE(item.action, VCBridge::AuthoringBindingItem::Action::Set);
        QVERIFY(!item.controlSlotID.has_value());
        QVERIFY(item.sourceKey.has_value());
        QVERIFY(item.idleValue.has_value());
        QVERIFY(item.activeValue.has_value());
        QVERIFY(item.monitorValue.has_value());
    }

    QVERIFY2(result.is_object(), result.dump().c_str());
}

void VCAuthoringInputTools_Test::prepareBindings_rejectsInvalidRequestsBeforeBridge_data()
{
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<QString>("errorPart");

    QTest::newRow("unknown-top-field")
        << QByteArray(R"({"items":[],"bogus":1})")
        << QStringLiteral("Unknown fields");
    QTest::newRow("missing-items")
        << QByteArray(R"({"expectedBasis":"x"})")
        << QStringLiteral("items");
    QTest::newRow("missing-kind")
        << QByteArray(R"({"items":[{"widgetID":7,"action":"clear"}]})")
        << QStringLiteral("kind");
    QTest::newRow("missing-action-nonfunction")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","controlSlotID":1}]})")
        << QStringLiteral("action");
    QTest::newRow("items-not-array")
        << QByteArray(R"({"items":{"widgetID":1}})")
        << QStringLiteral("items");
    QTest::newRow("duplicate-target-widget")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"function","action":"clear"},{"widgetID":7,"kind":"key","action":"set","controlSlotID":0,"keySequence":"K"}]})")
        << QStringLiteral("duplicate");
    QTest::newRow("unknown-item-field")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"function","action":"clear","nope":1}]})")
        << QStringLiteral("Unknown fields");
    QTest::newRow("widgetid-sentinel")
        << QByteArray(R"({"items":[{"widgetID":4294967295,"kind":"function","action":"clear"}]})")
        << QStringLiteral("widgetID");
    QTest::newRow("bad-kind-case-variant")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"Function","action":"set","functionID":1}]})")
        << QStringLiteral("kind");
    QTest::newRow("bad-action-case-variant")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"function","action":"Set","functionID":1}]})")
        << QStringLiteral("action");
    QTest::newRow("function-clear-with-functionid")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"function","action":"clear","functionID":1}]})")
        << QStringLiteral("functionID");
    QTest::newRow("function-set-missing-functionid")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"function","action":"set"}]})")
        << QStringLiteral("functionID");
    QTest::newRow("controller-append-has-sourcekey")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"append","controlSlotID":2,"sourceKey":{"controlSlotID":2,"inputUniverseID":1,"inputChannel":7,"childSubpage":0},"destination":{"inputUniverseID":1,"inputChannel":8,"profileName":"APC"}}]})")
        << QStringLiteral("sourceKey");
    QTest::newRow("controller-move-missing-sourcekey")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"move","controlSlotID":2,"destination":{"inputUniverseID":1,"inputChannel":8,"profileName":"APC"}}]})")
        << QStringLiteral("sourceKey");
    QTest::newRow("controller-remove-missing-sourcekey")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"remove","controlSlotID":2}]})")
        << QStringLiteral("sourceKey");
    QTest::newRow("controller-append-missing-destination-profile")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"append","controlSlotID":2,"destination":{"inputUniverseID":1,"inputChannel":8}}]})")
        << QStringLiteral("profileName");
    QTest::newRow("controller-move-slot-mismatch")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"move","controlSlotID":1,"sourceKey":{"controlSlotID":2,"inputUniverseID":1,"inputChannel":7,"childSubpage":0},"destination":{"inputUniverseID":1,"inputChannel":8,"profileName":"APC"}}]})")
        << QStringLiteral("controlSlotID");
    QTest::newRow("partial-sourcekey")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"remove","controlSlotID":2,"sourceKey":{"controlSlotID":2,"inputUniverseID":1,"inputChannel":7}}]})")
        << QStringLiteral("Source key");
    QTest::newRow("sourcekey-overflow-channel")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"remove","controlSlotID":2,"sourceKey":{"controlSlotID":2,"inputUniverseID":1,"inputChannel":65536,"childSubpage":0}}]})")
        << QStringLiteral("inputChannel");
    QTest::newRow("sourcekey-overflow-child-subpage")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"remove","controlSlotID":2,"sourceKey":{"controlSlotID":2,"inputUniverseID":1,"inputChannel":7,"childSubpage":65536}}]})")
        << QStringLiteral("childSubpage");
    QTest::newRow("sourcekey-invalid-universe-sentinel")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"remove","controlSlotID":2,"sourceKey":{"controlSlotID":2,"inputUniverseID":4294967295,"inputChannel":7,"childSubpage":0}}]})")
        << QStringLiteral("inputUniverseID");
    QTest::newRow("destination-overflow-channel")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"append","controlSlotID":2,"destination":{"inputUniverseID":1,"inputChannel":65536,"profileName":"APC"}}]})")
        << QStringLiteral("inputChannel");
    QTest::newRow("destination-invalid-universe-sentinel")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"append","controlSlotID":2,"destination":{"inputUniverseID":4294967295,"inputChannel":8,"profileName":"APC"}}]})")
        << QStringLiteral("inputUniverseID");
    QTest::newRow("control-slot-overflow")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"append","controlSlotID":256,"destination":{"inputUniverseID":1,"inputChannel":8,"profileName":"APC"}}]})")
        << QStringLiteral("controlSlotID");
    QTest::newRow("sourcekey-bad-nested-field")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"controller","action":"remove","controlSlotID":2,"sourceKey":{"controlSlotID":2,"inputUniverseID":1,"inputChannel":7,"childSubpage":0,"bad":1}}]})")
        << QStringLiteral("Unknown fields");
    QTest::newRow("key-remove-missing-sequence")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"key","action":"remove","controlSlotID":1}]})")
        << QStringLiteral("keySequence");
    QTest::newRow("feedback-missing-groups")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"feedback","sourceKey":{"controlSlotID":1,"inputUniverseID":0,"inputChannel":7,"childSubpage":0}}]})")
        << QStringLiteral("values or channels");
    QTest::newRow("feedback-has-outer-slot-inapplicable")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"feedback","controlSlotID":1,"sourceKey":{"controlSlotID":1,"inputUniverseID":0,"inputChannel":7,"childSubpage":0},"idleValue":10,"activeValue":20,"monitorValue":30}]})")
        << QStringLiteral("controlSlotID");
    QTest::newRow("feedback-has-outer-action-inapplicable")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"feedback","action":"set","sourceKey":{"controlSlotID":1,"inputUniverseID":0,"inputChannel":7,"childSubpage":0},"idleValue":10,"activeValue":20,"monitorValue":30}]})")
        << QStringLiteral("action");
    QTest::newRow("feedback-partial-values")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"feedback","sourceKey":{"controlSlotID":1,"inputUniverseID":0,"inputChannel":7,"childSubpage":0},"idleValue":10,"activeValue":20}]})")
        << QStringLiteral("values group");
    QTest::newRow("feedback-values-out-of-range")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"feedback","sourceKey":{"controlSlotID":1,"inputUniverseID":0,"inputChannel":7,"childSubpage":0},"idleValue":10,"activeValue":20,"monitorValue":256}]})")
        << QStringLiteral("0..255");
    QTest::newRow("feedback-partial-channels")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"feedback","sourceKey":{"controlSlotID":1,"inputUniverseID":0,"inputChannel":7,"childSubpage":0},"idleChannel":-1,"activeChannel":0}]})")
        << QStringLiteral("channels group");
    QTest::newRow("feedback-channels-out-of-range")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"feedback","sourceKey":{"controlSlotID":1,"inputUniverseID":0,"inputChannel":7,"childSubpage":0},"idleChannel":-1,"activeChannel":0,"monitorChannel":16}]})")
        << QStringLiteral("-1..15");
    QTest::newRow("bad-diff-detail")
        << QByteArray(R"({"items":[{"widgetID":7,"kind":"function","action":"clear"}],"diffDetail":"verbose"})")
        << QStringLiteral("diffDetail");
}

void VCAuthoringInputTools_Test::prepareBindings_rejectsInvalidRequestsBeforeBridge()
{
    QFETCH(QByteArray, argsJson);
    QFETCH(QString, errorPart);

    TestVCAuthoringBridge bridge;
    auto tm = makeInputToolManager(m_doc, &bridge);

    QVERIFY2(tm.has("vc_prepare_bindings"), "Missing vc_prepare_bindings registration");
    if (!tm.has("vc_prepare_bindings"))
        return;

    bool threw = false;
    const Json result = invokeNoThrow(tm, "vc_prepare_bindings", Json::parse(argsJson.constData()), threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.prepareAuthoringCalls, 0);
    assertInvalidArgumentEnvelope(result, errorPart);
}

void VCAuthoringInputTools_Test::prepareBindings_rejectsItemCountBounds()
{
    TestVCAuthoringBridge bridge;
    auto tm = makeInputToolManager(m_doc, &bridge);

    QVERIFY2(tm.has("vc_prepare_bindings"), "Missing vc_prepare_bindings registration");
    if (!tm.has("vc_prepare_bindings"))
        return;

    Json empty = Json::array();
    bool threw = false;
    Json result = invokeNoThrow(tm, "vc_prepare_bindings", Json{{"items", empty}}, threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.prepareAuthoringCalls, 0);
    assertInvalidArgumentEnvelope(result, QStringLiteral("1..128"));

    Json maxItems = Json::array();
    for (int i = 0; i < 128; ++i)
        maxItems.push_back({{"widgetID", i + 1}, {"kind", "function"}, {"action", "clear"}});
    result = invokeNoThrow(tm, "vc_prepare_bindings", Json{{"items", maxItems}}, threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.prepareAuthoringCalls, 1);
    QVERIFY2(result.is_object(), result.dump().c_str());

    Json tooMany = Json::array();
    for (int i = 0; i < 129; ++i)
        tooMany.push_back({{"widgetID", i + 1}, {"kind", "function"}, {"action", "clear"}});
    bridge.prepareAuthoringCalls = 0;
    result = invokeNoThrow(tm, "vc_prepare_bindings", Json{{"items", tooMany}}, threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.prepareAuthoringCalls, 0);
    assertInvalidArgumentEnvelope(result, QStringLiteral("128"));
}

void VCAuthoringInputTools_Test::authoringInputTools_bridgeResultShapeConsistency()
{
    TestVCAuthoringBridge bridge;
    bridge.nextReadResult.code = QStringLiteral("unsupportedBridge");
    bridge.nextReadResult.error = QStringLiteral("Authoring operations are not supported by this bridge");
    bridge.nextPreparationResult.code = QStringLiteral("unsupportedPreparation");
    bridge.nextPreparationResult.error = QStringLiteral("Bindings not implemented at this checkpoint");
    auto tm = makeInputToolManager(m_doc, &bridge);

    QVERIFY2(tm.has("vc_query_controls"), "Missing vc_query_controls registration");
    QVERIFY2(tm.has("vc_prepare_bindings"), "Missing vc_prepare_bindings registration");
    if (!tm.has("vc_query_controls") || !tm.has("vc_prepare_bindings"))
        return;

    bool threw = false;
    Json queryResult = invokeNoThrow(tm, "vc_query_controls",
        Json{{"kind", "targets"}, {"widgetIDs", Json::array({1})}}, threw);
    QVERIFY2(!threw, queryResult.dump().c_str());
    Json prepResult = invokeNoThrow(tm, "vc_prepare_bindings",
        Json{{"items", Json::array({{{"widgetID", 1}, {"kind", "function"}, {"action", "clear"}}})}}, threw);
    QVERIFY2(!threw, prepResult.dump().c_str());

    QCOMPARE(bridge.queryAuthoringCalls, 1);
    QCOMPARE(bridge.prepareAuthoringCalls, 1);

    QVERIFY2(queryResult.is_object(), queryResult.dump().c_str());
    QVERIFY2(prepResult.is_object(), prepResult.dump().c_str());
    QVERIFY2(queryResult.contains("version"), queryResult.dump().c_str());
    QVERIFY2(queryResult.contains("code"), queryResult.dump().c_str());
    QVERIFY2(queryResult.contains("error"), queryResult.dump().c_str());
    QVERIFY2(queryResult.contains("limitations"), queryResult.dump().c_str());
    QVERIFY2(queryResult.at("limitations").is_array(), queryResult.dump().c_str());

    QVERIFY2(prepResult.contains("version"), prepResult.dump().c_str());
    QVERIFY2(prepResult.contains("code"), prepResult.dump().c_str());
    QVERIFY2(prepResult.contains("error"), prepResult.dump().c_str());
    QVERIFY2(prepResult.contains("limitations"), prepResult.dump().c_str());
    QVERIFY2(prepResult.at("limitations").is_array(), prepResult.dump().c_str());

    const Json encodedSuccess = mcp::encodeToolResult(Json{
        {"content", Json::array({{{"type", "text"}, {"text", Json{{"version", 1}, {"code", "ok"}}.dump()}}})}
    });
    QVERIFY2(encodedSuccess.contains("isError"), encodedSuccess.dump().c_str());
    QCOMPARE(encodedSuccess.value("isError", true), false);
    QVERIFY2(encodedSuccess.contains("structuredContent"), encodedSuccess.dump().c_str());

    const Json encodedBridgeError = mcp::encodeToolResult(Json{
        {"content", Json::array({{{"type", "text"}, {"text", queryResult.dump()}}})}
    });
    QVERIFY2(encodedBridgeError.contains("isError"), encodedBridgeError.dump().c_str());
    QCOMPARE(encodedBridgeError.value("isError", false), true);
    QVERIFY2(encodedBridgeError.contains("structuredContent"), encodedBridgeError.dump().c_str());
}

QTEST_MAIN(VCAuthoringInputTools_Test)
