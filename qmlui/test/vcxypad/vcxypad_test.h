/*
  Q Light Controller Plus - Unit test
  vcxypad_test.h

  Licensed under the Apache License, Version 2.0
*/

#ifndef VCXYPAD_TEST_H
#define VCXYPAD_TEST_H

#include <QObject>

class VCXYPad_Test : public QObject
{
    Q_OBJECT

private slots:
    /* fork API (normalized qreal) -> upstream API (display units) */
    void forkRangeReadBackAsDisplayUnits_data();
    void forkRangeReadBackAsDisplayUnits();

    /* upstream API (display units) -> fork API (normalized qreal) */
    void displayRangeStoredAsNormalized_data();
    void displayRangeStoredAsNormalized();

    /* entry addressing */
    void groupEntryMatchedByGroupId();
    void plainHeadMatchedByFixtureAndHead();
    void forkApiIgnoresGroupIdGuard();

    /* degrees scaling */
    void degreesScaleIsSmallestSpanOfSelection();
    void degreesWriteScaleIsResolvedPerEntry();
    void degreesWithoutSpanLeavesRangeUntouched();

    /* removal */
    void removeHead_data();
    void removeHead();
    void removeHeadsRemovesTheSelectedRow();

    /* selection edge cases */
    void headsRangeInfoOfEmptySelection();
};

#endif // VCXYPAD_TEST_H
