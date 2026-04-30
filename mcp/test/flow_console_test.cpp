/*
  Q Light Controller Plus - Unit test
  flow_console_test.cpp

  Copyright (C) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include <QtTest>
#include <QBuffer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "flow_console_test.h"
#include "flowconsole.h"
#include "vcwidget.h"
#include "doc.h"

void FlowConsole_Test::initTestCase()
{
    m_doc = new Doc(this);
    m_fc = new FlowConsole(nullptr, m_doc);
}

void FlowConsole_Test::cleanup()
{
    m_fc->resetContents();
}

/*********************************************************************
 * Pages
 *********************************************************************/

void FlowConsole_Test::pages_defaultHasOnePage()
{
    QCOMPARE(m_fc->pagesCount(), 1);
    QCOMPARE(m_fc->selectedPage(), 0);
}

void FlowConsole_Test::pages_addPage()
{
    int idx = m_fc->addPage("My Page");
    QCOMPARE(idx, 1);
    QCOMPARE(m_fc->pagesCount(), 2);
}

void FlowConsole_Test::pages_selectPage()
{
    m_fc->addPage("Page 2");
    m_fc->setSelectedPage(1);
    QCOMPARE(m_fc->selectedPage(), 1);
}

void FlowConsole_Test::pages_selectPageOutOfRange()
{
    m_fc->setSelectedPage(99);
    QCOMPARE(m_fc->selectedPage(), 0); // unchanged
    m_fc->setSelectedPage(-1);
    QCOMPARE(m_fc->selectedPage(), 0); // unchanged
}

/*********************************************************************
 * Sections — CRUD
 *********************************************************************/

void FlowConsole_Test::section_addToPage()
{
    int id = m_fc->addSection(0, "Dimmers", "full", 4, false);
    QVERIFY(id > 0);

    QVariantList sections = m_fc->sectionsForPage(0);
    QCOMPARE(sections.count(), 1);
    QVariantMap s = sections[0].toMap();
    QCOMPARE(s["caption"].toString(), QString("Dimmers"));
    QCOMPARE(s["sizePreset"].toString(), QString("full"));
    QCOMPARE(s["columns"].toInt(), 4);
    QCOMPARE(s["isSolo"].toBool(), false);
}

void FlowConsole_Test::section_addMultiple()
{
    m_fc->addSection(0, "A", "full", 4, false);
    m_fc->addSection(0, "B", "half", 3, true);
    m_fc->addSection(0, "C", "third", 2, false);

    QVariantList sections = m_fc->sectionsForPage(0);
    QCOMPARE(sections.count(), 3);
    QCOMPARE(sections[0].toMap()["caption"].toString(), QString("A"));
    QCOMPARE(sections[1].toMap()["caption"].toString(), QString("B"));
    QCOMPARE(sections[1].toMap()["isSolo"].toBool(), true);
    QCOMPARE(sections[2].toMap()["sizePreset"].toString(), QString("third"));
}

void FlowConsole_Test::section_addToInvalidPage()
{
    int id = m_fc->addSection(99, "Bad", "full", 4, false);
    QCOMPARE(id, -1);
}

void FlowConsole_Test::section_remove()
{
    int id = m_fc->addSection(0, "ToRemove", "full", 4, false);
    QCOMPARE(m_fc->sectionsForPage(0).count(), 1);

    bool ok = m_fc->removeSection(id);
    QVERIFY(ok);
    QCOMPARE(m_fc->sectionsForPage(0).count(), 0);
}

void FlowConsole_Test::section_removeNonExistent()
{
    QVERIFY(!m_fc->removeSection(99999));
}

void FlowConsole_Test::section_setCaption()
{
    int id = m_fc->addSection(0, "Old", "full", 4, false);
    QVERIFY(m_fc->setSectionCaption(id, "New"));
    QCOMPARE(m_fc->sectionsForPage(0)[0].toMap()["caption"].toString(), QString("New"));
}

void FlowConsole_Test::section_setSizePreset()
{
    int id = m_fc->addSection(0, "S", "full", 4, false);
    QVERIFY(m_fc->setSectionSizePreset(id, "half"));
    QCOMPARE(m_fc->sectionsForPage(0)[0].toMap()["sizePreset"].toString(), QString("half"));
    QVERIFY(m_fc->setSectionSizePreset(id, "quarter"));
    QCOMPARE(m_fc->sectionsForPage(0)[0].toMap()["sizePreset"].toString(), QString("quarter"));
}

