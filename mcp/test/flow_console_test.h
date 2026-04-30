/*
  Q Light Controller Plus - Unit test
  flow_console_test.h

  Copyright (C) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef FLOW_CONSOLE_TEST_H
#define FLOW_CONSOLE_TEST_H

#include <QObject>

class Doc;
class FlowConsole;

class FlowConsole_Test final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    // Pages
    void pages_defaultHasOnePage();
    void pages_addPage();
    void pages_selectPage();
    void pages_selectPageOutOfRange();

    // Sections — CRUD
    void section_addToPage();
    void section_addMultiple();
    void section_addToInvalidPage();
    void section_remove();
    void section_removeNonExistent();
    void section_setCaption();
    void section_setSizePreset();
    void section_setSizePresetInvalid();
    void section_setColumns();
    void section_setColumnsClamped();
    void section_setCollapsed();

    // Widgets — CRUD
    void widget_addButton();
    void widget_addSlider();
    void widget_addAllTypes_data();
    void widget_addAllTypes();
    void widget_addToInvalidSection();
    void widget_addUnknownType();
    void widget_remove();
    void widget_removeNonExistent();
    void widget_colSpan();
    void widget_colSpanClamped();

    // Reorder
    void reorder_moveUp();
    void reorder_moveDown();
    void reorder_moveUpAtTop();
    void reorder_moveDownAtBottom();
    void reorder_byIndex();

    // Selection
    void select_widget();
    void select_widgetAutoSelectsSection();
    void select_section();
    void select_deselectAll();
    void select_widgetInfo();
    void select_sectionInfo();

    // Save/Load XML round-trip
    void xml_saveEmptyConsole();
    void xml_saveAndLoadSections();
    void xml_saveAndLoadWidgets();
    void xml_roundTripPreservesOrder();
    void xml_roundTripPreservesColSpan();
    void xml_roundTripMultiplePages();
    void xml_loadEmptyCreatesDefaultPage();

    // Reset
    void reset_clearsEverything();

private:
    Doc *m_doc;
    FlowConsole *m_fc;
};

#endif // FLOW_CONSOLE_TEST_H
