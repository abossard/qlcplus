/*
  Q Light Controller Plus
  pagebuilder.cpp

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

#include <QFontMetricsF>
#include <cmath>

#include "pagebuilder.h"
#include "virtualconsole.h"
#include "gridlayout.h"
#include "vceditor.h"
#include "vcsoloframe.h"
#include "vcbutton.h"
#include "vcslider.h"
#include "vcxypad.h"
#include "vcanimation.h"
#include "fixturegroup.h"
#include "rgbalgorithm.h"
#include "rgbmatrix.h"
#include "vcframe.h"
#include "qlcchannel.h"
#include "fixture.h"
#include "scene.h"
#include "doc.h"
#include "tardis.h"

namespace
{

/** Fixture and channel pairs */
using ChannelSet = QSet<QPair<quint32, quint32>>;

/** The channels the buttons of $section and its parts set */
void collectButtonChannels(const PagePlanner::Section &section, ChannelSet &out)
{
    for (const PagePlanner::Slot &slot : section.items)
        for (const PagePlanner::Choice &choice : slot.choices)
            for (const PagePlanner::Value &value : choice.values)
                out.insert({ value.fixture, value.channel });
    for (const PagePlanner::Section &part : section.parts)
        collectButtonChannels(part, out);
}

/** The lines $caption takes in $width, wrapped after spaces and slashes as
 *  the slider's Text item wraps it; 0 when a piece is wider than a line */
int captionLines(const QFontMetricsF &metrics, const QString &caption, qreal width)
{
    int lines = 1;
    QString line, piece;
    for (int i = 0; i < caption.size(); ++i)
    {
        piece += caption.at(i);
        if (caption.at(i) != ' ' && caption.at(i) != '/' && i < caption.size() - 1)
            continue;
        if (!line.isEmpty() && metrics.horizontalAdvance((line + piece).trimmed()) > width)
        {
            lines++;
            line.clear();
        }
        line += piece;
        piece.clear();
        if (metrics.horizontalAdvance(line.trimmed()) > width)
            return 0;
    }
    return lines;
}

/** A slider width from $width up at which $caption takes three lines at
 *  most: every line more takes height from the fader */
qreal sliderWidth(const QFont &font, const QString &caption, qreal width)
{
    const QFontMetricsF metrics(font);
    qreal fit = width;
    // the Text item keeps a little room at its sides
    for (int lines = captionLines(metrics, caption, fit - 4); (lines == 0 || lines > 3) && fit < width * 3;
         lines = captionLines(metrics, caption, fit - 4))
        fit += 2;
    // whole pixels up, so rounding the widget size never takes room from the caption
    return fit > width ? std::ceil(fit) : width;
}

/** Record the creation of $id for undo, as the managers do */
void recordCreated(Tardis::ActionCodes code, quint32 id)
{
    if (Tardis *tardis = Tardis::instance())
        tardis->enqueueAction(code, id, QVariant(), tardis->actionToByteArray(code, id));
}

/** Logical size of the widget of $slot, in pixel density units */
QSizeF slotSize(const PagePlanner::Slot &slot, bool strip)
{
    if (slot.matrixGrid.isValid())
        return QSizeF(60, 52);
    if (!slot.choices.isEmpty())
        return QSizeF(17, 17);
    if (slot.kind == FixtureControls::PanTilt)
        return QSizeF(52, 52);
    if (strip)
        return QSizeF(11, 40);
    return QSizeF(16, 52);
}

/** Children of a frame in a flow no wider than $maxWidth: their rects and
 *  the content size they need */
GridLayout::ArrangeResult flow(const QList<QSizeF> &sizes, qreal gap, qreal maxWidth, QSizeF &content)
{
    qreal width = gap;
    for (const QSizeF &size : sizes)
        width += size.width() + gap;
    width = qMin(width, maxWidth);
    const GridLayout::ArrangeSpec spec { GridLayout::ArrangeMode::Flow, gap, 1, false };
    GridLayout::ArrangeResult result = GridLayout::arrange(spec, QRectF(0, 0, width, 1e6), sizes, sizes);
    if (!result.error.isEmpty())
        qWarning() << "[PageBuilder]" << result.error;
    qreal bottom = 0, right = 0;
    for (const QRectF &rect : result.rects)
    {
        bottom = qMax(bottom, rect.bottom());
        right = qMax(right, rect.right());
    }
    content = QSizeF(right + gap, bottom + gap);
    return result;
}

