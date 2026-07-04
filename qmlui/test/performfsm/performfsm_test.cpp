/*
  Q Light Controller Plus - Unit test
  performfsm_test.cpp

  Licensed under the Apache License, Version 2.0
*/

#include "performfsm_test.h"
#include "performfsm.h"

#include <QSignalSpy>

using State = PerformFsm::PerformState;

void PerformFsm_Test::initialStateIsIdle()
{
    PerformFsm fsm;
    QCOMPARE(fsm.state(), State::Idle);
    QCOMPARE(fsm.enabled(), false);
    QCOMPARE(fsm.activeShowId(), PerformFsm::InvalidShowId);
    QCOMPARE(fsm.readOnly(), false);
}

void PerformFsm_Test::enableWithoutShowArms()
{
    PerformFsm fsm;
    QSignalSpy stateSpy(&fsm, &PerformFsm::stateChanged);

    fsm.setPerformEnabled(true);
    QCOMPARE(fsm.state(), State::Armed);
    QCOMPARE(stateSpy.count(), 1);
    QCOMPARE(stateSpy[0][0].value<State>(), State::Armed);

    // idempotent
    fsm.setPerformEnabled(true);
    QCOMPARE(stateSpy.count(), 1);
}

void PerformFsm_Test::showResolutionMovesArmedToSuspendedOrLive()
{
    PerformFsm fsm;
    fsm.setPerformEnabled(true);
    QCOMPARE(fsm.state(), State::Armed);

    QSignalSpy showSpy(&fsm, &PerformFsm::activeShowChanged);

    // deck not playing: resolving a show yields Suspended
    fsm.setActiveShow(42);
    QCOMPARE(fsm.state(), State::Suspended);
    QCOMPARE(fsm.activeShowId(), 42u);
    QCOMPARE(showSpy.count(), 1);
    QCOMPARE(showSpy[0][0].toUInt(), 42u);

    // deck playing while Armed: resolving goes straight to Live
    PerformFsm fsm2;
    fsm2.setPerformEnabled(true);
    fsm2.setDeckPlaying(true);
    QCOMPARE(fsm2.state(), State::Armed); // playing alone is not enough
    fsm2.setActiveShow(7);
    QCOMPARE(fsm2.state(), State::Live);
}

void PerformFsm_Test::playPauseTogglesLiveSuspended()
{
    PerformFsm fsm;
    fsm.setPerformEnabled(true);
    fsm.setActiveShow(42);
    QCOMPARE(fsm.state(), State::Suspended);

    QSignalSpy stateSpy(&fsm, &PerformFsm::stateChanged);

    fsm.setDeckPlaying(true);
    QCOMPARE(fsm.state(), State::Live);
    fsm.setDeckPlaying(false); // "VDJ stops -> the progression stops"
    QCOMPARE(fsm.state(), State::Suspended);
    fsm.setDeckPlaying(true);
    QCOMPARE(fsm.state(), State::Live);

    QCOMPARE(stateSpy.count(), 3);
}

void PerformFsm_Test::deckSwitchEmitsActiveShowChangedKeepingState()
{
    PerformFsm fsm;
    fsm.setPerformEnabled(true);
    fsm.setDeckPlaying(true);
    fsm.setActiveShow(1);
    QCOMPARE(fsm.state(), State::Live);

    QSignalSpy stateSpy(&fsm, &PerformFsm::stateChanged);
    QSignalSpy showSpy(&fsm, &PerformFsm::activeShowChanged);

    // handover: Live -> Live, but consumers must hear about the new show
    fsm.setActiveShow(2);
    QCOMPARE(fsm.state(), State::Live);
    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(showSpy.count(), 1);
    QCOMPARE(showSpy[0][0].toUInt(), 2u);

    // losing the show (deck without one) falls back to Armed
    fsm.setActiveShow(PerformFsm::InvalidShowId);
    QCOMPARE(fsm.state(), State::Armed);
    QCOMPARE(showSpy.count(), 2);
}

void PerformFsm_Test::disableReturnsToIdleFromAnyState()
{
    PerformFsm fsm;
    fsm.setPerformEnabled(true);
    fsm.setActiveShow(42);
    fsm.setDeckPlaying(true);
    QCOMPARE(fsm.state(), State::Live);

    fsm.setPerformEnabled(false);
    QCOMPARE(fsm.state(), State::Idle);
    QCOMPARE(fsm.readOnly(), false);
    // inputs are retained; re-enable restores Live
    fsm.setPerformEnabled(true);
    QCOMPARE(fsm.state(), State::Live);
}

void PerformFsm_Test::resetKeepsToggleAndReArms()
{
    PerformFsm fsm;
    fsm.setPerformEnabled(true);
    fsm.setActiveShow(42);
    fsm.setDeckPlaying(true);
    QCOMPARE(fsm.state(), State::Live);

    QSignalSpy showSpy(&fsm, &PerformFsm::activeShowChanged);

    fsm.reset(); // VDJ disconnect
    QCOMPARE(fsm.state(), State::Armed);      // toggle survives, show is gone
    QCOMPARE(fsm.enabled(), true);
    QCOMPARE(fsm.activeShowId(), PerformFsm::InvalidShowId);
    QCOMPARE(showSpy.count(), 1);
    QCOMPARE(showSpy[0][0].toUInt(), PerformFsm::InvalidShowId);

    // reset while Idle stays Idle and does not re-emit
    PerformFsm idle;
    QSignalSpy idleShowSpy(&idle, &PerformFsm::activeShowChanged);
    idle.reset();
    QCOMPARE(idle.state(), State::Idle);
    QCOMPARE(idleShowSpy.count(), 0);
}

void PerformFsm_Test::readOnlyDerivation()
{
    PerformFsm fsm;
    QCOMPARE(fsm.readOnly(), false);
    fsm.setPerformEnabled(true);
    QCOMPARE(fsm.readOnly(), true);   // Armed is engaged: read-only
    fsm.setActiveShow(42);
    QCOMPARE(fsm.readOnly(), true);   // Suspended
    fsm.setDeckPlaying(true);
    QCOMPARE(fsm.readOnly(), true);   // Live
    fsm.setPerformEnabled(false);
    QCOMPARE(fsm.readOnly(), false);  // Idle
}

void PerformFsm_Test::inputsWhileDisabledDoNotChangeState()
{
    PerformFsm fsm;
    QSignalSpy stateSpy(&fsm, &PerformFsm::stateChanged);
    QSignalSpy showSpy(&fsm, &PerformFsm::activeShowChanged);

    // inputs are tracked but the state stays Idle until enabled
    fsm.setActiveShow(42);
    fsm.setDeckPlaying(true);
    QCOMPARE(fsm.state(), State::Idle);
    QCOMPARE(stateSpy.count(), 0);
    QCOMPARE(showSpy.count(), 1); // show tracking is still announced

    fsm.setPerformEnabled(true);
    QCOMPARE(fsm.state(), State::Live); // pre-set inputs take effect at once
}

void PerformFsm_Test::stateToStringCoversAll()
{
    QCOMPARE(PerformFsm::stateToString(State::Idle), QString("Idle"));
    QCOMPARE(PerformFsm::stateToString(State::Armed), QString("Armed"));
    QCOMPARE(PerformFsm::stateToString(State::Live), QString("Live"));
    QCOMPARE(PerformFsm::stateToString(State::Suspended), QString("Suspended"));
}

QTEST_GUILESS_MAIN(PerformFsm_Test)
