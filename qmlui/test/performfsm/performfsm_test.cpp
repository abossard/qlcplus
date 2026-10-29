/*
  Q Light Controller Plus - Unit test
  performfsm_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "performfsm_test.h"
#include "performfsm.h"

#include <QSignalSpy>

using State = PerformFsm::PerformState;
using Effect = PerformFsm::Effect;
using Deck = PerformFsm::DeckInputs;
using Inputs = PerformFsm::Inputs;
using Drive = PerformFsm::Drive;
using Snapshot = PerformFsm::Snapshot;
using Facts = PerformFsm::ShowFacts;
using FactsOf = QHash<quint32, PerformFsm::ShowFacts>;
using Step = PerformFsm::Step;

static const quint32 None = PerformFsm::InvalidShowId;

Q_DECLARE_METATYPE(QList<PerformFsm::Step>)
Q_DECLARE_METATYPE(QList<PerformFsm::Drive>)

/** The decks from deck 1 on, the rest empty */
static Inputs decks(int active, const QList<Deck> &list)
{
    Inputs inputs;
    inputs.activeDeck = active;
    for (int i = 0; i < list.size(); i++)
        inputs.decks[i] = list[i];
    return inputs;
}

/** Perform on, driving these */
static Snapshot engaged(const QList<Drive> &drives = {})
{
    return Snapshot{ true, drives };
}

/** The active deck's Show */
static Drive dom(quint32 show, State state, int deck = 1)
{
    return Drive{ show, deck, state, true };
}

/** Another deck's Show */
static Drive sub(quint32 show, State state, int deck = 2)
{
    return Drive{ show, deck, state, false };
}

static QStringList names(const QList<Drive> &drives)
{
    QStringList out;
    for (const Drive &d : drives)
        out << QStringLiteral("%1@%2 %3%4").arg(d.showId).arg(d.deck).arg(PerformFsm::stateToString(d.state),
                                                                          d.dominant ? " dominant" : "");
    return out;
}

void PerformFsm_Test::initialStateIsIdle()
{
    PerformFsm fsm;
    QCOMPARE(fsm.state(), State::Idle);
    QCOMPARE(fsm.enabled(), false);
    QCOMPARE(fsm.activeShowId(), None);
    QCOMPARE(fsm.readOnly(), false);
    QVERIFY(fsm.snapshot() == Snapshot());
}

void PerformFsm_Test::stateTable_data()
{
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<Inputs>("inputs");
    QTest::addColumn<State>("state");
    QTest::addColumn<quint32>("activeShow");

    QTest::newRow("off, deck playing a resolved song") << false << decks(1, { { 42, true, false } }) << State::Idle << None;
    QTest::newRow("on, no show") << true << decks(1, { { None, true, false } }) << State::Armed << None;
    QTest::newRow("on, show, deck paused") << true << decks(1, { { 42, false, false } }) << State::Suspended << 42u;
    QTest::newRow("on, show ended, deck paused") << true << decks(1, { { 42, false, true } }) << State::Suspended << 42u;
    QTest::newRow("on, show, deck playing") << true << decks(1, { { 42, true, false } }) << State::Live << 42u;
    QTest::newRow("on, show ended, deck playing on") << true << decks(1, { { 42, true, true } }) << State::Ended << 42u;
    // the state is the active deck's
    QTest::newRow("on, no active deck") << true << decks(0, { { 42, true, false } }) << State::Armed << None;
    QTest::newRow("on, the active deck without a show")
        << true << decks(2, { { 42, true, false }, { None, true, false } }) << State::Armed << None;
    QTest::newRow("on, the second deck active")
        << true << decks(2, { { 41, true, false }, { 42, false, false } }) << State::Suspended << 42u;
    QTest::newRow("on, the active deck shares its show")
        << true << decks(2, { { 42, true, false }, { 42, false, false } }) << State::Suspended << 42u;
}

void PerformFsm_Test::stateTable()
{
    QFETCH(bool, enabled);
    QFETCH(Inputs, inputs);
    QFETCH(State, state);
    QFETCH(quint32, activeShow);

    PerformFsm fsm;
    fsm.setInputs(inputs);
    fsm.setPerformEnabled(enabled);
    QCOMPARE(fsm.state(), state);
    QCOMPARE(fsm.activeShowId(), activeShow);
    QCOMPARE(fsm.readOnly(), state != State::Idle);

    // the same inputs again change nothing
    QSignalSpy spy(&fsm, &PerformFsm::stateChanged);
    fsm.setInputs(inputs);
    fsm.setPerformEnabled(enabled);
    QCOMPARE(spy.count(), 0);
}

