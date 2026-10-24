/*
  Q Light Controller Plus
  vceditor.cpp

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
#include <QQmlEngine>
#include <QQuickItem>
#include <QScopedValueRollback>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <algorithm>
#include <cmath>

#include "vceditor.h"
#include "virtualconsole.h"
#include "contextmanager.h"
#include "qlcfile.h"
#include "tardis.h"
#include "vcframe.h"
#include "vcpage.h"

#define KXMLQLCVCFrameArrange         QStringLiteral("Arrange")
#define KXMLQLCVCFrameArrangeMode     QStringLiteral("Mode")
#define KXMLQLCVCFrameArrangeGap      QStringLiteral("Gap")
#define KXMLQLCVCFrameArrangeColumns  QStringLiteral("Columns")
#define KXMLQLCVCFrameArrangeFill     QStringLiteral("Fill")

static const QStringList arrangeModeNames {"None", "Row", "Column", "Grid", "Flow"};

/*********************************************************************
 * VCArrangement
 *********************************************************************/

VCArrangement::VCArrangement(VCFrame *frame)
    : QObject(frame)
{
    m_spec.gap = 10;
    m_spec.columns = 4;
}

VCArrangement *VCArrangement::of(const VCFrame *frame, bool create)
{
    if (frame == nullptr)
        return nullptr;
    VCArrangement *arrangement = frame->findChild<VCArrangement *>(QString(), Qt::FindDirectChildrenOnly);
    if (arrangement == nullptr && create)
    {
        arrangement = new VCArrangement(const_cast<VCFrame *>(frame));
        QQmlEngine::setObjectOwnership(arrangement, QQmlEngine::CppOwnership);
    }
    return arrangement;
}

GridLayout::ArrangeSpec VCArrangement::specOf(const VCFrame *frame)
{
    const VCArrangement *arrangement = of(frame, false);
    return arrangement ? arrangement->m_spec : GridLayout::ArrangeSpec();
}

QVariantMap VCArrangement::toMap(const GridLayout::ArrangeSpec &spec)
{
    return {{"mode", int(spec.mode)}, {"gap", spec.gap}, {"columns", spec.columns}, {"fill", spec.fill}};
}

GridLayout::ArrangeSpec VCArrangement::fromMap(const QVariantMap &map)
{
    GridLayout::ArrangeSpec spec;
    spec.mode = GridLayout::ArrangeMode(map.value("mode").toInt());
    spec.gap = map.value("gap").toReal();
    spec.columns = map.value("columns").toInt();
    spec.fill = map.value("fill").toBool();
    return spec;
}

void VCArrangement::setSpec(const GridLayout::ArrangeSpec &spec)
{
    m_spec = spec;
    emit changed();
}

bool VCArrangement::isXMLTag(const QXmlStreamReader &root)
{
    return root.name() == KXMLQLCVCFrameArrange;
}

void VCArrangement::loadXML(VCFrame *frame, QXmlStreamReader &root)
{
    const QXmlStreamAttributes attrs = root.attributes();
    GridLayout::ArrangeSpec spec = of(frame, true)->m_spec;
    spec.mode = GridLayout::ArrangeMode(qMax(0, int(arrangeModeNames.indexOf(attrs.value(KXMLQLCVCFrameArrangeMode).toString()))));
    if (attrs.hasAttribute(KXMLQLCVCFrameArrangeGap))
        spec.gap = qMax(qreal(0), attrs.value(KXMLQLCVCFrameArrangeGap).toDouble());
    if (attrs.hasAttribute(KXMLQLCVCFrameArrangeColumns))
        spec.columns = qMax(1, attrs.value(KXMLQLCVCFrameArrangeColumns).toInt());
    spec.fill = attrs.value(KXMLQLCVCFrameArrangeFill).toString() == KXMLQLCTrue;
    of(frame, true)->setSpec(spec);
    root.skipCurrentElement();
}

