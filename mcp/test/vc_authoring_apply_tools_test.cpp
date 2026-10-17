/*
  Q Light Controller Plus - Unit test
  vc_authoring_apply_tools_test.cpp
*/

#include <QtTest>
#include <nlohmann/json.hpp>

#include <exception>
#include <optional>

#include "vc_authoring_apply_tools_test.h"
#include "tool_registry.h"
#include "vcbridge.h"
#include "doc.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {

class TestVCAuthoringApplyBridge final : public VCBridge
{
public:
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
    WidgetDetails getWidgetDetails(int widgetID) const override
    {
        if (widgetID == recordPanelWidgetID)
            return recordPanelWidgetDetails;
        return WidgetDetails();
    }
    bool setXYPadPosition(int, qreal, qreal) override { return true; }
    bool setAudioTriggerCapture(int, bool) override { return true; }
    bool setAudioTriggerVolume(int, int) override { return true; }
    bool configureFrame(int, const FrameConfig &) override { return true; }
    int findWidgetByCaption(int, const QString &, const QString &) const override { return -1; }
    bool setWidgetPage(int, int) override { return true; }
    bool configureSlider(int, const SliderConfig &) override { return true; }
    bool setWidgetCaption(int, const QString &) override
    {
        ++setWidgetCaptionCalls;
        return true;
    }
    bool setButtonAction(int, const QString &) override { return true; }
    bool configureButton(int, const ButtonConfig &) override { return true; }
    bool configureRecordPanel(int widgetID, const RecordPanelConfig &config) override
    {
        ++configureRecordPanelCalls;
        lastConfiguredRecordPanelWidgetID = widgetID;
        lastRecordPanelConfig = config;
        return true;
    }
    int createWidgetOnPage(int, const QString &, const QRect &, const QString &, int) override { return -1; }
    WidgetSnapshot snapshotFrame(int) const override { return WidgetSnapshot(); }
    LayoutEditResult alignLayout(int, int, int, int, bool) override { return LayoutEditResult(); }

    AuthoringPreparationResult prepareAuthoring(const AuthoringRequest &request) override
    {
        ++prepareAuthoringCalls;
        lastRecordPanelRequest.reset();
        lastLayoutRequest.reset();
        if (const AuthoringRecordPanelRequest *recordPanel = std::get_if<AuthoringRecordPanelRequest>(&request))
            lastRecordPanelRequest = *recordPanel;
        if (const AuthoringLayoutRequest *layout = std::get_if<AuthoringLayoutRequest>(&request))
            lastLayoutRequest = *layout;
        return nextPrepareResult;
    }

    AuthoringApplyResult applyAuthoring(const QString &preparationToken) override
    {
        ++applyAuthoringCalls;
        lastToken = preparationToken;
        return nextApplyResult;
    }

    int prepareAuthoringCalls = 0;
    int applyAuthoringCalls = 0;
    QString lastToken;
    std::optional<AuthoringRecordPanelRequest> lastRecordPanelRequest;
    std::optional<AuthoringLayoutRequest> lastLayoutRequest;
    AuthoringPreparationResult nextPrepareResult;
    AuthoringApplyResult nextApplyResult;
    int setWidgetCaptionCalls = 0;
    int configureRecordPanelCalls = 0;
    int recordPanelWidgetID = -1;
    int lastConfiguredRecordPanelWidgetID = -1;
    WidgetDetails recordPanelWidgetDetails;
    RecordPanelConfig lastRecordPanelConfig;
};

Json parsedToolResult(const Json &value)
{
    if (value.is_string())
        return Json::parse(value.get<std::string>());
    return value;
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

} // namespace

void VCAuthoringApplyTools_Test::init()
{
    m_doc = new Doc(this);
}

