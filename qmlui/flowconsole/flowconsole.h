/*
  Q Light Controller Plus
  flowconsole.h

  Copyright (c) Massimo Callegari

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

#ifndef FLOWCONSOLE_H
#define FLOWCONSOLE_H

#include <QHash>

#include "previewcontext.h"
#include "flowlayoutdata.h"

class Doc;
class VCWidget;
class QXmlStreamReader;
class QXmlStreamWriter;

class FlowConsole : public PreviewContext
{
    Q_OBJECT

    Q_PROPERTY(bool editMode READ editMode WRITE setEditMode NOTIFY editModeChanged)
    Q_PROPERTY(int pagesCount READ pagesCount NOTIFY pagesCountChanged)
    Q_PROPERTY(int selectedPage READ selectedPage WRITE setSelectedPage NOTIFY selectedPageChanged)
    Q_PROPERTY(int selectedWidgetId READ selectedWidgetId WRITE setSelectedWidgetId NOTIFY selectedWidgetChanged)
    Q_PROPERTY(int selectedSectionId READ selectedSectionId WRITE setSelectedSectionId NOTIFY selectedSectionChanged)
    Q_PROPERTY(QVariantMap selectedWidgetInfo READ selectedWidgetInfo NOTIFY selectedWidgetChanged)
    Q_PROPERTY(QVariantMap selectedSectionInfo READ selectedSectionInfo NOTIFY selectedSectionChanged)

public:
    explicit FlowConsole(QQuickView *view, Doc *doc, QObject *parent = nullptr);
    ~FlowConsole();

    bool editMode() const;
    void setEditMode(bool editMode);

    /*********************************************************************
     * Pages
     *********************************************************************/

    int pagesCount() const;
    int selectedPage() const;
    void setSelectedPage(int page);
    Q_INVOKABLE int addPage(const QString &name);

    /*********************************************************************
     * Sections
     *********************************************************************/

    Q_INVOKABLE int addSection(int pageIndex, const QString &caption,
                               const QString &sizePreset, int columns, bool solo);
    Q_INVOKABLE bool removeSection(int sectionId);
    Q_INVOKABLE QVariantList sectionsForPage(int pageIndex) const;

    Q_INVOKABLE bool setSectionCaption(int sectionId, const QString &caption);
    Q_INVOKABLE bool setSectionSizePreset(int sectionId, const QString &preset);
    Q_INVOKABLE bool setSectionColumns(int sectionId, int columns);
    Q_INVOKABLE bool setSectionCollapsed(int sectionId, bool collapsed);

    /*********************************************************************
     * Widgets
     *********************************************************************/

    Q_INVOKABLE int addWidget(int sectionId, const QString &type, int colSpan);
    Q_INVOKABLE bool removeWidget(quint32 widgetId);
    Q_INVOKABLE bool reorderWidget(quint32 widgetId, int newIndex);
    Q_INVOKABLE bool moveWidgetUp(quint32 widgetId);
    Q_INVOKABLE bool moveWidgetDown(quint32 widgetId);
    Q_INVOKABLE bool setWidgetColSpan(quint32 widgetId, int colSpan);
    Q_INVOKABLE QVariantList widgetsForSection(int sectionId) const;
    Q_INVOKABLE QObject *widget(quint32 id) const;

    /** Set widget caption */
    Q_INVOKABLE bool setWidgetCaption(quint32 widgetId, const QString &caption);

    /** Get all pages as a QVariantList for MCP */
    Q_INVOKABLE QVariantList pages() const;

    /*********************************************************************
     * Selection
     *********************************************************************/

    int selectedWidgetId() const;
    void setSelectedWidgetId(int widgetId);
    Q_INVOKABLE void selectWidget(int widgetId);
    Q_INVOKABLE void deselectAll();

    int selectedSectionId() const;
    void setSelectedSectionId(int sectionId);
    Q_INVOKABLE void selectSection(int sectionId);

    QVariantMap selectedWidgetInfo() const;
    QVariantMap selectedSectionInfo() const;

    /** Get the widget's current colSpan */
    Q_INVOKABLE int widgetColSpan(quint32 widgetId) const;

    /** Get the section ID that contains a widget */
    Q_INVOKABLE int sectionForWidget(quint32 widgetId) const;

    /** Reset contents when a new document is loaded */
    void resetContents();

    /*********************************************************************
     * Save / Load
     *********************************************************************/

    bool saveXML(QXmlStreamWriter *doc) const;
    bool loadXML(QXmlStreamReader &root);

signals:
    void editModeChanged(bool editMode);
    void pagesCountChanged();
    void selectedPageChanged();
    void sectionsChanged();
    void selectedWidgetChanged();
    void selectedSectionChanged();

private:
    FlowSection *findSection(int sectionId);
    FlowSection *findSectionForWidget(quint32 widgetId);
    int widgetIndexInSection(const FlowSection *section, quint32 widgetId) const;
    quint32 newWidgetId();
    VCWidget *createWidget(const QString &type);

    bool m_editMode;
    int m_selectedPage;
    int m_selectedWidgetId;
    int m_selectedSectionId;
    QList<FlowPage> m_pages;
    quint32 m_nextSectionId;
    quint32 m_nextWidgetId;
    QHash<quint32, VCWidget*> m_widgetsMap;
};

#endif // FLOWCONSOLE_H