void VCArrangement::saveXML(const VCFrame *frame, QXmlStreamWriter *doc)
{
    /* only frames that arrange their children, so older versions see nothing new */
    const GridLayout::ArrangeSpec spec = specOf(frame);
    if (spec.mode == GridLayout::ArrangeMode::None)
        return;
    doc->writeStartElement(KXMLQLCVCFrameArrange);
    doc->writeAttribute(KXMLQLCVCFrameArrangeMode, arrangeModeNames.value(int(spec.mode)));
    doc->writeAttribute(KXMLQLCVCFrameArrangeGap, QString::number(spec.gap));
    doc->writeAttribute(KXMLQLCVCFrameArrangeColumns, QString::number(spec.columns));
    doc->writeAttribute(KXMLQLCVCFrameArrangeFill, spec.fill ? KXMLQLCTrue : KXMLQLCFalse);
    doc->writeEndElement();
}

void VCArrangement::copy(const VCFrame *from, VCFrame *to)
{
    if (of(from, false))
        of(to, true)->setSpec(specOf(from));
}

/*********************************************************************
 * VCEditor
 *********************************************************************/

VCEditor::VCEditor(VirtualConsole *vc)
    : QObject(vc)
    , m_vc(vc)
{
}

/*********************************************************************
 * Undo
 *********************************************************************/

void VCEditor::recordProperty(VCWidget *widget, const char *property,
                              const QVariant &oldValue, const QVariant &newValue)
{
    if (Tardis::instance() == nullptr || widget->id() == VCWidget::invalidId())
        return;
    const QString name = QString::fromLatin1(property);
    Tardis::instance()->enqueueAction(Tardis::VCEditorAction, widget->id(),
                                      QVariantMap{{"property", name}, {"value", oldValue}},
                                      QVariantMap{{"property", name}, {"value", newValue}});
}

void VCEditor::recordPage(int index, bool added)
{
    if (Tardis::instance() == nullptr)
        return;
    // a page comes back from its XML with its own ID, so later steps on it keep working
    VCPage *page = m_vc->page(index);
    const QVariantMap present {{"page", index},
                               {"xml", Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, page->id())}};
    const QVariantMap absent {{"page", index}};
    Tardis::instance()->enqueueAction(Tardis::VCEditorAction, page->id(),
                                      added ? absent : present, added ? present : absent);
}

bool VCEditor::replay(quint32 objID, const QVariant &value)
{
    const QVariantMap action = value.toMap();
    if (action.contains("page"))
    {
        const int index = action.value("page").toInt();
        if (action.contains("xml"))
            return restorePage(index, action.value("xml").toByteArray());
        m_vc->deletePage(index);
        return true;
    }
    VCWidget *widget = m_vc->widget(objID);
    if (widget == nullptr)
        return false;
    if (action.contains("arrangement"))
    {
        VCArrangement::of(qobject_cast<VCFrame *>(widget), true)->setSpec(
            VCArrangement::fromMap(action.value("arrangement").toMap()));
        return true;
    }
    if (action.contains("orderIndex"))
    {
        /* a widget restored by undo goes back to its place among its siblings */
        return m_vc->restoreWidgetParent(action.value("widgetID").toUInt(), objID,
                                         action.value("subpage").toInt(), action.value("orderIndex").toInt());
    }
    return widget->setProperty(action.value("property").toString().toLatin1(), action.value("value"));
}

void VCEditor::resetContents()
{
    for (VCPage *page : m_vc->m_pages)
        delete VCArrangement::of(page, false);
}