void VCAuthoringApplyTools_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void VCAuthoringApplyTools_Test::tool_registeredWithExpectedSchemaAndAnnotations()
{
    TestVCAuthoringApplyBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    QVERIFY2(tm.has("vc_apply_preparation"), "Expected vc_apply_preparation to be registered");
    if (!tm.has("vc_apply_preparation"))
        return;

    const auto &tool = tm.get("vc_apply_preparation");
    QVERIFY(tool.annotations().has_value());
    const Json annotations = *tool.annotations();
    QCOMPARE(annotations.value("readOnlyHint", true), false);
    QCOMPARE(annotations.value("destructiveHint", false), true);
    QCOMPARE(annotations.value("idempotentHint", true), false);
    QCOMPARE(annotations.value("openWorldHint", true), false);

    const Json schema = tool.input_schema();
    QCOMPARE(schema.value("type", std::string()), std::string("object"));
    QVERIFY(!schemaContainsCombinators(schema));
    QVERIFY(schema.contains("required"));
    QCOMPARE(schema.at("required"), Json::array({"preparationToken"}));
    QVERIFY(schema.contains("properties"));
    QVERIFY(schema.at("properties").contains("preparationToken"));
}

void VCAuthoringApplyTools_Test::prepareLayout_registeredWithExpectedSchemaAndAnnotations()
{
    TestVCAuthoringApplyBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    QVERIFY2(tm.has("vc_prepare_layout"), "Expected vc_prepare_layout to be registered");
    if (!tm.has("vc_prepare_layout"))
        return;

    const auto &tool = tm.get("vc_prepare_layout");
    QVERIFY(tool.annotations().has_value());
    const Json annotations = *tool.annotations();
    QCOMPARE(annotations.value("readOnlyHint", false), true);
    QCOMPARE(annotations.value("destructiveHint", true), false);
    QCOMPARE(annotations.value("idempotentHint", true), false);
    QCOMPARE(annotations.value("openWorldHint", true), false);

    const Json schema = tool.input_schema();
    QCOMPARE(schema.value("type", std::string()), std::string("object"));
    QVERIFY(!schemaContainsCombinators(schema));
    QVERIFY(schema.contains("required"));
    QCOMPARE(schema.at("required"), Json::array({"operation"}));
    QVERIFY(schema.contains("properties"));
    const Json props = schema.at("properties");
    for (const char *field : {"operation", "pageWidgetID", "frameWidgetID", "name", "totalPages",
                              "pageLabels", "pagesLoop", "expectedBasis", "requireAtomic", "diffDetail"})
        QVERIFY2(props.contains(field), field);
}

void VCAuthoringApplyTools_Test::prepareLayout_rejectsInvalidRequestsBeforeBridge_data()
{
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<QString>("errorPart");

    QTest::newRow("unknown-top-field")
        << QByteArray(R"({"operation":"pageCreate","name":"Page A","bogus":1})")
        << QStringLiteral("Unknown fields");
    QTest::newRow("operation-not-string")
        << QByteArray(R"({"operation":1})")
        << QStringLiteral("operation");
    QTest::newRow("invalid-operation")
        << QByteArray(R"({"operation":"geometry"})")
        << QStringLiteral("operation");
    QTest::newRow("pagecreate-missing-name")
        << QByteArray(R"({"operation":"pageCreate"})")
        << QStringLiteral("name");
    QTest::newRow("pagerename-missing-page")
        << QByteArray(R"({"operation":"pageRename","name":"Renamed"})")
        << QStringLiteral("pageWidgetID");
    QTest::newRow("pagedelete-missing-page")
        << QByteArray(R"({"operation":"pageDelete"})")
        << QStringLiteral("pageWidgetID");
    QTest::newRow("framepages-missing-frame")
        << QByteArray(R"({"operation":"framePages","totalPages":2})")
        << QStringLiteral("frameWidgetID");
    QTest::newRow("framepages-no-config")
        << QByteArray(R"({"operation":"framePages","frameWidgetID":7})")
        << QStringLiteral("at least one");
    QTest::newRow("framepages-totalpages-invalid")
        << QByteArray(R"({"operation":"framePages","frameWidgetID":7,"totalPages":0})")
        << QStringLiteral("totalPages");
    QTest::newRow("framepages-page-labels-not-array")
        << QByteArray(R"({"operation":"framePages","frameWidgetID":7,"pageLabels":"x"})")
        << QStringLiteral("pageLabels");
    QTest::newRow("framepages-pagesloop-not-bool")
        << QByteArray(R"({"operation":"framePages","frameWidgetID":7,"pagesLoop":"true"})")
        << QStringLiteral("pagesLoop");
    QTest::newRow("diffdetail-invalid")
        << QByteArray(R"({"operation":"pageCreate","name":"A","diffDetail":"verbose"})")
        << QStringLiteral("diffDetail");
    QTest::newRow("requireatomic-invalid")
        << QByteArray(R"({"operation":"pageCreate","name":"A","requireAtomic":"true"})")
        << QStringLiteral("requireAtomic");
}

