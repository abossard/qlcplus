/*
  Q Light Controller Plus
  virtualconsole.cpp

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

#include <QtCore/qbuffer.h>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSet>
#include <QSignalBlocker>
#include <QScopedValueRollback>
#include <memory>
#include <vector>
#include <cmath>
#include <QPointer>
#include <QTimer>
#include <algorithm>

#include "virtualconsole.h"
#include "contextmanager.h"
#include "qlcinputchannel.h"
#include "vcrecordpanel.h"
#include "vcaudiotriggers.h"
#include "audiosparklineitem.h"
#include "vcanimation.h"
#include "vcspeeddial.h"
#include "inputpatch.h"
#include "treemodel.h"
#include "vccuelist.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vcframe.h"
#include "vclabel.h"
#include "vclayoutadapter.h"
#include "gridlayout.h"
#include "vcxypad.h"
#include "vcclock.h"
#include "vcpage.h"
#include "tardis.h"
#include "vceditor.h"
#include "doc.h"
#include "app.h"

namespace
{
bool finiteEdges(const QRectF &rect)
{
    return qIsFinite(rect.left()) && qIsFinite(rect.top()) &&
           qIsFinite(rect.right()) && qIsFinite(rect.bottom());
}

/** Provenance of a mapped external event, taken from the plugin actually
 *  patched to its input universe. Only a MIDI or OSC control surface is a
 *  person operating something: ArtNet, E1.31, loopback and audio-driven feeds
 *  reach the very same dispatch and must not be mistaken for user input. */
ShowCommandOrigin inputOriginForUniverse(Doc *doc, quint32 universe)
{
    if (doc == nullptr)
        return ShowCommandOrigin::Programmatic;

    InputPatch *patch = doc->inputOutputMap()->inputPatch(universe);
    if (patch != nullptr && patch->pluginName() == QLatin1String("MIDI"))
        return ShowCommandOrigin::Midi;
    if (patch != nullptr && patch->pluginName() == QLatin1String("OSC"))
        return ShowCommandOrigin::Osc;

    return ShowCommandOrigin::Programmatic;
}
}

#define KXMLQLCVCProperties             QStringLiteral("Properties")
#define KXMLQLCVCPropertiesSize         QStringLiteral("Size")
#define KXMLQLCVCPropertiesSizeWidth    QStringLiteral("Width")
#define KXMLQLCVCPropertiesSizeHeight   QStringLiteral("Height")

#define KXMLQLCVCPropertiesGrandMaster              QStringLiteral("GrandMaster")
#define KXMLQLCVCPropertiesGrandMasterVisible       QStringLiteral("Visible")
#define KXMLQLCVCPropertiesGrandMasterChannelMode   QStringLiteral("ChannelMode")
#define KXMLQLCVCPropertiesGrandMasterValueMode     QStringLiteral("ValueMode")
#define KXMLQLCVCPropertiesGrandMasterSliderMode    QStringLiteral("SliderMode")

#define KXMLQLCVCPropertiesInput            QStringLiteral("Input")
#define KXMLQLCVCPropertiesInputUniverse    QStringLiteral("Universe")
#define KXMLQLCVCPropertiesInputChannel     QStringLiteral("Channel")

#define DEFAULT_VC_PAGES_NUMBER 4

VirtualConsole::VirtualConsole(QQuickView *view, Doc *doc,
                               ContextManager *ctxManager, QObject *parent)
    : PreviewContext(view, doc, "VC", parent)
    , m_editMode(false)
    , m_snapping(true)
    , m_snappingSize(0)
    , m_loadStatus(Cleared)
    , m_contextManager(ctxManager)
    , m_selectedPage(0)
    , m_latestWidgetId(0)
    , m_clipboardIsCut(false)
    , m_inputDetectionEnabled(false)
    , m_externalInputEnabled(true)
    , m_autoDetectionWidget(nullptr)
    , m_autoDetectionSource(nullptr)
    , m_inputChannelsTree(nullptr)
    , m_autoDetectionKey(QKeySequence())
    , m_autoDetectionKeyId(UINT_MAX)
{
    Q_ASSERT(doc != nullptr);

    setContextResource("qrc:/VirtualConsole.qml");
    setContextTitle(tr("Virtual Console"));

    for (int i = 0; i < DEFAULT_VC_PAGES_NUMBER; i++)
    {
        VCPage *page = new VCPage(view, m_doc, this, i, this);
        QQmlEngine::setObjectOwnership(page, QQmlEngine::CppOwnership);
        addWidgetToMap(page);
        m_contextManager->registerContext(page->previewContext());
        m_pages.append(page);
    }
    m_outlineModel = new VCOutlineModel(this, this);
    m_editor = new VCEditor(this);

    view->rootContext()->setContextProperty("virtualConsole", this);
    view->rootContext()->setContextProperty("vcEditor", m_editor);
    qmlRegisterUncreatableType<GrandMaster>("org.qlcplus.classes", 1, 0, "GrandMaster", "Can't create a GrandMaster!");
    qmlRegisterUncreatableType<QLCInputChannel>("org.qlcplus.classes", 1, 0, "QLCInputChannel", "Can't create a QLCInputChannel!");

    qmlRegisterType<VCWidget>("org.qlcplus.classes", 1, 0, "VCWidget");
    qmlRegisterType<VCFrame>("org.qlcplus.classes", 1, 0, "VCFrame");
    qmlRegisterType<VCPage>("org.qlcplus.classes", 1, 0, "VCPage");
    qmlRegisterType<VCButton>("org.qlcplus.classes", 1, 0, "VCButton");
    qmlRegisterType<VCLabel>("org.qlcplus.classes", 1, 0, "VCLabel");
    qmlRegisterType<VCSlider>("org.qlcplus.classes", 1, 0, "VCSlider");
    qmlRegisterType<VCAnimation>("org.qlcplus.classes", 1, 0, "VCAnimation");
    qmlRegisterType<VCAudioTriggers>("org.qlcplus.classes", 1, 0, "VCAudioTriggers");
    qmlRegisterType<AudioSparklineItem>("org.qlcplus.classes", 1, 0, "AudioSparkline");
    qmlRegisterType<VCXYPad>("org.qlcplus.classes", 1, 0, "VCXYPad");
    qmlRegisterType<VCSpeedDial>("org.qlcplus.classes", 1, 0, "VCSpeedDial");
    qmlRegisterType<VCClock>("org.qlcplus.classes", 1, 0, "VCClock");
    qmlRegisterType<VCClockSchedule>("org.qlcplus.classes", 1, 0, "VCClockSchedule");
    qmlRegisterType<VCCueList>("org.qlcplus.classes", 1, 0, "VCCueList");
    qmlRegisterType<VCRecordPanel>("org.qlcplus.classes", 1, 0, "VCRecordPanel");

    connect(m_doc->inputOutputMap(), SIGNAL(inputValueChanged(quint32,quint32,uchar,QString)),
            this, SLOT(slotInputValueChanged(quint32,quint32,uchar)));
}

qreal VirtualConsole::pixelDensity() const
{
    if (m_detached)
    {
        return PreviewContext::pixelDensity();
    }
    else
    {
        App *app = qobject_cast<App *>(m_view);
        if (app == nullptr)
            return 0;
        return app->pixelDensity();
    }

    return 0;
}

QString VirtualConsole::workspaceChangeError() const
{
    return m_authoringBusy
        ? tr("Cannot replace the workspace while a console edit is busy. Retry after the edit finishes.")
        : QString();
}

void VirtualConsole::resetContents()
{
    const QString error = workspaceChangeError();
    if (!error.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << error;
        emit editRejected(error);
        return;
    }
    cancelLayout();
    m_unlockedPagePINs.clear();
    int pageIndex = 0;
    resetWidgetSelection();
    setEditMode(false);

    foreach (VCPage *page, m_pages)
    {
        page->deleteChildren();
        page->deleteAllInputSources();
        page->deleteAllKeySequences();
        page->resetInputSourcesMap();
        page->resetProperties(pageIndex++);
    }

    m_widgetsMap.clear();
    m_latestWidgetId = 0;
    for (VCPage *page : m_pages)
        addWidgetToMap(page);
    m_editor->resetContents();
    m_loadStatus = Cleared;
    // the view follows, or it keeps showing a page edits no longer target
    if (m_selectedPage != 0)
    {
        m_selectedPage = 0;
        emit selectedPageChanged(0);
    }
}


bool VirtualConsole::editMode() const
{
    return m_editMode;
}

void VirtualConsole::setEditMode(bool editMode)
{
    if (m_editMode == editMode)
        return;

    if (editMode == false)
    {
        m_clipboardIDList.clear();
        m_clipboardIsCut = false;
        emit clipboardItemsCountChanged();

        resetWidgetSelection();
    }

    m_editMode = editMode;
    emit editModeChanged(editMode);
}

bool VirtualConsole::snapping() const
{
    return m_snapping;
}

void VirtualConsole::setSnapping(bool enable)
{
    if (m_snapping == enable)
        return;

    m_snapping = enable;
    emit snappingChanged(enable);
}

qreal VirtualConsole::snappingSize() const
{
    if (m_snappingSize > 0)
        return m_snappingSize;
    return qMax(pixelDensity() * 3, 5.0);
}

void VirtualConsole::setSnappingSize(qreal size)
{
    size = qMax(size, 5.0);
    if (qFuzzyCompare(m_snappingSize, size))
        return;
    m_snappingSize = size;
    emit snappingSizeChanged(size);
}

VirtualConsole::LoadStatus VirtualConsole::loadStatus() const
{
    return m_loadStatus;
}

QVariantList VirtualConsole::usageList(quint32 fid) const
{
    QVariantList list;

    QHash<quint32, VCWidget *>::const_iterator i = m_widgetsMap.constBegin();
    while (i != m_widgetsMap.constEnd())
    {
        VCWidget *widget = i.value();
        bool found = false;

        if (widget == nullptr)
        {
            ++i;
            continue;
        }
        switch (widget->type())
        {
            case VCWidget::ButtonWidget:
            {
                VCButton *button = qobject_cast<VCButton *>(widget);
                if (button->functionID() == fid)
                    found = true;
            }
            break;
            case VCWidget::SliderWidget:
            {
                VCSlider *slider = qobject_cast<VCSlider *>(widget);
                if (slider->controlledFunction() == fid)
                    found = true;
            }
            break;
            case VCWidget::CueListWidget:
            {
                VCCueList *cuelist = qobject_cast<VCCueList *>(widget);
                if (cuelist->chaserID() == fid)
                    found = true;
            }
            break;
            case VCWidget::ClockWidget:
            {
                VCClock *clock = qobject_cast<VCClock *>(widget);
                for (VCClockSchedule *schedule : clock->schedules())
                {
                    if (schedule->functionID() == fid)
                    {
                        found = true;
                        // a single match will do
                        break;
                    }
                }
            }
            break;
            default:
            break;
        }

        if (found)
        {
            QVariantMap wMap;
            wMap.insert("classRef", QVariant::fromValue(widget));
            wMap.insert("label", QString("%1").arg(widget->caption()));
            list.append(wMap);
        }

        ++i;
    }

    if (list.isEmpty())
    {
        QVariantMap noneMap;
        noneMap.insert("label", tr("<None>"));
        list.append(noneMap);
    }

    return list;
}

QVariantList VirtualConsole::widgetsList(QVariantList typeFilters, quint32 excludeWidgetId) const
{
    QVariantList list;
    QSet<int> filters;

    for (const QVariant &type : typeFilters)
        filters.insert(type.toInt());

    QList<quint32> wIDs = m_widgetsMap.keys();
    std::sort(wIDs.begin(), wIDs.end());

    for (const quint32 wid : wIDs)
    {
        VCWidget *widget = m_widgetsMap.value(wid, nullptr);
        if (widget == nullptr || widget->id() == excludeWidgetId)
            continue;

        if (!filters.isEmpty() && !filters.contains(widget->type()))
            continue;

        QVariantMap wMap;
        wMap.insert("classRef", QVariant::fromValue(widget));
        wMap.insert("id", widget->id());
        wMap.insert("type", widget->type());
        wMap.insert("label", widget->caption());
        wMap.insert("icon", widgetIcon(widget->type()));
        list.append(wMap);
    }

    if (list.isEmpty())
    {
        QVariantMap noneMap;
        noneMap.insert("label", tr("<None>"));
        list.append(noneMap);
    }

    return list;
}

/*********************************************************************
 * Pages
 *********************************************************************/

void VirtualConsole::renderPage(QQuickItem *parent, QQuickItem *contentItem, int page) const
{
    if (parent == nullptr)
        return;

    if (page < 0 || page >= m_pages.count())
        return;

    QRectF pageRect = m_pages.at(page)->geometry();
    parent->setProperty("contentWidth", pageRect.width());
    parent->setProperty("contentHeight", pageRect.height());
    VCPage *pageObject = m_pages.at(page);
    connect(pageObject, &VCWidget::geometryChanged, parent, [pageObject, parent]() {
        parent->setProperty("contentWidth", pageObject->geometry().width());
        parent->setProperty("contentHeight", pageObject->geometry().height());
    });

    qDebug() << "[VC] renderPage. Parent:" << parent << "contents rect:" << pageRect;

    m_pages.at(page)->render(m_view, contentItem);
}

void VirtualConsole::enableFlicking(bool enable)
{
    QQuickItem *currPage = currentPageItem();
    if (currPage == nullptr)
        return;

    currPage->setProperty("interactive", enable);

    // Kill kinetic scroll momentum when disabling, otherwise the
    // Flickable keeps moving during widget drag operations
    if (!enable)
    {
        QQuickItem *content = currPage->property("contentItem").value<QQuickItem *>();
        if (content)
            QMetaObject::invokeMethod(currPage, "cancelFlick");
    }
}

VCPage *VirtualConsole::page(int page) const
{
    if (page < 0 || page >= m_pages.count())
        return nullptr;

    return m_pages.at(page);
}

QQuickItem *VirtualConsole::currentPageItem() const
{
    VCPage *currPage = page(m_selectedPage);
    if (currPage == nullptr)
        return nullptr;

    QQuickItem *pageItem = nullptr;
    PreviewContext *ctx = currPage->previewContext();

    if (ctx->detached())
    {
        pageItem = ctx->view()->rootObject();
    }
    else
    {
        QString currPage = QString("vcPage%1").arg(m_selectedPage);
        pageItem = qobject_cast<QQuickItem*>(m_view->rootObject()->findChild<QObject *>(currPage));
    }
    return pageItem;
}

int VirtualConsole::pagesCount() const
{
    return m_pages.count();
}

void VirtualConsole::addPage(int index)
{
    VCPage *page = new VCPage(m_view, m_doc, this, index, this);
    QQmlEngine::setObjectOwnership(page, QQmlEngine::CppOwnership);
    addWidgetToMap(page);
    m_contextManager->registerContext(page->previewContext());
    m_pages.insert(index, page);
    m_editor->recordPage(index, true);

    emit pagesCountChanged();

    if (index == m_selectedPage)
    {
        m_selectedPage++;
        emit selectedPageChanged(m_selectedPage);
    }
}

void VirtualConsole::deletePage(int index)
{
    if (index < 0 || index >= m_pages.count())
        return;

    if (m_pages.count() == 1)
        return;

    m_editor->recordPage(index, false);
    m_pages.at(index)->deleteChildren();
    VCPage *page = m_pages.takeAt(index);
    m_unlockedPagePINs.remove(page->id());
    removeWidgetFromMap(page);
    m_contextManager->unregisterContext(page->previewContext()->name());
    delete page;

    emit pagesCountChanged();

    int oldSelectedPage = m_selectedPage;
    if (index < m_selectedPage)
    {
        m_selectedPage--;
    }
    else if (index == m_selectedPage && m_selectedPage >= m_pages.count())
    {
        m_selectedPage = m_pages.count() - 1;
    }

    if (m_selectedPage != oldSelectedPage)
        emit selectedPageChanged(m_selectedPage);
}

bool VirtualConsole::setPagePIN(int index, QString currentPIN, QString newPIN)
{
    bool ok = false;
    int iPIN;

    Q_UNUSED(iPIN)

    if (index < 0 || index >= m_pages.count())
        return false;

    /* A PIN must be either empty or 4 digits */
    if (newPIN.length() != 0 && newPIN.length() != 4)
        return false;

    VCPage *page = m_pages.at(index);

    /* If the current PIN is set, check if
     * the entered PIN is numeric */
    if (page->PIN() != 0)
    {
        iPIN = currentPIN.toInt(&ok);
        if (ok == false)
            return false;
    }

    /* Check if the current PIN matches with the page PIN */
    if (page->PIN() != 0 &&
        page->PIN() != currentPIN.toInt())
        return false;

    /* At last, set the new PIN for the page */
    if (newPIN.isEmpty())
        page->setPIN(0);
    else
    {
        /* If the new PIN is numeric */
        iPIN = newPIN.toInt(&ok);
        if (ok == false)
            return false;

        page->setPIN(newPIN.toInt());
    }

    return true;
}