bool VCEditor::restorePage(int index, const QByteArray &xml)
{
    /* widgets read their ID only from a reader with a device */
    QBuffer buffer;
    buffer.setData(xml);
    buffer.open(QIODevice::ReadOnly | QIODevice::Text);
    QXmlStreamReader reader(&buffer);
    if (index < 0 || index > m_vc->m_pages.count() || !reader.readNextStartElement())
        return false;

    VCPage *page = new VCPage(m_vc->m_view, m_vc->m_doc, m_vc, index, m_vc);
    QQmlEngine::setObjectOwnership(page, QQmlEngine::CppOwnership);
    if (!page->loadXML(reader))
    {
        delete page;
        return false;
    }
    m_vc->addWidgetToMap(page);
    m_vc->m_contextManager->registerContext(page->previewContext());
    m_vc->m_pages.insert(index, page);
    /* like a loaded page, it and its widgets take input again */
    for (const auto &source : page->inputSources())
        page->mapInputSource(source, page, false);
    page->mapChildrenInputSources();
    page->buildKeySequenceMap();
    m_vc->updatePageInputs();

    /* keep the shown page selected, before the page tabs rebuild and check
       the tab at the old index, which could ask for the PIN of the new page */
    if (index <= m_vc->m_selectedPage && m_vc->m_pages.count() > 1)
    {
        m_vc->m_selectedPage++;
        emit m_vc->selectedPageChanged(m_vc->m_selectedPage);
    }
    emit m_vc->pagesCountChanged();
    /* then show the page undo brings back, unless that asks for a PIN first */
    if (!page->requirePIN())
        m_vc->setSelectedPage(index);
    return true;
}

/*********************************************************************
 * Selection
 *********************************************************************/

QVariantList VCEditor::widgetsInRect(quint32 frameID, QRectF logicalRect) const
{
    QVariantList ids;
    VCFrame *frame = m_vc->targetFrame(frameID);
    if (frame == nullptr)
        return ids;
    for (VCWidget *child : frame->children(false))
    {
        const QRectF overlap = child->logicalGeometry().intersected(logicalRect);
        if (child->page() == frame->currentPage() && overlap.width() > 0 && overlap.height() > 0)
            ids.append(child->id());
    }
    return ids;
}

void VCEditor::setWidgetsSize(VCWidget *refWidget, int orientations)
{
    if (refWidget == nullptr)
        return;

    const QSizeF size = refWidget->logicalGeometry().size();
    QMap<quint32, QRectF> rectangles;
    for (const QVariant &id : m_vc->selectedWidgetIDs())
    {
        VCWidget *widget = m_vc->widget(id.toUInt());
        const QString error = m_vc->nativeAuthoringError(widget);
        if (!error.isEmpty())
        {
            m_vc->reportEdit(m_vc->editResult(false, false, error));
            return;
        }
        QRectF rect = widget->logicalGeometry();
        if (orientations & Qt::Horizontal)
            rect.setWidth(size.width());
        if (orientations & Qt::Vertical)
            rect.setHeight(size.height());
        rectangles.insert(widget->id(), rect);
    }
    m_vc->reportEdit(m_vc->applyGeometryPlan(rectangles));
}

void VCEditor::duplicateSelection()
{
    QString error;
    const QList<VCWidget *> roots = m_vc->selectedRoots(m_vc->selectedWidgetIDs(), &error);
    if (!error.isEmpty() || roots.isEmpty())
    {
        m_vc->reportEdit(m_vc->editResult(false, false, error.isEmpty() ? tr("No widgets selected") : error));
        return;
    }
    VCFrame *parent = qobject_cast<VCFrame *>(roots.first()->parent());
    QRectF bounds;
    QVariantList ids;
    for (VCWidget *root : roots)
    {
        if (root->parent() != parent)
        {
            m_vc->reportEdit(m_vc->editResult(false, false, tr("Duplicate a selection from one parent at a time")));
            return;
        }
        bounds = bounds.united(root->logicalGeometry());
        ids.append(root->id());
    }
    const QVariantMap guide = m_vc->snapGuide(parent->id());
    const qreal gap = guide.value("enabled").toBool() ? guide.value("step").toSizeF().width() : 10;
    QQuickItem *area = parent->renderItem() ?
        parent->renderItem()->findChild<QQuickItem *>(QString("frameDropArea%1").arg(parent->id())) : nullptr;
    QScopedValueRollback<bool> keepCut(m_vc->m_clipboardIsCut, false);
    m_vc->reportEdit(m_vc->pasteWidgets(ids, parent->id(), bounds.topRight() + QPointF(gap, 0), area, true));
}

