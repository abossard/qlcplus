/*
  Q Light Controller Plus
  flowconsole.cpp

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

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QQmlEngine>
#include <QQmlContext>
#include <QDebug>

#define KXMLQLCFlowConsole       QStringLiteral("FlowConsole")
#define KXMLQLCFlowPage          QStringLiteral("FlowPage")
#define KXMLQLCFlowPageName      QStringLiteral("Name")
#define KXMLQLCFlowSection       QStringLiteral("FlowSection")
#define KXMLQLCFlowSectionID     QStringLiteral("ID")
#define KXMLQLCFlowCaption       QStringLiteral("Caption")
#define KXMLQLCFlowSizePreset    QStringLiteral("SizePreset")
#define KXMLQLCFlowColumns       QStringLiteral("Columns")
#define KXMLQLCFlowSolo          QStringLiteral("Solo")
#define KXMLQLCFlowWidget        QStringLiteral("FlowWidget")
#define KXMLQLCFlowWidgetType    QStringLiteral("Type")
#define KXMLQLCFlowWidgetOrder   QStringLiteral("Order")
#define KXMLQLCFlowWidgetColSpan QStringLiteral("ColSpan")
#define KXMLQLCFlowCollapsed     QStringLiteral("Collapsed")
#define KXMLQLCFlowWidgetID      QStringLiteral("WidgetID")

#include "flowconsole.h"
#include "mastertimer.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vclabel.h"
#include "vcclock.h"
#include "vccuelist.h"
#include "vcxypad.h"
#include "vcspeeddial.h"
#include "vcanimation.h"
#include "vcaudiotriggers.h"

FlowConsole::FlowConsole(QQuickView *view, Doc *doc, QObject *parent)
    : PreviewContext(view, doc, "FC", parent)
    , m_editMode(false)
    , m_selectedPage(0)
    , m_selectedWidgetId(-1)
    , m_selectedSectionId(-1)
    , m_nextSectionId(1)
    , m_nextWidgetId(50000)
{
    setContextResource("qrc:/FlowConsole.qml");
    setContextTitle(tr("Flow Console"));

    if (view)
        view->rootContext()->setContextProperty("flowConsole", this);

    FlowPage defaultPage;
    defaultPage.name = tr("Page 1");
    m_pages.append(defaultPage);
}

FlowConsole::~FlowConsole()
{
    qDeleteAll(m_widgetsMap);
}

bool FlowConsole::editMode() const
{
    return m_editMode;
}

void FlowConsole::setEditMode(bool editMode)
{
    if (m_editMode == editMode)
        return;

    // Don't allow entering edit mode while functions are running
    if (editMode && m_doc && m_doc->masterTimer()
        && m_doc->masterTimer()->runningFunctions() > 0)
    {
        qDebug() << "[FlowConsole] cannot enter edit mode while functions are running";
        return;
    }

    m_editMode = editMode;
    emit editModeChanged(m_editMode);
}

/*********************************************************************
 * Pages
 *********************************************************************/

int FlowConsole::pagesCount() const
{
    return m_pages.count();
}

int FlowConsole::selectedPage() const
{
    return m_selectedPage;
}

void FlowConsole::setSelectedPage(int page)
{
    if (page < 0 || page >= m_pages.count() || page == m_selectedPage)
        return;

    m_selectedPage = page;
    emit selectedPageChanged();
    emit sectionsChanged();
}

int FlowConsole::addPage(const QString &name)
{
    FlowPage page;
    page.name = name.isEmpty() ? tr("Page %1").arg(m_pages.count() + 1) : name;
    m_pages.append(page);
    emit pagesCountChanged();
    return m_pages.count() - 1;
}

/*********************************************************************
 * Sections
 *********************************************************************/

int FlowConsole::addSection(int pageIndex, const QString &caption,
                            const QString &sizePreset, int columns, bool solo)
{
    if (pageIndex < 0 || pageIndex >= m_pages.count())
        return -1;

    FlowSection section;
    section.id = m_nextSectionId++;
    section.caption = caption;
    section.sizePreset = sizePreset;
    section.columns = qBound(1, columns, 12);
    section.isSolo = solo;

    m_pages[pageIndex].sections.append(section);
    emit sectionsChanged();

    return section.id;
}

