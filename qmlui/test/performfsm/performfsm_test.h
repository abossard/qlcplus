/*
  Q Light Controller Plus - Unit test
  performfsm_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef PERFORMFSM_TEST_H
#define PERFORMFSM_TEST_H

#include <QObject>
#include <QtTest>

class PerformFsm_Test : public QObject
{
    Q_OBJECT

private slots:
    void initialStateIsIdle();
    void enableWithoutShowArms();
    void showResolutionMovesArmedToSuspendedOrLive();
    void playPauseTogglesLiveSuspended();
    void deckSwitchEmitsActiveShowChangedKeepingState();
    void disableReturnsToIdleFromAnyState();
    void resetKeepsToggleAndReArms();
    void readOnlyDerivation();
    void inputsWhileDisabledDoNotChangeState();
    void stateToStringCoversAll();
};

#endif // PERFORMFSM_TEST_H