bool VCEditor::placeNear(QMap<quint32, QRectF> &rectangles, VCFrame *target, int subpage, qreal step) const
{
    const QRectF bounds = target->contentRect();
    QRectF group;
    for (const QRectF &rect : std::as_const(rectangles))
        group = group.united(rect);
    QList<QRectF> siblings;
    for (VCWidget *child : target->children(false))
        if (child->page() == subpage)
            siblings.append(child->logicalGeometry());
    const qreal stride = step > 0 ? step : 10;
    const auto aligned = [stride, step](qreal value) { return step > 0 ? std::ceil(value / stride) * stride : value; };
    const QPointF start(qBound(bounds.left(), group.left(), bounds.right()), qBound(bounds.top(), group.top(), bounds.bottom()));
    QList<qreal> rows;
    for (qreal y = aligned(start.y()); y + group.height() <= bounds.bottom(); y += stride)
        rows.append(y);
    for (qreal y = aligned(bounds.top()); y < start.y(); y += stride)
        rows.append(y);
    for (int row = 0; row < rows.size(); row++)
        for (qreal x = aligned(row == 0 ? start.x() : bounds.left()); x + group.width() <= bounds.right(); x += stride)
        {
            const QPointF offset = QPointF(x, rows.at(row)) - group.topLeft();
            bool free = true;
            for (int i = 0; i < siblings.size() && free; i++)
            {
                const QRectF overlap = group.translated(offset).intersected(siblings.at(i));
                free = overlap.width() <= 0 || overlap.height() <= 0;
            }
            if (!free)
                continue;
            QMap<quint32, QRectF> moved;
            for (auto it = rectangles.constBegin(); it != rectangles.constEnd(); ++it)
                moved.insert(it.key(), it.value().translated(offset));
            if (m_vc->geometryError(moved, target, subpage, true).isEmpty())
            {
                rectangles = moved;
                return true;
            }
        }
    return false;
}

/*********************************************************************
 * Arrangement
 *********************************************************************/

QObject *VCEditor::arrangement(quint32 frameID)
{
    return VCArrangement::of(m_vc->targetFrame(frameID), true);
}

QString VCEditor::arrangePlan(QMap<quint32, QRectF> &rectangles,
                              const QHash<quint32, GridLayout::ArrangeSpec> &specs) const
{
    const auto specOf = [&specs](const VCFrame *frame) {
        return specs.contains(frame->id()) ? specs.value(frame->id()) : VCArrangement::specOf(frame);
    };
    QList<VCFrame *> pending;
    const auto queue = [&](VCFrame *frame) {
        if (frame && specOf(frame).mode != GridLayout::ArrangeMode::None && !pending.contains(frame))
            pending.append(frame);
    };
    for (auto it = rectangles.constBegin(); it != rectangles.constEnd(); ++it)
        if (VCWidget *entry = m_vc->widget(it.key()))
        {
            queue(qobject_cast<VCFrame *>(entry));
            queue(qobject_cast<VCFrame *>(entry->parent()));
        }
    // outer frames first, so the frames inside arrange in their new size
    const auto depth = [](QObject *object) {
        int level = 0;
        for (; object; object = object->parent())
            level++;
        return level;
    };
    std::stable_sort(pending.begin(), pending.end(), [&depth](VCFrame *a, VCFrame *b) { return depth(a) < depth(b); });

    for (int i = 0; i < pending.size(); i++)
    {
        VCFrame *frame = pending.at(i);
        QRectF content = frame->contentRect();
        if (rectangles.contains(frame->id()))
        {
            const QSizeF size = rectangles.value(frame->id()).size();
            content.setWidth(size.width());
            content.setHeight(qMax(qreal(0), size.height() - content.top()));
        }
        for (int subpage = 0; subpage < frame->totalPagesNumber(); subpage++)
        {
            QList<VCWidget *> children;
            QList<QSizeF> sizes, minimums;
            for (const QVariant &id : frame->orderedChildIDs(subpage))
            {
                VCWidget *child = m_vc->widget(id.toUInt());
                children.append(child);
                sizes.append(rectangles.value(child->id(), child->logicalGeometry()).size());
                minimums.append(child->minimumSize());
            }
            const GridLayout::ArrangeResult arranged = GridLayout::arrange(specOf(frame), content, sizes, minimums);
            if (!arranged.error.isEmpty())
                return tr("Frame %1: %2").arg(frame->caption(), arranged.error);
            for (int c = 0; c < children.size(); c++)
            {
                rectangles.insert(children.at(c)->id(), arranged.rects.at(c));
                if (arranged.rects.at(c).size() != children.at(c)->logicalGeometry().size())
                    queue(qobject_cast<VCFrame *>(children.at(c)));
            }
        }
    }
    return QString();
}

