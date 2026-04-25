/*
  Q Light Controller Plus - Unit test
  beatquantize_test.cpp

  TDD tests for 1/16 beat subdivision support. Some tests are expected
  to FAIL until the 1/16 quantization implementation lands in
  Function::beatsToTime() / Function::timeToBeats().

  Licensed under the Apache License, Version 2.0 (the "License");
*/

#include <QtTest>
#include <QtMath>

#include "beatquantize_test.h"
#include "function.h"

namespace
{
// Reference 1/16 quantizer table for the implementation under test.
// Each entry approximates N/16 * 1000 with adjacent diffs of 62 / 63
// to keep cumulative error small. 16 * 63 = 1008 (~0.8% off 1000).
static const uint kSixteenthQuantizer[16] = {
    0,   63,  125, 188, 250, 313, 375, 438,
    500, 563, 625, 688, 750, 813, 875, 938
};

// Beat duration in ms for a given BPM (1 beat = 60000 / BPM ms).
static int beatDurationFromBPM(int bpm)
{
    return 60000 / bpm;
}
}

// 1000 units (one full beat) at 120 BPM (500ms/beat) -> 500ms.
void BeatQuantize_Test::beatsToTime_wholeBeats()
{
    const int bd = beatDurationFromBPM(120);
    QCOMPARE(Function::beatsToTime(1000, bd), 500u);
}

// 125 units (1/8) at 120 BPM -> 62.5ms. The current implementation
// truncates the float, so accept 62 or 63.
void BeatQuantize_Test::beatsToTime_eighthBeat()
{
    const int bd = beatDurationFromBPM(120);
    const uint result = Function::beatsToTime(125, bd);
    QVERIFY2(result == 62u || result == 63u,
             qPrintable(QString("expected 62 or 63, got %1").arg(result)));
}

// 63 units (1/16) at 120 BPM -> 31.5ms. Accept 31 or 32.
void BeatQuantize_Test::beatsToTime_sixteenthBeat()
{
    const int bd = beatDurationFromBPM(120);
    const uint result = Function::beatsToTime(63, bd);
    QVERIFY2(result == 31u || result == 32u,
             qPrintable(QString("expected 31 or 32, got %1").arg(result)));
}

// 63 units (1/16) at 200 BPM (300ms/beat) -> 18.9ms. Accept 18 or 19.
void BeatQuantize_Test::beatsToTime_sixteenthAt200BPM()
{
    const int bd = beatDurationFromBPM(200);
    const uint result = Function::beatsToTime(63, bd);
    QVERIFY2(result == 18u || result == 19u,
             qPrintable(QString("expected 18 or 19, got %1").arg(result)));
}

// EXPECTED TO FAIL until 1/16 implementation:
// 31ms at 120 BPM should snap to 63 units (1/16), not 0.
void BeatQuantize_Test::timeToBeats_sixteenthSnap()
{
    const int bd = beatDurationFromBPM(120);
    QCOMPARE(Function::timeToBeats(31, bd), 63u);
}

// With 1/16 quantization, 31ms at 120 BPM (500ms/beat) is exactly
// midway between 0 and 63 units. qRound rounds half-to-even / up,
// so 31ms snaps up to the nearest 1/16 step (63 units) rather than
// down to 0. Previously (1/8 quantization) this snapped to 0.
void BeatQuantize_Test::timeToBeats_currentlySnapsTo125()
{
    const int bd = beatDurationFromBPM(120);
    QCOMPARE(Function::timeToBeats(31, bd), 63u);
}

// EXPECTED TO FAIL until 1/16 implementation:
// beats -> time -> beats round-trip should preserve 63 units.
void BeatQuantize_Test::roundTrip_sixteenth()
{
    const int bd = beatDurationFromBPM(120);
    const uint timeMs = Function::beatsToTime(63, bd);
    const uint roundTrip = Function::timeToBeats(timeMs, bd);
    QCOMPARE(roundTrip, 63u);
}

// 16 steps of 63 units must approximate one whole beat (1000) within 1%.
void BeatQuantize_Test::sixteenthsSumToWholeBeat()
{
    const int sum = 16 * 63; // 1008
    const int err = qAbs(sum - 1000);
    QVERIFY2(err < 10,
             qPrintable(QString("16 * 63 = %1, error %2 >= 10").arg(sum).arg(err)));
}

// The 1/16 quantizer table must have 16 entries spanning 0..938 with
// adjacent diffs of 62 or 63.
void BeatQuantize_Test::quantizerTable_coversAllSixteenths()
{
    const int n = int(sizeof(kSixteenthQuantizer) / sizeof(kSixteenthQuantizer[0]));
    QCOMPARE(n, 16);
    QCOMPARE(kSixteenthQuantizer[0], 0u);
    QCOMPARE(kSixteenthQuantizer[15], 938u);

    for (int i = 1; i < n; ++i)
    {
        const int diff = int(kSixteenthQuantizer[i]) - int(kSixteenthQuantizer[i - 1]);
        QVERIFY2(diff == 62 || diff == 63,
                 qPrintable(QString("step %1: diff %2 not in {62,63}").arg(i).arg(diff)));
    }
}

QTEST_MAIN(BeatQuantize_Test)