void VCAuthoringApplyTools_Test::prepareLayout_rejectsInvalidRequestsBeforeBridge()
{
    QFETCH(QByteArray, argsJson);
    QFETCH(QString, errorPart);

    TestVCAuthoringApplyBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    QVERIFY2(tm.has("vc_prepare_layout"), "Expected vc_prepare_layout registration");
    if (!tm.has("vc_prepare_layout"))
        return;

    bool threw = false;
    const Json result = invokeNoThrow(tm, "vc_prepare_layout", Json::parse(argsJson.constData()), threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.prepareAuthoringCalls, 0);
    QCOMPARE(bridge.applyAuthoringCalls, 0);
    assertInvalidArgumentEnvelope(result, errorPart);
}

void VCAuthoringApplyTools_Test::prepareLayout_capturesTypedRequestAndReturnsBridgePayload()
{
    TestVCAuthoringApplyBridge bridge;
    bridge.nextPrepareResult.basis = QStringLiteral("basis-layout");
    bridge.nextPrepareResult.preparationToken = QStringLiteral("prep-layout");
    bridge.nextPrepareResult.atomicScope = VCBridge::AuthoringPreparationResult::AtomicScope::IndependentItems;
    bridge.nextPrepareResult.diffDetail = VCBridge::AuthoringPreparationResult::DiffDetail::Full;

    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    const Json args = Json::parse(R"({
        "operation":"framePages",
        "frameWidgetID":42,
        "totalPages":3,
        "pageLabels":["One","Two","Three"],
        "pagesLoop":true,
        "expectedBasis":"basis-1",
        "requireAtomic":true,
        "diffDetail":"full"
    })");

    bool threw = false;
    const Json result = invokeNoThrow(tm, "vc_prepare_layout", args, threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.prepareAuthoringCalls, 1);
    QCOMPARE(bridge.applyAuthoringCalls, 0);
    QVERIFY2(bridge.lastLayoutRequest.has_value(), "Expected AuthoringLayoutRequest dispatch");
    QVERIFY2(!bridge.lastRecordPanelRequest.has_value(), "Did not expect AuthoringRecordPanelRequest dispatch");

    const VCBridge::AuthoringLayoutRequest &captured = *bridge.lastLayoutRequest;
    QCOMPARE(captured.operation, VCBridge::AuthoringLayoutRequest::Operation::FramePages);
    QVERIFY(captured.frameWidgetID.has_value());
    QCOMPARE(captured.frameWidgetID.value(), quint32(42));
    QVERIFY(captured.totalPages.has_value());
    QCOMPARE(captured.totalPages.value(), 3);
    QVERIFY(captured.pageLabels.has_value());
    QCOMPARE(captured.pageLabels.value(), QStringList({QStringLiteral("One"), QStringLiteral("Two"), QStringLiteral("Three")}));
    QVERIFY(captured.pagesLoop.has_value());
    QCOMPARE(captured.pagesLoop.value(), true);
    QCOMPARE(captured.expectedBasis, QStringLiteral("basis-1"));
    QCOMPARE(captured.requireAtomic, true);
    QCOMPARE(captured.diffDetail, QStringLiteral("full"));

    QVERIFY2(result.is_object(), result.dump().c_str());
    QCOMPARE(result.value("preparationToken", std::string()), std::string("prep-layout"));
    QCOMPARE(result.value("atomicScope", std::string()), std::string("independentItems"));
    QCOMPARE(result.value("diffDetail", std::string()), std::string("full"));
}