bool FlowConsole::removeSection(int sectionId)
{
    for (auto &page : m_pages)
    {
        for (int i = 0; i < page.sections.count(); i++)
        {
            if (page.sections[i].id == (quint32)sectionId)
            {
                // Clear selection if it points to this section or its widgets
                if (m_selectedSectionId == sectionId)
                    deselectAll();

                // Delete widgets owned by this section
                for (auto &entry : page.sections[i].widgets)
                {
                    if (entry.widget)
                    {
                        if (m_selectedWidgetId == (int)entry.widget->id())
                            setSelectedWidgetId(-1);
                        m_widgetsMap.remove(entry.widget->id());
                        delete entry.widget;
                    }
                }
                page.sections.removeAt(i);
                emit sectionsChanged();
                return true;
            }
        }
    }
    return false;
}

QVariantList FlowConsole::sectionsForPage(int pageIndex) const
{
    QVariantList result;
    if (pageIndex < 0 || pageIndex >= m_pages.count())
        return result;

    const FlowPage &page = m_pages[pageIndex];
    for (const FlowSection &section : page.sections)
    {
        QVariantMap map;
        map["id"] = section.id;
        map["caption"] = section.caption;
        map["sizePreset"] = section.sizePreset;
        map["columns"] = section.columns;
        map["isSolo"] = section.isSolo;
        map["isCollapsed"] = section.isCollapsed;
        map["widgetCount"] = section.widgets.count();
        result.append(map);
    }

    return result;
}

bool FlowConsole::setSectionCaption(int sectionId, const QString &caption)
{
    FlowSection *s = findSection(sectionId);
    if (!s) return false;
    s->caption = caption;
    emit sectionsChanged();
    return true;
}

bool FlowConsole::setSectionSizePreset(int sectionId, const QString &preset)
{
    FlowSection *s = findSection(sectionId);
    if (!s) return false;
    if (preset != "full" && preset != "half" && preset != "third" && preset != "quarter")
        return false;
    s->sizePreset = preset;
    emit sectionsChanged();
    return true;
}

bool FlowConsole::setSectionColumns(int sectionId, int columns)
{
    FlowSection *s = findSection(sectionId);
    if (!s) return false;
    s->columns = qBound(1, columns, 12);
    emit sectionsChanged();
    return true;
}

bool FlowConsole::setSectionCollapsed(int sectionId, bool collapsed)
{
    FlowSection *s = findSection(sectionId);
    if (!s) return false;
    s->isCollapsed = collapsed;
    emit sectionsChanged();
    return true;
}

/*********************************************************************
 * Widgets
 *********************************************************************/

int FlowConsole::addWidget(int sectionId, const QString &type, int colSpan)
{
    FlowSection *section = findSection(sectionId);
    if (!section)
        return -1;

    VCWidget *widget = createWidget(type);
    if (!widget)
        return -1;

    FlowWidgetEntry entry;
    entry.widget = widget;
    entry.colSpan = qBound(1, colSpan, section->columns);

    section->widgets.append(entry);
    emit sectionsChanged();

    qDebug() << "[FlowConsole] added widget" << widget->id() << "type:" << type
             << "to section" << sectionId;

    return widget->id();
}

bool FlowConsole::removeWidget(quint32 widgetId)
{
    FlowSection *section = findSectionForWidget(widgetId);
    if (!section) return false;

    for (int i = 0; i < section->widgets.count(); i++)
    {
        if (section->widgets[i].widget && section->widgets[i].widget->id() == widgetId)
        {
            if (m_selectedWidgetId == (int)widgetId)
                setSelectedWidgetId(-1);

            VCWidget *w = section->widgets[i].widget;
            section->widgets.removeAt(i);
            m_widgetsMap.remove(widgetId);
            delete w;
            emit sectionsChanged();
            return true;
        }
    }
    return false;
}