void FlowConsole_Test::section_setSizePresetInvalid()
{
    int id = m_fc->addSection(0, "S", "full", 4, false);
    QVERIFY(!m_fc->setSectionSizePreset(id, "invalid"));
    QCOMPARE(m_fc->sectionsForPage(0)[0].toMap()["sizePreset"].toString(), QString("full")); // unchanged
}

void FlowConsole_Test::section_setColumns()
{
    int id = m_fc->addSection(0, "S", "full", 4, false);
    QVERIFY(m_fc->setSectionColumns(id, 6));
    QCOMPARE(m_fc->sectionsForPage(0)[0].toMap()["columns"].toInt(), 6);
}

void FlowConsole_Test::section_setColumnsClamped()
{
    int id = m_fc->addSection(0, "S", "full", 4, false);
    m_fc->setSectionColumns(id, 0);
    QCOMPARE(m_fc->sectionsForPage(0)[0].toMap()["columns"].toInt(), 1); // clamped to min 1
    m_fc->setSectionColumns(id, 100);
    QCOMPARE(m_fc->sectionsForPage(0)[0].toMap()["columns"].toInt(), 12); // clamped to max 12
}

void FlowConsole_Test::section_setCollapsed()
{
    int id = m_fc->addSection(0, "S", "full", 4, false);
    QVERIFY(m_fc->setSectionCollapsed(id, true));
    QCOMPARE(m_fc->sectionsForPage(0)[0].toMap()["isCollapsed"].toBool(), true);
    QVERIFY(m_fc->setSectionCollapsed(id, false));
    QCOMPARE(m_fc->sectionsForPage(0)[0].toMap()["isCollapsed"].toBool(), false);
}

/*********************************************************************
 * Widgets — CRUD
 *********************************************************************/

void FlowConsole_Test::widget_addButton()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int wid = m_fc->addWidget(sid, "button", 1);
    QVERIFY(wid >= 0);

    QObject *w = m_fc->widget(wid);
    QVERIFY(w != nullptr);

    QVariantList widgets = m_fc->widgetsForSection(sid);
    QCOMPARE(widgets.count(), 1);
    QCOMPARE(widgets[0].toMap()["id"].toUInt(), (quint32)wid);
    QCOMPARE(widgets[0].toMap()["colSpan"].toInt(), 1);
}

void FlowConsole_Test::widget_addSlider()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int wid = m_fc->addWidget(sid, "slider", 2);
    QVERIFY(wid >= 0);

    QVariantList widgets = m_fc->widgetsForSection(sid);
    QCOMPARE(widgets.count(), 1);
    QCOMPARE(widgets[0].toMap()["colSpan"].toInt(), 2);
}

void FlowConsole_Test::widget_addAllTypes_data()
{
    QTest::addColumn<QString>("type");
    QTest::newRow("button") << "button";
    QTest::newRow("slider") << "slider";
    QTest::newRow("label") << "label";
    QTest::newRow("clock") << "clock";
    QTest::newRow("cuelist") << "cuelist";
    QTest::newRow("xypad") << "xypad";
    QTest::newRow("speedDial") << "speedDial";
    QTest::newRow("matrix") << "matrix";
    QTest::newRow("audioTrigger") << "audioTrigger";
}

void FlowConsole_Test::widget_addAllTypes()
{
    QFETCH(QString, type);
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int wid = m_fc->addWidget(sid, type, 1);
    QVERIFY2(wid >= 0, qPrintable("Failed to create widget type: " + type));
    QVERIFY(m_fc->widget(wid) != nullptr);
}

void FlowConsole_Test::widget_addToInvalidSection()
{
    int wid = m_fc->addWidget(99999, "button", 1);
    QCOMPARE(wid, -1);
}

void FlowConsole_Test::widget_addUnknownType()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int wid = m_fc->addWidget(sid, "nonexistent", 1);
    QCOMPARE(wid, -1);
}

void FlowConsole_Test::widget_remove()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int wid = m_fc->addWidget(sid, "button", 1);

    QVERIFY(m_fc->removeWidget(wid));
    QCOMPARE(m_fc->widgetsForSection(sid).count(), 0);
    QVERIFY(m_fc->widget(wid) == nullptr);
}