void VCAuthoringApplyTools_Test::prepareLayout_capturesTypedPageOps_data()
{
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<int>("expectedOperation");
    QTest::addColumn<int>("expectedPageWidgetID");
    QTest::addColumn<QString>("expectedName");
    QTest::addColumn<QString>("expectedBasis");
    QTest::addColumn<bool>("expectedRequireAtomic");
    QTest::addColumn<QString>("expectedDiffDetail");

    QTest::newRow("page-create")
        << QByteArray(R"({"operation":"pageCreate","name":"Created Page","expectedBasis":"basis-create","requireAtomic":false,"diffDetail":"summary"})")
        << int(VCBridge::AuthoringLayoutRequest::Operation::PageCreate)
        << -1
        << QStringLiteral("Created Page")
        << QStringLiteral("basis-create")
        << false
        << QStringLiteral("summary");

    QTest::newRow("page-rename")
        << QByteArray(R"({"operation":"pageRename","pageWidgetID":77,"name":"Renamed Page","expectedBasis":"basis-rename","requireAtomic":true,"diffDetail":"full"})")
        << int(VCBridge::AuthoringLayoutRequest::Operation::PageRename)
        << 77
        << QStringLiteral("Renamed Page")
        << QStringLiteral("basis-rename")
        << true
        << QStringLiteral("full");

    QTest::newRow("page-delete")
        << QByteArray(R"({"operation":"pageDelete","pageWidgetID":88,"expectedBasis":"basis-delete","requireAtomic":true,"diffDetail":"summary"})")
        << int(VCBridge::AuthoringLayoutRequest::Operation::PageDelete)
        << 88
        << QString()
        << QStringLiteral("basis-delete")
        << true
        << QStringLiteral("summary");
}

void VCAuthoringApplyTools_Test::prepareLayout_capturesTypedPageOps()
{
    QFETCH(QByteArray, argsJson);
    QFETCH(int, expectedOperation);
    QFETCH(int, expectedPageWidgetID);
    QFETCH(QString, expectedName);
    QFETCH(QString, expectedBasis);
    QFETCH(bool, expectedRequireAtomic);
    QFETCH(QString, expectedDiffDetail);

    TestVCAuthoringApplyBridge bridge;
    bridge.nextPrepareResult.basis = QStringLiteral("basis-page-op");
    bridge.nextPrepareResult.preparationToken = QStringLiteral("prep-page-op");
    bridge.nextPrepareResult.atomicScope = VCBridge::AuthoringPreparationResult::AtomicScope::IndependentItems;
    bridge.nextPrepareResult.diffDetail = VCBridge::AuthoringPreparationResult::DiffDetail::Summary;

    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    bool threw = false;
    const Json result = invokeNoThrow(tm, "vc_prepare_layout", Json::parse(argsJson.constData()), threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.prepareAuthoringCalls, 1);
    QCOMPARE(bridge.applyAuthoringCalls, 0);
    QVERIFY2(bridge.lastLayoutRequest.has_value(), "Expected AuthoringLayoutRequest dispatch");

    const VCBridge::AuthoringLayoutRequest &captured = *bridge.lastLayoutRequest;
    QCOMPARE(int(captured.operation), expectedOperation);
    QCOMPARE(captured.expectedBasis, expectedBasis);
    QCOMPARE(captured.requireAtomic, expectedRequireAtomic);
    QCOMPARE(captured.diffDetail, expectedDiffDetail);

    if (expectedPageWidgetID >= 0)
    {
        QVERIFY(captured.pageWidgetID.has_value());
        QCOMPARE(int(captured.pageWidgetID.value()), expectedPageWidgetID);
    }
    else
    {
        QVERIFY(!captured.pageWidgetID.has_value());
    }

    if (!expectedName.isEmpty())
    {
        QVERIFY(captured.name.has_value());
        QCOMPARE(captured.name.value(), expectedName);
    }
    else
    {
        QVERIFY(!captured.name.has_value());
    }

    QVERIFY2(result.is_object(), result.dump().c_str());
    QCOMPARE(result.value("preparationToken", std::string()), std::string("prep-page-op"));
}

