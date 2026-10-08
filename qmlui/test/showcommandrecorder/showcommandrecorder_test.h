/*
  Q Light Controller Plus - Unit test
  showcommandrecorder_test.h

  Copyright (c) QLC+ contributors

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef SHOWCOMMANDRECORDER_TEST_H
#define SHOWCOMMANDRECORDER_TEST_H

#include <QObject>

class ShowCommandRecorder_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void nativeQml_presetClickAuthorsOnce_data();
    void nativeQml_presetClickAuthorsOnce();
    void nativeQml_algorithmSelectionAuthorsOnce_data();
    void nativeQml_algorithmSelectionAuthorsOnce();
    void nativePlanning_immutableValues_data();
    void nativePlanning_immutableValues();
    void xyPadRanges_nativeOutputAndGeometry_data();
    void xyPadRanges_nativeOutputAndGeometry();
    void nativeReplay_claimedBindingCancels_data();
    void nativeReplay_claimedBindingCancels();
    void externalInput_authorsOnlyThroughMidiPatch_data();
    void externalInput_authorsOnlyThroughMidiPatch();

    void onlyUserInputAuthors_data();
    void onlyUserInputAuthors();

    void sliderUserValue_survivesLaterAudioValue();

    void monitoringToggle_firstUserClickAcquires_data();
    void monitoringToggle_firstUserClickAcquires();

    void recordStop_finalizesExtentAtLastItem();

    void finalizeExtent_endsAtLastRetainedItem_data();
    void finalizeExtent_endsAtLastRetainedItem();
    void newTake_endsEditedExtentAtContent_data();
    void newTake_endsEditedExtentAtContent();

    void loopSegment_keepsDepartureEnd();
    void loopSegment_settlesPendingGestureBeforeEpoch();

    void invalidEdits_areRejectedNotClamped_data();
    void invalidEdits_areRejectedNotClamped();
    void failedFinalization_keepsRecording();
    void recordRebind_finalizesPreviousShowAtLastItem();

    void pendingSliderGesture_settlesIntoItsOwnShow();

    void sliderPosition_capturesAcceptedRangeAtInput();

    void relativeInput_keepsItsProvenance_data();
    void relativeInput_keepsItsProvenance();

    void rewoundTake_replaysItsOwnRecordedCommand();

    void recordedTarget_survivesWidgetRetarget();

    void armedRecorder_bindsFirstShowAndAuthorsWhileFrozen();

    void recordingExtent_outlivesLastCommand();

    void editor_retimeValueDeleteAndExtent();
    void timeline_groupsSelectAndRevealSamples();
    void timeline_clipSelectionTakesKeyboardFocus();
    void timeline_clipSelectionTakesKeyboardFocus_data();
    void timeline_localKeysDoNotReachVc_data();
    void timeline_localKeysDoNotReachVc();
    void timeline_ordinaryHoldKeepsSelectedIdentity_data();
    void timeline_ordinaryHoldKeepsSelectedIdentity();
    void timeline_unhandledKeysKeepVcRoute_data();
    void timeline_unhandledKeysKeepVcRoute();
    void timeline_keyboardCellAndReturn_data();
    void timeline_keyboardCellAndReturn();
    void timeline_numericAuthoredSelection_data();
    void timeline_numericAuthoredSelection();
    void timeline_numericKeyboard_data();
    void timeline_numericKeyboard();
    void timeline_vcOriginalRelease_data();
    void timeline_vcOriginalRelease();
    void timeline_localControlLifecycle_data();
    void timeline_localControlLifecycle();
    void timeline_timingFieldFocus();
    void timeline_localModalDoesNotReachVc();
    void timeline_clipboardAndDeleteKeepTheirDomain_data();
    void timeline_clipboardAndDeleteKeepTheirDomain();
    void timeline_contextEntryAndVisibleFocus();
    void timeline_zoomFromKeyboard_data();
    void timeline_zoomFromKeyboard();
    void timeline_zoomAfterFeedback_data();
    void timeline_zoomAfterFeedback();
    void selectionNavigation_plan_data();
    void selectionNavigation_plan();
    void timeline_fitSelection_data();
    void timeline_fitSelection();
    void timeline_fitClipSelection_data();
    void timeline_fitClipSelection();
    void timeline_fitSelectionRows_data();
    void timeline_fitSelectionRows();
    void timeline_fitSelectionResult_data();
    void timeline_fitSelectionResult();
    void timeline_fitSelectionAfterUserScroll_data();
    void timeline_fitSelectionAfterUserScroll();
    void timeline_viewCommandsAfterUserScroll_data();
    void timeline_viewCommandsAfterUserScroll();
    void timeline_viewPreservedAfterUserScroll_data();
    void timeline_viewPreservedAfterUserScroll();
    void timeline_revealInsideVisibleView_data();
    void timeline_revealInsideVisibleView();
    void timeline_viewExtent_data();
    void timeline_viewExtent();
    void timeline_divisionSwitchKeepsRuler_data();
    void timeline_divisionSwitchKeepsRuler();
    void timeline_dragTracksPointer_data();
    void timeline_dragTracksPointer();
    void timeline_selectionNavigationRange_data();
    void timeline_selectionNavigationRange();
    void timeline_goToSelectionStart_data();
    void timeline_goToSelectionStart();
    void timeline_selectionNavigationOwner_data();
    void timeline_selectionNavigationOwner();
    void timeline_selectionNavigationDisabled_data();
    void timeline_selectionNavigationDisabled();
    void timeline_selectionNavigationShowSwitch_data();
    void timeline_selectionNavigationShowSwitch();
    void timeline_goToSelectionStartExternal_data();
    void timeline_goToSelectionStartExternal();
    void timeline_selectionNavigationExternalBoundary_data();
    void timeline_selectionNavigationExternalBoundary();
    void timeline_selectionNavigationKeyboard_data();
    void timeline_selectionNavigationKeyboard();
    void timeline_selectionNavigationReachable_data();
    void timeline_selectionNavigationReachable();
    void timeline_keyboardOffscreenFocus_data();
    void timeline_keyboardOffscreenFocus();
    void timeline_trackControlsFromKeyboard_data();
    void timeline_trackControlsFromKeyboard();
    void timeline_trackExitToMovement_data();
    void timeline_trackExitToMovement();
    void timeline_recordingButtonsFromKeyboard_data();
    void timeline_recordingButtonsFromKeyboard();
    void timeline_feedbackRemainsReachable_data();
    void timeline_feedbackRemainsReachable();
    void timeline_movesExactSelectionAfterRegroup_data();
    void timeline_movesExactSelectionAfterRegroup();
    void timeline_dragCommitsOnceOrCancels_data();
    void timeline_dragCommitsOnceOrCancels();

    void editor_typedActionAndTargetEdits();

    void replayedButtonOn_startsItsSceneThroughShowPlayback();
    void nativeFunctionReceipt_settlesQueuedOperation_data();
    void nativeFunctionReceipt_settlesQueuedOperation();
    void replayedCollectionOn_thenChildOff_stopsTheChild_data();
    void replayedCollectionOn_thenChildOff_stopsTheChild();
    void replay_survivesSelectionChange();
    void guiCancel_dropsWorkQueuedBeforeIt_data();
    void guiCancel_dropsWorkQueuedBeforeIt();
    void cancel_dropsParkedRunWithoutFurtherBatch_data();
    void cancel_dropsParkedRunWithoutFurtherBatch();
    void replayedButtonOn_atContentEnd_runsBeforeShowEnds();
    void replayedPlayingPair_waitsForItsCoupledPredecessor_data();
    void replayedPlayingPair_waitsForItsCoupledPredecessor();
    void replayedSliderPosition_thenLegacyIntensity_endsAtLegacyValue();
    void replayedSliderPosition_supersededByNewerInput_doesNotStall();
    void replayedSliderPosition_cancelledByReconfiguration_data();
    void replayedSliderPosition_cancelledByReconfiguration();
    void replayedSliderPosition_functionDeleted_showStillEnds();
    void cancelledSliderReplay_neverWritesLate();
    void cancelledSliderReplay_laterSameValueStillWrites_data();
    void cancelledSliderReplay_laterSameValueStillWrites();
    void cancelledLevelReplay_restoresPriorControlState_data();
    void cancelledLevelReplay_restoresPriorControlState();
    void replayedSliderStart_isLiveBeforeLegacyIntensity();
    void animationFader_waitsForNativeStart_data();
    void animationFader_waitsForNativeStart();
    void animationFader_sameValueLiveSurvivesStop_data();
    void animationFader_sameValueLiveSurvivesStop();
    void replayedSliderStart_racingOwnWrite_isLiveBeforeLegacyIntensity_data();
    void replayedSliderStart_racingOwnWrite_isLiveBeforeLegacyIntensity();
    void replayedMatchingSliderPosition_takesNoAction_data();
    void replayedMatchingSliderPosition_takesNoAction();
    void replayedChildOff_afterLegacyStart_stopsTheChild_data();
    void replayedChildOff_afterLegacyStart_stopsTheChild();
    void replayedSoloSiblingOn_afterCollectionStart_winsLikeNative_data();
    void replayedSoloSiblingOn_afterCollectionStart_winsLikeNative();
    void replayedButtonOn_afterLegacyStop_endsActive_data();
    void replayedButtonOn_afterLegacyStop_endsActive();
    void replayedSubmaster_thenLegacyIntensity_endsAtLegacyValue();
    void replayedSubmaster_retiresPendingChildWriteBeforeLegacy();
    void replayedSubmaster_childStartIsLiveBeforeLegacy();
    void replayedSliderZero_thenOnSameFunction_endsActive_data();
    void replayedSliderZero_thenOnSameFunction_endsActive();
    void replayedSlider_reconfiguredInWriteWindow_neverRetargets_data();
    void replayedSlider_reconfiguredInWriteWindow_neverRetargets();
    void replayedLevelSlider_channelsReplacedInWriteWindow_neverRetargets_data();
    void replayedLevelSlider_channelsReplacedInWriteWindow_neverRetargets();
    void replayedGrandMasterPosition_completesOnReturn();
    void levelSliderValue_outputOnlyWhenMeantForDmx_data();
    void levelSliderValue_outputOnlyWhenMeantForDmx();
    void adjustFaderFeedback_isShownNotWrittenBack_data();
    void adjustFaderFeedback_isShownNotWrittenBack();
    void adjustFaderStopFeedback_leavesTheAttributeReset_data();
    void adjustFaderStopFeedback_leavesTheAttributeReset();
    void adjustFaderFeedback_isWhatACancelRestores();
    void adjustFaderOnFunctionStart_stillWritesItsValue_data();
    void adjustFaderOnFunctionStart_stillWritesItsValue();
    void adjustFaderOutputRequest_stillWrites_data();
    void adjustFaderOutputRequest_stillWrites();

    void seek_forwardPlaysRecordedButtonOn();
    void seek_forwardPlaysMixedHistoryInAuthoredOrder_data();
    void seek_forwardPlaysMixedHistoryInAuthoredOrder();
    void play_fromStoppedCursor_runsOnlyFromTheCursor();
    void playThrough_reachesLiteralNativeState_data();
    void playThrough_reachesLiteralNativeState();

    // C1: a backward move returns what its window's timeline operations changed
    void timelineRollback_data();
    void timelineRollback();
    void timelineRollback_sliderZeroRollbackReappliesAcrossWraps();
    void timelineRollback_sliderZeroReleaseKeepsLiveButtonOwner();
    void vdjAdoption_firstFreshSampleBeginsTheTraversal_data();
    void vdjAdoption_firstFreshSampleBeginsTheTraversal();

    // C1: a forward jump plays its crossed interval serially through native
    // dispatch; Play from a cursor and backward moves replay no history
    void jump_soloIntervalRunsSerially();
    void seek_backwardRollsBackOnlyItsWindow();
    void jump_mixedTieAtDestinationRunsOnceInOrder_data();
    void jump_mixedTieAtDestinationRunsOnceInOrder();
    void jump_soloSliderHistoryEndsLikeNative();
    void jump_intervalLag_data();
    void jump_intervalLag();
    void play_stoppedCaptureAtCursorReplays();
    void play_liveInputAfterColdPlayDoesNotEcho();
    void play_liveInputAfterRunningRestartDoesNotEcho_data();
    void play_liveInputAfterRunningRestartDoesNotEcho();
    void seek_liveInputAfterRequestedSeekDoesNotEcho_data();
    void seek_liveInputAfterRequestedSeekDoesNotEcho();
    void retiredRunner_endsAtItsClaimedOperation_data();
    void retiredRunner_endsAtItsClaimedOperation();
    void retiredRunner_guiRestartDuringClaimedOperation_realTimer();
    void retiredRunner_legacyJumpEndsAtItsClaimedValue();
    void play_armedAcrossPlayReplaysCursorCaptures();

    // nonblocking Pause/Resume
    void pause_keepsVcStartedFunctions_resumeRunsNoHistory();
    void resume_afterPausedMove_jumpsWithoutStopping_data();
    void resume_afterPausedMove_jumpsWithoutStopping();
    void resume_externalShowAfterPausedMove_keepsItsTraversal();
    void pausing_showsOwedCrossedWorkUntilDrained_data();
    void pausing_showsOwedCrossedWorkUntilDrained();
    void ignoredExternalSeek_keepsCrossedWork_data();
    void ignoredExternalSeek_keepsCrossedWork();
    void claimedOperation_sourceSwitchRunsNoStaleWork_data();
    void claimedOperation_sourceSwitchRunsNoStaleWork();

    // accepted user requests wait only behind work they depend on
    void userRequest_waitsBehindCrossedReplayOnItsControl();
    void userRequest_independentSliderRespondsAtOnce_data();
    void userRequest_independentSliderRespondsAtOnce();
    void userClick_normalizedOnceToNativeDesiredState_data();
    void userClick_normalizedOnceToNativeDesiredState();
    void userRequest_survivesReplayCancelNotWorkspaceReset_data();
    void userRequest_survivesReplayCancelNotWorkspaceReset();
    void userRequest_keepsItsAcceptedDestination_data();
    void userRequest_keepsItsAcceptedDestination();
    void levelChannelSnapshot_sharesNoTimerStorage();
    void recArmedUserInput_waitsBehindCoupledReplay_data();
    void recArmedUserInput_waitsBehindCoupledReplay();

    // accepted input capture
    void acceptedSliderInput_recordsEachChangedPosition_data();
    void acceptedSliderInput_recordsEachChangedPosition();
    void movedHostCursor_stampsAcceptedInput_data();
    void movedHostCursor_stampsAcceptedInput();
    void acceptedButtonInput_recordsDesiredState_data();
    void acceptedButtonInput_recordsDesiredState();
    void acceptedSliderModes_recordPrimaryValue_data();
    void acceptedSliderModes_recordPrimaryValue();
    void clickAndGoPreset_userPathRecordsSliderValue();
    void clickAndGoPreset_replayAfterRangeChange_appliesAcceptedValue_data();
    void clickAndGoPreset_replayAfterRangeChange_appliesAcceptedValue();
    void clickAndGoColors_userPathRecordsTypedPayload();
    void clickAndGoColors_replayAppliesDistinctColorsWithSameBrightness_data();
    void clickAndGoColors_replayAppliesDistinctColorsWithSameBrightness();
    void clickAndGoColors_replayInvalidPayloadHasNoPartialEffect();
    void sliderReset_replayReleasesOverrideToNonzeroMonitor();
    void sliderReset_savedReplayReleasesNativeFader_data();
    void sliderReset_savedReplayReleasesNativeFader();
    void animationFader_userPathRecordsTypedPayload();
    void animationFader_replayZeroThenRestartAcrossWraps();
    void xyPadPosition_userPathRecordsTypedPayload();
    void xyPadFloor_userPathRecordsTypedPayload_data();
    void xyPadFloor_userPathRecordsTypedPayload();
    void xyPadFloor_savedReplayMatchesNativeOutput_data();
    void xyPadFloor_savedReplayMatchesNativeOutput();
    void xyPadFunctionChoice_releaseSettlesItsSource_data();
    void xyPadFunctionChoice_releaseSettlesItsSource();
    void xyPadPositionChoice_frozenTransition_data();
    void xyPadPositionChoice_frozenTransition();
    void xyPadFunctionChoice_backwardPreservesActualOwner_data();
    void xyPadFunctionChoice_backwardPreservesActualOwner();
    void xyPadChoice_pendingLifetimeCancels_data();
    void xyPadChoice_pendingLifetimeCancels();
    void nativeChoice_newOccurrenceRefusesIncompatibleBinding_data();
    void nativeChoice_newOccurrenceRefusesIncompatibleBinding();
    void xyPadGroup_savedReplayUsesCurrentHeads_data();
    void xyPadGroup_savedReplayUsesCurrentHeads();
    void xyPadGroup_multiheadSelectionRestores_data();
    void xyPadGroup_multiheadSelectionRestores();
    void xyPadWriter_bindingRetiresExactlyOnce_data();
    void xyPadWriter_bindingRetiresExactlyOnce();
    void xyPadWriter_cancelledSameValueWrites_data();
    void xyPadWriter_cancelledSameValueWrites();
    void xyPadWriter_floorModeChangeCancels_data();
    void xyPadWriter_floorModeChangeCancels();
    void xyPadWriter_floorProjectionChangeCancels_data();
    void xyPadWriter_floorProjectionChangeCancels();
    void replayedXYPosition_configurationChangeReportsCancellation_data();
    void replayedXYPosition_configurationChangeReportsCancellation();
    void animationContent_acceptedTextReachesNativePlayback_data();
    void animationContent_acceptedTextReachesNativePlayback();
    void animationContent_backwardRestoresActualContent_data();
    void animationContent_backwardRestoresActualContent();
    void animationContent_multistepBackwardClosure_data();
    void animationContent_multistepBackwardClosure();
    void animationContent_callbackFailure_data();
    void animationContent_callbackFailure();
    void animationColor_backwardDoesNotRedirectAfterRebind_data();
    void animationColor_backwardDoesNotRedirectAfterRebind();
    void animationContent_nativePropertiesStayFrozen_data();
    void animationContent_nativePropertiesStayFrozen();
    void animationColor_indexedNativeOutput_data();
    void animationColor_indexedNativeOutput();
    void selectedNative_userIngress_data();
    void selectedNative_userIngress();
    void typedDraft_transaction_data();
    void typedDraft_transaction();
    void typedDraft_nativeMetadataRefusesWholeCandidate_data();
    void typedDraft_nativeMetadataRefusesWholeCandidate();
    void xyPadPosition_replayAppliesRecordedPositionAcrossWraps();
    void xyPadPosition_replayMatchesDirectNativeOutputFractional_data();
    void xyPadPosition_replayMatchesDirectNativeOutputFractional();
    void xyPadPosition_rollbackIgnoresLaterRangeEdit();
    void replayedMatchingXYPosition_takesNoAction();
    void replayedXYPosition_waitsForRecordedWriteBeforeLegacy();
    void replayedXYPosition_nativeWritePrecedesRetirement();
    void replayedXYPosition_floorModeStillRetiresWriteBeforeLegacy();
    void replayedXYPosition_floorModeWritableRetiresAppliedBeforeLegacy();
    void replayedXYPosition_supersededPendingWrite_stillReleasesLegacy();
    void replayedXYPosition_stopRestartWhilePending_keepsTraversalIsolation();
    void replayedXYPosition_rollbackWaitBlocksLaterStartUntilRetire();
    void replayedXYPosition_deleteAfterDetachedRetirement_releasesWait();
    void replayedXYPosition_deleteWhileTimerThreadWriting_releasesWaitExactOnce();
    void animationFader_nonMatrixBinding_isUnboundAndSkipped();
    void unsupportedInput_reportsAndStaysLive_data();
    void unsupportedInput_reportsAndStaysLive();
    void flashMixedIngressOverlap_recordsExactTimesAndOrder();
    void sliderFlashReplay_restoresRuntimePreFlashValue();
    void holdCapture_assignsPairAndPersistsXml_data();
    void holdCapture_assignsPairAndPersistsXml();
    void seekInsideHold_appliesOnlyCurrentHoldState_data();
    void seekInsideHold_appliesOnlyCurrentHoldState();
    void freshStartInsideHold_sliderFlashRestoresRuntimeBaselineAcrossBackwardLoop();
    void replayedGlobalButtons_applyDesiredStatesAndFreezeHoldSharedRelease();
    void recOff_closesFreezeHoldWithoutCapturingLaterRelease();
    void pause_replayFlashHoldStaysActiveUntilResumeRelease_data();
    void pause_replayFlashHoldStaysActiveUntilResumeRelease();
    void replayStop_releasesOnlyReplayOwnedFlash_data();
    void replayStop_releasesOnlyReplayOwnedFlash();
    void replayStop_releasesOnlyReplayOwnedSliderFlash_data();
    void replayStop_releasesOnlyReplayOwnedSliderFlash();
    void checkpoint_midHoldSavesTemporaryClosureAndKeepsLiveOpen_data();
    void checkpoint_midHoldSavesTemporaryClosureAndKeepsLiveOpen();
    void replayStop_releasesOnlyReplayOwnedFreezeHold_data();
    void replayStop_releasesOnlyReplayOwnedFreezeHold();
    void replayStop_doesNotRollbackValues_butBackwardSeekDoes();
    void replayStop_releasesShowOwnedButtonStart_keepsManualOwner();
    void replayStop_preservesUnownedGlobalLatches_data();
    void replayStop_preservesUnownedGlobalLatches();
    void liveFreezeSameValueIntent_survivesReplayStop();
    void editingActiveReplayFreezeHold_releasesImmediately();
    void editingActiveReplayHoldEdits_releaseImmediately_data();
    void editingActiveReplayHoldEdits_releaseImmediately();
    void editingPlayedGlobalLatchEvents_areDataOnlyUntilBoundary_data();
    void editingPlayedGlobalLatchEvents_areDataOnlyUntilBoundary();
    void holdPairSelection_requiresWholePair_data();
    void holdPairSelection_requiresWholePair();
    void holdPairSelection_refusesMalformedPair();
    void holdPairPaste_assignsFreshPairIdsAndUndoRedoKeepsThem();
    void recordingsEditor_holdPairPasteUndoRedoKeepsFreshPairIds();
    void patchedInput_recordsOnlyUserRoutes_data();
    void patchedInput_recordsOnlyUserRoutes();
    void patchedXY_coarseFineInterleavesPointer_data();
    void patchedXY_coarseFineInterleavesPointer();
    void nonUserChanges_recordNothing_data();
    void nonUserChanges_recordNothing();
    void checkpoint_savesAcceptedContentEnd_data();
    void checkpoint_savesAcceptedContentEnd();
    void takeBoundary_savesContentEndWithoutNewInput_data();
    void takeBoundary_savesContentEndWithoutNewInput();
    void takeBoundary_marksPersistedChangeModified_data();
    void takeBoundary_marksPersistedChangeModified();
    void automaticShowChange_handsRecordingOver_data();
    void automaticShowChange_handsRecordingOver();
    void manualShowSelection_lockedWhileArmed_data();
    void manualShowSelection_lockedWhileArmed();
    void performFollow_handsOverOnlyWhileEngaged_data();
    void performFollow_handsOverOnlyWhileEngaged();
    void appPerform_handsRecordingOver_data();
    void appPerform_handsRecordingOver();
    void appSave_checkpointsRecording_data();
    void appSave_checkpointsRecording();
    void checkpoint_midHoldFailedSaveRetry_keepsOpenState_data();
    void checkpoint_midHoldFailedSaveRetry_keepsOpenState();
    void appReplacement_confirmsRecording_data();
    void appReplacement_confirmsRecording();
    void appRecordControls_shareOneRecorder_data();
    void appRecordControls_shareOneRecorder();
    void appRecordControls_followTargetRename_data();
    void appRecordControls_followTargetRename();
    void appPerformGap_firstSelectionBinds_data();
    void appPerformGap_firstSelectionBinds();
    void appMcpReplacement_needsDiscardWhileRecording_data();
    void appMcpReplacement_needsDiscardWhileRecording();
    // Recordings editor in the Show editor
    void recordingsTab_listsAndEditsTheTake();
    void recordingsEdit_previewCommitCancel_data();
    void recordingsEdit_previewCommitCancel();
    void recordingsTable_draftEndsWithReason_data();
    void recordingsTable_draftEndsWithReason();
    void recordingsTable_draftEndsWhenTargetIsDeleted_data();
    void recordingsTable_draftEndsWhenTargetIsDeleted();
    void recordingsEdit_liveStretchSavesAndReloads();
    void recordingsEdit_conflicts_data();
    void recordingsEdit_conflicts();
    void recordingsTable_draftSurvivesCapture_data();
    void recordingsTable_draftSurvivesCapture();
    void recordingsLane_dragSurvivesCapture_data();
    void recordingsLane_dragSurvivesCapture();
    void recordingsLane_spanSurface_data();
    void recordingsLane_spanSurface();
    void showManager_followPausedDuringRecordingGesture();
    void recordingsEdit_workspaceReplacedEndsSession_data();
    void recordingsEdit_workspaceReplacedEndsSession();
    void recordingsEdit_selectionChangeEndsSession_data();
    void recordingsEdit_selectionChangeEndsSession();
    void recordingsLane_refusedReleaseCommitsNothing_data();
    void recordingsLane_refusedReleaseCommitsNothing();
    void recordingsTable_keysContinueFromDraftRow_data();
    void recordingsTable_keysContinueFromDraftRow();
    void recordingsLane_pressFreezesBeforeThreshold_data();
    void recordingsLane_pressFreezesBeforeThreshold();
    void recordingsPerform_editsWhileVdjDrives_data();
    void recordingsPerform_editsWhileVdjDrives();
    void recordingsEditor_editsWhileLive_data();
    void recordingsEditor_editsWhileLive();
    void recordingsEditor_rejectsInvalidEditsWhole_data();
    void recordingsEditor_rejectsInvalidEditsWhole();
    void recordingsEditor_deleteUndoRedo_keepsLaterCapture();
    void recordingsEditor_movesGroupByMusicalSteps_data();
    void recordingsEditor_movesGroupByMusicalSteps();
    void recordingsEditor_snapsGroupToGrid_data();
    void recordingsEditor_snapsGroupToGrid();
    void recordingsEditor_groupEditsRejectWhole();
    void recordingsEditor_staleEditorNeverEditsAnotherShow();
    void recordingsEditor_undoStepsKeepNativeGrouping();
    void recordingsEditor_restoredShowNeverReusesHistoryIds();
    void recordingsEditor_awayAndBackKeepsTieOrder_data();
    void recordingsEditor_awayAndBackKeepsTieOrder();
    void recordingsEditor_visibleRowsFollowTheirControls();
    void recordingsEditor_restoredControlsReturnToTheirRows();
    void recordingsEditor_handlesTheWholeEventIdRange_data();
    void recordingsEditor_handlesTheWholeEventIdRange();
    void recordingsEditor_copyTakesTheExactSelection_data();
    void recordingsEditor_copyTakesTheExactSelection();
    void recordingsEditor_pasteLandsAtThePlayhead();
    void recordingsEditor_refusedPasteKeepsEverything_data();
    void recordingsEditor_refusedPasteKeepsEverything();
    void recordingsEditor_pasteUndoRedoKeepsLaterCapture();
    void recordingsEditor_textFieldsKeepTheirCopyPaste_data();
    void recordingsEditor_textFieldsKeepTheirCopyPaste();
    void recordingsEditor_toolbarAndShortcutsFollowTheDomain_data();
    void recordingsEditor_toolbarAndShortcutsFollowTheDomain();
    void recordingsEditor_pasteButtonKeepsTheFocusedListDomain();
    void recordingsEditor_clipboardLivesForTheWorkspace_data();
    void recordingsEditor_clipboardLivesForTheWorkspace();
    void recordingsEditor_pasteKeepsUnreadyReferences();
    void recordingsEditor_pasteIntoAnotherShow();
    void recordingsEditor_pasteRefusesADeletedCopiedTarget_data();
    void recordingsEditor_pasteRefusesADeletedCopiedTarget();
    void recordingsEditor_pasteWaitsForTheCellDraft_data();
    void recordingsEditor_pasteWaitsForTheCellDraft();
    void recordingsEditor_pasteValidatesWhole_data();
    void recordingsEditor_pasteValidatesWhole();
    void recordingsEditor_pasteWhileLive_data();
    void recordingsEditor_pasteWhileLive();
    void recordingsEditor_pasteIdsStayAboveEveryIssuedId_data();
    void recordingsEditor_pasteIdsStayAboveEveryIssuedId();
    void recordingsEditor_historyKeepsNewestStepsAtCapacity_data();
    void recordingsEditor_historyKeepsNewestStepsAtCapacity();
    void recordingsEditor_historyEvictsWholeNativeSteps();
    void recordingsEditor_nativeUndoKeepsItsNewestWindow_data();
    void recordingsEditor_nativeUndoKeepsItsNewestWindow();
    void recordingsEditor_workspaceReuseDropsOldEditsAndCallbacks();

    // shared debug panel
    void debugPanel_capturesOnlyWhileOpen();
    void debugPanel_boundsHistoryAndWakes();
    void debugPanel_tracesFlowDecisionsAndOutcomes();
    void debugPanel_leavesRecordingAndReplayUnchanged();
    void debugPanel_deferredOutcomeStaysInItsObservation_data();
    void debugPanel_deferredOutcomeStaysInItsObservation();
    void debugPanel_referencedControlsUseReplayResolution();
    void debugPanel_appLaunchersShareOnePanel_data();
    void debugPanel_appLaunchersShareOnePanel();
    void debugPanel_appReadingPositionAndProblems();
    void debugPanel_appResetStartsFromTheNewWorkspace();
    // diagnostics correction D1-D8
    void debugPanel_deferredOutcomeKeepsAcceptedIdentity();
    void recordingsEditor_editKeepsPendingCapture_data();
    void recordingsEditor_editKeepsPendingCapture();
    void debugPanel_closedPendingOutcomeIsNotBackfilled_data();
    void debugPanel_closedPendingOutcomeIsNotBackfilled();
    void debugPanel_checkpointIsItsOwnObservedOperation();
    void debugPanel_committedChangesReportCommandAndExtentDeltas();
    void debugPanel_panelHiddenOrUnloadedEndsObservation_data();
    void debugPanel_panelHiddenOrUnloadedEndsObservation();
    void debugPanel_soleDroppedDiagnosticWakes();
    void debugPanel_rejectedKeyboardInputIsObserved_data();
    void debugPanel_rejectedKeyboardInputIsObserved();
    void debugPanel_directProgrammaticRequestsAreObservedOnce_data();
    void debugPanel_directProgrammaticRequestsAreObservedOnce();
    void debugPanel_debugRefocusesTheOpenPanel();
    // recheck R1-R4
    void debugPanel_closedCheckpointBuildsNoDiagnostic_data();
    void debugPanel_closedCheckpointBuildsNoDiagnostic();
    void debugPanel_firstRecordedIdentityIsConsistent();
    void debugPanel_commandDeltaKeepsChangedFacts_data();
    void debugPanel_commandDeltaKeepsChangedFacts();
    void debugPanel_directRequestClaimsNoExecution_data();
    void debugPanel_directRequestClaimsNoExecution();
    void debugPanel_transportUsesTheAcceptanceClock_data();
    void debugPanel_transportUsesTheAcceptanceClock();
    // native accessibility of the recording UI
    void accessibleVcControls_actAsUserInput_data();
    void accessibleVcControls_actAsUserInput();
    void accessiblePlayhead_seeksLikeTheRuler_data();
    void accessiblePlayhead_seeksLikeTheRuler();
    void accessibleRecordings_rowsTabsAndGroupActions();
    void accessibleVirtualConsole_pagesAndButtons();
    void accessibleFunctionList_activatesLikeDoubleClick_data();
    void accessibleFunctionList_activatesLikeDoubleClick();

    // repairing a recorded passage
    void recordingsPassage_readoutAndActionTargets();
    void recordingsPassage_keepsPassageWhileEditingOneRow();
    void recordingsPassage_filterPruningAndEmptyState();
    void recordingsPassage_showSwitchClearsPassage();
    void recordingsSplit_timelineAboveSameTable();
    void recordingsSplit_keyboardReachesBothPanes_data();
    void recordingsSplit_keyboardReachesBothPanes();
    void recordingsSplit_readOnlyTraversalLeavesTheSegment_data();
    void recordingsSplit_readOnlyTraversalLeavesTheSegment();
    void timeline_splitTabLeavesOutsideTheSplit_data();
    void timeline_splitTabLeavesOutsideTheSplit();
    void timeline_segmentEndsAreDistinct_data();
    void timeline_segmentEndsAreDistinct();
    void recordingsSplit_exitAfterTheSegment_data();
    void recordingsSplit_exitAfterTheSegment();
    void recordingsPassage_presentationKeepsAuthoredStateAndHistory();
    void recordingsSplit_keepsPassageSelectionAndDraft();
    void recordingsPassage_summaryReadableWhenItGrows_data();
    void recordingsPassage_summaryReadableWhenItGrows();
    void recordingsSplit_focusOutlineStaysInTimeline_data();
    void recordingsSplit_focusOutlineStaysInTimeline();
    void recordingsPassage_showAllInTabChain_data();
    void recordingsPassage_showAllInTabChain();
    void recordingsTab_undoBacktabWithoutPassage_data();
    void recordingsTab_undoBacktabWithoutPassage();
    void recordingsSplit_viewChangeKeepsTypedDraft();
    void recordingsEditor_nativeFields_data();
    void recordingsEditor_nativeFields();
    void recordingsEditor_floorNativeBounds_data();
    void recordingsEditor_floorNativeBounds();
    void recordingsEditor_colorCanvasReopens();
    void recordingsEditor_boundContent_data();
    void recordingsEditor_boundContent();
    void recordingsEditor_boundFields_data();
    void recordingsEditor_boundFields();
    void typedDraft_floorAtomicCandidate_data();
    void typedDraft_floorAtomicCandidate();
    void typedDraft_boundCompound_data();
    void typedDraft_boundCompound();
    void typedDraft_boundCompatibility_data();
    void typedDraft_boundCompatibility();
    void recordingsEditor_contentFields_data();
    void recordingsEditor_contentFields();
    void recordingsEditor_typedHistoryKeepsPayload_data();
    void recordingsEditor_typedHistoryKeepsPayload();
    void playedNativeChoice_editsRemainDataOnly_data();
    void playedNativeChoice_editsRemainDataOnly();

    // per-control recording lanes
    void recordingLanes_rowsFollowGlobalGroups_data();
    void recordingLanes_rowsFollowGlobalGroups();
    void recordingLanes_expandedRowsShowEveryGroup();
    void recordingLanes_rowsFollowControlChanges();
    void recordingLanes_toggleKeepsAuthoredState_data();
    void recordingLanes_toggleKeepsAuthoredState();
    void recordingLanes_toggleRevealsFocusedItem_data();
    void recordingLanes_toggleRevealsFocusedItem();
    void recordingLanes_tabRevealsHeaderControls_data();
    void recordingLanes_tabRevealsHeaderControls();
    void recordingLanes_titleClearsToggle_data();
    void recordingLanes_titleClearsToggle();
    void recordingLanes_tabReachesToggleAndEveryGroup_data();
    void recordingLanes_tabReachesToggleAndEveryGroup();
    void recordingLanes_tabFollowsRemovedGroup_data();
    void recordingLanes_tabFollowsRemovedGroup();
    void recordingLanes_keyHoldRegroupsInItsRow_data();
    void recordingLanes_keyHoldRegroupsInItsRow();

    // recorded slider positions drawn on the timeline
    void sliderGraph_stepGeometry_data();
    void sliderGraph_stepGeometry();
    void sliderGraph_sampleSummary_data();
    void sliderGraph_sampleSummary();
    void timeline_sliderSamplesSnapshot_data();
    void timeline_sliderSamplesSnapshot();
    void timeline_sliderGraphDrawsPositions();
    void timeline_sliderGraphFollowsRuler_data();
    void timeline_sliderGraphFollowsRuler();
    void timeline_sliderGraphSimplifiesDenseColumns();
    void timeline_sliderGraphVisibleWindow();
    void timeline_sliderGraphMemoryBound_data();
    void timeline_sliderGraphMemoryBound();
    void timeline_sliderGraphFollowsEdits();
    void timeline_sliderGraphDimsUnreadyControls();
    void timeline_sliderGraphHoverReadout();
    void timeline_sliderGraphSelectedSamples();
    void timeline_sliderGraphPreservesData();
    void timeline_sliderGraphKeepsInput();
    void timeline_sliderGraphClipsInSplit();
    void timeline_sliderGraphPerformance();
    void timeline_sliderGraphAllocation_data();
    void timeline_sliderGraphAllocation();
};

#endif // SHOWCOMMANDRECORDER_TEST_H