/** A button running a Scene of the values of $choice */
VCButton *createChoiceButton(Doc *doc, VCFrame *frame, const PagePlanner::Choice &choice, const QString &name)
{
    auto *scene = new Scene(doc);
    scene->setName(name);
    for (const PagePlanner::Value &value : choice.values)
        scene->setValue(value.fixture, value.channel, value.value);
    if (!doc->addFunction(scene))
    {
        delete scene;
        return nullptr;
    }
    recordCreated(Tardis::FunctionCreate, scene->id());
    VCButton *button = qobject_cast<VCButton *>(frame->addWidget(nullptr, "Button", QPoint()));
    if (button == nullptr)
        return nullptr;
    button->setCaption(choice.label);
    button->setFunctionID(scene->id());
    if (choice.color.isValid())
    {
        button->setBackgroundColor(choice.color);
        const qreal luma = 0.299 * choice.color.red() + 0.587 * choice.color.green() + 0.114 * choice.color.blue();
        button->setForegroundColor(luma > 140 ? Qt::black : Qt::white);
    }
    if (!choice.picture.isEmpty())
        button->setBackgroundImage(choice.picture);
    return button;
}

/** An Animation widget running an RGB matrix over the heads of $slot,
 *  placed in a fixture group of its grid */
VCWidget *createMatrixWidget(Doc *doc, VCFrame *frame, const PagePlanner::Slot &slot)
{
    const Fixture *fixture = doc->fixture(slot.heads.first().fixture);
    RGBAlgorithm *algorithm = RGBAlgorithm::algorithm(doc, "Stripes");
    if (fixture == nullptr || algorithm == nullptr)
    {
        delete algorithm;
        return nullptr;
    }
    auto *group = new FixtureGroup(doc);
    group->setName(QString("%1 %2").arg(fixture->name(), slot.label));
    group->setSize(slot.matrixGrid);
    const int width = slot.matrixGrid.width();
    for (int i = 0; i < slot.heads.size(); ++i)
        group->assignHead(QLCPoint(i % width, i / width), GroupHead(slot.heads.at(i).fixture, slot.heads.at(i).head));
    doc->addFixtureGroup(group);
    recordCreated(Tardis::FixtureGroupCreate, group->id());

    auto *matrix = new RGBMatrix(doc);
    matrix->setName(group->name());
    matrix->setFixtureGroup(group->id());
    matrix->setAlgorithm(algorithm);
    if (!doc->addFunction(matrix))
    {
        delete matrix;
        return nullptr;
    }
    recordCreated(Tardis::FunctionCreate, matrix->id());
    VCAnimation *animation = qobject_cast<VCAnimation *>(frame->addWidget(nullptr, "Animation", QPoint()));
    if (animation == nullptr)
        return nullptr;
    animation->setCaption(slot.label);
    animation->setFunctionID(matrix->id());
    return animation;
}

VCWidget *createSlotWidget(Doc *doc, VCFrame *frame, const PagePlanner::Slot &slot,
                           const ChannelSet &buttonChannels)
{
    if (slot.matrixGrid.isValid())
        return createMatrixWidget(doc, frame, slot);
    // one choice is one button, no solo frame around it
    if (slot.choices.size() == 1)
    {
        VCButton *button = createChoiceButton(doc, frame, slot.choices.first(),
                                              QString("%1: %2").arg(frame->caption(), slot.label));
        if (button && slot.flash)
            button->setActionType(VCButton::Flash);
        return button;
    }

    if (slot.kind == FixtureControls::PanTilt)
    {
        VCXYPad *pad = qobject_cast<VCXYPad *>(frame->addWidget(nullptr, "XYPad", QPoint()));
        if (pad == nullptr)
            return nullptr;
        for (const PagePlanner::Head &head : slot.heads)
            pad->addHead(int(head.fixture), head.head);
        pad->setCaption(slot.label);
        return pad;
    }

    VCSlider *slider = qobject_cast<VCSlider *>(frame->addWidget(nullptr, "Slider", QPoint()));
    if (slider == nullptr)
        return nullptr;
    slider->setSliderMode(VCSlider::Level);
    // A level slider writes one 8 bit value: fine channels stay with their coarse
    for (const PagePlanner::Channel &ch : slot.channels)
    {
        const Fixture *fixture = doc->fixture(ch.fixture);
        const QLCChannel *channel = fixture ? fixture->channel(ch.channel) : nullptr;
        if (channel == nullptr || channel->controlByte() != QLCChannel::MSB)
            continue;
        slider->addLevelChannel(ch.fixture, ch.channel);
        // A button sets this channel too: the fader follows it. Intensity
        // channels add up (HTP), so their faders keep mixing with the buttons;
        // a picker beside buttons needs no override box, the buttons show the pick
        if (slot.kind == FixtureControls::Fader && channel->group() != QLCChannel::Intensity &&
            buttonChannels.contains({ ch.fixture, ch.channel }))
            slider->setMonitorEnabled(true);
    }
    if (slot.kind == FixtureControls::ColorMixer)
        slider->setClickAndGoType(VCSlider::CnGColors);
    else if (slot.kind == FixtureControls::Presets)
        slider->setClickAndGoType(VCSlider::CnGPreset);
    slider->setCaption(slot.label);
    return slider;
}

