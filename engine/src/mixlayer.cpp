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

/* Bounds every walk over the follow graph. A real graph is a few nodes deep
   (fader, Scene, Chaser, Show); the bound only stops a cycle that two
   concurrent follow() calls could still close. */
static const int kMaxVisits = 256;

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

void MixLayer::follow(const QList<QSharedPointer<const MixLayer>> &parents)
{
    QList<QSharedPointer<const MixLayer>> links;
    for (const QSharedPointer<const MixLayer> &parent : parents)
    {
        int budget = kMaxVisits;
        if (parent != nullptr && parent->reaches(this, budget) == false)
            links.append(parent);
    }

    QMutexLocker locker(&m_parentsMutex);
    m_parents = links;
}

void MixLayer::follow(const QSharedPointer<const MixLayer> &parent)
{
    follow(QList<QSharedPointer<const MixLayer>>({ parent }));
}

QList<QSharedPointer<const MixLayer>> MixLayer::parents() const
{
    QMutexLocker locker(&m_parentsMutex);
    return m_parents;
}

bool MixLayer::reaches(const MixLayer *node, int &budget) const
{
    if (this == node)
        return true;
    if (--budget <= 0)
        return false;

    for (const QSharedPointer<const MixLayer> &parent : parents())
    {
        if (parent->reaches(node, budget))
            return true;
    }
    return false;
}

int MixLayer::effective() const
{
    int budget = kMaxVisits;
    return effective(budget);
}

int MixLayer::effective(int &budget) const
{
    const int layer = m_layer;
    if (--budget <= 0)
        return layer;

    const QList<QSharedPointer<const MixLayer>> links = parents();
    if (links.isEmpty())
        return layer;

    int highest = links.first()->effective(budget);
    for (int i = 1; i < links.count(); i++)
        highest = qMax(highest, links.at(i)->effective(budget));

    return layer + highest;
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

void MixLayer::followSources(Function *function, const QList<FunctionParent> &sources)
{
    if (sources.isEmpty())
        return;

    // Any other source (VC widget, Master, a Function no longer in the Doc) counts as layer 0
    static const QSharedPointer<const MixLayer> ground = QSharedPointer<const MixLayer>::create();

    QList<QSharedPointer<const MixLayer>> starters;
    Doc *doc = function->doc();
    for (const FunctionParent &source : sources)
    {
        Function *starter = nullptr;
        if (doc != nullptr && source.type() == FunctionParent::Function && source.id() != function->id())
            starter = doc->function(source.id());

        QSharedPointer<const MixLayer> layer = starter != nullptr ? starter->mixLayer() : ground;
        if (starters.contains(layer) == false)
            starters.append(layer);
    }

    function->mixLayer()->follow(starters);
}

QSharedPointer<MixLayer> Function::mixLayer() const
{
    return m_mixLayer;
}

QSharedPointer<MixLayer> GenericFader::mixLayer() const
{
    return m_mixLayer;
}