bool FlowConsole::reorderWidget(quint32 widgetId, int newIndex)
{
    FlowSection *section = findSectionForWidget(widgetId);
    if (!section) return false;

    int oldIndex = -1;
    for (int i = 0; i < section->widgets.count(); i++)
    {
        if (section->widgets[i].widget && section->widgets[i].widget->id() == widgetId)
        {
            oldIndex = i;
            break;
        }
    }

    if (oldIndex < 0) return false;
    newIndex = qBound(0, newIndex, section->widgets.count() - 1);
    if (oldIndex == newIndex) return true;

    section->widgets.move(oldIndex, newIndex);
    emit sectionsChanged();
    return true;
}

bool FlowConsole::setWidgetColSpan(quint32 widgetId, int colSpan)
{
    for (auto &page : m_pages)
    {
        for (auto &section : page.sections)
        {
            for (auto &entry : section.widgets)
            {
                if (entry.widget && entry.widget->id() == widgetId)
                {
                    entry.colSpan = qBound(1, colSpan, section.columns);
                    emit sectionsChanged();
                    return true;
                }
            }
        }
    }
    return false;
}

QVariantList FlowConsole::widgetsForSection(int sectionId) const
{
    QVariantList result;

    for (const auto &page : m_pages)
    {
        for (const auto &section : page.sections)
        {
            if (section.id == (quint32)sectionId)
            {
                for (const auto &entry : section.widgets)
                {
                    if (!entry.widget) continue;
                    QVariantMap map;
                    map["id"] = entry.widget->id();
                    map["type"] = entry.widget->type();
                    map["colSpan"] = entry.colSpan;
                    result.append(map);
                }
                return result;
            }
        }
    }
    return result;
}

QObject *FlowConsole::widget(quint32 id) const
{
    return m_widgetsMap.value(id, nullptr);
}

bool FlowConsole::setWidgetCaption(quint32 widgetId, const QString &caption)
{
    VCWidget *w = m_widgetsMap.value(widgetId, nullptr);
    if (!w) return false;
    w->setCaption(caption);
    return true;
}

QVariantList FlowConsole::pages() const
{
    QVariantList result;
    for (int i = 0; i < m_pages.count(); i++)
    {
        QVariantMap map;
        map["index"] = i;
        map["name"] = m_pages[i].name;
        map["sectionCount"] = m_pages[i].sections.count();
        result.append(map);
    }
    return result;
}

bool FlowConsole::moveWidgetUp(quint32 widgetId)
{
    FlowSection *section = findSectionForWidget(widgetId);
    if (!section) return false;
    int idx = widgetIndexInSection(section, widgetId);
    if (idx <= 0) return false;
    section->widgets.move(idx, idx - 1);
    emit sectionsChanged();
    return true;
}

bool FlowConsole::moveWidgetDown(quint32 widgetId)
{
    FlowSection *section = findSectionForWidget(widgetId);
    if (!section) return false;
    int idx = widgetIndexInSection(section, widgetId);
    if (idx < 0 || idx >= section->widgets.count() - 1) return false;
    section->widgets.move(idx, idx + 1);
    emit sectionsChanged();
    return true;
}

/*********************************************************************
 * Selection
 *********************************************************************/

int FlowConsole::selectedWidgetId() const
{
    return m_selectedWidgetId;
}

void FlowConsole::setSelectedWidgetId(int widgetId)
{
    if (m_selectedWidgetId == widgetId)
        return;
    m_selectedWidgetId = widgetId;
    emit selectedWidgetChanged();
}

void FlowConsole::selectWidget(int widgetId)
{
    setSelectedWidgetId(widgetId);
    // Auto-select the section containing this widget
    if (widgetId >= 0)
    {
        FlowSection *s = findSectionForWidget(widgetId);
        if (s)
            setSelectedSectionId(s->id);
    }
}

void FlowConsole::deselectAll()
{
    setSelectedWidgetId(-1);
    setSelectedSectionId(-1);
}

