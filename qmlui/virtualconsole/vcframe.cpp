/*
  Q Light Controller Plus
  vcframe.cpp

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
#include <QDebug>

#include "tardis.h"
#include "vcframe.h"
#include "vclabel.h"
#include "vcclock.h"
#include "vcxypad.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vccuelist.h"
#include "vcsoloframe.h"
#include "simplecrypt.h"
#include "vcanimation.h"
#include "vcspeeddial.h"
#include "virtualconsole.h"
#include "vcaudiotriggers.h"
#include "vcrecordpanel.h"
#include <QQmlExpression>
#include <QQmlContext>
#include <algorithm>
#include <cmath>

static const quint64 encKey = 0x5131632B5067334B; // this is "Q1c+Pg3K"

VCFrame::VCFrame(Doc *doc, VirtualConsole *vc, QObject *parent)
    : VCWidget(doc, parent)
    , m_vc(vc)
    , m_showHeader(true)
    , m_showEnable(true)
    , m_isCollapsed(false)
    , m_multiPageMode(false)
    , m_currentPage(0)
    , m_totalPagesNumber(1)
    , m_pagesLoop(false)
    , m_PIN(0)
    , m_validatedPIN(false)
    , m_submasterValue(1)
{
    setType(VCWidget::FrameWidget);

    registerExternalControl(INPUT_NEXT_PAGE_ID, tr("Next Page"), true);
    registerExternalControl(INPUT_PREVIOUS_PAGE_ID, tr("Previous Page"), true);
    registerExternalControl(INPUT_ENABLE_ID, tr("Enable"), true);
    registerExternalControl(INPUT_COLLAPSE_ID, tr("Collapse"), true);
}

VCFrame::~VCFrame()
{
    deleteChildren();

    if (m_item)
        delete m_item;
}

void VCFrame::initializeProperties()
{
    resetBackgroundColor();
    resetForegroundColor();
    setPagesLoop(false);
    m_PIN = 0;
    m_validatedPIN = false;
    m_submasterValue = 1;
}

QString VCFrame::defaultCaption() const
{
    return tr("Frame %1").arg(id() + 1);
}

void VCFrame::setupLookAndFeel(qreal pixelDensity, int page)
{
    setPage(page);
    setDefaultFontSize(pixelDensity * 3.5);
}

void VCFrame::render(QQuickView *view, QQuickItem *parent)
{
    if (view == nullptr || parent == nullptr)
        return;

    QQmlComponent *component = new QQmlComponent(view->engine(), QUrl("qrc:/VCFrameItem.qml"));

    if (component->isError())
    {
        qDebug() << component->errors();
        delete component;
        return;
    }

    m_item = createRenderItem(component, view, parent);
    if (m_item == nullptr)
        qWarning() << Q_FUNC_INFO << "Unable to create frame component" << component->errors();
    delete component;
    if (m_item == nullptr)
        return;

    m_item->setParentItem(parent);
    m_item->setProperty("frameObj", QVariant::fromValue(this));
    m_item->setProperty("isSelected", isEditing());

    if (m_pagesMap.count() > 0)
    {
        QString chName = QString("frameDropArea%1").arg(id());
        QQuickItem *childrenArea = qobject_cast<QQuickItem*>(m_item->findChild<QObject *>(chName));

        QMap <VCWidget *, int>::iterator it = m_pagesMap.begin();
        for (; it != m_pagesMap.end(); it++)
            it.key()->render(view, childrenArea);
    }
}

QString VCFrame::propertiesResource() const
{
    /** If this frame is a top level frame, then it means
     *  it is a VC page, so return a specific properties resource */
    if (parent() == m_vc)
        return QString("qrc:/VCPageProperties.qml");

    return QString("qrc:/VCFrameProperties.qml");
}

VCWidget *VCFrame::createCopy(VCWidget *parent) const
{
    Q_ASSERT(parent != nullptr);

    VCFrame *frame = new VCFrame(m_doc, m_vc, parent);
    QQmlEngine::setObjectOwnership(frame, QQmlEngine::CppOwnership);

    if (frame->copyFrom(this) == false)
    {
        delete frame;
        frame = nullptr;
    }

    return frame;
}

bool VCFrame::copyFrom(const VCWidget *widget)
{
    const VCFrame *frame = qobject_cast<const VCFrame*> (widget);
    if (frame == nullptr)
        return false;

    setShowHeader(frame->showHeader());
    setShowEnable(frame->showEnable());

    setMultiPageMode(frame->multiPageMode());
    setTotalPagesNumber(frame->totalPagesNumber());
    setPagesLoop(frame->pagesLoop());
    m_arrangeMode = frame->m_arrangeMode;
    m_arrangeGap = frame->m_arrangeGap;
    m_arrangeColumns = frame->m_arrangeColumns;
    m_arrangeFill = frame->m_arrangeFill;

    setLayoutMode(frame->layoutMode());
    setGridColumns(frame->gridColumns());
    setGridRowHeight(frame->gridRowHeight());
    setGridCompact(frame->gridCompact());

    m_pageLabels = frame->m_pageLabels;
    QListIterator <VCWidget*> it(frame->children(false));
    while (it.hasNext() == true)
    {
        VCWidget *child = it.next();
        VCWidget *childCopy = nullptr;

        /* findChildren() is recursive, so the list contains all
           possible child widgets below this frame. Each frame must
           save only its direct children to preserve hierarchy, so
           save only such widgets that have this widget as their
           direct parent. */
        if (child->parent() == widget)
        {
            childCopy = child->createCopy(this);
            m_vc->addWidgetToMap(childCopy);

            qDebug() << "Child copy in parent:" << childCopy->caption() << ", page:" << childCopy->page();
        }

        if (childCopy != nullptr)
        {
            addWidgetToPageMap(childCopy);
            checkSubmasterConnection(childCopy);
        }
    }

    /* Copy common stuff */
    const bool copied = VCWidget::copyFrom(widget);
    setCurrentPage(frame->currentPage());
    for (VCWidget *child : children(false))
        child->setVisible(child->page() == currentPage());
    return copied;
}

bool VCFrame::hasChildren() const
{
    return !m_pagesMap.isEmpty();
}

QList<VCWidget *> VCFrame::children(bool recursive) const
{
    QList<VCWidget *> widgetsList;
    for (VCWidget *widget : m_childOrder)
    {
        if (m_pagesMap.contains(widget))
        {
            widgetsList.append(widget);
            if (recursive)
                if (VCFrame *frame = qobject_cast<VCFrame *>(widget))
                widgetsList.append(frame->children(true));
        }
    }
    return widgetsList;
}

QVariantList VCFrame::orderedChildIDs(int subpage) const
{
    QVariantList ids;
    for (VCWidget *child : children(false))
        if (m_pagesMap.value(child) == subpage)
            ids.append(child->id());
    return ids;
}

QRectF VCFrame::contentRect() const
{
    qreal top = 0;
    if (showHeader())
    {
        top = m_vc ? m_vc->pixelDensity() * 7 : 28;
        if (m_vc && m_vc->view() && m_vc->view()->rootObject())
        {
            QObject *root = m_vc->view()->rootObject();
            QQmlExpression expression(qmlContext(root), root, "UISettings.listItemHeight");
            QVariant height = expression.evaluate();
            if (!expression.hasError() && height.toReal() > 0)
                top = height.toReal();
            else
                qWarning() << Q_FUNC_INFO << "Cannot measure frame header" << expression.error();
        }
    }
    const QSizeF size = logicalGeometry().size();
    return QRectF(0, top, size.width(), qMax(qreal(0), size.height() - top));
}

void VCFrame::setChildOrder(int subpage, const QList<VCWidget *> &order)
{
    QList<VCWidget *> old;
    for (VCWidget *child : m_childOrder)
        if (m_pagesMap.value(child) == subpage)
            old.append(child);
    if (old == order)
        return;
    for (VCWidget *child : old)
        m_childOrder.removeAll(child);
    m_childOrder.append(order);
    setDocModified();
    emit childOrderChanged(subpage);
}

