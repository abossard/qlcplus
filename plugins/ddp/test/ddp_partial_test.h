/*
  Q Light Controller Plus
  ddp_partial_test.h
*/
#ifndef DDP_PARTIAL_TEST_H
#define DDP_PARTIAL_TEST_H

#include <QObject>

class DDP_Partial_Test final : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Wire-level basics
    void firstFrame_emitsFullSnapshot();
    void identicalSecondFrame_emitsNothing();
    void singlePixelDiff_emitsOnePacket();
    void smallGap_coalescesIntoOnePacket();
    void largeGap_emitsTwoPacketsPushOnLast();
    void chunkBoundaries_data();
    void chunkBoundaries();

    // Mode transitions / invalidation
    void keepAlive_emitsFullAfterInterval();
    void rgbToRgbw_invalidatesBaseline();
    void setDestAddress_invalidatesBaseline();
    void setPixelCount_invalidatesAllBaselines();
    void transmissionModeFlip_invalidates();

    // Safety guards
    void misalignedDdpOffset_emitsNothing();
    void reservedDestId_emitsNothing();
    void autoModeOddLength_dropsTrailingByte();
    void pixelCountLargerThanSrc_zeroPadsTail();

    // Throttling
    void fpsThrottle_bypassedForKeepAlive();
    void skipUnchanged_doesNotSuppressPartialKeepAlive();

    // Wire-cost upgrade
    void heavyDiff_upgradesToFull();

    // Receiver simulation (canonical WLED semantics)
    void wledReceiverSim_partialMatchesSource();

    // Sequence
    void sequence_cyclesOneToFifteen_noZero();
    void sameEndpointDifferentDestId_sharesSequence();
    void multiUniverse_over15Chunks_oneSequence();

    // Coverage-shift
    void offsetShiftClearsOldRange();
    void multiPacketCoverageClear();
    void destIdShift_clearsWithOldDestId();

    // Throttle
    void fullMode_fpsThrottleEnforced();
    void partialMode_fpsThrottleBetweenDiffs();

    // RGBW
    void rgbwMisalignedOffset_rejected();

    // Multi-universe refresh grouping
    void multiUniverse_cleanCompleterPushOnly_data();
    void multiUniverse_cleanCompleterPushOnly();
    void stalledMember_fallsBackWithinTwoTicks();
    void droppedUpdate_repairedByKeepAlive_data();
    void droppedUpdate_repairedByKeepAlive();
    void destIdChange_dropsPendingBeforeCycleClose();
};

#endif // DDP_PARTIAL_TEST_H
