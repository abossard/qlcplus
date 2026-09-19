/*
  Q Light Controller Plus - Unit test
  timingdiagnostics_test.h

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

#ifndef TIMINGDIAGNOSTICS_TEST_H
#define TIMINGDIAGNOSTICS_TEST_H

#include <QObject>

class TimingDiagnostics_Test : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void disabledGateEmitsNothing_data();
    void disabledGateEmitsNothing();

    void rareEventFlushesWithinOneInterval();

    void finalTailIsNotDropped();

    void rateBoundOneSummaryPerInterval();

    void periodRetainsCountAndMax();

    void periodUnknownMaxWriteIsNotZero();

    void fractionalMsResidualIsExact();

    void hueHandoffSplitsWaitFromWorker_data();
    void hueHandoffSplitsWaitFromWorker();

    /* production runtime control + bounded non-consuming snapshot (C0-1/C0-5) */
    void productionControlRoundTrip();
    void snapshotSurvivesFlushDisableAndIsNonConsuming();
    /* capture identity rejects in-flight samples across a control change (C0-8) */
    void staleCaptureSampleIsDiscarded();
    /* worker CPU is a separate field, unknown-preserving (C0-3) */
    void workerCpuIsSeparateFieldAndUnknownAware();

    /* snapshot must pair captureId with its own aggregates atomically (C0-8) */
    void snapshotCaptureIdMatchesAggregatesUnderConcurrency();

    /* a retained (disabled) capture reports a frozen duration, not growing age */
    void disabledCaptureDurationIsFrozen();
};

#endif
