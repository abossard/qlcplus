#include <QtTest>
#include <nlohmann/json.hpp>

#include <algorithm>

#include "vc_tool_surface_test.h"
#include "tool_registry.h"
#include "vcbridge.h"
#include "doc.h"
#include "function.h"

#include <fastmcpp/tools/manager.hpp>

using Json = nlohmann::json;

namespace {

class TestVCBridge final : public VCBridge
{
public:
    TestVCBridge()
    {
        WidgetDetails frame;
        frame.id = 42;
        frame.type = "Frame";
        frame.machineType = "frame";
        frame.multipageMode = true;
        frame.currentPage = 0;
        frame.totalPages = 3;
        details.insert(frame.id, frame);
    }

    int addPage(const QString &) override { return 0; }
    QList<PageInfo> pages() const override { return {}; }
    int pagesCount() const override { return 0; }
    int addFrame(int, const QRect &, const QString &, bool) override { return 1; }
    int addButton(int parentID, const QRect &geometry, quint32 functionID,
                  const QString &caption, const QString &action, int) override
    {
        ++creationCalls;
        WidgetDetails button;
        button.id = 100;
        button.type = "Button";
        button.machineType = "button";
        button.parentID = parentID;
        button.geometry = geometry;
        button.functionID = functionID;
        button.caption = caption;
        button.action = action;
        details.insert(button.id, button);
        return button.id;
    }
    int addSlider(int, const QRect &, const QString &, const QString &, quint32,
                  const QList<QPair<quint32, quint32>> &) override
        { ++creationCalls; return 3; }
    int addXYPad(int, const QRect &, const QList<quint32> &) override { return 4; }
    int addCueList(int, const QRect &, quint32, const QString &) override { return 5; }
    int addLabel(int, const QRect &, const QString &) override { return 6; }
    bool mapWidgetInput(int, quint32, quint32) override { return true; }
    bool setWidgetFeedback(int, int, int, int, int, int, int) override { return true; }
    bool setWidgetColors(int, const QColor &, const QColor &) override { return true; }
    int addSpeedDial(int, const QRect &, const QList<quint32> &) override { return 7; }
    int addAudioTriggers(int, const QRect &) override { ++actuationCalls; return 8; }
    int addClock(int, const QRect &, const QString &) override { return 9; }
    int addRecordPanel(int, const QRect &) override { return 10; }
    WidgetDetails getWidgetDetails(int widgetID) const override
    {
        if (details.contains(widgetID))
            return details.value(widgetID);
        WidgetDetails details;
        details.id = widgetID;
        if (widgetID == 1) { details.type = "XY Pad"; details.machineType = "xypad"; }
        else if (widgetID == 2) { details.type = "Frame"; details.machineType = "frame"; }
        else if (widgetID == 3) { details.type = "Audio Triggers"; details.machineType = "audioTrigger"; }
        else details.id = -1;
        return details;
    }
    bool setXYPadPosition(int, qreal, qreal) override { ++actuationCalls; return true; }
    bool setAudioTriggerCapture(int, bool) override { ++actuationCalls; return true; }
    bool setAudioTriggerVolume(int, int) override { ++actuationCalls; return true; }
    bool configureFrame(int, const FrameConfig &) override { ++actuationCalls; return true; }
    int findWidgetByCaption(int parentID, const QString &type,
                            const QString &caption) const override
    {
        for (const WidgetDetails &entry : details)
            if (entry.parentID == parentID && entry.machineType == type && entry.caption == caption)
                return entry.id;
        return -1;
    }
    bool setWidgetPage(int widgetID, int pageIndex) override
    {
        ++pageMoveCalls;
        if (rejectPageMoves)
            return false;
        if (!details.contains(widgetID) || pageIndex < 0 || pageIndex >= 3)
            return false;
        details[widgetID].childPageIndex = pageIndex;
        return true;
    }
    bool configureSlider(int widgetID, const SliderConfig &config) override
    {
        ++configurationCalls;
        if (details.contains(widgetID) && config.clickAndGoType)
            details[widgetID].clickAndGoType = *config.clickAndGoType;
        return true;
    }
    bool setWidgetCaption(int widgetID, const QString &caption) override
    {
        if (!details.contains(widgetID))
            return false;
        ++configurationCalls;
        details[widgetID].caption = caption;
        return true;
    }
    bool setButtonAction(int widgetID, const QString &action) override
    {
        if (!details.contains(widgetID))
            return false;
        ++configurationCalls;
        details[widgetID].action = action;
        return true;
    }
    bool configureButton(int widgetID, const ButtonConfig &config) override
    {
        if (!details.contains(widgetID))
            return false;
        if (config.functionID)
            details[widgetID].functionID = *config.functionID;
        return !config.action || setButtonAction(widgetID, *config.action);
    }
    int createWidgetOnPage(int parentID, const QString &type, const QRect &geometry,
                           const QString &caption, int subpage) override
    {
        if (type != "Button")
            return -1;
        const int id = addButton(parentID, geometry, Function::invalidId(),
                                 caption, QStringLiteral("toggle"), 0);
        details[id].childPageIndex = subpage;
        return id;
    }
    WidgetSnapshot snapshotFrame(int frameID) const override
    {
        WidgetSnapshot snapshot;
        snapshot.id = frameID;
        snapshot.type = 4;
        snapshot.geometry = QRect(0, 0, 400, 300);
        WidgetSnapshot button;
        button.id = 100;
        button.type = 1;
        button.geometry = QRect(5, 45, 100, 60);
        snapshot.children.append(button);
        return snapshot;
    }
    LayoutEditResult alignLayout(int frameID, int pageIndex, int columns,
                                 int padding, bool dryRun) override
    {
        ++alignmentCalls;
        alignmentFrame = frameID;
        alignmentPage = pageIndex;
        alignmentColumns = columns;
        alignmentPadding = padding;
        LayoutEditResult result;
        result.geometries.insert(100, QRectF(1.25, 45, 100.5, 60));
        result.applied = !dryRun;
        return result;
    }