void VCAuthoringApplyTools_Test::prepareRecordPanel_registeredWithExpectedSchemaAndAnnotations()
{
    TestVCAuthoringApplyBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    QVERIFY2(tm.has("vc_prepare_record_panel"), "Expected vc_prepare_record_panel to be registered");
    if (!tm.has("vc_prepare_record_panel"))
        return;

    const auto &tool = tm.get("vc_prepare_record_panel");
    QVERIFY(tool.annotations().has_value());
    const Json annotations = *tool.annotations();
    QCOMPARE(annotations.value("readOnlyHint", false), true);
    QCOMPARE(annotations.value("destructiveHint", true), false);
    QCOMPARE(annotations.value("idempotentHint", true), false);
    QCOMPARE(annotations.value("openWorldHint", true), false);

    const Json schema = tool.input_schema();
    QCOMPARE(schema.value("type", std::string()), std::string("object"));
    QVERIFY(!schemaContainsCombinators(schema));
    QVERIFY(schema.contains("required"));
    QCOMPARE(schema.at("required"), Json::array({"items"}));
    QVERIFY(schema.contains("properties"));
    const Json props = schema.at("properties");
    for (const char *field : {"items", "expectedBasis", "requireAtomic", "diffDetail"})
        QVERIFY2(props.contains(field), field);
    const Json itemProps = props.at("items").at("items").at("properties");
    for (const char *field : {"widgetID", "targetFolder", "scenePrefix", "chaserPrefix",
                              "defaultFadeIn", "defaultHold", "defaultFadeOut"})
        QVERIFY2(itemProps.contains(field), field);
}

void VCAuthoringApplyTools_Test::prepareRecordPanel_rejectsInvalidRequestsBeforeBridge_data()
{
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<QString>("errorPart");

    QTest::newRow("unknown-top-field")
        << QByteArray(R"({"items":[{"widgetID":7,"targetFolder":"Shows"}],"bogus":1})")
        << QStringLiteral("Unknown fields");
    QTest::newRow("missing-items")
        << QByteArray(R"({"expectedBasis":"x"})")
        << QStringLiteral("items");
    QTest::newRow("items-not-array")
        << QByteArray(R"({"items":{"widgetID":7}})")
        << QStringLiteral("items");
    QTest::newRow("empty-items")
        << QByteArray(R"({"items":[]})")
        << QStringLiteral("1..128");
    QTest::newRow("item-not-object")
        << QByteArray(R"({"items":[1]})")
        << QStringLiteral("objects");
    QTest::newRow("missing-widgetid")
        << QByteArray(R"({"items":[{"targetFolder":"Shows"}]})")
        << QStringLiteral("widgetID");
    QTest::newRow("duplicate-widgetid")
        << QByteArray(R"({"items":[{"widgetID":7,"targetFolder":"A"},{"widgetID":7,"scenePrefix":"B"}]})")
        << QStringLiteral("duplicate");
    QTest::newRow("unknown-item-field")
        << QByteArray(R"({"items":[{"widgetID":7,"targetFolder":"A","bad":1}]})")
        << QStringLiteral("Unknown fields");
    QTest::newRow("no-config-fields")
        << QByteArray(R"({"items":[{"widgetID":7}]})")
        << QStringLiteral("at least one");
    QTest::newRow("string-field-type")
        << QByteArray(R"({"items":[{"widgetID":7,"targetFolder":1}]})")
        << QStringLiteral("targetFolder");
    QTest::newRow("timing-out-of-range")
        << QByteArray(R"({"items":[{"widgetID":7,"defaultFadeIn":-1}]})")
        << QStringLiteral("defaultFadeIn");
    QTest::newRow("timing-over-int-max")
        << QByteArray(R"({"items":[{"widgetID":7,"defaultFadeOut":2147483648}]})")
        << QStringLiteral("defaultFadeOut");
    QTest::newRow("invalid-diff-detail")
        << QByteArray(R"({"items":[{"widgetID":7,"targetFolder":"A"}],"diffDetail":"verbose"})")
        << QStringLiteral("diffDetail");
    QTest::newRow("expected-basis-type")
        << QByteArray(R"({"items":[{"widgetID":7,"targetFolder":"A"}],"expectedBasis":7})")
        << QStringLiteral("expectedBasis");
    QTest::newRow("require-atomic-type")
        << QByteArray(R"({"items":[{"widgetID":7,"targetFolder":"A"}],"requireAtomic":"true"})")
        << QStringLiteral("requireAtomic");
}