QVariantMap VCEditor::arrangeFrame(quint32 frameID, QVariantMap settings)
{
    if (m_vc->m_authoringBusy)
        return m_vc->editResult(false, false, tr("Another console edit is being committed"));
    VCFrame *frame = m_vc->targetFrame(frameID);
    if (!frame)
        return m_vc->editResult(false, false, tr("Unknown frame %1").arg(frameID));
    QString error = m_vc->nativeAuthoringError(frame);
    if (!error.isEmpty())
        return m_vc->editResult(false, false, error);
    for (const QString &key : settings.keys())
        if (key != "mode" && key != "gap" && key != "columns" && key != "fill")
            return m_vc->editResult(false, false, tr("Unknown arrangement setting %1").arg(key));

    const GridLayout::ArrangeSpec old = VCArrangement::of(frame, true)->specOf(frame);
    GridLayout::ArrangeSpec spec = old;
    spec.mode = GridLayout::ArrangeMode(settings.value("mode", int(spec.mode)).toInt());
    spec.gap = settings.value("gap", spec.gap).toReal();
    spec.columns = settings.value("columns", spec.columns).toInt();
    spec.fill = settings.value("fill", spec.fill).toBool();
    if (spec.mode < GridLayout::ArrangeMode::None || spec.mode > GridLayout::ArrangeMode::Flow ||
        spec.gap < 0 || spec.columns < 1)
        return m_vc->editResult(false, false, tr("Invalid arrangement"));
    // a frame left free stays free whatever the settings: nothing to record
    if (spec.mode == GridLayout::ArrangeMode::None && old.mode == GridLayout::ArrangeMode::None)
        return m_vc->editResult(true, false, {});

    // the children are placed and checked before anything changes
    QMap<quint32, QRectF> rectangles {{frame->id(), frame->logicalGeometry()}};
    error = arrangePlan(rectangles, {{frame->id(), spec}});
    rectangles.remove(frame->id());
    if (error.isEmpty() && !rectangles.isEmpty())
        error = m_vc->geometryError(rectangles);
    if (!error.isEmpty())
        return m_vc->editResult(false, false, error);

    // the same arrangement again only moves children that strayed
    if (VCArrangement::toMap(spec) == VCArrangement::toMap(old))
        return rectangles.isEmpty() ? m_vc->editResult(true, false, {}) : m_vc->commitGeometry(rectangles);

    Tardis::instance()->beginBatch("arrangeFrame");
    Tardis::instance()->enqueueAction(Tardis::VCEditorAction, frame->id(),
                                      QVariantMap{{"arrangement", VCArrangement::toMap(old)}},
                                      QVariantMap{{"arrangement", VCArrangement::toMap(spec)}});
    VCArrangement::of(frame, true)->setSpec(spec);
    m_vc->m_doc->setModified();
    const QVariantMap result = rectangles.isEmpty() ? m_vc->editResult(true, true, {}) : m_vc->commitGeometry(rectangles);
    Tardis::instance()->endBatch();
    return result;
}

int VCEditor::dropIndex(VCFrame *target, int subpage, const QRectF &rect) const
{
    if (VCArrangement::specOf(target).mode == GridLayout::ArrangeMode::None)
        return -1;
    int index = 0;
    for (const QVariant &id : target->orderedChildIDs(subpage))
    {
        // the widget itself counts too: structure edits index the order before the move
        const QRectF sibling = m_vc->widget(id.toUInt())->logicalGeometry();
        const bool rowAbove = sibling.center().y() < rect.top() - sibling.height() / 2;
        const bool sameRow = qAbs(sibling.center().y() - rect.top()) <= sibling.height() / 2;
        if (!rowAbove && !(sameRow && sibling.center().x() < rect.left()))
            break;
        index++;
    }
    return index;
}

