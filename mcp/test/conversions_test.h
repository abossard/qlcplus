/*
  Q Light Controller Plus - Unit test
  conversions_test.h

  Tests for beatStringToValue / valueToBeatString in mcp/tools/conversions.h

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#ifndef CONVERSIONS_TEST_H
#define CONVERSIONS_TEST_H

#include <QObject>

class Conversions_Test : public QObject
{
    Q_OBJECT

private slots:
    void beatStringToValue_fractions_data();
    void beatStringToValue_fractions();

    void beatStringToValue_decimal_data();
    void beatStringToValue_decimal();

    void beatStringToValue_invalid_data();
    void beatStringToValue_invalid();

    void beatStringToValue_overflow();

    void qlcStringToTime_beatTextInput_data();
    void qlcStringToTime_beatTextInput();

    void timeToQlcString_currentBeatDisplay_data();
    void timeToQlcString_currentBeatDisplay();

    void engineBeatTiming_characterization_data();
    void engineBeatTiming_characterization();

    void engineBeatTiming_roundTripLoss();

    void snapToBeatGrid_data();
    void snapToBeatGrid();

    void durationBeatSnap_data();
    void durationBeatSnap();

    void holdSpeedBeatSnap();

    void beatsToTimeRounding_data();
    void beatsToTimeRounding();

    void valueToBeatString_canonical_data();
    void valueToBeatString_canonical();

    void valueToBeatString_composite_data();
    void valueToBeatString_composite();

    void valueToBeatString_offgrid();

    void valueToBeatString_zero();

    void valueToBeatString_gcdReduction_data();
    void valueToBeatString_gcdReduction();

    void roundTrip_allCanonical();
};

#endif // CONVERSIONS_TEST_H