void VCAuthoringApplyTools_Test::prepareRecordPanel_rejectsInvalidRequestsBeforeBridge()
{
    QFETCH(QByteArray, argsJson);
    QFETCH(QString, errorPart);

    TestVCAuthoringApplyBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    QVERIFY2(tm.has("vc_prepare_record_panel"), "Expected vc_prepare_record_panel registration");
    if (!tm.has("vc_prepare_record_panel"))
        return;

    bool threw = false;
    const Json result = invokeNoThrow(tm, "vc_prepare_record_panel", Json::parse(argsJson.constData()), threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.prepareAuthoringCalls, 0);
    QCOMPARE(bridge.applyAuthoringCalls, 0);
    assertInvalidArgumentEnvelope(result, errorPart);
}

void VCAuthoringApplyTools_Test::prepareRecordPanel_capturesTypedRequestAndReturnsBridgePayload()
{
    TestVCAuthoringApplyBridge bridge;
    bridge.nextPrepareResult.basis = QStringLiteral("basis-42");
    bridge.nextPrepareResult.preparationToken = QStringLiteral("prep-42");
    bridge.nextPrepareResult.atomicScope = VCBridge::AuthoringPreparationResult::AtomicScope::IndependentItems;
    bridge.nextPrepareResult.diffDetail = VCBridge::AuthoringPreparationResult::DiffDetail::Full;

    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    const Json args = Json::parse(R"({
        "expectedBasis":"b42",
        "requireAtomic":true,
        "diffDetail":"full",
        "items":[
          {"widgetID":7,"targetFolder":"Shows","defaultFadeIn":100,"defaultHold":200,"defaultFadeOut":300},
          {"widgetID":8,"scenePrefix":"SC","chaserPrefix":"CH"}
        ]
      })");

    bool threw = false;
    const Json result = invokeNoThrow(tm, "vc_prepare_record_panel", args, threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.prepareAuthoringCalls, 1);
    QCOMPARE(bridge.applyAuthoringCalls, 0);
    QVERIFY2(bridge.lastRecordPanelRequest.has_value(), "Expected AuthoringRecordPanelRequest dispatch");

    const VCBridge::AuthoringRecordPanelRequest &captured = *bridge.lastRecordPanelRequest;
    QCOMPARE(captured.expectedBasis, QStringLiteral("b42"));
    QCOMPARE(captured.requireAtomic, true);
    QCOMPARE(captured.diffDetail, QStringLiteral("full"));
    QCOMPARE(captured.items.size(), 2);

    const VCBridge::AuthoringRecordPanelItem &first = captured.items.at(0);
    QCOMPARE(first.widgetID, quint32(7));
    QVERIFY(first.targetFolder.has_value());
    QCOMPARE(first.targetFolder.value_or(QString()), QStringLiteral("Shows"));
    QVERIFY(first.defaultFadeIn.has_value());
    QCOMPARE(first.defaultFadeIn.value_or(0), uint(100));
    QVERIFY(first.defaultHold.has_value());
    QCOMPARE(first.defaultHold.value_or(0), uint(200));
    QVERIFY(first.defaultFadeOut.has_value());
    QCOMPARE(first.defaultFadeOut.value_or(0), uint(300));
    QVERIFY(!first.scenePrefix.has_value());
    QVERIFY(!first.chaserPrefix.has_value());

    const VCBridge::AuthoringRecordPanelItem &second = captured.items.at(1);
    QCOMPARE(second.widgetID, quint32(8));
    QVERIFY(!second.targetFolder.has_value());
    QVERIFY(second.scenePrefix.has_value());
    QCOMPARE(second.scenePrefix.value_or(QString()), QStringLiteral("SC"));
    QVERIFY(second.chaserPrefix.has_value());
    QCOMPARE(second.chaserPrefix.value_or(QString()), QStringLiteral("CH"));
    QVERIFY(!second.defaultFadeIn.has_value());
    QVERIFY(!second.defaultHold.has_value());
    QVERIFY(!second.defaultFadeOut.has_value());

    QVERIFY2(result.is_object(), result.dump().c_str());
    QCOMPARE(result.value("preparationToken", std::string()), std::string("prep-42"));
    QCOMPARE(result.value("atomicScope", std::string()), std::string("independentItems"));
    QCOMPARE(result.value("diffDetail", std::string()), std::string("full"));
}