void VCFrame::inferChildOrder()
{
    const auto byRow = [](VCWidget *a, VCWidget *b) {
        const QRectF ar = a->logicalGeometry(), br = b->logicalGeometry();
        if (ar.y() != br.y()) return ar.y() < br.y();
        if (ar.x() != br.x()) return ar.x() < br.x();
        return a->id() < b->id();
    };
    const auto byColumn = [](VCWidget *a, VCWidget *b) {
        const QRectF ar = a->logicalGeometry(), br = b->logicalGeometry();
        if (ar.x() != br.x()) return ar.x() < br.x();
        if (ar.y() != br.y()) return ar.y() < br.y();
        return a->id() < b->id();
    };
    QList<VCWidget *> ordered;
    for (int page = 0; page < totalPagesNumber(); ++page)
    {
        QList<VCWidget *> candidates;
        for (VCWidget *child : m_childOrder)
            if (m_pagesMap.value(child) == page)
                candidates.append(child);
        if (qobject_cast<VCPage *>(this))
        {
            QList<VCWidget *> byX = candidates;
            std::sort(byX.begin(), byX.end(), byColumn);
            QList<QList<VCWidget *>> columns;
            qreal right = 0;
            bool valid = true;
            for (VCWidget *child : byX)
            {
                const QRectF rect = child->logicalGeometry();
                if (!qIsFinite(rect.left()) || !qIsFinite(rect.right()) ||
                    !qIsFinite(rect.top()) || !qIsFinite(rect.bottom()) ||
                    rect.width() <= 0 || rect.height() <= 0)
                {
                    valid = false;
                    break;
                }
                if (columns.isEmpty() || rect.left() >= right)
                {
                    columns.append(QList<VCWidget *>{child});
                    right = rect.right();
                }
                else
                {
                    columns.last().append(child);
                    right = qMax(right, rect.right());
                }
            }
            if (valid && columns.size() > 1)
            {
                for (auto &column : columns)
                {
                    std::sort(column.begin(), column.end(), byRow);
                    ordered.append(column);
                }
                continue;
            }
        }
        std::sort(candidates.begin(), candidates.end(), byRow);
        const qreal tolerance = 1;
        while (!candidates.isEmpty())
        {
            const qreal anchor = candidates.first()->logicalGeometry().y();
            QList<VCWidget *> row;
            while (!candidates.isEmpty() &&
                   candidates.first()->logicalGeometry().y() - anchor <= tolerance)
                row.append(candidates.takeFirst());
            std::sort(row.begin(), row.end(), byColumn);
            ordered.append(row);
        }
    }
    for (VCWidget *child : m_childOrder)
        if (!ordered.contains(child))
            ordered.append(child);
    m_childOrder = ordered;
}

VCWidget *VCFrame::addWidget(QQuickItem *parent, QString wType, QPoint pos)
{
    return addWidgetUnchecked(parent, wType, pos);
}

VCWidget *VCFrame::addWidgetUnchecked(QQuickItem *parent, QString wType, QPoint pos)
{
    VCWidget::WidgetType type = stringToType(wType);

    qDebug() << "[VCFrame] adding widget of type:" << wType << pos;

    if (m_vc->snapping())
    {
        pos.setX(qRound((qreal)pos.x() / m_vc->snappingSize()) * m_vc->snappingSize());
        pos.setY(qRound((qreal)pos.y() / m_vc->snappingSize()) * m_vc->snappingSize());
    }

    switch (type)
    {
        case FrameWidget:
        {
            VCFrame *frame = new VCFrame(m_doc, m_vc, this);
            QQmlEngine::setObjectOwnership(frame, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(frame);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, frame->id()));
            frame->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 50, m_vc->pixelDensity() * 50));
            setupWidget(frame, currentPage());
            frame->render(m_vc->view(), parent);
            return frame;
        }
        break;
        case SoloFrameWidget:
        {
            VCSoloFrame *soloframe = new VCSoloFrame(m_doc, m_vc, this);
            QQmlEngine::setObjectOwnership(soloframe, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(soloframe);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, soloframe->id()));
            soloframe->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 50, m_vc->pixelDensity() * 50));
            setupWidget(soloframe, currentPage());
            soloframe->render(m_vc->view(), parent);
            return soloframe;
        }
        break;
        case ButtonWidget:
        {
            VCButton *button = new VCButton(m_doc, this);
            QQmlEngine::setObjectOwnership(button, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(button);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, button->id()));
            button->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 17, m_vc->pixelDensity() * 17));
            setupWidget(button, currentPage());
            button->render(m_vc->view(), parent);
            return button;
        }
        break;
        case LabelWidget:
        {
            VCLabel *label = new VCLabel(m_doc, this);
            QQmlEngine::setObjectOwnership(label, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(label);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, label->id()));
            label->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 25, m_vc->pixelDensity() * 8));
            setupWidget(label, currentPage());
            label->render(m_vc->view(), parent);
            return label;
        }
        break;
        case SliderWidget:
        {
            VCSlider *slider = new VCSlider(m_doc, this);
            QQmlEngine::setObjectOwnership(slider, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(slider);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, slider->id()));
            if (wType == "Knob")
            {
                slider->setWidgetStyle(VCSlider::WKnob);
                slider->setCaption(slider->defaultCaption());
                slider->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 15, m_vc->pixelDensity() * 22));
            }
            else
                slider->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 15, m_vc->pixelDensity() * 40));
            setupWidget(slider, currentPage());
            slider->render(m_vc->view(), parent);
            return slider;
        }
        break;
        case AnimationWidget:
        {
            VCAnimation *animation = new VCAnimation(m_doc, this);
            QQmlEngine::setObjectOwnership(animation, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(animation);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, animation->id()));
            animation->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 60, m_vc->pixelDensity() * 45));
            setupWidget(animation, currentPage());
            animation->render(m_vc->view(), parent);
            return animation;
        }
        break;
        case AudioTriggersWidget:
        {
            VCAudioTriggers *audioTrigger = new VCAudioTriggers(m_doc, m_vc, this);
            QQmlEngine::setObjectOwnership(audioTrigger, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(audioTrigger);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, audioTrigger->id()));
            audioTrigger->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 50, m_vc->pixelDensity() * 50));
            setupWidget(audioTrigger, currentPage());
            audioTrigger->render(m_vc->view(), parent);
            return audioTrigger;
        }
        break;
        case XYPadWidget:
        {
            VCXYPad *xyPad = new VCXYPad(m_doc, this);
            QQmlEngine::setObjectOwnership(xyPad, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(xyPad);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, xyPad->id()));
            xyPad->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 50, m_vc->pixelDensity() * 50));
            setupWidget(xyPad, currentPage());
            xyPad->render(m_vc->view(), parent);
            return xyPad;
        }
        break;
        case SpeedWidget:
        {
            VCSpeedDial *speed = new VCSpeedDial(m_doc, this);
            QQmlEngine::setObjectOwnership(speed, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(speed);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, speed->id()));
            speed->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 80, m_vc->pixelDensity() * 50));
            setupWidget(speed, currentPage());
            speed->render(m_vc->view(), parent);
            return speed;
        }
        break;
        case ClockWidget:
        {
            VCClock *clock = new VCClock(m_doc, this);
            QQmlEngine::setObjectOwnership(clock, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(clock);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, clock->id()));
            clock->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 40, m_vc->pixelDensity() * 10));
            setupWidget(clock, currentPage());
            clock->render(m_vc->view(), parent);
            return clock;
        }
        break;
        case CueListWidget:
        {
            VCCueList *cuelist = new VCCueList(m_doc, this);
            QQmlEngine::setObjectOwnership(cuelist, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(cuelist);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, cuelist->id()));
            cuelist->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 80, m_vc->pixelDensity() * 50));
            setupWidget(cuelist, currentPage());
            cuelist->render(m_vc->view(), parent);
            return cuelist;
        }
        break;
        case RecordPanelWidget:
        {
            VCRecordPanel *recordPanel = new VCRecordPanel(m_doc, this);
            QQmlEngine::setObjectOwnership(recordPanel, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(recordPanel);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, recordPanel->id()));
            recordPanel->setGeometry(QRect(pos.x(), pos.y(), m_vc->pixelDensity() * 60, m_vc->pixelDensity() * 30));
            setupWidget(recordPanel, currentPage());
            recordPanel->render(m_vc->view(), parent);
            return recordPanel;
        }
        break;
        default:
        break;
    }

    return nullptr;
}