bool VirtualConsole::validatePagePIN(int index, QString PIN, bool remember) const
{
    if (index < 0 || index >= m_pages.count())
        return false;

    if (m_pages.at(index)->PIN() != PIN.toInt())
        return false;

    if (remember)
        m_pages.at(index)->validatePIN();
    else
        m_unlockedPagePINs.insert(m_pages.at(index)->id(), m_pages.at(index)->PIN());

    return true;
}

int VirtualConsole::selectedPage() const
{
    return m_selectedPage;
}

void VirtualConsole::setSelectedPage(int selectedPage)
{
    if (m_pages.isEmpty())
        return;

    int clampedPage = qBound(0, selectedPage, m_pages.count() - 1);
    if (m_selectedPage == clampedPage)
        return;

    m_unlockedPagePINs.remove(m_pages.at(m_selectedPage)->id());
    m_selectedPage = clampedPage;
    emit selectedPageChanged(clampedPage);
    if (m_editMode)
    {
        /* Edits act on the selection, so it never holds widgets of a page out of sight */
        QVariantList visible;
        for (const QVariant &id : selectedWidgetIDs())
        {
            QObject *owner = widget(id.toUInt());
            while (owner && owner != m_pages.at(clampedPage))
                owner = owner->parent();
            if (owner)
                visible.append(id);
        }
        setSelectedWidgetIDs(visible);
    }
}

void VirtualConsole::setPageInteraction(bool enable)
{
    QQuickItem *page = currentPageItem();
    if (page != nullptr)
        page->setProperty("interactive", enable);
}

void VirtualConsole::setPageScale(qreal factor)
{
    VCPage *activePage = page(m_selectedPage);
    if (activePage == nullptr)
        return;

    activePage->setPageScale(factor);
}

/*********************************************************************
 * Widgets
 *********************************************************************/

quint32 VirtualConsole::newWidgetId()
{
    /* This results in an endless loop if there are UINT_MAX-1 widgets. That,
       however, seems a bit unlikely. */
    while (m_widgetsMap.contains(m_latestWidgetId) ||
           m_latestWidgetId == VCWidget::invalidId())
    {
        m_latestWidgetId++;
    }

    return m_latestWidgetId;
}

void VirtualConsole::addWidgetToMap(VCWidget* widget)
{
    // Valid ID ?
    if (widget->id() != VCWidget::invalidId())
    {
        // Maybe we don't know this widget yet
        if (!m_widgetsMap.contains(widget->id()))
        {
            m_widgetsMap.insert(widget->id(), widget);
            emit widgetsChanged();
            return;
        }

        // Maybe we already know this widget
        if (m_widgetsMap[widget->id()] == widget)
        {
            qDebug() << Q_FUNC_INFO << "widget" << widget->id() << "already in map";
            return;
        }

        // This widget id conflicts with another one we have to change it.
        qDebug() << Q_FUNC_INFO << "widget id" << widget->id() << "conflicts, creating a new ID";
    }

    quint32 wid = newWidgetId();
    Q_ASSERT(!m_widgetsMap.contains(wid));
    qDebug() << Q_FUNC_INFO << "id=" << wid;
    widget->setID(wid);
    m_widgetsMap.insert(wid, widget);
    emit widgetsChanged();
}

void VirtualConsole::removeWidgetFromMap(VCWidget *widget)
{
    if (widget == nullptr)
        return;

    if (m_itemsMap.remove(widget->id()) > 0)
    {
        emit selectedWidgetChanged();
        emit selectedWidgetsCountChanged();
    }
    if (m_widgetsMap.remove(widget->id()) > 0)
        emit widgetsChanged();
}

VCWidget *VirtualConsole::widget(quint32 id) const
{
    if (id == VCWidget::invalidId())
        return nullptr;

    return m_widgetsMap.value(id, nullptr);
}

QList<VCWidget *> VirtualConsole::widgetsByRecordingId(const QUuid &id) const
{
    QList<VCWidget *> matches;
    if (id.isNull())
        return matches;

    for (VCWidget *widget : m_widgetsMap)
    {
        if (widget->recordingId() == id)
            matches.append(widget);
    }
    return matches;
}

void VirtualConsole::setWidgetSelection(quint32 wID, QQuickItem *item, bool enable, bool multi)
{
    if (!editMode())
        return;
    VCWidget *vcWidget = widget(wID);
    if (!vcWidget)
        return;
    if (!multi)
        resetWidgetSelection();

    if (enable)
    {
        QQuickItem *projection = item ? item : vcWidget->renderItem();
        m_itemsMap[wID] = projection;
        m_lastSelectedID = wID;
        vcWidget->setIsEditing(nativeAuthoringError(vcWidget).isEmpty());
        if (projection)
        {
            projection->setProperty("isSelected", true);
            if (!m_selectionProjections.contains(projection))
            {
                m_selectionProjections.insert(projection);
                connect(projection, &QObject::destroyed, this, [this, projection]() {
                    m_selectionProjections.remove(projection);
                    for (auto it = m_itemsMap.begin(); it != m_itemsMap.end(); ++it)
                        if (it.value() == projection) it.value() = nullptr;
                });
            }
        }
    }
    else
    {
        QQuickItem *projection = m_itemsMap.take(wID);
        if (projection)
            projection->setProperty("isSelected", false);
        vcWidget->setIsEditing(false);
    }

    emit selectedWidgetChanged();
    emit selectedWidgetsCountChanged();
}

void VirtualConsole::setSelectedWidgetIDs(QVariantList ids)
{
    QSignalBlocker block(this);
    resetWidgetSelection();
    if (editMode())
    {
        QSet<quint32> visited;
        for (const QVariant &value : ids)
        {
            bool valid = false;
            quint32 id = value.toUInt(&valid);
            if (valid && !visited.contains(id))
            {
                visited.insert(id);
                setWidgetSelection(id, nullptr, true, true);
            }
        }
    }
    block.unblock();
    emit selectedWidgetChanged();
    emit selectedWidgetsCountChanged();
}

VCOutlineModel *VirtualConsole::outlineModel() const
{
    return m_outlineModel;
}

void VirtualConsole::resetWidgetSelection()
{
    for (auto it = m_itemsMap.constBegin(); it != m_itemsMap.constEnd(); ++it)
    {
        if (it.value())
            it.value()->setProperty("isSelected", false);
        if (VCWidget *selected = widget(it.key()))
            selected->setIsEditing(false);
    }
    m_itemsMap.clear();

    emit selectedWidgetChanged();
    emit selectedWidgetsCountChanged();
}

QStringList VirtualConsole::selectedWidgetNames() const
{
    QStringList names;

    {
        QMap<quint32, QQuickItem*>::const_iterator it = m_itemsMap.begin();
        for (; it != m_itemsMap.end(); it++)
        {
            VCWidget *vcWidget = m_widgetsMap[it.key()];
            if (vcWidget != nullptr)
            {
                if (vcWidget->caption().isEmpty())
                    names << vcWidget->typeToString(vcWidget->type());
                else
                    names << vcWidget->caption();
            }
        }
    }

    return names;
}

int VirtualConsole::selectedWidgetsCount() const
{
    return m_itemsMap.count();
}

QVariantList VirtualConsole::selectedWidgetIDs() const
{
    QVariantList ids;

    QMap<quint32, QQuickItem*>::const_iterator it = m_itemsMap.begin();
    for (; it != m_itemsMap.end(); it++)
    {
        ids << it.key();
    }

    return ids;
}

void VirtualConsole::moveWidget(VCWidget *widget, VCFrame *targetFrame, QPoint pos) const
{
    if (widget == nullptr || targetFrame == nullptr)
        return;
    VirtualConsole *owner = const_cast<VirtualConsole *>(this);
    const qreal scale = qobject_cast<VCPage *>(targetFrame)
        ? qobject_cast<VCPage *>(targetFrame)->pageScale() : targetFrame->scaleFactor();
    QRectF rect = widget->logicalGeometry();
    rect.moveTopLeft(QPointF(pos) / scale);
    owner->reportEdit(owner->editDrop(widget->id(), targetFrame->id(), targetFrame->currentPage(), rect));
}

bool VirtualConsole::reparentWidget(VCWidget *widget, VCFrame *targetFrame) const
{
    if (widget == nullptr || targetFrame == nullptr)
        return false;
    return const_cast<VirtualConsole *>(this)->restoreWidgetParent(
        widget->id(), targetFrame->id(), targetFrame->currentPage(), -1);
}

VCFrame *VirtualConsole::targetFrame(quint32 id) const
{
    if (VCFrame *frame = qobject_cast<VCFrame *>(widget(id)))
        return frame;
    for (VCPage *page : m_pages)
        if (page->id() == id)
            return page;
    return nullptr;
}

QList<VCWidget *> VirtualConsole::selectedRoots(const QVariantList &ids, QString *error) const
{
    QSet<quint32> requested;
    for (const QVariant &value : ids)
    {
        bool valid = false;
        const quint32 id = value.toUInt(&valid);
        VCWidget *entry = valid ? widget(id) : nullptr;
        if (!entry || qobject_cast<VCPage *>(entry))
        {
            *error = tr("Unknown or non-editable widget %1").arg(value.toString());
            return {};
        }
        requested.insert(id);
    }
    QList<VCWidget *> roots;
    for (VCPage *page : m_pages)
        for (VCWidget *entry : page->children(true))
        {
            if (!requested.contains(entry->id()))
                continue;
            bool ancestorSelected = false;
            for (QObject *parent = entry->parent(); parent; parent = parent->parent())
                if (VCWidget *ancestor = qobject_cast<VCWidget *>(parent))
                    if (requested.contains(ancestor->id()))
                        ancestorSelected = true;
            if (!ancestorSelected)
                roots.append(entry);
        }
    if (!requested.isEmpty() && roots.isEmpty())
        *error = tr("Selection is not attached to a console page");
    return roots;
}

QVariantMap VirtualConsole::editResult(bool accepted, bool applied, const QString &error,
                                       const QMap<quint32, QRectF> &rectangles,
                                       VCFrame *destination, int subpage) const
{
    QVariantList ids, geometries;
    for (auto it = rectangles.constBegin(); it != rectangles.constEnd(); ++it)
    {
        VCWidget *entry = widget(it.key());
        VCFrame *parent = destination ? destination
            : (entry ? qobject_cast<VCFrame *>(entry->parent()) : nullptr);
        ids.append(it.key());
        geometries.append(QVariantMap{{"id", it.key()}, {"rect", it.value()},
            {"parentID", parent ? QVariant(parent->id()) : QVariant(-1)},
            {"subpage", subpage >= 0 ? subpage : (entry ? entry->page() : 0)}});
    }
    return {{"accepted", accepted}, {"applied", applied}, {"error", error}, {"targetEligible", true},
            {"widgetIDs", ids}, {"geometries", geometries}};
}

QString VirtualConsole::geometryError(const QMap<quint32, QRectF> &rectangles,
                                      VCFrame *destination, int subpage, bool copying) const
{
    const auto less = GridLayout::less;
    const auto area = [&less](const QRectF &a, const QRectF &b) {
        const qreal left = qMax(a.left(), b.left()), right = qMin(a.right(), b.right());
        const qreal top = qMax(a.top(), b.top()), bottom = qMin(a.bottom(), b.bottom());
        return less(left, right) && less(top, bottom) ? (right - left) * (bottom - top) : qreal(0);
    };
    const auto excess = [&less](qreal required, qreal available) {
        return less(available, required) ? required - available : qreal(0);
    };
    const auto outside = [&excess](const QRectF &rect, const QRectF &bounds) {
        return excess(bounds.left(), rect.left()) + excess(bounds.top(), rect.top()) +
               excess(rect.right(), bounds.right()) + excess(rect.bottom(), bounds.bottom());
    };
    for (auto it = rectangles.constBegin(); it != rectangles.constEnd(); ++it)
    {
        VCWidget *entry = widget(it.key());
        if (!entry)
            return tr("Unknown widget %1").arg(it.key());
        const QRectF rect = it.value();
        const QRectF original = entry->logicalGeometry();
        if (!copying && !destination && rect == original)
            continue;
        if (!qIsFinite(rect.x()) || !qIsFinite(rect.y()) ||
            !qIsFinite(rect.width()) || !qIsFinite(rect.height()) ||
            rect.width() <= 0 || rect.height() <= 0)
            return tr("Invalid geometry for widget %1").arg(it.key());
        const QSizeF minimum = entry->minimumSize();
        if ((less(rect.width(), minimum.width()) && (copying || less(rect.width(), original.width()))) ||
            (less(rect.height(), minimum.height()) && (copying || less(rect.height(), original.height()))))
            return tr("Widget %1 is below its usable minimum size").arg(it.key());
        VCFrame *parent = destination ? destination : qobject_cast<VCFrame *>(entry->parent());
        if (!parent && !qobject_cast<VCPage *>(entry))
            return tr("Widget %1 has no parent frame").arg(it.key());
        QRectF bounds = parent ? parent->contentRect() : QRectF(QPointF(0, 0), rect.size());
        if (parent && !destination && rectangles.contains(parent->id()))
        {
            const QSizeF size = rectangles.value(parent->id()).size();
            bounds.setWidth(size.width());
            bounds.setHeight(qMax(qreal(0), size.height() - bounds.top()));
        }
        if (!finiteEdges(bounds))
            return tr("Invalid content bounds for widget %1").arg(entry->id());
        if (parent && (less(rect.left(), bounds.left()) || less(rect.top(), bounds.top()) ||
                       less(bounds.right(), rect.right()) || less(bounds.bottom(), rect.bottom())))
        {
            const bool sameDomain = entry->parent() == parent && (subpage < 0 || entry->page() == subpage);
            const bool improved = !copying && sameDomain &&
                !less(rect.left(), qMin(bounds.left(), original.left())) &&
                !less(rect.top(), qMin(bounds.top(), original.top())) &&
                !less(qMax(bounds.right(), original.right()), rect.right()) &&
                !less(qMax(bounds.bottom(), original.bottom()), rect.bottom());
            if (!improved)
                return tr("Widget %1 does not fit frame %2 content").arg(it.key()).arg(parent->id());
        }

        QList<VCWidget *> siblings = parent ? parent->children(false) : QList<VCWidget *>{};
        if (destination && !copying)
            for (auto moved = rectangles.constBegin(); moved != rectangles.constEnd(); ++moved)
                if (!siblings.contains(widget(moved.key())))
                    siblings.append(widget(moved.key()));
        const int page = subpage >= 0 ? subpage : entry->page();
        for (VCWidget *sibling : siblings)
        {
            if (!sibling || (!copying && sibling == entry) ||
                (copying ? sibling->page() != page :
                 ((!rectangles.contains(sibling->id()) && sibling->page() != page) ||
                  (!destination && sibling->page() != page))))
                continue;
            const QRectF siblingRect = copying ? sibling->logicalGeometry()
                                               : rectangles.value(sibling->id(), sibling->logicalGeometry());
            const bool sameDomain = entry->parent() == parent && entry->page() == page &&
                                    sibling->parent() == parent && sibling->page() == page;
            const qreal before = !copying && sameDomain ? area(entry->logicalGeometry(), sibling->logicalGeometry()) : 0;
            if (less(before, area(rect, siblingRect)))
                return tr("Widget %1 would obscure widget %2").arg(entry->id()).arg(sibling->id());
        }
        if (copying)
            for (auto other = rectangles.constBegin(); other != rectangles.constEnd(); ++other)
                if (other.key() != it.key() && area(rect, other.value()) > 0)
                    return tr("Pasted widgets %1 and %2 would overlap").arg(it.key()).arg(other.key());
        if (VCFrame *frame = qobject_cast<VCFrame *>(entry))
        {
            QRectF childBounds = frame->contentRect();
            const QRectF oldBounds = childBounds;
            childBounds.setWidth(rect.width());
            childBounds.setHeight(qMax(qreal(0), rect.height() - childBounds.top()));
            for (VCWidget *child : frame->children(false))
            {
                const QRectF childRect = rectangles.value(child->id(), child->logicalGeometry());
                if (less(outside(child->logicalGeometry(), oldBounds), outside(childRect, childBounds)))
                    return tr("Frame %1 would clip child %2").arg(frame->id()).arg(child->id());
            }
        }
    }
    return {};
}

