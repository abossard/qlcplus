/*
  Q Light Controller Plus - Unit test
  showfactory_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef SHOWFACTORY_TEST_H
#define SHOWFACTORY_TEST_H

#include <QObject>
#include <QtTest>

class ShowFactory_Test : public QObject
{
    Q_OBJECT

private slots:
    void showNameForSong_data();
    void showNameForSong();
    void buildShow_data();
    void buildShow();
    void indexFollowsTheDoc();
    void choicesWinUntilTheDocIsCleared();
    void overrideGoesWithItsShow();
    void choicesSurviveSaveAndLoad_data();
    void choicesSurviveSaveAndLoad();
    void choicesFollowTheWorkspaceFolder();
    void malformedChoiceIsDropped();
    void clipChangesReachTheIndex();
    void normalizePath_data();
    void normalizePath();
};

#endif // SHOWFACTORY_TEST_H