void VCFrame::addWidget(QQuickItem *parent, VCWidget *widget, QPoint pos)
{
    const qreal scale = qobject_cast<VCPage *>(this) ? qobject_cast<VCPage *>(this)->pageScale() : scaleFactor();
    QPointF logicalPosition = QPointF(pos) / scale;
    if (m_vc->snapping())
    {
        const qreal step = m_vc->snappingSize();
        if (step > 0)
        {
            logicalPosition.setX(std::round(logicalPosition.x() / step) * step);
            logicalPosition.setY(std::round(logicalPosition.y() / step) * step);
        }
    }

    QQmlEngine::setObjectOwnership(widget, QQmlEngine::CppOwnership);
    m_vc->addWidgetToMap(widget);
    Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                      Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, widget->id()));
    QRectF rect = widget->logicalGeometry();
    rect.moveTopLeft(logicalPosition);
    widget->setLogicalGeometry(rect);
    widget->setScaleFactor(scale);
    if (VCFrame *frame = qobject_cast<VCFrame *>(widget))
        for (VCWidget *child : frame->children(true))
            child->setScaleFactor(scale);
    addWidgetToPageMap(widget);
    checkSubmasterConnection(widget);
    widget->render(m_vc->view(), parent);
}

void VCFrame::addWidgetMatrix(QQuickItem *parent, QString matrixType, QPoint pos, QSize matrixSize, QSize widgetSize, bool soloFrame)
{
    QString error = m_vc->nativeAuthoringError(this);
    if (matrixSize.width() <= 0 || matrixSize.height() <= 0 ||
        widgetSize.width() <= 0 || widgetSize.height() <= 0 ||
        (matrixType != "buttonmatrix" && matrixType != "slidermatrix"))
        error = tr("Invalid widget matrix dimensions or type");
    VCWidget prototype(nullptr, this);
    prototype.setType(matrixType == "buttonmatrix" ? ButtonWidget : SliderWidget);
    prototype.setupLookAndFeel(m_vc->pixelDensity(), currentPage());
    const QSizeF minimum = prototype.minimumSize();
    if (widgetSize.width() < minimum.width() || widgetSize.height() < minimum.height())
        error = tr("Matrix controls are below their usable minimum size");
    const qreal density = m_vc->pixelDensity();
    const QSizeF extent(qMax(matrixSize.width() * qreal(widgetSize.width()) + density * 2, density * 20),
                         qMax(matrixSize.height() * qreal(widgetSize.height()) + density * 2, density * 20));
    QRectF matrixRect(QPointF(pos), extent);
    if (error.isEmpty())
    {
        const QVariantMap preview = m_vc->previewCreateGeometry(id(), soloFrame ? "Solo frame" : "Frame",
                                                               matrixRect, currentPage());
        if (!preview.value("accepted").toBool())
            error = preview.value("error").toString();
    }
    if (!error.isEmpty())
    {
        m_vc->reportEdit(m_vc->editResult(false, false, error));
        return;
    }
    VCFrame *frame;
    qreal totalWidth = extent.width(), totalHeight = extent.height();
    qreal yPos = density;

    qDebug() << "Matrix size" << matrixSize << "widget size" << widgetSize;
    qDebug() << "Frame total width" << totalWidth << ", height" << totalHeight;

    if (soloFrame)
    {
        VCSoloFrame *solo = new VCSoloFrame(m_doc, m_vc, this);
        frame = qobject_cast<VCFrame *>(solo);
    }
    else
    {
        frame = new VCFrame(m_doc, m_vc, this);
    }

    QQmlEngine::setObjectOwnership(frame, QQmlEngine::CppOwnership);
    m_vc->addWidgetToMap(frame);
    Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                      Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, frame->id()));
    frame->setLogicalGeometry(QRectF(pos.x(), pos.y(), totalWidth, totalHeight));
    frame->setShowHeader(false);
    setupWidget(frame, currentPage());

    for (int row = 0; row < matrixSize.height(); row++)
    {
        qreal xPos = density;

        for (int col = 0; col < matrixSize.width(); col++)
        {
            VCWidget *widget = frame->addWidgetUnchecked(nullptr, matrixType == "buttonmatrix" ? "Button" : "Slider",
                                                        QPointF(xPos, yPos).toPoint());
            if (!widget)
            {
                qCritical() << Q_FUNC_INFO << "Admitted matrix control type could not be constructed";
                m_vc->deleteVCWidgets({frame->id()});
                emit m_vc->editRejected(tr("Cannot construct the widget matrix"));
                return;
            }
            widget->setLogicalGeometry(QRectF(xPos, yPos, widgetSize.width(), widgetSize.height()));
            xPos += widgetSize.width();
        }
        yPos += widgetSize.height();
    }

    frame->render(m_vc->view(), parent);
}

void VCFrame::addWidgetsFromClipboard(QQuickItem *parent, QVariantList idsList, QPoint pos)
{
    const qreal scale = qobject_cast<VCPage *>(this)
        ? qobject_cast<VCPage *>(this)->pageScale() : scaleFactor();
    m_vc->reportEdit(m_vc->pasteWidgets(idsList, id(), QPointF(pos) / scale, parent));
}

void VCFrame::addFunctions(QQuickItem *parent, QVariantList idsList, QPoint pos, int keyModifiers)
{
    //qDebug() << "modifiers:" << QString::number(keyModifiers, 16);

    if (m_vc->snapping())
    {
        pos.setX(qRound((qreal)pos.x() / m_vc->snappingSize()) * m_vc->snappingSize());
        pos.setY(qRound((qreal)pos.y() / m_vc->snappingSize()) * m_vc->snappingSize());
    }

    QPoint currPos = pos;

    for (QVariant vID : idsList) // C++11
    {
        quint32 funcID = vID.toUInt();
        Function *func = m_doc->function(funcID);

        if (func == nullptr)
            continue;

        if (keyModifiers & Qt::ShiftModifier)
        {
            VCSlider *slider = new VCSlider(m_doc, this);
            QQmlEngine::setObjectOwnership(slider, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(slider);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, slider->id()));
            slider->setGeometry(QRect(currPos.x(), currPos.y(), m_vc->pixelDensity() * 15, m_vc->pixelDensity() * 40));
            slider->setCaption(func->name());
            slider->setControlledFunction(funcID);
            setupWidget(slider, currentPage());

            slider->render(m_vc->view(), parent);

            currPos.setX(currPos.x() + slider->geometry().width());
            if (currPos.x() >= geometry().width())
            {
                currPos.setX(pos.x());
                currPos.setY(currPos.y() + slider->geometry().height());
            }
        }
        else if (keyModifiers & Qt::ControlModifier)
        {
            Function *f = m_doc->function(funcID);
            if (f->type() != Function::ChaserType)
                return;

            VCCueList *cuelist = new VCCueList(m_doc, this);
            QQmlEngine::setObjectOwnership(cuelist, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(cuelist);

            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, cuelist->id()));
            cuelist->setGeometry(QRect(currPos.x(), currPos.y(), m_vc->pixelDensity() * 80, m_vc->pixelDensity() * 50));
            cuelist->setCaption(func->name());
            cuelist->setChaserID(funcID);
            setupWidget(cuelist, currentPage());

            cuelist->render(m_vc->view(), parent);

            currPos.setX(currPos.x() + cuelist->geometry().width());
            if (currPos.x() >= geometry().width())
            {
                currPos.setX(pos.x());
                currPos.setY(currPos.y() + cuelist->geometry().height());
            }
        }
        else
        {
            VCButton *button = new VCButton(m_doc, this);
            QQmlEngine::setObjectOwnership(button, QQmlEngine::CppOwnership);
            m_vc->addWidgetToMap(button);
            Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, this->id(), QVariant(),
                                              Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, button->id()));
            button->setGeometry(QRect(currPos.x(), currPos.y(), m_vc->pixelDensity() * 17, m_vc->pixelDensity() * 17));
            button->setCaption(func->name());
            button->setFunctionID(funcID);
            setupWidget(button, currentPage());

            button->render(m_vc->view(), parent);

            currPos.setX(currPos.x() + button->geometry().width());
            if (currPos.x() >= geometry().width())
            {
                currPos.setX(pos.x());
                currPos.setY(currPos.y() + button->geometry().height());
            }
        }
    }
}

void VCFrame::deleteChildren()
{
    if (m_pagesMap.isEmpty())
        return;

    QMapIterator <VCWidget*, int> it(m_pagesMap);
    while (it.hasNext() == true)
    {
        it.next();
        VCWidget *widget = it.key();
        if (widget->type() == FrameWidget || widget->type() == SoloFrameWidget)
        {
            VCFrame *frame = qobject_cast<VCFrame*>(widget);
            frame->deleteChildren();
        }
        /* Remove the widget from the frame pages map */
        m_pagesMap.remove(widget);
        m_childOrder.removeAll(widget);
        /* Remove it also from the global VC widgets map */
        m_vc->removeWidgetFromMap(widget);
        delete widget;
    }
}

