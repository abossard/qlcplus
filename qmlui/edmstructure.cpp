/*
  Q Light Controller Plus
  edmstructure.cpp

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

#include "edmstructure.h"

#include <QPointer>
#include <QSettings>
#include <QThreadPool>

#define SETTINGS_AUTO_ANALYSE "edmanalysis/autoAnalyse"

/** The live values are published at least this often, in bars */
static const double kLiveStepBars = 1.0 / 16.0;

EdmStructure::EdmStructure(DjFsm *fsm, EdmAnalysisQueue *queue, const QString &analysisDir, QObject *parent)
    : QObject(parent)
    , m_fsm(fsm)
    , m_queue(queue)
    , m_dir(analysisDir)
    , m_reader(analysisDir)
{
    for (Deck &d : m_decks)
        d.view = new EdmDeckStructure(this);

    connect(m_fsm, &DjFsm::songChanged, this, &EdmStructure::slotSongChanged);
    connect(m_fsm, &DjFsm::settled, this, &EdmStructure::slotSettled);
    connect(m_queue, &EdmAnalysisQueue::entryChanged, this, &EdmStructure::slotEntryChanged);
}

QObject *EdmStructure::deck(int deck)
{
    return deck >= 1 && deck <= DjFsm::MaxDecks ? m_decks[deck - 1].view : nullptr;
}

QString EdmStructure::analysisStateForPath(const QString &path)
{
    for (const Deck &d : m_decks)
    {
        if (d.path == path && d.path.isEmpty() == false)
            return d.view->m_state;
    }
    const QString state = m_queue->entry(path).state;
    if (state.isEmpty() == false)
        return state;
    return m_reader.analysis(path).valid ? "ready" : "none";
}

void EdmStructure::retry(const QString &path)
{
    m_queue->retry(path);
}

void EdmStructure::analyse(const QString &path)
{
    if (m_queue->entry(path).state == "failed")
        m_queue->retry(path);
    else if (m_reader.analysis(path).valid == false)
        m_queue->enqueue(path);
}

void EdmStructure::slotSongChanged(int deck)
{
    syncDeck(deck);
}

void EdmStructure::slotSettled()
{
    for (int deck = 1; deck <= DjFsm::MaxDecks; deck++)
        syncDeck(deck);

    const int active = m_fsm->activeDeck();
    for (int deck = 1; deck <= m_fsm->deckCount(); deck++)
        advance(deck, active);

    if (active != m_active)
    {
        m_active = active;
        emit activeDeckChanged();
        if (active > 0 && m_decks[active - 1].analysis.valid == false)
            emit deckLost(active, true);
    }
}

void EdmStructure::slotEntryChanged(const QString &path)
{
    for (int deck = 1; deck <= DjFsm::MaxDecks; deck++)
    {
        if (m_decks[deck - 1].path == path && m_decks[deck - 1].missed)
            takeQueueState(deck);
    }
    emit analysisStateChanged(path);
}

void EdmStructure::syncDeck(int deck)
{
    const DjFsm::Deck &source = m_fsm->deckAt(deck - 1);
    const QString path = source.state == DjFsm::DeckState::Empty ? QString() : source.song.filepath;
    Deck &d = m_decks[deck - 1];
    if (path == d.path)
        return;

    const QString was = d.path;
    d.path = path;
    d.token++;
    d.missed = false;
    d.hasLast = false;
    d.view->m_path = path;
    setAnalysis(deck, EdmAnalysis());
    if (path.isEmpty())
        setState(deck, "none");
    else
        lookUp(deck);
    if (was.isEmpty() == false)
        emit analysisStateChanged(was);
}

void EdmStructure::lookUp(int deck)
{
    Deck &d = m_decks[deck - 1];
    const EdmAnalysis found = m_reader.analysis(d.path);
    if (found.valid)
    {
        setAnalysis(deck, found);
        setState(deck, "ready");
        return;
    }

    setState(deck, "lookup");
    const QPointer<EdmStructure> self(this);
    const QString dir = m_dir, path = d.path;
    const int token = d.token;
    QThreadPool::globalInstance()->start([self, dir, path, deck, token]() {
        const QString id = EdmAnalysisReader(dir).idByContent(path);
        if (self.isNull() == false)
            QMetaObject::invokeMethod(self.data(), [self, deck, token, id]() {
                if (self.isNull() == false)
                    self->contentLookupDone(deck, token, id);
            }, Qt::QueuedConnection);
    });
}

