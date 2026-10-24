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
using Inputs = PerformFsm::Inputs;
using Facts = PerformFsm::ShowFacts;
using Step = PerformFsm::Step;

static const quint32 None = PerformFsm::InvalidShowId;

Q_DECLARE_METATYPE(QList<PerformFsm::Step>)

void PerformFsm_Test::initialStateIsIdle()
{
    PerformFsm fsm;
    QCOMPARE(fsm.state(), State::Idle);
    QCOMPARE(fsm.enabled(), false);
    QCOMPARE(fsm.activeShowId(), None);
    QCOMPARE(fsm.readOnly(), false);
}

void PerformFsm_Test::stateTable_data()
{
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<Inputs>("inputs");
    QTest::addColumn<State>("state");
    QTest::addColumn<quint32>("activeShow");

    QTest::newRow("off, deck playing a resolved song") << false << Inputs{ 42, true, false } << State::Idle << None;
    QTest::newRow("on, no show") << true << Inputs{ None, true, false } << State::Armed << None;
    QTest::newRow("on, show, deck paused") << true << Inputs{ 42, false, false } << State::Suspended << 42u;
    QTest::newRow("on, show ended, deck paused") << true << Inputs{ 42, false, true } << State::Suspended << 42u;
    QTest::newRow("on, show, deck playing") << true << Inputs{ 42, true, false } << State::Live << 42u;
    QTest::newRow("on, show ended, deck playing on") << true << Inputs{ 42, true, true } << State::Ended << 42u;
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

void PerformFsm_Test::inputChangeEmitsOnce_data()
{
    QTest::addColumn<Inputs>("from");
    QTest::addColumn<Inputs>("to");
    QTest::addColumn<int>("emissions");

    // deck handover: a paused deck 1 with A hands over to a playing deck 2 with B
    QTest::newRow("handover, show and transport at once") << Inputs{ 1, false, false } << Inputs{ 2, true, false } << 1;
    QTest::newRow("handover keeping Live") << Inputs{ 1, true, false } << Inputs{ 2, true, false } << 1;
    QTest::newRow("eject: show and transport gone at once") << Inputs{ 1, true, false } << Inputs{ None, false, false } << 1;
    QTest::newRow("play") << Inputs{ 1, false, false } << Inputs{ 1, true, false } << 1;
    QTest::newRow("the runner ended while paused") << Inputs{ 1, false, false } << Inputs{ 1, false, true } << 0;
    QTest::newRow("backward jump below where it ended") << Inputs{ 1, true, true } << Inputs{ 1, true, false } << 1;
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
    QTest::addColumn<State>("prevState");
    QTest::addColumn<quint32>("prevShow");
    QTest::addColumn<State>("state");
    QTest::addColumn<quint32>("show");
    QTest::addColumn<Facts>("facts");
    QTest::addColumn<QList<Step>>("steps");

    const Facts stopped{ false, false, false };
    const Facts queued{ false, false, true };
    const Facts running{ true, false, false };
    const Facts paused{ true, true, false };

    QTest::newRow("nothing to nothing") << State::Idle << None << State::Idle << None << stopped << QList<Step>{};
    QTest::newRow("engage without a show")
        << State::Idle << None << State::Armed << None << stopped
        << QList<Step>{ { Effect::ResolveRecorder, None } };
    QTest::newRow("engage on a paused deck")
        << State::Idle << None << State::Suspended << 7u << stopped
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::ResolveRecorder, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("engage on a playing deck")
        << State::Idle << None << State::Live << 7u << stopped
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::StartShow, 7 }, { Effect::ResolveRecorder, 7 },
                        { Effect::SelectShow, 7 } };
    QTest::newRow("engage on a playing deck, the show already running")
        << State::Idle << None << State::Live << 7u << running
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::ResolveRecorder, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("engage on a playing deck whose show ended")
        << State::Idle << None << State::Ended << 7u << stopped
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::ResolveRecorder, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("the deck's song resolves a show")
        << State::Armed << None << State::Live << 7u << stopped
        << QList<Step>{ { Effect::AdoptShow, 7 }, { Effect::StartShow, 7 }, { Effect::ResolveRecorder, 7 },
                        { Effect::SelectShow, 7 } };
    QTest::newRow("play") << State::Suspended << 7u << State::Live << 7u << stopped
                          << QList<Step>{ { Effect::StartShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("resume") << State::Suspended << 7u << State::Live << 7u << paused
                            << QList<Step>{ { Effect::ResumeShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("pause") << State::Live << 7u << State::Suspended << 7u << running
                           << QList<Step>{ { Effect::PauseShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("pause before the first tick cancels the start")
        << State::Live << 7u << State::Suspended << 7u << queued
        << QList<Step>{ { Effect::StopShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("pause a show that never started") << State::Live << 7u << State::Suspended << 7u << stopped
                                                     << QList<Step>{ { Effect::SelectShow, 7 } };
    QTest::newRow("stopped from outside while live: started again")
        << State::Live << 7u << State::Live << 7u << stopped << QList<Step>{ { Effect::StartShow, 7 } };
    QTest::newRow("live, start still queued") << State::Live << 7u << State::Live << 7u << queued << QList<Step>{};
    QTest::newRow("the runner ended, the show still winding down")
        << State::Live << 7u << State::Ended << 7u << running
        << QList<Step>{ { Effect::StopShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("ended, the runner stopped it") << State::Live << 7u << State::Ended << 7u << stopped
                                                  << QList<Step>{ { Effect::SelectShow, 7 } };
    QTest::newRow("ended: a paused deck plays on")
        << State::Suspended << 7u << State::Ended << 7u << paused
        << QList<Step>{ { Effect::StopShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("ended: a paused deck plays on with a start still queued")
        << State::Suspended << 7u << State::Ended << 7u << queued
        << QList<Step>{ { Effect::StopShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("ended, then the deck pauses") << State::Ended << 7u << State::Suspended << 7u << stopped
                                                 << QList<Step>{ { Effect::SelectShow, 7 } };
    QTest::newRow("backward jump below where it ended") << State::Ended << 7u << State::Live << 7u << stopped
                                                        << QList<Step>{ { Effect::StartShow, 7 }, { Effect::SelectShow, 7 } };
    QTest::newRow("handover, both decks playing")
        << State::Live << 7u << State::Live << 8u << stopped
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::StopShow, 7 }, { Effect::ReleaseShow, 7 },
                        { Effect::AdoptShow, 8 }, { Effect::StartShow, 8 }, { Effect::ResolveRecorder, 8 },
                        { Effect::SelectShow, 8 } };
    QTest::newRow("handover to a paused deck")
        << State::Live << 7u << State::Suspended << 8u << stopped
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::StopShow, 7 }, { Effect::ReleaseShow, 7 },
                        { Effect::AdoptShow, 8 }, { Effect::ResolveRecorder, 8 }, { Effect::SelectShow, 8 } };
    QTest::newRow("eject or disconnect: the deck is empty")
        << State::Live << 7u << State::Armed << None << stopped
        << QList<Step>{ { Effect::ResolveRecorder, None }, { Effect::StopShow, 7 }, { Effect::ReleaseShow, 7 } };
    QTest::newRow("disengage while live")
        << State::Live << 7u << State::Idle << None << running
        << QList<Step>{ { Effect::ReleaseRecorder, None }, { Effect::StopShow, 7 }, { Effect::ReleaseShow, 7 } };
    QTest::newRow("disengage while suspended")
        << State::Suspended << 7u << State::Idle << None << paused
        << QList<Step>{ { Effect::ReleaseRecorder, None }, { Effect::StopShow, 7 }, { Effect::ReleaseShow, 7 } };
    QTest::newRow("disengage while armed") << State::Armed << None << State::Idle << None << stopped
                                           << QList<Step>{ { Effect::ReleaseRecorder, None } };
}

void PerformFsm_Test::planTable()
{
    QFETCH(State, prevState);
    QFETCH(quint32, prevShow);
    QFETCH(State, state);
    QFETCH(quint32, show);
    QFETCH(Facts, facts);
    QFETCH(QList<Step>, steps);

    const auto names = [](const QList<Step> &list) {
        QStringList out;
        for (const Step &step : list)
            out << QStringLiteral("%1(%2)").arg(QMetaEnum::fromType<Effect>().valueToKey(int(step.effect))).arg(step.showId);
        return out;
    };
    QCOMPARE(names(PerformFsm::plan(prevState, prevShow, state, show, facts)), names(steps));
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