    int alignmentCalls = 0;
    int alignmentFrame = -1;
    int alignmentPage = -1;
    int alignmentColumns = -1;
    int alignmentPadding = -1;
    bool rejectPageMoves = false;
    int pageMoveCalls = 0;
    int actuationCalls = 0;
    int creationCalls = 0;
    int configurationCalls = 0;
    QMap<int, WidgetDetails> details;
};

Json itemProperties(const Json &schema)
{
    return schema.at("properties").at("items").at("items").at("properties");
}

Json parsedToolResult(const Json &value)
{
    return value.is_string() ? Json::parse(value.get<std::string>()) : value;
}

}

void VCToolSurface_Test::init()
{
    m_doc = new Doc(this);
}

void VCToolSurface_Test::cleanup()
{
    delete m_doc;
    m_doc = nullptr;
}

void VCToolSurface_Test::runtimeFields_absentFromSchemas_data()
{
    QTest::addColumn<QString>("toolName");
    QTest::addColumn<QString>("fieldName");
    QTest::addColumn<bool>("mustRemain");

    for (const char *field : {"xyPadPosition", "currentPage", "captureEnabled", "volumeLevel"})
        QTest::newRow(qPrintable(QString("update-%1").arg(field)))
            << QString("vc_update_widgets") << QString(field) << false;
    QTest::newRow("create-captureEnabled")
        << QString("vc_create_widgets") << QString("captureEnabled") << false;
    QTest::newRow("create-volumeLevel")
        << QString("vc_create_widgets") << QString("volumeLevel") << true;
    QTest::newRow("update-caption")
        << QString("vc_update_widgets") << QString("caption") << true;
    QTest::newRow("create-parentID")
        << QString("vc_create_widgets") << QString("parentID") << true;
}

void VCToolSurface_Test::runtimeFields_absentFromSchemas()
{
    QFETCH(QString, toolName);
    QFETCH(QString, fieldName);
    QFETCH(bool, mustRemain);

    TestVCBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCCreateTools(tm, m_doc, &bridge);
    registerVCUpdateTools(tm, m_doc, &bridge);

    const Json properties = itemProperties(tm.input_schema_for(toolName.toStdString()));
    QCOMPARE(properties.contains(fieldName.toStdString()), mustRemain);
}

