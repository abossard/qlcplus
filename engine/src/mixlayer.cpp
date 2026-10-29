/*
  Q Light Controller Plus
  mixlayer.cpp

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

#include <QVector>
#include <algorithm>

#include "genericfader.h"
#include "mixlayer.h"
#include "function.h"
#include "doc.h"

/* A chain is a few nodes deep (fader, Scene, Chaser, Show). The cap only
   bounds a cycle that two concurrent follow() calls could still close. */
static const int kMaxChainDepth = 64;

MixLayer::MixLayer(int layer)
    : m_layer(layer)
{
}

int MixLayer::layer() const
{
    return m_layer;
}

void MixLayer::setLayer(int layer)
{
    m_layer = layer;
}

void MixLayer::follow(const QSharedPointer<const MixLayer> &parent)
{
    QSharedPointer<const MixLayer> link = parent;
    QSharedPointer<const MixLayer> node = parent;
    for (int depth = 0; node != nullptr && depth < kMaxChainDepth; depth++)
    {
        if (node.data() == this)
        {
            link.clear();
            break;
        }
        node = node->parent();
    }

    QMutexLocker locker(&m_parentMutex);
    m_parent = link;
}

QSharedPointer<const MixLayer> MixLayer::parent() const
{
    QMutexLocker locker(&m_parentMutex);
    return m_parent;
}

int MixLayer::effective() const
{
    int layer = m_layer;
    QSharedPointer<const MixLayer> node = parent();
    for (int depth = 0; node != nullptr && depth < kMaxChainDepth; depth++)
    {
        layer += node->m_layer;
        node = node->parent();
    }
    return layer;
}

void MixLayer::sortForWrite(QList<QSharedPointer<GenericFader>> &faders)
{
    struct Entry
    {
        int priority;
        int layer;
        QSharedPointer<GenericFader> fader;
    };

    /* Read every key once: a layer can change on another thread meanwhile,
       and the comparison must stay consistent during the sort */
    QVector<Entry> entries;
    entries.reserve(faders.size());
    for (const QSharedPointer<GenericFader> &fader : std::as_const(faders))
        entries.append({ fader->priority(), fader->mixLayer()->effective(), fader });

    std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b)
    {
        if (a.priority != b.priority)
            return a.priority < b.priority;
        return a.layer < b.layer;
    });

    for (int i = 0; i < entries.size(); i++)
        faders[i] = entries.at(i).fader;
}

void MixLayer::followStarter(Function *function, const FunctionParent &source)
{
    Function *starter = nullptr;
    if (source.type() == FunctionParent::Function && source.id() != function->id() &&
        function->doc() != nullptr)
    {
        starter = function->doc()->function(source.id());
    }

    function->mixLayer()->follow(starter != nullptr ? starter->mixLayer() : QSharedPointer<MixLayer>());
}

QSharedPointer<MixLayer> Function::mixLayer() const
{
    return m_mixLayer;
}

QSharedPointer<MixLayer> GenericFader::mixLayer() const
{
    return m_mixLayer;
}
