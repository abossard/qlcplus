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
 * newest wins). HTP merging does not depend on the order.
 *
 * Every Function and every GenericFader owns one MixLayer. A node can follow
 * another node: its effective layer is its own layer plus the effective layer
 * of the followed node. A Function's faders follow the Function. A run started
 * by another Function (Chaser and Sequence steps, Collection members, Show
 * clips) follows the starter; a run started by anything else follows nothing.
 * A second start source of an already running Function changes nothing. So
 * setting the layer of a running Function moves its faders and the faders of
 * everything it started, from the next Universe write on.
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

    /** Follow $parent: add its effective layer to this node's own layer.
     *  A null $parent, or one that already follows this node, removes the link */
    void follow(const QSharedPointer<const MixLayer> &parent);

    /** The own layer plus the effective layer of the followed node */
    int effective() const;

    /** Order $faders for writing: by FaderPriority, then by effective layer,
     *  keeping the current order for equal keys */
    static void sortForWrite(QList<QSharedPointer<GenericFader>> &faders);

    /** Called by Function::start() when $source is the first start source of
     *  $function: follow the starting Function, or nothing for other sources */
    static void followStarter(Function *function, const FunctionParent &source);

private:
    QSharedPointer<const MixLayer> parent() const;

    std::atomic<int> m_layer;
    mutable QMutex m_parentMutex;
    QSharedPointer<const MixLayer> m_parent;
};

/** @} */

#endif