void PerformFsm_Test::drivesTable_data()
{
    QTest::addColumn<Inputs>("inputs");
    QTest::addColumn<QList<Drive>>("drives");

    QTest::newRow("no deck") << Inputs() << QList<Drive>{};
    QTest::newRow("one playing deck") << decks(1, { { 7, true, false } }) << QList<Drive>{ dom(7, State::Live) };
    QTest::newRow("two playing decks, the first active")
        << decks(1, { { 7, true, false }, { 8, true, false } })
        << QList<Drive>{ dom(7, State::Live), sub(8, State::Live) };
    QTest::newRow("two playing decks, the second active")
        << decks(2, { { 7, true, false }, { 8, true, false } })
        << QList<Drive>{ sub(7, State::Live, 1), dom(8, State::Live, 2) };
    QTest::newRow("a paused deck and a deck playing past its show")
        << decks(1, { { 7, false, false }, { 8, true, true } })
        << QList<Drive>{ dom(7, State::Suspended), sub(8, State::Ended) };
    QTest::newRow("a deck without a show drives nothing")
        << decks(1, { { None, true, false }, { 8, true, false } }) << QList<Drive>{ sub(8, State::Live) };
    QTest::newRow("no active deck: none dominant")
        << decks(0, { { 7, true, false }, { 8, false, false } })
        << QList<Drive>{ sub(7, State::Live, 1), sub(8, State::Suspended, 2) };
    QTest::newRow("four decks, the third active")
        << decks(3, { { 5, true, false }, { 6, false, false }, { 7, true, false }, { 8, true, true } })
        << QList<Drive>{ sub(5, State::Live, 1), sub(6, State::Suspended, 2), dom(7, State::Live, 3),
                         sub(8, State::Ended, 4) };
    QTest::newRow("one show on two decks: the active deck drives it")
        << decks(2, { { 7, false, false }, { 7, true, false } }) << QList<Drive>{ dom(7, State::Live, 2) };
    QTest::newRow("one show on two decks, neither active: the lower deck drives it")
        << decks(3, { { 7, true, false }, { 7, false, false }, { 9, true, false } })
        << QList<Drive>{ sub(7, State::Live, 1), dom(9, State::Live, 3) };
}

void PerformFsm_Test::drivesTable()
{
    QFETCH(Inputs, inputs);
    QFETCH(QList<Drive>, drives);

    PerformFsm fsm;
    fsm.setInputs(inputs);
    QCOMPARE(names(fsm.snapshot().drives), QStringList());
    fsm.setPerformEnabled(true);
    QVERIFY(fsm.snapshot().engaged);
    QCOMPARE(names(fsm.snapshot().drives), names(drives));
    fsm.setPerformEnabled(false);
    QVERIFY(fsm.snapshot() == Snapshot());
}

void PerformFsm_Test::inputChangeEmitsOnce_data()
{
    QTest::addColumn<Inputs>("from");
    QTest::addColumn<Inputs>("to");
    QTest::addColumn<int>("emissions");

    // deck handover: a paused deck 1 with A hands over to a playing deck 2 with B
    QTest::newRow("handover, show and transport at once")
        << decks(1, { { 1, false, false }, { 2, true, false } }) << decks(2, { { 1, false, false }, { 2, true, false } }) << 1;
    QTest::newRow("handover keeping Live")
        << decks(1, { { 1, true, false }, { 2, true, false } }) << decks(2, { { 1, true, false }, { 2, true, false } }) << 1;
    QTest::newRow("eject: show and transport gone at once")
        << decks(1, { { 1, true, false } }) << decks(1, { { None, false, false } }) << 1;
    QTest::newRow("play") << decks(1, { { 1, false, false } }) << decks(1, { { 1, true, false } }) << 1;
    QTest::newRow("the runner ended while paused") << decks(1, { { 1, false, false } }) << decks(1, { { 1, false, true } }) << 0;
    QTest::newRow("backward jump below where it ended") << decks(1, { { 1, true, true } }) << decks(1, { { 1, true, false } }) << 1;
    QTest::newRow("another deck plays")
        << decks(1, { { 1, true, false }, { 2, false, false } }) << decks(1, { { 1, true, false }, { 2, true, false } }) << 1;
    QTest::newRow("another deck plays a song without a show")
        << decks(1, { { 1, true, false }, { None, false, false } }) << decks(1, { { 1, true, false }, { None, true, false } }) << 0;
    QTest::newRow("the deck sharing the active deck's show pauses")
        << decks(1, { { 1, true, false }, { 1, true, false } }) << decks(1, { { 1, true, false }, { 1, false, false } }) << 0;
    QTest::newRow("a shared show follows the active deck")
        << decks(1, { { 1, true, false }, { 1, true, false } }) << decks(2, { { 1, true, false }, { 1, true, false } }) << 1;
    QTest::newRow("handover and a load in one write")
        << decks(1, { { 1, true, false }, { 2, true, false } }) << decks(2, { { 3, true, false }, { 2, true, false } }) << 1;
}

