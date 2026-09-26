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
    void timeline_keyboardOffscreenFocus_data();
    void timeline_keyboardOffscreenFocus();
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

    void seek_restoresRecordedButtonOn();
    void seek_restoresMixedHistoryInAuthoredOrder_data();
    void seek_restoresMixedHistoryInAuthoredOrder();
    void play_fromStoppedCursor_restoresOnce();
    void playThrough_reachesLiteralNativeState_data();
    void playThrough_reachesLiteralNativeState();

    // serial catch-up: Play at a new position runs the prefix through native dispatch
    void catchUp_soloPrefixRunsSerially();
    void catchUp_backwardSeekRunsPrefixOnce();
    void catchUp_mixedTieAtDestinationRunsOnceInOrder_data();
    void catchUp_mixedTieAtDestinationRunsOnceInOrder();
    void catchUp_soloSliderHistoryEndsLikeNative();
    void catchUp_prefixLag_data();
    void catchUp_prefixLag();
    void catchUp_stoppedCaptureReplaysOnPlay();
    void catchUp_liveInputAfterColdPlayDoesNotEcho();
    void catchUp_liveInputAfterRunningRestartDoesNotEcho_data();
    void catchUp_liveInputAfterRunningRestartDoesNotEcho();
    void catchUp_liveInputAfterRequestedSeekDoesNotEcho_data();
    void catchUp_liveInputAfterRequestedSeekDoesNotEcho();
    void retiredRunner_endsAtItsClaimedOperation_data();
    void retiredRunner_endsAtItsClaimedOperation();
    void retiredRunner_guiRestartDuringClaimedOperation_realTimer();
    void retiredRunner_legacySeekEndsAtItsClaimedValue();
    void catchUp_armedAcrossPlayReplaysOnlyHistory();

    // nonblocking Pause/Resume
    void pause_keepsVcStartedFunctions_resumeRunsNoPrefix();
    void resume_afterPausedMove_catchesUpWithoutStopping_data();
    void resume_afterPausedMove_catchesUpWithoutStopping();
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
    void unsupportedInput_reportsAndStaysLive_data();
    void unsupportedInput_reportsAndStaysLive();
    void patchedInput_recordsOnlyUserRoutes_data();
    void patchedInput_recordsOnlyUserRoutes();
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
    void recordingsEditor_needsStoppedPlaybackAndRecOff_data();
    void recordingsEditor_needsStoppedPlaybackAndRecOff();
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
};

#endif // SHOWCOMMANDRECORDER_TEST_H
