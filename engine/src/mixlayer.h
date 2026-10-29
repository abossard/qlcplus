/*
  Q Light Controller Plus
  mixlayer.h

  Copyright (c) QLC+ contributors

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

#ifndef MIXLAYER_H
#define MIXLAYER_H

#include <QSharedPointer>
#include <QMutex>
#include <QList>
#include <atomic>

class FunctionParent;
class GenericFader;
class Function;

/** @addtogroup engine Engine
 * @{
 */

/**
 * The LTP mix layer of a Function or of a GenericFader.
 *
 * Within one Universe::FaderPriority band, a Universe writes the faders of a
 * lower layer first, so a fader of a higher layer wins LTP channels regardless
 * of creation order. Faders on the same layer keep the creation order (the
 * newest wins). Normal HTP merging (the highest value) does not depend on the
 * order.
 *
 * Every Function and every GenericFader owns one MixLayer. A node can follow
 * other nodes: its effective layer is its own layer plus the highest effective
 * layer among them. A Function's faders follow the Function, except flash
 * faders, which follow nothing. A running Function follows every Function
 * among its start sources (Chaser and Sequence steps, Collection members, Show
 * clips) and takes the highest; any other source (VC widget, Master, a Function
 * no longer in the Doc) counts as layer 0, so a manual start never drops below
 * today's order. When the last source leaves, the links stay until the next
 * start, so a fade out keeps its layer. Setting the layer of a running Function
 * moves its faders and the faders of everything it started, from the next
 * Universe write on. Order dependent blend modes (Additive, Mask, Subtractive)
 * compose from the lowest layer up.
 *
 * Example, a parent that runs a child with its own weight (the ShowRunner for
 * a DJSET clip): child->mixLayer()->setLayer(dominant ? 0 : -1);
 * The child and its descendants then lose LTP to the dominant one, while
 * faders of anything else on layer 0 keep today's creation order.
 *
 * The layer is runtime state and is not saved. Every method is thread-safe.
 */
class MixLayer final
{
public:
    explicit MixLayer(int layer = 0);

    /** Get/Set this node's own layer. Default 0, negative values are allowed */
    int layer() const;
    void setLayer(int layer);

    /** Follow $parents instead of the current ones. Null parents, and parents
     *  that already follow this node, are skipped */
    void follow(const QList<QSharedPointer<const MixLayer>> &parents);
    void follow(const QSharedPointer<const MixLayer> &parent);

    /** The own layer plus the highest effective layer of the followed nodes */
    int effective() const;

    /** Order $faders for writing: by FaderPriority, then by effective layer,
     *  keeping the current order for equal keys */
    static void sortForWrite(QList<QSharedPointer<GenericFader>> &faders);

    /** Called by Function with its sources lock held after each update of
     *  $sources: follow them, or keep the links when none is left */
    static void followSources(Function *function, const QList<FunctionParent> &sources);

private:
    QList<QSharedPointer<const MixLayer>> parents() const;
    bool reaches(const MixLayer *node, int &budget) const;
    int effective(int &budget) const;

    std::atomic<int> m_layer;
    mutable QMutex m_parentsMutex;
    QList<QSharedPointer<const MixLayer>> m_parents;
};

/** @} */

#endif
