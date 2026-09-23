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

    void monitoringToggle_firstUserClickStops_data();
    void monitoringToggle_firstUserClickStops();

    void recordStop_finalizesExtentAtCaptureEnd();

    void finalizeExtent_neverShrinksAuthoredExtent_data();
    void finalizeExtent_neverShrinksAuthoredExtent();
    void newTake_preservesCurrentlyEditedExtent_data();
    void newTake_preservesCurrentlyEditedExtent();

    void loopSegment_keepsDepartureEnd();
    void loopSegment_settlesPendingGestureBeforeEpoch();
    void lateInputTimestamp_isNotAClockRewind();

    void resolvedGesture_survivesDisarmBeforeDelivery();

    void invalidEdits_areRejectedNotClamped_data();
    void invalidEdits_areRejectedNotClamped();
    void failedFinalization_keepsRecording();
    void recordRebind_finalizesPreviousShowExtent();

    void pendingSliderGesture_settlesIntoItsOwnShow();

    void sliderCoalescing_matchesNativeExecution_data();
    void sliderCoalescing_matchesNativeExecution();

    void sliderIntensity_capturesEffectiveValueAtInput();

    void relativeInput_keepsItsProvenance_data();
    void relativeInput_keepsItsProvenance();

    void rewoundTake_replaysItsOwnRecordedCommand();

    void recordedTarget_survivesWidgetRetarget();

    void armedRecorder_bindsFirstShowAndAuthorsWhileFrozen();

    void recordingExtent_outlivesLastCommand();

    void editor_retimeValueDeleteAndExtent();

    void commandEditorQml_rendersTheModel();
    void commandEditorQml_editsTimeAndIntensity();
    void editor_typedActionAndTargetEdits();
};

#endif // SHOWCOMMANDRECORDER_TEST_H
