#include <QtTest>
#include <nlohmann/json.hpp>

#include "vc_tool_surface_test.h"
#include "tool_registry.h"
#include "vcbridge.h"
#include "doc.h"

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
        if (widgetID == 1) details.type = "XY Pad";
        else if (widgetID == 2) details.type = "Frame";
        else if (widgetID == 3) details.type = "Audio Triggers";
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
            if (entry.parentID == parentID && entry.type == type && entry.caption == caption)
                return entry.id;
        return -1;
    }
    bool setWidgetPage(int widgetID, int pageIndex) override
    {
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
    QCOMPARE(created[0]["status"].get<std::string>(), std::string("created"));
    const int widgetID = created[0]["widgetID"].get<int>();
    Json queried = parsedToolResult(
        tm.invoke("vc_query_widgets", {{"widgetIDs", Json::array({widgetID, 42})}}));
    QCOMPARE(queried[0]["childPageIndex"].get<int>(), 1);
    QCOMPARE(queried[1]["currentPage"].get<int>(), 0);

    const Json repeated = parsedToolResult(tm.invoke("vc_create_widgets", args));
    QCOMPARE(repeated[0]["status"].get<std::string>(), std::string("existing"));
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

QTEST_MAIN(VCToolSurface_Test)