void VCAuthoringApplyTools_Test::applyPreparation_rejectsInvalidRequestsBeforeBridge_data()
{
    QTest::addColumn<QByteArray>("toolName");
    QTest::addColumn<QByteArray>("argsJson");
    QTest::addColumn<QString>("errorPart");

    QTest::newRow("missing-token")
        << QByteArray("vc_apply_preparation")
        << QByteArray(R"({})")
        << QStringLiteral("preparationToken");
    QTest::newRow("token-not-string")
        << QByteArray("vc_apply_preparation")
        << QByteArray(R"({"preparationToken":1})")
        << QStringLiteral("string");
    QTest::newRow("token-empty")
        << QByteArray("vc_apply_preparation")
        << QByteArray(R"({"preparationToken":"   "})")
        << QStringLiteral("non-empty");
    QTest::newRow("unknown-field")
        << QByteArray("vc_apply_preparation")
        << QByteArray(R"({"preparationToken":"abc","unexpected":true})")
        << QStringLiteral("unexpected");
    QTest::newRow("record-panel-invalid-timing")
        << QByteArray("vc_update_widgets")
        << QByteArray(R"({"items":[{"widgetID":99,"caption":"Should Not Apply","defaultHold":-1}]})")
        << QStringLiteral("defaultHold");
    QTest::newRow("record-panel-timing-over-int-max")
        << QByteArray("vc_update_widgets")
        << QByteArray(R"({"items":[{"widgetID":99,"defaultFadeOut":2147483648}]})")
        << QStringLiteral("defaultFadeOut");
    QTest::newRow("record-panel-folder-not-string")
        << QByteArray("vc_update_widgets")
        << QByteArray(R"({"items":[{"widgetID":99,"targetFolder":42}]})")
        << QStringLiteral("targetFolder");
}

void VCAuthoringApplyTools_Test::applyPreparation_rejectsInvalidRequestsBeforeBridge()
{
    QFETCH(QByteArray, toolName);
    QFETCH(QByteArray, argsJson);
    QFETCH(QString, errorPart);

    TestVCAuthoringApplyBridge bridge;
    bridge.recordPanelWidgetID = 99;
    bridge.recordPanelWidgetDetails.id = 99;
    bridge.recordPanelWidgetDetails.type = QStringLiteral("RecordPanel");
    bridge.recordPanelWidgetDetails.machineType = QStringLiteral("recordPanel");
    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    QVERIFY2(tm.has(toolName.constData()), toolName.constData());
    if (!tm.has(toolName.constData()))
        return;

    bool threw = false;
    const Json result = invokeNoThrow(tm, toolName.constData(), Json::parse(argsJson.constData()), threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.applyAuthoringCalls, 0);
    QCOMPARE(bridge.configureRecordPanelCalls, 0);
    QCOMPARE(bridge.setWidgetCaptionCalls, 0);

    QString error;
    if (toolName == QByteArray("vc_apply_preparation"))
    {
        QVERIFY2(result.is_object(), result.dump().c_str());
        QCOMPARE(QString::fromStdString(result.value("code", std::string())), QStringLiteral("invalidArgument"));
        QVERIFY2(result.contains("error"), result.dump().c_str());
        error = QString::fromStdString(result.value("error", std::string()));
    }
    else
    {
        QVERIFY2(result.is_array(), result.dump().c_str());
        QCOMPARE(int(result.size()), 1);
        QVERIFY2(result.at(0).is_object(), result.dump().c_str());
        QVERIFY2(result.at(0).contains("error"), result.dump().c_str());
        error = QString::fromStdString(result.at(0).value("error", std::string()));
    }
    QVERIFY2(error.contains(errorPart, Qt::CaseInsensitive), qPrintable(error));
}

