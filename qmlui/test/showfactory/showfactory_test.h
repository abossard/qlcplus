/*
  Q Light Controller Plus - Unit test
  showfactory_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef SHOWFACTORY_TEST_H
#define SHOWFACTORY_TEST_H

#include <QObject>

class Doc;

class ShowFactory_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void createShowForSong_data();
    void createShowForSong();
    void dedupByName();
    void showIdForFilepath();
    void signalEmitted();
    void emptyFilepathRejected();

private:
    Doc *m_doc = nullptr;
};

#endif // SHOWFACTORY_TEST_H
