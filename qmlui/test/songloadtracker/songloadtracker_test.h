/*
  Q Light Controller Plus - Unit test
  songloadtracker_test.h
*/

#ifndef SONGLOADTRACKER_TEST_H
#define SONGLOADTRACKER_TEST_H

#include <QObject>

class SongLoadTracker_Test : public QObject
{
    Q_OBJECT

private slots:
    void initialState();
    void normalLoadSequence_data();
    void normalLoadSequence();
    void outOfOrderFields();
    void placeholderTitleFiltered();
    void emptyFilepathResets();
    void linkedDeckDedup();
    void disconnectResetsAll();
    void loadedOffClearsLoadedBit();
    void newFilepathResetsSlot();
};

#endif // SONGLOADTRACKER_TEST_H