void FlowConsole_Test::widget_removeNonExistent()
{
    QVERIFY(!m_fc->removeWidget(99999));
}

void FlowConsole_Test::widget_colSpan()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int wid = m_fc->addWidget(sid, "button", 1);

    QCOMPARE(m_fc->widgetColSpan(wid), 1);
    QVERIFY(m_fc->setWidgetColSpan(wid, 3));
    QCOMPARE(m_fc->widgetColSpan(wid), 3);
}

void FlowConsole_Test::widget_colSpanClamped()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int wid = m_fc->addWidget(sid, "button", 1);

    m_fc->setWidgetColSpan(wid, 10); // exceeds section columns (4)
    QCOMPARE(m_fc->widgetColSpan(wid), 4); // clamped to max

    m_fc->setWidgetColSpan(wid, 0);
    QCOMPARE(m_fc->widgetColSpan(wid), 1); // clamped to min
}

/*********************************************************************
 * Reorder
 *********************************************************************/

void FlowConsole_Test::reorder_moveUp()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int w1 = m_fc->addWidget(sid, "button", 1);
    int w2 = m_fc->addWidget(sid, "button", 1);
    int w3 = m_fc->addWidget(sid, "button", 1);

    // Move w3 up — should become [w1, w3, w2]
    QVERIFY(m_fc->moveWidgetUp(w3));
    QVariantList wl = m_fc->widgetsForSection(sid);
    QCOMPARE(wl[0].toMap()["id"].toUInt(), (quint32)w1);
    QCOMPARE(wl[1].toMap()["id"].toUInt(), (quint32)w3);
    QCOMPARE(wl[2].toMap()["id"].toUInt(), (quint32)w2);
}

void FlowConsole_Test::reorder_moveDown()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int w1 = m_fc->addWidget(sid, "button", 1);
    int w2 = m_fc->addWidget(sid, "button", 1);
    int w3 = m_fc->addWidget(sid, "button", 1);

    // Move w1 down — should become [w2, w1, w3]
    QVERIFY(m_fc->moveWidgetDown(w1));
    QVariantList wl = m_fc->widgetsForSection(sid);
    QCOMPARE(wl[0].toMap()["id"].toUInt(), (quint32)w2);
    QCOMPARE(wl[1].toMap()["id"].toUInt(), (quint32)w1);
    QCOMPARE(wl[2].toMap()["id"].toUInt(), (quint32)w3);
}

void FlowConsole_Test::reorder_moveUpAtTop()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int w1 = m_fc->addWidget(sid, "button", 1);
    m_fc->addWidget(sid, "button", 1);

    QVERIFY(!m_fc->moveWidgetUp(w1)); // already at top
}

void FlowConsole_Test::reorder_moveDownAtBottom()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    m_fc->addWidget(sid, "button", 1);
    int w2 = m_fc->addWidget(sid, "button", 1);

    QVERIFY(!m_fc->moveWidgetDown(w2)); // already at bottom
}

void FlowConsole_Test::reorder_byIndex()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int w1 = m_fc->addWidget(sid, "button", 1);
    int w2 = m_fc->addWidget(sid, "button", 1);
    int w3 = m_fc->addWidget(sid, "button", 1);

    // Move w1 to index 2 — should become [w2, w3, w1]
    QVERIFY(m_fc->reorderWidget(w1, 2));
    QVariantList wl = m_fc->widgetsForSection(sid);
    QCOMPARE(wl[0].toMap()["id"].toUInt(), (quint32)w2);
    QCOMPARE(wl[1].toMap()["id"].toUInt(), (quint32)w3);
    QCOMPARE(wl[2].toMap()["id"].toUInt(), (quint32)w1);
}

/*********************************************************************
 * Selection
 *********************************************************************/

void FlowConsole_Test::select_widget()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int wid = m_fc->addWidget(sid, "button", 1);

    m_fc->selectWidget(wid);
    QCOMPARE(m_fc->selectedWidgetId(), wid);
}

void FlowConsole_Test::select_widgetAutoSelectsSection()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int wid = m_fc->addWidget(sid, "button", 1);

    m_fc->selectWidget(wid);
    QCOMPARE(m_fc->selectedSectionId(), sid);
}