void VCToolSurface_Test::legacyRuntimeFields_rejectedWithoutActuation_data()
{
    QTest::addColumn<QString>("toolName");
    QTest::addColumn<QByteArray>("arguments");

    QTest::newRow("update-xy") << QString("vc_update_widgets")
        << QByteArray(R"({"items":[{"widgetID":1,"xyPadPosition":{"x":0.2,"y":0.8}}]})");
    QTest::newRow("update-page") << QString("vc_update_widgets")
        << QByteArray(R"({"items":[{"widgetID":2,"currentPage":1}]})");
    QTest::newRow("update-capture") << QString("vc_update_widgets")
        << QByteArray(R"({"items":[{"widgetID":3,"captureEnabled":true}]})");
    QTest::newRow("update-volume") << QString("vc_update_widgets")
        << QByteArray(R"({"items":[{"widgetID":3,"volumeLevel":220}]})");
    QTest::newRow("create-capture") << QString("vc_create_widgets")
        << QByteArray(R"({"items":[{"type":"audioTrigger","parentID":7,"captureEnabled":true}]})");
}

void VCToolSurface_Test::legacyRuntimeFields_rejectedWithoutActuation()
{
    QFETCH(QString, toolName);
    QFETCH(QByteArray, arguments);

    TestVCBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCCreateTools(tm, m_doc, &bridge);
    registerVCUpdateTools(tm, m_doc, &bridge);

    const Json result = parsedToolResult(
        tm.invoke(toolName.toStdString(), Json::parse(arguments.constData())));
    QVERIFY(result.is_array());
    QCOMPARE(result.size(), size_t(1));
    QVERIFY2(result[0].contains("error"), result.dump().c_str());
    QCOMPARE(bridge.actuationCalls, 0);
}

void VCToolSurface_Test::childPageIndex_createAndUpsert_preservesCurrentPage()
{
    TestVCBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCCreateTools(tm, m_doc, &bridge);
    registerQueryTools(tm, m_doc, &bridge);

    const Json args = {{"items", Json::array({{
        {"type", "button"}, {"parentID", 42}, {"caption", "Page 2 Go"},
        {"childPageIndex", 1}
    }})}};

    const Json created = parsedToolResult(tm.invoke("vc_create_widgets", args));
    QCOMPARE(created[0]["outcome"].get<std::string>(), std::string("created"));
    const int widgetID = created[0]["widgetID"].get<int>();
    Json queried = parsedToolResult(
        tm.invoke("vc_query_widgets", {{"widgetIDs", Json::array({widgetID, 42})}}));
    QCOMPARE(queried[0]["childPageIndex"].get<int>(), 1);
    QCOMPARE(queried[1]["currentPage"].get<int>(), 0);

    const Json repeated = parsedToolResult(tm.invoke("vc_create_widgets", args));
    QCOMPARE(repeated[0]["outcome"].get<std::string>(), std::string("existing"));
    queried = parsedToolResult(tm.invoke("vc_query_widgets", {{"widgetIDs", Json::array({widgetID, 42})}}));
    QCOMPARE(queried[0]["childPageIndex"].get<int>(), 1);
    QCOMPARE(queried[1]["currentPage"].get<int>(), 0);
}

void VCToolSurface_Test::invalidChildPageIndex_rejectedBeforeCreateOrUpsert_data()
{
    QTest::addColumn<bool>("existing");
    QTest::addColumn<QByteArray>("arguments");

    QTest::newRow("new-widget")
        << false
        << QByteArray(R"({"items":[{"type":"button","parentID":42,"caption":"Invalid New","childPageIndex":99}]})");
    QTest::newRow("caption-matched-upsert")
        << true
        << QByteArray(R"({"items":[{"type":"slider","parentID":42,"caption":"Existing Slider","mode":"level","clickAndGoType":"colors","childPageIndex":99}]})");
}