int FlowConsole::selectedSectionId() const
{
    return m_selectedSectionId;
}

void FlowConsole::setSelectedSectionId(int sectionId)
{
    if (m_selectedSectionId == sectionId)
        return;
    m_selectedSectionId = sectionId;
    emit selectedSectionChanged();
}

void FlowConsole::selectSection(int sectionId)
{
    setSelectedSectionId(sectionId);
    setSelectedWidgetId(-1);
}

QVariantMap FlowConsole::selectedWidgetInfo() const
{
    QVariantMap info;
    if (m_selectedWidgetId < 0) return info;

    VCWidget *w = m_widgetsMap.value(m_selectedWidgetId, nullptr);
    if (!w) return info;

    info["id"] = w->id();
    info["type"] = w->type();
    info["typeName"] = VCWidget::typeToString(w->type());
    info["caption"] = w->caption();

    // Find colSpan
    for (const auto &page : m_pages)
    {
        for (const auto &section : page.sections)
        {
            for (const auto &entry : section.widgets)
            {
                if (entry.widget && entry.widget->id() == (quint32)m_selectedWidgetId)
                {
                    info["colSpan"] = entry.colSpan;
                    info["sectionId"] = section.id;
                    info["maxColumns"] = section.columns;
                    return info;
                }
            }
        }
    }
    return info;
}

QVariantMap FlowConsole::selectedSectionInfo() const
{
    QVariantMap info;
    if (m_selectedSectionId < 0) return info;

    for (const auto &page : m_pages)
    {
        for (const auto &section : page.sections)
        {
            if (section.id == (quint32)m_selectedSectionId)
            {
                info["id"] = section.id;
                info["caption"] = section.caption;
                info["sizePreset"] = section.sizePreset;
                info["columns"] = section.columns;
                info["isSolo"] = section.isSolo;
                info["widgetCount"] = section.widgets.count();
                return info;
            }
        }
    }
    return info;
}

int FlowConsole::widgetColSpan(quint32 widgetId) const
{
    for (const auto &page : m_pages)
    {
        for (const auto &section : page.sections)
        {
            for (const auto &entry : section.widgets)
            {
                if (entry.widget && entry.widget->id() == widgetId)
                    return entry.colSpan;
            }
        }
    }
    return 1;
}

int FlowConsole::sectionForWidget(quint32 widgetId) const
{
    for (const auto &page : m_pages)
    {
        for (const auto &section : page.sections)
        {
            for (const auto &entry : section.widgets)
            {
                if (entry.widget && entry.widget->id() == widgetId)
                    return section.id;
            }
        }
    }
    return -1;
}

void FlowConsole::resetContents()
{
    m_editMode = false;
    m_selectedPage = 0;
    m_selectedWidgetId = -1;
    m_selectedSectionId = -1;
    m_nextSectionId = 1;
    m_nextWidgetId = 50000;

    // Delete all widgets
    qDeleteAll(m_widgetsMap);
    m_widgetsMap.clear();

    m_pages.clear();
    FlowPage defaultPage;
    defaultPage.name = tr("Page 1");
    m_pages.append(defaultPage);

    emit editModeChanged(m_editMode);
    emit pagesCountChanged();
    emit selectedPageChanged();
    emit sectionsChanged();
}

/*********************************************************************
 * Save / Load
 *********************************************************************/