void VCFrame::setupWidget(VCWidget *widget, int page)
{
    if (m_vc->loadStatus() != VirtualConsole::Loading)
    {
        widget->setupLookAndFeel(m_vc->pixelDensity(), page);
        /* a widget joining a frame page out of sight stays hidden, also when undo restores it */
        widget->setVisible(widget->page() == currentPage());
    }

    addWidgetToPageMap(widget);
    widget->setScaleFactor(qobject_cast<VCPage *>(this) ? qobject_cast<VCPage *>(this)->pageScale() : scaleFactor());
    if (VCFrame *frame = qobject_cast<VCFrame *>(widget))
        for (VCWidget *child : frame->children(true))
            child->setScaleFactor(widget->scaleFactor());

    if (widget->type() != VCWidget::SliderWidget ||
            qobject_cast<VCSlider *>(widget)->sliderMode() != VCSlider::SliderMode::Submaster)
    {
        widget->adjustIntensity(m_submasterValue * intensity());
    }

    checkSubmasterConnection(widget);
}

void VCFrame::checkSubmasterConnection(VCWidget *widget)
{
    if (widget->type() == VCWidget::SliderWidget)
    {
        VCSlider *slider = qobject_cast<VCSlider *>(widget);

        // always connect a slider in case it emits a submaster signal
        connect(slider, SIGNAL(submasterValueChanged(qreal)),
                this, SLOT(slotSubmasterValueChanged(qreal)), Qt::UniqueConnection);

        if (slider->sliderMode() == VCSlider::SliderMode::Submaster)
        {
            qreal value = qreal(slider->value()) / qreal(UCHAR_MAX);
            applySubmasterValue(value * slider->intensity(), slider);
        }
    }
}

void VCFrame::addWidgetToPageMap(VCWidget *widget)
{
    if (!m_childOrder.contains(widget))
        m_childOrder.append(widget);
    m_pagesMap.insert(widget, widget->page());
    if (VCSlider *slider = qobject_cast<VCSlider *>(widget))
        connect(slider, SIGNAL(submasterValueChanged(qreal)),
                this, SLOT(slotSubmasterValueChanged(qreal)), Qt::UniqueConnection);

    // if we're a normal Frame and we have a Solo Frame parent
    // then passthrough the widget functionStarting signal.
    // If we're not into a Solo Frame parent, then don't even connect
    // the signal, so each widget will know to immediately start the Function
    if (xmlTagName() == KXMLQLCVCFrame && hasSoloParent() == true)
    {
        qDebug() << "------ FRAME ----- connect";
        connect(widget, &VCWidget::functionStarting, this, &VCWidget::functionStarting, Qt::UniqueConnection);
    }

    // otherwise, if we're a Solo Frame, connect the widget
    // functionStarting signal to a slot to handle the event
    if (xmlTagName() == KXMLQLCVCSoloFrame)
    {
        qDebug() << "------ SOLO FRAME ----- connect";
        connect(widget, &VCWidget::functionStarting, this, &VCFrame::slotFunctionStarting, Qt::UniqueConnection);
    }
    emit childOrderChanged(widget->page());
}

void VCFrame::removeWidgetFromPageMap(VCWidget *widget)
{
    m_pagesMap.remove(widget);
    m_childOrder.removeAll(widget);
    if (VCSlider *slider = qobject_cast<VCSlider *>(widget))
        disconnect(slider, SIGNAL(submasterValueChanged(qreal)),
                   this, SLOT(slotSubmasterValueChanged(qreal)));

    // disconnect function start event. See addWidgetToPageMap
    if (xmlTagName() == KXMLQLCVCFrame && hasSoloParent() == true)
    {
        qDebug() << "------ FRAME --//--- disconnect";
        disconnect(widget, &VCWidget::functionStarting, this, &VCWidget::functionStarting);
    }

    if (xmlTagName() == KXMLQLCVCSoloFrame)
    {
        qDebug() << "------ SOLO FRAME --//--- disconnect";
        disconnect(widget, &VCWidget::functionStarting, this, &VCFrame::slotFunctionStarting);
    }
    emit childOrderChanged(widget->page());
}

void VCFrame::refreshFunctionRouting()
{
    for (VCWidget *child : children(false))
    {
        disconnect(child, &VCWidget::functionStarting, this, &VCWidget::functionStarting);
        disconnect(child, &VCWidget::functionStarting, this, &VCFrame::slotFunctionStarting);
        if (type() == SoloFrameWidget)
            connect(child, &VCWidget::functionStarting, this, &VCFrame::slotFunctionStarting, Qt::UniqueConnection);
        else if (hasSoloParent())
            connect(child, &VCWidget::functionStarting, this, &VCWidget::functionStarting, Qt::UniqueConnection);
        if (VCFrame *frame = qobject_cast<VCFrame *>(child))
            frame->refreshFunctionRouting();
    }
}

QList<VCWidget *> VCFrame::reconcileSubmasters(const QList<VCWidget *> &arrivals)
{
    VCSlider *driver = qobject_cast<VCSlider *>(m_submasterDriver.data());
    if (!driver || !m_pagesMap.contains(driver) || driver->sliderMode() != VCSlider::Submaster)
    {
        driver = nullptr;
        for (VCWidget *child : children(false))
            if (VCSlider *slider = qobject_cast<VCSlider *>(child))
                if (slider->sliderMode() == VCSlider::Submaster)
                    driver = slider;
    }
    for (VCWidget *child : arrivals)
        if (VCSlider *slider = qobject_cast<VCSlider *>(child))
            if (slider->sliderMode() == VCSlider::Submaster)
                driver = slider;
    const qreal value = driver ? qreal(driver->value()) / UCHAR_MAX * driver->intensity() : 1;
    const bool changed = value != m_submasterValue;
    m_submasterDriver = driver;
    m_submasterValue = value;
    QList<VCWidget *> effects;
    for (VCWidget *child : changed ? children(false) : arrivals)
    {
        VCSlider *slider = qobject_cast<VCSlider *>(child);
        if (!slider || slider->sliderMode() != VCSlider::Submaster)
            effects.append(child);
    }
    return effects;
}

bool VCFrame::moveWidgetToPage(VCWidget *widget, int pageIndex)
{
    if (widget == nullptr || widget->parent() != this ||
        pageIndex < 0 || pageIndex >= totalPagesNumber())
        return false;

    const int previousPage = widget->page();
    if (previousPage == pageIndex)
        return true;
    m_pagesMap.remove(widget);
    m_childOrder.removeAll(widget);
    widget->setPage(pageIndex);
    widget->remapInputSources(pageIndex);
    m_pagesMap.insert(widget, pageIndex);
    m_childOrder.append(widget);
    widget->setVisible(pageIndex == currentPage());
    widget->updateFeedback();
    setDocModified();
    emit childOrderChanged(previousPage);
    emit childOrderChanged(pageIndex);
    return true;
}


/*********************************************************************
 * Disable state
 *********************************************************************/

void VCFrame::setDisabled(bool disable)
{
    /* The enable button also works while operating: only editing makes history */
    if (disable != isDisabled() && m_vc && m_vc->editMode())
        enqueueTardisAction(Tardis::VCWidgetDisabled, QVariant(isDisabled()), QVariant(disable));
    for (VCWidget *widget : children(true)) // C++11
        widget->setDisabled(disable);

    VCWidget::setDisabled(disable);

    updateFeedback();
}
/*********************************************************************
 * Header
 *********************************************************************/

bool VCFrame::showHeader() const
{
    return m_showHeader;
}

void VCFrame::setShowHeader(bool showHeader)
{
    if (m_showHeader == showHeader)
        return;

    enqueueTardisAction(Tardis::VCFrameShowHeader, QVariant(m_showHeader), QVariant(showHeader));
    m_showHeader = showHeader;
    emit showHeaderChanged(showHeader);
}

/*********************************************************************
 * Enable button
 *********************************************************************/

bool VCFrame::showEnable() const
{
    return m_showEnable;
}

void VCFrame::setShowEnable(bool showEnable)
{
    if (m_showEnable == showEnable)
        return;

    enqueueTardisAction(Tardis::VCFrameShowEnable, QVariant(m_showEnable), QVariant(showEnable));
    m_showEnable = showEnable;
    emit showEnableChanged(showEnable);
}

/*********************************************************************
 * Collapsed state
 *********************************************************************/

bool VCFrame::isCollapsed() const
{
    return m_isCollapsed;
}

