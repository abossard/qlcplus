/*
  Q Light Controller Plus - Unit test
  showcommandtrack_test.h

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

#ifndef SHOWCOMMANDTRACK_TEST_H
#define SHOWCOMMANDTRACK_TEST_H

#include <QObject>

class ShowCommandTrack_Test final : public QObject
{
    Q_OBJECT

private slots:
    // authored model
    void commandValidation_data();
    void commandValidation();
    void insertKeepsTimeAndInsertionOrder();
    void insertRejectsDuplicateId();
    void replaceEditsOnlyTheAddressedCommand();
    void retimeMovesOneCommand();
    void removeLeavesTheRestUntouched();
    void extentIsAuthoredIndependently();
    void referencedTargetsAndRemap();

    // serialization
    void saveWritesNamedVersionedFields();
    void saveLoadRoundTripPreservesOrderAndValues();
    void loadReplacesPreviousContents();
    void loadIsTransactional_data();
    void loadIsTransactional();
    void loadLeavesReaderUsableAfterFailure();
    void loadRecoversFromNestedTrackNames();
    void eventIdsRunOutExplicitly();

    // playback transitions
    void advanceAppliesDueCommandsOnce();
    void advanceIgnoresPausedAndBackwardTransport();
    void seekRestoresLatestValues_data();
    void seekRestoresLatestValues();
    void seekKeepsIntentAndLoopReplaysCommands();
    void extentNeverSynthesizesEffects();

    // recording transitions
    void armingBindsTheFirstResolvedShow();
    void bindingSurvivesPauseSeekAndLoop();
    void recordsOnlyAcceptedUserInput_data();
    void recordsOnlyAcceptedUserInput();
    void recordingIsGatedByPhaseAndTransport_data();
    void recordingIsGatedByPhaseAndTransport();
    void liveCommandDoesNotEchoUntilTheNextTraversal();
    void liveInputMarksOnlyItsOwnEvent();
};

#endif // SHOWCOMMANDTRACK_TEST_H
