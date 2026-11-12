/*
  Q Light Controller Plus
  edmstructuredirector.cpp

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

#include "edmstructuredirector.h"

#include <QSet>

#include <algorithm>

EdmLookDecision edmLookDecide(const EdmStructureMapping &mapping, const EdmLookState &state, const EdmLookEvent &event)
{
    EdmLookDecision decision;
    decision.state = state;
    const bool jumpedAway = event.jumped && event.sameSection == false;
    const bool trigger = event.sectionStart || jumpedAway || event.becameActive;
    if (mapping.autoEnabled() == false || event.activeDeck == false || trigger == false)
        return decision;

    decision.state.overridden = false;
    const quint32 target = mapping.widget(event.label);
    if (target == kNoLookWidget)
        return decision;

    for (quint32 id : event.mappedOn)
    {
        if (id != target)
            decision.release.append(id);
    }
    if (event.mappedOn.contains(target) == false)
        decision.press = target;
    return decision;
}

EdmLookState edmLookExternalChange(const EdmStructureMapping &mapping, const EdmLookState &state, quint32 widgetId)
{
    EdmLookState result = state;
    if (mapping.autoEnabled() && mapping.widgets().contains(widgetId))
        result.overridden = true;
    return result;
}

EdmStructureDirector::EdmStructureDirector(EdmStructure *structure, EdmStructureMapping *mapping,
                                           EdmLookSurface *surface, QObject *parent)
    : QObject(parent)
    , m_mapping(mapping)
    , m_surface(surface)
{
    connect(structure, &EdmStructure::deckAdvanced, this, &EdmStructureDirector::slotDeckAdvanced);
    connect(structure, &EdmStructure::deckLost, this, &EdmStructureDirector::slotDeckLost);
    connect(m_surface, &EdmLookSurface::changed, this, &EdmStructureDirector::slotWidgetChanged);
    connect(m_mapping, &EdmStructureMapping::changed, this, [this]() {
        m_surface->watch(m_mapping->widgets());
        emit statusChanged();
    });
    m_surface->watch(m_mapping->widgets());
}

void EdmStructureDirector::slotDeckAdvanced(int deck, const EdmAnalysis &analysis, const EdmSongPosition &position,
                                            const EdmCrossings &crossings, bool activeDeck)
{
    if (activeDeck == false)
    {
        if (deck == m_deck)
        {
            m_deck = 0;
            emit statusChanged();
        }
        return;
    }

    EdmLookEvent event;
    event.activeDeck = true;
    event.becameActive = deck != m_deck;
    event.jumped = crossings.jumped;
    event.sameSection = deck == m_deck && position.section == m_section;
    for (const EdmCrossing &c : crossings.crossed)
        event.sectionStart = event.sectionStart || c.kind == EdmCrossing::SectionStart;
    event.label = position.section >= 0 ? analysis.sections[position.section].label : QString();

    QSet<quint32> mapped;
    for (quint32 id : m_mapping->widgets())
        mapped.insert(id);
    for (quint32 id : mapped)
    {
        if (m_surface->isOn(id))
            event.mappedOn.append(id);
    }
    std::sort(event.mappedOn.begin(), event.mappedOn.end());

    m_deck = deck;
    m_section = position.section;
    m_label = event.label;
    apply(edmLookDecide(*m_mapping, m_state, event));
    emit statusChanged();
}

void EdmStructureDirector::slotDeckLost(int deck, bool activeDeck)
{
    if (activeDeck || deck == m_deck)
    {
        m_deck = 0;
        m_label.clear();
        emit statusChanged();
    }
}

void EdmStructureDirector::slotWidgetChanged(quint32 widgetId)
{
    const bool on = m_surface->isOn(widgetId);
    const bool wasOn = m_known.value(widgetId, false);
    m_known[widgetId] = on;
    if (on == wasOn)
        return;
    if (m_expected.contains(widgetId) && m_expected.value(widgetId) == on)
    {
        m_expected.remove(widgetId);
        return;
    }

    m_state = edmLookExternalChange(*m_mapping, m_state, widgetId);
    emit statusChanged();
}

void EdmStructureDirector::apply(const EdmLookDecision &decision)
{
    m_state = decision.state;
    m_expected.clear();
    for (quint32 release : decision.release)
    {
        if (m_surface->isOn(release))
        {
            m_expected.insert(release, false);
            m_surface->set(release, false);
        }
    }
    if (decision.press != kNoLookWidget && m_surface->isOn(decision.press) == false)
    {
        m_expected.insert(decision.press, true);
        m_surface->set(decision.press, true);
    }
}