void VCFrame::setCollapsed(bool isCollapsed)
{
    if (m_isCollapsed == isCollapsed)
        return;

    /* Operators fold frames during a show: only editing makes history */
    if (m_vc && m_vc->editMode())
        enqueueTardisAction(Tardis::VCFrameCollapsed, QVariant(m_isCollapsed), QVariant(isCollapsed));
    m_isCollapsed = isCollapsed;
    emit collapsedChanged(isCollapsed);
    setDocModified();
}

/*********************************************************************
 * Multi page mode
 *********************************************************************/

bool VCFrame::multiPageMode() const
{
    return m_multiPageMode;
}

void VCFrame::setMultiPageMode(bool multiPageMode)
{
    if (m_multiPageMode == multiPageMode)
        return;

    enqueueTardisAction(Tardis::VCFrameMultiPage, QVariant(m_multiPageMode), QVariant(multiPageMode));
    m_multiPageMode = multiPageMode;

    // when enabling, make sure the label and shortcut control for the
    // first page (index 0) exist
    if (multiPageMode)
        ensureFirstPage();

    // without pages only the first one shows
    for (VCWidget *child : children(false))
        child->setVisible(child->page() == currentPage());

    emit multiPageModeChanged(multiPageMode);
    setDocModified();
}

void VCFrame::ensureFirstPage()
{
    if (m_pageLabels.contains(0))
        return;

    // register the label and shortcut control for the first page (index 0).
    // Further pages are added on demand by setTotalPagesNumber()
    m_pageLabels.insert(0, tr("Page 1"));
    registerExternalControl(INPUT_SHORTCUT_BASE_ID, m_pageLabels.value(0), true);
    emit pageLabelsChanged();
}

void VCFrame::setTotalPagesNumber(int num)
{
    if (m_totalPagesNumber == num)
        return;

    enqueueTardisAction(Tardis::VCFramePagesNumber, QVariant(m_totalPagesNumber), QVariant(num));

    // pages rely on the first page (index 0) being registered
    ensureFirstPage();

    if (num < m_totalPagesNumber)
    {
        for (int i = m_totalPagesNumber - 1; i >= num; i--)
        {
            m_pageLabels.remove(i);
            unregisterExternalControl(INPUT_SHORTCUT_BASE_ID + i);
        }
    }
    else
    {
        for (int i = m_totalPagesNumber; i < num; i++)
        {
            QString name = tr("Page %1").arg(i + 1);
            m_pageLabels.insert(i, name);
            registerExternalControl(INPUT_SHORTCUT_BASE_ID + i, name, true);
        }
    }

    m_totalPagesNumber = num;
    setDocModified();
    emit totalPagesNumberChanged(num);
    emit pageLabelsChanged();

    // widgets of removed pages stay, hidden, until pages are added back
    if (m_currentPage >= num)
        setCurrentPage(num - 1);
}

int VCFrame::totalPagesNumber() const
{
    return m_totalPagesNumber;
}

int VCFrame::currentPage() const
{
    if (m_multiPageMode == false)
        return 0;
    return m_currentPage;
}

void VCFrame::setCurrentPage(int pageNum)
{
    if (pageNum < 0 || pageNum >= m_totalPagesNumber)
        return;

    m_currentPage = pageNum;

    QMapIterator <VCWidget*, int> it(m_pagesMap);
    while (it.hasNext() == true)
    {
        it.next();
        int page = it.value();
        VCWidget *widget = it.key();
        if (page == m_currentPage)
        {
            widget->setVisible(true);
        }
        else
        {
            widget->setVisible(false);
        }
    }

    updateFeedback();

    setDocModified();
    emit currentPageChanged(m_currentPage);
}

void VCFrame::setPagesLoop(bool pagesLoop)
{
    if (m_pagesLoop == pagesLoop)
        return;

    enqueueTardisAction(Tardis::VCFramePagesLoop, QVariant(m_pagesLoop), QVariant(pagesLoop));
    m_pagesLoop = pagesLoop;
    setDocModified();
    emit pagesLoopChanged(pagesLoop);
}

bool VCFrame::pagesLoop() const
{
    return m_pagesLoop;
}

QStringList VCFrame::pageLabels() const
{
    return m_pageLabels.values();
}

void VCFrame::setShortcutName(int pageIndex, QString name)
{
    m_pageLabels[pageIndex] = name;

    // Keep the registered external (input/key) control description in sync with
    // the page name, so a bound input channel shows the current page label.
    // Re-registering with the same id updates the entry in place; the id is what
    // input sources reference, so existing bindings are preserved.
    registerExternalControl(INPUT_SHORTCUT_BASE_ID + pageIndex, name, true);

    setDocModified();

    emit pageLabelsChanged();
}

void VCFrame::gotoPreviousPage()
{
    if (m_pagesLoop && m_currentPage == 0)
        setCurrentPage(m_totalPagesNumber - 1);
    else
        setCurrentPage(m_currentPage - 1);

    sendFeedback(m_currentPage, INPUT_PREVIOUS_PAGE_ID);
}

void VCFrame::gotoNextPage()
{
    if (m_pagesLoop && m_currentPage == m_totalPagesNumber - 1)
        setCurrentPage(0);
    else
        setCurrentPage(m_currentPage + 1);

    sendFeedback(m_currentPage, INPUT_NEXT_PAGE_ID);
}

void VCFrame::gotoPage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= m_totalPagesNumber)
        return;

    setCurrentPage(pageIndex);
}

void VCFrame::cloneFirstPage()
{
    if (m_totalPagesNumber == 1)
        return;

    Tardis::instance()->beginBatch("cloneFirstPage");
    for (int pg = 1; pg < totalPagesNumber(); pg++)
    {
        QListIterator <VCWidget*> it(this->findChildren<VCWidget*>());
        while (it.hasNext() == true)
        {
            VCWidget* child = it.next();
            if (child->page() == 0 && child->parent() == this)
            {
                VCWidget *newWidget = child->createCopy(this);
                m_vc->addWidgetToMap(newWidget);
                newWidget->setPage(pg);
                newWidget->remapInputSources(pg);

                setupWidget(newWidget, pg);
                newWidget->render(m_vc->view(), m_item);
                Tardis::instance()->enqueueAction(Tardis::VCWidgetCreate, id(), QVariant(),
                                                  Tardis::instance()->actionToByteArray(Tardis::VCWidgetCreate, newWidget->id()));
            }
        }
    }
    Tardis::instance()->endBatch();
}

/*********************************************************************
 * PIN
 *********************************************************************/

int VCFrame::PIN() const
{
    return m_PIN;
}

void VCFrame::setPIN(int newPIN)
{
    if (newPIN == m_PIN)
        return;

    m_PIN = newPIN;
    setDocModified();
    emit PINChanged(newPIN);
}

void VCFrame::validatePIN()
{
    m_validatedPIN = true;
}

bool VCFrame::requirePIN() const
{
    if (m_PIN == 0 || m_validatedPIN == true)
        return false;

    return true;
}

/*********************************************************************
 * Widget Function
 *********************************************************************/

void VCFrame::slotFunctionStarting(VCWidget *widget, quint32 fid, qreal fIntensity)
{
    Q_UNUSED(widget)
    Q_UNUSED(fid)
    Q_UNUSED(fIntensity)

    if (xmlTagName() == KXMLQLCVCFrame)
        qDebug() << "[VCFrame] ERROR ! This should never happen!";
}

/*********************************************************************
 * Submasters
 *********************************************************************/

void VCFrame::adjustIntensity(qreal intensity)
{
    VCWidget::adjustIntensity(intensity);

    if (isDisabled())
        return;

    QListIterator <VCWidget*> it(this->findChildren<VCWidget*>());
    while (it.hasNext() == true)
    {
        VCWidget* child = it.next();
        if (child != nullptr && child->parent() == this)
        {
            if (child->type() == WidgetType::SliderWidget)
            {
                VCSlider* slider = reinterpret_cast<VCSlider*>(child);
                if (slider != nullptr && slider->sliderMode() == VCSlider::SliderMode::Submaster)
                    continue;
            }

            child->adjustIntensity(m_submasterValue * intensity);
        }
    }
}

void VCFrame::slotSubmasterValueChanged(qreal submasterValue)
{
    qDebug() << Q_FUNC_INFO << "val:" << submasterValue;
    applySubmasterValue(submasterValue, qobject_cast<VCWidget *>(sender()));
}

void VCFrame::applySubmasterValue(qreal submasterValue, VCWidget *submaster)
{
    m_submasterValue = submasterValue;
    if (submaster)
        m_submasterDriver = submaster;

    QListIterator <VCWidget*> it(this->findChildren<VCWidget*>());
    while (it.hasNext() == true)
    {
        VCWidget* child = it.next();
        if (child != nullptr && child->parent() == this && child != submaster)
            child->adjustIntensity(submasterValue * intensity());
    }
}