/** A solo frame of buttons for the choices of $slot, each running a Scene
 *  of the values that choice needs on every model */
VCFrame *buildChoices(VirtualConsole *vc, Doc *doc, VCFrame *parent, const PagePlanner::Slot &slot,
                      const QString &prefix)
{
    const qreal pd = qMax(qreal(1), vc->pixelDensity());
    const qreal gap = qRound(pd * 1.5);
    VCFrame *solo = qobject_cast<VCFrame *>(parent->addWidget(nullptr, "Solo frame", QPoint()));
    if (solo == nullptr)
        return nullptr;
    solo->setCaption(slot.label);
    const qreal top = solo->contentRect().top();

    QList<VCWidget *> buttons;
    QList<QSizeF> sizes;
    for (const PagePlanner::Choice &choice : slot.choices)
    {
        VCButton *button = createChoiceButton(doc, solo, choice, QString("%1 %2: %3").arg(prefix, slot.label, choice.label));
        if (button == nullptr)
            continue;
        buttons.append(button);
        sizes.append((QSizeF(17, 17) * pd).toSize());
    }

    // as many rows as fit beside a fader, so a section stays one row of widgets tall
    const qreal button = sizes.value(0).width();
    const qreal faderHeight = 52 * pd; // slotSize() of a fader
    const int rows = qMax(1, int((faderHeight - top) / (button + gap)));
    const int columns = qMax(1, int(std::ceil(qreal(sizes.size()) / rows)));
    QSizeF content;
    const GridLayout::ArrangeResult placed = flow(sizes, gap, gap + columns * (button + gap), content);
    for (int i = 0; i < buttons.size() && i < placed.rects.size(); ++i)
        buttons[i]->setLogicalGeometry(placed.rects[i].translated(0, top));
    const qreal header = QFontMetricsF(solo->font()).horizontalAdvance(slot.label) + top * 2 + gap * 2;
    solo->setLogicalGeometry(QRectF(0, 0, std::ceil(qMax(content.width(), header)), std::ceil(content.height() + top)));
    VCArrangement::of(solo, true)->setSpec({ GridLayout::ArrangeMode::Flow, gap, 1, false });
    return solo;
}

/** A frame in $parent for $section: its slots, then a frame per part,
 *  in a flow no wider than $maxWidth, the frame as wide as its caption */