void VCToolSurface_Test::invalidChildPageIndex_rejectedBeforeCreateOrUpsert()
{
    QFETCH(bool, existing);
    QFETCH(QByteArray, arguments);

    TestVCBridge bridge;
    if (existing)
    {
        VCBridge::WidgetDetails slider;
        slider.id = 101;
        slider.type = "Slider";
        slider.machineType = "slider";
        slider.parentID = 42;
        slider.caption = "Existing Slider";
        slider.childPageIndex = 1;
        slider.clickAndGoType = "none";
        bridge.details.insert(slider.id, slider);
    }

    fastmcpp::tools::ToolManager tm;
    registerVCCreateTools(tm, m_doc, &bridge);
    registerQueryTools(tm, m_doc, &bridge);
    const int widgetCountBefore = bridge.details.size();
    const QString clickAndGoBefore = existing
        ? bridge.details.value(101).clickAndGoType
        : QString();

    const Json result = parsedToolResult(
        tm.invoke("vc_create_widgets", Json::parse(arguments.constData())));

    QVERIFY(result.is_array());
    QCOMPARE(result.size(), size_t(1));
    QVERIFY2(result[0].contains("error"), result.dump().c_str());
    QVERIFY2(result[0]["error"].get<std::string>().find("outside the parent frame page range") != std::string::npos,
             result.dump().c_str());
    QCOMPARE(bridge.creationCalls, 0);
    QCOMPARE(bridge.configurationCalls, 0);
    QCOMPARE(bridge.details.size(), widgetCountBefore);
    if (existing)
    {
        QCOMPARE(bridge.details.value(101).childPageIndex, 1);
        QCOMPARE(bridge.details.value(101).clickAndGoType, clickAndGoBefore);
    }
    else
    {
        QVERIFY(!bridge.details.contains(100));
    }
}

void VCToolSurface_Test::buttonActions_advertisedInSchemas_data()
{
    QTest::addColumn<QString>("toolName");
    QTest::addColumn<QString>("action");

    for (const char *tool : {"vc_create_widgets", "vc_update_widgets"})
        for (const char *action : {"toggle", "flash", "blackout", "stopall", "freeze", "freezehold"})
            QTest::newRow(qPrintable(QString("%1-%2").arg(tool).arg(action)))
                << QString(tool) << QString(action);
}

void VCToolSurface_Test::buttonActions_advertisedInSchemas()
{
    QFETCH(QString, toolName);
    QFETCH(QString, action);

    TestVCBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCCreateTools(tm, m_doc, &bridge);
    registerVCUpdateTools(tm, m_doc, &bridge);

    const Json properties = itemProperties(tm.input_schema_for(toolName.toStdString()));
    QVERIFY2(properties.contains("action"), toolName.toUtf8().constData());
    const Json advertised = properties.at("action").at("enum");
    QVERIFY2(std::find(advertised.begin(), advertised.end(), Json(action.toStdString())) != advertised.end(),
             advertised.dump().c_str());
}

void VCToolSurface_Test::freezeAction_captionUpsertKeepsIdentity()
{
    TestVCBridge bridge;
    VCBridge::WidgetDetails existing;
    existing.id = 200;
    existing.type = "Button";
    existing.machineType = "button";
    existing.parentID = 42;
    existing.caption = "Hold the look";
    existing.action = "toggle";
    bridge.details.insert(existing.id, existing);

    fastmcpp::tools::ToolManager tm;
    registerVCCreateTools(tm, m_doc, &bridge);
    registerVCUpdateTools(tm, m_doc, &bridge);

    // A caption-matched create keeps the widget's identity and its action.
    const Json created = parsedToolResult(tm.invoke("vc_create_widgets", {{"items", Json::array({{
        {"type", "button"}, {"parentID", 42}, {"caption", "Hold the look"}, {"action", "freeze"}
    }})}}));
    QCOMPARE(created[0]["outcome"].get<std::string>(), std::string("existing"));
    QCOMPARE(created[0]["widgetID"].get<int>(), 200);
    QCOMPARE(bridge.creationCalls, 0);
    QCOMPARE(bridge.details.value(200).action, QString("toggle"));

    // An explicit update is what changes it.
    const Json updated = parsedToolResult(tm.invoke("vc_update_widgets", {{"items", Json::array({{
        {"widgetID", 200}, {"action", "freeze"}
    }})}}));
    QVERIFY2(!updated[0].contains("error"), updated.dump().c_str());
    QCOMPARE(bridge.details.value(200).action, QString("freeze"));
}