/*********************************************************************
 * Arrangement
 *********************************************************************/

VCFrame::ArrangeMode VCFrame::arrangeMode() const
{
    return m_arrangeMode;
}

void VCFrame::setArrangeMode(ArrangeMode mode)
{
    if (m_arrangeMode == mode)
        return;
    enqueueTardisAction(Tardis::VCFrameArrangeMode, int(m_arrangeMode), int(mode));
    m_arrangeMode = mode;
    setDocModified();
    emit arrangeChanged();
}

qreal VCFrame::arrangeGap() const
{
    return m_arrangeGap;
}

void VCFrame::setArrangeGap(qreal gap)
{
    if (m_arrangeGap == gap || gap < 0)
        return;
    enqueueTardisAction(Tardis::VCFrameArrangeGap, m_arrangeGap, gap);
    m_arrangeGap = gap;
    setDocModified();
    emit arrangeChanged();
}

int VCFrame::arrangeColumns() const
{
    return m_arrangeColumns;
}

void VCFrame::setArrangeColumns(int columns)
{
    if (m_arrangeColumns == columns || columns < 1)
        return;
    enqueueTardisAction(Tardis::VCFrameArrangeColumns, m_arrangeColumns, columns);
    m_arrangeColumns = columns;
    setDocModified();
    emit arrangeChanged();
}

bool VCFrame::arrangeFill() const
{
    return m_arrangeFill;
}

void VCFrame::setArrangeFill(bool fill)
{
    if (m_arrangeFill == fill)
        return;
    enqueueTardisAction(Tardis::VCFrameArrangeFill, m_arrangeFill, fill);
    m_arrangeFill = fill;
    setDocModified();
    emit arrangeChanged();
}

/*********************************************************************
 * Grid Layout
 *********************************************************************/

VCFrame::LayoutMode VCFrame::layoutMode() const
{
    return m_layoutMode;
}

void VCFrame::setLayoutMode(LayoutMode mode)
{
    if (m_layoutMode == mode)
        return;

    enqueueTardisAction(Tardis::VCFrameLayoutMode,
                        QVariant((int)m_layoutMode), QVariant((int)mode));
    m_layoutMode = mode;
    emit layoutModeChanged();
    setDocModified();
}

int VCFrame::gridColumns() const
{
    return m_gridColumns;
}

void VCFrame::setGridColumns(int columns)
{
    if (columns <= 0)
        columns = 12;

    if (m_gridColumns == columns)
        return;

    enqueueTardisAction(Tardis::VCFrameGridColumns,
                        QVariant(m_gridColumns), QVariant(columns));
    m_gridColumns = columns;
    emit gridColumnsChanged();
    setDocModified();
}

int VCFrame::gridRowHeight() const
{
    return m_gridRowHeight;
}

void VCFrame::setGridRowHeight(int rowHeight)
{
    if (rowHeight < 0)
        rowHeight = 0;

    if (m_gridRowHeight == rowHeight)
        return;

    enqueueTardisAction(Tardis::VCFrameGridRowHeight,
                        QVariant(m_gridRowHeight), QVariant(rowHeight));
    m_gridRowHeight = rowHeight;
    emit gridRowHeightChanged();
    setDocModified();
}

bool VCFrame::gridCompact() const
{
    return m_gridCompact;
}

void VCFrame::setGridCompact(bool compact)
{
    if (m_gridCompact == compact)
        return;

    enqueueTardisAction(Tardis::VCFrameGridCompact,
                        QVariant(m_gridCompact), QVariant(compact));
    m_gridCompact = compact;
    emit gridCompactChanged();
    setDocModified();
}

/*********************************************************************
 * External input
 *********************************************************************/

void VCFrame::updateFeedback()
{
    if (isDisabled())
    {
        // temporarily revert the disabled state otherwise
        // this feedback will never go through
        m_isDisabled = false;
        sendFeedback(0, INPUT_ENABLE_ID, VCWidget::LowerValue);
        m_isDisabled = true;
    }
    else
    {
        sendFeedback(UCHAR_MAX, INPUT_ENABLE_ID, VCWidget::UpperValue);
    }

    QListIterator <VCWidget*> it(this->findChildren<VCWidget*>());
    while (it.hasNext() == true)
    {
        VCWidget* child = it.next();
        if (child->parent() == this && child->page() == currentPage())
            child->updateFeedback();
    }

    for (int &pIdx : m_pageLabels.keys())
    {
        if (pIdx == m_currentPage)
            sendFeedback(UCHAR_MAX, INPUT_SHORTCUT_BASE_ID + pIdx, VCWidget::UpperValue);
        else
            sendFeedback(0, INPUT_SHORTCUT_BASE_ID + pIdx, VCWidget::LowerValue);
    }
}

void VCFrame::slotInputValueChanged(quint8 id, uchar value)
{
    if (value != UCHAR_MAX)
        return;

    switch(id)
    {
        case INPUT_NEXT_PAGE_ID:
            gotoNextPage();
        break;
        case INPUT_PREVIOUS_PAGE_ID:
            gotoPreviousPage();
        break;
        case INPUT_ENABLE_ID:
            setDisabled(isDisabled() ? false : true);
        break;
        case INPUT_COLLAPSE_ID:
            setCollapsed(!isCollapsed());
        break;
        default:
            if (id < INPUT_SHORTCUT_BASE_ID || id > INPUT_SHORTCUT_BASE_ID + m_totalPagesNumber)
                break;
            setCurrentPage(id - INPUT_SHORTCUT_BASE_ID);
        break;
    }
}

/*****************************************************************************
 * Load & Save
 *****************************************************************************/

QString VCFrame::xmlTagName() const
{
    return KXMLQLCVCFrame;
}