void EdmStructure::contentLookupDone(int deck, int token, const QString &id)
{
    Deck &d = m_decks[deck - 1];
    if (token != d.token)
        return;

    const EdmAnalysis found = m_reader.analysisById(id);
    if (found.valid)
    {
        setAnalysis(deck, found);
        setState(deck, "ready");
        return;
    }

    d.missed = true;
    if (QSettings().value(SETTINGS_AUTO_ANALYSE, false).toBool())
        m_queue->enqueue(d.path);
    takeQueueState(deck);
}

void EdmStructure::takeQueueState(int deck)
{
    const Deck &d = m_decks[deck - 1];
    const EdmQueueEntry entry = m_queue->entry(d.path);
    if (entry.state == "ready")
    {
        const EdmAnalysis found = m_reader.analysisById(entry.id);
        setAnalysis(deck, found);
        if (found.valid)
            setState(deck, "ready");
        else
            setState(deck, "failed", QString(), tr("The analysis file is missing."));
    }
    else
    {
        setState(deck, entry.state.isEmpty() ? "none" : entry.state, entry.stage, entry.error);
    }
}

void EdmStructure::setState(int deck, const QString &state, const QString &stage, const QString &error)
{
    EdmDeckStructure *view = m_decks[deck - 1].view;
    if (view->m_state == state && view->m_stage == stage && view->m_error == error)
        return;

    view->m_state = state;
    view->m_stage = stage;
    view->m_error = error;
    emit view->staticChanged();
    emit analysisStateChanged(m_decks[deck - 1].path);
}

void EdmStructure::setAnalysis(int deck, const EdmAnalysis &analysis)
{
    Deck &d = m_decks[deck - 1];
    d.analysis = analysis;
    d.hasLast = false;

    EdmDeckStructure *view = d.view;
    view->m_sections.clear();
    view->m_events.clear();
    for (const EdmSection &s : analysis.sections)
        view->m_sections.append(QVariantMap{{"label", s.label}, {"startBar", s.startBar}, {"endBar", s.endBar}, {"confidence", s.confidence}});
    for (const EdmEvent &e : analysis.events)
        view->m_events.append(QVariantMap{{"kind", e.kind}, {"bar", e.bar}, {"confidence", e.confidence}});
    view->m_totalBars = analysis.valid ? analysis.sections.last().endBar : 0;
    emit view->staticChanged();
}

void EdmStructure::setLive(Deck &d, const EdmSongPosition &p)
{
    EdmDeckStructure *view = d.view;
    const bool sectionMoved = p.section != view->m_section;
    view->m_bar = p.bar;
    view->m_section = p.section;
    view->m_barInSection = p.barInSection;
    view->m_barsToNextSection = p.barsToNextSection;
    const bool hasNext = p.section >= 0 && p.section + 1 < d.analysis.sections.size();
    view->m_nextSectionLabel = hasNext ? d.analysis.sections[p.section + 1].label : QString();
    view->m_nextSectionConfidence = hasNext ? d.analysis.sections[p.section + 1].confidence : -1;
    view->m_barsToDrop = p.barsToNextEvent;
    view->m_dropConfidence = p.nextEvent >= 0 ? d.analysis.events[p.nextEvent].confidence : -1;

    if (sectionMoved || qAbs(p.bar - d.notifiedBar) >= kLiveStepBars)
    {
        d.notifiedBar = p.bar;
        emit view->liveChanged();
    }
}

void EdmStructure::advance(int deck, int active)
{
    Deck &d = m_decks[deck - 1];
    const DjFsm::Deck &source = m_fsm->deckAt(deck - 1);

    if (d.analysis.valid == false || source.state == DjFsm::DeckState::Empty)
    {
        if (d.followed)
        {
            d.followed = false;
            setLive(d, EdmSongPosition());
            emit deckLost(deck, deck == m_active);
        }
        return;
    }
    if (source.positionStale)
        return;

    const bool isActive = deck == active;
    const EdmSongPosition position = edmSongPosition(d.analysis, source.elapsedMs, "drop");
    const EdmCrossings crossings = d.hasLast ? edmCrossings(d.analysis, d.lastBar, position.bar)
                                             : edmFirstCrossings(d.analysis, position.bar);
    const bool activeChanged = d.followed && d.lastActive != isActive;

    d.followed = true;
    d.hasLast = true;
    d.lastBar = position.bar;
    d.lastActive = isActive;
    setLive(d, position);

    if (crossings.crossed.isEmpty() == false || crossings.jumped || activeChanged)
        emit deckAdvanced(deck, d.analysis, position, crossings, isActive);
}