void VCToolSurface_Test::invalidButtonAction_rejectedBeforeMutation_data()
{
    QTest::addColumn<QByteArray>("arguments");

    // Each batch pairs a rejected item with a valid sibling. The rejected item also
    // carries a caption change, so a mutation before validation would be visible.
    QTest::newRow("update-unknown-action") << QByteArray(
        R"({"items":[{"widgetID":100,"action":"frezee","caption":"Renamed"},)"
        R"({"widgetID":101,"caption":"Sibling Renamed"}]})");
    QTest::newRow("update-cross-type-action") << QByteArray(
        R"({"items":[{"widgetID":102,"action":"freeze","caption":"Renamed"},)"
        R"({"widgetID":101,"caption":"Sibling Renamed"}]})");
    QTest::newRow("update-uppercase-action") << QByteArray(
        R"({"items":[{"widgetID":100,"action":"Freeze","caption":"Renamed"},)"
        R"({"widgetID":101,"caption":"Sibling Renamed"}]})");
}

void VCToolSurface_Test::invalidButtonAction_rejectedBeforeMutation()
{
    QFETCH(QByteArray, arguments);

    TestVCBridge bridge;
    VCBridge::WidgetDetails button;
    button.id = 100;
    button.type = "Button";
    button.machineType = "button";
    button.caption = "Original Button";
    button.action = "toggle";
    bridge.details.insert(button.id, button);

    VCBridge::WidgetDetails sibling;
    sibling.id = 101;
    sibling.type = "Button";
    sibling.machineType = "button";
    sibling.caption = "Original Sibling";
    sibling.action = "toggle";
    bridge.details.insert(sibling.id, sibling);

    VCBridge::WidgetDetails slider;
    slider.id = 102;
    slider.type = "Slider";
    slider.machineType = "slider";
    slider.caption = "Original Slider";
    bridge.details.insert(slider.id, slider);

    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);

    const Json result = parsedToolResult(
        tm.invoke("vc_update_widgets", Json::parse(arguments.constData())));

    QVERIFY(result.is_array());
    QCOMPARE(result.size(), size_t(2));
    QVERIFY2(result[0].contains("error"), result.dump().c_str());

    // The rejected item mutated nothing, not even its valid caption field.
    QCOMPARE(bridge.details.value(100).caption, QString("Original Button"));
    QCOMPARE(bridge.details.value(100).action, QString("toggle"));
    QCOMPARE(bridge.details.value(102).caption, QString("Original Slider"));

    // The valid sibling in the same batch still applied.
    QVERIFY2(!result[1].contains("error"), result.dump().c_str());
    QCOMPARE(bridge.details.value(101).caption, QString("Sibling Renamed"));
}

void VCToolSurface_Test::setupVCTools_remainRegistered()
{
    TestVCBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCCreateTools(tm, m_doc, &bridge);
    registerVCInputTools(tm, m_doc, &bridge);
    registerVCLayoutTools(tm, m_doc, &bridge);

    for (const char *name : {"vc_create_widgets", "vc_map_inputs", "vc_reflow_frame"})
        QVERIFY2(tm.has(name), name);
}

void VCToolSurface_Test::alignmentAlgorithm_advertised()
{
    TestVCBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCLayoutTools(tm, m_doc, &bridge);
    const Json algorithms = tm.input_schema_for("vc_reflow_frame")
        .at("properties").at("algorithm").at("enum");
    QVERIFY(std::find(algorithms.begin(), algorithms.end(), Json("align")) != algorithms.end());
}

void VCToolSurface_Test::layoutApplyFailure_reported()
{
    TestVCBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCLayoutTools(tm, m_doc, &bridge);
    const Json result = parsedToolResult(tm.invoke(
        "vc_reflow_frame", Json{{"frameID", 42}, {"dryRun", false}}));
    QVERIFY2(result.contains("error"), result.dump().c_str());
    QVERIFY(!result.value("applied", false));
}

void VCToolSurface_Test::geometryFailure_preservesOtherFields_data()
{
    QTest::addColumn<QByteArray>("geometry");
    QTest::newRow("move") << QByteArray(R"({"x":10,"y":20})");
    QTest::newRow("resize") << QByteArray(R"({"width":160,"height":90})");
}