QRectF VirtualConsole::admittedRectangle(VCWidget *entry, VCFrame *parent, QRectF request,
                                        bool snap, QString *error) const
{
    if (!entry || !parent || !qIsFinite(request.x()) || !qIsFinite(request.y()) ||
        !qIsFinite(request.width()) || !qIsFinite(request.height()) ||
        request.width() <= 0 || request.height() <= 0)
    {
        *error = tr("Invalid widget geometry or parent");
        return {};
    }
    const QRectF bounds = parent->contentRect();
    if (!finiteEdges(bounds))
    {
        *error = tr("Frame %1 has invalid content bounds").arg(parent->id());
        return {};
    }
    const QSizeF minimum = entry->minimumSize();
    const QRectF old = entry->logicalGeometry();
    const QVariantMap guide = snapGuide(parent->id());
    const qreal grid = snap && guide.value("enabled").toBool() ? guide.value("step").toSizeF().width() : 0;
    auto nearest = [grid](qreal value) {
        return grid > 0 ? std::round(value / grid) * grid : value;
    };
    auto up = [grid](qreal value) {
        return grid > 0 ? GridLayout::snapUp(value, grid) : value;
    };
    auto down = [grid](qreal value) {
        return grid > 0 ? -GridLayout::snapUp(-value, grid) : value;
    };
    auto equal = [](qreal a, qreal b) {
        return a == b || qAbs(a - b) <= GridLayout::arithmeticTolerance(a, b);
    };
    const bool resizeX = !equal(request.width(), old.width());
    const bool resizeY = !equal(request.height(), old.height());
    struct Axis
    {
        qreal origin;
        qreal size;
    };
    const auto axis = [&](qreal requestedOrigin, qreal requestedSize,
                          qreal oldOrigin, qreal oldSize, qreal minimumSize,
                          qreal boundStart, qreal boundEnd, bool otherResize) -> Axis {
        const auto fail = [&]() -> Axis {
            *error = tr("Widget %1 cannot fit its minimum size and snap grid in frame %2")
                .arg(entry->id()).arg(parent->id());
            return {};
        };
        const bool resizing = !equal(requestedSize, oldSize);
        const bool originalExtent = qIsFinite(oldSize) && oldSize > 0;
        const bool fixedEnd = originalExtent && resizing &&
                              equal(requestedOrigin + requestedSize, oldOrigin + oldSize);
        const bool fixedOrigin = originalExtent && !fixedEnd && (resizing || otherResize) &&
                                 equal(requestedOrigin, oldOrigin);
        if (fixedOrigin || fixedEnd)
        {
            // Legacy clipping may stay or improve; geometryError checks the actual bounds.
            boundStart = qMin(boundStart, oldOrigin);
            boundEnd = qMax(boundEnd, oldOrigin + oldSize);
        }
        qreal origin = fixedOrigin ? oldOrigin : nearest(requestedOrigin);
        qreal size = resizing ? requestedSize : oldSize;
        if (resizing)
        {
            const qreal required = up(minimumSize);
            qreal end = fixedEnd ? oldOrigin + oldSize : nearest(requestedOrigin + requestedSize);
            if (fixedEnd)
            {
                const qreal first = up(boundStart);
                qreal last = down(end - required);
                if (GridLayout::less(end, boundStart) || GridLayout::less(boundEnd, end) ||
                    GridLayout::less(last, first))
                    return fail();
                last = qMax(first, last);
                origin = qBound(first, origin, last);
                size = end - origin;
            }
            else if (fixedOrigin)
            {
                qreal first = up(origin + required);
                const qreal last = down(boundEnd);
                if (GridLayout::less(origin, boundStart) || GridLayout::less(boundEnd, origin) ||
                    GridLayout::less(last, first))
                    return fail();
                first = qMin(first, last);
                end = qBound(first, end, last);
                size = end - origin;
            }
            else
                size = qMax(required, end - origin);
            if (GridLayout::less(size, required))
                return fail();
            size = qMax(size, minimumSize);
        }
        if (fixedOrigin || fixedEnd)
        {
            if (GridLayout::less(origin, boundStart) || GridLayout::less(boundEnd, origin + size))
                return fail();
            return {origin, size};
        }
        const qreal first = up(boundStart);
        qreal last = down(boundEnd - size);
        if (GridLayout::less(boundEnd, first + size))
            return fail();
        last = qMax(first, last);
        return {qBound(first, origin, last), size};
    };
    const Axis horizontal = axis(request.left(), request.width(), old.left(), old.width(),
                                 minimum.width(), bounds.left(), bounds.right(), resizeY);
    if (!error->isEmpty())
        return {};
    const Axis vertical = axis(request.top(), request.height(), old.top(), old.height(),
                               minimum.height(), bounds.top(), bounds.bottom(), resizeX);
    if (!error->isEmpty())
        return {};
    return QRectF(horizontal.origin, vertical.origin, horizontal.size, vertical.size);
}

QVariantMap VirtualConsole::previewGeometry(quint32 id, QRectF logicalRect, bool snap)
{
    VCWidget *entry = widget(id);
    QString error;
    if (!entry || qobject_cast<VCPage *>(entry))
        return editResult(false, false, tr("Unknown or non-editable widget %1").arg(id));
    error = nativeAuthoringError(entry);
    if (!error.isEmpty())
        return editResult(false, false, error);
    QRectF rect = admittedRectangle(entry, qobject_cast<VCFrame *>(entry->parent()), logicalRect, snap, &error);
    // children of arranged frames move along, checked here and placed again on commit
    QMap<quint32, QRectF> rectangles{{id, rect}};
    if (error.isEmpty())
        error = m_editor->arrangePlan(rectangles);
    if (error.isEmpty())
        error = geometryError(rectangles);
    return editResult(error.isEmpty(), false, error, error.isEmpty() ? QMap<quint32, QRectF>{{id, rect}} : QMap<quint32, QRectF>{});
}

QVariantMap VirtualConsole::editGeometry(quint32 id, QRectF logicalRect, bool snap)
{
    /* a page has no frame to fit in, only its children limit its size */
    if (qobject_cast<VCPage *>(widget(id)))
        return applyGeometryPlan({{id, QRectF(QPointF(0, 0), logicalRect.size())}});

    QVariantMap preview = previewGeometry(id, logicalRect, snap);
    if (!preview.value("accepted").toBool())
        return preview;
    const QRectF rect = preview.value("geometries").toList().first().toMap().value("rect").toRectF();
    return applyGeometryPlan({{id, rect}});
}

QVariantMap VirtualConsole::applyGeometryPlan(const QMap<quint32, QRectF> &rectangles, bool snap)
{
    if (m_authoringBusy)
        return editResult(false, false, tr("Another console edit is being committed"));
    QMap<quint32, QRectF> candidates = rectangles;
    QString error;
    if (snap)
        for (auto it = candidates.begin(); it != candidates.end(); ++it)
        {
            VCWidget *entry = widget(it.key());
            it.value() = admittedRectangle(entry, entry ? qobject_cast<VCFrame *>(entry->parent()) : nullptr,
                                           it.value(), true, &error);
            if (!error.isEmpty())
                return editResult(false, false, error);
        }
    error = m_editor->arrangePlan(candidates);
    if (error.isEmpty())
        error = geometryError(candidates);
    if (!error.isEmpty())
        return editResult(false, false, error);
    return commitGeometry(candidates);
}

QVariantMap VirtualConsole::previewGeometryPlan(const QMap<quint32, QRectF> &rectangles) const
{
    const QString error = geometryError(rectangles);
    return editResult(error.isEmpty(), false, error, error.isEmpty() ? rectangles : QMap<quint32, QRectF>{});
}

QVariantMap VirtualConsole::commitGeometry(const QMap<quint32, QRectF> &candidates)
{
    if (m_authoringBusy)
        return editResult(false, false, tr("Another console edit is being committed"));
    bool changed = false;
    for (auto it = candidates.constBegin(); it != candidates.constEnd(); ++it)
        changed = changed || widget(it.key())->logicalGeometry() != it.value();
    if (!changed)
        return editResult(true, false, {}, candidates);
    QScopedValueRollback<bool> busy(m_authoringBusy, true);
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (auto it = candidates.constBegin(); it != candidates.constEnd(); ++it)
        blockers.emplace_back(new QSignalBlocker(widget(it.key())));
    Tardis *tardis = Tardis::instance();
    if (tardis) tardis->beginBatch("consoleGeometryEdit");
    for (auto it = candidates.constBegin(); it != candidates.constEnd(); ++it)
        widget(it.key())->setLogicalGeometry(it.value());
    m_doc->setModified();
    if (tardis) tardis->endBatch();
    blockers.clear();
    for (auto it = candidates.constBegin(); it != candidates.constEnd(); ++it)
        emit widget(it.key())->geometryChanged();
    return editResult(true, true, {}, candidates);
}

QVariantMap VirtualConsole::previewDrop(quint32 id, quint32 targetFrameID, int targetSubpage,
                                        QRectF logicalRect, int insertionIndex)
{
    VCWidget *entry = widget(id);
    VCFrame *target = targetFrame(targetFrameID);
    for (QObject *ancestor = target; ancestor; ancestor = ancestor->parent())
        if (ancestor == entry)
        {
            QVariantMap result = editResult(false, false, tr("Cannot move a frame into itself or a descendant"));
            result["targetEligible"] = false;
            return result;
        }
    QString error;
    QRectF rect = admittedRectangle(entry, target, logicalRect, true, &error);
    if (!error.isEmpty())
        return editResult(false, false, error);
    if (insertionIndex < 0)
        insertionIndex = m_editor->dropIndex(target, targetSubpage, rect);
    if (entry->parent() == target && entry->page() == targetSubpage && insertionIndex < 0)
    {
        error = geometryError({{id, rect}});
        return editResult(error.isEmpty(), false, error, error.isEmpty() ? QMap<quint32, QRectF>{{id, rect}} : QMap<quint32, QRectF>{});
    }
    return structureEdit({id}, targetFrameID, targetSubpage, insertionIndex, {{id, rect}}, true);
}

QVariantMap VirtualConsole::editDrop(quint32 id, quint32 targetFrameID, int targetSubpage,
                                     QRectF logicalRect, int insertionIndex)
{
    if (insertionIndex < 0 && targetFrame(targetFrameID))
        insertionIndex = m_editor->dropIndex(targetFrame(targetFrameID), targetSubpage, logicalRect);
    const QVariantMap preview = previewDrop(id, targetFrameID, targetSubpage, logicalRect, insertionIndex);
    if (!preview.value("accepted").toBool())
        return preview;
    const QRectF rect = preview.value("geometries").toList().first().toMap().value("rect").toRectF();
    if (widget(id)->parent() == targetFrame(targetFrameID) && widget(id)->page() == targetSubpage && insertionIndex < 0)
        return applyGeometryPlan({{id, rect}});
    return structureEdit({id}, targetFrameID, targetSubpage, insertionIndex, {{id, rect}}, false);
}

QVariantMap VirtualConsole::editStructure(QVariantList ids, quint32 targetFrameID,
                                          int targetSubpage, int insertionIndex)
{
    return structureEdit(ids, targetFrameID, targetSubpage, insertionIndex, {}, false);
}

QVariantMap VirtualConsole::editStructureBefore(QVariantList ids, quint32 targetFrameID,
                                                int targetSubpage, int beforeID)
{
    VCFrame *target = targetFrame(targetFrameID);
    if (!target)
        return editResult(false, false, tr("Unknown target frame %1").arg(targetFrameID));
    QString error = nativeAuthoringError(target);
    if (!error.isEmpty())
        return editResult(false, false, error);
    const QList<VCWidget *> roots = selectedRoots(ids, &error);
    if (!error.isEmpty())
        return editResult(false, false, error);
    for (VCWidget *root : roots)
    {
        error = nativeAuthoringError(root);
        if (!error.isEmpty())
            return editResult(false, false, error);
    }
    const QVariantList order = target->orderedChildIDs(targetSubpage);
    int index = -1;
    if (beforeID >= 0)
    {
        for (int i = 0; i < order.size(); ++i)
            if (order[i].toUInt() == quint32(beforeID))
                index = i;
        if (index < 0)
            return editResult(false, false, tr("Insertion anchor %1 is not in the target subpage").arg(beforeID));
    }
    return editStructure(ids, targetFrameID, targetSubpage, index);
}

QVariantMap VirtualConsole::structureEdit(const QVariantList &ids, quint32 targetID, int subpage,
                                          int insertionIndex, const QMap<quint32, QRectF> &rectangles,
                                          bool preview)
{
    if (m_authoringBusy)
        return editResult(false, false, tr("Another console edit is being committed"));
    VCFrame *target = targetFrame(targetID);
    if (!target || subpage < 0 || subpage >= target->totalPagesNumber())
        return editResult(false, false, tr("Invalid target frame or subpage"));
    QString error;
    const QList<VCWidget *> roots = selectedRoots(ids, &error);
    if (!error.isEmpty())
        return editResult(false, false, error);
    if (roots.isEmpty())
        return editResult(false, false, tr("No widgets selected"));
    QMap<quint32, QRectF> candidates;
    for (VCWidget *root : roots)
    {
        for (QObject *ancestor = target; ancestor; ancestor = ancestor->parent())
            if (ancestor == root)
                return editResult(false, false, tr("Cannot move widget %1 into itself or a descendant").arg(root->id()));
        candidates.insert(root->id(), rectangles.value(root->id(), root->logicalGeometry()));
    }
    QList<VCWidget *> order;
    for (VCWidget *child : target->children(false))
        if (child->page() == subpage)
            order.append(child);
    if (insertionIndex < -1 || insertionIndex > order.size())
        return editResult(false, false, tr("Invalid insertion position"));
    int adjustedIndex = insertionIndex < 0 ? order.size() : insertionIndex;
    for (int i = 0; i < (insertionIndex < 0 ? order.size() : insertionIndex); ++i)
        if (roots.contains(order[i]))
            --adjustedIndex;
    for (VCWidget *root : roots)
        order.removeAll(root);
    for (int i = 0; i < roots.size(); ++i)
        order.insert(adjustedIndex + i, roots[i]);
    QMap<quint32, QRectF> siblings;
    error = m_editor->arrangeStructure(target, subpage, order, candidates, siblings);
    QMap<quint32, QRectF> checked = candidates;
    checked.insert(siblings);
    if (error.isEmpty())
        error = geometryError(checked, target, subpage);
    if (!error.isEmpty())
        return editResult(false, false, error);
    if (preview)
        return editResult(true, false, {}, candidates, target, subpage);

    bool changed = false;
    QVariantList oldOrder = target->orderedChildIDs(subpage), newOrder;
    for (VCWidget *entry : order) newOrder.append(entry->id());
    changed = oldOrder != newOrder;
    for (VCWidget *root : roots)
        changed = changed || root->parent() != target || root->page() != subpage ||
                  root->logicalGeometry() != candidates.value(root->id());
    if (!changed)
        return editResult(true, false, {}, candidates, target, subpage);
    return m_editor->commitStructure(roots, target, subpage, order, candidates, siblings);
}

