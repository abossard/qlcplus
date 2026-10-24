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
    void skipUnchanged_deliversEveryChange_data();
    void skipUnchanged_deliversEveryChange();
    void fpsPacing_toleratesTickJitter_data();
    void fpsPacing_toleratesTickJitter();

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
    void componentChange_clearsAbandonedPixels_data();
    void componentChange_clearsAbandonedPixels();

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
    void emptyMember_doesNotStallDestination_data();
    void emptyMember_doesNotStallDestination();
    void largeInstall_tickFitsBudget_data();
    void largeInstall_tickFitsBudget();
};

#endif // DDP_PARTIAL_TEST_H