VCFrame *buildSection(VirtualConsole *vc, Doc *doc, VCFrame *parent,
                      const PagePlanner::Section &section, qreal maxWidth, const ChannelSet &buttonChannels)
{
    const qreal pd = qMax(qreal(1), vc->pixelDensity());
    const qreal gap = qRound(pd * 1.5);
    VCFrame *frame = qobject_cast<VCFrame *>(parent->addWidget(nullptr, "Frame", QPoint()));
    if (frame == nullptr)
        return nullptr;
    frame->setCaption(section.title);
    const qreal top = frame->contentRect().top();

    QList<VCWidget *> children;
    QList<QSizeF> sizes;
    for (const PagePlanner::Slot &slot : section.items)
    {
        if (slot.choices.size() > 1)
        {
            if (VCFrame *solo = buildChoices(vc, doc, frame, slot, section.title))
            {
                children.append(solo);
                sizes.append(solo->logicalGeometry().size());
            }
            continue;
        }
        VCWidget *widget = createSlotWidget(doc, frame, slot, buttonChannels);
        if (widget == nullptr)
            continue;
        children.append(widget);
        QSizeF size = slotSize(slot, section.strip) * pd;
        if (qobject_cast<VCSlider *>(widget))
            size.setWidth(sliderWidth(widget->font(), widget->caption(), size.width()));
        // whole pixels: the arrangement rounds edges and refuses anything below its size
        sizes.append(size.toSize());
    }
    for (const PagePlanner::Section &part : section.parts)
    {
        VCFrame *child = buildSection(vc, doc, frame, part, maxWidth - 2 * gap, buttonChannels);
        if (child == nullptr)
            continue;
        children.append(child);
        sizes.append(child->logicalGeometry().size());
    }

    // a strip with a physical layout wraps after its columns
    const qreal width = section.columns > 0 && !sizes.isEmpty()
                      ? qMin(maxWidth, gap + section.columns * (sizes.first().width() + gap)) : maxWidth;
    QSizeF content;
    const GridLayout::ArrangeResult placed = flow(sizes, gap, width, content);
    for (int i = 0; i < children.size() && i < placed.rects.size(); ++i)
        children[i]->setLogicalGeometry(placed.rects[i].translated(0, top));
    // the header holds a collapse button and an enable box beside the caption
    const qreal header = QFontMetricsF(frame->font()).horizontalAdvance(section.title) + top * 2 + gap * 2;
    frame->setLogicalGeometry(QRectF(0, 0, std::ceil(qMax(content.width(), header)),
                                     std::ceil(content.height() + top)));
    VCArrangement::of(frame, true)->setSpec({ GridLayout::ArrangeMode::Flow, gap, 1, false });
    return frame;
}

} // namespace

PageBuilder::Result PageBuilder::build(VirtualConsole *vc, Doc *doc, VCFrame *target,
                                       const PagePlanner::Page &page, const QRectF &area)
{
    const qreal gap = qRound(qMax(qreal(1), vc->pixelDensity()) * 1.5);
    Result result;
    QList<QSizeF> frameSizes;
    GridLayout::ArrangeResult placed;
    ChannelSet buttonChannels;
    for (const PagePlanner::Section &section : page.sections)
        collectButtonChannels(section, buttonChannels);
    for (const PagePlanner::Section &section : page.sections)
    {
        const Made before = made(doc);
        VCFrame *frame = buildSection(vc, doc, target, section, area.width() - 2 * gap, buttonChannels);
        if (frame == nullptr)
        {
            result.sections++;
            continue;
        }
        frameSizes.append(frame->logicalGeometry().size());
        QSizeF used;
        const GridLayout::ArrangeResult next = flow(frameSizes, gap, area.width(), used);
        if (used.height() > area.height() && !result.frames.isEmpty())
        {
            // no room below the others: undo this section, scenes and groups included,
            // through the editor so undo history records the deletion
            vc->deleteVCWidgets(QVariantList { QVariant(frame->id()) });
            deleteMadeSince(doc, before);
            break;
        }
        placed = next;
        result.size = used;
        result.frames.append(frame);
        result.sections++;
    }

    for (int i = 0; i < result.frames.size() && i < placed.rects.size(); ++i)
        result.frames[i]->setLogicalGeometry(placed.rects[i].translated(area.topLeft()));
    return result;
}

PageBuilder::Made PageBuilder::made(const Doc *doc)
{
    Made result;
    for (const Function *function : doc->functions())
        result.functions.insert(function->id());
    for (const FixtureGroup *group : doc->fixtureGroups())
        result.groups.insert(group->id());
    return result;
}

void PageBuilder::deleteMadeSince(Doc *doc, const Made &before)
{
    Tardis *tardis = Tardis::instance();
    // functions first: a matrix uses its fixture group
    for (const quint32 id : made(doc).functions - before.functions)
    {
        if (tardis)
            tardis->enqueueAction(Tardis::FunctionDelete, id, tardis->actionToByteArray(Tardis::FunctionDelete, id),
                                  QVariant());
        doc->deleteFunction(id);
    }
    for (const quint32 id : made(doc).groups - before.groups)
    {
        if (tardis)
            tardis->enqueueAction(Tardis::FixtureGroupDelete, id,
                                  tardis->actionToByteArray(Tardis::FixtureGroupDelete, id), QVariant());
        doc->deleteFixtureGroup(id);
    }
}