bool VCFrame::loadWidgetXML(QXmlStreamReader &root, bool render)
{
    if (root.name() == KXMLQLCVCFrame)
    {
        /* Create a new frame into its parent */
        VCFrame* frame = new VCFrame(m_doc, m_vc, this);

        if (frame->loadXML(root) == false)
            delete frame;
        else
        {
            QQmlEngine::setObjectOwnership(frame, QQmlEngine::CppOwnership);
            setupWidget(frame, frame->page());
            m_vc->addWidgetToMap(frame);
            if (render && m_item)
                frame->render(m_vc->view(), m_item);
        }
    }
    else if (root.name() == KXMLQLCVCSoloFrame)
    {
        /* Create a new frame into its parent */
        VCSoloFrame *soloframe = new VCSoloFrame(m_doc, m_vc, this);
        if (soloframe->loadXML(root) == false)
            delete soloframe;
        else
        {
            QQmlEngine::setObjectOwnership(soloframe, QQmlEngine::CppOwnership);
            setupWidget(soloframe, soloframe->page());
            m_vc->addWidgetToMap(soloframe);
            if (render && m_item)
                soloframe->render(m_vc->view(), m_item);
        }
    }
    else if (root.name() == KXMLQLCVCButton)
    {
        /* Create a new button into its parent */
        VCButton *button = new VCButton(m_doc, this);
        if (button->loadXML(root) == false)
            delete button;
        else
        {
            QQmlEngine::setObjectOwnership(button, QQmlEngine::CppOwnership);
            setupWidget(button, button->page());
            m_vc->addWidgetToMap(button);
            if (render && m_item)
                button->render(m_vc->view(), m_item);
        }
    }
    else if (root.name() == KXMLQLCVCLabel)
    {
        /* Create a new label into its parent */
        VCLabel *label = new VCLabel(m_doc, this);
        if (label->loadXML(root) == false)
            delete label;
        else
        {
            QQmlEngine::setObjectOwnership(label, QQmlEngine::CppOwnership);
            setupWidget(label, label->page());
            m_vc->addWidgetToMap(label);
            if (render && m_item)
                label->render(m_vc->view(), m_item);
        }
    }
    else if (root.name() == KXMLQLCVCSlider)
    {
        /* Create a new slider into its parent */
        VCSlider *slider = new VCSlider(m_doc, this);
        if (slider->loadXML(root) == false)
            delete slider;
        else
        {
            QQmlEngine::setObjectOwnership(slider, QQmlEngine::CppOwnership);
            setupWidget(slider, slider->page());
            m_vc->addWidgetToMap(slider);
            if (render && m_item)
                slider->render(m_vc->view(), m_item);
        }
    }
    else if (root.name() == KXMLQLCVCAnimation)
    {
        /* Create a new clock into its parent */
        VCAnimation *animation = new VCAnimation(m_doc, this);
        if (animation->loadXML(root) == false)
            delete animation;
        else
        {
            QQmlEngine::setObjectOwnership(animation, QQmlEngine::CppOwnership);
            setupWidget(animation, animation->page());
            m_vc->addWidgetToMap(animation);
            if (render && m_item)
                animation->render(m_vc->view(), m_item);
        }
    }
    else if (root.name() == KXMLQLCVCAudioTriggers)
    {
        /* Create a new clock into its parent */
        VCAudioTriggers *audioTrigger = new VCAudioTriggers(m_doc, m_vc, this);
        if (audioTrigger->loadXML(root) == false)
            delete audioTrigger;
        else
        {
            QQmlEngine::setObjectOwnership(audioTrigger, QQmlEngine::CppOwnership);
            setupWidget(audioTrigger, audioTrigger->page());
            m_vc->addWidgetToMap(audioTrigger);
            if (render && m_item)
                audioTrigger->render(m_vc->view(), m_item);
        }
    }
    else if (root.name() == KXMLQLCVCSpeedDial)
    {
        /* Create a new speedDial into its parent */
        VCSpeedDial *speedDial = new VCSpeedDial(m_doc, this);
        if (speedDial->loadXML(root) == false)
            delete speedDial;
        else
        {
            QQmlEngine::setObjectOwnership(speedDial, QQmlEngine::CppOwnership);
            setupWidget(speedDial, speedDial->page());
            m_vc->addWidgetToMap(speedDial);
            if (render && m_item)
                speedDial->render(m_vc->view(), m_item);
        }
    }
    else if (root.name() == KXMLQLCVCXYPad)
    {
        /* Create a new xyPad into its parent */
        VCXYPad *xyPad = new VCXYPad(m_doc, this);
        if (xyPad->loadXML(root) == false)
            delete xyPad;
        else
        {
            QQmlEngine::setObjectOwnership(xyPad, QQmlEngine::CppOwnership);
            setupWidget(xyPad, xyPad->page());
            m_vc->addWidgetToMap(xyPad);
            if (render && m_item)
                xyPad->render(m_vc->view(), m_item);
        }
    }
    else if (root.name() == KXMLQLCVCClock)
    {
        /* Create a new clock into its parent */
        VCClock *clock = new VCClock(m_doc, this);
        if (clock->loadXML(root) == false)
            delete clock;
        else
        {
            QQmlEngine::setObjectOwnership(clock, QQmlEngine::CppOwnership);
            setupWidget(clock, clock->page());
            m_vc->addWidgetToMap(clock);
            if (render && m_item)
                clock->render(m_vc->view(), m_item);
        }
    }
    else if (root.name() == KXMLQLCVCCueList)
    {
        /* Create a new cue list into its parent */
        VCCueList *cuelist = new VCCueList(m_doc, this);
        if (cuelist->loadXML(root) == false)
            delete cuelist;
        else
        {
            QQmlEngine::setObjectOwnership(cuelist, QQmlEngine::CppOwnership);
            setupWidget(cuelist, cuelist->page());
            m_vc->addWidgetToMap(cuelist);
            if (render && m_item)
                cuelist->render(m_vc->view(), m_item);
        }
    }
    else if (root.name() == KXMLQLCVCRecordPanel)
    {
        VCRecordPanel *recordPanel = new VCRecordPanel(m_doc, this);
        if (recordPanel->loadXML(root) == false)
            delete recordPanel;
        else
        {
            QQmlEngine::setObjectOwnership(recordPanel, QQmlEngine::CppOwnership);
            setupWidget(recordPanel, recordPanel->page());
            m_vc->addWidgetToMap(recordPanel);
            if (render && m_item)
                recordPanel->render(m_vc->view(), m_item);
        }
    }
    else
    {
        return false;
    }

    return true;
}

