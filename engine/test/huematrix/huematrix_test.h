/*
  Q Light Controller Plus - Unit test
  huematrix_test.h

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

#ifndef HUEMATRIX_TEST_H
#define HUEMATRIX_TEST_H

#include <QObject>

class Doc;

class HUEMatrix_Test : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    /* AC1 */
    void functionType();

    /* AC5 */
    void audioScriptsAreNotOfferedToRGBMatrix();
    void hueMatrixOffersAllAudioScripts();

    /* AC4 */
    void hsvScriptProducesFiniteNonUniformMap();

    /* AC6 */
    void upstreamScriptStillWorksOnHueScript();

    /* AC7 */
    void forkPropertyRoundTripsInMemory_data();
    void forkPropertyRoundTripsInMemory();

    /* AC8 */
    void forkPropertiesSurviveXmlRoundTrip();
    void unknownXmlValuesFallBackToDefaults();

    /* AC9 */
    void unavailableAlgorithmIsRejected();

    /* AC11 */
    void scriptPropertyAttrDoesNotCollide();

    /* AC12 */
    void hueMatrixSupportsScriptPropertyAttributes();
    void hueMatrixSupportsForkAttributes();

    /* built-in algorithm edge case */
    void builtInAlgorithmStillRunsTransformPipeline();

    /* AC13 */
    void hsvScriptRgbMapIsSafeFromAnotherThread();

    /* AC16 */
    void builtInAudioAlgorithmReportsUsesAudio();

    /* AC14 */
    void hueCacheOffersAudioScriptsAfterStartupStyleLoad();

    /* AC15 */
    void unavailableAlgorithmWarnsOnWorkspaceLoad();

    /* AC17 */
    void hueMatrixHasItsOwnIcon();

    /* AC18 */
    void everyHueMatrixIconSiteUsesTheHueIcon();

    /* AC19 */
    void builtInAudioIsReachableByNameOnHueMatrix();
    void noHueScriptShadowsABuiltInName();

    /* AC20 */
    void destructorGivesUpOnAnAsyncTaskThatNeverClears();
    void destructorReturnsImmediatelyWhenNothingIsInFlight();

    /* AC24 */
    void asyncPrecomputeProducesAConsumableMap();
    void asyncPrecomputeIsThrottledToOneTaskInFlight();
    void asyncPrecomputeIsSkippedForNonScriptAlgorithms();
    void precomputedMapIsRejectedWhenTheGenerationMoved();
    void inFlightPrecomputeIsDiscardedWhenInvalidatedMidFlight();

    /* AC21 */
    void forkOnlyAudioProfileIdTagWarnsOnRGBMatrixLoad();

    /* AC23 */
    void audioAlgorithmRecomputesTheMapEveryTick();

private:
    Doc *m_doc;
};

#endif
