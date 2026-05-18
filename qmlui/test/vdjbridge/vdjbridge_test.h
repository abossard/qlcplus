/*
  Q Light Controller Plus - Unit test
  vdjbridge_test.h
*/

#ifndef VDJBRIDGE_TEST_H
#define VDJBRIDGE_TEST_H

#include <QObject>

class VdjBridge_Test : public QObject
{
    Q_OBJECT

private slots:
    void initialState();
    void beatUpdatesBpmAndConnected();
    void beatChangeResetsCounter();
};

#endif