QString VCEditor::arrangeStructure(VCFrame *target, int subpage, const QList<VCWidget *> &order,
                                   QMap<quint32, QRectF> &candidates, QMap<quint32, QRectF> &siblings) const
{
    const GridLayout::ArrangeSpec spec = VCArrangement::specOf(target);
    if (spec.mode == GridLayout::ArrangeMode::None)
        return QString();
    QList<QSizeF> sizes, minimums;
    for (VCWidget *child : order)
    {
        sizes.append(candidates.value(child->id(), child->logicalGeometry()).size());
        minimums.append(child->minimumSize());
    }
    const GridLayout::ArrangeResult arranged = GridLayout::arrange(spec, target->contentRect(), sizes, minimums);
    if (!arranged.error.isEmpty())
        return tr("Frame %1: %2").arg(target->caption(), arranged.error);
    for (int i = 0; i < order.size(); i++)
    {
        if (candidates.contains(order.at(i)->id()))
            candidates.insert(order.at(i)->id(), arranged.rects.at(i));
        else if (arranged.rects.at(i) != order.at(i)->logicalGeometry())
            siblings.insert(order.at(i)->id(), arranged.rects.at(i));
    }
    Q_UNUSED(subpage)
    return QString();
}

QVariantMap VCEditor::commitStructure(const QList<VCWidget *> &roots, VCFrame *target, int subpage,
                                      const QList<VCWidget *> &order, const QMap<quint32, QRectF> &candidates,
                                      const QMap<quint32, QRectF> &siblings)
{
    QList<VCFrame *> sources;
    for (VCWidget *root : roots)
        if (VCFrame *source = qobject_cast<VCFrame *>(root->parent()); source != target && !sources.contains(source))
            sources.append(source);
    Tardis::instance()->beginBatch("arrangedStructureEdit");
    const QVariantMap result = m_vc->commitStructure(roots, target, subpage, order, candidates);
    if (result.value("accepted").toBool())
    {
        if (!siblings.isEmpty())
            m_vc->commitGeometry(siblings);
        rearrange(sources);
    }
    Tardis::instance()->endBatch();
    return result;
}

void VCEditor::deleteWidgets(const QVariantList &ids)
{
    QList<VCFrame *> parents;
    for (const QVariant &id : ids)
        if (VCWidget *widget = m_vc->widget(id.toUInt()))
            if (VCFrame *parent = qobject_cast<VCFrame *>(widget->parent()); !parents.contains(parent))
                parents.append(parent);
    if (Tardis::instance())
        Tardis::instance()->beginBatch("deleteWidgets");
    m_vc->commitWidgetDeletion(ids);
    // parents deleted along with their frames are gone by now
    parents.removeIf([this](VCFrame *frame) { return !m_vc->m_widgetsMap.values().contains(frame); });
    rearrange(parents);
    if (Tardis::instance())
        Tardis::instance()->endBatch();
}

void VCEditor::rearrange(const QList<VCFrame *> &frames)
{
    QMap<quint32, QRectF> rectangles;
    for (VCFrame *frame : frames)
        if (frame && VCArrangement::specOf(frame).mode != GridLayout::ArrangeMode::None)
            rectangles.insert(frame->id(), frame->logicalGeometry());
    if (rectangles.isEmpty() || !arrangePlan(rectangles).isEmpty())
        return;
    for (VCFrame *frame : frames)
        if (frame)
            rectangles.remove(frame->id());
    if (!rectangles.isEmpty())
        m_vc->commitGeometry(rectangles);
}

bool VCEditor::isArranged(const VCFrame *frame) const
{
    return VCArrangement::specOf(frame).mode != GridLayout::ArrangeMode::None;
}

