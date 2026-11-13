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

#include <QDir>

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

    // the page, a shell run or a rewrite by the tool: the decks re-read what changed
    // a batch run changes it thousands of times: one refresh once it is quiet
    QDir().mkpath(m_dir + "/final");
    m_watcher.addPaths({m_dir, m_dir + "/final"});
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(500);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &EdmStructure::slotFolderChanged);
    connect(&m_refreshTimer, &QTimer::timeout, this, &EdmStructure::refreshDecks);
}

/** The same song structure: a re-read that changes nothing keeps the deck as it is */
static bool sameStructure(const EdmAnalysis &a, const EdmAnalysis &b)
{
    if (a.valid != b.valid || a.id != b.id || a.sections.size() != b.sections.size() || a.events.size() != b.events.size()
        || a.grid.barMs != b.grid.barMs || a.grid.firstDownbeatMs != b.grid.firstDownbeatMs)
        return false;
    for (int i = 0; i < a.sections.size(); i++)
    {
        const EdmSection &x = a.sections[i], &y = b.sections[i];
        if (x.label != y.label || x.startBar != y.startBar || x.endBar != y.endBar || x.confidence != y.confidence
            || x.unsure != y.unsure)
            return false;
    }
    for (int i = 0; i < a.events.size(); i++)
    {
        if (a.events[i].kind != b.events[i].kind || a.events[i].bar != b.events[i].bar)
            return false;
    }
    return true;
}

void EdmStructure::slotFolderChanged()
{
    m_refreshTimer.start();   // every change restarts the wait: one refresh once the folder is quiet
}

void EdmStructure::refreshDecks()
{
    for (int deck = 1; deck <= DjFsm::MaxDecks; deck++)
    {
        const Deck &d = m_decks[deck - 1];
        const QString state = d.view->m_state;
        if (d.path.isEmpty() || state == "lookup" || state == "queued" || state == "analysing")
            continue;
        const EdmAnalysis found = m_reader.analysis(d.path);
        if (found.valid && sameStructure(found, d.analysis) == false)
        {
            setAnalysis(deck, found);
            setState(deck, "ready");
        }
    }
    emit analysisStateChanged(QString());
    emit analysisFilesChanged();
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
    // a run under way, else a file on disk, else what the queue last made of it
    const QString state = m_queue->entry(path).state;
    if (state == "queued" || state == "analysing")
        return state;
    if (m_reader.analysis(path).valid)
        return "ready";
    return state.isEmpty() ? "none" : state;
}

void EdmStructure::retry(const QString &path)
{
    m_queue->retry(path);
}

void EdmStructure::analyse(const QString &path)
{
    if (m_reader.analysis(path).valid)
        refreshDecks();
    else if (m_queue->entry(path).state.isEmpty() == false)
        m_queue->retry(path);
    else
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

    // every deck: one a smaller deck count leaves out is lost like an ejected one
    const int active = m_fsm->activeDeck();
    for (int deck = 1; deck <= DjFsm::MaxDecks; deck++)
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
    setLive(d, EdmSongPosition());
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
        view->m_sections.append(QVariantMap{{"label", s.label}, {"startBar", s.startBar}, {"endBar", s.endBar}, {"confidence", s.confidence}, {"unsure", s.unsure}});
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
            // the active one, unless another deck took over in this same write
            emit deckLost(deck, deck == m_active && (active == 0 || active == deck));
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

    if (crossings.crossed.isEmpty() == false || activeChanged)
        emit deckAdvanced(deck, d.analysis, position, crossings, isActive);
}