bool VCFrame::loadXML(QXmlStreamReader &root)
{
    if (root.name() != xmlTagName())
    {
        qWarning() << Q_FUNC_INFO << "Frame node not found";
        return false;
    }

    /* Widget commons */
    loadXMLCommon(root);

    int currentPage = 0;
    const bool storedOrder = root.attributes().value(KXMLQLCVCFrameChildOrder) == QStringLiteral("1");

    /* Children */
    while (root.readNextStartElement())
    {
        if (root.name() == KXMLQLCWindowState)
        {
            /* Frame geometry (visibility is ignored) */
            qreal x = 0, y = 0, w = 0, h = 0;
            bool visible = false;
            loadXMLWindowState(root, &x, &y, &w, &h, &visible);
            setLogicalGeometry(QRectF(x, y, w, h));
        }
        else if (root.name() == KXMLQLCVCWidgetAppearance)
        {
            /* Frame appearance */
            loadXMLAppearance(root);
        }
        else if (root.name() == KXMLQLCVCFrameShowHeader)
        {
            if (root.readElementText() == KXMLQLCTrue)
                setShowHeader(true);
            else
                setShowHeader(false);
        }
        else if (root.name() == KXMLQLCVCFrameIsCollapsed)
        {
            /* Collapsed */
            if (root.readElementText() == KXMLQLCTrue)
                setCollapsed(true);
        }
        else if (root.name() == KXMLQLCVCFrameShowEnableButton)
        {
            if (root.readElementText() == KXMLQLCTrue)
                setShowEnable(true);
            else
                setShowEnable(false);
        }
        else if (this->type() == SoloFrameWidget && root.name() == KXMLQLCVCSoloFrameMixing)
        {
            if (root.readElementText() == KXMLQLCTrue)
                reinterpret_cast<VCSoloFrame*>(this)->setSoloframeMixing(true);
            else
                reinterpret_cast<VCSoloFrame*>(this)->setSoloframeMixing(false);
        }
        else if (this->type() == SoloFrameWidget && root.name() == KXMLQLCVCSoloFrameExclude)
        {
            if (root.readElementText() == KXMLQLCTrue)
                reinterpret_cast<VCSoloFrame*>(this)->setExcludeMonitoredFunctions(true);
            else
                reinterpret_cast<VCSoloFrame*>(this)->setExcludeMonitoredFunctions(false);
        }
        else if (root.name() == KXMLQLCVCFramePIN)
        {
            SimpleCrypt crypter(encKey);
            QString decPin = crypter.decryptToString(root.readElementText());
            setPIN(decPin.toInt());
        }
        else if (root.name() == KXMLQLCVCFrameMultipage)
        {
            setMultiPageMode(true);
            QXmlStreamAttributes attrs = root.attributes();
            if (attrs.hasAttribute(KXMLQLCVCFramePagesNumber))
                setTotalPagesNumber(attrs.value(KXMLQLCVCFramePagesNumber).toInt());

            if (attrs.hasAttribute(KXMLQLCVCFrameCurrentPage))
                currentPage = attrs.value(KXMLQLCVCFrameCurrentPage).toInt();

            if (attrs.hasAttribute(KXMLQLCVCFramePagesLoop))
                setPagesLoop(true);

            root.skipCurrentElement();
        }
#if 0
        else if (root.name() == KXMLQLCVCSoloFrameMixing && this->type() == SoloFrameWidget)
        {
            if (root.readElementText() == KXMLQLCTrue)
                reinterpret_cast<VCSoloFrame*>(this)->setSoloframeMixing(true);
            else
                reinterpret_cast<VCSoloFrame*>(this)->setSoloframeMixing(false);
        }
#endif
        else if (root.name() == KXMLQLCVCFrameEnableSource)
        {
            loadXMLSources(root, INPUT_ENABLE_ID);
        }
        else if (root.name() == KXMLQLCVCFrameNext)
        {
            loadXMLSources(root, INPUT_NEXT_PAGE_ID);
        }
        else if (root.name() == KXMLQLCVCFramePrevious)
        {
            loadXMLSources(root, INPUT_PREVIOUS_PAGE_ID);
        }
        else if (root.name() == KXMLQLCVCFramePagesLoop) // LEGACY
        {
            if (root.readElementText() == KXMLQLCTrue)
                setPagesLoop(true);
            else
                setPagesLoop(false);
        }
        else if (root.name() == KXMLQLCVCFrameShortcut)
        {
            int page = 0;
            QString name;

            QXmlStreamAttributes attrs = root.attributes();
            if (attrs.hasAttribute(KXMLQLCVCFrameShortcutPage))
                page = attrs.value(KXMLQLCVCFrameShortcutPage).toInt();

            if (attrs.hasAttribute(KXMLQLCVCFrameShortcutName))
                name = attrs.value(KXMLQLCVCFrameShortcutName).toString();
            m_pageLabels.insert(page, name);

            registerExternalControl(INPUT_SHORTCUT_BASE_ID + page, name, true);
            loadXMLSources(root, INPUT_SHORTCUT_BASE_ID + page);

            //root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCVCFrameArrange)
        {
            const QXmlStreamAttributes attrs = root.attributes();
            const QStringList modes {"None", "Row", "Column", "Grid", "Flow"};
            m_arrangeMode = ArrangeMode(qMax(0, int(modes.indexOf(attrs.value(KXMLQLCVCFrameArrangeMode).toString()))));
            if (attrs.hasAttribute(KXMLQLCVCFrameArrangeGap))
                m_arrangeGap = qMax(qreal(0), attrs.value(KXMLQLCVCFrameArrangeGap).toDouble());
            if (attrs.hasAttribute(KXMLQLCVCFrameArrangeColumns))
                m_arrangeColumns = qMax(1, attrs.value(KXMLQLCVCFrameArrangeColumns).toInt());
            m_arrangeFill = attrs.value(KXMLQLCVCFrameArrangeFill).toString() == KXMLQLCTrue;
            root.skipCurrentElement();
        }
        else if (root.name() == KXMLQLCVCFrameGridLayout)
        {
            QXmlStreamAttributes attrs = root.attributes();
            QString mode = attrs.value(KXMLQLCVCFrameGridMode).toString();
            m_layoutMode = (mode == KXMLQLCVCFrameGridModeGrid) ? LayoutGrid : LayoutFree;

            if (attrs.hasAttribute(KXMLQLCVCFrameGridColumns))
            {
                int cols = attrs.value(KXMLQLCVCFrameGridColumns).toInt();
                m_gridColumns = (cols > 0) ? cols : 12;
            }

            if (attrs.hasAttribute(KXMLQLCVCFrameGridRowHeight))
            {
                int rh = attrs.value(KXMLQLCVCFrameGridRowHeight).toInt();
                m_gridRowHeight = (rh >= 0) ? rh : 0;
            }

            if (attrs.hasAttribute(KXMLQLCVCFrameGridCompact))
                m_gridCompact = (attrs.value(KXMLQLCVCFrameGridCompact).toString() != KXMLQLCFalse);

            root.skipCurrentElement();
        }
        else
        {
            if (loadExtraXML(root) == false && loadWidgetXML(root) == false)
            {
                qWarning() << Q_FUNC_INFO << "Unknown frame tag:" << root.name().toString();
                root.skipCurrentElement();
            }
        }
    }

    if (multiPageMode() == true)
        setCurrentPage(currentPage);
    if (!storedOrder)
        inferChildOrder();

    return true;
}

bool VCFrame::saveXML(QXmlStreamWriter *doc) const
{
    Q_ASSERT(doc != nullptr);

    /* VC Frame entry */
    doc->writeStartElement(xmlTagName());
    doc->writeAttribute(KXMLQLCVCFrameChildOrder, QStringLiteral("1"));

    saveXMLCommon(doc);

    /* Save appearance */
    saveXMLAppearance(doc);

    /* Save widget proportions only for child frames */
    saveXMLWindowState(doc);

    /* Allow resize */
    doc->writeTextElement(KXMLQLCVCFrameAllowResize, allowResize() ? KXMLQLCTrue : KXMLQLCFalse);

    /* ShowHeader */
    doc->writeTextElement(KXMLQLCVCFrameShowHeader, showHeader() ? KXMLQLCTrue : KXMLQLCFalse);

    /* ShowEnableButton */
    doc->writeTextElement(KXMLQLCVCFrameShowEnableButton, showEnable() ? KXMLQLCTrue : KXMLQLCFalse);

    /* Solo frame mixing and monitored functions exclusion */
    if (this->type() == SoloFrameWidget)
    {
        const VCSoloFrame *solo = reinterpret_cast<const VCSoloFrame*>(this);
        if (solo->soloframeMixing())
            doc->writeTextElement(KXMLQLCVCSoloFrameMixing, KXMLQLCTrue);

        if (solo->excludeMonitoredFunctions())
            doc->writeTextElement(KXMLQLCVCSoloFrameExclude, KXMLQLCTrue);
    }

    /* Collapsed */
    doc->writeTextElement(KXMLQLCVCFrameIsCollapsed, isCollapsed() ? KXMLQLCTrue : KXMLQLCFalse);

    /* Disabled */
    doc->writeTextElement(KXMLQLCVCFrameIsDisabled, isDisabled() ? KXMLQLCTrue : KXMLQLCFalse);

    /* Optional PIN */
    if (PIN() != 0)
    {
        SimpleCrypt crypter(encKey);
        QString encPin = crypter.encryptToString(QString("%1").arg(PIN()));
        doc->writeTextElement(KXMLQLCVCFramePIN, encPin);
    }

    /* Enable control */
    saveXMLInputControl(doc, INPUT_ENABLE_ID, false, KXMLQLCVCFrameEnableSource);

    /* Multipage mode */
    if (multiPageMode() == true)
    {
        doc->writeStartElement(KXMLQLCVCFrameMultipage);
        doc->writeAttribute(KXMLQLCVCFramePagesNumber, QString::number(totalPagesNumber()));
        doc->writeAttribute(KXMLQLCVCFrameCurrentPage, QString::number(currentPage()));
        if (pagesLoop())
            doc->writeAttribute(KXMLQLCVCFramePagesLoop, KXMLQLCTrue);
        doc->writeEndElement();

        saveXMLInputControl(doc, INPUT_NEXT_PAGE_ID, false, KXMLQLCVCFrameNext);
        saveXMLInputControl(doc, INPUT_PREVIOUS_PAGE_ID, false, KXMLQLCVCFramePrevious);

        /* Write shortcuts, if any */
        QMapIterator <int, QString> it(m_pageLabels);
        while (it.hasNext() == true)
        {
            it.next();

            doc->writeStartElement(KXMLQLCVCFrameShortcut);
            doc->writeAttribute(KXMLQLCVCFrameShortcutPage, QString::number(it.key()));
            doc->writeAttribute(KXMLQLCVCFrameShortcutName, it.value());

            saveXMLInputControl(doc, INPUT_SHORTCUT_BASE_ID + it.key());

            doc->writeEndElement();
        }
    }

    /* Grid layout */
    if (m_layoutMode != LayoutFree || m_gridColumns != 12 || m_gridRowHeight != 0 || m_gridCompact != true)
    {
        doc->writeStartElement(KXMLQLCVCFrameGridLayout);
        doc->writeAttribute(KXMLQLCVCFrameGridMode,
                            m_layoutMode == LayoutGrid ? KXMLQLCVCFrameGridModeGrid : KXMLQLCVCFrameGridModeFree);
        doc->writeAttribute(KXMLQLCVCFrameGridColumns, QString::number(m_gridColumns));
        doc->writeAttribute(KXMLQLCVCFrameGridRowHeight, QString::number(m_gridRowHeight));
        doc->writeAttribute(KXMLQLCVCFrameGridCompact, m_gridCompact ? KXMLQLCTrue : KXMLQLCFalse);
        doc->writeEndElement();
    }

    /* Arrangement, only when the frame arranges its children */
    if (m_arrangeMode != ArrangeNone)
    {
        const QStringList modes {"None", "Row", "Column", "Grid", "Flow"};
        doc->writeStartElement(KXMLQLCVCFrameArrange);
        doc->writeAttribute(KXMLQLCVCFrameArrangeMode, modes.value(int(m_arrangeMode)));
        doc->writeAttribute(KXMLQLCVCFrameArrangeGap, QString::number(m_arrangeGap));
        doc->writeAttribute(KXMLQLCVCFrameArrangeColumns, QString::number(m_arrangeColumns));
        doc->writeAttribute(KXMLQLCVCFrameArrangeFill, m_arrangeFill ? KXMLQLCTrue : KXMLQLCFalse);
        doc->writeEndElement();
    }

    /* Save children */
    foreach (const VCWidget *child, children(false))
        child->saveXML(doc);

    /* Allow subclasses to save additional tags */
    saveExtraXML(doc);

    /* End the <Frame> tag */
    doc->writeEndElement();

    return true;
}

bool VCFrame::loadExtraXML(QXmlStreamReader &root)
{
    Q_UNUSED(root)
    return false;
}

void VCFrame::saveExtraXML(QXmlStreamWriter *doc) const
{
    Q_UNUSED(doc)
}