QVariantMap VirtualConsole::commitStructure(const QList<VCWidget *> &roots, VCFrame *target, int subpage,
                                            const QList<VCWidget *> &order,
                                            const QMap<quint32, QRectF> &candidates, bool recordHistory)
{
    QScopedValueRollback<bool> busy(m_authoringBusy, true);
    cancelLayout();
    emit structureAboutToChange();
    QSet<VCFrame *> frames{target};
    QSet<VCWidget *> affected;
    for (VCWidget *root : roots)
    {
        frames.insert(qobject_cast<VCFrame *>(root->parent()));
        affected.insert(root);
        if (VCFrame *frame = qobject_cast<VCFrame *>(root))
            for (VCWidget *child : frame->children(true))
                affected.insert(child);
    }
    std::vector<std::unique_ptr<QSignalBlocker>> blockers;
    for (VCFrame *frame : frames) if (frame) blockers.emplace_back(new QSignalBlocker(frame));
    for (VCWidget *entry : affected) if (!frames.contains(qobject_cast<VCFrame *>(entry)))
        blockers.emplace_back(new QSignalBlocker(entry));
    Tardis *tardis = Tardis::instance();
    if (tardis) tardis->beginBatch("consoleStructureEdit");
    QHash<VCFrame *, QList<VCWidget *>> arrivals;
    QSet<VCFrame *> ancestryFrames;
    QMap<quint32, QVariantMap> oldLocations;
    for (VCWidget *root : roots)
    {
        VCFrame *source = qobject_cast<VCFrame *>(root->parent());
        const QVariantList sourceOrder = source->orderedChildIDs(root->page());
        const int index = sourceOrder.indexOf(QVariant(root->id()));
        oldLocations[root->id()] = {{"parentID", source->id()}, {"subpage", root->page()}, {"orderIndex", index},
            {"beforeID", index + 1 < sourceOrder.size() ? sourceOrder[index + 1] : QVariant(VCWidget::invalidId())}};
    }
    for (VCWidget *root : roots)
    {
        VCFrame *source = qobject_cast<VCFrame *>(root->parent());
        const bool moved = source != target || root->page() != subpage;
        if (source != target)
        {
            ancestryFrames.insert(source);
            ancestryFrames.insert(target);
            arrivals[target].append(root);
        }
        const int index = order.indexOf(root);
        const QVariantMap location{{"parentID", target->id()}, {"subpage", subpage}, {"orderIndex", index},
            {"beforeID", index + 1 < order.size() ? QVariant(order[index + 1]->id()) : QVariant(VCWidget::invalidId())}};
        if (recordHistory && tardis && oldLocations.value(root->id()) != location)
            tardis->enqueueAction(Tardis::VCWidgetReparent, root->id(), oldLocations.value(root->id()), location);
        if (moved)
        {
            source->removeWidgetFromPageMap(root);
            root->setParent(target);
            root->setPage(subpage);
            root->remapInputSources(subpage);
            target->addWidgetToPageMap(root);
            root->setVisible(subpage == target->currentPage());
            root->setScaleFactor(qobject_cast<VCPage *>(target)
                ? qobject_cast<VCPage *>(target)->pageScale() : target->scaleFactor());
            if (VCFrame *frame = qobject_cast<VCFrame *>(root))
            {
                for (VCWidget *child : frame->children(true))
                    child->setScaleFactor(root->scaleFactor());
                frame->refreshFunctionRouting();
            }
            QQuickItem *item = root->renderItem();
            QQuickItem *parentItem = target->renderItem();
            if (item && parentItem && parentItem->window())
            {
                QQuickItem *area = parentItem->findChild<QQuickItem *>(QString("frameDropArea%1").arg(target->id()));
                if (area) item->setParentItem(area);
                else
                {
                    qWarning() << Q_FUNC_INFO << "Target render item has no child area" << target->id();
                    root->discardRenderItem();
                }
            }
            else if (item)
                root->discardRenderItem();
        }
        root->setLogicalGeometry(candidates.value(root->id()));
    }
    target->setChildOrder(subpage, order);
    QSet<VCWidget *> gainEffects;
    for (VCFrame *frame : ancestryFrames)
        for (VCWidget *entry : frame->reconcileSubmasters(arrivals.value(frame)))
            gainEffects.insert(entry);
    for (VCWidget *entry : gainEffects)
    {
        bool covered = false;
        for (QObject *parent = entry->parent(); parent; parent = parent->parent())
            if (gainEffects.contains(qobject_cast<VCWidget *>(parent)))
                covered = true;
        if (!covered)
        {
            VCFrame *parent = qobject_cast<VCFrame *>(entry->parent());
            entry->adjustIntensity(parent->m_submasterValue * parent->intensity());
        }
    }
    for (VCPage *page : m_pages)
    {
        page->resetInputSourcesMap();
        for (const auto &source : page->inputSources())
            page->mapInputSource(source, page, false);
        page->mapChildrenInputSources();
        page->buildKeySequenceMap();
    }
    updatePageInputs();
    m_doc->setModified();
    if (tardis) tardis->endBatch();
    blockers.clear();
    for (VCWidget *entry : affected)
    {
        emit entry->geometryChanged();
        emit entry->pageChanged(entry->page());
        emit entry->isVisibleChanged(entry->isVisible());
    }
    for (VCFrame *frame : frames)
        if (frame)
            for (int page = 0; page < frame->totalPagesNumber(); ++page)
                emit frame->childOrderChanged(page);
    QVariantMap result = editResult(true, true, {}, candidates, target, subpage);
    QVariantList committedIDs;
    for (VCWidget *root : roots) committedIDs.append(root->id());
    result["widgetIDs"] = committedIDs;
    emit structureChanged(committedIDs);
    return result;
}

bool VirtualConsole::restoreWidgetParent(quint32 id, quint32 parentID, int subpage, int orderIndex, quint32 beforeID)
{
    VCWidget *entry = widget(id);
    VCFrame *target = targetFrame(parentID);
    if (m_authoringBusy || !entry || !target || qobject_cast<VCPage *>(entry) ||
        subpage < 0 || subpage >= target->totalPagesNumber())
    {
        qWarning() << Q_FUNC_INFO << "Cannot restore widget parent, page or order" << id << parentID << subpage;
        return false;
    }
    for (QObject *parent = target; parent; parent = parent->parent())
        if (parent == entry)
        {
            qWarning() << Q_FUNC_INFO << "Refusing cyclic parent restoration" << id << parentID;
            return false;
        }
    QList<VCWidget *> order;
    for (VCWidget *child : target->children(false))
        if (child != entry && child->page() == subpage)
            order.append(child);
    int insertion = orderIndex < 0 ? order.size() : qBound(0, orderIndex, int(order.size()));
    if (beforeID != VCWidget::invalidId())
        for (int i = 0; i < order.size(); ++i)
            if (order[i]->id() == beforeID)
                insertion = i;
    order.insert(insertion, entry);
    return commitStructure({entry}, target, subpage, order, {{id, entry->logicalGeometry()}}, false)
        .value("accepted").toBool();
}

GridLayout::LayoutPlan VirtualConsole::calculateLayout(quint32 frameID, const QVariantMap &options,
                                                       GridLayout::LayoutSnapshot *source,
                                                       GridLayout::LayoutOptions *values,
                                                       QString *error) const
{
    VCFrame *root = targetFrame(frameID);
    if (!root)
    {
        *error = tr("Unknown layout container %1").arg(frameID);
        return {};
    }
    const QSet<QString> allowed{"algorithm", "padding", "pad", "gap", "columns",
                               "allowGrowth", "allowHeightGrowth", "allowWidthGrowth"};
    for (auto it = options.constBegin(); it != options.constEnd(); ++it)
        if (!allowed.contains(it.key()))
        {
            *error = tr("Unknown layout option %1").arg(it.key());
            return {};
        }
    *source = VCLayoutAdapter::capture(root);
    const QString algorithm = options.value("algorithm", "align").toString();
    if (algorithm != "align" && algorithm != "compact")
    {
        *error = tr("Unknown layout algorithm %1").arg(algorithm);
        return {};
    }
    if (options.contains("columns"))
    {
        bool valid;
        int columns = options.value("columns").toInt(&valid);
        if (!valid || columns < 0)
        {
            *error = tr("Invalid layout column count");
            return {};
        }
        for (auto &container : source->containers)
            if (container.id == frameID)
                for (auto &page : container.pages)
                    page.columns = columns;
    }
    values->padding = options.value("padding", options.value("pad", 5)).toReal();
    values->gap = options.value("gap", values->padding).toReal();
    values->allowHeightGrowth = options.value("allowHeightGrowth", options.value("allowGrowth", false)).toBool();
    values->allowWidthGrowth = options.value("allowWidthGrowth", false).toBool();
    GridLayout::LayoutPlan plan = algorithm == "compact" ? GridLayout::compact(*source, *values)
                                                         : GridLayout::align(*source, *values);
    if (!plan.ok())
        *error = QString("%1: %2 (widget %3, parent %4, required %5, available %6)")
            .arg(plan.error.code, plan.error.message).arg(plan.error.widgetId).arg(plan.error.parentId)
            .arg(plan.error.required).arg(plan.error.available);
    return plan;
}

QMap<quint32, QRectF> VirtualConsole::layoutRectangles(const GridLayout::LayoutSnapshot &source,
                                                      const GridLayout::LayoutPlan &plan,
                                                      const GridLayout::LayoutOptions &options,
                                                      QString *error) const
{
    QMap<quint32, QRectF> rectangles;
    const GridLayout::Diagnostic diagnostic = GridLayout::validate(source, plan, options);
    if (!diagnostic.code.isEmpty())
    {
        *error = QString("%1: %2").arg(diagnostic.code, diagnostic.message);
        return {};
    }
    for (const auto &placement : plan.placements)
    {
        const auto original = std::find_if(source.items.begin(), source.items.end(),
            [&](const auto &item) { return item.id == placement.id; });
        if (original == source.items.end() || original->parentId != placement.parentId ||
            original->subpage != placement.subpage || original->order != placement.order ||
            rectangles.contains(placement.id))
        {
            *error = tr("Layout policy changed widget identity, hierarchy or order");
            return {};
        }
        rectangles.insert(placement.id, placement.rect);
    }
    for (const auto &growth : plan.growth)
    {
        VCFrame *frame = targetFrame(growth.id);
        const auto original = std::find_if(source.containers.begin(), source.containers.end(),
            [&](const auto &container) { return container.id == growth.id; });
        if (!frame || original == source.containers.end())
        {
            *error = tr("Layout policy returned an unknown container");
            return {};
        }
        QRectF rect = rectangles.value(growth.id, frame->logicalGeometry());
        rect.setSize(growth.size);
        rectangles.insert(growth.id, rect);
    }
    if (rectangles.contains(source.rootId) && !qobject_cast<VCPage *>(targetFrame(source.rootId)))
        *error = geometryError({{source.rootId, rectangles.value(source.rootId)}});
    return rectangles;
}

QVariantMap VirtualConsole::layoutResult(const GridLayout::LayoutSnapshot &source,
                                         const GridLayout::LayoutPlan &plan,
                                         const QMap<quint32, QRectF> &rectangles) const
{
    QVariantMap result = editResult(true, false, {}, rectangles);
    QVariantList geometries = result.value("geometries").toList();
    for (QVariant &value : geometries)
    {
        QVariantMap geometry = value.toMap();
        VCWidget *entry = widget(geometry.value("id").toUInt());
        QRectF pageRect = geometry.value("rect").toRectF();
        VCPage *page = qobject_cast<VCPage *>(entry);
        for (QObject *parent = entry ? entry->parent() : nullptr; parent; parent = parent->parent())
        {
            if ((page = qobject_cast<VCPage *>(parent)))
                break;
            if (VCWidget *ancestor = qobject_cast<VCWidget *>(parent))
                pageRect.translate(rectangles.value(ancestor->id(), ancestor->logicalGeometry()).topLeft());
        }
        geometry["pageRect"] = pageRect;
        geometry["pageIndex"] = m_pages.indexOf(page);
        value = geometry;
    }
    result["geometries"] = geometries;
    QVariantList growth;
    QStringList warnings;
    for (const auto &item : plan.growth)
    {
        const auto original = std::find_if(source.containers.begin(), source.containers.end(),
            [&](const auto &container) { return container.id == item.id; });
        growth.append(QVariantMap{{"id", item.id}, {"from", original->size}, {"to", item.size}});
        warnings.append(tr("Frame %1 grows from %2 x %3 to %4 x %5")
            .arg(item.id).arg(original->size.width()).arg(original->size.height())
            .arg(item.size.width()).arg(item.size.height()));
    }
    result["growth"] = growth;
    result["warnings"] = warnings;
    return result;
}

QVariantList VirtualConsole::layoutPreview() const
{
    return m_layoutPreview;
}

void VirtualConsole::cancelLayout()
{
    m_hasLayoutPreview = false;
    m_layoutSource = {};
    m_layoutState = {};
    m_layoutPlan = {};
    m_layoutOptions = {};
    if (!m_layoutPreview.isEmpty())
    {
        m_layoutPreview.clear();
        emit layoutPreviewChanged();
    }
}

QVariantMap VirtualConsole::previewLayout(quint32 frameID, QVariantMap options)
{
    const QString accessError = nativeAuthoringError(targetFrame(frameID));
    if (!accessError.isEmpty())
        return editResult(false, false, accessError);
    cancelLayout();
    QString error;
    GridLayout::LayoutSnapshot source;
    GridLayout::LayoutOptions values;
    GridLayout::LayoutPlan plan = calculateLayout(frameID, options, &source, &values, &error);
    QMap<quint32, QRectF> rectangles;
    if (error.isEmpty())
        rectangles = layoutRectangles(source, plan, values, &error);
    if (!error.isEmpty())
        return editResult(false, false, error);
    QVariantMap result = layoutResult(source, plan, rectangles);
    // Retain actual state, not option-adjusted columns, as the stale-edit baseline.
    m_layoutSource = source;
    m_layoutState = VCLayoutAdapter::capture(targetFrame(frameID));
    m_layoutPlan = plan;
    m_layoutOptions = values;
    m_layoutPreview = result.value("geometries").toList();
    m_hasLayoutPreview = true;
    emit layoutPreviewChanged();
    return result;
}

QVariantMap VirtualConsole::confirmLayout()
{
    if (!m_hasLayoutPreview)
        return editResult(false, false, tr("No prepared layout"));
    VCFrame *root = targetFrame(m_layoutSource.rootId);
    const QString accessError = nativeAuthoringError(root);
    if (!accessError.isEmpty())
    {
        cancelLayout();
        return editResult(false, false, accessError);
    }
    if (!root || !VCLayoutAdapter::sameSnapshot(m_layoutState, VCLayoutAdapter::capture(root)))
    {
        cancelLayout();
        return editResult(false, false, tr("Prepared layout is stale; preview it again"));
    }
    QString error;
    QMap<quint32, QRectF> rectangles = layoutRectangles(m_layoutSource, m_layoutPlan, m_layoutOptions, &error);
    QVariantMap result = error.isEmpty() ? commitGeometry(rectangles) : editResult(false, false, error);
    cancelLayout();
    return result;
}

QVariantMap VirtualConsole::arrangeLayout(quint32 frameID, QVariantMap options, bool dryRun)
{
    QString error;
    GridLayout::LayoutSnapshot source;
    GridLayout::LayoutOptions values;
    GridLayout::LayoutPlan plan = calculateLayout(frameID, options, &source, &values, &error);
    QMap<quint32, QRectF> rectangles;
    if (error.isEmpty())
        rectangles = layoutRectangles(source, plan, values, &error);
    if (!error.isEmpty())
        return editResult(false, false, error);
    QVariantMap result = layoutResult(source, plan, rectangles);
    if (!dryRun)
    {
        QVariantMap applied = commitGeometry(rectangles);
        if (!applied.value("accepted").toBool())
            return applied;
        result["applied"] = applied.value("applied");
    }
    return result;
}

QVariantMap VirtualConsole::createPreflight(quint32 targetID, const QString &type, QRectF rect,
                                           int subpage, bool fitDefault, const QString &caption) const
{
    const auto less = GridLayout::less;
    VCFrame *target = targetFrame(targetID);
    const VCWidget::WidgetType widgetType = VCWidget::stringToType(type);
    if (!target || widgetType == VCWidget::UnknownWidget)
        return editResult(false, false, tr("Unknown target frame or widget type %1").arg(type));
    const int page = subpage < 0 ? target->currentPage() : subpage;
    if (page < 0 || page >= target->totalPagesNumber())
        return editResult(false, false, tr("Invalid target subpage %1").arg(page));
    VCWidget prototype(nullptr, target);
    prototype.setType(widgetType);
    prototype.setupLookAndFeel(pixelDensity(), page);
    quint32 nextID = m_latestWidgetId;
    while (m_widgetsMap.contains(nextID) || nextID == VCWidget::invalidId())
        ++nextID;
    prototype.setCaption(caption.isEmpty() ? QString("%1 %2").arg(type).arg(nextID + 1) : caption);
    const QSizeF minimum = prototype.minimumSize();
    const QRectF bounds = target->contentRect();
    if (fitDefault)
    {
        QSizeF preferred = minimum * (widgetType == VCWidget::FrameWidget ||
                                     widgetType == VCWidget::SoloFrameWidget ? 2 : 1.5);
        preferred.setWidth(qMin(preferred.width(), bounds.width()));
        preferred.setHeight(qMin(preferred.height(), bounds.height()));
        rect.setSize(preferred);
        QString error;
        rect = admittedRectangle(&prototype, target, rect, true, &error);
        if (!error.isEmpty())
            return editResult(false, false, error);
    }
    if (!finiteEdges(bounds) || !qIsFinite(rect.x()) || !qIsFinite(rect.y()) ||
        !qIsFinite(rect.width()) || !qIsFinite(rect.height()) ||
        less(rect.width(), minimum.width()) || less(rect.height(), minimum.height()) ||
        less(rect.left(), bounds.left()) || less(rect.top(), bounds.top()) ||
        less(bounds.right(), rect.right()) || less(bounds.bottom(), rect.bottom()))
    {
        QVariantMap result = editResult(false, false, tr("New %1 cannot fit its usable size in frame %2 content")
                                        .arg(type).arg(targetID));
        result["minimumSize"] = minimum;
        result["contentRect"] = bounds;
        result["requestedRect"] = rect;
        return result;
    }
    /* an arranged frame places the new control itself */
    const QString arranged = m_editor->arrangedFit(target, page, {rect.size()}, {minimum});
    if (!arranged.isEmpty())
        return editResult(false, false, arranged);
    for (VCWidget *sibling : target->children(false))
        if (!m_editor->isArranged(target) && sibling->page() == page &&
            less(qMax(rect.left(), sibling->logicalGeometry().left()),
                 qMin(rect.right(), sibling->logicalGeometry().right())) &&
            less(qMax(rect.top(), sibling->logicalGeometry().top()),
                 qMin(rect.bottom(), sibling->logicalGeometry().bottom())))
            return editResult(false, false, tr("New control would obscure widget %1").arg(sibling->id()));
    QVariantMap result = editResult(true, false, {});
    result["minimumSize"] = minimum;
    result["rect"] = rect;
    result["geometries"] = QVariantList{QVariantMap{{"id", -1}, {"rect", rect},
                                                   {"parentID", targetID}, {"subpage", page}}};
    return result;
}

