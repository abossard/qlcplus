/*
  Q Light Controller Plus - Unit test
  vcbutton_test.h

  Copyright (c) Massimo Callegari

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

#ifndef VCBUTTON_TEST_H
#define VCBUTTON_TEST_H

#include <QObject>

class VCButton_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void freezeAction_survivesXmlRoundTrip_data();
    void freezeAction_survivesXmlRoundTrip();

    void mcpBridge_freezeAction_data();
    void mcpBridge_freezeAction();

    void freezePress_togglesGlobalLatchOnce_data();
    void freezePress_togglesGlobalLatchOnce();

    void freezeHoldGesture_holdsWhilePressed_data();
    void freezeHoldGesture_holdsWhilePressed();

    void freezeRoutesThroughRealInputDispatch_data();
    void freezeRoutesThroughRealInputDispatch();

    void freezeHold_onlyOwningControlReleases_data();
    void freezeHold_onlyOwningControlReleases();
    void freezeModeSwitch_releasesAndRearms();
    void freezeHold_pointerInterruptionReleases_data();
    void freezeHold_pointerInterruptionReleases();

    void freezeState_sharedAcrossButtons();
    void freezeModes_componentStateAndInteraction();

    void freezeEditorRow_adoptsActionWithoutActuating_data();
    void freezeEditorRow_adoptsActionWithoutActuating();
    void freezeHold_releasesWhenHeldControlGoesAway_data();
    void freezeHold_releasesWhenHeldControlGoesAway();
    void freezeButton_leavesAttachedFunctionAlone();

    void freezeWorkspace_survivesClearAndLoad();

    void existingActions_preserved_data();
    void existingActions_preserved();

    void existingActions_qmlPointerPathUnchanged_data();
    void existingActions_qmlPointerPathUnchanged();
};

#endif // VCBUTTON_TEST_H
