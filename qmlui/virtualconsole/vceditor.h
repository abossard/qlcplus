/*
  Q Light Controller Plus
  vceditor.h

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

#ifndef VCEDITOR_H
#define VCEDITOR_H

#include <QObject>
#include <QHash>
#include <QMap>
#include <QRectF>
#include <QVariant>

#include "gridlayout.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class FunctionManager;
class VirtualConsole;
class QQuickView;
class VCWidget;
class VCFrame;

/** How a frame arranges its children. It lives on the frame as a child
 *  object, so the frame itself only loads, saves and copies it */
class VCArrangement final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int mode READ mode NOTIFY changed)
    Q_PROPERTY(qreal gap READ gap NOTIFY changed)
    Q_PROPERTY(int columns READ columns NOTIFY changed)
    Q_PROPERTY(bool fill READ fill NOTIFY changed)

public:
    /** The arrangement of $frame, made on demand when $create is set */
    static VCArrangement *of(const VCFrame *frame, bool create);
    /** What $frame does, None for a frame that was never arranged */
    static GridLayout::ArrangeSpec specOf(const VCFrame *frame);

    static QVariantMap toMap(const GridLayout::ArrangeSpec &spec);
    static GridLayout::ArrangeSpec fromMap(const QVariantMap &map);

    int mode() const { return int(m_spec.mode); }
    qreal gap() const { return m_spec.gap; }
    int columns() const { return m_spec.columns; }
    bool fill() const { return m_spec.fill; }
    void setSpec(const GridLayout::ArrangeSpec &spec);

    /* Frame XML hooks */
    static bool isXMLTag(const QXmlStreamReader &root);
    static void loadXML(VCFrame *frame, QXmlStreamReader &root);
    static void saveXML(const VCFrame *frame, QXmlStreamWriter *doc);
    static void copy(const VCFrame *from, VCFrame *to);

signals:
    void changed();

private:
    explicit VCArrangement(VCFrame *frame);
    GridLayout::ArrangeSpec m_spec;
};

/** Virtual Console editing that upstream does not have. The console and
 *  its frames call in at a few places, QML reaches it as "vcEditor" */
class VCEditor final : public QObject
{
    Q_OBJECT

public:
    explicit VCEditor(VirtualConsole *vc);

    /*********************************************************************
     * Undo
     *********************************************************************/
    /** Record a change of the Q_PROPERTY $property of $widget */
    static void recordProperty(VCWidget *widget, const char *property,
                               const QVariant &oldValue, const QVariant &newValue);

    /** Record that the page at $index was just added, or is about to be deleted */
    void recordPage(int index, bool added);

    /** Apply the old (undo) or new (redo) $value of a recorded editor action */
    bool replay(quint32 objID, const QVariant &value);

    /** Forget what the pages, which outlive a workspace, carried of it */
    void resetContents();

    /** Whether the console side panel in $view shows the editor of the current
     *  Function of $functions, which then owns the Delete key */
    static bool showsFunctionEditor(QQuickView *view, FunctionManager *functions);

    /*********************************************************************
     * Selection
     *********************************************************************/
    /** IDs of the children of $frameID on its shown page that a
     *  $logicalRect band touches, in child order */
    Q_INVOKABLE QVariantList widgetsInRect(quint32 frameID, QRectF logicalRect) const;

    /** Give the selected widgets the width and/or height of $refWidget,
     *  as Qt::Horizontal and Qt::Vertical in $orientations ask */
    Q_INVOKABLE void setWidgetsSize(VCWidget *refWidget, int orientations);

    /** Copies right of the selected widgets, in the same frame, leaving the clipboard alone */
    void duplicateSelection();

    /** Move $rectangles, refused at their place in $target, to the first free spot
     *  in reading order from there. Returns false when there is none */
    bool placeNear(QMap<quint32, QRectF> &rectangles, VCFrame *target, int subpage, qreal step) const;

    /*********************************************************************
     * Arrangement
     *********************************************************************/
    /** The arrangement of frame $frameID, for QML to follow */
    Q_INVOKABLE QObject *arrangement(quint32 frameID);

    /** Set how frame $frameID arranges its children, from the optional "mode",
     *  "gap", "columns" and "fill" of $settings, and arrange them. Refused
     *  when the children do not fit, then nothing changes */
    Q_INVOKABLE QVariantMap arrangeFrame(quint32 frameID, QVariantMap settings);

    /** Add to $rectangles the children of the arranged frames they resize, or
     *  whose children they resize, from the outer frames in. $specs replace
     *  the arrangement of the frames they name */
    QString arrangePlan(QMap<quint32, QRectF> &rectangles,
                        const QHash<quint32, GridLayout::ArrangeSpec> &specs = {}) const;

    /** Where a widget dropped at $rect goes in the child order of the arranged
     *  $target: before the first child whose center lies right of or below its
     *  top left corner. -1 when $target is not arranged */
    int dropIndex(VCFrame *target, int subpage, const QRectF &rect) const;

    /** For a structure edit into the arranged $target, place the children of
     *  $order: $candidates get the places of the moved roots, $siblings the
     *  places of the others. An error when they do not fit */
    QString arrangeStructure(VCFrame *target, int subpage, const QList<VCWidget *> &order,
                             QMap<quint32, QRectF> &candidates, QMap<quint32, QRectF> &siblings) const;

    /** The structure edit of the console, followed in the same undo step by the
     *  $siblings moves and by the arranged frames the roots leave closing up */
    QVariantMap commitStructure(const QList<VCWidget *> &roots, VCFrame *target, int subpage,
                                const QList<VCWidget *> &order, const QMap<quint32, QRectF> &candidates,
                                const QMap<quint32, QRectF> &siblings);

    /** Delete the widgets $ids, the arranged frames they leave close up in the same undo step */
    void deleteWidgets(const QVariantList &ids);

    bool isArranged(const VCFrame *frame) const;

    /** Why widgets of $sizes cannot join the end of the arranged $frame once its children
     *  $leaving are gone, empty when they fit or $frame is not arranged */
    QString arrangedFit(VCFrame *frame, int subpage, const QList<QSizeF> &sizes,
                        const QList<QSizeF> &minimums, const QList<quint32> &leaving = {}) const;
    /** The same for copies of the widgets keyed in $joining, at their sizes there,
     *  which leave their place when they were $cut */
    QString arrangedFit(VCFrame *frame, int subpage, const QMap<quint32, QRectF> &joining, bool cut = false) const;

    /** Arrange $frame again inside a running edit, whose undo step takes the moves */
    void arrangeJoined(VCFrame *frame);

    /** A frame at $logicalPosition in $targetID with $matrixSize controls of $matrixType
     *  ("buttonmatrix" or "slidermatrix") of $widgetSize, arranged in a grid that
     *  keeps them filling the frame. One undo step; errors go to editRejected */
    Q_INVOKABLE void addWidgetMatrix(quint32 targetID, QString matrixType, QPointF logicalPosition,
                                     QSize matrixSize, QSize widgetSize, bool soloFrame);

signals:
    /** Emitted from a widget item: show the properties of the selection */
    void widgetPropertiesRequested();
    /** Emitted from a widget item: show the widget menu at $scenePos */
    void widgetMenuRequested(QPointF scenePos);
    /** Show $message in the console, like the reason of a refused edit */
    void messageRequested(QString message);

private:
    bool restorePage(int index, const QByteArray &xml);
    /** Arrange $frames again, after children left or joined them */
    void rearrange(const QList<VCFrame *> &frames);

    VirtualConsole *m_vc;
};

#endif