QVariantMap VirtualConsole::previewCreate(quint32 targetID, QString type, QPointF logicalPosition, int subpage)
{
    const QString error = nativeAuthoringError(targetFrame(targetID));
    if (!error.isEmpty())
        return editResult(false, false, error);
    return createPreflight(targetID, type, QRectF(logicalPosition, QSizeF(1, 1)), subpage, true);
}

QVariantMap VirtualConsole::previewCreateGeometry(quint32 targetID, QString type, QRectF logicalRect, int subpage)
{
    return createPreflight(targetID, type, logicalRect, subpage, false);
}

VCWidget *VirtualConsole::commitWidgetCreation(VCFrame *target, const QString &type,
                                               const QRectF &rect, int subpage, quint32 functionID)
{
    QQuickItem *area = nullptr;
    if (target->renderItem())
        area = target->renderItem()->findChild<QQuickItem *>(QString("frameDropArea%1").arg(target->id()));
    VCWidget *created = target->addWidgetUnchecked(area, type, rect.topLeft().toPoint());
    if (!created)
        return nullptr;
    created->setLogicalGeometry(rect);
    target->moveWidgetToPage(created, subpage);
    if (functionID != Function::invalidId())
    {
        if (VCButton *button = qobject_cast<VCButton *>(created)) button->setFunctionID(functionID);
        else if (VCSlider *slider = qobject_cast<VCSlider *>(created)) slider->setControlledFunction(functionID);
        else if (VCCueList *list = qobject_cast<VCCueList *>(created)) list->setChaserID(functionID);
        created->setCaption(m_doc->function(functionID)->name());
    }
    created->setVisible(subpage == target->currentPage());
    return created;
}

QVariantMap VirtualConsole::createWidget(quint32 targetID, QString type, QPointF logicalPosition, int subpage)
{
    QVariantMap preview = previewCreate(targetID, type, logicalPosition, subpage);
    if (!preview.value("accepted").toBool())
        return preview;
    return createWidgetGeometry(targetID, type, preview.value("rect").toRectF(), subpage);
}

QVariantMap VirtualConsole::createWidgetGeometry(quint32 targetID, QString type, QRectF logicalRect, int subpage)
{
    if (m_authoringBusy)
        return editResult(false, false, tr("Another console edit is being committed"));
    QVariantMap preview = previewCreateGeometry(targetID, type, logicalRect, subpage);
    if (!preview.value("accepted").toBool())
        return preview;
    VCFrame *target = targetFrame(targetID);
    const int page = subpage < 0 ? target->currentPage() : subpage;
    QScopedValueRollback<bool> busy(m_authoringBusy, true);
    emit structureAboutToChange();
    QSignalBlocker blockOwner(this);
    QSignalBlocker blockParent(target);
    Tardis *tardis = Tardis::instance();
    if (tardis) tardis->beginBatch("consoleCreate");
    VCWidget *created = commitWidgetCreation(target, type, logicalRect, page);
    if (created)
        m_editor->arrangeJoined(target);
    if (tardis) tardis->endBatch();
    if (!created)
    {
        qWarning() << Q_FUNC_INFO << "Admitted widget type could not be created" << type;
        return editResult(false, false, tr("Cannot construct widget type %1").arg(type));
    }
    m_doc->setModified();
    QVariantMap result = editResult(true, true, {}, {{created->id(), logicalRect}});
    blockParent.unblock();
    blockOwner.unblock();
    emit target->childOrderChanged(page);
    emit widgetsChanged();
    emit structureChanged({created->id()});
    return result;
}

QVariantMap VirtualConsole::createFunctions(quint32 targetID, QVariantList functionIDs,
                                           QPointF logicalPosition, int keyModifiers, int subpage)
{
    if (m_authoringBusy)
        return editResult(false, false, tr("Another console edit is being committed"));
    VCFrame *target = targetFrame(targetID);
    if (!target || functionIDs.isEmpty())
        return editResult(false, false, tr("Missing target frame or Functions"));
    const QString accessError = nativeAuthoringError(target);
    if (!accessError.isEmpty())
        return editResult(false, false, accessError);
    const int page = subpage < 0 ? target->currentPage() : subpage;
    const QString type = keyModifiers & Qt::ShiftModifier ? "Slider"
        : (keyModifiers & Qt::ControlModifier ? "Cue list" : "Button");
    const QRectF bounds = target->contentRect();
    QPointF position = logicalPosition;
    qreal rowHeight = 0;
    QVector<QPair<quint32, QRectF>> creations;
    QList<QSizeF> sizes, minimums;
    for (const QVariant &value : functionIDs)
    {
        Function *function = m_doc->function(value.toUInt());
        if (!function || (type == "Cue list" && function->type() != Function::ChaserType))
            return editResult(false, false, tr("Invalid Function %1 for %2").arg(value.toString(), type));
        QVariantMap preview = createPreflight(targetID, type, QRectF(position, QSizeF(1, 1)),
                                              page, true, function->name());
        if (!preview.value("accepted").toBool())
            return preview;
        QRectF rect = preview.value("rect").toRectF();
        sizes.append(rect.size());
        minimums.append(preview.value("minimumSize").toSizeF());
        if (!creations.isEmpty() && (rect.left() < position.x() || rect.right() > bounds.right()))
        {
            position = QPointF(qMax(bounds.left(), logicalPosition.x()), position.y() + rowHeight);
            rowHeight = 0;
            preview = createPreflight(targetID, type, QRectF(position, rect.size()), page, true, function->name());
            if (!preview.value("accepted").toBool())
                return preview;
            rect = preview.value("rect").toRectF();
        }
        for (const auto &existing : creations)
            if (rect.intersects(existing.second))
                return editResult(false, false, tr("Insufficient content space for all dropped Functions"));
        creations.append(qMakePair(function->id(), rect));
        rowHeight = qMax(rowHeight, rect.height());
        position = QPointF(rect.right() + qMax(qreal(1), snappingSize()), rect.top());
    }
    /* an arranged frame places the new controls itself, when all of them fit */
    const QString arranged = m_editor->arrangedFit(target, page, sizes, minimums);
    if (!arranged.isEmpty())
        return editResult(false, false, arranged);
    QScopedValueRollback<bool> busy(m_authoringBusy, true);
    emit structureAboutToChange();
    QSignalBlocker blockOwner(this), blockParent(target);
    Tardis *tardis = Tardis::instance();
    if (tardis) tardis->beginBatch("consoleFunctionDrop");
    QMap<quint32, QRectF> rectangles;
    for (const auto &creation : creations)
    {
        VCWidget *created = commitWidgetCreation(target, type, creation.second, page, creation.first);
        Q_ASSERT(created);
        rectangles.insert(created->id(), creation.second);
    }
    m_editor->arrangeJoined(target);
    if (tardis) tardis->endBatch();
    m_doc->setModified();
    blockOwner.unblock();
    blockParent.unblock();
    emit target->childOrderChanged(page);
    emit widgetsChanged();
    QVariantMap result = editResult(true, true, {}, rectangles);
    emit structureChanged(result.value("widgetIDs").toList());
    return result;
}

QVariantMap VirtualConsole::previewSelectionDrop(QVariantList ids, quint32 anchorID, quint32 targetID,
                                                 int subpage, QPointF logicalAnchorTopLeft)
{
    return selectionDrop(ids, anchorID, targetID, subpage, logicalAnchorTopLeft, true);
}

QVariantMap VirtualConsole::editSelectionDrop(QVariantList ids, quint32 anchorID, quint32 targetID,
                                              int subpage, QPointF logicalAnchorTopLeft)
{
    return selectionDrop(ids, anchorID, targetID, subpage, logicalAnchorTopLeft, false);
}

QVariantMap VirtualConsole::selectionDrop(const QVariantList &ids, quint32 anchorID, quint32 targetID,
                                          int subpage, QPointF logicalAnchorTopLeft, bool preview)
{
    QString error;
    const QList<VCWidget *> roots = selectedRoots(ids, &error);
    VCFrame *target = targetFrame(targetID);
    VCWidget *anchor = widget(anchorID);
    if (!error.isEmpty())
        return editResult(false, false, error);
    if (!target || !anchor || roots.isEmpty() || subpage < 0 || subpage >= target->totalPagesNumber() ||
        !qIsFinite(logicalAnchorTopLeft.x()) || !qIsFinite(logicalAnchorTopLeft.y()))
        return editResult(false, false, tr("Invalid selection drop target or anchor"));
    VCWidget *anchorRoot = nullptr;
    for (QObject *ancestor = anchor; ancestor; ancestor = ancestor->parent())
        if (roots.contains(qobject_cast<VCWidget *>(ancestor)))
            anchorRoot = qobject_cast<VCWidget *>(ancestor);
    if (!anchorRoot)
        return editResult(false, false, tr("Drag anchor is not in the selected roots"));
    QObject *sourceParent = roots.first()->parent();
    const int sourcePage = roots.first()->page();
    QRectF occupied = roots.first()->logicalGeometry();
    for (VCWidget *root : roots)
    {
        error = nativeAuthoringError(root);
        if (!error.isEmpty())
            return editResult(false, false, error);
        if (root->parent() != sourceParent || root->page() != sourcePage)
            return editResult(false, false, tr("Move a selection from one parent and subpage at a time"));
        for (QObject *ancestor = target; ancestor; ancestor = ancestor->parent())
            if (ancestor == root)
            {
                QVariantMap result = editResult(false, false, tr("Cannot move a frame into itself or a descendant"));
                result["targetEligible"] = false;
                return result;
            }
        occupied = occupied.united(root->logicalGeometry());
    }
    error = nativeAuthoringError(target);
    if (!error.isEmpty())
        return editResult(false, false, error);
    QPointF anchorOffset;
    for (VCWidget *entry = anchor; entry && entry != anchorRoot;
         entry = qobject_cast<VCWidget *>(entry->parent()))
        anchorOffset += entry->logicalGeometry().topLeft();
    const QRectF bounds = target->contentRect();
    const QPointF oldAnchor = anchorRoot->logicalGeometry().topLeft();
    QPointF desiredAnchor = logicalAnchorTopLeft - anchorOffset;
    const qreal grid = snapping() ? snappingSize() : 0;
    const qreal minX = oldAnchor.x() + bounds.left() - occupied.left();
    const qreal maxX = oldAnchor.x() + bounds.right() - occupied.right();
    const qreal minY = oldAnchor.y() + bounds.top() - occupied.top();
    const qreal maxY = oldAnchor.y() + bounds.bottom() - occupied.bottom();
    auto bounded = [grid](qreal value, qreal low, qreal high, bool *valid) {
        if (grid > 0)
        {
            low = std::ceil(low / grid) * grid;
            high = std::floor(high / grid) * grid;
            value = std::round(value / grid) * grid;
        }
        if (high < low) { *valid = false; return qreal(0); }
        return qBound(low, value, high);
    };
    bool fits = true;
    desiredAnchor.setX(bounded(desiredAnchor.x(), minX, maxX, &fits));
    desiredAnchor.setY(bounded(desiredAnchor.y(), minY, maxY, &fits));
    if (!fits)
        return editResult(false, false, tr("Selected group cannot fit the target content and snap grid"));
    const QPointF delta = desiredAnchor - oldAnchor;
    QMap<quint32, QRectF> rectangles;
    for (VCWidget *root : roots)
        rectangles.insert(root->id(), root->logicalGeometry().translated(delta));
    if (sourceParent == target && sourcePage == subpage)
    {
        error = geometryError(rectangles);
        if (!error.isEmpty())
            return editResult(false, false, error);
        return preview ? editResult(true, false, {}, rectangles) : applyGeometryPlan(rectangles);
    }
    return structureEdit(ids, targetID, subpage, -1, rectangles, preview);
}

QVariantMap VirtualConsole::revealWidget(quint32 id)
{
    VCWidget *entry = widget(id);
    if (!entry)
        entry = targetFrame(id);
    if (!entry)
        return editResult(false, false, tr("Unknown widget %1").arg(id));
    QList<VCFrame *> ancestors;
    VCPage *owningPage = qobject_cast<VCPage *>(entry);
    for (QObject *owner = entry->parent(); owner; owner = owner->parent())
        if (VCFrame *frame = qobject_cast<VCFrame *>(owner))
        {
            ancestors.prepend(frame);
            if (VCPage *page = qobject_cast<VCPage *>(frame))
                owningPage = page;
        }
    if (!owningPage)
        return editResult(false, false, tr("Widget is not attached to a console page"));
    const QString accessError = nativeAuthoringError(entry);
    if (!accessError.isEmpty())
        return editResult(false, false, accessError);
    if (owningPage->previewContext()->detached() &&
        (!owningPage->previewContext()->view() || !owningPage->previewContext()->view()->rootObject()))
        return editResult(false, false, tr("Detached page view is unavailable"));
    setSelectedPage(m_pages.indexOf(owningPage));
    VCWidget *child = entry;
    QList<QPair<QPointer<VCFrame>, int>> scope;
    for (auto it = ancestors.crbegin(); it != ancestors.crend(); ++it)
    {
        (*it)->setCollapsed(false);
        (*it)->setCurrentPage(child->page());
        scope.append({QPointer<VCFrame>(*it), child->page()});
        child = *it;
    }
    QRectF pageRect = entry->logicalGeometry();
    for (QObject *owner = entry->parent(); owner && owner != owningPage; owner = owner->parent())
        if (VCWidget *ancestor = qobject_cast<VCWidget *>(owner))
            pageRect.translate(ancestor->logicalGeometry().topLeft());
    QPointer<VCWidget> guarded(entry);
    QPointer<VCPage> requestedPage(owningPage);
    QPointer<QQuickItem> requestedView(currentPageItem());
    const QString context = m_contextManager ? m_contextManager->currentContext() : QString();
    const qreal scale = owningPage->pageScale();
    QTimer::singleShot(0, this, [this, guarded, requestedPage, requestedView, context, scope, pageRect, scale]() {
        QTimer::singleShot(0, this, [this, guarded, requestedPage, requestedView, context, scope, pageRect, scale]() {
            if (!guarded || !requestedPage || page(m_selectedPage) != requestedPage ||
                (m_contextManager && m_contextManager->currentContext() != context))
                return;
            QObject *child = guarded;
            for (const auto &location : scope)
            {
                if (!location.first || child->parent() != location.first ||
                    location.first->currentPage() != location.second || location.first->isCollapsed())
                    return;
                child = location.first;
            }
            if (child != requestedPage)
                return;
            QQuickItem *view = currentPageItem();
            if (!view)
            {
                emit editRejected(tr("Console page view is unavailable"));
                return;
            }
            if (view != requestedView)
                return;
            const QRectF displayed(pageRect.topLeft() * scale, pageRect.size() * scale);
            qreal x = view->property("contentX").toReal(), y = view->property("contentY").toReal();
            if (displayed.left() < x) x = displayed.left();
            else if (displayed.right() > x + view->width()) x = displayed.right() - view->width();
            if (displayed.top() < y) y = displayed.top();
            else if (displayed.bottom() > y + view->height()) y = displayed.bottom() - view->height();
            view->setProperty("contentX", qMax(qreal(0), x));
            view->setProperty("contentY", qMax(qreal(0), y));
        });
    });
    QVariantMap result = editResult(true, true, {});
    result["pageIndex"] = m_pages.indexOf(owningPage);
    result["rect"] = pageRect;
    return result;
}

QVariantMap VirtualConsole::revealFramePage(quint32 frameID, int subpage)
{
    VCFrame *frame = targetFrame(frameID);
    if (!frame || subpage < 0 || subpage >= frame->totalPagesNumber())
        return editResult(false, false, tr("Invalid frame subpage"));
    QVariantMap result = revealWidget(frameID);
    if (result.value("accepted").toBool())
    {
        frame->setCollapsed(false);
        frame->setCurrentPage(subpage);
    }
    return result;
}

QQuickItem *VirtualConsole::popupRoot(QQuickItem *anchor) const
{
    Q_UNUSED(anchor)
    return m_mainView ? m_mainView->rootObject() : nullptr;
}

QVariantMap VirtualConsole::snapGuide(quint32 frameID) const
{
    VCFrame *frame = targetFrame(frameID);
    if (!frame)
        return {{"error", tr("Unknown snap target frame %1").arg(frameID)}};
    return {{"origin", QPointF(0, 0)}, {"step", QSizeF(snappingSize(), snappingSize())},
            {"bounds", frame->contentRect()}, {"enabled", snapping() && snappingSize() > 0}};
}