void VCAuthoringApplyTools_Test::applyPreparation_invokesBridgeAndReturnsBridgePayload()
{
    TestVCAuthoringApplyBridge bridge;
    bridge.nextApplyResult.applied = true;
    bridge.nextApplyResult.tokenConsumed = true;
    bridge.nextApplyResult.atomicScope = VCBridge::AuthoringApplyResult::AtomicScope::IndependentItems;
    VCBridge::AuthoringApplyItem item;
    item.index = 0;
    item.status = VCBridge::AuthoringApplyItem::Status::Ok;
    item.outcome = VCBridge::AuthoringApplyItem::Outcome::Applied;
    item.targetWidgetID = 7;
    item.changedCount = 1;
    VCBridge::AuthoringFieldChange change;
    change.field = QStringLiteral("functionID");
    change.before = qulonglong(0);
    change.after = qulonglong(99);
    change.changed = true;
    change.note = QStringLiteral("applied");
    item.changes.append(change);
    bridge.nextApplyResult.items.append(item);

    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    bool threw = false;
    const Json result = invokeNoThrow(
        tm, "vc_apply_preparation", Json{{"preparationToken", "prepared-token"}}, threw);
    QVERIFY2(!threw, result.dump().c_str());
    QCOMPARE(bridge.applyAuthoringCalls, 1);
    QCOMPARE(bridge.lastToken, QStringLiteral("prepared-token"));
    QVERIFY2(result.is_object(), result.dump().c_str());
    QCOMPARE(result.value("applied", false), true);
    QCOMPARE(result.value("tokenConsumed", false), true);
    QCOMPARE(result.value("atomicScope", std::string()), std::string("independentItems"));
    QVERIFY(result.contains("items"));
    QVERIFY(result.at("items").is_array());
    QCOMPARE(int(result.at("items").size()), 1);
    QCOMPARE(result.at("items").at(0).value("targetWidgetID", -1), 7);
}

void VCAuthoringApplyTools_Test::updateWidgets_recordPanel_acceptsValidPartialConfig()
{
    TestVCAuthoringApplyBridge bridge;
    bridge.recordPanelWidgetID = 99;
    bridge.recordPanelWidgetDetails.id = 99;
    bridge.recordPanelWidgetDetails.type = QStringLiteral("RecordPanel");
    bridge.recordPanelWidgetDetails.machineType = QStringLiteral("recordPanel");

    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    QVERIFY2(tm.has("vc_update_widgets"), "Expected vc_update_widgets registration");
    if (!tm.has("vc_update_widgets"))
        return;

    bool threw = false;
    const Json result = invokeNoThrow(
        tm,
        "vc_update_widgets",
        Json::parse(R"({"items":[{"widgetID":99,"defaultHold":1234}]})"),
        threw);
    QVERIFY2(!threw, result.dump().c_str());

    QCOMPARE(bridge.configureRecordPanelCalls, 1);
    QCOMPARE(bridge.lastConfiguredRecordPanelWidgetID, 99);
    QVERIFY(bridge.lastRecordPanelConfig.defaultHold.has_value());
    QCOMPARE(bridge.lastRecordPanelConfig.defaultHold.value_or(0), uint(1234));
    QVERIFY(!bridge.lastRecordPanelConfig.targetFolder.has_value());
    QVERIFY(!bridge.lastRecordPanelConfig.scenePrefix.has_value());
    QVERIFY(!bridge.lastRecordPanelConfig.chaserPrefix.has_value());
    QVERIFY(!bridge.lastRecordPanelConfig.defaultFadeIn.has_value());
    QVERIFY(!bridge.lastRecordPanelConfig.defaultFadeOut.has_value());
    QCOMPARE(bridge.setWidgetCaptionCalls, 0);
    QCOMPARE(bridge.applyAuthoringCalls, 0);

    QVERIFY2(result.is_array(), result.dump().c_str());
    QCOMPARE(int(result.size()), 1);
    QVERIFY2(result.at(0).is_object(), result.dump().c_str());
    QCOMPARE(result.at(0).value("status", std::string()), std::string("ok"));
    QCOMPARE(result.at(0).value("widgetID", -1), 99);
    QVERIFY(result.at(0).contains("changes"));
    QVERIFY2(result.at(0).at("changes").is_array(), result.dump().c_str());
    QCOMPARE(int(result.at(0).at("changes").size()), 1);
    QCOMPARE(result.at(0).at("changes").at(0).value("property", std::string()), std::string("recordPanelConfig"));
    QCOMPARE(result.at(0).at("changes").at(0).value("status", std::string()), std::string("ok"));
}

QTEST_MAIN(VCAuthoringApplyTools_Test)