void FlowConsole_Test::select_section()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    m_fc->selectSection(sid);
    QCOMPARE(m_fc->selectedSectionId(), sid);
    QCOMPARE(m_fc->selectedWidgetId(), -1); // widget deselected
}

void FlowConsole_Test::select_deselectAll()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int wid = m_fc->addWidget(sid, "button", 1);
    m_fc->selectWidget(wid);

    m_fc->deselectAll();
    QCOMPARE(m_fc->selectedWidgetId(), -1);
    QCOMPARE(m_fc->selectedSectionId(), -1);
}

void FlowConsole_Test::select_widgetInfo()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int wid = m_fc->addWidget(sid, "button", 2);

    m_fc->selectWidget(wid);
    QVariantMap info = m_fc->selectedWidgetInfo();
    QCOMPARE(info["id"].toUInt(), (quint32)wid);
    QCOMPARE(info["colSpan"].toInt(), 2);
    QCOMPARE(info["sectionId"].toUInt(), (quint32)sid);
    QCOMPARE(info["maxColumns"].toInt(), 4);
}

void FlowConsole_Test::select_sectionInfo()
{
    int sid = m_fc->addSection(0, "TestSection", "half", 6, true);
    m_fc->selectSection(sid);
    QVariantMap info = m_fc->selectedSectionInfo();
    QCOMPARE(info["caption"].toString(), QString("TestSection"));
    QCOMPARE(info["sizePreset"].toString(), QString("half"));
    QCOMPARE(info["columns"].toInt(), 6);
    QCOMPARE(info["isSolo"].toBool(), true);
}

/*********************************************************************
 * Save/Load XML round-trip
 *********************************************************************/

static QByteArray saveToXml(const FlowConsole *fc)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    const_cast<FlowConsole*>(fc)->saveXML(&writer);
    writer.writeEndDocument();
    buffer.close();
    return data;
}

static bool loadFromXml(FlowConsole *fc, const QByteArray &data)
{
    QXmlStreamReader reader(data);
    reader.readNextStartElement(); // advance to <FlowConsole>
    return fc->loadXML(reader);
}

void FlowConsole_Test::xml_saveEmptyConsole()
{
    QByteArray xml = saveToXml(m_fc);
    QVERIFY(xml.contains("FlowConsole"));
    QVERIFY(xml.contains("FlowPage"));
}

void FlowConsole_Test::xml_saveAndLoadSections()
{
    m_fc->addSection(0, "Dimmers", "full", 6, false);
    m_fc->addSection(0, "Effects", "half", 3, true);

    QByteArray xml = saveToXml(m_fc);
    m_fc->resetContents();
    QCOMPARE(m_fc->sectionsForPage(0).count(), 0);

    QVERIFY(loadFromXml(m_fc, xml));
    QVariantList sections = m_fc->sectionsForPage(0);
    QCOMPARE(sections.count(), 2);
    QCOMPARE(sections[0].toMap()["caption"].toString(), QString("Dimmers"));
    QCOMPARE(sections[0].toMap()["sizePreset"].toString(), QString("full"));
    QCOMPARE(sections[0].toMap()["columns"].toInt(), 6);
    QCOMPARE(sections[1].toMap()["caption"].toString(), QString("Effects"));
    QCOMPARE(sections[1].toMap()["isSolo"].toBool(), true);
}

void FlowConsole_Test::xml_saveAndLoadWidgets()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    int w1 = m_fc->addWidget(sid, "button", 1);
    int w2 = m_fc->addWidget(sid, "slider", 2);
    Q_UNUSED(w1); Q_UNUSED(w2);

    QByteArray xml = saveToXml(m_fc);
    m_fc->resetContents();

    QVERIFY(loadFromXml(m_fc, xml));

    QVariantList sections = m_fc->sectionsForPage(0);
    QCOMPARE(sections.count(), 1);

    int loadedSid = sections[0].toMap()["id"].toInt();
    QVariantList widgets = m_fc->widgetsForSection(loadedSid);
    QCOMPARE(widgets.count(), 2);

    // First widget should be a button (type 1)
    QCOMPARE(widgets[0].toMap()["type"].toInt(), (int)VCWidget::ButtonWidget);
    QCOMPARE(widgets[0].toMap()["colSpan"].toInt(), 1);

    // Second widget should be a slider (type 2)
    QCOMPARE(widgets[1].toMap()["type"].toInt(), (int)VCWidget::SliderWidget);
    QCOMPARE(widgets[1].toMap()["colSpan"].toInt(), 2);
}