void VCToolSurface_Test::geometryFailure_preservesOtherFields()
{
    QFETCH(QByteArray, geometry);
    TestVCBridge bridge;
    VCBridge::WidgetDetails button;
    button.id = 100;
    button.type = "Button";
    button.machineType = "button";
    button.caption = "Original";
    button.geometry = QRect(5, 45, 100, 60);
    bridge.details.insert(button.id, button);
    Json item = Json::parse(geometry.constData());
    item["widgetID"] = 100;
    item["caption"] = "Changed";
    fastmcpp::tools::ToolManager tm;
    registerVCUpdateTools(tm, m_doc, &bridge);
    const Json result = parsedToolResult(tm.invoke(
        "vc_update_widgets", Json{{"items", Json::array({item})}}));
    QVERIFY2(result.at(0).contains("error"), result.dump().c_str());
    QCOMPARE(bridge.details.value(100).caption, QString("Original"));
    QCOMPARE(bridge.details.value(100).geometry, QRect(5, 45, 100, 60));
    QCOMPARE(bridge.configurationCalls, 0);
}

void VCToolSurface_Test::alignmentDispatch_preservesFractionalGeometry()
{
    TestVCBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCLayoutTools(tm, m_doc, &bridge);
    const Json result = parsedToolResult(tm.invoke("vc_reflow_frame",
        Json{{"algorithm", "align"}, {"frameID", 42}, {"columns", 3},
             {"pad", 7}, {"dryRun", true}}));
    QVERIFY2(!result.contains("error"), result.dump().c_str());
    QCOMPARE(bridge.alignmentCalls, 1);
    QCOMPARE(bridge.alignmentFrame, 42);
    QCOMPARE(bridge.alignmentPage, -1);
    QCOMPARE(bridge.alignmentColumns, 3);
    QCOMPARE(bridge.alignmentPadding, 7);
    QVERIFY(!result.at("applied").get<bool>());
    QCOMPARE(result.at("changes").at(0).at("geometry").at("x").get<double>(), 1.25);
    QCOMPARE(result.at("changes").at(0).at("geometry").at("width").get<double>(), 100.5);
}

void VCToolSurface_Test::alignmentSizingOptions_rejected_data()
{
    QTest::addColumn<QString>("option");
    for (const char *option : {"buttonWidth", "buttonHeight", "sliderWidth", "sliderHeight", "framePad"})
        QTest::newRow(option) << QString::fromLatin1(option);
}

void VCToolSurface_Test::alignmentSizingOptions_rejected()
{
    QFETCH(QString, option);
    TestVCBridge bridge;
    fastmcpp::tools::ToolManager tm;
    registerVCLayoutTools(tm, m_doc, &bridge);
    Json request{{"algorithm", "align"}, {"frameID", 42}, {"dryRun", true}};
    request[option.toStdString()] = 100;
    const Json result = parsedToolResult(tm.invoke("vc_reflow_frame", request));
    QVERIFY2(result.contains("error"), result.dump().c_str());
    QCOMPARE(bridge.alignmentCalls, 0);
}

void VCToolSurface_Test::rejectedSubpageUpsert_preservesFields_data()
{
    QTest::addColumn<QString>("type");
    QTest::newRow("slider") << QStringLiteral("slider");
    QTest::newRow("frame") << QStringLiteral("frame");
}

void VCToolSurface_Test::rejectedSubpageUpsert_preservesFields()
{
    QFETCH(QString, type);
    TestVCBridge bridge;
    VCBridge::WidgetDetails widget;
    widget.id = 101;
    widget.parentID = 42;
    widget.type = type == "slider" ? "Slider" : "Frame";
    widget.machineType = type;
    widget.caption = "Existing";
    widget.childPageIndex = 0;
    widget.clickAndGoType = "none";
    bridge.details.insert(widget.id, widget);
    bridge.rejectPageMoves = true;
    Json item{{"type", type.toStdString()}, {"parentID", 42},
              {"caption", "Existing"}, {"childPageIndex", 2}};
    if (type == "slider")
        item["clickAndGoType"] = "colors";
    fastmcpp::tools::ToolManager tm;
    registerVCCreateTools(tm, m_doc, &bridge);
    const Json result = parsedToolResult(tm.invoke("vc_create_widgets",
        Json{{"items", Json::array({item})}}));
    QVERIFY2(result.at(0).contains("error"), result.dump().c_str());
    QCOMPARE(bridge.pageMoveCalls, type == "slider" ? 1 : 0);
    QCOMPARE(bridge.configurationCalls, 0);
    QCOMPARE(bridge.details.value(101).childPageIndex, 0);
    QCOMPARE(bridge.details.value(101).clickAndGoType, QStringLiteral("none"));
}

QTEST_MAIN(VCToolSurface_Test)