bool FlowConsole::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);

    doc->writeStartElement(KXMLQLCFlowConsole);

    for (const auto &page : m_pages)
    {
        doc->writeStartElement(KXMLQLCFlowPage);
        doc->writeAttribute(KXMLQLCFlowPageName, page.name);

        for (const auto &section : page.sections)
        {
            doc->writeStartElement(KXMLQLCFlowSection);
            doc->writeAttribute(KXMLQLCFlowSectionID, QString::number(section.id));
            doc->writeAttribute(KXMLQLCFlowCaption, section.caption);
            doc->writeAttribute(KXMLQLCFlowSizePreset, section.sizePreset);
            doc->writeAttribute(KXMLQLCFlowColumns, QString::number(section.columns));
            if (section.isSolo)
                doc->writeAttribute(KXMLQLCFlowSolo, QStringLiteral("True"));
            if (section.isCollapsed)
                doc->writeAttribute(KXMLQLCFlowCollapsed, QStringLiteral("True"));

            for (int i = 0; i < section.widgets.count(); i++)
            {
                const FlowWidgetEntry &entry = section.widgets[i];
                if (!entry.widget) continue;

                doc->writeStartElement(KXMLQLCFlowWidget);
                doc->writeAttribute(KXMLQLCFlowWidgetType,
                    VCWidget::typeToString(entry.widget->type()));
                doc->writeAttribute(KXMLQLCFlowWidgetOrder, QString::number(i));
                doc->writeAttribute(KXMLQLCFlowWidgetColSpan, QString::number(entry.colSpan));
                doc->writeAttribute(KXMLQLCFlowWidgetID, QString::number(entry.widget->id()));

                // Save the widget's own properties (caption, colors, function, etc.)
                entry.widget->saveXML(doc);

                doc->writeEndElement(); // FlowWidget
            }

            doc->writeEndElement(); // FlowSection
        }

        doc->writeEndElement(); // FlowPage
    }

    doc->writeEndElement(); // FlowConsole
    return true;
}

bool FlowConsole::loadXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCFlowConsole)
    {
        qWarning() << Q_FUNC_INFO << "FlowConsole node not found";
        return false;
    }

    // Clear existing data
    qDeleteAll(m_widgetsMap);
    m_widgetsMap.clear();
    m_pages.clear();
    m_nextSectionId = 1;

    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCFlowPage)
        {
            FlowPage page;
            page.name = root.attributes().value(KXMLQLCFlowPageName).toString();

            while (root.readNextStartElement())
            {
                if (root.name() == KXMLQLCFlowSection)
                {
                    FlowSection section;
                    QXmlStreamAttributes attrs = root.attributes();
                    section.id = attrs.value(KXMLQLCFlowSectionID).toUInt();
                    section.caption = attrs.value(KXMLQLCFlowCaption).toString();
                    section.sizePreset = attrs.value(KXMLQLCFlowSizePreset).toString();
                    section.columns = attrs.value(KXMLQLCFlowColumns).toInt();
                    section.isSolo = attrs.value(KXMLQLCFlowSolo).toString() == "True";
                    section.isCollapsed = attrs.value(KXMLQLCFlowCollapsed).toString() == "True";

                    if (section.id >= m_nextSectionId)
                        m_nextSectionId = section.id + 1;

                    while (root.readNextStartElement())
                    {
                        if (root.name() == KXMLQLCFlowWidget)
                        {
                            QXmlStreamAttributes wAttrs = root.attributes();
                            QString typeStr = wAttrs.value(KXMLQLCFlowWidgetType).toString();
                            int colSpan = wAttrs.value(KXMLQLCFlowWidgetColSpan).toInt();
                            quint32 savedWidgetId = wAttrs.value(KXMLQLCFlowWidgetID).toUInt();

                            // Map type string to our create type string
                            QString createType;
                            if (typeStr == "Button") createType = "button";
                            else if (typeStr == "Slider") createType = "slider";
                            else if (typeStr == "Label") createType = "label";
                            else if (typeStr == "Clock") createType = "clock";
                            else if (typeStr == "Cue list") createType = "cuelist";
                            else if (typeStr == "XY Pad") createType = "xypad";
                            else if (typeStr == "Speed dial") createType = "speedDial";
                            else if (typeStr == "Animation") createType = "matrix";
                            else if (typeStr == "Audio Triggers") createType = "audioTrigger";

                            VCWidget *widget = createWidget(createType);
                            if (widget)
                            {
                                // Restore saved widget ID if available
                                if (savedWidgetId > 0)
                                {
                                    m_widgetsMap.remove(widget->id());
                                    widget->setID(savedWidgetId);
                                    m_widgetsMap.insert(savedWidgetId, widget);
                                    if (savedWidgetId >= m_nextWidgetId)
                                        m_nextWidgetId = savedWidgetId + 1;
                                }

                                // Load the widget's own XML (nested inside FlowWidget)
                                if (root.readNextStartElement())
                                {
                                    widget->loadXML(root);
                                }

                                FlowWidgetEntry entry;
                                entry.widget = widget;
                                entry.colSpan = qBound(1, colSpan, section.columns);
                                section.widgets.append(entry);
                            }

                            // Skip any remaining content in FlowWidget
                            while (!root.isEndElement() || root.name() != KXMLQLCFlowWidget)
                                root.readNext();
                        }
                        else
                        {
                            root.skipCurrentElement();
                        }
                    }

                    page.sections.append(section);
                }
                else
                {
                    root.skipCurrentElement();
                }
            }

            m_pages.append(page);
        }
        else
        {
            root.skipCurrentElement();
        }
    }

    // Ensure at least one page
    if (m_pages.isEmpty())
    {
        FlowPage defaultPage;
        defaultPage.name = tr("Page 1");
        m_pages.append(defaultPage);
    }

    m_selectedPage = 0;
    emit pagesCountChanged();
    emit selectedPageChanged();
    emit sectionsChanged();

    return true;
}