void VirtualConsole::setWidgetsAlignment(VCWidget *refWidget, int alignment)
{
    if (refWidget == nullptr)
        return;

    QRectF refGeom = refWidget->logicalGeometry();
    QMap<quint32, QRectF> rectangles;

    QMapIterator<quint32, QQuickItem*> it(m_itemsMap);
    while (it.hasNext())
    {
        it.next();

        VCWidget *widget = m_widgetsMap[it.key()];
        const QString error = nativeAuthoringError(widget);
        if (!error.isEmpty())
        {
            reportEdit(editResult(false, false, error));
            return;
        }
        if (widget->parent() != refWidget->parent() || widget->page() != refWidget->page())
        {
            reportEdit(editResult(false, false, tr("Align a selection from one parent and subpage at a time")));
            return;
        }
        QRectF wGeom = widget->logicalGeometry();

        switch(alignment)
        {
            case Qt::AlignTop:
                wGeom.moveTop(refGeom.y());
            break;
            case Qt::AlignLeft:
                wGeom.moveLeft(refGeom.x());
            break;
            case Qt::AlignRight:
            {
                // TODO: for now, let's do an ignorant alignment, without considering
                // that widgets can be nested into VC frames...
                const qreal right = refGeom.x() + refGeom.width();
                wGeom.moveLeft(right - wGeom.width());
            }
            break;
            case Qt::AlignBottom:
            {
                // TODO: for now, let's do an ignorant alignment, without considering
                // that widgets can be nested into VC frames...
                const qreal bottom = refGeom.y() + refGeom.height();
                wGeom.moveTop(bottom - wGeom.height());
            }
            break;
            case Qt::AlignHCenter:
                wGeom.moveLeft(refGeom.center().x() - wGeom.width() / 2);
            break;
            case Qt::AlignVCenter:
                wGeom.moveTop(refGeom.center().y() - wGeom.height() / 2);
            break;
        }
        rectangles.insert(widget->id(), wGeom);
    }
    reportEdit(applyGeometryPlan(rectangles));
}

void VirtualConsole::setWidgetsDistribution(int direction)
{
    if (m_itemsMap.count() < 3)
        return;

    qreal min = 1000000;
    qreal max = 0;
    qreal widgetsSize = 0;
    qreal gap = 0;
    QVector<VCWidget *> sortedWidgets;
    QVector<qreal> sortedPos;

    /* cycle through selected widgets and do the following:
     * 1- calculate the total width/height
     * 2- sort the widgets from the leftmost/topmost item
     * 3- detect the minimum and maximum items position
     */
    QMapIterator<quint32, QQuickItem*> it(m_itemsMap);
    while (it.hasNext())
    {
        it.next();

        VCWidget *widget = m_widgetsMap[it.key()];
        const QString error = nativeAuthoringError(widget);
        if (!error.isEmpty())
        {
            reportEdit(editResult(false, false, error));
            return;
        }
        if (!sortedWidgets.isEmpty() &&
            (widget->parent() != sortedWidgets.first()->parent() || widget->page() != sortedWidgets.first()->page()))
        {
            reportEdit(editResult(false, false, tr("Distribute a selection from one parent and subpage at a time")));
            return;
        }
        QRectF wGeom = widget->logicalGeometry();
        qreal pos = direction == Qt::Horizontal ? wGeom.x() : wGeom.y();
        qreal size = direction == Qt::Horizontal ? wGeom.width() : wGeom.height();
        int i = 0;

        // 1
        widgetsSize += size;

        // 2
        for (i = 0; i < sortedPos.count(); i++)
        {
            if (pos < sortedPos[i])
                break;
        }
        if (sortedPos.isEmpty() || i == sortedWidgets.count())
        {
            sortedWidgets.append(widget);
            sortedPos.append(pos);
        }
        else
        {
            sortedWidgets.insert(i, widget);
            sortedPos.insert(i, pos);
        }

        // 3
        if (pos + size > max)
            max = pos + size;
        if (pos < min)
            min = pos;
    }

    gap = ((max - min) - widgetsSize) / (sortedWidgets.count() - 1);

    qreal newPos = min;
    QMap<quint32, QRectF> rectangles;

    for (int idx = 0; idx < sortedWidgets.count(); idx++)
    {
        VCWidget *widget = sortedWidgets[idx];
        QRectF wGeom = widget->logicalGeometry();
        qreal size = direction == Qt::Horizontal ? wGeom.width() : wGeom.height();

        // the first and last widget don't need any adjustment
        if (idx > 0 && idx < sortedWidgets.count() - 1)
        {
            if (direction == Qt::Horizontal)
                wGeom.moveLeft(newPos);
            else
                wGeom.moveTop(newPos);
        }

        newPos += size + gap;
        rectangles.insert(widget->id(), wGeom);
    }
    reportEdit(applyGeometryPlan(rectangles));
}

void VirtualConsole::setWidgetsCaption(QString caption)
{
    QMapIterator<quint32, QQuickItem*> it(m_itemsMap);
    while (it.hasNext())
    {
        it.next();

        VCWidget *widget = m_widgetsMap[it.key()];
        widget->setCaption(caption);
    }
}

void VirtualConsole::setWidgetsForegroundColor(QColor color)
{
    QMapIterator<quint32, QQuickItem*> it(m_itemsMap);
    while (it.hasNext())
    {
        it.next();

        VCWidget *widget = m_widgetsMap[it.key()];
        widget->setForegroundColor(color);
    }
}

void VirtualConsole::setWidgetsBackgroundColor(QColor color)
{
    QMapIterator<quint32, QQuickItem*> it(m_itemsMap);
    while (it.hasNext())
    {
        it.next();

        VCWidget *widget = m_widgetsMap[it.key()];
        widget->setBackgroundColor(color);
    }
}

void VirtualConsole::setWidgetsBackgroundImage(QString path)
{
    QMapIterator<quint32, QQuickItem*> it(m_itemsMap);
    while (it.hasNext())
    {
        it.next();

        VCWidget *widget = m_widgetsMap[it.key()];
        widget->setBackgroundImage(path);
    }
}

void VirtualConsole::setWidgetsFont(QFont font)
{
    QMapIterator<quint32, QQuickItem*> it(m_itemsMap);
    while (it.hasNext())
    {
        it.next();

        VCWidget *widget = m_widgetsMap[it.key()];
        widget->setFont(font);
    }
}

void VirtualConsole::deleteVCWidgets(QVariantList IDList)
{
    if (m_authoringBusy)
    {
        qWarning() << Q_FUNC_INFO << "Deletion refused during another console commit";
        return;
    }
    m_editor->deleteWidgets(IDList);
}

void VirtualConsole::commitWidgetDeletion(const QVariantList &IDList)
{
    QScopedValueRollback<bool> busy(m_authoringBusy, true);
    cancelLayout();
    foreach (QVariant id, IDList)
    {
        quint32 wID = id.toUInt();
        VCWidget *w = widget(wID);
        if (w == nullptr)
            continue;

        /* 1- remove the widget from its parent frame page map, remembering
         *    its place among the siblings so undo can put it back there */
        VCFrame *parentFrame = qobject_cast<VCFrame *>(w->parent());
        QVariantMap location;
        if (parentFrame != nullptr)
        {
            location = {{"widgetID", wID}, {"subpage", w->page()},
                        {"orderIndex", parentFrame->orderedChildIDs(w->page()).indexOf(wID)}};
            parentFrame->removeWidgetFromPageMap(w);
        }

        /* 2- if the widget is a frame, delete also all its children.
         *    The frame XML saved below holds them, so they need no undo action of their own */
        if (w->type() == VCWidget::FrameWidget || w->type() == VCWidget::SoloFrameWidget)
        {
            VCFrame *frame = qobject_cast<VCFrame *>(w);
            for (VCWidget *child : frame->children(true))
            {
                /* Remove any input source/key sequence binding pointing to the
                 * child, otherwise the page maps would keep dangling pointers */
                for (VCPage *page : m_pages)
                    page->removeWidgetFromMaps(child);
                removeWidgetFromMap(child);
            }
        }

        /* Remove any input source/key sequence binding pointing to the widget,
         * otherwise the page maps would keep dangling pointers to freed memory */
        for (VCPage *page : m_pages)
            page->removeWidgetFromMaps(w);

        /* 3- remove the widget from the global VC widgets map */
        VCFrame *parent = qobject_cast<VCFrame *>(w->parent());
        Tardis::instance()->enqueueAction(Tardis::VCWidgetDelete, parent->id(),
                                          Tardis::instance()->actionToByteArray(Tardis::VCWidgetDelete, w->id()),
                                          location);
        removeWidgetFromMap(w);

        /* 4- perform the actual widget deletion */
        delete w;
    }
    if (IDList.isEmpty() == false)
        emit widgetsChanged();
}

QString VirtualConsole::nativeAuthoringError(VCWidget *entry) const
{
    if (App *app = qobject_cast<App *>(m_mainView))
        if (!(app->accessMask() & App::AC_VCEditing))
            return tr("Virtual Console editing is unavailable");
    for (QObject *owner = entry; owner; owner = owner->parent())
        if (VCPage *page = qobject_cast<VCPage *>(owner))
            if (page->requirePIN() && m_unlockedPagePINs.value(page->id(), -1) != page->PIN())
                return tr("Unlock the protected console page before editing");
    return {};
}

void VirtualConsole::reportEdit(const QVariantMap &result)
{
    if (!result.value("accepted").toBool())
    {
        const QString error = result.value("error").toString();
        qWarning() << Q_FUNC_INFO << error;
        emit editRejected(error);
    }
}

QVariantMap VirtualConsole::deleteAuthoringWidgets(QVariantList ids)
{
    if (m_authoringBusy)
        return editResult(false, false, tr("Another console edit is being committed"));
    QString error;
    const QList<VCWidget *> roots = selectedRoots(ids, &error);
    if (!error.isEmpty())
        return editResult(false, false, error);
    QVariantList rootIDs;
    for (VCWidget *root : roots)
    {
        error = nativeAuthoringError(root);
        if (!error.isEmpty())
            return editResult(false, false, error);
        rootIDs.append(root->id());
    }
    deleteVCWidgets(rootIDs);
    QVariantMap result = editResult(true, !roots.isEmpty(), {});
    result["widgetIDs"] = rootIDs;
    return result;
}

void VirtualConsole::selectAll()
{
    VCFrame *page = qobject_cast<VCFrame *>(this->page(m_selectedPage));
    if (page == nullptr)
        return;
    QList<VCWidget *> children = page->children(false);
    for (VCWidget *child : children)
    {
        QQuickItem *item = qobject_cast<QQuickItem *>(m_view->rootObject()->findChild<QObject *>(
            QString("vcWidget_%1").arg(child->id())));
        setWidgetSelection(child->id(), item, true, true);
    }
}

void VirtualConsole::duplicateSelection()
{
    if (m_itemsMap.isEmpty())
        return;

    m_editor->duplicateSelection();
}

void VirtualConsole::nudgeWidgets(int dx, int dy)
{
    if (m_itemsMap.isEmpty())
        return;

    QString error;
    const QList<VCWidget *> roots = selectedRoots(selectedWidgetIDs(), &error);
    if (!error.isEmpty() || roots.isEmpty())
    {
        reportEdit(editResult(false, false, error.isEmpty() ? tr("No widgets selected") : error));
        return;
    }
    VCWidget *anchor = roots.first();
    VCFrame *parent = qobject_cast<VCFrame *>(anchor->parent());
    const qreal step = snapping() && snappingSize() > 0 ? snappingSize() : 1;
    reportEdit(editSelectionDrop(selectedWidgetIDs(), anchor->id(), parent->id(), anchor->page(),
                                 anchor->logicalGeometry().topLeft() + QPointF(dx * step, dy * step)));
}

void VirtualConsole::autoLayoutPage()
{
    VCPage *page = this->page(m_selectedPage);
    if (page == nullptr)
        return;

    Tardis::instance()->beginBatch("autoLayoutPage");

    VCBridge::WidgetSnapshot snap = VCLayoutAdapter::snapshotFrame(page);
    VCBridge::ReflowOptions opts;
    opts.pad = snappingSize();
    opts.gridSize = snappingSize();

    if (page->layoutMode() == VCFrame::LayoutGrid)
    {
        int cellW = GridLayout::cellWidth((int)page->geometry().width(), page->gridColumns());
        int cellH = page->gridRowHeight() > 0 ? page->gridRowHeight() : snappingSize();
        if (cellW <= 0)
            cellW = snappingSize() > 0 ? snappingSize() : 20;
        if (cellH <= 0)
            cellH = 20;

        QVector<GridLayout::GridItem> gridItems;
        gridItems.reserve(snap.children.size());
        for (const auto &child : snap.children)
        {
            GridLayout::GridItem gi;
            gi.id = child.id;
            gi.cell = GridLayout::pixelsToCells(child.geometry, cellW, cellH);
            gridItems.append(gi);
        }

        gridItems = GridLayout::compactVertical(gridItems);

        VCBridge::LayoutPlan plan;
        for (const auto &gi : gridItems)
        {
            QRect pixelRect = GridLayout::cellsToPixels(gi.cell, cellW, cellH);
            // Preserve original width/height in pixels — don't force to cell multiples
            auto origIt = std::find_if(snap.children.begin(), snap.children.end(),
                [&](const VCBridge::WidgetSnapshot &s) { return s.id == gi.id; });
            if (origIt != snap.children.end())
            {
                pixelRect.setWidth(origIt->geometry.width());
                pixelRect.setHeight(origIt->geometry.height());
            }
            plan.geometries.insert(gi.id, pixelRect);
        }
        VCLayoutAdapter::applyPlan(this, plan);
    }
    else
    {
        VCBridge::LayoutPlan plan = VCBridge::reflowPage(snap, opts);
        VCLayoutAdapter::applyPlan(this, plan);
    }

    Tardis::instance()->endBatch();
}

#if 0
// Previous flow-layout implementation — kept for reference. Superseded by the
// unified VCBridge::reflowPage() algorithm shared with the MCP reflow tool.
void VirtualConsole::autoLayoutPage()
{
    if (m_selectedPage >= m_pages.count())
        return;

    VCFrame *page = m_pages.at(m_selectedPage);
    QList<VCWidget *> children = page->children(false);
    if (children.isEmpty())
        return;

    // Group widgets by type: buttons, sliders, then others
    QList<VCWidget *> buttons, sliders, others;
    for (VCWidget *w : children)
    {
        if (w->type() == VCWidget::ButtonWidget)
            buttons.append(w);
        else if (w->type() == VCWidget::SliderWidget)
            sliders.append(w);
        else
            others.append(w);
    }

    qreal padding = snappingSize();
    qreal pageWidth = page->geometry().width();
    qreal curY = padding;

    auto flowLayout = [&](QList<VCWidget *> &widgets) {
        if (widgets.isEmpty()) return;
        qreal curX = padding;
        qreal rowMaxH = 0;

        for (VCWidget *w : widgets)
        {
            QRectF geo = w->geometry();
            if (curX + geo.width() > pageWidth - padding && curX > padding)
            {
                curX = padding;
                curY += rowMaxH + padding;
                rowMaxH = 0;
            }
            geo.moveTo(qRound(curX), qRound(curY));
            w->setGeometry(geo);

            QQuickItem *item = qobject_cast<QQuickItem *>(m_view->rootObject()->findChild<QObject *>(
                QString("vcWidget_%1").arg(w->id())));
            if (item)
            {
                item->setX(geo.x());
                item->setY(geo.y());
            }

            curX += geo.width() + padding;
            rowMaxH = qMax(rowMaxH, (qreal)geo.height());
        }
        curY += rowMaxH + padding;
    };

    Tardis::instance()->beginBatch("autoLayoutPage");
    flowLayout(buttons);
    flowLayout(sliders);
    flowLayout(others);
    Tardis::instance()->endBatch();
}
#endif

VCWidget *VirtualConsole::selectedWidget() const
{
    VCWidget *selected = m_itemsMap.isEmpty() ? qobject_cast<VCWidget *>(page(m_selectedPage))
                         : widget(m_itemsMap.contains(m_lastSelectedID) ? m_lastSelectedID : m_itemsMap.firstKey());
    return nativeAuthoringError(selected).isEmpty() ? selected : nullptr;
}

void VirtualConsole::requestAddMatrixPopup(VCFrame *frame, QQuickItem *parent, QString widgetType, QPoint pos)
{
    QQuickItem *vcItem = qobject_cast<QQuickItem*>(m_view->rootObject()->findChild<QObject *>("virtualConsole"));
    if (vcItem == nullptr)
        return;

    QMetaObject::invokeMethod(vcItem, "requestMatrixPopup",
            Q_ARG(QVariant, QVariant::fromValue(frame)),
            Q_ARG(QVariant, QVariant::fromValue(parent)),
            Q_ARG(QVariant, widgetType),
            Q_ARG(QVariant, pos));
}