void FlowConsole_Test::xml_roundTripPreservesOrder()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    m_fc->addWidget(sid, "button", 1);  // idx 0
    m_fc->addWidget(sid, "slider", 1);  // idx 1
    m_fc->addWidget(sid, "label", 1);   // idx 2
    m_fc->addWidget(sid, "clock", 1);   // idx 3

    QByteArray xml = saveToXml(m_fc);
    m_fc->resetContents();
    QVERIFY(loadFromXml(m_fc, xml));

    int loadedSid = m_fc->sectionsForPage(0)[0].toMap()["id"].toInt();
    QVariantList widgets = m_fc->widgetsForSection(loadedSid);
    QCOMPARE(widgets.count(), 4);

    // Verify type order preserved
    QCOMPARE(widgets[0].toMap()["type"].toInt(), (int)VCWidget::ButtonWidget);
    QCOMPARE(widgets[1].toMap()["type"].toInt(), (int)VCWidget::SliderWidget);
    QCOMPARE(widgets[2].toMap()["type"].toInt(), (int)VCWidget::LabelWidget);
    QCOMPARE(widgets[3].toMap()["type"].toInt(), (int)VCWidget::ClockWidget);
}

void FlowConsole_Test::xml_roundTripPreservesColSpan()
{
    int sid = m_fc->addSection(0, "S", "full", 6, false);
    m_fc->addWidget(sid, "button", 1);
    m_fc->addWidget(sid, "slider", 3);
    m_fc->addWidget(sid, "label", 2);

    QByteArray xml = saveToXml(m_fc);
    m_fc->resetContents();
    QVERIFY(loadFromXml(m_fc, xml));

    int loadedSid = m_fc->sectionsForPage(0)[0].toMap()["id"].toInt();
    QVariantList widgets = m_fc->widgetsForSection(loadedSid);
    QCOMPARE(widgets[0].toMap()["colSpan"].toInt(), 1);
    QCOMPARE(widgets[1].toMap()["colSpan"].toInt(), 3);
    QCOMPARE(widgets[2].toMap()["colSpan"].toInt(), 2);
}

void FlowConsole_Test::xml_roundTripMultiplePages()
{
    m_fc->addSection(0, "Page1Section", "full", 4, false);
    m_fc->addPage("Page 2");
    m_fc->addSection(1, "Page2Section", "half", 3, true);

    QByteArray xml = saveToXml(m_fc);
    m_fc->resetContents();
    QVERIFY(loadFromXml(m_fc, xml));

    QCOMPARE(m_fc->pagesCount(), 2);
    QCOMPARE(m_fc->sectionsForPage(0).count(), 1);
    QCOMPARE(m_fc->sectionsForPage(0)[0].toMap()["caption"].toString(), QString("Page1Section"));
    QCOMPARE(m_fc->sectionsForPage(1).count(), 1);
    QCOMPARE(m_fc->sectionsForPage(1)[0].toMap()["caption"].toString(), QString("Page2Section"));
}

void FlowConsole_Test::xml_loadEmptyCreatesDefaultPage()
{
    // Save with no pages (manually clear)
    QByteArray xml = "<?xml version=\"1.0\"?><FlowConsole></FlowConsole>";
    QVERIFY(loadFromXml(m_fc, xml));
    QCOMPARE(m_fc->pagesCount(), 1); // default page created
}

/*********************************************************************
 * Reset
 *********************************************************************/

void FlowConsole_Test::reset_clearsEverything()
{
    int sid = m_fc->addSection(0, "S", "full", 4, false);
    m_fc->addWidget(sid, "button", 1);
    m_fc->addWidget(sid, "slider", 2);
    m_fc->addPage("Page 2");
    m_fc->setEditMode(true);

    m_fc->resetContents();

    QCOMPARE(m_fc->pagesCount(), 1);
    QCOMPARE(m_fc->selectedPage(), 0);
    QCOMPARE(m_fc->editMode(), false);
    QCOMPARE(m_fc->sectionsForPage(0).count(), 0);
    QCOMPARE(m_fc->selectedWidgetId(), -1);
    QCOMPARE(m_fc->selectedSectionId(), -1);
}

QTEST_MAIN(FlowConsole_Test)