void PerformFsm_Test::inputChangeEmitsOnce()
{
    QFETCH(Inputs, from);
    QFETCH(Inputs, to);
    QFETCH(int, emissions);

    PerformFsm fsm;
    fsm.setPerformEnabled(true);
    fsm.setInputs(from);
    QSignalSpy spy(&fsm, &PerformFsm::stateChanged);
    fsm.setInputs(to);
    QCOMPARE(spy.count(), emissions);

    // the toggle is one more change each way
    fsm.setPerformEnabled(false);
    QCOMPARE(fsm.state(), State::Idle);
    QCOMPARE(spy.count(), emissions + 1);
    fsm.setPerformEnabled(true);
    QCOMPARE(spy.count(), emissions + 2);
}

void PerformFsm_Test::planTable_data()
{
    QTest::addColumn<Snapshot>("prev");
    QTest::addColumn<Snapshot>("next");
    QTest::addColumn<FactsOf>("facts");
    QTest::addColumn<QList<Step>>("steps");

    const Facts stopped{ false, false, false };
    const Facts queued{ false, false, true };
    const Facts running{ true, false, false };
    const Facts paused{ true, true, false };
    const Snapshot idle;
    const State Live = State::Live;
    const State Suspended = State::Suspended;
    const State Ended = State::Ended;

    // --- the active deck alone ---
    QTest::newRow("nothing to nothing") << idle << idle << FactsOf{} << QList<Step>{};
    QTest::newRow("engage without a show")
        << idle << engaged() << FactsOf{} << QList<Step>{ { Effect::ResolveRecorder, None } };
    QTest::newRow("engage on a paused deck")
        << idle << engaged({ dom(7, Suspended) }) << FactsOf{ { 7, stopped } }
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::ResolveRecorder, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("engage on a playing deck")
        << idle << engaged({ dom(7, Live) }) << FactsOf{ { 7, stopped } }
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::StartShow, 7 }, { Effect::ResolveRecorder, 7 },
                        { Effect::SelectShow, 7 } };
    QTest::newRow("engage on a playing deck, the show already running")
        << idle << engaged({ dom(7, Live) }) << FactsOf{ { 7, running } }
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::ResolveRecorder, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("engage on a playing deck whose show ended")
        << idle << engaged({ dom(7, Ended) }) << FactsOf{ { 7, stopped } }
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::ResolveRecorder, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("the deck's song resolves a show")
        << engaged() << engaged({ dom(7, Live) }) << FactsOf{ { 7, stopped } }
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::StartShow, 7 }, { Effect::ResolveRecorder, 7 },
                        { Effect::SelectShow, 7 } };
    QTest::newRow("play") << engaged({ dom(7, Suspended) }) << engaged({ dom(7, Live) }) << FactsOf{ { 7, stopped } }
                          << QList<Step>{ { Effect::StartShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("resume") << engaged({ dom(7, Suspended) }) << engaged({ dom(7, Live) }) << FactsOf{ { 7, paused } }
                            << QList<Step>{ { Effect::ResumeShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("pause") << engaged({ dom(7, Live) }) << engaged({ dom(7, Suspended) }) << FactsOf{ { 7, running } }
                           << QList<Step>{ { Effect::PauseShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("pause before the first tick cancels the start")
        << engaged({ dom(7, Live) }) << engaged({ dom(7, Suspended) }) << FactsOf{ { 7, queued } }
        << QList<Step>{ { Effect::StopShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("pause a show that never started")
        << engaged({ dom(7, Live) }) << engaged({ dom(7, Suspended) }) << FactsOf{ { 7, stopped } }
        << QList<Step>{ { Effect::SelectShow, 7 } };
    QTest::newRow("stopped from outside while live: started again")
        << engaged({ dom(7, Live) }) << engaged({ dom(7, Live) }) << FactsOf{ { 7, stopped } }
        << QList<Step>{ { Effect::StartShow, 7 } };
    QTest::newRow("live, start still queued")
        << engaged({ dom(7, Live) }) << engaged({ dom(7, Live) }) << FactsOf{ { 7, queued } } << QList<Step>{};
    QTest::newRow("the runner ended, the show still winding down")
        << engaged({ dom(7, Live) }) << engaged({ dom(7, Ended) }) << FactsOf{ { 7, running } }
        << QList<Step>{ { Effect::StopShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("ended, the runner stopped it")
        << engaged({ dom(7, Live) }) << engaged({ dom(7, Ended) }) << FactsOf{ { 7, stopped } }
        << QList<Step>{ { Effect::SelectShow, 7 } };
    QTest::newRow("ended: a paused deck plays on")
        << engaged({ dom(7, Suspended) }) << engaged({ dom(7, Ended) }) << FactsOf{ { 7, paused } }
        << QList<Step>{ { Effect::StopShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("ended: a paused deck plays on with a start still queued")
        << engaged({ dom(7, Suspended) }) << engaged({ dom(7, Ended) }) << FactsOf{ { 7, queued } }
        << QList<Step>{ { Effect::StopShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("ended, then the deck pauses")
        << engaged({ dom(7, Ended) }) << engaged({ dom(7, Suspended) }) << FactsOf{ { 7, stopped } }
        << QList<Step>{ { Effect::SelectShow, 7 } };
    QTest::newRow("backward jump below where it ended")
        << engaged({ dom(7, Ended) }) << engaged({ dom(7, Live) }) << FactsOf{ { 7, stopped } }
        << QList<Step>{ { Effect::StartShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("eject or disconnect: the deck is empty")
        << engaged({ dom(7, Live) }) << engaged() << FactsOf{}
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::StopShow, 7 }, { Effect::ReleaseShow, 7 } };
    QTest::newRow("disengage while live")
        << engaged({ dom(7, Live) }) << idle << FactsOf{}
        << QList<Step>{ { Effect::ReleaseRecorder, None }, { Effect::StopShow, 7 }, { Effect::ReleaseShow, 7 } };
    QTest::newRow("disengage while suspended")
        << engaged({ dom(7, Suspended) }) << idle << FactsOf{}
        << QList<Step>{ { Effect::ReleaseRecorder, None }, { Effect::StopShow, 7 }, { Effect::ReleaseShow, 7 } };
    QTest::newRow("disengage while armed")
        << engaged() << idle << FactsOf{} << QList<Step>{ { Effect::ReleaseRecorder, None } };

    // --- handover: the old Show plays on with its deck, dominance moves ---
    QTest::newRow("handover, both decks playing")
        << engaged({ dom(7, Live, 1), sub(8, Live, 2) }) << engaged({ sub(7, Live, 1), dom(8, Live, 2) })
        << FactsOf{ { 7, running }, { 8, running } }
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::DemoteShow, 7 }, { Effect::PromoteShow, 8 },
                        { Effect::ResolveRecorder, 8 }, { Effect::SelectShow, 8 } };
    QTest::newRow("handover to a paused deck")
        << engaged({ dom(7, Live, 1), sub(8, Suspended, 2) }) << engaged({ sub(7, Live, 1), dom(8, Suspended, 2) })
        << FactsOf{ { 7, running }, { 8, paused } }
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::DemoteShow, 7 }, { Effect::PromoteShow, 8 },
                        { Effect::ResolveRecorder, 8 }, { Effect::SelectShow, 8 } };
    QTest::newRow("handover from a paused deck")
        << engaged({ dom(7, Suspended, 1), sub(8, Live, 2) }) << engaged({ sub(7, Suspended, 1), dom(8, Live, 2) })
        << FactsOf{ { 7, paused }, { 8, running } }
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::DemoteShow, 7 }, { Effect::PromoteShow, 8 },
                        { Effect::ResolveRecorder, 8 }, { Effect::SelectShow, 8 } };
    QTest::newRow("handover and play in one write")
        << engaged({ dom(7, Live, 1), sub(8, Suspended, 2) }) << engaged({ sub(7, Live, 1), dom(8, Live, 2) })
        << FactsOf{ { 7, running }, { 8, paused } }
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::DemoteShow, 7 }, { Effect::PromoteShow, 8 },
                        { Effect::ResumeShow, 8 }, { Effect::ResolveRecorder, 8 }, { Effect::SelectShow, 8 } };
    QTest::newRow("handover to a deck without a show")
        << engaged({ dom(7, Live, 1) }) << engaged({ sub(7, Live, 1) }) << FactsOf{ { 7, running } }
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::DemoteShow, 7 } };
    QTest::newRow("handover from a deck without a show")
        << engaged({ sub(8, Live, 2) }) << engaged({ dom(8, Live, 2) }) << FactsOf{ { 8, running } }
        << QList<Step>{ { Effect::PromoteShow, 8 }, { Effect::ResolveRecorder, 8 }, { Effect::SelectShow, 8 } };
    QTest::newRow("handover onto a load")
        << engaged({ dom(7, Live, 1) }) << engaged({ sub(7, Live, 1), dom(9, Live, 2) })
        << FactsOf{ { 7, running }, { 9, stopped } }
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::DemoteShow, 7 }, { Effect::AdoptShow, 9 },
                        { Effect::StartShow, 9 }, { Effect::ResolveRecorder, 9 }, { Effect::SelectShow, 9 } };
    QTest::newRow("handover while the old deck ejects")
        << engaged({ dom(7, Live, 1), sub(8, Live, 2) }) << engaged({ dom(8, Live, 2) }) << FactsOf{ { 8, running } }
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::StopShow, 7 }, { Effect::ReleaseShow, 7 },
                        { Effect::PromoteShow, 8 }, { Effect::ResolveRecorder, 8 }, { Effect::SelectShow, 8 } };

    // --- the other decks: their Shows follow them, the recorder and the Show Manager do not ---
    QTest::newRow("engage on two playing decks")
        << idle << engaged({ dom(7, Live, 1), sub(8, Live, 2) }) << FactsOf{ { 7, stopped }, { 8, stopped } }
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::StartShow, 7 }, { Effect::AdoptShow, 8 },
                        { Effect::StartShow, 8 }, { Effect::ResolveRecorder, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("engage with the second deck active")
        << idle << engaged({ sub(7, Live, 1), dom(8, Suspended, 2) }) << FactsOf{ { 7, stopped }, { 8, stopped } }
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::StartShow, 7 }, { Effect::AdoptShow, 8 },
                        { Effect::ResolveRecorder, 8 }, { Effect::SelectShow, 8 } };
    QTest::newRow("engage, only another deck has a show")
        << idle << engaged({ sub(8, Live, 2) }) << FactsOf{ { 8, stopped } }
        << QList<Step>{ { Effect::AdoptShow, 8 }, { Effect::StartShow, 8 }, { Effect::ResolveRecorder, None } };
    QTest::newRow("another deck loads a show")
        << engaged({ dom(7, Live) }) << engaged({ dom(7, Live), sub(8, Suspended) })
        << FactsOf{ { 7, running }, { 8, stopped } } << QList<Step>{ { Effect::AdoptShow, 8 } };
    QTest::newRow("another deck plays")
        << engaged({ dom(7, Live), sub(8, Suspended) }) << engaged({ dom(7, Live), sub(8, Live) })
        << FactsOf{ { 7, running }, { 8, stopped } } << QList<Step>{ { Effect::StartShow, 8 } };
    QTest::newRow("another deck pauses")
        << engaged({ dom(7, Live), sub(8, Live) }) << engaged({ dom(7, Live), sub(8, Suspended) })
        << FactsOf{ { 7, running }, { 8, running } } << QList<Step>{ { Effect::PauseShow, 8 } };
    QTest::newRow("another deck resumes")
        << engaged({ dom(7, Live), sub(8, Suspended) }) << engaged({ dom(7, Live), sub(8, Live) })
        << FactsOf{ { 7, running }, { 8, paused } } << QList<Step>{ { Effect::ResumeShow, 8 } };
    QTest::newRow("another deck ejects")
        << engaged({ dom(7, Live), sub(8, Live) }) << engaged({ dom(7, Live) }) << FactsOf{ { 7, running } }
        << QList<Step>{ { Effect::StopShow, 8 }, { Effect::ReleaseShow, 8 } };
    QTest::newRow("another deck changes its song")
        << engaged({ dom(7, Live), sub(8, Live) }) << engaged({ dom(7, Live), sub(9, Live) })
        << FactsOf{ { 7, running }, { 9, stopped } }
        << QList<Step>{ { Effect::StopShow, 8 }, { Effect::ReleaseShow, 8 }, { Effect::AdoptShow, 9 },
                        { Effect::StartShow, 9 } };
    QTest::newRow("another deck's show ended")
        << engaged({ dom(7, Live), sub(8, Live) }) << engaged({ dom(7, Live), sub(8, Ended) })
        << FactsOf{ { 7, running }, { 8, running } } << QList<Step>{ { Effect::StopShow, 8 } };
    QTest::newRow("another deck's show stopped from outside: started again")
        << engaged({ dom(7, Live), sub(8, Live) }) << engaged({ dom(7, Live), sub(8, Live) })
        << FactsOf{ { 7, running }, { 8, stopped } } << QList<Step>{ { Effect::StartShow, 8 } };

    // --- one Show on two decks: it moves, it never restarts ---
    QTest::newRow("a shared show follows the active deck")
        << engaged({ dom(7, Live, 1) }) << engaged({ dom(7, Live, 2) }) << FactsOf{ { 7, running } }
        << QList<Step>{ { Effect::FollowDeck, 7 } };
    QTest::newRow("a shared show follows the active deck, paused there")
        << engaged({ dom(7, Live, 1) }) << engaged({ dom(7, Suspended, 2) }) << FactsOf{ { 7, running } }
        << QList<Step>{ { Effect::FollowDeck, 7 }, { Effect::PauseShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("a shared show takes the new active deck's clock before it is promoted")
        << engaged({ sub(7, Live, 1), dom(9, Live, 3) }) << engaged({ dom(7, Live, 2), sub(9, Live, 3) })
        << FactsOf{ { 7, running }, { 9, running } }
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::DemoteShow, 9 }, { Effect::FollowDeck, 7 },
                        { Effect::PromoteShow, 7 }, { Effect::ResolveRecorder, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("a shared show stays with the lower deck when the active deck moves on")
        << engaged({ dom(7, Live, 2) }) << engaged({ sub(7, Live, 1), dom(9, Live, 3) })
        << FactsOf{ { 7, running }, { 9, stopped } }
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::DemoteShow, 7 }, { Effect::FollowDeck, 7 },
                        { Effect::AdoptShow, 9 }, { Effect::StartShow, 9 }, { Effect::ResolveRecorder, 9 },
                        { Effect::SelectShow, 9 } };

    // --- every driven Show leaves ---
    QTest::newRow("disengage with two decks")
        << engaged({ dom(7, Live, 1), sub(8, Suspended, 2) }) << idle << FactsOf{}
        << QList<Step>{ { Effect::ReleaseRecorder, None }, { Effect::StopShow, 7 }, { Effect::ReleaseShow, 7 },
                        { Effect::StopShow, 8 }, { Effect::ReleaseShow, 8 } };
    QTest::newRow("disengage, only another deck has a show")
        << engaged({ sub(8, Live, 2) }) << idle << FactsOf{}
        << QList<Step>{ { Effect::ReleaseRecorder, None }, { Effect::StopShow, 8 }, { Effect::ReleaseShow, 8 } };
    QTest::newRow("disconnect with two decks")
        << engaged({ sub(7, Live, 1), dom(8, Live, 2) }) << engaged() << FactsOf{}
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::StopShow, 7 }, { Effect::ReleaseShow, 7 },
                        { Effect::StopShow, 8 }, { Effect::ReleaseShow, 8 } };
}

void PerformFsm_Test::planTable()
{
    QFETCH(Snapshot, prev);
    QFETCH(Snapshot, next);
    QFETCH(FactsOf, facts);
    QFETCH(QList<Step>, steps);

    const auto names = [](const QList<Step> &list) {
        QStringList out;
        for (const Step &step : list)
            out << QStringLiteral("%1(%2)").arg(QMetaEnum::fromType<Effect>().valueToKey(int(step.effect))).arg(step.showId);
        return out;
    };
    QCOMPARE(names(PerformFsm::plan(prev, next, facts)), names(steps));
}

void PerformFsm_Test::stateToStringCoversAll()
{
    QCOMPARE(PerformFsm::stateToString(State::Idle), QString("Idle"));
    QCOMPARE(PerformFsm::stateToString(State::Armed), QString("Armed"));
    QCOMPARE(PerformFsm::stateToString(State::Live), QString("Live"));
    QCOMPARE(PerformFsm::stateToString(State::Suspended), QString("Suspended"));
    QCOMPARE(PerformFsm::stateToString(State::Ended), QString("Ended"));
}

QTEST_GUILESS_MAIN(PerformFsm_Test)