QString VirtualConsole::widgetIcon(int type) const
{
    switch (type)
    {
        case VCWidget::ButtonWidget: return "qrc:/button.svg";
        case VCWidget::SliderWidget: return "qrc:/slider.svg";
        case VCWidget::XYPadWidget: return "qrc:/xypad.svg";
        case VCWidget::FrameWidget: return "qrc:/frame.svg";
        case VCWidget::SoloFrameWidget: return "qrc:/soloframe.svg";
        case VCWidget::SpeedWidget: return "qrc:/speed.svg";
        case VCWidget::CueListWidget: return "qrc:/cuelist.svg";
        case VCWidget::LabelWidget: return "qrc:/label.svg";
        case VCWidget::AudioTriggersWidget: return "qrc:/audiotriggers.svg";
        case VCWidget::AnimationWidget: return "qrc:/animation.svg";
        case VCWidget::ClockWidget: return "qrc:/clock.svg";
        default:
            qDebug() << "Unhandled widget type" << type << ". FIXME";
        break;
    }

    return "";
}


/*********************************************************************
 * Clipboard
 *********************************************************************/

void VirtualConsole::copyToClipboard()
{
    m_clipboardIDList.clear();
    for (quint32 wID : m_itemsMap.keys())
        m_clipboardIDList.append(wID);

    m_clipboardIsCut = false;

    emit clipboardItemsCountChanged();
}

void VirtualConsole::cutToClipboard()
{
    m_clipboardIDList.clear();
    for (quint32 wID : m_itemsMap.keys())
        m_clipboardIDList.append(wID);

    m_clipboardIsCut = true;

    emit clipboardItemsCountChanged();
}

void VirtualConsole::pasteFromClipboard()
{
    VCFrame *frame = nullptr;

    QMapIterator<quint32, QQuickItem*> it(m_itemsMap);
    while (it.hasNext())
    {
        it.next();

        VCWidget *widget = m_widgetsMap[it.key()];
        if (widget->type() == VCWidget::FrameWidget ||
            widget->type() == VCWidget::SoloFrameWidget)
        {
            frame = qobject_cast<VCFrame*>(widget);
            break;
        }
    }

    // no selected frame found ? Paste on current page
    if (frame == nullptr)
    {
        frame = qobject_cast<VCFrame*>(page(selectedPage()));
        if (frame == nullptr)
            return;
    }

    // like created widgets, copies go into the drop area above the frame chrome
    QQuickItem *renderParent = frame->renderItem() ?
        frame->renderItem()->findChild<QQuickItem *>(QString("frameDropArea%1").arg(frame->id())) : nullptr;

    reportEdit(pasteWidgets(m_clipboardIDList, frame->id(), frame->contentRect().topLeft(), renderParent, true));
}

QVariantMap VirtualConsole::pasteWidgets(const QVariantList &ids, quint32 targetID,
                                         QPointF logicalPosition, QQuickItem *renderParent, bool nearPosition)
{
    if (m_authoringBusy)
        return editResult(false, false, tr("Another console edit is being committed"));
    VCFrame *target = targetFrame(targetID);
    if (!target)
        return editResult(false, false, tr("Unknown target frame"));
    QString error = nativeAuthoringError(target);
    if (!error.isEmpty())
        return editResult(false, false, error);
    const QList<VCWidget *> roots = selectedRoots(ids, &error);
    if (!error.isEmpty())
        return editResult(false, false, error);
    if (roots.isEmpty())
        return editResult(false, false, tr("No widgets to paste"));
    if (m_clipboardIsCut && ids != m_clipboardIDList)
        return editResult(false, false, tr("The cut clipboard changed; select its current widgets again"));
    const int subpage = target->currentPage();
    const QRectF bounds = target->contentRect();
    const QVariantMap guide = snapGuide(targetID);
    const qreal step = guide.value("enabled").toBool() ? guide.value("step").toSizeF().width() : 0;
    QPointF position = logicalPosition;
    qreal rowHeight = 0;
    QMap<quint32, QRectF> rectangles;
    for (VCWidget *root : roots)
    {
        error = nativeAuthoringError(root);
        if (!error.isEmpty())
            return editResult(false, false, error);
        for (QObject *ancestor = target; ancestor; ancestor = ancestor->parent())
            if (ancestor == root)
                return editResult(false, false, tr("Cannot paste a frame into itself or its descendants"));
        if (root->type() == VCWidget::UnknownWidget)
            return editResult(false, false, tr("Cannot copy an unknown widget type"));
        if (VCFrame *frame = qobject_cast<VCFrame *>(root))
        {
            GridLayout::LayoutSnapshot snapshot = VCLayoutAdapter::capture(frame);
            for (auto &container : snapshot.containers)
                for (auto &page : container.pages)
                    page.snappingEnabled = false;
            GridLayout::LayoutPlan identity;
            for (const auto &item : snapshot.items)
                identity.placements.append({item.id, item.parentId, item.subpage, item.order, item.rect});
            const auto diagnostic = GridLayout::validate(snapshot, identity, {});
            if (!diagnostic.code.isEmpty())
                return editResult(false, false, diagnostic.message);
        }
        if (step > 0)
        {
            position.setX(std::round(position.x() / step) * step);
            position.setY(std::round(position.y() / step) * step);
        }
        QRectF rect = root->logicalGeometry();
        rect.moveTopLeft(position);
        rectangles.insert(root->id(), rect);
        rowHeight = qMax(rowHeight, rect.height());
        position.setX(rect.right());
        if (position.x() >= bounds.right())
        {
            position = QPointF(logicalPosition.x(), position.y() + rowHeight);
            rowHeight = 0;
        }
    }
    const bool arranged = m_editor->isArranged(target);
    error = arranged ? m_editor->arrangedFit(target, subpage, rectangles)
                     : geometryError(rectangles, target, subpage, true);
    // a free spot does not help an arranged frame that has no room left
    if (!error.isEmpty() && nearPosition && !arranged && m_editor->placeNear(rectangles, target, subpage, step))
        error.clear();
    if (!error.isEmpty())
        return editResult(false, false, error);

    QScopedValueRollback<bool> busy(m_authoringBusy, true);
    cancelLayout();
    emit structureAboutToChange();
    QSignalBlocker blockOwner(this), blockDoc(m_doc);
    QSet<VCFrame *> parents{target};
    for (VCWidget *root : roots)
        parents.insert(qobject_cast<VCFrame *>(root->parent()));
    std::vector<std::unique_ptr<QSignalBlocker>> parentBlockers;
    for (VCFrame *frame : parents)
        if (frame)
            parentBlockers.emplace_back(new QSignalBlocker(frame));
    const bool wasModified = m_doc->isModified();
    const quint32 previousID = m_latestWidgetId;
    const QVariantList previousSelection = selectedWidgetIDs();
    QMap<quint32, QRectF> copies;
    QList<VCWidget *> created;
    const qreal scale = qobject_cast<VCPage *>(target)
        ? qobject_cast<VCPage *>(target)->pageScale() : target->scaleFactor();
    {
        QScopedValueRollback<LoadStatus> staging(m_loadStatus, Loading);
        for (VCWidget *root : roots)
        {
            VCWidget *copy = root->createCopy(target);
            if (!copy)
            {
                for (VCWidget *entry : created)
                {
                    target->removeWidgetFromPageMap(entry);
                    removeWidgetFromMap(entry);
                    delete entry;
                }
                m_latestWidgetId = previousID;
                if (!wasModified)
                    m_doc->resetModified();
                qCritical() << Q_FUNC_INFO << "Admitted widget could not be copied" << root->id();
                return editResult(false, false, tr("Cannot copy widget %1").arg(root->id()));
            }
            QQmlEngine::setObjectOwnership(copy, QQmlEngine::CppOwnership);
            const QRectF rect = rectangles.value(root->id());
            copy->setLogicalGeometry(rect);
            copy->setPage(subpage);
            copy->remapInputSources(subpage);
            copy->setScaleFactor(scale);
            if (VCFrame *frame = qobject_cast<VCFrame *>(copy))
                for (VCWidget *child : frame->children(true))
                    child->setScaleFactor(scale);
            copy->setVisible(subpage == target->currentPage());
            addWidgetToMap(copy);
            target->addWidgetToPageMap(copy);
            created.append(copy);
            copies.insert(copy->id(), rect);
        }
    }
    Tardis *tardis = Tardis::instance();
    if (tardis)
    {
        tardis->beginBatch("consolePaste");
        for (VCWidget *copy : created)
            tardis->enqueueAction(Tardis::VCWidgetCreate, targetID, {},
                                  tardis->actionToByteArray(Tardis::VCWidgetCreate, copy->id()));
    }
    if (m_clipboardIsCut)
    {
        QVariantList sourceIDs;
        for (VCWidget *root : roots)
            sourceIDs.append(root->id());
        commitWidgetDeletion(sourceIDs);
        m_clipboardIDList.clear();
        m_clipboardIsCut = false;
    }
    for (VCWidget *entry : target->reconcileSubmasters(created))
        entry->adjustIntensity(target->m_submasterValue * target->intensity());
    for (VCWidget *copy : created)
    {
        if (VCFrame *frame = qobject_cast<VCFrame *>(copy))
            frame->refreshFunctionRouting();
        copy->render(m_view, renderParent);
    }
    for (VCPage *page : m_pages)
    {
        page->resetInputSourcesMap();
        for (const auto &source : page->inputSources())
            page->mapInputSource(source, page, false);
        page->mapChildrenInputSources();
        page->buildKeySequenceMap();
    }
    updatePageInputs();
    m_editor->arrangeJoined(target);
    if (tardis)
        tardis->endBatch();
    parentBlockers.clear();
    blockOwner.unblock();
    blockDoc.unblock();
    m_doc->setModified();
    for (VCFrame *frame : parents)
        if (frame)
            for (int page = 0; page < frame->totalPagesNumber(); ++page)
                emit frame->childOrderChanged(page);
    if (previousSelection != selectedWidgetIDs())
    {
        emit selectedWidgetChanged();
        emit selectedWidgetsCountChanged();
    }
    /* go on working with what was pasted */
    setSelectedWidgetIDs(QVariantList(copies.keyBegin(), copies.keyEnd()));
    emit clipboardItemsCountChanged();
    emit widgetsChanged();
    QVariantMap result = editResult(true, true, {}, copies);
    emit structureChanged(result.value("widgetIDs").toList());
    return result;
}

QVariantList VirtualConsole::clipboardItemsList() const
{
    return m_clipboardIDList;
}

int VirtualConsole::clipboardItemsCount() const
{
    int count = 0;

    // count the clipboard items, including the children of the
    // frames, since those are pasted along with their parent
    for (QVariant wID : m_clipboardIDList)
    {
        VCWidget *w = widget(wID.toUInt());
        if (w == nullptr)
            continue;

        count++;

        if (w->type() == VCWidget::FrameWidget ||
            w->type() == VCWidget::SoloFrameWidget)
        {
            VCFrame *frame = qobject_cast<VCFrame *>(w);
            count += frame->children(true).count();
        }
    }

    return count;
}

bool VirtualConsole::clipboardIsCut() const
{
    return m_clipboardIsCut;
}

void VirtualConsole::flushClipboardAfterPaste(quint32 targetFrameID)
{
    if (m_clipboardIsCut == false)
        return;

    // a cut/paste: remove the source widgets and empty the clipboard.
    // Never delete the frame the widgets have been pasted into
    QVariantList toDelete;
    for (QVariant wID : m_clipboardIDList)
    {
        if (wID.toUInt() == targetFrameID)
            continue;
        toDelete.append(wID);
    }

    deleteVCWidgets(toDelete);

    m_clipboardIDList.clear();
    m_clipboardIsCut = false;
    emit clipboardItemsCountChanged();
}

/*********************************************************************
 * External input
 *********************************************************************/

bool VirtualConsole::createAndDetectInputSource(VCWidget *widget)
{
    /** Do not allow multiple detections at once! */
    if (m_inputDetectionEnabled == true || widget == nullptr)
        return false;

    /** Create an empty input source and add it to the requested widget */
    QSharedPointer<QLCInputSource> source = QSharedPointer<QLCInputSource>(new QLCInputSource());
    widget->addInputSource(source);

    enableInputSourceAutoDetection(widget, source->id(), QLCInputSource::invalidUniverse, QLCInputSource::invalidChannel);

    return true;
}

void VirtualConsole::createAndAddInputSource(VCWidget *widget, quint32 universe, quint32 channel)
{
    quint32 controlId = 0; // this is a blind guess, but every widget should have a 0 control ID
    if (m_pages.contains(widget))
        controlId = INPUT_ENABLE_ID;

    QSharedPointer<QLCInputSource> source = QSharedPointer<QLCInputSource>(new QLCInputSource());
    source->setID(controlId);
    source->setUniverse(universe);
    source->setChannel(channel);
    widget->addInputSource(source);

    for (VCPage *page : m_pages)
        page->mapInputSource(source, widget, true);
}

bool VirtualConsole::enableInputSourceAutoDetection(VCWidget *widget, quint32 id, quint32 universe, quint32 channel)
{
    /** Do not allow multiple detections at once! */
    if (m_inputDetectionEnabled == true || widget == nullptr)
        return false;

    qDebug() << "[enableInputSourceAutoDetection] id:" << id << ",uni:" << universe << ",ch:" << channel;

    /** Save also the reference to the requested widget and source, otherwise
     *  the slotInputValueChanged method will not know where to act */
    m_autoDetectionSource = widget->inputSource(id, universe, channel);

    if (m_autoDetectionSource.isNull())
    {
        qDebug() << "Input source is null. Aborting autodetection.";
        return false;
    }

    m_autoDetectionWidget = widget;

    qDebug() << "Autodetection enabled on widget" << widget->caption() << widget->id();

    /** Finally raise the auto detection flag, to
     *  modify the behaviour of slotInputValueChanged */
    m_inputDetectionEnabled = true;

    /** Note that VC pages should update their multi hash map as well,
     *  but since the input source is still invalid, this action is deferred
     *  to when the first input signal comes from an external controller */

    return true;
}

void VirtualConsole::deleteInputSource(VCWidget *widget, quint32 id, quint32 universe, quint32 channel)
{
    if (widget == nullptr)
        return;

    /** In case an autodetection process is running, stop it */
    disableAutoDetection();

    for (VCPage *page : m_pages) // C++11
        page->unMapInputSource(id, universe, channel, widget, true);

    widget->deleteInputSurce(id, universe, channel);
}

void VirtualConsole::disableAutoDetection()
{
    m_inputDetectionEnabled = false;
    m_autoDetectionWidget = nullptr;
    m_autoDetectionSource.clear();
    m_autoDetectionKey = QKeySequence();
    m_autoDetectionKeyId = UINT_MAX;

    updatePageInputs();
}

void VirtualConsole::updatePageInputs()
{
    m_pagesInputSourcesMap.clear();
    m_pagesKeySequencesMap.clear();

    for (int i = 0; i < m_pages.count(); i++)
    {
        VCPage *page = m_pages.at(i);

        for (quint32 &inputSourceKey : page->pageInputSources())
            m_pagesInputSourcesMap[i] = inputSourceKey;

        for (QKeySequence &seq : page->pageKeySequences())
            m_pagesKeySequencesMap[i] = seq;
    }
}

QVariant VirtualConsole::inputChannelsModel()
{
    if (m_inputChannelsTree == nullptr)
    {
        m_inputChannelsTree = new TreeModel(this);
        QQmlEngine::setObjectOwnership(m_inputChannelsTree, QQmlEngine::CppOwnership);
        QStringList treeColumns;
        treeColumns << "classRef" << "type" << "id";
        m_inputChannelsTree->setColumnNames(treeColumns);
        m_inputChannelsTree->enableSorting(false);
    }
    else
    {
        m_inputChannelsTree->clear();
    }

    for (Universe *universe : m_doc->inputOutputMap()->universes())
    {
        /* Get the patch associated to the current universe */
        InputPatch *patch = universe->inputPatch();
        if (patch == nullptr)
            continue;

        QLCInputProfile *profile = patch->profile();
        if (profile == nullptr)
            continue;

        QString nodePath = QString("%1: %2").arg(universe->name()).arg(profile->name());

        QMapIterator <quint32, QLCInputChannel*> it(profile->channels());
        while (it.hasNext() == true)
        {
            QLCInputChannel *channel = it.next().value();
            int itemID = (universe->id() << 16) | it.key();

            QVariantList chParams;
            chParams.append(QVariant::fromValue(channel)); // classRef
            chParams.append(App::ChannelDragItem); // type
            chParams.append(itemID); // id
            m_inputChannelsTree->addItem(QString("%1: %2").arg(it.key() + 1).arg(channel->name()), chParams, nodePath);
        }

        // add also the Universe node data
        QVariantList uniParams;
        uniParams.append(QVariant()); // classRef
        uniParams.append(App::UniverseDragItem); // type
        uniParams.append(universe->id()); // id

        m_inputChannelsTree->setPathData(nodePath, uniParams);
    }

    return QVariant::fromValue(m_inputChannelsTree);
}