/*********************************************************************
 * Private helpers
 *********************************************************************/

FlowSection *FlowConsole::findSection(int sectionId)
{
    for (auto &page : m_pages)
    {
        for (auto &section : page.sections)
        {
            if (section.id == (quint32)sectionId)
                return &section;
        }
    }
    return nullptr;
}

FlowSection *FlowConsole::findSectionForWidget(quint32 widgetId)
{
    for (auto &page : m_pages)
    {
        for (auto &section : page.sections)
        {
            for (const auto &entry : section.widgets)
            {
                if (entry.widget && entry.widget->id() == widgetId)
                    return &section;
            }
        }
    }
    return nullptr;
}

int FlowConsole::widgetIndexInSection(const FlowSection *section, quint32 widgetId) const
{
    for (int i = 0; i < section->widgets.count(); i++)
    {
        if (section->widgets[i].widget && section->widgets[i].widget->id() == widgetId)
            return i;
    }
    return -1;
}

quint32 FlowConsole::newWidgetId()
{
    return m_nextWidgetId++;
}

VCWidget *FlowConsole::createWidget(const QString &type)
{
    VCWidget *widget = nullptr;

    if (type == "button")
    {
        VCButton *button = new VCButton(m_doc, this);
        widget = button;
    }
    else if (type == "slider")
    {
        VCSlider *slider = new VCSlider(m_doc, this);
        widget = slider;
    }
    else if (type == "label")
    {
        VCLabel *label = new VCLabel(m_doc, this);
        widget = label;
    }
    else if (type == "clock")
    {
        VCClock *clock = new VCClock(m_doc, this);
        widget = clock;
    }
    else if (type == "cuelist")
    {
        VCCueList *cuelist = new VCCueList(m_doc, this);
        widget = cuelist;
    }
    else if (type == "xypad")
    {
        VCXYPad *xypad = new VCXYPad(m_doc, this);
        widget = xypad;
    }
    else if (type == "speedDial")
    {
        VCSpeedDial *speedDial = new VCSpeedDial(m_doc, this);
        widget = speedDial;
    }
    else if (type == "matrix")
    {
        VCAnimation *animation = new VCAnimation(m_doc, this);
        widget = animation;
    }
    else if (type == "audioTrigger")
    {
        VCAudioTriggers *audioTriggers = new VCAudioTriggers(m_doc, nullptr, this);
        widget = audioTriggers;
    }
    else
    {
        qWarning() << "[FlowConsole] unknown widget type:" << type;
        return nullptr;
    }

    QQmlEngine::setObjectOwnership(widget, QQmlEngine::CppOwnership);
    quint32 id = newWidgetId();
    widget->setID(id);
    m_widgetsMap.insert(id, widget);

    return widget;
}