QString VCEditor::arrangedFit(VCFrame *frame, int subpage, const QList<QSizeF> &sizes,
                              const QList<QSizeF> &minimums, const QList<quint32> &leaving) const
{
    if (!isArranged(frame))
        return QString();
    QList<QSizeF> allSizes, allMinimums;
    for (const QVariant &id : frame->orderedChildIDs(subpage))
    {
        if (leaving.contains(id.toUInt()))
            continue;
        VCWidget *child = m_vc->widget(id.toUInt());
        allSizes.append(child->logicalGeometry().size());
        allMinimums.append(child->minimumSize());
    }
    allSizes.append(sizes);
    allMinimums.append(minimums);
    const GridLayout::ArrangeResult arranged =
        GridLayout::arrange(VCArrangement::specOf(frame), frame->contentRect(), allSizes, allMinimums);
    return arranged.error.isEmpty() ? QString() : tr("Frame %1: %2").arg(frame->caption(), arranged.error);
}

QString VCEditor::arrangedFit(VCFrame *frame, int subpage, const QMap<quint32, QRectF> &joining, bool cut) const
{
    QList<QSizeF> sizes, minimums;
    for (auto it = joining.constBegin(); it != joining.constEnd(); ++it)
    {
        sizes.append(it.value().size());
        minimums.append(m_vc->widget(it.key())->minimumSize());
    }
    return arrangedFit(frame, subpage, sizes, minimums, cut ? joining.keys() : QList<quint32>());
}

void VCEditor::arrangeJoined(VCFrame *frame)
{
    QMap<quint32, QRectF> rectangles {{frame->id(), frame->logicalGeometry()}};
    if (!isArranged(frame) || !arrangePlan(rectangles).isEmpty())
        return;
    rectangles.remove(frame->id());
    for (auto it = rectangles.constBegin(); it != rectangles.constEnd(); ++it)
        if (VCWidget *widget = m_vc->widget(it.key()); widget && widget->logicalGeometry() != it.value())
            widget->setLogicalGeometry(it.value());
}

void VCEditor::addWidgetMatrix(quint32 targetID, QString matrixType, QPointF logicalPosition,
                               QSize matrixSize, QSize widgetSize, bool soloFrame)
{
    const qreal gap = 5;
    if (matrixSize.isEmpty() || widgetSize.isEmpty() || (matrixType != "buttonmatrix" && matrixType != "slidermatrix"))
    {
        emit m_vc->editRejected(tr("Invalid widget matrix dimensions or type"));
        return;
    }
    const QVariantMap guide = m_vc->snapGuide(targetID);
    if (guide.value("enabled").toBool())
    {
        const qreal step = guide.value("step").toSizeF().width();
        logicalPosition = QPointF(std::round(logicalPosition.x() / step) * step, std::round(logicalPosition.y() / step) * step);
    }
    const QSizeF size(matrixSize.width() * widgetSize.width() + gap * (matrixSize.width() + 1),
                      matrixSize.height() * widgetSize.height() + gap * (matrixSize.height() + 1));
    const QString type = matrixType == "buttonmatrix" ? "Button" : "Slider";

    Tardis::instance()->beginBatch("widgetMatrix");
    QVariantMap result = m_vc->createWidgetGeometry(targetID, soloFrame ? "Solo frame" : "Frame",
                                                    QRectF(logicalPosition, size), -1);
    const quint32 frameID = result.value("widgetIDs").toList().value(0).toUInt();
    if (result.value("accepted").toBool())
    {
        qobject_cast<VCFrame *>(m_vc->widget(frameID))->setShowHeader(false);
        result = arrangeFrame(frameID, {{"mode", int(GridLayout::ArrangeMode::Grid)}, {"columns", matrixSize.width()},
                                        {"gap", gap}, {"fill", true}});
    }
    for (int i = 0; i < matrixSize.width() * matrixSize.height() && result.value("accepted").toBool(); i++)
        result = m_vc->createWidgetGeometry(frameID, type, QRectF(QPointF(gap, gap), QSizeF(widgetSize)), -1);
    Tardis::instance()->endBatch();
    if (!result.value("accepted").toBool())
    {
        if (frameID != 0 && m_vc->widget(frameID))
            m_vc->deleteVCWidgets({frameID});
        emit m_vc->editRejected(result.value("error").toString());
    }
}