QVariantList VirtualConsole::universeListModel() const
{
    QVariantList list;

    for (Universe *universe : m_doc->inputOutputMap()->universes())
    {
        QString name = universe->name();

        /* Get the patch associated to the current universe */
        InputPatch *patch = universe->inputPatch();
        if (patch != nullptr)
        {
            QLCInputProfile *profile = patch->profile();
            if (profile != nullptr)
                name = QString("%1: %2").arg(universe->name()).arg(profile->name());
        }

        QVariantMap uniMap;
        uniMap.insert("mIcon", "");
        uniMap.insert("mLabel", name);
        uniMap.insert("mValue", universe->id());
        list.append(uniMap);
    }

    return list;
}

void VirtualConsole::enableExternalInput(bool enable)
{
    if (m_externalInputEnabled == enable)
        return;

    qDebug() << "[VirtualConsole] external input enabled:" << enable;

    m_externalInputEnabled = enable;
}

bool VirtualConsole::externalInputEnabled() const
{
    return m_externalInputEnabled;
}

void VirtualConsole::slotInputValueChanged(quint32 universe, quint32 channel, uchar value)
{
    //qDebug() << "Input signal received. Universe:" << universe << ", channel:" << channel << ", value:" << value;

    /** Another context (e.g. the Scene Editor bottom panel) is using the
     *  external controller surface, so no widget must be triggered here.
     *  An ongoing autodetection still wins, otherwise the user would not
     *  be able to map a widget while the input is disabled. */
    if (m_externalInputEnabled == false && m_inputDetectionEnabled == false)
        return;

    if (m_inputDetectionEnabled == false)
    {
        quint32 inputSourceKey = (universe << 16) | channel;

        /** first check if this key sequence is a page activation */
        for (int pageIndex : m_pagesInputSourcesMap.keys(inputSourceKey))
        {
            QQuickItem *vcItem = qobject_cast<QQuickItem*>(m_view->rootObject()->findChild<QObject *>("virtualConsole"));
            if (vcItem == nullptr)
                return;

            QMetaObject::invokeMethod(vcItem, "activatePage", Q_ARG(QVariant, pageIndex));
            return;
        }

        /** otherwise forward it to the currently selected page */
        const int activePageIndex = selectedPage();
        VCPage *activePage = page(activePageIndex);
        if (activePage == nullptr)
            return;
        const ShowCommandOrigin origin = inputOriginForUniverse(m_doc, universe);
        activePage->inputValueChanged(inputSourceKey, value, origin);

        // Inherit mode: if the active page doesn't have a mapping for this key,
        // also dispatch to all Normal pages (global pool fallback)
        if (activePage->externalInputMode() == VCPage::Inherit &&
            !activePage->hasInputSourceForKey(inputSourceKey))
        {
            for (int i = 0; i < m_pages.count(); i++)
            {
                if (i != activePageIndex &&
                    m_pages.at(i)->externalInputMode() == VCPage::Normal)
                {
                    m_pages.at(i)->inputValueChangedGlobal(inputSourceKey, value, origin);
                }
            }
        }
    }
    else
    {
        /** The widget reference must be not NULL, otherwise
         *  it means something went nuts */
        Q_ASSERT(m_autoDetectionWidget != nullptr);

        const quint32 sourceId = m_autoDetectionSource->id();
        const quint32 oldUniverse = m_autoDetectionSource->universe();
        const quint32 oldChannel = m_autoDetectionSource->channel();

        for (VCPage *page : m_pages) // C++11
            page->unMapInputSource(sourceId, oldUniverse, oldChannel, m_autoDetectionWidget, true);

        m_autoDetectionWidget->updateInputSource(m_autoDetectionSource, universe, channel);

        for (VCPage *page : m_pages) // C++11
            page->mapInputSource(m_autoDetectionSource, m_autoDetectionWidget, true);

        /** At last, disable the autodetection process */
        disableAutoDetection();
    }
}

/*********************************************************************
 * Keyboard input
 *********************************************************************/

bool VirtualConsole::createAndDetectInputKey(VCWidget *widget)
{
    /** Do not allow multiple detections at once ! */
    if (m_inputDetectionEnabled == true || widget == nullptr)
        return false;

    quint32 controlId = 0;
    widget->addKeySequence(QKeySequence());

    /** VC Pages controls don't start from 0 */
    if (m_pages.contains(widget))
        controlId = INPUT_ENABLE_ID;

    enableKeyAutoDetection(widget, controlId, "");

    return true;
}

bool VirtualConsole::enableKeyAutoDetection(VCWidget *widget, quint32 id, QString keyText)
{
    /** Do not allow multiple detections at once! */
    if (m_inputDetectionEnabled == true || widget == nullptr)
        return false;

    qDebug() << "[enableKeyAutoDetection] id:" << id << ", key:" << keyText;

    m_autoDetectionKey = QKeySequence(keyText);
    m_autoDetectionWidget = widget;
    m_autoDetectionKeyId = id;
    m_inputDetectionEnabled = true;

    return true;
}

void VirtualConsole::updateKeySequenceControlID(VCWidget *widget, quint32 id, QString keyText)
{
    if (widget == nullptr)
        return;

    qDebug() << "Setting control ID" << id << "to widget" << widget->caption() << "sequence" << keyText;

    QKeySequence seq(keyText);

    widget->updateKeySequenceControlID(seq, id);

    /** Update also the key sequence maps in VC pages */
    for (VCPage *page : m_pages) // C++11
        page->updateKeySequenceIDInMap(seq, id, widget, true);
}

void VirtualConsole::deleteKeySequence(VCWidget *widget, quint32 id, QString keyText)
{
    if (widget == nullptr)
        return;

    /** In case an autodetection process is running, stop it */
    disableAutoDetection();

    QKeySequence seq(keyText);

    for (VCPage *page : m_pages) // C++11
        page->unMapKeySequence(seq, id, widget, true);

    widget->deleteKeySequence(seq);
}

void VirtualConsole::handleKeyEvent(QKeyEvent *e, bool pressed)
{
    if (m_inputDetectionEnabled == false)
    {
        /* Ignore the repeating events */
        if (e->isAutoRepeat())
            return;

        if ((e->modifiers() & Qt::ControlModifier) &&
            e->key() == Qt::Key_E && pressed == false)
        {
            setEditMode(!editMode());
            return;
        }

        QKeySequence seq(e->key() | e->modifiers());

        /** first check if this key sequence is a page activation */
        for (int pageIndex : m_pagesKeySequencesMap.keys(seq))
        {
            QQuickItem *vcItem = qobject_cast<QQuickItem*>(m_view->rootObject()->findChild<QObject *>("virtualConsole"));
            if (vcItem == nullptr)
                return;

            QMetaObject::invokeMethod(vcItem, "activatePage", Q_ARG(QVariant, pageIndex));
            return;
        }

        /** otherwise forward it to the currently selected page */
        const int activePageIndex = selectedPage();
        VCPage *activePage = page(activePageIndex);
        if (activePage == nullptr)
            return;
        activePage->handleKeyEvent(seq, pressed);

        // Inherit mode: if the active page doesn't have this key sequence,
        // also dispatch to all Normal pages
        if (activePage->externalInputMode() == VCPage::Inherit &&
            !activePage->hasKeySequence(seq))
        {
            for (int i = 0; i < m_pages.count(); i++)
            {
                if (i != activePageIndex &&
                    m_pages.at(i)->externalInputMode() == VCPage::Normal)
                {
                    m_pages.at(i)->handleKeyEventGlobal(seq, pressed);
                }
            }
        }
    }
    else
    {
        Q_ASSERT(m_autoDetectionWidget != nullptr);

        /** consider only the key release */
        if (pressed == true)
            return;

        QKeySequence seq(e->key() | e->modifiers());
        qDebug() << "Got key sequence:" << seq.toString(QKeySequence::NativeText);
        m_autoDetectionWidget->updateKeySequence(m_autoDetectionKey, seq, m_autoDetectionKeyId);

        for (VCPage *page : m_pages) // C++11
            page->mapKeySequence(seq, m_autoDetectionKeyId, m_autoDetectionWidget, true);

        /** At last, disable the autodetection process */
        disableAutoDetection();
    }
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

bool VirtualConsole::loadXML(QXmlStreamReader &root)
{
    const QString error = workspaceChangeError();
    if (!error.isEmpty())
    {
        qWarning() << Q_FUNC_INFO << error;
        emit editRejected(error);
        return false;
    }
    cancelLayout();
    m_unlockedPagePINs.clear();
    int currPageIdx = 0;

    if (root.name() != KXMLQLCVirtualConsole)
    {
        qWarning() << Q_FUNC_INFO << "Virtual Console node not found";
        return false;
    }

    m_loadStatus = Loading;
    for (VCPage *page : m_pages)
        for (auto it = m_widgetsMap.begin(); it != m_widgetsMap.end(); )
        {
            if (it.value() == page)
                it = m_widgetsMap.erase(it);
            else
                ++it;
        }

    while (root.readNextStartElement())
    {
        //qDebug() << "VC tag:" << root.name();
        if (root.name() == KXMLQLCVCFrame)
        {
            if (currPageIdx == m_pages.count())
            {
                VCPage *page = new VCPage(m_view, m_doc, this, currPageIdx, this);
                QQmlEngine::setObjectOwnership(page, QQmlEngine::CppOwnership);
                m_contextManager->registerContext(page->previewContext());
                m_pages.append(page);
            }
            /* Contents */
            m_pages.at(currPageIdx)->loadXML(root);
            if (m_pages.at(currPageIdx)->caption().isEmpty())
                m_pages.at(currPageIdx)->setCaption(tr("Page %1").arg(currPageIdx + 1));

            m_pages.at(currPageIdx)->buildKeySequenceMap();
            currPageIdx++;

        }
        else if (root.name() == KXMLQLCVCProperties)
        {
            loadPropertiesXML(root);
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Virtual Console tag"
                       << root.name().toString();
            root.skipCurrentElement();
        }
    }

    // delete the exceeding pages. deletePage() never deletes the
    // last one, so stop there if no page was loaded at all
    while (m_pages.count() > qMax(currPageIdx, 1))
        deletePage(m_pages.count() - 1);

    updatePageInputs();

    m_loadStatus = Loaded;

    return true;
}

bool VirtualConsole::loadXMLLegacyInput(QXmlStreamReader &root, quint32* uni, quint32* ch) const
{
    if (root.name() != KXMLQLCVCWidgetInput)
    {
        qWarning() << Q_FUNC_INFO << "Input node not found!";
        return false;
    }
    else
    {
        QXmlStreamAttributes attrs = root.attributes();
        *uni = attrs.value(KXMLQLCVCWidgetInputUniverse).toString().toUInt();
        *ch = attrs.value(KXMLQLCVCWidgetInputChannel).toString().toUInt();
        root.skipCurrentElement();
    }

    return true;
}

bool VirtualConsole::loadPropertiesXML(QXmlStreamReader &root)
{
    if (root.name() != KXMLQLCVCProperties)
    {
        qWarning() << Q_FUNC_INFO << "Virtual Console properties node not found";
        return false;
    }

    GrandMaster::ChannelMode gmLegacyChannelMode = GrandMaster::Intensity;
    GrandMaster::ValueMode gmLegacyValueMode = GrandMaster::Reduce;
    GrandMaster::SliderMode gmLegacySliderMode = GrandMaster::Normal;
    QSharedPointer<QLCInputSource> gmLegacyInputSource;

    QString str;
    while (root.readNextStartElement())
    {
        /** This is a legacy property, converted into
         *  VCFrame "WindowState" tag */
        if (root.name() == KXMLQLCVCPropertiesSize)
        {
            QSize sz;

            /* Width */
            str = root.attributes().value(KXMLQLCVCPropertiesSizeWidth).toString();
            if (str.isEmpty() == false)
                sz.setWidth(str.toInt());

            /* Height */
            str = root.attributes().value(KXMLQLCVCPropertiesSizeHeight).toString();
            if (str.isEmpty() == false)
                sz.setHeight(str.toInt());

            /* Set size if both are valid */
            if (sz.isValid() == true)
                m_pages.at(0)->setGeometry(QRect(0, 0, sz.width(), sz.height()));
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCVCPropertiesGrandMaster)
        {
            QXmlStreamAttributes attrs = root.attributes();

            str = attrs.value(KXMLQLCVCPropertiesGrandMasterChannelMode).toString();
            gmLegacyChannelMode = GrandMaster::stringToChannelMode(str);

            str = attrs.value(KXMLQLCVCPropertiesGrandMasterValueMode).toString();
            gmLegacyValueMode = GrandMaster::stringToValueMode(str);

            if (attrs.hasAttribute(KXMLQLCVCPropertiesGrandMasterSliderMode))
            {
                str = attrs.value(KXMLQLCVCPropertiesGrandMasterSliderMode).toString();
                gmLegacySliderMode = GrandMaster::stringToSliderMode(str);
            }

            QXmlStreamReader::TokenType tType = root.readNext();
            if (tType == QXmlStreamReader::Characters)
                tType = root.readNext();

            // check if there is a Input tag defined
            if (tType == QXmlStreamReader::StartElement)
            {
                if (root.name() == KXMLQLCVCPropertiesInput)
                {
                    quint32 universe = InputOutputMap::invalidUniverse();
                    quint32 channel = QLCChannel::invalid();
                    /* External input */
                    if (loadXMLLegacyInput(root, &universe, &channel) == true)
                        gmLegacyInputSource = QSharedPointer<QLCInputSource>(new QLCInputSource(universe, channel));
                }
                root.skipCurrentElement();
            }
        }
        else
        {
            qWarning() << Q_FUNC_INFO << "Unknown Virtual Console property tag:"
                       << root.name().toString();
            root.skipCurrentElement();
        }
    }

    /* Now check if there's the need to create a GrandMaster
     * as a VC Slider in GrandMaster mode */
    if (gmLegacyChannelMode != GrandMaster::Intensity || gmLegacyValueMode != GrandMaster::Reduce ||
        gmLegacySliderMode != GrandMaster::Normal || gmLegacyInputSource.isNull() == false)
    {
        VCSlider *slider = new VCSlider(m_doc, m_pages.at(0));
        QQmlEngine::setObjectOwnership(slider, QQmlEngine::CppOwnership);
        slider->setGeometry(QRect(0, 0, pixelDensity() * 10, m_pages.at(0)->geometry().height()));
        slider->setDefaultFontSize(pixelDensity() * 3.5);
        slider->setSliderMode(VCSlider::GrandMaster);

        slider->setGrandMasterChannelMode(gmLegacyChannelMode);
        slider->setGrandMasterValueMode(gmLegacyValueMode);

        if (gmLegacySliderMode == GrandMaster::Inverted)
            slider->setInvertedAppearance(true);

        if (gmLegacyInputSource.isNull() == false)
            slider->addInputSource(gmLegacyInputSource);

        m_pages.at(0)->addWidgetToPageMap(slider);
        addWidgetToMap(slider);
    }

    return true;
}

bool VirtualConsole::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);

    /* Virtual Console entry */
    doc->writeStartElement(KXMLQLCVirtualConsole);

    /* Contents */
    for (int i = 0; i < m_pages.count(); i++)
        m_pages.at(i)->saveXML(doc);

    /* Properties */
    //m_properties.saveXML(doc);

    /* End the <VirtualConsole> tag */
    doc->writeEndElement();

    return true;
}

void VirtualConsole::postLoad()
{
    /** apply GM values */
    m_doc->inputOutputMap()->setGrandMasterValue(255);
    //m_doc->inputOutputMap()->setGrandMasterValueMode(m_properties.grandMasterValueMode());
    //m_doc->inputOutputMap()->setGrandMasterChannelMode(m_properties.grandMasterChannelMode());

    /** Go through widgets, check IDs and register widgets to the map
      * This code is the same as the one in addWidgetToMap()
      * We have to repeat it to limit conflicts if
      * one widget was not saved with a valid ID,
      * as addWidgetToMap ensures the widget WILL be added */

    QList<VCWidget *> invalidWidgetsList;
    QList<VCWidget *> widgetsList;

    foreach (VCPage *page, m_pages)
    {
        widgetsList.append(page);
        widgetsList.append(page->children(true));
    }

    foreach (VCWidget *widget, widgetsList)
    {
        quint32 wid = widget->id();
        if (wid != VCWidget::invalidId())
        {
            if (!m_widgetsMap.contains(wid))
                m_widgetsMap.insert(wid, widget);
            else if (m_widgetsMap[wid] != widget)
                invalidWidgetsList.append(widget);
        }
        else
            invalidWidgetsList.append(widget);

        widget->updateFeedback();
    }
    foreach (VCWidget *widget, invalidWidgetsList)
        addWidgetToMap(widget);

    /** Now for each page, map the children widgets input
     *  sources, to look it up later when input signals start
     *  to roll */
    foreach (VCPage *page, m_pages)
        page->mapChildrenInputSources();
    emit widgetsChanged();
}
