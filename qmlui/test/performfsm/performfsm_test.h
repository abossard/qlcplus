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
    void stateTable_data();
    void stateTable();
    void inputChangeEmitsOnce_data();
    void inputChangeEmitsOnce();
    void planTable_data();
    void planTable();
    void stateToStringCoversAll();
};

#endif // PERFORMFSM_TEST_H
